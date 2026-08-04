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
	void OnAuthoredItemsChanged() noexcept override;
	void OnGeneratedItemsRealized() override;

public:
	ToolBar();
	UIClass Type() override { return UIClass::UI_ToolBar; }
	/**
	 * Framework Theme Style captured by ToolBar for a directly hosted item type.
	 * This is the native projection of WPF's ToolBar.*StyleKey resources.
	 */
	static const wchar_t* DefaultItemStyleResourceKey(UIClass type) noexcept;
	static void RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	void EnsureBindingPropertiesRegistered() override
	{
		RegisterDependencyProperties();
	}
#endif

private:
	void PrepareItemStyles();
};
