#include "Application.h"

#include <windows.h>

namespace
{
	static UINT GetSystemDpiFallback()
	{
		HDC hdc = GetDC(NULL);
		if (!hdc) return 96;
		const int dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
		ReleaseDC(NULL, hdc);
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
		static bool done = false;
		if (done) return;
		done = true;

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

Dictionary<HWND, class Form*>  Application::Forms = Dictionary<HWND, class Form*>();

std::string Application::ExecutablePath()
{
	char path[MAX_PATH];
	GetModuleFileNameA(NULL, path, MAX_PATH);
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
	SHGetSpecialFolderPathA(NULL, path, CSIDL_LOCAL_APPDATA, FALSE);
	return std::string(path);
}
std::string Application::UserAppDataPath()
{
	char path[MAX_PATH];
	SHGetSpecialFolderPathA(NULL, path, CSIDL_APPDATA, FALSE);
	return std::string(path);
}
RegistryKey Application::UserAppDataRegistry()
{
	return RegistryKey(HKEY_CURRENT_USER, StringHelper::Format("Software\\%s", ApplicationName().c_str()));
} 

void Application::EnsureDpiAwareness()
{
	EnableDpiAwarenessOnce();
}

UINT Application::GetSystemDpi()
{
	return QueryDpiForWindow(NULL);
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