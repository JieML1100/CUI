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

		/** Publishes logical focus owned by FocusManager. */
		static void PublishLogicalFocusState(
			Control& target, bool isFocused)
		{
			target.SetIsFocusedCore(isFocused);
		}

		/** Publishes keyboard focus owned by FocusManager. */
		static void PublishKeyboardFocusState(
			Control& target, bool isFocused)
		{
			target.SetIsKeyboardFocusedCore(isFocused);
		}

		/** Publishes keyboard-focus-within along the active focus route. */
		static void PublishKeyboardFocusWithinState(
			Control& target, bool isFocusWithin)
		{
			target.SetIsKeyboardFocusWithinCore(isFocusWithin);
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
