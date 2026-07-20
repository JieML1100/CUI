#include "Selector.h"

#include "Form.h"

#include <algorithm>
#include <utility>

namespace
{
	template<typename TValue>
	ControlPropertyOptions<Selector, TValue> SelectorPropertyOptions(
		TValue defaultValue,
		int order,
		ControlPropertyEditorKind editor,
		ControlPropertyPersistence persistence =
			ControlPropertyPersistence::Metadata)
	{
		ControlPropertyOptions<Selector, TValue> options;
		options.DefaultValue = std::move(defaultValue);
		options.Flags = ControlPropertyFlags::AffectsRender
			| ControlPropertyFlags::TracksLocalValue;
		options.Design.Category = L"Data";
		options.Design.CategoryOrder = 80;
		options.Design.Order = order;
		options.Design.Editor = editor;
		options.Design.Persistence = persistence;
		return options;
	}

	auto SelectorPropertySubscriber(const wchar_t* propertyName)
	{
		return [propertyName = std::wstring(propertyName)](
			Selector& target,
			BindingPropertyMetadata::ChangeHandler handler,
			DataSourceUpdateMode)
		{
			return target.OnPropertyValueChanged.Subscribe(
				[propertyName, handler = std::move(handler)](
					Control*, const ControlPropertyChangedEventArgs& args)
				{
					if (_wcsicmp(args.PropertyName.c_str(), propertyName.c_str()) == 0)
						handler();
				});
		};
	}
}

SelectorItem::SelectorItem()
{
}

bool SelectorItem::Initialize(
	Selector& owner,
	const BindingSourceReference& item,
	const ItemTemplateReference& contentTemplate,
	const std::wstring& displayMemberPath,
	size_t index,
	std::wstring* outError)
{
	_owner = &owner;
	if (!InitializeItem(item, contentTemplate, displayMemberPath,
		index, L"ListBoxItem", outError))
	{
		_owner = nullptr;
		return false;
	}
	return true;
}

void SelectorItem::EnsureBindingPropertiesRegistered()
{
	ItemContainerControl::EnsureBindingPropertiesRegistered();
}

void SelectorItem::ActivateItem()
{
	if (_owner) _owner->SelectIndex(static_cast<int>(ItemIndex()));
}

void SelectorItem::FocusOwner()
{
	if (_owner && _owner->ParentForm)
		_owner->ParentForm->SetSelectedControl(_owner, true);
}

Selector::Selector(int x, int y, int width, int height)
	: ItemsControl(x, y, width, height)
{
	OnGotFocus += [this](Control*) { UpdateContainerSelection(); };
	OnLostFocus += [this](Control*) { UpdateContainerSelection(); };
}

void Selector::EnsureBindingPropertiesRegistered()
{
	ItemsControl::EnsureBindingPropertiesRegistered();
	static const bool registered = []
	{
		auto indexOptions = SelectorPropertyOptions(
			-1, 40, ControlPropertyEditorKind::Number);
		indexOptions.Coerce = [](
			Selector&, const int& proposed) -> std::optional<int>
		{
			return (std::max)(-1, proposed);
		};
		indexOptions.Changed = [](
			Selector& target, const int& oldValue, const int& newValue)
		{
			target.ApplySelectedIndexChange(oldValue, newValue);
		};
		indexOptions.Design.Minimum = -1.0;
		indexOptions.Design.Step = 1.0;
		BindingPropertyRegistry::Register<Selector, int>(
			L"SelectedIndex",
			[](Selector& target) { return target.GetSelectedIndex(); },
			[](Selector& target, const int& value)
			{ target.SetSelectedIndex(value); },
			SelectorPropertySubscriber(L"SelectedIndex"),
			std::move(indexOptions));

		auto valuePathOptions = SelectorPropertyOptions(
			std::wstring{}, 50, ControlPropertyEditorKind::Text);
		BindingPropertyRegistry::Register<Selector, std::wstring>(
			L"SelectedValuePath",
			[](Selector& target) { return target.GetSelectedValuePath(); },
			[](Selector& target, const std::wstring& value)
			{ target.SetSelectedValuePath(value); }, {},
			std::move(valuePathOptions));

		auto itemOptions = SelectorPropertyOptions(
			BindingValue{}, 60, ControlPropertyEditorKind::Auto,
			ControlPropertyPersistence::Transient);
		itemOptions.Design.Browsable = false;
		BindingPropertyRegistry::Register<Selector, BindingValue>(
			L"SelectedItem",
			[](Selector& target) { return target.GetSelectedItem(); },
			[](Selector& target, const BindingValue& value)
			{ target.SetSelectedItem(value); },
			[](Selector& target,
				BindingPropertyMetadata::ChangeHandler handler,
				DataSourceUpdateMode)
			{
				return target._selectedItemChanged.Subscribe(
					[handler = std::move(handler)](Selector*) { handler(); });
			}, std::move(itemOptions));

		auto valueOptions = SelectorPropertyOptions(
			BindingValue{}, 70, ControlPropertyEditorKind::Auto,
			ControlPropertyPersistence::Transient);
		valueOptions.Design.Browsable = false;
		BindingPropertyRegistry::Register<Selector, BindingValue>(
			L"SelectedValue",
			[](Selector& target) { return target.GetSelectedValue(); },
			[](Selector& target, const BindingValue& value)
			{ target.SetSelectedValue(value); },
			[](Selector& target,
				BindingPropertyMetadata::ChangeHandler handler,
				DataSourceUpdateMode)
			{
				return target._selectedValueChanged.Subscribe(
					[handler = std::move(handler)](Selector*) { handler(); });
			}, std::move(valueOptions));

		auto containerStyleOptions = SelectorPropertyOptions(
			std::wstring{}, 80, ControlPropertyEditorKind::Text,
			ControlPropertyPersistence::Transient);
		containerStyleOptions.Design.Browsable = false;
		BindingPropertyRegistry::Register<Selector, std::wstring>(
			L"ItemContainerStyle",
			[](Selector& target) { return target.GetItemContainerStyle(); },
			[](Selector& target, const std::wstring& value)
			{ target.SetItemContainerStyle(value); }, {},
			std::move(containerStyleOptions));
		return true;
	}();
	(void)registered;
}

int Selector::ClampIndex(int value) const noexcept
{
	if (value < 0) return -1;
	const auto source = GetItemsSource();
	if (!source) return value;
	if (source.Get()->Count() == 0) return -1;
	return (std::min)(value, static_cast<int>(source.Get()->Count()) - 1);
}

void Selector::SetSelectedIndex(int value)
{
	(void)SetPropertyField(L"SelectedIndex", _selectedIndex, ClampIndex(value));
}

bool Selector::SelectIndex(int value)
{
	const int normalized = ClampIndex(value);
	if (_selectedIndex == normalized) return false;
	SetCurrentSelectedIndex(normalized);
	return true;
}

BindingValue Selector::GetSelectedItem() const
{
	return _selectedItemIdentity
		? BindingValue(_selectedItemIdentity)
		: BindingValue{};
}

void Selector::SetSelectedItem(const BindingValue& value)
{
	BindingSourceReference item;
	if (value.Empty())
	{
		SetCurrentSelectedIndex(-1);
		return;
	}
	if (!value.TryGet(item) || !item || !GetItemsSource())
	{
		SetCurrentSelectedIndex(-1);
		return;
	}
	SetCurrentSelectedIndex(FindBindingListItemByValue(
		GetItemsSource(), std::wstring{}, BindingValue(item)));
}

void Selector::SetSelectedValuePath(std::wstring value)
{
	if (_selectedValuePath == value) return;
	_selectedValuePath = std::move(value);
	const auto source = GetPropertyValueSource(L"SelectedValue");
	BindingValue configured;
	if (source != ControlPropertyValueSource::Default
		&& TryGetPropertyValue(L"SelectedValue", source, configured))
		SetSelectedValue(configured);
	else
		RefreshSelectedItemState(false);
}

BindingValue Selector::GetSelectedValue() const
{
	if (_selectedIndex < 0) return {};
	BindingValue value;
	return TryGetBindingListItemValue(
		GetItemsSource(), static_cast<size_t>(_selectedIndex),
		_selectedValuePath, value)
		? value : BindingValue{};
}

void Selector::SetSelectedValue(const BindingValue& value)
{
	SetCurrentSelectedIndex(value.Empty()
		? -1
		: FindBindingListItemByValue(
			GetItemsSource(), _selectedValuePath, value));
}

void Selector::SetItemContainerStyle(std::wstring value)
{
	if (_itemContainerStyle == value) return;
	_itemContainerStyle = std::move(value);
	for (const auto& [index, item] : GetRealizedItems())
	{
		(void)item;
		if (auto* container = dynamic_cast<SelectorItem*>(GetGeneratedItem(index)))
			container->SetStyleId(_itemContainerStyle);
	}
}

void Selector::SetItemContainerTemplate(ControlTemplateReference value)
{
	if (_itemContainerTemplate == value) return;
	auto previous = _itemContainerTemplate;
	_itemContainerTemplate = std::move(value);
	if (RebuildGeneratedItems()) return;
	const auto error = LastTemplateError();
	_itemContainerTemplate = std::move(previous);
	(void)RebuildGeneratedItems();
	SetLastTemplateError(error);
}

std::unique_ptr<Control> Selector::BuildGeneratedItem(
	const BindingSourceReference& item,
	size_t index,
	BindingPathObservation& observation)
{
	observation = {};
	std::unique_ptr<SelectorItem> container;
	if (_itemContainerTemplate)
	{
		if (_itemContainerTemplate.Get()->TargetType()
			!= UIClass::UI_SelectorItem)
		{
			SetLastTemplateError(
				L"ItemContainerTemplate TargetType 必须是 ListBoxItem。");
			return {};
		}
		std::wstring error;
		auto built = _itemContainerTemplate.Get()->Build(&error);
		auto* itemContainer = dynamic_cast<SelectorItem*>(built.get());
		if (!itemContainer)
		{
			SetLastTemplateError(error.empty()
				? L"ItemContainerTemplate 未生成 ListBoxItem。" : error);
			return {};
		}
		container.reset(static_cast<SelectorItem*>(built.release()));
	}
	else container = std::make_unique<SelectorItem>();
	container->SetStyleId(_itemContainerStyle);
	std::wstring error;
	if (!container->Initialize(
		*this, item, GetItemTemplate(), GetDisplayMemberPath(), index, &error))
	{
		SetLastTemplateError(error.empty()
			? L"ListBoxItem 内容初始化失败。" : std::move(error));
		return {};
	}
	return container;
}

void Selector::OnGeneratedItemsRebuilt()
{
	RestoreSelectionAfterRebuild();
}

void Selector::OnGeneratedItemsRealized()
{
	UpdateContainerSelection();
}

void Selector::OnGeneratedItemIndexChanged(
	Control&,
	size_t,
	size_t newIndex)
{
	if (auto* item = dynamic_cast<SelectorItem*>(GetGeneratedItem(newIndex)))
		item->SetItemIndex(newIndex);
}

void Selector::SetCurrentSelectedIndex(int value)
{
	const int normalized = ClampIndex(value);
	if (_selectedIndex == normalized)
	{
		RefreshSelectedItemState(false);
		return;
	}
	(void)SetCurrentPropertyField(
		L"SelectedIndex", _selectedIndex, normalized);
}

void Selector::ApplySelectedIndexChange(int oldValue, int newValue)
{
	if (oldValue == newValue) return;
	RefreshSelectedItemState(true);
	EnsureSelectedItemVisible();
}

void Selector::RestoreSelectionAfterRebuild()
{
	int restored = -1;
	BindingValue configured;
	const auto itemSource = GetPropertyValueSource(L"SelectedItem");
	if (itemSource != ControlPropertyValueSource::Default
		&& TryGetPropertyValue(L"SelectedItem", itemSource, configured)
		&& !configured.Empty())
		restored = FindBindingListItemByValue(
			GetItemsSource(), std::wstring{}, configured);
	if (restored < 0)
	{
		const auto valueSource = GetPropertyValueSource(L"SelectedValue");
		if (valueSource != ControlPropertyValueSource::Default
			&& TryGetPropertyValue(L"SelectedValue", valueSource, configured)
			&& !configured.Empty())
			restored = FindBindingListItemByValue(
				GetItemsSource(), _selectedValuePath, configured);
	}
	if (restored < 0 && _selectedItemIdentity)
		restored = FindBindingListItemByValue(
			GetItemsSource(), std::wstring{},
			BindingValue(_selectedItemIdentity));
	if (restored < 0) restored = ClampIndex(_selectedIndex);

	if (_selectedIndex != restored)
		SetCurrentSelectedIndex(restored);
	else
		RefreshSelectedItemState(false);
}

void Selector::RefreshSelectedItemState(bool raiseSelectionChanged)
{
	BindingSourceReference next;
	const auto source = GetItemsSource();
	if (source && _selectedIndex >= 0)
		(void)source.Get()->TryGetItem(
			static_cast<size_t>(_selectedIndex), next);
	const bool itemChanged = next.Shared() != _selectedItemIdentity.Shared();
	_selectedItemIdentity = std::move(next);
	_selectedItemObservation = ObserveBindingPaths(
		_selectedItemIdentity, { _selectedValuePath },
		[this] { NotifySelectionProjectionsChanged(); });
	UpdateContainerSelection();
	NotifySelectionProjectionsChanged();
	if (raiseSelectionChanged || itemChanged)
		OnSelectionChanged(this);
}

void Selector::UpdateContainerSelection()
{
	for (const auto& [index, item] : GetRealizedItems())
	{
		(void)item;
		if (auto* container = dynamic_cast<SelectorItem*>(GetGeneratedItem(index)))
		{
			const bool selected = static_cast<int>(index) == _selectedIndex;
			container->SetSelected(selected);
			const bool ownerFocused = HasControlStyleState(
				GetEffectiveStyleState(), ControlStyleState::Focused);
			container->SetKeyboardFocusWithin(selected && ownerFocused);
		}
	}
}

void Selector::EnsureSelectedItemVisible()
{
	if (_selectedIndex < 0) return;
	(void)BringItemIntoView(static_cast<size_t>(_selectedIndex));
}

bool Selector::HandlesNavigationKey(WPARAM key) const
{
	switch (key)
	{
	case VK_UP:
	case VK_DOWN:
	case VK_HOME:
	case VK_END:
	case VK_PRIOR:
	case VK_NEXT:
		return true;
	default:
		return ItemsControl::HandlesNavigationKey(key);
	}
}

bool Selector::ProcessMessage(
	UINT message, WPARAM wParam, LPARAM lParam,
	int localX, int localY)
{
	if (message == WM_KEYDOWN && GetItemsSource())
	{
		const int count = static_cast<int>(GetItemsSource().Get()->Count());
		int next = _selectedIndex;
		switch (wParam)
		{
		case VK_UP: next = _selectedIndex < 0 ? count - 1 : _selectedIndex - 1; break;
		case VK_DOWN: next = _selectedIndex < 0 ? 0 : _selectedIndex + 1; break;
		case VK_HOME: next = 0; break;
		case VK_END: next = count - 1; break;
		case VK_PRIOR: next = _selectedIndex < 0 ? 0 : _selectedIndex - 5; break;
		case VK_NEXT: next = _selectedIndex < 0 ? 0 : _selectedIndex + 5; break;
		default: return ItemsControl::ProcessMessage(
			message, wParam, lParam, localX, localY);
		}
		if (count > 0) SelectIndex((std::clamp)(next, 0, count - 1));
		KeyEventArgs args(static_cast<Keys>(wParam));
		OnKeyDown(this, args);
		return true;
	}
	return ItemsControl::ProcessMessage(
		message, wParam, lParam, localX, localY);
}

void Selector::NotifySelectionProjectionsChanged()
{
	auto synchronize = [this](
		const wchar_t* propertyName,
		const BindingValue& current)
	{
		const auto source = GetPropertyValueSource(propertyName);
		if (source == ControlPropertyValueSource::Default) return;
		BindingValue stored;
		if (!TryGetPropertyValue(propertyName, source, stored)
			|| !BindingItemValuesEqual(stored, current))
			(void)TrySetCurrentPropertyValue(propertyName, current);
	};
	synchronize(L"SelectedItem", GetSelectedItem());
	synchronize(L"SelectedValue", GetSelectedValue());
	_selectedItemChanged(this);
	_selectedValueChanged(this);
}
