#pragma once

#include <cstdint>
#include <functional>
#include <memory>

/**
 * @file Threading.h
 * @brief CUI 的 UI 线程亲和与跨线程封送设施。
 *
 * 设计目标：
 * - 明确"哪个线程是 UI 线程"（第一个创建 Form / 初始化渲染的线程）。
 * - 提供 `PostToUIThread(...)` 封送原语，让任意工作线程把回调安全地交回 UI 线程执行。
 * - 提供 `AssertUIThread()` / `IsUIThread()` 用于调试期防护。
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
	 * 幂等。通常由 Form 构造 / 应用启动时自动调用，用户一般无需直接调用。
	 * 若已由另一线程登记，则本次调用忽略（首个登记线程胜出）。
	 */
	void InitializeUIThread();

	/** @brief 当前线程是否是已登记的 UI 线程。 */
	bool IsUIThread() noexcept;

	/** @brief 返回 UI 线程的 Win32 线程 ID；尚未登记时返回 0。 */
	std::uint32_t GetUIThreadId() noexcept;

	/**
	 * @brief 调试期断言：要求当前在 UI 线程。Release 构建下为 no-op。
	 *
	 * 命中断言会触发调试中断，用于尽早暴露"工作线程直接触碰 UI"的违规。
	 */
	void AssertUIThread(const char* reason = nullptr) noexcept;

	/**
	 * @brief 是否已经完成 UI 线程登记且 dispatcher 可用。
	 *
	 * 用于判断 `PostToUIThread` 是否能真正投递（应用尚未创建窗口/泵时可能为 false）。
	 */
	bool HasUIThreadDispatcher() noexcept;

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
	 * @brief 便捷封装：在工作线程上调用时封送回 UI 线程，已在 UI 线程则立即执行。
	 *
	 * 适合"既可能被 UI 线程也可能被工作线程触发"的代码路径（如 MediaPlayer 状态变更）。
	 * 注意：与 `PostToUIThread` 不同，此函数在 UI 线程上是同步立即执行的。
	 */
	void InvokeOnUIThread(std::function<void()> fn);

	/**
	 * @brief 由消息泵在每个循环调用，排空并执行已封送的回调。
	 *
	 * 框架内部使用（Form::DoEvent / dispatcher 窗口过程）。普通用户无需调用。
	 * 仅在 UI 线程上调用有效。
	 */
	void PumpUIThreadCallbacks();
}
