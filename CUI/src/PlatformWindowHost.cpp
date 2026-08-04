#include "PlatformWindowHost.h"

#include <atomic>
#include <mutex>

namespace
{
	constexpr wchar_t PlatformWindowClassName[] = L"Cui.PlatformWindowHost";
	using NativeMessageHook = bool (*)(HWND, UINT, WPARAM, LPARAM);
	std::atomic<NativeMessageHook> nativeMessageHook{ nullptr };
}

PlatformWindowHost::~PlatformWindowHost()
{
	Destroy();
}

bool PlatformWindowHost::EnsureWindowClass()
{
	static std::once_flag once;
	static bool registered = false;
	std::call_once(once, []
	{
		WNDCLASSW descriptor{};
		descriptor.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
		descriptor.lpfnWndProc = WindowProcedure;
		descriptor.hInstance = ::GetModuleHandleW(nullptr);
		descriptor.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
		descriptor.lpszClassName = PlatformWindowClassName;
		registered = ::RegisterClassW(&descriptor) != 0
			|| ::GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
	});
	return registered;
}

bool PlatformWindowHost::Create(
	const std::wstring& title,
	POINT origin,
	SIZE size,
	MessageHandler handler)
{
	if (_handle || !handler || !EnsureWindowClass()) return false;
	_handler = std::move(handler);
	_dispatchEnabled = false;
	_handle = ::CreateWindowExW(
		0,
		PlatformWindowClassName,
		title.c_str(),
		WS_POPUP,
		origin.x,
		origin.y,
		size.cx,
		size.cy,
		nullptr,
		nullptr,
		::GetModuleHandleW(nullptr),
		this);
	if (!_handle)
	{
		_handler = {};
		return false;
	}
	_dispatchEnabled = true;
	return true;
}

void PlatformWindowHost::Destroy() noexcept
{
	auto* handle = _handle;
	_dispatchEnabled = false;
	_handler = {};
	_handle = nullptr;
	if (!handle) return;
	::SetWindowLongPtrW(handle, GWLP_USERDATA, 0);
	if (::IsWindow(handle)) (void)::DestroyWindow(handle);
}

bool PlatformWindowHost::InstallNativeMessageHook(
	NativeMessageHook hook) noexcept
{
	if (!hook) return false;
	auto expected = static_cast<NativeMessageHook>(nullptr);
	return nativeMessageHook.compare_exchange_strong(
		expected, hook,
		std::memory_order_release,
		std::memory_order_acquire)
		|| expected == hook;
}

LRESULT CALLBACK PlatformWindowHost::WindowProcedure(
	HWND window,
	UINT message,
	WPARAM wParam,
	LPARAM lParam)
{
	if (const auto hook = nativeMessageHook.load(std::memory_order_acquire);
		hook && hook(window, message, wParam, lParam)) return 0;

	auto* host = reinterpret_cast<PlatformWindowHost*>(
		::GetWindowLongPtrW(window, GWLP_USERDATA));
	if (message == WM_NCCREATE)
	{
		const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
		host = create
			? static_cast<PlatformWindowHost*>(create->lpCreateParams) : nullptr;
		if (host)
		{
			host->_handle = window;
			::SetWindowLongPtrW(
				window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(host));
		}
	}

	LRESULT result = 0;
	if (host && host->_dispatchEnabled && host->_handler)
		result = host->_handler(window, message, wParam, lParam);
	else
		result = ::DefWindowProcW(window, message, wParam, lParam);

	if (message == WM_NCDESTROY && host)
	{
		::SetWindowLongPtrW(window, GWLP_USERDATA, 0);
		host->_dispatchEnabled = false;
		host->_handler = {};
		host->_handle = nullptr;
	}
	return result;
}
