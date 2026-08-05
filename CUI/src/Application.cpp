#include "Application.h"

#include "Core/Threading.h"
#include "EventInfrastructure.h"
#include "Resource.h"
#include "Style.h"
#include "Window.h"

#include <Shellapi.h>

#include <algorithm>
#include <atomic>
#include <exception>
#include <filesystem>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <utility>

#pragma comment(lib, "Shell32.lib")

namespace
{
	struct PlatformWindowRegistration final
	{
		Window* WindowPointer = nullptr;
		Application* Owner = nullptr;
	};

	std::mutex applicationLifetimeMutex;
	std::atomic<Application*> currentApplication{ nullptr };
	bool applicationCreated = false;

	std::mutex platformWindowsMutex;
	std::unordered_map<HWND, PlatformWindowRegistration> platformWindows;

	std::mutex resourceResolverMutex;
	std::shared_ptr<const ResourceResolver> resourceResolver;

	std::wstring ExecutableDirectoryW()
	{
		std::wstring path(32768, L'\0');
		const DWORD length = GetModuleFileNameW(
			nullptr, path.data(), static_cast<DWORD>(path.size()));
		if (!length || length >= path.size())
			return std::filesystem::current_path().wstring();
		path.resize(length);
		return std::filesystem::path(path).parent_path().wstring();
	}

	std::shared_ptr<const ResourceResolver> CreateDefaultResourceResolver()
	{
		auto resolver = std::make_shared<ResourceResolver>();
		std::vector<std::wstring> roots{ ExecutableDirectoryW() };
		const auto current = std::filesystem::current_path().wstring();
		if (_wcsicmp(roots.front().c_str(), current.c_str()) != 0)
			roots.push_back(current);
		resolver->AddSource(
			std::make_shared<FileResourceSource>(std::move(roots)));
		return resolver;
	}

	std::vector<std::wstring> CommandLineArguments()
	{
		int count = 0;
		auto** values = ::CommandLineToArgvW(::GetCommandLineW(), &count);
		if (!values) return {};
		std::vector<std::wstring> result;
		if (count > 1)
		{
			result.reserve(static_cast<size_t>(count - 1));
			for (int index = 1; index < count; ++index)
				result.emplace_back(values[index] ? values[index] : L"");
		}
		::LocalFree(values);
		return result;
	}

	UINT GetSystemDpiFallback()
	{
		HDC hdc = GetDC(nullptr);
		if (!hdc) return 96;
		const int dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
		ReleaseDC(nullptr, hdc);
		return dpiX > 0 ? static_cast<UINT>(dpiX) : 96;
	}

	UINT QueryDpiForWindow(HWND hwnd)
	{
		if (auto user32 = GetModuleHandleW(L"user32.dll"))
		{
			using GetDpiForWindowFunction = UINT(WINAPI*)(HWND);
			auto getDpiForWindow =
				reinterpret_cast<GetDpiForWindowFunction>(
					GetProcAddress(user32, "GetDpiForWindow"));
			if (getDpiForWindow && hwnd)
			{
				const UINT dpi = getDpiForWindow(hwnd);
				if (dpi >= 96) return dpi;
			}

			using GetDpiForSystemFunction = UINT(WINAPI*)();
			auto getDpiForSystem =
				reinterpret_cast<GetDpiForSystemFunction>(
					GetProcAddress(user32, "GetDpiForSystem"));
			if (getDpiForSystem)
			{
				const UINT dpi = getDpiForSystem();
				if (dpi >= 96) return dpi;
			}
		}
		return GetSystemDpiFallback();
	}

	void EnableDpiAwarenessOnce()
	{
		static bool dpiAwarenessConfigured = false;
		if (dpiAwarenessConfigured) return;
		dpiAwarenessConfigured = true;

		if (auto user32 = GetModuleHandleW(L"user32.dll"))
		{
			using SetProcessDpiAwarenessContextFunction =
				BOOL(WINAPI*)(HANDLE);
			auto setContext =
				reinterpret_cast<SetProcessDpiAwarenessContextFunction>(
					GetProcAddress(
						user32, "SetProcessDpiAwarenessContext"));
			if (setContext)
			{
				if (setContext(reinterpret_cast<HANDLE>(-4))) return;
				if (setContext(reinterpret_cast<HANDLE>(-3))) return;
			}
		}

		if (auto shcore = LoadLibraryW(L"Shcore.dll"))
		{
			using SetProcessDpiAwarenessFunction = HRESULT(WINAPI*)(int);
			auto setAwareness =
				reinterpret_cast<SetProcessDpiAwarenessFunction>(
					GetProcAddress(shcore, "SetProcessDpiAwareness"));
			if (setAwareness && SUCCEEDED(setAwareness(2)))
			{
				FreeLibrary(shcore);
				return;
			}
			FreeLibrary(shcore);
		}

		if (auto user32 = GetModuleHandleW(L"user32.dll"))
		{
			using SetProcessDpiAwareFunction = BOOL(WINAPI*)();
			auto setAware =
				reinterpret_cast<SetProcessDpiAwareFunction>(
					GetProcAddress(user32, "SetProcessDPIAware"));
			if (setAware) setAware();
		}
	}

	bool IsValidShutdownMode(::ShutdownMode value) noexcept
	{
		return value == ::ShutdownMode::OnLastWindowClose
			|| value == ::ShutdownMode::OnMainWindowClose
			|| value == ::ShutdownMode::OnExplicitShutdown;
	}
}

Application::Application()
{
	std::scoped_lock lock(applicationLifetimeMutex);
	if (applicationCreated)
		throw std::logic_error(
			"Only one Application can be created in this process");
	applicationCreated = true;
	currentApplication.store(this, std::memory_order_release);
}

Application::~Application()
{
	if (!_shutdownCompleted && CheckAccess())
	{
		try
		{
			RequestShutdown(_exitCode);
			CompleteShutdown();
		}
		catch (...)
		{
			// Destruction must still detach the singleton even when an Exit
			// handler throws.
		}
	}
#if CUI_ENABLE_DYNAMIC_XAML
	_resourcesConnection.Disconnect();
#endif
	{
		std::scoped_lock lock(platformWindowsMutex);
		for (auto& [handle, registration] : platformWindows)
		{
			(void)handle;
			if (registration.Owner == this)
				registration.Owner = nullptr;
		}
	}
	Application* expected = this;
	(void)currentApplication.compare_exchange_strong(
		expected, nullptr, std::memory_order_acq_rel);
}

Application* Application::Current() noexcept
{
	return currentApplication.load(std::memory_order_acquire);
}

void Application::RegisterWindow(Window& window)
{
	const HWND handle = window.Handle;
	if (!handle) return;

	auto* owner = Current();
	if (owner && (owner->DispatcherThreadId() != window.DispatcherThreadId()
		|| owner->_isShuttingDown))
		owner = nullptr;
	{
		std::scoped_lock lock(platformWindowsMutex);
		platformWindows[handle] = { &window, owner };
	}
	if (owner) owner->RegisterApplicationWindow(window);
}

void Application::UnregisterWindow(Window& window) noexcept
{
	Application* owner = nullptr;
	{
		std::scoped_lock lock(platformWindowsMutex);
		auto found = platformWindows.find(window.Handle);
		if (found == platformWindows.end())
		{
			found = std::find_if(
				platformWindows.begin(), platformWindows.end(),
				[&window](const auto& entry)
				{
					return entry.second.WindowPointer == &window;
				});
		}
		if (found != platformWindows.end())
		{
			owner = found->second.Owner;
			platformWindows.erase(found);
		}
	}
	if (owner) owner->UnregisterApplicationWindow(window);
}

void Application::RegisterApplicationWindow(Window& window)
{
	if (std::find(_windows.begin(), _windows.end(), &window)
		!= _windows.end()) return;
	_windows.push_back(&window);
	if (!_mainWindow) _mainWindow = &window;
	window.RebuildStyleSubscriptions(false);
	(void)window.RefreshDynamicResourceValues(false);
	(void)window.RefreshStyleValues(false);
}

void Application::UnregisterApplicationWindow(Window& window) noexcept
{
	const bool wasMainWindow = _mainWindow == &window;
	_windows.erase(
		std::remove(_windows.begin(), _windows.end(), &window),
		_windows.end());

	const bool shouldShutdown = !_isShuttingDown
		&& ((_shutdownMode == ::ShutdownMode::OnLastWindowClose
				&& _windows.empty())
			|| (_shutdownMode == ::ShutdownMode::OnMainWindowClose
				&& wasMainWindow));
	if (wasMainWindow) _mainWindow = nullptr;
	if (shouldShutdown) RequestShutdown(0);
}

void Application::AdoptWindow(Window& window)
{
	VerifyAccess();
	if (!window.CheckAccess())
		throw std::invalid_argument(
			"Application.Run Window must belong to the Application thread");
	RegisterApplicationWindow(window);
	if (!window.Handle) return;

	std::scoped_lock lock(platformWindowsMutex);
	auto found = platformWindows.find(window.Handle);
	if (found == platformWindows.end())
	{
		platformWindows[window.Handle] = { &window, this };
		return;
	}
	if (found->second.Owner && found->second.Owner != this)
		throw std::logic_error("Window already belongs to another Application");
	found->second.WindowPointer = &window;
	found->second.Owner = this;
}

std::vector<Window*> Application::GetWindows() const
{
	VerifyAccess();
	return _windows;
}

std::vector<Window*> Application::GetPlatformWindows()
{
	std::vector<Window*> result;
	std::scoped_lock lock(platformWindowsMutex);
	result.reserve(platformWindows.size());
	for (const auto& [handle, registration] : platformWindows)
		if (handle && registration.WindowPointer)
			result.push_back(registration.WindowPointer);
	return result;
}

std::vector<HWND> Application::GetPlatformWindowHandles()
{
	std::vector<HWND> result;
	std::scoped_lock lock(platformWindowsMutex);
	result.reserve(platformWindows.size());
	for (const auto& [handle, registration] : platformWindows)
		if (handle && registration.WindowPointer) result.push_back(handle);
	return result;
}

Window* Application::FindWindow(HWND handle) noexcept
{
	std::scoped_lock lock(platformWindowsMutex);
	const auto found = platformWindows.find(handle);
	return found == platformWindows.end()
		? nullptr : found->second.WindowPointer;
}

bool Application::IsWindowClosingForShutdown(
	const Window& window) noexcept
{
	std::scoped_lock lock(platformWindowsMutex);
	const auto found = std::find_if(
		platformWindows.begin(), platformWindows.end(),
		[&window](const auto& entry)
		{
			return entry.second.WindowPointer == &window;
		});
	return found != platformWindows.end()
		&& found->second.Owner
		&& found->second.Owner->_isShuttingDown;
}

Window* Application::GetMainWindow() const
{
	VerifyAccess();
	return _mainWindow;
}

void Application::SetMainWindow(Window* value)
{
	VerifyAccess();
	_mainWindow = value;
}

::ShutdownMode Application::GetShutdownMode() const
{
	VerifyAccess();
	return _shutdownMode;
}

void Application::SetShutdownMode(::ShutdownMode value)
{
	VerifyAccess();
	if (!IsValidShutdownMode(value))
		throw std::invalid_argument("Invalid Application ShutdownMode");
	if (_isShuttingDown || _shutdownCompleted)
		throw std::logic_error(
			"Application ShutdownMode cannot change during shutdown");
	_shutdownMode = value;
}

#if CUI_ENABLE_DYNAMIC_XAML
void Application::ConnectResources(
	const std::shared_ptr<ControlStyleSheet>& resources)
{
	_resourcesConnection.Disconnect();
	if (!resources) return;
	const auto lifetime = WeakLifetimeToken();
	_resourcesConnection = resources->SubscribeChanged(
		[this, lifetime]()
		{
			const auto token = lifetime.lock();
			if (!token || !*token) return;
			OnResourcesChanged();
		});
}

std::shared_ptr<ControlStyleSheet> Application::GetResources()
{
	std::scoped_lock lock(_resourcesMutex);
	if (!_resources)
	{
		_resources = std::make_shared<ControlStyleSheet>();
		ConnectResources(_resources);
	}
	return _resources;
}

void Application::SetResources(
	std::shared_ptr<ControlStyleSheet> value)
{
	bool changed = false;
	{
		std::scoped_lock lock(_resourcesMutex);
		if (_resources == value) return;
		_resources = std::move(value);
		ConnectResources(_resources);
		changed = true;
	}
	if (changed) OnResourcesChanged();
}
#else
std::shared_ptr<const ControlStyleSheet> Application::GetResources() const
{
	std::scoped_lock lock(_resourcesMutex);
	return _resources;
}

void Application::SetResources(
	std::shared_ptr<const ControlStyleSheet> value)
{
	bool changed = false;
	{
		std::scoped_lock lock(_resourcesMutex);
		if (_resources == value) return;
		_resources = std::move(value);
		changed = true;
	}
	if (changed) OnResourcesChanged();
}
#endif

std::shared_ptr<const ControlStyleSheet>
Application::GetResourcesSnapshot() const
{
	std::scoped_lock lock(_resourcesMutex);
	return _resources;
}

bool Application::TryFindResource(
	const std::wstring& resourceKey,
	BindingValue& value) const
{
	if (resourceKey.empty()) return false;
	const auto resources = GetResourcesSnapshot();
	return resources
		&& resources->TryGetResource(resourceKey, value);
}

BindingValue Application::FindResource(
	const std::wstring& resourceKey) const
{
	BindingValue result;
	if (TryFindResource(resourceKey, result)) return result;
	throw std::out_of_range("Application resource key was not found");
}

void Application::OnResourcesChanged()
{
	if (CheckAccess())
	{
		if (_isShuttingDown || _shutdownCompleted) return;
		InvalidateApplicationResources();
		return;
	}

	const auto lifetime = WeakLifetimeToken();
	(void)TryPost(
		[this, lifetime]()
		{
			const auto token = lifetime.lock();
			if (!token || !*token || _isShuttingDown
				|| _shutdownCompleted) return;
			InvalidateApplicationResources();
		});
}

void Application::InvalidateApplicationResources()
{
	VerifyAccess();
	const auto windows = _windows;
	for (auto* window : windows)
	{
		if (!window) continue;
		window->RebuildStyleSubscriptions(true);
		(void)window->RefreshDynamicResourceValues(true);
		(void)window->RefreshStyleValues(true);
	}
}

std::shared_ptr<const ResourceResolver>
Application::GetResourceResolver()
{
	std::scoped_lock lock(resourceResolverMutex);
	if (!resourceResolver)
		resourceResolver = CreateDefaultResourceResolver();
	return resourceResolver;
}

void Application::SetResourceResolver(
	std::shared_ptr<const ResourceResolver> resolver)
{
	std::scoped_lock lock(resourceResolverMutex);
	resourceResolver = resolver ? std::move(resolver)
		: CreateDefaultResourceResolver();
}

void Application::ConfigureResourceDirectories(
	const std::vector<std::wstring>& directories)
{
	auto resolver = std::make_shared<ResourceResolver>();
	resolver->AddSource(
		std::make_shared<FileResourceSource>(directories));
	SetResourceResolver(std::move(resolver));
}

void Application::ResetResourceResolver()
{
	std::scoped_lock lock(resourceResolverMutex);
	resourceResolver = CreateDefaultResourceResolver();
}

void Application::EnsureDpiAwareness()
{
	EnableDpiAwarenessOnce();
}

UINT Application::GetSystemDpi()
{
	return QueryDpiForWindow(nullptr);
}

UINT Application::GetDpiForWindow(HWND hwnd)
{
	return QueryDpiForWindow(hwnd);
}

int Application::ScaleInt(int value, UINT fromDpi, UINT toDpi)
{
	if (fromDpi == 0) fromDpi = 96;
	if (toDpi == 0) toDpi = 96;
	if (fromDpi == toDpi) return value;
	return MulDiv(value, static_cast<int>(toDpi),
		static_cast<int>(fromDpi));
}

float Application::ScaleFloat(
	float value, UINT fromDpi, UINT toDpi)
{
	if (fromDpi == 0) fromDpi = 96;
	if (toDpi == 0) toDpi = 96;
	if (fromDpi == toDpi) return value;
	return value * (static_cast<float>(toDpi)
		/ static_cast<float>(fromDpi));
}

SystemVisualPreferences Application::NormalizeSystemVisualPreferences(
	SystemVisualPreferences value) noexcept
{
	value.TextScalePercent =
		(std::clamp)(value.TextScalePercent, 100U, 225U);
	return value;
}

SystemVisualPreferences Application::QuerySystemVisualPreferences()
{
	SystemVisualPreferences result;
	HIGHCONTRASTW highContrast{};
	highContrast.cbSize = sizeof(highContrast);
	if (::SystemParametersInfoW(
		SPI_GETHIGHCONTRAST, sizeof(highContrast), &highContrast, 0))
	{
		result.HighContrast =
			(highContrast.dwFlags & HCF_HIGHCONTRASTON) != 0;
	}

	BOOL animationsEnabled = TRUE;
	if (::SystemParametersInfoW(
		SPI_GETCLIENTAREAANIMATION, 0, &animationsEnabled, 0))
		result.AnimationsEnabled = animationsEnabled != FALSE;

	BOOL keyboardCues = FALSE;
	if (::SystemParametersInfoW(
		SPI_GETKEYBOARDCUES, 0, &keyboardCues, 0))
		result.KeyboardCuesAlwaysVisible = keyboardCues != FALSE;

	DWORD textScale = 100;
	DWORD textScaleSize = sizeof(textScale);
	if (::RegGetValueW(
		HKEY_CURRENT_USER,
		L"Software\\Microsoft\\Accessibility",
		L"TextScaleFactor",
		RRF_RT_REG_DWORD,
		nullptr,
		&textScale,
		&textScaleSize) == ERROR_SUCCESS)
	{
		result.TextScalePercent = static_cast<UINT>(textScale);
	}
	return NormalizeSystemVisualPreferences(result);
}

void Application::OnStartup(StartupEventArgs& args)
{
	VerifyAccess();
	cui::framework::EventAccess::Raise(Startup, this, args);
}

void Application::OnExit(ExitEventArgs& args)
{
	VerifyAccess();
	cui::framework::EventAccess::Raise(Exit, this, args);
}

void Application::RequestShutdown(int exitCode) noexcept
{
	if (_isShuttingDown || _shutdownCompleted) return;
	_exitCode = exitCode;
	_isShuttingDown = true;
}

void Application::CompleteShutdown()
{
	VerifyAccess();
	if (_shutdownCompleted) return;
	_isShuttingDown = true;

	while (!_windows.empty())
	{
		auto* window = _windows.front();
		const HWND handle = window ? window->Handle : nullptr;
		if (window && handle && ::IsWindow(handle))
			window->Close();

		const auto stillRegistered =
			std::find(_windows.begin(), _windows.end(), window);
		if (stillRegistered == _windows.end()) continue;
		if (handle && ::IsWindow(handle))
			(void)::DestroyWindow(handle);

		const auto stillPresent =
			std::find(_windows.begin(), _windows.end(), window);
		if (stillPresent == _windows.end()) continue;
		if (_mainWindow == window) _mainWindow = nullptr;
		_windows.erase(stillPresent);
		std::scoped_lock lock(platformWindowsMutex);
		for (auto& [registeredHandle, registration] : platformWindows)
			if (registration.WindowPointer == window
				&& registration.Owner == this)
				registration.Owner = nullptr;
	}

	ExitEventArgs args(_exitCode);
	std::exception_ptr exitFailure;
	try
	{
		OnExit(args);
	}
	catch (...)
	{
		exitFailure = std::current_exception();
	}
	_exitCode = args.ApplicationExitCode;
	_mainWindow = nullptr;
#if CUI_ENABLE_DYNAMIC_XAML
	_resourcesConnection.Disconnect();
#endif

	Application* expected = this;
	(void)currentApplication.compare_exchange_strong(
		expected, nullptr, std::memory_order_acq_rel);
	_shutdownCompleted = true;
	cui::ShutdownUIThreadDispatcher();

	if (exitFailure) std::rethrow_exception(exitFailure);
}

int Application::RunInternal(Window* window)
{
	VerifyAccess();
	if (_runStarted || _shutdownCompleted)
		throw std::logic_error(
			"Application.Run can be called only once");

	if (window)
	{
		AdoptWindow(*window);
		if (!_mainWindow) _mainWindow = window;
	}
	_runStarted = true;

	if (!_isShuttingDown && !_startupRaised)
	{
		_startupRaised = true;
		StartupEventArgs args;
		args.Args = CommandLineArguments();
		OnStartup(args);
	}

	if (!_isShuttingDown && window)
		window->Show();

	MSG message{};
	while (!_isShuttingDown)
	{
		const BOOL result = ::GetMessageW(&message, nullptr, 0, 0);
		if (result < 0)
		{
			RequestShutdown(-1);
			break;
		}
		if (result == 0)
		{
			RequestShutdown(static_cast<int>(message.wParam));
			break;
		}
		::TranslateMessage(&message);
		::DispatchMessageW(&message);
		cui::PumpUIThreadCallbacks();
	}

	CompleteShutdown();
	return _exitCode;
}

int Application::Run()
{
	return RunInternal(nullptr);
}

int Application::Run(Window& window)
{
	return RunInternal(&window);
}

void Application::Shutdown(int exitCode)
{
	VerifyAccess();
	RequestShutdown(exitCode);
}
