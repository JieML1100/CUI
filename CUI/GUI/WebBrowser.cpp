#include "WebBrowser.h"
#include "Form.h"

#include <windowsx.h>
#include <algorithm>
#include <dcomp.h>

#include <wrl.h>
#include <wrl/client.h>
using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

WebBrowser::WebBrowser(int x, int y, int width, int height)
{
	this->Location = { x, y };
	this->Size = { width, height };
	this->BackColor = Colors::White;
	_lastInitHr = E_PENDING;
	_lastControllerHr = E_PENDING;
	_lastGetWebViewHr = E_PENDING;

	this->OnSizeChanged += [&](class Control* s) { 
		(void)s; 
		EnsureControllerBounds(); 
	};
	this->OnMoved += [&](class Control* s) { 
		(void)s; 
		EnsureControllerBounds(); 
	};
}

WebBrowser::~WebBrowser()
{
	_webview.Reset();
	if (_compositionController && _cursorChangedToken.value != 0)
	{
		_compositionController->remove_CursorChanged(_cursorChangedToken);
		_cursorChangedToken.value = 0;
	}
	_compositionController.Reset();
	_controller.Reset();
	_env.Reset();

	_dcompClip.Reset();
	_dcompVisual.Reset();
}

std::wstring WebBrowser::JsStringLiteral(const std::wstring& s)
{
	std::wstring out;
	out.reserve(s.size() + 8);
	out.push_back(L'"');
	for (wchar_t c : s)
	{
		switch (c)
		{
		case L'\\': out += L"\\\\"; break;
		case L'"': out += L"\\\""; break;
		case L'\r': out += L"\\r"; break;
		case L'\n': out += L"\\n"; break;
		case L'\t': out += L"\\t"; break;
		default:
			if (c >= 0 && c < 0x20)
			{
				wchar_t buf[8];
				swprintf_s(buf, L"\\u%04x", (unsigned)c);
				out += buf;
			}
			else
			{
				out.push_back(c);
			}
			break;
		}
	}
	out.push_back(L'"');
	return out;
}

static int HexVal(wchar_t c)
{
	if (c >= L'0' && c <= L'9') return (int)(c - L'0');
	if (c >= L'a' && c <= L'f') return 10 + (int)(c - L'a');
	if (c >= L'A' && c <= L'F') return 10 + (int)(c - L'A');
	return -1;
}

static std::wstring JsonUnquote(const std::wstring& json)
{
	if (json == L"null") return L"";
	if (json.size() < 2) return json;
	if (json.front() != L'"' || json.back() != L'"') return json;

	std::wstring out;
	out.reserve(json.size());
	for (size_t i = 1; i + 1 < json.size(); i++)
	{
		wchar_t c = json[i];
		if (c != L'\\')
		{
			out.push_back(c);
			continue;
		}
		if (i + 1 >= json.size() - 1) break;
		wchar_t e = json[++i];
		switch (e)
		{
		case L'"': out.push_back(L'"'); break;
		case L'\\': out.push_back(L'\\'); break;
		case L'/': out.push_back(L'/'); break;
		case L'b': out.push_back(L'\b'); break;
		case L'f': out.push_back(L'\f'); break;
		case L'n': out.push_back(L'\n'); break;
		case L'r': out.push_back(L'\r'); break;
		case L't': out.push_back(L'\t'); break;
		case L'u':
		{
			if (i + 4 >= json.size() - 1) break;
			int h1 = HexVal(json[i + 1]);
			int h2 = HexVal(json[i + 2]);
			int h3 = HexVal(json[i + 3]);
			int h4 = HexVal(json[i + 4]);
			if (h1 < 0 || h2 < 0 || h3 < 0 || h4 < 0) break;
			wchar_t uc = (wchar_t)((h1 << 12) | (h2 << 8) | (h3 << 4) | h4);
			out.push_back(uc);
			i += 4;
		}
		break;
		default:
			out.push_back(e);
			break;
		}
	}
	return out;
}

void WebBrowser::EnsureInitialized()
{
	if (Application::IsDesignMode()) return;
	if (_initialized) return;
	if (!this->ParentForm || !this->ParentForm->Handle) return;

	_initialized = true;
	_lastInitHr = E_PENDING;
	_lastControllerHr = E_PENDING;
	_lastGetWebViewHr = E_PENDING;
	_webviewReady = false;
	_navCompletedCount = 0;

	_lastCoInitHr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

	// DirectComposition：为每个 WebBrowser 创建一个独立的 Visual，并挂到 Form 的 Web 容器层
	IDCompositionDevice* dcompDevice = this->ParentForm->GetDCompDevice();
	IDCompositionVisual* container = this->ParentForm->GetWebContainerVisual();
	if (!dcompDevice || !container)
	{
		_lastInitHr = E_NOINTERFACE;
		this->PostRender();
		return;
	}

	if (!_dcompVisual)
	{
		HRESULT hrv = dcompDevice->CreateVisual(&_dcompVisual);
		if (FAILED(hrv) || !_dcompVisual)
		{
			_lastInitHr = hrv;
			this->PostRender();
			return;
		}
		dcompDevice->CreateRectangleClip(&_dcompClip);
		if (_dcompClip)
		{
			_dcompVisual->SetClip(_dcompClip.Get());
		}
		// 插入到容器末尾（多个 WebBrowser 时保持顺序）
		container->AddVisual(_dcompVisual.Get(), FALSE, nullptr);
		this->ParentForm->CommitComposition();
	}

	auto envCompleted = Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
		[this](HRESULT result, ICoreWebView2Environment* env) -> HRESULT
		{
			_lastInitHr = result;
			if (FAILED(result) || !env)
			{
				this->PostRender();
				return S_OK;
			}
			_env = env;

			auto ctlCompleted = Callback<ICoreWebView2CreateCoreWebView2CompositionControllerCompletedHandler>(
				[this](HRESULT result2, ICoreWebView2CompositionController* compositionController) -> HRESULT
				{
					_lastControllerHr = result2;
					if (FAILED(result2) || !compositionController)
					{
						this->PostRender();
						return S_OK;
					}
					_compositionController = compositionController;
					_controller.Reset();
					// 同一对象上也实现 ICoreWebView2Controller
					_compositionController.As(&_controller);
					_webview.Reset();
					_lastGetWebViewHr = _controller->get_CoreWebView2(_webview.GetAddressOf());

					// 将 WebView2 视觉树挂到我们的 DComp Visual
					if (_compositionController && _dcompVisual)
					{
						_compositionController->put_RootVisualTarget(_dcompVisual.Get());
						_rootAttached = true;
						this->ParentForm->CommitComposition();
					}

					// CursorChanged：缓存 system cursor id，交给 Form::UpdateCursor 使用
					if (_compositionController)
					{
						_cursorChangedToken.value = 0;
						_compositionController->add_CursorChanged(
							Callback<ICoreWebView2CursorChangedEventHandler>(
								[this](ICoreWebView2CompositionController* sender, IUnknown* args) -> HRESULT
								{
									(void)args;
									UINT32 id = 0;
									if (sender && SUCCEEDED(sender->get_SystemCursorId(&id)))
									{
										_lastSystemCursorId = id;
										_hasSystemCursorId = true;
									}
									else
									{
										_hasSystemCursorId = false;
									}
									// 如果当前鼠标在 WebBrowser 上，立刻刷新一次光标
									if (this->ParentForm && this->ParentForm->UnderMouse == this)
										this->ParentForm->UpdateCursorFromCurrentMouse();
									return S_OK;
								}).Get(),
							&_cursorChangedToken);
					}

					EnsureControllerBounds();
					_controller->put_IsVisible(TRUE);

					ComPtr<ICoreWebView2Settings> settings;
					if (_webview && SUCCEEDED(_webview->get_Settings(&settings)) && settings)
					{
						settings->put_AreDefaultContextMenusEnabled(TRUE);
						settings->put_IsStatusBarEnabled(FALSE);
						settings->put_IsZoomControlEnabled(TRUE);
					}

					_webviewReady = (SUCCEEDED(_lastGetWebViewHr) && _webview != nullptr);

					// 导航完成时触发重绘（仅用于占位提示更新；实际页面由 WebView2 自身渲染）
					if (_webview)
					{
						EventRegistrationToken tok{};
						_webview->add_NavigationCompleted(
							Callback<ICoreWebView2NavigationCompletedEventHandler>(
								[this](ICoreWebView2* sender, ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT
								{
									_navCompletedCount++;
									this->PostRender();
									return S_OK;
								}).Get(),
							&tok);

						// 内容加载完成时也触发重绘
						_webview->add_ContentLoading(
							Callback<ICoreWebView2ContentLoadingEventHandler>(
								[this](ICoreWebView2* sender, ICoreWebView2ContentLoadingEventArgs* args) -> HRESULT
								{
									this->PostRender();
									return S_OK;
								}).Get(),
							&tok);
					}

					// 处理延迟的 Navigate/Html
					if (!_pendingHtml.empty())
					{
						auto html = _pendingHtml;
						_pendingHtml.clear();
						SetHtml(html);
					}
					if (!_pendingUrl.empty())
					{
						auto url = _pendingUrl;
						_pendingUrl.clear();
						Navigate(url);
					}

					this->PostRender();
					return S_OK;
				});

			ComPtr<ICoreWebView2Environment3> env3;
			HRESULT hrEnv3 = env->QueryInterface(IID_PPV_ARGS(&env3));
			if (FAILED(hrEnv3) || !env3)
			{
				_lastControllerHr = hrEnv3;
				this->PostRender();
				return S_OK;
			}
			env3->CreateCoreWebView2CompositionController(this->ParentForm->Handle, ctlCompleted.Get());
			return S_OK;
		});

	HRESULT hrStart = CreateCoreWebView2EnvironmentWithOptions(nullptr, nullptr, nullptr, envCompleted.Get());
	if (FAILED(hrStart))
	{
		_lastInitHr = hrStart;
		this->PostRender();
	}
}

void WebBrowser::EnsureControllerBounds()
{
	if (!this->ParentForm || !this->ParentForm->Handle) return;

	int w = std::max(1, this->Width);
	int h = std::max(1, this->Height);

	POINT abs = this->AbsLocation;
	int top = (this->ParentForm && this->ParentForm->VisibleHead) ? this->ParentForm->HeadHeight : 0;
	int x = abs.x;
	int y = abs.y + top;

	const bool parentEnabled = ::IsWindowEnabled(this->ParentForm->Handle) != FALSE;
	const bool visible = (parentEnabled && this->IsVisual && this->Visible && _webviewReady);

	if (_controller)
	{
		RECT rc{ 0,0,w,h };
		_controller->put_Bounds(rc);
		_controller->put_IsVisible(visible ? TRUE : FALSE);
		_controller->NotifyParentWindowPositionChanged();
	}

	if (_dcompVisual)
	{
		_dcompVisual->SetOffsetX((float)x);
		_dcompVisual->SetOffsetY((float)y);
		if (_dcompClip)
		{
			_dcompClip->SetLeft(0.0f);
			_dcompClip->SetTop(0.0f);
			_dcompClip->SetRight((float)w);
			_dcompClip->SetBottom((float)h);
		}

		// 关键：隐藏时断开 RootVisualTarget，避免“隐藏页残留显示上一帧”
		if (_compositionController)
		{
			if (!visible && _rootAttached)
			{
				_compositionController->put_RootVisualTarget(nullptr);
				_rootAttached = false;
			}
			else if (visible && !_rootAttached)
			{
				_compositionController->put_RootVisualTarget(_dcompVisual.Get());
				_rootAttached = true;
			}
		}

		// 位置/裁剪/挂载更新需要 Commit
		if (this->ParentForm) this->ParentForm->CommitComposition();
	}
}

void WebBrowser::Update()
{
	// 设计器中不创建真实 WebView2，避免生成 "<exe>.WebView2" 目录。
	// 仅绘制黑色占位矩形即可。
	if (Application::IsDesignMode())
	{
		// 注意：TabControl 为了同步原生控件显示/隐藏，会对所有页递归调用 WebBrowser::Update()
		//（包括当前不可见的页）。设计模式下必须尊重 IsVisual，否则隐藏页也会被绘制出来。
		if (!this->IsVisual || !this->Visible) return;
		if (!this->ParentForm || !this->ParentForm->Render) return;
		auto abs = this->AbsLocation;
		auto sz = this->ActualSize();
		this->ParentForm->Render->FillRect((float)abs.x, (float)abs.y, (float)sz.cx, (float)sz.cy, Colors::Black);
		return;
	}

	EnsureInitialized();
	EnsureControllerBounds();

	if (!this->ParentForm || !this->ParentForm->Render) return;

	auto abs = this->AbsLocation;
	auto sz = this->ActualSize();
}

bool WebBrowser::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof)
{
	// Composition 模式下需要显式转发鼠标输入
	ForwardMouseMessageToWebView(message, wParam, lParam, xof, yof);
	Control::ProcessMessage(message, wParam, lParam, xof, yof);
	return true;
}

bool WebBrowser::TryGetSystemCursorId(UINT32& outId) const
{
	if (!_webviewReady || !_compositionController) return false;
	if (!_hasSystemCursorId) return false;
	outId = _lastSystemCursorId;
	return true;
}

bool WebBrowser::ForwardMouseMessageToWebView(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof)
{
	(void)lParam;
	if (!_webviewReady || !_compositionController) return false;
	if (!this->Visible || !this->IsVisual) return false;

	COREWEBVIEW2_MOUSE_EVENT_KIND kind{};
	UINT32 mouseData = 0;

	switch (message)
	{
	case WM_MOUSEMOVE: kind = COREWEBVIEW2_MOUSE_EVENT_KIND_MOVE; break;
	case WM_LBUTTONDOWN: kind = COREWEBVIEW2_MOUSE_EVENT_KIND_LEFT_BUTTON_DOWN; break;
	case WM_LBUTTONUP: kind = COREWEBVIEW2_MOUSE_EVENT_KIND_LEFT_BUTTON_UP; break;
	case WM_RBUTTONDOWN: kind = COREWEBVIEW2_MOUSE_EVENT_KIND_RIGHT_BUTTON_DOWN; break;
	case WM_RBUTTONUP: kind = COREWEBVIEW2_MOUSE_EVENT_KIND_RIGHT_BUTTON_UP; break;
	case WM_MBUTTONDOWN: kind = COREWEBVIEW2_MOUSE_EVENT_KIND_MIDDLE_BUTTON_DOWN; break;
	case WM_MBUTTONUP: kind = COREWEBVIEW2_MOUSE_EVENT_KIND_MIDDLE_BUTTON_UP; break;
	case WM_MOUSEWHEEL:
		kind = COREWEBVIEW2_MOUSE_EVENT_KIND_WHEEL;
		mouseData = (UINT32)GET_WHEEL_DELTA_WPARAM(wParam);
		break;
	case WM_MOUSEHWHEEL:
		kind = COREWEBVIEW2_MOUSE_EVENT_KIND_HORIZONTAL_WHEEL;
		mouseData = (UINT32)GET_WHEEL_DELTA_WPARAM(wParam);
		break;
	default:
		return false;
	}

	COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS vkeys = (COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS)0;
	if (wParam & MK_LBUTTON) vkeys = (COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS)(vkeys | COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_LEFT_BUTTON);
	if (wParam & MK_RBUTTON) vkeys = (COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS)(vkeys | COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_RIGHT_BUTTON);
	if (wParam & MK_MBUTTON) vkeys = (COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS)(vkeys | COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_MIDDLE_BUTTON);
	if (wParam & MK_XBUTTON1) vkeys = (COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS)(vkeys | COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_X_BUTTON1);
	if (wParam & MK_XBUTTON2) vkeys = (COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS)(vkeys | COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_X_BUTTON2);
	if (GetKeyState(VK_CONTROL) & 0x8000) vkeys = (COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS)(vkeys | COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_CONTROL);
	if (GetKeyState(VK_SHIFT) & 0x8000) vkeys = (COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS)(vkeys | COREWEBVIEW2_MOUSE_EVENT_VIRTUAL_KEYS_SHIFT);

	POINT pt{ xof, yof };
	_compositionController->SendMouseInput(kind, vkeys, mouseData, pt);

	// 尽量同步光标（Form 的 UpdateCursor 会覆盖一次，这里在鼠标移动时再补一刀）
	if (message == WM_MOUSEMOVE && this->ParentForm && this->ParentForm->UnderMouse == this)
	{
		UINT32 id = 0;
		if (SUCCEEDED(_compositionController->get_SystemCursorId(&id)) && id != 0)
		{
			_lastSystemCursorId = id;
			_hasSystemCursorId = true;
			auto h = LoadCursorW(NULL, MAKEINTRESOURCEW((ULONG_PTR)id));
			if (h) ::SetCursor(h);
		}
	}

	// 点入时尝试把焦点交给 WebView
	if (_controller && (message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN || message == WM_MBUTTONDOWN))
	{
		_controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
	}

	return true;
}

void WebBrowser::Navigate(const std::wstring& url)
{
	if (!_webviewReady || !_webview)
	{
		_pendingUrl = url;
		return;
	}
	_webview->Navigate(url.c_str());
}

void WebBrowser::SetHtml(const std::wstring& html)
{
	if (!_webviewReady || !_webview)
	{
		_pendingHtml = html;
		return;
	}
	_webview->NavigateToString(html.c_str());
}

void WebBrowser::Reload()
{
	if (_webview)
	{
		_webview->Reload();
	}
}

void WebBrowser::ExecuteScriptAsync(const std::wstring& script,
	std::function<void(HRESULT hr, const std::wstring& jsonResult)> callback)
{
	if (!_webviewReady || !_webview)
	{
		if (callback) callback(E_PENDING, L"");
		return;
	}

	auto cb = Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
		[callback, this](HRESULT errorCode, LPCWSTR resultObjectAsJson) -> HRESULT
		{
			if (callback)
				callback(errorCode, resultObjectAsJson ? resultObjectAsJson : L"");
			this->PostRender();
			return S_OK;
		});

	_webview->ExecuteScript(script.c_str(), cb.Get());
}

void WebBrowser::GetHtmlAsync(std::function<void(HRESULT hr, const std::wstring& html)> callback)
{
	ExecuteScriptAsync(L"document.documentElement.outerHTML",
		[callback](HRESULT hr, const std::wstring& json) {
			if (callback) callback(hr, JsonUnquote(json));
		});
}

void WebBrowser::SetElementInnerHtmlAsync(const std::wstring& cssSelector, const std::wstring& html,
	std::function<void(HRESULT hr)> callback)
{
	std::wstring script =
		L"(function(){"
		L"const el=document.querySelector(" + JsStringLiteral(cssSelector) + L");"
		L"if(el){el.innerHTML=" + JsStringLiteral(html) + L"; return true;} return false;"
		L"})();";

	ExecuteScriptAsync(script, [callback](HRESULT hr, const std::wstring&) {
		if (callback) callback(hr);
		});
}

void WebBrowser::QuerySelectorAllOuterHtmlAsync(const std::wstring& cssSelector,
	std::function<void(HRESULT hr, const std::wstring& jsonArray)> callback)
{
	std::wstring script =
		L"(function(){"
		L"const nodes=[...document.querySelectorAll(" + JsStringLiteral(cssSelector) + L")];"
		L"return nodes.map(n=>n.outerHTML);"
		L"})();";

	ExecuteScriptAsync(script, callback);
}
