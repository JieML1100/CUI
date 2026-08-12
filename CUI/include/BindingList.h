#pragma once

#include "Binding.h"
#include "ObservableCollection.h"
#include "RuntimeTypeMetadata.h"

#include <algorithm>
#include <functional>
#include <cwctype>
#include <initializer_list>
#include <limits>
#include <memory>
#include <map>
#include <new>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

/**
 * Read-only, observable list of binding records.
 *
 * Items are explicit BindingSourceReference values rather than std::any. This
 * keeps item-property discovery and DataTemplate binding on the same metadata
 * contract as an ordinary DataContext.
 */
class IBindingList
{
public:
	using ChangedHandler = std::function<void(const CollectionChangedEventArgs&)>;

	virtual ~IBindingList() = default;
	virtual size_t Count() const noexcept = 0;
	virtual bool TryGetItem(size_t index, BindingSourceReference& out) const = 0;
	virtual EventConnection SubscribeChanged(ChangedHandler handler) = 0;
#if CUI_ENABLE_DYNAMIC_XAML
	virtual DataTypeToken GetItemTypeToken() const noexcept
	{
		return MakeDataTypeToken(ItemTypeName());
	}
	virtual const std::wstring& ItemTypeName() const noexcept = 0;
#else
	virtual DataTypeToken GetItemTypeToken() const noexcept = 0;
#endif
};

/**
 * Optional stable identity for an item occurrence in a list projection.
 *
 * The identity distinguishes repeated references to the same binding object
 * and follows the same physical collection slot through Move and projection
 * reordering.  Consumers must treat a false result as "identity unavailable"
 * and fall back to their ordinary item/index semantics.
 */
class IBindingListOccurrenceIdentity
{
public:
	virtual ~IBindingListOccurrenceIdentity() = default;
	virtual bool TryGetItemOccurrenceIdentity(
		size_t index, size_t& result) const noexcept = 0;
};

/** Optional reverse lookup for a stable item-occurrence identity. */
class IBindingListOccurrenceLookup
{
public:
	virtual ~IBindingListOccurrenceLookup() = default;
	virtual bool TryGetItemIndexByOccurrenceIdentity(
		size_t identity, size_t& index) const noexcept = 0;
	/**
	 * True when each reverse lookup has a bounded cost independent of Count().
	 *
	 * Consumers must not infer this from the presence of the lookup interface:
	 * legacy adapters may preserve exact identity by scanning their source.
	 */
	virtual bool IsItemIndexByOccurrenceIdentityLookupBounded()
		const noexcept { return false; }
};

/**
 * Marker for an immutable random-access list that is already a stable snapshot.
 *
 * Count, item type, TryGetItem, optional occurrence identities, and optional
 * groups must remain unchanged for the lifetime of the list. SubscribeChanged
 * must not publish structural changes. Consumers may retain the list itself as
 * a transaction snapshot instead of copying every BindingSourceReference.
 */
class IBindingListStableSnapshot
{
public:
	virtual ~IBindingListStableSnapshot() = default;
};

/** One flattened level in a hierarchical grouped-list projection. */
struct BindingListGroup final
{
	BindingValue Key;
	std::wstring PropertyName;
	size_t Level = 0;
	size_t StartIndex = 0;
	size_t ItemCount = 0;
	std::map<std::wstring, BindingValue> Aggregates;
};

/** Optional grouping contract exposed by collection views. */
class IBindingListGroupView
{
public:
	using GroupsChangedHandler = std::function<void()>;
	virtual ~IBindingListGroupView() = default;
	virtual const std::vector<BindingListGroup>& Groups() const noexcept = 0;
	virtual EventConnection SubscribeGroupsChanged(
		GroupsChangedHandler handler) = 0;
};

/** Optional current-item contract exposed by collection views. */
class IBindingListCurrentView
{
public:
	using CurrentChangedHandler = std::function<void()>;

	virtual ~IBindingListCurrentView() = default;
	virtual int CurrentPosition() const noexcept = 0;
	virtual BindingSourceReference CurrentItem() const noexcept = 0;
	virtual bool MoveCurrentToPosition(int position) = 0;
	virtual EventConnection SubscribeCurrentChanged(
		CurrentChangedHandler handler) = 0;
};

/** Strong BindingValue payload used by ItemsSource properties. */
class BindingListReference final
{
public:
	BindingListReference() = default;
	explicit BindingListReference(std::shared_ptr<IBindingList> source)
		: _source(std::move(source)) {}

	template<typename T>
		requires std::is_base_of_v<IBindingList, T>
	explicit BindingListReference(std::shared_ptr<T> source)
		: _source(std::move(source)) {}

	IBindingList* Get() const noexcept { return _source.get(); }
	const std::shared_ptr<IBindingList>& Shared() const noexcept { return _source; }
	explicit operator bool() const noexcept { return static_cast<bool>(_source); }

	bool operator==(const BindingListReference& other) const noexcept
	{
		return _source == other._source;
	}

private:
	std::shared_ptr<IBindingList> _source;
};

/**
 * Optional O(1) bridge from a mutable view to its current stable snapshot.
 *
 * A successful result must implement IBindingListStableSnapshot and describe
 * the exact Count, item type, item order, and occurrence identities currently
 * exposed by the provider. The returned list owns all state needed to keep that
 * snapshot immutable after the provider changes.
 */
class IBindingListSnapshotProvider
{
public:
	virtual ~IBindingListSnapshotProvider() = default;
	virtual bool TryGetStableSnapshot(BindingListReference& result) const = 0;
};

namespace cui::framework
{
	class BindingListTestAccess;
}

/** Lightweight counters for a versioned binding-list checkpoint store. */
struct BindingListSnapshotStatistics final
{
	size_t Captures = 0;
	size_t RootCopies = 0;
	size_t EntryPageCopies = 0;
	size_t PositionPageCopies = 0;
	size_t FullRebuilds = 0;
	size_t MaintenanceFailures = 0;
	size_t CurrentEntryPages = 0;
	size_t CurrentPositionPages = 0;
};

/**
 * Default application-side observable record list.
 *
 * Alongside its contiguous mutable Items store it maintains a paged version
 * checkpoint. Before any snapshot is retained, append construction updates
 * those pages in place and remains amortized O(n). Snapshot capture is O(1).
 * A later mutation clones the page directory plus only touched entry/position
 * pages; middle insert/remove/move still has the same O(n) shifting cost as the
 * public contiguous collection itself.
 */
class ObservableBindingList final
	: public IBindingList,
	  public IBindingListOccurrenceIdentity,
	  public IBindingListOccurrenceLookup,
	  public IBindingListSnapshotProvider
{
public:
	using ItemCollection = ObservableCollection<BindingSourceReference>;

#if CUI_ENABLE_DYNAMIC_XAML
	explicit ObservableBindingList(std::wstring itemTypeName = {})
		: _itemTypeToken(MakeDataTypeToken(itemTypeName)),
		  _itemTypeName(std::move(itemTypeName))
	{
		InitializeOccurrenceIdentities();
	}
#else
	explicit ObservableBindingList(DataTypeToken itemTypeToken = {})
		: _itemTypeToken(itemTypeToken)
	{
		InitializeOccurrenceIdentities();
	}
#endif
	ObservableBindingList(const ObservableBindingList& other)
		: Items(other.Items),
		  _itemTypeToken(other._itemTypeToken)
#if CUI_ENABLE_DYNAMIC_XAML
		, _itemTypeName(other._itemTypeName)
#endif
	{
		InitializeOccurrenceIdentities();
	}
	ObservableBindingList(ObservableBindingList&& other)
		: Items(std::move(other.Items)),
		  _itemTypeToken(other._itemTypeToken)
#if CUI_ENABLE_DYNAMIC_XAML
		, _itemTypeName(std::move(other._itemTypeName))
#endif
		, _occurrenceIdentities(std::move(other._occurrenceIdentities)),
		  _nextOccurrenceIdentity(other._nextOccurrenceIdentity)
	{
		AttachOccurrenceTracking();
		_occurrenceMirrorSynchronized =
			_occurrenceIdentities.size() == Items.size();
		_occurrenceSynchronizedRevision = Items.MutationRevision();
		RebuildStableSnapshotState();
		other._occurrenceIdentities.clear();
		other._occurrenceMirrorSynchronized = true;
		other._occurrenceSynchronizedRevision =
			other.Items.MutationRevision();
		other.RebuildStableSnapshotState();
		// ObservableCollection keeps subscriptions on the moved-from instance.
		// Publish only after occurrence/version metadata has moved so the source's
		// internal tracker and public observers see one coherent empty Reset.
		cui::framework::ObservableCollectionMoveAccess::PublishMovedFromReset(
			other.Items, Items.size());
	}
	ObservableBindingList& operator=(const ObservableBindingList& other)
	{
		if (this == &other) return *this;
		_itemTypeToken = other._itemTypeToken;
#if CUI_ENABLE_DYNAMIC_XAML
		_itemTypeName = other._itemTypeName;
#endif
		Items = other.Items;
		return *this;
	}
	ObservableBindingList& operator=(ObservableBindingList&& other)
	{
		if (this == &other) return *this;
		const size_t movedFromOldSize = other.Items.size();
		cui::framework::ObservableCollectionMoveAccess::
			BeginMovedFromResetDeferral(other.Items, movedFromOldSize);
		_itemTypeToken = other._itemTypeToken;
#if CUI_ENABLE_DYNAMIC_XAML
		_itemTypeName = std::move(other._itemTypeName);
#endif
		try
		{
			Items = std::move(other.Items);
		}
		catch (...)
		{
			cui::framework::ObservableCollectionMoveAccess::
				CancelMovedFromResetDeferral(other.Items);
			throw;
		}
		// Destination Reset handlers may re-enter and add to the moved-from
		// collection while its notifications are deferred. Rebuild its mirrors
		// from the committed store before the one source Reset is published.
		try
		{
			other.RebuildOccurrenceIdentities();
			other._occurrenceMirrorSynchronized =
				other._occurrenceIdentities.size() == other.Items.size();
			other._occurrenceSynchronizedRevision =
				other.Items.MutationRevision();
			other.RebuildStableSnapshotState();
		}
		catch (...)
		{
			other._occurrenceIdentities.clear();
			other._occurrenceMirrorSynchronized = false;
			other._stableSnapshotState.reset();
			other._snapshotMirrorSynchronized = false;
			try
			{
				cui::framework::ObservableCollectionMoveAccess::
					EndMovedFromResetDeferral(other.Items);
			}
			catch (...) {}
			throw;
		}
		cui::framework::ObservableCollectionMoveAccess::
			EndMovedFromResetDeferral(other.Items);
		return *this;
	}

	size_t Count() const noexcept override { return Items.size(); }
	bool TryGetItem(size_t index, BindingSourceReference& out) const override
	{
		if (index >= Items.size() || !Items[index]) return false;
		out = Items[index];
		return true;
	}
	EventConnection SubscribeChanged(ChangedHandler handler) override
	{
		if (!handler) return {};
		return Items.Changed.Subscribe(
			[handler = std::move(handler)](
				ItemCollection*, const CollectionChangedEventArgs& change)
			{
				handler(change);
			});
	}
	bool TryGetItemOccurrenceIdentity(
		size_t index, size_t& result) const noexcept override
	{
		result = 0;
		if (Items.IsUpdating() || !_occurrenceMirrorSynchronized
			|| _occurrenceSynchronizedRevision != Items.MutationRevision()
			|| index >= _occurrenceIdentities.size()
			|| _occurrenceIdentities.size() != Items.size()) return false;
		result = _occurrenceIdentities[index];
		return result != 0;
	}
	bool TryGetItemIndexByOccurrenceIdentity(
		size_t identity, size_t& index) const noexcept override;
	bool IsItemIndexByOccurrenceIdentityLookupBounded()
		const noexcept override { return true; }
	bool TryGetStableSnapshot(BindingListReference& result) const override;
#if CUI_ENABLE_DYNAMIC_XAML
	const std::wstring& ItemTypeName() const noexcept override
	{
		return _itemTypeName;
	}
#endif
	DataTypeToken GetItemTypeToken() const noexcept override
	{
		return _itemTypeToken;
	}
#if CUI_ENABLE_DYNAMIC_XAML
	void SetItemTypeName(std::wstring value)
	{
		_itemTypeToken = MakeDataTypeToken(value);
		_itemTypeName = std::move(value);
	}
#endif
	void SetItemTypeToken(DataTypeToken value) noexcept
	{
		_itemTypeToken = value;
	}

	ItemCollection Items;

private:
	friend class cui::framework::BindingListTestAccess;
	static constexpr size_t StableSnapshotPageSize = 256;
	struct StableSnapshotEntry final
	{
		BindingSourceReference Item;
		size_t OccurrenceIdentity = 0;
	};
	struct StableSnapshotPage final
	{
		std::vector<StableSnapshotEntry> Entries;
	};
	struct StableSnapshotState final
	{
		std::vector<std::shared_ptr<StableSnapshotPage>> Pages;
		std::unordered_map<size_t,
			std::shared_ptr<std::vector<size_t>>> PositionPages;
		size_t Count = 0;
	};
	class StableSnapshot final
		: public IBindingList,
		  public IBindingListOccurrenceIdentity,
		  public IBindingListOccurrenceLookup,
		  public IBindingListStableSnapshot
	{
	public:
		StableSnapshot(
			std::shared_ptr<const StableSnapshotState> state,
			DataTypeToken itemTypeToken
#if CUI_ENABLE_DYNAMIC_XAML
			, std::wstring itemTypeName
#endif
			) noexcept
			: _state(std::move(state)),
			  _itemTypeToken(itemTypeToken)
#if CUI_ENABLE_DYNAMIC_XAML
			, _itemTypeName(std::move(itemTypeName))
#endif
		{
		}

		size_t Count() const noexcept override
		{
			return _state ? _state->Count : 0;
		}
		bool TryGetItem(
			size_t index, BindingSourceReference& out) const override
		{
			out = {};
			const auto* entry = EntryAt(index);
			if (!entry || !entry->Item) return false;
			out = entry->Item;
			return true;
		}
		EventConnection SubscribeChanged(ChangedHandler) override { return {}; }
		bool TryGetItemOccurrenceIdentity(
			size_t index, size_t& result) const noexcept override
		{
			result = 0;
			const auto* entry = EntryAt(index);
			if (!entry || entry->OccurrenceIdentity == 0) return false;
			result = entry->OccurrenceIdentity;
			return true;
		}
		bool TryGetItemIndexByOccurrenceIdentity(
			size_t identity, size_t& index) const noexcept override
		{
			index = 0;
			if (!_state || identity == 0) return false;
			const size_t tokenIndex = identity - 1;
			const size_t pageIndex = tokenIndex / StableSnapshotPageSize;
			const size_t offset = tokenIndex % StableSnapshotPageSize;
			const auto found = _state->PositionPages.find(pageIndex);
			if (found == _state->PositionPages.end() || !found->second
				|| offset >= found->second->size()) return false;
			index = (*found->second)[offset];
			const auto* entry = EntryAt(index);
			return entry && entry->OccurrenceIdentity == identity;
		}
		bool IsItemIndexByOccurrenceIdentityLookupBounded()
			const noexcept override { return true; }
		DataTypeToken GetItemTypeToken() const noexcept override
		{
			return _itemTypeToken;
		}
#if CUI_ENABLE_DYNAMIC_XAML
		const std::wstring& ItemTypeName() const noexcept override
		{
			return _itemTypeName;
		}
#endif

	private:
		const StableSnapshotEntry* EntryAt(size_t index) const noexcept
		{
			if (!_state || index >= _state->Count) return nullptr;
			const size_t pageIndex = index / StableSnapshotPageSize;
			const size_t offset = index % StableSnapshotPageSize;
			if (pageIndex >= _state->Pages.size()
				|| !_state->Pages[pageIndex]
				|| offset >= _state->Pages[pageIndex]->Entries.size())
				return nullptr;
			return &_state->Pages[pageIndex]->Entries[offset];
		}

		const std::shared_ptr<const StableSnapshotState> _state;
		const DataTypeToken _itemTypeToken;
#if CUI_ENABLE_DYNAMIC_XAML
		const std::wstring _itemTypeName;
#endif
	};

	size_t CreateOccurrenceIdentity() noexcept
	{
		if (_nextOccurrenceIdentity == 0
			|| _nextOccurrenceIdentity
				== (std::numeric_limits<size_t>::max)()) return 0;
		return _nextOccurrenceIdentity++;
	}

	void RebuildOccurrenceIdentities()
	{
		std::vector<size_t> rebuilt;
		rebuilt.reserve(Items.size());
		for (size_t index = 0; index < Items.size(); ++index)
		{
			const size_t identity = CreateOccurrenceIdentity();
			if (identity == 0)
			{
				rebuilt.clear();
				break;
			}
			rebuilt.push_back(identity);
		}
		_occurrenceIdentities = std::move(rebuilt);
	}

	void ApplyOccurrenceChange(const CollectionChangedEventArgs& change)
	{
		auto validRange = [](size_t index, size_t count, size_t size) noexcept
		{
			return index <= size && count <= size - index;
		};
		if (_occurrenceIdentities.size() != change.OldSize
			|| Items.size() != change.NewSize)
		{
			RebuildOccurrenceIdentities();
			return;
		}
		switch (change.Action)
		{
		case CollectionChangeAction::Add:
			if (change.OldCount != 0 || change.NewCount == 0
				|| change.NewIndex > _occurrenceIdentities.size()
				|| change.NewCount != Items.size()
					- _occurrenceIdentities.size()) break;
			{
				std::vector<size_t> additions;
				additions.reserve(change.NewCount);
				for (size_t index = 0; index < change.NewCount; ++index)
				{
					const size_t identity = CreateOccurrenceIdentity();
					if (identity == 0) break;
					additions.push_back(identity);
				}
				if (additions.size() != change.NewCount) break;
				_occurrenceIdentities.insert(
					_occurrenceIdentities.begin() + change.NewIndex,
					additions.begin(), additions.end());
				return;
			}
		case CollectionChangeAction::Remove:
			if (change.NewCount != 0 || change.OldCount == 0
				|| !validRange(change.OldIndex, change.OldCount,
					_occurrenceIdentities.size())
				|| _occurrenceIdentities.size() - change.OldCount
					!= Items.size()) break;
			_occurrenceIdentities.erase(
				_occurrenceIdentities.begin() + change.OldIndex,
				_occurrenceIdentities.begin() + change.OldIndex
					+ change.OldCount);
			return;
		case CollectionChangeAction::Move:
			if (change.OldCount == 0
				|| change.OldCount != change.NewCount
				|| !validRange(change.OldIndex, change.OldCount,
					_occurrenceIdentities.size())
				|| change.NewIndex > _occurrenceIdentities.size()
					- change.OldCount) break;
			{
				std::vector<size_t> moved(
					_occurrenceIdentities.begin() + change.OldIndex,
					_occurrenceIdentities.begin() + change.OldIndex
						+ change.OldCount);
				_occurrenceIdentities.erase(
					_occurrenceIdentities.begin() + change.OldIndex,
					_occurrenceIdentities.begin() + change.OldIndex
						+ change.OldCount);
				_occurrenceIdentities.insert(
					_occurrenceIdentities.begin() + change.NewIndex,
					moved.begin(), moved.end());
				return;
			}
		case CollectionChangeAction::Swap:
			if (change.OldCount != 1 || change.NewCount != 1
				|| change.OldIndex >= _occurrenceIdentities.size()
				|| change.NewIndex >= _occurrenceIdentities.size()) break;
			std::swap(_occurrenceIdentities[change.OldIndex],
				_occurrenceIdentities[change.NewIndex]);
			return;
		case CollectionChangeAction::Replace:
			if (change.OldIndex != change.NewIndex
				|| change.OldCount == 0
				|| change.OldCount != change.NewCount
				|| !validRange(change.OldIndex, change.OldCount,
					_occurrenceIdentities.size())) break;
			// Replace changes the record in an existing physical collection slot.
			return;
		case CollectionChangeAction::Reset:
		default:
			break;
		}
		RebuildOccurrenceIdentities();
	}

	void AttachOccurrenceTracking()
	{
		_occurrenceConnection = Items.Changed.Subscribe(
			[this](ItemCollection*, const CollectionChangedEventArgs& change)
			{
				try
				{
					if (_failNextOccurrenceMaintenanceForTesting)
					{
						_failNextOccurrenceMaintenanceForTesting = false;
						throw std::bad_alloc{};
					}
					ApplyOccurrenceChange(change);
					_occurrenceMirrorSynchronized =
						_occurrenceIdentities.size() == Items.size();
					_occurrenceSynchronizedRevision =
						Items.MutationRevision();
				}
				catch (...)
				{
					// Occurrence identity and the version checkpoint are internal
					// mirrors. The Items mutation is already committed, so allocation
					// failure invalidates both without suppressing later subscribers.
					_occurrenceIdentities.clear();
					_occurrenceMirrorSynchronized = false;
					_stableSnapshotState.reset();
					_snapshotMirrorSynchronized = false;
					++_snapshotStatistics.MaintenanceFailures;
					return;
				}
				try
				{
					if (_failNextSnapshotMaintenanceForTesting)
					{
						_failNextSnapshotMaintenanceForTesting = false;
						throw std::bad_alloc{};
					}
					ApplyStableSnapshotChange(change);
					_snapshotMirrorSynchronized =
						_stableSnapshotState
						&& _stableSnapshotState->Count == Items.size();
					_snapshotSynchronizedRevision =
						Items.MutationRevision();
				}
				catch (...)
				{
					_stableSnapshotState.reset();
					_snapshotMirrorSynchronized = false;
					++_snapshotStatistics.MaintenanceFailures;
				}
			});
	}

	void InitializeOccurrenceIdentities()
	{
		AttachOccurrenceTracking();
		RebuildOccurrenceIdentities();
		_occurrenceMirrorSynchronized =
			_occurrenceIdentities.size() == Items.size();
		_occurrenceSynchronizedRevision = Items.MutationRevision();
		RebuildStableSnapshotState();
	}

	static const StableSnapshotEntry& SnapshotEntryAt(
		const StableSnapshotState& state, size_t index)
	{
		return state.Pages.at(index / StableSnapshotPageSize)
			->Entries.at(index % StableSnapshotPageSize);
	}
	StableSnapshotEntry& MutableSnapshotEntryAt(
		StableSnapshotState& state, size_t index)
	{
		const size_t pageIndex = index / StableSnapshotPageSize;
		auto& page = state.Pages.at(pageIndex);
		if (page.use_count() != 1)
		{
			page = std::make_shared<StableSnapshotPage>(*page);
			++_snapshotStatistics.EntryPageCopies;
		}
		return page->Entries.at(index % StableSnapshotPageSize);
	}
	void AppendSnapshotEntry(
		StableSnapshotState& state, StableSnapshotEntry entry)
	{
		const size_t pageIndex = state.Count / StableSnapshotPageSize;
		if (pageIndex == state.Pages.size())
		{
			auto page = std::make_shared<StableSnapshotPage>();
			page->Entries.reserve(StableSnapshotPageSize);
			state.Pages.push_back(std::move(page));
		}
		auto& page = state.Pages[pageIndex];
		if (page.use_count() != 1)
		{
			page = std::make_shared<StableSnapshotPage>(*page);
			++_snapshotStatistics.EntryPageCopies;
		}
		page->Entries.push_back(std::move(entry));
		++state.Count;
	}
	void RemoveLastSnapshotEntry(StableSnapshotState& state)
	{
		if (state.Count == 0) return;
		const size_t pageIndex = (state.Count - 1) / StableSnapshotPageSize;
		auto& page = state.Pages.at(pageIndex);
		if (page.use_count() != 1)
		{
			page = std::make_shared<StableSnapshotPage>(*page);
			++_snapshotStatistics.EntryPageCopies;
		}
		page->Entries.pop_back();
		--state.Count;
		if (page->Entries.empty()) state.Pages.pop_back();
	}
	StableSnapshotEntry CurrentSnapshotEntry(size_t index) const
	{
		StableSnapshotEntry result;
		if (index >= Items.size()
			|| index >= _occurrenceIdentities.size()) return result;
		result.Item = Items[index];
		result.OccurrenceIdentity = _occurrenceIdentities[index];
		return result;
	}
	void SetSnapshotOccurrencePosition(
		StableSnapshotState& state, size_t identity, size_t index)
	{
		if (identity == 0) return;
		const size_t tokenIndex = identity - 1;
		const size_t pageIndex = tokenIndex / StableSnapshotPageSize;
		const size_t offset = tokenIndex % StableSnapshotPageSize;
		auto found = state.PositionPages.find(pageIndex);
		if (found == state.PositionPages.end())
		{
			if (index == CollectionChangedEventArgs::Npos) return;
			auto page = std::make_shared<std::vector<size_t>>(
				StableSnapshotPageSize, CollectionChangedEventArgs::Npos);
			found = state.PositionPages.emplace(
				pageIndex, std::move(page)).first;
		}
		auto& page = found->second;
		if (page.use_count() != 1)
		{
			page = std::make_shared<std::vector<size_t>>(*page);
			++_snapshotStatistics.PositionPageCopies;
		}
		(*page)[offset] = index;
		if (index == CollectionChangedEventArgs::Npos
			&& std::all_of(page->begin(), page->end(), [](size_t candidate)
				{ return candidate == CollectionChangedEventArgs::Npos; }))
			state.PositionPages.erase(found);
	}
	void RefreshSnapshotOccurrencePositions(
		StableSnapshotState& state, size_t first, size_t last)
	{
		last = (std::min)(last, state.Count);
		for (size_t index = first; index < last; ++index)
		{
			const auto& entry = SnapshotEntryAt(state, index);
			SetSnapshotOccurrencePosition(
				state, entry.OccurrenceIdentity, index);
		}
	}
	void RebuildStableSnapshotState()
	{
		++_snapshotStatistics.FullRebuilds;
		auto rebuilt = std::make_shared<StableSnapshotState>();
		rebuilt->Pages.reserve(
			(Items.size() + StableSnapshotPageSize - 1)
				/ StableSnapshotPageSize);
		for (size_t index = 0; index < Items.size(); ++index)
		{
			auto entry = CurrentSnapshotEntry(index);
			const size_t identity = entry.OccurrenceIdentity;
			AppendSnapshotEntry(*rebuilt, std::move(entry));
			SetSnapshotOccurrencePosition(*rebuilt, identity, index);
		}
		_stableSnapshotState = std::move(rebuilt);
		_snapshotMirrorSynchronized = true;
		_snapshotSynchronizedRevision = Items.MutationRevision();
	}
	void ApplyStableSnapshotChange(const CollectionChangedEventArgs& change)
	{
		if (!_stableSnapshotState
			|| _stableSnapshotState->Count != change.OldSize
			|| Items.size() != change.NewSize
			|| _occurrenceIdentities.size() != Items.size())
		{
			RebuildStableSnapshotState();
			return;
		}
		auto validRange = [](size_t index, size_t count, size_t size) noexcept
		{
			return index <= size && count <= size - index;
		};
		const bool copyRoot = _stableSnapshotState.use_count() != 1;
		auto candidate = copyRoot
			? std::make_shared<StableSnapshotState>(*_stableSnapshotState)
			: _stableSnapshotState;
		if (copyRoot) ++_snapshotStatistics.RootCopies;
		auto rebuild = [this]
		{
			RebuildStableSnapshotState();
		};
		try
		{
			switch (change.Action)
			{
			case CollectionChangeAction::Add:
				if (change.OldCount != 0 || change.NewCount == 0
					|| change.NewIndex > candidate->Count
					|| change.NewCount != change.NewSize - change.OldSize)
				{
					rebuild();
					return;
				}
				{
					std::vector<StableSnapshotEntry> additions;
					additions.reserve(change.NewCount);
					for (size_t offset = 0; offset < change.NewCount; ++offset)
						additions.push_back(CurrentSnapshotEntry(
							change.NewIndex + offset));
					const size_t oldCount = candidate->Count;
					for (size_t offset = 0; offset < change.NewCount; ++offset)
						AppendSnapshotEntry(*candidate, {});
					for (size_t index = oldCount; index > change.NewIndex; --index)
						MutableSnapshotEntryAt(*candidate,
							index + change.NewCount - 1) =
							SnapshotEntryAt(*candidate, index - 1);
					for (size_t offset = 0; offset < additions.size(); ++offset)
						MutableSnapshotEntryAt(*candidate,
							change.NewIndex + offset) = std::move(additions[offset]);
					RefreshSnapshotOccurrencePositions(
						*candidate, change.NewIndex, candidate->Count);
				}
				break;
			case CollectionChangeAction::Remove:
				if (change.NewCount != 0 || change.OldCount == 0
					|| !validRange(change.OldIndex, change.OldCount,
						candidate->Count)
					|| change.OldSize - change.OldCount != change.NewSize)
				{
					rebuild();
					return;
				}
				{
					std::vector<size_t> removedIdentities;
					removedIdentities.reserve(change.OldCount);
					for (size_t offset = 0; offset < change.OldCount; ++offset)
						removedIdentities.push_back(SnapshotEntryAt(
							*candidate, change.OldIndex + offset)
							.OccurrenceIdentity);
				for (size_t index = change.OldIndex;
					index + change.OldCount < candidate->Count; ++index)
					MutableSnapshotEntryAt(*candidate, index) =
						SnapshotEntryAt(*candidate, index + change.OldCount);
				for (size_t offset = 0; offset < change.OldCount; ++offset)
					RemoveLastSnapshotEntry(*candidate);
					for (const size_t identity : removedIdentities)
						SetSnapshotOccurrencePosition(*candidate, identity,
							CollectionChangedEventArgs::Npos);
					RefreshSnapshotOccurrencePositions(
						*candidate, change.OldIndex, candidate->Count);
				}
				break;
			case CollectionChangeAction::Replace:
				if (change.OldIndex != change.NewIndex
					|| change.OldCount == 0
					|| change.OldCount != change.NewCount
					|| !validRange(change.NewIndex, change.NewCount,
						candidate->Count))
				{
					rebuild();
					return;
				}
				for (size_t offset = 0; offset < change.NewCount; ++offset)
					MutableSnapshotEntryAt(*candidate,
						change.NewIndex + offset) = CurrentSnapshotEntry(
							change.NewIndex + offset);
				break;
			case CollectionChangeAction::Move:
				if (change.OldCount != 1 || change.NewCount != 1
					|| change.OldIndex >= candidate->Count
					|| change.NewIndex >= candidate->Count)
				{
					rebuild();
					return;
				}
				{
					auto moved = SnapshotEntryAt(*candidate, change.OldIndex);
					if (change.OldIndex < change.NewIndex)
						for (size_t index = change.OldIndex;
							index < change.NewIndex; ++index)
							MutableSnapshotEntryAt(*candidate, index) =
								SnapshotEntryAt(*candidate, index + 1);
					else
						for (size_t index = change.OldIndex;
							index > change.NewIndex; --index)
							MutableSnapshotEntryAt(*candidate, index) =
								SnapshotEntryAt(*candidate, index - 1);
					MutableSnapshotEntryAt(*candidate, change.NewIndex) =
						std::move(moved);
					RefreshSnapshotOccurrencePositions(*candidate,
						(std::min)(change.OldIndex, change.NewIndex),
						(std::max)(change.OldIndex, change.NewIndex) + 1);
				}
				break;
			case CollectionChangeAction::Swap:
				if (change.OldCount != 1 || change.NewCount != 1
					|| change.OldIndex >= candidate->Count
					|| change.NewIndex >= candidate->Count)
				{
					rebuild();
					return;
				}
				{
					auto first = SnapshotEntryAt(*candidate, change.OldIndex);
					auto second = SnapshotEntryAt(*candidate, change.NewIndex);
					MutableSnapshotEntryAt(*candidate, change.OldIndex) =
						std::move(second);
					MutableSnapshotEntryAt(*candidate, change.NewIndex) =
						std::move(first);
					RefreshSnapshotOccurrencePositions(
						*candidate, change.OldIndex, change.OldIndex + 1);
					RefreshSnapshotOccurrencePositions(
						*candidate, change.NewIndex, change.NewIndex + 1);
				}
				break;
			case CollectionChangeAction::Reset:
			default:
				rebuild();
				return;
			}
			if (candidate->Count != Items.size())
			{
				rebuild();
				return;
			}
			_stableSnapshotState = std::move(candidate);
		}
		catch (...)
		{
			// A shared previous checkpoint was never touched. Mark only the live
			// mirror unavailable so a consumer falls back to ordinary materialization;
			// never advertise a partially updated version as an immutable snapshot.
			_stableSnapshotState.reset();
			throw;
		}
	}

	DataTypeToken _itemTypeToken;
#if CUI_ENABLE_DYNAMIC_XAML
	std::wstring _itemTypeName;
#endif
	std::vector<size_t> _occurrenceIdentities;
	std::uint64_t _occurrenceSynchronizedRevision = 0;
	std::uint64_t _snapshotSynchronizedRevision = 0;
	size_t _nextOccurrenceIdentity = 1;
	EventConnection _occurrenceConnection;
	std::shared_ptr<StableSnapshotState> _stableSnapshotState;
	bool _occurrenceMirrorSynchronized = false;
	bool _snapshotMirrorSynchronized = false;
	mutable BindingListSnapshotStatistics _snapshotStatistics;
	bool _failNextSnapshotMaintenanceForTesting = false;
	bool _failNextOccurrenceMaintenanceForTesting = false;
};

namespace cui::framework
{
	/** White-box access kept out of ObservableBindingList's application API. */
	class BindingListTestAccess final
	{
	public:
		static BindingListSnapshotStatistics SnapshotStatistics(
			const ObservableBindingList& source) noexcept
		{
			auto result = source._snapshotStatistics;
			if (source._stableSnapshotState)
			{
				result.CurrentEntryPages =
					source._stableSnapshotState->Pages.size();
				result.CurrentPositionPages =
					source._stableSnapshotState->PositionPages.size();
			}
			return result;
		}
		static void FailNextSnapshotMaintenance(
			ObservableBindingList& source) noexcept
		{
			source._failNextSnapshotMaintenanceForTesting = true;
		}
		static void FailNextOccurrenceMaintenance(
			ObservableBindingList& source) noexcept
		{
			source._failNextOccurrenceMaintenanceForTesting = true;
		}
	};
}

inline bool ObservableBindingList::TryGetStableSnapshot(
	BindingListReference& result) const
{
	result = {};
	const auto state = _stableSnapshotState;
	if (Items.IsUpdating() || !_occurrenceMirrorSynchronized
		|| !_snapshotMirrorSynchronized
		|| _occurrenceSynchronizedRevision != Items.MutationRevision()
		|| _snapshotSynchronizedRevision != Items.MutationRevision()
		|| !state || state->Count != Items.size()
		|| _occurrenceIdentities.size() != Items.size()) return false;
	++_snapshotStatistics.Captures;
	result = BindingListReference(std::make_shared<StableSnapshot>(
		state, _itemTypeToken
#if CUI_ENABLE_DYNAMIC_XAML
		, _itemTypeName
#endif
		));
	return true;
}

inline bool ObservableBindingList::TryGetItemIndexByOccurrenceIdentity(
	size_t identity, size_t& index) const noexcept
{
	index = 0;
	if (identity == 0 || Items.IsUpdating()
		|| !_occurrenceMirrorSynchronized || !_snapshotMirrorSynchronized
		|| _occurrenceSynchronizedRevision != Items.MutationRevision()
		|| _snapshotSynchronizedRevision != Items.MutationRevision()
		|| !_stableSnapshotState) return false;
	const size_t tokenIndex = identity - 1;
	const size_t pageIndex = tokenIndex / StableSnapshotPageSize;
	const size_t offset = tokenIndex % StableSnapshotPageSize;
	const auto found = _stableSnapshotState->PositionPages.find(pageIndex);
	if (found == _stableSnapshotState->PositionPages.end() || !found->second
		|| offset >= found->second->size()) return false;
	const size_t candidate = (*found->second)[offset];
	if (candidate >= Items.size()
		|| candidate >= _occurrenceIdentities.size()
		|| _occurrenceIdentities[candidate] != identity) return false;
	index = candidate;
	return true;
}

/**
 * Immutable list emitted by the AOT compiler for static DataList resources.
 * It owns only record references plus their stable type token and has no
 * observable-collection machinery. Production carries no diagnostic type
 * name at all.
 */
class CompiledBindingList final
	: public IBindingList,
	  public IBindingListOccurrenceIdentity,
	  public IBindingListOccurrenceLookup,
	  public IBindingListStableSnapshot
{
public:
	explicit CompiledBindingList(
		std::vector<BindingSourceReference> items = {},
		DataTypeToken itemTypeToken = {}
#if CUI_ENABLE_DYNAMIC_XAML
		, std::wstring diagnosticItemTypeName = {}
#endif
		) noexcept
		: _items(std::move(items)),
		  _itemTypeToken(itemTypeToken)
#if CUI_ENABLE_DYNAMIC_XAML
		, _diagnosticItemTypeName(std::move(diagnosticItemTypeName))
#endif
	{
	}

	size_t Count() const noexcept override { return _items.size(); }
	bool TryGetItem(size_t index, BindingSourceReference& out) const override
	{
		if (index >= _items.size() || !_items[index]) return false;
		out = _items[index];
		return true;
	}
	EventConnection SubscribeChanged(ChangedHandler) override { return {}; }
	bool TryGetItemOccurrenceIdentity(
		size_t index, size_t& result) const noexcept override
	{
		result = 0;
		if (index >= _items.size()) return false;
		result = index + 1;
		return result != 0;
	}
	bool TryGetItemIndexByOccurrenceIdentity(
		size_t identity, size_t& index) const noexcept override
	{
		index = 0;
		if (identity == 0 || identity > _items.size()) return false;
		index = identity - 1;
		return true;
	}
	bool IsItemIndexByOccurrenceIdentityLookupBounded()
		const noexcept override { return true; }
	DataTypeToken GetItemTypeToken() const noexcept override
	{
		return _itemTypeToken;
	}
#if CUI_ENABLE_DYNAMIC_XAML
	const std::wstring& ItemTypeName() const noexcept override
	{
		return _diagnosticItemTypeName;
	}
#endif

private:
	const std::vector<BindingSourceReference> _items;
	const DataTypeToken _itemTypeToken;
#if CUI_ENABLE_DYNAMIC_XAML
	const std::wstring _diagnosticItemTypeName;
#endif
};

/** Owns all subscriptions needed to observe member/indexer record paths. */
struct BindingPathObservation final
{
	std::vector<std::shared_ptr<IBindingSource>> Owners;
	std::vector<std::shared_ptr<IBindingList>> ListOwners;
	std::vector<EventConnection> Connections;
};

#if CUI_ENABLE_DYNAMIC_XAML
inline BindingPathObservation ObserveBindingPaths(
	const BindingSourceReference& source,
	std::span<const std::wstring> paths,
	std::function<void()> changed)
{
	BindingPathObservation result;
	if (!source || !changed) return result;
	result.Owners.push_back(source.Shared());
	struct PathWatch final
	{
		IBindingSource* Source = nullptr;
		std::unordered_set<std::wstring> Properties;
		bool AllProperties = false;
	};
	std::vector<PathWatch> watches;
	std::vector<IBindingList*> listWatches;
	auto watch = [&](IBindingSource& current, std::wstring property)
	{
		auto found = std::find_if(watches.begin(), watches.end(),
			[&](const auto& candidate) { return candidate.Source == &current; });
		if (found == watches.end())
		{
			watches.push_back(PathWatch{ &current });
			found = std::prev(watches.end());
		}
		if (property.empty()) found->AllProperties = true;
		else found->Properties.insert(std::move(property));
	};
	for (const auto& rawPath : paths)
	{
		if (rawPath.empty())
		{
			watch(*source.Get(), {});
			continue;
		}
		std::vector<BindingPathStep> steps;
		if (!TryParseBindingPropertyPath(rawPath, steps)) continue;
		IBindingSource* currentSource = source.Get();
		IBindingList* currentList = nullptr;
		for (size_t index = 0; index < steps.size(); ++index)
		{
			const auto& step = steps[index];
			if (currentSource) watch(*currentSource, step.Value);
			else if (currentList
				&& std::find(listWatches.begin(), listWatches.end(), currentList)
					== listWatches.end())
				listWatches.push_back(currentList);
			if (index + 1 == steps.size()) break;

			BindingValue value;
			if (currentSource)
			{
				if (!currentSource->TryGetValue(step.Value, value)) break;
			}
			else
			{
				if (!currentList || step.Kind != BindingPathStepKind::Indexer) break;
				size_t itemIndex = 0;
				if (step.Value.empty()
					|| !std::all_of(step.Value.begin(), step.Value.end(),
						[](wchar_t ch) { return std::iswdigit(ch) != 0; })) break;
				try
				{
					itemIndex = static_cast<size_t>(std::stoull(step.Value));
				}
				catch (...)
				{
					break;
				}
				BindingSourceReference item;
				if (!currentList->TryGetItem(itemIndex, item) || !item) break;
				value = BindingValue(item);
			}

			BindingSourceReference nested;
			if (value.TryGet(nested) && nested)
			{
				result.Owners.push_back(nested.Shared());
				currentSource = nested.Get();
				currentList = nullptr;
				continue;
			}
			BindingListReference nestedList;
			if (value.TryGet(nestedList) && nestedList)
			{
				result.ListOwners.push_back(nestedList.Shared());
				currentSource = nullptr;
				currentList = nestedList.Get();
				continue;
			}
			break;
		}
	}
	for (auto& pathWatch : watches)
	{
		const bool allProperties = pathWatch.AllProperties;
		auto properties = std::move(pathWatch.Properties);
		result.Connections.push_back(pathWatch.Source->PropertyChanged().Subscribe(
			[changed, allProperties, properties = std::move(properties)](
				const PropertyChangedEventArgs& args)
			{
				if (allProperties || args.PropertyName.empty()
					|| properties.contains(args.PropertyName)) changed();
			}));
	}
	for (auto* list : listWatches)
		result.Connections.push_back(list->SubscribeChanged(
			[changed](const CollectionChangedEventArgs&) { changed(); }));
	return result;
}

inline BindingPathObservation ObserveBindingPaths(
	const BindingSourceReference& source,
	std::initializer_list<std::wstring> paths,
	std::function<void()> changed)
{
	return ObserveBindingPaths(
		source,
		std::span<const std::wstring>{ paths.begin(), paths.size() },
		std::move(changed));
}
#endif

/**
 * Observes immutable AOT binding paths without parsing or retaining member
 * names.  An empty path keeps the legacy "whole record" meaning; token-less
 * notifications are likewise treated as an all-properties invalidation.
 */
inline BindingPathObservation ObserveBindingPaths(
	const BindingSourceReference& source,
	std::span<const CompiledBindingPathView> paths,
	std::function<void()> changed)
{
	BindingPathObservation result;
	if (!source || !changed) return result;
	result.Owners.push_back(source.Shared());
	struct PathWatch final
	{
		IBindingSource* Source = nullptr;
		std::unordered_set<std::uint64_t> Properties;
		bool AllProperties = false;
	};
	std::vector<PathWatch> watches;
	std::vector<IBindingList*> listWatches;
	auto watch = [&](IBindingSource& current,
		BindingSourcePropertyToken property)
	{
		auto found = std::find_if(watches.begin(), watches.end(),
			[&](const auto& candidate) { return candidate.Source == &current; });
		if (found == watches.end())
		{
			watches.push_back(PathWatch{ &current });
			found = std::prev(watches.end());
		}
		if (!property) found->AllProperties = true;
		else found->Properties.insert(property.Value);
	};
	auto watchList = [&](IBindingList& list)
	{
		if (std::find(listWatches.begin(), listWatches.end(), &list)
			== listWatches.end())
			listWatches.push_back(&list);
	};

	for (const auto path : paths)
	{
		if (path.Version != CompiledBindingPathVersion) continue;
		if (path.Empty())
		{
			watch(*source.Get(), {});
			continue;
		}
		IBindingSource* currentSource = source.Get();
		IBindingList* currentList = nullptr;
		for (size_t index = 0; index < path.Steps.size(); ++index)
		{
			const auto& step = path.Steps[index];
			if (currentSource)
			{
				if (step.Kind != CompiledBindingPathStepKind::Property
					|| !step.Property) break;
				if (HasCompiledBindingPathCapability(step.Capabilities,
					CompiledBindingPathCapabilities::Observe))
					watch(*currentSource, step.Property);
			}
			else if (currentList)
			{
				if (step.Kind != CompiledBindingPathStepKind::ListIndex) break;
				watchList(*currentList);
			}
			else
			{
				break;
			}
			if (index + 1 == path.Steps.size()) break;

			BindingValue value;
			if (currentSource)
			{
				if (!HasCompiledBindingPathCapability(step.Capabilities,
						CompiledBindingPathCapabilities::Read)
					|| !currentSource->TryGetValue(step.Property, value)) break;
			}
			else
			{
				BindingSourceReference item;
				if (!currentList->TryGetItem(step.ListIndex, item) || !item) break;
				value = BindingValue(std::move(item));
			}

			BindingSourceReference nested;
			if (value.TryGet(nested) && nested)
			{
				result.Owners.push_back(nested.Shared());
				currentSource = nested.Get();
				currentList = nullptr;
				continue;
			}
			BindingListReference nestedList;
			if (value.TryGet(nestedList) && nestedList)
			{
				result.ListOwners.push_back(nestedList.Shared());
				currentSource = nullptr;
				currentList = nestedList.Get();
				continue;
			}
			break;
		}
	}

	for (auto& pathWatch : watches)
	{
		const bool allProperties = pathWatch.AllProperties;
		auto properties = std::move(pathWatch.Properties);
		result.Connections.push_back(pathWatch.Source->PropertyChanged().Subscribe(
			[changed, allProperties, properties = std::move(properties)](
				const PropertyChangedEventArgs& args)
			{
				if (allProperties || !args.PropertyToken
					|| properties.contains(args.PropertyToken.Value)) changed();
			}));
	}
	for (auto* list : listWatches)
		result.Connections.push_back(list->SubscribeChanged(
			[changed](const CollectionChangedEventArgs&) { changed(); }));
	return result;
}

inline BindingPathObservation ObserveBindingPaths(
	const BindingSourceReference& source,
	std::initializer_list<CompiledBindingPathView> paths,
	std::function<void()> changed)
{
	return ObserveBindingPaths(
		source,
		std::span<const CompiledBindingPathView>{ paths.begin(), paths.size() },
		std::move(changed));
}

#if CUI_ENABLE_DYNAMIC_XAML
inline std::wstring GetBindingRecordText(
	const BindingSourceReference& item,
	const std::wstring& path)
{
	if (!item || path.empty()) return {};
	BindingValue value;
	return TryGetBindingPathValue(*item.Get(), path, value)
		? value.ToString() : std::wstring{};
}
#endif

inline std::wstring GetBindingRecordText(
	const BindingSourceReference& item,
	CompiledBindingPathView path)
{
	if (!item || path.Empty()) return {};
	BindingValue value;
	return TryGetBindingPathValue(*item.Get(), path, value)
		? value.ToString() : std::wstring{};
}

/** Reads one ItemsSource value. An empty path denotes the item record itself. */
#if CUI_ENABLE_DYNAMIC_XAML
inline bool TryGetBindingListItemValue(
	const BindingListReference& source,
	size_t index,
	const std::wstring& path,
	BindingValue& out)
{
	if (!source) return false;
	BindingSourceReference item;
	if (!source.Get()->TryGetItem(index, item) || !item) return false;
	if (path.empty())
	{
		out = BindingValue(item);
		return true;
	}
	return TryGetBindingPathValue(*item.Get(), path, out);
}
#endif

inline bool TryGetBindingListItemValue(
	const BindingListReference& source,
	size_t index,
	CompiledBindingPathView path,
	BindingValue& out)
{
	if (!source) return false;
	BindingSourceReference item;
	if (!source.Get()->TryGetItem(index, item) || !item) return false;
	if (path.Empty())
	{
		out = BindingValue(item);
		return true;
	}
	return TryGetBindingPathValue(*item.Get(), path, out);
}

/** SelectedValue equality preserves scalar types and record identity. */
inline bool BindingItemValuesEqual(
	const BindingValue& left,
	const BindingValue& right)
{
	BindingSourceReference leftItem;
	BindingSourceReference rightItem;
	if (left.TryGet(leftItem) && right.TryGet(rightItem))
		return leftItem.Shared() == rightItem.Shared();
	return BindingValuesEqual(left, right);
}

#if CUI_ENABLE_DYNAMIC_XAML
inline int FindBindingListItemByValue(
	const BindingListReference& source,
	const std::wstring& path,
	const BindingValue& value)
{
	if (!source) return -1;
	for (size_t index = 0; index < source.Get()->Count(); ++index)
	{
		BindingValue candidate;
		if (TryGetBindingListItemValue(source, index, path, candidate)
			&& BindingItemValuesEqual(candidate, value))
			return static_cast<int>(index);
	}
	return -1;
}
#endif

inline int FindBindingListItemByValue(
	const BindingListReference& source,
	CompiledBindingPathView path,
	const BindingValue& value)
{
	if (!source) return -1;
	for (size_t index = 0; index < source.Get()->Count(); ++index)
	{
		BindingValue candidate;
		if (TryGetBindingListItemValue(source, index, path, candidate)
			&& BindingItemValuesEqual(candidate, value))
			return static_cast<int>(index);
	}
	return -1;
}
