#pragma once

#include "BindingList.h"

#include <map>
#include <memory>
#include <optional>
#include <vector>

class Control;

/**
 * Owns the index-to-container state behind ItemsControl.
 *
 * The visual parent still owns realized controls. Recycled controls are owned
 * here. Structural collection changes are translated once so selection,
 * layout, hit testing and recycling all observe the same index mapping.
 */
class ItemContainerGenerator final
{
public:
	ItemContainerGenerator();
	~ItemContainerGenerator();
	ItemContainerGenerator(ItemContainerGenerator&&) noexcept;
	ItemContainerGenerator& operator=(ItemContainerGenerator&&) noexcept;
	ItemContainerGenerator(const ItemContainerGenerator&) = delete;
	ItemContainerGenerator& operator=(const ItemContainerGenerator&) = delete;

	struct RealizedItem final
	{
		Control* Visual = nullptr;
		BindingPathObservation Observation;
	};
	struct RecycledItem final
	{
		std::unique_ptr<Control> Visual;
		BindingPathObservation Observation;
	};
	struct IndexChange final
	{
		Control* Visual = nullptr;
		size_t OldIndex = 0;
		size_t NewIndex = 0;
	};

	using RealizedMap = std::map<size_t, RealizedItem>;
	using RecycledMap = std::map<size_t, RecycledItem>;

	size_t SourceCount() const noexcept { return _sourceCount; }
	void SetSourceCount(size_t value) noexcept { _sourceCount = value; }
	size_t RealizedCount() const noexcept { return _realized.size(); }
	size_t RecycledCount() const noexcept { return _recycled.size(); }
	const RealizedMap& RealizedItems() const noexcept { return _realized; }
	const RecycledMap& RecycledItems() const noexcept { return _recycled; }
	RecycledMap& RecycledItems() noexcept { return _recycled; }

	Control* GetRealized(size_t index) const noexcept;
	bool ContainsRealized(size_t index) const noexcept;
	void StoreRealized(
		size_t index,
		Control* visual,
		BindingPathObservation observation = {});
	RealizedItem TakeRealized(size_t index);
	bool TakeRecycled(size_t index, RecycledItem& output);
	void StoreRecycled(size_t index, RecycledItem item);
	void DiscardRecycled(size_t index);
	void ClearRealized() noexcept { _realized.clear(); }
	void ClearRecycled() noexcept { _recycled.clear(); }

	bool CanApply(
		const CollectionChangedEventArgs& change,
		size_t actualNewCount) const noexcept;
	std::vector<size_t> InvalidatedRealizedIndices(
		const CollectionChangedEventArgs& change) const;
	std::vector<IndexChange> Apply(
		const CollectionChangedEventArgs& change);
	static std::optional<size_t> MapIndex(
		const CollectionChangedEventArgs& change,
		size_t oldIndex) noexcept;

private:
	size_t _sourceCount = 0;
	RealizedMap _realized;
	RecycledMap _recycled;
};
