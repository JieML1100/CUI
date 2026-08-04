#include "ItemsControl.h"
#include "DependencyPropertyInfrastructure.h"
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

#if CUI_ENABLE_DYNAMIC_XAML
namespace cui::design
{
	const std::wstring& AuthoredBindingListItemTypeName(
		const IBindingList& source) noexcept;
}
#endif
#include <vector>

namespace
{
	bool SameCompiledBindingPath(
		CompiledBindingPathView left,
		CompiledBindingPathView right) noexcept
	{
		return left.Version == right.Version
			&& left.Steps.data() == right.Steps.data()
			&& left.Steps.size() == right.Steps.size();
	}

	void ValidateCompiledMemberPath(
		CompiledBindingPathView path,
		const char* propertyName)
	{
		if (path.Version != CompiledBindingPathVersion)
			throw std::invalid_argument(std::string(propertyName)
				+ " compiled path version is unsupported");
		for (const auto& step : path.Steps)
		{
			if (!HasCompiledBindingPathCapability(step.Capabilities,
				CompiledBindingPathCapabilities::Read))
				throw std::invalid_argument(std::string(propertyName)
					+ " compiled path requires read capability");
			if (step.Kind == CompiledBindingPathStepKind::Property
				&& !step.Property)
				throw std::invalid_argument(std::string(propertyName)
					+ " compiled property steps require property tokens");
		}
	}

	bool VisualSubtreeContains(
		const Control* root,
		const Control* candidate) noexcept
	{
		if (!root || !candidate) return false;
		for (auto* current = candidate; current;
			current = current->GetVisualParent())
		{
			if (current == root) return true;
		}
		return false;
	}

	class MaterializedItemsSourceSnapshot final
		: public IBindingList,
		  public IBindingListGroupView
	{
	public:
		MaterializedItemsSourceSnapshot(
			DataTypeToken itemTypeToken,
#if CUI_ENABLE_DYNAMIC_XAML
			std::wstring itemTypeName,
#endif
			std::vector<BindingSourceReference> items,
			std::vector<BindingListGroup> groups)
			: _itemTypeToken(itemTypeToken),
#if CUI_ENABLE_DYNAMIC_XAML
			  _itemTypeName(std::move(itemTypeName)),
#endif
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
		const std::vector<BindingListGroup>& Groups() const noexcept override
		{
			return _groups;
		}
		EventConnection SubscribeGroupsChanged(
			GroupsChangedHandler) override { return {}; }
		bool CanApply(
			const CollectionChangedEventArgs& change,
			size_t actualNewCount,
			DataTypeToken itemTypeToken) const noexcept
		{
			if (change.Action == CollectionChangeAction::Reset
				|| change.OldSize != _items.size()
				|| change.NewSize != actualNewCount
				|| itemTypeToken != _itemTypeToken) return false;
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
		DataTypeToken _itemTypeToken;
#if CUI_ENABLE_DYNAMIC_XAML
		std::wstring _itemTypeName;
#endif
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
				source.Get()->GetItemTypeToken(),
#if CUI_ENABLE_DYNAMIC_XAML
				cui::design::AuthoredBindingListItemTypeName(*source.Get()),
#endif
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
			change, source.Get()->Count(), source.Get()->GetItemTypeToken()))
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

	template<typename TValue>
	DependencyPropertyOptions<ItemsControl, TValue> DataOptions(
		TValue defaultValue
		CUI_DESIGN_METADATA_ARGUMENTS(
			int order,
			DependencyPropertyPersistence persistence =
				DependencyPropertyPersistence::Metadata))
	{
		DependencyPropertyOptions<ItemsControl, TValue> options;
		options.DefaultValue = std::move(defaultValue);
		options.Flags = DependencyPropertyFlags::AffectsMeasure
			| DependencyPropertyFlags::AffectsArrange
			| DependencyPropertyFlags::AffectsRender;
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Data";
		options.Design.CategoryOrder = 80;
		options.Design.Order = order;
		options.Design.Editor = DependencyPropertyEditorKind::Auto;
		options.Design.Persistence = persistence;
		)
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

	bool TextEquals(
		const std::wstring& left,
		const std::wstring& right,
		bool caseSensitive) noexcept
	{
		if (left.size() != right.size()) return false;
		if (caseSensitive) return left == right;
		return std::equal(
			left.begin(), left.end(), right.begin(),
			[](wchar_t leftCharacter, wchar_t rightCharacter)
			{
				return std::towlower(leftCharacter)
					== std::towlower(rightCharacter);
			});
	}

	bool TextStartsWith(
		const std::wstring& value,
		const std::wstring& prefix,
		bool caseSensitive) noexcept
	{
		if (prefix.empty() || prefix.size() > value.size()) return false;
		if (caseSensitive)
			return std::equal(prefix.begin(), prefix.end(), value.begin());
		return std::equal(
			prefix.begin(), prefix.end(), value.begin(),
			[](wchar_t prefixCharacter, wchar_t valueCharacter)
			{
				return std::towlower(prefixCharacter)
					== std::towlower(valueCharacter);
			});
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
				Control::VerticalAlignmentProperty(),
				BindingValue(VerticalAlignment::Top),
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
		bool SetLocalLayoutInvalidation(bool value) noexcept
		{
			return std::exchange(_localLayoutInvalidation, value);
		}

	private:
		bool ShouldPropagateLayoutInvalidation() const noexcept override
		{
			// Realization caused solely by a changed scroll offset does not
			// change the virtual host's total extent. Keep that structural
			// mutation inside the host and commit its new children immediately.
			return !_localLayoutInvalidation;
		}

		size_t _itemCount = 0;
		float _itemHeight = 0.0f;
		float _headerHeight = 0.0f;
		std::vector<size_t> _headerCounts;
		std::vector<float> _offsets;
		std::unordered_map<Control*, size_t> _indices;
		bool _localLayoutInvalidation = false;
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
			if (_changingHeaders)
				throw std::logic_error(
					"GroupedItemHost does not support reentrant header changes");
			const auto initialChildren = GetVisualChildrenView();
			if (!_item
				|| _headerCount > initialChildren.size()
				|| initialChildren.size() != _headerCount + 1
				|| initialChildren[_headerCount] != _item
				|| _contexts.size() != _headerCount)
				throw std::logic_error(
					"GroupedItemHost current header metadata is invalid");
			if (contexts.size() != headers.size()
				|| std::any_of(
					headers.begin(), headers.end(),
					[](const auto& header) { return !header; }))
				throw std::invalid_argument(
					"GroupedItemHost header visuals and contexts do not match");
			_changingHeaders = true;
			struct HeaderChangeGuard final
			{
				bool& Value;
				~HeaderChangeGuard() { Value = false; }
			} changeGuard{ _changingHeaders };

			struct PreviousChild final
			{
				ControlWeakReference Lifetime;
				ControlWeakReference LogicalParent;
				bool IsItem = false;
				size_t HeaderIndex = static_cast<size_t>(-1);
				std::unique_ptr<Control> Owner;
			};

			const ControlWeakReference previousItemLifetime(_item);
			const auto previousHeaderCount = _headerCount;
			const auto previousFixedHeaderHeight = _fixedHeaderHeight;
			const auto previousFixedItemHeight = _fixedItemHeight;
			const auto previousContexts = _contexts;
			std::vector<BindingSourceReference> restoredContextsScratch;
			restoredContextsScratch.reserve(previousHeaderCount);
			std::vector<ControlWeakReference> candidateHeaders;
			candidateHeaders.reserve(headers.size());
			std::vector<PreviousChild> previousChildren;
			previousChildren.reserve(GetVisualChildrenView().size());
			size_t visualIndex = 0;
			for (auto* child : GetVisualChildrenView())
			{
				previousChildren.push_back(PreviousChild{
					ControlWeakReference(child),
					ControlWeakReference(child
						? child->GetLogicalParent() : nullptr),
					child && child == _item,
					visualIndex < previousHeaderCount
						? visualIndex : static_cast<size_t>(-1),
					{} });
				++visualIndex;
			}

			std::exception_ptr firstNotificationError;
			std::exception_ptr transactionError;
			auto rememberFirst = [&](std::exception_ptr error)
			{
				if (error && !firstNotificationError)
					firstNotificationError = std::move(error);
			};
			auto fail = [&](const char* message)
			{
				if (!transactionError)
					transactionError = std::make_exception_ptr(
						std::logic_error(message));
			};
			auto isOwnedHere = [this](Control* child) noexcept
			{
				return child && child->GetVisualParent() == this
					&& IndexOfVisualChild(child) >= 0;
			};

			// Never publish a raw item while callbacks can detach, transfer or
			// destroy it. The slot is republished only from a verified final tree.
			_item = nullptr;
			_headerCount = 0;

			for (auto& entry : previousChildren)
			{
				auto* live = entry.Lifetime.Get();
				if (!live)
				{
					fail("GroupedItemHost previous child expired");
					break;
				}
				bool ownershipCommit = false;
				std::exception_ptr notificationError;
				try
				{
					entry.Owner =
						cui::framework::TreeAccess::DetachVisualChild(
							*this, live, &ownershipCommit,
							&notificationError);
				}
				catch (...)
				{
					transactionError = std::current_exception();
					break;
				}
				rememberFirst(notificationError);
				live = entry.Lifetime.Get();
				if (!entry.Owner || entry.Owner.get() != live)
				{
					fail(isOwnedHere(live)
						? "GroupedItemHost previous child detach did not commit"
						: "GroupedItemHost previous child ownership escaped");
					break;
				}
			}
			if (!transactionError && VisualChildCount() != 0)
				fail("GroupedItemHost detach left unexpected visual children");

			if (!transactionError)
			{
				for (auto& header : headers)
				{
					if (!header)
					{
						fail("GroupedItemHost header is null");
						break;
					}
					const ControlWeakReference lifetime(header.get());
					std::exception_ptr attachError;
					try
					{
						(void)cui::framework::TreeAccess::
							InsertOwnedVisualChildPreserving(
								*this, VisualChildCount(), header, this);
					}
					catch (...)
					{
						attachError = std::current_exception();
					}
					auto* live = lifetime.Get();
					const bool attached = isOwnedHere(live)
						&& live->GetLogicalParent() == this;
					if (!attached)
					{
						transactionError = attachError
							? attachError
							: std::make_exception_ptr(std::logic_error(
								"GroupedItemHost header attachment did not commit"));
						break;
					}
					candidateHeaders.push_back(lifetime);
					rememberFirst(attachError);
				}
			}

			auto itemEntry = std::find_if(
				previousChildren.begin(), previousChildren.end(),
				[](const PreviousChild& entry) { return entry.IsItem; });
			ControlWeakReference committedItem;
			if (!transactionError)
			{
				if (itemEntry == previousChildren.end() || !itemEntry->Owner)
					fail("GroupedItemHost lost its item owner");
				else
				{
					const auto itemLifetime = itemEntry->Lifetime;
					auto* logicalParent = itemEntry->LogicalParent.Get();
					std::exception_ptr attachError;
					try
					{
						(void)cui::framework::TreeAccess::
							InsertOwnedVisualChildPreserving(
								*this, VisualChildCount(),
								itemEntry->Owner, logicalParent);
					}
					catch (...)
					{
						attachError = std::current_exception();
					}
					auto* live = itemLifetime.Get();
					if (!isOwnedHere(live)
						|| live->GetLogicalParent() != logicalParent)
						transactionError = attachError
							? attachError
							: std::make_exception_ptr(std::logic_error(
								"GroupedItemHost item attachment did not commit"));
					else
					{
						committedItem = itemLifetime;
						rememberFirst(attachError);
					}
				}
			}

			if (!transactionError)
			{
				const auto children = GetVisualChildrenView();
				bool exact = children.size() == candidateHeaders.size() + 1;
				for (size_t index = 0;
					exact && index < candidateHeaders.size(); ++index)
					exact = children[index] == candidateHeaders[index].Get();
				exact = exact && children.back() == committedItem.Get();
				if (!exact)
					fail("GroupedItemHost header transaction final state is invalid");
			}

			if (!transactionError)
			{
				_item = committedItem.Get();
				_headerCount = candidateHeaders.size();
				_fixedHeaderHeight =
					(std::max)(0.0f, fixedHeaderHeight);
				_fixedItemHeight =
					(std::max)(0.0f, fixedItemHeight);
				_contexts = std::move(contexts);
				InvalidateLayout();
				if (firstNotificationError)
					std::rethrow_exception(firstNotificationError);
				return;
			}
			if (firstNotificationError)
				transactionError = firstNotificationError;

			// Remove every candidate or callback-injected visual before restoring
			// the old weak identities. Detached owners are intentionally local:
			// failed replacement content must not survive the transaction.
			for (size_t pass = 0; pass < 1024; ++pass)
			{
				Control* unexpected = nullptr;
				for (auto* child : GetVisualChildrenView())
				{
					const bool wasPrevious = std::any_of(
						previousChildren.begin(), previousChildren.end(),
						[child](const PreviousChild& entry)
						{ return entry.Lifetime.Get() == child; });
					if (child && !wasPrevious)
					{
						unexpected = child;
						break;
					}
				}
				if (!unexpected) break;
				const ControlWeakReference childReference(unexpected);
				auto* child = childReference.Get();
				if (!child || !isOwnedHere(child)) continue;
				bool ownershipCommit = false;
				std::exception_ptr ignoredNotificationError;
				try
				{
					auto discarded =
						cui::framework::TreeAccess::DetachVisualChild(
							*this, child, &ownershipCommit,
							&ignoredNotificationError);
					(void)discarded;
				}
				catch (...)
				{
					// Rollback is best-effort, but metadata below is always
					// republished from the surviving weak identities.
				}
			}

			for (auto& entry : previousChildren)
			{
				auto* live = entry.Lifetime.Get();
				if (isOwnedHere(live)) continue;
				if (!entry.Owner || entry.Owner.get() != live) continue;
				try
				{
					(void)cui::framework::TreeAccess::
						InsertOwnedVisualChildPreserving(
							*this, VisualChildCount(), entry.Owner,
							entry.LogicalParent.Get());
				}
				catch (...)
				{
					// Preserve transactionError. A post-commit restoration is
					// recognized by the final weak/parent scan below.
				}
			}

			size_t restoreIndex = 0;
			for (const auto& entry : previousChildren)
			{
				auto* live = entry.Lifetime.Get();
				if (!isOwnedHere(live)) continue;
				const int currentIndex = IndexOfVisualChild(live);
				if (currentIndex != static_cast<int>(restoreIndex))
				try
				{
					(void)MoveVisualChild(
						currentIndex, static_cast<int>(restoreIndex));
				}
				catch (...) {}
				++restoreIndex;
			}

			auto* restoredItem = previousItemLifetime.Get();
			_item = isOwnedHere(restoredItem) ? restoredItem : nullptr;
			for (const auto& entry : previousChildren)
			{
				if (entry.HeaderIndex == static_cast<size_t>(-1)
					|| entry.HeaderIndex >= previousContexts.size())
					continue;
				if (isOwnedHere(entry.Lifetime.Get()))
					restoredContextsScratch.push_back(
						previousContexts[entry.HeaderIndex]);
			}
			_headerCount = restoredContextsScratch.size();
			_contexts = std::move(restoredContextsScratch);
			_fixedHeaderHeight = previousFixedHeaderHeight;
			_fixedItemHeight = previousFixedItemHeight;
			InvalidateLayout();
			std::rethrow_exception(transactionError);
		}

	private:
		Control* _item = nullptr;
		size_t _headerCount = 0;
		float _fixedHeaderHeight = 0.0f;
		float _fixedItemHeight = 0.0f;
		std::vector<BindingSourceReference> _contexts;
		bool _changingHeaders = false;
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

struct ItemsControl::DirectVisualMutationFrame final
{
	explicit DirectVisualMutationFrame(
		ItemsControl& owner,
		Control* after)
		: Owner(owner),
		Previous(owner._activeDirectVisualMutationFrame),
		After(after),
		HasAfter(after != nullptr)
	{
		const auto before = owner.GetVisualChildrenView();
		auto* previousInfrastructure = std::find(
			before.begin(), before.end(),
			owner._controlTemplateRoot) != before.end()
			? owner._controlTemplateRoot
			: std::find(
				before.begin(), before.end(),
				owner._itemsHost) != before.end()
				? owner._itemsHost : nullptr;
		if (previousInfrastructure)
		{
			Before = previousInfrastructure;
			HasBefore = true;
		}
		owner._activeDirectVisualMutationFrame = this;
	}
	DirectVisualMutationFrame(
		const DirectVisualMutationFrame&) = delete;
	DirectVisualMutationFrame& operator=(
		const DirectVisualMutationFrame&) = delete;
	~DirectVisualMutationFrame()
	{
		Owner._activeDirectVisualMutationFrame = Previous;
	}

	ItemsControl& Owner;
	DirectVisualMutationFrame* Previous = nullptr;
	ControlWeakReference Before;
	ControlWeakReference After;
	bool HasBefore = false;
	bool HasAfter = false;
};

ItemsControl::ItemsControl()
	: Control()
{
	auto itemsHost = CreateItemsHost();
	_itemsHost = itemsHost.get();
	cui::framework::TreeAccess::SetTemplatedParent(*_itemsHost, this);
	DirectVisualMutationFrame mutation(*this, _itemsHost);
	cui::framework::TreeAccess::AddOwnedVisualChild(
		*this, std::move(itemsHost), nullptr);
	RefreshItemsScrollOwner();
}

bool ItemsControl::ValidateVisualChildCollection(
	std::span<Control* const> children,
	std::string& error) const
{
	if (_activeDirectVisualMutationFrame)
	{
		const auto& frame = *_activeDirectVisualMutationFrame;
		auto matches = [&](
			bool hasExpected,
			const ControlWeakReference& expected) noexcept
		{
			if (!hasExpected) return children.empty();
			return children.size() == 1 && children.front()
				&& expected.Get() == children.front();
		};
		if (matches(frame.HasBefore, frame.Before)
			|| matches(frame.HasAfter, frame.After))
			return true;
		error =
			"ItemsControl direct-child transaction rejected an unexpected visual";
		return false;
	}
	if (_controlTemplateRoot && children.size() == 1
		&& children.front() == _controlTemplateRoot)
		return true;
	if (!_controlTemplateRoot && _itemsHost && children.size() == 1
		&& children.front() == _itemsHost)
		return true;
	error = "ItemsControl direct children are owned by its ControlTemplate or ItemsPanelTemplate host";
	return false;
}

const DependencyProperty& ItemsControl::ItemsSourceProperty()
{
	static const auto registration = []
	{
		auto options = DataOptions(
			BindingListReference{}
			CUI_DESIGN_METADATA_ARGUMENTS(
				10, DependencyPropertyPersistence::Native));
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Browsable = false;
		)
		return DependencyPropertyRegistry::RegisterStatic<
			ItemsControl, BindingListReference>(
				DependencyPropertyRegistrationLiteral(L"ItemsSource"),
				[](ItemsControl& target) { return target.GetItemsSource(); },
				[](ItemsControl& target, const BindingListReference& value)
				{ target.SetItemsSource(value); }, {}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& ItemsControl::ItemTemplateProperty()
{
	static const auto registration = []
	{
		auto options = DataOptions(
			ItemTemplateReference{}
			CUI_DESIGN_METADATA_ARGUMENTS(
				20, DependencyPropertyPersistence::Native));
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Browsable = false;
		)
		return DependencyPropertyRegistry::RegisterStatic<
			ItemsControl, ItemTemplateReference>(
				DependencyPropertyRegistrationLiteral(L"ItemTemplate"),
				[](ItemsControl& target) { return target.GetItemTemplate(); },
				[](ItemsControl& target, const ItemTemplateReference& value)
				{ target.SetItemTemplate(value); }, {}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& ItemsControl::GroupStyleProperty()
{
	static const auto registration = []
	{
		auto options = DataOptions(
			GroupStyleReference{}
			CUI_DESIGN_METADATA_ARGUMENTS(
				25, DependencyPropertyPersistence::Native));
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Browsable = false;
		)
		return DependencyPropertyRegistry::RegisterStatic<
			ItemsControl, GroupStyleReference>(
				DependencyPropertyRegistrationLiteral(L"GroupStyle"),
				[](ItemsControl& target) { return target.GetGroupStyle(); },
				[](ItemsControl& target, const GroupStyleReference& value)
				{ target.SetGroupStyle(value); }, {}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& ItemsControl::ItemsPanelProperty()
{
	static const auto registration = []
	{
		auto options = DataOptions(
			ItemsPanelTemplateReference{}
			CUI_DESIGN_METADATA_ARGUMENTS(
				30, DependencyPropertyPersistence::Native));
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Browsable = false;
		)
		return DependencyPropertyRegistry::RegisterStatic<
			ItemsControl, ItemsPanelTemplateReference>(
				DependencyPropertyRegistrationLiteral(L"ItemsPanel"),
				[](ItemsControl& target) { return target.GetItemsPanel(); },
				[](ItemsControl& target, const ItemsPanelTemplateReference& value)
				{ target.SetItemsPanel(value); }, {}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& ItemsControl::ItemContainerStyleProperty()
{
	static const auto registration = []
	{
		auto options = DataOptions(
			std::wstring{}
			CUI_DESIGN_METADATA_ARGUMENTS(
				50, DependencyPropertyPersistence::Native));
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Browsable = false;
		)
		return DependencyPropertyRegistry::RegisterStatic<
			ItemsControl, std::wstring>(
				DependencyPropertyRegistrationLiteral(L"ItemContainerStyle"),
				[](ItemsControl& target) { return target.GetItemContainerStyle(); },
				[](ItemsControl& target, const std::wstring& value)
				{ target.SetItemContainerStyle(value); }, {},
				std::move(options));
	}();
	return *registration;
}

void ItemsControl::RegisterDependencyProperties()
{
	Control::RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	RegisterDesignDependencyProperties();
#endif
}

const DependencyProperty& ItemsControl::IsTextSearchEnabledProperty()
{
	static const auto registration = []
	{
		DependencyPropertyOptions<ItemsControl, bool> options;
		options.DefaultValue = false;
		options.Flags = DependencyPropertyFlags::None;
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Behavior";
		options.Design.CategoryOrder = 300;
		options.Design.Order = 60;
		options.Design.Editor = DependencyPropertyEditorKind::Boolean;
		options.Design.Persistence =
			DependencyPropertyPersistence::Metadata;
		)
		options.Changed = [](
			ItemsControl& target, const bool&, const bool& enabled)
		{
			if (!enabled) target.ResetTextSearch();
		};
		return DependencyPropertyRegistry::RegisterStatic<ItemsControl, bool>(
			DependencyPropertyRegistrationLiteral(L"IsTextSearchEnabled"),
			std::move(options));
	}();
	return *registration;
}

const DependencyProperty& ItemsControl::IsTextSearchCaseSensitiveProperty()
{
	static const auto registration = []
	{
		DependencyPropertyOptions<ItemsControl, bool> options;
		options.DefaultValue = false;
		options.Flags = DependencyPropertyFlags::None;
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Behavior";
		options.Design.CategoryOrder = 300;
		options.Design.Order = 70;
		options.Design.Editor = DependencyPropertyEditorKind::Boolean;
		options.Design.Persistence =
			DependencyPropertyPersistence::Metadata;
		)
		options.Changed = [](
			ItemsControl& target, const bool&, const bool&)
		{
			target.ResetTextSearch();
		};
		return DependencyPropertyRegistry::RegisterStatic<ItemsControl, bool>(
			DependencyPropertyRegistrationLiteral(
				L"IsTextSearchCaseSensitive"),
			std::move(options));
	}();
	return *registration;
}

bool ItemsControl::GetIsTextSearchEnabled() const
{
	return GetDependencyPropertyValue<bool>(
		IsTextSearchEnabledProperty());
}

void ItemsControl::SetIsTextSearchEnabled(bool value)
{
	(void)SetDependencyPropertyValue(
		IsTextSearchEnabledProperty(), value);
}

bool ItemsControl::GetIsTextSearchCaseSensitive() const
{
	return GetDependencyPropertyValue<bool>(
		IsTextSearchCaseSensitiveProperty());
}

void ItemsControl::SetIsTextSearchCaseSensitive(bool value)
{
	(void)SetDependencyPropertyValue(
		IsTextSearchCaseSensitiveProperty(), value);
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
			*panel, WrapPanel::OrientationProperty(),
			BindingValue(definition.Orientation),
			DependencyPropertyValueSource::Template);
		(void)cui::framework::DependencyPropertyAccess::SetValue(
			*panel, WrapPanel::ItemWidthProperty(),
			BindingValue(definition.ItemWidth),
			DependencyPropertyValueSource::Template);
		(void)cui::framework::DependencyPropertyAccess::SetValue(
			*panel, WrapPanel::ItemHeightProperty(),
			BindingValue(definition.ItemHeight),
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
			*panel, StackPanel::OrientationProperty(),
			BindingValue(definition.Orientation),
			DependencyPropertyValueSource::Template);
		result = std::move(panel);
	}
	(void)cui::framework::DependencyPropertyAccess::SetValue(
		*result, Control::VerticalAlignmentProperty(),
		BindingValue(VerticalAlignment::Top),
		DependencyPropertyValueSource::Template);
	return result;
}

std::unique_ptr<Panel> ItemsControl::TakeItemsHost()
{
	if (!_itemsHost) return {};
	auto* requestedHost = _itemsHost;
	const ControlWeakReference hostLifetime(requestedHost);
	if (_templateItemsPresenter
		&& _templateItemsPresenter->GetItemsHost() == _itemsHost)
	{
		auto* presenter = _templateItemsPresenter;
		const ControlWeakReference presenterLifetime(presenter);
		std::unique_ptr<Panel> detached;
		try
		{
			detached = presenter->DetachItemsHost();
		}
		catch (...)
		{
			auto* liveHost = dynamic_cast<Panel*>(hostLifetime.Get());
			auto* livePresenter = dynamic_cast<ItemsPresenter*>(
				presenterLifetime.Get());
			_itemsHost = liveHost && livePresenter
				&& livePresenter->GetItemsHost() == liveHost
				&& liveHost->GetVisualParent() == livePresenter
				&& livePresenter->IndexOfVisualChild(liveHost) >= 0
				? liveHost : nullptr;
			if (!livePresenter && _templateItemsPresenter == presenter)
				_templateItemsPresenter = nullptr;
			throw;
		}
		auto* liveHost = dynamic_cast<Panel*>(hostLifetime.Get());
		auto* livePresenter = dynamic_cast<ItemsPresenter*>(
			presenterLifetime.Get());
		if (detached)
		{
			if (detached.get() != liveHost)
				throw std::logic_error(
					"ItemsPresenter returned an unexpected ItemsHost owner");
			_itemsHost = detached.get();
			return detached;
		}
		if (liveHost && livePresenter
			&& livePresenter->GetItemsHost() == liveHost
			&& liveHost->GetVisualParent() == livePresenter
			&& livePresenter->IndexOfVisualChild(liveHost) >= 0)
		{
			_itemsHost = liveHost;
			return {};
		}
		// A committed callback may transfer or destroy the host and still make
		// DetachItemsHost return no owner. Never retain the old raw slot.
		_itemsHost = nullptr;
		if (!livePresenter && _templateItemsPresenter == presenter)
			_templateItemsPresenter = nullptr;
		return {};
	}
	if (_detachedItemsHost)
	{
		auto detached = std::move(_detachedItemsHost);
		_itemsHost = detached.get();
		return detached;
	}
	if (requestedHost->GetVisualParent() != this
		|| IndexOfVisualChild(requestedHost) < 0)
	{
		_itemsHost = nullptr;
		return {};
	}
	std::unique_ptr<Control> detached;
	try
	{
		DirectVisualMutationFrame mutation(*this, nullptr);
		detached = DetachVisualChild(requestedHost);
	}
	catch (...)
	{
		auto* liveHost = dynamic_cast<Panel*>(hostLifetime.Get());
		_itemsHost = liveHost && liveHost->GetVisualParent() == this
			&& IndexOfVisualChild(liveHost) >= 0 ? liveHost : nullptr;
		throw;
	}
	auto* liveHost = dynamic_cast<Panel*>(hostLifetime.Get());
	if (liveHost && liveHost->GetVisualParent() == this
		&& IndexOfVisualChild(liveHost) >= 0)
	{
		_itemsHost = liveHost;
		return {};
	}
	auto result = std::unique_ptr<Panel>(
		static_cast<Panel*>(detached.release()));
	_itemsHost = result ? result.get() : nullptr;
	return result;
}

void ItemsControl::PlaceItemsHost(std::unique_ptr<Panel> host)
{
	if (!host) throw std::invalid_argument("ItemsControl ItemsHost is null");
	auto* raw = host.get();
	const ControlWeakReference lifetime(raw);
	_itemsHost = raw;
	try
	{
		if (_templateItemsPresenter)
		{
			if (!cui::framework::TreeAccess::
				SetLogicalParentPreservingOwnership(
					host, nullptr))
				throw std::logic_error(
					"ItemsHost ownership changed during logical-parent publication");
			if (host && !host->GetTemplatedParent()
				&& !cui::framework::TreeAccess::
					SetTemplatedParentPreservingOwnership(
						host, this))
				throw std::logic_error(
					"ItemsHost ownership changed during template-parent publication");
			if (!host)
				throw std::logic_error(
					"ItemsHost ownership was transferred before presenter attachment");
			_templateItemsPresenter->SetItemsHost(std::move(host));
			auto* live = dynamic_cast<Panel*>(lifetime.Get());
			if (!live
				|| _templateItemsPresenter->GetItemsHost() != live
				|| live->GetVisualParent() != _templateItemsPresenter)
				throw std::logic_error(
					"ItemsPresenter did not retain the ItemsHost");
		}
		else if (_controlTemplateRoot)
		{
			if (!host->GetTemplatedParent()
				&& !cui::framework::TreeAccess::
					SetTemplatedParentPreservingOwnership(
						host, this))
				throw std::logic_error(
					"ItemsHost ownership changed during template-parent publication");
			if (!host || !cui::framework::TreeAccess::
				SetLogicalParentPreservingOwnership(
					host, nullptr))
				throw std::logic_error(
					"ItemsHost ownership changed during logical-parent publication");
			_detachedItemsHost = std::move(host);
		}
		else
		{
			if (!host->GetTemplatedParent()
				&& !cui::framework::TreeAccess::
					SetTemplatedParentPreservingOwnership(
						host, this))
				throw std::logic_error(
					"ItemsHost ownership changed during template-parent publication");
			try
			{
				DirectVisualMutationFrame mutation(*this, raw);
				(void)cui::framework::TreeAccess::
					InsertOwnedVisualChildPreserving(
						*this, VisualChildCount(),
						host, nullptr);
			}
			catch (...)
			{
				throw;
			}
		}
	}
	catch (...)
	{
		auto* live = dynamic_cast<Panel*>(lifetime.Get());
		if (live && host.get() == live)
			_detachedItemsHost = std::move(host);
		const bool presenterOwns = live && _templateItemsPresenter
			&& _templateItemsPresenter->GetItemsHost() == live
			&& live->GetVisualParent() == _templateItemsPresenter;
		const bool detachedOwner = live
			&& _detachedItemsHost.get() == live;
		const bool directOwner = live
			&& live->GetVisualParent() == this
			&& IndexOfVisualChild(live) >= 0;
		_itemsHost = presenterOwns || detachedOwner || directOwner
			? live : nullptr;
		throw;
	}
	auto* live = dynamic_cast<Panel*>(lifetime.Get());
	const bool presenterOwns = live && _templateItemsPresenter
		&& _templateItemsPresenter->GetItemsHost() == live
		&& live->GetVisualParent() == _templateItemsPresenter
		&& _templateItemsPresenter->IndexOfVisualChild(live) >= 0;
	const bool detachedOwner = live
		&& _detachedItemsHost.get() == live;
	const bool directOwner = live
		&& live->GetVisualParent() == this
		&& IndexOfVisualChild(live) >= 0;
	if (!presenterOwns && !detachedOwner && !directOwner)
	{
		_itemsHost = nullptr;
		throw std::logic_error(
			"ItemsControl ItemsHost placement did not commit");
	}
	_itemsHost = live;
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
					(void)RealizeVirtualViewport(true);
			});
}

void ItemsControl::ClearPendingTemplateItemsPresenter() noexcept
{
	_pendingTemplateItemsPresenter.Reset();
}

bool ItemsControl::CommitPendingTemplateItemsPresenter()
{
	const auto candidateReference = _pendingTemplateItemsPresenter;
	auto* candidate = dynamic_cast<ItemsPresenter*>(
		candidateReference.Get());
	if (!candidate || candidate->GetTemplatedParent() != this)
	{
		ClearPendingTemplateItemsPresenter();
		return false;
	}
	auto* root = _controlTemplateRoot;
	if (!root || !VisualSubtreeContains(root, candidate))
		return false;
	if (_templateItemsPresenter)
	{
		const bool alreadyCommitted =
			_templateItemsPresenter == candidate
			&& candidate->GetItemsHost() == _itemsHost
			&& _itemsHost
			&& _itemsHost->GetVisualParent() == candidate
			&& candidate->IndexOfVisualChild(_itemsHost) >= 0;
		if (alreadyCommitted)
			ClearPendingTemplateItemsPresenter();
		return alreadyCommitted;
	}

	const ControlWeakReference rootLifetime(root);
	auto host = TakeItemsHost();
	if (!host)
		throw std::logic_error(
			"ItemsControl lost its ItemsHost while committing ItemsPresenter");
	const ControlWeakReference hostLifetime(host.get());
	candidate = dynamic_cast<ItemsPresenter*>(candidateReference.Get());
	root = rootLifetime.Get();
	if (!candidate || !root || _controlTemplateRoot != root
		|| root->GetVisualParent() != this
		|| IndexOfVisualChild(root) < 0
		|| candidate->GetTemplatedParent() != this
		|| !VisualSubtreeContains(root, candidate))
	{
		ClearPendingTemplateItemsPresenter();
		PlaceItemsHost(std::move(host));
		return false;
	}

	_templateItemsPresenter = candidate;
	try
	{
		_itemsPresenterParentChanged =
			cui::framework::TreeAccess::SubscribeVisualParentChanged(
			*candidate,
			[this](Control* sender, Control*, Control*)
			{
				auto* presenter =
					dynamic_cast<ItemsPresenter*>(sender);
				if (presenter
					&& _templateItemsPresenter == presenter
					&& _controlTemplateRoot
					&& !VisualSubtreeContains(
						_controlTemplateRoot, presenter))
				{
					std::unique_ptr<Panel> recoveredHost;
					auto* presenterHost = presenter->GetItemsHost();
					if (presenterHost
						&& presenterHost == _itemsHost
						&& presenterHost->GetVisualParent()
						== presenter
						&& presenter->IndexOfVisualChild(
							presenterHost) >= 0)
					{
						try
						{
							recoveredHost =
								presenter->DetachItemsHost();
						}
						catch (...) {}
					}
					// The presenter is no longer reachable from the active
					// template root. Clear the template-owner identity while
					// its parent-change transaction still observes any further
					// ownership transfer.
					(void)ClearTemplateOwnerSubtree(
						presenter, this);
					_itemsPresenterParentChanged.Disconnect();
					_templateItemsPresenter = nullptr;
					_itemsHost = nullptr;
					if (recoveredHost)
					{
						try
						{
							PlaceItemsHost(
								std::move(recoveredHost));
						}
						catch (...) {}
					}
				}
				RefreshItemsScrollOwner();
				if (IsVirtualizing())
					(void)RealizeVirtualViewport();
			});
	}
	catch (...)
	{
		const auto originalError = std::current_exception();
		_templateItemsPresenter = nullptr;
		_itemsHost = nullptr;
		ClearPendingTemplateItemsPresenter();
		try { PlaceItemsHost(std::move(host)); }
		catch (...) {}
		std::rethrow_exception(originalError);
	}

	auto finalCommitted = [&]() noexcept
	{
		auto* liveRoot = rootLifetime.Get();
		auto* livePresenter = dynamic_cast<ItemsPresenter*>(
			candidateReference.Get());
		auto* liveHost = dynamic_cast<Panel*>(
			hostLifetime.Get());
		return liveRoot && _controlTemplateRoot == liveRoot
			&& liveRoot->GetVisualParent() == this
			&& IndexOfVisualChild(liveRoot) >= 0
			&& livePresenter
			&& _templateItemsPresenter == livePresenter
			&& livePresenter->GetTemplatedParent() == this
			&& VisualSubtreeContains(liveRoot, livePresenter)
			&& liveHost && _itemsHost == liveHost
			&& livePresenter->GetItemsHost() == liveHost
			&& liveHost->GetVisualParent() == livePresenter
			&& livePresenter->IndexOfVisualChild(liveHost) >= 0;
	};
	auto rollback = [&]() noexcept
	{
		std::unique_ptr<Panel> recoveredHost;
		auto* livePresenter = dynamic_cast<ItemsPresenter*>(
			candidateReference.Get());
		auto* liveHost = dynamic_cast<Panel*>(
			hostLifetime.Get());
		if (livePresenter && liveHost
			&& livePresenter->GetItemsHost() == liveHost
			&& liveHost->GetVisualParent() == livePresenter
			&& livePresenter->IndexOfVisualChild(liveHost) >= 0)
		{
			try { recoveredHost = livePresenter->DetachItemsHost(); }
			catch (...) {}
		}
		auto* liveRoot = rootLifetime.Get();
		livePresenter = dynamic_cast<ItemsPresenter*>(
			candidateReference.Get());
		if (livePresenter
			&& livePresenter->GetTemplatedParent() == this
			&& (!liveRoot
				|| !VisualSubtreeContains(
					liveRoot, livePresenter)))
			(void)ClearTemplateOwnerSubtree(
				livePresenter, this);
		if (!recoveredHost && liveHost
			&& _detachedItemsHost.get() == liveHost)
			recoveredHost = std::move(_detachedItemsHost);
		_itemsPresenterParentChanged.Disconnect();
		_templateItemsPresenter = nullptr;
		_itemsHost = nullptr;
		ClearPendingTemplateItemsPresenter();
		if (recoveredHost)
		{
			try { PlaceItemsHost(std::move(recoveredHost)); }
			catch (...) {}
		}
	};

	try
	{
		PlaceItemsHost(std::move(host));
	}
	catch (...)
	{
		const auto originalError = std::current_exception();
		if (finalCommitted())
		{
			ClearPendingTemplateItemsPresenter();
			RefreshItemsScrollOwner();
			std::rethrow_exception(originalError);
		}
		rollback();
		std::rethrow_exception(originalError);
	}
	if (!finalCommitted())
	{
		rollback();
		throw std::logic_error(
			"ItemsControl ItemsPresenter commitment did not retain the active root and ItemsHost");
	}
	ClearPendingTemplateItemsPresenter();
	RefreshItemsScrollOwner();
	return true;
}

bool ItemsControl::RegisterTemplateItemsPresenter(
	ItemsPresenter* presenter)
{
	if (!presenter || presenter->GetTemplatedParent() != this)
		return false;
	if (_templateItemsPresenter == presenter) return true;
	if (_templateItemsPresenter
		&& _templateItemsPresenter != presenter)
		return false;
	_pendingTemplateItemsPresenter = presenter;
	if (_controlTemplateRoot)
	{
		if (!VisualSubtreeContains(_controlTemplateRoot, presenter))
		{
			ClearPendingTemplateItemsPresenter();
			return false;
		}
		return CommitPendingTemplateItemsPresenter();
	}
	RequestLayout();
	return true;
}

Control* ItemsControl::SetControlTemplateRoot(
	std::unique_ptr<Control> value)
{
	if (value && value.get() == _controlTemplateRoot)
	{
		(void)value.release();
		return _controlTemplateRoot;
	}
	if (!value)
	{
		(void)DetachVisualChildTemplateRoot();
		return nullptr;
	}
	if (_controlTemplateRoot)
		throw std::logic_error("ItemsControl already owns a ControlTemplate root");
	auto* candidateRoot = value.get();
	if (auto* pending = dynamic_cast<ItemsPresenter*>(
		_pendingTemplateItemsPresenter.Get()))
	{
		if (pending->GetTemplatedParent() != this)
			ClearPendingTemplateItemsPresenter();
		else if (!VisualSubtreeContains(candidateRoot, pending)
			&& pending->GetVisualParent())
		{
			ClearPendingTemplateItemsPresenter();
			throw std::logic_error(
				"ItemsControl pending ItemsPresenter does not belong to the candidate template root");
		}
	}
	else if (_pendingTemplateItemsPresenter.HasValue())
		ClearPendingTemplateItemsPresenter();
	if (!_templateItemsPresenter)
	{
		auto host = TakeItemsHost();
		if (!host) throw std::logic_error("ItemsControl lost its ItemsHost");
		_detachedItemsHost = std::move(host);
	}
	const ControlWeakReference candidateLifetime(candidateRoot);
	const ControlWeakReference presenterLifetime(
		_templateItemsPresenter);
	_controlTemplateRoot = candidateRoot;
	try
	{
		DirectVisualMutationFrame mutation(*this, candidateRoot);
		(void)cui::framework::TreeAccess::
			InsertOwnedVisualChildPreserving(
				*this, VisualChildCount(), value, nullptr);
		auto* live = candidateLifetime.Get();
		if (!live || live->GetVisualParent() != this
			|| IndexOfVisualChild(live) < 0)
			throw std::logic_error(
				"ItemsControl template root attachment did not commit");
	}
	catch (...)
	{
		const auto originalError = std::current_exception();
		auto* live = candidateLifetime.Get();
		if (live && (live->GetVisualParent() == this
			|| IndexOfVisualChild(live) >= 0))
		{
			// The collection owns a post-commit failure. Keep it published so
			// the caller's AbortTemplateApplication can detach it normally.
			_controlTemplateRoot = live;
			MarkControlTemplateRootAttached();
		}
		else
		{
			std::unique_ptr<Panel> host;
			auto* presenter = dynamic_cast<ItemsPresenter*>(
				presenterLifetime.Get());
			if (presenter && _templateItemsPresenter == presenter)
			{
				auto* presenterHost = presenter->GetItemsHost();
				if (presenterHost && presenterHost == _itemsHost
					&& presenterHost->GetVisualParent() == presenter)
				{
					try { host = presenter->DetachItemsHost(); }
					catch (...) {}
				}
			}
			if (!host && _detachedItemsHost)
				host = std::move(_detachedItemsHost);
			_itemsPresenterParentChanged.Disconnect();
			_templateItemsPresenter = nullptr;
			_itemsHost = nullptr;
			ClearPendingTemplateItemsPresenter();
			if (_controlTemplateRoot == candidateRoot)
				_controlTemplateRoot = nullptr;
			if (value)
				(void)ClearTemplateOwnerSubtreePreservingOwnership(
					value, this);
			else
				(void)ClearTemplateOwnerSubtree(live, this);
			try { ClearDeclarativeTemplateScope(); }
			catch (...) {}
			if (host)
			{
				try { PlaceItemsHost(std::move(host)); }
				catch (...) {}
			}
			MarkControlTemplateRootDetached();
			try { OnControlTemplatePresentationChanged(); }
			catch (...) {}
		}
		std::rethrow_exception(originalError);
	}
	auto* liveRoot = candidateLifetime.Get();
	if (!liveRoot || liveRoot->GetVisualParent() != this
		|| IndexOfVisualChild(liveRoot) < 0)
		throw std::logic_error(
			"ItemsControl template root ownership is invalid");
	_controlTemplateRoot = liveRoot;
	MarkControlTemplateRootAttached();
	try
	{
		if (auto* pending = dynamic_cast<ItemsPresenter*>(
			_pendingTemplateItemsPresenter.Get()))
		{
			if (pending->GetTemplatedParent() != this
				|| (pending->GetVisualParent()
					&& !VisualSubtreeContains(liveRoot, pending)))
			{
				ClearPendingTemplateItemsPresenter();
				throw std::logic_error(
					"ItemsControl pending ItemsPresenter escaped the active template root");
			}
			(void)CommitPendingTemplateItemsPresenter();
		}
		if (_templateItemsPresenter
			&& !VisualSubtreeContains(
				liveRoot, _templateItemsPresenter))
			throw std::logic_error(
				"ItemsControl active ItemsPresenter is outside the template root");
		OnControlTemplatePresentationChanged();
	}
	catch (...)
	{
		const auto originalError = std::current_exception();
		try { (void)DetachVisualChildTemplateRoot(); }
		catch (...) {}
		std::rethrow_exception(originalError);
	}
	RefreshItemsScrollOwner();
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
		ClearPendingTemplateItemsPresenter();
		ClearDeclarativeTemplateScope();
		if (host) PlaceItemsHost(std::move(host));
		MarkControlTemplateRootDetached();
		OnControlTemplatePresentationChanged();
		RequestLayout();
		InvalidateVisual();
		return {};
	}
	auto* previous = _controlTemplateRoot;
	const ControlWeakReference previousLifetime(previous);
	const ControlWeakReference presenterLifetime(
		_templateItemsPresenter);
	std::unique_ptr<Control> root;
	std::exception_ptr notificationError;
	bool visualOwnershipCommit = false;
	try
	{
		DirectVisualMutationFrame mutation(*this, nullptr);
		root = cui::framework::TreeAccess::DetachVisualChild(
			*this, previous, &visualOwnershipCommit,
			&notificationError);
	}
	catch (...)
	{
		auto* live = previousLifetime.Get();
		if (live && live->GetVisualParent() == this
			&& IndexOfVisualChild(live) >= 0)
		{
			_controlTemplateRoot = live;
			MarkControlTemplateRootAttached();
		}
		throw;
	}

	auto* livePrevious = previousLifetime.Get();
	if (livePrevious && livePrevious->GetVisualParent() == this
		&& IndexOfVisualChild(livePrevious) >= 0)
	{
		_controlTemplateRoot = livePrevious;
		MarkControlTemplateRootAttached();
		if (notificationError)
			std::rethrow_exception(notificationError);
		throw std::logic_error(
			"ItemsControl template root detach did not commit");
	}

	std::exception_ptr cleanupError;
	std::unique_ptr<Panel> host;
	auto* presenter = dynamic_cast<ItemsPresenter*>(
		presenterLifetime.Get());
	if (presenter && _templateItemsPresenter == presenter)
	{
		auto* presenterHost = presenter->GetItemsHost();
		if (presenterHost && presenterHost == _itemsHost
			&& presenterHost->GetVisualParent() == presenter)
		{
			try { host = presenter->DetachItemsHost(); }
			catch (...)
			{
				cleanupError = std::current_exception();
			}
		}
	}
	if (!host && _detachedItemsHost)
		host = std::move(_detachedItemsHost);
	_itemsPresenterParentChanged.Disconnect();
	_templateItemsPresenter = nullptr;
	ClearPendingTemplateItemsPresenter();
	_itemsHost = nullptr;
	_controlTemplateRoot = nullptr;
	auto ownerError = root
		? ClearTemplateOwnerSubtreePreservingOwnership(root, this)
		: ClearTemplateOwnerSubtree(livePrevious, this);
	if (!cleanupError) cleanupError = ownerError;
	try { ClearDeclarativeTemplateScope(); }
	catch (...)
	{
		if (!cleanupError) cleanupError = std::current_exception();
	}
	if (host)
	{
		try { PlaceItemsHost(std::move(host)); }
		catch (...)
		{
			if (!cleanupError) cleanupError = std::current_exception();
		}
	}
	MarkControlTemplateRootDetached();
	try { OnControlTemplatePresentationChanged(); }
	catch (...)
	{
		if (!cleanupError) cleanupError = std::current_exception();
	}
	RequestLayout();
	InvalidateVisual();
	if (!cleanupError) cleanupError = notificationError;
	if (cleanupError)
		std::rethrow_exception(cleanupError);
	return root;
}

void ItemsControl::SetItemsPanel(ItemsPanelTemplateReference value)
{
	if (_migratingAuthoredItems)
		throw std::logic_error(
			"ItemsControl does not support reentrant ItemsHost migration");
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

	if (!_authoredItems.empty())
	{
		std::vector<ControlWeakReference> authoredItems;
		std::vector<Control*> restoredItems;
		authoredItems.reserve(_authoredItems.size());
		restoredItems.reserve(_authoredItems.size());
		for (auto* item : _authoredItems)
			authoredItems.emplace_back(item);

		_migratingAuthoredItems = true;
		struct AuthoredMigrationGuard final
		{
			bool& Value;
			~AuthoredMigrationGuard() { Value = false; }
		} migrationGuard{ _migratingAuthoredItems };
		// Raw authored slots are unpublished for the complete callback-bearing
		// transaction. They are reconstructed only from a verified final host.
		_authoredItems.clear();
		auto oldGenerator = std::move(_generator);
		std::unique_ptr<Panel> oldHost;
		std::unique_ptr<Control> inFlight;
		try
		{
			oldHost = TakeItemsHost();
			if (!oldHost)
				throw std::logic_error(
					"ItemsControl authored ItemsHost ownership is invalid");
			const ControlWeakReference oldHostLifetime(oldHost.get());
			if (!cui::framework::TreeAccess::
				InvokePreservingVisualOwnership(
					oldHost,
					[&](Control&)
					{
						PlaceItemsHost(std::move(replacement));
					}))
				throw std::logic_error(
					"ItemsControl previous ItemsHost ownership escaped during migration");
			auto* replacementHost = _itemsHost;
			const ControlWeakReference replacementHostLifetime(
				replacementHost);
			if (!replacementHost)
				throw std::logic_error(
					"ItemsControl replacement ItemsHost did not commit");

			for (const auto& itemReference : authoredItems)
			{
				std::exception_ptr notificationError;
				const bool retainedOldHost =
					cui::framework::TreeAccess::
					InvokePreservingVisualOwnership(
						oldHost,
						[&](Control& previousRoot)
						{
							auto* previousHost =
								dynamic_cast<Panel*>(&previousRoot);
							auto* nextHost = dynamic_cast<Panel*>(
								replacementHostLifetime.Get());
							auto* liveItem = itemReference.Get();
							if (!previousHost || !nextHost
								|| _itemsHost != nextHost || !liveItem
								|| liveItem->GetVisualParent()
								!= previousHost
								|| previousHost->IndexOfVisualChild(
									liveItem) < 0)
								throw std::logic_error(
									"ItemsControl authored item ownership changed during migration");
							bool ownershipCommit = false;
							inFlight =
								cui::framework::TreeAccess::DetachVisualChild(
									*previousHost, liveItem,
									&ownershipCommit,
									&notificationError);
							if (!inFlight
								|| inFlight.get() != itemReference.Get())
								throw std::logic_error(
									"ItemsControl authored item detach did not commit");
							(void)cui::framework::TreeAccess::
								InsertOwnedVisualChildPreserving(
									*nextHost,
									nextHost->VisualChildCount(),
									inFlight, this);
							liveItem = itemReference.Get();
							if (!liveItem
								|| liveItem->GetVisualParent() != nextHost
								|| nextHost->IndexOfVisualChild(
									liveItem) < 0
								|| liveItem->GetLogicalParent() != this)
								throw std::logic_error(
									"ItemsControl authored item attachment did not commit");
							if (notificationError)
								std::rethrow_exception(notificationError);
						});
				if (!retainedOldHost)
					throw std::logic_error(
						"ItemsControl previous ItemsHost was transferred during migration");
			}

			auto* liveReplacement = dynamic_cast<Panel*>(
				replacementHostLifetime.Get());
			auto* liveOldHost = dynamic_cast<Panel*>(
				oldHostLifetime.Get());
			if (!liveReplacement || _itemsHost != liveReplacement
				|| !liveOldHost || oldHost.get() != liveOldHost
				|| liveReplacement->VisualChildCount()
				!= static_cast<int>(authoredItems.size()))
				throw std::logic_error(
					"ItemsControl authored migration final host is invalid");
			const auto children =
				liveReplacement->GetVisualChildrenView();
			for (size_t index = 0;
				index < authoredItems.size(); ++index)
			{
				auto* liveItem = authoredItems[index].Get();
				if (!liveItem || children[index] != liveItem
					|| liveItem->GetVisualParent() != liveReplacement
					|| liveItem->GetLogicalParent() != this)
					throw std::logic_error(
						"ItemsControl authored migration order did not commit");
				restoredItems.push_back(liveItem);
			}
			_authoredItems = std::move(restoredItems);
			_lastTemplateError.clear();
			RequestLayout();
			InvalidateVisual();
			return true;
		}
		catch (...)
		{
			// Put an interrupted detach back first; never carry an owner token
			// across later callback-bearing rollback operations.
			if (inFlight && oldHost)
			{
				try
				{
					(void)cui::framework::TreeAccess::
						InvokePreservingVisualOwnership(
							oldHost,
							[&](Control& previousRoot)
							{
								(void)cui::framework::TreeAccess::
									InsertOwnedVisualChildPreserving(
										previousRoot,
										previousRoot.VisualChildCount(),
										inFlight, this);
							});
				}
				catch (...) {}
			}
			inFlight.reset();

			for (const auto& itemReference : authoredItems)
			{
				if (!oldHost) break;
				try
				{
					(void)cui::framework::TreeAccess::
						InvokePreservingVisualOwnership(
							oldHost,
							[&](Control& previousRoot)
							{
								auto* previousHost =
									dynamic_cast<Panel*>(&previousRoot);
								auto* liveItem = itemReference.Get();
								if (!previousHost || !liveItem
									|| (liveItem->GetVisualParent()
										== previousHost
										&& previousHost->
										IndexOfVisualChild(liveItem) >= 0))
									return;
								auto* currentHost = _itemsHost;
								if (!currentHost
									|| liveItem->GetVisualParent()
									!= currentHost
									|| currentHost->IndexOfVisualChild(
										liveItem) < 0)
									return;
								bool ownershipCommit = false;
								std::exception_ptr ignoredNotificationError;
								auto owner =
									cui::framework::TreeAccess::
									DetachVisualChild(
										*currentHost, liveItem,
										&ownershipCommit,
										&ignoredNotificationError);
								if (!owner) return;
								try
								{
									(void)cui::framework::TreeAccess::
										InsertOwnedVisualChildPreserving(
											*previousHost,
											previousHost->VisualChildCount(),
											owner, this);
								}
								catch (...) {}
							});
				}
				catch (...) {}
			}
			if (oldHost)
			{
				try
				{
					(void)cui::framework::TreeAccess::
						InvokePreservingVisualOwnership(
							oldHost,
							[&](Control& previousRoot)
							{
								auto* previousHost =
									dynamic_cast<Panel*>(&previousRoot);
								if (!previousHost) return;
								size_t targetIndex = 0;
								for (const auto& itemReference
									: authoredItems)
								{
									auto* liveItem = itemReference.Get();
									if (!liveItem
										|| liveItem->GetVisualParent()
										!= previousHost)
										continue;
									const int currentIndex =
										previousHost->
										IndexOfVisualChild(liveItem);
									if (currentIndex >= 0
										&& currentIndex
										!= static_cast<int>(
											targetIndex))
										(void)previousHost->
											MoveVisualChild(
												currentIndex,
												static_cast<int>(
													targetIndex));
									++targetIndex;
								}
							});
				}
				catch (...) {}
			}

			try
			{
				if (oldHost)
				{
					(void)cui::framework::TreeAccess::
						InvokePreservingVisualOwnership(
							oldHost,
							[&](Control&)
							{
								auto failedHost = TakeItemsHost();
								(void)failedHost;
							});
				}
				else
				{
					auto failedHost = TakeItemsHost();
					(void)failedHost;
				}
			}
			catch (...) {}
			_itemsPanel = previousPanel;
			if (oldHost)
			{
				try { PlaceItemsHost(std::move(oldHost)); }
				catch (...) {}
			}
			_generator = std::move(oldGenerator);
			restoredItems.clear();
			auto* restoredHost = _itemsHost;
			for (const auto& itemReference : authoredItems)
			{
				auto* liveItem = itemReference.Get();
				if (restoredHost && liveItem
					&& liveItem->GetVisualParent() == restoredHost
					&& restoredHost->IndexOfVisualChild(liveItem) >= 0
					&& liveItem->GetLogicalParent() == this)
					restoredItems.push_back(liveItem);
			}
			_authoredItems = std::move(restoredItems);
			_lastTemplateError = L"ItemsPanelTemplate 无法迁移 authored Items。";
			return false;
		}
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
	ResetTextSearch();
	--_itemsSourceUpdateDepth;
}

void ItemsControl::SetCustomProjectionItemsSource(BindingListReference value)
{
	if (value && !_authoredItems.empty())
		throw std::logic_error(
			"ItemsControl Items and ItemsSource cannot both be populated");
	_itemsSource = std::move(value);
	ResetTextSearch();
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
			ResetTextSearch();
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
	if (RebuildGeneratedItems())
	{
		ResetTextSearch();
		return;
	}
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

void ItemsControl::SetCompiledDisplayMemberPath(
	CompiledBindingPathView value)
{
	ValidateCompiledMemberPath(value, "DisplayMemberPath");
	if (SameCompiledBindingPath(_compiledDisplayMemberPath, value)
#if CUI_ENABLE_DYNAMIC_XAML
		&& _displayMemberPath.empty()
#endif
		) return;
	_compiledDisplayMemberPath = value;
#if CUI_ENABLE_DYNAMIC_XAML
	_displayMemberPath.clear();
#endif
	ResetTextSearch();
	if (_itemsSource && !_itemTemplate) (void)RebuildGeneratedItems();
}

std::wstring ItemsControl::GetDisplayMemberText(
	const BindingSourceReference& item) const
{
	if (!_compiledDisplayMemberPath.Empty())
		return GetBindingRecordText(item, _compiledDisplayMemberPath);
#if CUI_ENABLE_DYNAMIC_XAML
	return ReadAuthoredDisplayMemberText(item);
#else
	return {};
#endif
}

BindingPathObservation ItemsControl::ObserveDisplayMemberPath(
	const BindingSourceReference& item,
	std::function<void()> changed) const
{
	if (!_compiledDisplayMemberPath.Empty())
		return ObserveBindingPaths(
			item, { _compiledDisplayMemberPath }, std::move(changed));
#if CUI_ENABLE_DYNAMIC_XAML
	return ObserveAuthoredDisplayMemberPath(item, std::move(changed));
#else
	return {};
#endif
}

std::wstring ItemsControl::GetTextSearchItemText(size_t index) const
{
	if (index >= ItemCount()) return {};
	if (!_itemsSource)
	{
		auto* item = GetAuthoredItem(index);
		return item ? item->GetDisplayText() : std::wstring{};
	}

	BindingSourceReference item;
	if (!_itemsSource.Get()->TryGetItem(index, item) || !item) return {};
	auto result = GetDisplayMemberText(item);
	if (!result.empty()) return result;

	// An authored DataTemplate can expose a better semantic projection than a
	// record with no DisplayMemberPath. Use it when that item is already
	// realized, without forcing realization as a side effect of searching.
	auto* realized = GetGeneratedItem(index);
	return realized ? realized->GetDisplayText() : std::wstring{};
}

bool ItemsControl::ProcessTextSearchInput(
	const TextCompositionEventArgs& input)
{
	if (!GetIsTextSearchEnabled() || input.Text.empty()
		|| ItemCount() == 0) return false;

	const auto now = ::GetTickCount64();
	const auto timeout = static_cast<std::uint64_t>(
		::GetDoubleClickTime()) * 2u;
	if (_textSearchActive
		&& now - _textSearchLastInputTick > timeout)
		ResetTextSearch();

	const size_t count = ItemCount();
	const size_t start = _textSearchActive
		&& _textSearchMatchedIndex >= 0
		&& static_cast<size_t>(_textSearchMatchedIndex) < count
		? static_cast<size_t>(_textSearchMatchedIndex) : 0;
	const bool repeatedChunk = !_textSearchChunks.empty()
		&& TextEquals(
			_textSearchChunks.back(), input.Text, false);
	const auto newPrefix = _textSearchPrefix + input.Text;
	const bool caseSensitive = GetIsTextSearchCaseSensitive();
	size_t match = count;
	size_t fallback = count;

	for (size_t offset = 0; offset < count; ++offset)
	{
		const size_t index = (start + offset) % count;
		const auto itemText = GetTextSearchItemText(index);
		if (TextStartsWith(itemText, newPrefix, caseSensitive))
		{
			match = index;
			break;
		}
		if (repeatedChunk && offset != 0 && fallback == count
			&& TextStartsWith(
				itemText, _textSearchPrefix, caseSensitive))
			fallback = index;
	}

	const bool acceptedNewChunk = match != count;
	if (!acceptedNewChunk) match = fallback;
	if (match != count)
	{
		if (!_textSearchActive || match != start)
			OnTextSearchMatch(match);
		_textSearchMatchedIndex = static_cast<int>(match);
		if (acceptedNewChunk)
		{
			_textSearchPrefix = newPrefix;
			_textSearchChunks.push_back(input.Text);
		}
		_textSearchActive = true;
	}

	if (_textSearchActive)
		_textSearchLastInputTick = now;
	return match != count;
}

void ItemsControl::ProcessTextSearchKey(const InputReport& input)
{
	if (!GetIsTextSearchEnabled()
		|| input.Kind != InputReportKind::KeyDown
		|| input.Key != Key::Back
		|| !_textSearchActive) return;

	const auto now = ::GetTickCount64();
	const auto timeout = static_cast<std::uint64_t>(
		::GetDoubleClickTime()) * 2u;
	if (now - _textSearchLastInputTick > timeout)
	{
		ResetTextSearch();
		return;
	}
	if (_textSearchChunks.empty()) return;
	const auto chunkLength = _textSearchChunks.back().size();
	_textSearchChunks.pop_back();
	if (chunkLength <= _textSearchPrefix.size())
		_textSearchPrefix.resize(_textSearchPrefix.size() - chunkLength);
	else
		_textSearchPrefix.clear();
	_textSearchLastInputTick = now;
}

void ItemsControl::ResetTextSearch() noexcept
{
	_textSearchPrefix.clear();
	_textSearchChunks.clear();
	_textSearchMatchedIndex = -1;
	_textSearchLastInputTick = 0;
	_textSearchActive = false;
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

bool ItemsControl::ClearGroupedItemLogicalParentPreservingOwnership(
	std::unique_ptr<Control>& visual)
{
	if (!visual || !dynamic_cast<GroupedItemHost*>(visual.get()))
		return true;
	return cui::framework::TreeAccess::InvokePreservingVisualOwnership(
		visual,
		[](Control& root)
		{
			auto* logicalItem = UnwrapGeneratedItem(&root);
			if (logicalItem && logicalItem != &root)
				cui::framework::TreeAccess::SetLogicalParent(
					*logicalItem, nullptr);
		});
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
	ResetTextSearch();
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
	if (_migratingAuthoredItems)
		throw std::logic_error(
			"ItemsControl does not support authored-item mutation during ItemsHost migration");
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
	const ControlWeakReference lifetime(item);
	bool structuralCommit = false;
	try
	{
		cui::framework::TreeAccess::InsertVisualChild(
			*_itemsHost, static_cast<int>(index), item, this,
			&structuralCommit);
	}
	catch (...)
	{
		auto* live = lifetime.Get();
		if (live && _itemsHost
			&& _itemsHost->IndexOfVisualChild(live) >= 0)
		{
			auto detached = _itemsHost->DetachVisualChild(live);
			if (detached.get() == live)
				(void)detached.release();
		}
		live = lifetime.Get();
		if (live && !live->GetVisualParent()
			&& live->GetLogicalParent() == this)
			cui::framework::TreeAccess::SetLogicalParent(
				*live, nullptr);
		throw;
	}
	auto* live = lifetime.Get();
	if (!live || !_itemsHost
		|| live->GetVisualParent() != _itemsHost
		|| _itemsHost->IndexOfVisualChild(live) < 0
		|| live->GetLogicalParent() != this)
		throw std::logic_error(
			structuralCommit
				? "ItemsControl item ownership changed during insertion"
				: "ItemsControl item insertion did not commit");
	_authoredItems.insert(_authoredItems.begin() + index, live);
	NotifyAuthoredItemsChanged();
	return live;
}

Control* ItemsControl::InsertItemControl(
	size_t index, std::unique_ptr<Control> item)
{
	if (_migratingAuthoredItems)
		throw std::logic_error(
			"ItemsControl does not support authored-item mutation during ItemsHost migration");
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

	auto* raw = item.get();
	const ControlWeakReference lifetime(raw);
	try
	{
		(void)cui::framework::TreeAccess::
			InsertOwnedVisualChildPreserving(
				*_itemsHost, static_cast<int>(index),
				item, this);
	}
	catch (...)
	{
		auto* live = lifetime.Get();
		if (live && _itemsHost
			&& _itemsHost->IndexOfVisualChild(live) >= 0)
		{
			bool ownershipCommit = false;
			std::exception_ptr ignoredNotificationError;
			auto recovered =
				cui::framework::TreeAccess::DetachVisualChild(
					*_itemsHost, live, &ownershipCommit,
					&ignoredNotificationError);
			if (recovered)
				item.reset(recovered.release());
		}
		throw;
	}
	auto* live = lifetime.Get();
	if (!live || !_itemsHost
		|| live->GetVisualParent() != _itemsHost
		|| _itemsHost->IndexOfVisualChild(live) < 0
		|| live->GetLogicalParent() != this)
		throw std::logic_error(
			"ItemsControl owned item was transferred during insertion");
	_authoredItems.insert(_authoredItems.begin() + index, live);
	NotifyAuthoredItemsChanged();
	return live;
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
	if (_migratingAuthoredItems)
		throw std::logic_error(
			"ItemsControl does not support authored-item mutation during ItemsHost migration");
	if (index >= _authoredItems.size() || !_itemsHost) return {};
	auto* raw = _authoredItems[index];
	const ControlWeakReference lifetime(raw);
	auto* sourceHost = _itemsHost;
	auto result = sourceHost->DetachVisualChild(raw);
	auto* live = lifetime.Get();
	if (live && live->GetVisualParent() == sourceHost
		&& sourceHost->IndexOfVisualChild(live) >= 0)
		return {};
	// The visual removal has committed even when a callback transferred or
	// destroyed the item and no owner token can be returned.
	_authoredItems.erase(_authoredItems.begin() + index);
	std::exception_ptr parentError;
	if (result)
	{
		try
		{
			(void)cui::framework::TreeAccess::
				SetLogicalParentPreservingOwnership(
					result, nullptr, &parentError);
		}
		catch (...)
		{
			parentError = std::current_exception();
		}
	}
	try { NotifyAuthoredItemsChanged(); }
	catch (...)
	{
		if (!parentError) parentError = std::current_exception();
	}
	if (parentError)
		std::rethrow_exception(parentError);
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
	if (_migratingAuthoredItems)
		throw std::logic_error(
			"ItemsControl does not support authored-item mutation during ItemsHost migration");
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
	if (_migratingAuthoredItems)
		throw std::logic_error(
			"ItemsControl does not support authored-item mutation during ItemsHost migration");
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
#if CUI_ENABLE_DYNAMIC_XAML
		auto groupItems = std::make_shared<ObservableBindingList>(
			cui::design::AuthoredBindingListItemTypeName(*_itemsSource.Get()));
		groupItems->SetItemTypeToken(_itemsSource.Get()->GetItemTypeToken());
#else
		auto groupItems = std::make_shared<ObservableBindingList>(
			_itemsSource.Get()->GetItemTypeToken());
#endif
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
	bool initialized = false;
	if (!cui::framework::TreeAccess::InvokePreservingVisualOwnership(
		visual,
		[&](Control& container)
		{
			initialized = InitializeGeneratedContainer(container);
		}))
		throw std::logic_error(
			"generated container ownership changed during initialization");
	if (!initialized) return false;
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
		*presenter, Control::VerticalAlignmentProperty(),
		BindingValue(VerticalAlignment::Top),
		DependencyPropertyValueSource::Theme);
	cui::framework::StyleAccess::SetResourceKey(
		*presenter, _itemContainerStyle);
	presenter->SetContentTypeToken(_itemTemplate
		? _itemTemplate.Get()->GetDataTypeToken()
		: (_itemsSource ? _itemsSource.Get()->GetItemTypeToken()
			: DataTypeToken{}));
	presenter->SetCompiledDisplayMemberPath(_compiledDisplayMemberPath);
#if CUI_ENABLE_DYNAMIC_XAML
	ApplyAuthoredGeneratedItemProjection(*presenter);
#endif
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
	if (!_itemsHost)
		throw std::logic_error("ItemsControl has no ItemsHost");
	if (!item.Visual)
		throw std::invalid_argument(
			"ItemsControl prepared item has no visual");
	const auto index = item.Index;
	auto* requestedHost = _itemsHost;
	auto* requestedVisual = item.Visual.get();
	const bool groupedVisual =
		dynamic_cast<GroupedItemHost*>(requestedVisual) != nullptr;
	auto* requestedLogicalItem = groupedVisual
		? UnwrapGeneratedItem(requestedVisual) : requestedVisual;
	const ControlWeakReference hostLifetime(requestedHost);
	const ControlWeakReference visualLifetime(requestedVisual);
	const ControlWeakReference logicalItemLifetime(requestedLogicalItem);
	auto validateAttachment = [&]() -> std::pair<Panel*, Control*>
	{
		auto* liveHost = dynamic_cast<Panel*>(hostLifetime.Get());
		auto* liveVisual = visualLifetime.Get();
		auto* liveLogicalItem = logicalItemLifetime.Get();
		const bool rootOwned = liveHost && _itemsHost == liveHost
			&& liveVisual
			&& liveVisual->GetVisualParent() == liveHost
			&& liveHost->IndexOfVisualChild(liveVisual) >= 0;
		const bool logicalOwned = groupedVisual
			? liveLogicalItem
				&& liveLogicalItem != liveVisual
				&& UnwrapGeneratedItem(liveVisual) == liveLogicalItem
				&& liveLogicalItem->GetLogicalParent() == this
			: liveLogicalItem == liveVisual
				&& liveVisual
				&& liveVisual->GetLogicalParent() == this;
		if (!rootOwned || !logicalOwned)
			throw std::logic_error(
				"ItemsControl prepared item ownership did not commit");
		return { liveHost, liveVisual };
	};

	try
	{
		(void)cui::framework::TreeAccess::
			InsertOwnedVisualChildPreserving(
				*requestedHost, requestedHost->VisualChildCount(),
				item.Visual, groupedVisual ? nullptr : this);
		auto [liveHost, liveVisual] = validateAttachment();

		// Generated containers may be realized after the document-wide style
		// pass. Re-resolve only after the live inheritance route exists.
		if (!cui::framework::StyleAccess::Theme(*liveVisual))
			if (const auto theme =
				cui::framework::StyleAccess::Theme(*this))
				(void)cui::framework::StyleAccess::SetTheme(
					*liveVisual, std::move(theme), true);
		std::tie(liveHost, liveVisual) = validateAttachment();
		if (!cui::framework::StyleAccess::DocumentStyles(*liveVisual))
			if (const auto styles =
				cui::framework::StyleAccess::DocumentStyles(*this))
				(void)cui::framework::StyleAccess::SetDocumentStyles(
					*liveVisual, std::move(styles), true);
		std::tie(liveHost, liveVisual) = validateAttachment();
		if (auto* virtualHost =
			dynamic_cast<VirtualizingItemsHost*>(liveHost))
		{
			virtualHost->RegisterItem(liveVisual, index);
			std::tie(liveHost, liveVisual) = validateAttachment();
			virtualHost = dynamic_cast<VirtualizingItemsHost*>(liveHost);
			if (!virtualHost || virtualHost->IndexOf(liveVisual) != index)
				throw std::logic_error(
					"ItemsControl virtual item registration did not commit");
		}
		_generator.StoreRealized(
			index, liveVisual, std::move(item.Observation));
		if (_generator.GetRealized(index) != visualLifetime.Get())
			throw std::logic_error(
				"ItemsControl generator realization did not commit");
	}
	catch (...)
	{
		const auto originalError = std::current_exception();
		if (_generator.GetRealized(index) == requestedVisual)
			(void)_generator.TakeRealized(index);

		auto* liveHost = dynamic_cast<Panel*>(hostLifetime.Get());
		auto* liveVisual = visualLifetime.Get();
		if (auto* virtualHost =
			dynamic_cast<VirtualizingItemsHost*>(liveHost))
			virtualHost->UnregisterItem(requestedVisual);
		if (liveHost && liveVisual
			&& liveVisual->GetVisualParent() == liveHost
			&& liveHost->IndexOfVisualChild(liveVisual) >= 0)
		{
			try
			{
				bool ownershipCommit = false;
				std::exception_ptr ignoredNotificationError;
				auto recovered =
					cui::framework::TreeAccess::DetachVisualChild(
						*liveHost, liveVisual, &ownershipCommit,
						&ignoredNotificationError);
				if (recovered)
				{
					try
					{
						(void)ClearGroupedItemLogicalParentPreservingOwnership(
							recovered);
						std::exception_ptr ignoredParentError;
						if (recovered)
							(void)cui::framework::TreeAccess::
								SetLogicalParentPreservingOwnership(
									recovered, nullptr,
									&ignoredParentError);
					}
					catch (...) {}
				}
			}
			catch (...) {}
		}
		std::rethrow_exception(originalError);
	}
}

void ItemsControl::ReorderRealizedChildren()
{
	if (!_itemsHost || IsVirtualizing()
		|| _generator.RealizedCount() < 2) return;
	struct RealizedOrderEntry final
	{
		size_t Index = 0;
		Control* Identity = nullptr;
		ControlWeakReference Lifetime;
	};

	auto* requestedHost = _itemsHost;
	const ControlWeakReference hostLifetime(requestedHost);
	std::vector<RealizedOrderEntry> desired;
	std::vector<ControlWeakReference> previousOrder;
	std::unordered_set<Control*> identities;
	desired.reserve(_generator.RealizedCount());
	previousOrder.reserve(
		static_cast<size_t>(requestedHost->VisualChildCount()));
	identities.reserve(_generator.RealizedCount());
	for (const auto& [index, item] : _generator.RealizedItems())
	{
		auto* visual = item.Visual;
		if (!visual || !identities.insert(visual).second)
			throw std::logic_error(
				"ItemsControl generator contains an invalid realized identity");
		const ControlWeakReference lifetime(visual);
		if (lifetime.Get() != visual
			|| visual->GetVisualParent() != requestedHost
			|| requestedHost->IndexOfVisualChild(visual) < 0)
			throw std::logic_error(
				"ItemsControl generator is not owned by its ItemsHost");
		desired.push_back({ index, visual, lifetime });
	}
	for (auto* child : requestedHost->GetVisualChildrenView())
		previousOrder.emplace_back(child);
	if (previousOrder.size() != desired.size())
		throw std::logic_error(
			"ItemsControl ItemsHost contains untracked realized children");

	try
	{
		for (size_t targetIndex = 0;
			targetIndex < desired.size(); ++targetIndex)
		{
			auto* liveHost = dynamic_cast<Panel*>(hostLifetime.Get());
			auto* liveVisual = desired[targetIndex].Lifetime.Get();
			if (!liveHost || _itemsHost != liveHost
				|| !liveVisual
				|| liveVisual->GetVisualParent() != liveHost)
				throw std::logic_error(
					"ItemsControl realized ownership changed during reorder");
			const int currentIndex =
				liveHost->IndexOfVisualChild(liveVisual);
			if (currentIndex < 0)
				throw std::logic_error(
					"ItemsControl realized child escaped during reorder");
			if (currentIndex != static_cast<int>(targetIndex)
				&& !liveHost->MoveVisualChild(
					currentIndex, static_cast<int>(targetIndex)))
				throw std::logic_error(
					"ItemsControl realized child could not be reordered");
		}

		auto* liveHost = dynamic_cast<Panel*>(hostLifetime.Get());
		if (!liveHost || _itemsHost != liveHost
			|| liveHost->VisualChildCount()
			!= static_cast<int>(desired.size()))
			throw std::logic_error(
				"ItemsControl ItemsHost changed during reorder");
		const auto children = liveHost->GetVisualChildrenView();
		for (size_t index = 0; index < desired.size(); ++index)
		{
			auto* live = desired[index].Lifetime.Get();
			if (!live || children[index] != live
				|| live->GetVisualParent() != liveHost)
				throw std::logic_error(
					"ItemsControl realized order did not commit");
		}
	}
	catch (...)
	{
		const auto originalError = std::current_exception();
		// Moving never relinquishes ownership. Restore the old order in place
		// when all captured identities still belong to the same host.
		try
		{
			auto* liveHost = dynamic_cast<Panel*>(hostLifetime.Get());
			bool canRestore = liveHost && _itemsHost == liveHost
				&& liveHost->VisualChildCount()
				== static_cast<int>(previousOrder.size());
			for (const auto& reference : previousOrder)
			{
				auto* child = reference.Get();
				canRestore = canRestore && child
					&& child->GetVisualParent() == liveHost
					&& liveHost->IndexOfVisualChild(child) >= 0;
			}
			if (canRestore)
			{
				for (size_t targetIndex = 0;
					targetIndex < previousOrder.size(); ++targetIndex)
				{
					auto* child = previousOrder[targetIndex].Get();
					const int currentIndex =
						liveHost->IndexOfVisualChild(child);
					if (currentIndex != static_cast<int>(targetIndex))
						(void)liveHost->MoveVisualChild(
							currentIndex,
							static_cast<int>(targetIndex));
				}
			}
		}
		catch (...) {}

		// A reentrant collection observer may still have transferred or
		// destroyed a visual. Remove only those invalid identities so the
		// generator never publishes stale raw pointers.
		auto* liveHost = dynamic_cast<Panel*>(hostLifetime.Get());
		for (const auto& entry : desired)
		{
			if (_generator.GetRealized(entry.Index) != entry.Identity)
				continue;
			auto* live = entry.Lifetime.Get();
			if (!liveHost || _itemsHost != liveHost || !live
				|| live->GetVisualParent() != liveHost
				|| liveHost->IndexOfVisualChild(live) < 0)
				(void)_generator.TakeRealized(entry.Index);
		}
		std::rethrow_exception(originalError);
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
			(void)ClearGroupedItemLogicalParentPreservingOwnership(
				detached);
			std::exception_ptr parentError;
			if (detached)
				(void)cui::framework::TreeAccess::
					SetLogicalParentPreservingOwnership(
						detached, nullptr, &parentError);
			if (parentError)
				std::rethrow_exception(parentError);
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

bool ItemsControl::RealizeVirtualRange(
	size_t first, size_t last, bool localLayoutForScroll)
{
	if (!IsVirtualizing() || !_itemsHost || _realizingViewport) return true;
	const size_t count = ItemCount();
	first = (std::min)(first, count);
	last = (std::clamp)(last, first, count);
	auto* virtualHost = dynamic_cast<VirtualizingItemsHost*>(_itemsHost);
	const bool previousLocalLayout = virtualHost
		? virtualHost->SetLocalLayoutInvalidation(localLayoutForScroll)
		: false;
	struct LocalLayoutGuard final
	{
		VirtualizingItemsHost* Host;
		bool Previous;
		~LocalLayoutGuard()
		{
			if (Host) (void)Host->SetLocalLayoutInvalidation(Previous);
		}
	} localLayoutGuard{ virtualHost, previousLocalLayout };
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
		if (virtualHost)
			virtualHost->UnregisterItem(visual);
		auto detached = _itemsHost->DetachVisualChild(visual);
		if (detached)
		{
			(void)ClearGroupedItemLogicalParentPreservingOwnership(
				detached);
			std::exception_ptr parentError;
			if (detached)
				(void)cui::framework::TreeAccess::
					SetLogicalParentPreservingOwnership(
						detached, nullptr, &parentError);
			if (parentError)
				std::rethrow_exception(parentError);
		}
		if (detached)
			_generator.StoreRecycled(index, {
				std::move(detached), std::move(item.Observation) });
	}
	for (auto& addition : additions)
		AttachPreparedItem(std::move(addition));
	TrimRecyclePool(first, last);
	_itemsHost->InvalidateLayout();
	if (localLayoutForScroll)
	{
		// The extent is unchanged, so a scroll-only realization can measure and
		// arrange its fixed host without scheduling a new root layout pass.
		_itemsHost->UpdateLayout();
		CommitItemsLayout();
	}
	else
		RequestLayout();
	OnGeneratedItemsRealized();
	_realizingViewport = false;
	return true;
}

bool ItemsControl::RealizeVirtualViewport(bool localLayoutForScroll)
{
	if (!IsVirtualizing()) return true;
	const auto [first, last] = VirtualRangeForViewport();
	return RealizeVirtualRange(first, last, localLayoutForScroll);
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
					(void)ClearGroupedItemLogicalParentPreservingOwnership(
						detached);
					std::exception_ptr parentError;
					if (detached)
						(void)cui::framework::TreeAccess::
							SetLogicalParentPreservingOwnership(
								detached, nullptr, &parentError);
					if (parentError)
						std::rethrow_exception(parentError);
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
		_itemsSource, CompiledBindingPathView{},
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
		? _itemsSource.Get()->GetItemTypeToken() : DataTypeToken{};
	if (_itemsSource && _itemTemplate
		&& !AreDataTypesCompatible(sourceItemType,
			_itemTemplate.Get()->GetDataTypeToken()))
	{
		_lastTemplateError = L"ItemTemplate DataType 与 ItemsSource ItemType 不一致。";
		return false;
	}
	if (_groupStyle)
	{
		const auto* style = _groupStyle.Get();
		if (style->HeaderTemplate
			&& style->HeaderTemplate.Get()->GetDataTypeToken()
				!= CollectionViewGroupDataTypeToken)
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
		(void)RealizeVirtualViewport(true);
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

void ItemsControl::OnApplyTemplate()
{
	if (_pendingTemplateItemsPresenter.HasValue()
		&& !CommitPendingTemplateItemsPresenter())
		ClearPendingTemplateItemsPresenter();
	if (_templateItemsPresenter
		&& (!_controlTemplateRoot
			|| !VisualSubtreeContains(
				_controlTemplateRoot, _templateItemsPresenter)))
		throw std::logic_error(
			"ItemsControl active ItemsPresenter is outside the applied template root");
	Control::OnApplyTemplate();
}

void ItemsControl::PreparePresentation()
{
	if (_pendingTemplateItemsPresenter.HasValue())
		(void)CommitPendingTemplateItemsPresenter();
	Control::PreparePresentation();
	if (IsVirtualizing()) (void)RealizeVirtualViewport();
}

bool ItemsControl::ProcessInput(const InputReport& input)
{
	if (input.Kind == InputReportKind::FocusLost)
		ResetTextSearch();
	ProcessTextSearchKey(input);
	return Control::ProcessInput(input);
}

bool ItemsControl::ApplyTextInput(
	const TextCompositionEventArgs& input)
{
	if (!GetIsTextSearchEnabled() || input.Text.empty())
		return Control::ApplyTextInput(input);
	(void)ProcessTextSearchInput(input);
	// WPF marks committed text handled once text search is enabled even when
	// the current prefix has no matching item, preventing access-key fallback.
	return true;
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
