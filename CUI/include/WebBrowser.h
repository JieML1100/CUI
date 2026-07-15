#pragma once
#include "Control.h"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

/**
 * @file WebBrowser.h
 * @brief WebBrowser：基于 WebView2 Composition 的浏览器控件。
 *
 * 公共类布局不依赖 CUI_ENABLE_WEBVIEW2。WebView2/COM/DComp 类型全部隐藏在
 * 实现对象中，因此应用、Designer 与测试无需包含 WebView2 SDK，也不会因宏不同
 * 产生 ABI 分裂。
 */
class WebBrowser : public Control
{
public:
	enum class InitializationState
	{
		NotStarted,
		Initializing,
		Ready,
		Failed,
		Unsupported
	};

	enum class PendingNavigationKind
	{
		None,
		Url,
		Html
	};

	struct NavigationStartingArgs
	{
		std::wstring Uri;
		bool IsUserInitiated = false;
		bool IsRedirected = false;
		bool Cancel = false;
	};

	struct NavigationCompletedArgs
	{
		std::wstring Uri;
		UINT64 NavigationId = 0;
		bool IsSuccess = false;
		bool IsHttpErrorStatus = false;
		int HttpStatusCode = 0;
		/** WebView2 COREWEBVIEW2_WEB_ERROR_STATUS 的稳定整数值。 */
		int WebErrorStatus = 0;
	};

	struct ContentLoadingArgs
	{
		UINT64 NavigationId = 0;
		bool IsErrorPage = false;
		bool IsInSameDocument = false;
	};

	struct DomContentLoadedArgs
	{
		UINT64 NavigationId = 0;
	};

	struct SourceChangedArgs
	{
		std::wstring Uri;
		bool IsNewDocument = false;
	};

	struct HistoryChangedArgs
	{
		bool CanGoBack = false;
		bool CanGoForward = false;
	};

	struct DocumentTitleChangedArgs
	{
		std::wstring Title;
	};

	struct NewWindowRequestedArgs
	{
		std::wstring Uri;
		bool IsUserInitiated = false;
		bool Handled = false;
	};

	struct ProcessFailedArgs
	{
		/** WebView2 COREWEBVIEW2_PROCESS_FAILED_KIND 的稳定整数值。 */
		int Kind = 0;
	};

	struct WebMessageReceivedArgs
	{
		std::wstring Message;
	};

	using NavigationStartingEvent = Event<void(
		class WebBrowser*, NavigationStartingArgs&)>;
	using NavigationCompletedEvent = Event<void(
		class WebBrowser*, const NavigationCompletedArgs&)>;
	using ContentLoadingEvent = Event<void(
		class WebBrowser*, const ContentLoadingArgs&)>;
	using DomContentLoadedEvent = Event<void(
		class WebBrowser*, const DomContentLoadedArgs&)>;
	using SourceChangedEvent = Event<void(
		class WebBrowser*, const SourceChangedArgs&)>;
	using HistoryChangedEvent = Event<void(
		class WebBrowser*, const HistoryChangedArgs&)>;
	using DocumentTitleChangedEvent = Event<void(
		class WebBrowser*, const DocumentTitleChangedArgs&)>;
	using NewWindowRequestedEvent = Event<void(
		class WebBrowser*, NewWindowRequestedArgs&)>;
	using ProcessFailedEvent = Event<void(
		class WebBrowser*, const ProcessFailedArgs&)>;
	using WebMessageReceivedEvent = Event<void(
		class WebBrowser*, const WebMessageReceivedArgs&)>;
	using JsInvokeHandler = std::function<std::wstring(
		const std::wstring& payload)>;

	WebBrowser(int x, int y, int width, int height);
	~WebBrowser() override;
	WebBrowser(const WebBrowser&) = delete;
	WebBrowser& operator=(const WebBrowser&) = delete;

	UIClass Type() override { return UIClass::UI_WebBrowser; }
	bool HandlesMouseWheel() const override { return true; }
	void EnsureBindingPropertiesRegistered() override;
	void Update() override;
	bool ProcessMessage(
		UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY) override;
	void SyncNativeSurface() override;
	bool TryGetSystemCursorId(UINT32& outId) const override;

	/** 请求初始化；异步请求已接受或已就绪时返回 true。 */
	bool TryInitialize();
	bool IsInitialized() const;
	bool IsWebViewReady() const;
	InitializationState GetInitializationState() const;
	HRESULT GetLastInitializationError() const;
	HRESULT GetLastEnvironmentError() const;
	HRESULT GetLastControllerError() const;
	HRESULT GetLastWebViewError() const;

	/** 导航已执行或成功排队时返回 true。 */
	bool TryNavigate(const std::wstring& url);
	bool TrySetHtml(const std::wstring& html);
	bool TryReload();
	bool TryStop();
	bool TryGoBack();
	bool TryGoForward();
	void Navigate(const std::wstring& url);
	void SetHtml(const std::wstring& html);
	void Reload();
	void Stop();
	void GoBack();
	void GoForward();

	bool CanGoBack() const;
	bool CanGoForward() const;
	std::wstring GetSource() const;
	std::wstring GetDocumentTitle() const;
	bool IsNavigating() const;
	bool IsWebViewVisible() const;

	bool HasPendingNavigation() const;
	PendingNavigationKind GetPendingNavigationKind() const;
	std::wstring GetPendingUrl() const;
	void ClearPendingNavigation();

	double GetZoomFactor() const;
	void SetZoomFactor(double factor);
	bool GetAreDefaultContextMenusEnabled() const;
	void SetAreDefaultContextMenusEnabled(bool value);
	bool GetIsStatusBarEnabled() const;
	void SetIsStatusBarEnabled(bool value);
	bool GetIsZoomControlEnabled() const;
	void SetIsZoomControlEnabled(bool value);
	std::wstring GetInitialUrl() const;
	void SetInitialUrl(std::wstring value);

	PROPERTY(double, ZoomFactor);
	PROPERTY(bool, AreDefaultContextMenusEnabled);
	PROPERTY(bool, IsStatusBarEnabled);
	PROPERTY(bool, IsZoomControlEnabled);
	PROPERTY(std::wstring, InitialUrl);

	void ExecuteScriptAsync(
		const std::wstring& script,
		std::function<void(HRESULT hr, const std::wstring& jsonResult)> callback = {});
	void RegisterJsInvokeHandler(
		const std::wstring& name, JsInvokeHandler handler);
	void UnregisterJsInvokeHandler(const std::wstring& name);
	void ClearJsInvokeHandlers();
	void GetHtmlAsync(
		std::function<void(HRESULT hr, const std::wstring& html)> callback);
	void SetElementInnerHtmlAsync(
		const std::wstring& cssSelector,
		const std::wstring& html,
		std::function<void(HRESULT hr)> callback = {});
	void QuerySelectorAllOuterHtmlAsync(
		const std::wstring& cssSelector,
		std::function<void(HRESULT hr, const std::wstring& jsonArray)> callback);

	NavigationStartingEvent OnNavigationStarting;
	NavigationCompletedEvent OnNavigationCompleted;
	NavigationCompletedEvent OnNavigationFailed;
	ContentLoadingEvent OnContentLoading;
	DomContentLoadedEvent OnDOMContentLoaded;
	SourceChangedEvent OnSourceChanged;
	HistoryChangedEvent OnHistoryChanged;
	DocumentTitleChangedEvent OnDocumentTitleChanged;
	NewWindowRequestedEvent OnNewWindowRequested;
	ProcessFailedEvent OnProcessFailed;
	WebMessageReceivedEvent OnWebMessageReceived;

private:
	struct Impl;
	std::unique_ptr<Impl> _impl;

	void EnsureInitialized();
	void EnsureInteropInstalled();
	void EnsureControllerBounds();
	void ApplyWebViewSettings();
	bool ForwardMouseMessageToWebView(
		UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY);

	static std::wstring JsStringLiteral(const std::wstring& value);
	static std::wstring UrlEncodeUtf8(const std::wstring& value);
	static std::wstring UrlDecodeUtf8(const std::wstring& value);
	static bool TryParseCuiUrl(
		const std::wstring& url,
		std::wstring& outAction,
		std::unordered_map<std::wstring, std::wstring>& outQuery);
};
