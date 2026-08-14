#include "StatusBar.h"
#include "Layout/StackPanel.h"
#include "StyleInfrastructure.h"

#include <memory>

StatusBarItem::StatusBarItem()
	: ContentControl()
{
#if CUI_ENABLE_DYNAMIC_XAML
	EnsureBindingPropertiesRegistered();
#endif
}

void StatusBarItem::RegisterDependencyProperties()
{
	ContentControl::RegisterDependencyProperties();
}

#if CUI_ENABLE_DYNAMIC_XAML
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
#endif

bool StatusBarItem::Initialize(
	const BindingSourceReference& item,
	const ItemTemplateReference& contentTemplate,
	CompiledBindingPathView displayMemberPath,
	DataTypeToken itemTypeToken,
	std::wstring* outError)
{
	if (!item)
	{
		if (outError) *outError = L"StatusBarItem 缺少数据项。";
		return false;
	}
	SetContentTypeToken(contentTemplate
		? contentTemplate.Get()->GetDataTypeToken() : itemTypeToken);
	SetCompiledDisplayMemberPath(displayMemberPath);
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
#if CUI_ENABLE_DYNAMIC_XAML
	EnsureBindingPropertiesRegistered();
#endif
}

void StatusBar::RegisterDependencyProperties()
{
	ItemsControl::RegisterDependencyProperties();
}

std::unique_ptr<Panel> StatusBar::CreateItemsHost() const
{
	auto panel = std::make_unique<StackPanel>();
	panel->SetOrientation(Orientation::Horizontal);
	panel->VerticalAlignment = VerticalAlignment::Stretch;
	return panel;
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
	bool initialized = false;
#if CUI_ENABLE_DYNAMIC_XAML
	if (GetCompiledDisplayMemberPath().Empty())
		initialized = container->Initialize(
			item, GetItemTemplate(), GetDisplayMemberPath(),
			source ? source.Get()->ItemTypeName() : std::wstring{}, &error);
	else
#endif
		initialized = container->Initialize(
			item, GetItemTemplate(), GetCompiledDisplayMemberPath(),
			source ? source.Get()->GetItemTypeToken() : DataTypeToken{}, &error);
	if (!initialized)
	{
		SetLastTemplateError(error.empty()
			? L"StatusBarItem 内容初始化失败。" : std::move(error));
		return {};
	}
	return container;
}

void StatusBar::OnAuthoredItemsChanged() noexcept
{
	try { PrepareItemStyles(); }
	catch (...) {}
}

void StatusBar::OnGeneratedItemsRealized()
{
	PrepareItemStyles();
}

void StatusBar::PrepareItemStyles()
{
	auto prepare = [this](Control* item)
	{
		if (!item) return;
		cui::framework::StyleAccess::SetResourceKey(
			*item, GetItemContainerStyle());
		if (cui::framework::StyleAccess::HasVisibleStyleRules(*item))
			(void)cui::framework::StyleAccess::Refresh(*item, true);
	};
	for (size_t index = 0; index < AuthoredItemCount(); ++index)
		prepare(GetAuthoredItem(index));
	for (size_t index = 0; index < ItemCount(); ++index)
		prepare(GetGeneratedItem(index));
}
