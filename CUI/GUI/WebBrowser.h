#pragma once
#include "Control.h"

#include <functional>
#include <string>

// WebView2 (NuGet: Microsoft.Web.WebView2)
#include <WebView2.h>
#include <wrl/client.h>

// 说明：
// - 旧实现通过 CapturePreview(PNG)->IStream->WIC 解码->D2D Bitmap 绘制，CPU 开销很大且交互易失效
// - 新实现改为“原生嵌入式 WebView2”：让 WebView2 直接在子窗口中 GPU 渲染
//   从而避免抓帧/解码链路，滚轮/拖动等交互也由系统消息自然驱动
class WebBrowser : public Control
{
public:
	WebBrowser(int x, int y, int width, int height);
	~WebBrowser() override;

	UIClass Type() override { return UIClass::UI_WebBrowser; }
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof) override;

	// 基础能力（类似 C# WebBrowser）
	void Navigate(const std::wstring& url);
	void SetHtml(const std::wstring& html);              // 等同 NavigateToString
	void Reload();

	// JS/DOM 能力：ExecuteScriptAsync 返回 JSON 编码字符串（WebView2 行为）
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

	static std::wstring JsStringLiteral(const std::wstring& s);

private:
	HWND _hostHwnd = NULL;
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
	Microsoft::WRL::ComPtr<ICoreWebView2> _webview;
};
