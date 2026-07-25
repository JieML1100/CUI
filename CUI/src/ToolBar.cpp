#include "ToolBar.h"

#include <memory>

ToolBar::ToolBar()
	: HeaderedItemsControl()
{
	(void)TrySetPropertyValue(
		L"Background", BindingValue(cui::theme::palette::Surface),
		DependencyPropertyValueSource::Theme);
	(void)TrySetPropertyValue(
		L"BorderBrush", BindingValue(cui::theme::palette::Border),
		DependencyPropertyValueSource::Theme);
	(void)TrySetPropertyValue(
		L"BorderThickness", BindingValue(Thickness(1.0f)),
		DependencyPropertyValueSource::Theme);
	(void)TrySetPropertyValue(
		L"Padding", BindingValue(Thickness(6.0f, 4.0f, 6.0f, 4.0f)),
		DependencyPropertyValueSource::Theme);

	auto panel = std::make_shared<ItemsPanelTemplate>();
	panel->Kind = ItemsPanelKind::Stack;
	panel->Orientation = Orientation::Horizontal;
	(void)TrySetPropertyValue(
		L"ItemsPanel",
		BindingValue(ItemsPanelTemplateReference(std::move(panel))),
		DependencyPropertyValueSource::Theme);
}

void ToolBar::RegisterDependencyProperties()
{
	HeaderedItemsControl::RegisterDependencyProperties();
}
