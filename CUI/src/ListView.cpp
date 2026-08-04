#include "ListView.h"
#include "StyleInfrastructure.h"

void ListViewItem::RegisterDependencyProperties()
{
	ListBoxItem::RegisterDependencyProperties();
}

ListView::ListView()
	: ListBox()
{
}

void ListView::RegisterDependencyProperties()
{
	ListBox::RegisterDependencyProperties();
}

std::unique_ptr<Control> ListView::BuildGeneratedItem(
	const BindingSourceReference& item,
	size_t index,
	BindingPathObservation& observation)
{
	observation = {};
	std::unique_ptr<ListViewItem> container;
	const auto itemContainerTemplate = GetItemContainerTemplate();
	if (itemContainerTemplate)
	{
		if (itemContainerTemplate.Get()->TargetType()
			!= UIClass::UI_ListViewItem)
		{
			SetLastTemplateError(
				L"ItemContainerTemplate TargetType 必须是 ListViewItem。");
			return {};
		}
		std::wstring error;
		auto built = itemContainerTemplate.Get()->Build(&error);
		auto* itemContainer = dynamic_cast<ListViewItem*>(built.get());
		if (!itemContainer)
		{
			SetLastTemplateError(error.empty()
				? L"ItemContainerTemplate 未生成 ListViewItem。" : error);
			return {};
		}
		container.reset(static_cast<ListViewItem*>(built.release()));
	}
	else container = std::make_unique<ListViewItem>();

	cui::framework::StyleAccess::SetResourceKey(
		*container, GetItemContainerStyle());
	std::wstring error;
	bool initialized = false;
#if CUI_ENABLE_DYNAMIC_XAML
	if (GetCompiledDisplayMemberPath().Empty())
		initialized = container->InitializeItem(
			item, GetItemTemplate(), GetDisplayMemberPath(),
			index, L"ListViewItem", &error);
	else
#endif
		initialized = container->InitializeItem(
			item, GetItemTemplate(), GetCompiledDisplayMemberPath(),
			index, L"ListViewItem", &error);
	if (!initialized)
	{
		SetLastTemplateError(error.empty()
			? L"ListViewItem 内容初始化失败。" : std::move(error));
		return {};
	}
	return container;
}
