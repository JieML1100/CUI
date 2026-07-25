#pragma once

#include "Control.h"

/**
 * WPF-style structural separator.
 *
 * Separator is a real item control, not a flag on MenuItem. Its visual is a
 * thin rule whose axis follows the arranged aspect ratio.
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
	void EnsureBindingPropertiesRegistered() override
	{
		RegisterDependencyProperties();
	}
protected:
	void OnRender() override;
};
