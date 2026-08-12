#include "CollectionViewSource.h"
#include "DependencyPropertyInfrastructure.h"
#include "EventInfrastructure.h"
#include "ItemsControl.h"
#include "Selector.h"
#include "TreeView.h"

#include <algorithm>
#include <stdexcept>
#include <typeindex>
#include <unordered_set>
#include <utility>

#if !CUI_ENABLE_DYNAMIC_XAML
#error CollectionAdapters.Design.cpp requires the Design runtime flavor.
#endif

namespace
{
	const DependencyProperty& RegisteredItemsControlProperty(
		const wchar_t* propertyName)
	{
		ItemsControl::RegisterDependencyProperties();
		const std::type_index ownerTypes[] = {
			std::type_index(typeid(ItemsControl))
		};
		const auto* metadata = DependencyPropertyRegistry::FindRegistered(
			ownerTypes, propertyName);
		if (!metadata)
			throw std::logic_error(
				"ItemsControl dependency property is not registered");
		return metadata->Property();
	}

	const DependencyProperty& RegisteredSelectorProperty(
		const wchar_t* propertyName)
	{
		Selector::RegisterDependencyProperties();
		const std::type_index ownerTypes[] = {
			std::type_index(typeid(Selector))
		};
		const auto* metadata = DependencyPropertyRegistry::FindRegistered(
			ownerTypes, propertyName);
		if (!metadata)
			throw std::logic_error(
				"Selector dependency property is not registered");
		return metadata->Property();
	}

	template<typename TOwner>
	const DependencyProperty& RegisteredTreeProperty(
		const wchar_t* propertyName)
	{
		TOwner::RegisterDependencyProperties();
		const std::type_index ownerTypes[] = {
			std::type_index(typeid(TOwner))
		};
		const auto* metadata = DependencyPropertyRegistry::FindRegistered(
			ownerTypes, propertyName);
		if (!metadata)
			throw std::logic_error(
				"Tree dependency property is not registered");
		return metadata->Property();
	}
}

namespace cui::design
{
	const std::wstring& AuthoredBindingListItemTypeName(
		const IBindingList& source) noexcept
	{
		return source.ItemTypeName();
	}

	template<typename TDescription>
	static bool HasAuthoredDescriptionPath(
		const TDescription& description) noexcept
	{
		return !description.PropertyName.empty();
	}

	template<typename TDescription>
	static bool TryReadAuthoredDescription(
		IBindingSource& source,
		const TDescription& description,
		BindingValue& value)
	{
		return !description.PropertyName.empty()
			&& TryGetBindingPathValue(
				source, description.PropertyName, value);
	}

	bool HasAuthoredCollectionDescriptionPath(
		const CollectionSortDescription& description) noexcept
	{
		return HasAuthoredDescriptionPath(description);
	}

	bool HasAuthoredCollectionDescriptionPath(
		const CollectionFilterDescription& description) noexcept
	{
		return HasAuthoredDescriptionPath(description);
	}

	bool HasAuthoredCollectionDescriptionPath(
		const CollectionGroupDescription& description) noexcept
	{
		return HasAuthoredDescriptionPath(description);
	}

	bool HasAuthoredCollectionDescriptionPath(
		const CollectionAggregateDescription& description) noexcept
	{
		return HasAuthoredDescriptionPath(description);
	}

	bool TryReadAuthoredCollectionDescription(
		IBindingSource& source,
		const CollectionSortDescription& description,
		BindingValue& value)
	{
		return TryReadAuthoredDescription(source, description, value);
	}

	bool TryReadAuthoredCollectionDescription(
		IBindingSource& source,
		const CollectionFilterDescription& description,
		BindingValue& value)
	{
		return TryReadAuthoredDescription(source, description, value);
	}

	bool TryReadAuthoredCollectionDescription(
		IBindingSource& source,
		const CollectionGroupDescription& description,
		BindingValue& value)
	{
		return TryReadAuthoredDescription(source, description, value);
	}

	bool TryReadAuthoredCollectionDescription(
		IBindingSource& source,
		const CollectionAggregateDescription& description,
		BindingValue& value)
	{
		return TryReadAuthoredDescription(source, description, value);
	}

	const std::wstring& AuthoredCollectionGroupPropertyName(
		const CollectionGroupDescription& description) noexcept
	{
		return description.PropertyName;
	}
}

const std::wstring& CollectionViewSource::ItemTypeName() const noexcept
{
	static const std::wstring empty;
	return _source ? _source.Get()->ItemTypeName() : empty;
}

void CollectionViewSource::SetSourceBindingPath(std::wstring value)
{
	if (_sourceBindingPath == value
		&& _sourceCompiledBindingPath.Empty()) return;
	_sourceBindingPath = std::move(value);
	_sourceCompiledBindingPath = {};
	ResolveBoundSource();
}

void CollectionViewSource::ClearAuthoredSourceBindingPath() noexcept
{
	_sourceBindingPath.clear();
}

bool CollectionViewSource::HasAuthoredSourceBindingPath() const noexcept
{
	return !_sourceBindingPath.empty();
}

void CollectionViewSource::ResolveAuthoredSourceBinding(
	BindingListReference& resolved)
{
	BindingValue value;
	(void)(TryGetBindingPathValue(
		*_dataContext.Get(), _sourceBindingPath, value)
		&& value.TryGet(resolved));
	_sourceBindingObservation = ObserveBindingPaths(
		_dataContext, { _sourceBindingPath },
		[this] { ResolveBoundSource(); });
}

void CollectionViewSource::AppendAuthoredItemObservations()
{
	if (!_source) return;
	std::vector<std::wstring> paths;
	paths.reserve(
		(_isLiveFilteringRequested ? _filterDescriptions.size() : 0)
		+ (_isLiveSortingRequested ? _sortDescriptions.size() : 0)
		+ (_isLiveGroupingRequested
			? _groupDescriptions.size() + _aggregateDescriptions.size() : 0));
	if (_isLiveFilteringRequested)
		for (const auto& filter : _filterDescriptions)
			if (filter.CompiledPath.Empty() && !filter.PropertyName.empty())
				paths.push_back(filter.PropertyName);
	if (_isLiveSortingRequested)
		for (const auto& sort : _sortDescriptions)
			if (sort.CompiledPath.Empty() && !sort.PropertyName.empty())
				paths.push_back(sort.PropertyName);
	if (_isLiveGroupingRequested)
	{
		for (const auto& group : _groupDescriptions)
			if (group.CompiledPath.Empty() && !group.PropertyName.empty())
				paths.push_back(group.PropertyName);
		for (const auto& aggregate : _aggregateDescriptions)
			if (aggregate.CompiledPath.Empty() && !aggregate.PropertyName.empty())
				paths.push_back(aggregate.PropertyName);
	}
	std::sort(paths.begin(), paths.end());
	paths.erase(std::unique(paths.begin(), paths.end()), paths.end());
	if (paths.empty()) return;
	auto* const source = _source.Get();
	const size_t sourceCount = source->Count();
	std::unordered_set<const IBindingSource*> observedItems;
	for (size_t index = 0; index < sourceCount; ++index)
	{
		BindingSourceReference item;
		if (!source->TryGetItem(index, item) || !item) continue;
		// Authored paths have the same object-level invalidation semantics as
		// compiled paths: one notification refreshes every repeated occurrence.
		if (!observedItems.insert(item.Get()).second) continue;
		_itemObservations.push_back(ObserveBindingPaths(
			item, std::span<const std::wstring>{ paths },
			[this] { Refresh(); }));
	}
}

const DependencyProperty& ItemsControl::DisplayMemberPathProperty()
{
	static const auto& property =
		RegisteredItemsControlProperty(L"DisplayMemberPath");
	return property;
}

void ItemsControl::RegisterDesignDependencyProperties()
{
	(void)ItemsSourceProperty();
	(void)ItemTemplateProperty();
	(void)GroupStyleProperty();
	(void)ItemsPanelProperty();
	(void)ItemContainerStyleProperty();
	(void)IsTextSearchEnabledProperty();
	(void)IsTextSearchCaseSensitiveProperty();
	static const bool registered = []
	{
		DependencyPropertyOptions<ItemsControl, std::wstring> options;
		options.DefaultValue = std::wstring{};
		options.Flags = DependencyPropertyFlags::AffectsMeasure
			| DependencyPropertyFlags::AffectsArrange
			| DependencyPropertyFlags::AffectsRender;
		options.Design.Category = L"Data";
		options.Design.CategoryOrder = 80;
		options.Design.Order = 40;
		options.Design.Editor = DependencyPropertyEditorKind::Text;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		DependencyPropertyRegistry::Register<ItemsControl, std::wstring>(
			L"DisplayMemberPath",
			[](ItemsControl& target) { return target.GetDisplayMemberPath(); },
			[](ItemsControl& target, const std::wstring& value)
			{ target.SetDisplayMemberPath(value); }, {}, std::move(options));
		return true;
	}();
	(void)registered;
}

void ItemsControl::SetDisplayMemberPath(std::wstring value)
{
	if (_displayMemberPath == value && _compiledDisplayMemberPath.Empty()) return;
	_displayMemberPath = std::move(value);
	_compiledDisplayMemberPath = {};
	ResetTextSearch();
	if (_itemsSource && !_itemTemplate) (void)RebuildGeneratedItems();
}

std::wstring ItemsControl::ReadAuthoredDisplayMemberText(
	const BindingSourceReference& item) const
{
	return GetBindingRecordText(item, _displayMemberPath);
}

BindingPathObservation ItemsControl::ObserveAuthoredDisplayMemberPath(
	const BindingSourceReference& item,
	std::function<void()> changed) const
{
	return _displayMemberPath.empty()
		? BindingPathObservation{}
		: ObserveBindingPaths(
			item, { _displayMemberPath }, std::move(changed));
}

void ItemsControl::ApplyAuthoredGeneratedItemProjection(
	ContentPresenter& presenter) const
{
	presenter.SetContentTypeName(_itemTemplate
		? _itemTemplate.Get()->DataTypeName()
		: (_itemsSource
			? cui::design::AuthoredBindingListItemTypeName(*_itemsSource.Get())
			: std::wstring{}));
	if (_compiledDisplayMemberPath.Empty())
		presenter.SetDisplayMemberPath(_displayMemberPath);
}

bool ListBoxItem::Initialize(
	const BindingSourceReference& item,
	const ItemTemplateReference& contentTemplate,
	const std::wstring& displayMemberPath,
	size_t index,
	std::wstring* outError)
{
	return InitializeItem(item, contentTemplate, displayMemberPath,
		index, L"ListBoxItem", outError);
}

const DependencyProperty& Selector::SelectedValuePathProperty()
{
	static const auto& property =
		RegisteredSelectorProperty(L"SelectedValuePath");
	return property;
}

void Selector::RegisterDesignDependencyProperties()
{
	(void)SelectedIndexProperty();
	(void)SelectedItemProperty();
	(void)SelectedValueProperty();
	(void)IsSynchronizedWithCurrentItemProperty();
	(void)IsSelectionActivePropertyKey().Property();

	static const bool dynamicPathRegistered = []
	{
		DependencyPropertyOptions<Selector, std::wstring> options;
		options.DefaultValue = std::wstring{};
		options.Flags = DependencyPropertyFlags::AffectsRender;
		options.Design.Category = L"Data";
		options.Design.CategoryOrder = 80;
		options.Design.Order = 50;
		options.Design.Editor = DependencyPropertyEditorKind::Text;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		DependencyPropertyRegistry::Register<Selector, std::wstring>(
			L"SelectedValuePath",
			[](Selector& target) { return target.GetSelectedValuePath(); },
			[](Selector& target, const std::wstring& value)
			{ target.SetSelectedValuePath(value); }, {}, std::move(options));
		return true;
	}();
	(void)dynamicPathRegistered;
}

void Selector::SetSelectedValuePath(std::wstring value)
{
	if (_selectedValuePath == value && _compiledSelectedValuePath.Empty()) return;
	_selectedValuePath = std::move(value);
	_compiledSelectedValuePath = {};
	const auto source = GetPropertyValueSource(SelectedValueProperty());
	BindingValue configured;
	if (source != DependencyPropertyValueSource::Default
		&& TryGetPropertyValue(SelectedValueProperty(), source, configured))
		SetSelectedValue(configured);
	else
		RefreshSelectedItemState(false);
}

bool Selector::HasAuthoredSelectedValuePath() const noexcept
{
	return !_selectedValuePath.empty();
}

bool Selector::TryReadAuthoredSelectedValue(
	IBindingSource& item,
	BindingValue& value) const
{
	return !_selectedValuePath.empty()
		&& TryGetBindingPathValue(item, _selectedValuePath, value);
}

BindingPathObservation Selector::ObserveAuthoredSelectedValuePath(
	const BindingSourceReference& item,
	std::function<void()> changed) const
{
	return _selectedValuePath.empty()
		? BindingPathObservation{}
		: ObserveBindingPaths(
			item, { _selectedValuePath }, std::move(changed));
}

bool Selector::TryReadAuthoredSelectedValueAt(
	size_t index,
	BindingValue& value) const
{
	return TryGetBindingListItemValue(
		GetItemsView(), index, _selectedValuePath, value);
}

int Selector::FindAuthoredSelectedValue(
	const BindingValue& value) const
{
	return FindBindingListItemByValue(
		GetItemsView(), _selectedValuePath, value);
}

bool Selector::InitializeAuthoredGeneratedContainer(
	ListBoxItem& container,
	const BindingSourceReference& item,
	size_t index,
	std::wstring& error) const
{
	return container.Initialize(
		item, GetItemTemplate(), GetDisplayMemberPath(), index, &error);
}

void TreeViewItem::RegisterDesignDependencyProperties()
{
	(void)IsExpandedProperty();
	(void)HasItemsProperty();
	(void)LevelPropertyKey().Property();
	(void)IsSelectedProperty();
}

const DependencyProperty& TreeView::SelectedValuePathProperty()
{
	static const auto& property =
		RegisteredTreeProperty<TreeView>(L"SelectedValuePath");
	return property;
}

void TreeView::RegisterDesignDependencyProperties()
{
	(void)SelectedItemPropertyKey().Property();
	(void)SelectedValuePropertyKey().Property();

	static const bool dynamicPathRegistered = []
	{
		DependencyPropertyOptions<TreeView, std::wstring> options;
		options.DefaultValue = std::wstring{};
		options.Design.Category = L"Data";
		options.Design.CategoryOrder = 80;
		options.Design.Order = 40;
		options.Design.Editor = DependencyPropertyEditorKind::Text;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		DependencyPropertyRegistry::Register<TreeView, std::wstring>(
			L"SelectedValuePath",
			[](TreeView& target) { return target.GetSelectedValuePath(); },
			[](TreeView& target, const std::wstring& value)
			{ target.SetSelectedValuePath(value); }, {}, std::move(options));
		return true;
	}();
	(void)dynamicPathRegistered;
}

void TreeView::SetDisplayMemberPath(std::wstring value)
{
	ItemsControl::SetDisplayMemberPath(std::move(value));
	RebuildAuthoredDataDescendants();
	RefreshHierarchy();
}

void TreeView::SetImplicitItemTemplateResolver(
	ImplicitItemTemplateResolver value)
{
	_implicitItemTemplateResolver = std::move(value);
	if (GetItemsSource()) (void)RebuildGeneratedItems();
	RebuildAuthoredDataDescendants();
	RefreshHierarchy();
}

ItemTemplateReference TreeView::ResolveAuthoredImplicitItemTemplate(
	const BindingListReference& source) const
{
	return source && _implicitItemTemplateResolver
		? _implicitItemTemplateResolver(source.Get()->ItemTypeName())
		: ItemTemplateReference{};
}

void TreeView::AppendAuthoredItemTypeDiagnostic(
	const BindingListReference& source,
	std::wstring& error) const
{
	if (source && !source.Get()->ItemTypeName().empty())
		error += L" ItemType=" + source.Get()->ItemTypeName();
}

void TreeView::ApplyAuthoredGeneratedContainerProjection(
	TreeViewItem& container,
	const ItemsControl& projectionOwner) const
{
	container.SetHeaderTypeName(container._headerDataTemplate
		? container._headerDataTemplate.Get()->DataTypeName()
		: std::wstring{});
	if (!container.GetCompiledDisplayMemberPath().Empty()) return;
	container.SetDisplayMemberPath(projectionOwner.GetDisplayMemberPath());
	container.SetHeaderDisplayMemberPath(
		projectionOwner.GetDisplayMemberPath());
}

bool TreeView::HasAuthoredSelectedValuePath() const noexcept
{
	return !_selectedValuePath.empty();
}

BindingValue TreeView::ReadAuthoredSelectedValue() const
{
	if (!_selectedContainer || _selectedValuePath.empty()) return {};
	BindingValue result;
	if (_selectedContainer->_dataItem)
		return TryGetBindingPathValue(
			*_selectedContainer->_dataItem.Get(), _selectedValuePath, result)
			? result : BindingValue{};
	return _selectedContainer->TryGetValue(_selectedValuePath, result)
		? result : BindingValue{};
}

BindingPathObservation TreeView::ObserveAuthoredSelectedValuePath(
	std::function<void()> changed) const
{
	return !_selectedContainer || !_selectedContainer->_dataItem
		|| _selectedValuePath.empty()
		? BindingPathObservation{}
		: ObserveBindingPaths(
			_selectedContainer->_dataItem,
			{ _selectedValuePath }, std::move(changed));
}

void TreeView::SetSelectedValuePath(std::wstring value)
{
	if (_selectedValuePath == value && _compiledSelectedValuePath.Empty()) return;
	_selectedValuePath = std::move(value);
	_compiledSelectedValuePath = {};
	RefreshSelectedItemObservation();
	cui::framework::EventAccess::Raise(_selectedValueChanged, this);
}
