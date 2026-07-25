#pragma once

#include "HeaderedItemsControl.h"

/**
 * WPF-style ToolBar item host.
 *
 * ToolBar owns no private button model and performs no immediate-mode item
 * layout. Direct XAML controls populate Items; data records use ItemsSource,
 * ItemTemplate and the common ItemsControl generator. Appearance and sizing
 * belong to item styles and ItemsPanelTemplate.
 */
class ToolBar final : public HeaderedItemsControl
{
protected:
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override
	{
		return std::make_unique<AutomationPeer>(
			*this, AutomationControlType::ToolBar, L"ToolBar");
	}

public:
	ToolBar();
	UIClass Type() override { return UIClass::UI_ToolBar; }
	static void RegisterDependencyProperties();
	void EnsureBindingPropertiesRegistered() override
	{
		RegisterDependencyProperties();
	}
};
