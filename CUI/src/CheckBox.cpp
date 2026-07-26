#pragma once
#include "CheckBox.h"

UIClass CheckBox::Type() { return UIClass::UI_CheckBox; }

void CheckBox::BeforeDefaultMouseUp(MouseButton button, MouseEventArgs& e, bool hasMatchingPress)
{
	(void)e;
	if (button == MouseButton::Left && hasMatchingPress)
		SetChecked(!IsChecked);
}

bool CheckBox::Invoke()
{
	if (!IsEnabled || !IsVisible) return false;
	SetChecked(!IsChecked);
	RoutedEventArgs eventArgs;
	Click(this, eventArgs);
	return true;
}

void CheckBox::SetChecked(bool checked)
{
	if (IsChecked == checked) return;
	(void)TrySetCurrentPropertyValue(
		L"IsChecked", BindingValue(checked));
}

void CheckBox::Toggle()
{
	SetChecked(!IsChecked);
}
