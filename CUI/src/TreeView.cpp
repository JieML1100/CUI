#include "TreeView.h"
#include "EventInfrastructure.h"
#include "StyleInfrastructure.h"

#include "ScrollViewer.h"
#include "TemplateInfrastructure.h"
#include "ToggleButton.h"
#include "Window.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cwctype>
#include <stdexcept>
#include <unordered_set>

namespace
{
	constexpr int DefaultTreeChevronSlot = 18;

	bool SameCompiledBindingPath(
		CompiledBindingPathView left,
		CompiledBindingPathView right) noexcept
	{
		return left.Version == right.Version
			&& left.Steps.data() == right.Steps.data()
			&& left.Steps.size() == right.Steps.size();
	}

	void ValidateCompiledTreeValuePath(CompiledBindingPathView path)
	{
		if (path.Version != CompiledBindingPathVersion)
			throw std::invalid_argument(
				"TreeView.SelectedValuePath compiled version is unsupported");
		for (const auto& step : path.Steps)
		{
			if (!HasCompiledBindingPathCapability(step.Capabilities,
				CompiledBindingPathCapabilities::Read)
				|| (step.Kind == CompiledBindingPathStepKind::Property
					&& !step.Property))
				throw std::invalid_argument(
					"TreeView.SelectedValuePath requires readable property tokens");
		}
	}

	template<typename TValue>
	DependencyPropertyOptions<TreeViewItem, TValue> TreeItemStateOptions(
		TValue defaultValue,
		bool readOnly
		CUI_DESIGN_METADATA_ARGUMENTS(
			int order,
			DependencyPropertyPersistence persistence))
	{
		DependencyPropertyOptions<TreeViewItem, TValue> options;
		options.DefaultValue = std::move(defaultValue);
		options.Flags = DependencyPropertyFlags::AffectsMeasure
			| DependencyPropertyFlags::AffectsArrange
			| DependencyPropertyFlags::AffectsRender;
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"State";
		options.Design.CategoryOrder = 70;
		options.Design.Order = order;
		options.Design.Browsable = !readOnly;
		options.Design.Persistence = persistence;
		)
		options.IsReadOnly = readOnly;
		return options;
	}

	bool PointInRect(float x, float y, const D2D1_RECT_F& rect) noexcept
	{
		return x >= rect.left && x <= rect.right
			&& y >= rect.top && y <= rect.bottom;
	}

}

const DependencyProperty& TreeViewItem::IsExpandedProperty()
{
	static const auto registration =
		DependencyPropertyRegistry::RegisterStatic<TreeViewItem, bool>(
			DependencyPropertyRegistrationLiteral(L"IsExpanded"),
			[](TreeViewItem& target) { return target.GetIsExpanded(); },
			[](TreeViewItem& target, const bool& value)
			{ target.ApplyIsExpandedValue(value); },
			[](TreeViewItem& target,
				DependencyPropertyMetadata::ChangeHandler handler,
				DataSourceUpdateMode)
			{
				return target._expandedChanged.Subscribe(
					[handler = std::move(handler)](TreeViewItem*) { handler(); });
			}, TreeItemStateOptions(
				false, false CUI_DESIGN_METADATA_ARGUMENTS(
					10, DependencyPropertyPersistence::Metadata)));
	return *registration;
}

const DependencyProperty& TreeViewItem::HasItemsProperty()
{
	return HasItemsPropertyKey().Property();
}

const DependencyProperty& TreeViewItem::IsSelectedProperty()
{
	static const auto registration =
		DependencyPropertyRegistry::RegisterStatic<TreeViewItem, bool>(
			DependencyPropertyRegistrationLiteral(L"IsSelected"),
			[](TreeViewItem& target) { return target.GetIsSelected(); },
			[](TreeViewItem& target, const bool& value)
			{ target.ApplyIsSelectedValue(value); },
			[](TreeViewItem& target,
				DependencyPropertyMetadata::ChangeHandler handler,
				DataSourceUpdateMode)
			{
				return target._selectedChanged.Subscribe(
					[handler = std::move(handler)](TreeViewItem*) { handler(); });
			}, TreeItemStateOptions(
				false, false CUI_DESIGN_METADATA_ARGUMENTS(
					40, DependencyPropertyPersistence::Metadata)));
	return *registration;
}

const DependencyPropertyKey& TreeViewItem::HasItemsPropertyKey()
{
	static const auto registration =
		DependencyPropertyRegistry::RegisterReadOnlyStatic<TreeViewItem, bool>(
			DependencyPropertyRegistrationLiteral(L"HasItems"),
			[](TreeViewItem& target) { return target.GetHasItems(); },
			[](TreeViewItem& target, const bool& value)
			{
				(void)target.SetReadOnlyPropertyField(
					HasItemsPropertyKey(), target._hasItems, value);
			},
			[](TreeViewItem& target,
				DependencyPropertyMetadata::ChangeHandler handler,
				DataSourceUpdateMode)
			{
				return target._hasItemsChanged.Subscribe(
					[handler = std::move(handler)](TreeViewItem*) { handler(); });
			}, TreeItemStateOptions(
				false, true CUI_DESIGN_METADATA_ARGUMENTS(
					20, DependencyPropertyPersistence::Transient)));
	return registration.Key();
}

const DependencyPropertyKey& TreeViewItem::LevelPropertyKey()
{
	static const auto registration =
		DependencyPropertyRegistry::RegisterReadOnlyStatic<TreeViewItem, int>(
			DependencyPropertyRegistrationLiteral(L"Level"),
			[](TreeViewItem& target) { return target.GetLevel(); },
			[](TreeViewItem& target, const int& value)
			{
				(void)target.SetReadOnlyPropertyField(
					LevelPropertyKey(), target._level, value);
			},
			[](TreeViewItem& target,
				DependencyPropertyMetadata::ChangeHandler handler,
				DataSourceUpdateMode)
			{
				return target._levelChanged.Subscribe(
					[handler = std::move(handler)](TreeViewItem*) { handler(); });
			}, TreeItemStateOptions(
				0, true CUI_DESIGN_METADATA_ARGUMENTS(
					30, DependencyPropertyPersistence::Transient)));
	return registration.Key();
}

TreeViewItem::TreeViewItem()
	: HeaderedItemsControl()
{
#if CUI_ENABLE_DYNAMIC_XAML
	EnsureBindingPropertiesRegistered();
#endif
	_accessibilityId = AllocateAccessibilityVirtualId();
	RetainEventConnection(OnMouseDown.Subscribe(
		[this](Control*, MouseEventArgs& args)
		{
			HandleHeaderPointer(args, false);
		}));
	RetainEventConnection(OnMouseDoubleClick.Subscribe(
		[this](Control*, MouseEventArgs& args)
		{
			HandleHeaderPointer(args, true);
		}));
	ApplyExpansionPresentation();
}

void TreeViewItem::RegisterDependencyProperties()
{
	HeaderedItemsControl::RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	RegisterDesignDependencyProperties();
#endif
}

cui::core::Insets
TreeViewItem::GetHeaderPresentationInsets() const noexcept
{
	return {};
}

cui::core::Insets
TreeViewItem::GetItemsPresentationInsets() const noexcept
{
	return {};
}

bool TreeViewItem::ValidateAuthoredItemControl(
	const Control& item, std::string& error) const
{
	if (dynamic_cast<const TreeViewItem*>(&item)) return true;
	error = "TreeViewItem authored Items must be TreeViewItem controls";
	return false;
}

BindingListReference TreeViewItem::GetItemsSource() const noexcept
{
	if (_generatedContainer && _hierarchicalItemsSource)
		return _hierarchicalItemsSource;
	return HeaderedItemsControl::GetItemsSource();
}

void TreeViewItem::SetItemsSource(BindingListReference value)
{
	if (!_generatedContainer)
	{
		HeaderedItemsControl::SetItemsSource(std::move(value));
		SyncHasItems();
		if (_owner && !_initializingGeneratedContainer)
			_owner->RefreshHierarchy();
		return;
	}

	_hierarchicalItemsObservation = {};
	_hierarchicalItemsSource = std::move(value);
	RefreshHierarchicalCollectionObservation();
	if (_expanded || GetMaterializedItemsSource())
		HeaderedItemsControl::SetItemsSource(_hierarchicalItemsSource);
	SyncHasItems();
	if (_owner && !_initializingGeneratedContainer)
		_owner->RefreshHierarchy();
}

size_t TreeViewItem::ItemCount() const noexcept
{
	if (_generatedContainer && _hierarchicalItemsSource)
		return _hierarchicalItemsSource.Get()->Count();
	return HeaderedItemsControl::ItemCount();
}

TreeViewItem* TreeViewItem::ContainerFromIndex(size_t index) const noexcept
{
	if (_generatedContainer && _hierarchicalItemsSource
		&& !GetMaterializedItemsSource()) return nullptr;
	return dynamic_cast<TreeViewItem*>(
		HeaderedItemsControl::GetGeneratedItem(index));
}

bool TreeViewItem::InitializeGenerated(
	TreeView& owner,
	const ItemsControl& projectionOwner,
	const BindingSourceReference& item,
	ItemTemplateReference headerTemplate,
	CompiledBindingPathView displayMemberPath,
	int level,
	std::wstring* outError)
{
	if (outError) outError->clear();
	_initializingGeneratedContainer = true;
	_generatedContainer = true;
	_owner = &owner;
	_parentItem = nullptr;
	_dataItem = item;
	_headerDataTemplate = std::move(headerTemplate);
	if (!SetReadOnlyPropertyField(
		LevelPropertyKey(),
		_level, level))
	{
		if (outError) *outError =
			L"TreeViewItem Level 只读属性状态无法发布。";
		_initializingGeneratedContainer = false;
		return false;
	}
	SetHeaderTypeToken(_headerDataTemplate
		? _headerDataTemplate.Get()->GetDataTypeToken()
		: DataTypeToken{});
	if (!displayMemberPath.Empty())
	{
		SetCompiledDisplayMemberPath(displayMemberPath);
		SetCompiledHeaderDisplayMemberPath(displayMemberPath);
	}
#if CUI_ENABLE_DYNAMIC_XAML
	owner.ApplyAuthoredGeneratedContainerProjection(
		*this, projectionOwner);
#endif
	SetHeaderTemplate(_headerDataTemplate);
	if (!SetDataContext(item))
	{
		if (outError) *outError =
			L"TreeViewItem 无法采用数据项 DataContext。";
		_initializingGeneratedContainer = false;
		return false;
	}
	SetHeader(BindingValue(item));
	if (!LastHeaderError().empty())
	{
		if (outError) *outError = LastHeaderError();
		_initializingGeneratedContainer = false;
		return false;
	}
	if (!RefreshHierarchicalItemsSource(outError))
	{
		_initializingGeneratedContainer = false;
		return false;
	}
	_initializingGeneratedContainer = false;
	ApplyExpansionPresentation();
	SyncHasItems();
	return true;
}

bool TreeViewItem::RefreshHierarchicalItemsSource(std::wstring* outError)
{
	if (outError) outError->clear();
	BindingListReference source;
	if (_headerDataTemplate && _headerDataTemplate.Get()->IsHierarchical())
	{
		if (!_headerDataTemplate.Get()->TryGetVisualChildItemsSource(
			_dataItem, source, outError)) return false;
	}

	BindingPathObservation observation;
	if (_headerDataTemplate && _headerDataTemplate.Get()->IsHierarchical())
	{
		const ControlWeakReference self(this);
		observation = _headerDataTemplate.Get()->ObserveChildItemsSource(
			_dataItem,
			[self]
			{
				auto* item = dynamic_cast<TreeViewItem*>(self.Get());
				if (!item) return;
				std::wstring error;
				if (!item->RefreshHierarchicalItemsSource(&error)
					&& !error.empty()) item->SetLastTemplateError(std::move(error));
			});
	}

	const bool materialized = static_cast<bool>(GetMaterializedItemsSource());
	_hierarchicalItemsSource = std::move(source);
	_hierarchicalItemsObservation = std::move(observation);
	RefreshHierarchicalCollectionObservation();
	if ((_expanded || materialized)
		&& GetMaterializedItemsSource() != _hierarchicalItemsSource)
	{
		HeaderedItemsControl::SetItemsSource(_hierarchicalItemsSource);
		if (!LastTemplateError().empty())
		{
			if (outError) *outError = LastTemplateError();
			return false;
		}
	}
	else if (GetMaterializedItemsSource() == _hierarchicalItemsSource)
	{
		SetLastTemplateError({});
	}
	if (_owner && LastTemplateError().empty()) _owner->SetHierarchyError({});
	SyncHasItems();
	if (_owner && !_initializingGeneratedContainer)
		_owner->RefreshHierarchy();
	return true;
}

void TreeViewItem::RefreshHierarchicalCollectionObservation()
{
	_hierarchicalItemsChanged.Disconnect();
	if (!_hierarchicalItemsSource) return;
	auto* sourceIdentity = _hierarchicalItemsSource.Get();
	const ControlWeakReference self(this);
	_hierarchicalItemsChanged = sourceIdentity->SubscribeChanged(
		[self, sourceIdentity](const CollectionChangedEventArgs&)
		{
			auto* item = dynamic_cast<TreeViewItem*>(self.Get());
			if (!item
				|| item->_hierarchicalItemsSource.Get() != sourceIdentity) return;
			item->SyncHasItems();
		});
}

bool TreeViewItem::EnsureChildrenRealized()
{
	if (!_generatedContainer || !_hierarchicalItemsSource) return true;
	if (GetMaterializedItemsSource() == _hierarchicalItemsSource) return true;
	_materializingExpansionChildren = true;
	HeaderedItemsControl::SetItemsSource(_hierarchicalItemsSource);
	_materializingExpansionChildren = false;
	if (!LastTemplateError().empty()) return false;
	if (_owner) _owner->BindExpandedSubtree(*this);
	return true;
}

bool TreeViewItem::HierarchyMigrationInProgress() const noexcept
{
	if (_owner && _owner->AuthoredHierarchyMigrationInProgress())
		return true;
	for (auto* current = this; current;
		current = current->_parentItem)
		if (current->IsAuthoredItemsMigrationInProgress())
			return true;
	return false;
}

void TreeViewItem::CollectMaterializedChildContainers(
	std::vector<TreeViewItem*>& output) const
{
	if (_generatedContainer && _hierarchicalItemsSource
		&& !GetMaterializedItemsSource()) return;
	for (size_t index = 0;
		index < HeaderedItemsControl::ItemCount();
		++index)
	{
		auto* child = dynamic_cast<TreeViewItem*>(
			HeaderedItemsControl::GetGeneratedItem(index));
		if (child) output.push_back(child);
	}
}

void TreeViewItem::BindHierarchy(
	TreeView& owner, TreeViewItem* parent, int level)
{
	_owner = &owner;
	_parentItem = parent;
	if (_level != level)
	{
		(void)SetReadOnlyPropertyField(
			LevelPropertyKey(),
			_level, level);
		cui::framework::EventAccess::Raise(_levelChanged, this);
	}
	SyncHasItems();
	ApplyExpansionPresentation();
}

void TreeViewItem::UnbindHierarchy() noexcept
{
	_owner = nullptr;
	_parentItem = nullptr;
}

void TreeViewItem::ApplyIsExpandedValue(bool value)
{
	if (_expanded == value) return;
	const bool restoreFocus =
		!value && ShouldRestoreFocusAfterCollapse();
	if (!SetPropertyField(IsExpandedProperty(), _expanded, value)) return;
	cui::framework::EventAccess::Raise(_expandedChanged, this);
	RoutedEventArgs args;
	if (value) Expanded(this, args);
	else Collapsed(this, args);
	ApplyExpansionPresentation();
	if (_owner)
	{
		if (value) _owner->BindExpandedSubtree(*this);
		else (void)_owner->HandleSelectionAndCollapsed(*this);
		// Expansion mutates the realized hierarchy and therefore the geometry
		// used by the very next pointer hit test.  Commit the owning ItemsControl
		// boundary now so rendering and input cannot observe the old row map.
		_owner->UpdateLayout();
	}
	RestoreFocusAfterCollapse(restoreFocus);
}

void TreeViewItem::SyncHasItems()
{
	const auto source = GetItemsSource();
	const bool value = source
		? source.Get()->Count() != 0 : AuthoredItemCount() != 0;
	if (_hasItems == value) return;
	if (!SetReadOnlyPropertyField(
		HasItemsPropertyKey(),
		_hasItems, value)) return;
	cui::framework::EventAccess::Raise(_hasItemsChanged, this);
}

void TreeViewItem::ApplyIsSelectedValue(bool value)
{
	if (_selected == value) return;
	if (!SetPropertyField(IsSelectedProperty(), _selected, value)) return;
	SetStyleState(ControlStyleState::Selected, value);
	cui::framework::EventAccess::Raise(_selectedChanged, this);
	RoutedEventArgs args;
	if (value) Selected(this, args);
	else Unselected(this, args);
}

void TreeViewItem::OnIsMouseOverChanged(bool, bool)
{
}

void TreeViewItem::ApplyExpansionPresentation()
{
	if (_expanderPart)
		(void)_expanderPart->TrySetCurrentPropertyValue(
			ToggleButton::IsCheckedProperty(),
			BindingValue(NullableBool(_expanded)));
	// The ItemsPresenter owns template chrome, while the generated ItemsHost is
	// the semantic layout boundary for descendants. Keep that internal host in
	// lockstep with IsExpanded as well, so custom templates and lazy hierarchy
	// realization cannot leave collapsed children participating in layout.
	if (auto* itemsHost = GetItemsHost())
		(void)itemsHost->TrySetCurrentPropertyValue(
			Control::VisibilityProperty(),
			BindingValue(_expanded ? L"Visible" : L"Collapsed"));
	RequestLayout();
	InvalidateVisual();
}

void TreeViewItem::OnControlTemplatePresentationChanged()
{
	_expanderClickConnection.Disconnect();
	_expanderPart = nullptr;
	HeaderedItemsControl::OnControlTemplatePresentationChanged();
	_expanderPart = dynamic_cast<ToggleButton*>(
		FindDeclarativeTemplatePart(
			MakeTemplatePartToken(L"PART_Expander")));
	if (_expanderPart)
	{
		const ControlWeakReference lifetime(this);
		_expanderClickConnection = _expanderPart->Click.Subscribe(
			[lifetime](Control* sender, RoutedEventArgs&)
			{
				auto* item =
					dynamic_cast<TreeViewItem*>(lifetime.Get());
				auto* expander = item ? item->_expanderPart : nullptr;
				if (!item || sender != expander) return;
				item->SetCurrentIsExpanded(
					expander->IsChecked == true);
			});
	}
	ApplyExpansionPresentation();
}

Control* TreeViewItem::GetHeaderInteractionVisual() noexcept
{
	if (auto* chrome =
		FindDeclarativeTemplatePart(
			MakeTemplatePartToken(L"PART_HeaderChrome")))
		return chrome;
	return GetHeaderVisual();
}

bool TreeViewItem::IsExpanderSource(Control* source) const noexcept
{
	if (!_expanderPart || !source) return false;
	for (auto* current = source; current;
		current = current->GetRoutedParent())
	{
		if (current == _expanderPart) return true;
		if (current == this) break;
	}
	return false;
}

void TreeViewItem::HandleHeaderPointer(
	MouseEventArgs& args,
	bool doubleClick)
{
	if (args.Handled || args.ChangedButton != MouseButton::Left
		|| !_owner || !IsEffectivelyEnabled()) return;
	if (IsExpanderSource(args.OriginalSource)) return;
	auto* header = GetHeaderInteractionVisual();
	bool withinHeader = false;
	for (auto* current = args.OriginalSource; current;
		current = current->GetRoutedParent())
	{
		if (current == header)
		{
			withinHeader = true;
			break;
		}
		if (current == this) break;
	}
	if (!withinHeader && args.OriginalSource != this) return;
	// A direct report targets the behavior host when no finer template hit was
	// supplied (including the native fallback path). Preserve the TreeView
	// chevron slot contract without competing with an actual PART_Expander,
	// whose routed Click owns template-authored input.
	if (!doubleClick && args.OriginalSource == this && _hasItems
		&& args.X >= 0 && args.X <= DefaultTreeChevronSlot)
	{
		SetCurrentIsExpanded(!_expanded);
		args.Handled = true;
		return;
	}
	const ControlWeakReference lifetime(this);
	if (!_owner->FocusAndSelectItem(this, true)) return;
	auto* live = dynamic_cast<TreeViewItem*>(lifetime.Get());
	if (!live) return;
	if (doubleClick && live->_hasItems)
		live->SetCurrentIsExpanded(!live->_expanded);
	args.Handled = true;
}

bool TreeViewItem::ShouldRestoreFocusAfterCollapse() const noexcept
{
	if (!_owner || !_expanded) return false;
	auto* focused = GetPresentationWindow()
		? GetPresentationWindow()->GetKeyboardFocusedElement()
		: nullptr;
	if (!focused || focused == this) return false;
	for (auto* current = focused; current;
		current = current->GetRoutedParent())
	{
		if (current == this) return true;
	}
	return false;
}

void TreeViewItem::RestoreFocusAfterCollapse(bool requested)
{
	if (requested && _owner)
		(void)_owner->FocusAndSelectItem(this, true);
}

bool TreeViewItem::HandlesNavigationKey(Key key) const
{
	return _owner
		? _owner->HandlesNavigationKey(key)
		: HeaderedItemsControl::HandlesNavigationKey(key);
}

bool TreeViewItem::ProcessInput(const InputReport& input)
{
	if (input.Kind == InputReportKind::KeyDown && _owner)
	{
		if (_owner->ProcessItemNavigationKey(this, input))
		{
			auto args = input.CreateKeyEventArgs();
			OnKeyDown(this, args);
			return true;
		}
	}
	return HeaderedItemsControl::ProcessInput(input);
}

void TreeViewItem::SetIsExpanded(bool value)
{
	if (value && !EnsureChildrenRealized()) return;
	(void)SetPropertyField(IsExpandedProperty(), _expanded, value);
}

void TreeViewItem::SetCurrentIsExpanded(bool value)
{
	if (value && !EnsureChildrenRealized()) return;
	(void)SetCurrentPropertyField(IsExpandedProperty(), _expanded, value);
}

void TreeViewItem::SetIsSelected(bool value)
{
	if (!SetPropertyField(IsSelectedProperty(), _selected, value)) return;
	if (_owner)
	{
		if (value) (void)_owner->SelectItem(this, true);
		else if (_owner->GetSelectedContainer() == this)
			(void)_owner->SelectItem(nullptr, false);
		return;
	}
}

void TreeViewItem::SetCurrentIsSelected(bool value)
{
	(void)SetCurrentPropertyField(IsSelectedProperty(), _selected, value);
}

std::unique_ptr<Control> TreeViewItem::BuildGeneratedItem(
	const BindingSourceReference& item,
	size_t,
	BindingPathObservation& observation)
{
	observation = {};
	if (!_owner)
	{
		SetLastTemplateError(L"TreeViewItem 尚未连接所属 TreeView。");
		return {};
	}
	for (auto* ancestor = this; ancestor; ancestor = ancestor->_parentItem)
	{
		if (!ancestor->_dataItem || ancestor->_dataItem.Get() != item.Get())
			continue;
		auto error = std::wstring(L"TreeView 数据层级不能形成循环。");
		SetLastTemplateError(error);
		_owner->SetHierarchyError(std::move(error));
		return {};
	}
	auto source = GetMaterializedItemsSource();
	std::wstring error;
	auto container = _owner->CreateGeneratedContainer(
		source, item, GetItemTemplate(), *this,
		GetCompiledDisplayMemberPath(),
		GetItemContainerStyle(),
		_level + 1, &error);
	if (!container)
	{
		SetLastTemplateError(error);
		_owner->SetHierarchyError(std::move(error));
	}
	else _owner->SetHierarchyError({});
	return container;
}

void TreeViewItem::OnBeforeGeneratedItemsRebuilt()
{
	if (_owner && !_initializingGeneratedContainer
		&& !_materializingExpansionChildren
		&& !HierarchyMigrationInProgress())
		_owner->PrepareHierarchyMutation();
}

void TreeViewItem::OnGeneratedItemsRebuilt()
{
	SyncHasItems();
	if (_owner && !_initializingGeneratedContainer
		&& !_materializingExpansionChildren
		&& !HierarchyMigrationInProgress())
		_owner->CompleteHierarchyMutation();
}

void TreeViewItem::OnGeneratedItemsRealized()
{
	SyncHasItems();
	if (_owner && !_initializingGeneratedContainer
		&& !_materializingExpansionChildren
		&& !HierarchyMigrationInProgress())
		_owner->RefreshHierarchy();
}

void TreeViewItem::OnAuthoredItemsChanged() noexcept
{
	SyncHasItems();
	if (_owner && !_initializingGeneratedContainer
		&& !_materializingExpansionChildren
		&& !HierarchyMigrationInProgress())
		_owner->RefreshHierarchy();
}

TreeView::TreeView()
	: ItemsControl()
{
#if CUI_ENABLE_DYNAMIC_XAML
	EnsureBindingPropertiesRegistered();
#endif
}

TreeView::~TreeView()
{
	for (const auto& reference : _boundContainers)
		if (auto* item = dynamic_cast<TreeViewItem*>(reference.Get()))
			item->UnbindHierarchy();
	_boundContainers.clear();
	_boundContainerIndex.clear();
}

const DependencyPropertyKey& TreeView::SelectedItemPropertyKey()
{
	static const auto registration = []
	{
		DependencyPropertyOptions<TreeView, BindingValue> options;
		options.DefaultValue = BindingValue{};
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Data";
		options.Design.CategoryOrder = 80;
		options.Design.Order = 50;
		options.Design.Browsable = false;
		options.Design.Persistence = DependencyPropertyPersistence::Transient;
		)
		return DependencyPropertyRegistry::RegisterReadOnlyStatic<
			TreeView, BindingValue>(
				DependencyPropertyRegistrationLiteral(L"SelectedItem"),
				[](TreeView& target) { return target.GetSelectedItem(); }, {},
				[](TreeView& target,
					DependencyPropertyMetadata::ChangeHandler handler,
					DataSourceUpdateMode)
				{
					return target._selectedItemChanged.Subscribe(
						[handler = std::move(handler)](TreeView*) { handler(); });
				}, std::move(options));
	}();
	return registration.Key();
}

const DependencyPropertyKey& TreeView::SelectedValuePropertyKey()
{
	static const auto registration = []
	{
		DependencyPropertyOptions<TreeView, BindingValue> options;
		options.DefaultValue = BindingValue{};
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Data";
		options.Design.CategoryOrder = 80;
		options.Design.Order = 60;
		options.Design.Browsable = false;
		options.Design.Persistence = DependencyPropertyPersistence::Transient;
		)
		return DependencyPropertyRegistry::RegisterReadOnlyStatic<
			TreeView, BindingValue>(
				DependencyPropertyRegistrationLiteral(L"SelectedValue"),
				[](TreeView& target) { return target.GetSelectedValue(); }, {},
				[](TreeView& target,
					DependencyPropertyMetadata::ChangeHandler handler,
					DataSourceUpdateMode)
				{
					return target._selectedValueChanged.Subscribe(
						[handler = std::move(handler)](TreeView*) { handler(); });
				}, std::move(options));
	}();
	return registration.Key();
}

void TreeView::RegisterDependencyProperties()
{
	ItemsControl::RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	RegisterDesignDependencyProperties();
#endif
}

bool TreeView::ValidateAuthoredItemControl(
	const Control& item, std::string& error) const
{
	if (dynamic_cast<const TreeViewItem*>(&item)) return true;
	error = "TreeView authored Items must be TreeViewItem controls";
	return false;
}

ItemTemplateReference TreeView::ResolveDataItemTemplate(
	const BindingListReference& source,
	const ItemTemplateReference& localTemplate,
	int level,
	std::wstring* outError) const
{
	if (outError) outError->clear();
	if (!source) return {};
	const auto itemType = source.Get()->GetItemTypeToken();
	ItemTemplateReference result = localTemplate;
	if (!result && level != 0)
	{
		const auto rootTemplate = GetItemTemplate();
		if (rootTemplate && AreDataTypesCompatible(
			itemType, rootTemplate.Get()->GetDataTypeToken()))
			result = rootTemplate;
	}
	if (!result && _compiledImplicitItemTemplateResolver)
		result = _compiledImplicitItemTemplateResolver(itemType);
#if CUI_ENABLE_DYNAMIC_XAML
	if (!result) result = ResolveAuthoredImplicitItemTemplate(source);
#endif
	if (result && !AreDataTypesCompatible(
		itemType, result.Get()->GetDataTypeToken()))
	{
		if (outError) *outError =
			L"TreeView ItemTemplate DataType 与 ItemsSource ItemType 不一致。";
#if CUI_ENABLE_DYNAMIC_XAML
		if (outError) AppendAuthoredItemTypeDiagnostic(source, *outError);
#endif
		return {};
	}
	return result;
}

std::unique_ptr<TreeViewItem> TreeView::CreateGeneratedContainer(
	const BindingListReference& source,
	const BindingSourceReference& item,
	const ItemTemplateReference& localTemplate,
	const ItemsControl& projectionOwner,
	CompiledBindingPathView displayMemberPath,
	const std::wstring& containerStyle,
	int level,
	std::wstring* outError)
{
	if (outError) outError->clear();
	std::unique_ptr<TreeViewItem> container;
	if (_itemContainerTemplate)
	{
		if (_itemContainerTemplate.Get()->TargetType()
			!= UIClass::UI_TreeViewItem)
		{
			if (outError) *outError =
				L"ItemContainerStyle.Template TargetType 必须是 TreeViewItem。";
			return {};
		}
		std::wstring error;
		auto built = _itemContainerTemplate.Get()->Build(&error);
		auto* typed = dynamic_cast<TreeViewItem*>(built.get());
		if (!typed)
		{
			if (outError) *outError = error.empty()
				? L"ItemContainerStyle.Template 未生成 TreeViewItem。"
				: std::move(error);
			return {};
		}
		container.reset(static_cast<TreeViewItem*>(built.release()));
	}
	else container = std::make_unique<TreeViewItem>();

	std::wstring templateError;
	auto itemTemplate = ResolveDataItemTemplate(
		source, localTemplate, level, &templateError);
	if (!templateError.empty())
	{
		if (outError) *outError = std::move(templateError);
		return {};
	}
	cui::framework::StyleAccess::SetResourceKey(
		*container, containerStyle);
	if (!container->InitializeGenerated(
		*this, projectionOwner, item,
		std::move(itemTemplate), displayMemberPath,
		level, outError)) return {};
	return container;
}

std::unique_ptr<Control> TreeView::BuildGeneratedItem(
	const BindingSourceReference& item,
	size_t,
	BindingPathObservation& observation)
{
	observation = {};
	std::wstring error;
	auto container = CreateGeneratedContainer(
		GetItemsSource(), item, GetItemTemplate(), *this,
		GetCompiledDisplayMemberPath(),
		GetItemContainerStyle(), 0, &error);
	if (!container) SetLastTemplateError(std::move(error));
	return container;
}

void TreeView::SetItemsSource(BindingListReference value)
{
	ItemsControl::SetItemsSource(std::move(value));
	RefreshHierarchy();
}

void TreeView::SetItemTemplate(ItemTemplateReference value)
{
	ItemsControl::SetItemTemplate(std::move(value));
	RebuildAuthoredDataDescendants();
	RefreshHierarchy();
}

void TreeView::SetCompiledDisplayMemberPath(
	CompiledBindingPathView value)
{
	ItemsControl::SetCompiledDisplayMemberPath(value);
	RebuildAuthoredDataDescendants();
	RefreshHierarchy();
}

void TreeView::SetCompiledImplicitItemTemplateResolver(
	CompiledImplicitItemTemplateResolver value)
{
	_compiledImplicitItemTemplateResolver = std::move(value);
	if (GetItemsSource()) (void)RebuildGeneratedItems();
	RebuildAuthoredDataDescendants();
	RefreshHierarchy();
}

void TreeView::SetItemContainerTemplate(ControlTemplateReference value)
{
	if (_itemContainerTemplate == value) return;
	auto previous = _itemContainerTemplate;
	_itemContainerTemplate = std::move(value);
	if ((!GetItemsSource() || RebuildGeneratedItems()))
	{
		RebuildAuthoredDataDescendants();
		RefreshHierarchy();
		return;
	}
	const auto error = LastTemplateError();
	_itemContainerTemplate = std::move(previous);
	(void)RebuildGeneratedItems();
	RebuildAuthoredDataDescendants();
	SetLastTemplateError(error);
}

TreeViewItem* TreeView::ContainerFromIndex(size_t index) const noexcept
{
	return dynamic_cast<TreeViewItem*>(ItemsControl::GetGeneratedItem(index));
}

void TreeView::PrepareHierarchyMutation()
{
	if (_hierarchyMutationDepth++ != 0) return;
	_selectionRestoreIdentity = {};
	_selectionRestoreAuthored = nullptr;
	if (!_selectedContainer) return;
	if (_selectedContainer->_dataItem)
		_selectionRestoreIdentity = _selectedContainer->_dataItem;
	else _selectionRestoreAuthored = ControlWeakReference(_selectedContainer);
}

void TreeView::CompleteHierarchyMutation()
{
	if (_hierarchyMutationDepth == 0)
	{
		RefreshHierarchy();
		return;
	}
	if (--_hierarchyMutationDepth == 0) RefreshHierarchy(true);
}

void TreeView::CollectContainers(
	TreeViewItem* parent,
	int level,
	std::vector<TreeViewItem*>& output)
{
	const size_t count = parent ? parent->ItemCount() : ItemCount();
	for (size_t index = 0; index < count; ++index)
	{
		auto* item = parent
			? parent->ContainerFromIndex(index)
			: dynamic_cast<TreeViewItem*>(GetGeneratedItem(index));
		if (!item) continue;
		item->BindHierarchy(*this, parent, level);
		output.push_back(item);
		CollectContainers(item, level + 1, output);
	}
}

void TreeView::CollectMaterializedChildren(
	TreeViewItem* parent,
	std::vector<TreeViewItem*>& output) const
{
	if (!parent) return;
	std::vector<TreeViewItem*> children;
	parent->CollectMaterializedChildContainers(children);
	for (auto* child : children)
	{
		if (!child) continue;
		output.push_back(child);
		CollectMaterializedChildren(child, output);
	}
}

void TreeView::BindExpandedSubtree(TreeViewItem& parent)
{
	if (parent._owner != this) return;
	std::vector<TreeViewItem*> descendants;
	CollectContainers(&parent, parent._level + 1, descendants);
	for (auto* item : descendants)
	{
		if (!item || !_boundContainerIndex.insert(item).second) continue;
		_boundContainers.emplace_back(item);
	}
	if (_selectedContainer
		&& !_boundContainerIndex.contains(_selectedContainer))
		(void)HandleSelectionAndCollapsed(parent);
	InvalidateVisual();
}

void TreeView::RefreshHierarchy(bool restoreSelection)
{
	TreeViewItem* previousLive = nullptr;
	for (const auto& reference : _boundContainers)
	{
		auto* item = dynamic_cast<TreeViewItem*>(reference.Get());
		if (!item) continue;
		if (item == _selectedContainer) previousLive = item;
		item->UnbindHierarchy();
	}
	_boundContainers.clear();
	_boundContainerIndex.clear();

	std::vector<TreeViewItem*> containers;
	CollectContainers(nullptr, 0, containers);
	_boundContainers.reserve(containers.size());
	_boundContainerIndex.reserve(containers.size());
	for (auto* item : containers)
	{
		_boundContainers.emplace_back(item);
		_boundContainerIndex.insert(item);
	}

	auto contains = [&](const TreeViewItem* candidate)
	{
		return candidate
			&& _boundContainerIndex.contains(
				const_cast<TreeViewItem*>(candidate));
	};
	TreeViewItem* candidate = contains(_selectedContainer)
		? _selectedContainer : nullptr;
	if (!candidate && restoreSelection && _selectionRestoreIdentity)
	{
		const auto identity = _selectionRestoreIdentity.Get();
		const auto found = std::find_if(
			containers.begin(), containers.end(),
			[identity](const TreeViewItem* item)
			{ return item && item->_dataItem.Get() == identity; });
		if (found != containers.end()) candidate = *found;
	}
	if (!candidate && restoreSelection)
	{
		auto* authored = dynamic_cast<TreeViewItem*>(
			_selectionRestoreAuthored.Get());
		if (contains(authored)) candidate = authored;
	}
	if (!candidate)
	{
		const auto selected = std::find_if(
			containers.begin(), containers.end(),
			[](const TreeViewItem* item)
			{ return item && item->GetIsSelected(); });
		if (selected != containers.end()) candidate = *selected;
	}

	const auto previousRaw = _selectedContainer;
	const BindingValue previousItem = GetSelectedItem();
	_selectedContainer = candidate;
	for (auto* item : containers)
		item->SetCurrentIsSelected(item == candidate);
	if (previousLive && previousLive != candidate
		&& !contains(previousLive)) previousLive->SetCurrentIsSelected(false);

	_selectionRestoreIdentity = {};
	_selectionRestoreAuthored = nullptr;
	if (previousRaw != candidate)
	{
		RoutedPropertyChangedEventArgs<BindingValue> args(
			previousItem, GetSelectedItem());
		SelectedItemChanged(this, args);
		NotifySelectionProjectionChanged(true);
	}
	else RefreshSelectedItemObservation();
	InvalidateVisual();
}

bool TreeView::ContainsContainer(const TreeViewItem* item) const noexcept
{
	return item && item->_owner == this
		&& _boundContainerIndex.contains(
			const_cast<TreeViewItem*>(item));
}

void TreeView::CollectVisibleContainers(std::vector<TreeViewItem*>& output)
{
	std::function<void(ItemsControl&, TreeViewItem*)> collect =
		[&](ItemsControl& owner, TreeViewItem* parent)
		{
			for (size_t index = 0; index < owner.ItemCount(); ++index)
			{
				auto* item = parent
					? parent->ContainerFromIndex(index)
					: dynamic_cast<TreeViewItem*>(owner.GetGeneratedItem(index));
				if (!item || !item->IsVisible) continue;
				output.push_back(item);
				if (item->GetIsExpanded()) collect(*item, item);
			}
		};
	collect(*this, nullptr);
}

BindingValue TreeView::GetSelectedItem() const
{
	if (!_selectedContainer) return {};
	if (_selectedContainer->_dataItem)
		return BindingValue(_selectedContainer->_dataItem);
	return BindingValue(static_cast<Control*>(_selectedContainer));
}

BindingValue TreeView::GetSelectedValue() const
{
	if (!_selectedContainer) return {};
#if CUI_ENABLE_DYNAMIC_XAML
	if (_compiledSelectedValuePath.Empty()
		&& HasAuthoredSelectedValuePath())
		return ReadAuthoredSelectedValue();
#endif
	if (_compiledSelectedValuePath.Empty()) return GetSelectedItem();
	BindingValue result;
	if (_selectedContainer->_dataItem)
		return TryGetBindingPathValue(
			*_selectedContainer->_dataItem.Get(),
			_compiledSelectedValuePath, result)
			? result : BindingValue{};
	return TryGetBindingPathValue(
		*_selectedContainer, _compiledSelectedValuePath, result)
		? result : BindingValue{};
}

void TreeView::SetCompiledSelectedValuePath(
	CompiledBindingPathView value)
{
	ValidateCompiledTreeValuePath(value);
	if (SameCompiledBindingPath(_compiledSelectedValuePath, value)
#if CUI_ENABLE_DYNAMIC_XAML
		&& _selectedValuePath.empty()
#endif
		) return;
	_compiledSelectedValuePath = value;
#if CUI_ENABLE_DYNAMIC_XAML
	_selectedValuePath.clear();
#endif
	RefreshSelectedItemObservation();
	cui::framework::EventAccess::Raise(_selectedValueChanged, this);
}

void TreeView::RefreshSelectedItemObservation()
{
	_selectedItemObservation = {};
	if (!_selectedContainer || !_selectedContainer->_dataItem) return;
	auto changed = [this]
	{
		RefreshSelectedItemObservation();
		cui::framework::EventAccess::Raise(_selectedValueChanged, this);
	};
	if (!_compiledSelectedValuePath.Empty())
		_selectedItemObservation = ObserveBindingPaths(
			_selectedContainer->_dataItem,
			{ _compiledSelectedValuePath }, std::move(changed));
#if CUI_ENABLE_DYNAMIC_XAML
	else _selectedItemObservation =
		ObserveAuthoredSelectedValuePath(std::move(changed));
#endif
}

void TreeView::NotifySelectionProjectionChanged(bool itemChanged)
{
	RefreshSelectedItemObservation();
	if (itemChanged)
		cui::framework::EventAccess::Raise(_selectedItemChanged, this);
	cui::framework::EventAccess::Raise(_selectedValueChanged, this);
}

bool TreeView::BringItemIntoView(
	TreeViewItem& item, bool expandAncestors)
{
	if (!ContainsContainer(&item)) return false;
	if (expandAncestors)
		for (auto* parent = item._parentItem; parent;
			parent = parent->_parentItem) parent->SetCurrentIsExpanded(true);
	UpdateLayout();
	for (auto* current = item.GetVisualParent(); current;
		current = current->GetVisualParent())
		if (auto* scroll = dynamic_cast<ScrollViewer*>(current))
			return scroll->BringDescendantIntoView(&item);
	return true;
}

bool TreeView::ApplySelection(TreeViewItem* item, bool bringIntoView)
{
	if (item && !ContainsContainer(item)) return false;
	if (_selectedContainer == item)
	{
		if (item && bringIntoView)
			(void)BringItemIntoView(*item, true);
		return false;
	}
	auto* previous = _selectedContainer;
	const BindingValue previousItem = GetSelectedItem();
	_selectedContainer = item;
	if (previous) previous->SetCurrentIsSelected(false);
	if (item)
	{
		item->SetCurrentIsSelected(true);
		if (bringIntoView) (void)BringItemIntoView(*item, true);
	}
	RoutedPropertyChangedEventArgs<BindingValue> args(
		previousItem, GetSelectedItem());
	SelectedItemChanged(this, args);
	NotifySelectionProjectionChanged(true);
	InvalidateVisual();
	return true;
}

bool TreeView::SelectItem(TreeViewItem* item, bool bringIntoView)
{
	return ApplySelection(item, bringIntoView);
}

bool TreeView::FocusAndSelectItem(
	TreeViewItem* item,
	bool bringIntoView)
{
	if (!item || !ContainsContainer(item)
		|| !item->IsVisible || !item->IsEffectivelyEnabled())
		return false;
	const ControlWeakReference lifetime(item);
	(void)ApplySelection(item, bringIntoView);
	item = dynamic_cast<TreeViewItem*>(lifetime.Get());
	return item && item->Focus();
}

bool TreeView::HandleSelectionAndCollapsed(TreeViewItem& collapsed)
{
	if (!_selectedContainer || _selectedContainer == &collapsed)
		return false;
	for (auto* parent = _selectedContainer->_parentItem;
		parent; parent = parent->_parentItem)
	{
		if (parent != &collapsed) continue;
		return FocusAndSelectItem(&collapsed, true);
	}
	return false;
}

void TreeView::RestoreFocusOnPointerDown()
{
	if (_selectedContainer
		&& FocusAndSelectItem(_selectedContainer, false)) return;
	(void)Focus();
}

ScrollViewer* TreeView::GetScrollHost() noexcept
{
	if (auto* part = dynamic_cast<ScrollViewer*>(
		FindDeclarativeTemplatePart(
			MakeTemplatePartToken(L"PART_ScrollViewer"))))
		return part;
	for (auto* current = GetVisualParent(); current;
		current = current->GetVisualParent())
		if (auto* scroll = dynamic_cast<ScrollViewer*>(current))
			return scroll;
	return nullptr;
}

bool TreeView::HandleScrollKey(Key key)
{
	auto* scroll = GetScrollHost();
	if (!scroll) return false;
	switch (key)
	{
	case Key::PageUp:
		scroll->PageUp();
		return true;
	case Key::PageDown:
		scroll->PageDown();
		return true;
	case Key::Home:
		scroll->ScrollToHome();
		return true;
	case Key::End:
		scroll->ScrollToEnd();
		return true;
	default:
		return false;
	}
}

bool TreeView::ExpandSubtree(TreeViewItem* item)
{
	if (!item || !ContainsContainer(item)) return false;
	if (item->GetHasItems())
		item->SetCurrentIsExpanded(true);
	std::vector<TreeViewItem*> children;
	item->CollectMaterializedChildContainers(children);
	bool expanded = item->GetHasItems();
	for (auto* child : children)
		expanded = ExpandSubtree(child) || expanded;
	return expanded;
}

bool TreeView::ProcessItemNavigationKey(
	TreeViewItem* origin,
	const InputReport& input)
{
	if (input.Kind != InputReportKind::KeyDown
		|| !HandlesNavigationKey(input.Key)) return false;
	if (!origin || !ContainsContainer(origin))
		origin = _selectedContainer;
	if (input.Key == Key::Space)
		return origin && FocusAndSelectItem(origin, true);
	if (input.Key == Key::Add)
	{
		if (origin && origin->GetHasItems())
			origin->SetCurrentIsExpanded(true);
		return true;
	}
	if (input.Key == Key::Subtract)
	{
		if (origin) origin->SetCurrentIsExpanded(false);
		return true;
	}
	if (input.Key == Key::Multiply)
	{
		(void)ExpandSubtree(origin);
		return true;
	}

	std::vector<TreeViewItem*> visible;
	CollectVisibleContainers(visible);
	visible.erase(std::remove_if(
		visible.begin(), visible.end(),
		[](TreeViewItem* item)
		{
			return !item || !item->IsEffectivelyEnabled();
		}), visible.end());
	if (visible.empty()) return true;
	auto current = std::find(visible.begin(), visible.end(), origin);
	int index = current == visible.end()
		? -1 : static_cast<int>(current - visible.begin());
	TreeViewItem* next = origin;
	switch (input.Key)
	{
	case Key::Up:
		next = visible[static_cast<size_t>((std::max)(0, index - 1))];
		break;
	case Key::Down:
		next = visible[static_cast<size_t>((std::min)(
			static_cast<int>(visible.size()) - 1, index + 1))];
		break;
	case Key::Home:
		next = visible.front();
		break;
	case Key::End:
		next = visible.back();
		break;
	case Key::PageUp:
		next = visible[static_cast<size_t>((std::max)(0, index - 8))];
		break;
	case Key::PageDown:
		next = visible[static_cast<size_t>((std::min)(
			static_cast<int>(visible.size()) - 1, index + 8))];
		break;
	case Key::Left:
		if (origin && origin->GetIsExpanded())
			origin->SetCurrentIsExpanded(false);
		else if (origin)
			next = origin->_parentItem;
		break;
	case Key::Right:
		if (origin && origin->GetHasItems()
			&& !origin->GetIsExpanded())
			origin->SetCurrentIsExpanded(true);
		else if (origin && origin->GetIsExpanded())
			next = origin->ContainerFromIndex(0);
		break;
	default:
		break;
	}
	(void)HandleScrollKey(input.Key);
	if (next && next != origin)
		(void)FocusAndSelectItem(next, true);
	return true;
}

void TreeView::UpdateHover(TreeViewItem* item)
{
	if (item && !ContainsContainer(item)) item = nullptr;
	if (_hoveredContainer == item) return;
	_hoveredContainer = item;
}

TreeViewItem* TreeView::HitTestItem(
	float localX, float localY, float* relativeRowY)
{
	if (relativeRowY) *relativeRowY = 0.5f;
	const auto treeBounds = GetAbsoluteBoundsDip();
	const float absoluteX = treeBounds.left + localX;
	const float absoluteY = treeBounds.top + localY;
	std::vector<TreeViewItem*> visible;
	CollectVisibleContainers(visible);
	for (auto current = visible.rbegin(); current != visible.rend(); ++current)
	{
		auto* item = *current;
		if (!item) continue;
		auto* header = item->GetHeaderVisual();
		const auto bounds = header
			? header->GetAbsoluteBoundsDip() : item->GetAbsoluteBoundsDip();
		if (!PointInRect(absoluteX, absoluteY, bounds)) continue;
		if (relativeRowY)
		{
			const float height = bounds.bottom - bounds.top;
			*relativeRowY = height > 0.0f
				? (std::clamp)((absoluteY - bounds.top) / height, 0.0f, 1.0f)
				: 0.5f;
		}
		return item;
	}
	return nullptr;
}

void TreeView::SetDropTarget(
	TreeViewItem* item, TreeViewDropPosition position)
{
	if (item && !ContainsContainer(item)) item = nullptr;
	if (!item) position = TreeViewDropPosition::None;
	if (_dropTarget == item && _dropPosition == position) return;
	_dropTarget = item;
	_dropPosition = position;
	InvalidateVisual();
}

void TreeView::ClearDropTarget()
{
	SetDropTarget(nullptr, TreeViewDropPosition::None);
}

bool TreeView::HandlesNavigationKey(Key key) const
{
	switch (key)
	{
	case Key::Up:
	case Key::Down:
	case Key::Left:
	case Key::Right:
	case Key::Home:
	case Key::End:
	case Key::PageUp:
	case Key::PageDown:
	case Key::Space:
	case Key::Add:
	case Key::Subtract:
	case Key::Multiply:
		return true;
	default:
		return false;
	}
}

bool TreeView::ProcessInput(const InputReport& input)
{
	if (!IsEffectivelyEnabled() || !IsVisible) return true;
	if (input.Kind == InputReportKind::PointerDown
		&& input.ChangedButton == MouseButton::Left)
	{
		RestoreFocusOnPointerDown();
	}
	if (input.Kind == InputReportKind::KeyDown
		&& ProcessItemNavigationKey(_selectedContainer, input))
	{
		auto args = input.CreateKeyEventArgs();
		OnKeyDown(this, args);
		return true;
	}
	return ItemsControl::ProcessInput(input);
}

bool TreeView::ApplyItemContainerStyle()
{
	return ItemsControl::ApplyItemContainerStyle();
}

void TreeView::OnBeforeGeneratedItemsRebuilt()
{
	PrepareHierarchyMutation();
}

void TreeView::OnGeneratedItemsRebuilt()
{
	CompleteHierarchyMutation();
}

void TreeView::OnGeneratedItemsRealized()
{
	RefreshHierarchy();
}

void TreeView::OnAuthoredItemsChanged() noexcept
{
	RefreshHierarchy();
	(void)ApplyItemContainerStyle();
}

void TreeView::RebuildAuthoredDataDescendants()
{
	std::function<void(ItemsControl&)> rebuild = [&](ItemsControl& owner)
	{
		for (size_t index = 0; index < owner.AuthoredItemCount(); ++index)
		{
			auto* item = dynamic_cast<TreeViewItem*>(
				owner.GetAuthoredItem(index));
			if (!item) continue;
			if (item->GetMaterializedItemsSource())
				(void)item->RebuildGeneratedItems();
			else rebuild(*item);
		}
	};
	rebuild(*this);
}

void TreeView::DrawDropIndicator()
{
	if (!_dropTarget || _dropPosition == TreeViewDropPosition::None
		|| !ContainsContainer(_dropTarget)
		|| !GetPresentationWindow() || !GetDrawingContext()) return;
	auto* header = _dropTarget->GetHeaderVisual();
	const auto target = header
		? header->GetAbsoluteBoundsDip() : _dropTarget->GetAbsoluteBoundsDip();
	const auto tree = GetAbsoluteBoundsDip();
	const float left = (std::max)(0.0f, target.left - tree.left);
	const float top = (std::max)(0.0f, target.top - tree.top);
	const float right = (std::min)(
		GetActualSizeDip().width, target.right - tree.left);
	const float bottom = (std::min)(
		GetActualSizeDip().height, target.bottom - tree.top);
	if (right <= left || bottom <= top) return;
	auto* render = GetDrawingContext();
	const auto accent = cui::theme::palette::Accent;
	if (_dropPosition == TreeViewDropPosition::Inside)
		render->DrawRoundRect(left, top, right - left, bottom - top,
			accent, 1.5f, 4.0f);
	else
	{
		const float y = _dropPosition == TreeViewDropPosition::Before
			? top + 1.0f : bottom - 1.0f;
		render->DrawLine(left, y, right, y, accent, 2.0f);
	}
}

void TreeView::OnRender()
{
	if (!IsVisible || !GetPresentationWindow() || !GetDrawingContext()) return;
	BeginRender();
	DrawDropIndicator();
	EndRender();
}
