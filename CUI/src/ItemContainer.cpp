#include "ItemContainer.h"
#include "EventInfrastructure.h"

#include <utility>

ItemContainerControl::ItemContainerControl()
	: ContentControl()
{
	EnsureBindingPropertiesRegistered();
	(void)TrySetPropertyValue(
		L"VerticalAlignment", BindingValue(::VerticalAlignment::Top),
		DependencyPropertyValueSource::Theme);
	(void)TrySetPropertyValue(
		L"Padding", BindingValue(Thickness(8.0f, 4.0f, 8.0f, 4.0f)),
		DependencyPropertyValueSource::Theme);
	(void)TrySetPropertyValue(
		L"BorderThickness", BindingValue(Thickness(0.0f)),
		DependencyPropertyValueSource::Theme);
	RetainEventConnection(OnMouseDown.Subscribe(
		[this](Control*, MouseEventArgs& args)
		{
			if (args.ChangedButton != MouseButton::Left) return;
			ActivateItem();
			FocusOwner();
		}));
	UpdateThemeBackground();
}

bool ItemContainerControl::InitializeItem(
	const BindingSourceReference& item,
	const ItemTemplateReference& contentTemplate,
	const std::wstring& displayMemberPath,
	size_t index,
	const std::wstring& publicTypeName,
	std::wstring* outError)
{
	if (!item)
	{
		if (outError) *outError = publicTypeName + L" 缺少数据项。";
		return false;
	}
	_index = index;
	SetContentTypeName(contentTemplate
		? contentTemplate.Get()->DataTypeName() : std::wstring{});
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

void ItemContainerControl::RegisterDependencyProperties()
{
	ContentControl::RegisterDependencyProperties();
	static const bool registered = []
	{
		auto options = []
		{
			DependencyPropertyOptions<ItemContainerControl, bool> value;
			value.DefaultValue = false;
			value.Flags = DependencyPropertyFlags::AffectsRender;
			value.Design.Browsable = true;
			value.Design.Category = L"State";
			value.Design.Editor = DependencyPropertyEditorKind::Boolean;
			value.Design.Persistence = DependencyPropertyPersistence::Metadata;
			value.IsReadOnly = false;
			return value;
		};
		DependencyPropertyRegistry::Register<ItemContainerControl, bool>(
			L"IsSelected",
			[](ItemContainerControl& target) { return target.GetIsSelected(); },
			[](ItemContainerControl& target, const bool& value)
			{ target.ApplyIsSelectedValue(value); },
			[](ItemContainerControl& target,
				DependencyPropertyMetadata::ChangeHandler handler,
				DataSourceUpdateMode)
			{
				return target._selectedChanged.Subscribe(
					[handler = std::move(handler)](ItemContainerControl*)
					{ handler(); });
			}, options());
		return true;
	}();
	(void)registered;
}

void ItemContainerControl::SetIsSelected(bool value)
{
	if (!SetPropertyField(L"IsSelected", _selected, value)) return;
	OnIsSelectedRequested(_selected);
}

void ItemContainerControl::SetCurrentIsSelected(bool value)
{
	(void)SetCurrentPropertyField(L"IsSelected", _selected, value);
}

void ItemContainerControl::ApplyIsSelectedValue(bool value)
{
	if (_selected == value) return;
	if (!SetPropertyField(L"IsSelected", _selected, value)) return;
	SetStyleState(ControlStyleState::Selected, value);
	UpdateThemeBackground();
	cui::framework::EventAccess::Raise(_selectedChanged, this);
	RoutedEventArgs args;
	if (value) Selected(this, args);
	else Unselected(this, args);
}

void ItemContainerControl::UpdateThemeBackground()
{
	const auto state = GetEffectiveStyleState();
	const auto color = _selected
		? cui::theme::palette::AccentSelected
		: HasControlStyleState(state, ControlStyleState::Hovered)
			? cui::theme::palette::AccentSoft
			: D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 0.0f };
	(void)TrySetPropertyValue(
		L"Background", BindingValue(color),
		DependencyPropertyValueSource::Theme);
}
