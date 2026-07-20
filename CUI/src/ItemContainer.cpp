#include "ItemContainer.h"

#include <utility>

ItemContainerControl::ItemContainerControl()
	: ContentControl(0, 0, 0, 0)
{
	SetAutoSize(true, true);
	HAlign = HorizontalAlignment::Stretch;
	VAlign = VerticalAlignment::Top;
	EnsureBindingPropertiesRegistered();
	(void)TrySetPropertyValue(
		L"Padding", BindingValue(Thickness(8.0f, 4.0f, 8.0f, 4.0f)),
		ControlPropertyValueSource::Theme);
	(void)TrySetPropertyValue(
		L"BorderThickness", BindingValue(0.0f),
		ControlPropertyValueSource::Theme);
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
	if (!_selectionGestureAttached)
	{
		AttachSelectionGesture(*this);
		_selectionGestureAttached = true;
	}
	if (outError) outError->clear();
	return true;
}

void ItemContainerControl::EnsureBindingPropertiesRegistered()
{
	ContentControl::EnsureBindingPropertiesRegistered();
	static const bool registered = []
	{
		auto options = []
		{
			ControlPropertyOptions<ItemContainerControl, bool> value;
			value.DefaultValue = false;
			value.Flags = ControlPropertyFlags::AffectsRender;
			value.Design.Browsable = false;
			value.Design.Category = L"State";
			value.Design.Persistence = ControlPropertyPersistence::Transient;
			value.IsReadOnly = true;
			return value;
		};
		BindingPropertyRegistry::Register<ItemContainerControl, bool>(
			L"IsSelected",
			[](ItemContainerControl& target) { return target.GetIsSelected(); }, {},
			[](ItemContainerControl& target,
				BindingPropertyMetadata::ChangeHandler handler,
				DataSourceUpdateMode)
			{
				return target._selectedChanged.Subscribe(
					[handler = std::move(handler)](ItemContainerControl*)
					{ handler(); });
			}, options());
		BindingPropertyRegistry::Register<ItemContainerControl, bool>(
			L"IsMouseOver",
			[](ItemContainerControl& target) { return target.GetIsMouseOver(); }, {},
			[](ItemContainerControl& target,
				BindingPropertyMetadata::ChangeHandler handler,
				DataSourceUpdateMode)
			{
				return target._mouseOverChanged.Subscribe(
					[handler = std::move(handler)](ItemContainerControl*)
					{ handler(); });
			}, options());
		BindingPropertyRegistry::Register<ItemContainerControl, bool>(
			L"IsKeyboardFocusWithin",
			[](ItemContainerControl& target)
			{ return target.GetIsKeyboardFocusWithin(); }, {},
			[](ItemContainerControl& target,
				BindingPropertyMetadata::ChangeHandler handler,
				DataSourceUpdateMode)
			{
				return target._keyboardFocusWithinChanged.Subscribe(
					[handler = std::move(handler)](ItemContainerControl*)
					{ handler(); });
			}, options());
		return true;
	}();
	(void)registered;
}

void ItemContainerControl::AttachSelectionGesture(Control& control)
{
	control.OnMouseDown += [this](Control*, MouseEventArgs args)
	{
		if (args.Buttons != MouseButtons::Left) return;
		ActivateItem();
		FocusOwner();
	};
	control.OnMouseEnter += [this](Control*, MouseEventArgs)
	{
		SetMouseOver(true);
	};
	control.OnMouseLeave += [this](Control*, MouseEventArgs)
	{
		SetMouseOver(false);
	};
	for (auto* child : control.Children)
		if (child) AttachSelectionGesture(*child);
}

void ItemContainerControl::SetSelected(bool value)
{
	if (_selected == value) return;
	(void)SetPropertyField(L"IsSelected", _selected, value);
	SetStyleState(ControlStyleState::Selected, value);
	UpdateThemeBackground();
	_selectedChanged(this);
}

void ItemContainerControl::SetMouseOver(bool value)
{
	if (_mouseOver == value) return;
	(void)SetPropertyField(L"IsMouseOver", _mouseOver, value);
	SetStyleState(ControlStyleState::Hovered, value);
	UpdateThemeBackground();
	_mouseOverChanged(this);
}

void ItemContainerControl::SetKeyboardFocusWithin(bool value)
{
	if (_keyboardFocusWithin == value) return;
	(void)SetPropertyField(
		L"IsKeyboardFocusWithin", _keyboardFocusWithin, value);
	SetStyleState(ControlStyleState::Focused, value);
	_keyboardFocusWithinChanged(this);
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
		L"BackColor", BindingValue(color),
		ControlPropertyValueSource::Theme);
}
