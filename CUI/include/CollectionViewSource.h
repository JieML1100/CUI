#pragma once

#include "BindingList.h"

#include <functional>
#include <string>
#include <vector>

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
	std::wstring PropertyName;
	CollectionSortDirection Direction = CollectionSortDirection::Ascending;
	bool IgnoreCase = true;

	bool operator==(const CollectionSortDescription&) const = default;
};

struct CollectionFilterDescription final
{
	std::wstring PropertyName;
	CollectionFilterOperator Operator = CollectionFilterOperator::Equals;
	BindingValue Value;
	bool IgnoreCase = true;
};

struct CollectionGroupDescription final
{
	std::wstring PropertyName;
	CollectionSortDirection Direction = CollectionSortDirection::Ascending;
	bool IgnoreCase = true;

	bool operator==(const CollectionGroupDescription&) const = default;
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
	std::wstring PropertyName;
	CollectionAggregateFunction Function = CollectionAggregateFunction::Count;

	bool operator==(const CollectionAggregateDescription&) const = default;
};

/**
 * Reusable ICollectionView-style projection over any IBindingList.
 *
 * Filtering and stable multi-key sorting never copy records. The view emits a
 * precise sequence of Add/Remove/Move changes so ItemsControl can preserve
 * unaffected containers and selection identity.
 */
class CollectionViewSource final
	: public IBindingList,
	  public IBindingListGroupView
{
public:
	using FilterPredicate = std::function<bool(
		const BindingSourceReference& item)>;

	size_t Count() const noexcept override { return _items.size(); }
	bool TryGetItem(size_t index, BindingSourceReference& out) const override;
	EventConnection SubscribeChanged(ChangedHandler handler) override;
	const std::wstring& ItemTypeName() const noexcept override;

	BindingListReference GetSource() const noexcept { return _source; }
	void SetSource(BindingListReference value);
	const std::wstring& SourceBindingPath() const noexcept
	{
		return _sourceBindingPath;
	}
	void SetSourceBindingPath(std::wstring value);
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

	int CurrentPosition() const noexcept { return _currentPosition; }
	BindingSourceReference CurrentItem() const noexcept { return _currentItem; }
	bool MoveCurrentToPosition(int position);
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
	std::wstring _sourceBindingPath;
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
};
