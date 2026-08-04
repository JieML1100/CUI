#pragma once
#include "CheckBox.h"

UIClass CheckBox::Type() { return UIClass::UI_CheckBox; }

CheckBox::CheckBox()
{
	RegisterDependencyProperties();
}

void CheckBox::RegisterDependencyProperties()
{
	ToggleButton::RegisterDependencyProperties();
}

bool CheckBox::OnAccessKey(bool isMultiple)
{
	if (!IsKeyboardFocused)
		(void)Focus();
	return ToggleButton::OnAccessKey(isMultiple);
}

bool CheckBox::ProcessInput(const InputReport& input)
{
	if (input.Kind == InputReportKind::KeyDown
		&& !IsThreeState && IsEffectivelyEnabled() && IsVisible)
	{
		if (input.Key == Key::OemPlus || input.Key == Key::Add)
		{
			SetPressed(false);
			SetChecked(true);
			return true;
		}
		if (input.Key == Key::OemMinus || input.Key == Key::Subtract)
		{
			SetPressed(false);
			SetChecked(false);
			return true;
		}
	}
	return ToggleButton::ProcessInput(input);
}

bool CheckBox::Invoke()
{
	return ButtonBase::Invoke();
}

void CheckBox::SetChecked(bool checked)
{
	if (IsChecked == checked) return;
	(void)TrySetCurrentPropertyValue(
		IsCheckedProperty(), BindingValue(NullableBool(checked)));
}

void CheckBox::SetIndeterminate()
{
	if (!IsChecked.HasValue()) return;
	(void)TrySetCurrentPropertyValue(
		IsCheckedProperty(), BindingValue(NullableBool{}));
}

void CheckBox::Toggle()
{
	OnToggle();
}
