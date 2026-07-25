#include "StatusBar.h"
#include "StyleInfrastructure.h"

#include <memory>

StatusBarItem::StatusBarItem()
	: ContentControl()
{
	(void)TrySetPropertyValue(
		L"HorizontalAlignment", BindingValue(::HorizontalAlignment::Left),
		DependencyPropertyValueSource::Theme);
	(void)TrySetPropertyValue(
		L"Padding", BindingValue(Thickness(8.0f, 3.0f, 8.0f, 3.0f)),
		DependencyPropertyValueSource::Theme);
	(void)TrySetPropertyValue(
		L"BorderThickness", BindingValue(Thickness(0.0f)),
		DependencyPropertyValueSource::Theme);
}

void StatusBarItem::RegisterDependencyProperties()
{
	ContentControl::RegisterDependencyProperties();
}

bool StatusBarItem::Initialize(
	const BindingSourceReference& item,
	const ItemTemplateReference& contentTemplate,
	const std::wstring& displayMemberPath,
	const std::wstring& itemTypeName,
	std::wstring* outError)
{
	if (!item)
	{
		if (outError) *outError = L"StatusBarItem 缺少数据项。";
		return false;
	}
	SetContentTypeName(contentTemplate
		? contentTemplate.Get()->DataTypeName() : itemTypeName);
	SetDisplayMemberPath(displayMemberPath);
	SetContentTemplate(contentTemplate);
	SetContent(BindingValue(item));
	if (!LastContentError().empty())
	{
		if (outError) *outError = LastContentError();
		return false;
	}
	if (outError) outError->clear();
	return true;
}

StatusBar::StatusBar()
	: ItemsControl()
{
	RendererBackgroundColor = cui::theme::palette::SurfaceSubtle;
	RendererBorderColor = cui::theme::palette::Border;
	(void)TrySetPropertyValue(
		L"BorderThickness", BindingValue(Thickness(1.0f)),
		DependencyPropertyValueSource::Theme);
	RendererForegroundColor = cui::theme::palette::TextSecondary;

	auto panel = std::make_shared<ItemsPanelTemplate>();
	panel->Kind = ItemsPanelKind::Stack;
	panel->Orientation = Orientation::Horizontal;
	(void)TrySetPropertyValue(
		L"ItemsPanel",
		BindingValue(ItemsPanelTemplateReference(std::move(panel))),
		DependencyPropertyValueSource::Theme);
}

void StatusBar::RegisterDependencyProperties()
{
	ItemsControl::RegisterDependencyProperties();
}

std::unique_ptr<Control> StatusBar::BuildGeneratedItem(
	const BindingSourceReference& item,
	size_t,
	BindingPathObservation& observation)
{
	observation = {};
	auto container = std::make_unique<StatusBarItem>();
	cui::framework::StyleAccess::SetResourceKey(
		*container, GetItemContainerStyle());
	const auto source = GetItemsSource();
	std::wstring error;
	if (!container->Initialize(
		item,
		GetItemTemplate(),
		GetDisplayMemberPath(),
		source ? source.Get()->ItemTypeName() : std::wstring{},
		&error))
	{
		SetLastTemplateError(error.empty()
			? L"StatusBarItem 内容初始化失败。" : std::move(error));
		return {};
	}
	return container;
}
