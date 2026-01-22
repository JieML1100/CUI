#include "WebBrowser.h"
#include "Form.h"

#include <CppUtils/Utils/Convert.h>

#include <windowsx.h>
#include <algorithm>
#include <dcomp.h>
#include <unordered_map>

#include <wrl.h>
#include <wrl/client.h>
using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

static int HexVal(wchar_t c);

static std::wstring ToW(const std::string& s)
{
	return Convert::Utf8ToUnicode(s);
}

static std::string ToU8(const std::wstring& s)
{
	return Convert::UnicodeToUtf8(s);
}

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
	if (_webview)
	{
		if (_navStartingToken.value != 0) { _webview->remove_NavigationStarting(_navStartingToken); _navStartingToken.value = 0; }
		if (_navCompletedToken.value != 0) { _webview->remove_NavigationCompleted(_navCompletedToken); _navCompletedToken.value = 0; }
		if (_contentLoadingToken.value != 0) { _webview->remove_ContentLoading(_contentLoadingToken); _contentLoadingToken.value = 0; }
		if (_sourceChangedToken.value != 0) { _webview->remove_SourceChanged(_sourceChangedToken); _sourceChangedToken.value = 0; }
		if (_historyChangedToken.value != 0) { _webview->remove_HistoryChanged(_historyChangedToken); _historyChangedToken.value = 0; }
		if (_documentTitleChangedToken.value != 0) { _webview->remove_DocumentTitleChanged(_documentTitleChangedToken); _documentTitleChangedToken.value = 0; }
		if (_newWindowRequestedToken.value != 0) { _webview->remove_NewWindowRequested(_newWindowRequestedToken); _newWindowRequestedToken.value = 0; }
		if (_processFailedToken.value != 0) { _webview->remove_ProcessFailed(_processFailedToken); _processFailedToken.value = 0; }
	}
	if (_webview && _webMessageToken.value != 0)
	{
		_webview->remove_WebMessageReceived(_webMessageToken);
		_webMessageToken.value = 0;
	}
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

std::wstring WebBrowser::UrlEncodeUtf8(const std::wstring& s)
{
	static auto isUnreserved = [](unsigned char c) -> bool
	{
		if (c >= 'a' && c <= 'z') return true;
		if (c >= 'A' && c <= 'Z') return true;
		if (c >= '0' && c <= '9') return true;
		switch (c)
		{
		case '-': case '_': case '.': case '~': return true;
		default: return false;
		}
	};

	std::string u8 = ToU8(s);
	std::wstring out;
	out.reserve(u8.size() * 3);
	for (unsigned char c : u8)
	{
		if (isUnreserved(c))
		{
			out.push_back((wchar_t)c);
		}
		else
		{
			wchar_t buf[4];
			swprintf_s(buf, L"%%%02X", (unsigned)c);
			out.append(buf);
		}
	}
	return out;
}

std::wstring WebBrowser::UrlDecodeUtf8(const std::wstring& s)
{
	std::string bytes;
	bytes.reserve(s.size());
	for (size_t i = 0; i < s.size(); i++)
	{
		wchar_t c = s[i];
		if (c == L'%' && i + 2 < s.size())
		{
			int h1 = HexVal(s[i + 1]);
			int h2 = HexVal(s[i + 2]);
			if (h1 >= 0 && h2 >= 0)
			{
				bytes.push_back((char)((h1 << 4) | h2));
				i += 2;
				continue;
			}
		}
		// encodeURIComponent 不会把空格变成 '+'，但这里兼容一下
		if (c == L'+')
			bytes.push_back(' ');
		else
			bytes.push_back((char)(c & 0xFF));
	}
	return ToW(bytes);
}

bool WebBrowser::TryParseCuiUrl(const std::wstring& url, std::wstring& outAction, std::unordered_map<std::wstring, std::wstring>& outQuery)
{
	outAction.clear();
	outQuery.clear();

	constexpr const wchar_t* kPrefix = L"cui://";
	if (url.rfind(kPrefix, 0) != 0) return false;

	std::wstring rest = url.substr(wcslen(kPrefix));
	// action?key=val&key2=val2
	size_t qpos = rest.find(L'?');
	std::wstring action = (qpos == std::wstring::npos) ? rest : rest.substr(0, qpos);
	if (action.empty()) return false;
	outAction = action;

	if (qpos == std::wstring::npos) return true;
	std::wstring query = rest.substr(qpos + 1);

	size_t pos = 0;
	while (pos < query.size())
	{
		size_t amp = query.find(L'&', pos);
		std::wstring pair = (amp == std::wstring::npos) ? query.substr(pos) : query.substr(pos, amp - pos);
		pos = (amp == std::wstring::npos) ? query.size() : amp + 1;
		if (pair.empty()) continue;
		size_t eq = pair.find(L'=');
		std::wstring k = (eq == std::wstring::npos) ? pair : pair.substr(0, eq);
		std::wstring v = (eq == std::wstring::npos) ? L"" : pair.substr(eq + 1);
		if (k.empty()) continue;
		outQuery[k] = v;
	}
	return true;
}

void WebBrowser::RegisterJsInvokeHandler(const std::wstring& name, JsInvokeHandler handler)
{
	_invokeHandlers[name] = std::move(handler);
}

void WebBrowser::UnregisterJsInvokeHandler(const std::wstring& name)
{
	auto it = _invokeHandlers.find(name);
	if (it != _invokeHandlers.end()) _invokeHandlers.erase(it);
}

void WebBrowser::ClearJsInvokeHandlers()
{
	_invokeHandlers.clear();
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
	_isNavigating = false;
	_cachedSource.clear();
	_cachedTitle.clear();

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
					EnsureInteropInstalled();

					// WebView2 事件注册
					if (_webview)
					{
						_navStartingToken.value = 0;
						_webview->add_NavigationStarting(
							Callback<ICoreWebView2NavigationStartingEventHandler>(
								[this](ICoreWebView2* sender, ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT
								{
									(void)sender;
									WebBrowser::NavigationStartingArgs ev;
									LPWSTR raw = nullptr;
									if (args && SUCCEEDED(args->get_Uri(&raw)) && raw)
									{
										ev.Uri = raw;
										CoTaskMemFree(raw);
									}
									BOOL isUser = FALSE;
									BOOL isRedirected = FALSE;
									if (args)
									{
										args->get_IsUserInitiated(&isUser);
										args->get_IsRedirected(&isRedirected);
									}
									ev.IsUserInitiated = (isUser != FALSE);
									ev.IsRedirected = (isRedirected != FALSE);
									_isNavigating = true;
									OnNavigationStarting(this, ev);
									if (args && ev.Cancel)
										args->put_Cancel(TRUE);
									return S_OK;
								}).Get(),
							&_navStartingToken);

						_navCompletedToken.value = 0;
						_webview->add_NavigationCompleted(
							Callback<ICoreWebView2NavigationCompletedEventHandler>(
								[this](ICoreWebView2* sender, ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT
								{
									(void)sender;
									WebBrowser::NavigationCompletedArgs ev;
									BOOL isSuccess = FALSE;
									COREWEBVIEW2_WEB_ERROR_STATUS status = COREWEBVIEW2_WEB_ERROR_STATUS_UNKNOWN;
									UINT64 navId = 0;
									if (args)
									{
										args->get_IsSuccess(&isSuccess);
										args->get_WebErrorStatus(&status);
										args->get_NavigationId(&navId);
									}
									ev.IsSuccess = (isSuccess != FALSE);
									ev.WebErrorStatus = status;
									ev.NavigationId = navId;
									LPWSTR raw = nullptr;
									if (_webview && SUCCEEDED(_webview->get_Source(&raw)) && raw)
									{
										ev.Uri = raw;
										_cachedSource = ev.Uri;
										CoTaskMemFree(raw);
									}
#if defined(__ICoreWebView2NavigationCompletedEventArgs2_INTERFACE_DEFINED__)
									// HttpStatusCode 需要较新的 WebView2 SDK，旧版本保持默认值
#endif

									_isNavigating = false;
									OnNavigationCompleted(this, ev);
									if (!ev.IsSuccess)
										OnNavigationFailed(this, ev);
									_navCompletedCount++;
									this->PostRender();
									return S_OK;
								}).Get(),
							&_navCompletedToken);

						_contentLoadingToken.value = 0;
						_webview->add_ContentLoading(
							Callback<ICoreWebView2ContentLoadingEventHandler>(
								[this](ICoreWebView2* sender, ICoreWebView2ContentLoadingEventArgs* args) -> HRESULT
								{
									(void)sender;
									WebBrowser::ContentLoadingArgs ev;
									BOOL isError = FALSE;
									UINT64 navId = 0;
									if (args)
									{
										args->get_IsErrorPage(&isError);
										args->get_NavigationId(&navId);
									}
									ev.IsErrorPage = (isError != FALSE);
									ev.NavigationId = navId;
									OnContentLoading(this, ev);
									this->PostRender();
									return S_OK;
								}).Get(),
							&_contentLoadingToken);

							// DOMContentLoaded 事件改为 JS 注入触发（兼容旧 WebView2 SDK）

						_sourceChangedToken.value = 0;
						_webview->add_SourceChanged(
							Callback<ICoreWebView2SourceChangedEventHandler>(
								[this](ICoreWebView2* sender, ICoreWebView2SourceChangedEventArgs* args) -> HRESULT
								{
									(void)sender;
									WebBrowser::SourceChangedArgs ev;
									BOOL isNew = FALSE;
									if (args)
										args->get_IsNewDocument(&isNew);
									ev.IsNewDocument = (isNew != FALSE);
									LPWSTR raw = nullptr;
									if (_webview && SUCCEEDED(_webview->get_Source(&raw)) && raw)
									{
										ev.Uri = raw;
										_cachedSource = ev.Uri;
										CoTaskMemFree(raw);
									}
									OnSourceChanged(this, ev);
									return S_OK;
								}).Get(),
							&_sourceChangedToken);

						_historyChangedToken.value = 0;
						_webview->add_HistoryChanged(
							Callback<ICoreWebView2HistoryChangedEventHandler>(
								[this](ICoreWebView2* sender, IUnknown* args) -> HRESULT
								{
									(void)sender;
									(void)args;
									WebBrowser::HistoryChangedArgs ev;
									BOOL canBack = FALSE;
									BOOL canForward = FALSE;
									if (_webview)
									{
										_webview->get_CanGoBack(&canBack);
										_webview->get_CanGoForward(&canForward);
									}
									ev.CanGoBack = (canBack != FALSE);
									ev.CanGoForward = (canForward != FALSE);
									OnHistoryChanged(this, ev);
									return S_OK;
								}).Get(),
							&_historyChangedToken);

						_documentTitleChangedToken.value = 0;
						_webview->add_DocumentTitleChanged(
							Callback<ICoreWebView2DocumentTitleChangedEventHandler>(
								[this](ICoreWebView2* sender, IUnknown* args) -> HRESULT
								{
									(void)sender;
									(void)args;
									WebBrowser::DocumentTitleChangedArgs ev;
									LPWSTR raw = nullptr;
									if (_webview && SUCCEEDED(_webview->get_DocumentTitle(&raw)) && raw)
									{
										ev.Title = raw;
										_cachedTitle = ev.Title;
										CoTaskMemFree(raw);
									}
									OnDocumentTitleChanged(this, ev);
									return S_OK;
								}).Get(),
							&_documentTitleChangedToken);

						_newWindowRequestedToken.value = 0;
						_webview->add_NewWindowRequested(
							Callback<ICoreWebView2NewWindowRequestedEventHandler>(
								[this](ICoreWebView2* sender, ICoreWebView2NewWindowRequestedEventArgs* args) -> HRESULT
								{
									(void)sender;
									WebBrowser::NewWindowRequestedArgs ev;
									LPWSTR raw = nullptr;
									if (args && SUCCEEDED(args->get_Uri(&raw)) && raw)
									{
										ev.Uri = raw;
										CoTaskMemFree(raw);
									}
									BOOL isUser = FALSE;
									if (args)
										args->get_IsUserInitiated(&isUser);
									ev.IsUserInitiated = (isUser != FALSE);
									OnNewWindowRequested(this, ev);
									if (args && ev.Handled)
										args->put_Handled(TRUE);
									return S_OK;
								}).Get(),
							&_newWindowRequestedToken);

						_processFailedToken.value = 0;
						_webview->add_ProcessFailed(
							Callback<ICoreWebView2ProcessFailedEventHandler>(
								[this](ICoreWebView2* sender, ICoreWebView2ProcessFailedEventArgs* args) -> HRESULT
								{
									(void)sender;
									WebBrowser::ProcessFailedArgs ev;
									COREWEBVIEW2_PROCESS_FAILED_KIND kind = COREWEBVIEW2_PROCESS_FAILED_KIND_UNKNOWN_PROCESS_EXITED;
									if (args)
										args->get_ProcessFailedKind(&kind);
									ev.Kind = kind;
									OnProcessFailed(this, ev);
									return S_OK;
								}).Get(),
							&_processFailedToken);
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

void WebBrowser::EnsureInteropInstalled()
{
	if (_interopInstalled) return;
	if (!_webviewReady || !_webview) return;

	// 1) 注入 JS 桥：window.CUI.invoke(name, payload) -> postMessage("cui://invoke?..."), 并监听回包
	// 2) C++ 侧回包：postMessage("cui://resp?...&result=...")
	std::wstring bridgeJs =
		L"(function(){"
		L" if(window.CUI && window.CUI.__installed) return;"
		L" if(!window.chrome || !chrome.webview || !chrome.webview.postMessage) return;"
		L" const pending = new Map();"
		L" let seq = 0;"
		L" function enc(s){ return encodeURIComponent(s==null?'':String(s)); }"
		L" function post(url){ chrome.webview.postMessage(url); }"
		L" function notifyDom(){ try{ post('cui://domcontentloaded'); }catch(e){} }"
		L" if(document.readyState === 'loading'){ document.addEventListener('DOMContentLoaded', notifyDom, {once:true}); }"
		L" else { setTimeout(notifyDom,0); }"
		L" chrome.webview.addEventListener('message', function(ev){"
		L"   try{"
		L"     const msg = String(ev.data||'');"
		L"     if(!msg.startsWith('cui://resp?')) return;"
		L"     const q = msg.substring('cui://resp?'.length);"
		L"     const params = new URLSearchParams(q);"
		L"     const id = params.get('id');"
		L"     const ok = params.get('ok') === '1';"
		L"     const res = decodeURIComponent(params.get('result')||'');"
		L"     const err = decodeURIComponent(params.get('error')||'');"
		L"     const p = pending.get(id);"
		L"     if(!p) return;"
		L"     pending.delete(id);"
		L"     ok ? p.resolve(res) : p.reject(new Error(err||'CUI invoke failed'));"
		L"   }catch(e){}"
		L" });"
		L" window.CUI = {"
		L"   __installed:true,"
		L"   invoke: function(name, payload){"
		L"     const id = String(++seq);"
		L"     const url = 'cui://invoke?id='+id+'&name='+enc(name)+'&payload='+enc(payload);"
		L"     return new Promise(function(resolve,reject){"
		L"       pending.set(id,{resolve:resolve,reject:reject});"
		L"       post(url);"
		L"     });"
		L"   },"
		L"   notify: function(name, payload){"
		L"     const url = 'cui://notify?name='+enc(name)+'&payload='+enc(payload);"
		L"     post(url);"
		L"   }"
		L" };"
		L"})();";

	// 让桥在每次文档创建时都存在（Navigate / NavigateToString 都覆盖）
	ComPtr<ICoreWebView2_4> web4;
	if (SUCCEEDED(_webview.As(&web4)) && web4)
	{
		web4->AddScriptToExecuteOnDocumentCreated(bridgeJs.c_str(), nullptr);
	}
	else
	{
		// 退化：直接执行一次（对当前页有效）
		ExecuteScriptAsync(bridgeJs);
	}

	// WebMessageReceived：接收 JS -> C++
	_webMessageToken.value = 0;
	_webview->add_WebMessageReceived(
		Callback<ICoreWebView2WebMessageReceivedEventHandler>(
			[this](ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT
			{
				(void)sender;
				if (!args || !_webview) return S_OK;

				LPWSTR raw = nullptr;
				if (FAILED(args->TryGetWebMessageAsString(&raw)) || !raw) return S_OK;
				std::wstring msg(raw);
				CoTaskMemFree(raw);

				std::wstring action;
				std::unordered_map<std::wstring, std::wstring> q;
				if (!TryParseCuiUrl(msg, action, q))
				{
					WebBrowser::WebMessageReceivedArgs ev{ msg };
					OnWebMessageReceived(this, ev);
					return S_OK;
				}

				auto get = [&](const wchar_t* k) -> std::wstring
				{
					auto it = q.find(k);
					return (it == q.end()) ? L"" : it->second;
				};

				if (action == L"invoke")
				{
					std::wstring id = get(L"id");
					std::wstring name = UrlDecodeUtf8(get(L"name"));
					std::wstring payload = UrlDecodeUtf8(get(L"payload"));

					std::wstring ok = L"0";
					std::wstring result;
					std::wstring error;

					auto it = _invokeHandlers.find(name);
					if (it != _invokeHandlers.end() && it->second)
					{
						try
						{
							result = it->second(payload);
							ok = L"1";
						}
						catch (...)
						{
							error = L"handler exception";
						}
					}
					else
					{
						error = L"handler not found: " + name;
					}

					std::wstring resp =
						L"cui://resp?id=" + UrlEncodeUtf8(id) +
						L"&ok=" + ok +
						L"&result=" + UrlEncodeUtf8(result) +
						L"&error=" + UrlEncodeUtf8(error);
					_webview->PostWebMessageAsString(resp.c_str());
				}
				else if (action == L"notify")
				{
					// 预留：如需可在这里扩展 OnNotify 事件
				}
				else if (action == L"domcontentloaded")
				{
					WebBrowser::DomContentLoadedArgs ev;
					ev.NavigationId = 0;
					OnDOMContentLoaded(this, ev);
				}
				else
				{
					WebBrowser::WebMessageReceivedArgs ev{ msg };
					OnWebMessageReceived(this, ev);
				}
				return S_OK;
			}).Get(),
		&_webMessageToken);

	_interopInstalled = true;
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

void WebBrowser::Stop()
{
	if (_webview)
	{
		_webview->Stop();
	}
}

void WebBrowser::GoBack()
{
	if (_webview)
	{
		_webview->GoBack();
	}
}

void WebBrowser::GoForward()
{
	if (_webview)
	{
		_webview->GoForward();
	}
}

bool WebBrowser::CanGoBack() const
{
	if (!_webview) return false;
	BOOL v = FALSE;
	_webview->get_CanGoBack(&v);
	return v != FALSE;
}

bool WebBrowser::CanGoForward() const
{
	if (!_webview) return false;
	BOOL v = FALSE;
	_webview->get_CanGoForward(&v);
	return v != FALSE;
}

std::wstring WebBrowser::GetSource() const
{
	if (_webview)
	{
		LPWSTR raw = nullptr;
		if (SUCCEEDED(_webview->get_Source(&raw)) && raw)
		{
			std::wstring s = raw;
			CoTaskMemFree(raw);
			return s;
		}
	}
	return _cachedSource;
}

std::wstring WebBrowser::GetDocumentTitle() const
{
	if (_webview)
	{
		LPWSTR raw = nullptr;
		if (SUCCEEDED(_webview->get_DocumentTitle(&raw)) && raw)
		{
			std::wstring s = raw;
			CoTaskMemFree(raw);
			return s;
		}
	}
	return _cachedTitle;
}

double WebBrowser::GetZoomFactor() const
{
	if (!_controller) return 1.0;
	double f = 1.0;
	_controller->get_ZoomFactor(&f);
	return f;
}

void WebBrowser::SetZoomFactor(double factor)
{
	if (_controller)
	{
		_controller->put_ZoomFactor(factor);
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
