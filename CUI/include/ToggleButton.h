#pragma once

#include "ButtonBase.h"

/** WPF-style owner of the toggle state shared by CheckBox/RadioButton/Switch. */
class ToggleButton : public ButtonBase
{
	friend class ToggleAutomationPeer;

private:
	NullableBool _isChecked{ false };
	bool _isThreeState = false;

protected:
	/** Derived behavior hook; the dependency property remains owned by ToggleButton. */
	virtual void OnIsCheckedChanged(
		NullableBool oldValue, NullableBool newValue)
	{
		(void)oldValue;
		(void)newValue;
	}
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override
	{
		return std::make_unique<ToggleAutomationPeer>(
			*this, AutomationControlType::Button, L"Button");
	}
	/** WPF toggle hook invoked before Click is raised. */
	virtual void OnToggle();
	bool OnClick() override;

public:
	using UIElement::Checked;
	using UIElement::Unchecked;
	using UIElement::Indeterminate;

	ToggleButton();
	UIClass Type() override { return UIClass::UI_ToggleButton; }
	static void RegisterDependencyProperties();
	static const DependencyProperty& IsCheckedProperty();
	static const DependencyProperty& IsThreeStateProperty();
#if CUI_ENABLE_DYNAMIC_XAML
	void EnsureBindingPropertiesRegistered() override
	{
		RegisterDependencyProperties();
	}
#endif

	/** WPF bool? state: false, true, or indeterminate. */
	PROPERTY(NullableBool, IsChecked);
	GET(NullableBool, IsChecked);
	SET(NullableBool, IsChecked);
	PROPERTY(bool, IsThreeState);
	GET(bool, IsThreeState);
	SET(bool, IsThreeState);
	bool IsCheckedForAccessibility() const noexcept override
	{
		return _isChecked == true;
	}
	AutomationToggleState GetToggleStateForAccessibility() const noexcept override
	{
		if (!_isChecked.HasValue())
			return AutomationToggleState::Indeterminate;
		return _isChecked == true
			? AutomationToggleState::On
			: AutomationToggleState::Off;
	}
};
