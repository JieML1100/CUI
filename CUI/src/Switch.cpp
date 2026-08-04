#include "Switch.h"

UIClass Switch::Type() { return UIClass::UI_Switch; }

Switch::Switch() = default;

void Switch::SetChecked(bool checked)
{
	if (IsChecked == checked) return;
	(void)TrySetCurrentPropertyValue(
		IsCheckedProperty(), BindingValue(NullableBool(checked)));
}

void Switch::Toggle()
{
	SetChecked(!(IsChecked == true));
}

bool Switch::IsAnimationRunning()
{
	// The framework theme owns Switch transitions; the native behavior host no
	// longer maintains a parallel animation clock.
	return false;
}

bool Switch::GetAnimatedInvalidRect(D2D1_RECT_F&)
{
	return false;
}

bool Switch::Invoke()
{
	return ButtonBase::Invoke();
}
