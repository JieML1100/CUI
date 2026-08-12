#pragma once

#include "BindingList.h"

#include <deque>
#include <functional>
#include <list>
#include <memory>
#include <string>
#include <unordered_map>
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
	  public IBindingListOccurrenceIdentity,
	  public IBindingListOccurrenceLookup,
	  public IBindingListSnapshotProvider,
	  public IBindingListGroupView,
	  public IBindingListCurrentView,
	  public std::enable_shared_from_this<CollectionViewSource>
{
public:
	using FilterPredicate = std::function<bool(
		const BindingSourceReference& item)>;
	CollectionViewSource();
	~CollectionViewSource() override;

	size_t Count() const noexcept override
	{
		return _sourcePassThrough && _source
			? _source.Get()->Count() : _items.size();
	}
	bool TryGetItem(size_t index, BindingSourceReference& out) const override;
	EventConnection SubscribeChanged(ChangedHandler handler) override;
	DataTypeToken GetItemTypeToken() const noexcept override;
	bool TryGetStableSnapshot(BindingListReference& result) const override;
	/**
	 * Returns the stable identity of the source collection slot projected at
	 * index.
	 *
	 * Identities are unique within this view, follow their item through Move,
	 * survive Replace of that slot, and are retired by Remove. Add allocates a
	 * new identity; Reset or a different Source rebuilds all identities. They let
	 * consumers distinguish repeated references to the same binding object
	 * without treating the current view index as identity.
	 */
	bool TryGetItemOccurrenceIdentity(
		size_t viewIndex, size_t& result) const noexcept override;
	bool TryGetItemIndexByOccurrenceIdentity(
		size_t identity, size_t& viewIndex) const noexcept override;
	bool IsItemIndexByOccurrenceIdentityLookupBounded()
		const noexcept override;
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
	/**
	 * Requests WPF-style live shaping.  The request switches default to false;
	 * descriptions always shape an explicit Refresh, but item-property changes
	 * are observed only for the requested dimensions.
	 */
	bool GetCanChangeLiveSorting() const noexcept { return true; }
	bool GetIsLiveSortingRequested() const noexcept
	{
		return _isLiveSortingRequested;
	}
	bool GetIsLiveSorting() const noexcept { return _isLiveSortingRequested; }
	void SetIsLiveSortingRequested(bool value);
	const std::vector<CollectionFilterDescription>& FilterDescriptions() const noexcept
	{
		return _filterDescriptions;
	}
	void SetFilterDescriptions(std::vector<CollectionFilterDescription> value);
	bool GetCanChangeLiveFiltering() const noexcept { return true; }
	bool GetIsLiveFilteringRequested() const noexcept
	{
		return _isLiveFilteringRequested;
	}
	bool GetIsLiveFiltering() const noexcept { return _isLiveFilteringRequested; }
	void SetIsLiveFilteringRequested(bool value);
	const std::vector<CollectionGroupDescription>& GroupDescriptions() const noexcept
	{
		return _groupDescriptions;
	}
	void SetGroupDescriptions(std::vector<CollectionGroupDescription> value);
	bool GetCanChangeLiveGrouping() const noexcept { return true; }
	bool GetIsLiveGroupingRequested() const noexcept
	{
		return _isLiveGroupingRequested;
	}
	bool GetIsLiveGrouping() const noexcept { return _isLiveGroupingRequested; }
	void SetIsLiveGroupingRequested(bool value);
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
	/**
	 * Publishes a complex projection rebuild as one Reset.  DataGrid opts into
	 * this so ItemsControl can apply an occurrence permutation once instead of
	 * processing a quadratic run of vector erase/insert Move notifications.
	 * The general CollectionViewSource default remains precise changes.
	 */
	void SetUseResetNotificationForComplexRefresh(bool value) noexcept
	{
		_useResetNotificationForComplexRefresh = value;
	}
	// Compatibility alias retained for callers from the first atomic-sort slice.
	void SetUseResetNotificationForSort(bool value) noexcept
	{
		SetUseResetNotificationForComplexRefresh(value);
	}
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
		const size_t count = Count();
		return MoveCurrentToPosition(
			count == 0 ? -1 : static_cast<int>(count) - 1);
	}
	Event<void(CollectionViewSource*)> CurrentChanged;

private:
	struct SourceSlotIdentity final
	{
		size_t Token = 0;
		size_t Revision = 0;
	};

	struct ProjectionItem final
	{
		BindingSourceReference Item;
		size_t Token = 0;
		size_t Revision = 0;
	};
	struct PassThroughOccurrencePosition final
	{
		size_t Index = 0;
		std::list<size_t>::iterator Recency;
	};
	struct PassThroughGeneratedOccurrenceIndex;
	class PassThroughGeneratedStableSnapshot;
	struct PendingSourceChange final
	{
		CollectionChangedEventArgs Change;
		IBindingList* Source = nullptr;
		size_t ConnectionRevision = 0;
	};
	struct SourceCallbackLifetime final
	{
		CollectionViewSource* Owner = nullptr;
	};

	BindingListReference _source;
	EventConnection _sourceChanged;
	std::shared_ptr<SourceCallbackLifetime> _sourceCallbackLifetime;
#if CUI_ENABLE_DYNAMIC_XAML
	std::wstring _sourceBindingPath;
#endif
	CompiledBindingPathView _sourceCompiledBindingPath;
	BindingSourceReference _dataContext;
	BindingPathObservation _sourceBindingObservation;
	std::vector<BindingPathObservation> _itemObservations;
	std::vector<SourceSlotIdentity> _sourceSlots;
	std::vector<ProjectionItem> _items;
	std::unordered_map<size_t, size_t> _occurrenceIndex;
	mutable std::unordered_map<size_t, PassThroughOccurrencePosition>
		_passThroughOccurrenceIndex;
	mutable std::list<size_t> _passThroughOccurrenceRecency;
	std::shared_ptr<PassThroughGeneratedOccurrenceIndex>
		_passThroughGeneratedOccurrences;
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
	size_t _currentItemOccurrenceIdentity = 0;
	int _currentPosition = -1;
	size_t _publishedCount = 0;
	size_t _sourceConnectionRevision = 0;
	size_t _sourceStructureRevision = 0;
	size_t _observedSourceStructureRevision = 0;
	IBindingList* _observedSource = nullptr;
	std::vector<CompiledBindingPathView> _observedCompiledPaths;
#if CUI_ENABLE_DYNAMIC_XAML
	std::vector<std::wstring> _observedAuthoredPaths;
#endif
	size_t _nextSourceSlotRevision = 1;
	std::deque<PendingSourceChange> _pendingSourceChanges;
	bool _refreshing = false;
	bool _refreshPending = false;
	bool _drainingSourceChanges = false;
	bool _sourcePassThrough = false;
	bool _passThroughUsesGeneratedOccurrences = false;
	bool _passThroughGeneratedOccurrencesValid = false;
	bool _passThroughGeneratedOccurrenceResetPending = false;
	bool _itemObservationsDirty = true;
	bool _isLiveSortingRequested = false;
	bool _isLiveFilteringRequested = false;
	bool _isLiveGroupingRequested = false;
	bool _useResetNotificationForComplexRefresh = false;
	bool _resetProjectionOnNextRefresh = false;

	bool PassesFilters(
		const BindingSourceReference& item,
		const FilterPredicate& predicate,
		std::span<const CollectionFilterDescription> descriptions) const;
	bool CanUseSourcePassThrough() const noexcept;
	void ClearItemObservations() noexcept;
	void SetResolvedSource(BindingListReference value);
	void QueueSourceChanged(
		const CollectionChangedEventArgs& change,
		IBindingList* source,
		size_t connectionRevision);
	void HandleSourceChanged(
		const CollectionChangedEventArgs& change,
		IBindingList* source,
		size_t connectionRevision);
	bool IsCurrentSourceConnection(
		const IBindingList* source,
		size_t connectionRevision) const noexcept;
	void ApplySourceChange(const CollectionChangedEventArgs& change);
	void RebuildSourceSlots(size_t count);
	SourceSlotIdentity CreateSourceSlotIdentity();
	size_t CreateSourceSlotRevision();
	void ResolveBoundSource();
	void RefreshProjection(bool publishComplexReset);
	void RebuildProjection();
	void PublishProjection(
		std::vector<ProjectionItem> target,
		bool publishReset = false);
	void RebuildOccurrenceIndex();
	void RefreshOccurrenceIndex(size_t first, size_t last);
	void CachePassThroughOccurrencePosition(
		size_t identity, size_t index) const noexcept;
	void ApplyPassThroughOccurrenceChange(
		const CollectionChangedEventArgs& change) noexcept;
	bool InitializePassThroughGeneratedOccurrences(size_t count) noexcept;
	void InvalidatePassThroughGeneratedOccurrences() noexcept;
	void ApplyPassThroughGeneratedOccurrenceChange(
		const CollectionChangedEventArgs& change) noexcept;
	bool TryGetPassThroughGeneratedOccurrenceIdentity(
		size_t index, size_t& identity) const noexcept;
	bool TryGetPassThroughGeneratedOccurrenceIndex(
		size_t identity, size_t& index) const noexcept;
	std::vector<BindingListGroup> BuildGroups(
		const std::vector<ProjectionItem>& items,
		std::span<const CollectionGroupDescription> descriptions,
		std::span<const CollectionAggregateDescription> aggregates,
		std::span<const BindingValue> groupKeys) const;
	void RebuildItemObservations();
	void SetLiveShapingRequest(bool& target, bool value);
	void RestoreCurrentItem();
#if CUI_ENABLE_DYNAMIC_XAML
	void ClearAuthoredSourceBindingPath() noexcept;
	bool HasAuthoredSourceBindingPath() const noexcept;
	void ResolveAuthoredSourceBinding(BindingListReference& resolved);
	void AppendAuthoredItemObservations();
#endif
};
