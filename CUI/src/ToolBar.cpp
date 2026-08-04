#include "ToolBar.h"
#include "StyleInfrastructure.h"

#include <memory>

const wchar_t* ToolBar::DefaultItemStyleResourceKey(UIClass type) noexcept
{
	switch (type)
	{
	case UIClass::UI_Button: return L"CuiToolBarButtonStyle";
	case UIClass::UI_ToggleButton: return L"CuiToolBarToggleButtonStyle";
	case UIClass::UI_CheckBox: return L"CuiToolBarCheckBoxStyle";
	case UIClass::UI_RadioButton: return L"CuiToolBarRadioButtonStyle";
	case UIClass::UI_ComboBox: return L"CuiToolBarComboBoxStyle";
	case UIClass::UI_TextBox: return L"CuiToolBarTextBoxStyle";
	case UIClass::UI_Menu: return L"CuiToolBarMenuStyle";
	case UIClass::UI_Separator: return L"CuiToolBarSeparatorStyle";
	default: return nullptr;
	}
}

ToolBar::ToolBar()
	: HeaderedItemsControl()
{
	auto panel = std::make_shared<ItemsPanelTemplate>();
	panel->Kind = ItemsPanelKind::Stack;
	panel->Orientation = Orientation::Horizontal;
	(void)TrySetPropertyValue(
		ItemsControl::ItemsPanelProperty(),
		BindingValue(ItemsPanelTemplateReference(std::move(panel))),
		DependencyPropertyValueSource::Theme);
}

void ToolBar::RegisterDependencyProperties()
{
	HeaderedItemsControl::RegisterDependencyProperties();
}

void ToolBar::OnAuthoredItemsChanged() noexcept
{
	try { PrepareItemStyles(); }
	catch (...) {}
}

void ToolBar::OnGeneratedItemsRealized()
{
	PrepareItemStyles();
}

void ToolBar::PrepareItemStyles()
{
	auto prepare = [this](Control* item)
	{
		if (!item) return;
		const auto& containerStyle = GetItemContainerStyle();
		if (!containerStyle.empty())
			cui::framework::StyleAccess::SetResourceKey(*item, containerStyle);
		else if (cui::framework::StyleAccess::ResourceKey(*item).empty())
			if (const auto* key = DefaultItemStyleResourceKey(item->Type()))
				cui::framework::StyleAccess::SetResourceKey(
					*item, key, false, true);
		if (cui::framework::StyleAccess::HasVisibleStyleRules(*item))
			(void)cui::framework::StyleAccess::Refresh(*item, true);
	};
	for (size_t index = 0; index < AuthoredItemCount(); ++index)
		prepare(GetAuthoredItem(index));
	for (size_t index = 0; index < ItemCount(); ++index)
		prepare(GetGeneratedItem(index));
}
