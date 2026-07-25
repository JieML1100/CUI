#pragma once

#include "ContentControl.h"

/** WPF-style semantic base for controls activated by a matching press/release. */
class ButtonBase : public ContentControl
{
private:
	bool _isPressed = false;
	static void EnsureClassHandlers();
	static void HandleDescendantPointerPress(
		Control* sender, RoutedEventArgs& args);

protected:
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override
	{
		return std::make_unique<InvokeAutomationPeer>(
			*this, AutomationControlType::Button, L"Button");
	}
	bool DefaultRaiseClickOnLeftButtonUp() const override { return true; }
	void OnPressedVisualStateChanged(bool value) override;

public:
	using UIElement::Click;

	ButtonBase();
	UIClass Type() override { return UIClass::UI_ButtonBase; }
	static void RegisterDependencyProperties();
	void EnsureBindingPropertiesRegistered() override
	{
		RegisterDependencyProperties();
	}
	READONLY_PROPERTY(bool, IsPressed);
	GET(bool, IsPressed);
};
