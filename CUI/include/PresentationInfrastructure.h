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

		static bool AdvanceVisualStateAnimations(
			Control& target, unsigned long long nowMilliseconds)
		{
			return target.AdvanceVisualStateAnimations(nowMilliseconds);
		}
	};
}
