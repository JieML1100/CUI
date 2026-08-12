#pragma once

#include "ControlWeakReference.h"
#include "Selector.h"

#include <iterator>
#include <vector>

/** WPF ListBox selection policy. */
enum class SelectionMode : int
{
	Single = 0,
	Multiple = 1,
	Extended = 2,
};

/**
 * Read-only selected-index view.
 *
 * The common shape is a small sorted vector. SelectAll may instead retain the
 * logical [0, Count) range plus sparse exclusions, so querying membership,
 * size, front/back, or a realized container does not allocate Count integers.
 * Iteration is deliberately lazy; clients that enumerate every selected index
 * still pay O(selected count), matching WPF's SelectedItems enumeration edge.
 */
class SelectedIndexCollection final
{
public:
	struct ExcludedInterval final
	{
		size_t Start = 0;
		size_t Count = 0;

		bool operator==(const ExcludedInterval& other) const noexcept
		{
			return Start == other.Start && Count == other.Count;
		}
	};

	/**
	 * Sorted logical view of every excluded index.
	 *
	 * Storage is interval-compressed, while this view deliberately retains the
	 * old vector-like contract used by selection/UIA consumers. Enumerating all
	 * excluded indices is still O(excluded count); interval-aware code should use
	 * Intervals()/CountInRange() instead.
	 */
	class ExcludedIndexCollection final
	{
	public:
		class const_iterator final
		{
		public:
			using iterator_category = std::random_access_iterator_tag;
			using iterator_concept = std::random_access_iterator_tag;
			using value_type = int;
			using difference_type = std::ptrdiff_t;
			using pointer = void;
			using reference = int;

			const_iterator() = default;
			int operator*() const noexcept;
			int operator[](difference_type offset) const noexcept
			{ return *(*this + offset); }
			const_iterator& operator++() noexcept
			{ ++_ordinal; return *this; }
			const_iterator operator++(int) noexcept
			{ auto copy = *this; ++*this; return copy; }
			const_iterator& operator--() noexcept
			{ --_ordinal; return *this; }
			const_iterator operator--(int) noexcept
			{ auto copy = *this; --*this; return copy; }
			const_iterator& operator+=(difference_type offset) noexcept;
			const_iterator& operator-=(difference_type offset) noexcept
			{ return *this += -offset; }
			friend const_iterator operator+(
				const_iterator iterator, difference_type offset) noexcept
			{ iterator += offset; return iterator; }
			friend const_iterator operator+(
				difference_type offset, const_iterator iterator) noexcept
			{ iterator += offset; return iterator; }
			friend const_iterator operator-(
				const_iterator iterator, difference_type offset) noexcept
			{ iterator -= offset; return iterator; }
			friend difference_type operator-(
				const const_iterator& left,
				const const_iterator& right) noexcept
			{
				return static_cast<difference_type>(left._ordinal)
					- static_cast<difference_type>(right._ordinal);
			}
			bool operator==(const const_iterator& other) const noexcept
			{
				return _owner == other._owner && _ordinal == other._ordinal;
			}
			bool operator<(const const_iterator& other) const noexcept
			{ return _ordinal < other._ordinal; }
			bool operator>(const const_iterator& other) const noexcept
			{ return other < *this; }
			bool operator<=(const const_iterator& other) const noexcept
			{ return !(other < *this); }
			bool operator>=(const const_iterator& other) const noexcept
			{ return !(*this < other); }

		private:
			friend class ExcludedIndexCollection;
			const_iterator(const ExcludedIndexCollection* owner,
				size_t ordinal) noexcept : _owner(owner), _ordinal(ordinal) {}
			const ExcludedIndexCollection* _owner = nullptr;
			size_t _ordinal = 0;
		};

		size_t size() const noexcept { return _count; }
		bool empty() const noexcept { return _count == 0; }
		int operator[](size_t ordinal) const noexcept;
		const_iterator begin() const noexcept { return { this, 0 }; }
		const_iterator end() const noexcept { return { this, size() }; }
		bool Contains(int value) const noexcept;
		size_t CountBefore(size_t exclusiveEnd) const noexcept;
		size_t CountInRange(size_t start, size_t count) const noexcept;
		const std::vector<ExcludedInterval>& Intervals() const noexcept
		{ return _intervals; }
		operator std::vector<int>() const;
		bool operator==(const ExcludedIndexCollection& other) const noexcept
		{ return _intervals == other._intervals; }

	private:
		friend class SelectedIndexCollection;
		void Clear() noexcept;
		void SetRanges(
			std::vector<ExcludedInterval> ranges, size_t rangeCount);
		void AddRange(size_t start, size_t count, size_t rangeCount);
		bool Toggle(size_t value);
		void RebuildPrefixCounts();

		std::vector<ExcludedInterval> _intervals;
		std::vector<size_t> _prefixCounts;
		size_t _count = 0;
	};

	class const_iterator final
	{
	public:
		using iterator_category = std::forward_iterator_tag;
		using value_type = int;
		using difference_type = std::ptrdiff_t;
		using pointer = void;
		using reference = int;

		const_iterator() = default;
		int operator*() const noexcept;
		const_iterator& operator++() noexcept;
		const_iterator operator++(int) noexcept
		{
			auto copy = *this;
			++*this;
			return copy;
		}
		bool operator==(const const_iterator& other) const noexcept
		{
			return _owner == other._owner && _ordinal == other._ordinal;
		}

	private:
		friend class SelectedIndexCollection;
		const_iterator(
			const SelectedIndexCollection* owner, size_t ordinal) noexcept
			: _owner(owner), _ordinal(ordinal) {}
		const SelectedIndexCollection* _owner = nullptr;
		size_t _ordinal = 0;
	};

	SelectedIndexCollection() = default;
	explicit SelectedIndexCollection(std::vector<int> values)
		: _values(std::move(values)) {}

	size_t size() const noexcept
	{
		return _fullRange ? _rangeCount - _excluded.size() : _values.size();
	}
	bool empty() const noexcept { return size() == 0; }
	int operator[](size_t ordinal) const noexcept;
	int front() const noexcept { return (*this)[0]; }
	int back() const noexcept { return (*this)[size() - 1]; }
	const_iterator begin() const noexcept { return { this, 0 }; }
	const_iterator end() const noexcept { return { this, size() }; }
	bool Contains(int value) const noexcept;
	bool IsFullRange() const noexcept
	{
		return _fullRange && _excluded.empty();
	}
	bool IsRangeBacked() const noexcept { return RangeCount() != 0; }
	size_t RangeCount() const noexcept { return _fullRange ? _rangeCount : 0; }
	const ExcludedIndexCollection& ExcludedIndices() const noexcept
	{
		return _excluded;
	}
	const std::vector<ExcludedInterval>& ExcludedIntervals() const noexcept
	{
		return _excluded.Intervals();
	}
	const std::vector<int>& DenseValues() const noexcept { return _values; }
	void SetDense(std::vector<int> values);
	void SetFullRange(size_t count);
	void SetExcludedRanges(std::vector<ExcludedInterval> ranges);
	void ExcludeRange(size_t start, size_t count);
	void Clear() noexcept;
	bool Toggle(int value);

	friend bool operator==(
		const SelectedIndexCollection& left,
		const SelectedIndexCollection& right) noexcept;
	friend bool operator==(
		const SelectedIndexCollection& left,
		const std::vector<int>& right) noexcept;
	friend bool operator==(
		const std::vector<int>& left,
		const SelectedIndexCollection& right) noexcept
	{
		return right == left;
	}

private:
	std::vector<int> _values;
	ExcludedIndexCollection _excluded;
	size_t _rangeCount = 0;
	bool _fullRange = false;
};

/** Templated list selector. ListView specializes its item container. */
class ListBox : public Selector
{
protected:
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override
	{
		return std::make_unique<ListBoxAutomationPeer>(*this);
	}

public:
	ListBox();
	UIClass Type() override { return UIClass::UI_ListBox; }
	/** WPF ListBox.SelectionMode property identity. */
	static const DependencyProperty& SelectionModeProperty();
	static void RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	void EnsureBindingPropertiesRegistered() override
	{
		RegisterDependencyProperties();
	}
#endif

	SelectionMode GetSelectionMode() const noexcept
	{
		return _selectionMode;
	}
	void SetSelectionMode(SelectionMode value);
	__declspec(property(get = GetSelectionMode, put = SetSelectionMode))
		SelectionMode SelectionModeValue;

	const SelectedIndexCollection& GetSelectedIndices() const noexcept
	{
		return _selectedIndices;
	}
	size_t SelectedIndexCount() const noexcept
	{
		return _selectedIndices.size();
	}
	bool AreAllItemsSelected() const noexcept
	{
		return _selectedIndices.IsFullRange()
			&& _selectedIndices.RangeCount() == SelectionItemCount();
	}
	bool IsSelectedIndexRangeBacked() const noexcept
	{
		return _selectedIndices.IsRangeBacked();
	}
	std::vector<BindingValue> GetSelectedItems() const;
	void SelectAll();
	void UnselectAll();
	bool SelectIndex(int value) override;
	bool IsIndexSelected(size_t index) const noexcept override;

	/** Framework-facing item-container callbacks. */
	void NotifyItemClicked(
		size_t index, MouseButton button, ModifierKeys modifiers);
	void RequestItemSelection(size_t index, bool selected);
	bool ProcessItemKey(size_t itemIndex, const InputReport& input);

	bool HandlesNavigationKey(Key key) const override;
	// ListBox owns an item viewport even when a theme does not supply the
	// conventional ScrollViewer template. Generated containers must not paint
	// beyond the selector's arranged slot.
	bool ClipsChildren() override { return true; }

protected:
	bool ProcessInput(const InputReport& input) override;
	std::unique_ptr<ItemsSourceTransactionState>
		CaptureItemsSourceTransactionState() override;
	void RestoreItemsSourceTransactionState(
		ItemsSourceTransactionState& state) noexcept override;
	void OnItemsSourceCollectionChangePreparing(
		const CollectionChangedEventArgs& change,
		const BindingListReference& previousSnapshot) override;
	void OnItemsSourceTransactionCommitted() override;
	bool TryResolveSelectionOccurrencePermutation(
		const BindingListReference& previousSnapshot,
		std::vector<size_t>& oldToNew) const noexcept;
	void OnSelectedIndexChanged(int oldValue, int newValue) override;
	void OnGeneratedItemsRebuilt() override;
	void OnAuthoredItemsChanged() noexcept override;

private:
	struct SourceSelectionIdentity final
	{
		BindingSourceReference Item;
		size_t Occurrence = 0;
		bool HasOccurrence = false;
	};
	struct ListBoxItemsSourceTransactionState final
		: ItemsSourceTransactionState
	{
		std::unique_ptr<ItemsSourceTransactionState> SelectorState;
		SelectedIndexCollection SelectedIndices;
		BindingListReference FullRangeSnapshot;
		SelectionChangedItemCollection PendingRemovedItems;
		SelectionChangedItemCollection PendingAddedItems;
		std::vector<SourceSelectionIdentity> SelectedSourceIdentities;
		std::vector<ControlWeakReference> SelectedAuthoredIdentities;
		SourceSelectionIdentity PrimarySourceIdentity;
		SourceSelectionIdentity AnchorSourceIdentity;
		ControlWeakReference PrimaryAuthoredIdentity;
		ControlWeakReference AnchorAuthoredIdentity;
		int PendingOldPrimary = -1;
		bool PendingSelectionChange = false;
		bool SkipSelectionIdentityRestoreOnce = false;
		bool ApplyingSelection = false;
		int AnchorIndex = -1;
		int FocusedIndex = -1;
		size_t SelectionRevision = 1;
	};
	SelectionMode _selectionMode = SelectionMode::Single;
	SelectedIndexCollection _selectedIndices;
	int _anchorIndex = -1;
	int _focusedIndex = -1;
	bool _applyingSelection = false;
	bool _restoringSelectionIdentities = false;
	std::vector<SourceSelectionIdentity> _selectedSourceIdentities;
	std::vector<ControlWeakReference> _selectedAuthoredIdentities;
	SourceSelectionIdentity _primarySourceIdentity;
	SourceSelectionIdentity _anchorSourceIdentity;
	ControlWeakReference _primaryAuthoredIdentity;
	ControlWeakReference _anchorAuthoredIdentity;
	SelectionChangedItemCollection _pendingSelectionRemovedItems;
	SelectionChangedItemCollection _pendingSelectionAddedItems;
	int _pendingSelectionOldPrimary = -1;
	bool _pendingSelectionChange = false;
	bool _skipSelectionIdentityRestoreOnce = false;
	size_t _selectionRevision = 1;

	void ApplySelectionModeChange(
		SelectionMode oldValue, SelectionMode newValue);
	bool ApplySelection(
		std::vector<int> indices,
		int preferredPrimary,
		int actionIndex);
	bool ApplySelection(
		SelectedIndexCollection indices,
		int preferredPrimary,
		int actionIndex);
	void SelectOnly(int index);
	void ToggleIndex(int index);
	void SelectRange(int index, bool clearCurrent);
	void MakeKeyboardSelection(int index, ModifierKeys modifiers);
	bool HandleSelectionKey(
		int itemIndex, const InputReport& input, bool fromItem);
	void FocusIndex(int index);
	void UpdateAnchor(int index) noexcept;
	void CaptureSelectionIdentities() noexcept;
	bool TryGetStableItemsSnapshot(BindingListReference& snapshot) const noexcept;
	bool TryCaptureFullRangeSnapshot() noexcept;
	bool UpdatePrimarySelectionState(
		int primary,
		bool refreshSelectedItemState,
		bool ensureSelectedItemVisible = true);
	bool TryRestoreRangeSelectionAfterReset(
		const CollectionChangedEventArgs& change,
		const BindingListReference& previousSnapshot,
		const SelectedIndexCollection& previousIndices,
		int previousPrimary,
		int previousAnchor,
		int previousFocus,
		bool& aborted);
	void AdvanceSelectionRevision() noexcept
	{
		if (++_selectionRevision == 0) _selectionRevision = 1;
	}
	void RestoreSelectionIdentities();
	BindingListReference _selectedFullRangeSnapshot;
};
