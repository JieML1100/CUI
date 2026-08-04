#pragma once

#include <Windows.h>

#include "CuiBuildFeatures.h"
#include "DispatcherObject.h"
#include "Event.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

class Control;
class ControlStyleSheet;
class ResourceResolver;
class Window;

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

/** WPF-compatible policy controlling when an Application shuts down. */
enum class ShutdownMode : std::uint8_t
{
	OnLastWindowClose = 0,
	OnMainWindowClose = 1,
	OnExplicitShutdown = 2,
};

/** Arguments raised once when the Application dispatcher starts. */
struct StartupEventArgs final : EventArgs
{
	std::vector<std::wstring> Args;
};

/** Mutable process exit code carried by Application.Exit. */
struct ExitEventArgs final : EventArgs
{
	explicit ExitEventArgs(int exitCode = 0)
		: ApplicationExitCode(exitCode) {}

	int ApplicationExitCode = 0;
};

using ApplicationStartupEvent =
	Event<void(class Application*, StartupEventArgs&)>;
using ApplicationExitEvent =
	Event<void(class Application*, ExitEventArgs&)>;

/**
 * WPF-shaped application lifetime owner.
 *
 * Application owns the dispatcher run boundary, its Window collection,
 * MainWindow, shutdown policy and application ResourceDictionary projection.
 * Process-wide HWND lookup, DPI queries and the default URI resolver remain
 * platform services and are intentionally kept behind static interop methods.
 */
class Application : public DispatcherObject
{
private:
	friend class Control;
	friend class Window;

	static void RegisterWindow(Window& window);
	static void UnregisterWindow(Window& window) noexcept;
	static bool IsWindowClosingForShutdown(
		const Window& window) noexcept;
	static std::vector<Window*> GetPlatformWindows();

	static UINT GetSystemDpi();
	static UINT GetDpiForWindow(HWND hwnd);
	static int ScaleInt(int value, UINT fromDpi, UINT toDpi);
	static float ScaleFloat(float value, UINT fromDpi, UINT toDpi);

	void RegisterApplicationWindow(Window& window);
	void UnregisterApplicationWindow(Window& window) noexcept;
	void AdoptWindow(Window& window);
	void RequestShutdown(int exitCode) noexcept;
	void CompleteShutdown();
	int RunInternal(Window* window);

	std::shared_ptr<const ControlStyleSheet>
		GetResourcesSnapshot() const;
#if CUI_ENABLE_DYNAMIC_XAML
	void ConnectResources(
		const std::shared_ptr<ControlStyleSheet>& resources);
#endif
	void OnResourcesChanged();
	void InvalidateApplicationResources();

	std::vector<Window*> _windows;
	Window* _mainWindow = nullptr;
	::ShutdownMode _shutdownMode = ::ShutdownMode::OnLastWindowClose;
	bool _runStarted = false;
	bool _startupRaised = false;
	bool _isShuttingDown = false;
	bool _shutdownCompleted = false;
	int _exitCode = 0;

	mutable std::mutex _resourcesMutex;
#if CUI_ENABLE_DYNAMIC_XAML
	std::shared_ptr<ControlStyleSheet> _resources;
	EventConnection _resourcesConnection;
#else
	std::shared_ptr<const ControlStyleSheet> _resources;
#endif

protected:
	virtual void OnStartup(StartupEventArgs& args);
	virtual void OnExit(ExitEventArgs& args);

public:
	Application();
	~Application() override;

	/** Returns the sole Application for this process, or null after shutdown. */
	static Application* Current() noexcept;

	/** Read-only snapshot of windows owned by this Application dispatcher. */
	std::vector<Window*> GetWindows() const;
	Window* GetMainWindow() const;
	void SetMainWindow(Window* value);

	::ShutdownMode GetShutdownMode() const;
	void SetShutdownMode(::ShutdownMode value);

#if CUI_ENABLE_DYNAMIC_XAML
	/** Design-runtime mutable ResourceDictionary projection. */
	std::shared_ptr<ControlStyleSheet> GetResources();
	void SetResources(std::shared_ptr<ControlStyleSheet> value);
#else
	/** Production application resources are installed as one immutable snapshot. */
	std::shared_ptr<const ControlStyleSheet> GetResources() const;
	void SetResources(std::shared_ptr<const ControlStyleSheet> value);
#endif
	bool TryFindResource(
		const std::wstring& resourceKey,
		BindingValue& value) const;
	BindingValue FindResource(const std::wstring& resourceKey) const;

	ApplicationStartupEvent Startup;
	ApplicationExitEvent Exit;

	// ---- Platform resource URI resolution ----
	/** Returns the configured resolver; defaults to file/directory sources. */
	static std::shared_ptr<const ResourceResolver> GetResourceResolver();
	/** Installs the resolver snapshot used by subsequent resource loads. */
	static void SetResourceResolver(
		std::shared_ptr<const ResourceResolver> resolver);
	/** Startup convenience for the built-in file resource source. */
	static void ConfigureResourceDirectories(
		const std::vector<std::wstring>& directories);
	/** Restores startup/current-directory file lookup. */
	static void ResetResourceResolver();

	// ---- Platform interop and DPI helpers ----
	/** Explicit HWND interop lookup without exposing the mutable registry. */
	static Window* FindWindow(HWND handle) noexcept;
	static void EnsureDpiAwareness();
	static SystemVisualPreferences QuerySystemVisualPreferences();
	static SystemVisualPreferences NormalizeSystemVisualPreferences(
		SystemVisualPreferences value) noexcept;

	/** Starts this Application's dispatcher. May be called only once. */
	int Run();
	/** Adopts, assigns and shows window before entering the dispatcher. */
	int Run(Window& window);
	/** Requests non-cancelable application shutdown with the supplied code. */
	void Shutdown(int exitCode = 0);

	bool IsShuttingDown() const noexcept { return _isShuttingDown; }
	bool IsShutdown() const noexcept { return _shutdownCompleted; }
};
