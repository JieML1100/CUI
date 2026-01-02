#pragma once
#include "Control.h"

#include <functional>
#include <string>

// WebView2 (NuGet: Microsoft.Web.WebView2)
#include <WebView2.h>
#include <wrl/client.h>

#if defined(_MSC_VER)
#pragma comment(lib, "Ole32.lib")
// WebView2 loader（需要调用方提供对应的 lib 搜索路径，通常通过 NuGet targets 解决）
#pragma comment(lib, "WebView2LoaderStatic.lib")
#endif

// Forward declarations for DirectComposition
struct IDCompositionVisual;
struct IDCompositionRectangleClip;

/**
 * @file WebBrowser.h
 * @brief WebBrowser：基于 WebView2 的浏览器控件（Composition 模式）。
 *
 * 关键点：
 * - 运行时使用 WebView2 CompositionController 并挂载到 Form 提供的 DirectComposition 容器层
 * - 设计器模式下不会创建真实 WebView2（避免生成 `<exe>.WebView2` 目录），仅绘制占位
 * - 鼠标输入需要显式转发给 WebView2（见 ForwardMouseMessageToWebView）
 * - Navigate/SetHtml 支持“延迟执行”：未就绪时会缓存到 _pendingUrl/_pendingHtml
 */

class WebBrowser : public Control
{
public:
	/** @brief 创建 WebBrowser。 */
	WebBrowser(int x, int y, int width, int height);
	~WebBrowser() override;

	UIClass Type() override { return UIClass::UI_WebBrowser; }
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof) override;

	// WebView2 Composition 光标支持：返回 system cursor id（可用于 LoadCursor(MAKEINTRESOURCE)）
	/**
	 * @brief 获取 WebView2 当前建议的系统光标 id。
	 * @return true 表示可用。
	 */
	bool TryGetSystemCursorId(UINT32& outId) const;

	/** @brief 导航到指定 URL（未就绪时会缓存）。 */
	void Navigate(const std::wstring& url);
	/** @brief 直接设置 HTML（NavigateToString，未就绪时会缓存）。 */
	void SetHtml(const std::wstring& html);
	/** @brief 重新加载当前页面。 */
	void Reload();

	/**
	 * @brief 异步执行脚本。
	 * @param script JS 脚本。
	 * @param callback 回调返回 HRESULT 与 JSON 字符串结果（WebView2 约定）。
	 */
	void ExecuteScriptAsync(const std::wstring& script,
		std::function<void(HRESULT hr, const std::wstring& jsonResult)> callback = {});

	/** @brief 异步获取当前页面 HTML（实现依赖注入的 JS）。 */
	void GetHtmlAsync(std::function<void(HRESULT hr, const std::wstring& html)> callback);
	/** @brief 异步设置匹配元素的 innerHTML。 */
	void SetElementInnerHtmlAsync(const std::wstring& cssSelector, const std::wstring& html,
		std::function<void(HRESULT hr)> callback = {});
	/** @brief 异步查询匹配元素的 outerHTML 列表（以 JSON 数组返回）。 */
	void QuerySelectorAllOuterHtmlAsync(const std::wstring& cssSelector,
		std::function<void(HRESULT hr, const std::wstring& jsonArray)> callback);

	// 是否可见且就绪
	/** @brief WebView 是否已创建且当前控件可见。 */
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
