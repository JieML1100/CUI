#include "WebBrowser.h"
#include "Form.h"

#include <CppUtils/Graphics/Graphics1.h>
#include <CppUtils/Utils/StringHelper.h>

#include <windowsx.h>
#include <algorithm>

#include <wrl.h>
#include <wrl/client.h>

#pragma comment(lib, "Ole32.lib")

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

static const wchar_t* kWebHostClassName = L"CUI_WebViewHost";

static void EnsureWebHostClass()
{
	static bool inited = false;
	if (inited) return;

	WNDCLASSW wc{};
	wc.lpfnWndProc = DefWindowProcW;
	wc.hInstance = GetModuleHandleW(NULL);
	wc.lpszClassName = kWebHostClassName;
	RegisterClassW(&wc);
	inited = true;
}

WebBrowser::WebBrowser(int x, int y, int width, int height)
{
	this->Location = { x, y };
	this->Size = { width, height };
	this->BackColor = Colors::White;
	_lastInitHr = E_PENDING;
	_lastControllerHr = E_PENDING;
	_lastGetWebViewHr = E_PENDING;

	// 位置/尺寸变化时同步 Controller bounds
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
	_controller.Reset();
	_env.Reset();

	if (_hostHwnd && IsWindow(_hostHwnd))
	{
		DestroyWindow(_hostHwnd);
		_hostHwnd = NULL;
	}
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
	if (_initialized) return;
	if (!this->ParentForm || !this->ParentForm->Handle) return;

	_initialized = true;
	_lastInitHr = E_PENDING;
	_lastControllerHr = E_PENDING;
	_lastGetWebViewHr = E_PENDING;
	_webviewReady = false;
	_navCompletedCount = 0;

	// WebView2 推荐 STA
	_lastCoInitHr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

	EnsureWebHostClass();
	if (!_hostHwnd)
	{
		// 创建宿主窗口（子窗口）。WebView2 将在此窗口内创建并渲染自身的子窗口。
		_hostHwnd = CreateWindowExW(
			0,
			kWebHostClassName,
			L"",
			WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
			0, 0, std::max(1, this->Width), std::max(1, this->Height),
			this->ParentForm->Handle,
			NULL,
			GetModuleHandleW(NULL),
			NULL);
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

			auto ctlCompleted = Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
				[this](HRESULT result2, ICoreWebView2Controller* controller) -> HRESULT
				{
					_lastControllerHr = result2;
					if (FAILED(result2) || !controller)
					{
						this->PostRender();
						return S_OK;
					}
					_controller = controller;
					_webview.Reset();
					_lastGetWebViewHr = _controller->get_CoreWebView2(_webview.GetAddressOf());

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
									(void)sender;
									(void)args;
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
									(void)sender;
									(void)args;
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

			env->CreateCoreWebView2Controller(_hostHwnd, ctlCompleted.Get());
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
	if (!_hostHwnd || !IsWindow(_hostHwnd)) return;

	int w = std::max(1, this->Width);
	int h = std::max(1, this->Height);

	// 注意：Control::AbsLocation 不包含标题栏高度；子窗口坐标是相对 Form 客户区
	POINT abs = this->AbsLocation;
	int top = (this->ParentForm && this->ParentForm->VisibleHead) ? this->ParentForm->HeadHeight : 0;
	int x = abs.x;
	int y = abs.y + top;

	// 控件不可见时隐藏宿主窗口，避免抢占鼠标/焦点
	if (!this->IsVisual || !this->Visible || !_webviewReady)
	{
		ShowWindow(_hostHwnd, SW_HIDE);
	}
	else
	{
		ShowWindow(_hostHwnd, SW_SHOWNOACTIVATE);
	}

	MoveWindow(_hostHwnd, x, y, w, h, FALSE);

	if (_controller)
	{
		RECT rc{ 0,0,w,h };
		_controller->put_Bounds(rc);
		_controller->NotifyParentWindowPositionChanged();
	}
}

void WebBrowser::Update()
{
	EnsureInitialized();
	EnsureControllerBounds();

	if (!this->ParentForm || !this->ParentForm->Render) return;

	auto abs = this->AbsLocation;
	auto sz = this->ActualSize();

	// 仅在 WebView 未就绪时绘制占位（就绪后由 WebView2 子窗口自行绘制，无需抓帧/贴图）
	if (!_webviewReady)
	{
		this->ParentForm->Render->FillRect((float)abs.x, (float)abs.y, (float)sz.cx, (float)sz.cy, this->BackColor);
		std::wstring status = L"WebBrowser: ";
		if (!_initialized)
			status += L"not initialized";
		else if (!_webviewReady)
			status += L"initializing...";
		else if (_navCompletedCount == 0)
			status += L"waiting for navigation...";
		else
			status += L"loading...";

		this->ParentForm->Render->DrawString(status, (float)abs.x + 8.0f, (float)abs.y + 6.0f, Colors::DimGrey);
	}
}

bool WebBrowser::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof)
{
	Control::ProcessMessage(message, wParam, lParam, xof, yof);
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
			// 脚本执行后可能改变内容：占位区域刷新一下（实际页面由 WebView2 自行呈现）
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
