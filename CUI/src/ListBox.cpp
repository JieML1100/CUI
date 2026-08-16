#include "ListBox.h"

#include "DependencyPropertyInfrastructure.h"
#include "Window.h"

#include <algorithm>
#include <limits>
#include <unordered_map>
#include <utility>

namespace
{
	template<typename TCallback>
	class ListBoxScopeExit final
	{
	public:
		explicit ListBoxScopeExit(TCallback callback)
			: _callback(std::move(callback)) {}
		ListBoxScopeExit(const ListBoxScopeExit&) = delete;
		ListBoxScopeExit& operator=(const ListBoxScopeExit&) = delete;
		~ListBoxScopeExit() noexcept { _callback(); }

	private:
		TCallback _callback;
	};
	template<typename TCallback>
	ListBoxScopeExit(TCallback) -> ListBoxScopeExit<TCallback>;

	template<typename TCollection>
	bool ContainsIndex(const TCollection& values, int value)
	{
		if constexpr (requires { values.Contains(value); })
			return values.Contains(value);
		else return std::binary_search(values.begin(), values.end(), value);
	}

	using SelectionExclusionInterval =
		SelectedIndexCollection::ExcludedInterval;

	void AppendMappedExclusionIntersection(
		std::vector<SelectionExclusionInterval>& result,
		const SelectionExclusionInterval& interval,
		size_t sourceStart, size_t sourceEnd, size_t destinationStart)
	{
		const size_t intervalEnd = interval.Start + interval.Count;
		const size_t begin = (std::max)(interval.Start, sourceStart);
		const size_t end = (std::min)(intervalEnd, sourceEnd);
		if (begin >= end) return;
		result.push_back({ destinationStart + begin - sourceStart, end - begin });
	}

	std::vector<SelectionExclusionInterval> MapSelectionExclusionIntervals(
		const SelectedIndexCollection& previous,
		const CollectionChangedEventArgs& change)
	{
		struct TranslationSegment final
		{
			size_t SourceStart = 0;
			size_t SourceEnd = 0;
			size_t DestinationStart = 0;
		};
		const size_t oldCount = previous.RangeCount();
		std::vector<TranslationSegment> segments;
		const auto append = [&](size_t sourceStart, size_t sourceEnd,
			size_t destinationStart)
		{
			sourceStart = (std::min)(sourceStart, oldCount);
			sourceEnd = (std::min)(sourceEnd, oldCount);
			if (sourceStart < sourceEnd)
				segments.push_back({ sourceStart, sourceEnd, destinationStart });
		};
		const auto boundedEnd = [](size_t start, size_t count, size_t limit)
		{
			return start >= limit ? limit
				: start + (std::min)(count, limit - start);
		};

		switch (change.Action)
		{
		case CollectionChangeAction::Add:
			if (change.NewIndex == CollectionChangedEventArgs::Npos)
				append(0, oldCount, 0);
			else
			{
				const size_t insertion = (std::min)(change.NewIndex, oldCount);
				append(0, insertion, 0);
				append(insertion, oldCount, insertion + change.NewCount);
			}
			break;
		case CollectionChangeAction::Remove:
		case CollectionChangeAction::Replace:
			if (change.OldIndex == CollectionChangedEventArgs::Npos)
				append(0, oldCount, 0);
			else
			{
				const size_t removal = (std::min)(change.OldIndex, oldCount);
				const size_t removalEnd = boundedEnd(
					removal, change.OldCount, oldCount);
				append(0, removal, 0);
				append(removalEnd, oldCount,
					removal + (change.Action == CollectionChangeAction::Replace
						? change.NewCount : 0));
			}
			break;
		case CollectionChangeAction::Move:
			if (change.OldIndex == CollectionChangedEventArgs::Npos
				|| change.NewIndex == CollectionChangedEventArgs::Npos
				|| change.OldCount == 0
				|| change.OldCount != change.NewCount
				|| change.OldIndex >= oldCount
				|| change.OldCount > oldCount - change.OldIndex)
			{
				append(0, oldCount, 0);
				break;
			}
			if (change.OldIndex < change.NewIndex)
			{
				append(0, change.OldIndex, 0);
				append(change.OldIndex,
					change.OldIndex + change.OldCount, change.NewIndex);
				append(change.OldIndex + change.OldCount,
					boundedEnd(change.NewIndex, change.OldCount, oldCount),
					change.OldIndex);
				append(boundedEnd(change.NewIndex, change.OldCount, oldCount),
					oldCount,
					boundedEnd(change.NewIndex, change.OldCount, oldCount));
			}
			else if (change.NewIndex < change.OldIndex)
			{
				append(0, change.NewIndex, 0);
				append(change.NewIndex, change.OldIndex,
					change.NewIndex + change.OldCount);
				append(change.OldIndex,
					change.OldIndex + change.OldCount, change.NewIndex);
				append(change.OldIndex + change.OldCount, oldCount,
					change.OldIndex + change.OldCount);
			}
			else append(0, oldCount, 0);
			break;
		default:
			append(0, oldCount, 0);
			break;
		}

		std::vector<SelectionExclusionInterval> result;
		result.reserve(previous.ExcludedIntervals().size()
			* (std::max)(size_t{ 1 }, segments.size()));
		for (const auto& interval : previous.ExcludedIntervals())
			for (const auto& segment : segments)
				AppendMappedExclusionIntersection(result, interval,
					segment.SourceStart, segment.SourceEnd,
					segment.DestinationStart);
		return result;
	}
}

int SelectedIndexCollection::const_iterator::operator*() const noexcept
{
	return _owner ? (*_owner)[_ordinal] : -1;
}

int SelectedIndexCollection::ExcludedIndexCollection::const_iterator::
operator*() const noexcept
{
	return _owner ? (*_owner)[_ordinal] : -1;
}

SelectedIndexCollection::ExcludedIndexCollection::const_iterator&
SelectedIndexCollection::ExcludedIndexCollection::const_iterator::operator+=(
	difference_type offset) noexcept
{
	if (offset >= 0) _ordinal += static_cast<size_t>(offset);
	else _ordinal -= static_cast<size_t>(-(offset + 1)) + size_t{ 1 };
	return *this;
}

int SelectedIndexCollection::ExcludedIndexCollection::operator[](
	size_t ordinal) const noexcept
{
	if (ordinal >= _count) return -1;
	const auto found = std::upper_bound(
		_prefixCounts.begin(), _prefixCounts.end(), ordinal);
	if (found == _prefixCounts.end()) return -1;
	const size_t interval = static_cast<size_t>(
		found - _prefixCounts.begin());
	const size_t before = interval == 0 ? 0 : _prefixCounts[interval - 1];
	const size_t value = _intervals[interval].Start + ordinal - before;
	return value <= static_cast<size_t>((std::numeric_limits<int>::max)())
		? static_cast<int>(value) : -1;
}

bool SelectedIndexCollection::ExcludedIndexCollection::Contains(
	int value) const noexcept
{
	if (value < 0) return false;
	const size_t raw = static_cast<size_t>(value);
	const auto found = std::upper_bound(_intervals.begin(), _intervals.end(), raw,
		[](size_t index, const ExcludedInterval& interval)
		{ return index < interval.Start; });
	if (found == _intervals.begin()) return false;
	const auto& interval = *std::prev(found);
	return raw - interval.Start < interval.Count;
}

size_t SelectedIndexCollection::ExcludedIndexCollection::CountBefore(
	size_t exclusiveEnd) const noexcept
{
	if (exclusiveEnd == 0 || _intervals.empty()) return 0;
	size_t low = 0;
	size_t high = _intervals.size();
	while (low < high)
	{
		const size_t middle = low + (high - low) / 2;
		const auto& interval = _intervals[middle];
		if (interval.Start + interval.Count <= exclusiveEnd)
			low = middle + 1;
		else high = middle;
	}
	size_t result = low == 0 ? 0 : _prefixCounts[low - 1];
	if (low < _intervals.size() && _intervals[low].Start < exclusiveEnd)
		result += (std::min)(exclusiveEnd - _intervals[low].Start,
			_intervals[low].Count);
	return result;
}

size_t SelectedIndexCollection::ExcludedIndexCollection::CountInRange(
	size_t start, size_t count) const noexcept
{
	if (count == 0) return 0;
	const size_t end = count > (std::numeric_limits<size_t>::max)() - start
		? (std::numeric_limits<size_t>::max)() : start + count;
	return CountBefore(end) - CountBefore(start);
}

SelectedIndexCollection::ExcludedIndexCollection::operator
std::vector<int>() const
{
	std::vector<int> values;
	values.reserve(_count);
	for (const int value : *this) values.push_back(value);
	return values;
}

void SelectedIndexCollection::ExcludedIndexCollection::Clear() noexcept
{
	_intervals.clear();
	_prefixCounts.clear();
	_count = 0;
}

void SelectedIndexCollection::ExcludedIndexCollection::RebuildPrefixCounts()
{
	_prefixCounts.clear();
	_prefixCounts.reserve(_intervals.size());
	_count = 0;
	for (const auto& interval : _intervals)
	{
		_count += interval.Count;
		_prefixCounts.push_back(_count);
	}
}

void SelectedIndexCollection::ExcludedIndexCollection::SetRanges(
	std::vector<ExcludedInterval> ranges, size_t rangeCount)
{
	for (auto& range : ranges)
	{
		if (range.Start >= rangeCount) range.Count = 0;
		else range.Count = (std::min)(range.Count,
			rangeCount - range.Start);
	}
	ranges.erase(std::remove_if(ranges.begin(), ranges.end(),
		[](const ExcludedInterval& range) { return range.Count == 0; }),
		ranges.end());
	std::sort(ranges.begin(), ranges.end(),
		[](const ExcludedInterval& left, const ExcludedInterval& right)
		{
			return left.Start != right.Start
				? left.Start < right.Start : left.Count < right.Count;
		});
	_intervals.clear();
	_intervals.reserve(ranges.size());
	for (const auto& range : ranges)
	{
		if (_intervals.empty())
		{
			_intervals.push_back(range);
			continue;
		}
		auto& previous = _intervals.back();
		const size_t previousEnd = previous.Start + previous.Count;
		const size_t rangeEnd = range.Start + range.Count;
		if (range.Start <= previousEnd)
			previous.Count = (std::max)(previousEnd, rangeEnd)
				- previous.Start;
		else _intervals.push_back(range);
	}
	RebuildPrefixCounts();
}

void SelectedIndexCollection::ExcludedIndexCollection::AddRange(
	size_t start, size_t count, size_t rangeCount)
{
	if (count == 0 || start >= rangeCount) return;
	auto ranges = _intervals;
	ranges.push_back({ start, (std::min)(count, rangeCount - start) });
	SetRanges(std::move(ranges), rangeCount);
}

bool SelectedIndexCollection::ExcludedIndexCollection::Toggle(size_t value)
{
	const auto found = std::lower_bound(_intervals.begin(), _intervals.end(), value,
		[](const ExcludedInterval& interval, size_t index)
		{ return interval.Start + interval.Count <= index; });
	const size_t position = static_cast<size_t>(found - _intervals.begin());
	if (found != _intervals.end() && value >= found->Start
		&& value - found->Start < found->Count)
	{
		const size_t intervalEnd = found->Start + found->Count;
		if (found->Count == 1)
			_intervals.erase(found);
		else if (value == found->Start)
		{
			++_intervals[position].Start;
			--_intervals[position].Count;
		}
		else if (value + 1 == intervalEnd)
			--_intervals[position].Count;
		else
		{
			const ExcludedInterval tail{
				value + 1, intervalEnd - value - 1 };
			_intervals[position].Count = value - found->Start;
			_intervals.insert(_intervals.begin() + position + 1, tail);
		}
		RebuildPrefixCounts();
		return false;
	}
	_intervals.insert(_intervals.begin() + position, { value, 1 });
	// A point may bridge the intervals on either side.
	SetRanges(std::move(_intervals),
		static_cast<size_t>((std::numeric_limits<int>::max)()) + size_t{ 1 });
	return true;
}

SelectedIndexCollection::const_iterator&
SelectedIndexCollection::const_iterator::operator++() noexcept
{
	++_ordinal;
	return *this;
}

int SelectedIndexCollection::operator[](size_t ordinal) const noexcept
{
	if (!_fullRange)
		return ordinal < _values.size() ? _values[ordinal] : -1;
	if (ordinal >= size()) return -1;
	size_t low = ordinal;
	const size_t maximumShift = (std::min)(
		_excluded.size(), _rangeCount - 1 - ordinal);
	size_t high = ordinal + maximumShift;
	while (low < high)
	{
		const size_t middle = low + (high - low) / 2;
		const size_t excludedThrough = _excluded.CountBefore(middle + 1);
		const size_t selectedThrough = middle + 1 - excludedThrough;
		if (selectedThrough > ordinal) high = middle;
		else low = middle + 1;
	}
	return low < _rangeCount
		&& low <= static_cast<size_t>((std::numeric_limits<int>::max)())
		? static_cast<int>(low) : -1;
}

bool SelectedIndexCollection::Contains(int value) const noexcept
{
	if (value < 0) return false;
	if (!_fullRange)
		return std::binary_search(_values.begin(), _values.end(), value);
	return static_cast<size_t>(value) < _rangeCount
		&& !_excluded.Contains(value);
}

void SelectedIndexCollection::SetDense(std::vector<int> values)
{
	_values = std::move(values);
	_excluded.Clear();
	_rangeCount = 0;
	_fullRange = false;
}

void SelectedIndexCollection::SetFullRange(size_t count)
{
	_values.clear();
	_excluded.Clear();
	_rangeCount = (std::min)(count,
		static_cast<size_t>((std::numeric_limits<int>::max)()) + size_t{ 1 });
	_fullRange = true;
}

void SelectedIndexCollection::SetExcludedRanges(
	std::vector<ExcludedInterval> ranges)
{
	if (!_fullRange) return;
	_excluded.SetRanges(std::move(ranges), _rangeCount);
}

void SelectedIndexCollection::ExcludeRange(size_t start, size_t count)
{
	if (!_fullRange) return;
	_excluded.AddRange(start, count, _rangeCount);
}

void SelectedIndexCollection::Clear() noexcept
{
	_values.clear();
	_excluded.Clear();
	_rangeCount = 0;
	_fullRange = false;
}

bool SelectedIndexCollection::Toggle(int value)
{
	if (value < 0) return false;
	if (!_fullRange)
	{
		auto found = std::lower_bound(_values.begin(), _values.end(), value);
		if (found != _values.end() && *found == value)
		{
			_values.erase(found);
			return false;
		}
		_values.insert(found, value);
		return true;
	}
	if (static_cast<size_t>(value) >= _rangeCount) return false;
	return !_excluded.Toggle(static_cast<size_t>(value));
}

bool operator==(
	const SelectedIndexCollection& left,
	const SelectedIndexCollection& right) noexcept
{
	if (left.size() != right.size()) return false;
	if (left._fullRange == right._fullRange)
		return left._fullRange
			? left._rangeCount == right._rangeCount
				&& left._excluded == right._excluded
			: left._values == right._values;
	return std::equal(left.begin(), left.end(), right.begin(), right.end());
}

bool operator==(
	const SelectedIndexCollection& left,
	const std::vector<int>& right) noexcept
{
	return left.size() == right.size()
		&& std::equal(left.begin(), left.end(), right.begin(), right.end());
}

const DependencyProperty& ListBox::SelectionModeProperty()
{
	static const auto registration = []
	{
		DependencyPropertyOptions<ListBox, int> options;
		options.DefaultValue = static_cast<int>(SelectionMode::Single);
		options.Flags = DependencyPropertyFlags::AffectsRender;
		options.Validate = [](const int& value)
		{
			return value >= static_cast<int>(SelectionMode::Single)
				&& value <= static_cast<int>(SelectionMode::Extended);
		};
		options.Changed = [](
			ListBox& target, const int& oldValue, const int& newValue)
		{
			target.ApplySelectionModeChange(
				static_cast<SelectionMode>(oldValue),
				static_cast<SelectionMode>(newValue));
		};
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Behavior";
		options.Design.CategoryOrder = 110;
		options.Design.Order = 10;
		options.Design.Editor = DependencyPropertyEditorKind::Choice;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		options.Design.Choices = {
			{ L"Single", BindingValue(static_cast<int>(SelectionMode::Single)) },
			{ L"Multiple", BindingValue(static_cast<int>(SelectionMode::Multiple)) },
			{ L"Extended", BindingValue(static_cast<int>(SelectionMode::Extended)) }
		};
		)
		return DependencyPropertyRegistry::RegisterStatic<ListBox, int>(
			DependencyPropertyRegistrationLiteral(L"SelectionMode"),
			[](ListBox& target)
			{ return static_cast<int>(target._selectionMode); },
			[](ListBox& target, const int& value)
			{ target._selectionMode = static_cast<SelectionMode>(value); },
			{}, std::move(options));
	}();
	return *registration;
}

ListBox::ListBox()
	: Selector()
{
#if CUI_ENABLE_DYNAMIC_XAML
	EnsureBindingPropertiesRegistered();
#endif
}

void ListBox::RegisterDependencyProperties()
{
	Selector::RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	(void)SelectionModeProperty();
#endif
}

void ListBox::SetSelectionMode(SelectionMode value)
{
	(void)TrySetPropertyValue(
		SelectionModeProperty(),
		BindingValue(static_cast<int>(value)));
}

std::vector<BindingValue> ListBox::GetSelectedItems() const
{
	const ControlWeakReference ownerLifetime(
		const_cast<ListBox*>(this));
	// A custom stable snapshot may invoke application code from TryGetItem.
	// Retry a bounded number of times when that code changes selection/source;
	// callers receive one coherent generation or an empty result, never a
	// prefix from the old selection followed by values from the new one.
	for (int attempt = 0; attempt < 3; ++attempt)
	{
		auto* live = dynamic_cast<ListBox*>(ownerLifetime.Get());
		if (!live) return {};
		const auto indices = live->_selectedIndices;
		const size_t selectionRevision = live->_selectionRevision;
		const auto source = live->GetItemsView();
		BindingListReference snapshot;
		if (source && !live->TryGetStableItemsSnapshot(snapshot)) return {};
		live = dynamic_cast<ListBox*>(ownerLifetime.Get());
		if (!live) return {};
		if (live->_selectionRevision != selectionRevision
			|| live->GetItemsView().Shared() != source.Shared())
			continue;

		std::vector<BindingValue> result;
		result.reserve(indices.size());
		bool invalidated = false;
		for (const int index : indices)
		{
			if (index < 0)
			{
				invalidated = true;
				break;
			}
			BindingValue value;
			if (source)
			{
				if (!snapshot
					|| static_cast<size_t>(index) >= snapshot.Get()->Count())
				{
					invalidated = true;
					break;
				}
				BindingSourceReference item;
				if (snapshot.Get()->TryGetItem(
					static_cast<size_t>(index), item) && item)
					value = BindingValue(std::move(item));
			}
			else
			{
				auto* item = live->GetAuthoredItem(
					static_cast<size_t>(index));
				if (item) value = BindingValue(item);
			}

			live = dynamic_cast<ListBox*>(ownerLifetime.Get());
			if (!live) return {};
			if (live->_selectionRevision != selectionRevision
				|| live->GetItemsView().Shared() != source.Shared())
			{
				invalidated = true;
				break;
			}
			if (!value.Empty()) result.push_back(std::move(value));
		}
		if (!invalidated) return result;
	}
	return {};
}

void ListBox::SelectAll()
{
	if (_selectionMode == SelectionMode::Single) return;
	SelectedIndexCollection selected;
	selected.SetFullRange(SelectionItemCount());
	(void)ApplySelection(
		std::move(selected),
		GetSelectedIndex() >= 0 ? GetSelectedIndex() : 0,
		GetSelectedIndex());
}

void ListBox::UnselectAll()
{
	(void)ApplySelection(std::vector<int>{}, -1, GetSelectedIndex());
}

bool ListBox::SelectIndex(int value)
{
	const int normalized = ClampIndex(value);
	if (normalized < 0)
	{
		if (_selectedIndices.empty() && GetSelectedIndex() < 0)
			return false;
		return ApplySelection(
			std::vector<int>{}, -1, GetSelectedIndex());
	}
	if (_selectionMode == SelectionMode::Single)
		return ApplySelection(
			std::vector<int>{ normalized }, normalized, normalized);

	auto selected = _selectedIndices;
	if (!ContainsIndex(selected, normalized)) (void)selected.Toggle(normalized);
	return ApplySelection(std::move(selected), normalized, normalized);
}

bool ListBox::IsIndexSelected(size_t index) const noexcept
{
	if (index > static_cast<size_t>((std::numeric_limits<int>::max)()))
		return false;
	return ContainsIndex(_selectedIndices, static_cast<int>(index));
}

void ListBox::ApplySelectionModeChange(
	SelectionMode,
	SelectionMode newValue)
{
	if (newValue == SelectionMode::Single && _selectedIndices.size() > 1)
	{
		const int primary = GetSelectedIndex() >= 0
			? GetSelectedIndex() : _selectedIndices.front();
		(void)ApplySelection(
			std::vector<int>{ primary }, primary, primary);
	}
}

bool ListBox::ApplySelection(
	std::vector<int> indices,
	int preferredPrimary,
	int actionIndex)
{
	const ControlWeakReference ownerLifetime(this);
	auto* live = dynamic_cast<ListBox*>(ownerLifetime.Get());
	if (!live) return true;
	const BindingListReference operationSource = live->GetItemsView();
	const size_t operationRevision = live->_selectionRevision;
	const int count = static_cast<int>(live->SelectionItemCount());
	live = dynamic_cast<ListBox*>(ownerLifetime.Get());
	if (!live || live->GetItemsView().Shared() != operationSource.Shared()
		|| live->_selectionRevision != operationRevision) return true;
	indices.erase(std::remove_if(
		indices.begin(), indices.end(),
		[count](int value) { return value < 0 || value >= count; }),
		indices.end());
	std::sort(indices.begin(), indices.end());
	indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
	if (live->_selectionMode == SelectionMode::Single && indices.size() > 1)
		indices = { preferredPrimary >= 0
			? preferredPrimary : indices.back() };
	return live->ApplySelection(SelectedIndexCollection(std::move(indices)),
		preferredPrimary, actionIndex);
}

bool ListBox::ApplySelection(
	SelectedIndexCollection indices,
	int preferredPrimary,
	int actionIndex)
{
	const ControlWeakReference ownerLifetime(this);
	auto* liveOwner = dynamic_cast<ListBox*>(ownerLifetime.Get());
	if (!liveOwner) return true;
	const BindingListReference operationSource = liveOwner->GetItemsView();
	const size_t operationRevision = liveOwner->_selectionRevision;
	const auto currentOperationOwner = [&]() -> ListBox*
	{
		auto* live = dynamic_cast<ListBox*>(ownerLifetime.Get());
		return live
			&& live->GetItemsView().Shared() == operationSource.Shared()
			&& live->_selectionRevision == operationRevision
			? live : nullptr;
	};
	const size_t count = liveOwner->SelectionItemCount();
	liveOwner = currentOperationOwner();
	if (!liveOwner) return true;
	if (indices.IsFullRange() && indices.RangeCount() != count)
		indices.SetFullRange(count);
	if (liveOwner->_selectionMode == SelectionMode::Single
		&& indices.size() > 1)
	{
		const int retained = preferredPrimary >= 0
			&& indices.Contains(preferredPrimary)
			? preferredPrimary : indices.back();
		indices.SetDense(retained >= 0
			? std::vector<int>{ retained } : std::vector<int>{});
	}
	BindingListReference nextRangeSnapshot;
	if (indices.IsRangeBacked()
		&& !liveOwner->TryGetStableItemsSnapshot(nextRangeSnapshot))
	{
		// A logical range must be anchored to immutable item occurrences. Legacy
		// mutable sources retain the established dense behavior.
		indices.SetDense(std::vector<int>(indices.begin(), indices.end()));
	}
	liveOwner = currentOperationOwner();
	if (!liveOwner) return true;

	int primary = -1;
	if (preferredPrimary >= 0 && ContainsIndex(indices, preferredPrimary))
		primary = preferredPrimary;
	else if (!indices.empty())
		primary = indices.back();

	const auto previousIndices = liveOwner->_selectedIndices;
	const auto previousRangeSnapshot = liveOwner->_selectedFullRangeSnapshot;
	const int previousPrimary = liveOwner->GetSelectedIndex();
	if (previousIndices == indices && previousPrimary == primary)
	{
		if (actionIndex >= 0) liveOwner->FocusIndex(actionIndex);
		return false;
	}

	SelectionChangedItemCollection removed;
	SelectionChangedItemCollection added;
	bool operationInvalidated = false;
	const auto itemAt = [&](const BindingListReference& snapshot,
		int index) -> BindingValue
	{
		if (index < 0 || operationInvalidated) return {};
		if (!snapshot)
		{
			auto* live = currentOperationOwner();
			if (!live)
			{
				operationInvalidated = true;
				return {};
			}
			auto result = live->GetSelectionItemAt(
				static_cast<size_t>(index));
			operationInvalidated = !currentOperationOwner();
			return result;
		}
		BindingSourceReference item;
		auto result = static_cast<size_t>(index) < snapshot.Get()->Count()
			&& snapshot.Get()->TryGetItem(static_cast<size_t>(index), item)
			&& item ? BindingValue(std::move(item)) : BindingValue{};
		operationInvalidated = !currentOperationOwner();
		return result;
	};
	if (!previousIndices.IsRangeBacked() && !indices.IsRangeBacked())
	{
		std::vector<BindingValue> removedValues;
		std::vector<BindingValue> addedValues;
		for (const int index : previousIndices)
			if (!indices.Contains(index))
				removedValues.push_back(itemAt({}, index));
		for (const int index : indices)
			if (!previousIndices.Contains(index))
				addedValues.push_back(itemAt(nextRangeSnapshot, index));
		removed = SelectionChangedItemCollection(std::move(removedValues));
		added = SelectionChangedItemCollection(std::move(addedValues));
	}
	else if (!previousIndices.IsRangeBacked())
	{
		std::vector<int> exclusions = indices.ExcludedIndices();
		std::vector<BindingValue> removedValues;
		for (const int index : previousIndices)
		{
			if (indices.Contains(index)) exclusions.push_back(index);
			else removedValues.push_back(itemAt({}, index));
		}
		removed = SelectionChangedItemCollection(std::move(removedValues));
		added = SelectionChangedItemCollection::FromSnapshotRange(
			nextRangeSnapshot.Shared(), indices.RangeCount(),
			std::move(exclusions));
	}
	else if (!indices.IsRangeBacked())
	{
		std::vector<int> exclusions = previousIndices.ExcludedIndices();
		std::vector<BindingValue> addedValues;
		for (const int index : indices)
		{
			if (previousIndices.Contains(index)) exclusions.push_back(index);
			else addedValues.push_back(itemAt({}, index));
		}
		removed = SelectionChangedItemCollection::FromSnapshotRange(
			previousRangeSnapshot.Shared(), previousIndices.RangeCount(),
			std::move(exclusions));
		added = SelectionChangedItemCollection(std::move(addedValues));
	}
	else
	{
		std::vector<BindingValue> removedValues;
		std::vector<BindingValue> addedValues;
		const size_t common = (std::min)(
			previousIndices.RangeCount(), indices.RangeCount());
		for (const int index : previousIndices.ExcludedIndices())
			if (index >= 0 && static_cast<size_t>(index) < common
				&& indices.Contains(index))
				addedValues.push_back(itemAt(nextRangeSnapshot, index));
		for (const int index : indices.ExcludedIndices())
			if (index >= 0 && static_cast<size_t>(index) < common
				&& previousIndices.Contains(index))
				removedValues.push_back(
					itemAt(previousRangeSnapshot, index));
		std::vector<int> removedTailExclusions;
		for (const int index : previousIndices.ExcludedIndices())
			if (index >= 0 && static_cast<size_t>(index) >= common)
				removedTailExclusions.push_back(
					static_cast<int>(static_cast<size_t>(index) - common));
		std::vector<int> addedTailExclusions;
		for (const int index : indices.ExcludedIndices())
			if (index >= 0 && static_cast<size_t>(index) >= common)
				addedTailExclusions.push_back(
					static_cast<int>(static_cast<size_t>(index) - common));
		removed = SelectionChangedItemCollection::FromValuesAndSnapshotSlice(
			std::move(removedValues), previousRangeSnapshot.Shared(),
			common, previousIndices.RangeCount() - common,
			std::move(removedTailExclusions));
		added = SelectionChangedItemCollection::FromValuesAndSnapshotSlice(
			std::move(addedValues), nextRangeSnapshot.Shared(),
			common, indices.RangeCount() - common,
			std::move(addedTailExclusions));
	}
	if (operationInvalidated) return true;
	liveOwner = currentOperationOwner();
	if (!liveOwner) return true;
	// Capture every ListBox/Selector projection before publishing the tentative
	// primary index. A source or DP callback may throw after SelectedIndex has
	// changed; restoring only the index collection would leave SelectedItem and
	// its observation in a different generation.
	auto rollbackState =
		liveOwner->ListBox::CaptureItemsSourceTransactionState();

	{
		liveOwner->_applyingSelection = true;
		liveOwner->_selectedIndices = std::move(indices);
		liveOwner->AdvanceSelectionRevision();
		const size_t tentativeRevision = liveOwner->_selectionRevision;
		ListBoxScopeExit restoreApplying([ownerLifetime]
		{
			if (auto* live = dynamic_cast<ListBox*>(ownerLifetime.Get()))
				live->_applyingSelection = false;
		});
		try
		{
			liveOwner->SetCurrentSelectedIndexWithoutSelectionChanged(primary);
			auto* live = dynamic_cast<ListBox*>(ownerLifetime.Get());
			if (!live) return true;
			if (live->GetItemsView().Shared() != operationSource.Shared()
				|| live->_selectionRevision != tentativeRevision)
				return true;
			live->RefreshSelectedItemState(false);
			live = dynamic_cast<ListBox*>(ownerLifetime.Get());
			if (!live) return true;
			if (live->GetItemsView().Shared() != operationSource.Shared()
				|| live->_selectionRevision != tentativeRevision)
				return true;
		}
		catch (...)
		{
			if (auto* live = dynamic_cast<ListBox*>(ownerLifetime.Get());
				live && rollbackState)
				live->ListBox::RestoreItemsSourceTransactionState(
					*rollbackState);
			throw;
		}
	}
	auto* live = dynamic_cast<ListBox*>(ownerLifetime.Get());
	if (!live) return true;
	if (live->_selectedIndices.IsRangeBacked())
	{
		if (!live->TryCaptureFullRangeSnapshot())
		{
			live = dynamic_cast<ListBox*>(ownerLifetime.Get());
			if (!live) return true;
			// Lazy range selection is valid only while a stable record snapshot can
			// anchor the selected occurrence domain. Preserve correctness for legacy
			// mutable lists by falling back to the established dense representation.
			std::vector<int> dense(
				live->_selectedIndices.begin(), live->_selectedIndices.end());
			live->_selectedIndices.SetDense(std::move(dense));
			live->AdvanceSelectionRevision();
			live->CaptureSelectionIdentities();
		}
	}
	else live->CaptureSelectionIdentities();
	live = dynamic_cast<ListBox*>(ownerLifetime.Get());
	if (!live) return true;
	live->RaiseSelectionChanged(
		previousPrimary, primary, std::move(removed), std::move(added));
	live = dynamic_cast<ListBox*>(ownerLifetime.Get());
	if (!live) return true;
	if (actionIndex >= 0) live->FocusIndex(actionIndex);
	return true;
}

void ListBox::SelectOnly(int index)
{
	const ControlWeakReference ownerLifetime(this);
	(void)ApplySelection(
		index < 0 ? std::vector<int>{} : std::vector<int>{ index },
		index, index);
	auto* live = dynamic_cast<ListBox*>(ownerLifetime.Get());
	if (!live) return;
	live->UpdateAnchor(index);
	if (live->_selectedIndices.IsRangeBacked())
		(void)live->TryCaptureFullRangeSnapshot();
	else live->CaptureSelectionIdentities();
}

void ListBox::ToggleIndex(int index)
{
	const ControlWeakReference ownerLifetime(this);
	auto selected = _selectedIndices;
	(void)selected.Toggle(index);
	(void)ApplySelection(
		std::move(selected),
		IsIndexSelected(static_cast<size_t>(index))
			? GetSelectedIndex() : index,
		index);
	auto* live = dynamic_cast<ListBox*>(ownerLifetime.Get());
	if (!live) return;
	live->UpdateAnchor(index);
	if (live->_selectedIndices.IsRangeBacked())
		(void)live->TryCaptureFullRangeSnapshot();
	else live->CaptureSelectionIdentities();
}

void ListBox::SelectRange(int index, bool clearCurrent)
{
	if (_anchorIndex < 0) _anchorIndex =
		GetSelectedIndex() >= 0 ? GetSelectedIndex() : index;
	const int first = (std::min)(_anchorIndex, index);
	const int last = (std::max)(_anchorIndex, index);
	std::vector<int> selected = clearCurrent
		? std::vector<int>{} : std::vector<int>(
			_selectedIndices.begin(), _selectedIndices.end());
	for (int candidate = first; candidate <= last; ++candidate)
		selected.push_back(candidate);
	(void)ApplySelection(std::move(selected), index, index);
}

void ListBox::MakeKeyboardSelection(
	int index,
	ModifierKeys modifiers)
{
	const bool control =
		HasModifier(modifiers, ModifierKeys::Control);
	const bool shift =
		HasModifier(modifiers, ModifierKeys::Shift);
	if (_selectionMode == SelectionMode::Extended && shift)
		SelectRange(index, !control);
	else if (_selectionMode == SelectionMode::Extended && control)
		FocusIndex(index);
	else
		SelectOnly(index);
}

void ListBox::NotifyItemClicked(
	size_t index,
	MouseButton button,
	ModifierKeys modifiers)
{
	if (index >= SelectionItemCount()) return;
	const int itemIndex = static_cast<int>(index);
	if (button == MouseButton::Right)
	{
		if (!IsIndexSelected(index)) SelectOnly(itemIndex);
		return;
	}
	if (button != MouseButton::Left && button != MouseButton::None) return;

	if (_selectionMode == SelectionMode::Single)
		SelectOnly(itemIndex);
	else if (_selectionMode == SelectionMode::Multiple)
		ToggleIndex(itemIndex);
	else if (HasModifier(modifiers, ModifierKeys::Shift))
		SelectRange(
			itemIndex,
			!HasModifier(modifiers, ModifierKeys::Control));
	else if (HasModifier(modifiers, ModifierKeys::Control))
		ToggleIndex(itemIndex);
	else
		SelectOnly(itemIndex);
}

void ListBox::RequestItemSelection(size_t index, bool selected)
{
	if (index >= SelectionItemCount()) return;
	const int itemIndex = static_cast<int>(index);
	if (selected)
	{
		if (_selectionMode == SelectionMode::Single)
			SelectOnly(itemIndex);
		else
			(void)SelectIndex(itemIndex);
	}
	else if (IsIndexSelected(index))
	{
		if (_selectionMode == SelectionMode::Single)
			UnselectAll();
		else
			ToggleIndex(itemIndex);
	}
}

bool ListBox::ProcessItemKey(
	size_t itemIndex,
	const InputReport& input)
{
	if (itemIndex >= SelectionItemCount()
		|| input.Kind != InputReportKind::KeyDown) return false;
	if (input.Key == Key::Space || input.Key == Key::Return)
	{
		const int index = static_cast<int>(itemIndex);
		if (_selectionMode == SelectionMode::Multiple
			|| (_selectionMode == SelectionMode::Extended
				&& input.HasModifier(ModifierKeys::Control)))
			ToggleIndex(index);
		else if (_selectionMode == SelectionMode::Extended
			&& input.HasModifier(ModifierKeys::Shift))
			SelectRange(index, true);
		else
			SelectOnly(index);
		return true;
	}
	return HandleSelectionKey(
		static_cast<int>(itemIndex), input, true);
}

bool ListBox::HandleSelectionKey(
	int itemIndex,
	const InputReport& input,
	bool)
{
	if (input.Kind != InputReportKind::KeyDown
		|| SelectionItemCount() == 0) return false;
	const int count = static_cast<int>(SelectionItemCount());
	int next = itemIndex >= 0 ? itemIndex : GetSelectedIndex();
	if (next < 0) next = 0;
	switch (input.Key)
	{
	case Key::Up: --next; break;
	case Key::Down: ++next; break;
	case Key::Home: next = 0; break;
	case Key::End: next = count - 1; break;
	case Key::PageUp: next -= 5; break;
	case Key::PageDown: next += 5; break;
	default:
		if (input.Key == Key::A
			&& input.HasModifier(ModifierKeys::Control)
			&& _selectionMode != SelectionMode::Single)
		{
			SelectAll();
			return true;
		}
		return false;
	}
	next = (std::clamp)(next, 0, count - 1);
	MakeKeyboardSelection(next, input.Modifiers);
	return true;
}

bool ListBox::HandlesNavigationKey(Key key) const
{
	return key == Key::Space || key == Key::Return
		|| key == Key::A
		|| Selector::HandlesNavigationKey(key);
}

bool ListBox::ProcessInput(const InputReport& input)
{
	if (HandleSelectionKey(
		_focusedIndex >= 0 ? _focusedIndex : GetSelectedIndex(),
		input, false))
	{
		auto args = input.CreateKeyEventArgs();
		OnKeyDown(this, args);
		return true;
	}
	return Selector::ProcessInput(input);
}

std::unique_ptr<ItemsControl::ItemsSourceTransactionState>
ListBox::CaptureItemsSourceTransactionState()
{
	auto state = std::make_unique<ListBoxItemsSourceTransactionState>();
	state->SelectedIndices = _selectedIndices;
	state->FullRangeSnapshot = _selectedFullRangeSnapshot;
	state->PendingRemovedItems = _pendingSelectionRemovedItems;
	state->PendingAddedItems = _pendingSelectionAddedItems;
	state->SelectedSourceIdentities = _selectedSourceIdentities;
	state->SelectedAuthoredIdentities = _selectedAuthoredIdentities;
	state->PrimarySourceIdentity = _primarySourceIdentity;
	state->AnchorSourceIdentity = _anchorSourceIdentity;
	state->PrimaryAuthoredIdentity = _primaryAuthoredIdentity;
	state->AnchorAuthoredIdentity = _anchorAuthoredIdentity;
	state->PendingOldPrimary = _pendingSelectionOldPrimary;
	state->PendingSelectionChange = _pendingSelectionChange;
	state->SkipSelectionIdentityRestoreOnce =
		_skipSelectionIdentityRestoreOnce;
	state->ApplyingSelection = _applyingSelection;
	state->AnchorIndex = _anchorIndex;
	state->FocusedIndex = _focusedIndex;
	state->SelectionRevision = _selectionRevision;
	state->SelectorState = Selector::CaptureItemsSourceTransactionState();
	return state;
}

void ListBox::RestoreItemsSourceTransactionState(
	ItemsSourceTransactionState& state) noexcept
{
	auto* list = dynamic_cast<ListBoxItemsSourceTransactionState*>(&state);
	if (!list) return;
	const ControlWeakReference ownerLifetime(this);
	if (list->SelectorState)
		Selector::RestoreItemsSourceTransactionState(*list->SelectorState);
	auto* live = dynamic_cast<ListBox*>(ownerLifetime.Get());
	if (!live) return;
	const size_t currentRevision = live->_selectionRevision;
	live->_selectedIndices = std::move(list->SelectedIndices);
	live->_selectedFullRangeSnapshot = list->FullRangeSnapshot;
	live->_pendingSelectionRemovedItems = std::move(list->PendingRemovedItems);
	live->_pendingSelectionAddedItems = std::move(list->PendingAddedItems);
	live->_selectedSourceIdentities = std::move(list->SelectedSourceIdentities);
	live->_selectedAuthoredIdentities = std::move(list->SelectedAuthoredIdentities);
	live->_primarySourceIdentity = std::move(list->PrimarySourceIdentity);
	live->_anchorSourceIdentity = std::move(list->AnchorSourceIdentity);
	live->_primaryAuthoredIdentity = list->PrimaryAuthoredIdentity;
	live->_anchorAuthoredIdentity = list->AnchorAuthoredIdentity;
	live->_pendingSelectionOldPrimary = list->PendingOldPrimary;
	live->_pendingSelectionChange = list->PendingSelectionChange;
	live->_skipSelectionIdentityRestoreOnce =
		list->SkipSelectionIdentityRestoreOnce;
	live->_applyingSelection = list->ApplyingSelection;
	live->_anchorIndex = list->AnchorIndex;
	live->_focusedIndex = list->FocusedIndex;
	if (currentRevision == list->SelectionRevision)
		live->_selectionRevision = list->SelectionRevision;
	else
	{
		// A revision is an observation clock, not rollback payload. Readers may
		// have seen the rejected tentative state, so restoration must publish a
		// fresh boundary instead of moving the clock backwards.
		live->_selectionRevision = currentRevision;
		live->AdvanceSelectionRevision();
	}
	try { live->UpdateContainerSelection(); }
	catch (...) {}
}

bool ListBox::TryResolveSelectionOccurrencePermutation(
	const BindingListReference& previousSnapshot,
	std::vector<size_t>& oldToNew) const noexcept
{
	oldToNew.clear();
	try
	{
		const auto current = GetItemsView();
		if (!previousSnapshot || !current
			|| previousSnapshot.Get()->Count() != current.Get()->Count())
			return false;
		const auto* oldOccurrences = dynamic_cast<
			const IBindingListOccurrenceIdentity*>(previousSnapshot.Get());
		const auto* newOccurrences = dynamic_cast<
			const IBindingListOccurrenceIdentity*>(current.Get());
		const auto* lookup = dynamic_cast<
			const IBindingListOccurrenceLookup*>(current.Get());
		if (!oldOccurrences || !newOccurrences) return false;
		const size_t count = current.Get()->Count();
		std::unordered_map<size_t, size_t> indicesByOccurrence;
		if (!lookup || !lookup->IsItemIndexByOccurrenceIdentityLookupBounded())
		{
			indicesByOccurrence.reserve(count);
			for (size_t index = 0; index < count; ++index)
			{
				size_t occurrence = 0;
				if (!newOccurrences->TryGetItemOccurrenceIdentity(
					index, occurrence)
					|| !indicesByOccurrence.emplace(occurrence, index).second)
					return false;
			}
		}
		oldToNew.resize(count);
		std::vector<uint8_t> seen(count, 0);
		for (size_t oldIndex = 0; oldIndex < count; ++oldIndex)
		{
			size_t occurrence = 0;
			if (!oldOccurrences->TryGetItemOccurrenceIdentity(
				oldIndex, occurrence)) return false;
			size_t newIndex = 0;
			if (lookup && lookup->IsItemIndexByOccurrenceIdentityLookupBounded())
			{
				if (!lookup->TryGetItemIndexByOccurrenceIdentity(
					occurrence, newIndex)) return false;
			}
			else
			{
				const auto found = indicesByOccurrence.find(occurrence);
				if (found == indicesByOccurrence.end()) return false;
				newIndex = found->second;
			}
			if (newIndex >= count || seen[newIndex]) return false;
			BindingSourceReference oldItem;
			BindingSourceReference newItem;
			size_t newOccurrence = 0;
			if (!previousSnapshot.Get()->TryGetItem(oldIndex, oldItem)
				|| !current.Get()->TryGetItem(newIndex, newItem)
				|| !oldItem || !newItem
				|| oldItem.Shared() != newItem.Shared()
				|| !newOccurrences->TryGetItemOccurrenceIdentity(
					newIndex, newOccurrence)
				|| newOccurrence != occurrence) return false;
			seen[newIndex] = 1;
			oldToNew[oldIndex] = newIndex;
		}
		return true;
	}
	catch (...)
	{
		oldToNew.clear();
		return false;
	}
}

bool ListBox::TryRestoreRangeSelectionAfterReset(
	const CollectionChangedEventArgs& change,
	const BindingListReference& previousSnapshot,
	const SelectedIndexCollection& previousIndices,
	int previousPrimary,
	int previousAnchor,
	int previousFocus,
	bool& aborted)
{
	aborted = false;
	if (!previousSnapshot
		|| previousSnapshot.Get()->Count() != change.OldSize
		|| previousIndices.RangeCount() != change.OldSize)
		return false;

	const ControlWeakReference ownerLifetime(this);
	const size_t selectionRevision = _selectionRevision;
	const auto currentSource = GetItemsView();
	BindingListReference currentSnapshot;
	if (!currentSource || !TryGetStableItemsSnapshot(currentSnapshot)
		|| !currentSnapshot
		|| currentSnapshot.Get()->Count() != change.NewSize)
		return false;
	const auto invalidated = [&]()
	{
		auto* live = dynamic_cast<ListBox*>(ownerLifetime.Get());
		if (!live)
		{
			aborted = true;
			return true;
		}
		if (live->_selectionRevision != selectionRevision
			|| live->GetItemsView().Shared() != currentSource.Shared())
		{
			aborted = true;
			return true;
		}
		return false;
	};
	if (invalidated()) return false;

	struct MatchState final
	{
		size_t SelectedCount = 0;
		size_t MatchedCount = 0;
		size_t NewRetainedVisited = 0;
		size_t OldRetainedVisited = 0;
	};
	struct ScalarMatch final
	{
		IBindingSource* Item = nullptr;
		size_t SelectedRank = 0;
		int NewIndex = -1;
	};
	std::unordered_map<IBindingSource*, MatchState> matches;
	// Repeated-object ranges are common (and are the cheapest Reset case). Do
	// not preallocate one hash bucket per selected row before distinctness is
	// known; the table grows naturally only when identities are actually unique.
	matches.reserve((std::min)(previousIndices.size(), size_t{ 4096 }));
	ScalarMatch primaryMatch;
	ScalarMatch anchorMatch;
	ScalarMatch focusMatch;
	const auto captureScalar = [](ScalarMatch& scalar,
		IBindingSource* item, size_t rank)
	{
		scalar.Item = item;
		scalar.SelectedRank = rank;
	};

	for (size_t oldIndex = 0; oldIndex < change.OldSize; ++oldIndex)
	{
		if (oldIndex > static_cast<size_t>(
			(std::numeric_limits<int>::max)())
			|| !previousIndices.Contains(static_cast<int>(oldIndex))) continue;
		BindingSourceReference item;
		if (!previousSnapshot.Get()->TryGetItem(oldIndex, item) || !item)
			return false;
		if (invalidated()) return false;
		auto& state = matches[item.Get()];
		const size_t selectedRank = state.SelectedCount++;
		if (static_cast<int>(oldIndex) == previousPrimary)
			captureScalar(primaryMatch, item.Get(), selectedRank);
		if (static_cast<int>(oldIndex) == previousAnchor)
			captureScalar(anchorMatch, item.Get(), selectedRank);
		if (static_cast<int>(oldIndex) == previousFocus)
			captureScalar(focusMatch, item.Get(), selectedRank);
	}

	size_t retainedCount = 0;
	for (size_t newIndex = 0; newIndex < change.NewSize; ++newIndex)
	{
		BindingSourceReference item;
		const bool populated = currentSnapshot.Get()->TryGetItem(
			newIndex, item) && item;
		if (invalidated()) return false;
		auto found = populated ? matches.find(item.Get()) : matches.end();
		if (found == matches.end()
			|| found->second.MatchedCount >= found->second.SelectedCount)
			continue;
		const size_t selectedRank = found->second.MatchedCount++;
		++retainedCount;
		const int selectedIndex = static_cast<int>(newIndex);
		const auto mapScalar = [&](ScalarMatch& scalar)
		{
			if (scalar.Item == item.Get()
				&& scalar.SelectedRank == selectedRank)
				scalar.NewIndex = selectedIndex;
		};
		mapScalar(primaryMatch);
		mapScalar(anchorMatch);
		mapScalar(focusMatch);
	}
	SelectedIndexCollection next;
	std::vector<int> denseRetained;
	const bool useDenseRetained = retainedCount != 0
		&& retainedCount <= change.NewSize - retainedCount;
	if (useDenseRetained) denseRetained.reserve(retainedCount);
	else if (retainedCount != 0)
		next.SetFullRange(change.NewSize);
	if (retainedCount != 0)
	{
		for (size_t newIndex = 0; newIndex < change.NewSize; ++newIndex)
		{
			BindingSourceReference item;
			const bool populated = currentSnapshot.Get()->TryGetItem(
				newIndex, item) && item;
			if (invalidated()) return false;
			auto found = populated ? matches.find(item.Get()) : matches.end();
			const bool selected = found != matches.end()
				&& found->second.NewRetainedVisited++
					< found->second.MatchedCount;
			if (useDenseRetained)
			{
				if (selected)
					denseRetained.push_back(static_cast<int>(newIndex));
			}
			else if (!selected)
				(void)next.Toggle(static_cast<int>(newIndex));
		}
		if (useDenseRetained) next.SetDense(std::move(denseRetained));
	}

	const size_t previousSelectedCount = previousIndices.size();
	const size_t missingCount = previousSelectedCount - retainedCount;
	SelectionChangedItemCollection removed;
	if (missingCount != 0)
	{
		const size_t denseExclusionCount =
			previousIndices.ExcludedIndices().size() + retainedCount;
		const bool useIncludedIndices = missingCount <= denseExclusionCount;
		std::vector<int> payloadIndices = useIncludedIndices
			? std::vector<int>{} : previousIndices.ExcludedIndices();
		payloadIndices.reserve(useIncludedIndices
			? missingCount : denseExclusionCount);
		for (size_t oldIndex = 0; oldIndex < change.OldSize; ++oldIndex)
		{
			if (oldIndex > static_cast<size_t>(
				(std::numeric_limits<int>::max)())
				|| !previousIndices.Contains(static_cast<int>(oldIndex))) continue;
			BindingSourceReference item;
			if (!previousSnapshot.Get()->TryGetItem(oldIndex, item) || !item)
				return false;
			if (invalidated()) return false;
			auto found = matches.find(item.Get());
			if (found == matches.end()) return false;
			const bool retained = found->second.OldRetainedVisited++
				< found->second.MatchedCount;
			if (useIncludedIndices ? !retained : retained)
				payloadIndices.push_back(static_cast<int>(oldIndex));
		}
		removed = useIncludedIndices
			? SelectionChangedItemCollection::FromSnapshotIndices(
				previousSnapshot.Shared(), std::move(payloadIndices))
			: SelectionChangedItemCollection::FromSnapshotRange(
				previousSnapshot.Shared(), previousIndices.RangeCount(),
				std::move(payloadIndices));
	}
	if (invalidated()) return false;

	auto* live = dynamic_cast<ListBox*>(ownerLifetime.Get());
	if (!live)
	{
		aborted = true;
		return false;
	}
	live->_pendingSelectionRemovedItems = std::move(removed);
	live->_pendingSelectionAddedItems = {};
	live->_pendingSelectionOldPrimary = previousPrimary;
	live->_pendingSelectionChange =
		!live->_pendingSelectionRemovedItems.empty();
	live->_selectedIndices = std::move(next);
	live->_selectedFullRangeSnapshot = live->_selectedIndices.IsRangeBacked()
		? currentSnapshot : BindingListReference{};
	live->_selectedSourceIdentities.clear();
	live->_primarySourceIdentity = {};
	live->_anchorSourceIdentity = {};
	// A sparse survivor set deliberately changes from range-backed to dense.
	// Its identities belong to the new Reset snapshot and will be captured by
	// OnGeneratedItemsRebuilt; do not run the ordinary pre-Reset identity restore
	// against the intentionally cleared old vectors.
	live->_skipSelectionIdentityRestoreOnce =
		!live->_selectedIndices.IsRangeBacked();
	live->AdvanceSelectionRevision();
	const int primary = primaryMatch.NewIndex >= 0
		? primaryMatch.NewIndex
		: (live->_selectedIndices.empty()
			? -1 : live->_selectedIndices.back());
	const int anchor = anchorMatch.NewIndex >= 0
		&& live->_selectedIndices.Contains(anchorMatch.NewIndex)
		? anchorMatch.NewIndex : primary;
	const int focus = focusMatch.NewIndex >= 0
		&& live->_selectedIndices.Contains(focusMatch.NewIndex)
		? focusMatch.NewIndex : primary;
	if (!live->UpdatePrimarySelectionState(primary, false))
	{
		aborted = true;
		return true;
	}
	live = dynamic_cast<ListBox*>(ownerLifetime.Get());
	if (!live)
	{
		aborted = true;
		return true;
	}
	live->_anchorIndex = anchor;
	live->_focusedIndex = focus;
	return true;
}

bool ListBox::UpdatePrimarySelectionState(
	int primary,
	bool refreshSelectedItemState,
	bool ensureSelectedItemVisible)
{
	const ControlWeakReference ownerLifetime(this);
	_applyingSelection = true;
	ListBoxScopeExit restoreApplying([ownerLifetime]
	{
		if (auto* live = dynamic_cast<ListBox*>(ownerLifetime.Get()))
			live->_applyingSelection = false;
	});
	SetCurrentSelectedIndexWithoutSelectionChanged(
		primary, ensureSelectedItemVisible);
	auto* live = dynamic_cast<ListBox*>(ownerLifetime.Get());
	if (!live) return false;
	if (refreshSelectedItemState)
		live->RefreshSelectedItemState(false);
	return static_cast<bool>(ownerLifetime.Get());
}

void ListBox::OnItemsSourceCollectionChangePreparing(
	const CollectionChangedEventArgs& change,
	const BindingListReference& previousSnapshot)
{
	const ControlWeakReference ownerLifetime(this);
	const bool validDensePermutation = [&]() noexcept
	{
		if (_selectedIndices.IsRangeBacked()
			|| change.OldSize != change.NewSize
			|| change.NewSize != SelectionItemCount()) return false;
		switch (change.Action)
		{
		case CollectionChangeAction::Move:
			return change.OldCount > 0
				&& change.OldCount == change.NewCount
				&& change.OldIndex != CollectionChangedEventArgs::Npos
				&& change.NewIndex != CollectionChangedEventArgs::Npos
				&& change.OldIndex <= change.OldSize
				&& change.OldCount <= change.OldSize - change.OldIndex
				&& change.NewCount <= change.NewSize
				&& change.NewIndex <= change.NewSize - change.NewCount;
		case CollectionChangeAction::Swap:
			return change.OldCount == 1 && change.NewCount == 1
				&& change.OldIndex < change.OldSize
				&& change.NewIndex < change.NewSize;
		default:
			return false;
		}
	}();
	if (validDensePermutation)
	{
		const auto mapIndex = [&](int oldIndex) -> std::optional<int>
		{
			if (oldIndex < 0) return -1;
			const size_t raw = static_cast<size_t>(oldIndex);
			if (raw >= change.OldSize) return std::nullopt;
			const auto mapped = ItemContainerGenerator::MapIndex(change, raw);
			if (!mapped || *mapped > static_cast<size_t>(
				(std::numeric_limits<int>::max)())) return std::nullopt;
			return static_cast<int>(*mapped);
		};
		std::vector<int> remapped;
		remapped.reserve(_selectedIndices.size());
		for (const int oldIndex : _selectedIndices)
		{
			const auto mapped = mapIndex(oldIndex);
			if (!mapped) return;
			remapped.push_back(*mapped);
		}
		const auto primary = mapIndex(GetSelectedIndex());
		const auto anchor = mapIndex(_anchorIndex);
		const auto focus = mapIndex(_focusedIndex);
		if (!primary || !anchor || !focus) return;
		std::sort(remapped.begin(), remapped.end());
		remapped.erase(
			std::unique(remapped.begin(), remapped.end()), remapped.end());
		_selectedIndices.SetDense(std::move(remapped));
		AdvanceSelectionRevision();
		_pendingSelectionRemovedItems = {};
		_pendingSelectionAddedItems = {};
		_pendingSelectionOldPrimary = *primary;
		_pendingSelectionChange = false;
		// ItemsControl has not captured the old viewport anchor yet. Updating the
		// primary occurrence here must not BringIntoView the new index; otherwise
		// the collection-change mapper applies to an already shifted offset.
		if (!UpdatePrimarySelectionState(*primary, false, false)) return;
		auto* live = dynamic_cast<ListBox*>(ownerLifetime.Get());
		if (!live) return;
		live->_anchorIndex = *anchor;
		live->_focusedIndex = *focus;
		live->_skipSelectionIdentityRestoreOnce = true;
		return;
	}
	if (!_selectedIndices.IsRangeBacked() || !previousSnapshot
		|| _selectedIndices.RangeCount() != change.OldSize) return;
	const auto previousIndices = _selectedIndices;
	const int previousPrimary = GetSelectedIndex();
	_pendingSelectionRemovedItems = {};
	_pendingSelectionAddedItems = {};
	_pendingSelectionOldPrimary = previousPrimary;
	_pendingSelectionChange = false;
	if (change.Action == CollectionChangeAction::Reset)
	{
		std::vector<size_t> oldToNew;
		if (TryResolveSelectionOccurrencePermutation(
			previousSnapshot, oldToNew))
		{
			SelectedIndexCollection next;
			next.SetFullRange(change.NewSize);
			for (const int excluded : previousIndices.ExcludedIndices())
				if (excluded >= 0
					&& static_cast<size_t>(excluded) < oldToNew.size())
					(void)next.Toggle(static_cast<int>(
						oldToNew[static_cast<size_t>(excluded)]));
			const auto remapScalar = [&](int index)
			{
				return index >= 0 && static_cast<size_t>(index) < oldToNew.size()
					? static_cast<int>(oldToNew[static_cast<size_t>(index)])
					: -1;
			};
			_selectedIndices = std::move(next);
			AdvanceSelectionRevision();
			const int primary = remapScalar(previousPrimary);
			const int anchor = remapScalar(_anchorIndex);
			const int focus = remapScalar(_focusedIndex);
			if (!UpdatePrimarySelectionState(primary, false)) return;
			auto* live = dynamic_cast<ListBox*>(ownerLifetime.Get());
			if (!live) return;
			live->_anchorIndex = anchor;
			live->_focusedIndex = focus;
			live->_selectedFullRangeSnapshot = {};
			return;
		}
		bool resetRestoreAborted = false;
		if (TryRestoreRangeSelectionAfterReset(
			change, previousSnapshot, previousIndices,
			previousPrimary, _anchorIndex, _focusedIndex,
			resetRestoreAborted)
			|| resetRestoreAborted)
			return;
		_pendingSelectionRemovedItems =
			SelectionChangedItemCollection::FromSnapshotRange(
				previousSnapshot.Shared(), previousIndices.RangeCount(),
				previousIndices.ExcludedIndices());
		_pendingSelectionChange =
			!_pendingSelectionRemovedItems.empty();
		_selectedIndices.Clear();
		AdvanceSelectionRevision();
		_selectedFullRangeSnapshot = {};
		_selectedSourceIdentities.clear();
		_primarySourceIdentity = {};
		_anchorSourceIdentity = {};
		if (!UpdatePrimarySelectionState(-1, false)) return;
		auto* live = dynamic_cast<ListBox*>(ownerLifetime.Get());
		if (!live) return;
		live->_anchorIndex = -1;
		live->_focusedIndex = -1;
		live->_skipSelectionIdentityRestoreOnce = true;
		return;
	}
	SelectionChangedItemCollection removedItems;
	if ((change.Action == CollectionChangeAction::Remove
		|| change.Action == CollectionChangeAction::Replace)
		&& change.OldIndex != CollectionChangedEventArgs::Npos
		&& change.OldIndex < previousSnapshot.Get()->Count())
	{
		const size_t removedCount = (std::min)(change.OldCount,
			previousSnapshot.Get()->Count() - change.OldIndex);
		const auto& exclusions = previousIndices.ExcludedIndices();
		const size_t excludedCount = exclusions.CountInRange(
			change.OldIndex, removedCount);
		// A fully excluded Replace range carries no RemovedItems. Avoid expanding
		// a million-slot interval merely to prove that its selected complement is
		// empty.
		if (excludedCount != removedCount)
		{
			std::vector<int> sliceExclusions;
			sliceExclusions.reserve(excludedCount);
			const size_t firstOrdinal = exclusions.CountBefore(change.OldIndex);
			for (size_t offset = 0; offset < excludedCount; ++offset)
			{
				const int excluded = exclusions[firstOrdinal + offset];
				if (excluded >= 0)
					sliceExclusions.push_back(static_cast<int>(
						static_cast<size_t>(excluded) - change.OldIndex));
			}
			removedItems = SelectionChangedItemCollection::
				FromValuesAndSnapshotSlice({}, previousSnapshot.Shared(),
					change.OldIndex, removedCount, std::move(sliceExclusions));
		}
	}
	_pendingSelectionRemovedItems = std::move(removedItems);
	_pendingSelectionChange = !_pendingSelectionRemovedItems.empty();

	SelectedIndexCollection next;
	next.SetFullRange(change.NewSize);
	// Preserve the old selected occurrence domain: newly allocated Add slots and
	// Replace new slots are excluded. Existing explicit exclusions shift with
	// the same index algebra as the collection operation. Mapping entire runs
	// keeps the work proportional to exclusion intervals, not affected rows.
	next.SetExcludedRanges(MapSelectionExclusionIntervals(
		previousIndices, change));
	const auto mapOldIndex = [&](int oldIndex) -> int
	{
		if (oldIndex < 0) return -1;
		const size_t raw = static_cast<size_t>(oldIndex);
		switch (change.Action)
		{
		case CollectionChangeAction::Add:
			return change.NewIndex != CollectionChangedEventArgs::Npos
				&& raw >= change.NewIndex
				? static_cast<int>(raw + change.NewCount) : oldIndex;
		case CollectionChangeAction::Remove:
			if (change.OldIndex == CollectionChangedEventArgs::Npos)
				return oldIndex;
			if (raw >= change.OldIndex
				&& raw < change.OldIndex + change.OldCount) return -1;
			return raw >= change.OldIndex + change.OldCount
				? static_cast<int>(raw - change.OldCount) : oldIndex;
		case CollectionChangeAction::Replace:
			if (change.OldIndex == CollectionChangedEventArgs::Npos)
				return oldIndex;
			if (raw >= change.OldIndex
				&& raw < change.OldIndex + change.OldCount) return -1;
			return raw >= change.OldIndex + change.OldCount
				? static_cast<int>(raw - change.OldCount + change.NewCount)
				: oldIndex;
		case CollectionChangeAction::Move:
			if (change.OldIndex == CollectionChangedEventArgs::Npos
				|| change.NewIndex == CollectionChangedEventArgs::Npos
				|| change.OldCount == 0
				|| change.OldCount != change.NewCount)
				return oldIndex;
			if (raw >= change.OldIndex
				&& raw < change.OldIndex + change.OldCount)
				return static_cast<int>(
					change.NewIndex + raw - change.OldIndex);
			if (change.OldIndex < change.NewIndex
				&& raw >= change.OldIndex + change.OldCount
				&& raw < change.NewIndex + change.OldCount)
				return static_cast<int>(raw - change.OldCount);
			if (change.NewIndex < change.OldIndex
				&& raw >= change.NewIndex && raw < change.OldIndex)
				return static_cast<int>(raw + change.OldCount);
			return oldIndex;
		case CollectionChangeAction::Swap:
			if (raw == change.OldIndex)
				return static_cast<int>(change.NewIndex);
			if (raw == change.NewIndex)
				return static_cast<int>(change.OldIndex);
			return oldIndex;
		default: return oldIndex;
		}
	};
	if (change.Action == CollectionChangeAction::Swap
		&& change.OldIndex != CollectionChangedEventArgs::Npos
		&& change.NewIndex != CollectionChangedEventArgs::Npos
		&& change.OldIndex != change.NewIndex
		&& change.OldIndex < previousIndices.RangeCount()
		&& change.NewIndex < previousIndices.RangeCount())
	{
		const bool oldExcluded = !previousIndices.Contains(
			static_cast<int>(change.OldIndex));
		const bool newExcluded = !previousIndices.Contains(
			static_cast<int>(change.NewIndex));
		if (oldExcluded != newExcluded)
		{
			(void)next.Toggle(static_cast<int>(change.OldIndex));
			(void)next.Toggle(static_cast<int>(change.NewIndex));
		}
	}
	if (change.Action == CollectionChangeAction::Add
		|| change.Action == CollectionChangeAction::Replace)
	{
		const size_t begin = change.NewIndex;
		if (begin != CollectionChangedEventArgs::Npos)
			next.ExcludeRange(begin, change.NewCount);
	}
	_selectedIndices = std::move(next);
	AdvanceSelectionRevision();
	_selectedFullRangeSnapshot = {};
	const auto mapScalar = [&](int value)
	{
		const int mapped = mapOldIndex(value);
		return mapped >= 0 && _selectedIndices.Contains(mapped)
			? mapped : (_selectedIndices.empty() ? -1 : _selectedIndices.back());
	};
	const int primary = mapScalar(previousPrimary);
	const int anchor = mapScalar(_anchorIndex);
	const int focus = mapScalar(_focusedIndex);
	if (!UpdatePrimarySelectionState(primary, false)) return;
	auto* live = dynamic_cast<ListBox*>(ownerLifetime.Get());
	if (!live) return;
	live->_anchorIndex = anchor;
	live->_focusedIndex = focus;
	if (!live->_selectedIndices.IsRangeBacked())
		live->_skipSelectionIdentityRestoreOnce = true;
}

void ListBox::OnItemsSourceTransactionCommitted()
{
	if (!_pendingSelectionChange) return;
	auto removed = std::move(_pendingSelectionRemovedItems);
	auto added = std::move(_pendingSelectionAddedItems);
	const int oldPrimary = _pendingSelectionOldPrimary;
	_pendingSelectionRemovedItems = {};
	_pendingSelectionAddedItems = {};
	_pendingSelectionOldPrimary = -1;
	_pendingSelectionChange = false;
	RaiseSelectionChanged(
		oldPrimary, GetSelectedIndex(), std::move(removed), std::move(added));
}

void ListBox::OnSelectedIndexChanged(int, int newValue)
{
	if (_applyingSelection || _restoringSelectionIdentities) return;
	_selectedIndices.SetDense(newValue >= 0
		? std::vector<int>{ newValue } : std::vector<int>{});
	AdvanceSelectionRevision();
	_focusedIndex = newValue;
	UpdateAnchor(newValue);
	CaptureSelectionIdentities();
}

void ListBox::OnGeneratedItemsRebuilt()
{
	const ControlWeakReference ownerLifetime(this);
	if (IsItemsSourceReplacementInProgress())
	{
		// A different ItemsSource is a different occurrence domain. Selector
		// clears its primary item/index; do not subsequently restore dense or
		// range identities captured from the retired source into unrelated rows.
		Selector::OnGeneratedItemsRebuilt();
		auto* live = dynamic_cast<ListBox*>(ownerLifetime.Get());
		if (!live) return;
		live->_selectedIndices.Clear();
		live->AdvanceSelectionRevision();
		live->_selectedFullRangeSnapshot = {};
		live->_selectedSourceIdentities.clear();
		live->_primarySourceIdentity = {};
		live->_anchorSourceIdentity = {};
		live->_anchorIndex = -1;
		live->_focusedIndex = -1;
		return;
	}
	if (_skipSelectionIdentityRestoreOnce)
	{
		_skipSelectionIdentityRestoreOnce = false;
		const int primary = GetSelectedIndex() >= 0
			&& _selectedIndices.Contains(GetSelectedIndex())
			? GetSelectedIndex()
			: (_selectedIndices.empty() ? -1 : _selectedIndices.back());
		if (!UpdatePrimarySelectionState(primary, true)) return;
		auto* live = dynamic_cast<ListBox*>(ownerLifetime.Get());
		if (!live) return;
		live->CaptureSelectionIdentities();
		return;
	}
	if (_selectionMode == SelectionMode::Single)
	{
		// Selector owns WPF's single-selection remapping rules, including
		// keeping the index on Replace and following the CollectionView
		// current item when the selected record leaves the view.
		Selector::OnGeneratedItemsRebuilt();
		auto* live = dynamic_cast<ListBox*>(ownerLifetime.Get());
		if (!live) return;
		live->_selectedIndices.SetDense(live->GetSelectedIndex() >= 0
			? std::vector<int>{ live->GetSelectedIndex() }
			: std::vector<int>{});
		live->AdvanceSelectionRevision();
		live->CaptureSelectionIdentities();
		return;
	}
	if (_selectedIndices.IsRangeBacked())
	{
		if (_selectedFullRangeSnapshot)
		{
			BindingListReference currentSnapshot;
			const bool sameSnapshot = TryGetStableItemsSnapshot(currentSnapshot)
				&& currentSnapshot.Shared()
					== _selectedFullRangeSnapshot.Shared();
			if (!sameSnapshot)
			{
				std::vector<size_t> oldToNew;
				if (TryResolveSelectionOccurrencePermutation(
					_selectedFullRangeSnapshot, oldToNew))
				{
					SelectedIndexCollection remapped;
					remapped.SetFullRange(oldToNew.size());
					for (const int excluded :
						_selectedIndices.ExcludedIndices())
						if (excluded >= 0 && static_cast<size_t>(excluded)
							< oldToNew.size())
							(void)remapped.Toggle(static_cast<int>(
								oldToNew[static_cast<size_t>(excluded)]));
					const auto remapScalar = [&](int index)
					{
						return index >= 0 && static_cast<size_t>(index)
							< oldToNew.size()
							? static_cast<int>(oldToNew[
								static_cast<size_t>(index)]) : -1;
					};
					_selectedIndices = std::move(remapped);
					AdvanceSelectionRevision();
					const int primary = remapScalar(GetSelectedIndex());
					const int anchor = remapScalar(_anchorIndex);
					const int focus = remapScalar(_focusedIndex);
					if (!UpdatePrimarySelectionState(primary, false)) return;
					auto* live = dynamic_cast<ListBox*>(ownerLifetime.Get());
					if (!live) return;
					live->_anchorIndex = anchor;
					live->_focusedIndex = focus;
				}
				else
				{
					_pendingSelectionRemovedItems =
						SelectionChangedItemCollection::FromSnapshotRange(
							_selectedFullRangeSnapshot.Shared(),
							_selectedIndices.RangeCount(),
							_selectedIndices.ExcludedIndices());
					_pendingSelectionAddedItems = {};
					_pendingSelectionOldPrimary = GetSelectedIndex();
					_pendingSelectionChange =
						!_pendingSelectionRemovedItems.empty();
					_selectedIndices.Clear();
					AdvanceSelectionRevision();
					_selectedFullRangeSnapshot = {};
					_selectedSourceIdentities.clear();
					_primarySourceIdentity = {};
					_anchorSourceIdentity = {};
					if (!UpdatePrimarySelectionState(-1, true)) return;
					auto* live = dynamic_cast<ListBox*>(ownerLifetime.Get());
					if (!live) return;
					live->_anchorIndex = -1;
					live->_focusedIndex = -1;
					return;
				}
			}
		}
		const size_t count = SelectionItemCount();
		if (_selectedIndices.RangeCount() != count)
		{
			// Only the preparing hook may change a live range domain. A source
			// replacement cannot implicitly select unrelated records.
			_selectedIndices.Clear();
			AdvanceSelectionRevision();
		}
		const int primary = GetSelectedIndex() >= 0
			&& _selectedIndices.Contains(GetSelectedIndex())
			? GetSelectedIndex()
			: (_selectedIndices.empty() ? -1 : _selectedIndices.back());
		if (!UpdatePrimarySelectionState(primary, true)) return;
		auto* live = dynamic_cast<ListBox*>(ownerLifetime.Get());
		if (!live) return;
		if (live->_anchorIndex < 0
			|| !live->_selectedIndices.Contains(live->_anchorIndex))
			live->_anchorIndex = primary;
		if (live->_focusedIndex < 0
			|| !live->_selectedIndices.Contains(live->_focusedIndex))
			live->_focusedIndex = primary;
		if (!live->_selectedIndices.empty()
			&& !live->TryCaptureFullRangeSnapshot())
		{
			live = dynamic_cast<ListBox*>(ownerLifetime.Get());
			if (!live) return;
			live->_selectedIndices.SetDense(std::vector<int>(
				live->_selectedIndices.begin(), live->_selectedIndices.end()));
			live->AdvanceSelectionRevision();
			live->CaptureSelectionIdentities();
		}
		return;
	}
	auto sourceIdentities = _selectedSourceIdentities;
	auto primaryIdentity = _primarySourceIdentity;
	auto anchorIdentity = _anchorSourceIdentity;
	const auto source = GetItemsView();
	const auto* occurrenceView = source
		? dynamic_cast<const IBindingListOccurrenceIdentity*>(source.Get())
		: nullptr;
	const auto* occurrenceLookup = source
		? dynamic_cast<const IBindingListOccurrenceLookup*>(source.Get())
		: nullptr;
	const auto retained = [&](const SourceSelectionIdentity& identity)
	{
		if (!identity.Item || !identity.HasOccurrence || !occurrenceView)
			return false;
		if (occurrenceLookup)
		{
			size_t index = 0;
			if (!occurrenceLookup->TryGetItemIndexByOccurrenceIdentity(
				identity.Occurrence, index)
				|| index >= source.Get()->Count()) return false;
			BindingSourceReference item;
			size_t occurrence = 0;
			return source.Get()->TryGetItem(index, item) && item
				&& item.Shared() == identity.Item.Shared()
				&& occurrenceView->TryGetItemOccurrenceIdentity(
					index, occurrence)
				&& occurrence == identity.Occurrence;
		}
		for (size_t index = 0; index < source.Get()->Count(); ++index)
		{
			BindingSourceReference item;
			size_t occurrence = 0;
			if (source.Get()->TryGetItem(index, item) && item
				&& item.Shared() == identity.Item.Shared()
				&& occurrenceView->TryGetItemOccurrenceIdentity(
					index, occurrence)
				&& occurrence == identity.Occurrence) return true;
		}
		return false;
	};
	const bool canRemapWithoutSelectionDelta =
		!sourceIdentities.empty()
		&& std::all_of(
			sourceIdentities.begin(), sourceIdentities.end(), retained)
		&& retained(primaryIdentity);
	if (canRemapWithoutSelectionDelta)
	{
		// The selected physical occurrences still exist. Going through
		// Selector's object-only single-selection restoration would publish a
		// transient change (and choose the first duplicate) before this class
		// can restore the exact multi-selection. Remap the complete transaction
		// directly; a genuinely removed occurrence still takes the base path.
		RestoreSelectionIdentities();
		return;
	}
	_restoringSelectionIdentities = true;
	try
	{
		Selector::OnGeneratedItemsRebuilt();
	}
	catch (...)
	{
		if (auto* live = dynamic_cast<ListBox*>(ownerLifetime.Get()))
			live->_restoringSelectionIdentities = false;
		throw;
	}
	auto* live = dynamic_cast<ListBox*>(ownerLifetime.Get());
	if (!live) return;
	live->_restoringSelectionIdentities = false;
	live->_selectedSourceIdentities = std::move(sourceIdentities);
	live->_primarySourceIdentity = std::move(primaryIdentity);
	live->_anchorSourceIdentity = std::move(anchorIdentity);
	live->RestoreSelectionIdentities();
}

void ListBox::OnAuthoredItemsChanged() noexcept
{
	if (_selectionMode == SelectionMode::Single)
	{
		Selector::OnAuthoredItemsChanged();
		_selectedIndices.SetDense(GetSelectedIndex() >= 0
			? std::vector<int>{ GetSelectedIndex() }
			: std::vector<int>{});
		AdvanceSelectionRevision();
		CaptureSelectionIdentities();
		return;
	}
	const auto authoredIdentities = _selectedAuthoredIdentities;
	const auto primaryIdentity = _primaryAuthoredIdentity;
	const auto anchorIdentity = _anchorAuthoredIdentity;
	_restoringSelectionIdentities = true;
	Selector::OnAuthoredItemsChanged();
	_restoringSelectionIdentities = false;
	_selectedAuthoredIdentities = authoredIdentities;
	_primaryAuthoredIdentity = primaryIdentity;
	_anchorAuthoredIdentity = anchorIdentity;
	try { RestoreSelectionIdentities(); }
	catch (...) {}
}

void ListBox::FocusIndex(int index)
{
	if (index < 0
		|| static_cast<size_t>(index) >= SelectionItemCount()) return;
	_focusedIndex = index;
	(void)BringItemIntoView(static_cast<size_t>(index));
	auto* item = GetItemsView()
		? GetGeneratedItem(static_cast<size_t>(index))
		: GetAuthoredItem(static_cast<size_t>(index));
	if (auto* window = GetPresentationWindow())
		window->SetKeyboardFocus(item ? item : this, true);
}

void ListBox::UpdateAnchor(int index) noexcept
{
	_anchorIndex = index >= 0 ? index : -1;
}

bool ListBox::TryGetStableItemsSnapshot(
	BindingListReference& snapshot) const noexcept
{
	snapshot = {};
	try
	{
		const auto source = GetItemsView();
		if (!source) return false;
		if (dynamic_cast<const IBindingListStableSnapshot*>(source.Get()))
			snapshot = source;
		else if (const auto* provider = dynamic_cast<
			const IBindingListSnapshotProvider*>(source.Get()))
		{
			BindingListReference stable;
			if (!provider->TryGetStableSnapshot(stable)) return false;
			if (!stable
				|| !dynamic_cast<const IBindingListStableSnapshot*>(stable.Get())
				|| stable.Get()->Count() != source.Get()->Count()
				|| stable.Get()->GetItemTypeToken()
					!= source.Get()->GetItemTypeToken()) return false;
			snapshot = std::move(stable);
		}
		else return false;
		return static_cast<bool>(snapshot);
	}
	catch (...)
	{
		snapshot = {};
		return false;
	}
}

bool ListBox::TryCaptureFullRangeSnapshot() noexcept
{
	if (!_selectedIndices.IsRangeBacked()) return false;
	BindingListReference stable;
	if (!TryGetStableItemsSnapshot(stable)) return false;
	try
	{
		const auto source = GetItemsView();
		if (!source) return false;
		const auto* occurrenceView =
			dynamic_cast<const IBindingListOccurrenceIdentity*>(source.Get());
		const auto capture = [&](int index, SourceSelectionIdentity& identity)
		{
			identity = {};
			if (index < 0 || static_cast<size_t>(index) >= source.Get()->Count()
				|| !source.Get()->TryGetItem(
					static_cast<size_t>(index), identity.Item)
				|| !identity.Item) return;
			if (occurrenceView)
				identity.HasOccurrence = occurrenceView->
					TryGetItemOccurrenceIdentity(
						static_cast<size_t>(index), identity.Occurrence);
		};
		_selectedSourceIdentities.clear();
		_selectedAuthoredIdentities.clear();
		_primaryAuthoredIdentity = nullptr;
		_anchorAuthoredIdentity = nullptr;
		capture(GetSelectedIndex(), _primarySourceIdentity);
		capture(_anchorIndex, _anchorSourceIdentity);
		_selectedFullRangeSnapshot = std::move(stable);
		return true;
	}
	catch (...)
	{
		_selectedFullRangeSnapshot = {};
		_selectedSourceIdentities.clear();
		_primarySourceIdentity = {};
		_anchorSourceIdentity = {};
		return false;
	}
}

void ListBox::CaptureSelectionIdentities() noexcept
{
	_selectedFullRangeSnapshot = {};
	_selectedSourceIdentities.clear();
	_selectedAuthoredIdentities.clear();
	_primarySourceIdentity = {};
	_anchorSourceIdentity = {};
	_primaryAuthoredIdentity = nullptr;
	_anchorAuthoredIdentity = nullptr;
	try
	{
		if (const auto source = GetItemsView())
		{
			const auto* occurrenceView =
				dynamic_cast<const IBindingListOccurrenceIdentity*>(
					source.Get());
			auto capture = [&](size_t index,
				SourceSelectionIdentity& identity)
			{
				identity = {};
				if (index >= source.Get()->Count()
					|| !source.Get()->TryGetItem(index, identity.Item)
					|| !identity.Item) return false;
				if (occurrenceView)
					identity.HasOccurrence =
						occurrenceView->TryGetItemOccurrenceIdentity(
							index, identity.Occurrence);
				return true;
			};
			for (const int index : _selectedIndices)
			{
				SourceSelectionIdentity identity;
				if (index >= 0 && capture(
					static_cast<size_t>(index), identity))
					_selectedSourceIdentities.push_back(
						std::move(identity));
			}
			const int primary = GetSelectedIndex();
			if (primary >= 0)
				(void)capture(
					static_cast<size_t>(primary), _primarySourceIdentity);
			if (_anchorIndex >= 0)
				(void)capture(
					static_cast<size_t>(_anchorIndex), _anchorSourceIdentity);
			return;
		}
		for (const int index : _selectedIndices)
			if (index >= 0)
				_selectedAuthoredIdentities.emplace_back(
					GetAuthoredItem(static_cast<size_t>(index)));
		if (GetSelectedIndex() >= 0)
			_primaryAuthoredIdentity =
				GetAuthoredItem(static_cast<size_t>(GetSelectedIndex()));
		if (_anchorIndex >= 0)
			_anchorAuthoredIdentity =
				GetAuthoredItem(static_cast<size_t>(_anchorIndex));
	}
	catch (...)
	{
		_selectedSourceIdentities.clear();
		_selectedAuthoredIdentities.clear();
		_primarySourceIdentity = {};
		_anchorSourceIdentity = {};
		_primaryAuthoredIdentity = nullptr;
		_anchorAuthoredIdentity = nullptr;
	}
}

void ListBox::RestoreSelectionIdentities()
{
	const ControlWeakReference ownerLifetime(this);
	std::vector<int> restored;
	int primary = -1;
	int anchor = -1;
	if (const auto source = GetItemsView())
	{
		const auto* occurrenceView =
			dynamic_cast<const IBindingListOccurrenceIdentity*>(source.Get());
		const auto* occurrenceLookup =
			dynamic_cast<const IBindingListOccurrenceLookup*>(source.Get());
		const auto populated = [](const SourceSelectionIdentity& identity)
		{ return static_cast<bool>(identity.Item); };
		const bool canUseLookup = occurrenceView && occurrenceLookup
			&& std::all_of(
				_selectedSourceIdentities.begin(),
				_selectedSourceIdentities.end(),
				[](const SourceSelectionIdentity& identity)
				{ return !identity.Item || identity.HasOccurrence; })
			&& (!populated(_primarySourceIdentity)
				|| _primarySourceIdentity.HasOccurrence)
			&& (!populated(_anchorSourceIdentity)
				|| _anchorSourceIdentity.HasOccurrence);
		if (canUseLookup)
		{
			bool aborted = false;
			const auto resolve = [&](const SourceSelectionIdentity& identity,
				int& result)
			{
				result = -1;
				if (!identity.Item) return;
				size_t index = 0;
				const bool located = occurrenceLookup->
					TryGetItemIndexByOccurrenceIdentity(
						identity.Occurrence, index);
				auto* live = dynamic_cast<ListBox*>(ownerLifetime.Get());
				if (!live || live->GetItemsView().Shared() != source.Shared())
				{
					aborted = true;
					return;
				}
				if (!located || index >= source.Get()->Count()
					|| index > static_cast<size_t>(
						(std::numeric_limits<int>::max)())) return;
				BindingSourceReference item;
				size_t occurrence = 0;
				const bool matches = source.Get()->TryGetItem(index, item)
					&& item && item.Shared() == identity.Item.Shared()
					&& occurrenceView->TryGetItemOccurrenceIdentity(
						index, occurrence)
					&& occurrence == identity.Occurrence;
				live = dynamic_cast<ListBox*>(ownerLifetime.Get());
				if (!live || live->GetItemsView().Shared() != source.Shared())
				{
					aborted = true;
					return;
				}
				if (matches) result = static_cast<int>(index);
			};
			resolve(_primarySourceIdentity, primary);
			resolve(_anchorSourceIdentity, anchor);
			for (const auto& identity : _selectedSourceIdentities)
			{
				int index = -1;
				resolve(identity, index);
				if (index >= 0) restored.push_back(index);
			}
			if (aborted) return;
			std::sort(restored.begin(), restored.end());
			restored.erase(
				std::unique(restored.begin(), restored.end()), restored.end());
		}
		else
		{
			for (size_t index = 0; index < source.Get()->Count(); ++index)
			{
				BindingSourceReference item;
				if (!source.Get()->TryGetItem(index, item) || !item) continue;
				size_t occurrence = 0;
				const bool hasOccurrence = occurrenceView
					&& occurrenceView->TryGetItemOccurrenceIdentity(
						index, occurrence);
				const auto matches = [&](const SourceSelectionIdentity& identity)
				{
					if (!identity.Item
						|| identity.Item.Shared() != item.Shared()) return false;
					return !identity.HasOccurrence || !hasOccurrence
						|| identity.Occurrence == occurrence;
				};
				if (matches(_primarySourceIdentity))
					primary = static_cast<int>(index);
				if (matches(_anchorSourceIdentity))
					anchor = static_cast<int>(index);
				if (std::any_of(
					_selectedSourceIdentities.begin(),
					_selectedSourceIdentities.end(),
					matches))
					restored.push_back(static_cast<int>(index));
			}
		}
	}
	else
	{
		for (size_t index = 0; index < AuthoredItemCount(); ++index)
		{
			auto* item = GetAuthoredItem(index);
			if (_primaryAuthoredIdentity.Get() == item)
				primary = static_cast<int>(index);
			if (_anchorAuthoredIdentity.Get() == item)
				anchor = static_cast<int>(index);
			if (std::any_of(
				_selectedAuthoredIdentities.begin(),
				_selectedAuthoredIdentities.end(),
				[item](const ControlWeakReference& selected)
				{ return selected.Get() == item; }))
				restored.push_back(static_cast<int>(index));
		}
	}
	if (primary < 0 && !restored.empty()) primary = restored.back();
	if (anchor < 0) anchor = primary;
	_selectedIndices.SetDense(std::move(restored));
	AdvanceSelectionRevision();
	if (!UpdatePrimarySelectionState(primary, true)) return;
	auto* live = dynamic_cast<ListBox*>(ownerLifetime.Get());
	if (!live) return;
	live->_anchorIndex = anchor;
	live->CaptureSelectionIdentities();
}
