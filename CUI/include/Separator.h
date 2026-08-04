#pragma once

#include "Control.h"

/**
 * WPF-style structural separator.
 *
 * Separator is a real control, not a flag on MenuItem. It owns no native
 * pixels; its theme can replace the thin-rule ControlTemplate per container.
 */
class Separator final : public Control
{
protected:
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override
	{
		return std::make_unique<AutomationPeer>(
			*this, AutomationControlType::Separator, L"Separator");
	}

public:
	Separator();
	UIClass Type() override { return UIClass::UI_Separator; }
	static void RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	void EnsureBindingPropertiesRegistered() override
	{
		RegisterDependencyProperties();
	}
#endif
};
