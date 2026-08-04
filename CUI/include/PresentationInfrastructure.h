#pragma once

#include "Control.h"

namespace cui::framework
{
	/** Narrow framework/test bridge for protected presentation lifecycle hooks. */
	struct PresentationAccess final
	{
		static void Prepare(Control& target)
		{
			target.PreparePresentation();
		}

		static void NotifyDpiChanged(Control& target, float dpiScale)
		{
			target.NotifyDpiChanged(dpiScale);
		}

		static void NotifyDeviceResourcesInvalidated(Control& target) noexcept
		{
			target.NotifyDeviceResourcesInvalidated();
		}

		static bool BreaksVisualPresentationInheritance(
			const Control& target) noexcept
		{
			return target.BreaksVisualPresentationInheritance();
		}

		static bool AdvanceVisualStateAnimations(
			Control& target, unsigned long long nowMilliseconds)
		{
			return target.AdvanceVisualStateAnimations(nowMilliseconds);
		}

		/** Queues exactly one native animation leaf without advancing siblings. */
		static bool InvalidateNativeAnimationFrame(Control& target)
		{
			if (!target.IsAnimationRunning()) return false;
			D2D1_RECT_F rect{};
			if (target.GetAnimatedInvalidRect(rect))
				target.InvalidateVisualRect(rect);
			else
				target.InvalidateVisual();
			return true;
		}

		static void InvalidateMeasureSubtree(Control& target)
		{
			target.InvalidateMeasureSubtree();
		}

		static void InvalidateVisualRect(
			Control& target, const D2D1_RECT_F& contentRect)
		{
			target.InvalidateVisualRect(contentRect);
		}
	};
}
