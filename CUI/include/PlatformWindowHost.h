#pragma once

#include <Windows.h>

#include <functional>
#include <string>

class NotifyIcon;

/**
 * Owns the Win32 HWND boundary for one CUI window.
 *
 * The host knows nothing about controls, layout, resources or rendering. Its
 * only responsibilities are native class registration, HWND lifetime and
 * forwarding platform messages to the retained Window-side callback.
 */
class PlatformWindowHost final
{
public:
	using MessageHandler = std::function<LRESULT(
		HWND, UINT, WPARAM, LPARAM)>;

	PlatformWindowHost() = default;
	~PlatformWindowHost();
	PlatformWindowHost(const PlatformWindowHost&) = delete;
	PlatformWindowHost& operator=(const PlatformWindowHost&) = delete;
	PlatformWindowHost(PlatformWindowHost&&) = delete;
	PlatformWindowHost& operator=(PlatformWindowHost&&) = delete;

	bool Create(
		const std::wstring& title,
		POINT origin,
		SIZE size,
		MessageHandler handler);
	void Destroy() noexcept;

	HWND NativeHandle() const noexcept { return _handle; }
	bool IsCreated() const noexcept
	{
		return _handle != nullptr && ::IsWindow(_handle);
	}

private:
	using NativeMessageHook = bool (*)(
		HWND, UINT, WPARAM, LPARAM);

	friend class NotifyIcon;
	/**
	 * Installs the process-wide optional native-message projection. Repeating
	 * the same installation is harmless; a competing hook is rejected.
	 */
	static bool InstallNativeMessageHook(
		NativeMessageHook hook) noexcept;

	HWND _handle = nullptr;
	MessageHandler _handler;
	bool _dispatchEnabled = false;

	static bool EnsureWindowClass();
	static LRESULT CALLBACK WindowProcedure(
		HWND window, UINT message, WPARAM wParam, LPARAM lParam);
};
