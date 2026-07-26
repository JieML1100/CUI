#include "ItemsControl.h"
#include "DependencyPropertyInfrastructure.h"
#include "XamlInfrastructure.h"
#include "StyleInfrastructure.h"
#include "TreeInfrastructure.h"

#include "Window.h"
#include "Label.h"
#include "ContentPresenter.h"
#include "Layout/OverlayLayout.h"
#include "Layout/StackPanel.h"
#include "Layout/WrapPanel.h"

#include <algorithm>
#include <cmath>
#include <cwctype>
#include <exception>
#include <iterator>
#include <limits>
#include <span>
#include <stdexcept>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace
{
	void ClearTemplateOwner(Control* root, Control* owner)
	{
		if (!root || !owner) return;
		std::vector<Control*> stack{ root };
		while (!stack.empty())
		{
			auto* current = stack.back();
			stack.pop_back();
			if (!current) continue;
			for (auto* child : current->GetVisualChildrenView())
				if (child) stack.push_back(child);
			if (current->GetTemplatedParent() == owner)
				cui::framework::XamlAccess::SetTemplatedParent(*current, nullptr);
		}
	}

	class MaterializedItemsSourceSnapshot final
		: public IBindingList,
		  public IBindingListGroupView
	{
	public:
		MaterializedItemsSourceSnapshot(
			std::wstring itemTypeName,
			std::vector<BindingSourceReference> items,
			std::vector<BindingListGroup> groups)
			: _itemTypeName(std::move(itemTypeName)),
			  _items(std::move(items)),
			  _groups(std::move(groups)) {}

		size_t Count() const noexcept override { return _items.size(); }
		bool TryGetItem(
			size_t index,
			BindingSourceReference& out) const override
		{
			if (index >= _items.size() || !_items[index]) return false;
			out = _items[index];
			return true;
		}
		EventConnection SubscribeChanged(ChangedHandler) override { return {}; }
		const std::wstring& ItemTypeName() const noexcept override
		{
			return _itemTypeName;
		}
		const std::vector<BindingListGroup>& Groups() const noexcept override
		{
			return _groups;
		}
		EventConnection SubscribeGroupsChanged(
			GroupsChangedHandler) override { return {}; }
		bool CanApply(
			const CollectionChangedEventArgs& change,
			size_t actualNewCount,
			const std::wstring& itemTypeName) const noexcept
		{
			if (change.Action == CollectionChangeAction::Reset
				|| change.OldSize != _items.size()
				|| change.NewSize != actualNewCount
				|| itemTypeName != _itemTypeName) return false;
			switch (change.Action)
			{
			case CollectionChangeAction::Add:
				return change.NewIndex <= change.OldSize
					&& change.OldCount == 0 && change.NewCount > 0
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
			case CollectionChangeAction::Swap:
				return change.OldCount == 1 && change.NewCount == 1
					&& change.OldSize == change.NewSize
					&& change.OldIndex < change.OldSize
					&& change.NewIndex < change.NewSize;
			default:
				return false;
			}
		}
		void Reserve(size_t count) { _items.reserve(count); }
		void Apply(
			const CollectionChangedEventArgs& change,
			std::vector<BindingSourceReference> changedItems,
			std::vector<BindingListGroup> groups) noexcept
		{
			switch (change.Action)
			{
			case CollectionChangeAction::Add:
				_items.insert(
					_items.begin() + change.NewIndex,
					std::make_move_iterator(changedItems.begin()),
					std::make_move_iterator(changedItems.end()));
				break;
			case CollectionChangeAction::Remove:
				_items.erase(
					_items.begin() + change.OldIndex,
					_items.begin() + change.OldIndex + change.OldCount);
				break;
			case CollectionChangeAction::Replace:
				for (size_t offset = 0; offset < changedItems.size(); ++offset)
					_items[change.NewIndex + offset] =
						std::move(changedItems[offset]);
				break;
			case CollectionChangeAction::Move:
				if (change.OldIndex < change.NewIndex)
					std::rotate(
						_items.begin() + change.OldIndex,
						_items.begin() + change.OldIndex + 1,
						_items.begin() + change.NewIndex + 1);
				else
					std::rotate(
						_items.begin() + change.NewIndex,
						_items.begin() + change.OldIndex,
						_items.begin() + change.OldIndex + 1);
				break;
			case CollectionChangeAction::Swap:
				std::swap(
					_items[change.OldIndex], _items[change.NewIndex]);
				break;
			default:
				break;
			}
			_groups.swap(groups);
		}

	private:
		std::wstring _itemTypeName;
		std::vector<BindingSourceReference> _items;
		std::vector<BindingListGroup> _groups;
	};

	bool TryCaptureItemsSourceSnapshot(
		const BindingListReference& source,
		BindingListReference& output,
		std::wstring& error)
	{
		output = {};
		error.clear();
		if (!source) return true;
		std::vector<BindingSourceReference> items;
		items.reserve(source.Get()->Count());
		for (size_t index = 0; index < source.Get()->Count(); ++index)
		{
			BindingSourceReference item;
			if (!source.Get()->TryGetItem(index, item) || !item)
			{
				error = L"ItemsSource 无法读取索引 "
					+ std::to_wstring(index) + L"。";
				return false;
			}
			items.push_back(std::move(item));
		}
		std::vector<BindingListGroup> groups;
		if (const auto* grouped = dynamic_cast<const IBindingListGroupView*>(
			source.Get()))
			groups = grouped->Groups();
		output = BindingListReference(
			std::make_shared<MaterializedItemsSourceSnapshot>(
				source.Get()->ItemTypeName(),
				std::move(items), std::move(groups)));
		return true;
	}

	struct PreparedMaterializedSnapshotChange final
	{
		BindingListReference Replacement;
		std::vector<BindingSourceReference> ChangedItems;
		std::vector<BindingListGroup> Groups;
		bool Replace = false;
	};

	bool TryPrepareMaterializedSnapshotChange(
		const BindingListReference& source,
		const BindingListReference& materializedSnapshot,
		const CollectionChangedEventArgs& change,
		PreparedMaterializedSnapshotChange& output,
		std::wstring& error)
	{
		output = {};
		error.clear();
		auto* snapshot = materializedSnapshot
			? dynamic_cast<MaterializedItemsSourceSnapshot*>(
				materializedSnapshot.Get()) : nullptr;
		if (!source || !snapshot || !snapshot->CanApply(
			change, source.Get()->Count(), source.Get()->ItemTypeName()))
		{
			output.Replace = true;
			return TryCaptureItemsSourceSnapshot(
				source, output.Replacement, error);
		}

		if (const auto* grouped = dynamic_cast<const IBindingListGroupView*>(
			source.Get()))
			output.Groups = grouped->Groups();
		if (change.Action == CollectionChangeAction::Add
			|| change.Action == CollectionChangeAction::Replace)
		{
			output.ChangedItems.reserve(change.NewCount);
			for (size_t offset = 0; offset < change.NewCount; ++offset)
			{
				BindingSourceReference item;
				const size_t index = change.NewIndex + offset;
				if (!source.Get()->TryGetItem(index, item) || !item)
				{
					error = L"ItemsSource 无法读取索引 "
						+ std::to_wstring(index) + L"。";
					return false;
				}
				output.ChangedItems.push_back(std::move(item));
			}
		}
		// All allocations happen before the visual tree commits. Apply() then
		// consists solely of noexcept moves/erases/rotations and a groups swap.
		if (change.Action == CollectionChangeAction::Add)
			snapshot->Reserve(change.NewSize);
		return true;
	}

	bool EqualsTypeName(const std::wstring& left, const std::wstring& right)
	{
		return left == right;
	}

	template<typename TValue>
	DependencyPropertyOptions<ItemsControl, TValue> DataOptions(
		TValue defaultValue,
		int order,
		DependencyPropertyPersistence persistence = DependencyPropertyPersistence::Metadata)
	{
		DependencyPropertyOptions<ItemsControl, TValue> options;
		options.DefaultValue = std::move(defaultValue);
		options.Flags = DependencyPropertyFlags::AffectsMeasure
			| DependencyPropertyFlags::AffectsArrange
			| DependencyPropertyFlags::AffectsRender;
		options.Design.Category = L"Data";
		options.Design.CategoryOrder = 80;
		options.Design.Order = order;
		options.Design.Editor = DependencyPropertyEditorKind::Auto;
		options.Design.Persistence = persistence;
		return options;
	}

	const ItemsPanelTemplate& DefaultItemsPanel() noexcept
	{
		static const ItemsPanelTemplate value{};
		return value;
	}

	bool IsFiniteNonNegative(float value) noexcept
	{
		return std::isfinite(value) && value >= 0.0f;
	}

	// Internal estimate used only to seed virtualized grouped extents. It is not
	// an authored GroupStyle member; realized headers measure from their template.
	constexpr float VirtualizedGroupHeaderEstimate = 24.0f;

	bool IsValidItemsPanel(const ItemsPanelTemplate& value) noexcept
	{
		if (!IsFiniteNonNegative(value.ItemWidth)
			|| !IsFiniteNonNegative(value.ItemHeight)
			|| !IsFiniteNonNegative(value.CacheLength)) return false;
		if (value.Kind == ItemsPanelKind::VirtualizingStack)
			return value.Orientation == Orientation::Vertical
				&& value.ItemHeight > 0.0f;
		return true;
	}

	class VirtualizingItemsHost;

	class VirtualizingStackLayoutEngine final : public LayoutEngine
	{
	public:
		explicit VirtualizingStackLayoutEngine(VirtualizingItemsHost& owner)
			: _owner(owner) {}
		cui::core::Size Measure(
			LayoutContext& context,
			const cui::core::Constraints& available) override;
		void Arrange(LayoutContext& context, cui::core::Rect finalRect) override;

	private:
		VirtualizingItemsHost& _owner;
	};

	class VirtualizingItemsHost final : public Panel
	{
	public:
		VirtualizingItemsHost()
		{
			SetLayoutEngine(new VirtualizingStackLayoutEngine(*this));
			(void)TrySetPropertyValue(
				L"VerticalAlignment", BindingValue(VerticalAlignment::Top),
				DependencyPropertyValueSource::Template);
		}

		void SetConfiguration(
			size_t itemCount,
			float itemHeight,
			std::vector<size_t> headerCounts,
			float headerHeight)
		{
			if (_itemCount == itemCount && _itemHeight == itemHeight
				&& _headerCounts == headerCounts
				&& _headerHeight == headerHeight) return;
			_itemCount = itemCount;
			_itemHeight = itemHeight;
			_headerCounts = std::move(headerCounts);
			_headerHeight = headerHeight;
			_offsets.assign(_itemCount + 1, 0.0f);
			for (size_t index = 0; index < _itemCount; ++index)
			{
				const size_t headerCount = index < _headerCounts.size()
					? _headerCounts[index] : 0;
				const float extent = _itemHeight
					+ static_cast<float>(headerCount) * _headerHeight;
				_offsets[index + 1] = _offsets[index] + extent;
			}
			InvalidateLayout();
		}
		void RegisterItem(Control* control, size_t index)
		{
			if (!control) return;
			_indices[control] = index;
			InvalidateLayout();
		}
		void UnregisterItem(Control* control)
		{
			_indices.erase(control);
			InvalidateLayout();
		}
		void SynchronizeAuthoredItems(
			std::span<Control* const> items)
		{
			std::unordered_map<Control*, size_t> next;
			next.reserve(items.size());
			for (size_t index = 0; index < items.size(); ++index)
				if (items[index]) next.emplace(items[index], index);
			if (_indices == next) return;
			_indices = std::move(next);
			InvalidateLayout();
		}
		size_t IndexOf(const Control* control) const noexcept
		{
			const auto found = _indices.find(const_cast<Control*>(control));
			return found == _indices.end()
				? (std::numeric_limits<size_t>::max)() : found->second;
		}
		float ItemHeight() const noexcept { return _itemHeight; }
		float ItemTop(size_t index) const noexcept
		{
			return index < _offsets.size() ? _offsets[index] : ContentHeight();
		}
		float ItemExtent(size_t index) const noexcept
		{
			if (index >= _itemCount || index + 1 >= _offsets.size()) return 0.0f;
			return _offsets[index + 1] - _offsets[index];
		}
		size_t IndexAtOffset(float offset) const noexcept
		{
			if (_itemCount == 0) return 0;
			offset = (std::clamp)(offset, 0.0f,
				(std::max)(0.0f, ContentHeight() - 0.0001f));
			const auto found = std::upper_bound(
				_offsets.begin(), _offsets.end(), offset);
			return (std::min)(_itemCount - 1,
				found == _offsets.begin() ? size_t{ 0 }
					: static_cast<size_t>(std::distance(
						_offsets.begin(), found) - 1));
		}
		float ContentHeight() const noexcept
		{
			return _offsets.empty() ? 0.0f : _offsets.back();
		}

	private:
		size_t _itemCount = 0;
		float _itemHeight = 0.0f;
		float _headerHeight = 0.0f;
		std::vector<size_t> _headerCounts;
		std::vector<float> _offsets;
		std::unordered_map<Control*, size_t> _indices;
	};

	class GroupedItemHost;

	class GroupedItemsLayoutEngine final : public LayoutEngine
	{
	public:
		explicit GroupedItemsLayoutEngine(GroupedItemHost& owner)
			: _owner(owner) {}
		cui::core::Size Measure(
			LayoutContext& context,
			const cui::core::Constraints& available) override;
		void Arrange(
			LayoutContext& context,
			cui::core::Rect finalRect) override;

	private:
		GroupedItemHost& _owner;
	};

	class GroupedItemHost final : public Panel
	{
	public:
		GroupedItemHost(
			std::unique_ptr<Control> item,
			Control* itemLogicalParent)
			: Panel()
		{
			SetLayoutEngine(new GroupedItemsLayoutEngine(*this));
			_item = item.get();
			cui::framework::TreeAccess::AddOwnedVisualChild(
				*this, std::move(item), itemLogicalParent);
		}

		Control* Item() const noexcept { return _item; }
		size_t HeaderCount() const noexcept { return _headerCount; }
		float FixedHeaderHeight() const noexcept { return _fixedHeaderHeight; }
		float FixedItemHeight() const noexcept { return _fixedItemHeight; }
		std::unique_ptr<Control> TakeItem()
		{
			auto result = DetachVisualChild(_item);
			_item = nullptr;
			return result;
		}
		void SetHeaders(
			std::vector<std::unique_ptr<Control>> headers,
			std::vector<BindingSourceReference> contexts,
			float fixedHeaderHeight = 0.0f,
			float fixedItemHeight = 0.0f)
		{
			auto* itemLogicalParent = _item
				? _item->GetLogicalParent() : nullptr;
			auto item = DetachVisualChild(_item);
			while (!GetVisualChildrenView().empty())
			{
				auto discarded = DetachVisualChild(GetVisualChildrenView().front());
				(void)discarded;
			}
			_headerCount = headers.size();
			_fixedHeaderHeight = (std::max)(0.0f, fixedHeaderHeight);
			_fixedItemHeight = (std::max)(0.0f, fixedItemHeight);
			_contexts = std::move(contexts);
			for (auto& header : headers)
				AddOwned(std::move(header));
			_item = item.get();
			if (item)
				cui::framework::TreeAccess::AddOwnedVisualChild(
					*this, std::move(item), itemLogicalParent);
			InvalidateLayout();
		}

	private:
		Control* _item = nullptr;
		size_t _headerCount = 0;
		float _fixedHeaderHeight = 0.0f;
		float _fixedItemHeight = 0.0f;
		std::vector<BindingSourceReference> _contexts;
	};

	struct GroupedLayoutItem final
	{
		Control* Child = nullptr;
		Thickness Margin{};
		cui::core::Size Desired{};
		float OuterHeight = 0.0f;
	};

	float DeflateGroupedExtent(float extent, float before, float after)
	{
		return std::isfinite(extent)
			? (std::max)(0.0f, extent - before - after)
			: cui::core::Infinity;
	}

	std::vector<GroupedLayoutItem> MeasureGroupedItems(
		GroupedItemHost& owner,
		LayoutContext& context,
		float availableWidth)
	{
		std::vector<GroupedLayoutItem> result;
		result.reserve(static_cast<size_t>((std::max)(0, context.ChildCount())));
		for (int childIndex = 0; childIndex < context.ChildCount(); ++childIndex)
		{
			auto* child = context.ChildAt(childIndex);
			if (!child || child->IsCollapsed()) continue;
			const bool isHeader = static_cast<size_t>(childIndex)
				< owner.HeaderCount();
			const auto margin = child->Margin;
			const float fixedOuterHeight = isHeader
				? owner.FixedHeaderHeight() : owner.FixedItemHeight();
			const float childWidth = DeflateGroupedExtent(
				availableWidth, margin.Left, margin.Right);
			const float childHeight = fixedOuterHeight > 0.0f
				? (std::max)(0.0f,
					fixedOuterHeight - margin.Top - margin.Bottom)
				: cui::core::Infinity;
			const auto desired = child->Measure(cui::core::Constraints{
				cui::core::Size{ childWidth, childHeight } });
			result.push_back(GroupedLayoutItem{
				child, margin, desired,
				fixedOuterHeight > 0.0f ? fixedOuterHeight
					: desired.height + margin.Top + margin.Bottom });
		}
		return result;
	}

	cui::core::Size GroupedItemsLayoutEngine::Measure(
		LayoutContext& context,
		const cui::core::Constraints& available)
	{
		const auto items = MeasureGroupedItems(
			_owner, context, available.Normalized().maximum.width);
		cui::core::Size desired{};
		for (const auto& item : items)
		{
			desired.width = (std::max)(desired.width,
				item.Margin.Left + item.Desired.width
					+ item.Margin.Right);
			desired.height += item.OuterHeight;
		}
		_needsLayout = false;
		return desired;
	}

	void GroupedItemsLayoutEngine::Arrange(
		LayoutContext& context,
		cui::core::Rect finalRect)
	{
		finalRect = finalRect.Normalized();
		const auto items = MeasureGroupedItems(
			_owner, context, finalRect.width);
		float currentY = finalRect.y;
		for (size_t index = 0; index < items.size(); ++index)
		{
			const auto& item = items[index];
			const float availableWidth = DeflateGroupedExtent(
				finalRect.width, item.Margin.Left, item.Margin.Right);
			const float availableHeight = (std::max)(0.0f,
				item.OuterHeight - item.Margin.Top - item.Margin.Bottom);
			float width = (std::min)(item.Desired.width, availableWidth);
			float height = (std::min)(item.Desired.height, availableHeight);
			if (item.Child->HorizontalAlignment == HorizontalAlignment::Stretch)
				width = availableWidth;
			if (item.Child->VerticalAlignment == VerticalAlignment::Stretch)
				height = availableHeight;
			float x = finalRect.x + item.Margin.Left;
			if (item.Child->HorizontalAlignment == HorizontalAlignment::Center)
				x += (availableWidth - width) * 0.5f;
			else if (item.Child->HorizontalAlignment == HorizontalAlignment::Right)
				x += availableWidth - width;
			float y = currentY + item.Margin.Top;
			if (item.Child->VerticalAlignment == VerticalAlignment::Center)
				y += (availableHeight - height) * 0.5f;
			else if (item.Child->VerticalAlignment == VerticalAlignment::Bottom)
				y += availableHeight - height;
			item.Child->Arrange(cui::core::Rect{ x, y, width, height });
			currentY += item.OuterHeight;
		}
		_needsLayout = false;
	}

	cui::core::Size VirtualizingStackLayoutEngine::Measure(
		LayoutContext& context,
		const cui::core::Constraints& available)
	{
		const auto maximum = available.Normalized().maximum;
		float desiredWidth = 0.0f;
		for (int index = 0; index < context.ChildCount(); ++index)
		{
			auto* child = context.ChildAt(index);
			if (!child || child->IsCollapsed()) continue;
			const auto margin = child->Margin;
			const float width = std::isfinite(maximum.width)
				? (std::max)(0.0f, maximum.width - margin.Left - margin.Right)
				: cui::core::Infinity;
			const auto itemIndex = _owner.IndexOf(child);
			if (itemIndex == (std::numeric_limits<size_t>::max)()) continue;
			const float height = (std::max)(0.0f,
				_owner.ItemExtent(itemIndex) - margin.Top - margin.Bottom);
			const auto desired = child->Measure(
				cui::core::Constraints{ cui::core::Size{ width, height } });
			desiredWidth = (std::max)(desiredWidth,
				desired.width + margin.Left + margin.Right);
		}
		_needsLayout = false;
		return cui::core::Size{ desiredWidth, _owner.ContentHeight() };
	}

	void VirtualizingStackLayoutEngine::Arrange(
		LayoutContext& context,
		cui::core::Rect finalRect)
	{
		finalRect = finalRect.Normalized();
		const float width = finalRect.width;
		for (int childIndex = 0; childIndex < context.ChildCount(); ++childIndex)
		{
			auto* child = context.ChildAt(childIndex);
			if (!child || child->IsCollapsed()) continue;
			const auto itemIndex = _owner.IndexOf(child);
			if (itemIndex == (std::numeric_limits<size_t>::max)()) continue;
			const auto margin = child->Margin;
			const float cellTop = _owner.ItemTop(itemIndex);
			child->Arrange(cui::core::Rect{
				finalRect.x + margin.Left,
				finalRect.y + cellTop + margin.Top,
				(std::max)(0.0f, width - margin.Left - margin.Right),
				(std::max)(0.0f,
					_owner.ItemExtent(itemIndex) - margin.Top - margin.Bottom) });
		}
		_needsLayout = false;
	}
}

ItemsControl::ItemsControl()
	: Control()
{
	auto itemsHost = CreateItemsHost();
	_itemsHost = itemsHost.get();
	_changingItemsHost = true;
	cui::framework::XamlAccess::SetTemplatedParent(*_itemsHost, this);
	cui::framework::TreeAccess::AddOwnedVisualChild(
		*this, std::move(itemsHost), nullptr);
	_changingItemsHost = false;
	RefreshItemsScrollOwner();
}

bool ItemsControl::ValidateVisualChildCollection(
	std::span<Control* const> children,
	std::string& error) const
{
	if (_changingItemsHost || _changingTemplateInfrastructure) return true;
	if (_controlTemplateRoot && children.size() == 1
		&& children.front() == _controlTemplateRoot)
		return true;
	if (!_controlTemplateRoot && _itemsHost && children.size() == 1
		&& children.front() == _itemsHost)
		return true;
	error = "ItemsControl direct children are owned by its ControlTemplate or ItemsPanelTemplate host";
	return false;
}

void ItemsControl::RegisterDependencyProperties()
{
	Control::RegisterDependencyProperties();
	static const bool registered = []
	{
		auto sourceOptions = DataOptions(
			BindingListReference{}, 10,
			DependencyPropertyPersistence::Native);
		sourceOptions.Design.Browsable = false;
		DependencyPropertyRegistry::Register<ItemsControl, BindingListReference>(
			L"ItemsSource",
			[](ItemsControl& target) { return target.GetItemsSource(); },
			[](ItemsControl& target, const BindingListReference& value)
			{ target.SetItemsSource(value); }, {}, std::move(sourceOptions));

		auto templateOptions = DataOptions(
			ItemTemplateReference{}, 20,
			DependencyPropertyPersistence::Native);
		templateOptions.Design.Browsable = false;
		DependencyPropertyRegistry::Register<ItemsControl, ItemTemplateReference>(
			L"ItemTemplate",
			[](ItemsControl& target) { return target.GetItemTemplate(); },
			[](ItemsControl& target, const ItemTemplateReference& value)
			{ target.SetItemTemplate(value); }, {}, std::move(templateOptions));

		auto groupOptions = DataOptions(
			GroupStyleReference{}, 25,
			DependencyPropertyPersistence::Native);
		groupOptions.Design.Browsable = false;
		DependencyPropertyRegistry::Register<ItemsControl, GroupStyleReference>(
			L"GroupStyle",
			[](ItemsControl& target) { return target.GetGroupStyle(); },
			[](ItemsControl& target, const GroupStyleReference& value)
			{ target.SetGroupStyle(value); }, {}, std::move(groupOptions));

		auto panelOptions = DataOptions(
			ItemsPanelTemplateReference{}, 30,
			DependencyPropertyPersistence::Native);
		panelOptions.Design.Browsable = false;
		DependencyPropertyRegistry::Register<ItemsControl, ItemsPanelTemplateReference>(
			L"ItemsPanel",
			[](ItemsControl& target) { return target.GetItemsPanel(); },
			[](ItemsControl& target, const ItemsPanelTemplateReference& value)
			{ target.SetItemsPanel(value); }, {}, std::move(panelOptions));

		auto pathOptions = DataOptions(std::wstring{}, 40);
		pathOptions.Design.Editor = DependencyPropertyEditorKind::Text;
		DependencyPropertyRegistry::Register<ItemsControl, std::wstring>(
			L"DisplayMemberPath",
			[](ItemsControl& target) { return target.GetDisplayMemberPath(); },
			[](ItemsControl& target, const std::wstring& value)
			{ target.SetDisplayMemberPath(value); }, {}, std::move(pathOptions));

		auto containerStyleOptions = DataOptions(
			std::wstring{}, 50,
			DependencyPropertyPersistence::Native);
		containerStyleOptions.Design.Browsable = false;
		DependencyPropertyRegistry::Register<ItemsControl, std::wstring>(
			L"ItemContainerStyle",
			[](ItemsControl& target) { return target.GetItemContainerStyle(); },
			[](ItemsControl& target, const std::wstring& value)
			{ target.SetItemContainerStyle(value); }, {},
			std::move(containerStyleOptions));
		return true;
	}();
	(void)registered;
}

const ItemsPanelTemplate& ItemsControl::EffectiveItemsPanel() const noexcept
{
	return _itemsPanel ? *_itemsPanel.Get() : DefaultItemsPanel();
}

bool ItemsControl::IsVirtualizing() const noexcept
{
	return EffectiveItemsPanel().Kind == ItemsPanelKind::VirtualizingStack;
}

void ItemsControl::ConfigureVirtualHost()
{
	auto* host = dynamic_cast<VirtualizingItemsHost*>(_itemsHost);
	if (!host) return;
	std::vector<size_t> headerCounts(ItemCount(), 0);
	if (IsGroupingActive() && _groupStyle)
	{
		if (const auto* grouped = dynamic_cast<const IBindingListGroupView*>(
			_itemsSource.Get()))
			for (const auto& group : grouped->Groups())
				if (group.StartIndex < headerCounts.size())
					++headerCounts[group.StartIndex];
	}
	const auto& panel = EffectiveItemsPanel();
	host->SetConfiguration(ItemCount(), panel.ItemHeight,
		std::move(headerCounts), IsGroupingActive()
			? VirtualizedGroupHeaderEstimate : 0.0f);
	if (!_itemsSource)
		host->SynchronizeAuthoredItems(_authoredItems);
}

std::unique_ptr<Panel> ItemsControl::CreateItemsHost() const
{
	const auto& definition = EffectiveItemsPanel();
	std::unique_ptr<Panel> result;
	if (definition.Kind == ItemsPanelKind::Wrap)
	{
		auto panel = std::make_unique<WrapPanel>();
		(void)cui::framework::DependencyPropertyAccess::SetValue(
			*panel, L"Orientation", BindingValue(definition.Orientation),
			DependencyPropertyValueSource::Template);
		(void)cui::framework::DependencyPropertyAccess::SetValue(
			*panel, L"ItemWidth", BindingValue(definition.ItemWidth),
			DependencyPropertyValueSource::Template);
		(void)cui::framework::DependencyPropertyAccess::SetValue(
			*panel, L"ItemHeight", BindingValue(definition.ItemHeight),
			DependencyPropertyValueSource::Template);
		result = std::move(panel);
	}
	else if (definition.Kind == ItemsPanelKind::VirtualizingStack)
	{
		result = std::make_unique<VirtualizingItemsHost>();
	}
	else
	{
		auto panel = std::make_unique<StackPanel>();
		(void)cui::framework::DependencyPropertyAccess::SetValue(
			*panel, L"Orientation", BindingValue(definition.Orientation),
			DependencyPropertyValueSource::Template);
		result = std::move(panel);
	}
	(void)cui::framework::DependencyPropertyAccess::SetValue(
		*result, L"VerticalAlignment", BindingValue(VerticalAlignment::Top),
		DependencyPropertyValueSource::Template);
	return result;
}

std::unique_ptr<Panel> ItemsControl::TakeItemsHost()
{
	if (!_itemsHost) return {};
	if (_templateItemsPresenter
		&& _templateItemsPresenter->GetItemsHost() == _itemsHost)
		return _templateItemsPresenter->DetachItemsHost();
	if (_detachedItemsHost)
		return std::move(_detachedItemsHost);
	if (_itemsHost->GetVisualParent() != this) return {};
	_changingItemsHost = true;
	std::unique_ptr<Control> detached;
	try
	{
		detached = DetachVisualChild(_itemsHost);
		_changingItemsHost = false;
	}
	catch (...)
	{
		_changingItemsHost = false;
		throw;
	}
	return std::unique_ptr<Panel>(
		static_cast<Panel*>(detached.release()));
}

void ItemsControl::PlaceItemsHost(std::unique_ptr<Panel> host)
{
	if (!host) throw std::invalid_argument("ItemsControl ItemsHost is null");
	auto* raw = host.get();
	_itemsHost = raw;
	if (_templateItemsPresenter)
	{
		cui::framework::XamlAccess::SetLogicalParent(*raw, nullptr);
		if (!raw->GetTemplatedParent())
			cui::framework::XamlAccess::SetTemplatedParent(*raw, this);
		try
		{
			_templateItemsPresenter->SetItemsHost(std::move(host));
			cui::framework::XamlAccess::SetLogicalParent(*raw, nullptr);
		}
		catch (...)
		{
			_itemsHost = _templateItemsPresenter->GetItemsHost() == raw
				? raw : nullptr;
			throw;
		}
	}
	else if (_controlTemplateRoot)
	{
		if (!raw->GetTemplatedParent())
			cui::framework::XamlAccess::SetTemplatedParent(*raw, this);
		cui::framework::XamlAccess::SetLogicalParent(*raw, nullptr);
		_detachedItemsHost = std::move(host);
	}
	else
	{
		_changingItemsHost = true;
		try
		{
			if (!raw->GetTemplatedParent())
				cui::framework::XamlAccess::SetTemplatedParent(*raw, this);
			cui::framework::TreeAccess::AddOwnedVisualChild(
				*this, std::move(host), nullptr);
			_changingItemsHost = false;
		}
		catch (...)
		{
			_changingItemsHost = false;
			_itemsHost = ContainsControl(raw) ? raw : nullptr;
			throw;
		}
	}
	RefreshItemsScrollOwner();
}

void ItemsControl::ReplaceItemsHostCore(std::unique_ptr<Panel> host)
{
	if (!host) throw std::invalid_argument("ItemsControl ItemsHost is null");
	if (_controlTemplateRoot || _templateItemsPresenter)
		throw std::logic_error("cannot replace a templated ItemsHost");
	if ((_itemsHost && !_itemsHost->GetVisualChildrenView().empty())
		|| _generator.RealizedCount() != 0)
		throw std::logic_error("cannot replace a non-empty ItemsHost");

	auto previous = TakeItemsHost();
	try
	{
		PlaceItemsHost(std::move(host));
	}
	catch (...)
	{
		if (previous) PlaceItemsHost(std::move(previous));
		throw;
	}
	RequestLayout();
}

void ItemsControl::RefreshItemsScrollOwner()
{
	_scrollChanged.Disconnect();
	_itemsScrollOwner = nullptr;
	if (_templateItemsPresenter)
	{
		for (auto* current = _templateItemsPresenter->GetVisualParent();
			current && current != this;
			current = current->GetVisualParent())
		{
			if (auto* scroll = dynamic_cast<ScrollViewer*>(current))
			{
				_itemsScrollOwner = scroll;
				break;
			}
		}
	}
	if (_itemsScrollOwner)
		_scrollChanged = _itemsScrollOwner->OnScrollChanged.Subscribe(
			[this](Control*, ScrollChangedEventArgs&)
			{
				if (IsVirtualizing() && !_applyingCollectionChange)
					(void)RealizeVirtualViewport();
			});
}

bool ItemsControl::RegisterTemplateItemsPresenter(
	ItemsPresenter* presenter)
{
	if (!presenter || presenter->GetTemplatedParent() != this
		|| (_templateItemsPresenter
		&& _templateItemsPresenter != presenter)) return false;
	if (_templateItemsPresenter == presenter) return true;
	auto host = TakeItemsHost();
	if (!host) return false;
	_templateItemsPresenter = presenter;
	_itemsPresenterParentChanged =
		cui::framework::TreeAccess::SubscribeVisualParentChanged(*presenter,
		[this](Control*, Control*, Control*)
		{
			RefreshItemsScrollOwner();
			if (IsVirtualizing()) (void)RealizeVirtualViewport();
		});
	try
	{
		PlaceItemsHost(std::move(host));
		return true;
	}
	catch (...)
	{
		if (presenter->GetItemsHost() == _itemsHost)
		{
			RefreshItemsScrollOwner();
			throw;
		}
		_itemsPresenterParentChanged.Disconnect();
		_templateItemsPresenter = nullptr;
		throw;
	}
}

Control* ItemsControl::SetControlTemplateRoot(
	std::unique_ptr<Control> value)
{
	if (!value)
	{
		(void)DetachVisualChildTemplateRoot();
		return nullptr;
	}
	if (_controlTemplateRoot)
		throw std::logic_error("ItemsControl already owns a ControlTemplate root");
	if (!_templateItemsPresenter)
	{
		auto host = TakeItemsHost();
		if (!host) throw std::logic_error("ItemsControl lost its ItemsHost");
		_detachedItemsHost = std::move(host);
	}
	_controlTemplateRoot = value.get();
	_changingTemplateInfrastructure = true;
	try
	{
		cui::framework::TreeAccess::AddOwnedVisualChild(
			*this, std::move(value), nullptr);
		_changingTemplateInfrastructure = false;
	}
	catch (...)
	{
		_changingTemplateInfrastructure = false;
		_controlTemplateRoot = nullptr;
		if (_detachedItemsHost)
			PlaceItemsHost(std::move(_detachedItemsHost));
		OnControlTemplatePresentationChanged();
		throw;
	}
	RefreshItemsScrollOwner();
	MarkControlTemplateRootAttached();
	OnControlTemplatePresentationChanged();
	RequestLayout();
	InvalidateVisual();
	return _controlTemplateRoot;
}

std::unique_ptr<Control> ItemsControl::DetachVisualChildTemplateRoot()
{
	if (!_controlTemplateRoot)
	{
		auto host = TakeItemsHost();
		_itemsPresenterParentChanged.Disconnect();
		_templateItemsPresenter = nullptr;
		ClearDeclarativeTemplateScope();
		if (host) PlaceItemsHost(std::move(host));
		MarkControlTemplateRootDetached();
		OnControlTemplatePresentationChanged();
		RequestLayout();
		InvalidateVisual();
		return {};
	}
	_changingTemplateInfrastructure = true;
	std::unique_ptr<Control> root;
	try
	{
		root = DetachVisualChild(_controlTemplateRoot);
		_changingTemplateInfrastructure = false;
	}
	catch (...)
	{
		_changingTemplateInfrastructure = false;
		throw;
	}
	auto host = TakeItemsHost();
	_itemsPresenterParentChanged.Disconnect();
	_templateItemsPresenter = nullptr;
	_controlTemplateRoot = nullptr;
	ClearTemplateOwner(root.get(), this);
	ClearDeclarativeTemplateScope();
	if (host) PlaceItemsHost(std::move(host));
	MarkControlTemplateRootDetached();
	OnControlTemplatePresentationChanged();
	RequestLayout();
	InvalidateVisual();
	return root;
}

void ItemsControl::SetItemsPanel(ItemsPanelTemplateReference value)
{
	if (_itemsPanel == value) return;
	const auto& definition = value ? *value.Get() : DefaultItemsPanel();
	if (!IsValidItemsPanel(definition))
	{
		_lastTemplateError = L"ItemsPanelTemplate 配置无效；VirtualizingStackPanel 需要正数 ItemHeight 且只支持 Vertical。";
		return;
	}
	(void)ReplaceItemsHost(std::move(value));
}

bool ItemsControl::ReplaceItemsHost(ItemsPanelTemplateReference value)
{
	const auto previousPanel = _itemsPanel;
	_itemsPanel = std::move(value);
	std::unique_ptr<Panel> replacement;
	try
	{
		replacement = CreateItemsHost();
	}
	catch (...)
	{
		_itemsPanel = previousPanel;
		_lastTemplateError = L"ItemsPanelTemplate 无法创建 ItemsHost。";
		return false;
	}

	auto oldGenerator = std::move(_generator);
	auto oldHost = TakeItemsHost();
	if (!oldHost)
	{
		_itemsPanel = previousPanel;
		_generator = std::move(oldGenerator);
		_lastTemplateError = L"ItemsHost 所有权无效。";
		return false;
	}
	PlaceItemsHost(std::move(replacement));
	if (!_authoredItems.empty())
	{
		try
		{
			for (auto* item : _authoredItems)
			{
				auto owner = oldHost->DetachVisualChild(item);
				if (!owner)
					throw std::logic_error(
						"ItemsControl authored item ownership is invalid");
				cui::framework::TreeAccess::AddOwnedVisualChild(
					*_itemsHost, std::move(owner), this);
			}
			_lastTemplateError.clear();
			RequestLayout();
			InvalidateVisual();
			return true;
		}
		catch (...)
		{
			// Reassemble the previous host in authored order before restoring it.
			// Pointer storage is already reserved and remains the source of truth.
			std::vector<std::unique_ptr<Control>> owners;
			owners.reserve(_authoredItems.size());
			for (auto* item : _authoredItems)
			{
				std::unique_ptr<Control> owner;
				if (item->GetVisualParent() == _itemsHost)
					owner = _itemsHost->DetachVisualChild(item);
				else if (item->GetVisualParent() == oldHost.get())
					owner = oldHost->DetachVisualChild(item);
				if (!owner) continue;
				owners.push_back(std::move(owner));
			}
			for (auto& owner : owners)
				cui::framework::TreeAccess::AddOwnedVisualChild(
					*oldHost, std::move(owner), this);
			auto failedHost = TakeItemsHost();
			_itemsPanel = previousPanel;
			PlaceItemsHost(std::move(oldHost));
			_generator = std::move(oldGenerator);
			_lastTemplateError = L"ItemsPanelTemplate 无法迁移 authored Items。";
			return false;
		}
	}

	if (RebuildGeneratedItems()) return true;
	const auto error = _lastTemplateError;
	auto failedHost = TakeItemsHost();
	_itemsPanel = previousPanel;
	PlaceItemsHost(std::move(oldHost));
	_generator = std::move(oldGenerator);
	_lastTemplateError = error;
	return false;
}

void ItemsControl::SetItemsSource(BindingListReference value)
{
	if (_itemsSource == value) return;
	if (value && !_authoredItems.empty())
		throw std::logic_error(
			"ItemsControl Items and ItemsSource cannot both be populated");
	if (IsItemsSourceUpdateInProgress())
		throw std::logic_error(
			"ItemsControl does not support reentrant ItemsSource changes");
	BindingListReference candidateSnapshot;
	std::wstring snapshotError;
	if (!TryCaptureItemsSourceSnapshot(
		value, candidateSnapshot, snapshotError))
	{
		_lastTemplateError = std::move(snapshotError);
		return;
	}
	auto candidateHost = CreateItemsHost();
	auto derivedState = CaptureItemsSourceTransactionState();
	++_itemsSourceUpdateDepth;
	const auto previous = _itemsSource;
	const auto previousSnapshot = _materializedItemsSourceSnapshot;
	// Keep the old subscriptions alive until the candidate source and its
	// complete generated tree commit. This makes a failed/throwing template
	// rollback allocation-free and preserves the previous live view.
	auto previousItemsSourceChanged = std::move(_itemsSourceChanged);
	auto previousGroupsChanged = std::move(_groupsChanged);
	_itemsSource = std::move(value);
	auto restorePrevious = [&]() noexcept
	{
		_itemsSourceChanged.Disconnect();
		_groupsChanged.Disconnect();
		_itemsSource = previous;
		_itemsSourceChanged = std::move(previousItemsSourceChanged);
		_groupsChanged = std::move(previousGroupsChanged);
	};
	auto restoreDerivedState = [&]() noexcept
	{
		if (!derivedState) return;
		RestoreItemsSourceTransactionState(*derivedState);
		derivedState.reset();
	};
	try
	{
		if (_itemsSource)
		{
			auto* sourceIdentity = _itemsSource.Get();
			_itemsSourceChanged = _itemsSource.Get()->SubscribeChanged(
				[this, sourceIdentity](const CollectionChangedEventArgs& change)
				{
					if (_itemsSource.Get() == sourceIdentity
						&& !IsItemsSourceUpdateInProgress())
						HandleItemsSourceChange(change);
				});
			if (auto* grouped = dynamic_cast<IBindingListGroupView*>(
				_itemsSource.Get()))
				_groupsChanged = grouped->SubscribeGroupsChanged(
					[this, sourceIdentity]
					{
						if (_itemsSource.Get() == sourceIdentity
							&& !IsItemsSourceUpdateInProgress())
							RefreshGroupHeaders();
					});
		}
	}
	catch (...)
	{
		restorePrevious();
		restoreDerivedState();
		--_itemsSourceUpdateDepth;
		throw;
	}
	// A non-standard IBindingList may publish synchronously from SubscribeChanged.
	// Those notifications are suppressed above; recapture once after all
	// subscriptions exist so the candidate tree observes their final state.
	try
	{
		if (!TryCaptureItemsSourceSnapshot(
			_itemsSource, candidateSnapshot, snapshotError))
		{
			restorePrevious();
			restoreDerivedState();
			--_itemsSourceUpdateDepth;
			_lastTemplateError = std::move(snapshotError);
			return;
		}
	}
	catch (...)
	{
		restorePrevious();
		restoreDerivedState();
		--_itemsSourceUpdateDepth;
		throw;
	}

	// Materialize against an off-tree host. The currently committed host and
	// generator stay alive until both generation and the derived source-change
	// hook have committed, so a throwing hook cannot strand a new source above
	// the old visual tree (or destroy the tree needed for rollback).
	auto* previousHostRaw = _itemsHost;
	auto previousGenerator = std::move(_generator);
	_generator = ItemContainerGenerator{};
	_itemsHost = candidateHost.get();
	bool rebuilt = false;
	try
	{
		rebuilt = RebuildGeneratedItems();
	}
	catch (...)
	{
		_itemsHost = previousHostRaw;
		_generator = std::move(previousGenerator);
		_materializedItemsSourceSnapshot = previousSnapshot;
		restorePrevious();
		restoreDerivedState();
		--_itemsSourceUpdateDepth;
		throw;
	}
	if (!rebuilt)
	{
		const auto error = _lastTemplateError;
		_itemsHost = previousHostRaw;
		_generator = std::move(previousGenerator);
		_materializedItemsSourceSnapshot = previousSnapshot;
		restorePrevious();
		restoreDerivedState();
		--_itemsSourceUpdateDepth;
		_lastTemplateError = error;
		return;
	}

	std::unique_ptr<Panel> previousHost;
	try
	{
		_itemsHost = previousHostRaw;
		previousHost = TakeItemsHost();
		if (!previousHost)
			throw std::logic_error("ItemsControl lost its committed ItemsHost");
		PlaceItemsHost(std::move(candidateHost));
	}
	catch (...)
	{
		// A placement failure is rare (normally allocation failure), but the
		// logical source and generator still have the same strong guarantee.
		if (previousHost)
		{
			try
			{
				if (_itemsHost && _itemsHost != previousHost.get())
				{
					auto failedHost = TakeItemsHost();
					(void)failedHost;
				}
				PlaceItemsHost(std::move(previousHost));
			}
			catch (...) {}
		}
		else _itemsHost = previousHostRaw;
		_generator = std::move(previousGenerator);
		_materializedItemsSourceSnapshot = previousSnapshot;
		restorePrevious();
		restoreDerivedState();
		--_itemsSourceUpdateDepth;
		throw;
	}

	const auto candidate = _itemsSource;
	try
	{
		OnItemsSourceChanged(previous, candidate);
	}
	catch (...)
	{
		const auto failure = std::current_exception();
		try
		{
			auto failedHost = TakeItemsHost();
			(void)failedHost;
			PlaceItemsHost(std::move(previousHost));
		}
		catch (...) {}
		_generator = std::move(previousGenerator);
		_materializedItemsSourceSnapshot = previousSnapshot;
		restorePrevious();
		// Derived controls may have committed secondary state (for example a
		// current-view subscription) before throwing. Give them the symmetric
		// notification after the base state is restored; preserve the original
		// exception if even this best-effort rollback hook fails.
		try { OnItemsSourceChanged(candidate, previous); }
		catch (...) {}
		restoreDerivedState();
		--_itemsSourceUpdateDepth;
		std::rethrow_exception(failure);
	}
	_materializedItemsSourceSnapshot = std::move(candidateSnapshot);
	--_itemsSourceUpdateDepth;
}

void ItemsControl::SetCustomProjectionItemsSource(BindingListReference value)
{
	if (value && !_authoredItems.empty())
		throw std::logic_error(
			"ItemsControl Items and ItemsSource cannot both be populated");
	_itemsSource = std::move(value);
}

void ItemsControl::HandleItemsSourceChange(
	const CollectionChangedEventArgs& change)
{
	const auto failedSource = _itemsSource;
	PreparedMaterializedSnapshotChange preparedSnapshot;
	std::wstring snapshotError;
	std::unique_ptr<ItemsSourceTransactionState> derivedState;
	++_itemsSourceUpdateDepth;
	auto restoreDerivedState = [&]() noexcept
	{
		if (!derivedState) return;
		RestoreItemsSourceTransactionState(*derivedState);
		derivedState.reset();
	};
	auto rollbackToMaterializedSnapshot = [&]
	{
		_itemsSourceChanged.Disconnect();
		_groupsChanged.Disconnect();
		_itemsSource = _materializedItemsSourceSnapshot;
		try { OnItemsSourceChanged(failedSource, _itemsSource); }
		catch (...) {}
	};
	try
	{
		// A live collection has already mutated before publishing this callback.
		// Capture inside the guarded region so even token allocation failure pins
		// the control back to its last materialized snapshot.
		derivedState = CaptureItemsSourceTransactionState();
		const bool prepared = TryPrepareMaterializedSnapshotChange(
			_itemsSource, _materializedItemsSourceSnapshot,
			change, preparedSnapshot, snapshotError);
		const bool rebuilt = prepared
			&& (ApplyCollectionChange(change) || RebuildGeneratedItems());
		if (rebuilt)
		{
			if (preparedSnapshot.Replace)
				_materializedItemsSourceSnapshot =
					std::move(preparedSnapshot.Replacement);
			else
			{
				auto* snapshot =
					dynamic_cast<MaterializedItemsSourceSnapshot*>(
						_materializedItemsSourceSnapshot.Get());
				if (!snapshot)
					throw std::logic_error(
						"ItemsControl materialized snapshot is unavailable");
				snapshot->Apply(
					change,
					std::move(preparedSnapshot.ChangedItems),
					std::move(preparedSnapshot.Groups));
			}
			derivedState.reset();
		}
		else
		{
			const auto error = prepared
				? _lastTemplateError : snapshotError;
			rollbackToMaterializedSnapshot();
			restoreDerivedState();
			_lastTemplateError = error;
		}
		--_itemsSourceUpdateDepth;
	}
	catch (...)
	{
		const auto failure = std::current_exception();
		const auto error = _lastTemplateError;
		rollbackToMaterializedSnapshot();
		// ApplyCollectionChange prepares template visuals before touching the
		// committed tree, but virtual/layout and derived rebuilt callbacks are
		// extensible and may throw after commit. Re-materialize the immutable old
		// snapshot so even that exceptional boundary converges source and visuals.
		try { (void)RebuildGeneratedItems(); }
		catch (...) {}
		restoreDerivedState();
		_lastTemplateError = error;
		--_itemsSourceUpdateDepth;
		std::rethrow_exception(failure);
	}
}

void ItemsControl::SetItemTemplate(ItemTemplateReference value)
{
	if (_itemTemplate == value) return;
	const auto previous = _itemTemplate;
	_itemTemplate = std::move(value);
	if (RebuildGeneratedItems()) return;
	const auto error = _lastTemplateError;
	_itemTemplate = previous;
	_lastTemplateError = error;
}

void ItemsControl::SetGroupStyle(GroupStyleReference value)
{
	if (_groupStyle == value) return;
	const auto previous = _groupStyle;
	_groupStyle = std::move(value);
	if (RebuildGeneratedItems()) return;
	const auto error = _lastTemplateError;
	_groupStyle = previous;
	(void)RebuildGeneratedItems();
	_lastTemplateError = error;
}

void ItemsControl::SetDisplayMemberPath(std::wstring value)
{
	if (_displayMemberPath == value) return;
	_displayMemberPath = std::move(value);
	if (_itemsSource && !_itemTemplate) (void)RebuildGeneratedItems();
}

void ItemsControl::SetItemContainerStyle(std::wstring value)
{
	if (_itemContainerStyle == value) return;
	const auto previous = _itemContainerStyle;
	_itemContainerStyle = std::move(value);
	if (ApplyItemContainerStyle()) return;
	const auto error = _lastTemplateError.empty()
		? L"ItemContainerStyle 无法应用到项容器。" : _lastTemplateError;
	_itemContainerStyle = previous;
	(void)ApplyItemContainerStyle();
	_lastTemplateError = error;
}

void ItemsControl::SetGeneratedContainerInitializer(
	GeneratedContainerInitializer value)
{
	if (!_generatedContainerInitializer && !value)
		return;
	auto previous = std::move(_generatedContainerInitializer);
	_generatedContainerInitializer = std::move(value);
	if (RebuildGeneratedItems()) return;
	const auto error = _lastTemplateError;
	_generatedContainerInitializer = std::move(previous);
	(void)RebuildGeneratedItems();
	_lastTemplateError = error;
}

bool ItemsControl::InitializeGeneratedContainer(Control& container)
{
	if (!_generatedContainerInitializer) return true;
	std::wstring error;
	if (_generatedContainerInitializer(container, &error)) return true;
	_lastTemplateError = error.empty()
		? L"生成项容器的 XAML 类型身份初始化失败。" : std::move(error);
	return false;
}

bool ItemsControl::ApplyItemContainerStyle()
{
	for (size_t index = 0; index < ItemCount(); ++index)
	{
		auto* container = GetGeneratedItem(index);
		if (!container) continue;
		cui::framework::StyleAccess::SetResourceKey(
			*container, _itemContainerStyle);
		if (!cui::framework::StyleAccess::HasVisibleStyleRules(*container))
			continue;
		if (!cui::framework::StyleAccess::Refresh(*container, true)) return false;
	}
	return true;
}

Control* ItemsControl::UnwrapGeneratedItem(Control* visual) noexcept
{
	if (auto* grouped = dynamic_cast<GroupedItemHost*>(visual))
		return grouped->Item();
	return visual;
}

Control* ItemsControl::GetGeneratedItem(size_t index) const noexcept
{
	if (!_itemsSource)
		return index < _authoredItems.size() ? _authoredItems[index] : nullptr;
	return UnwrapGeneratedItem(_generator.GetRealized(index));
}

size_t ItemsControl::GeneratedItemCount() const noexcept
{
	return _itemsSource ? _generator.RealizedCount() : _authoredItems.size();
}

size_t ItemsControl::ItemCount() const noexcept
{
	return _itemsSource ? _itemsSource.Get()->Count() : _authoredItems.size();
}

ItemsControl::AuthoredItemsUpdateScope::AuthoredItemsUpdateScope(
	ItemsControl& owner) noexcept
	: _owner(&owner)
{
	_owner->BeginAuthoredItemsUpdate();
}

ItemsControl::AuthoredItemsUpdateScope::AuthoredItemsUpdateScope(
	AuthoredItemsUpdateScope&& other) noexcept
	: _owner(std::exchange(other._owner, nullptr))
{
}

ItemsControl::AuthoredItemsUpdateScope&
ItemsControl::AuthoredItemsUpdateScope::operator=(
	AuthoredItemsUpdateScope&& other) noexcept
{
	if (this == &other) return *this;
	if (_owner) _owner->EndAuthoredItemsUpdate();
	_owner = std::exchange(other._owner, nullptr);
	return *this;
}

ItemsControl::AuthoredItemsUpdateScope::~AuthoredItemsUpdateScope()
{
	if (_owner) _owner->EndAuthoredItemsUpdate();
}

ItemsControl::AuthoredItemsUpdateScope
ItemsControl::DeferAuthoredItemsChanges() noexcept
{
	return AuthoredItemsUpdateScope(*this);
}

void ItemsControl::BeginAuthoredItemsUpdate() noexcept
{
	++_authoredItemsUpdateDepth;
}

void ItemsControl::EndAuthoredItemsUpdate() noexcept
{
	if (_authoredItemsUpdateDepth == 0) return;
	--_authoredItemsUpdateDepth;
	if (_authoredItemsUpdateDepth != 0 || !_authoredItemsChangedPending) return;
	_authoredItemsChangedPending = false;
	OnAuthoredItemsChanged();
	ConfigureVirtualHost();
	RequestLayout();
	InvalidateVisual();
}

void ItemsControl::NotifyAuthoredItemsChanged()
{
	if (_authoredItemsUpdateDepth != 0)
	{
		_authoredItemsChangedPending = true;
		return;
	}
	OnAuthoredItemsChanged();
	ConfigureVirtualHost();
	RequestLayout();
	InvalidateVisual();
}

Control* ItemsControl::InsertItemControl(size_t index, Control* item)
{
	if (!item) throw std::invalid_argument("ItemsControl item is null");
	if (_itemsSource)
		throw std::logic_error(
			"ItemsControl Items and ItemsSource cannot both be populated");
	if (!_itemsHost) throw std::logic_error("ItemsControl has no ItemsHost");
	if (index > _authoredItems.size())
		throw std::out_of_range("ItemsControl item index is out of range");
	std::string itemError;
	if (!ValidateAuthoredItemControl(*item, itemError))
		throw std::invalid_argument(itemError.empty()
			? "ItemsControl rejected authored item" : itemError);
	_authoredItems.reserve(_authoredItems.size() + 1);
	try
	{
		cui::framework::TreeAccess::InsertVisualChild(
			*_itemsHost, static_cast<int>(index), item, this);
	}
	catch (...)
	{
		auto detached = _itemsHost->DetachVisualChild(item);
		if (detached.get() == item) (void)detached.release();
		if (item->GetLogicalParent() == this)
			cui::framework::XamlAccess::SetLogicalParent(*item, nullptr);
		throw;
	}
	_authoredItems.insert(_authoredItems.begin() + index, item);
	NotifyAuthoredItemsChanged();
	return item;
}

Control* ItemsControl::InsertItemControl(
	size_t index, std::unique_ptr<Control> item)
{
	if (!item) throw std::invalid_argument("ItemsControl item is null");
	auto* raw = item.get();
	try
	{
		(void)InsertItemControl(index, raw);
	}
	catch (...)
	{
		if (raw->GetVisualParent() == _itemsHost) (void)item.release();
		throw;
	}
	(void)item.release();
	return raw;
}

Control* ItemsControl::AddItemControl(std::unique_ptr<Control> item)
{
	return InsertItemControl(_authoredItems.size(), std::move(item));
}

Control* ItemsControl::AdoptItemControl(Control* item)
{
	return InsertItemControl(_authoredItems.size(), item);
}

std::unique_ptr<Control> ItemsControl::DetachItemControlAt(size_t index)
{
	if (index >= _authoredItems.size() || !_itemsHost) return {};
	auto* raw = _authoredItems[index];
	auto result = _itemsHost->DetachVisualChild(raw);
	if (!result) return {};
	_authoredItems.erase(_authoredItems.begin() + index);
	cui::framework::XamlAccess::SetLogicalParent(*raw, nullptr);
	NotifyAuthoredItemsChanged();
	return result;
}

std::unique_ptr<Control> ItemsControl::DetachItemControl(Control* item)
{
	const auto found = std::find(_authoredItems.begin(), _authoredItems.end(), item);
	return found == _authoredItems.end() ? std::unique_ptr<Control>{}
		: DetachItemControlAt(static_cast<size_t>(
			std::distance(_authoredItems.begin(), found)));
}

bool ItemsControl::RemoveItemControlAt(size_t index)
{
	return static_cast<bool>(DetachItemControlAt(index));
}

bool ItemsControl::RemoveItemControl(Control* item)
{
	return static_cast<bool>(DetachItemControl(item));
}

bool ItemsControl::MoveItemControl(size_t oldIndex, size_t newIndex)
{
	if (oldIndex >= _authoredItems.size() || newIndex >= _authoredItems.size())
		return false;
	if (oldIndex == newIndex) return true;
	if (!_itemsHost) return false;
	_itemsHost->MoveVisualChild(
		static_cast<int>(oldIndex), static_cast<int>(newIndex));
	auto* item = _authoredItems[oldIndex];
	_authoredItems.erase(_authoredItems.begin() + oldIndex);
	_authoredItems.insert(_authoredItems.begin() + newIndex, item);
	NotifyAuthoredItemsChanged();
	return true;
}

void ItemsControl::ClearItemControls()
{
	std::vector<std::unique_ptr<Control>> removed;
	removed.reserve(_authoredItems.size());
	auto update = DeferAuthoredItemsChanges();
	while (!_authoredItems.empty())
		removed.push_back(DetachItemControlAt(_authoredItems.size() - 1));
}

Control* ItemsControl::GetAuthoredItem(size_t index) const noexcept
{
	return index < _authoredItems.size() ? _authoredItems[index] : nullptr;
}

bool ItemsControl::IsGroupingActive() const noexcept
{
	if (!_groupStyle || !_itemsSource) return false;
	const auto* grouped = dynamic_cast<const IBindingListGroupView*>(
		_itemsSource.Get());
	return grouped && !grouped->Groups().empty();
}

ItemsControl::PreparedGroupHeaders ItemsControl::BuildGroupHeaders(
	size_t index,
	const BindingSourceReference& item)
{
	PreparedGroupHeaders result;
	if (!IsGroupingActive()) return result;
	const auto* grouped = dynamic_cast<const IBindingListGroupView*>(
		_itemsSource.Get());
	if (!grouped) return result;
	for (const auto& group : grouped->Groups())
	{
		if (group.StartIndex != index) continue;
		auto groupItems = std::make_shared<ObservableBindingList>(
			_itemsSource.Get()->ItemTypeName());
		for (size_t offset = 0; offset < group.ItemCount; ++offset)
		{
			BindingSourceReference member;
			if (_itemsSource.Get()->TryGetItem(group.StartIndex + offset, member)
				&& member) groupItems->Items.push_back(std::move(member));
		}
		const bool isBottomLevel = std::none_of(
			grouped->Groups().begin(), grouped->Groups().end(),
			[&](const BindingListGroup& candidate)
			{
				return candidate.Level == group.Level + 1
					&& candidate.StartIndex >= group.StartIndex
					&& candidate.StartIndex < group.StartIndex + group.ItemCount;
			});
		auto context = std::make_shared<ObservableObject>();
		auto aggregates = std::make_shared<ObservableObject>();
		for (const auto& [name, value] : group.Aggregates)
			(void)aggregates->DefineProperty(name, value, true, false, true);
		(void)context->DefineProperty(L"Key", group.Key.ToString(), true, false, true);
		(void)context->DefineProperty(L"Name", group.Key.ToString(), true, false, true);
		(void)context->DefineProperty(L"PropertyName", group.PropertyName,
			true, false, true);
		(void)context->DefineProperty(L"Level", static_cast<int>(group.Level),
			true, false, true);
		(void)context->DefineProperty(L"StartIndex",
			static_cast<long long>(group.StartIndex), true, false, true);
		(void)context->DefineProperty(L"ItemCount",
			static_cast<long long>(group.ItemCount), true, false, true);
		(void)context->DefineProperty(L"IsBottomLevel", isBottomLevel,
			true, false, true);
		(void)context->DefineProperty(L"FirstItem", item, true, false, true);
		(void)context->DefineProperty(L"Items",
			BindingListReference(groupItems), true, false, true);
		(void)context->DefineProperty(L"Aggregates",
			BindingSourceReference(aggregates), true, false, true);
		BindingSourceReference contextReference(context);
		std::unique_ptr<Control> header;
		if (_groupStyle.Get()->HeaderTemplate)
			header = _groupStyle.Get()->HeaderTemplate.Get()->Build(
				contextReference, index, &_lastTemplateError);
		else
		{
			auto label = std::make_unique<Label>();
			label->Text = group.Key.ToString();
			header = std::move(label);
		}
		if (!header)
		{
			if (_lastTemplateError.empty())
				_lastTemplateError = L"GroupStyle HeaderTemplate 未生成视觉根。";
			return {};
		}
		result.Visuals.push_back(std::move(header));
		result.Contexts.push_back(std::move(contextReference));
	}
	return result;
}

void ItemsControl::RefreshGroupHeaders()
{
	_lastTemplateError.clear();
	const bool active = IsGroupingActive();
	if (active
		&& ((!IsVirtualizing() && _generator.RealizedCount() != ItemCount())
			|| (IsVirtualizing() && ItemCount() != 0
				&& _generator.RealizedCount() == 0)))
	{
		(void)RebuildGeneratedItems();
		return;
	}
	for (const auto& [index, realized] : _generator.RealizedItems())
	{
		const bool wrapped = dynamic_cast<GroupedItemHost*>(
			realized.Visual) != nullptr;
		if (wrapped != active)
		{
			(void)RebuildGeneratedItems();
			return;
		}
	}
	if (!active) return;
	struct Pending final
	{
		GroupedItemHost* Host = nullptr;
		std::vector<std::unique_ptr<Control>> Headers;
		std::vector<BindingSourceReference> Contexts;
	};
	std::vector<Pending> pending;
	pending.reserve(_generator.RealizedCount());
	for (const auto& [index, realized] : _generator.RealizedItems())
	{
		BindingSourceReference item;
		if (!_itemsSource.Get()->TryGetItem(index, item) || !item)
		{
			_lastTemplateError = L"分组 ItemsSource 无法读取项。";
			return;
		}
		auto headers = BuildGroupHeaders(index, item);
		if (!_lastTemplateError.empty()) return;
		pending.push_back({
			static_cast<GroupedItemHost*>(realized.Visual),
			std::move(headers.Visuals), std::move(headers.Contexts) });
	}
	for (auto& item : pending)
		item.Host->SetHeaders(
			std::move(item.Headers), std::move(item.Contexts),
			IsVirtualizing() ? VirtualizedGroupHeaderEstimate : 0.0f,
			IsVirtualizing() ? EffectiveItemsPanel().ItemHeight : 0.0f);
	ConfigureVirtualHost();
	_itemsHost->InvalidateLayout();
	RequestLayout();
	InvalidateVisual();
}

bool ItemsControl::PrepareGeneratedItem(
	size_t index,
	PreparedItem& output,
	bool allowRecycle)
{
	output = {};
	output.Index = index;
	ItemContainerGenerator::RecycledItem recycled;
	if (allowRecycle && _generator.TakeRecycled(index, recycled))
	{
		output.Visual = std::move(recycled.Visual);
		output.Observation = std::move(recycled.Observation);
		output.WasRecycled = true;
		auto* grouped = dynamic_cast<GroupedItemHost*>(output.Visual.get());
		if (!IsGroupingActive())
		{
			if (grouped) output.Visual = grouped->TakeItem();
			return output.Visual != nullptr;
		}
		BindingSourceReference recycledItem;
		if (!_itemsSource || !_itemsSource.Get()->TryGetItem(index, recycledItem)
			|| !recycledItem)
		{
			_lastTemplateError = L"ItemsSource 无法读取回收项索引 "
				+ std::to_wstring(index) + L"。";
			return false;
		}
		auto headers = BuildGroupHeaders(index, recycledItem);
		if (!_lastTemplateError.empty()) return false;
		if (grouped)
			grouped->SetHeaders(
				std::move(headers.Visuals), std::move(headers.Contexts),
				IsVirtualizing() ? VirtualizedGroupHeaderEstimate : 0.0f,
				IsVirtualizing() ? EffectiveItemsPanel().ItemHeight : 0.0f);
		else
		{
			auto host = std::make_unique<GroupedItemHost>(
				std::move(output.Visual), this);
			host->SetHeaders(
				std::move(headers.Visuals), std::move(headers.Contexts),
				IsVirtualizing() ? VirtualizedGroupHeaderEstimate : 0.0f,
				IsVirtualizing() ? EffectiveItemsPanel().ItemHeight : 0.0f);
			output.Visual = std::move(host);
		}
		return true;
	}

	BindingSourceReference item;
	if (!_itemsSource || !_itemsSource.Get()->TryGetItem(index, item) || !item)
	{
		_lastTemplateError = L"ItemsSource 无法读取索引 "
			+ std::to_wstring(index) + L"。";
		return false;
	}
	auto visual = BuildGeneratedItem(item, index, output.Observation);
	if (!visual)
	{
		if (_lastTemplateError.empty())
			_lastTemplateError = L"项容器未生成视觉根。";
		return false;
	}
	if (!InitializeGeneratedContainer(*visual)) return false;
	if (IsGroupingActive())
	{
		auto headers = BuildGroupHeaders(index, item);
		if (!_lastTemplateError.empty()) return false;
		auto grouped = std::make_unique<GroupedItemHost>(
			std::move(visual), this);
		grouped->SetHeaders(
			std::move(headers.Visuals), std::move(headers.Contexts),
			IsVirtualizing() ? VirtualizedGroupHeaderEstimate : 0.0f,
			IsVirtualizing() ? EffectiveItemsPanel().ItemHeight : 0.0f);
		visual = std::move(grouped);
	}
	output.Visual = std::move(visual);
	return true;
}

std::unique_ptr<Control> ItemsControl::BuildGeneratedItem(
	const BindingSourceReference& item,
	size_t index,
	BindingPathObservation& observation)
{
	observation = {};
	auto presenter = std::make_unique<ContentPresenter>();
	(void)cui::framework::DependencyPropertyAccess::SetValue(
		*presenter, L"VerticalAlignment", BindingValue(VerticalAlignment::Top),
		DependencyPropertyValueSource::Theme);
	cui::framework::StyleAccess::SetResourceKey(
		*presenter, _itemContainerStyle);
	presenter->SetContentTypeName(_itemTemplate
		? _itemTemplate.Get()->DataTypeName()
		: (_itemsSource ? _itemsSource.Get()->ItemTypeName() : std::wstring{}));
	presenter->SetDisplayMemberPath(_displayMemberPath);
	presenter->SetContentTemplate(_itemTemplate);
	try
	{
		presenter->SetContent(BindingValue(item));
	}
	catch (...)
	{
		// ContentPresenter owns the template diagnostic buffer.  Preserve it on
		// the ItemsControl transaction before the temporary presenter unwinds.
		_lastTemplateError = presenter->LastTemplateError();
		throw;
	}
	if (!presenter->LastTemplateError().empty())
	{
		_lastTemplateError = presenter->LastTemplateError();
		return {};
	}
	return WrapGeneratedItem(std::move(presenter), item, index);
}

void ItemsControl::AttachPreparedItem(PreparedItem&& item)
{
	if (!_itemsHost || !item.Visual) return;
	const auto index = item.Index;
	auto* visual = item.Visual.get();
	if (auto* grouped = dynamic_cast<GroupedItemHost*>(visual))
	{
		cui::framework::TreeAccess::AddOwnedVisualChild(
			*_itemsHost, std::move(item.Visual), nullptr);
		// GroupedItemHost is presentation infrastructure.  The item container
		// remains the ItemsControl logical child just as it does without a
		// GroupStyle; otherwise ListBoxItem cannot resolve its owning Selector.
		if (auto* logicalItem = grouped->Item())
			cui::framework::XamlAccess::SetLogicalParent(*logicalItem, this);
	}
	else
		cui::framework::TreeAccess::AddOwnedVisualChild(
			*_itemsHost, std::move(item.Visual), this);
	// Generated containers may be realized after the document-wide style pass.
	// Re-resolve their keyed/implicit style only after the live inheritance
	// route exists, so lazy virtualization has the same template semantics as
	// containers materialized with the original XAML tree.
	if (!cui::framework::StyleAccess::Theme(*visual))
		if (const auto theme = cui::framework::StyleAccess::Theme(*this))
		(void)cui::framework::StyleAccess::SetTheme(
			*visual, std::move(theme), true);
	if (!cui::framework::StyleAccess::DocumentStyles(*visual))
		if (const auto styles =
			cui::framework::StyleAccess::DocumentStyles(*this))
		(void)cui::framework::StyleAccess::SetDocumentStyles(
			*visual, std::move(styles), true);
	if (auto* virtualHost = dynamic_cast<VirtualizingItemsHost*>(_itemsHost))
		virtualHost->RegisterItem(visual, index);
	_generator.StoreRealized(
		index, visual, std::move(item.Observation));
}

void ItemsControl::ReorderRealizedChildren()
{
	if (!_itemsHost || IsVirtualizing()
		|| _generator.RealizedCount() < 2) return;
	std::vector<std::unique_ptr<Control>> ordered;
	ordered.reserve(_generator.RealizedCount());
	for (const auto& [index, item] : _generator.RealizedItems())
	{
		(void)index;
		auto detached = _itemsHost->DetachVisualChild(item.Visual);
		if (detached)
			ordered.push_back(std::move(detached));
	}
	for (auto& item : ordered)
	{
		auto* visual = item.get();
		if (auto* grouped = dynamic_cast<GroupedItemHost*>(visual))
		{
			cui::framework::TreeAccess::AddOwnedVisualChild(
				*_itemsHost, std::move(item), nullptr);
			if (auto* logicalItem = grouped->Item())
				cui::framework::XamlAccess::SetLogicalParent(
					*logicalItem, this);
		}
		else
			cui::framework::TreeAccess::AddOwnedVisualChild(
				*_itemsHost, std::move(item), this);
	}
}

void ItemsControl::ClearRealizedItems(bool keepForRecycle)
{
	if (!_itemsHost)
	{
		_generator.ClearRealized();
		return;
	}
	std::vector<size_t> indices;
	indices.reserve(_generator.RealizedCount());
	for (const auto& [index, item] : _generator.RealizedItems())
	{
		(void)item;
		indices.push_back(index);
	}
	for (const auto index : indices)
	{
		auto item = _generator.TakeRealized(index);
		auto* visual = item.Visual;
		if (auto* virtualHost = dynamic_cast<VirtualizingItemsHost*>(_itemsHost))
			virtualHost->UnregisterItem(visual);
		auto detached = _itemsHost->DetachVisualChild(visual);
		if (detached)
		{
			if (auto* logicalItem = UnwrapGeneratedItem(detached.get());
				logicalItem && logicalItem != detached.get())
				cui::framework::XamlAccess::SetLogicalParent(
					*logicalItem, nullptr);
			cui::framework::XamlAccess::SetLogicalParent(*detached, nullptr);
		}
		if (keepForRecycle && detached)
			_generator.StoreRecycled(index, {
				std::move(detached), std::move(item.Observation) });
	}
}

std::pair<size_t, size_t> ItemsControl::VirtualRangeForViewport() const noexcept
{
	auto* scroll = ItemsScrollOwner();
	if (!scroll) return ShouldRealizeVirtualItemsWithoutViewport()
		? std::pair<size_t, size_t>{ 0, ItemCount() }
		: std::pair<size_t, size_t>{ 0, 0 };
	return VirtualRangeForOffset(static_cast<float>(scroll->VerticalOffset));
}

std::pair<size_t, size_t> ItemsControl::VirtualRangeForOffset(
	float offset) const noexcept
{
	const size_t count = ItemCount();
	if (count == 0) return { 0, 0 };
	const auto& panel = EffectiveItemsPanel();
	auto* self = const_cast<ItemsControl*>(this);
	self->ConfigureVirtualHost();
	const auto* host = dynamic_cast<const VirtualizingItemsHost*>(_itemsHost);
	if (!host || host->ContentHeight() <= 0.0f) return { 0, count };
	auto* scroll = ItemsScrollOwner();
	if (!scroll) return ShouldRealizeVirtualItemsWithoutViewport()
		? std::pair<size_t, size_t>{ 0, count }
		: std::pair<size_t, size_t>{ 0, 0 };
	const auto size = const_cast<ScrollViewer*>(scroll)->GetActualSizeDip();
	const float viewport = (std::max)(1.0f, size.height);
	const float cache = panel.CacheLength * viewport;
	const size_t first = host->IndexAtOffset(
		(std::max)(0.0f, offset - cache));
	const float endOffset = (std::min)(host->ContentHeight(),
		(std::max)(0.0f, offset) + viewport + cache);
	const size_t last = endOffset >= host->ContentHeight()
		? count : (std::min)(count, host->IndexAtOffset(endOffset) + 1);
	return { first, last };
}

bool ItemsControl::RealizeVirtualRange(size_t first, size_t last)
{
	if (!IsVirtualizing() || !_itemsHost || _realizingViewport) return true;
	const size_t count = ItemCount();
	first = (std::min)(first, count);
	last = (std::clamp)(last, first, count);
	_realizingViewport = true;
	struct RealizingViewportGuard final
	{
		bool& Value;
		~RealizingViewportGuard() { Value = false; }
	} realizingGuard{ _realizingViewport };
	std::vector<PreparedItem> additions;
	for (size_t index = first; index < last; ++index)
	{
		if (_generator.ContainsRealized(index)) continue;
		PreparedItem item;
		if (!PrepareGeneratedItem(index, item))
		{
			for (auto& prepared : additions)
				if (prepared.WasRecycled)
					_generator.StoreRecycled(prepared.Index, {
						std::move(prepared.Visual),
						std::move(prepared.Observation) });
			_realizingViewport = false;
			return false;
		}
		additions.push_back(std::move(item));
	}

	std::vector<size_t> removals;
	for (const auto& [index, item] : _generator.RealizedItems())
	{
		(void)item;
		if (index < first || index >= last) removals.push_back(index);
	}
	if (additions.empty() && removals.empty())
	{
		// PreparePresentation is evaluated for every retained frame.  A stable
		// virtual range is not a tree mutation and must not invalidate measure;
		// doing so creates a render -> realization -> layout -> render loop
		// (closed ComboBox instances are the common trigger).
		TrimRecyclePool(first, last);
		return true;
	}
	for (const auto index : removals)
	{
		auto item = _generator.TakeRealized(index);
		auto* visual = item.Visual;
		if (auto* virtualHost = dynamic_cast<VirtualizingItemsHost*>(_itemsHost))
			virtualHost->UnregisterItem(visual);
		auto detached = _itemsHost->DetachVisualChild(visual);
		if (detached)
		{
			if (auto* logicalItem = UnwrapGeneratedItem(detached.get());
				logicalItem && logicalItem != detached.get())
				cui::framework::XamlAccess::SetLogicalParent(
					*logicalItem, nullptr);
			cui::framework::XamlAccess::SetLogicalParent(*detached, nullptr);
		}
		if (detached)
			_generator.StoreRecycled(index, {
				std::move(detached), std::move(item.Observation) });
	}
	for (auto& addition : additions)
		AttachPreparedItem(std::move(addition));
	TrimRecyclePool(first, last);
	_itemsHost->InvalidateLayout();
	RequestLayout();
	OnGeneratedItemsRealized();
	_realizingViewport = false;
	return true;
}

bool ItemsControl::RealizeVirtualViewport()
{
	if (!IsVirtualizing()) return true;
	const auto [first, last] = VirtualRangeForViewport();
	return RealizeVirtualRange(first, last);
}

void ItemsControl::TrimRecyclePool(size_t first, size_t last)
{
	const size_t visible = (std::max)(size_t{ 1 }, last - first);
	const size_t limit = (std::max)(size_t{ 4 }, visible * 2);
	const size_t center = first + visible / 2;
	auto& recycled = _generator.RecycledItems();
	while (recycled.size() > limit)
	{
		const auto low = recycled.begin();
		const auto high = std::prev(recycled.end());
		const size_t lowDistance = center > low->first
			? center - low->first : low->first - center;
		const size_t highDistance = center > high->first
			? center - high->first : high->first - center;
		recycled.erase(highDistance >= lowDistance ? high : low);
	}
}

bool ItemsControl::ApplyCollectionChange(
	const CollectionChangedEventArgs& change)
{
	const size_t newCount = ItemCount();
	if (!_itemsHost || _applyingCollectionChange
		|| !_generator.CanApply(change, newCount)) return false;

	auto* scrollOwner = ItemsScrollOwner();
	float desiredScroll = scrollOwner
		? static_cast<float>(scrollOwner->VerticalOffset) : 0.0f;
	if (IsVirtualizing() && change.OldSize != 0)
	{
		auto* virtualHost = dynamic_cast<VirtualizingItemsHost*>(_itemsHost);
		if (virtualHost)
		{
			const size_t oldAnchor = (std::min)(change.OldSize - 1,
				virtualHost->IndexAtOffset(desiredScroll));
			const float withinItem = desiredScroll
				- virtualHost->ItemTop(oldAnchor);
			auto mappedAnchor = ItemContainerGenerator::MapIndex(
				change, oldAnchor);
			if (!mappedAnchor && newCount != 0)
			{
				const size_t replacement = change.Action
					== CollectionChangeAction::Replace
					? change.NewIndex : change.OldIndex;
				mappedAnchor = (std::min)(replacement, newCount - 1);
			}
			ConfigureVirtualHost();
			desiredScroll = mappedAnchor
				? virtualHost->ItemTop(*mappedAnchor) + withinItem : 0.0f;
			const float contentHeight = virtualHost->ContentHeight();
			const float viewport = scrollOwner
				? (std::max)(1.0f, scrollOwner->GetActualSizeDip().height)
				: contentHeight;
			desiredScroll = (std::clamp)(
				desiredScroll, 0.0f,
				(std::max)(0.0f, contentHeight - viewport));
		}
	}
	else if (IsVirtualizing()) ConfigureVirtualHost();

	size_t first = 0;
	size_t last = newCount;
	if (IsVirtualizing())
		std::tie(first, last) = VirtualRangeForOffset(desiredScroll);
	std::unordered_set<size_t> occupied;
	occupied.reserve(_generator.RealizedCount());
	for (const auto& [oldIndex, item] : _generator.RealizedItems())
	{
		(void)item;
		if (const auto mapped = ItemContainerGenerator::MapIndex(
			change, oldIndex)) occupied.insert(*mapped);
	}

	std::vector<PreparedItem> additions;
	additions.reserve(last - first);
	for (size_t index = first; index < last; ++index)
	{
		if (occupied.contains(index)) continue;
		PreparedItem item;
		if (!PrepareGeneratedItem(index, item, false)) return false;
		additions.push_back(std::move(item));
	}

	_applyingCollectionChange = true;
	try
	{
		{
			ScopedLayoutUpdate layout(*this);
			OnBeforeGeneratedItemsRebuilt();
			for (const auto index
				: _generator.InvalidatedRealizedIndices(change))
			{
				auto item = _generator.TakeRealized(index);
				if (!item.Visual) continue;
				if (auto* virtualHost =
					dynamic_cast<VirtualizingItemsHost*>(_itemsHost))
					virtualHost->UnregisterItem(item.Visual);
				auto detached = _itemsHost->DetachVisualChild(item.Visual);
				if (detached)
				{
					if (auto* logicalItem =
						UnwrapGeneratedItem(detached.get());
						logicalItem && logicalItem != detached.get())
						cui::framework::XamlAccess::SetLogicalParent(
							*logicalItem, nullptr);
					cui::framework::XamlAccess::SetLogicalParent(
						*detached, nullptr);
				}
			}
			for (const auto& indexChange : _generator.Apply(change))
			{
				if (!indexChange.Visual) continue;
				if (auto* virtualHost =
					dynamic_cast<VirtualizingItemsHost*>(_itemsHost))
					virtualHost->RegisterItem(
						indexChange.Visual, indexChange.NewIndex);
				OnGeneratedItemIndexChanged(
					*indexChange.Visual,
					indexChange.OldIndex,
					indexChange.NewIndex);
			}
			ConfigureVirtualHost();
			for (auto& addition : additions)
			{
				_generator.DiscardRecycled(addition.Index);
				AttachPreparedItem(std::move(addition));
			}
			ReorderRealizedChildren();
			_itemsHost->InvalidateLayout();
			RequestLayout();
			InvalidateVisual();
		}
		UpdateLayout();
		if (IsVirtualizing() && scrollOwner)
			scrollOwner->ScrollToVerticalOffset(desiredScroll);
		OnGeneratedItemsRebuilt();
		if (IsVirtualizing())
		{
			(void)RealizeVirtualViewport();
			const auto range = VirtualRangeForViewport();
			TrimRecyclePool(range.first, range.second);
		}
		OnGeneratedItemsRealized();
		_applyingCollectionChange = false;
		return true;
	}
	catch (...)
	{
		_applyingCollectionChange = false;
		throw;
	}
}

void ItemsControl::RefreshGeneratedItem(
	const std::weak_ptr<IBindingSource>& itemIdentity)
{
	const auto item = itemIdentity.lock();
	if (!item || !_itemsSource) return;
	const int index = FindBindingListItemByValue(
		_itemsSource, std::wstring{},
		BindingValue(BindingSourceReference(item)));
	if (index < 0) return;
	const size_t itemIndex = static_cast<size_t>(index);
	if (!_generator.ContainsRealized(itemIndex))
	{
		_generator.DiscardRecycled(itemIndex);
		return;
	}
	const CollectionChangedEventArgs refresh{
		CollectionChangeAction::Replace,
		itemIndex,
		itemIndex,
		1,
		1,
		ItemCount(),
		ItemCount()
	};
	(void)ApplyCollectionChange(refresh);
}

bool ItemsControl::RebuildGeneratedItems()
{
	_lastTemplateError.clear();
	const auto sourceItemType = _itemsSource
		? _itemsSource.Get()->ItemTypeName() : std::wstring{};
	if (_itemsSource && _itemTemplate
		&& !sourceItemType.empty()
		&& !_itemTemplate.Get()->DataTypeName().empty()
		&& !EqualsTypeName(_itemTemplate.Get()->DataTypeName(),
			sourceItemType))
	{
		_lastTemplateError = L"ItemTemplate DataType 与 ItemsSource ItemType 不一致。";
		return false;
	}
	if (_groupStyle)
	{
		const auto* style = _groupStyle.Get();
		if (style->HeaderTemplate
			&& !EqualsTypeName(
				style->HeaderTemplate.Get()->DataTypeName(),
				std::wstring(CollectionViewGroupDataTypeName)))
		{
			_lastTemplateError = L"GroupStyle HeaderTemplate DataType 必须为 CollectionViewGroup。";
			return false;
		}
	}
	if (!_itemsHost)
	{
		_lastTemplateError = L"ItemsHost 不可用。";
		return false;
	}

	_generator.ClearRecycled();
	std::vector<PreparedItem> prepared;
	size_t first = 0;
	size_t last = ItemCount();
	if (IsVirtualizing())
		std::tie(first, last) = VirtualRangeForViewport();
	prepared.reserve(last - first);
	for (size_t index = first; index < last; ++index)
	{
		PreparedItem item;
		if (!PrepareGeneratedItem(index, item)) return false;
		prepared.push_back(std::move(item));
	}

	{
		ScopedLayoutUpdate layout(*this);
		OnBeforeGeneratedItemsRebuilt();
		ClearRealizedItems(false);
		_generator.SetSourceCount(ItemCount());
		ConfigureVirtualHost();
		for (auto& item : prepared) AttachPreparedItem(std::move(item));
		OnGeneratedItemsRebuilt();
		_itemsHost->InvalidateLayout();
		RequestLayout();
		InvalidateVisual();
	}
	if (auto* scrollOwner = ItemsScrollOwner())
	{
		UpdateLayout();
		double verticalOffset = scrollOwner->VerticalOffset;
		if (auto* virtualHost =
			dynamic_cast<VirtualizingItemsHost*>(_itemsHost))
		{
			const float viewport = (std::max)(
				0.0f, scrollOwner->GetActualSizeDip().height);
			verticalOffset = (std::min)(verticalOffset,
				static_cast<double>(std::ceil((std::max)(
					0.0f, virtualHost->ContentHeight() - viewport))));
		}
		scrollOwner->ScrollToVerticalOffset(verticalOffset);
		if (IsVirtualizing()) (void)RealizeVirtualViewport();
	}
	return true;
}

bool ItemsControl::BringItemIntoView(size_t index)
{
	if (index >= ItemCount()) return false;
	auto* scrollOwner = ItemsScrollOwner();
	if (IsVirtualizing())
	{
		ConfigureVirtualHost();
		auto* host = dynamic_cast<VirtualizingItemsHost*>(_itemsHost);
		if (!host) return false;
		const float top = host->ItemTop(index);
		const float bottom = top + host->ItemExtent(index);
		if (!scrollOwner)
		{
			(void)RealizeVirtualRange(0, ItemCount());
			return GetGeneratedItem(index) != nullptr;
		}
		const float viewport = (std::max)(
			1.0f, scrollOwner->GetActualSizeDip().height);
		float target = static_cast<float>(scrollOwner->VerticalOffset);
		if (top < target) target = top;
		else if (bottom > target + viewport)
			target = bottom - viewport;
		UpdateLayout();
		scrollOwner->ScrollToVerticalOffset(
			std::ceil((std::max)(0.0f, target)));
		(void)RealizeVirtualViewport();
		if (auto* item = GetGeneratedItem(index))
		{
			(void)scrollOwner->BringDescendantIntoView(item);
			return true;
		}
		return false;
	}
	if (auto* item = GetGeneratedItem(index))
		return scrollOwner
			? scrollOwner->BringDescendantIntoView(item) : true;
	return false;
}

void ItemsControl::PreparePresentation()
{
	Control::PreparePresentation();
	if (IsVirtualizing()) (void)RealizeVirtualViewport();
}

void ItemsControl::OnRender()
{
	if (!IsVisible || !GetPresentationWindow() || !GetDrawingContext()) return;
	BeginRender();
	EndRender();
}

cui::core::Size ItemsControl::MeasureCore(
	const cui::core::Constraints& available)
{
	return cui::layout::MeasureOverlayChildren(
		GetLayoutChildrenView(), available,
		GetSpecifiedLayout().padding);
}

void ItemsControl::Arrange(cui::core::Rect finalRect)
{
	Control::Arrange(finalRect);
	PerformPendingLayout();
}

void ItemsControl::RequestLayout()
{
	_itemsLayoutPending = true;
	Control::RequestLayout();
}

void ItemsControl::OnComputedLayoutSizeChanged()
{
	_itemsLayoutPending = true;
}

void ItemsControl::PerformPendingLayout()
{
	if (IsLayoutSuspended() || !_itemsLayoutPending) return;
	if (GetControlTemplateRoot())
	{
		// Control::Arrange owns the ControlTemplate root slot. ItemsPanel
		// padding is expressed inside the template, not by shrinking its root.
		// Commit descendant changes without assigning the root a second slot.
		GetControlTemplateRoot()->UpdateLayout();
		_itemsLayoutPending = false;
		return;
	}
	const auto size = GetActualSizeDip();
	const auto padding = GetSpecifiedLayout().padding;
	cui::layout::ArrangeOverlayChildren(
		GetLayoutChildrenView(),
		cui::core::Rect{
			padding.left,
			padding.top,
			(std::max)(0.0f, size.width - padding.Horizontal()),
			(std::max)(0.0f, size.height - padding.Vertical()) });
	_itemsLayoutPending = false;
}

std::unique_ptr<Control> ItemsControl::WrapGeneratedItem(
	std::unique_ptr<Control> visual,
	const BindingSourceReference&,
	size_t)
{
	return visual;
}
