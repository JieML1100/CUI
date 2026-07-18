#include "Application.h"
#include <shlobj_core.h>
#include <algorithm>
#include "Resource.h"
#include <filesystem>
#include <mutex>
namespace
{
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
		resolver->AddSource(std::make_shared<FileResourceSource>(std::move(roots)));
		return resolver;
	}

	static UINT GetSystemDpiFallback()
	{
		HDC hdc = GetDC(nullptr);
		if (!hdc) return 96;
		const int dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
		ReleaseDC(nullptr, hdc);
		return (dpiX > 0) ? (UINT)dpiX : 96;
	}

	static UINT QueryDpiForWindow(HWND hwnd)
	{
		// Prefer Win10+ GetDpiForWindow
		auto user32 = GetModuleHandleW(L"user32.dll");
		if (user32)
		{
			typedef UINT(WINAPI* GetDpiForWindow_t)(HWND);
			auto pGetDpiForWindow = (GetDpiForWindow_t)GetProcAddress(user32, "GetDpiForWindow");
			if (pGetDpiForWindow && hwnd)
			{
				UINT dpi = pGetDpiForWindow(hwnd);
				if (dpi >= 96) return dpi;
			}

			typedef UINT(WINAPI* GetDpiForSystem_t)();
			auto pGetDpiForSystem = (GetDpiForSystem_t)GetProcAddress(user32, "GetDpiForSystem");
			if (pGetDpiForSystem)
			{
				UINT dpi = pGetDpiForSystem();
				if (dpi >= 96) return dpi;
			}
		}
		return GetSystemDpiFallback();
	}

	static void EnableDpiAwarenessOnce()
	{
		static bool dpiAwarenessConfigured = false;
		if (dpiAwarenessConfigured) return;
		dpiAwarenessConfigured = true;

		// 1) Win10+ Per-Monitor V2
		if (auto user32 = GetModuleHandleW(L"user32.dll"))
		{
			typedef BOOL(WINAPI* SetProcessDpiAwarenessContext_t)(HANDLE);
			auto pSetCtx = (SetProcessDpiAwarenessContext_t)GetProcAddress(user32, "SetProcessDpiAwarenessContext");
			if (pSetCtx)
			{
				// DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 = (HANDLE)-4
				HANDLE PMV2 = (HANDLE)-4;
				if (pSetCtx(PMV2)) return;
				// fallback to PER_MONITOR_AWARE = (HANDLE)-3
				HANDLE PMV1 = (HANDLE)-3;
				if (pSetCtx(PMV1)) return;
			}
		}

		// 2) Win8.1+ Shcore SetProcessDpiAwareness
		if (auto shcore = LoadLibraryW(L"Shcore.dll"))
		{
			typedef HRESULT(WINAPI* SetProcessDpiAwareness_t)(int);
			auto pSet = (SetProcessDpiAwareness_t)GetProcAddress(shcore, "SetProcessDpiAwareness");
			if (pSet)
			{
				// PROCESS_PER_MONITOR_DPI_AWARE = 2
				if (SUCCEEDED(pSet(2)))
				{
					FreeLibrary(shcore);
					return;
				}
			}
			FreeLibrary(shcore);
		}

		// 3) Vista+ system DPI aware
		if (auto user32 = GetModuleHandleW(L"user32.dll"))
		{
			typedef BOOL(WINAPI* SetProcessDPIAware_t)();
			auto pSet = (SetProcessDPIAware_t)GetProcAddress(user32, "SetProcessDPIAware");
			if (pSet)
				pSet();
		}
	}
}

std::unordered_map<HWND, class Form*>  Application::Forms = std::unordered_map<HWND, class Form*>();

std::string Application::ExecutablePath()
{
	char path[MAX_PATH];
	GetModuleFileNameA(nullptr, path, MAX_PATH);
	return std::string(path);
}
std::string Application::StartupPath()
{
	std::string path = ExecutablePath();
	return path.substr(0, path.find_last_of("\\"));
}
std::string Application::ApplicationName()
{
	std::string path = ExecutablePath();
	std::string exe = path.substr(path.find_last_of("\\") + 1);
	return exe.substr(0, exe.find_last_of("."));
}
std::string Application::LocalUserAppDataPath()
{
	char path[MAX_PATH];
	SHGetSpecialFolderPathA(nullptr, path, CSIDL_LOCAL_APPDATA, FALSE);
	return std::string(path);
}
std::string Application::UserAppDataPath()
{
	char path[MAX_PATH];
	SHGetSpecialFolderPathA(nullptr, path, CSIDL_APPDATA, FALSE);
	return std::string(path);
}

std::shared_ptr<const ResourceResolver> Application::GetResourceResolver()
{
	std::scoped_lock lock(resourceResolverMutex);
	if (!resourceResolver) resourceResolver = CreateDefaultResourceResolver();
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
	resolver->AddSource(std::make_shared<FileResourceSource>(directories));
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
	return MulDiv(value, (int)toDpi, (int)fromDpi);
}

float Application::ScaleFloat(float value, UINT fromDpi, UINT toDpi)
{
	if (fromDpi == 0) fromDpi = 96;
	if (toDpi == 0) toDpi = 96;
	if (fromDpi == toDpi) return value;
	return value * ((float)toDpi / (float)fromDpi);
}

SystemVisualPreferences Application::NormalizeSystemVisualPreferences(
	SystemVisualPreferences value) noexcept
{
	value.TextScalePercent = (std::clamp)(value.TextScalePercent, 100U, 225U);
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
		result.HighContrast = (highContrast.dwFlags & HCF_HIGHCONTRASTON) != 0;
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
	if (::RegGetValueW(HKEY_CURRENT_USER,
		L"Software\\Microsoft\\Accessibility", L"TextScaleFactor",
		RRF_RT_REG_DWORD, nullptr, &textScale, &textScaleSize) == ERROR_SUCCESS)
	{
		result.TextScalePercent = static_cast<UINT>(textScale);
	}
	return NormalizeSystemVisualPreferences(result);
}
