#pragma once

#include "Control.h"

#include <utility>

namespace cui::framework
{
	/** Internal observation bridge for visual/logical/template parent changes. */
	struct TreeAccess final
	{
		TreeAccess() = delete;

		static void SetLogicalParent(Control& target, Control* parent)
		{
			target.SetLogicalParent(parent);
		}

		/**
		 * Publishes one logical edge while reporting whether a callback
		 * transferred the target into any visual ownership transaction.
		 */
		static void SetLogicalParent(
			Control& target,
			Control* parent,
			bool* visualOwnershipCommit)
		{
			target.VerifyAccess();
			target.SetLogicalParentCoreObservingVisualOwnership(
				parent, visualOwnershipCommit);
		}

		template<typename T>
		static bool SetLogicalParentPreservingOwnership(
			std::unique_ptr<T>& child,
			Control* parent,
			std::exception_ptr* notificationError = nullptr)
		{
			static_assert(std::is_base_of_v<Control, T>,
				"T must derive from Control");
			if (notificationError) *notificationError = {};
			if (!child) return false;
			auto* raw = child.release();
			const ControlWeakReference lifetime(raw);
			bool visualOwnershipCommit = false;
			try
			{
				SetLogicalParent(
					*raw, parent, &visualOwnershipCommit);
			}
			catch (...)
			{
				const auto error = std::current_exception();
				auto* live = lifetime.Get();
				if (live && !visualOwnershipCommit
					&& !live->GetVisualParent()
					&& !live->GetPresentationWindow())
					child.reset(static_cast<T*>(live));
				const bool committed = !live
					|| visualOwnershipCommit
					|| live->GetVisualParent()
					|| live->GetPresentationWindow()
					|| live->GetLogicalParent() == parent;
				if (!committed || !notificationError)
					std::rethrow_exception(error);
				*notificationError = error;
				return child != nullptr;
			}
			auto* live = lifetime.Get();
			if (!live || visualOwnershipCommit
				|| live->GetVisualParent()
				|| live->GetPresentationWindow())
				return false;
			child.reset(static_cast<T*>(live));
			if (live->GetLogicalParent() != parent)
				throw std::logic_error(
					"logical parent publication did not commit");
			return true;
		}

		static void SetTemplatedParent(Control& target, Control* parent)
		{
			target.SetTemplatedParent(parent);
		}

		static void SetTemplatedParent(
			Control& target,
			Control* parent,
			bool* visualOwnershipCommit)
		{
			target.VerifyAccess();
			target.SetTemplatedParentCoreObservingVisualOwnership(
				parent, visualOwnershipCommit);
		}

		template<typename T>
		static bool SetTemplatedParentPreservingOwnership(
			std::unique_ptr<T>& child,
			Control* parent,
			std::exception_ptr* notificationError = nullptr)
		{
			static_assert(std::is_base_of_v<Control, T>,
				"T must derive from Control");
			if (notificationError) *notificationError = {};
			if (!child) return false;
			auto* raw = child.release();
			const ControlWeakReference lifetime(raw);
			bool visualOwnershipCommit = false;
			try
			{
				SetTemplatedParent(
					*raw, parent, &visualOwnershipCommit);
			}
			catch (...)
			{
				const auto error = std::current_exception();
				auto* live = lifetime.Get();
				if (live && !visualOwnershipCommit
					&& !live->GetVisualParent()
					&& !live->GetPresentationWindow())
					child.reset(static_cast<T*>(live));
				const bool committed = !live
					|| visualOwnershipCommit
					|| live->GetVisualParent()
					|| live->GetPresentationWindow()
					|| live->GetTemplatedParent() == parent;
				if (!committed || !notificationError)
					std::rethrow_exception(error);
				*notificationError = error;
				return child != nullptr;
			}
			auto* live = lifetime.Get();
			if (!live || visualOwnershipCommit
				|| live->GetVisualParent()
				|| live->GetPresentationWindow())
				return false;
			child.reset(static_cast<T*>(live));
			if (live->GetTemplatedParent() != parent)
				throw std::logic_error(
					"templated parent publication did not commit");
			return true;
		}

		template<typename T, typename F>
		static bool InvokePreservingVisualOwnership(
			std::unique_ptr<T>& child,
			F&& callback)
		{
			static_assert(std::is_base_of_v<Control, T>,
				"T must derive from Control");
			if (!child) return false;
			auto* raw = child.release();
			const ControlWeakReference lifetime(raw);
			bool visualOwnershipCommit = false;
			try
			{
				raw->InvokeWithVisualOwnershipObservationCore(
					[&] { std::forward<F>(callback)(*raw); },
					&visualOwnershipCommit);
			}
			catch (...)
			{
				auto* live = lifetime.Get();
				if (live && !visualOwnershipCommit
					&& !live->GetVisualParent()
					&& !live->GetPresentationWindow())
					child.reset(static_cast<T*>(live));
				throw;
			}
			auto* live = lifetime.Get();
			if (!live || visualOwnershipCommit
				|| live->GetVisualParent()
				|| live->GetPresentationWindow())
				return false;
			child.reset(static_cast<T*>(live));
			return true;
		}

		/**
		 * Attaches an implementation-owned visual with its final logical parent
		 * in one tree transaction. This prevents infrastructure hosts such as
		 * ItemsPresenter from temporarily becoming the logical owner.
		 */
		static Control* InsertVisualChild(
			Control& visualParent,
			int index,
			Control* child,
			Control* logicalParent,
			bool* structuralCommit = nullptr)
		{
			return visualParent.InsertVisualChildWithLogicalParent(
				index, child, logicalParent, structuralCommit);
		}

		template<typename T>
		static T* InsertOwnedVisualChildPreserving(
			Control& visualParent,
			int index,
			std::unique_ptr<T>& child,
			Control* logicalParent)
		{
			static_assert(std::is_base_of_v<Control, T>,
				"T must derive from Control");
			if (!child)
				throw std::invalid_argument("不能添加空控件");
			T* raw = child.release();
			const ControlWeakReference lifetime(raw);
			bool structuralCommit = false;
			try
			{
				(void)InsertVisualChild(
					visualParent, index, raw, logicalParent,
					&structuralCommit);
			}
			catch (...)
			{
				auto* live = lifetime.Get();
				const bool requestedParentOwns = live
					&& visualParent.IndexOfVisualChild(live) >= 0;
				if (requestedParentOwns
					&& live->GetVisualParent() == &visualParent
					&& live->GetLogicalParent() != logicalParent)
				{
					try { SetLogicalParent(*live, logicalParent); }
					catch (...) {}
				}
				if (live && !structuralCommit && !requestedParentOwns
					&& !live->GetVisualParent())
					child.reset(static_cast<T*>(live));
				throw;
			}
			auto* live = lifetime.Get();
			if (!live)
				throw std::logic_error(
					"owned infrastructure visual was destroyed during attachment");
			if (live->GetVisualParent() != &visualParent
				|| visualParent.IndexOfVisualChild(live) < 0
				|| live->GetLogicalParent() != logicalParent)
			{
				if (live && !structuralCommit && !live->GetVisualParent()
					&& visualParent.IndexOfVisualChild(live) < 0)
					child.reset(static_cast<T*>(live));
				throw std::logic_error(
					"owned infrastructure visual attachment did not commit");
			}
			return static_cast<T*>(live);
		}

		template<typename T>
		static T* InsertOwnedVisualChild(
			Control& visualParent,
			int index,
			std::unique_ptr<T> child,
			Control* logicalParent)
		{
			return InsertOwnedVisualChildPreserving(
				visualParent, index, child, logicalParent);
		}

		static std::unique_ptr<Control> DetachVisualChild(
			Control& visualParent,
			Control* child,
			bool* visualOwnershipCommit,
			std::exception_ptr* notificationError = nullptr)
		{
			return visualParent.DetachVisualChildCore(
				child, visualOwnershipCommit, notificationError);
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
