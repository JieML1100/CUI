#pragma once

#include "Control.h"

#include <utility>

namespace cui::framework
{
	/** Internal observation bridge for visual/logical/template parent changes. */
	struct TreeAccess final
	{
		template<typename F>
		static EventConnection SubscribeVisualParentChanged(
			Control& target, F&& handler)
		{
			return target.OnVisualParentChanged.Subscribe(
				std::forward<F>(handler));
		}

		template<typename F>
		static EventConnection SubscribeLogicalParentChanged(
			Control& target, F&& handler)
		{
			return target.OnLogicalParentChanged.Subscribe(
				std::forward<F>(handler));
		}

		template<typename F>
		static EventConnection SubscribeTemplatedParentChanged(
			Control& target, F&& handler)
		{
			return target.OnTemplatedParentChanged.Subscribe(
				std::forward<F>(handler));
		}
	};
}
