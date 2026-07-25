#pragma once

#include "ButtonBase.h"

/** WPF-style owner of the toggle state shared by CheckBox/RadioButton/Switch. */
class ToggleButton : public ButtonBase
{
private:
	bool _isChecked = false;

protected:
	/** Derived behavior hook; the dependency property remains owned by ToggleButton. */
	virtual void OnIsCheckedChanged(bool oldValue, bool newValue)
	{
		(void)oldValue;
		(void)newValue;
	}
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override
	{
		return std::make_unique<ToggleAutomationPeer>(
			*this, AutomationControlType::CheckBox, L"ToggleButton");
	}

public:
	using UIElement::Checked;
	using UIElement::Unchecked;

	ToggleButton();
	UIClass Type() override { return UIClass::UI_ToggleButton; }
	static void RegisterDependencyProperties();
	void EnsureBindingPropertiesRegistered() override
	{
		RegisterDependencyProperties();
	}

	PROPERTY(bool, IsChecked);
	GET(bool, IsChecked);
	SET(bool, IsChecked);
	bool IsCheckedForAccessibility() const noexcept override
	{
		return _isChecked;
	}
};
