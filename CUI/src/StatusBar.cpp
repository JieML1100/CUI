#include "StatusBar.h"
#include "StyleInfrastructure.h"

#include <memory>

StatusBarItem::StatusBarItem()
	: ContentControl()
{
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
