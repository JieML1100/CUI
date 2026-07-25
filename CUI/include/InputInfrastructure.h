#pragma once

#include "Control.h"

namespace cui::framework
{
	/** Narrow bridge for framework-owned normalized/text input and tests. */
	struct InputAccess final
	{
		static bool DispatchInput(
			Control& target, const InputReport& input)
		{
			return target.DispatchInput(input);
		}

		static bool ProcessCommandInput(
			Control& source, const KeyEventArgs& input)
		{
			return RoutedCommandManager::ProcessInput(source, input);
		}

		static bool ProcessCommandInput(
			Control& source,
			const MouseEventArgs& input,
			ModifierKeys modifiers = ModifierKeys::None)
		{
			return RoutedCommandManager::ProcessInput(
				source, input, modifiers);
		}

		static bool DispatchTextInput(
			Control& target, TextCompositionEventArgs& input)
		{
			return target.DispatchTextInput(input);
		}

		static bool ResolveTextInputCaretRect(
			Control& target, D2D1_RECT_F& rect)
		{
			return target.ResolveTextInputCaretRect(rect);
		}

		/** Publishes pointer-over state owned by the Window hit-test pipeline. */
		static void PublishPointerOverState(
			Control& target,
			bool isMouseOver,
			bool isMouseDirectlyOver)
		{
			target.SetMouseOverCore(isMouseOver, isMouseDirectlyOver);
		}
	};
}
