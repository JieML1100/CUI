#pragma once

#include "Control.h"
#include "ReverseInheritedProperty.h"

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

		/** Publishes the exact pointer origin after IsMouseOver propagation. */
		static void PublishMouseDirectlyOverState(
			Control& target, bool isMouseDirectlyOver)
		{
			target.SetIsMouseDirectlyOverCore(isMouseDirectlyOver);
		}

#if CUI_RUNTIME_FLAVOR_DESIGN
		/** Design/test seam for previewing pointer-triggered styles off-window. */
		static void PublishPointerOverState(
			Control& target,
			bool isMouseOver,
			bool isMouseDirectlyOver)
		{
			DependencyObject::DeferredPropertyChange change;
			if (target.StageReverseInheritedPropertyChange(
				cui::framework::ReverseInheritedPropertyKind::MouseOver,
				isMouseOver,
				change))
				target.PublishReverseInheritedPropertyChange(
					cui::framework::ReverseInheritedPropertyKind::MouseOver,
					change);
			target.SetIsMouseDirectlyOverCore(
				isMouseOver && isMouseDirectlyOver);
		}
#endif

		/** Publishes direct mouse capture owned by InputManager. */
		static void PublishMouseCaptureState(
			Control& target, bool isMouseCaptured)
		{
			target.SetIsMouseCapturedCore(isMouseCaptured);
		}
	};
}
