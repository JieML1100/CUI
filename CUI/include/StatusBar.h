#pragma once

#include "ItemsControl.h"

/** WPF-style content container generated for one StatusBar item. */
class StatusBarItem final : public ContentControl
{
protected:
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override
	{
		return std::make_unique<AutomationPeer>(
			*this, AutomationControlType::Group, L"StatusBarItem");
	}

public:
	StatusBarItem();
	UIClass Type() override { return UIClass::UI_StatusBarItem; }
	static void RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	void EnsureBindingPropertiesRegistered() override
	{
		RegisterDependencyProperties();
	}
#endif

#if CUI_ENABLE_DYNAMIC_XAML
	bool Initialize(
		const BindingSourceReference& item,
		const ItemTemplateReference& contentTemplate,
		const std::wstring& displayMemberPath,
		const std::wstring& itemTypeName,
		std::wstring* outError = nullptr);
#endif
	bool Initialize(
		const BindingSourceReference& item,
		const ItemTemplateReference& contentTemplate,
		CompiledBindingPathView displayMemberPath,
		DataTypeToken itemTypeToken,
		std::wstring* outError = nullptr);
};

/**
 * WPF-style StatusBar.
 *
 * StatusBar is an ItemsControl. Its content is supplied by ItemsSource and
 * ItemTemplate, and every record is presented by a real StatusBarItem. The
 * former Part/Width drawing model is intentionally absent; item sizing and
 * appearance belong to StatusBarItem styles and the ItemsPanel.
 */
class StatusBar final : public ItemsControl
{
protected:
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override
	{
		return std::make_unique<AutomationPeer>(
			*this, AutomationControlType::StatusBar, L"StatusBar");
	}

public:
	StatusBar();
	UIClass Type() override { return UIClass::UI_StatusBar; }
	static void RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	void EnsureBindingPropertiesRegistered() override
	{
		RegisterDependencyProperties();
	}
#endif

protected:
	std::unique_ptr<Panel> CreateItemsHost() const override;
	std::unique_ptr<Control> BuildGeneratedItem(
		const BindingSourceReference& item,
		size_t index,
		BindingPathObservation& observation) override;
	void OnAuthoredItemsChanged() noexcept override;
	void OnGeneratedItemsRealized() override;

private:
	void PrepareItemStyles();
};
