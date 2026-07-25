#pragma once

#include "Control.h"

namespace cui::framework
{
	/**
	 * Narrow bridge for native fallback chrome that has no public WPF state DP.
	 *
	 * ControlStyleSelector deliberately cannot observe this cache. XAML Trigger
	 * conditions must resolve through dependency-property metadata instead.
	 */
	struct NativeVisualStateAccess final
	{
		NativeVisualStateAccess() = delete;

		static void Set(
			Control& target, ControlStyleState state, bool enabled = true)
		{
			target.SetStyleState(state, enabled);
		}
	};
}
