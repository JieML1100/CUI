#pragma once

#include "Event.h"

#include <cstdint>

/**
 * Platform-neutral input report consumed by control default behavior.
 *
 * PlatformWindowHost/Window is the only layer that translates native messages
 * into this shape. Controls, declarative behaviors and hosted surfaces never
 * receive UINT/WPARAM/LPARAM or infer keyboard state from Win32 globals.
 */
enum class InputReportKind : std::uint8_t
{
	PointerMove,
	PointerLeave,
	PointerDown,
	PointerUp,
	PointerDoubleClick,
	MouseWheel,
	HorizontalMouseWheel,
	KeyDown,
	KeyUp,
	FocusGained,
	FocusLost,
	CaptureLost,
	Cancel,
};

struct InputReport final
{
	static constexpr int WheelDeltaUnit = 120;

	InputReportKind Kind = InputReportKind::PointerMove;
	int X = 0;
	int Y = 0;
	MouseButton ChangedButton = MouseButton::None;
	MouseButtonStates ButtonStates;
	int ClickCount = 0;
	int WheelDelta = 0;
	/** Physical key consumed by native default behavior. */
	::Key Key = ::Key::None;
	/** Non-None only when this physical key arrived as a system key. */
	::Key SystemKey = ::Key::None;
	ModifierKeys Modifiers = ModifierKeys::None;
	bool IsRepeat = false;

	[[nodiscard]] bool HasModifier(ModifierKeys modifier) const noexcept
	{
		return ::HasModifier(Modifiers, modifier);
	}

	[[nodiscard]] bool IsButtonPressed(MouseButton button) const noexcept
	{
		return ButtonStates.IsPressed(button);
	}

	[[nodiscard]] InputReport Retarget(int localX, int localY) const
	{
		auto result = *this;
		result.X = localX;
		result.Y = localY;
		return result;
	}

	[[nodiscard]] MouseEventArgs CreateMouseEventArgs() const
	{
		auto result = MouseEventArgs(
			ChangedButton,
			ButtonStates.Get(ChangedButton),
			ButtonStates,
			ClickCount, X, Y, WheelDelta);
		result.Modifiers = Modifiers;
		return result;
	}

	[[nodiscard]] KeyEventArgs CreateKeyEventArgs() const
	{
		auto result = KeyEventArgs(Key, Modifiers, SystemKey);
		result.IsRepeat = IsRepeat;
		return result;
	}
};
