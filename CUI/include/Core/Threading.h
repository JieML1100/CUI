#pragma once

#include <cstdint>
#include <functional>
#include <memory>

/**
 * @file Threading.h
 * @brief CUI 的 UI 线程亲和与跨线程封送设施。
 *
 * 设计目标：
 * - 明确"哪个线程是 UI 线程"（第一个创建 Window / 初始化渲染的线程）。
 * - 提供 `PostToUIThread(...)` 封送原语，让任意工作线程把回调安全地交回 UI 线程执行。
 * - 提供 `IsUIThread()` 供调度器边界查询；对象访问统一由
 *   `DispatcherObject::CheckAccess/VerifyAccess` 约束。
 *
 * 实现说明：
 * - 封送底层依赖一个专用的、线程安全的隐藏消息窗口（ dispatcher window ），
 *   它由 UI 线程拥有并随消息泵驱动。工作线程向其 PostMessage，从而唤醒
 *   UI 线程的消息循环并执行入队的回调。
 * - 回调队列本身是加锁的 MPMC 容器；消息窗口只是"唤醒信号"，不承载数据。
 */

namespace cui
{
	/**
	 * @brief 将当前线程登记为 UI 线程，并建立封送所需的 dispatcher 窗口。
	 *
	 * 幂等。通常由 Window 构造 / 应用启动时自动调用，用户一般无需直接调用。
	 * 若已由另一线程登记，则本次调用忽略（首个登记线程胜出）。
	 */
	void InitializeUIThread();

	/** @brief 当前线程是否是已登记的 UI 线程。 */
	bool IsUIThread() noexcept;

	/** @brief 返回 UI 线程的 Win32 线程 ID；尚未登记时返回 0。 */
	std::uint32_t GetUIThreadId() noexcept;

	/**
	 * @brief 是否已经完成 UI 线程登记且 dispatcher 可用。
	 *
	 * 用于判断 `PostToUIThread` 是否能真正投递（应用尚未创建窗口/泵时可能为 false）。
	 */
	bool HasUIThreadDispatcher() noexcept;

	/**
	 * @brief Permanently terminates the process UI dispatcher for Application exit.
	 *
	 * Must run on the owning UI thread. Pending callbacks are discarded and
	 * later PostToUIThread calls fail instead of silently recreating a dispatcher
	 * after Application shutdown.
	 */
	void ShutdownUIThreadDispatcher() noexcept;

	/**
	 * @brief 将回调封送到 UI 线程异步执行。
	 *
	 * - 若当前已在 UI 线程，仍走队列（保证顺序一致、避免重入）。
	 * - 若 dispatcher 不可用（进程退出阶段），返回 false 且回调被丢弃。
	 * - 回调以 shared_ptr 包装，保证跨线程所有权安全。
	 *
	 * @return true 表示已成功入队并通知 UI 线程；false 表示无法投递。
	 */
	bool PostToUIThread(std::function<void()> fn);

	/**
	 * @brief 由消息泵在每个循环调用，排空并执行已封送的回调。
	 *
	 * 框架内部使用（Application::Run / dispatcher 窗口过程）。普通用户无需调用。
	 * 仅在 UI 线程上调用有效。
	 */
	void PumpUIThreadCallbacks();
}
