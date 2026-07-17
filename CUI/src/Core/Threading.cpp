#include "Core/Threading.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <atomic>
#include <cassert>
#include <deque>
#include <mutex>

namespace cui
{
namespace
{
	// 自定义消息：通知 dispatcher 窗口"回调队列非空，请排空"。
	constexpr UINT kMsgPumpCallbacks = WM_APP + 0x51;
	constexpr wchar_t kDispatcherClassName[] = L"CUI.UIThreadDispatcher";

	std::atomic<std::uint32_t> g_uiThreadId{ 0 };
	std::atomic<HWND> g_dispatcherHwnd{ nullptr };

	std::mutex g_queueMutex;
	std::deque<std::function<void()>> g_callbackQueue;
	// 防止对同一空队列重复 PostMessage 的风暴。
	std::atomic<bool> g_pumpPosted{ false };

	std::deque<std::function<void()>> StealCallbacks()
	{
		std::deque<std::function<void()>> local;
		{
			std::lock_guard<std::mutex> lock(g_queueMutex);
			local.swap(g_callbackQueue);
			g_pumpPosted.store(false, std::memory_order_relaxed);
		}
		return local;
	}

	void DrainCallbacks()
	{
		auto local = StealCallbacks();
		for (auto& fn : local)
		{
			if (!fn) continue;
			try
			{
				fn();
			}
			catch (...)
			{
				// 回调异常不应导致消息泵崩溃；吞掉以保持泵存活。
			}
		}
	}

	LRESULT CALLBACK DispatcherWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		if (msg == kMsgPumpCallbacks)
		{
			DrainCallbacks();
			return 0;
		}
		if (msg == WM_DESTROY)
		{
			g_dispatcherHwnd.store(nullptr, std::memory_order_release);
		}
		return DefWindowProcW(hwnd, msg, wParam, lParam);
	}

	HWND CreateDispatcherWindow()
	{
		static std::atomic<bool> s_classRegistered{ false };
		bool expected = false;
		if (s_classRegistered.compare_exchange_strong(expected, true))
		{
			WNDCLASSEXW wc{};
			wc.cbSize = sizeof(wc);
			wc.lpfnWndProc = &DispatcherWndProc;
			wc.hInstance = GetModuleHandleW(nullptr);
			wc.lpszClassName = kDispatcherClassName;
			if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
			{
				s_classRegistered.store(false);
				return nullptr;
			}
		}

		// HWND_MESSAGE：纯消息窗口，不参与绘制与 Z 序。
		HWND hwnd = CreateWindowExW(
			0, kDispatcherClassName, L"", 0,
			0, 0, 0, 0,
			HWND_MESSAGE, nullptr, GetModuleHandleW(nullptr), nullptr);
		return hwnd;
	}
}

void InitializeUIThread()
{
	const std::uint32_t current = static_cast<std::uint32_t>(::GetCurrentThreadId());
	std::uint32_t expected = 0;
	if (!g_uiThreadId.compare_exchange_strong(expected, current))
	{
		// 已有线程登记；若就是本线程则补建 dispatcher，否则忽略。
		if (expected != current) return;
	}

	if (g_dispatcherHwnd.load(std::memory_order_acquire) == nullptr)
	{
		HWND hwnd = CreateDispatcherWindow();
		if (hwnd)
		{
			g_dispatcherHwnd.store(hwnd, std::memory_order_release);
		}
	}
}

bool IsUIThread() noexcept
{
	const std::uint32_t id = g_uiThreadId.load(std::memory_order_acquire);
	return id != 0 && id == static_cast<std::uint32_t>(::GetCurrentThreadId());
}

std::uint32_t GetUIThreadId() noexcept
{
	return g_uiThreadId.load(std::memory_order_acquire);
}

bool HasUIThreadDispatcher() noexcept
{
	return g_dispatcherHwnd.load(std::memory_order_acquire) != nullptr;
}

void AssertUIThread(const char* reason) noexcept
{
#if defined(_DEBUG)
	if (!IsUIThread())
	{
		// 输出诊断后触发断言；仅在 UI 线程已登记时才强制。
		if (g_uiThreadId.load(std::memory_order_acquire) != 0)
		{
			char buffer[160]{};
			_snprintf_s(buffer, _TRUNCATE,
				"CUI: cross-thread UI access detected (%s)",
				reason ? reason : "no reason given");
			OutputDebugStringA(buffer);
			OutputDebugStringA("\n");
			assert(false && "CUI UI-thread affinity violated");
		}
	}
#else
	(void)reason;
#endif
}

bool PostToUIThread(std::function<void()> fn)
{
	if (!fn) return false;

	// 若尚无 dispatcher（如应用尚未创建窗口），尝试惰性登记当前线程。
	if (!HasUIThreadDispatcher())
	{
		InitializeUIThread();
	}

	HWND hwnd = g_dispatcherHwnd.load(std::memory_order_acquire);
	if (hwnd == nullptr) return false;

	{
		std::lock_guard<std::mutex> lock(g_queueMutex);
		g_callbackQueue.push_back(std::move(fn));
	}

	bool expected = false;
	if (g_pumpPosted.compare_exchange_strong(expected, true, std::memory_order_relaxed))
	{
		if (!::PostMessageW(hwnd, kMsgPumpCallbacks, 0, 0))
		{
			g_pumpPosted.store(false, std::memory_order_relaxed);
			return false;
		}
	}
	return true;
}

void InvokeOnUIThread(std::function<void()> fn)
{
	if (!fn) return;
	if (IsUIThread())
	{
		fn();
		return;
	}
	PostToUIThread(std::move(fn));
}

void PumpUIThreadCallbacks()
{
	if (!IsUIThread()) return;
	DrainCallbacks();
}
}
