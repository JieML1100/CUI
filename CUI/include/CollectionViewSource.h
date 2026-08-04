#pragma once

#include "BindingList.h"

#include <functional>
#include <string>
#include <utility>
#include <vector>

[[nodiscard]] constexpr bool SameCompiledCollectionPath(
	CompiledBindingPathView left,
	CompiledBindingPathView right) noexcept
{
	return left.Version == right.Version
		&& left.Steps.data() == right.Steps.data()
		&& left.Steps.size() == right.Steps.size();
}

enum class CollectionSortDirection : uint8_t
{
	Ascending,
	Descending
};

enum class CollectionFilterOperator : uint8_t
{
	Equals,
	NotEquals,
	LessThan,
	LessThanOrEqual,
	GreaterThan,
	GreaterThanOrEqual,
	Contains,
	StartsWith,
	EndsWith,
	IsEmpty,
	IsNotEmpty
};

struct CollectionSortDescription final
{
#if CUI_ENABLE_DYNAMIC_XAML
	std::wstring PropertyName;
#endif
	CollectionSortDirection Direction = CollectionSortDirection::Ascending;
	bool IgnoreCase = true;
	CompiledBindingPathView CompiledPath;

	[[nodiscard]] static CollectionSortDescription FromCompiledPath(
		CompiledBindingPathView path,
		CollectionSortDirection direction = CollectionSortDirection::Ascending,
		bool ignoreCase = true) noexcept
	{
		CollectionSortDescription result;
		result.Direction = direction;
		result.IgnoreCase = ignoreCase;
		result.CompiledPath = path;
		return result;
	}

	bool operator==(const CollectionSortDescription& other) const noexcept
	{
#if CUI_ENABLE_DYNAMIC_XAML
		if (PropertyName != other.PropertyName) return false;
#endif
		return Direction == other.Direction && IgnoreCase == other.IgnoreCase
			&& SameCompiledCollectionPath(CompiledPath, other.CompiledPath);
	}
};

struct CollectionFilterDescription final
{
#if CUI_ENABLE_DYNAMIC_XAML
	std::wstring PropertyName;
#endif
	CollectionFilterOperator Operator = CollectionFilterOperator::Equals;
	BindingValue Value;
	bool IgnoreCase = true;
	CompiledBindingPathView CompiledPath;

	[[nodiscard]] static CollectionFilterDescription FromCompiledPath(
		CompiledBindingPathView path,
		CollectionFilterOperator filterOperator,
		BindingValue value = {},
		bool ignoreCase = true)
	{
		CollectionFilterDescription result;
		result.Operator = filterOperator;
		result.Value = std::move(value);
		result.IgnoreCase = ignoreCase;
		result.CompiledPath = path;
		return result;
	}
};

struct CollectionGroupDescription final
{
#if CUI_ENABLE_DYNAMIC_XAML
	std::wstring PropertyName;
#endif
	CollectionSortDirection Direction = CollectionSortDirection::Ascending;
	bool IgnoreCase = true;
	CompiledBindingPathView CompiledPath;

	[[nodiscard]] static CollectionGroupDescription FromCompiledPath(
		CompiledBindingPathView path,
		CollectionSortDirection direction = CollectionSortDirection::Ascending,
		bool ignoreCase = true) noexcept
	{
		CollectionGroupDescription result;
		result.Direction = direction;
		result.IgnoreCase = ignoreCase;
		result.CompiledPath = path;
		return result;
	}

	bool operator==(const CollectionGroupDescription& other) const noexcept
	{
#if CUI_ENABLE_DYNAMIC_XAML
		if (PropertyName != other.PropertyName) return false;
#endif
		return Direction == other.Direction && IgnoreCase == other.IgnoreCase
			&& SameCompiledCollectionPath(CompiledPath, other.CompiledPath);
	}
};

enum class CollectionAggregateFunction : uint8_t
{
	Count,
	Sum,
	Average,
	Min,
	Max
};

/** One named value computed over every materialized group range. */
struct CollectionAggregateDescription final
{
	std::wstring Name;
#if CUI_ENABLE_DYNAMIC_XAML
	std::wstring PropertyName;
#endif
	CollectionAggregateFunction Function = CollectionAggregateFunction::Count;
	CompiledBindingPathView CompiledPath;

	[[nodiscard]] static CollectionAggregateDescription FromCompiledPath(
		std::wstring name,
		CompiledBindingPathView path,
		CollectionAggregateFunction function = CollectionAggregateFunction::Count)
	{
		CollectionAggregateDescription result;
		result.Name = std::move(name);
		result.Function = function;
		result.CompiledPath = path;
		return result;
	}

	bool operator==(const CollectionAggregateDescription& other) const noexcept
	{
		if (Name != other.Name || Function != other.Function) return false;
#if CUI_ENABLE_DYNAMIC_XAML
		if (PropertyName != other.PropertyName) return false;
#endif
		return SameCompiledCollectionPath(CompiledPath, other.CompiledPath);
	}
};

#if CUI_ENABLE_DYNAMIC_XAML
namespace cui::design
{
	bool HasAuthoredCollectionDescriptionPath(
		const CollectionSortDescription& description) noexcept;
	bool HasAuthoredCollectionDescriptionPath(
		const CollectionFilterDescription& description) noexcept;
	bool HasAuthoredCollectionDescriptionPath(
		const CollectionGroupDescription& description) noexcept;
	bool HasAuthoredCollectionDescriptionPath(
		const CollectionAggregateDescription& description) noexcept;
	bool TryReadAuthoredCollectionDescription(
		IBindingSource& source,
		const CollectionSortDescription& description,
		BindingValue& value);
	bool TryReadAuthoredCollectionDescription(
		IBindingSource& source,
		const CollectionFilterDescription& description,
		BindingValue& value);
	bool TryReadAuthoredCollectionDescription(
		IBindingSource& source,
		const CollectionGroupDescription& description,
		BindingValue& value);
	bool TryReadAuthoredCollectionDescription(
		IBindingSource& source,
		const CollectionAggregateDescription& description,
		BindingValue& value);
	const std::wstring& AuthoredCollectionGroupPropertyName(
		const CollectionGroupDescription& description) noexcept;
}
#endif

/**
 * Reusable ICollectionView-style projection over any IBindingList.
 *
 * Filtering and stable multi-key sorting never copy records. The view emits a
 * precise sequence of Add/Remove/Move changes so ItemsControl can preserve
 * unaffected containers and selection identity.
 */
class CollectionViewSource final
	: public IBindingList,
	  public IBindingListGroupView,
	  public IBindingListCurrentView
{
public:
	using FilterPredicate = std::function<bool(
		const BindingSourceReference& item)>;

	size_t Count() const noexcept override { return _items.size(); }
	bool TryGetItem(size_t index, BindingSourceReference& out) const override;
	EventConnection SubscribeChanged(ChangedHandler handler) override;
	DataTypeToken GetItemTypeToken() const noexcept override;
#if CUI_ENABLE_DYNAMIC_XAML
	const std::wstring& ItemTypeName() const noexcept override;
#endif

	BindingListReference GetSource() const noexcept { return _source; }
	void SetSource(BindingListReference value);
#if CUI_ENABLE_DYNAMIC_XAML
	const std::wstring& SourceBindingPath() const noexcept
	{
		return _sourceBindingPath;
	}
	void SetSourceBindingPath(std::wstring value);
#endif
	CompiledBindingPathView SourceCompiledBindingPath() const noexcept
	{
		return _sourceCompiledBindingPath;
	}
	void SetCompiledSourceBindingPath(CompiledBindingPathView value);
	void BindDataContext(BindingSourceReference value);
	const std::vector<CollectionSortDescription>& SortDescriptions() const noexcept
	{
		return _sortDescriptions;
	}
	void SetSortDescriptions(std::vector<CollectionSortDescription> value);
	const std::vector<CollectionFilterDescription>& FilterDescriptions() const noexcept
	{
		return _filterDescriptions;
	}
	void SetFilterDescriptions(std::vector<CollectionFilterDescription> value);
	const std::vector<CollectionGroupDescription>& GroupDescriptions() const noexcept
	{
		return _groupDescriptions;
	}
	void SetGroupDescriptions(std::vector<CollectionGroupDescription> value);
	const std::vector<CollectionAggregateDescription>& AggregateDescriptions() const noexcept
	{
		return _aggregateDescriptions;
	}
	void SetAggregateDescriptions(
		std::vector<CollectionAggregateDescription> value);
	const std::vector<BindingListGroup>& Groups() const noexcept override
	{
		return _groups;
	}
	EventConnection SubscribeGroupsChanged(
		GroupsChangedHandler handler) override;
	void SetFilterPredicate(FilterPredicate value);
	void Refresh();

	int CurrentPosition() const noexcept override { return _currentPosition; }
	BindingSourceReference CurrentItem() const noexcept override
	{
		return _currentItem;
	}
	bool MoveCurrentToPosition(int position) override;
	EventConnection SubscribeCurrentChanged(
		CurrentChangedHandler handler) override;
	bool MoveCurrentToFirst() { return MoveCurrentToPosition(0); }
	bool MoveCurrentToLast()
	{
		return MoveCurrentToPosition(
			_items.empty() ? -1 : static_cast<int>(_items.size()) - 1);
	}
	Event<void(CollectionViewSource*)> CurrentChanged;

private:
	BindingListReference _source;
	EventConnection _sourceChanged;
#if CUI_ENABLE_DYNAMIC_XAML
	std::wstring _sourceBindingPath;
#endif
	CompiledBindingPathView _sourceCompiledBindingPath;
	BindingSourceReference _dataContext;
	BindingPathObservation _sourceBindingObservation;
	std::vector<BindingPathObservation> _itemObservations;
	std::vector<BindingSourceReference> _items;
	std::vector<CollectionSortDescription> _sortDescriptions;
	std::vector<CollectionFilterDescription> _filterDescriptions;
	std::vector<CollectionGroupDescription> _groupDescriptions;
	std::vector<CollectionAggregateDescription> _aggregateDescriptions;
	std::vector<BindingListGroup> _groups;
	Event<void(CollectionViewSource*)> _groupsChanged;
	FilterPredicate _filterPredicate;
	Event<void(CollectionViewSource*, const CollectionChangedEventArgs&)>
		_changed;
	BindingSourceReference _currentItem;
	int _currentPosition = -1;
	bool _refreshing = false;
	bool _refreshPending = false;

	bool PassesFilters(const BindingSourceReference& item) const;
	void SetResolvedSource(BindingListReference value);
	void ResolveBoundSource();
	void RebuildProjection();
	void PublishProjection(
		std::vector<BindingSourceReference> target);
	std::vector<BindingListGroup> BuildGroups(
		const std::vector<BindingSourceReference>& items) const;
	void RebuildItemObservations();
	void RestoreCurrentItem();
#if CUI_ENABLE_DYNAMIC_XAML
	void ClearAuthoredSourceBindingPath() noexcept;
	bool HasAuthoredSourceBindingPath() const noexcept;
	void ResolveAuthoredSourceBinding(BindingListReference& resolved);
	void AppendAuthoredItemObservations();
#endif
};
