#include "Selector.h"
#include "DependencyPropertyInfrastructure.h"
#include "EventInfrastructure.h"
#include "ListBox.h"
#include "StyleInfrastructure.h"

#include "Window.h"

#include <algorithm>
#include <exception>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace
{
	bool SameCompiledBindingPath(
		CompiledBindingPathView left,
		CompiledBindingPathView right) noexcept
	{
		return left.Version == right.Version
			&& left.Steps.data() == right.Steps.data()
			&& left.Steps.size() == right.Steps.size();
	}

	void ValidateCompiledSelectedValuePath(CompiledBindingPathView path)
	{
		if (path.Version != CompiledBindingPathVersion)
			throw std::invalid_argument(
				"SelectedValuePath compiled path version is unsupported");
		for (const auto& step : path.Steps)
		{
			if (!HasCompiledBindingPathCapability(step.Capabilities,
				CompiledBindingPathCapabilities::Read))
				throw std::invalid_argument(
					"SelectedValuePath requires read capability");
			if (step.Kind == CompiledBindingPathStepKind::Property
				&& !step.Property)
				throw std::invalid_argument(
					"SelectedValuePath property steps require property tokens");
		}
	}

	void AppendObservation(
		BindingPathObservation& destination,
		BindingPathObservation source)
	{
		destination.Owners.insert(destination.Owners.end(),
			std::make_move_iterator(source.Owners.begin()),
			std::make_move_iterator(source.Owners.end()));
		destination.ListOwners.insert(destination.ListOwners.end(),
			std::make_move_iterator(source.ListOwners.begin()),
			std::make_move_iterator(source.ListOwners.end()));
		destination.Connections.insert(destination.Connections.end(),
			std::make_move_iterator(source.Connections.begin()),
			std::make_move_iterator(source.Connections.end()));
	}

	template<typename TValue>
	DependencyPropertyOptions<Selector, TValue> SelectorPropertyOptions(
		TValue defaultValue
		CUI_DESIGN_METADATA_ARGUMENTS(
			int order,
			DependencyPropertyEditorKind editor,
			DependencyPropertyPersistence persistence =
				DependencyPropertyPersistence::Metadata))
	{
		DependencyPropertyOptions<Selector, TValue> options;
		options.DefaultValue = std::move(defaultValue);
		options.Flags = DependencyPropertyFlags::AffectsRender;
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Data";
		options.Design.CategoryOrder = 80;
		options.Design.Order = order;
		options.Design.Editor = editor;
		options.Design.Persistence = persistence;
		)
		return options;
	}

}

ListBoxItem::ListBoxItem()
{
}

bool ListBoxItem::Initialize(
	const BindingSourceReference& item,
	const ItemTemplateReference& contentTemplate,
	CompiledBindingPathView displayMemberPath,
	size_t index,
	std::wstring* outError)
{
	return InitializeItem(item, contentTemplate, displayMemberPath,
		index, L"ListBoxItem", outError);
}

void ListBoxItem::RegisterDependencyProperties()
{
	ItemContainerControl::RegisterDependencyProperties();
}

std::unique_ptr<AutomationPeer>
ListBoxItem::OnCreateAutomationPeer()
{
	return std::make_unique<ListBoxItemAutomationPeer>(*this);
}

bool ListBoxItem::HandlesNavigationKey(Key key) const
{
	return key == Key::Space || key == Key::Return
		|| ItemContainerControl::HandlesNavigationKey(key);
}

bool ListBoxItem::ProcessInput(const InputReport& input)
{
	if (input.Kind == InputReportKind::KeyDown
		&& (input.Key == Key::Space || input.Key == Key::Return))
	{
		const ControlWeakReference lifetime(this);
		auto* list = dynamic_cast<ListBox*>(GetLogicalParent());
		if (list)
			(void)list->ProcessItemKey(ItemIndex(), input);
		else
			ActivateItem(MouseButton::None, input.Modifiers);
		auto* live = dynamic_cast<ListBoxItem*>(lifetime.Get());
		if (!live) return true;
		live->FocusOwner();
		live = dynamic_cast<ListBoxItem*>(lifetime.Get());
		if (!live) return true;
		auto args = input.CreateKeyEventArgs();
		live->OnKeyDown(live, args);
		return true;
	}
	return ItemContainerControl::ProcessInput(input);
}

void ListBoxItem::ActivateItem(
	MouseButton button,
	ModifierKeys modifiers)
{
	if (auto* owner = dynamic_cast<ListBox*>(GetLogicalParent()))
	{
		owner->NotifyItemClicked(ItemIndex(), button, modifiers);
		return;
	}
	if (auto* owner = dynamic_cast<Selector*>(GetLogicalParent()))
		owner->SelectIndex(static_cast<int>(ItemIndex()));
}

void ListBoxItem::FocusOwner()
{
	auto* owner = dynamic_cast<Selector*>(GetLogicalParent());
	if (owner && owner->GetPresentationWindow())
		owner->GetPresentationWindow()->SetKeyboardFocus(owner, true);
}

void ListBoxItem::OnIsSelectedRequested(bool value)
{
	auto* owner = dynamic_cast<Selector*>(GetLogicalParent());
	if (!owner) return;
	const int index = static_cast<int>(ItemIndex());
	if (auto* list = dynamic_cast<ListBox*>(owner))
	{
		list->RequestItemSelection(ItemIndex(), value);
		return;
	}
	if (value) (void)owner->SelectIndex(index);
	else if (owner->GetSelectedIndex() == index)
		(void)owner->SelectIndex(-1);
}

Selector::Selector()
	: ItemsControl()
{
	OnGotFocus += [this](Control*)
	{
		UpdateSelectionActiveState();
		UpdateContainerSelection();
	};
	OnLostFocus += [this](Control*)
	{
		UpdateSelectionActiveState();
		UpdateContainerSelection();
	};
	RetainEventConnection(OnPropertyValueChanged.Subscribe(
		[this](DependencyObject*,
			const DependencyPropertyChangedEventArgs& args)
		{
			if (args.Property == &Control::IsKeyboardFocusWithinProperty())
				UpdateSelectionActiveState();
		}));
	UpdateSelectionActiveState();
}

const DependencyProperty& Selector::SelectedIndexProperty()
{
	static const auto registration = []
	{
		auto options = SelectorPropertyOptions(
			-1 CUI_DESIGN_METADATA_ARGUMENTS(
				40, DependencyPropertyEditorKind::Number));
		options.Flags = options.Flags
			| DependencyPropertyFlags::BindsTwoWayByDefault;
		options.Coerce = [](
			Selector&, const int& proposed) -> std::optional<int>
		{
			return (std::max)(-1, proposed);
		};
		options.Changed = [](
			Selector& target, const int& oldValue, const int& newValue)
		{
			target.ApplySelectedIndexChange(oldValue, newValue);
		};
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Minimum = -1.0;
		options.Design.Step = 1.0;
		)
		return DependencyPropertyRegistry::RegisterStatic<Selector, int>(
			DependencyPropertyRegistrationLiteral(L"SelectedIndex"),
			[](Selector& target) { return target.GetSelectedIndex(); },
			[](Selector& target, const int& value)
			{ target.SetSelectedIndex(value); }, {}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& Selector::IsSynchronizedWithCurrentItemProperty()
{
	static const auto registration = []
	{
		auto options = SelectorPropertyOptions(
			false CUI_DESIGN_METADATA_ARGUMENTS(
				80, DependencyPropertyEditorKind::Boolean));
		options.Changed = [](
			Selector& target, const bool& oldValue, const bool&)
		{
			try
			{
				target.ReconnectCurrentView();
			}
			catch (...)
			{
				const auto failure = std::current_exception();
				if (!target._rollingBackCurrentItemSynchronizationProperty)
				{
					target._rollingBackCurrentItemSynchronizationProperty = true;
					try
					{
						(void)target.TrySetCurrentPropertyValue(
							IsSynchronizedWithCurrentItemProperty(),
							BindingValue(oldValue));
					}
					catch (...)
					{
						// Preserve the original subscription failure.
					}
					target._rollingBackCurrentItemSynchronizationProperty = false;
				}
				std::rethrow_exception(failure);
			}
		};
		return DependencyPropertyRegistry::RegisterStatic<Selector, bool>(
			DependencyPropertyRegistrationLiteral(
				L"IsSynchronizedWithCurrentItem"),
			[](Selector& target)
			{ return target.GetIsSynchronizedWithCurrentItem(); },
			[](Selector& target, const bool& value)
			{ target.SetIsSynchronizedWithCurrentItem(value); }, {},
			std::move(options));
	}();
	return *registration;
}

const DependencyProperty& Selector::SelectedItemProperty()
{
	static const auto registration = []
	{
		auto options = SelectorPropertyOptions(
			BindingValue{} CUI_DESIGN_METADATA_ARGUMENTS(
				60, DependencyPropertyEditorKind::Auto,
				DependencyPropertyPersistence::Native));
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Browsable = false;
		)
		return DependencyPropertyRegistry::RegisterStatic<Selector, BindingValue>(
			DependencyPropertyRegistrationLiteral(L"SelectedItem"),
			[](Selector& target) { return target.GetSelectedItem(); },
			[](Selector& target, const BindingValue& value)
			{ target.SetSelectedItem(value); },
			[](Selector& target,
				DependencyPropertyMetadata::ChangeHandler handler,
				DataSourceUpdateMode)
			{
				return target._selectedItemChanged.Subscribe(
					[handler = std::move(handler)](Selector*) { handler(); });
			}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& Selector::SelectedValueProperty()
{
	static const auto registration = []
	{
		auto options = SelectorPropertyOptions(
			BindingValue{} CUI_DESIGN_METADATA_ARGUMENTS(
				70, DependencyPropertyEditorKind::Auto,
				DependencyPropertyPersistence::Native));
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Browsable = false;
		)
		return DependencyPropertyRegistry::RegisterStatic<Selector, BindingValue>(
			DependencyPropertyRegistrationLiteral(L"SelectedValue"),
			[](Selector& target) { return target.GetSelectedValue(); },
			[](Selector& target, const BindingValue& value)
			{ target.SetSelectedValue(value); },
			[](Selector& target,
				DependencyPropertyMetadata::ChangeHandler handler,
				DataSourceUpdateMode)
			{
				return target._selectedValueChanged.Subscribe(
					[handler = std::move(handler)](Selector*) { handler(); });
			}, std::move(options));
	}();
	return *registration;
}

const DependencyPropertyKey& Selector::IsSelectionActivePropertyKey()
{
	static const auto registration = []
	{
		DependencyPropertyOptions<Control, bool> options;
		options.DefaultValue = false;
		options.Flags = DependencyPropertyFlags::Inherits
			| DependencyPropertyFlags::AffectsRender;
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Browsable = false;
		options.Design.Category = L"State";
		options.Design.Persistence = DependencyPropertyPersistence::Transient;
		)
		return DependencyPropertyRegistry::RegisterReadOnlyStatic<Control, bool>(
			DependencyPropertyRegistrationLiteral(L"IsSelectionActive"),
			std::move(options));
	}();
	return registration.Key();
}

void Selector::RegisterDependencyProperties()
{
	ItemsControl::RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	RegisterDesignDependencyProperties();
#endif
}

void Selector::SetIsSelectionActive(
	Control& target,
	bool value)
{
	(void)cui::framework::DependencyPropertyAccess::SetReadOnlyValue(
		target,
		IsSelectionActivePropertyKey(),
		BindingValue(value));
}

void Selector::VisitDeclaredInheritedProperties(
	void* context, InheritedPropertyVisitor visitor) const
{
	ItemsControl::VisitDeclaredInheritedProperties(context, visitor);
	if (visitor)
		visitor(context, IsSelectionActivePropertyKey().Property());
}

int Selector::ClampIndex(int value) const noexcept
{
	if (value < 0) return -1;
	const size_t count = SelectionItemCount();
	if (count == 0) return GetItemsView() ? -1 : value;
	return (std::min)(value, static_cast<int>(count) - 1);
}

size_t Selector::SelectionItemCount() const noexcept
{
	return ItemsControl::ItemCount();
}

void Selector::SetSelectedIndex(int value)
{
	(void)SetPropertyField(
		SelectedIndexProperty(), _selectedIndex, ClampIndex(value));
}

bool Selector::SelectIndex(int value)
{
	const int normalized = ClampIndex(value);
	if (_selectedIndex == normalized) return false;
	SetCurrentSelectedIndex(normalized);
	return true;
}

bool Selector::IsIndexSelected(size_t index) const noexcept
{
	return _selectedIndex >= 0
		&& static_cast<size_t>(_selectedIndex) == index;
}

BindingValue Selector::GetSelectedItem() const
{
	if (!GetItemsView() && _selectedAuthoredItemIdentity)
		return BindingValue(
			static_cast<Control*>(_selectedAuthoredItemIdentity));
	return _selectedItemIdentity
		? BindingValue(_selectedItemIdentity)
		: BindingValue{};
}

void Selector::SetSelectedItem(const BindingValue& value)
{
	if (value.Empty())
	{
		SetCurrentSelectedIndex(-1);
		return;
	}
	if (!GetItemsView())
	{
		Control* item = nullptr;
		if (!value.TryGet(item) || !item)
		{
			SetCurrentSelectedIndex(-1);
			return;
		}
		int index = -1;
		for (size_t candidate = 0; candidate < AuthoredItemCount(); ++candidate)
		{
			if (GetAuthoredItem(candidate) != item) continue;
			index = static_cast<int>(candidate);
			break;
		}
		SetCurrentSelectedIndex(index);
		return;
	}
	BindingSourceReference item;
	if (!value.TryGet(item) || !item || !GetItemsView())
	{
		SetCurrentSelectedIndex(-1);
		return;
	}
	SetCurrentSelectedIndex(FindBindingListItemByValue(
		GetItemsView(), CompiledBindingPathView{}, BindingValue(item)));
}

void Selector::SetCompiledSelectedValuePath(
	CompiledBindingPathView value)
{
	ValidateCompiledSelectedValuePath(value);
	if (SameCompiledBindingPath(_compiledSelectedValuePath, value)
#if CUI_ENABLE_DYNAMIC_XAML
		&& _selectedValuePath.empty()
#endif
		) return;
	_compiledSelectedValuePath = value;
#if CUI_ENABLE_DYNAMIC_XAML
	_selectedValuePath.clear();
#endif
	const auto source = GetPropertyValueSource(SelectedValueProperty());
	BindingValue configured;
	if (source != DependencyPropertyValueSource::Default
		&& TryGetPropertyValue(SelectedValueProperty(), source, configured))
		SetSelectedValue(configured);
	else
		RefreshSelectedItemState(false);
}

bool Selector::HasSelectedValuePath() const noexcept
{
	if (!_compiledSelectedValuePath.Empty()) return true;
#if CUI_ENABLE_DYNAMIC_XAML
	return HasAuthoredSelectedValuePath();
#else
	return false;
#endif
}

bool Selector::TryReadSelectedValue(
	IBindingSource& item, BindingValue& value) const
{
	if (!_compiledSelectedValuePath.Empty())
		return TryGetBindingPathValue(
			item, _compiledSelectedValuePath, value);
#if CUI_ENABLE_DYNAMIC_XAML
	return TryReadAuthoredSelectedValue(item, value);
#else
	return false;
#endif
}

BindingPathObservation Selector::ObserveItemProjectionPaths(
	const BindingSourceReference& item,
	std::function<void()> changed) const
{
	auto result = ObserveDisplayMemberPath(item, changed);
	if (!_compiledSelectedValuePath.Empty())
		AppendObservation(result, ObserveBindingPaths(
			item, { _compiledSelectedValuePath }, std::move(changed)));
#if CUI_ENABLE_DYNAMIC_XAML
	else AppendObservation(result, ObserveAuthoredSelectedValuePath(
		item, std::move(changed)));
#endif
	return result;
}

BindingValue Selector::GetSelectedValue() const
{
	if (_selectedIndex < 0) return {};
	if (!GetItemsView())
	{
		auto* item = GetAuthoredItem(static_cast<size_t>(_selectedIndex));
		if (!item) return {};
		if (!HasSelectedValuePath())
			return BindingValue(static_cast<Control*>(item));
		BindingValue value;
		return TryReadSelectedValue(*item, value)
			? value : BindingValue{};
	}
	BindingValue value;
#if CUI_ENABLE_DYNAMIC_XAML
	if (_compiledSelectedValuePath.Empty())
		return TryReadAuthoredSelectedValueAt(
			static_cast<size_t>(_selectedIndex), value)
			? value : BindingValue{};
#endif
	return TryGetBindingListItemValue(GetItemsView(),
		static_cast<size_t>(_selectedIndex),
		_compiledSelectedValuePath, value)
		? value : BindingValue{};
}

void Selector::SetSelectedValue(const BindingValue& value)
{
	if (!GetItemsView())
	{
		if (value.Empty())
		{
			SetCurrentSelectedIndex(-1);
			return;
		}
		int match = -1;
		for (size_t index = 0; index < AuthoredItemCount(); ++index)
		{
			auto* item = GetAuthoredItem(index);
			if (!item) continue;
			BindingValue candidate;
			if (!HasSelectedValuePath())
				candidate = BindingValue(static_cast<Control*>(item));
			else if (!TryReadSelectedValue(*item, candidate))
				continue;
			if (!BindingValuesEqual(candidate, value)) continue;
			match = static_cast<int>(index);
			break;
		}
		SetCurrentSelectedIndex(match);
		return;
	}
	if (value.Empty())
	{
		SetCurrentSelectedIndex(-1);
		return;
	}
#if CUI_ENABLE_DYNAMIC_XAML
	if (_compiledSelectedValuePath.Empty())
	{
		SetCurrentSelectedIndex(FindAuthoredSelectedValue(value));
		return;
	}
#endif
	SetCurrentSelectedIndex(FindBindingListItemByValue(
		GetItemsView(), _compiledSelectedValuePath, value));
}

void Selector::SetIsSynchronizedWithCurrentItem(bool value)
{
	(void)SetPropertyField(
		IsSynchronizedWithCurrentItemProperty(),
		_isSynchronizedWithCurrentItem,
		value);
}

bool Selector::ApplyItemContainerStyle()
{
	for (size_t index = 0; index < ItemCount(); ++index)
	{
		if (auto* container = GetGeneratedItem(index))
		{
			cui::framework::StyleAccess::SetResourceKey(
				*container, GetItemContainerStyle());
			if (!cui::framework::StyleAccess::HasVisibleStyleRules(*container))
				continue;
			if (!cui::framework::StyleAccess::Refresh(*container, true)) return false;
		}
	}
	return true;
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
	std::unique_ptr<ListBoxItem> container;
	if (_itemContainerTemplate)
	{
		if (_itemContainerTemplate.Get()->TargetType()
			!= UIClass::UI_ListBoxItem)
		{
			SetLastTemplateError(
				L"ItemContainerTemplate TargetType 必须是 ListBoxItem。");
			return {};
		}
		std::wstring error;
		auto built = _itemContainerTemplate.Get()->Build(&error);
		auto* itemContainer = dynamic_cast<ListBoxItem*>(built.get());
		if (!itemContainer)
		{
			SetLastTemplateError(error.empty()
				? L"ItemContainerTemplate 未生成 ListBoxItem。" : error);
			return {};
		}
		container.reset(static_cast<ListBoxItem*>(built.release()));
	}
	else container = std::make_unique<ListBoxItem>();
	cui::framework::StyleAccess::SetResourceKey(
		*container, GetItemContainerStyle());
	std::wstring error;
	bool initialized = false;
#if CUI_ENABLE_DYNAMIC_XAML
	if (GetCompiledDisplayMemberPath().Empty())
		initialized = InitializeAuthoredGeneratedContainer(
			*container, item, index, error);
	else
#endif
		initialized = container->Initialize(
			item, GetItemTemplate(), GetCompiledDisplayMemberPath(), index, &error);
	if (!initialized)
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

std::unique_ptr<ItemsControl::ItemsSourceTransactionState>
Selector::CaptureItemsSourceTransactionState()
{
	// Allocate before moving the observation so an allocation failure leaves
	// the established selection fully intact.
	auto state =
		std::make_unique<SelectionItemsSourceTransactionState>();
	state->SelectedIndex = _selectedIndex;
	state->SelectedItemIdentity = _selectedItemIdentity;
	state->SelectedItemObservation = std::move(_selectedItemObservation);
	return state;
}

void Selector::RestoreItemsSourceTransactionState(
	ItemsSourceTransactionState& state) noexcept
{
	auto* selection =
		dynamic_cast<SelectionItemsSourceTransactionState*>(&state);
	if (!selection) return;

	const bool selectionChanged =
		_selectedIndex != selection->SelectedIndex
		|| _selectedItemIdentity.Shared()
			!= selection->SelectedItemIdentity.Shared();
	if (selectionChanged)
	{
		int restoredIndex = -1;
		try
		{
			const auto source = GetItemsView();
			BindingSourceReference indexedItem;
			if (selection->SelectedItemIdentity
				&& source && selection->SelectedIndex >= 0
				&& static_cast<size_t>(selection->SelectedIndex)
					< source.Get()->Count()
				&& source.Get()->TryGetItem(
					static_cast<size_t>(selection->SelectedIndex), indexedItem)
				&& indexedItem.Shared()
					== selection->SelectedItemIdentity.Shared())
			{
				restoredIndex = selection->SelectedIndex;
			}
			else if (selection->SelectedItemIdentity)
				restoredIndex = FindBindingListItemByValue(
					source, CompiledBindingPathView{},
					BindingValue(selection->SelectedItemIdentity));
			if (restoredIndex < 0)
				restoredIndex = ClampIndex(selection->SelectedIndex);
			// Restore the effective SelectedIndex layer as well as the backing
			// field. ItemsControl keeps its update-depth guard active here, so
			// this cannot feed the old current view back into itself.
			SetCurrentSelectedIndex(restoredIndex);
		}
		catch (...)
		{
			// Preserve the original source-switch failure. The exact identity
			// and observation are restored below without invoking user code.
			restoredIndex = selection->SelectedIndex;
			_selectedIndex = restoredIndex;
		}
		selection->SelectedIndex = restoredIndex;
	}

	// Reuse the original observation instead of rebuilding it during failure
	// handling. This both disconnects the candidate item and makes rollback
	// allocation-free after the transaction snapshot has been captured.
	_selectedItemObservation =
		std::move(selection->SelectedItemObservation);
	_selectedItemIdentity = selection->SelectedItemIdentity;
	_selectedIndex = selection->SelectedIndex;
	try { UpdateContainerSelection(); }
	catch (...) {}
}

void Selector::OnGeneratedItemsRealized()
{
	UpdateContainerSelection();
}

void Selector::OnTextSearchMatch(size_t index)
{
	if (index < SelectionItemCount())
		(void)SelectIndex(static_cast<int>(index));
}

void Selector::OnGeneratedItemIndexChanged(
	Control&,
	size_t,
	size_t newIndex)
{
	if (auto* item = dynamic_cast<ItemContainerControl*>(
		GetGeneratedItem(newIndex)))
		item->SetItemIndex(newIndex);
}

void Selector::OnAuthoredItemsChanged() noexcept
{
	int requested = -1;
	for (size_t index = 0; index < AuthoredItemCount(); ++index)
	{
		auto* container = dynamic_cast<ItemContainerControl*>(
			GetAuthoredItem(index));
		if (!container) continue;
		container->SetItemIndex(index);
		if (container->GetIsSelected()
			&& container != _selectedAuthoredItemIdentity)
			requested = static_cast<int>(index);
	}

	int restored = -1;
	if (requested >= 0)
		restored = requested;
	else if (_selectedAuthoredItemIdentity)
	{
		for (size_t index = 0; index < AuthoredItemCount(); ++index)
		{
			if (GetAuthoredItem(index) != _selectedAuthoredItemIdentity)
				continue;
			restored = static_cast<int>(index);
			break;
		}
	}
	if (restored < 0)
	{
		const auto source = GetPropertyValueSource(SelectedIndexProperty());
		BindingValue configured;
		int configuredIndex = -1;
		if (source != DependencyPropertyValueSource::Default
			&& TryGetPropertyValue(SelectedIndexProperty(), source, configured)
			&& configured.TryGet(configuredIndex)
			&& configuredIndex >= 0
			&& (static_cast<size_t>(configuredIndex) < AuthoredItemCount()
				|| _selectedIndex == configuredIndex))
		{
			restored = configuredIndex;
		}
	}
	if (restored < 0) restored = ClampIndex(_selectedIndex);
	try
	{
		if (_selectedIndex != restored)
			SetCurrentSelectedIndex(restored);
		else
			RefreshSelectedItemState(false);
	}
	catch (...) {}
}

void Selector::OnItemsSourceChanged(
	const BindingListReference&,
	const BindingListReference&)
{
	ReconnectCurrentView();
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
		SelectedIndexProperty(), _selectedIndex, normalized);
}

void Selector::SetCurrentSelectedIndexWithoutSelectionChanged(
	int value)
{
	const bool previous = _suppressSelectionChanged;
	_suppressSelectionChanged = true;
	try
	{
		SetCurrentSelectedIndex(value);
		_suppressSelectionChanged = previous;
	}
	catch (...)
	{
		_suppressSelectionChanged = previous;
		throw;
	}
}

void Selector::ApplySelectedIndexChange(int oldValue, int newValue)
{
	if (oldValue == newValue) return;
	OnSelectedIndexChanged(oldValue, newValue);
	RefreshSelectedItemState(
		!_suppressSelectionChanged, oldValue);
	EnsureSelectedItemVisible();
	SynchronizeCurrentViewFromSelection();
}

BindingValue Selector::GetSelectionItemAt(size_t index) const
{
	if (const auto source = GetItemsView())
	{
		BindingSourceReference item;
		if (index < source.Get()->Count()
			&& source.Get()->TryGetItem(index, item)
			&& item)
			return BindingValue(std::move(item));
		return {};
	}
	if (auto* item = GetAuthoredItem(index))
		return BindingValue(item);
	return {};
}

void Selector::RaiseSelectionChanged(
	int oldIndex,
	int newIndex,
	std::vector<BindingValue> removedItems,
	std::vector<BindingValue> addedItems)
{
	if (_suppressSelectionChanged) return;
	SelectionChangedEventArgs args(
		oldIndex, newIndex,
		std::move(removedItems),
		std::move(addedItems));
	SelectionChanged(this, args);
}

void Selector::RestoreSelectionAfterRebuild()
{
	int restored = -1;
	BindingValue configured;
	const auto itemSource = GetPropertyValueSource(SelectedItemProperty());
	if (itemSource != DependencyPropertyValueSource::Default
		&& TryGetPropertyValue(SelectedItemProperty(), itemSource, configured)
		&& !configured.Empty())
		restored = FindBindingListItemByValue(
			GetItemsView(), CompiledBindingPathView{}, configured);
	if (restored < 0)
	{
		const auto valueSource = GetPropertyValueSource(SelectedValueProperty());
		if (valueSource != DependencyPropertyValueSource::Default
			&& TryGetPropertyValue(
				SelectedValueProperty(), valueSource, configured)
			&& !configured.Empty())
		{
#if CUI_ENABLE_DYNAMIC_XAML
			if (_compiledSelectedValuePath.Empty())
				restored = FindAuthoredSelectedValue(configured);
			else
#endif
				restored = FindBindingListItemByValue(
					GetItemsView(), _compiledSelectedValuePath, configured);
		}
	}
	if (restored < 0 && _selectedItemIdentity)
		restored = FindBindingListItemByValue(
			GetItemsView(), CompiledBindingPathView{},
			BindingValue(_selectedItemIdentity));
	if (restored < 0)
	{
		const auto indexSource = GetPropertyValueSource(SelectedIndexProperty());
		int configuredIndex = -1;
		if (indexSource != DependencyPropertyValueSource::Default
			&& TryGetPropertyValue(
				SelectedIndexProperty(), indexSource, configured)
			&& configured.TryGet(configuredIndex)
			&& configuredIndex >= 0
			&& static_cast<size_t>(configuredIndex) < ItemCount())
		{
			restored = configuredIndex;
		}
	}
	if (restored < 0) restored = ClampIndex(_selectedIndex);

	if (_selectedIndex != restored)
		SetCurrentSelectedIndex(restored);
	else
		RefreshSelectedItemState(false);
}

void Selector::RefreshSelectedItemState(
	bool raiseSelectionChanged,
	std::optional<int> previousIndex)
{
	const BindingValue previousItem = GetSelectedItem();
	BindingSourceReference next;
	const auto source = GetItemsView();
	if (source && _selectedIndex >= 0)
		(void)source.Get()->TryGetItem(
			static_cast<size_t>(_selectedIndex), next);
	Control* nextAuthored = nullptr;
	if (!source && _selectedIndex >= 0)
		nextAuthored = GetAuthoredItem(static_cast<size_t>(_selectedIndex));
	const bool itemChanged = next.Shared() != _selectedItemIdentity.Shared()
		|| nextAuthored != _selectedAuthoredItemIdentity;
	_selectedItemIdentity = std::move(next);
	_selectedAuthoredItemIdentity = nextAuthored;
	if (!_compiledSelectedValuePath.Empty())
		_selectedItemObservation = ObserveBindingPaths(
			_selectedItemIdentity, { _compiledSelectedValuePath },
			[this] { NotifySelectionProjectionsChanged(); });
#if CUI_ENABLE_DYNAMIC_XAML
	else _selectedItemObservation = ObserveAuthoredSelectedValuePath(
		_selectedItemIdentity,
		[this] { NotifySelectionProjectionsChanged(); });
#else
	else _selectedItemObservation = {};
#endif
	UpdateContainerSelection();
	NotifySelectionProjectionsChanged();
	if ((raiseSelectionChanged || itemChanged)
		&& !_suppressSelectionChanged)
	{
		const BindingValue currentItem = GetSelectedItem();
		std::vector<BindingValue> removed;
		std::vector<BindingValue> added;
		if (!previousItem.Empty()) removed.push_back(previousItem);
		if (!currentItem.Empty()) added.push_back(currentItem);
		RaiseSelectionChanged(
			previousIndex.value_or(_selectedIndex),
			_selectedIndex,
			std::move(removed),
			std::move(added));
	}
}

void Selector::UpdateContainerSelection()
{
	for (size_t index = 0; index < AuthoredItemCount(); ++index)
	{
		if (auto* container =
			dynamic_cast<ItemContainerControl*>(
				GetAuthoredItem(index)))
			container->SetCurrentIsSelected(
				IsIndexSelected(index));
	}
	for (size_t index = 0; index < ItemCount(); ++index)
	{
		if (auto* container = dynamic_cast<ItemContainerControl*>(
			GetGeneratedItem(index)))
		{
			container->SetCurrentIsSelected(
				IsIndexSelected(index));
		}
	}
}

void Selector::UpdateSelectionActiveState()
{
	SetIsSelectionActive(*this, IsKeyboardFocusWithin);
}

void Selector::EnsureSelectedItemVisible()
{
	if (_selectedIndex < 0) return;
	(void)BringItemIntoView(static_cast<size_t>(_selectedIndex));
}

bool Selector::HandlesNavigationKey(Key key) const
{
	switch (key)
	{
	case Key::Up:
	case Key::Down:
	case Key::Home:
	case Key::End:
	case Key::PageUp:
	case Key::PageDown:
		return true;
	default:
		return ItemsControl::HandlesNavigationKey(key);
	}
}

bool Selector::ProcessInput(const InputReport& input)
{
	if (input.Kind == InputReportKind::KeyDown && ItemCount() != 0)
	{
		const int count = static_cast<int>(ItemCount());
		int next = _selectedIndex;
		switch (input.Key)
		{
		case Key::Up: next = _selectedIndex < 0 ? count - 1 : _selectedIndex - 1; break;
		case Key::Down: next = _selectedIndex < 0 ? 0 : _selectedIndex + 1; break;
		case Key::Home: next = 0; break;
		case Key::End: next = count - 1; break;
		case Key::PageUp: next = _selectedIndex < 0 ? 0 : _selectedIndex - 5; break;
		case Key::PageDown: next = _selectedIndex < 0 ? 0 : _selectedIndex + 5; break;
		default: return Control::ProcessInput(input);
		}
		if (count > 0) SelectIndex((std::clamp)(next, 0, count - 1));
		auto args = input.CreateKeyEventArgs();
		OnKeyDown(this, args);
		return true;
	}
	return Control::ProcessInput(input);
}

void Selector::NotifySelectionProjectionsChanged()
{
	auto synchronize = [this](
		const DependencyProperty& property,
		const BindingValue& current)
	{
		const auto source = GetPropertyValueSource(property);
		if (source == DependencyPropertyValueSource::Default) return;
		BindingValue stored;
		if (!TryGetPropertyValue(property, source, stored)
			|| !BindingItemValuesEqual(stored, current))
			(void)TrySetCurrentPropertyValue(property, current);
	};
	synchronize(SelectedItemProperty(), GetSelectedItem());
	synchronize(SelectedValueProperty(), GetSelectedValue());
	cui::framework::EventAccess::Raise(_selectedItemChanged, this);
	cui::framework::EventAccess::Raise(_selectedValueChanged, this);
}

void Selector::ReconnectCurrentView()
{
	if (!_isSynchronizedWithCurrentItem)
	{
		_currentViewChanged.Disconnect();
		_pendingCurrentItemSynchronization =
			CurrentItemSynchronizationDirection::None;
		return;
	}
	const auto source = GetItemsView();
	auto* view = source
		? dynamic_cast<IBindingListCurrentView*>(source.Get())
		: nullptr;
	EventConnection replacement;
	if (view)
	{
		// A user implementation may invoke the handler synchronously from
		// SubscribeCurrentChanged() and then throw. Quarantine that callback as
		// a pending request until ownership of the returned connection commits;
		// on failure, restore the exact pre-subscription synchronization state.
		const bool wasSynchronizing = _synchronizingCurrentItem;
		const auto previousPending = _pendingCurrentItemSynchronization;
		_synchronizingCurrentItem = true;
		try
		{
			replacement = view->SubscribeCurrentChanged(
				[this] { SynchronizeSelectionFromCurrentView(); });
		}
		catch (...)
		{
			_synchronizingCurrentItem = wasSynchronizing;
			_pendingCurrentItemSynchronization = previousPending;
			throw;
		}
		_synchronizingCurrentItem = wasSynchronizing;
		_pendingCurrentItemSynchronization = previousPending;
	}

	// Subscribe before touching the established connection. This is the
	// Selector half of ItemsControl's source transaction: a throwing candidate
	// subscription leaves the old source's currency connection intact, so the
	// reverse OnItemsSourceChanged(new, old) hook can restore coherently.
	auto previous = std::move(_currentViewChanged);
	_currentViewChanged = std::move(replacement);
	if (!view) return;
	try
	{
		SynchronizeSelectionFromCurrentView();
	}
	catch (...)
	{
		_currentViewChanged = std::move(previous);
		_pendingCurrentItemSynchronization =
			CurrentItemSynchronizationDirection::None;
		throw;
	}
}

void Selector::SynchronizeSelectionFromCurrentView()
{
	if (!_isSynchronizedWithCurrentItem) return;
	RequestCurrentItemSynchronization(
		CurrentItemSynchronizationDirection::SelectionFromView);
}

void Selector::SynchronizeCurrentViewFromSelection()
{
	if (!_isSynchronizedWithCurrentItem
		|| IsItemsSourceUpdateInProgress()) return;
	RequestCurrentItemSynchronization(
		CurrentItemSynchronizationDirection::CurrentViewFromSelection);
}

void Selector::RequestCurrentItemSynchronization(
	CurrentItemSynchronizationDirection direction)
{
	constexpr size_t maximumConvergenceIterations = 64;
	if (_synchronizingCurrentItem)
	{
		// Event handlers are allowed to change either currency or selection.
		// Preserve the last nested request so the two projections converge after
		// the current notification unwinds instead of permanently diverging.
		_pendingCurrentItemSynchronization = direction;
		return;
	}

	auto next = direction;
	size_t iterations = 0;
	while (next != CurrentItemSynchronizationDirection::None)
	{
		if (iterations++ >= maximumConvergenceIterations)
		{
			// Host event handlers can intentionally redirect selection and
			// currency forever. Bound one synchronous convergence pass so such
			// an adversarial cycle cannot hang the UI thread. A later independent
			// selection/currency change may start a fresh bounded pass.
			_pendingCurrentItemSynchronization =
				CurrentItemSynchronizationDirection::None;
			break;
		}
		_pendingCurrentItemSynchronization =
			CurrentItemSynchronizationDirection::None;
		_synchronizingCurrentItem = true;
		try
		{
			if (next == CurrentItemSynchronizationDirection::SelectionFromView)
				ApplySelectionFromCurrentView();
			else
				ApplyCurrentViewFromSelection();
			_synchronizingCurrentItem = false;
		}
		catch (...)
		{
			_synchronizingCurrentItem = false;
			_pendingCurrentItemSynchronization =
				CurrentItemSynchronizationDirection::None;
			throw;
		}
		next = std::exchange(
			_pendingCurrentItemSynchronization,
			CurrentItemSynchronizationDirection::None);
		if (!_isSynchronizedWithCurrentItem)
			next = CurrentItemSynchronizationDirection::None;
	}
}

void Selector::ApplySelectionFromCurrentView()
{
	const auto source = GetItemsView();
	auto* view = source
		? dynamic_cast<IBindingListCurrentView*>(source.Get())
		: nullptr;
	if (!view) return;

	int selectedIndex = -1;
	const auto currentItem = view->CurrentItem();
	const int currentPosition = view->CurrentPosition();
	if (currentItem)
	{
		BindingSourceReference positionedItem;
		if (currentPosition >= 0
			&& static_cast<size_t>(currentPosition) < source.Get()->Count()
			&& source.Get()->TryGetItem(
				static_cast<size_t>(currentPosition), positionedItem)
			&& positionedItem.Shared() == currentItem.Shared())
		{
			selectedIndex = currentPosition;
		}
		else
		{
			selectedIndex = FindBindingListItemByValue(
				source, CompiledBindingPathView{}, BindingValue(currentItem));
		}
	}

	SetCurrentSelectedIndex(selectedIndex);
}

void Selector::ApplyCurrentViewFromSelection()
{
	const auto source = GetItemsView();
	auto* view = source
		? dynamic_cast<IBindingListCurrentView*>(source.Get())
		: nullptr;
	if (!view || view->CurrentPosition() == _selectedIndex) return;

	if (!view->MoveCurrentToPosition(_selectedIndex))
	{
		const int currentPosition = view->CurrentPosition();
		SetCurrentSelectedIndex(currentPosition);
	}
}
