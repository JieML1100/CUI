#pragma once

#include "Event.h"

namespace cui::framework
{
	/**
	 * Infrastructure-only publisher for CLR-shaped events.
	 *
	 * Event.h intentionally exposes subscriptions without exposing this bridge.
	 * Framework owners include this header in implementation translation units;
	 * ordinary event consumers do not receive a public Raise/Clear API.
	 */
	struct EventAccess final
	{
		template<typename Func, typename... Args>
		static void Raise(Event<Func>& event, Args&&... args)
		{
			event.InvokeCore(std::forward<Args>(args)...);
		}

		template<typename Func, typename TContinue, typename... Args>
		static void RaiseWhile(
			Event<Func>& event, TContinue&& shouldContinue, Args&&... args)
		{
			event.InvokeCoreWhile(
				std::forward<TContinue>(shouldContinue),
				std::forward<Args>(args)...);
		}

		template<typename Func>
		static void Clear(Event<Func>& event) noexcept
		{
			event.ClearCore();
		}
	};
}
