#pragma once
#include <Windows.h>
#include <string>
#include <unordered_map>
#include <memory>
#include <vector>

class ResourceResolver;

/** Windows accessibility and visual-effect preferences used by CUI rendering. */
struct SystemVisualPreferences
{
	bool HighContrast = false;
	bool AnimationsEnabled = true;
	bool KeyboardCuesAlwaysVisible = false;
	UINT TextScalePercent = 100;

	float TextScaleFactor() const noexcept
	{
		return static_cast<float>(TextScalePercent) / 100.0f;
	}
};
/**
 * @file Application.h
 * @brief CUI 应用级静态工具与全局状态。
 *
 * 该类不负责窗口渲染本身，而是提供：
 * - 窗口(Window)注册表
 * - 进程 Dispatcher 消息循环
 * - 应用级资源解析与平台视觉首选项
 */
class Application
{
private:
	friend class Window;
	static std::unordered_map<HWND, class Window*> _windows;
	static void RegisterWindow(class Window& window);
	static void UnregisterWindow(class Window& window) noexcept;
	static UINT GetSystemDpi();
	static UINT GetDpiForWindow(HWND hwnd);
	static int ScaleInt(int value, UINT fromDpi, UINT toDpi);
	static float ScaleFloat(float value, UINT fromDpi, UINT toDpi);

public:
	/** Read-only WPF-style snapshot of live application windows. */
	static std::vector<class Window*> GetWindows();
	/** Explicit HWND interop lookup without exposing the mutable registry. */
	static class Window* FindWindow(HWND handle) noexcept;

	// ---- Application resources ----
	/** Returns the configured resolver; defaults to file/directory sources. */
	static std::shared_ptr<const ResourceResolver> GetResourceResolver();
	/** Installs the resolver snapshot used by subsequent resource loads. */
	static void SetResourceResolver(std::shared_ptr<const ResourceResolver> resolver);
	/** Startup convenience for the built-in file resource source. */
	static void ConfigureResourceDirectories(
		const std::vector<std::wstring>& directories);
	/** Restores startup/current-directory file lookup. */
	static void ResetResourceResolver();

	// ---- DPI helpers ----
	/**
	 * @brief 尽可能启用 Per-Monitor V2 DPI Awareness（失败则自动降级）。
	 *
	 * 建议在创建任何窗口之前调用；Window 构造时也会兜底调用一次。
	 */
	static void EnsureDpiAwareness();
	/** Reads high contrast, client animation, keyboard cue and text scale settings. */
	static SystemVisualPreferences QuerySystemVisualPreferences();
	/** Clamps externally supplied preference snapshots to supported safe ranges. */
	static SystemVisualPreferences NormalizeSystemVisualPreferences(
		SystemVisualPreferences value) noexcept;

	/** Runs the process Dispatcher until the last registered window closes. */
	static int Run();

};
