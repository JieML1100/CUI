#include "ItemContainerGenerator.h"

#include "Control.h"

#include <algorithm>
#include <utility>

ItemContainerGenerator::ItemContainerGenerator() = default;
ItemContainerGenerator::~ItemContainerGenerator() = default;
ItemContainerGenerator::ItemContainerGenerator(
	ItemContainerGenerator&&) noexcept = default;
ItemContainerGenerator& ItemContainerGenerator::operator=(
	ItemContainerGenerator&&) noexcept = default;

Control* ItemContainerGenerator::GetRealized(size_t index) const noexcept
{
	const auto found = _realized.find(index);
	return found == _realized.end() ? nullptr : found->second.Visual;
}

bool ItemContainerGenerator::ContainsRealized(size_t index) const noexcept
{
	return _realized.contains(index);
}

void ItemContainerGenerator::StoreRealized(
	size_t index,
	Control* visual,
	BindingPathObservation observation)
{
	_realized[index] = RealizedItem{ visual, std::move(observation) };
}

ItemContainerGenerator::RealizedItem
ItemContainerGenerator::TakeRealized(size_t index)
{
	const auto found = _realized.find(index);
	if (found == _realized.end()) return {};
	auto result = std::move(found->second);
	_realized.erase(found);
	return result;
}

bool ItemContainerGenerator::TakeRecycled(
	size_t index,
	RecycledItem& output)
{
	const auto found = _recycled.find(index);
	if (found == _recycled.end()) return false;
	output = std::move(found->second);
	_recycled.erase(found);
	return true;
}

void ItemContainerGenerator::StoreRecycled(
	size_t index,
	RecycledItem item)
{
	_recycled[index] = std::move(item);
}

void ItemContainerGenerator::DiscardRecycled(size_t index)
{
	_recycled.erase(index);
}

bool ItemContainerGenerator::CanApply(
	const CollectionChangedEventArgs& change,
	size_t actualNewCount) const noexcept
{
	if (change.Action == CollectionChangeAction::Reset
		|| change.OldSize != _sourceCount
		|| change.NewSize != actualNewCount) return false;
	switch (change.Action)
	{
	case CollectionChangeAction::Add:
		return change.NewIndex <= change.OldSize
			&& change.OldCount == 0
			&& change.NewCount > 0
			&& change.OldSize + change.NewCount == change.NewSize;
	case CollectionChangeAction::Remove:
		return change.OldIndex < change.OldSize
			&& change.OldCount > 0
			&& change.OldIndex + change.OldCount <= change.OldSize
			&& change.NewCount == 0
			&& change.OldSize - change.OldCount == change.NewSize;
	case CollectionChangeAction::Replace:
		return change.OldIndex == change.NewIndex
			&& change.OldCount == change.NewCount
			&& change.OldCount > 0
			&& change.OldIndex + change.OldCount <= change.OldSize
			&& change.OldSize == change.NewSize;
	case CollectionChangeAction::Move:
		return change.OldCount == 1 && change.NewCount == 1
			&& change.OldSize == change.NewSize
			&& change.OldIndex < change.OldSize
			&& change.NewIndex < change.NewSize;
	case CollectionChangeAction::Swap:
		return change.OldCount == 1 && change.NewCount == 1
			&& change.OldSize == change.NewSize
			&& change.OldIndex < change.OldSize
			&& change.NewIndex < change.NewSize;
	default:
		return false;
	}
}

std::optional<size_t> ItemContainerGenerator::MapIndex(
	const CollectionChangedEventArgs& change,
	size_t oldIndex) noexcept
{
	switch (change.Action)
	{
	case CollectionChangeAction::Add:
		return oldIndex >= change.NewIndex
			? oldIndex + change.NewCount : oldIndex;
	case CollectionChangeAction::Remove:
		if (oldIndex >= change.OldIndex
			&& oldIndex < change.OldIndex + change.OldCount) return std::nullopt;
		return oldIndex >= change.OldIndex + change.OldCount
			? oldIndex - change.OldCount : oldIndex;
	case CollectionChangeAction::Replace:
		if (oldIndex >= change.OldIndex
			&& oldIndex < change.OldIndex + change.OldCount) return std::nullopt;
		return oldIndex;
	case CollectionChangeAction::Move:
		if (oldIndex == change.OldIndex) return change.NewIndex;
		if (change.OldIndex < change.NewIndex
			&& oldIndex > change.OldIndex && oldIndex <= change.NewIndex)
			return oldIndex - 1;
		if (change.OldIndex > change.NewIndex
			&& oldIndex >= change.NewIndex && oldIndex < change.OldIndex)
			return oldIndex + 1;
		return oldIndex;
	case CollectionChangeAction::Swap:
		if (oldIndex == change.OldIndex) return change.NewIndex;
		if (oldIndex == change.NewIndex) return change.OldIndex;
		return oldIndex;
	default:
		return std::nullopt;
	}
}

std::vector<size_t> ItemContainerGenerator::InvalidatedRealizedIndices(
	const CollectionChangedEventArgs& change) const
{
	std::vector<size_t> result;
	for (const auto& [index, item] : _realized)
	{
		(void)item;
		if (!MapIndex(change, index)) result.push_back(index);
	}
	return result;
}

std::vector<ItemContainerGenerator::IndexChange>
ItemContainerGenerator::Apply(const CollectionChangedEventArgs& change)
{
	std::vector<IndexChange> changes;
	RealizedMap realized;
	for (auto& [oldIndex, item] : _realized)
	{
		const auto mapped = MapIndex(change, oldIndex);
		if (!mapped) continue;
		if (*mapped != oldIndex)
			changes.push_back({ item.Visual, oldIndex, *mapped });
		realized.emplace(*mapped, std::move(item));
	}
	_realized = std::move(realized);

	RecycledMap recycled;
	for (auto& [oldIndex, item] : _recycled)
	{
		const auto mapped = MapIndex(change, oldIndex);
		if (!mapped) continue;
		if (*mapped != oldIndex)
			changes.push_back({ item.Visual.get(), oldIndex, *mapped });
		recycled.emplace(*mapped, std::move(item));
	}
	_recycled = std::move(recycled);
	_sourceCount = change.NewSize;
	return changes;
}
