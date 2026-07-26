#pragma once

#include "Control.h"

#include <utility>

namespace cui::framework
{
	/** Internal observation bridge for visual/logical/template parent changes. */
	struct TreeAccess final
	{
		/**
		 * Attaches an implementation-owned visual with its final logical parent
		 * in one tree transaction. This prevents infrastructure hosts such as
		 * ItemsPresenter from temporarily becoming the logical owner.
		 */
		static Control* InsertVisualChild(
			Control& visualParent,
			int index,
			Control* child,
			Control* logicalParent)
		{
			return visualParent.InsertVisualChildWithLogicalParent(
				index, child, logicalParent);
		}

		template<typename T>
		static T* InsertOwnedVisualChild(
			Control& visualParent,
			int index,
			std::unique_ptr<T> child,
			Control* logicalParent)
		{
			static_assert(std::is_base_of_v<Control, T>,
				"T must derive from Control");
			if (!child)
				throw std::invalid_argument("不能添加空控件");
			T* raw = child.release();
			try
			{
				(void)InsertVisualChild(
					visualParent, index, raw, logicalParent);
			}
			catch (...)
			{
				if (!raw->GetVisualParent())
					child.reset(raw);
				throw;
			}
			return raw;
		}

		template<typename T>
		static T* AddOwnedVisualChild(
			Control& visualParent,
			std::unique_ptr<T> child,
			Control* logicalParent)
		{
			return InsertOwnedVisualChild(
				visualParent,
				visualParent.VisualChildCount(),
				std::move(child),
				logicalParent);
		}

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
