#pragma once
#include "Control.h"

#include <functional>
#include <string>

// WebView2 (NuGet: Microsoft.Web.WebView2)
#include <WebView2.h>
#include <wrl/client.h>

// Forward declarations for DirectComposition
struct IDCompositionVisual;
struct IDCompositionRectangleClip;

class WebBrowser : public Control
{
public:
	WebBrowser(int x, int y, int width, int height);
	~WebBrowser() override;

	UIClass Type() override { return UIClass::UI_WebBrowser; }
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof) override;

	// WebView2 Composition 光标支持：返回 system cursor id（可用于 LoadCursor(MAKEINTRESOURCE)）
	bool TryGetSystemCursorId(UINT32& outId) const;

	void Navigate(const std::wstring& url);
	void SetHtml(const std::wstring& html);
	void Reload();

	void ExecuteScriptAsync(const std::wstring& script,
		std::function<void(HRESULT hr, const std::wstring& jsonResult)> callback = {});

	void GetHtmlAsync(std::function<void(HRESULT hr, const std::wstring& html)> callback);
	void SetElementInnerHtmlAsync(const std::wstring& cssSelector, const std::wstring& html,
		std::function<void(HRESULT hr)> callback = {});
	void QuerySelectorAllOuterHtmlAsync(const std::wstring& cssSelector,
		std::function<void(HRESULT hr, const std::wstring& jsonArray)> callback);

	// 是否可见且就绪
	bool IsWebViewVisible() const { return _webviewReady && this->Visible; }

private:
	void EnsureInitialized();
	void EnsureControllerBounds();
	bool ForwardMouseMessageToWebView(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof);

	static std::wstring JsStringLiteral(const std::wstring& s);

private:
	bool _initialized = false;
	bool _webviewReady = false;
	HRESULT _lastInitHr = S_OK;
	HRESULT _lastControllerHr = S_OK;
	HRESULT _lastGetWebViewHr = S_OK;
	HRESULT _lastCoInitHr = S_OK;
	int _navCompletedCount = 0;

	std::wstring _pendingUrl;
	std::wstring _pendingHtml;

	Microsoft::WRL::ComPtr<ICoreWebView2Environment> _env;
	Microsoft::WRL::ComPtr<ICoreWebView2Controller> _controller;
	Microsoft::WRL::ComPtr<ICoreWebView2CompositionController> _compositionController;
	Microsoft::WRL::ComPtr<ICoreWebView2> _webview;

	// DirectComposition visual used as RootVisualTarget for this WebView
	Microsoft::WRL::ComPtr<IDCompositionVisual> _dcompVisual;
	Microsoft::WRL::ComPtr<IDCompositionRectangleClip> _dcompClip;

	// cursor
	UINT32 _lastSystemCursorId = 0;
	bool _hasSystemCursorId = false;
	EventRegistrationToken _cursorChangedToken{};

	// RootVisualTarget attach state (解决隐藏页残留显示)
	bool _rootAttached = false;
};
