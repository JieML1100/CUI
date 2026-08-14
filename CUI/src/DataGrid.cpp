#define NOMINMAX
#include "DataGrid.h"

#include "AutomationPeer.h"
#include "DependencyPropertyInfrastructure.h"
#include "EventInfrastructure.h"
#include "ScrollViewer.h"
#include "StyleInfrastructure.h"
#include "Window.h"

#include <algorithm>
#include <cmath>
#include <cwchar>
#include <cwctype>
#include <exception>
#include <iterator>
#include <map>
#include <stdexcept>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace
{
	template<typename TCallback>
	class ScopeExit final
	{
	public:
		explicit ScopeExit(TCallback callback)
			: _callback(std::move(callback)) {}
		ScopeExit(const ScopeExit&) = delete;
		ScopeExit& operator=(const ScopeExit&) = delete;
		~ScopeExit() { _callback(); }

	private:
		TCallback _callback;
	};

	template<typename TCallback>
	auto MakeScopeExit(TCallback&& callback)
	{
		return ScopeExit<std::decay_t<TCallback>>(
			std::forward<TCallback>(callback));
	}

	class DataGridDisplayCheckBox final : public CheckBox
	{
	public:
		bool ParticipatesInInputHitTesting() const noexcept override
		{
			return false;
		}
	};

	constexpr double DataGridColumnHeaderGripperWidth = 8.0;

	GridLength SparseTrackLength(double value) noexcept
	{
		if (!std::isfinite(value) || value <= 0.0)
			return GridLength::Pixels(0.0f);
		return GridLength::Pixels(static_cast<float>((std::min)(value,
			static_cast<double>((std::numeric_limits<float>::max)()))));
	}

	double RenderSpaceX(const Control& control, int localX) noexcept
	{
		const auto raw = control.GetLocalToRenderTransform();
		return D2D1::Matrix3x2F(
			raw._11, raw._12, raw._21, raw._22, raw._31, raw._32)
			.TransformPoint(D2D1::Point2F(static_cast<float>(localX), 0.0f)).x;
	}

	D2D1_POINT_2F ToRenderSpace(
		const Control& control, float localX, float localY) noexcept
	{
		const auto raw = control.GetLocalToRenderTransform();
		return D2D1::Matrix3x2F(
			raw._11, raw._12, raw._21, raw._22, raw._31, raw._32)
			.TransformPoint(D2D1::Point2F(localX, localY));
	}

	bool IsPointInside(
		const D2D1_RECT_F& rect, D2D1_POINT_2F point) noexcept
	{
		return point.x >= rect.left && point.x < rect.right
			&& point.y >= rect.top && point.y < rect.bottom;
	}

	bool PublishAccessibilityBounds(
		Control* control, AccessibilityVirtualNode& node)
	{
		if (!control)
		{
			node.BoundsDip = {};
			node.BoundsAreRenderSpace = true;
			node.Visible = false;
			return false;
		}
		node.BoundsDip = control->GetRenderedAbsoluteRectDip();
		node.BoundsAreRenderSpace = true;
		const float width = node.BoundsDip.right - node.BoundsDip.left;
		const float height = node.BoundsDip.bottom - node.BoundsDip.top;
		if (!control->GetIsVisible() || width <= 0.0f || height <= 0.0f)
		{
			node.Visible = false;
			return false;
		}
		const auto center = D2D1::Point2F(
			(node.BoundsDip.left + node.BoundsDip.right) * 0.5f,
			(node.BoundsDip.top + node.BoundsDip.bottom) * 0.5f);
		const auto topLeft = D2D1::Point2F(
			node.BoundsDip.left + 0.5f, node.BoundsDip.top + 0.5f);
		const auto bottomRight = D2D1::Point2F(
			node.BoundsDip.right - 0.5f, node.BoundsDip.bottom - 0.5f);
		node.Visible = control->IsRenderPointInsideClip(center)
			|| control->IsRenderPointInsideClip(topLeft)
			|| control->IsRenderPointInsideClip(bottomRight);
		return node.Visible;
	}

	const ItemsPanelTemplateReference& DefaultDataGridRowsPanel()
	{
		static const auto definition = []
		{
			auto value = std::make_shared<ItemsPanelTemplate>();
			value->Kind = ItemsPanelKind::VirtualizingStack;
			value->Orientation = Orientation::Vertical;
			value->ItemHeight = 38.0f;
			value->CacheLength = 1.0f;
			return ItemsPanelTemplateReference(std::move(value));
		}();
		return definition;
	}

	std::wstring_view Trim(std::wstring_view value) noexcept
	{
		while (!value.empty() && std::iswspace(value.front()))
			value.remove_prefix(1);
		while (!value.empty() && std::iswspace(value.back()))
			value.remove_suffix(1);
		return value;
	}

	bool EqualsIgnoreCase(
		std::wstring_view left, std::wstring_view right) noexcept
	{
		if (left.size() != right.size()) return false;
		for (size_t index = 0; index < left.size(); ++index)
		{
			if (std::towlower(left[index]) != std::towlower(right[index]))
				return false;
		}
		return true;
	}

	std::optional<cui::drawing::Brush> ConvertBrush(
		const BindingValue& value)
	{
		cui::drawing::Brush brush;
		if (value.TryGet(brush)) return brush;
		D2D1_COLOR_F color{};
		if (value.TryGet(color))
			return cui::drawing::MakeSolidColorBrush(color);
		return std::nullopt;
	}

	template<typename TValue>
	DependencyPropertyOptions<DataGrid, TValue> DataGridOptions(
		TValue defaultValue,
		DependencyPropertyFlags flags = DependencyPropertyFlags::None)
	{
		DependencyPropertyOptions<DataGrid, TValue> options;
		options.DefaultValue = std::move(defaultValue);
		options.Flags = flags;
		return options;
	}

	bool HasColumnHeaders(DataGridHeadersVisibility value) noexcept
	{
		return (static_cast<int>(value)
			& static_cast<int>(DataGridHeadersVisibility::Column)) != 0;
	}

	bool HasRowHeaders(DataGridHeadersVisibility value) noexcept
	{
		return (static_cast<int>(value)
			& static_cast<int>(DataGridHeadersVisibility::Row)) != 0;
	}

	cui::drawing::Transform HorizontalTranslation(double offset)
	{
		cui::drawing::Transform transform;
		cui::drawing::TransformOperation operation;
		operation.Kind = cui::drawing::TransformKind::Translate;
		operation.X = static_cast<float>(offset);
		transform.Operations.push_back(operation);
		return transform;
	}

	bool HasHorizontalGridLines(DataGridGridLinesVisibility value) noexcept
	{
		return value == DataGridGridLinesVisibility::All
			|| value == DataGridGridLinesVisibility::Horizontal;
	}

	bool HasVerticalGridLines(DataGridGridLinesVisibility value) noexcept
	{
		return value == DataGridGridLinesVisibility::All
			|| value == DataGridGridLinesVisibility::Vertical;
	}

	bool HasSortPath(const CollectionSortDescription& description) noexcept
	{
#if CUI_ENABLE_DYNAMIC_XAML
		if (!description.PropertyName.empty()) return true;
#endif
		return !description.CompiledPath.Empty();
	}

	bool SameSortPath(
		const CollectionSortDescription& left,
		const CollectionSortDescription& right) noexcept
	{
#if CUI_ENABLE_DYNAMIC_XAML
		if (!left.PropertyName.empty() || !right.PropertyName.empty())
			return left.PropertyName == right.PropertyName;
#endif
		return SameCompiledCollectionPath(
			left.CompiledPath, right.CompiledPath);
	}

	bool SameCellIdentity(
		const DataGridCellInfo& left,
		const DataGridCellInfo& right) noexcept
	{
		return left == right;
	}

	bool SameCellItemAndColumn(
		const DataGridCellInfo& left,
		const DataGridCellInfo& right) noexcept
	{
		return SameCellIdentity(left, right);
	}

	bool SameCellCollection(
		const DataGridSelectedCellCollection& left,
		const DataGridSelectedCellCollection& right,
		bool ignoreLocators)
	{
		(void)ignoreLocators;
		return left.SameLogicalCells(right);
	}

	bool ContainsCellIdentity(
		const std::vector<DataGridCellInfo>& cells,
		const DataGridCellInfo& candidate) noexcept
	{
		return std::any_of(cells.begin(), cells.end(),
			[&candidate](const DataGridCellInfo& value)
			{ return SameCellIdentity(value, candidate); });
	}

	bool ContainsCellIdentity(
		const DataGridSelectedCellCollection& cells,
		const DataGridCellInfo& candidate) noexcept
	{
		return cells.Contains(candidate);
	}

	bool IsRoutedDescendantOrSelf(
		Control* element, Control* ancestor) noexcept
	{
		if (!element || !ancestor) return false;
		for (auto* current = element; current;
			current = current->GetRoutedParent())
		{
			if (current == ancestor) return true;
		}
		return false;
	}

	bool EditingElementOwnsKey(
		Key key, ModifierKeys modifiers) noexcept
	{
		if (key == Key::A
			&& HasModifier(modifiers, ModifierKeys::Control)) return true;
		return key == Key::Left || key == Key::Right
			|| key == Key::Up || key == Key::Down
			|| key == Key::Home || key == Key::End
			|| key == Key::Prior || key == Key::Next;
	}

	size_t RawOffsetForSelectedOrdinal(
		size_t selectedIndex,
		size_t regionSize,
		const std::vector<size_t>& excluded) noexcept
	{
		if (excluded.empty()) return selectedIndex;
		// The raw offset can move right by at most the number of exclusions.
		// Find the first prefix containing selectedIndex + 1 live cells so a
		// dense sparse-exclusion set remains O(log E) per indexed cell.
		size_t low = selectedIndex;
		const size_t maximumShift = (std::min)(
			excluded.size(), regionSize - 1 - selectedIndex);
		size_t high = selectedIndex + maximumShift;
		while (low < high)
		{
			const size_t middle = low + (high - low) / 2;
			const size_t excludedThrough = static_cast<size_t>(
				std::upper_bound(excluded.begin(), excluded.end(), middle)
					- excluded.begin());
			const size_t selectedThrough = middle + 1 - excludedThrough;
			if (selectedThrough > selectedIndex) high = middle;
			else low = middle + 1;
		}
		return low;
	}

	template<typename TInterval>
	size_t IntervalEndExclusive(const TInterval& interval) noexcept
	{
		return interval.Count > (std::numeric_limits<size_t>::max)()
			- interval.Start
			? (std::numeric_limits<size_t>::max)()
			: interval.Start + interval.Count;
	}

	template<typename TInterval>
	bool IsRowInIntervals(
		const std::vector<TInterval>& intervals, size_t row) noexcept
	{
		const auto after = std::upper_bound(
			intervals.begin(), intervals.end(), row,
			[](size_t value, const TInterval& interval)
			{ return value < interval.Start; });
		if (after == intervals.begin()) return false;
		const auto& interval = *std::prev(after);
		return row >= interval.Start
			&& row - interval.Start < interval.Count;
	}

	size_t CountIntervalRowsBefore(
		const std::vector<DataGridSelectedCellCollection::
			RegionExcludedOffsets::RowInterval>& intervals,
		size_t rawOffset) noexcept
	{
		if (rawOffset == 0 || intervals.empty()) return 0;
		const auto after = std::lower_bound(
			intervals.begin(), intervals.end(), rawOffset,
			[](const auto& interval, size_t value)
			{ return interval.Start < value; });
		if (after == intervals.begin()) return 0;
		const auto& interval = *std::prev(after);
		const size_t coveredInInterval = rawOffset <= interval.Start
			? 0 : (std::min)(interval.Count, rawOffset - interval.Start);
		return interval.ExcludedBefore + coveredInInterval;
	}

	size_t CountExcludedRowsBefore(
		const DataGridSelectedCellCollection::RegionExcludedOffsets& excluded,
		size_t rawOffset) noexcept
	{
		return CountIntervalRowsBefore(excluded.RowIntervals, rawOffset)
			+ static_cast<size_t>(std::lower_bound(
				excluded.Rows.begin(), excluded.Rows.end(), rawOffset)
				- excluded.Rows.begin());
	}

	bool IsProjectedRowExcluded(
		const DataGridSelectedCellCollection::RegionExcludedOffsets& excluded,
		size_t rawOffset) noexcept
	{
		return IsRowInIntervals(excluded.RowIntervals, rawOffset)
			|| std::binary_search(
				excluded.Rows.begin(), excluded.Rows.end(), rawOffset);
	}

	size_t RawRowOffsetForSelectedOrdinal(
		size_t selectedIndex, size_t projectedRows,
		const DataGridSelectedCellCollection::RegionExcludedOffsets& excluded)
		noexcept
	{
		const size_t intervalRows = excluded.RowIntervals.empty()
			? 0
			: excluded.RowIntervals.back().ExcludedBefore
				+ excluded.RowIntervals.back().Count;
		const size_t totalExcluded = intervalRows + excluded.Rows.size();
		if (totalExcluded == 0) return selectedIndex;
		size_t low = selectedIndex;
		const size_t maximumShift = (std::min)(
			totalExcluded, projectedRows - 1 - selectedIndex);
		size_t high = selectedIndex + maximumShift;
		while (low < high)
		{
			const size_t middle = low + (high - low) / 2;
			const size_t selectedThrough = middle + 1
				- CountExcludedRowsBefore(excluded, middle + 1);
			if (selectedThrough > selectedIndex) high = middle;
			else low = middle + 1;
		}
		return low;
	}

	const DependencyPropertyMetadataRegistration&
		DataGridCellFocusableMetadataRelation()
	{
		static const DependencyPropertyMetadataRegistration relation = []
		{
			const auto& property = Control::FocusableProperty();
			DependencyPropertyOptions<DataGridCell, bool> options;
			options.DefaultValue = true;
			CUI_DESIGN_METADATA_ONLY(
			const std::type_index ownerTypes[] = {
				std::type_index(typeid(Control))
			};
			const auto* base = DependencyPropertyRegistry::FindRegistered(
				ownerTypes, L"Focusable");
			if (!base)
				throw std::logic_error(
					"Control.Focusable must be registered before DataGridCell");
			options.Design = base->Design();
			)
			return DependencyPropertyRegistry::OverrideMetadataStatic<
				DataGridCell, ContentControl, bool>(
					property, std::move(options));
		}();
		return relation;
	}

	const DependencyPropertyMetadataRegistration&
		DataGridRowHeaderFocusableMetadataRelation()
	{
		static const DependencyPropertyMetadataRegistration relation = []
		{
			const auto& property = Control::FocusableProperty();
			DependencyPropertyOptions<DataGridRowHeader, bool> options;
			options.DefaultValue = false;
			CUI_DESIGN_METADATA_ONLY(
			const std::type_index ownerTypes[] = {
				std::type_index(typeid(Control))
			};
			const auto* base = DependencyPropertyRegistry::FindRegistered(
				ownerTypes, L"Focusable");
			if (!base)
				throw std::logic_error(
					"Control.Focusable must be registered before DataGridRowHeader");
			options.Design = base->Design();
			)
			return DependencyPropertyRegistry::OverrideMetadataStatic<
				DataGridRowHeader, Button, bool>(
					property, std::move(options));
		}();
		return relation;
	}

	const DependencyPropertyMetadataRegistration&
		DataGridRowHeaderClickModeMetadataRelation()
	{
		static const DependencyPropertyMetadataRegistration relation = []
		{
			const auto& property = ButtonBase::ClickModeProperty();
			DependencyPropertyOptions<DataGridRowHeader, ::ClickMode> options;
			options.DefaultValue = ::ClickMode::Press;
			CUI_DESIGN_METADATA_ONLY(
			const std::type_index ownerTypes[] = {
				std::type_index(typeid(ButtonBase))
			};
			const auto* base = DependencyPropertyRegistry::FindRegistered(
				ownerTypes, L"ClickMode");
			if (!base)
				throw std::logic_error(
					"ButtonBase.ClickMode must be registered before DataGridRowHeader");
			options.Design = base->Design();
			)
			return DependencyPropertyRegistry::OverrideMetadataStatic<
				DataGridRowHeader, Button, ::ClickMode>(
					property, std::move(options));
		}();
		return relation;
	}

}

/**
 * DataGrid rows already own one authoritative resolved-width prefix.  Feeding
 * those fixed widths back through a general Grid made every live row repeat
 * Auto/Star/span discovery and allocate a ColumnDefinition vector on each
 * resize frame.  This internal presenter keeps the real DataGridCell visual
 * children, but projects their known column rectangles in one linear pass.
 */
class DataGridCellsPresenter final : public Panel
{
private:
	class CellsLayoutEngine final : public LayoutEngine
	{
	public:
		explicit CellsLayoutEngine(DataGridCellsPresenter& owner) noexcept
			: _owner(owner) {}

		cui::core::Size Measure(
			LayoutContext& context,
			const cui::core::Constraints& available) override
		{
			auto* const presenter = &_owner;
			auto* const row = presenter->_row;
			auto* const owner = row ? row->GetDataGridOwner() : nullptr;
			if (!row || !owner || row->_cellsGrid != presenter)
			{
				_needsLayout = false;
				return {};
			}
			const size_t projectionRevision =
				owner->_columnWidthProjectionRevision;
			const ControlWeakReference presenterLifetime(presenter);
			const ControlWeakReference rowLifetime(row);
			const ControlWeakReference ownerLifetime(owner);
			const auto currentPresenter = [&]()
				-> DataGridCellsPresenter*
			{
				auto* livePresenter = dynamic_cast<DataGridCellsPresenter*>(
					presenterLifetime.Get());
				auto* liveRow = dynamic_cast<DataGridRow*>(rowLifetime.Get());
				auto* liveOwner = dynamic_cast<DataGrid*>(ownerLifetime.Get());
				return livePresenter && liveRow && liveOwner
					&& livePresenter == presenter
					&& livePresenter->_row == liveRow
					&& liveRow == row
					&& liveRow->_cellsGrid == livePresenter
					&& liveRow->GetDataGridOwner() == liveOwner
					&& liveOwner == owner
					&& liveOwner->_columnWidthProjectionRevision
						== projectionRevision
					? livePresenter : nullptr;
			};
			const auto maximum = available.Normalized().maximum;
			// A fixed DataGrid.RowHeight is the authoritative vertical measure.
			// Measuring every retained cell here is both unnecessary and expensive:
			// the outer row Grid probes its Auto column once unbounded and once with
			// the row slot, which otherwise alternates every cell template between
			// infinite and finite height on each column-resize frame. Arrange below
			// still measures cells against their final slots, so the resized column
			// updates content while unchanged columns retain their measure caches.
			const double fixedRowHeight = owner->GetRowHeight();
			if (std::isfinite(fixedRowHeight))
			{
				auto* livePresenter = currentPresenter();
				if (!livePresenter) return {};
				const float totalWidth = livePresenter->TotalColumnWidth();
				if (!currentPresenter()) return {};
				const double maximumFloat = static_cast<double>(
					(std::numeric_limits<float>::max)());
				float desiredHeight = static_cast<float>((std::clamp)(
					fixedRowHeight, 0.0, maximumFloat));
				if (std::isfinite(maximum.height))
					desiredHeight = (std::min)(desiredHeight, maximum.height);
				_needsLayout = false;
				return { totalWidth, desiredHeight };
			}
			struct CellSnapshot final
			{
				ControlWeakReference Lifetime;
				DataGridCell* Identity = nullptr;
				float Left = 0.0f;
				float Width = 0.0f;
			};
			std::vector<CellSnapshot> cells;
			const int childCount = context.ChildCount();
			cells.reserve(static_cast<size_t>((std::max)(0, childCount)));
			for (int childIndex = 0; childIndex < childCount; ++childIndex)
			{
				auto* livePresenter = currentPresenter();
				if (!livePresenter) return {};
				auto* cell = dynamic_cast<DataGridCell*>(
					context.ChildAt(childIndex));
				if (!cell || cell->IsCollapsed()) continue;
				CellSnapshot snapshot;
				snapshot.Lifetime = cell;
				snapshot.Identity = cell;
				const bool resolved = livePresenter->TryGetCellRect(
					cell, snapshot.Left, snapshot.Width);
				livePresenter = currentPresenter();
				auto* liveCell = dynamic_cast<DataGridCell*>(
					snapshot.Lifetime.Get());
				if (!livePresenter || liveCell != snapshot.Identity
					|| !livePresenter->IsCurrentCell(liveCell)) return {};
				if (resolved) cells.push_back(std::move(snapshot));
			}
			// An Auto DataGrid row must discover the wrapped cell's natural height.
			// The outer row Grid may offer the previous arranged height while a
			// column is being resized; feeding that finite value back here makes the
			// old height self-lock and prevents a narrower column from growing it.
			const float availableHeight = cui::core::Infinity;
			float desiredHeight = 0.0f;
			for (const auto& snapshot : cells)
			{
				auto* livePresenter = currentPresenter();
				auto* cell = dynamic_cast<DataGridCell*>(
					snapshot.Lifetime.Get());
				if (!livePresenter || cell != snapshot.Identity
					|| !livePresenter->IsCurrentCell(cell)) return {};
				if (cell->IsCollapsed()) continue;
				const Thickness margin = cell->Margin;
				const float childWidth = (std::max)(
					0.0f, snapshot.Width - margin.Left - margin.Right);
				const float childHeight = std::isfinite(availableHeight)
					? (std::max)(0.0f,
						availableHeight - margin.Top - margin.Bottom)
					: cui::core::Infinity;
				const bool widthChanged =
					owner->_columnWidthMeasureDirty.size()
						!= owner->ColumnCount()
					|| cell->_columnIndex
						>= owner->_columnWidthMeasureDirty.size()
					|| owner->_columnWidthMeasureDirty[cell->_columnIndex] != 0;
				const auto desired = widthChanged
					|| cell->GetComputedLayout().NeedsMeasure()
					? cell->Measure(cui::core::Constraints{
						cui::core::Size{ childWidth, childHeight } })
					: cell->GetDesiredSizeDip();
				livePresenter = currentPresenter();
				cell = dynamic_cast<DataGridCell*>(snapshot.Lifetime.Get());
				if (!livePresenter || cell != snapshot.Identity
					|| !livePresenter->IsCurrentCell(cell)) return {};
				desiredHeight = (std::max)(desiredHeight,
					desired.height + margin.Top + margin.Bottom);
			}
			auto* livePresenter = currentPresenter();
			if (!livePresenter) return {};
			const float totalWidth = livePresenter->TotalColumnWidth();
			if (!currentPresenter()) return {};
			_needsLayout = false;
			return { totalWidth, desiredHeight };
		}

		void Arrange(LayoutContext& context, cui::core::Rect finalRect) override
		{
			auto* const presenter = &_owner;
			auto* const row = presenter->_row;
			auto* const owner = row ? row->GetDataGridOwner() : nullptr;
			if (!row || !owner || row->_cellsGrid != presenter)
			{
				_needsLayout = false;
				return;
			}
			const size_t projectionRevision =
				owner->_columnWidthProjectionRevision;
			const ControlWeakReference presenterLifetime(presenter);
			const ControlWeakReference rowLifetime(row);
			const ControlWeakReference ownerLifetime(owner);
			const auto currentPresenter = [&]()
				-> DataGridCellsPresenter*
			{
				auto* livePresenter = dynamic_cast<DataGridCellsPresenter*>(
					presenterLifetime.Get());
				auto* liveRow = dynamic_cast<DataGridRow*>(rowLifetime.Get());
				auto* liveOwner = dynamic_cast<DataGrid*>(ownerLifetime.Get());
				return livePresenter && liveRow && liveOwner
					&& livePresenter == presenter
					&& livePresenter->_row == liveRow
					&& liveRow == row
					&& liveRow->_cellsGrid == livePresenter
					&& liveRow->GetDataGridOwner() == liveOwner
					&& liveOwner == owner
					&& liveOwner->_columnWidthProjectionRevision
						== projectionRevision
					? livePresenter : nullptr;
			};
			finalRect = finalRect.Normalized();
			const bool partialResizeArrange =
				owner->_columnWidthDirtyBegin
					!= DataGridCellInfo::InvalidIndex
				&& owner->_columnWidthDirtyEnd
					!= DataGridCellInfo::InvalidIndex
				&& owner->_columnWidthDirtyBegin
					< owner->_columnWidthDirtyEnd
				&& std::isfinite(presenter->_lastArrangedHeight)
				&& std::abs(presenter->_lastArrangedHeight - finalRect.height)
					<= 0.0001f;
			const size_t dirtyBegin = owner->_columnWidthDirtyBegin;
			const size_t dirtyEnd = owner->_columnWidthDirtyEnd;
			struct CellSnapshot final
			{
				ControlWeakReference Lifetime;
				DataGridCell* Identity = nullptr;
				float Left = 0.0f;
				float Width = 0.0f;
			};
			std::vector<CellSnapshot> cells;
			const int childCount = context.ChildCount();
			cells.reserve(static_cast<size_t>((std::max)(0, childCount)));
			for (int childIndex = 0; childIndex < childCount; ++childIndex)
			{
				auto* livePresenter = currentPresenter();
				if (!livePresenter) return;
				auto* cell = dynamic_cast<DataGridCell*>(
					context.ChildAt(childIndex));
				if (!cell || cell->IsCollapsed()) continue;
				if (partialResizeArrange
					&& (cell->_columnIndex < dirtyBegin
						|| cell->_columnIndex >= dirtyEnd)) continue;
				CellSnapshot snapshot;
				snapshot.Lifetime = cell;
				snapshot.Identity = cell;
				const bool resolved = livePresenter->TryGetCellRect(
					cell, snapshot.Left, snapshot.Width);
				livePresenter = currentPresenter();
				auto* liveCell = dynamic_cast<DataGridCell*>(
					snapshot.Lifetime.Get());
				if (!livePresenter || liveCell != snapshot.Identity
					|| !livePresenter->IsCurrentCell(liveCell)) return;
				if (resolved) cells.push_back(std::move(snapshot));
			}
			for (const auto& snapshot : cells)
			{
				auto* livePresenter = currentPresenter();
				auto* cell = dynamic_cast<DataGridCell*>(
					snapshot.Lifetime.Get());
				if (!livePresenter || cell != snapshot.Identity
					|| !livePresenter->IsCurrentCell(cell)) return;
				if (cell->IsCollapsed()) continue;

				const Thickness margin = cell->Margin;
				const float contentWidth = (std::max)(0.0f,
					snapshot.Width - margin.Left - margin.Right);
				const float contentHeight = (std::max)(0.0f,
					finalRect.height - margin.Top - margin.Bottom);
				float x = finalRect.x + snapshot.Left + margin.Left;
				float y = finalRect.y + margin.Top;
				const auto horizontal =
					cui::layout::ResolveHorizontalArrangeAlignment(*cell);
				const auto vertical =
					cui::layout::ResolveVerticalArrangeAlignment(*cell);
				// Auto rows already measured width-dirty cells with unbounded height
				// above. Measuring them again here with the arranged row height would
				// alternate the constraint on every ancestor probe and defeat the cache.
				// Arrange can consume that natural DesiredSize directly. Fixed rows skip
				// cell work in Measure. A default Stretch/Stretch cell needs no natural
				// size at this level: assigning its new slot makes its template root
				// arrange against the new width directly. Non-stretch alignment still
				// takes the bounded measure path to obtain its alignment offset.
				const bool fixedRowHeight =
					std::isfinite(owner->GetRowHeight());
				const bool stretchSlot =
					horizontal == HorizontalAlignment::Stretch
					&& vertical == VerticalAlignment::Stretch;
				const auto desired = fixedRowHeight && stretchSlot
					? cui::core::Size{ contentWidth, contentHeight }
					: fixedRowHeight
					? cell->Measure(cui::core::Constraints{
						cui::core::Size{ contentWidth, contentHeight } })
					: (cell->GetComputedLayout().NeedsMeasure()
						? cell->Measure(cui::core::Constraints{
							cui::core::Size{
								contentWidth, cui::core::Infinity } })
						: cell->GetDesiredSizeDip());
				livePresenter = currentPresenter();
				cell = dynamic_cast<DataGridCell*>(snapshot.Lifetime.Get());
				if (!livePresenter || cell != snapshot.Identity
					|| !livePresenter->IsCurrentCell(cell)) return;
				float width = desired.width;
				float height = desired.height;
				if (horizontal == HorizontalAlignment::Stretch)
					width = contentWidth;
				else if (horizontal == HorizontalAlignment::Center)
					x += (contentWidth - width) * 0.5f;
				else if (horizontal == HorizontalAlignment::Right)
					x += contentWidth - width;
				if (vertical == VerticalAlignment::Stretch)
					height = contentHeight;
				else if (vertical == VerticalAlignment::Center)
					y += (contentHeight - height) * 0.5f;
				else if (vertical == VerticalAlignment::Bottom)
					y += contentHeight - height;
				cell->Arrange(cui::core::Rect{ x, y, width, height });
				livePresenter = currentPresenter();
				cell = dynamic_cast<DataGridCell*>(snapshot.Lifetime.Get());
				if (!livePresenter || cell != snapshot.Identity
					|| !livePresenter->IsCurrentCell(cell)) return;
			}
			if (auto* livePresenter = currentPresenter())
				livePresenter->_lastArrangedHeight = finalRect.height;
			_needsLayout = false;
		}

	private:
		DataGridCellsPresenter& _owner;
	};

public:
	explicit DataGridCellsPresenter(DataGridRow& row)
		: _row(&row)
	{
		SetLayoutEngine(new CellsLayoutEngine(*this));
	}

	bool TryCommitResizeLayoutLocally(bool heightIsFixed)
	{
		if (!_row || _row->_cellsGrid != this
			|| !_row->GetDataGridOwner()
			|| !GetComputedLayout().hasArranged) return false;
		if (heightIsFixed)
		{
			// Fixed rows cannot affect the vertical item stack. The custom cells
			// engine can therefore update changed slots directly in the retained
			// panel without walking through Row/Grid/ItemsPresenter/ScrollViewer.
			UpdateLayout();
			return true;
		}

		const float previousDesiredHeight = GetDesiredSizeDip().height;
		const auto desired = Measure(cui::core::Constraints{
			cui::core::Size{
				cui::core::Infinity, cui::core::Infinity } });
		if (std::abs(desired.height - previousDesiredHeight) > 0.0001f)
			return false;
		const auto location = GetActualLocationDip();
		const auto size = GetActualSizeDip();
		Arrange(cui::core::Rect{
			location.x, location.y, size.width, size.height });
		return true;
	}

	void InvalidateColumnLayout(bool propagate)
	{
		if (propagate)
		{
			InvalidateLayout();
			return;
		}
		_needsMeasure = true;
		_needsArrange = true;
		if (_layoutEngine) _layoutEngine->Invalidate();
		// A column-width frame changes this presenter's layout policy, not every
		// retained cell.  The layout engine offers each cell its resolved slot;
		// Control::Measure naturally remeasures only cells whose constraint changed
		// while later cells can keep their desired-size/template caches and merely
		// move during Arrange.  The DataGrid owns the ancestor transaction.
		_layoutState.InvalidateMeasure();
	}

private:
	bool IsCurrentCell(DataGridCell* cell) const noexcept
	{
		if (!_row || _row->_cellsGrid != this || !cell
			|| cell->_row != _row) return false;
		auto* owner = _row->GetDataGridOwner();
		return owner && cell->_columnIndex < owner->ColumnCount()
			&& owner->GetColumnFromDisplayIndex(cell->_columnIndex) == cell->_column;
	}

	bool TryGetCellRect(
		DataGridCell* cell,
		float& left, float& width) const
	{
		left = 0.0f;
		width = 0.0f;
		auto* const presenter =
			const_cast<DataGridCellsPresenter*>(this);
		auto* const row = _row;
		if (!row || row->_cellsGrid != presenter
			|| !cell || cell->_row != row) return false;
		auto* const owner = row->GetDataGridOwner();
		if (!owner) return false;
		const ControlWeakReference presenterLifetime(presenter);
		const ControlWeakReference rowLifetime(row);
		const ControlWeakReference ownerLifetime(owner);
		const ControlWeakReference cellLifetime(cell);
		const size_t columnIndex = cell->_columnIndex;
		auto* const column = cell->_column;
		const size_t projectionRevision =
			owner->_columnWidthProjectionRevision;
		if (columnIndex >= owner->ColumnCount()
			|| column != owner->GetColumnFromDisplayIndex(columnIndex)) return false;
		double logicalLeft = 0.0;
		double logicalRight = 0.0;
		if (!owner->TryResolveColumnBounds(
			columnIndex, logicalLeft, logicalRight)) return false;
		auto* livePresenter = dynamic_cast<DataGridCellsPresenter*>(
			presenterLifetime.Get());
		auto* liveRow = dynamic_cast<DataGridRow*>(rowLifetime.Get());
		auto* liveOwner = dynamic_cast<DataGrid*>(ownerLifetime.Get());
		auto* liveCell = dynamic_cast<DataGridCell*>(cellLifetime.Get());
		if (livePresenter != presenter || liveRow != row
			|| liveOwner != owner || liveCell != cell
			|| livePresenter->_row != liveRow
			|| liveRow->_cellsGrid != livePresenter
			|| liveRow->GetDataGridOwner() != liveOwner
			|| liveOwner->_columnWidthProjectionRevision != projectionRevision
			|| liveCell->_row != liveRow
			|| liveCell->_columnIndex != columnIndex
			|| liveCell->_column != column
			|| columnIndex >= liveOwner->ColumnCount()
			|| liveOwner->GetColumnFromDisplayIndex(columnIndex) != column) return false;
		const double maximum = static_cast<double>(
			(std::numeric_limits<float>::max)());
		left = static_cast<float>((std::clamp)(
			logicalLeft, 0.0, maximum));
		width = static_cast<float>((std::clamp)(
			logicalRight - logicalLeft, 0.0, maximum));
		return true;
	}

	float TotalColumnWidth() const
	{
		auto* const presenter =
			const_cast<DataGridCellsPresenter*>(this);
		auto* const row = _row;
		if (!row || row->_cellsGrid != presenter) return 0.0f;
		auto* const owner = row->GetDataGridOwner();
		if (!owner || owner->ColumnCount() == 0) return 0.0f;
		const size_t columnCount = owner->ColumnCount();
		const size_t projectionRevision =
			owner->_columnWidthProjectionRevision;
		const ControlWeakReference presenterLifetime(presenter);
		const ControlWeakReference rowLifetime(row);
		const ControlWeakReference ownerLifetime(owner);
		double ignored = 0.0;
		double total = 0.0;
		if (!owner->TryResolveColumnBounds(
			columnCount - 1, ignored, total)) return 0.0f;
		auto* livePresenter = dynamic_cast<DataGridCellsPresenter*>(
			presenterLifetime.Get());
		auto* liveRow = dynamic_cast<DataGridRow*>(rowLifetime.Get());
		auto* liveOwner = dynamic_cast<DataGrid*>(ownerLifetime.Get());
		if (livePresenter != presenter || liveRow != row
			|| liveOwner != owner || livePresenter->_row != liveRow
			|| liveRow->_cellsGrid != livePresenter
			|| liveRow->GetDataGridOwner() != liveOwner
			|| liveOwner->ColumnCount() != columnCount
			|| liveOwner->_columnWidthProjectionRevision
				!= projectionRevision) return 0.0f;
		return static_cast<float>((std::clamp)(total, 0.0,
			static_cast<double>((std::numeric_limits<float>::max)())));
	}

	DataGridRow* _row = nullptr;
	float _lastArrangedHeight =
		(std::numeric_limits<float>::quiet_NaN)();
};

DataGridCellInfo DataGridSelectedCellCollection::const_iterator::operator*()
	const
{
	if (!_owner) throw std::out_of_range(
		"DataGrid selected-cell iterator is not dereferenceable");
	if (_excludedOffsets)
		return _owner->AtWithExcludedOffsets(_index, *_excludedOffsets);
	return _owner->at(_index);
}

DataGridSelectedCellCollection::DataGridSelectedCellCollection(
	DataGrid* owner) noexcept
	: _ownerLifetime(owner), _ownerBound(owner != nullptr)
{
}

DataGridSelectedCellCollection::const_iterator
DataGridSelectedCellCollection::begin() const
{
	const bool hasRowProjection = std::any_of(
		_regions.begin(), _regions.end(), [](const CellRegion& region)
		{
			return !region.IncludedRowOffsets.empty()
				|| !region.ExcludedSnapshotRowIntervals.empty()
				|| !region.ExcludedRows.empty();
		});
	if (_excludedCells.empty() && !hasRowProjection)
		return const_iterator(this, 0);
	return const_iterator(this, 0, ExcludedOffsets());
}

size_t DataGridSelectedCellCollection::size() const noexcept
{
	size_t result = _cells.size();
	const auto* lookup = _source
		? dynamic_cast<const IBindingListOccurrenceLookup*>(_source.Get())
		: nullptr;
	for (const auto& region : _regions)
	{
		const size_t columns = region.Columns.size();
		const size_t projectedRows = region.IncludedRowOffsets.empty()
			? region.Height : region.IncludedRowOffsets.size();
		size_t intervalRows = 0;
		for (const auto& interval : region.ExcludedSnapshotRowIntervals)
		{
			const size_t regionEnd = region.Height
				> (std::numeric_limits<size_t>::max)() - region.Top
				? (std::numeric_limits<size_t>::max)()
				: region.Top + region.Height;
			const size_t start = (std::max)(region.Top, interval.Start);
			const size_t end = (std::min)(
				regionEnd, IntervalEndExclusive(interval));
			if (start >= end) continue;
			if (region.IncludedRowOffsets.empty())
				intervalRows += end - start;
			else
			{
				const auto first = std::lower_bound(
					region.IncludedRowOffsets.begin(),
					region.IncludedRowOffsets.end(), start - region.Top);
				const auto last = std::lower_bound(
					first, region.IncludedRowOffsets.end(), end - region.Top);
				intervalRows += static_cast<size_t>(std::distance(first, last));
			}
		}
		const size_t excludedPointRows = static_cast<size_t>(std::count_if(
			region.ExcludedRows.begin(), region.ExcludedRows.end(),
			[&](const CellRegion::ExcludedRow& row)
			{
				if (row.SnapshotRow < region.Top
					|| row.SnapshotRow - region.Top >= region.Height) return false;
				if (IsRowInIntervals(
					region.ExcludedSnapshotRowIntervals, row.SnapshotRow))
					return false;
				return region.IncludedRowOffsets.empty()
					|| std::binary_search(region.IncludedRowOffsets.begin(),
						region.IncludedRowOffsets.end(),
						row.SnapshotRow - region.Top);
			}));
		const size_t excludedRows = intervalRows > projectedRows
			? projectedRows
			: (std::min)(projectedRows, intervalRows + excludedPointRows);
		const size_t rows = excludedRows < projectedRows
			? projectedRows - excludedRows : 0;
		if (columns != 0 && rows
			> (std::numeric_limits<size_t>::max)() / columns)
			return (std::numeric_limits<size_t>::max)();
		const size_t regionSize = rows * columns;
		size_t excludedCells = 0;
		if (lookup && columns != 0)
			for (const auto& excluded : _excludedCells)
			{
				if (std::find(region.Columns.begin(), region.Columns.end(),
					excluded.Column) == region.Columns.end()) continue;
				size_t snapshotRow = 0;
				if (!lookup->TryGetItemIndexByOccurrenceIdentity(
						excluded._itemOccurrence, snapshotRow)
					|| snapshotRow < region.Top
					|| snapshotRow - region.Top >= region.Height) continue;
				const size_t rowOffset = snapshotRow - region.Top;
				if (!region.IncludedRowOffsets.empty()
					&& !std::binary_search(region.IncludedRowOffsets.begin(),
						region.IncludedRowOffsets.end(), rowOffset)) continue;
				if (IsRowInIntervals(
					region.ExcludedSnapshotRowIntervals, snapshotRow)) continue;
				if (std::any_of(region.ExcludedRows.begin(),
					region.ExcludedRows.end(), [&](const auto& row)
					{ return row.Occurrence == excluded._itemOccurrence; }))
					continue;
				++excludedCells;
			}
		const size_t selectedRegionSize = excludedCells < regionSize
			? regionSize - excludedCells : 0;
		if (result > (std::numeric_limits<size_t>::max)()
			- selectedRegionSize)
			return (std::numeric_limits<size_t>::max)();
		result += selectedRegionSize;
	}
	return result;
}

bool DataGridSelectedCellCollection::TryCreateRegionCell(
	const CellRegion& region,
	size_t rowOffset,
	size_t columnOffset,
	DataGridCellInfo& result) const
{
	result = {};
	if (rowOffset >= region.Height
		|| columnOffset >= region.Columns.size()) return false;
	if (region.Top > (std::numeric_limits<size_t>::max)() - rowOffset)
		return false;
	const size_t snapshotRow = region.Top + rowOffset;
	if (IsRowInIntervals(
		region.ExcludedSnapshotRowIntervals, snapshotRow)) return false;
	auto* column = region.Columns[columnOffset];
	if (!column) return false;
	if (!_source || snapshotRow >= _source.Get()->Count()) return false;
	BindingSourceReference item;
	if (!_source.Get()->TryGetItem(snapshotRow, item) || !item) return false;
	const auto* identities =
		dynamic_cast<const IBindingListOccurrenceIdentity*>(_source.Get());
	size_t occurrence = DataGridCellInfo::InvalidIndex;
	if (!identities || !identities->TryGetItemOccurrenceIdentity(
			snapshotRow, occurrence)
		|| occurrence == DataGridCellInfo::InvalidIndex) return false;
	if (std::any_of(region.ExcludedRows.begin(), region.ExcludedRows.end(),
		[occurrence](const CellRegion::ExcludedRow& row)
		{ return row.Occurrence == occurrence; })) return false;
	auto* owner = dynamic_cast<DataGrid*>(_ownerLifetime.Get());
	if (_ownerBound && !owner) return false;
	std::optional<size_t> liveColumnIndex;
	if (owner)
	{
		const auto resolveLiveOwnerAndColumn = [&]() -> DataGrid*
		{
			auto* live = dynamic_cast<DataGrid*>(_ownerLifetime.Get());
			if (!live || live->_source.Shared()
				!= _ownerSourceIdentity.Shared()) return nullptr;
			const auto liveColumn = std::find_if(
				live->_columns.begin(), live->_columns.end(),
				[column](const auto& candidate)
				{ return candidate.get() == column; });
			if (liveColumn == live->_columns.end()) return nullptr;
			liveColumnIndex = static_cast<size_t>(std::distance(
				live->_columns.begin(), liveColumn));
			return live;
		};
		owner = resolveLiveOwnerAndColumn();
		if (!owner) return false;
		if (!region.UseSnapshotLocators
			&& owner->_source.Shared() == _ownerSourceIdentity.Shared())
		{
			// The common immutable/pass-through case has not reordered this
			// occurrence since the region snapshot was captured.  Validate the
			// snapshot index first so enumerating a large selected region stays
			// sequential and never performs one reverse lookup per cell.  A sort
			// or Move simply misses this fast path and falls back to the stable
			// occurrence lookup below.
			const auto liveItems = owner->GetItemsView();
			if (!liveItems) return false;
			const auto materializeCurrent = [&](size_t liveRow,
				BindingSourceReference liveItem) -> bool
			{
				result.Item = std::move(liveItem);
				result.Column = column;
				result.RowIndex = liveRow;
				result.ColumnIndex = *liveColumnIndex;
				result._itemOccurrence = occurrence;
				return true;
			};
			// When the selected region retained the current immutable view itself,
			// the item and occurrence read above already are the live validation.
			// Re-reading them would double source work for every enumerated cell.
			if (liveItems.Shared() == _source.Shared())
				return materializeCurrent(snapshotRow, std::move(item));
			size_t candidateOccurrence = DataGridCellInfo::InvalidIndex;
			const bool occurrenceRead = owner->TryGetItemOccurrenceAt(
				snapshotRow, candidateOccurrence);
			owner = resolveLiveOwnerAndColumn();
			if (!owner || owner->GetItemsView().Shared()
				!= liveItems.Shared()) return false;
			BindingSourceReference candidateItem;
			const bool itemRead = snapshotRow < liveItems.Get()->Count()
				&& liveItems.Get()->TryGetItem(snapshotRow, candidateItem);
			owner = resolveLiveOwnerAndColumn();
			if (!owner || owner->GetItemsView().Shared()
				!= liveItems.Shared()) return false;
			if (occurrenceRead && itemRead && candidateItem
				&& candidateOccurrence == occurrence
				&& candidateItem.Shared() == item.Shared())
				return materializeCurrent(
					snapshotRow, std::move(candidateItem));

			size_t liveRow = DataGridCellInfo::InvalidIndex;
			if (owner->TryResolveItemOccurrence(item, occurrence, liveRow))
			{
				owner = resolveLiveOwnerAndColumn();
				return owner && owner->TryCreateCellInfo(
					liveRow, *liveColumnIndex, result);
			}
			return false;
		}
	}
	result.Item = std::move(item);
	result.Column = column;
	result.RowIndex = snapshotRow;
	result.ColumnIndex = liveColumnIndex.value_or(
		columnOffset < region.ColumnIndices.size()
			? region.ColumnIndices[columnOffset] : columnOffset);
	result._itemOccurrence = occurrence;
	return true;
}

DataGridSelectedCellCollection::RegionExcludedOffsets
DataGridSelectedCellCollection::ExcludedOffsetsForRegion(
	const CellRegion& region) const
{
	RegionExcludedOffsets result;
	const auto* lookup = _source
		? dynamic_cast<const IBindingListOccurrenceLookup*>(_source.Get())
		: nullptr;
	if (!lookup) return result;
	const auto logicalRow = [&region](size_t snapshotRow)
		-> std::optional<size_t>
	{
		if (snapshotRow < region.Top
			|| snapshotRow - region.Top >= region.Height) return std::nullopt;
		const size_t offset = snapshotRow - region.Top;
		if (region.IncludedRowOffsets.empty()) return offset;
		const auto found = std::lower_bound(region.IncludedRowOffsets.begin(),
			region.IncludedRowOffsets.end(), offset);
		if (found == region.IncludedRowOffsets.end() || *found != offset)
			return std::nullopt;
		return static_cast<size_t>(std::distance(
			region.IncludedRowOffsets.begin(), found));
	};
	const size_t regionEnd = region.Height
		> (std::numeric_limits<size_t>::max)() - region.Top
		? (std::numeric_limits<size_t>::max)()
		: region.Top + region.Height;
	for (const auto& interval : region.ExcludedSnapshotRowIntervals)
	{
		const size_t start = (std::max)(region.Top, interval.Start);
		const size_t end = (std::min)(
			regionEnd, IntervalEndExclusive(interval));
		if (start >= end) continue;
		size_t projectedStart = 0;
		size_t projectedCount = 0;
		if (region.IncludedRowOffsets.empty())
		{
			projectedStart = start - region.Top;
			projectedCount = end - start;
		}
		else
		{
			const auto first = std::lower_bound(
				region.IncludedRowOffsets.begin(),
				region.IncludedRowOffsets.end(), start - region.Top);
			const auto last = std::lower_bound(
				first, region.IncludedRowOffsets.end(), end - region.Top);
			projectedStart = static_cast<size_t>(std::distance(
				region.IncludedRowOffsets.begin(), first));
			projectedCount = static_cast<size_t>(std::distance(first, last));
		}
		if (projectedCount == 0) continue;
		if (!result.RowIntervals.empty()
			&& IntervalEndExclusive(result.RowIntervals.back())
				>= projectedStart)
		{
			auto& previous = result.RowIntervals.back();
			const size_t mergedEnd = (std::max)(
				IntervalEndExclusive(previous),
				projectedStart + projectedCount);
			previous.Count = mergedEnd - previous.Start;
		}
		else result.RowIntervals.push_back({
			projectedStart, projectedCount, 0 });
	}
	size_t intervalPrefix = 0;
	for (auto& interval : result.RowIntervals)
	{
		interval.ExcludedBefore = intervalPrefix;
		intervalPrefix += interval.Count;
	}
	result.Rows.reserve(region.ExcludedRows.size());
	for (const auto& excluded : region.ExcludedRows)
		if (const auto row = logicalRow(excluded.SnapshotRow))
			if (!IsRowInIntervals(result.RowIntervals, *row))
				result.Rows.push_back(*row);
	std::sort(result.Rows.begin(), result.Rows.end());
	result.Rows.erase(std::unique(result.Rows.begin(), result.Rows.end()),
		result.Rows.end());
	result.Cells.reserve(_excludedCells.size());
	for (const auto& excluded : _excludedCells)
	{
		const auto column = std::find(
			region.Columns.begin(), region.Columns.end(), excluded.Column);
		if (column == region.Columns.end()) continue;
		size_t rowIndex = 0;
		if (!lookup->TryGetItemIndexByOccurrenceIdentity(
			excluded._itemOccurrence, rowIndex)) continue;
		const auto projectedRow = logicalRow(rowIndex);
		if (!projectedRow || IsProjectedRowExcluded(
			result, *projectedRow)) continue;
		const size_t selectedRow = *projectedRow
			- CountExcludedRowsBefore(result, *projectedRow);
		const size_t columnOffset = static_cast<size_t>(
			std::distance(region.Columns.begin(), column));
		result.Cells.push_back(
			selectedRow * region.Columns.size() + columnOffset);
	}
	std::sort(result.Cells.begin(), result.Cells.end());
	result.Cells.erase(std::unique(result.Cells.begin(), result.Cells.end()),
		result.Cells.end());
	return result;
}

std::shared_ptr<const std::vector<
	DataGridSelectedCellCollection::RegionExcludedOffsets>>
DataGridSelectedCellCollection::ExcludedOffsets() const
{
	if (_excludedOffsetsCache) return _excludedOffsetsCache;
	auto offsets = std::make_shared<std::vector<RegionExcludedOffsets>>();
	offsets->reserve(_regions.size());
	for (const auto& region : _regions)
		offsets->push_back(ExcludedOffsetsForRegion(region));
	_excludedOffsetsCache = offsets;
	return offsets;
}

DataGridCellInfo DataGridSelectedCellCollection::AtWithExcludedOffsets(
	size_t index,
	const std::vector<RegionExcludedOffsets>& excludedOffsets) const
{
	// The caller already paid to translate occurrence-based holes into these
	// compact offsets. Do not call size() here: size() must resolve raw sparse
	// holes, so doing that once per iterator dereference would turn a single
	// exclusion into one reverse occurrence lookup per selected cell.
	size_t selectedCount = _cells.size();
	for (size_t regionIndex = 0;
		regionIndex < _regions.size(); ++regionIndex)
	{
		const auto& region = _regions[regionIndex];
		if (region.Columns.empty() || region.Height == 0) continue;
		const size_t projectedRows = region.IncludedRowOffsets.empty()
			? region.Height : region.IncludedRowOffsets.size();
		static const RegionExcludedOffsets noExclusions;
		const auto& excluded = regionIndex < excludedOffsets.size()
			? excludedOffsets[regionIndex] : noExclusions;
		const size_t excludedRows = CountExcludedRowsBefore(
			excluded, projectedRows);
		const size_t selectedRows = excludedRows < projectedRows
			? projectedRows - excludedRows : 0;
		const size_t regionSize = region.Columns.size() != 0
			&& selectedRows > (std::numeric_limits<size_t>::max)()
				/ region.Columns.size()
			? (std::numeric_limits<size_t>::max)()
			: selectedRows * region.Columns.size();
		const size_t selectedSize = excluded.Cells.size() < regionSize
			? regionSize - excluded.Cells.size() : 0;
		if (selectedCount > (std::numeric_limits<size_t>::max)()
			- selectedSize)
		{
			selectedCount = (std::numeric_limits<size_t>::max)();
			break;
		}
		selectedCount += selectedSize;
	}
	if (index >= selectedCount) throw std::out_of_range(
		"DataGrid selected-cell index is out of range");
	auto* owner = dynamic_cast<DataGrid*>(_ownerLifetime.Get());
	if (_ownerBound && !owner) return {};
	if (index < _cells.size())
	{
		const auto& cell = _cells[index];
		if (owner && std::none_of(owner->_columns.begin(), owner->_columns.end(),
			[&cell](const auto& column)
			{ return column.get() == cell.Column; })) return {};
		return cell;
	}
	index -= _cells.size();
	for (size_t regionIndex = 0;
		regionIndex < _regions.size(); ++regionIndex)
	{
		const auto& region = _regions[regionIndex];
		if (region.Columns.empty() || region.Height == 0) continue;
		const size_t projectedRows = region.IncludedRowOffsets.empty()
			? region.Height : region.IncludedRowOffsets.size();
		static const RegionExcludedOffsets noExclusions;
		const auto& excluded = regionIndex < excludedOffsets.size()
			? excludedOffsets[regionIndex] : noExclusions;
		const size_t excludedRows = CountExcludedRowsBefore(
			excluded, projectedRows);
		const size_t selectedRows = excludedRows < projectedRows
			? projectedRows - excludedRows : 0;
		const size_t regionSize = selectedRows * region.Columns.size();
		const size_t selectedSize = excluded.Cells.size() < regionSize
			? regionSize - excluded.Cells.size() : 0;
		if (index >= selectedSize)
		{
			index -= selectedSize;
			continue;
		}
		const size_t flat = RawOffsetForSelectedOrdinal(
			index, regionSize, excluded.Cells);
		const size_t selectedRow = flat / region.Columns.size();
		const size_t logicalRow = RawRowOffsetForSelectedOrdinal(
			selectedRow, projectedRows, excluded);
		const size_t snapshotOffset = region.IncludedRowOffsets.empty()
			? logicalRow : region.IncludedRowOffsets[logicalRow];
		DataGridCellInfo result;
		if (flat < regionSize && TryCreateRegionCell(region,
			snapshotOffset,
			flat % region.Columns.size(), result)) return result;
		return {};
	}
	return {};
}

DataGridCellInfo DataGridSelectedCellCollection::at(size_t index) const
{
	if (index >= size()) throw std::out_of_range(
		"DataGrid selected-cell index is out of range");
	// Public snapshots keep column identity as a non-owning pointer. Once their
	// owner has died no cell, including a sparse one, may expose that pointer.
	auto* owner = dynamic_cast<DataGrid*>(_ownerLifetime.Get());
	if (_ownerBound && !owner) return {};
	if (!_regions.empty())
		return AtWithExcludedOffsets(index, *ExcludedOffsets());
	if (index < _cells.size())
	{
		const auto& cell = _cells[index];
		if (owner && std::none_of(owner->_columns.begin(), owner->_columns.end(),
			[&cell](const auto& column)
			{ return column.get() == cell.Column; })) return {};
		return cell;
	}
	index -= _cells.size();
	for (const auto& region : _regions)
	{
		if (region.Columns.empty() || region.Height == 0) continue;
		const size_t regionSize = region.Height * region.Columns.size();
		if (_excludedCells.empty())
		{
			if (index >= regionSize)
			{
				index -= regionSize;
				continue;
			}
			DataGridCellInfo result;
			if (TryCreateRegionCell(region,
				index / region.Columns.size(),
				index % region.Columns.size(), result)) return result;
			return {};
		}
	}
	return {};
}

bool DataGridSelectedCellCollection::Contains(
	const DataGridCellInfo& candidate) const noexcept
{
	try
	{
		if (ContainsCellIdentity(_cells, candidate)) return true;
		const auto* lookup = _source
			? dynamic_cast<const IBindingListOccurrenceLookup*>(_source.Get())
			: nullptr;
		if (!lookup || candidate._itemOccurrence
			== DataGridCellInfo::InvalidIndex) return false;
		size_t snapshotRow = 0;
		if (!lookup->TryGetItemIndexByOccurrenceIdentity(
			candidate._itemOccurrence, snapshotRow)) return false;
		BindingSourceReference snapshotItem;
		if (!_source.Get()->TryGetItem(snapshotRow, snapshotItem)
			|| !snapshotItem
			|| snapshotItem.Shared() != candidate.Item.Shared()) return false;
		for (const auto& region : _regions)
		{
			if (snapshotRow < region.Top
				|| snapshotRow - region.Top >= region.Height
				|| std::find(region.Columns.begin(), region.Columns.end(),
					candidate.Column) == region.Columns.end()) continue;
			const size_t offset = snapshotRow - region.Top;
			if (!region.IncludedRowOffsets.empty()
				&& !std::binary_search(region.IncludedRowOffsets.begin(),
					region.IncludedRowOffsets.end(), offset)) continue;
			if (IsRowInIntervals(
				region.ExcludedSnapshotRowIntervals, snapshotRow)) continue;
			if (std::any_of(region.ExcludedRows.begin(),
				region.ExcludedRows.end(), [&](const CellRegion::ExcludedRow& row)
				{ return row.Occurrence == candidate._itemOccurrence; }))
				continue;
			return !ContainsCellIdentity(_excludedCells, candidate);
		}
	}
	catch (...) {}
	return false;
}

void DataGridSelectedCellCollection::SetSparse(
	DataGrid* owner,
	BindingListReference source,
	std::vector<DataGridCellInfo> cells) noexcept
{
	_ownerLifetime = ControlWeakReference(owner);
	_ownerBound = owner != nullptr;
	_ownerSourceIdentity = owner ? owner->_source : source;
	_source = std::move(source);
	_cells = std::move(cells);
	_regions.clear();
	_excludedCells.clear();
	InvalidateExcludedOffsets();
}

void DataGridSelectedCellCollection::SetFullRegion(
	DataGrid* owner,
	BindingListReference source,
	size_t rowCount,
	std::vector<DataGridColumn*> columns) noexcept
{
	_ownerLifetime = ControlWeakReference(owner);
	_ownerBound = owner != nullptr;
	_ownerSourceIdentity = owner ? owner->_source : source;
	_source = std::move(source);
	_cells.clear();
	_regions.clear();
	_excludedCells.clear();
	InvalidateExcludedOffsets();
	if (rowCount == 0 || columns.empty()) return;
	CellRegion region;
	region.Height = rowCount;
	region.Columns = std::move(columns);
	region.ColumnIndices.resize(region.Columns.size());
	for (size_t index = 0; index < region.ColumnIndices.size(); ++index)
		region.ColumnIndices[index] = index;
	_regions.push_back(std::move(region));
}

bool DataGridSelectedCellCollection::SameLogicalCells(
	const DataGridSelectedCellCollection& other) const noexcept
{
	if (size() != other.size()) return false;
	const auto sameSet = [](const std::vector<DataGridCellInfo>& left,
		const std::vector<DataGridCellInfo>& right)
	{
		return left.size() == right.size()
			&& std::all_of(left.begin(), left.end(),
				[&right](const DataGridCellInfo& cell)
				{ return ContainsCellIdentity(right, cell); });
	};
	if (_ownerSourceIdentity.Shared() == other._ownerSourceIdentity.Shared()
		&& _source.Shared() == other._source.Shared()
		&& _regions.size() == other._regions.size())
	{
		bool regionsEqual = true;
		for (size_t index = 0; index < _regions.size(); ++index)
		{
			const auto& left = _regions[index];
			const auto& right = other._regions[index];
			if (left.Top != right.Top || left.Height != right.Height
				|| left.Columns != right.Columns
				|| left.IncludedRowOffsets != right.IncludedRowOffsets
				|| left.ExcludedSnapshotRowIntervals.size()
					!= right.ExcludedSnapshotRowIntervals.size()
				|| !std::equal(
					left.ExcludedSnapshotRowIntervals.begin(),
					left.ExcludedSnapshotRowIntervals.end(),
					right.ExcludedSnapshotRowIntervals.begin(),
					[](const auto& a, const auto& b)
					{
						return a.Start == b.Start && a.Count == b.Count;
					})
				|| left.UseSnapshotLocators != right.UseSnapshotLocators
				|| left.ExcludedRows.size() != right.ExcludedRows.size()
				|| !std::equal(left.ExcludedRows.begin(),
					left.ExcludedRows.end(), right.ExcludedRows.begin(),
					right.ExcludedRows.end(),
					[](const CellRegion::ExcludedRow& a,
						const CellRegion::ExcludedRow& b)
					{
						return a.Occurrence == b.Occurrence
							&& a.SnapshotRow == b.SnapshotRow;
					}))
			{
				regionsEqual = false;
				break;
			}
		}
		if (regionsEqual && sameSet(_cells, other._cells)
			&& sameSet(_excludedCells, other._excludedCells)) return true;
	}
	// Sparse selections preserve their historical identity-set comparison.
	if (_regions.empty() && other._regions.empty())
		return sameSet(_cells, other._cells);
	return false;
}

DataGridSelectedCellCollection DataGridSelectedCellCollection::Difference(
	const DataGridSelectedCellCollection& other) const
{
	auto* owner = dynamic_cast<DataGrid*>(_ownerLifetime.Get());
	DataGridSelectedCellCollection result(owner);
	if (_ownerBound && !owner) result._ownerBound = true;
	result._ownerSourceIdentity = _ownerSourceIdentity;
	result._source = _source;
	for (const auto& cell : _cells)
		if (!other.Contains(cell)) result._cells.push_back(cell);
	if (_regions.empty()) return result;
	if (other.empty())
	{
		result._regions = _regions;
		result._excludedCells = _excludedCells;
		return result;
	}

	const bool sameRows =
		_ownerSourceIdentity.Shared() == other._ownerSourceIdentity.Shared()
		&& _source.Shared() == other._source.Shared()
		&& _regions.size() == 1 && other._regions.size() == 1
		&& _regions.front().Top == other._regions.front().Top
		&& _regions.front().Height == other._regions.front().Height;
	if (sameRows)
	{
		const auto& left = _regions.front();
		const auto& right = other._regions.front();
		const auto rowExcluded = [](const CellRegion& region, size_t row)
		{
			return IsRowInIntervals(
				region.ExcludedSnapshotRowIntervals, row)
				|| std::any_of(region.ExcludedRows.begin(),
				region.ExcludedRows.end(), [row](const auto& excluded)
				{ return excluded.SnapshotRow == row; });
		};
		struct RowRun final { size_t Start = 0; size_t Count = 0; };
		std::vector<RowRun> removedRuns;
		const size_t leftEnd = left.Height
			> (std::numeric_limits<size_t>::max)() - left.Top
			? (std::numeric_limits<size_t>::max)()
			: left.Top + left.Height;
		const auto appendSelectedRuns = [&](size_t requestedStart,
			size_t requestedEnd)
		{
			const size_t start = (std::max)(left.Top, requestedStart);
			const size_t end = (std::min)(leftEnd, requestedEnd);
			if (start >= end) return;
			if (!left.IncludedRowOffsets.empty())
			{
				auto at = std::lower_bound(left.IncludedRowOffsets.begin(),
					left.IncludedRowOffsets.end(), start - left.Top);
				RowRun run;
				for (; at != left.IncludedRowOffsets.end(); ++at)
				{
					const size_t row = left.Top + *at;
					if (row >= end) break;
					if (rowExcluded(left, row)) continue;
					if (run.Count != 0
						&& run.Start + run.Count == row)
					{
						++run.Count;
						continue;
					}
					if (run.Count != 0) removedRuns.push_back(run);
					run = { row, 1 };
				}
				if (run.Count != 0) removedRuns.push_back(run);
				return;
			}

			// Subtract existing row holes using interval algebra.  Work is
			// proportional to existing holes/runs, never to requestedEnd-start.
			std::vector<RowRun> cuts;
			cuts.reserve(left.ExcludedSnapshotRowIntervals.size()
				+ left.ExcludedRows.size());
			for (const auto& interval : left.ExcludedSnapshotRowIntervals)
			{
				const size_t cutStart = (std::max)(start, interval.Start);
				const size_t cutEnd = (std::min)(
					end, IntervalEndExclusive(interval));
				if (cutStart < cutEnd)
					cuts.push_back({ cutStart, cutEnd - cutStart });
			}
			for (const auto& excluded : left.ExcludedRows)
				if (excluded.SnapshotRow >= start
					&& excluded.SnapshotRow < end)
					cuts.push_back({ excluded.SnapshotRow, 1 });
			std::sort(cuts.begin(), cuts.end(), [](const RowRun& a,
				const RowRun& b) { return a.Start < b.Start; });
			size_t cursor = start;
			for (const auto& cut : cuts)
			{
				if (cut.Start > cursor)
					removedRuns.push_back({ cursor, cut.Start - cursor });
				cursor = (std::max)(cursor, IntervalEndExclusive(cut));
				if (cursor >= end) break;
			}
			if (cursor < end) removedRuns.push_back({ cursor, end - cursor });
		};

		// A retired continuous old-snapshot range becomes one lazy region (or
		// one per pre-existing hole), instead of K IncludedRowOffsets.
		for (const auto& interval : right.ExcludedSnapshotRowIntervals)
			appendSelectedRuns(interval.Start, IntervalEndExclusive(interval));
		for (const auto& excluded : right.ExcludedRows)
		{
			if (rowExcluded(left, excluded.SnapshotRow)
				|| IsRowInIntervals(right.ExcludedSnapshotRowIntervals,
					excluded.SnapshotRow)) continue;
			appendSelectedRuns(excluded.SnapshotRow,
				excluded.SnapshotRow == (std::numeric_limits<size_t>::max)()
					? excluded.SnapshotRow : excluded.SnapshotRow + 1);
		}
		std::sort(removedRuns.begin(), removedRuns.end(),
			[](const RowRun& a, const RowRun& b) { return a.Start < b.Start; });
		std::vector<RowRun> mergedRuns;
		for (const auto& run : removedRuns)
		{
			if (run.Count == 0) continue;
			if (!mergedRuns.empty()
				&& IntervalEndExclusive(mergedRuns.back()) >= run.Start)
			{
				auto& previous = mergedRuns.back();
				const size_t end = (std::max)(
					IntervalEndExclusive(previous),
					IntervalEndExclusive(run));
				previous.Count = end - previous.Start;
			}
			else mergedRuns.push_back(run);
		}
		for (const auto& run : mergedRuns)
		{
			CellRegion removedRows;
			removedRows.Top = run.Start;
			removedRows.Height = run.Count;
			removedRows.Columns = left.Columns;
			removedRows.ColumnIndices = left.ColumnIndices;
			removedRows.UseSnapshotLocators = true;
			result._regions.push_back(std::move(removedRows));
		}
		if (!mergedRuns.empty())
		{
			const auto* lookup = _source
				? dynamic_cast<const IBindingListOccurrenceLookup*>(
					_source.Get()) : nullptr;
			if (lookup)
				for (const auto& excluded : _excludedCells)
				{
					if (std::find(left.Columns.begin(), left.Columns.end(),
						excluded.Column) == left.Columns.end()) continue;
					size_t snapshotRow = 0;
					if (!lookup->TryGetItemIndexByOccurrenceIdentity(
							excluded._itemOccurrence, snapshotRow)) continue;
					const auto after = std::upper_bound(
						mergedRuns.begin(), mergedRuns.end(), snapshotRow,
						[](size_t row, const RowRun& run)
						{ return row < run.Start; });
					if (after != mergedRuns.begin()
						&& snapshotRow - std::prev(after)->Start
							< std::prev(after)->Count)
						result._excludedCells.push_back(excluded);
				}
		}
		CellRegion removedColumns;
		removedColumns.Top = left.Top;
		removedColumns.Height = left.Height;
		removedColumns.IncludedRowOffsets = left.IncludedRowOffsets;
		removedColumns.ExcludedSnapshotRowIntervals =
			left.ExcludedSnapshotRowIntervals;
		removedColumns.ExcludedRows = left.ExcludedRows;
		for (size_t index = 0; index < left.Columns.size(); ++index)
			if (std::find(right.Columns.begin(), right.Columns.end(),
				left.Columns[index]) == right.Columns.end())
			{
				removedColumns.Columns.push_back(left.Columns[index]);
				removedColumns.ColumnIndices.push_back(
					index < left.ColumnIndices.size()
						? left.ColumnIndices[index] : index);
			}
		if (!removedColumns.Columns.empty())
		{
			result._regions.push_back(std::move(removedColumns));
			for (const auto& excluded : _excludedCells)
				if (std::find(result._regions.back().Columns.begin(),
					result._regions.back().Columns.end(), excluded.Column)
					!= result._regions.back().Columns.end()
					&& !ContainsCellIdentity(result._excludedCells, excluded))
					result._excludedCells.push_back(excluded);
		}
		// On columns shared by both domains, a newly excluded identity is the
		// exact removed-cell delta; no rectangle needs to be expanded.
		for (const auto& excluded : other._excludedCells)
			if (Contains(excluded) && !ContainsCellIdentity(
				result._cells, excluded)) result._cells.push_back(excluded);
		return result;
	}

	if (other._regions.empty())
	{
		result._regions = _regions;
		result._excludedCells = _excludedCells;
		for (const auto& retained : other._cells)
			if (Contains(retained)) result.Exclude(retained);
		return result;
	}

	// A raw Reset may replace the immutable checkpoint while ListBox restores
	// the WPF-selected object multiplicities.  Expanding two otherwise dense
	// row regions here would turn one O(rows) Reset into O(rows * columns)
	// DataGridCellInfo objects.  Compare selected row multiplicities once and
	// retain only missing old-row offsets in the old snapshot instead.
	if (_regions.size() == 1 && other._regions.size() == 1
		&& _excludedCells.empty() && other._excludedCells.empty()
		&& _regions.front().Columns == other._regions.front().Columns
		&& _source && other._source)
	{
		const auto& left = _regions.front();
		const auto& right = other._regions.front();
		const auto rowSelected = [](const CellRegion& region, size_t offset)
		{
			if (offset >= region.Height) return false;
			if (!region.IncludedRowOffsets.empty()
				&& !std::binary_search(region.IncludedRowOffsets.begin(),
					region.IncludedRowOffsets.end(), offset)) return false;
			const size_t row = region.Top + offset;
			return !IsRowInIntervals(
				region.ExcludedSnapshotRowIntervals, row)
				&& std::none_of(region.ExcludedRows.begin(),
				region.ExcludedRows.end(), [row](const auto& excluded)
				{ return excluded.SnapshotRow == row; });
		};
		std::unordered_map<IBindingSource*, size_t> retainedCounts;
		retainedCounts.reserve((std::min)(right.Height, size_t{ 4096 }));
		const size_t leftCount = _source.Get()->Count();
		const size_t rightCount = other._source.Get()->Count();
		bool readable = true;
		for (size_t offset = 0; offset < right.Height; ++offset)
		{
			if (!rowSelected(right, offset)) continue;
			BindingSourceReference item;
			if (right.Top > rightCount
				|| offset >= rightCount - right.Top
				|| !other._source.Get()->TryGetItem(
					right.Top + offset, item) || !item)
			{
				readable = false;
				break;
			}
			++retainedCounts[item.Get()];
		}
		CellRegion removedRows;
		removedRows.Top = left.Top;
		removedRows.Height = left.Height;
		removedRows.Columns = left.Columns;
		removedRows.ColumnIndices = left.ColumnIndices;
		removedRows.UseSnapshotLocators = true;
		if (readable)
			for (size_t offset = 0; offset < left.Height; ++offset)
			{
				if (!rowSelected(left, offset)) continue;
				BindingSourceReference item;
				if (left.Top > leftCount
					|| offset >= leftCount - left.Top
					|| !_source.Get()->TryGetItem(
						left.Top + offset, item) || !item)
				{
					readable = false;
					break;
				}
				auto retained = retainedCounts.find(item.Get());
				if (retained != retainedCounts.end()
					&& retained->second != 0)
				{
					--retained->second;
					continue;
				}
				removedRows.IncludedRowOffsets.push_back(offset);
			}
		if (readable)
		{
			if (!removedRows.IncludedRowOffsets.empty())
				result._regions.push_back(std::move(removedRows));
			return result;
		}
	}

	// Different dense row domains are not produced by the safe SelectAll slice.
	// Preserve correctness for a future caller by materializing only at this
	// explicit delta boundary instead of silently truncating the change.
	for (const auto& cell : *this)
		if (!other.Contains(cell)) result._cells.push_back(cell);
	return result;
}

void DataGridSelectedCellCollection::PruneColumns(
	const std::vector<std::unique_ptr<DataGridColumn>>& columns)
{
	InvalidateExcludedOffsets();
	for (auto& region : _regions)
	{
		std::vector<DataGridColumn*> retained;
		std::vector<size_t> indices;
		retained.reserve(region.Columns.size());
		indices.reserve(region.Columns.size());
		for (auto* column : region.Columns)
		{
			const auto found = std::find_if(columns.begin(), columns.end(),
				[column](const auto& candidate)
				{ return candidate.get() == column; });
			if (found == columns.end()) continue;
			retained.push_back(column);
			indices.push_back(static_cast<size_t>(
				std::distance(columns.begin(), found)));
		}
		region.Columns = std::move(retained);
		region.ColumnIndices = std::move(indices);
	}
	_regions.erase(std::remove_if(_regions.begin(), _regions.end(),
		[](const CellRegion& region)
		{ return region.Height == 0 || region.Columns.empty(); }), _regions.end());
	_excludedCells.erase(std::remove_if(
		_excludedCells.begin(), _excludedCells.end(),
		[this](const DataGridCellInfo& cell)
		{
			return std::none_of(_regions.begin(), _regions.end(),
				[&cell](const CellRegion& region)
				{
					return std::find(region.Columns.begin(),
						region.Columns.end(), cell.Column)
						!= region.Columns.end();
				});
		}), _excludedCells.end());
}

bool DataGridSelectedCellCollection::Exclude(
	const DataGridCellInfo& cell)
{
	if (!Contains(cell) || ContainsCellIdentity(_excludedCells, cell))
		return false;
	_excludedCells.push_back(cell);
	InvalidateExcludedOffsets();
	return true;
}

bool DataGridSelectedCellCollection::Include(
	const DataGridCellInfo& cell)
{
	const auto found = std::find_if(
		_excludedCells.begin(), _excludedCells.end(),
		[&cell](const DataGridCellInfo& candidate)
		{ return SameCellIdentity(candidate, cell); });
	if (found == _excludedCells.end()) return false;
	_excludedCells.erase(found);
	InvalidateExcludedOffsets();
	return true;
}

bool DataGridLength::IsValid() const noexcept
{
	if (!std::isfinite(Value) || Value < 0.0) return false;
	switch (UnitType)
	{
	case DataGridLengthUnitType::Auto:
	case DataGridLengthUnitType::Pixel:
	case DataGridLengthUnitType::SizeToCells:
	case DataGridLengthUnitType::SizeToHeader:
	case DataGridLengthUnitType::Star:
		return true;
	}
	return false;
}

bool DataGridLength::TryParse(
	std::wstring_view text, DataGridLength& out) noexcept
{
	try
	{
		text = Trim(text);
		if (EqualsIgnoreCase(text, L"Auto"))
		{
			out = Auto();
			return true;
		}
		if (EqualsIgnoreCase(text, L"SizeToHeader"))
		{
			out = SizeToHeader();
			return true;
		}
		if (EqualsIgnoreCase(text, L"SizeToCells"))
		{
			out = SizeToCells();
			return true;
		}

		bool star = !text.empty() && text.back() == L'*';
		if (star) text.remove_suffix(1);
		text = Trim(text);
		if (star && text.empty())
		{
			out = Star();
			return true;
		}
		if (text.empty()) return false;
		std::wstring buffer(text);
		wchar_t* end = nullptr;
		const double value = std::wcstod(buffer.c_str(), &end);
		if (!end || end != buffer.c_str() + buffer.size()) return false;
		DataGridLength parsed(value, star
			? DataGridLengthUnitType::Star
			: DataGridLengthUnitType::Pixel);
		if (!parsed.IsValid()) return false;
		out = parsed;
		return true;
	}
	catch (...)
	{
		return false;
	}
}

void DataGridColumn::SetHeader(BindingValue value)
{
	if (BindingValuesEqual(_header, value)) return;
	_header = std::move(value);
	NotifyOwnerChanged();
}

void DataGridColumn::SetWidth(DataGridLength value)
{
	if (!value.IsValid())
		throw std::invalid_argument("DataGridColumn.Width is invalid");
	if (_width == value) return;
	if (_owner && !_owner->_columnResizeSnapshot.empty())
		_owner->EndColumnResizeTransaction(true);
	const bool removedLastStar = _owner
		&& _width.UnitType == DataGridLengthUnitType::Star
		&& value.UnitType != DataGridLengthUnitType::Star
		&& std::none_of(_owner->_columns.begin(), _owner->_columns.end(),
			[this](const auto& column)
			{
				return column.get() != this
					&& column->_width.UnitType
						== DataGridLengthUnitType::Star;
			});
	_width = value;
	_runtimeWidth = {};
	if (removedLastStar)
	{
		// WPF releases compensation Display values once no Star column remains;
		// otherwise the former donor stays artificially narrow and leaves a gap.
		for (const auto& column : _owner->_columns)
		{
			if (column.get() == this
				|| !column->_runtimeWidth.HasDisplayOverride
				|| !std::isfinite(column->_runtimeWidth.Desired)) continue;
			column->_runtimeWidth = {};
		}
	}
	if (_owner) _owner->RefreshColumnWidths();
}

void DataGridColumn::SetMinWidth(double value)
{
	if (!std::isfinite(value) || value < 0.0 || value > _maxWidth)
		throw std::invalid_argument("DataGridColumn.MinWidth is invalid");
	if (_minWidth == value) return;
	if (_owner && !_owner->_columnResizeSnapshot.empty())
		_owner->EndColumnResizeTransaction(true);
	_minWidth = value;
	NotifyOwnerChanged();
}

void DataGridColumn::SetMaxWidth(double value)
{
	if (std::isnan(value) || value < _minWidth)
		throw std::invalid_argument("DataGridColumn.MaxWidth is invalid");
	if (_maxWidth == value) return;
	if (_owner && !_owner->_columnResizeSnapshot.empty())
		_owner->EndColumnResizeTransaction(true);
	_maxWidth = value;
	NotifyOwnerChanged();
}

void DataGridColumn::SetIsReadOnly(bool value)
{
	if (_isReadOnly == value) return;
	_isReadOnly = value;
	NotifyOwnerChanged();
}

void DataGridColumn::SetCanUserSort(bool value)
{
	if (_canUserSort == value) return;
	_canUserSort = value;
	NotifyOwnerChanged();
}

void DataGridColumn::SetCanUserResize(bool value)
{
	if (_canUserResize == value) return;
	if (_owner && !_owner->_columnResizeSnapshot.empty())
		_owner->EndColumnResizeTransaction(true);
	_canUserResize = value;
}

void DataGridColumn::SetDisplayIndex(size_t value)
{
	if (!_owner)
	{
		_displayIndex = value;
		return;
	}
	if (value >= _owner->ColumnCount())
		throw std::invalid_argument(
			"DataGridColumn.DisplayIndex is outside the current schema");
	(void)_owner->SetColumnDisplayIndex(*this, value);
}

void DataGridColumn::NotifyOwnerChanged()
{
	if (_owner) _owner->RefreshColumns();
}

#if CUI_ENABLE_DYNAMIC_XAML
void DataGridColumn::SetSortMemberPath(std::wstring value)
{
	if (_sortMemberPath == value && _compiledSortMemberPath.Empty()) return;
	_sortMemberPath = std::move(value);
	_compiledSortMemberPath = {};
	NotifyOwnerChanged();
}
#endif

void DataGridColumn::SetCompiledSortMemberPath(
	CompiledBindingPathView value)
{
	if (SameCompiledCollectionPath(_compiledSortMemberPath, value)) return;
	_compiledSortMemberPath = value;
#if CUI_ENABLE_DYNAMIC_XAML
	_sortMemberPath.clear();
#endif
	NotifyOwnerChanged();
}

#if CUI_ENABLE_DYNAMIC_XAML
void DataGridBoundColumn::SetBindingPath(std::wstring value)
{
	if (_bindingPath == value && _compiledBindingPath.Empty()) return;
	_bindingPath = std::move(value);
	_compiledBindingPath = {};
	NotifyOwnerChanged();
}
#endif

void DataGridBoundColumn::SetCompiledBindingPath(
	CompiledBindingPathView value)
{
	if (SameCompiledCollectionPath(_compiledBindingPath, value)) return;
	_compiledBindingPath = value;
#if CUI_ENABLE_DYNAMIC_XAML
	_bindingPath.clear();
#endif
	NotifyOwnerChanged();
}

void DataGridBoundColumn::SetBindingMode(BindingMode value)
{
	if (_bindingMode == value) return;
	_bindingMode = value;
	NotifyOwnerChanged();
}

void DataGridBoundColumn::SetDataSourceUpdateMode(
	DataSourceUpdateMode value)
{
	if (_dataSourceUpdateMode == value) return;
	_dataSourceUpdateMode = value;
	NotifyOwnerChanged();
}

void DataGridBoundColumn::SetBindingConverter(
	std::shared_ptr<const IBindingValueConverter> value)
{
	if (_converter == value) return;
	_converter = std::move(value);
	NotifyOwnerChanged();
}

void DataGridBoundColumn::SetFallbackValue(
	std::optional<BindingValue> value)
{
	_fallbackValue = std::move(value);
	NotifyOwnerChanged();
}

void DataGridBoundColumn::SetTargetNullValue(
	std::optional<BindingValue> value)
{
	_targetNullValue = std::move(value);
	NotifyOwnerChanged();
}

void DataGridBoundColumn::SetConverterParameter(
	std::optional<BindingValue> value)
{
	_converterParameter = std::move(value);
	NotifyOwnerChanged();
}

void DataGridBoundColumn::SetStringFormat(
	std::optional<std::wstring> value)
{
	_stringFormat = std::move(value);
	NotifyOwnerChanged();
}

Binding* DataGridBoundColumn::ApplyBinding(
	Control& target,
	const DependencyProperty& targetProperty,
	const BindingSourceReference& item,
	BindingMode defaultMode,
	bool forceMode) const
{
	if (!item) return nullptr;
	const auto mode = forceMode || _bindingMode == BindingMode::Default
		? defaultMode : _bindingMode;
	// Observable source-to-target modes use the generated element's stable
	// DataContext proxy so recycled content can retarget without rebuilding.
	// OneTime and OneWayToSource intentionally do not observe proxy retargets;
	// bind those modes directly to the current item and rebuild on row reuse.
	const bool bindCurrentItem = mode == BindingMode::OneTime
		|| mode == BindingMode::OneWayToSource;
#if CUI_ENABLE_DYNAMIC_XAML
	if (!_bindingPath.empty())
	{
		if (bindCurrentItem)
			return target.DataBindings.Add(
				targetProperty, item, _bindingPath, mode,
				_dataSourceUpdateMode, _converter, _fallbackValue,
				_targetNullValue, _converterParameter, _stringFormat);
		return target.DataBindings.Add(
			targetProperty, &target.DataContextSource(), _bindingPath, mode,
			_dataSourceUpdateMode, _converter, _fallbackValue,
			_targetNullValue, _converterParameter, _stringFormat);
	}
#endif
	if (!_compiledBindingPath.Empty())
	{
		if (bindCurrentItem)
			return target.DataBindings.Add(
				targetProperty, item, _compiledBindingPath, mode,
				_dataSourceUpdateMode, _converter, _fallbackValue,
				_targetNullValue, _converterParameter, _stringFormat);
		return target.DataBindings.Add(
			targetProperty, &target.DataContextSource(), _compiledBindingPath, mode,
			_dataSourceUpdateMode, _converter, _fallbackValue,
			_targetNullValue, _converterParameter, _stringFormat);
	}
	return nullptr;
}

std::unique_ptr<Control> DataGridTextColumn::GenerateElement(
	DataGridCell&, const BindingSourceReference& item) const
{
	auto label = std::make_unique<Label>();
	cui::framework::StyleAccess::SetResourceKey(
		*label, L"CuiDataGridTextElementStyle", false, true);
	(void)ApplyBinding(*label, Label::TextProperty(), item, BindingMode::OneWay);
	return label;
}

std::unique_ptr<Control> DataGridTextColumn::GenerateEditingElement(
	DataGridCell&, const BindingSourceReference& item) const
{
	auto editor = std::make_unique<TextBox>();
	// A DataGrid editor is hosted by the cell chrome.  Reusing the standalone
	// TextBox template would add a second rounded focus border, padding and a
	// clear button inside that chrome, unlike WPF's editing-element contract.
	cui::framework::StyleAccess::SetResourceKey(
		*editor, L"CuiDataGridTextEditingElementStyle", false, true);
	(void)ApplyBinding(
		*editor, TextBox::TextProperty(), item, BindingMode::TwoWay);
	return editor;
}

void DataGridCheckBoxColumn::SetIsThreeState(bool value)
{
	if (_isThreeState == value) return;
	_isThreeState = value;
	NotifyOwnerChanged();
}

std::unique_ptr<Control> DataGridCheckBoxColumn::GenerateElement(
	DataGridCell&, const BindingSourceReference& item) const
{
	auto checkBox = std::make_unique<DataGridDisplayCheckBox>();
	cui::framework::StyleAccess::SetResourceKey(
		*checkBox, L"CuiDataGridCheckBoxElementStyle", false, true);
	checkBox->SetFocusable(false);
	checkBox->SetIsThreeState(_isThreeState);
	(void)ApplyBinding(
		*checkBox, ToggleButton::IsCheckedProperty(), item,
		BindingMode::OneWay, true);
	return checkBox;
}

std::unique_ptr<Control> DataGridCheckBoxColumn::GenerateEditingElement(
	DataGridCell&, const BindingSourceReference& item) const
{
	auto checkBox = std::make_unique<CheckBox>();
	cui::framework::StyleAccess::SetResourceKey(
		*checkBox, L"CuiDataGridCheckBoxElementStyle", false, true);
	checkBox->SetIsThreeState(_isThreeState);
	(void)ApplyBinding(
		*checkBox, ToggleButton::IsCheckedProperty(), item,
		BindingMode::TwoWay);
	return checkBox;
}

void DataGridTemplateColumn::SetCellTemplate(ItemTemplateReference value)
{
	if (_cellTemplate == value) return;
	_cellTemplate = std::move(value);
	NotifyOwnerChanged();
}

void DataGridTemplateColumn::SetCellEditingTemplate(
	ItemTemplateReference value)
{
	if (_cellEditingTemplate == value) return;
	_cellEditingTemplate = std::move(value);
	NotifyOwnerChanged();
}

std::unique_ptr<Control> DataGridTemplateColumn::GenerateElement(
	DataGridCell& cell, const BindingSourceReference& item) const
{
	// A template callback may synchronously remove the column or destroy its
	// DataGrid.  Pin the managed template and do not touch the column after
	// entering user code.
	const auto cellTemplate = _cellTemplate;
	if (!cellTemplate) return std::make_unique<Label>();
	const size_t rowIndex = cell.GetRowOwner()
		? cell.GetRowOwner()->ItemIndex() : 0;
	return cellTemplate.Get()->Build(item, rowIndex);
}

std::unique_ptr<Control> DataGridTemplateColumn::GenerateEditingElement(
	DataGridCell& cell, const BindingSourceReference& item) const
{
	const auto editingTemplate = _cellEditingTemplate;
	if (editingTemplate)
	{
		const size_t rowIndex = cell.GetRowOwner()
			? cell.GetRowOwner()->ItemIndex() : 0;
		return editingTemplate.Get()->Build(item, rowIndex);
	}
	const auto cellTemplate = _cellTemplate;
	if (!cellTemplate) return std::make_unique<Label>();
	const size_t rowIndex = cell.GetRowOwner()
		? cell.GetRowOwner()->ItemIndex() : 0;
	return cellTemplate.Get()->Build(item, rowIndex);
}

const DependencyProperty& DataGridCell::IsSelectedProperty()
{
	static const auto registration = []
	{
		DependencyPropertyOptions<DataGridCell, bool> options;
		options.DefaultValue = false;
		options.Flags = DependencyPropertyFlags::AffectsRender;
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"State";
		options.Design.CategoryOrder = 70;
		options.Design.Order = 10;
		options.Design.Editor = DependencyPropertyEditorKind::Boolean;
		options.Design.Persistence = DependencyPropertyPersistence::Transient;
		options.Design.Browsable = false;
		)
		return DependencyPropertyRegistry::RegisterStatic<DataGridCell, bool>(
			DependencyPropertyRegistrationLiteral(L"IsSelected"),
			[](DataGridCell& target) { return target._isSelected; },
			[](DataGridCell& target, const bool& value)
			{ target.ApplyIsSelectedValue(value); }, {}, std::move(options));
	}();
	return *registration;
}

void DataGridCell::RegisterDependencyProperties()
{
	ContentControl::RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	(void)IsSelectedProperty();
#endif
	CUI_DESIGN_METADATA_ONLY(
	(void)DataGridCellFocusableMetadataRelation();
	)
}

DataGridCell::DataGridCell()
{
	RegisterDependencyProperties();
	auto activateCell =
		[this](Control*, MouseEventArgs& args)
		{
			if (args.ChangedButton != MouseButton::Left || !_row || !_column)
				return;
			if (_isEditing && _editingElement)
			{
				for (auto* current = args.OriginalSource; current;
					current = current->GetRoutedParent())
				{
					if (current != _editingElement) continue;
					// The editor already processed this pointer report.  Keep its
					// caret/capture/focus instead of treating the bubble as another
					// cell activation or a ListBox row click.
					args.Handled = true;
					return;
				}
			}
			auto* owner = _row->GetDataGridOwner();
			if (!owner) return;
			ControlWeakReference cellLifetime(this);
			ControlWeakReference ownerLifetime(owner);
			const size_t rowIndex = _row->ItemIndex();
			const size_t columnIndex = _columnIndex;
			const bool current = IsKeyboardFocusWithin
				&& owner->GetCurrentCell().IsValid()
				&& owner->GetCurrentCell().Item == _item
				&& owner->GetCurrentCell().Column == _column;
			const bool checkBoxActivation =
				[&]()
				{
					if (!dynamic_cast<DataGridCheckBoxColumn*>(_column))
						return false;
					// WPF's display CheckBox is not hit-testable. The cell still
					// enters edit on a repeated press anywhere, but preparation
					// toggles only when that press was over the checkbox glyph.
					auto* display = dynamic_cast<CheckBox*>(GetVisualContent());
					if (!display) return false;
					const auto renderPoint = args.HasRootPosition
						? D2D1::Point2F(args.RootX, args.RootY)
						: ToRenderSpace(*this,
							static_cast<float>(args.X),
							static_cast<float>(args.Y));
					D2D1_POINT_2F local{};
					if (!display->TryTransformRenderPointToLocal(
						renderPoint, local)
						|| !display->IsRenderPointInsideClip(renderPoint))
						return false;
					const auto size = display->GetActualSizeDip();
					return local.x >= 0.0f && local.y >= 0.0f
						&& local.x < size.width && local.y < size.height;
				}();
			args.Handled = true;
			// SetCurrentCell commits the old editor.  Do that before moving
			// focus, otherwise TextBox LostFocus can update the source before
			// CellEditEnding has a chance to cancel the transition.
			if (!owner->SetCurrentCell(rowIndex, columnIndex)) return;
			owner = dynamic_cast<DataGrid*>(ownerLifetime.Get());
			auto* liveCell = dynamic_cast<DataGridCell*>(cellLifetime.Get());
			if (!owner || !liveCell) return;
			if (auto* window = owner->GetPresentationWindow())
				window->SetKeyboardFocus(liveCell, true);
			owner = dynamic_cast<DataGrid*>(ownerLifetime.Get());
			if (!owner || !cellLifetime.Get()) return;
			const bool selected = owner->ApplySelectionForCellInput(
				rowIndex, columnIndex, args.Modifiers, true, true);
			if (!current || !selected
				|| HasModifier(args.Modifiers, ModifierKeys::Control)) return;
			if (!cellLifetime.Get()) return;
			owner = dynamic_cast<DataGrid*>(ownerLifetime.Get());
			if (owner) (void)owner->BeginEditCore(
				&args, checkBoxActivation);
		};
	RetainEventConnection(OnMouseDown.Subscribe(activateCell));
	// WPF raises the second physical press of a native double-click through the
	// same MouseLeftButtonDown class handler. CUI exposes WM_*DBLCLK as its own
	// routed event, so explicitly reuse the cell-activation transaction here.
	RetainEventConnection(OnMouseDoubleClick.Subscribe(std::move(activateCell)));
}

const DependencyPropertyMetadata*
DataGridCell::ResolveExactDependencyPropertyMetadata(
	const DependencyProperty& property) const
{
	if (&property == &Control::FocusableProperty())
		return &DataGridCellFocusableMetadataRelation().Metadata();
	return ContentControl::ResolveExactDependencyPropertyMetadata(property);
}

void DataGridCell::SetIsSelected(bool value)
{
	(void)TrySetPropertyValue(IsSelectedProperty(), BindingValue(value));
}

void DataGridCell::ApplyIsSelectedValue(bool value)
{
	if (_isSelected == value) return;
	if (!_synchronizingIsSelected && _row)
	{
		if (auto* owner = _row->GetDataGridOwner())
		{
			ControlWeakReference lifetime(this);
			owner->OnCellIsSelectedChanged(*this, value);
			auto* live = dynamic_cast<DataGridCell*>(lifetime.Get());
			if (!live || live->_isSelected == value) return;
		}
	}
	if (!SetPropertyField(IsSelectedProperty(), _isSelected, value)) return;
	SetStyleState(ControlStyleState::Selected, value);
	if (_suppressIsSelectedRoutedEvents) return;
	RoutedEventArgs args;
	if (value) Selected(this, args);
	else Unselected(this, args);
}

void DataGridCell::SetCurrentIsSelected(
	bool value, bool suppressRoutedEvents)
{
	if (_isSelected == value) return;
	ControlWeakReference lifetime(this);
	const bool previous = _synchronizingIsSelected;
	const bool previousSuppression = _suppressIsSelectedRoutedEvents;
	_synchronizingIsSelected = true;
	_suppressIsSelectedRoutedEvents = suppressRoutedEvents;
	try
	{
		(void)SetCurrentPropertyField(IsSelectedProperty(), _isSelected, value);
		if (auto* live = dynamic_cast<DataGridCell*>(lifetime.Get()))
		{
			live->_synchronizingIsSelected = previous;
			live->_suppressIsSelectedRoutedEvents = previousSuppression;
		}
	}
	catch (...)
	{
		if (auto* live = dynamic_cast<DataGridCell*>(lifetime.Get()))
		{
			live->_synchronizingIsSelected = previous;
			live->_suppressIsSelectedRoutedEvents = previousSuppression;
		}
		throw;
	}
}

bool DataGridCell::GetIsReadOnly() const noexcept
{
	if (!_row || !_column || !_row->GetDataGridOwner()) return true;
	if (_row->GetDataGridOwner()->GetIsReadOnly()
		|| _column->GetIsReadOnly()) return true;
	if (const auto* bound = dynamic_cast<const DataGridBoundColumn*>(_column))
	{
		return bound->GetBindingMode() == BindingMode::OneWay
			|| bound->GetBindingMode() == BindingMode::OneTime;
	}
	return false;
}

bool DataGridCell::Initialize(
	DataGridRow& row,
	DataGridColumn& column,
	const BindingSourceReference& item,
	size_t columnIndex,
	std::wstring* outError)
{
	_row = &row;
	_column = &column;
	_item = item;
	_columnIndex = columnIndex;
	(void)SetDataContext(item);
	auto* owner = row.GetDataGridOwner();
	if (!owner || columnIndex >= owner->ColumnCount()
		|| owner->GetColumnFromDisplayIndex(columnIndex) != &column) return false;
	const bool vertical = HasVerticalGridLines(
		owner->GetGridLinesVisibility());
	SetBorderThickness(Thickness(
		0.0f, 0.0f, vertical ? 1.0f : 0.0f, 0.0f));
	owner = row.GetDataGridOwner();
	if (!owner || columnIndex >= owner->ColumnCount()
		|| owner->GetColumnFromDisplayIndex(columnIndex) != &column) return false;
	SetBorderBrush(owner->GetVerticalGridLinesBrush());
	owner = row.GetDataGridOwner();
	if (!owner || columnIndex >= owner->ColumnCount()
		|| owner->GetColumnFromDisplayIndex(columnIndex) != &column) return false;
	if (!ReplaceContent(false, outError)) return false;
	owner = row.GetDataGridOwner();
	if (!owner || columnIndex >= owner->ColumnCount()
		|| owner->GetColumnFromDisplayIndex(columnIndex) != &column) return false;
	SetCurrentIsSelected(owner->IsCellSelected(row.ItemIndex(), columnIndex));
	return true;
}

bool DataGridCell::ReplaceContent(bool editing, std::wstring* outError)
{
	if (outError) outError->clear();
	if (!_column)
	{
		if (outError) *outError = L"DataGridCell 缺少 Column。";
		return false;
	}
	const ControlWeakReference cellLifetime(this);
	auto* column = _column;
	const auto item = _item;
	try
	{
		auto content = editing
			? column->GenerateEditingElement(*this, item)
			: column->GenerateElement(*this, item);
		auto* live = dynamic_cast<DataGridCell*>(cellLifetime.Get());
		if (!live) return false;
		if (!content) content = std::make_unique<Label>();
		auto* raw = content.get();
		(void)live->SetVisualContent(std::move(content));
		live = dynamic_cast<DataGridCell*>(cellLifetime.Get());
		if (!live) return false;
		live->_editingElement = editing ? raw : nullptr;
		live->_isEditing = editing;
		return true;
	}
	catch (const std::exception& error)
	{
		if (outError)
		{
			const auto* first = reinterpret_cast<const unsigned char*>(
				error.what());
			while (*first) outError->push_back(
				static_cast<wchar_t>(*first++));
		}
		return false;
	}
	catch (...)
	{
		if (outError) *outError = L"DataGridCell 内容生成失败。";
		return false;
	}
}

bool DataGridCell::BeginEdit()
{
	if (!_row || !_row->GetDataGridOwner()) return false;
	auto* owner = _row->GetDataGridOwner();
	ControlWeakReference ownerLifetime(owner);
	if (!owner->SetCurrentCell(_row->ItemIndex(), _columnIndex)) return false;
	owner = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	return owner ? owner->BeginEdit() : false;
}

bool DataGridCell::CommitEdit()
{
	return _row && _row->GetDataGridOwner()
		? _row->GetDataGridOwner()->CommitEdit() : false;
}

bool DataGridCell::CancelEdit()
{
	return _row && _row->GetDataGridOwner()
		? _row->GetDataGridOwner()->CancelEdit() : false;
}

bool DataGridCell::ProcessInput(const InputReport& input)
{
	if (input.Kind == InputReportKind::KeyDown
		&& _row && _row->GetDataGridOwner())
	{
		auto* owner = _row->GetDataGridOwner();
		if (input.Key == Key::F2)
		{
			KeyEventArgs args(input.Key, input.Modifiers);
			return owner->BeginEdit(&args);
		}
		if (input.Key == Key::Space && !_isEditing
			&& dynamic_cast<DataGridCheckBoxColumn*>(_column))
		{
			KeyEventArgs args(input.Key, input.Modifiers);
			return owner->BeginEditCore(&args, true);
		}
		if (input.Key == Key::Return && _isEditing)
			return owner->CommitEdit();
		if (input.Key == Key::Escape && _isEditing)
			return owner->CancelEdit();
	}
	return ContentControl::ProcessInput(input);
}

void DataGridCell::OnRender()
{
	// Grid-line and background chrome belongs to the declarative Cell template.
	ContentControl::OnRender();
}

DataGridRow::DataGridRow()
{
	RetainEventConnection(OnPropertyValueChanged.Subscribe(
		[this](DependencyObject*, const DependencyPropertyChangedEventArgs& args)
		{
			if ((args.Property != &ItemContainerControl::IsSelectedProperty()
				&& args.Property != &Control::IsMouseOverProperty()))
				return;
			if (args.Property == &ItemContainerControl::IsSelectedProperty())
				UpdateRowHeader();
			auto* owner = GetDataGridOwner();
			if (!owner) return;
			if (GetIsSelected() || IsMouseOver)
			{
				// Let the row state triggers own selected and pointer-over chrome.
				(void)ClearPropertyValue(Control::BackgroundProperty());
			}
			else
			{
				SetBackground((ItemIndex() % 2) != 0
					? owner->GetAlternatingRowBackground()
					: owner->GetRowBackground());
			}
		}));
}

DataGrid* DataGridRow::GetDataGridOwner() const noexcept
{
	return dynamic_cast<DataGrid*>(_ownerLifetime.Get());
}

DataGridCell* DataGridRow::GetCell(size_t columnIndex) const noexcept
{
	if (!_columnStorageIsSparse)
		return columnIndex < _cells.size() ? _cells[columnIndex] : nullptr;
	return columnIndex >= _realizedColumnBegin
		&& columnIndex < _realizedColumnEnd
		&& columnIndex - _realizedColumnBegin < _cells.size()
		? _cells[columnIndex - _realizedColumnBegin] : nullptr;
}

bool DataGridRow::Initialize(
	DataGrid& owner,
	const BindingSourceReference& item,
	size_t index,
	std::wstring* outError)
{
	if (outError) outError->clear();
	const ControlWeakReference ownerLifetime(&owner);
	_ownerLifetime = &owner;
	_item = item;
	SetItemIndex(index);
	(void)SetDataContext(item);
	auto* liveOwner = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!liveOwner) return false;
	if (std::isfinite(liveOwner->GetRowHeight()))
		SetHeight(cui::layout::Length::Fixed(
			static_cast<float>(liveOwner->GetRowHeight())));
	else SetHeight(cui::layout::Length::Auto());
	liveOwner = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!liveOwner) return false;
	SetBackground((index % 2) != 0
		? liveOwner->GetAlternatingRowBackground()
		: liveOwner->GetRowBackground());
	liveOwner = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!liveOwner) return false;
	const bool horizontal = HasHorizontalGridLines(
		liveOwner->GetGridLinesVisibility());
	SetBorderThickness(Thickness(
		0.0f, 0.0f, 0.0f, horizontal ? 1.0f : 0.0f));
	liveOwner = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!liveOwner) return false;
	SetBorderBrush(liveOwner->GetHorizontalGridLinesBrush());
	liveOwner = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!liveOwner) return false;

	auto rowLayout = std::make_unique<Grid>();
	const double rowHeaderWidth = liveOwner->ResolveRowHeaderWidth();
	const bool rowHeaderVisible = HasRowHeaders(
		liveOwner->GetHeadersVisibility());
	const bool rowHeaderAutoWidth =
		std::isnan(liveOwner->GetRowHeaderWidth());
	rowLayout->AddColumn(!rowHeaderVisible
		? GridLength::Pixels(0.0f)
		: rowHeaderAutoWidth
			? GridLength::Auto()
			: GridLength::Pixels(static_cast<float>(rowHeaderWidth)));
	rowLayout->AddColumn(GridLength::Auto());

	auto rowHeaderHost = std::make_unique<Grid>();
	// Horizontally scrolled cells may render beneath the frozen row-header
	// gutter. Keep that chrome above the cells for both rendering and hit test.
	rowHeaderHost->SetZIndex(1);
	auto rowHeader = std::make_unique<DataGridRowHeader>();
	rowHeader->Initialize(*this);
	Grid::SetColumn(*rowHeaderHost, 0);
	auto* rowHeaderRaw = rowHeaderHost->AddOwned(std::move(rowHeader));
	auto* rowHeaderHostRaw = rowLayout->AddOwned(std::move(rowHeaderHost));

	auto grid = std::make_unique<DataGridCellsPresenter>(*this);
	const auto realizedColumns = liveOwner->ResolveRealizedColumnRange();
	const size_t initialColumnCount = liveOwner->ColumnCount();
	const size_t initialWidthProjectionRevision =
		liveOwner->_columnWidthProjectionRevision;
	const size_t initialCellSelectionRevision =
		liveOwner->_cellSelectionRevision;
	const bool sparseColumns = liveOwner->GetEnableColumnVirtualization()
		&& initialColumnCount > 0;
	_columnStorageIsSparse = sparseColumns;
	_realizedColumnBegin = sparseColumns ? realizedColumns.first : 0;
	_realizedColumnEnd = sparseColumns
		? realizedColumns.second : initialColumnCount;
	_cells.assign(sparseColumns
		? _realizedColumnEnd - _realizedColumnBegin
		: initialColumnCount, nullptr);
	const size_t loopBegin = sparseColumns ? _realizedColumnBegin : 0;
	const size_t loopEnd = sparseColumns
		? _realizedColumnEnd : initialColumnCount;
	for (size_t columnIndex = loopBegin; columnIndex < loopEnd; ++columnIndex)
	{
		liveOwner = dynamic_cast<DataGrid*>(ownerLifetime.Get());
		if (!liveOwner || liveOwner->ColumnCount() != initialColumnCount)
			return false;
		auto* column = liveOwner->GetColumnFromDisplayIndex(columnIndex);
		if (!column) continue;
		liveOwner = dynamic_cast<DataGrid*>(ownerLifetime.Get());
		if (!liveOwner || liveOwner->ColumnCount() != initialColumnCount
			|| liveOwner->GetColumnFromDisplayIndex(columnIndex) != column) return false;
		auto cell = std::make_unique<DataGridCell>();
		if (!cell->Initialize(*this, *column, item, columnIndex, outError))
			return false;
		if (!ownerLifetime.Get()) return false;
		const size_t storageIndex = sparseColumns
			? columnIndex - _realizedColumnBegin : columnIndex;
		Grid::SetColumn(*cell, static_cast<int>(sparseColumns
			? storageIndex + 1 : columnIndex));
		_cells[storageIndex] = grid->AddOwned(std::move(cell));
	}
	if (!ownerLifetime.Get()) return false;
	Grid::SetColumn(*grid, 1);
	auto* cellsGridRaw = rowLayout->AddOwned(std::move(grid));
	const ControlWeakReference rowLifetime(this);
	_rowLayoutGrid = rowLayout.get();
	_rowHeaderHost = rowHeaderHostRaw;
	_rowHeader = rowHeaderRaw;
	_cellsGrid = cellsGridRaw;
	UpdateRowHeader();
	if (!rowLifetime.Get() || !ownerLifetime.Get()) return false;
	liveOwner = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!liveOwner) return false;
	if (liveOwner->_columnWidthProjectionRevision
		== initialWidthProjectionRevision)
		_appliedColumnWidthProjectionRevision =
			initialWidthProjectionRevision;
	if (liveOwner->_cellSelectionRevision == initialCellSelectionRevision)
		_appliedCellSelectionRevision = initialCellSelectionRevision;
	UpdateHorizontalScrollOffset(liveOwner->_horizontalScrollOffset);
	if (!rowLifetime.Get() || !ownerLifetime.Get()) return false;
	(void)SetVisualContent(std::move(rowLayout));
	return rowLifetime.Get() != nullptr && ownerLifetime.Get() != nullptr;
}

bool DataGridRow::RefreshRealizedColumns(
	size_t begin, size_t end, std::wstring* outError)
{
	if (outError) outError->clear();
	auto* owner = GetDataGridOwner();
	if (!owner || !_cellsGrid || !_columnStorageIsSparse
		|| begin > end || end > owner->ColumnCount()) return false;
	if (begin == _realizedColumnBegin && end == _realizedColumnEnd)
	{
		UpdateColumnWidths();
		return true;
	}

	const ControlWeakReference rowLifetime(this);
	const ControlWeakReference ownerLifetime(owner);
	const ControlWeakReference gridLifetime(_cellsGrid);
	const size_t columnCount = owner->ColumnCount();
	const size_t oldBegin = _realizedColumnBegin;
	const size_t oldEnd = _realizedColumnEnd;
	const auto oldCells = _cells;
	std::vector<DataGridCell*> next(end - begin, nullptr);
	std::vector<std::unique_ptr<DataGridCell>> created(end - begin);
	try
	{
		const size_t overlapBegin = (std::max)(begin, oldBegin);
		const size_t overlapEnd = (std::min)(end, oldEnd);
		for (size_t index = overlapBegin; index < overlapEnd; ++index)
		{
			const size_t oldSlot = index - oldBegin;
			if (oldSlot >= oldCells.size()) return false;
			auto* cell = oldCells[oldSlot];
			if (!cell || !_cellsGrid->ContainsControl(cell)
				|| cell->_columnIndex != index
				|| cell->_column != owner->GetColumnFromDisplayIndex(index)) return false;
			next[index - begin] = cell;
		}
		for (size_t index = begin; index < end; ++index)
		{
			const size_t slot = index - begin;
			if (next[slot]) continue;
			owner = dynamic_cast<DataGrid*>(ownerLifetime.Get());
			if (!owner || !gridLifetime.Get()
				|| owner->ColumnCount() != columnCount)
				return false;
			auto* column = owner->GetColumnFromDisplayIndex(index);
			if (!column) return false;
			auto cell = std::make_unique<DataGridCell>();
			if (!cell->Initialize(*this, *column, _item, index, outError))
				return false;
			if (!rowLifetime.Get() || !ownerLifetime.Get()
				|| !gridLifetime.Get()) return false;
			Grid::SetColumn(*cell, static_cast<int>(slot + 1));
			next[slot] = cell.get();
			created[slot] = std::move(cell);
		}
		// Do not mutate the established visual tree until every entering cell
		// has been generated successfully.  Column element/binding creation may
		// throw; staging preserves the complete old strip in that case.
		for (size_t slot = 0; slot < created.size(); ++slot)
		{
			auto* grid = dynamic_cast<DataGridCellsPresenter*>(
				gridLifetime.Get());
			if (!grid || !rowLifetime.Get() || !ownerLifetime.Get()) return false;
			if (created[slot])
				next[slot] = grid->AddOwned(std::move(created[slot]));
		}
	}
	catch (...)
	{
		if (outError && outError->empty())
			*outError = L"DataGrid 横向虚拟单元格更新失败。";
		return false;
	}
	for (size_t slot = 0; slot < oldCells.size(); ++slot)
	{
		const size_t index = oldBegin + slot;
		if (index >= begin && index < end) continue;
		auto* grid = dynamic_cast<DataGridCellsPresenter*>(
			gridLifetime.Get());
		auto* cell = oldCells[slot];
		if (!grid || (cell && grid->ContainsControl(cell)
			&& !grid->DeleteVisualChild(cell))) return false;
		if (!rowLifetime.Get() || !ownerLifetime.Get()) return false;
	}

	owner = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	auto* grid = dynamic_cast<DataGridCellsPresenter*>(gridLifetime.Get());
	if (!owner || !grid || owner->ColumnCount() != columnCount) return false;
	for (size_t slot = 0; slot < next.size(); ++slot)
	{
		auto* cell = next[slot];
		if (!cell || !grid->ContainsControl(cell)) return false;
		Grid::SetColumn(*cell, static_cast<int>(slot + 1));
	}
	_cells = std::move(next);
	_realizedColumnBegin = begin;
	_realizedColumnEnd = end;
	_appliedColumnWidthProjectionRevision = 0;
	UpdateColumnWidths();
	return rowLifetime.Get() != nullptr && ownerLifetime.Get() != nullptr;
}

void DataGridRow::UpdateColumnWidths(bool propagateLayoutInvalidation)
{
	auto* owner = GetDataGridOwner();
	if (!owner || !_cellsGrid) return;
	auto* const grid = _cellsGrid;
	const ControlWeakReference rowLifetime(this);
	const ControlWeakReference ownerLifetime(owner);
	const ControlWeakReference gridLifetime(grid);
	const size_t projectionRevision =
		owner->_columnWidthProjectionRevision;
	// A realized row receives this projection during Initialize.  The subsequent
	// realization callback also visits every retained row; treating an identical
	// revision as a fresh invalidation turns one entering/leaving row during a
	// vertical window resize into a complete DataGrid relayout.  New rows already
	// carry their normal construction/template dirtiness, so an applied revision
	// is a true no-op here just as it is for the header presenter.
	if (_appliedColumnWidthProjectionRevision == projectionRevision) return;
	const size_t columnCount = owner->ColumnCount();
	const bool sparseColumns = _columnStorageIsSparse;
	const size_t begin = sparseColumns
		? _realizedColumnBegin : 0;
	const size_t end = sparseColumns
		? _realizedColumnEnd : columnCount;
	if (begin > end || end > columnCount) return;
	if (columnCount > 0)
	{
		double ignored = 0.0;
		double total = 0.0;
		if (!owner->TryResolveColumnBounds(
			columnCount - 1, ignored, total)) return;
	}
	auto* row = dynamic_cast<DataGridRow*>(rowLifetime.Get());
	owner = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	auto* liveGrid = dynamic_cast<DataGridCellsPresenter*>(gridLifetime.Get());
	if (row != this || owner == nullptr || owner != ownerLifetime.Get()
		|| liveGrid != grid || row->_cellsGrid != liveGrid
		|| row->GetDataGridOwner() != owner
		|| owner->ColumnCount() != columnCount
		|| owner->_columnWidthProjectionRevision != projectionRevision
		|| row->_columnStorageIsSparse != sparseColumns
		|| row->_realizedColumnBegin != begin
		|| row->_realizedColumnEnd != end) return;
	liveGrid->InvalidateColumnLayout(propagateLayoutInvalidation);
	// DataGridRow is a ContentControl. Its child Grid may keep the same outer
	// row slot while the cells presenter changes width, so SizeChanged will not
	// set the content-layout pending bit for us. Mark that local transaction
	// explicitly without starting another ancestor walk.
	row->_contentLayoutPending = true;
	row = dynamic_cast<DataGridRow*>(rowLifetime.Get());
	owner = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	liveGrid = dynamic_cast<DataGridCellsPresenter*>(gridLifetime.Get());
	if (row == this && owner && liveGrid == grid
		&& row->_cellsGrid == liveGrid
		&& row->GetDataGridOwner() == owner
		&& owner->_columnWidthProjectionRevision == projectionRevision
		&& row->_columnStorageIsSparse == sparseColumns
		&& row->_realizedColumnBegin == begin
		&& row->_realizedColumnEnd == end)
		row->_appliedColumnWidthProjectionRevision = projectionRevision;
}

void DataGridRow::UpdateRowHeader()
{
	auto* owner = GetDataGridOwner();
	if (!owner || !_rowLayoutGrid || !_rowHeaderHost || !_rowHeader) return;
	const bool visible = HasRowHeaders(owner->GetHeadersVisibility());
	const bool autoWidth = std::isnan(owner->GetRowHeaderWidth());
	const double width = owner->ResolveRowHeaderWidth();
	const bool selected = GetIsSelected();
	const bool projectionCurrent = _rowHeaderProjectionInitialized
		&& _appliedRowHeaderVisible == visible
		&& _appliedRowHeaderAutoWidth == autoWidth
		&& (autoWidth
			|| std::abs(_appliedRowHeaderWidth - width) <= 0.0001);
	if (projectionCurrent)
	{
		_rowHeader->SetCurrentIsRowSelected(selected);
		return;
	}
	const ControlWeakReference rowLifetime(this);
	const ControlWeakReference ownerLifetime(owner);
	const ControlWeakReference layoutLifetime(_rowLayoutGrid);
	const ControlWeakReference hostLifetime(_rowHeaderHost);
	const ControlWeakReference headerLifetime(_rowHeader);
	auto* layout = dynamic_cast<Grid*>(layoutLifetime.Get());
	if (!layout) return;
	std::vector<ColumnDefinition> definitions;
	definitions.reserve(2);
	definitions.emplace_back(!visible
		? GridLength::Pixels(0.0f)
		: autoWidth
			? GridLength::Auto()
			: GridLength::Pixels(static_cast<float>(width)));
	definitions.emplace_back(GridLength::Auto());
	layout->ReplaceColumns(std::move(definitions));
	if (!rowLifetime.Get() || !ownerLifetime.Get()) return;
	auto* host = dynamic_cast<Grid*>(hostLifetime.Get());
	if (!host) return;
	host->SetVisibility(
		visible ? Visibility::Visible : Visibility::Collapsed);
	if (!rowLifetime.Get() || !ownerLifetime.Get()) return;
	auto* header = dynamic_cast<DataGridRowHeader*>(headerLifetime.Get());
	if (!header) return;
	header->SetVisibility(
		visible ? Visibility::Visible : Visibility::Collapsed);
	if (!rowLifetime.Get() || !ownerLifetime.Get()) return;
	header = dynamic_cast<DataGridRowHeader*>(headerLifetime.Get());
	if (!header) return;
	header->SetWidth(autoWidth
		? cui::layout::Length::Auto()
		: cui::layout::Length::Fixed(static_cast<float>(width)));
	if (!rowLifetime.Get() || !ownerLifetime.Get()) return;
	header = dynamic_cast<DataGridRowHeader*>(headerLifetime.Get());
	if (!header) return;
	header->SetCurrentIsRowSelected(selected);
	auto* row = dynamic_cast<DataGridRow*>(rowLifetime.Get());
	owner = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (row && owner && row->GetDataGridOwner() == owner)
	{
		row->_rowHeaderProjectionInitialized = true;
		row->_appliedRowHeaderVisible = visible;
		row->_appliedRowHeaderAutoWidth = autoWidth;
		row->_appliedRowHeaderWidth = width;
	}
}

void DataGridRow::UpdateHorizontalScrollOffset(double offset)
{
	if (!_rowHeaderHost) return;
	if (!std::isfinite(offset)) offset = 0.0;
	if (std::isfinite(_appliedHorizontalScrollOffset)
		&& std::abs(_appliedHorizontalScrollOffset - offset) <= 0.0001)
		return;
	if (std::abs(offset) <= 0.0001)
		_rowHeaderHost->ClearRenderTransform();
	else _rowHeaderHost->SetRenderTransform(HorizontalTranslation(offset));
	_appliedHorizontalScrollOffset = offset;
}

const DependencyProperty& DataGridRowHeader::IsRowSelectedProperty()
{
	return IsRowSelectedPropertyKey().Property();
}

const DependencyPropertyKey& DataGridRowHeader::IsRowSelectedPropertyKey()
{
	static const auto registration = []
	{
		DependencyPropertyOptions<DataGridRowHeader, bool> options;
		options.DefaultValue = false;
		options.Flags = DependencyPropertyFlags::AffectsRender;
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"State";
		options.Design.CategoryOrder = 70;
		options.Design.Order = 10;
		options.Design.Editor = DependencyPropertyEditorKind::Boolean;
		options.Design.Persistence = DependencyPropertyPersistence::Transient;
		options.Design.Browsable = false;
		)
		return DependencyPropertyRegistry::RegisterReadOnlyStatic<
			DataGridRowHeader, bool>(
				DependencyPropertyRegistrationLiteral(L"IsRowSelected"),
				[](DataGridRowHeader& target)
				{ return target._isRowSelected; },
				[](DataGridRowHeader& target, const bool& value)
				{
					(void)target.SetReadOnlyPropertyField(
						IsRowSelectedPropertyKey(),
						target._isRowSelected, value);
				}, {}, std::move(options));
	}();
	return registration.Key();
}

void DataGridRowHeader::RegisterDependencyProperties()
{
	Button::RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	(void)IsRowSelectedProperty();
#endif
	CUI_DESIGN_METADATA_ONLY(
	(void)DataGridRowHeaderFocusableMetadataRelation();
	(void)DataGridRowHeaderClickModeMetadataRelation();
	)
}

DataGridRowHeader::DataGridRowHeader()
{
	RegisterDependencyProperties();
	(void)TrySetCurrentPropertyValue(
		ButtonBase::ClickModeProperty(),
		BindingValue(::ClickMode::Press));
	RetainEventConnection(OnPreviewMouseDown.Subscribe(
		[this](Control*, MouseEventArgs& args)
		{
			if (args.ChangedButton == MouseButton::Left)
				_activationModifiers = args.Modifiers;
		}));
	RetainEventConnection(OnPreviewMouseDoubleClick.Subscribe(
		[this](Control*, MouseEventArgs& args)
		{
			if (args.ChangedButton == MouseButton::Left)
				_activationModifiers = args.Modifiers;
		}));
	RetainEventConnection(OnMouseDown.Subscribe(
		[this](Control*, MouseEventArgs& args)
		{
			if (args.ChangedButton != MouseButton::Left) return;
			_activationModifiers = args.Modifiers;
			// ButtonBase already owns the press. Stop the ancestor DataGridRow
			// from treating the same press as an ordinary row-container click.
			args.Handled = true;
		}));
	RetainEventConnection(OnMouseDoubleClick.Subscribe(
		[this](Control*, MouseEventArgs& args)
		{
			if (args.ChangedButton != MouseButton::Left) return;
			_activationModifiers = args.Modifiers;
			args.Handled = true;
		}));
}

const DependencyPropertyMetadata*
DataGridRowHeader::ResolveExactDependencyPropertyMetadata(
	const DependencyProperty& property) const
{
	if (&property == &Control::FocusableProperty())
		return &DataGridRowHeaderFocusableMetadataRelation().Metadata();
	if (&property == &ButtonBase::ClickModeProperty())
		return &DataGridRowHeaderClickModeMetadataRelation().Metadata();
	return Button::ResolveExactDependencyPropertyMetadata(property);
}

DataGridRow* DataGridRowHeader::GetRowOwner() const noexcept
{
	return dynamic_cast<DataGridRow*>(_rowLifetime.Get());
}

void DataGridRowHeader::InvalidateSharedWidthMeasure()
{
	RequestLayout();
}

cui::core::Size DataGridRowHeader::FinalizeMeasureCore(
	cui::core::Size intrinsic,
	const cui::core::Constraints& available)
{
	intrinsic = Button::FinalizeMeasureCore(intrinsic, available);
	auto* row = GetRowOwner();
	auto* owner = row ? row->GetDataGridOwner() : nullptr;
	double sharedWidth = intrinsic.width;
	if (owner && owner->TryShareRowHeaderDesiredWidth(
		*this, intrinsic.width, sharedWidth))
		intrinsic.width = static_cast<float>(sharedWidth);
	return intrinsic;
}

void DataGridRowHeader::Initialize(DataGridRow& row)
{
	_rowLifetime = &row;
	SetCurrentIsRowSelected(row.GetIsSelected());
}

void DataGridRowHeader::SetCurrentIsRowSelected(bool value)
{
	(void)SetReadOnlyPropertyField(
		IsRowSelectedPropertyKey(), _isRowSelected, value);
}

bool DataGridRowHeader::ProcessInput(const InputReport& input)
{
	if ((input.Kind == InputReportKind::PointerDown
		|| input.Kind == InputReportKind::PointerDoubleClick)
		&& input.ChangedButton == MouseButton::Left)
		_activationModifiers = input.Modifiers;
	else if (input.Kind == InputReportKind::KeyDown
		&& input.Key == Key::Space)
		_activationModifiers = input.Modifiers;
	else if (input.Kind == InputReportKind::Cancel
		|| input.Kind == InputReportKind::CaptureLost)
		_activationModifiers = ModifierKeys::None;
	return Button::ProcessInput(input);
}

bool DataGridRowHeader::OnClick()
{
	const ControlWeakReference headerLifetime(this);
	const ModifierKeys modifiers = _activationModifiers;
	_activationModifiers = ModifierKeys::None;
	const bool clicked = Button::OnClick();
	auto* header = dynamic_cast<DataGridRowHeader*>(headerLifetime.Get());
	if (!header) return clicked;
	if (header->IsMouseCaptured()) (void)header->ReleaseMouseCapture();
	header = dynamic_cast<DataGridRowHeader*>(headerLifetime.Get());
	if (!header) return clicked;
	auto* row = header->GetRowOwner();
	auto* owner = row ? row->GetDataGridOwner() : nullptr;
	return owner ? owner->HandleRowHeaderClick(*row, modifiers) || clicked
		: clicked;
}

DataGridColumnHeader::DataGridColumnHeader()
{
	// A templated header is normally hit through its Border/ContentPresenter.
	// Consume the routed descendant press at the header boundary so the native
	// resize behavior is independent of the theme's decorative subtree.
	RetainEventConnection(OnMouseDown.Subscribe(
		[this](Control*, MouseEventArgs& args)
		{
			if (_isResizing || args.ChangedButton != MouseButton::Left) return;
			size_t columnIndex = DataGridCellInfo::InvalidIndex;
			bool resizeFromLeftEdge = false;
			if (!TryResolveResizeColumn(
				args.X, columnIndex, resizeFromLeftEdge)) return;

			// The templated descendant has already started ButtonBase's click
			// transaction.  End it before the resize transaction acquires capture;
			// otherwise its synchronous CaptureLost would cancel the resize again.
			const ControlWeakReference lifetime(this);
			InputReport cancelButtonPress;
			cancelButtonPress.Kind = InputReportKind::Cancel;
			(void)ButtonBase::ProcessInput(cancelButtonPress);
			auto* source = dynamic_cast<DataGridColumnHeader*>(lifetime.Get());
			if (!source || !source->BeginColumnResize(args.X)) return;
			args.Handled = true;
		}));
	RetainEventConnection(OnMouseDoubleClick.Subscribe(
		[this](Control*, MouseEventArgs& args)
		{
			if (args.OriginalSource == this
				|| args.ChangedButton != MouseButton::Left || !_owner) return;
			size_t columnIndex = DataGridCellInfo::InvalidIndex;
			bool resizeFromLeftEdge = false;
			if (!TryResolveResizeColumn(
				args.X, columnIndex, resizeFromLeftEdge)
				|| !_owner->AutoSizeColumn(columnIndex)) return;
			InputReport cancelButtonPress;
			cancelButtonPress.Kind = InputReportKind::Cancel;
			(void)ButtonBase::ProcessInput(cancelButtonPress);
			args.Handled = true;
		}));
}

void DataGridColumnHeader::Initialize(
	DataGrid& owner, DataGridColumn& column, size_t index)
{
	_owner = &owner;
	_column = &column;
	_columnIndex = index;
	SetContent(column.GetHeader());
	if (std::isfinite(owner.GetColumnHeaderHeight()))
		SetHeight(cui::layout::Length::Fixed(
			static_cast<float>(owner.GetColumnHeaderHeight())));
	else SetHeight(cui::layout::Length::Auto());
}

bool DataGridColumnHeader::ProcessInput(const InputReport& input)
{
	if (input.Kind == InputReportKind::PointerDown
		&& input.ChangedButton == MouseButton::Left
		&& BeginColumnResize(input.X))
	{
		auto args = input.CreateMouseEventArgs();
		OnMouseDown(this, args);
		return true;
	}
	if (input.Kind == InputReportKind::PointerMove && _isResizing)
	{
		(void)ContinueColumnResize(input.X);
		auto args = input.CreateMouseEventArgs();
		OnMouseMove(this, args);
		return true;
	}
	if (input.Kind == InputReportKind::PointerUp && _isResizing
		&& input.ChangedButton == MouseButton::Left)
	{
		EndColumnResize(false);
		auto args = input.CreateMouseEventArgs();
		OnMouseUp(this, args);
		return true;
	}
	if ((input.Kind == InputReportKind::Cancel
		|| input.Kind == InputReportKind::CaptureLost) && _isResizing)
	{
		EndColumnResize(true);
		return true;
	}
	if (input.Kind == InputReportKind::PointerDoubleClick
		&& input.ChangedButton == MouseButton::Left)
	{
		size_t columnIndex = DataGridCellInfo::InvalidIndex;
		bool resizeFromLeftEdge = false;
		if (TryResolveResizeColumn(
			input.X, columnIndex, resizeFromLeftEdge)
			&& _owner && _owner->AutoSizeColumn(columnIndex))
		{
			auto args = input.CreateMouseEventArgs();
			OnMouseDoubleClick(this, args);
			return true;
		}
	}
	if (input.Kind == InputReportKind::PointerDown
		|| input.Kind == InputReportKind::KeyDown)
	{
		_multiColumnSortRequested =
			input.HasModifier(ModifierKeys::Shift);
	}
	return Button::ProcessInput(input);
}

CursorKind DataGridColumnHeader::QueryCursor(int localX, int localY)
{
	if (_isResizing) return CursorKind::SizeWE;
	const auto size = GetActualSizeDip();
	if (localY < 0 || static_cast<double>(localY) > size.height)
		return Button::QueryCursor(localX, localY);
	size_t columnIndex = DataGridCellInfo::InvalidIndex;
	bool resizeFromLeftEdge = false;
	return TryResolveResizeColumn(
		localX, columnIndex, resizeFromLeftEdge)
		? CursorKind::SizeWE : Button::QueryCursor(localX, localY);
}

bool DataGridColumnHeader::TryResolveResizeColumn(
	int localX, size_t& columnIndex,
	bool& resizeFromLeftEdge) const noexcept
{
	columnIndex = DataGridCellInfo::InvalidIndex;
	resizeFromLeftEdge = false;
	if (!_owner || !_owner->GetCanUserResizeColumns()
		|| _columnIndex == DataGridCellInfo::InvalidIndex) return false;
	const auto size = GetActualSizeDip();
	const double width = static_cast<double>(size.width);
	if (!std::isfinite(width) || width <= 0.0) return false;
	if (localX < 0 || static_cast<double>(localX) > width) return false;
	if (static_cast<double>(localX) >= width - DataGridColumnHeaderGripperWidth)
		columnIndex = _columnIndex;
	else if (localX <= DataGridColumnHeaderGripperWidth && _columnIndex > 0)
	{
		// The same divider exposes two distinct resize handles. Its left
		// header adjusts the left column from the right; its right header adjusts
		// the right column from the left. A fixed right column falls back to the
		// former behavior so the divider remains draggable from either hit zone.
		auto* current = _owner->GetColumnFromDisplayIndex(_columnIndex);
		if (current && current->GetCanUserResize())
		{
			columnIndex = _columnIndex;
			resizeFromLeftEdge = true;
		}
		else columnIndex = _columnIndex - 1;
	}
	else return false;
	auto* column = _owner->GetColumnFromDisplayIndex(columnIndex);
	return column && column->GetCanUserResize();
}

bool DataGridColumnHeader::BeginColumnResize(int localX)
{
	size_t columnIndex = DataGridCellInfo::InvalidIndex;
	bool resizeFromLeftEdge = false;
	if (!TryResolveResizeColumn(
		localX, columnIndex, resizeFromLeftEdge) || !_owner) return false;
	const double width = _owner->GetColumnDisplayWidth(columnIndex);
	if (!std::isfinite(width)
		|| !_owner->BeginColumnResizeTransaction(
			columnIndex, resizeFromLeftEdge)) return false;
	const ControlWeakReference ownerLifetime(_owner);
	bool transactionCommitted = false;
	auto rollbackTransaction = MakeScopeExit(
		[ownerLifetime, &transactionCommitted]
		{
			if (transactionCommitted) return;
			if (auto* owner = dynamic_cast<DataGrid*>(ownerLifetime.Get()))
				owner->EndColumnResizeTransaction(true);
		});
	_isResizing = true;
	_resizeFromLeftEdge = resizeFromLeftEdge;
	_resizingColumnIndex = columnIndex;
	_resizeStartRenderX = RenderSpaceX(*this, localX);
	_resizeStartWidth = width;
	// Normalized-input unit tests may exercise a detached header.  A presented
	// header, however, must own native mouse capture for the drag to be valid.
	if (!GetPresentationWindow())
	{
		transactionCommitted = true;
		return true;
	}
	const ControlWeakReference lifetime(this);
	const bool captured = CaptureMouse();
	auto* source = dynamic_cast<DataGridColumnHeader*>(lifetime.Get());
	if (!source) return false;
	if (!captured || !source->_isResizing || !source->IsMouseCaptured())
	{
		if (source->_isResizing) source->EndColumnResize(true);
		return false;
	}
	transactionCommitted = true;
	return true;
}

bool DataGridColumnHeader::ContinueColumnResize(int localX)
{
	if (!_isResizing || !_owner
		|| _resizingColumnIndex == DataGridCellInfo::InvalidIndex) return false;
	const double delta = RenderSpaceX(*this, localX) - _resizeStartRenderX;
	if (_owner->ResizeColumnInTransaction(
		_resizingColumnIndex,
		_resizeStartWidth + (_resizeFromLeftEdge ? -delta : delta)))
		return true;
	// An external schema/eligibility mutation can abort the owner transaction
	// while this header still owns capture. End the local gesture immediately so
	// later Move/Up input is not swallowed or accidentally committed.
	EndColumnResize(true);
	return false;
}

void DataGridColumnHeader::EndColumnResize(bool cancel)
{
	if (!_isResizing) return;
	_isResizing = false;
	_resizeFromLeftEdge = false;
	_resizingColumnIndex = DataGridCellInfo::InvalidIndex;
	_resizeStartRenderX = 0.0;
	_resizeStartWidth = 0.0;
	const ControlWeakReference ownerLifetime(_owner);
	if (_owner) _owner->EndColumnResizeTransaction(cancel);
	if (auto* owner = dynamic_cast<DataGrid*>(ownerLifetime.Get()))
		owner->ApplyPendingColumnWidths();
	if (auto* owner = dynamic_cast<DataGrid*>(ownerLifetime.Get()))
		owner->CommitColumnWidthLayoutToAncestors();
	if (IsMouseCaptured()) (void)ReleaseMouseCapture();
	// Reconciliation can replace this header, so it must be the final action.
	if (auto* owner = dynamic_cast<DataGrid*>(ownerLifetime.Get()))
	{
		owner->InvalidateRealizedColumnRange();
		owner->RefreshRealizedColumns();
	}
}

bool DataGridColumnHeader::OnClick()
{
	const ControlWeakReference lifetime(this);
	const bool sorted = _owner && _column
		? _owner->PerformSort(*_column, _multiColumnSortRequested)
		: false;
	if (lifetime.Get() != this) return sorted;
	_multiColumnSortRequested = false;
	const bool clicked = Button::OnClick();
	return sorted || clicked;
}

DataGridColumnHeadersPresenter::DataGridColumnHeadersPresenter() = default;

bool DataGridColumnHeadersPresenter::Initialize(
	DataGrid& owner, std::wstring* outError)
{
	if (outError) outError->clear();
	_owner = &owner;
	ClearVisualChildren();
	ClearColumns();
	const size_t columnCount = owner.ColumnCount();
	const size_t initialWidthProjectionRevision =
		owner._columnWidthProjectionRevision;
	const auto realizedColumns = owner.ResolveRealizedColumnRange();
	_columnStorageIsSparse = owner.GetEnableColumnVirtualization()
		&& columnCount > 0;
	_realizedColumnBegin = _columnStorageIsSparse
		? realizedColumns.first : 0;
	_realizedColumnEnd = _columnStorageIsSparse
		? realizedColumns.second : columnCount;
	_headers.assign(_columnStorageIsSparse
		? _realizedColumnEnd - _realizedColumnBegin : columnCount, nullptr);
	SetVisibility(HasColumnHeaders(owner.GetHeadersVisibility())
		? Visibility::Visible : Visibility::Collapsed);
	if (std::isfinite(owner.GetColumnHeaderHeight()))
		SetHeight(cui::layout::Length::Fixed(
			static_cast<float>(owner.GetColumnHeaderHeight())));
	else SetHeight(cui::layout::Length::Auto());
	try
	{
		if (_columnStorageIsSparse)
		{
			double prefix = 0.0;
			double ignored = 0.0;
			if (_realizedColumnBegin > 0
				&& !owner.TryResolveColumnBounds(
					_realizedColumnBegin, prefix, ignored)) return false;
			AddColumn(SparseTrackLength(prefix));
		}
		for (size_t index = _realizedColumnBegin;
			index < _realizedColumnEnd; ++index)
		{
			if (owner.ColumnCount() != columnCount) return false;
			auto* column = owner.GetColumnFromDisplayIndex(index);
			if (!column) continue;
			AddColumn(
				owner.ResolveColumnGridLength(index),
				static_cast<float>(column->GetMinWidth()),
				static_cast<float>((std::min)(
					column->GetMaxWidth(),
					static_cast<double>((std::numeric_limits<float>::max)()))));
			auto header = std::make_unique<DataGridColumnHeader>();
			header->Initialize(owner, *column, index);
			const size_t storageIndex = _columnStorageIsSparse
				? index - _realizedColumnBegin : index;
			Grid::SetColumn(*header, static_cast<int>(_columnStorageIsSparse
				? storageIndex + 1 : index));
			_headers[storageIndex] = AddOwned(std::move(header));
		}
		if (_columnStorageIsSparse)
		{
			if (owner.ColumnCount() != columnCount) return false;
			double realizedRight = 0.0;
			double total = 0.0;
			double ignored = 0.0;
			if (_realizedColumnEnd > 0
				&& !owner.TryResolveColumnBounds(
					_realizedColumnEnd - 1, ignored, realizedRight)) return false;
			if (columnCount > 0
				&& !owner.TryResolveColumnBounds(
					columnCount - 1, ignored, total)) return false;
			AddColumn(SparseTrackLength(
				(std::max)(0.0, total - realizedRight)));
		}
		if (owner._columnWidthProjectionRevision
			== initialWidthProjectionRevision)
			_appliedColumnWidthProjectionRevision =
				initialWidthProjectionRevision;
		return true;
	}
	catch (...)
	{
		if (outError) *outError = L"DataGrid 列标题生成失败。";
		return false;
	}
}

bool DataGridColumnHeadersPresenter::RefreshRealizedColumns(
	size_t begin, size_t end, std::wstring* outError)
{
	if (outError) outError->clear();
	if (!_owner || !_columnStorageIsSparse || begin > end
		|| end > _owner->ColumnCount()) return false;
	if (begin == _realizedColumnBegin && end == _realizedColumnEnd)
	{
		UpdateColumnWidths();
		return true;
	}

	const ControlWeakReference presenterLifetime(this);
	const ControlWeakReference ownerLifetime(_owner);
	const size_t columnCount = _owner->ColumnCount();
	const size_t oldBegin = _realizedColumnBegin;
	const size_t oldEnd = _realizedColumnEnd;
	const auto oldHeaders = _headers;
	std::vector<DataGridColumnHeader*> next(end - begin, nullptr);
	std::vector<std::unique_ptr<DataGridColumnHeader>> created(end - begin);
	try
	{
		const size_t overlapBegin = (std::max)(begin, oldBegin);
		const size_t overlapEnd = (std::min)(end, oldEnd);
		for (size_t index = overlapBegin; index < overlapEnd; ++index)
		{
			const size_t oldSlot = index - oldBegin;
			if (oldSlot >= oldHeaders.size()) return false;
			auto* header = oldHeaders[oldSlot];
			if (!header || !ContainsControl(header)
				|| header->_columnIndex != index
				|| header->_column != _owner->GetColumnFromDisplayIndex(index)) return false;
			next[index - begin] = header;
		}
		for (size_t index = begin; index < end; ++index)
		{
			const size_t slot = index - begin;
			if (next[slot]) continue;
			auto* owner = dynamic_cast<DataGrid*>(ownerLifetime.Get());
			auto* presenter = dynamic_cast<DataGridColumnHeadersPresenter*>(
				presenterLifetime.Get());
			if (!owner || !presenter || owner->ColumnCount() != columnCount)
				return false;
			auto* column = owner->GetColumnFromDisplayIndex(index);
			if (!column) return false;
			auto header = std::make_unique<DataGridColumnHeader>();
			header->Initialize(*owner, *column, index);
			Grid::SetColumn(*header, static_cast<int>(slot + 1));
			next[slot] = header.get();
			created[slot] = std::move(header);
		}
		for (size_t slot = 0; slot < created.size(); ++slot)
		{
			auto* presenter = dynamic_cast<DataGridColumnHeadersPresenter*>(
				presenterLifetime.Get());
			if (!presenter || !ownerLifetime.Get()) return false;
			if (created[slot])
				next[slot] = presenter->AddOwned(std::move(created[slot]));
		}
	}
	catch (...)
	{
		if (outError && outError->empty())
			*outError = L"DataGrid 横向虚拟列标题更新失败。";
		return false;
	}
	for (size_t slot = 0; slot < oldHeaders.size(); ++slot)
	{
		const size_t index = oldBegin + slot;
		if (index >= begin && index < end) continue;
		auto* presenter = dynamic_cast<DataGridColumnHeadersPresenter*>(
			presenterLifetime.Get());
		auto* header = oldHeaders[slot];
		if (!presenter || (header && presenter->ContainsControl(header)
			&& !presenter->DeleteVisualChild(header))) return false;
		if (!ownerLifetime.Get()) return false;
	}

	auto* owner = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	auto* presenter = dynamic_cast<DataGridColumnHeadersPresenter*>(
		presenterLifetime.Get());
	if (!owner || !presenter || owner->ColumnCount() != columnCount)
		return false;
	for (size_t slot = 0; slot < next.size(); ++slot)
	{
		auto* header = next[slot];
		if (!header || !presenter->ContainsControl(header)) return false;
		Grid::SetColumn(*header, static_cast<int>(slot + 1));
	}
	presenter->_headers = std::move(next);
	presenter->_realizedColumnBegin = begin;
	presenter->_realizedColumnEnd = end;
	presenter->_appliedColumnWidthProjectionRevision = 0;
	presenter->UpdateColumnWidths();
	return presenterLifetime.Get() != nullptr && ownerLifetime.Get() != nullptr;
}

void DataGridColumnHeadersPresenter::UpdateColumnWidths(
	bool propagateLayoutInvalidation)
{
	if (!_owner) return;
	const size_t projectionRevision =
		_owner->_columnWidthProjectionRevision;
	if (_appliedColumnWidthProjectionRevision == projectionRevision) return;
	const size_t columnCount = _owner->ColumnCount();
	const size_t begin = _columnStorageIsSparse
		? _realizedColumnBegin : 0;
	const size_t end = _columnStorageIsSparse
		? _realizedColumnEnd : columnCount;
	if (begin > end || end > columnCount) return;
	std::vector<ColumnDefinition> definitions;
	definitions.reserve((end - begin) + (_columnStorageIsSparse ? 2 : 0));
	if (_columnStorageIsSparse)
	{
		double prefix = 0.0;
		double ignored = 0.0;
		if (begin > 0 && !_owner->TryResolveColumnBounds(
			begin, prefix, ignored)) return;
		definitions.emplace_back(SparseTrackLength(prefix));
	}
	for (size_t index = begin; index < end; ++index)
	{
		if (!_owner || _owner->ColumnCount() != columnCount) return;
		auto* column = _owner->GetColumnFromDisplayIndex(index);
		if (!column) continue;
		definitions.emplace_back(
			_owner->ResolveColumnGridLength(index),
			static_cast<float>(column->GetMinWidth()),
			static_cast<float>((std::min)(
				column->GetMaxWidth(),
				static_cast<double>((std::numeric_limits<float>::max)()))));
	}
	if (_columnStorageIsSparse)
	{
		if (!_owner || _owner->ColumnCount() != columnCount) return;
		double realizedRight = 0.0;
		double total = 0.0;
		double ignored = 0.0;
		if (end > 0 && !_owner->TryResolveColumnBounds(
			end - 1, ignored, realizedRight)) return;
		if (columnCount > 0 && !_owner->TryResolveColumnBounds(
			columnCount - 1, ignored, total)) return;
		definitions.emplace_back(SparseTrackLength(
			(std::max)(0.0, total - realizedRight)));
	}
	if (!_owner || _owner->ColumnCount() != columnCount
		|| _owner->_columnWidthProjectionRevision != projectionRevision) return;
	const ControlWeakReference presenterLifetime(this);
	const ControlWeakReference ownerLifetime(_owner);
	ReplaceColumns(std::move(definitions), propagateLayoutInvalidation);
	auto* presenter = dynamic_cast<DataGridColumnHeadersPresenter*>(
		presenterLifetime.Get());
	auto* owner = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (presenter && owner && presenter->_owner == owner
		&& owner->_columnWidthProjectionRevision == projectionRevision)
		presenter->_appliedColumnWidthProjectionRevision = projectionRevision;
}

bool DataGridColumnHeadersPresenter::TryCommitResizeLayoutLocally(
	bool heightIsFixed)
{
	if (!_owner || !GetComputedLayout().hasArranged) return false;
	if (heightIsFixed)
	{
		UpdateLayout();
		return true;
	}
	const float previousDesiredHeight = GetDesiredSizeDip().height;
	const auto size = GetActualSizeDip();
	const auto desired = Measure(cui::core::Constraints{
		cui::core::Size{ size.width, cui::core::Infinity } });
	if (std::abs(desired.height - previousDesiredHeight) > 0.0001f)
		return false;
	const auto location = GetActualLocationDip();
	Arrange(cui::core::Rect{
		location.x, location.y, size.width, size.height });
	return true;
}

cui::core::Size DataGridColumnHeadersPresenter::MeasureCore(
	const cui::core::Constraints& available)
{
	bool hasPublishedScrollViewport = false;
	if (_owner)
		if (auto* scroll = dynamic_cast<ScrollViewer*>(
			_owner->_scrollViewer.Get()))
			hasPublishedScrollViewport = std::isfinite(
				scroll->GetViewportWidth()) && scroll->GetViewportWidth() > 0.0;
	// The bounded header viewport is useful as a first-layout/detached fallback.
	// Once the body ScrollViewer has published its viewport it is authoritative:
	// it excludes the vertical scrollbar, whereas the header host does not.
	// Alternating between those two widths would request layout forever.
	if (_owner && !hasPublishedScrollViewport
		&& std::isfinite(available.maximum.width))
		_owner->UpdateColumnViewportWidth(available.maximum.width);
	return Grid::MeasureCore(available);
}

const DependencyProperty& DataGrid::AutoGenerateColumnsProperty()
{
	static const auto registration = []
	{
		auto options = DataGridOptions(true);
		options.Changed = [](DataGrid& target, const bool&, const bool& value)
		{
			const ControlWeakReference lifetime(&target);
			if (value) target.EnsureAutoGeneratedColumns();
			else target.RemoveAutoGeneratedColumns();
			if (auto* live = dynamic_cast<DataGrid*>(lifetime.Get()))
			{
				live->RefreshColumns();
				live = dynamic_cast<DataGrid*>(lifetime.Get());
				if (live) live->FlushAutoGeneratedColumnsEvent();
			}
		};
		return DependencyPropertyRegistry::RegisterStatic<DataGrid, bool>(
			DependencyPropertyRegistrationLiteral(L"AutoGenerateColumns"),
			[](DataGrid& target) { return target._autoGenerateColumns; },
			[](DataGrid& target, const bool& value)
			{ target._autoGenerateColumns = value; }, {}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& DataGrid::IsReadOnlyProperty()
{
	static const auto registration = []
	{
		auto options = DataGridOptions(false);
		options.Changed = [](DataGrid& target, const bool&, const bool& value)
		{
			const ControlWeakReference lifetime(&target);
			if (value) (void)target.CancelEdit();
			if (auto* live = dynamic_cast<DataGrid*>(lifetime.Get()))
				live->InvalidateRows();
		};
		return DependencyPropertyRegistry::RegisterStatic<DataGrid, bool>(
			DependencyPropertyRegistrationLiteral(L"IsReadOnly"),
			[](DataGrid& target) { return target._isReadOnly; },
			[](DataGrid& target, const bool& value)
			{ target._isReadOnly = value; }, {}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& DataGrid::CanUserSortColumnsProperty()
{
	static const auto registration = []
	{
		auto options = DataGridOptions(true);
		options.Changed = [](DataGrid& target, const bool&, const bool&)
		{ target.RefreshHeaderPresenter(); };
		return DependencyPropertyRegistry::RegisterStatic<DataGrid, bool>(
			DependencyPropertyRegistrationLiteral(L"CanUserSortColumns"),
			[](DataGrid& target) { return target._canUserSortColumns; },
			[](DataGrid& target, const bool& value)
			{ target._canUserSortColumns = value; }, {}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& DataGrid::CanUserResizeColumnsProperty()
{
	static const auto registration = []
	{
		return DependencyPropertyRegistry::RegisterStatic<DataGrid, bool>(
			DependencyPropertyRegistrationLiteral(L"CanUserResizeColumns"),
			[](DataGrid& target) { return target._canUserResizeColumns; },
			[](DataGrid& target, const bool& value)
			{
				if (target._canUserResizeColumns == value) return;
				if (!target._columnResizeSnapshot.empty())
					target.EndColumnResizeTransaction(true);
				target._canUserResizeColumns = value;
			}, {}, DataGridOptions(true));
	}();
	return *registration;
}

const DependencyProperty& DataGrid::EnableColumnVirtualizationProperty()
{
	static const auto registration = []
	{
		auto options = DataGridOptions(false);
		options.Changed = [](DataGrid& target, const bool&, const bool&)
		{
			target.InvalidateRealizedColumnRange();
			target.RefreshColumns();
		};
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Behavior";
		options.Design.CategoryOrder = 110;
		options.Design.Order = 20;
		options.Design.Editor = DependencyPropertyEditorKind::Boolean;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		)
		return DependencyPropertyRegistry::RegisterStatic<DataGrid, bool>(
			DependencyPropertyRegistrationLiteral(L"EnableColumnVirtualization"),
			[](DataGrid& target) { return target._enableColumnVirtualization; },
			[](DataGrid& target, const bool& value)
			{ target._enableColumnVirtualization = value; }, {},
			std::move(options));
	}();
	return *registration;
}

const DependencyProperty& DataGrid::SelectionUnitProperty()
{
	static const auto registration = []
	{
		auto options = DataGridOptions(
			static_cast<int>(DataGridSelectionUnit::FullRow));
		options.Validate = [](const int& value)
		{
			return value == static_cast<int>(DataGridSelectionUnit::Cell)
				|| value == static_cast<int>(DataGridSelectionUnit::FullRow)
				|| value == static_cast<int>(
					DataGridSelectionUnit::CellOrRowHeader);
		};
		options.Changed = [](DataGrid& target,
			const int& oldValue, const int& newValue)
		{
			target.OnSelectionUnitChanged(
				static_cast<DataGridSelectionUnit>(oldValue),
				static_cast<DataGridSelectionUnit>(newValue));
		};
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Behavior";
		options.Design.CategoryOrder = 110;
		options.Design.Order = 30;
		options.Design.Editor = DependencyPropertyEditorKind::Choice;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		options.Design.Choices = {
			{ L"Cell", BindingValue(
				static_cast<int>(DataGridSelectionUnit::Cell)) },
			{ L"FullRow", BindingValue(
				static_cast<int>(DataGridSelectionUnit::FullRow)) },
			{ L"CellOrRowHeader", BindingValue(static_cast<int>(
				DataGridSelectionUnit::CellOrRowHeader)) }
		};
		)
		return DependencyPropertyRegistry::RegisterStatic<DataGrid, int>(
			DependencyPropertyRegistrationLiteral(L"SelectionUnit"),
			[](DataGrid& target)
			{ return static_cast<int>(target._selectionUnit); },
			[](DataGrid& target, const int& value)
			{ target._selectionUnit = static_cast<DataGridSelectionUnit>(value); },
			{}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& DataGrid::ColumnHeaderHeightProperty()
{
	static const auto registration = []
	{
		auto options = DataGridOptions(
			(std::numeric_limits<double>::quiet_NaN)(),
			DependencyPropertyFlags::AffectsMeasure);
		options.Validate = [](const double& value)
		{ return std::isnan(value) || (std::isfinite(value) && value >= 0.0); };
		options.Changed = [](DataGrid& target, const double&, const double&)
		{ target.RefreshHeaderPresenter(); };
		return DependencyPropertyRegistry::RegisterStatic<DataGrid, double>(
			DependencyPropertyRegistrationLiteral(L"ColumnHeaderHeight"),
			[](DataGrid& target) { return target._columnHeaderHeight; },
			[](DataGrid& target, const double& value)
			{ target._columnHeaderHeight = value; }, {}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& DataGrid::RowHeaderWidthProperty()
{
	static const auto registration = []
	{
		auto options = DataGridOptions(
			(std::numeric_limits<double>::quiet_NaN)(),
			DependencyPropertyFlags::AffectsMeasure);
		options.Validate = [](const double& value)
		{ return std::isnan(value) || (std::isfinite(value) && value >= 0.0); };
		options.Changed = [](DataGrid& target, const double&, const double& value)
		{
			target.InvalidateRowHeaderWidthBaseline();
			const double actual = std::isnan(value) ? 0.0 : value;
			const ControlWeakReference lifetime(&target);
			(void)target.SetCurrentRowHeaderActualWidth(actual);
			auto* live = dynamic_cast<DataGrid*>(lifetime.Get());
			if (live) live->RefreshHeadersVisibility();
			live = dynamic_cast<DataGrid*>(lifetime.Get());
			if (live && std::isnan(value))
				(void)live->RefreshRowHeaderActualWidth();
		};
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Appearance";
		options.Design.CategoryOrder = 20;
		options.Design.Order = 25;
		options.Design.Editor = DependencyPropertyEditorKind::Number;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		)
		return DependencyPropertyRegistry::RegisterStatic<DataGrid, double>(
			DependencyPropertyRegistrationLiteral(L"RowHeaderWidth"),
			[](DataGrid& target) { return target._rowHeaderWidth; },
			[](DataGrid& target, const double& value)
			{ target._rowHeaderWidth = value; }, {}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& DataGrid::RowHeaderActualWidthProperty()
{
	return RowHeaderActualWidthPropertyKey().Property();
}

const DependencyPropertyKey& DataGrid::RowHeaderActualWidthPropertyKey()
{
	static const auto registration = []
	{
		auto options = DataGridOptions(
			0.0, DependencyPropertyFlags::AffectsMeasure);
		options.Validate = [](const double& value)
		{ return std::isfinite(value) && value >= 0.0; };
		options.Changed = [](DataGrid& target, const double&, const double&)
		{ (void)target.RefreshRowHeaderActualWidthProjection(); };
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"State";
		options.Design.CategoryOrder = 70;
		options.Design.Order = 20;
		options.Design.Editor = DependencyPropertyEditorKind::Number;
		options.Design.Persistence = DependencyPropertyPersistence::Transient;
		options.Design.Browsable = false;
		)
		return DependencyPropertyRegistry::RegisterReadOnlyStatic<
			DataGrid, double>(
				DependencyPropertyRegistrationLiteral(L"RowHeaderActualWidth"),
				[](DataGrid& target)
				{ return target._rowHeaderActualWidth; },
				[](DataGrid& target, const double& value)
				{
					(void)target.SetReadOnlyPropertyField(
						RowHeaderActualWidthPropertyKey(),
						target._rowHeaderActualWidth, value);
				}, {}, std::move(options));
	}();
	return registration.Key();
}

const DependencyProperty& DataGrid::RowHeightProperty()
{
	static const auto registration = []
	{
		auto options = DataGridOptions(
			(std::numeric_limits<double>::quiet_NaN)(),
			DependencyPropertyFlags::AffectsMeasure);
		options.Validate = [](const double& value)
		{ return std::isnan(value) || (std::isfinite(value) && value > 0.0); };
		options.Changed = [](DataGrid& target, const double&, const double&)
		{ target.InvalidateRows(); };
		return DependencyPropertyRegistry::RegisterStatic<DataGrid, double>(
			DependencyPropertyRegistrationLiteral(L"RowHeight"),
			[](DataGrid& target) { return target._rowHeight; },
			[](DataGrid& target, const double& value)
			{ target._rowHeight = value; }, {}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& DataGrid::HeadersVisibilityProperty()
{
	static const auto registration = []
	{
		auto options = DataGridOptions(
			static_cast<int>(DataGridHeadersVisibility::All),
			DependencyPropertyFlags::AffectsMeasure);
		options.Validate = [](const int& value)
		{ return value >= 0 && value <= 3; };
		options.Changed = [](DataGrid& target, const int&, const int& value)
		{
			const ControlWeakReference lifetime(&target);
			target.RefreshHeadersVisibility();
			auto* live = dynamic_cast<DataGrid*>(lifetime.Get());
			if (live && HasRowHeaders(
				static_cast<DataGridHeadersVisibility>(value)))
				(void)live->RefreshRowHeaderActualWidth();
			live = dynamic_cast<DataGrid*>(lifetime.Get());
			if (live) live->NotifyAccessibilityStructureChanged();
		};
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Appearance";
		options.Design.CategoryOrder = 20;
		options.Design.Order = 30;
		options.Design.Editor = DependencyPropertyEditorKind::Choice;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		options.Design.Choices = {
			{ L"None", BindingValue(
				static_cast<int>(DataGridHeadersVisibility::None)) },
			{ L"Column", BindingValue(
				static_cast<int>(DataGridHeadersVisibility::Column)) },
			{ L"Row", BindingValue(
				static_cast<int>(DataGridHeadersVisibility::Row)) },
			{ L"All", BindingValue(
				static_cast<int>(DataGridHeadersVisibility::All)) }
		};
		)
		return DependencyPropertyRegistry::RegisterStatic<DataGrid, int>(
			DependencyPropertyRegistrationLiteral(L"HeadersVisibility"),
			[](DataGrid& target)
			{ return static_cast<int>(target._headersVisibility); },
			[](DataGrid& target, const int& value)
			{ target._headersVisibility = static_cast<DataGridHeadersVisibility>(value); },
			{}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& DataGrid::GridLinesVisibilityProperty()
{
	static const auto registration = []
	{
		auto options = DataGridOptions(
			static_cast<int>(DataGridGridLinesVisibility::All),
			DependencyPropertyFlags::AffectsRender);
		options.Validate = [](const int& value)
		{ return value >= 0 && value <= 3; };
		options.Changed = [](DataGrid& target, const int&, const int&)
		{ target.InvalidateRows(); };
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Appearance";
		options.Design.CategoryOrder = 20;
		options.Design.Order = 40;
		options.Design.Editor = DependencyPropertyEditorKind::Choice;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		options.Design.Choices = {
			{ L"All", BindingValue(
				static_cast<int>(DataGridGridLinesVisibility::All)) },
			{ L"Horizontal", BindingValue(
				static_cast<int>(DataGridGridLinesVisibility::Horizontal)) },
			{ L"None", BindingValue(
				static_cast<int>(DataGridGridLinesVisibility::None)) },
			{ L"Vertical", BindingValue(
				static_cast<int>(DataGridGridLinesVisibility::Vertical)) }
		};
		)
		return DependencyPropertyRegistry::RegisterStatic<DataGrid, int>(
			DependencyPropertyRegistrationLiteral(L"GridLinesVisibility"),
			[](DataGrid& target)
			{ return static_cast<int>(target._gridLinesVisibility); },
			[](DataGrid& target, const int& value)
			{ target._gridLinesVisibility = static_cast<DataGridGridLinesVisibility>(value); },
			{}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& DataGrid::RowBackgroundProperty()
{
	static const auto registration = []
	{
		auto options = DataGridOptions(
			cui::drawing::NoBrush(), DependencyPropertyFlags::AffectsRender);
		options.Convert = ConvertBrush;
		options.Equals = [](const cui::drawing::Brush& left,
			const cui::drawing::Brush& right) { return left == right; };
		options.Changed = [](DataGrid& target,
			const cui::drawing::Brush&, const cui::drawing::Brush&)
		{ target.InvalidateRows(); };
		return DependencyPropertyRegistry::RegisterStatic<
			DataGrid, cui::drawing::Brush>(
				DependencyPropertyRegistrationLiteral(L"RowBackground"),
				[](DataGrid& target) { return target._rowBackground; },
				[](DataGrid& target, const cui::drawing::Brush& value)
				{ target._rowBackground = value; }, {}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& DataGrid::AlternatingRowBackgroundProperty()
{
	static const auto registration = []
	{
		auto options = DataGridOptions(
			cui::drawing::NoBrush(), DependencyPropertyFlags::AffectsRender);
		options.Convert = ConvertBrush;
		options.Equals = [](const cui::drawing::Brush& left,
			const cui::drawing::Brush& right) { return left == right; };
		options.Changed = [](DataGrid& target,
			const cui::drawing::Brush&, const cui::drawing::Brush&)
		{ target.InvalidateRows(); };
		return DependencyPropertyRegistry::RegisterStatic<
			DataGrid, cui::drawing::Brush>(
				DependencyPropertyRegistrationLiteral(L"AlternatingRowBackground"),
				[](DataGrid& target) { return target._alternatingRowBackground; },
				[](DataGrid& target, const cui::drawing::Brush& value)
				{ target._alternatingRowBackground = value; }, {}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& DataGrid::HorizontalGridLinesBrushProperty()
{
	static const auto registration = []
	{
		auto options = DataGridOptions(
			cui::drawing::MakeSolidColorBrush(
				D2D1_COLOR_F{ 0.85f, 0.85f, 0.85f, 1.0f }),
			DependencyPropertyFlags::AffectsRender);
		options.Convert = ConvertBrush;
		options.Equals = [](const cui::drawing::Brush& left,
			const cui::drawing::Brush& right) { return left == right; };
		options.Changed = [](DataGrid& target,
			const cui::drawing::Brush&, const cui::drawing::Brush&)
		{ target.InvalidateRows(); };
		return DependencyPropertyRegistry::RegisterStatic<
			DataGrid, cui::drawing::Brush>(
				DependencyPropertyRegistrationLiteral(L"HorizontalGridLinesBrush"),
				[](DataGrid& target) { return target._horizontalGridLinesBrush; },
				[](DataGrid& target, const cui::drawing::Brush& value)
				{ target._horizontalGridLinesBrush = value; }, {}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& DataGrid::VerticalGridLinesBrushProperty()
{
	static const auto registration = []
	{
		auto options = DataGridOptions(
			cui::drawing::MakeSolidColorBrush(
				D2D1_COLOR_F{ 0.85f, 0.85f, 0.85f, 1.0f }),
			DependencyPropertyFlags::AffectsRender);
		options.Convert = ConvertBrush;
		options.Equals = [](const cui::drawing::Brush& left,
			const cui::drawing::Brush& right) { return left == right; };
		options.Changed = [](DataGrid& target,
			const cui::drawing::Brush&, const cui::drawing::Brush&)
		{ target.InvalidateRows(); };
		return DependencyPropertyRegistry::RegisterStatic<
			DataGrid, cui::drawing::Brush>(
				DependencyPropertyRegistrationLiteral(L"VerticalGridLinesBrush"),
				[](DataGrid& target) { return target._verticalGridLinesBrush; },
				[](DataGrid& target, const cui::drawing::Brush& value)
				{ target._verticalGridLinesBrush = value; }, {}, std::move(options));
	}();
	return *registration;
}

void DataGrid::RegisterDependencyProperties()
{
	ListBox::RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	(void)AutoGenerateColumnsProperty();
	(void)IsReadOnlyProperty();
	(void)CanUserSortColumnsProperty();
	(void)CanUserResizeColumnsProperty();
	(void)EnableColumnVirtualizationProperty();
	(void)SelectionUnitProperty();
	(void)ColumnHeaderHeightProperty();
	(void)RowHeaderWidthProperty();
	(void)RowHeaderActualWidthProperty();
	(void)RowHeightProperty();
	(void)HeadersVisibilityProperty();
	(void)GridLinesVisibilityProperty();
	(void)RowBackgroundProperty();
	(void)AlternatingRowBackgroundProperty();
	(void)HorizontalGridLinesBrushProperty();
	(void)VerticalGridLinesBrushProperty();
#endif
}

DataGrid::DataGrid()
	: _itemsView(std::make_shared<CollectionViewSource>()),
	  _selectedCells(this),
	  _horizontalGridLinesBrush(cui::drawing::MakeSolidColorBrush(
		  D2D1_COLOR_F{ 0.85f, 0.85f, 0.85f, 1.0f })),
	  _verticalGridLinesBrush(cui::drawing::MakeSolidColorBrush(
		  D2D1_COLOR_F{ 0.85f, 0.85f, 0.85f, 1.0f }))
{
	RegisterDependencyProperties();
	// WPF's DataGrid view publishes one terminal Refresh/Reset for sorting.
	// Keeping the generic CollectionViewSource precise-Move default lets other
	// controls opt into container-preserving changes without making a header
	// click execute one complete DataGrid transaction per moved row.
	_itemsView->SetUseResetNotificationForComplexRefresh(true);
	// WPF enables DataGrid row virtualization by default.  Install the native
	// default before generated XAML assigns ItemsSource: implicit theme styles
	// are applied only after InitializeComponent has built the complete tree.
	(void)TrySetPropertyValue(
		ItemsControl::ItemsPanelProperty(),
		BindingValue(DefaultDataGridRowsPanel()),
		DependencyPropertyValueSource::Theme);
	SetSelectionMode(SelectionMode::Extended);
	ListBox::SetItemsSource(BindingListReference(_itemsView));
	RetainEventConnection(OnKeyDown.Subscribe(
		[this](Control*, KeyEventArgs& args)
		{
			// Direct DataGrid input already ran HandleCellKey in ProcessInput.
			// This routed hook exists for an editing descendant such as TextBox.
			if (args.OriginalSource == this) return;
			if (auto* cell = ResolveCurrentCellContainer();
				cell && cell->GetIsEditing())
			{
				auto* editor = cell->GetEditingElement();
				if (dynamic_cast<TextBox*>(editor)
					&& EditingElementOwnsKey(args.Key, args.Modifiers)
					&& IsRoutedDescendantOrSelf(
						args.OriginalSource, editor)) return;
			}
			if (!args.Handled && HandleCellKey(args.Key, args.Modifiers))
				args.Handled = true;
		}));
	RetainEventConnection(OnPropertyValueChanged.Subscribe(
		[this](DependencyObject*, const DependencyPropertyChangedEventArgs& args)
		{
			if (args.Property != &ListBox::SelectionModeProperty()) return;
			const ControlWeakReference lifetime(this);
			RefreshHeadersVisibility();
			if (!lifetime.Get()
				|| GetSelectionMode() != SelectionMode::Single) return;
			if (_selectionUnit == DataGridSelectionUnit::FullRow)
			{
				SynchronizeSelectedCellsFromRows();
				return;
			}
			if (!GetSelectedIndices().empty())
			{
				const size_t row = static_cast<size_t>(
					GetSelectedIndices().back());
				std::vector<DataGridCellInfo> retainedRow;
				retainedRow.reserve(_columns.size());
				for (size_t column = 0; column < _columns.size(); ++column)
				{
					DataGridCellInfo info;
					if (TryCreateCellInfo(row, column, info))
						retainedRow.push_back(std::move(info));
				}
				_selectionAnchor = retainedRow.empty()
					? std::optional<DataGridCellInfo>{}
					: std::optional<DataGridCellInfo>{ retainedRow.front() };
				ResetSelectionRange();
				ApplySelectedCells(std::move(retainedRow));
				return;
			}
			if (_selectedCells.size() <= 1) return;
			DataGridCellInfo retained = _selectedCells.front();
			_selectionAnchor = _selectedCells.empty()
				? std::optional<DataGridCellInfo>{}
				: std::optional<DataGridCellInfo>{ retained };
			ResetSelectionRange();
			(void)ApplySelectedCells({ std::move(retained) });
		}));
}

std::unique_ptr<AutomationPeer> DataGrid::OnCreateAutomationPeer()
{
	return std::make_unique<DataGridAutomationPeer>(*this);
}

void DataGrid::PrepareMeasureCore(
	const cui::core::Constraints& available)
{
	ListBox::PrepareMeasureCore(available);
	// A native resize burst may publish dozens of Width values before the next
	// layout pass. Project only the newest value into the realized header/rows,
	// before their template subtree measures against it.
	ApplyPendingColumnWidths();
}

void DataGrid::PreparePresentation()
{
	const ControlWeakReference ownerLifetime(this);
	// Native column dragging schedules a DataGrid-local damage frame instead of
	// invalidating the Window measure root. Project the newest coalesced widths
	// immediately before the retained scene prepares this node, then let the
	// ordinary ItemsControl presentation hook commit only the invalid local paths.
	if (_columnWidthRefreshPending && !_columnResizeSnapshot.empty())
	{
		// Arrange invalidates every changed cell's old and new bounds. The damage
		// that brought us into this frame already covers that complete suffix, so
		// retain the geometry revisions but discard the duplicate future damage.
		// This prevents a drag from perpetually carrying one redundant paint turn.
		ScopedVisualInvalidation localFrame(*this, false);
		ApplyPendingColumnWidths(false);
		ListBox::PreparePresentation();
		if (auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get()))
		{
			live->_columnWidthDirtyBegin = DataGridCellInfo::InvalidIndex;
			live->_columnWidthDirtyEnd = DataGridCellInfo::InvalidIndex;
			live->_columnWidthDirtyVisualSpan =
				(std::numeric_limits<double>::quiet_NaN)();
			live->_columnWidthMeasureDirty.clear();
		}
		return;
	}
	if (_columnWidthRefreshPending) ApplyPendingColumnWidths();
	ListBox::PreparePresentation();
	if (auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get()))
	{
		live->_columnWidthDirtyBegin = DataGridCellInfo::InvalidIndex;
		live->_columnWidthDirtyEnd = DataGridCellInfo::InvalidIndex;
		live->_columnWidthDirtyVisualSpan =
			(std::numeric_limits<double>::quiet_NaN)();
		live->_columnWidthMeasureDirty.clear();
	}
}

void DataGrid::Arrange(cui::core::Rect finalRect)
{
	// Column headers and rows live in separate Grids so the header can remain
	// vertically frozen. Resolve every DataGridLength, including Star, against
	// one shared data viewport before either Grid is arranged; otherwise an
	// unbounded ScrollViewer row measures Star as content while the bounded
	// header distributes the same Stars across its viewport.
	double columnViewport = ResolveColumnViewportWidth(finalRect.width);
	// ScrollViewer publishes its exact viewport after arranging the body. During
	// continuous window resizing that value therefore describes the previous
	// DataGrid slot. Carry the owner's width delta forward so Star columns are
	// projected before the header/body walk, avoiding an old-width arrange plus a
	// corrective second pass on every WM_SIZE. A scrollbar state transition is
	// corrected locally by the subsequent ScrollChanged notification.
	if (auto* scroll = dynamic_cast<ScrollViewer*>(_scrollViewer.Get()))
	{
		const double published = scroll->GetViewportWidth();
		const auto previousSize = GetActualSizeDip();
		if (std::isfinite(published) && published > 0.0
			&& previousSize.width > 0.0f
			&& std::isfinite(finalRect.width))
			columnViewport = (std::max)(0.0,
				columnViewport + static_cast<double>(finalRect.width)
				- static_cast<double>(previousSize.width));
	}
	UpdateColumnViewportWidth(columnViewport, false);
	RefreshRealizedColumns();
	ListBox::Arrange(finalRect);
}

void DataGrid::SetItemsSource(BindingListReference value)
{
	const ControlWeakReference ownerLifetime(this);
	if (_source == value && _itemsView
		&& ListBox::GetItemsSource().Shared() == _itemsView) return;
	if (_settingItemsSource || IsItemsSourceUpdateInProgress())
		throw std::logic_error(
			"DataGrid does not support reentrant ItemsSource changes");
	// WPF does not carry a view's SortDescriptions across an ItemsSource
	// identity boundary. Doing so would also force an otherwise lazy million-row
	// source to materialize every item merely because the previous source was
	// sorted. Keep same-source recovery semantics, and clear column indicators
	// only after the replacement transaction commits.
	const bool clearSortState = _source
		&& _source.Shared() != value.Shared();
	auto previousAccessibilityState = CaptureAccessibilityIdentityState();
	_settingItemsSource = true;
	InvalidateItemOccurrenceCache();
	auto resetSettingItemsSource = MakeScopeExit([ownerLifetime]
	{
		if (auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get()))
		{
			live->_settingItemsSource = false;
			// Candidate construction itself precedes the main transaction try and
			// may throw after an old-view callback committed a real structure
			// change.  Flush that restored/old pending event here without ever
			// replacing the exception already leaving SetItemsSource.
			try { live->FlushAccessibilityStructureChange(); }
			catch (...) {}
		}
	});

	// Populate the ICollectionView before ItemsControl subscribes to it.  A
	// first projection publishes precise Add notifications; attaching an empty
	// view first would turn N source records into N snapshot/generator
	// transactions even though no prior containers can be preserved.
	auto candidate = std::make_shared<CollectionViewSource>();
	candidate->SetUseResetNotificationForComplexRefresh(true);
	if (!clearSortState && _itemsView)
		candidate->SetSortDescriptions(_itemsView->SortDescriptions());
	candidate->SetSource(value);
	EventConnection candidateRowHeaderSourceChanged;
	if (value)
	{
		auto* observedSource = value.Get();
		candidateRowHeaderSourceChanged = observedSource->SubscribeChanged(
			[ownerLifetime, observedSource](const CollectionChangedEventArgs& change)
			{
				if (change.Action != CollectionChangeAction::Reset) return;
				auto* owner = dynamic_cast<DataGrid*>(ownerLifetime.Get());
				if (!owner || owner->_source.Get() != observedSource
					|| owner->_settingItemsSource
					|| owner->IsItemsSourceUpdateInProgress()
					|| !std::isnan(owner->_rowHeaderWidth)) return;
				owner->InvalidateRowHeaderWidthBaseline();
				(void)owner->SetCurrentRowHeaderActualWidth(0.0);
				owner = dynamic_cast<DataGrid*>(ownerLifetime.Get());
				if (owner) (void)owner->RefreshRowHeaderActualWidth();
			});
	}
	auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live) return;

	const auto previousSource = live->_source;
	const auto previousView = live->_itemsView;
	const auto previousCurrentCell = live->_currentCell;
	const bool previousRuntimeAutoGenerationComplete =
		live->_runtimeAutoGenerationComplete;
	const bool previousAutoGeneratedColumnsEventPending =
		live->_autoGeneratedColumnsEventPending;
	size_t previousCurrentItemOrdinal = 0;
	bool hasPreviousCurrentItemOrdinal = false;
	if (previousView && previousCurrentCell.IsValid())
	{
		size_t ordinal = 0;
		for (size_t index = 0;; ++index)
		{
			const size_t previousCount = previousView->Count();
			live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
			if (!live) return;
			if (index >= previousCount) break;
			BindingSourceReference item;
			if (!previousView->TryGetItem(index, item))
			{
				live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
				if (!live) return;
				continue;
			}
			live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
			if (!live) return;
			if (!item || item.Shared() != previousCurrentCell.Item.Shared())
				continue;
			if (index == previousCurrentCell.RowIndex)
			{
				previousCurrentItemOrdinal = ordinal;
				hasPreviousCurrentItemOrdinal = true;
				break;
			}
			++ordinal;
		}
	}
	struct RuntimeAutoColumnSnapshot final
	{
		size_t Index = 0;
		std::unique_ptr<DataGridColumn> Column;
	};
	std::vector<RuntimeAutoColumnSnapshot> previousRuntimeAutoColumns;
#if CUI_ENABLE_DYNAMIC_XAML
	const auto previousLogicalColumns = live->_logicalColumns;
	const size_t runtimeAutoColumnCount = static_cast<size_t>(std::count_if(
		live->_columns.begin(), live->_columns.end(), [](const auto& column)
		{ return column->_isRuntimeAutoGenerated; }));
	if (runtimeAutoColumnCount != 0)
	{
		// Allocate both transaction buffers before moving a single definition.
		// AOT-generated columns remain authoritative and are never regenerated
		// from runtime reflection.
		previousRuntimeAutoColumns.reserve(runtimeAutoColumnCount);
		std::vector<std::unique_ptr<DataGridColumn>> retainedColumns;
		retainedColumns.reserve(live->_columns.size());
		for (size_t index = 0; index < live->_columns.size(); ++index)
		{
			if (live->_columns[index]->_isRuntimeAutoGenerated)
				previousRuntimeAutoColumns.push_back(
					{ index, std::move(live->_columns[index]) });
			else retainedColumns.push_back(std::move(live->_columns[index]));
		}
		live->_columns = std::move(retainedColumns);
		live->_logicalColumns.erase(std::remove_if(
			live->_logicalColumns.begin(), live->_logicalColumns.end(),
			[](const DataGridColumn* column)
			{ return column->_isRuntimeAutoGenerated; }),
			live->_logicalColumns.end());
		live->ReindexDisplayColumns();
	}
	live->_runtimeAutoGenerationComplete = false;
	live->_autoGeneratedColumnsEventPending = false;
#endif
	auto previousWidths = std::move(live->_resolvedColumnWidths);
	auto previousColumnWidthPrefix = std::move(live->_columnWidthPrefix);
	auto previousSampledContentWidths =
		std::move(live->_sampledColumnContentWidths);
	const size_t previousColumnContentWidthCacheEpoch =
		live->_columnContentWidthCacheEpoch;
	auto rollbackDerivedState = [&](DataGrid& target)
	{
		// A UIA query may have materialized identities for the candidate view
		// from a reentrant ItemsSource callback.  Mark that projection stale
		// before any candidate column is destroyed.
		target.InvalidateAccessibilityVirtualIdentities();
		target._source = previousSource;
		target._runtimeAutoGenerationComplete =
			previousRuntimeAutoGenerationComplete;
		target._autoGeneratedColumnsEventPending =
			previousAutoGeneratedColumnsEventPending;
		if (++target._cellSelectionRevision == 0)
			target._cellSelectionRevision = 1;
		target._itemsView = previousView;
		target._resolvedColumnWidths = std::move(previousWidths);
		target._columnWidthPrefix = std::move(previousColumnWidthPrefix);
		target._sampledColumnContentWidths =
			std::move(previousSampledContentWidths);
		target._columnContentWidthCacheEpoch =
			previousColumnContentWidthCacheEpoch;
		target._currentCell = previousCurrentCell;
		// Runtime auto-generation appends logical identities, but a customization
		// may request a display position. Remove the partial candidate from either
		// visual position, then restore the exact old logical/display projections.
		for (size_t index = target._columns.size(); index > 0;)
		{
			--index;
			if (!target._columns[index]->_isRuntimeAutoGenerated) continue;
			target._columns[index]->_owner = nullptr;
			target._columns[index]->_displayIndex =
				DataGridColumn::UnsetDisplayIndex;
			target._columns.erase(target._columns.begin() + index);
		}
		for (auto& snapshot : previousRuntimeAutoColumns)
		{
			const size_t index = (std::min)(
				snapshot.Index, target._columns.size());
			target._columns.insert(
				target._columns.begin() + index, std::move(snapshot.Column));
		}
#if CUI_ENABLE_DYNAMIC_XAML
		target._logicalColumns = previousLogicalColumns;
#endif
		target.ReindexDisplayColumns();
		target.PruneAccessibilityColumnIdentities();
		target.RestoreAccessibilityIdentityState(
			std::move(previousAccessibilityState));
	};
	try
	{
		// Candidate preparation above can re-enter and commit a real mutation to
		// the still-live old projection.  Preserve that pending notification in
		// the rollback seed before the identity boundary moves to the candidate;
		// notifications raised after this point belong to the tentative view and
		// must disappear if it fails.
		previousAccessibilityState.StructureChangePending =
			previousAccessibilityState.StructureChangePending
			|| live->_accessibilityStructureChangePending;
		live->_source = value;
		if (++live->_cellSelectionRevision == 0)
			live->_cellSelectionRevision = 1;
		live->_itemsView = candidate;
		// Candidate population is extensible and may have rebuilt the old view's
		// UIA table after the transaction's initial invalidation.  Switching the
		// live view is therefore an independent identity boundary: invalidate at
		// the assignment point before reflection, snapshotting, or UIA can observe
		// candidate rows through locators belonging to the old projection.
		live->InvalidateItemOccurrenceCache();
		live->InvalidateColumnContentWidthCache();
		// Auto-generated definitions must exist before a visible viewport asks
		// the base generator to compose its first row.  Keep this inside the
		// transaction because reflection or allocation may throw midway.
		live->EnsureAutoGeneratedColumns();
		live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
		if (!live) return;
		live->ListBox::SetItemsSource(BindingListReference(candidate));
	}
	catch (...)
	{
		live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
		if (live)
		{
			if (live->ListBox::GetItemsSource().Shared() != candidate)
				rollbackDerivedState(*live);
			live->RefreshSelectedCellContainersAfterRollback();
			live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
			if (live)
			{
				live->_settingItemsSource = false;
				// Preserve the original transaction failure if an accessibility
				// client itself fails while observing the restored old projection.
				try { live->FlushAccessibilityStructureChange(); }
				catch (...) {}
			}
		}
		throw;
	}
	live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live) return;
	if (live->ListBox::GetItemsSource().Shared() != candidate)
	{
		rollbackDerivedState(*live);
		live->RefreshSelectedCellContainersAfterRollback();
		live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
		if (live)
		{
			live->_settingItemsSource = false;
			live->FlushAccessibilityStructureChange();
		}
		return;
	}
	if (clearSortState)
		for (auto& column : live->_columns)
			column->_sortDirection.reset();
	if (!live->_currentCell.IsValid() && previousCurrentCell.IsValid()
		&& hasPreviousCurrentItemOrdinal && !live->_columns.empty())
	{
		size_t columnIndex = previousCurrentCell.ColumnIndex;
		const auto column = std::find_if(
			live->_columns.begin(), live->_columns.end(),
			[&previousCurrentCell](const auto& value)
			{ return value.get() == previousCurrentCell.Column; });
		if (column != live->_columns.end())
			columnIndex = static_cast<size_t>(
				std::distance(live->_columns.begin(), column));
		else columnIndex = (std::min)(
			columnIndex, live->_columns.size() - 1);
		size_t ordinal = 0;
		const auto items = live->GetItemsView();
		for (size_t rowIndex = 0; items
			&& rowIndex < items.Get()->Count(); ++rowIndex)
		{
			BindingSourceReference item;
			if (!items.Get()->TryGetItem(rowIndex, item) || !item
				|| item.Shared() != previousCurrentCell.Item.Shared()) continue;
			if (ordinal++ != previousCurrentItemOrdinal) continue;
			DataGridCellInfo restored;
			if (live->TryCreateCellInfo(rowIndex, columnIndex, restored))
			{
				live->_currentCell = std::move(restored);
				if (!live->_currentCellChangeDeferred)
				{
					live->_currentCellChangeDeferred = true;
					live->_deferredCurrentCellOld = previousCurrentCell;
				}
			}
			break;
		}
	}
	if (!live->ReconcileCurrentCellColumn()) return;
	live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live || !live->ReconcileSelectedCells()) return;
	live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live || !live->SynchronizeSelectedCellsFromRows()) return;
	live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live) return;
	// WPF's CurrentCell equality is item + column. Replacing ItemsSource may
	// assign a new occurrence token to the same object, but that internal token
	// refresh is not a logical CurrentCell change and must not raise the event.
	if (live->_currentCellChangeDeferred
		&& live->_deferredCurrentCellOld.IsValid()
		&& live->_currentCell.IsValid()
		&& live->_deferredCurrentCellOld.Item.Shared()
			== live->_currentCell.Item.Shared()
		&& live->_deferredCurrentCellOld.Column == live->_currentCell.Column)
	{
		live->_currentCellChangeDeferred = false;
		live->_deferredCurrentCellOld = {};
	}
	if (std::isnan(live->_rowHeaderWidth))
	{
		live->InvalidateRowHeaderWidthBaseline();
		(void)live->SetCurrentRowHeaderActualWidth(0.0);
		live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
		if (!live || !live->RefreshRowHeaderActualWidth()) return;
		live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
		if (!live) return;
	}
	live->_rowHeaderSourceChanged =
		std::move(candidateRowHeaderSourceChanged);
	for (auto& snapshot : previousRuntimeAutoColumns)
		if (snapshot.Column) snapshot.Column->_owner = nullptr;
	live->_autoColumnsChangedDuringPreparation = true;
	live->_settingItemsSource = false;
	live->FlushCommittedItemsSourceState();
	live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (live) live->FlushAccessibilityStructureChange();
}

void DataGrid::SetAutoGenerateColumns(bool value)
{
	(void)TrySetPropertyValue(
		AutoGenerateColumnsProperty(), BindingValue(value));
}

void DataGrid::SetIsReadOnly(bool value)
{
	(void)TrySetPropertyValue(IsReadOnlyProperty(), BindingValue(value));
}

void DataGrid::SetCanUserSortColumns(bool value)
{
	(void)TrySetPropertyValue(
		CanUserSortColumnsProperty(), BindingValue(value));
}

void DataGrid::SetCanUserResizeColumns(bool value)
{
	(void)TrySetPropertyValue(
		CanUserResizeColumnsProperty(), BindingValue(value));
}

void DataGrid::SetEnableColumnVirtualization(bool value)
{
	(void)TrySetPropertyValue(
		EnableColumnVirtualizationProperty(), BindingValue(value));
}

void DataGrid::SetSelectionUnit(DataGridSelectionUnit value)
{
	(void)TrySetPropertyValue(
		SelectionUnitProperty(), BindingValue(static_cast<int>(value)));
}

void DataGrid::SetColumnHeaderHeight(double value)
{
	(void)TrySetPropertyValue(
		ColumnHeaderHeightProperty(), BindingValue(value));
}

void DataGrid::SetRowHeaderWidth(double value)
{
	(void)TrySetPropertyValue(
		RowHeaderWidthProperty(), BindingValue(value));
}

bool DataGrid::SetCurrentRowHeaderActualWidth(double value)
{
	if (!std::isfinite(value) || value < 0.0) return false;
	return SetReadOnlyPropertyField(
		RowHeaderActualWidthPropertyKey(), _rowHeaderActualWidth, value);
}

void DataGrid::InvalidateRowHeaderWidthBaseline() noexcept
{
	if (++_rowHeaderWidthEpoch == 0) ++_rowHeaderWidthEpoch;
}

bool DataGrid::TryShareRowHeaderDesiredWidth(
	DataGridRowHeader& header,
	double desiredWidth,
	double& sharedWidth)
{
	sharedWidth = desiredWidth;
	if (!std::isfinite(desiredWidth) || desiredWidth < 0.0
		|| !std::isnan(_rowHeaderWidth)
		|| !HasRowHeaders(_headersVisibility)
		|| _settingItemsSource || IsItemsSourceUpdateInProgress()) return false;
	auto* row = header.GetRowOwner();
	if (!row || row->GetDataGridOwner() != this) return false;
	bool attached = false;
	for (auto* current = static_cast<Control*>(&header); current;
		current = current->GetVisualParent())
	{
		if (current->GetVisibility() == Visibility::Collapsed) return false;
		if (current == this)
		{
			attached = true;
			break;
		}
	}
	if (!attached) return false;
	const ControlWeakReference ownerLifetime(this);
	const ControlWeakReference headerLifetime(&header);
	if (desiredWidth > _rowHeaderActualWidth + 0.0001)
		(void)SetCurrentRowHeaderActualWidth(desiredWidth);
	auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	auto* liveHeader = dynamic_cast<DataGridRowHeader*>(headerLifetime.Get());
	if (!live || !liveHeader || !std::isnan(live->_rowHeaderWidth)
		|| !HasRowHeaders(live->_headersVisibility)
		|| live->_settingItemsSource || live->IsItemsSourceUpdateInProgress())
		return false;
	row = liveHeader->GetRowOwner();
	if (!row || row->GetDataGridOwner() != live) return false;
	sharedWidth = live->_rowHeaderActualWidth;
	return true;
}

void DataGrid::SetRowHeight(double value)
{
	(void)TrySetPropertyValue(RowHeightProperty(), BindingValue(value));
}

void DataGrid::SetHeadersVisibility(DataGridHeadersVisibility value)
{
	(void)TrySetPropertyValue(
		HeadersVisibilityProperty(), BindingValue(static_cast<int>(value)));
}

void DataGrid::SetGridLinesVisibility(DataGridGridLinesVisibility value)
{
	(void)TrySetPropertyValue(
		GridLinesVisibilityProperty(), BindingValue(static_cast<int>(value)));
}

void DataGrid::SetRowBackground(cui::drawing::Brush value)
{
	(void)TrySetPropertyValue(
		RowBackgroundProperty(), BindingValue(std::move(value)));
}

void DataGrid::SetAlternatingRowBackground(cui::drawing::Brush value)
{
	(void)TrySetPropertyValue(
		AlternatingRowBackgroundProperty(), BindingValue(std::move(value)));
}

void DataGrid::SetHorizontalGridLinesBrush(cui::drawing::Brush value)
{
	(void)TrySetPropertyValue(
		HorizontalGridLinesBrushProperty(), BindingValue(std::move(value)));
}

void DataGrid::SetVerticalGridLinesBrush(cui::drawing::Brush value)
{
	(void)TrySetPropertyValue(
		VerticalGridLinesBrushProperty(), BindingValue(std::move(value)));
}

DataGridColumn* DataGrid::AddColumn(
	std::unique_ptr<DataGridColumn> column)
{
	return AddColumnCore(std::move(column), false);
}

DataGridColumn* DataGrid::AddAutoGeneratedColumn(
	std::unique_ptr<DataGridColumn> column)
{
	return AddColumnCore(std::move(column), true);
}

DataGridColumn* DataGrid::AdoptColumn(DataGridColumn* column)
{
	return column ? AddColumn(std::unique_ptr<DataGridColumn>(column)) : nullptr;
}

DataGridColumn* DataGrid::AddColumnCore(
	std::unique_ptr<DataGridColumn> column, bool autoGenerated)
{
	if (!column) return nullptr;
	if (!_columnResizeSnapshot.empty()) EndColumnResizeTransaction(true);
	if (column->_owner)
		throw std::logic_error("DataGridColumn already has an owner");
	const size_t displayIndex = column->_displayIndex ==
		DataGridColumn::UnsetDisplayIndex
		? _columns.size()
		: (std::min)(column->_displayIndex, _columns.size());
	// Complete every potentially-throwing capacity change before publishing the
	// owner. The two projections are then committed without another allocation.
	_columns.reserve(_columns.size() + 1);
	_logicalColumns.reserve(_logicalColumns.size() + 1);
	if (column->_accessibilityIdentity == 0)
		column->_accessibilityIdentity = AllocateAccessibilityVirtualId();
	column->_owner = this;
	column->_isAutoGenerated = autoGenerated;
	column->_isRuntimeAutoGenerated = false;
	auto* raw = column.get();
	_columns.push_back(std::move(column));
	_logicalColumns.push_back(raw);
	if (displayIndex + 1 < _columns.size())
		std::rotate(
			_columns.begin() + displayIndex,
			_columns.end() - 1,
			_columns.end());
	ReindexDisplayColumns(displayIndex);
	RefreshColumns();
	return raw;
}

void DataGrid::ClearColumns()
{
	_runtimeAutoGenerationComplete = false;
	_autoGeneratedColumnsEventPending = false;
	if (_columns.empty()) return;
	if (!_columnResizeSnapshot.empty()) EndColumnResizeTransaction(true);
	const ControlWeakReference ownerLifetime(this);
	(void)CancelEdit();
	if (!ownerLifetime.Get()) return;
	_selectionAnchor.reset();
	ResetSelectionRange();
	if (!ApplySelectedCells({})) return;
	auto removed = std::move(_columns);
	_logicalColumns.clear();
	PruneAccessibilityColumnIdentities();
	for (auto& column : removed)
	{
		column->_owner = nullptr;
		column->_displayIndex = DataGridColumn::UnsetDisplayIndex;
		column->_sortDirection.reset();
	}
	const auto previous = _currentCell;
	_currentCell = {};
	// Keep removed definitions alive while observers inspect OldCell.Column.
	if (!RaiseCurrentCellChanged(previous)) return;
	RefreshColumns();
}

DataGridColumn* DataGrid::GetColumn(size_t index) const noexcept
{
	return index < _logicalColumns.size() ? _logicalColumns[index] : nullptr;
}

DataGridColumn* DataGrid::GetColumnFromDisplayIndex(size_t index) const noexcept
{
	return index < _columns.size() ? _columns[index].get() : nullptr;
}

void DataGrid::ReindexDisplayColumns(size_t begin) noexcept
{
	begin = (std::min)(begin, _columns.size());
	for (size_t index = begin; index < _columns.size(); ++index)
		_columns[index]->_displayIndex = index;
}

bool DataGrid::MoveColumn(
	size_t oldDisplayIndex, size_t newDisplayIndex)
{
	if (oldDisplayIndex >= _columns.size()) return false;
	return SetColumnDisplayIndex(
		*_columns[oldDisplayIndex], newDisplayIndex);
}

bool DataGrid::SetColumnDisplayIndex(
	DataGridColumn& column, size_t displayIndex)
{
	if (_changingColumnDisplayIndex || column._owner != this
		|| displayIndex >= _columns.size()
		|| column._displayIndex >= _columns.size()
		|| _columns[column._displayIndex].get() != &column) return false;
	const size_t oldDisplayIndex = column._displayIndex;
	if (oldDisplayIndex == displayIndex) return true;

	const ControlWeakReference ownerLifetime(this);
	if (!_columnResizeSnapshot.empty()) EndColumnResizeTransaction(true);
	(void)CancelEdit();
	auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live || live->_changingColumnDisplayIndex
		|| column._owner != live || displayIndex >= live->_columns.size()
		|| column._displayIndex >= live->_columns.size()
		|| live->_columns[column._displayIndex].get() != &column) return false;

	const size_t committedOldIndex = column._displayIndex;
	live->_changingColumnDisplayIndex = true;
	const auto resetChanging = MakeScopeExit([ownerLifetime]
	{
		if (auto* owner = dynamic_cast<DataGrid*>(ownerLifetime.Get()))
			owner->_changingColumnDisplayIndex = false;
	});
	const auto rotateProjection = [committedOldIndex, displayIndex](auto& values)
	{
		if (values.size() == 0) return;
		if (committedOldIndex < displayIndex)
			std::rotate(
				values.begin() + committedOldIndex,
				values.begin() + committedOldIndex + 1,
				values.begin() + displayIndex + 1);
		else std::rotate(
			values.begin() + displayIndex,
			values.begin() + committedOldIndex,
			values.begin() + committedOldIndex + 1);
	};
	rotateProjection(live->_columns);
	if (live->_sampledColumnContentWidths.size() == live->_columns.size())
		rotateProjection(live->_sampledColumnContentWidths);
	live->ReindexDisplayColumns((std::min)(
		committedOldIndex, displayIndex));
	live->RefreshColumnDisplayOrder();
	live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live || displayIndex >= live->_columns.size()
		|| live->_columns[displayIndex].get() != &column) return false;
	live->_changingColumnDisplayIndex = false;
	if (live->ColumnDisplayIndexChanged.Empty()) return true;
	DataGridColumnDisplayIndexChangedEventArgs args;
	args.Column = &column;
	args.OldDisplayIndex = committedOldIndex;
	args.NewDisplayIndex = displayIndex;
	cui::framework::EventAccess::RaiseWhile(
		live->ColumnDisplayIndexChanged,
		[&]() { return ownerLifetime.Get() != nullptr; },
		live, args);
	return ownerLifetime.Get() != nullptr;
}

void DataGrid::RemoveAutoGeneratedColumns()
{
	_runtimeAutoGenerationComplete = false;
	_autoGeneratedColumnsEventPending = false;
	const size_t autoCount = static_cast<size_t>(std::count_if(
		_columns.begin(), _columns.end(),
		[](const auto& column) { return column->_isAutoGenerated; }));
	if (autoCount == 0) return;
	if (!_columnResizeSnapshot.empty()) EndColumnResizeTransaction(true);
	std::vector<std::unique_ptr<DataGridColumn>> retained;
	std::vector<std::unique_ptr<DataGridColumn>> removed;
	retained.reserve(_columns.size() - autoCount);
	removed.reserve(autoCount);
	for (auto& column : _columns)
	{
		if (column->_isAutoGenerated)
		{
			column->_owner = nullptr;
			column->_displayIndex = DataGridColumn::UnsetDisplayIndex;
			removed.push_back(std::move(column));
		}
		else retained.push_back(std::move(column));
	}
	_columns = std::move(retained);
	_logicalColumns.erase(std::remove_if(
		_logicalColumns.begin(), _logicalColumns.end(),
		[](const DataGridColumn* column)
		{ return column->_isAutoGenerated; }), _logicalColumns.end());
	ReindexDisplayColumns();
	PruneAccessibilityColumnIdentities();
	// Keep removed definitions alive while CurrentCellChanged observers inspect
	// the old cell identity.
	if (!ReconcileCurrentCellColumn()) return;
	(void)ReconcileSelectedCells();
}

void DataGrid::EnsureAutoGeneratedColumns()
{
#if CUI_ENABLE_DYNAMIC_XAML
	const ControlWeakReference ownerLifetime(this);
	const auto source = _source;
	if (!_autoGenerateColumns || !source || _runtimeAutoGenerationComplete
		|| _autoGenerationInProgress) return;
	if (std::any_of(_columns.begin(), _columns.end(),
		[](const auto& column) { return column->_isAutoGenerated; }))
	{
		_runtimeAutoGenerationComplete = true;
		return;
	}
	if (!_columnResizeSnapshot.empty()) EndColumnResizeTransaction(true);
	std::vector<std::pair<DataGridColumn*, uint32_t>> columnSnapshot;
	columnSnapshot.reserve(_columns.size());
	for (const auto& column : _columns)
		columnSnapshot.emplace_back(
			column.get(), column->_accessibilityIdentity);
	const auto resolveCurrent = [&]() -> DataGrid*
	{
		auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
		if (!live || !live->_autoGenerateColumns
			|| live->_source.Shared() != source.Shared()
			|| live->_columns.size() != columnSnapshot.size()) return nullptr;
		for (size_t index = 0; index < columnSnapshot.size(); ++index)
			if (live->_columns[index].get() != columnSnapshot[index].first
				|| live->_columns[index]->_accessibilityIdentity
					!= columnSnapshot[index].second) return nullptr;
		return live;
	};
	const size_t count = source.Get()->Count();
	auto* live = resolveCurrent();
	if (!live || count == 0) return;
	BindingSourceReference first;
	const bool read = source.Get()->TryGetItem(0, first);
	live = resolveCurrent();
	if (!live || !read || !first) return;
	const auto properties = first.Get()->GetProperties();
	live = resolveCurrent();
	if (!live) return;
	live->_autoGenerationInProgress = true;
	auto resetGeneration = MakeScopeExit([ownerLifetime]
	{
		if (auto* owner = dynamic_cast<DataGrid*>(ownerLifetime.Get()))
			owner->_autoGenerationInProgress = false;
	});
	const bool customizeColumns = !live->AutoGeneratingColumn.Empty();
	std::vector<std::unique_ptr<DataGridColumn>> generated;
	generated.reserve(properties.size());
	for (const auto& property : properties)
	{
		if (!property.CanRead || property.Name.empty()) continue;
		std::unique_ptr<DataGridColumn> column;
		if (property.ValueKind == BindingValueKind::Bool
			|| property.ValueKind == BindingValueKind::NullableBool)
		{
			auto check = std::make_unique<DataGridCheckBoxColumn>();
			check->SetIsThreeState(
				property.ValueKind == BindingValueKind::NullableBool);
			column = std::move(check);
		}
		else column = std::make_unique<DataGridTextColumn>();
		column->SetHeader(BindingValue(property.Name));
		column->SetSortMemberPath(property.Name);
		column->SetIsReadOnly(!property.CanWrite);
		auto* bound = dynamic_cast<DataGridBoundColumn*>(column.get());
		if (bound) bound->SetBindingPath(property.Name);
		if (customizeColumns)
		{
			DataGridAutoGeneratingColumnEventArgs args;
			args.PropertyName = property.Name;
			args.PropertyKind = property.ValueKind;
			args.PropertyType = property.ValueType;
			args.Column = std::move(column);
			cui::framework::EventAccess::RaiseWhile(
				live->AutoGeneratingColumn,
				[&]() { return resolveCurrent() != nullptr; },
				live, args);
			live = resolveCurrent();
			if (!live) return;
			if (args.Cancel || !args.Column) continue;
			column = std::move(args.Column);
			if (column->_owner)
				throw std::logic_error(
					"AutoGeneratingColumn replacement already has an owner");
		}
		generated.push_back(std::move(column));
	}
	live = resolveCurrent();
	if (!live) return;
	live->_columns.reserve(live->_columns.size() + generated.size());
	live->_logicalColumns.reserve(
		live->_logicalColumns.size() + generated.size());
	size_t firstChangedDisplayIndex = live->_columns.size();
	for (auto& column : generated)
	{
		const size_t displayIndex = column->_displayIndex ==
			DataGridColumn::UnsetDisplayIndex
			? live->_columns.size()
			: (std::min)(column->_displayIndex, live->_columns.size());
		column->_accessibilityIdentity = AllocateAccessibilityVirtualId();
		column->_owner = live;
		column->_isAutoGenerated = true;
		column->_isRuntimeAutoGenerated = true;
		auto* raw = column.get();
		live->_columns.push_back(std::move(column));
		live->_logicalColumns.push_back(raw);
		if (displayIndex + 1 < live->_columns.size())
			std::rotate(
				live->_columns.begin() + displayIndex,
				live->_columns.end() - 1,
				live->_columns.end());
		firstChangedDisplayIndex = (std::min)(
			firstChangedDisplayIndex, displayIndex);
	}
	live->ReindexDisplayColumns(firstChangedDisplayIndex);
	live->_runtimeAutoGenerationComplete = true;
	if (!live->AutoGeneratedColumns.Empty())
		live->_autoGeneratedColumnsEventPending = true;
#endif
}

void DataGrid::FlushAutoGeneratedColumnsEvent()
{
	if (!_autoGeneratedColumnsEventPending || _settingItemsSource
		|| IsItemsSourceUpdateInProgress()) return;
	_autoGeneratedColumnsEventPending = false;
	if (AutoGeneratedColumns.Empty()) return;
	const ControlWeakReference ownerLifetime(this);
	cui::framework::EventAccess::RaiseWhile(
		AutoGeneratedColumns,
		[&]() { return ownerLifetime.Get() != nullptr; },
		this);
}

void DataGrid::RefreshColumns()
{
	const ControlWeakReference ownerLifetime(this);
	(void)CancelEdit();
	if (!ownerLifetime.Get()) return;
	InvalidateRealizedColumnRange();
	InvalidateAccessibilityVirtualIdentities();
	InvalidateColumnContentWidthCache();
	(void)RebuildGeneratedItems();
	if (auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get()))
		live->RefreshHeaderPresenter();
}

void DataGrid::RefreshColumnDisplayOrder()
{
	// Display reordering does not change a column's binding or content. Preserve
	// the per-column sampled widths (already rotated with the projection) and
	// invalidate only order-dependent width/prefix and realized visual state.
	const ControlWeakReference ownerLifetime(this);
	InvalidateRealizedColumnRange();
	InvalidateAccessibilityVirtualIdentities();
	InvalidateColumnWidthCache();
	_columnWidthRefreshPending = false;
	_columnWidthMeasureDirty.clear();
	_columnWidthDirtyBegin = DataGridCellInfo::InvalidIndex;
	_columnWidthDirtyEnd = DataGridCellInfo::InvalidIndex;
	++_columnContentWidthCacheEpoch;
	if (_columnContentWidthCacheEpoch == 0)
		_columnContentWidthCacheEpoch = 1;

	const bool previous = _preserveColumnContentWidthsDuringRowRebuild;
	_preserveColumnContentWidthsDuringRowRebuild = true;
	const auto restore = MakeScopeExit([ownerLifetime, previous]
	{
		if (auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get()))
			live->_preserveColumnContentWidthsDuringRowRebuild = previous;
	});
	(void)RebuildGeneratedItems();
	if (auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get()))
		live->RefreshHeaderPresenter();
}

void DataGrid::InvalidateRows()
{
	// Appearance/row-height changes rebuild realized containers, but neither the
	// source projection nor a column binding changed.  Preserve the expensive
	// Auto/SizeToCells sample while keeping the existing regeneration contract.
	// A source transaction that re-enters preparation remains authoritative and
	// ignores this guard in OnBeforeGeneratedItemsPrepared.
	const ControlWeakReference ownerLifetime(this);
	const bool previous = _preserveColumnContentWidthsDuringRowRebuild;
	_preserveColumnContentWidthsDuringRowRebuild = true;
	auto restore = MakeScopeExit([ownerLifetime, previous]
	{
		if (auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get()))
			live->_preserveColumnContentWidthsDuringRowRebuild = previous;
	});
	(void)RebuildGeneratedItems();
}

void DataGrid::RefreshHeaderPresenter()
{
	const ControlWeakReference ownerLifetime(this);
	auto* presenter = GetColumnHeadersPresenter();
	if (!presenter) return;
	std::wstring error;
	if (!presenter->Initialize(*this, &error)) SetLastTemplateError(error);
	auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live) return;
	live->RefreshHeadersVisibility();
}

bool DataGrid::RefreshRowHeaderActualWidthProjection()
{
	const ControlWeakReference ownerLifetime(this);
	const double width = _rowHeaderActualWidth;
	if (auto* selectAll = GetSelectAllButton())
	{
		selectAll->SetWidth(cui::layout::Length::Fixed(
			static_cast<float>(width)));
		if (!ownerLifetime.Get()) return false;
	}

	auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live) return false;
	std::vector<ControlWeakReference> headers;
	headers.reserve(live->GetRealizedItems().size());
	for (const auto& [index, realized] : live->GetRealizedItems())
	{
		(void)realized;
		auto* row = dynamic_cast<DataGridRow*>(live->GetGeneratedItem(index));
		auto* header = row ? row->GetRowHeader() : nullptr;
		if (header) headers.emplace_back(header);
	}
	for (const auto& headerLifetime : headers)
	{
		auto* header = dynamic_cast<DataGridRowHeader*>(headerLifetime.Get());
		live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
		if (!live) return false;
		auto* row = header ? header->GetRowOwner() : nullptr;
		if (!row || row->GetDataGridOwner() != live) continue;
		header->InvalidateSharedWidthMeasure();
		if (!ownerLifetime.Get()) return false;
	}
	live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live) return false;
	live->RefreshVirtualScrollMetrics();
	return ownerLifetime.Get() != nullptr;
}

bool DataGrid::RefreshRowHeaderActualWidth()
{
	if (!std::isnan(_rowHeaderWidth)
		|| !HasRowHeaders(_headersVisibility)
		|| GetVisibility() == Visibility::Collapsed) return true;
	const ControlWeakReference ownerLifetime(this);
	const size_t baselineEpoch = _rowHeaderWidthEpoch;
	const auto participatesInMeasure = [](Control* header,
		DataGrid* owner) noexcept
	{
		for (auto* current = header; current;
			current = current->GetVisualParent())
		{
			if (current->GetVisibility() == Visibility::Collapsed) return false;
			if (current == owner) return true;
		}
		return false;
	};
	std::vector<ControlWeakReference> headers;
	headers.reserve(GetRealizedItems().size());
	for (const auto& [index, realized] : GetRealizedItems())
	{
		(void)realized;
		auto* row = dynamic_cast<DataGridRow*>(GetGeneratedItem(index));
		auto* header = row ? row->GetRowHeader() : nullptr;
		if (header && participatesInMeasure(header, this))
			headers.emplace_back(header);
	}
	double widest = _rowHeaderActualWidth;
	for (const auto& headerLifetime : headers)
	{
		auto* header = dynamic_cast<DataGridRowHeader*>(headerLifetime.Get());
		if (!header || header->GetVisibility() == Visibility::Collapsed) continue;
		const auto desired = header->Measure(cui::core::Constraints::Unbounded());
		auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
		if (!live) return false;
		if (!std::isnan(live->_rowHeaderWidth)
			|| !HasRowHeaders(live->_headersVisibility)
			|| live->_rowHeaderWidthEpoch != baselineEpoch)
			return true;
		header = dynamic_cast<DataGridRowHeader*>(headerLifetime.Get());
		if (!header || !participatesInMeasure(header, live)) continue;
		widest = (std::max)(widest, static_cast<double>(desired.width));
	}
	auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live) return false;
	if (!std::isnan(live->_rowHeaderWidth)
		|| !HasRowHeaders(live->_headersVisibility)
		|| live->_rowHeaderWidthEpoch != baselineEpoch) return true;
	if (widest > live->_rowHeaderActualWidth + 0.0001)
	{
		(void)live->SetCurrentRowHeaderActualWidth(widest);
		return ownerLifetime.Get() != nullptr;
	}
	return true;
}

double DataGrid::ResolveRowHeaderWidth() const noexcept
{
	return _rowHeaderActualWidth;
}

void DataGrid::RefreshHeadersVisibility()
{
	const ControlWeakReference ownerLifetime(this);
	const auto headersVisibility = _headersVisibility;
	const double columnHeaderHeight = _columnHeaderHeight;
	if (auto* presenter = GetColumnHeadersPresenter())
	{
		const ControlWeakReference presenterLifetime(presenter);
		presenter->SetVisibility(HasColumnHeaders(headersVisibility)
			? Visibility::Visible : Visibility::Collapsed);
		if (!ownerLifetime.Get()) return;
		presenter = dynamic_cast<DataGridColumnHeadersPresenter*>(
			presenterLifetime.Get());
		if (!presenter) return;
		if (std::isfinite(columnHeaderHeight))
			presenter->SetHeight(cui::layout::Length::Fixed(
				static_cast<float>(columnHeaderHeight)));
		else presenter->SetHeight(cui::layout::Length::Auto());
	}
	auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live) return;
	if (auto* selectAll = live->GetSelectAllButton())
	{
		const ControlWeakReference selectAllLifetime(selectAll);
		const bool cornerVisible =
			live->_headersVisibility == DataGridHeadersVisibility::All;
		const double rowHeaderWidth = live->ResolveRowHeaderWidth();
		const double liveColumnHeaderHeight = live->_columnHeaderHeight;
		const bool selectAllEnabled =
			live->GetSelectionMode() == SelectionMode::Extended;
		selectAll->SetVisibility(
			cornerVisible ? Visibility::Visible : Visibility::Collapsed);
		if (!ownerLifetime.Get()) return;
		selectAll = dynamic_cast<Button*>(selectAllLifetime.Get());
		if (!selectAll) return;
		selectAll->SetWidth(cui::layout::Length::Fixed(
			static_cast<float>(rowHeaderWidth)));
		if (!ownerLifetime.Get()) return;
		selectAll = dynamic_cast<Button*>(selectAllLifetime.Get());
		if (!selectAll) return;
		if (std::isfinite(liveColumnHeaderHeight))
			selectAll->SetHeight(cui::layout::Length::Fixed(
				static_cast<float>(liveColumnHeaderHeight)));
		else selectAll->SetHeight(cui::layout::Length::Auto());
		if (!ownerLifetime.Get()) return;
		selectAll = dynamic_cast<Button*>(selectAllLifetime.Get());
		if (!selectAll) return;
		selectAll->SetIsEnabled(selectAllEnabled);
	}
	live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live) return;
	std::vector<ControlWeakReference> rows;
	rows.reserve(live->GetRealizedItems().size());
	for (const auto& [index, realized] : live->GetRealizedItems())
	{
		(void)realized;
		if (auto* row = dynamic_cast<DataGridRow*>(
			live->GetGeneratedItem(index))) rows.emplace_back(row);
	}
	for (const auto& rowLifetime : rows)
	{
		auto* row = dynamic_cast<DataGridRow*>(rowLifetime.Get());
		if (row && row->GetDataGridOwner() == live) row->UpdateRowHeader();
		live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
		if (!live) return;
	}
	live->RefreshHorizontalScrollAlignment();
	live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (live) live->RefreshVirtualScrollMetrics();
}

void DataGrid::RefreshHorizontalScrollAlignment()
{
	const ControlWeakReference ownerLifetime(this);
	if (auto* scroll = dynamic_cast<ScrollViewer*>(_scrollViewer.Get()))
		_horizontalScrollOffset = std::isfinite(scroll->GetHorizontalOffset())
			? scroll->GetHorizontalOffset() : 0.0;
	else _horizontalScrollOffset = 0.0;
	RefreshRealizedColumns();
	auto* liveAfterVirtualization = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!liveAfterVirtualization) return;
	const double horizontalOffset = _horizontalScrollOffset;
	if (auto* presenter = GetColumnHeadersPresenter())
	{
		if (std::abs(horizontalOffset) <= 0.0001)
			presenter->ClearRenderTransform();
		else presenter->SetRenderTransform(
			HorizontalTranslation(-horizontalOffset));
	}
	auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live) return;
	std::vector<ControlWeakReference> rows;
	rows.reserve(live->GetRealizedItems().size());
	for (const auto& [index, realized] : live->GetRealizedItems())
	{
		(void)realized;
		if (auto* row = dynamic_cast<DataGridRow*>(live->GetGeneratedItem(index)))
			rows.emplace_back(row);
	}
	for (const auto& rowLifetime : rows)
	{
		auto* row = dynamic_cast<DataGridRow*>(rowLifetime.Get());
		if (row && row->GetDataGridOwner() == live)
			row->UpdateHorizontalScrollOffset(horizontalOffset);
		live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
		if (!live) return;
	}
}

bool DataGrid::HandleRowHeaderClick(
	DataGridRow& row, ModifierKeys modifiers)
{
	if (row.GetDataGridOwner() != this) return false;
	size_t rowIndex = row.ItemIndex();
	if (rowIndex >= ItemCount()) return false;
	const auto targetView = GetItemsView();
	const BindingSourceReference targetItem = row.GetItem();
	size_t targetOccurrence = DataGridCellInfo::InvalidIndex;
	if (!TryGetItemOccurrenceAt(rowIndex, targetOccurrence)) return false;
	const ControlWeakReference ownerLifetime(this);
	size_t focusColumn = 0;
	auto resolveTarget = [&]() -> DataGridRow*
	{
		auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
		if (!live || live->GetItemsView().Shared() != targetView.Shared()
			|| !live->TryResolveItemOccurrence(
				targetItem, targetOccurrence, rowIndex)) return nullptr;
		return live->ResolveRow(rowIndex);
	};
	if (!_columns.empty())
	{
		const bool currentRow = _currentCell.IsValid()
			&& _currentCell.Item.Shared() == targetItem.Shared()
			&& _currentCell._itemOccurrence == targetOccurrence;
		if (currentRow)
		{
			focusColumn = (std::min)(
				_currentCell.ColumnIndex, _columns.size() - 1);
			if (auto* current = ResolveCurrentCellContainer();
				current && current->GetIsEditing()) (void)CommitEdit();
			if (!resolveTarget()) return false;
		}
		else if (!SetCurrentCell(rowIndex, 0)) return false;
		auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
		auto* liveRow = resolveTarget();
		if (!live || !liveRow) return false;
		if (auto* cell = liveRow->GetCell(focusColumn))
			if (auto* window = live->GetPresentationWindow())
				window->SetKeyboardFocus(cell, true);
	}
	auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live) return false;
	if (live->_selectionUnit == DataGridSelectionUnit::Cell) return true;
	if (!resolveTarget()) return false;
	live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live) return false;
	const bool replaceCellSelection = live->_selectionUnit
		== DataGridSelectionUnit::CellOrRowHeader
		&& !HasModifier(modifiers, ModifierKeys::Control)
		&& !HasModifier(modifiers, ModifierKeys::Shift);
	const auto previousRows = live->GetSelectedIndices();
	const bool previousReplacement = live->_replaceCellSelectionFromRows;
	if (replaceCellSelection) live->_replaceCellSelectionFromRows = true;
	auto restoreReplacement = MakeScopeExit(
		[ownerLifetime, previousReplacement]
		{
			if (auto* owner = dynamic_cast<DataGrid*>(ownerLifetime.Get()))
				owner->_replaceCellSelectionFromRows = previousReplacement;
		});
	live->NotifyItemClicked(rowIndex, MouseButton::Left, modifiers);
	live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live) return false;
	// SelectOnly is a no-op when the row was already the sole selected row.
	// WPF still clears independent CellOrRowHeader cells on a plain row-header
	// click, so perform the same synchronization when no row event ran.
	if (replaceCellSelection && live->GetSelectedIndices() == previousRows
		&& !live->SynchronizeSelectedCellsFromRows()) return false;
	return true;
}

void DataGrid::HandleSelectAll()
{
	if (GetSelectionMode() != SelectionMode::Extended
		|| ItemCount() == 0 || _columns.empty()) return;
	if (_selectionUnit == DataGridSelectionUnit::Cell) SelectAllCells();
	else ListBox::SelectAll();
}

void DataGrid::OnControlTemplatePresentationChanged()
{
	_selectAllClick.Disconnect();
	_dataGridScrollChanged.Disconnect();
	_headersPresenter.Reset();
	_selectAllButton.Reset();
	_scrollViewer.Reset();
	_horizontalScrollOffset = 0.0;
	_columnViewportWidth =
		(std::numeric_limits<double>::quiet_NaN)();
	InvalidateColumnWidthCache();
	const ControlWeakReference ownerLifetime(this);
	ListBox::OnControlTemplatePresentationChanged();
	auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live) return;
	if (!live->GetControlTemplateRoot()) return;
	auto* presenter = dynamic_cast<DataGridColumnHeadersPresenter*>(
		live->FindDeclarativeTemplatePart(
			MakeTemplatePartToken(L"PART_ColumnHeadersPresenter")));
	if (!presenter)
	{
		live->SetLastTemplateError(
			L"DataGrid ControlTemplate 必须包含 PART_ColumnHeadersPresenter。");
		return;
	}
	std::wstring error;
	if (!presenter->Initialize(*live, &error))
	{
		if (auto* owner = dynamic_cast<DataGrid*>(ownerLifetime.Get()))
			owner->SetLastTemplateError(std::move(error));
		return;
	}
	live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live) return;
	live->_headersPresenter = presenter;
	if (auto* selectAll = dynamic_cast<Button*>(
		live->FindDeclarativeTemplatePart(
			MakeTemplatePartToken(L"PART_SelectAllButton"))))
	{
		live->_selectAllButton = selectAll;
		live->_selectAllClick = selectAll->Click.Subscribe(
			[ownerLifetime](Control*, RoutedEventArgs& args)
			{
				args.Handled = true;
				if (auto* owner = dynamic_cast<DataGrid*>(ownerLifetime.Get()))
					owner->HandleSelectAll();
			});
	}
	live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live) return;
	if (auto* scroll = dynamic_cast<ScrollViewer*>(
		live->FindDeclarativeTemplatePart(
			MakeTemplatePartToken(L"PART_ScrollViewer"))))
	{
		live->_scrollViewer = scroll;
		live->_dataGridScrollChanged = scroll->OnScrollChanged.Subscribe(
			[ownerLifetime](Control*, ScrollChangedEventArgs& args)
			{
				if (auto* owner = dynamic_cast<DataGrid*>(ownerLifetime.Get()))
				{
					constexpr double epsilon = 0.000001;
					const bool horizontalChanged =
						!std::isfinite(owner->_horizontalScrollOffset)
						|| std::abs(args.HorizontalOffset
							- owner->_horizontalScrollOffset) > epsilon;
					const double resolvedViewportWidth =
						owner->ResolveColumnViewportWidth(args.ViewportWidth);
					const bool viewportWidthChanged =
						!std::isfinite(owner->_columnViewportWidth)
						|| std::abs(resolvedViewportWidth
							- owner->_columnViewportWidth) > epsilon;
					if (horizontalChanged)
						owner->_horizontalScrollOffset = args.HorizontalOffset;
					if (viewportWidthChanged)
						owner->UpdateColumnViewportWidth(resolvedViewportWidth);
					owner = dynamic_cast<DataGrid*>(ownerLifetime.Get());
					if (!owner) return;
					if (viewportWidthChanged
						&& !owner->TryCommitViewportColumnLayoutLocally())
						owner->RequestLayout();
					owner = dynamic_cast<DataGrid*>(ownerLifetime.Get());
					if (!owner) return;
					if (horizontalChanged)
						owner->RefreshHorizontalScrollAlignment();
				}
			});
	}
	live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live) return;
	live->RefreshHeadersVisibility();
	live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (live) (void)live->RefreshRowHeaderActualWidth();
}

std::unique_ptr<DataGridColumnHeadersPresenter>
DataGrid::CreateColumnHeadersPresenter()
{
	auto presenter = std::make_unique<DataGridColumnHeadersPresenter>();
	std::wstring error;
	if (!presenter->Initialize(*this, &error))
	{
		SetLastTemplateError(error);
		return {};
	}
	_headersPresenter = ControlWeakReference(presenter.get());
	RefreshHeadersVisibility();
	return presenter;
}

std::unique_ptr<Control> DataGrid::BuildGeneratedItem(
	const BindingSourceReference& item,
	size_t index,
	BindingPathObservation& observation)
{
	observation = {};
	const ControlWeakReference ownerLifetime(this);
	auto row = std::make_unique<DataGridRow>();
	std::wstring error;
	if (!row->Initialize(*this, item, index, &error))
	{
		if (auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get()))
			live->SetLastTemplateError(std::move(error));
		return {};
	}
	if (!ownerLifetime.Get()) return {};
	return row;
}

bool DataGrid::CanRecycleGeneratedItemAcrossIndices(
	const Control& visual, size_t oldIndex) const noexcept
{
	const auto* row = dynamic_cast<const DataGridRow*>(&visual);
	auto* mutableRow = const_cast<DataGridRow*>(row);
	if (!row || row->GetDataGridOwner() != this
		|| row->ItemIndex() != oldIndex
		|| row->GetVisualParent() || row->GetLogicalParent()
		|| row->GetPresentationWindow()
		|| mutableRow->IsKeyboardFocusWithin
		|| mutableRow->IsMouseCaptureWithin)
		return false;
	if (_currentCell.IsValid() && _currentCell.RowIndex == oldIndex)
		return false;
	for (auto* cell : row->GetCells())
	{
		if (!cell || cell->GetRowOwner() != row || cell->GetIsEditing())
			return false;
		const size_t columnIndex = cell->_columnIndex;
		if (columnIndex >= _columns.size()
			|| _columns[columnIndex].get() != cell->_column)
			return false;
	}
	return true;
}

bool DataGrid::TryRebindGeneratedItemAcrossIndices(
	Control& visual,
	size_t oldIndex,
	size_t newIndex,
	const BindingSourceReference& item,
	BindingPathObservation& observation,
	std::wstring* outError)
{
	if (outError) outError->clear();
	auto* row = dynamic_cast<DataGridRow*>(&visual);
	if (!item || newIndex >= ItemCount() || oldIndex == newIndex
		|| !row || !CanRecycleGeneratedItemAcrossIndices(*row, oldIndex))
	{
		if (outError) *outError = L"DataGrid 回收行与目标索引不兼容。";
		return false;
	}

	const ControlWeakReference ownerLifetime(this);
	const ControlWeakReference rowLifetime(row);
	const auto items = GetItemsView();
	const size_t columnCount = _columns.size();
	const size_t columnRevision = _columnWidthProjectionRevision;
	const size_t cellSelectionRevision = _cellSelectionRevision;
	std::vector<DataGridColumn*> columns;
	columns.reserve(columnCount);
	for (const auto& column : _columns) columns.push_back(column.get());
	auto resolveCurrent = [&]() -> std::pair<DataGrid*, DataGridRow*>
	{
		auto* owner = dynamic_cast<DataGrid*>(ownerLifetime.Get());
		auto* liveRow = dynamic_cast<DataGridRow*>(rowLifetime.Get());
		if (!owner || !liveRow || liveRow->GetDataGridOwner() != owner
			|| owner->GetItemsView().Shared() != items.Shared()
			|| owner->_columns.size() != columnCount
			|| owner->_columnWidthProjectionRevision != columnRevision)
			return {};
		for (size_t index = 0; index < columnCount; ++index)
			if (owner->_columns[index].get() != columns[index]) return {};
		return { owner, liveRow };
	};

	BindingSourceReference currentItem;
	if (!items || !items.Get()->TryGetItem(newIndex, currentItem)
		|| currentItem.Shared() != item.Shared())
	{
		if (outError) *outError = L"DataGrid 回收行的目标项已变化。";
		return false;
	}
	auto [owner, liveRow] = resolveCurrent();
	if (!owner || !liveRow)
	{
		if (outError) *outError = L"DataGrid 回收期间列或数据源已变化。";
		return false;
	}

	// DataGrid does not install an outer item observation, but clear a future
	// derived observation before the visual starts representing a new occurrence.
	observation = {};
	liveRow->_item = item;
	liveRow->SetItemIndex(newIndex);
	(void)liveRow->SetDataContext(item);
	std::tie(owner, liveRow) = resolveCurrent();
	if (!owner || !liveRow)
	{
		if (outError) *outError = L"DataGrid 回收行 DataContext 更新失败。";
		return false;
	}
	if (!liveRow->GetIsSelected() && !liveRow->IsMouseOver)
		liveRow->SetBackground((newIndex % 2) != 0
			? owner->GetAlternatingRowBackground()
			: owner->GetRowBackground());
	else (void)liveRow->ClearPropertyValue(Control::BackgroundProperty());
	if (std::isfinite(owner->GetRowHeight()))
		liveRow->SetHeight(cui::layout::Length::Fixed(
			static_cast<float>(owner->GetRowHeight())));
	else liveRow->SetHeight(cui::layout::Length::Auto());
	const bool horizontalLines = HasHorizontalGridLines(
		owner->GetGridLinesVisibility());
	liveRow->SetBorderThickness(Thickness(
		0.0f, 0.0f, 0.0f, horizontalLines ? 1.0f : 0.0f));
	liveRow->SetBorderBrush(owner->GetHorizontalGridLinesBrush());

	std::tie(owner, liveRow) = resolveCurrent();
	if (!owner || !liveRow)
	{
		if (outError) *outError = L"DataGrid 回收行外观更新失败。";
		return false;
	}
	if (liveRow->_columnStorageIsSparse)
	{
		const auto range = owner->ResolveRealizedColumnRange();
		if ((range.first != liveRow->_realizedColumnBegin
			|| range.second != liveRow->_realizedColumnEnd)
			&& !liveRow->RefreshRealizedColumns(
				range.first, range.second, outError)) return false;
	}

	std::vector<ControlWeakReference> cells;
	cells.reserve(liveRow->_cells.size());
	for (auto* cell : liveRow->_cells)
		if (cell) cells.emplace_back(cell);
	for (const auto& cellLifetime : cells)
	{
		std::tie(owner, liveRow) = resolveCurrent();
		auto* cell = dynamic_cast<DataGridCell*>(cellLifetime.Get());
		if (!owner || !liveRow || !cell || cell->GetRowOwner() != liveRow
			|| cell->_columnIndex >= columnCount
			|| columns[cell->_columnIndex] != cell->_column)
		{
			if (outError) *outError = L"DataGrid 回收行单元格结构已变化。";
			return false;
		}
		cell->_item = item;
		(void)cell->SetDataContext(item);
		std::tie(owner, liveRow) = resolveCurrent();
		cell = dynamic_cast<DataGridCell*>(cellLifetime.Get());
		if (!owner || !liveRow || !cell || cell->GetRowOwner() != liveRow
			|| cell->_columnIndex >= columnCount
			|| columns[cell->_columnIndex] != cell->_column)
		{
			if (outError) *outError = L"DataGrid 回收行 DataContext 更新失败。";
			return false;
		}
		// Built-in bound columns bind through the element DataContext proxy, so
		// modes that observe that proxy can retain their content and Binding.
		// OneTime and OneWayToSource deliberately do not observe source retargets;
		// rebuilding preserves the same initialization semantics as a fresh row.
		// CheckBox display bindings are always forced OneWay by GenerateElement.
		const auto* textColumn =
			dynamic_cast<DataGridTextColumn*>(cell->_column);
		const BindingMode textMode = textColumn
			? textColumn->GetBindingMode() : BindingMode::Default;
		const bool retainsBoundContent =
			dynamic_cast<DataGridCheckBoxColumn*>(cell->_column)
			|| (textColumn && textMode != BindingMode::OneTime
				&& textMode != BindingMode::OneWayToSource);
		if (!retainsBoundContent
			&& !cell->ReplaceContent(false, outError)) return false;
		std::tie(owner, liveRow) = resolveCurrent();
		cell = dynamic_cast<DataGridCell*>(cellLifetime.Get());
		if (!owner || !liveRow || !cell || cell->GetRowOwner() != liveRow)
		{
			if (outError) *outError = L"DataGrid 回收行绑定更新失败。";
			return false;
		}
		cell->SetCurrentIsSelected(
			!owner->_selectedCells.empty()
			&& owner->IsCellSelected(newIndex, cell->_columnIndex), true);
	}

	std::tie(owner, liveRow) = resolveCurrent();
	if (!owner || !liveRow)
	{
		if (outError) *outError = L"DataGrid 回收行最终投影失败。";
		return false;
	}
	liveRow->UpdateColumnWidths();
	liveRow->UpdateRowHeader();
	liveRow->UpdateHorizontalScrollOffset(owner->_horizontalScrollOffset);
	if (owner->_cellSelectionRevision == cellSelectionRevision)
		liveRow->_appliedCellSelectionRevision = cellSelectionRevision;
	else liveRow->_appliedCellSelectionRevision = 0;
	return ownerLifetime.Get() != nullptr && rowLifetime.Get() != nullptr;
}

void DataGrid::OnBeforeGeneratedItemsPrepared()
{
	const bool preserveContentWidths =
		_preserveColumnContentWidthsDuringRowRebuild
		&& !IsItemsSourceUpdateInProgress();
	if (!preserveContentWidths)
		InvalidateColumnContentWidthCache();
	const size_t previousCount = _columns.size();
	EnsureAutoGeneratedColumns();
	if (_columns.size() != previousCount)
	{
		_autoColumnsChangedDuringPreparation = true;
		// Delayed runtime auto-generation is a real schema change even when it
		// happened to be discovered by an otherwise row-only rebuild.
		if (preserveContentWidths)
			InvalidateColumnContentWidthCache();
	}
}

float DataGrid::GetVirtualizedItemHeight() const noexcept
{
	if (std::isfinite(_rowHeight) && _rowHeight > 0.0)
		return static_cast<float>((std::min)(
			_rowHeight, static_cast<double>((std::numeric_limits<float>::max)())));
	return ItemsControl::GetVirtualizedItemHeight();
}

struct DataGrid::DataGridItemsSourceTransactionState final
	: ItemsSourceTransactionState
{
	std::unique_ptr<ItemsSourceTransactionState> SelectorState;
	size_t ColumnCount = 0;
	std::vector<std::optional<GridLength>> ResolvedColumnWidths;
	std::vector<double> ColumnWidthPrefix;
	std::vector<double> RuntimeColumnDesiredWidths;
	std::vector<double> RuntimeColumnDisplayWidths;
	std::vector<bool> RuntimeColumnDisplayOverrides;
	std::vector<std::optional<double>> SampledColumnContentWidths;
	size_t ColumnContentWidthCacheEpoch = 1;
	DataGridCellInfo CurrentCell;
	DataGridSelectedCellCollection SelectedCells;
	std::optional<DataGridCellInfo> SelectionAnchor;
	std::vector<DataGridCellInfo> SelectionRangeBase;
	std::vector<std::pair<BindingSourceReference, size_t>> SelectedRowSnapshot;
	SelectedIndexCollection SelectedRowIndexSnapshot;
	size_t RowSelectionRevision = 1;
	bool AutoColumnsChangedDuringPreparation = false;
	bool RuntimeAutoGenerationComplete = false;
	bool AutoGeneratedColumnsEventPending = false;
	bool CurrentCellChangeDeferred = false;
	DataGridCellInfo DeferredCurrentCellOld;
	bool SelectedCellsChangeDeferred = false;
	DataGridSelectedCellCollection DeferredSelectedCellsOld;
	bool DeferredSelectedCellsIgnoreLocators = false;
	bool SelectionRangeActive = false;
	AccessibilityIdentityState AccessibilityIdentities;
};

std::unique_ptr<ItemsControl::ItemsSourceTransactionState>
DataGrid::CaptureItemsSourceTransactionState()
{
	auto state = std::make_unique<DataGridItemsSourceTransactionState>();
	state->ColumnCount = _columns.size();
	state->ResolvedColumnWidths = _resolvedColumnWidths;
	state->ColumnWidthPrefix = _columnWidthPrefix;
	state->RuntimeColumnDesiredWidths.reserve(_columns.size());
	state->RuntimeColumnDisplayWidths.reserve(_columns.size());
	state->RuntimeColumnDisplayOverrides.reserve(_columns.size());
	for (const auto& column : _columns)
	{
		state->RuntimeColumnDesiredWidths.push_back(
			column->_runtimeWidth.Desired);
		state->RuntimeColumnDisplayWidths.push_back(
			column->_runtimeWidth.Display);
		state->RuntimeColumnDisplayOverrides.push_back(
			column->_runtimeWidth.HasDisplayOverride);
	}
	state->SampledColumnContentWidths = _sampledColumnContentWidths;
	state->ColumnContentWidthCacheEpoch = _columnContentWidthCacheEpoch;
	state->CurrentCell = _currentCell;
	state->SelectedCells = _selectedCells;
	state->SelectionAnchor = _selectionAnchor;
	state->SelectionRangeBase = _selectionRangeBase;
	state->SelectedRowSnapshot = _selectedRowSnapshot;
	state->SelectedRowIndexSnapshot = _selectedRowIndexSnapshot;
	state->RowSelectionRevision = _rowSelectionRevision;
	state->AutoColumnsChangedDuringPreparation =
		_autoColumnsChangedDuringPreparation;
	state->RuntimeAutoGenerationComplete = _runtimeAutoGenerationComplete;
	state->AutoGeneratedColumnsEventPending =
		_autoGeneratedColumnsEventPending;
	state->CurrentCellChangeDeferred = _currentCellChangeDeferred;
	state->DeferredCurrentCellOld = _deferredCurrentCellOld;
	state->SelectedCellsChangeDeferred = _selectedCellsChangeDeferred;
	state->DeferredSelectedCellsOld = _deferredSelectedCellsOld;
	state->DeferredSelectedCellsIgnoreLocators =
		_deferredSelectedCellsIgnoreLocators;
	state->SelectionRangeActive = _selectionRangeActive;
	state->AccessibilityIdentities = CaptureAccessibilityIdentityState();
	state->SelectorState = ListBox::CaptureItemsSourceTransactionState();
	return state;
}

void DataGrid::RestoreItemsSourceTransactionState(
	ItemsSourceTransactionState& state) noexcept
{
	auto* dataGrid = dynamic_cast<DataGridItemsSourceTransactionState*>(&state);
	if (!dataGrid) return;
	// Restore may destroy auto-generated candidate columns after a reentrant
	// UIA query published their raw pointers.  Invalidate before erasing them.
	InvalidateAccessibilityVirtualIdentities();
	if (dataGrid->SelectorState)
		ListBox::RestoreItemsSourceTransactionState(
			*dataGrid->SelectorState);
	const size_t originalLogicalCount = (std::min)(
		dataGrid->ColumnCount, _logicalColumns.size());
	const auto candidateLogicalBegin =
		_logicalColumns.begin() + originalLogicalCount;
	for (auto candidate = candidateLogicalBegin;
		candidate != _logicalColumns.end(); ++candidate)
	{
		if (!(*candidate)->_isRuntimeAutoGenerated) continue;
		(*candidate)->_owner = nullptr;
		(*candidate)->_displayIndex = DataGridColumn::UnsetDisplayIndex;
	}
	_logicalColumns.erase(std::remove_if(
		candidateLogicalBegin, _logicalColumns.end(),
		[](const DataGridColumn* column)
		{ return column->_owner == nullptr; }),
		_logicalColumns.end());
	for (size_t index = _columns.size(); index > 0;)
	{
		--index;
		auto* const candidate = _columns[index].get();
		if (candidate->_owner != nullptr
			|| !candidate->_isRuntimeAutoGenerated) continue;
		_columns.erase(_columns.begin() + index);
	}
	ReindexDisplayColumns();
	PruneAccessibilityColumnIdentities();
	_resolvedColumnWidths = std::move(dataGrid->ResolvedColumnWidths);
	_columnWidthPrefix = std::move(dataGrid->ColumnWidthPrefix);
	const size_t runtimeWidthCount = (std::min)({
		_columns.size(), dataGrid->RuntimeColumnDesiredWidths.size(),
		dataGrid->RuntimeColumnDisplayWidths.size(),
		dataGrid->RuntimeColumnDisplayOverrides.size() });
	for (size_t index = 0; index < runtimeWidthCount; ++index)
	{
		_columns[index]->_runtimeWidth.Desired =
			dataGrid->RuntimeColumnDesiredWidths[index];
		_columns[index]->_runtimeWidth.Display =
			dataGrid->RuntimeColumnDisplayWidths[index];
		_columns[index]->_runtimeWidth.HasDisplayOverride =
			dataGrid->RuntimeColumnDisplayOverrides[index];
	}
	_sampledColumnContentWidths =
		std::move(dataGrid->SampledColumnContentWidths);
	_columnContentWidthCacheEpoch =
		dataGrid->ColumnContentWidthCacheEpoch;
	_currentCell = dataGrid->CurrentCell;
	_selectedCells = std::move(dataGrid->SelectedCells);
	_selectedCells._ownerLifetime = ControlWeakReference(this);
	_selectedCells._ownerBound = true;
	_selectedCells._ownerSourceIdentity = _source;
	_selectedCells.InvalidateExcludedOffsets();
	_deferredSelectedCellsOld = std::move(
		dataGrid->DeferredSelectedCellsOld);
	_deferredSelectedCellsOld._ownerLifetime = ControlWeakReference(this);
	_deferredSelectedCellsOld._ownerBound = true;
	if (_deferredSelectedCellsOld._source)
		_deferredSelectedCellsOld._ownerSourceIdentity = _source;
	_deferredSelectedCellsOld.InvalidateExcludedOffsets();
	_selectionAnchor = std::move(dataGrid->SelectionAnchor);
	_selectionRangeBase = std::move(dataGrid->SelectionRangeBase);
	_selectedRowSnapshot = std::move(dataGrid->SelectedRowSnapshot);
	_selectedRowIndexSnapshot = std::move(
		dataGrid->SelectedRowIndexSnapshot);
	_rowSelectionRevision = dataGrid->RowSelectionRevision;
	// A rollback restores the logical cell selection, but readers which observed
	// the failed candidate must still see an identity boundary. This revision is
	// a clock, not transaction payload, so never move it backwards.
	if (++_cellSelectionRevision == 0) _cellSelectionRevision = 1;
	_selectedCellsVisualRefreshPending = true;
	_autoColumnsChangedDuringPreparation =
		dataGrid->AutoColumnsChangedDuringPreparation;
	_runtimeAutoGenerationComplete =
		dataGrid->RuntimeAutoGenerationComplete;
	_autoGeneratedColumnsEventPending =
		dataGrid->AutoGeneratedColumnsEventPending;
	_currentCellChangeDeferred = dataGrid->CurrentCellChangeDeferred;
	_deferredCurrentCellOld = dataGrid->DeferredCurrentCellOld;
	_selectedCellsChangeDeferred = dataGrid->SelectedCellsChangeDeferred;
	_deferredSelectedCellsIgnoreLocators =
		dataGrid->DeferredSelectedCellsIgnoreLocators;
	_selectionRangeActive = dataGrid->SelectionRangeActive;
	RestoreAccessibilityIdentityState(
		std::move(dataGrid->AccessibilityIdentities));
	if (!_settingItemsSource)
		RefreshSelectedCellContainersAfterRollback();
}

void DataGrid::OnItemsSourceTransactionCommitted()
{
	const ControlWeakReference ownerLifetime(this);
	ListBox::OnItemsSourceTransactionCommitted();
	auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live) return;
	if (live->_settingItemsSource) return;
	live->FlushCommittedItemsSourceState();
	if (auto* current = dynamic_cast<DataGrid*>(ownerLifetime.Get()))
		current->FlushAccessibilityStructureChange();
}

void DataGrid::OnItemsSourceCollectionChangePreparing(
	const CollectionChangedEventArgs& change,
	const BindingListReference& previousSnapshot)
{
	const auto previousRowShape = GetSelectedIndices();
	ListBox::OnItemsSourceCollectionChangePreparing(change, previousSnapshot);
	if (!(previousRowShape == GetSelectedIndices()))
		if (++_rowSelectionRevision == 0) _rowSelectionRevision = 1;
	const bool cellRegion = _selectionUnit == DataGridSelectionUnit::Cell;
	const bool fullRowRegion =
		_selectionUnit == DataGridSelectionUnit::FullRow
		&& (GetSelectedIndices().IsRangeBacked()
			|| _selectedRowIndexSnapshot.IsRangeBacked());
	if ((!cellRegion && !fullRowRegion)
		|| !_selectedCells.IsRegionBacked()
		|| _selectedCells._ownerSourceIdentity.Shared() != _source.Shared())
		return;
	const auto captureRowShape = [this, fullRowRegion]()
	{
		if (!fullRowRegion) return;
		_selectedRowIndexSnapshot = GetSelectedIndices();
		_selectedRowSnapshot.clear();
	};
	// Add allocates identities that do not exist in the immutable SelectAll
	// checkpoint, so the new rows are naturally unselected. Move/Swap preserve
	// occurrence identities and need only locator reconciliation after commit.
	if (change.Action == CollectionChangeAction::Add
		|| change.Action == CollectionChangeAction::Move
		|| change.Action == CollectionChangeAction::Swap)
	{
		captureRowShape();
		return;
	}
	if (change.Action == CollectionChangeAction::Reset)
	{
		std::vector<size_t> oldToNew;
		if (TryResolveSelectionOccurrencePermutation(
			previousSnapshot, oldToNew))
		{
			captureRowShape();
			return;
		}
		// WPF rebuilds cell selection from surviving full-row selector ranges on
		// a non-permutation Reset. Cell-only mode has no such ranges, so clear the
		// current domain. A sort/deferred Move retains physical occurrence tokens
		// and follows the fast return above; ReconcileSelectedCells remaps locators.
		// RaiseSelectedCellsChangedCore binds the resulting RemovedCells regions
		// to their old immutable snapshot, without N x C materialization.
		const ControlWeakReference ownerLifetime(this);
		if (!ApplySelectedCells({}) && ownerLifetime.Get())
			throw std::runtime_error(
				"DataGrid failed to prepare Reset cell selection");
		captureRowShape();
		return;
	}
	if (change.Action != CollectionChangeAction::Remove
		&& change.Action != CollectionChangeAction::Replace) return;
	const auto* oldIdentities = previousSnapshot
		? dynamic_cast<const IBindingListOccurrenceIdentity*>(
			previousSnapshot.Get()) : nullptr;
	if (!oldIdentities || change.OldIndex == CollectionChangedEventArgs::Npos
		|| change.OldCount == 0
		|| change.OldIndex > previousSnapshot.Get()->Count()
		|| change.OldCount > previousSnapshot.Get()->Count() - change.OldIndex)
		return;

	auto selected = _selectedCells;
	bool changed = false;
	// CollectionView pass-through snapshots preserve source occurrence tokens,
	// so the old live index range maps to a continuous immutable checkpoint
	// range in the common mutable-list case.  Confirm only the two boundaries;
	// Replace also keeps physical occurrence tokens by contract.  A future
	// third-party provider with non-contiguous tokens falls back to the exact
	// occurrence path below.
	const auto* snapshotLookup = selected._source
		? dynamic_cast<const IBindingListOccurrenceLookup*>(
			selected._source.Get()) : nullptr;
	bool intervalApplied = false;
	if (snapshotLookup && change.OldCount != 0)
	{
		size_t firstOccurrence = DataGridCellInfo::InvalidIndex;
		size_t lastOccurrence = DataGridCellInfo::InvalidIndex;
		size_t firstSnapshotRow = DataGridCellInfo::InvalidIndex;
		size_t lastSnapshotRow = DataGridCellInfo::InvalidIndex;
		const size_t lastOldRow = change.OldIndex + change.OldCount - 1;
		if (oldIdentities->TryGetItemOccurrenceIdentity(
				change.OldIndex, firstOccurrence)
			&& oldIdentities->TryGetItemOccurrenceIdentity(
				lastOldRow, lastOccurrence)
			&& snapshotLookup->TryGetItemIndexByOccurrenceIdentity(
				firstOccurrence, firstSnapshotRow)
			&& snapshotLookup->TryGetItemIndexByOccurrenceIdentity(
				lastOccurrence, lastSnapshotRow)
			&& firstSnapshotRow <= lastSnapshotRow
			&& lastSnapshotRow - firstSnapshotRow + 1 == change.OldCount)
		{
			for (auto& region : selected._regions)
			{
				const size_t regionEnd = region.Height
					> (std::numeric_limits<size_t>::max)() - region.Top
					? (std::numeric_limits<size_t>::max)()
					: region.Top + region.Height;
				const size_t start = (std::max)(
					region.Top, firstSnapshotRow);
				const size_t end = (std::min)(regionEnd,
					lastSnapshotRow == (std::numeric_limits<size_t>::max)()
						? lastSnapshotRow : lastSnapshotRow + 1);
				if (start >= end) continue;
				if (!region.IncludedRowOffsets.empty())
				{
					const auto included = std::lower_bound(
						region.IncludedRowOffsets.begin(),
						region.IncludedRowOffsets.end(), start - region.Top);
					if (included == region.IncludedRowOffsets.end()
						|| *included >= end - region.Top) continue;
				}
				DataGridSelectedCellCollection::CellRegion::
					ExcludedSnapshotRowInterval added{
					start, end - start };
				auto& intervals = region.ExcludedSnapshotRowIntervals;
				intervals.push_back(added);
				std::sort(intervals.begin(), intervals.end(),
					[](const auto& a, const auto& b)
					{ return a.Start < b.Start; });
				std::vector<DataGridSelectedCellCollection::CellRegion::
					ExcludedSnapshotRowInterval> merged;
				merged.reserve(intervals.size());
				for (const auto& interval : intervals)
				{
					if (interval.Count == 0) continue;
					if (!merged.empty()
						&& IntervalEndExclusive(merged.back())
							>= interval.Start)
					{
						auto& previous = merged.back();
						const size_t mergedEnd = (std::max)(
							IntervalEndExclusive(previous),
							IntervalEndExclusive(interval));
						previous.Count = mergedEnd - previous.Start;
					}
					else merged.push_back(interval);
				}
				intervals = std::move(merged);
				region.ExcludedRows.erase(std::remove_if(
					region.ExcludedRows.begin(), region.ExcludedRows.end(),
					[&](const auto& row)
					{
						return IsRowInIntervals(
							intervals, row.SnapshotRow);
					}), region.ExcludedRows.end());
				changed = true;
			}
			if (changed)
			{
				selected._cells.erase(std::remove_if(
					selected._cells.begin(), selected._cells.end(),
					[&](const DataGridCellInfo& cell)
					{
						size_t snapshotRow = 0;
						return snapshotLookup->
							TryGetItemIndexByOccurrenceIdentity(
								cell._itemOccurrence, snapshotRow)
							&& snapshotRow >= firstSnapshotRow
							&& snapshotRow <= lastSnapshotRow;
					}), selected._cells.end());
				selected._excludedCells.erase(std::remove_if(
					selected._excludedCells.begin(),
					selected._excludedCells.end(),
					[&](const DataGridCellInfo& cell)
					{
						size_t snapshotRow = 0;
						return snapshotLookup->
							TryGetItemIndexByOccurrenceIdentity(
								cell._itemOccurrence, snapshotRow)
							&& snapshotRow >= firstSnapshotRow
							&& snapshotRow <= lastSnapshotRow;
					}), selected._excludedCells.end());
				intervalApplied = true;
			}
		}
	}
	if (!intervalApplied)
	for (size_t offset = 0; offset < change.OldCount; ++offset)
	{
		bool changedOccurrence = false;
		const size_t oldRow = change.OldIndex + offset;
		BindingSourceReference oldItem;
		size_t occurrence = DataGridCellInfo::InvalidIndex;
		if (!previousSnapshot.Get()->TryGetItem(oldRow, oldItem) || !oldItem
			|| !oldIdentities->TryGetItemOccurrenceIdentity(
				oldRow, occurrence)
			|| occurrence == DataGridCellInfo::InvalidIndex) continue;
		for (auto& region : selected._regions)
		{
			const auto* lookup = selected._source
				? dynamic_cast<const IBindingListOccurrenceLookup*>(
					selected._source.Get()) : nullptr;
			if (!lookup) continue;
			size_t snapshotRow = 0;
			if (!lookup->TryGetItemIndexByOccurrenceIdentity(
					occurrence, snapshotRow)
				|| snapshotRow < region.Top
				|| snapshotRow - region.Top >= region.Height) continue;
			BindingSourceReference snapshotItem;
			if (!selected._source.Get()->TryGetItem(
					snapshotRow, snapshotItem)
				|| !snapshotItem
				|| snapshotItem.Shared() != oldItem.Shared()) continue;
			const size_t rowOffset = snapshotRow - region.Top;
			if (!region.IncludedRowOffsets.empty()
				&& !std::binary_search(region.IncludedRowOffsets.begin(),
					region.IncludedRowOffsets.end(), rowOffset)) continue;
			if (std::any_of(region.ExcludedRows.begin(),
				region.ExcludedRows.end(), [occurrence](const auto& row)
				{ return row.Occurrence == occurrence; })) continue;
			region.ExcludedRows.push_back({
				oldItem, occurrence, snapshotRow });
			changedOccurrence = true;
		}
		const auto cellsEnd = std::remove_if(
			selected._cells.begin(), selected._cells.end(),
			[occurrence](const DataGridCellInfo& cell)
			{ return cell._itemOccurrence == occurrence; });
		if (cellsEnd != selected._cells.end())
		{
			selected._cells.erase(cellsEnd, selected._cells.end());
			changedOccurrence = true;
		}
		if (changedOccurrence)
		{
			selected._excludedCells.erase(std::remove_if(
				selected._excludedCells.begin(), selected._excludedCells.end(),
				[occurrence](const DataGridCellInfo& cell)
				{ return cell._itemOccurrence == occurrence; }),
				selected._excludedCells.end());
		}
		changed = changed || changedOccurrence;
	}
	if (!changed)
	{
		captureRowShape();
		return;
	}
	for (auto& region : selected._regions)
		std::sort(region.ExcludedRows.begin(), region.ExcludedRows.end(),
			[](const auto& left, const auto& right)
			{ return left.SnapshotRow < right.SnapshotRow; });
	selected.InvalidateExcludedOffsets();
	const ControlWeakReference ownerLifetime(this);
	if (!ApplySelectedCellCollection(std::move(selected))
		&& ownerLifetime.Get())
		throw std::runtime_error(
			"DataGrid failed to prepare mutable cell selection");
	if (ownerLifetime.Get()) captureRowShape();
}

void DataGrid::OnItemsSourceCollectionChangeCommitted(
	const CollectionChangedEventArgs& change)
{
	if (!std::isnan(_rowHeaderWidth)) return;
	const ControlWeakReference ownerLifetime(this);
	if (change.Action == CollectionChangeAction::Reset)
	{
		InvalidateRowHeaderWidthBaseline();
		(void)SetCurrentRowHeaderActualWidth(0.0);
	}
	auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (live) (void)live->RefreshRowHeaderActualWidth();
}

void DataGrid::OnItemsSourceChanged(
	const BindingListReference& oldValue,
	const BindingListReference& newValue)
{
	Selector::OnItemsSourceChanged(oldValue, newValue);
	if (_settingItemsSource || !_itemsView
		|| oldValue.Shared() != _itemsView
		|| newValue.Shared() == _itemsView) return;
	// ItemsControl pins a failed live mutation to its last materialized
	// snapshot. Publish that same source here and release the detached live view
	// so rows, CurrentCell and width measurement consume one projection.
	_source = newValue;
	if (++_cellSelectionRevision == 0) _cellSelectionRevision = 1;
	_itemsView.reset();
	_rowHeaderSourceChanged.Disconnect();
	InvalidateColumnContentWidthCache();
}

DataGridRow* DataGrid::ResolveRow(size_t rowIndex) const noexcept
{
	return dynamic_cast<DataGridRow*>(GetGeneratedItem(rowIndex));
}

DataGridCell* DataGrid::ResolveCurrentCellContainer() const noexcept
{
	if (!_currentCell.IsValid()) return nullptr;
	auto* row = ResolveRow(_currentCell.RowIndex);
	return row ? row->GetCell(_currentCell.ColumnIndex) : nullptr;
}

bool DataGrid::TryCreateCellInfo(
	size_t rowIndex, size_t columnIndex, DataGridCellInfo& result) const
{
	result = {};
	const ControlWeakReference ownerLifetime(
		const_cast<DataGrid*>(this));
	const auto items = GetItemsView();
	if (!items) return false;
	const size_t count = items.Get()->Count();
	auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live || live->GetItemsView().Shared() != items.Shared()
		|| rowIndex >= count || columnIndex >= live->_columns.size())
		return false;
	auto* const column = live->_columns[columnIndex].get();
	const uint32_t columnIdentity = column->_accessibilityIdentity;
	const auto resolveCurrent = [&]() -> DataGrid*
	{
		auto* owner = dynamic_cast<DataGrid*>(ownerLifetime.Get());
		return owner && owner->GetItemsView().Shared() == items.Shared()
			&& columnIndex < owner->_columns.size()
			&& owner->_columns[columnIndex].get() == column
			&& owner->_columns[columnIndex]->_accessibilityIdentity
				== columnIdentity
			? owner : nullptr;
	};
	size_t occurrence = DataGridCellInfo::InvalidIndex;
	if (!live->TryGetItemOccurrenceAt(rowIndex, occurrence)) return false;
	live = resolveCurrent();
	if (!live) return false;
	BindingSourceReference item;
	const bool read = items.Get()->TryGetItem(rowIndex, item);
	live = resolveCurrent();
	if (!live || !read || !item) return false;
	result.Item = std::move(item);
	result.Column = column;
	result.RowIndex = rowIndex;
	result.ColumnIndex = columnIndex;
	result._itemOccurrence = occurrence;
	return true;
}

bool DataGrid::TryGetItemOccurrenceAt(
	size_t rowIndex, size_t& occurrence) const
{
	occurrence = DataGridCellInfo::InvalidIndex;
	const ControlWeakReference ownerLifetime(
		const_cast<DataGrid*>(this));
	const auto items = GetItemsView();
	if (!items) return false;
	const size_t count = items.Get()->Count();
	auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live || live->GetItemsView().Shared() != items.Shared()
		|| rowIndex >= count) return false;
	if (const auto* identities =
		dynamic_cast<const IBindingListOccurrenceIdentity*>(items.Get()))
	{
		const bool read = identities->TryGetItemOccurrenceIdentity(
			rowIndex, occurrence);
		live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
		return read && occurrence != DataGridCellInfo::InvalidIndex
			&& live && live->GetItemsView().Shared() == items.Shared();
	}

	// Third-party lists that do not expose physical occurrence identities keep
	// the historical same-object ordinal fallback.  This is intentionally local
	// to the requested row: the normal CollectionViewSource path is O(1), while
	// selecting one row in a million-row view must never allocate an N-entry
	// occurrence cache merely to identify that row.
	BindingSourceReference target;
	if (!items.Get()->TryGetItem(rowIndex, target) || !target) return false;
	live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live || live->GetItemsView().Shared() != items.Shared()) return false;
	size_t ordinal = 0;
	for (size_t index = 0; index < rowIndex; ++index)
	{
		BindingSourceReference candidate;
		const bool read = items.Get()->TryGetItem(index, candidate);
		live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
		if (!live || live->GetItemsView().Shared() != items.Shared())
			return false;
		if (read && candidate
			&& candidate.Shared() == target.Shared()) ++ordinal;
	}
	occurrence = ordinal;
	return true;
}

bool DataGrid::TryGetStableSelectedCellRegionSnapshot(
	BindingListReference& snapshot) const
{
	snapshot = {};
	const auto items = GetItemsView();
	if (!items) return false;
	if (dynamic_cast<const IBindingListStableSnapshot*>(items.Get()))
		snapshot = items;
	else if (const auto* provider =
		dynamic_cast<const IBindingListSnapshotProvider*>(items.Get()))
	{
		if (!provider->TryGetStableSnapshot(snapshot)) return false;
	}
	if (!snapshot
		|| !dynamic_cast<const IBindingListStableSnapshot*>(snapshot.Get())
		|| !dynamic_cast<const IBindingListOccurrenceIdentity*>(snapshot.Get())
		|| !dynamic_cast<const IBindingListOccurrenceLookup*>(snapshot.Get())
		|| !dynamic_cast<const IBindingListOccurrenceIdentity*>(items.Get())
		|| !dynamic_cast<const IBindingListOccurrenceLookup*>(items.Get())
		|| snapshot.Get()->Count() != items.Get()->Count())
	{
		snapshot = {};
		return false;
	}
	return true;
}

bool DataGrid::TryResolveItemOccurrence(
	const BindingSourceReference& item,
	size_t occurrence,
	size_t& rowIndex) const
{
	rowIndex = DataGridCellInfo::InvalidIndex;
	const ControlWeakReference ownerLifetime(
		const_cast<DataGrid*>(this));
	const auto items = GetItemsView();
	if (!item || !items) return false;
	auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live || live->GetItemsView().Shared() != items.Shared()) return false;
	const size_t count = items.Get()->Count();
	live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live || live->GetItemsView().Shared() != items.Shared()) return false;
	if (const auto* lookup =
		dynamic_cast<const IBindingListOccurrenceLookup*>(items.Get()))
	{
		size_t candidateIndex = DataGridCellInfo::InvalidIndex;
		const bool resolved = lookup->TryGetItemIndexByOccurrenceIdentity(
			occurrence, candidateIndex);
		live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
		if (!live || live->GetItemsView().Shared() != items.Shared())
			return false;
		if (resolved && candidateIndex < count)
		{
			BindingSourceReference candidate;
			const bool read = items.Get()->TryGetItem(
				candidateIndex, candidate);
			live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
			if (!live || live->GetItemsView().Shared() != items.Shared())
				return false;
			size_t candidateOccurrence = DataGridCellInfo::InvalidIndex;
			if (read && candidate
				&& candidate.Shared() == item.Shared()
				&& live->TryGetItemOccurrenceAt(
					candidateIndex, candidateOccurrence)
				&& candidateOccurrence == occurrence)
			{
				rowIndex = candidateIndex;
				return true;
			}
		}
	}
	for (size_t index = 0; index < count; ++index)
	{
		BindingSourceReference candidate;
		const bool read = items.Get()->TryGetItem(index, candidate);
		live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
		if (!live || live->GetItemsView().Shared() != items.Shared())
			return false;
		if (!read || !candidate || candidate.Shared() != item.Shared())
			continue;
		size_t candidateOccurrence = DataGridCellInfo::InvalidIndex;
		if (!live->TryGetItemOccurrenceAt(index, candidateOccurrence))
			return false;
		if (candidateOccurrence != occurrence) continue;
		rowIndex = index;
		return true;
	}
	return false;
}

void DataGrid::InvalidateItemOccurrenceCache() const noexcept
{
	InvalidateAccessibilityVirtualIdentities();
}

DataGrid::AccessibilityIdentityState
DataGrid::CaptureAccessibilityIdentityState() const
{
	AccessibilityIdentityState state;
	state.Columns = _accessibilityColumns;
	state.Rows = _accessibilityRows;
	// A callback in the candidate transaction may legally remove authored
	// columns.  Preserve only stable identity seeds; never retain column pointers
	// or a locator table that could refer to objects destroyed by that callback.
	for (auto& column : state.Columns) column.Column = nullptr;
	for (auto& row : state.Rows)
		for (auto& cell : row.Cells) cell.Column = nullptr;
	state.StructureChangePending = _accessibilityStructureChangePending;
	return state;
}

void DataGrid::RestoreAccessibilityIdentityState(
	AccessibilityIdentityState&& state) noexcept
{
	std::vector<AccessibilityColumnIdentity> retiredColumns;
	std::vector<AccessibilityRowIdentity> retiredRows;
	std::unordered_map<uint32_t, AccessibilityNodeLocator> retiredLookup;
	retiredColumns.swap(_accessibilityColumns);
	retiredRows.swap(_accessibilityRows);
	retiredLookup.swap(_accessibilityNodeLookup);
	_accessibilityRowIndexLookup.clear();
	_accessibilityColumns.swap(state.Columns);
	_accessibilityRows.swap(state.Rows);
	_accessibilityIdentitiesDirty = true;
	++_accessibilityIdentityRevision;
	if (_accessibilityIdentityRevision == 0)
		++_accessibilityIdentityRevision;
	_accessibilityStructureChangePending = state.StructureChangePending;
	// Retired candidate item references are released only after the live object
	// has a complete, dirty seed projection.  A destructor that re-enters UIA
	// therefore observes a rebuildable state rather than a half-restored table.
}

void DataGrid::RequestAccessibilityStructureChanged()
{
	if (_settingItemsSource || IsItemsSourceUpdateInProgress())
	{
		_accessibilityStructureChangePending = true;
		return;
	}
	NotifyAccessibilityStructureChanged();
}

void DataGrid::FlushAccessibilityStructureChange()
{
	if (!_accessibilityStructureChangePending
		|| _settingItemsSource || IsItemsSourceUpdateInProgress()) return;
	_accessibilityStructureChangePending = false;
	NotifyAccessibilityStructureChanged();
}

void DataGrid::InvalidateAccessibilityVirtualIdentities() const noexcept
{
	_accessibilityIdentitiesDirty = true;
	++_accessibilityIdentityRevision;
	if (_accessibilityIdentityRevision == 0)
		++_accessibilityIdentityRevision;
}

void DataGrid::PruneAccessibilityColumnIdentities() noexcept
{
	const auto isLive = [this](uint32_t identity)
	{
		return identity != 0 && std::any_of(
			_columns.begin(), _columns.end(),
			[identity](const auto& column)
			{ return column->_accessibilityIdentity == identity; });
	};
	std::erase_if(_accessibilityColumns,
		[&isLive](const AccessibilityColumnIdentity& column)
		{ return !isLive(column.ColumnIdentity); });
	for (auto& row : _accessibilityRows)
		std::erase_if(row.Cells,
			[&isLive](const AccessibilityCellIdentity& cell)
			{ return !isLive(cell.ColumnIdentity); });
	InvalidateAccessibilityVirtualIdentities();
}

bool DataGrid::EnsureAccessibilityVirtualIdentities() const
{
	if (!_accessibilityIdentitiesDirty) return true;
	const ControlWeakReference ownerLifetime(
		const_cast<DataGrid*>(this));
	auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live || live->_buildingAccessibilityIdentities) return false;
	live->_buildingAccessibilityIdentities = true;
	auto resetBuilding = MakeScopeExit([ownerLifetime]
	{
		if (auto* owner = dynamic_cast<DataGrid*>(ownerLifetime.Get()))
			owner->_buildingAccessibilityIdentities = false;
	});
	const auto items = GetItemsView();
	if (!items)
	{
		live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
		if (!live) return false;
		live->_accessibilityColumns.clear();
		live->_accessibilityRows.clear();
		live->_accessibilityRowIndexLookup.clear();
		live->_accessibilityNodeLookup.clear();
		live->_accessibilityIdentitiesDirty = false;
		return true;
	}
	const size_t revision = live->_accessibilityIdentityRevision;

	const auto oldColumns = live->_accessibilityColumns;
	const auto oldRows = live->_accessibilityRows;
	std::unordered_map<uint32_t, uint32_t> oldHeaderIds;
	oldHeaderIds.reserve(oldColumns.size());
	for (const auto& entry : oldColumns)
		oldHeaderIds.emplace(entry.ColumnIdentity, entry.HeaderId);

	std::vector<AccessibilityColumnIdentity> columns;
	columns.reserve(live->_columns.size());
	for (const auto& column : live->_columns)
	{
		if (column->_accessibilityIdentity == 0)
			column->_accessibilityIdentity = AllocateAccessibilityVirtualId();
		const auto found = oldHeaderIds.find(column->_accessibilityIdentity);
		columns.push_back({ column.get(), column->_accessibilityIdentity,
			found != oldHeaderIds.end()
				? found->second : AllocateAccessibilityVirtualId() });
	}

	std::vector<AccessibilityRowIdentity> rows;
	rows.reserve(oldRows.size());
	for (auto next : oldRows)
	{
		if (!next.Item
			|| next.Occurrence == DataGridCellInfo::InvalidIndex) continue;
		size_t rowIndex = DataGridCellInfo::InvalidIndex;
		if (!live->TryResolveItemOccurrence(
			next.Item, next.Occurrence, rowIndex))
		{
			live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
			if (!live || live->GetItemsView().Shared() != items.Shared())
				return false;
			continue;
		}
		live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
		if (!live || live->GetItemsView().Shared() != items.Shared())
			return false;
		next.ViewIndex = rowIndex;
		if (next.RowId == 0) next.RowId = AllocateAccessibilityVirtualId();
		if (next.HeaderId == 0)
			next.HeaderId = AllocateAccessibilityVirtualId();
		const auto oldCells = std::move(next.Cells);
		next.Cells.clear();
		next.Cells.reserve(oldCells.size());
		for (auto cell : oldCells)
		{
			const auto column = std::find_if(
				columns.begin(), columns.end(),
				[&cell](const AccessibilityColumnIdentity& value)
				{ return value.ColumnIdentity == cell.ColumnIdentity; });
			if (column == columns.end() || cell.Id == 0) continue;
			cell.Column = column->Column;
			cell.ColumnIndex = static_cast<size_t>(
				std::distance(columns.begin(), column));
			next.Cells.push_back(std::move(cell));
		}
		rows.push_back(std::move(next));
	}

	std::unordered_map<uint32_t, AccessibilityNodeLocator> lookup;
	size_t sparseCellCount = 0;
	for (const auto& row : rows) sparseCellCount += row.Cells.size();
	lookup.reserve(columns.size() + rows.size() * 2 + sparseCellCount);
	std::unordered_map<size_t, size_t> rowLookup;
	rowLookup.reserve(rows.size());
	for (size_t column = 0; column < columns.size(); ++column)
		lookup.emplace(columns[column].HeaderId,
			AccessibilityNodeLocator{
				AccessibilityNodeKind::ColumnHeader,
				DataGridCellInfo::InvalidIndex, column });
	for (size_t row = 0; row < rows.size(); ++row)
	{
		if (!rowLookup.emplace(rows[row].ViewIndex, row).second)
			return false;
		lookup.emplace(rows[row].RowId,
			AccessibilityNodeLocator{
				AccessibilityNodeKind::Row, rows[row].ViewIndex,
				DataGridCellInfo::InvalidIndex });
		lookup.emplace(rows[row].HeaderId,
			AccessibilityNodeLocator{
				AccessibilityNodeKind::RowHeader, rows[row].ViewIndex,
				DataGridCellInfo::InvalidIndex });
		for (const auto& cell : rows[row].Cells)
			lookup.emplace(cell.Id,
				AccessibilityNodeLocator{
					AccessibilityNodeKind::Cell,
					rows[row].ViewIndex, cell.ColumnIndex });
	}

	live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live || live->GetItemsView().Shared() != items.Shared()
		|| live->_accessibilityIdentityRevision != revision
		|| live->_columns.size() != columns.size()) return false;
	for (size_t index = 0; index < columns.size(); ++index)
	{
		if (live->_columns[index].get() != columns[index].Column
			|| live->_columns[index]->_accessibilityIdentity
				!= columns[index].ColumnIdentity) return false;
	}
	live->_accessibilityColumns = std::move(columns);
	live->_accessibilityRows = std::move(rows);
	live->_accessibilityRowIndexLookup = std::move(rowLookup);
	live->_accessibilityNodeLookup = std::move(lookup);
	live->_accessibilityIdentitiesDirty = false;
	return true;
}

DataGrid::AccessibilityRowIdentity*
DataGrid::FindAccessibilityRowIdentity(size_t rowIndex) const noexcept
{
	const auto found = _accessibilityRowIndexLookup.find(rowIndex);
	if (found == _accessibilityRowIndexLookup.end()
		|| found->second >= _accessibilityRows.size()) return nullptr;
	auto& row = _accessibilityRows[found->second];
	return row.ViewIndex == rowIndex ? &row : nullptr;
}

DataGrid::AccessibilityRowIdentity*
DataGrid::EnsureAccessibilityRowIdentity(
	size_t rowIndex, bool ensureCells) const
{
	if (!EnsureAccessibilityVirtualIdentities()) return nullptr;
	const ControlWeakReference ownerLifetime(
		const_cast<DataGrid*>(this));
	auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	// A clean sparse identity table already proves this row belongs to the same
	// projection revision. Return it before invoking Count on a potentially lazy
	// million-row source; callers commonly ask for the row immediately after the
	// matching cell identity was created.
	if (live)
		if (auto* existing = live->FindAccessibilityRowIdentity(rowIndex))
		{
			(void)ensureCells;
			live->_accessibilityNodeLookup[existing->RowId] = {
				AccessibilityNodeKind::Row, rowIndex,
				DataGridCellInfo::InvalidIndex };
			live->_accessibilityNodeLookup[existing->HeaderId] = {
				AccessibilityNodeKind::RowHeader, rowIndex,
				DataGridCellInfo::InvalidIndex };
			for (const auto& cell : existing->Cells)
				live->_accessibilityNodeLookup[cell.Id] = {
					AccessibilityNodeKind::Cell, rowIndex, cell.ColumnIndex };
			return existing;
		}
	const auto items = live ? live->GetItemsView() : BindingListReference{};
	if (!live || !items) return nullptr;
	const size_t count = items.Get()->Count();
	live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live || live->GetItemsView().Shared() != items.Shared()
		|| rowIndex >= count) return nullptr;

	BindingSourceReference item;
	const bool read = items.Get()->TryGetItem(rowIndex, item);
	live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live || live->GetItemsView().Shared() != items.Shared()
		|| !read || !item) return nullptr;
	size_t occurrence = DataGridCellInfo::InvalidIndex;
	if (!live->TryGetItemOccurrenceAt(rowIndex, occurrence)) return nullptr;
	live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live || live->GetItemsView().Shared() != items.Shared()) return nullptr;

	AccessibilityRowIdentity next;
	next.Item = std::move(item);
	next.Occurrence = occurrence;
	next.ViewIndex = rowIndex;
	next.RowId = AllocateAccessibilityVirtualId();
	next.HeaderId = AllocateAccessibilityVirtualId();
	(void)ensureCells;
	const size_t sparseIndex = live->_accessibilityRows.size();
	live->_accessibilityRows.push_back(std::move(next));
	live->_accessibilityRowIndexLookup[rowIndex] = sparseIndex;
	auto* inserted = &live->_accessibilityRows.back();
	live->_accessibilityNodeLookup[inserted->RowId] = {
		AccessibilityNodeKind::Row, rowIndex,
		DataGridCellInfo::InvalidIndex };
	live->_accessibilityNodeLookup[inserted->HeaderId] = {
		AccessibilityNodeKind::RowHeader, rowIndex,
		DataGridCellInfo::InvalidIndex };
	for (const auto& cell : inserted->Cells)
		live->_accessibilityNodeLookup[cell.Id] = {
			AccessibilityNodeKind::Cell, rowIndex, cell.ColumnIndex };
	return inserted;
}

DataGrid::AccessibilityCellIdentity*
DataGrid::EnsureAccessibilityCellIdentity(
	size_t rowIndex, size_t columnIndex) const
{
	if (!EnsureAccessibilityVirtualIdentities()
		|| columnIndex >= _accessibilityColumns.size()) return nullptr;
	const ControlWeakReference ownerLifetime(
		const_cast<DataGrid*>(this));
	auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	auto* row = live
		? live->EnsureAccessibilityRowIdentity(rowIndex, false) : nullptr;
	if (!live || !row || columnIndex >= live->_accessibilityColumns.size())
		return nullptr;
	const auto& column = live->_accessibilityColumns[columnIndex];
	auto existing = std::find_if(
		row->Cells.begin(), row->Cells.end(),
		[&column](const AccessibilityCellIdentity& candidate)
		{ return candidate.ColumnIdentity == column.ColumnIdentity; });
	if (existing != row->Cells.end())
	{
		existing->Column = column.Column;
		existing->ColumnIndex = columnIndex;
		live->_accessibilityNodeLookup[existing->Id] = {
			AccessibilityNodeKind::Cell, rowIndex, columnIndex };
		return &*existing;
	}
	row->Cells.push_back({ column.Column, column.ColumnIdentity,
		columnIndex, AllocateAccessibilityVirtualId() });
	auto* inserted = &row->Cells.back();
	live->_accessibilityNodeLookup[inserted->Id] = {
		AccessibilityNodeKind::Cell, rowIndex, columnIndex };
	return inserted;
}

bool DataGrid::TryResolveAccessibilityVirtualNode(
	uint32_t id, AccessibilityNodeLocator& result) const
{
	result = {};
	if (id == 0 || !EnsureAccessibilityVirtualIdentities()) return false;
	const auto found = _accessibilityNodeLookup.find(id);
	if (found == _accessibilityNodeLookup.end()) return false;
	result = found->second;
	return true;
}

bool DataGrid::TryReadAccessibilityCellValue(
	size_t rowIndex, size_t columnIndex, std::wstring& result) const
{
	result.clear();
	const ControlWeakReference ownerLifetime(
		const_cast<DataGrid*>(this));
	const auto items = GetItemsView();
	if (!items) return false;
	const size_t count = items.Get()->Count();
	auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live || live->GetItemsView().Shared() != items.Shared()
		|| rowIndex >= count || columnIndex >= live->_columns.size())
		return false;
	const uint32_t columnIdentity =
		live->_columns[columnIndex]->_accessibilityIdentity;
#if CUI_ENABLE_DYNAMIC_XAML
	std::wstring bindingPath;
#endif
	CompiledBindingPathView compiledPath;
	if (const auto* bound = dynamic_cast<const DataGridBoundColumn*>(
		live->_columns[columnIndex].get()))
	{
#if CUI_ENABLE_DYNAMIC_XAML
		bindingPath = bound->GetBindingPath();
#endif
		compiledPath = bound->GetCompiledBindingPath();
	}
	const auto isCurrent = [&]()
	{
		auto* owner = dynamic_cast<DataGrid*>(ownerLifetime.Get());
		return owner && owner->GetItemsView().Shared() == items.Shared()
			&& columnIndex < owner->_columns.size()
			&& owner->_columns[columnIndex]->_accessibilityIdentity
				== columnIdentity;
	};
	BindingSourceReference item;
	const bool gotItem = items.Get()->TryGetItem(rowIndex, item);
	if (!isCurrent() || !gotItem || !item) return false;
	BindingValue value;
	bool read = false;
#if CUI_ENABLE_DYNAMIC_XAML
	if (!bindingPath.empty())
		read = TryGetBindingPathValue(
			*item.Get(), bindingPath, value);
#endif
	if (!isCurrent()) return false;
	if (!read && !compiledPath.Empty())
		read = TryGetBindingPathValue(
			*item.Get(), compiledPath, value);
	if (!isCurrent()) return false;
	if (read)
	{
		result = value.ToString();
		return true;
	}
	live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (live && columnIndex < live->_columns.size()
		&& dynamic_cast<DataGridTemplateColumn*>(
			live->_columns[columnIndex].get()))
		return false;
	auto* row = live ? live->ResolveRow(rowIndex) : nullptr;
	auto* cell = row ? row->GetCell(columnIndex) : nullptr;
	if (!cell) return false;
	const ControlWeakReference cellLifetime(cell);
	const auto snapshot = cell->GetAccessibilitySnapshot();
	if (!isCurrent() || !cellLifetime.Get()) return false;
	result = snapshot.Value.empty() ? snapshot.Name : snapshot.Value;
	return !result.empty();
}

bool DataGrid::TryGetAccessibilityVirtualNode(
	uint32_t id, AccessibilityVirtualNode& result)
{
	result = {};
	AccessibilityNodeLocator locator;
	if (!TryResolveAccessibilityVirtualNode(id, locator)) return false;
	if (locator.RowIndex != DataGridCellInfo::InvalidIndex
		&& locator.RowIndex >= ItemCount()) return false;
	if (locator.ColumnIndex != DataGridCellInfo::InvalidIndex
		&& locator.ColumnIndex >= _accessibilityColumns.size()) return false;

	const ControlWeakReference ownerLifetime(this);
	const auto gridSnapshot = GetAccessibilitySnapshot();
	auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	AccessibilityNodeLocator currentLocator;
	if (!live || !live->TryResolveAccessibilityVirtualNode(id, currentLocator))
		return false;
	locator = currentLocator;
	if (locator.RowIndex != DataGridCellInfo::InvalidIndex
		&& locator.RowIndex >= live->ItemCount()) return false;
	if (locator.ColumnIndex != DataGridCellInfo::InvalidIndex
		&& locator.ColumnIndex >= live->_accessibilityColumns.size()) return false;
	const std::wstring prefix = gridSnapshot.AutomationId.empty()
		? L"dataGrid" : gridSnapshot.AutomationId;
	result.Id = id;
	result.Enabled = live->IsEffectivelyEnabled();
	result.Visible = live->GetIsVisible();
	result.ReadOnly = true;
	result.RowSpan = 1;
	result.ColumnSpan = 1;

	switch (locator.Kind)
	{
	case AccessibilityNodeKind::ColumnHeader:
	{
		auto* column = _accessibilityColumns[locator.ColumnIndex].Column;
		if (!column) return false;
		result.ControlType = AutomationControlType::HeaderItem;
		result.ClassName = L"DataGridColumnHeader";
		result.KeyboardFocusable = false;
		result.HasKeyboardFocus = false;
		result.IsControlElement = true;
		result.IsContentElement = false;
		result.Patterns = AutomationPattern::ScrollItem
			| AutomationPattern::VirtualizedItem;
		if (_canUserSortColumns && column->GetCanUserSort())
			result.Patterns |= AutomationPattern::Invoke;
		result.Name = column->GetHeader().ToString();
		result.AutomationId = prefix + L".column-"
			+ std::to_wstring(id);
		result.Column = static_cast<int>(locator.ColumnIndex);
		auto* presenter = GetColumnHeadersPresenter();
		auto* header = presenter
			? presenter->GetHeader(locator.ColumnIndex) : nullptr;
		(void)PublishAccessibilityBounds(header, result);
		result.Visible = result.Visible
			&& HasColumnHeaders(_headersVisibility);
		return true;
	}
	case AccessibilityNodeKind::Row:
	{
		auto* rowIdentity = live->EnsureAccessibilityRowIdentity(
			locator.RowIndex, false);
		if (!rowIdentity) return false;
		const uint32_t rowId = rowIdentity->RowId;
		result.ControlType = AutomationControlType::DataItem;
		result.ClassName = L"DataGridRow";
		result.KeyboardFocusable = false;
		result.HasKeyboardFocus = false;
		result.IsControlElement = true;
		result.IsContentElement = true;
		result.SelectionReturnsNullWhenEmpty = true;
		result.Patterns = AutomationPattern::ScrollItem
			| AutomationPattern::Selection;
		if (!_isReadOnly && !_columns.empty())
			result.Patterns |= AutomationPattern::Invoke;
		if (_selectionUnit != DataGridSelectionUnit::Cell)
			result.Patterns |= AutomationPattern::SelectionItem;
		result.AutomationId = prefix + L".row-" + std::to_wstring(id);
		result.Row = static_cast<int>(locator.RowIndex);
		result.Selected = IsIndexSelected(locator.RowIndex);
		std::wstring value;
		if (!_columns.empty())
			(void)TryReadAccessibilityCellValue(
				locator.RowIndex, 0, value);
		AccessibilityNodeLocator currentLocator;
		if (!ownerLifetime.Get()
			|| !TryResolveAccessibilityVirtualNode(id, currentLocator)
			|| currentLocator.Kind != locator.Kind
			|| currentLocator.RowIndex != locator.RowIndex) return false;
		result.Name = value.empty()
			? L"Row " + std::to_wstring(locator.RowIndex + 1) : value;
		result.Value = result.Name;
		auto* row = ResolveRow(locator.RowIndex);
		(void)PublishAccessibilityBounds(row, result);
		if (!row)
			result.Patterns |= AutomationPattern::VirtualizedItem;
		return rowId == id;
	}
	case AccessibilityNodeKind::RowHeader:
	{
		if (!HasRowHeaders(_headersVisibility)) return false;
		auto* rowIdentity = live->EnsureAccessibilityRowIdentity(
			locator.RowIndex, false);
		if (!rowIdentity) return false;
		auto* row = ResolveRow(locator.RowIndex);
		auto* header = row ? row->GetRowHeader() : nullptr;
		if (!header || !header->GetIsVisible()) return false;
		result.ParentId = rowIdentity->RowId;
		result.ControlType = AutomationControlType::HeaderItem;
		result.ClassName = L"DataGridRowHeader";
		result.KeyboardFocusable = false;
		result.HasKeyboardFocus = false;
		result.IsControlElement = true;
		result.IsContentElement = false;
		result.Patterns = AutomationPattern::Invoke;
		result.Name = std::to_wstring(locator.RowIndex + 1);
		result.Value = result.Name;
		result.AutomationId = prefix + L".row-header-"
			+ std::to_wstring(id);
		result.Row = static_cast<int>(locator.RowIndex);
		result.Selected = IsIndexSelected(locator.RowIndex);
		(void)PublishAccessibilityBounds(header, result);
		return true;
	}
	case AccessibilityNodeKind::Cell:
	{
		auto* rowIdentity = live->EnsureAccessibilityRowIdentity(
			locator.RowIndex, false);
		auto* cellIdentity = live->EnsureAccessibilityCellIdentity(
			locator.RowIndex, locator.ColumnIndex);
		if (!rowIdentity || !cellIdentity) return false;
		const uint32_t rowId = rowIdentity->RowId;
		const uint32_t cellId = cellIdentity->Id;
		const uint32_t rowHeaderId = rowIdentity->HeaderId;
		const uint32_t columnIdentity = _accessibilityColumns[
			locator.ColumnIndex].ColumnIdentity;
		auto* column = _accessibilityColumns[locator.ColumnIndex].Column;
		if (!column || cellId != id) return false;
		result.ParentId = rowId;
		result.ControlType = AutomationControlType::Custom;
		result.ClassName = L"DataGridCell";
		result.IsControlElement = true;
		result.IsContentElement = true;
		result.Patterns = AutomationPattern::GridItem
			| AutomationPattern::TableItem
			| AutomationPattern::ScrollItem
			| AutomationPattern::Value;
		if (_selectionUnit != DataGridSelectionUnit::FullRow)
			result.Patterns |= AutomationPattern::SelectionItem;
		const auto* bound = dynamic_cast<const DataGridBoundColumn*>(column);
		const bool bindingReadOnly = bound
			&& (bound->GetBindingMode() == BindingMode::OneWay
				|| bound->GetBindingMode() == BindingMode::OneTime);
		result.ReadOnly = _isReadOnly || column->GetIsReadOnly()
			|| bindingReadOnly;
		if (!result.ReadOnly) result.Patterns |= AutomationPattern::Invoke;
		result.AutomationId = prefix + L".cell-" + std::to_wstring(id);
		result.Row = static_cast<int>(locator.RowIndex);
		result.Column = static_cast<int>(locator.ColumnIndex);
		result.Selected = _selectionUnit == DataGridSelectionUnit::FullRow
			? IsIndexSelected(locator.RowIndex)
			: IsCellSelected(locator.RowIndex, locator.ColumnIndex);
		(void)TryReadAccessibilityCellValue(
			locator.RowIndex, locator.ColumnIndex, result.Value);
		AccessibilityNodeLocator currentLocator;
		if (!ownerLifetime.Get()
			|| !TryResolveAccessibilityVirtualNode(id, currentLocator)
			|| currentLocator.Kind != locator.Kind
			|| currentLocator.RowIndex != locator.RowIndex
			|| currentLocator.ColumnIndex != locator.ColumnIndex
			|| locator.ColumnIndex >= _accessibilityColumns.size()
			|| _accessibilityColumns[locator.ColumnIndex].ColumnIdentity
				!= columnIdentity) return false;
		column = _accessibilityColumns[locator.ColumnIndex].Column;
		if (!column) return false;
		auto* row = ResolveRow(locator.RowIndex);
		auto* cell = row ? row->GetCell(locator.ColumnIndex) : nullptr;
		std::wstring cellName;
		if (cell && dynamic_cast<DataGridTemplateColumn*>(column))
		{
			// Template columns have no automation Value without a clipboard
			// binding, but WPF still obtains the cell Name from the realized
			// cell peer. Keep these two accessibility contracts independent.
			const ControlWeakReference cellLifetime(cell);
			const auto cellSnapshot = cell->GetAccessibilitySnapshot();
			live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
			cell = dynamic_cast<DataGridCell*>(cellLifetime.Get());
			AccessibilityNodeLocator refreshedLocator;
			if (!live || !cell
				|| !live->TryResolveAccessibilityVirtualNode(id, refreshedLocator)
				|| refreshedLocator.Kind != AccessibilityNodeKind::Cell
				|| refreshedLocator.RowIndex != locator.RowIndex
				|| refreshedLocator.ColumnIndex != locator.ColumnIndex
				|| locator.ColumnIndex >= live->_accessibilityColumns.size()
				|| live->_accessibilityColumns[
					locator.ColumnIndex].ColumnIdentity != columnIdentity)
				return false;
			row = live->ResolveRow(locator.RowIndex);
			if (!row || row->GetCell(locator.ColumnIndex) != cell) return false;
			column = live->_accessibilityColumns[locator.ColumnIndex].Column;
			if (!column) return false;
			cellName = cellSnapshot.Name;
		}
		result.Name = !cellName.empty() ? std::move(cellName)
			: (result.Value.empty()
				? column->GetHeader().ToString() : result.Value);
		result.KeyboardFocusable = cell && cell->CanReceiveKeyboardFocus();
		result.HasKeyboardFocus = cell && cell->GetIsKeyboardFocused();
		auto* rowHeader = row ? row->GetRowHeader() : nullptr;
		if (HasRowHeaders(live->_headersVisibility)
			&& rowHeader && rowHeader->GetIsVisible())
			result.RowHeaderId = rowHeaderId;
		(void)PublishAccessibilityBounds(cell, result);
		if (!cell)
			result.Patterns |= AutomationPattern::VirtualizedItem;
		return true;
	}
	}
	return false;
}

size_t DataGrid::GetAccessibilityVirtualChildCount(
	uint32_t parentId) const
{
	if (!EnsureAccessibilityVirtualIdentities()) return 0;
	if (parentId == 0)
		return (HasColumnHeaders(_headersVisibility)
			? _accessibilityColumns.size() : 0)
			+ ItemCount();
	AccessibilityNodeLocator locator;
	if (!TryResolveAccessibilityVirtualNode(parentId, locator)
		|| locator.Kind != AccessibilityNodeKind::Row
		|| locator.RowIndex >= ItemCount()) return 0;
	const auto* row = ResolveRow(locator.RowIndex);
	const auto* header = row ? row->GetRowHeader() : nullptr;
	const bool hasRowHeader = HasRowHeaders(_headersVisibility)
		&& header && header->GetIsVisible();
	return _accessibilityColumns.size() + (hasRowHeader ? 1 : 0);
}

bool DataGrid::TryGetAccessibilityVirtualChildAt(
	uint32_t parentId, size_t index, uint32_t& result) const
{
	result = 0;
	if (!EnsureAccessibilityVirtualIdentities()) return false;
	if (parentId == 0)
	{
		const size_t headerCount = HasColumnHeaders(_headersVisibility)
			? _accessibilityColumns.size() : 0;
		if (index < headerCount)
			result = _accessibilityColumns[index].HeaderId;
		else
		{
			index -= headerCount;
			if (index >= ItemCount()) return false;
			auto* row = EnsureAccessibilityRowIdentity(index, false);
			if (!row) return false;
			result = row->RowId;
		}
		return result != 0;
	}
	AccessibilityNodeLocator locator;
	if (!TryResolveAccessibilityVirtualNode(parentId, locator)
		|| locator.Kind != AccessibilityNodeKind::Row
		|| locator.RowIndex >= ItemCount()) return false;
	auto* rowIdentity = EnsureAccessibilityRowIdentity(
		locator.RowIndex, false);
	if (!rowIdentity) return false;
	const auto* row = ResolveRow(locator.RowIndex);
	const auto* header = row ? row->GetRowHeader() : nullptr;
	const bool hasRowHeader = HasRowHeaders(_headersVisibility)
		&& header && header->GetIsVisible();
	if (hasRowHeader)
	{
		if (index == 0)
		{
			result = rowIdentity->HeaderId;
			return result != 0;
		}
		--index;
	}
	if (index >= _accessibilityColumns.size()) return false;
	auto* cell = EnsureAccessibilityCellIdentity(locator.RowIndex, index);
	if (!cell) return false;
	result = cell->Id;
	return result != 0;
}

bool DataGrid::TryGetAccessibilityVirtualSibling(
	uint32_t parentId, uint32_t id, bool next, uint32_t& result) const
{
	result = 0;
	if (!EnsureAccessibilityVirtualIdentities()) return false;
	AccessibilityNodeLocator locator;
	if (!TryResolveAccessibilityVirtualNode(id, locator)) return false;
	size_t index = DataGridCellInfo::InvalidIndex;
	if (parentId == 0)
	{
		const size_t headerCount = HasColumnHeaders(_headersVisibility)
			? _accessibilityColumns.size() : 0;
		if (locator.Kind == AccessibilityNodeKind::ColumnHeader)
			index = locator.ColumnIndex;
		else if (locator.Kind == AccessibilityNodeKind::Row)
			index = headerCount + locator.RowIndex;
		else return false;
	}
	else
	{
		AccessibilityNodeLocator parent;
		if (!TryResolveAccessibilityVirtualNode(parentId, parent)
			|| parent.Kind != AccessibilityNodeKind::Row
			|| locator.RowIndex != parent.RowIndex) return false;
		const auto* realized = ResolveRow(parent.RowIndex);
		const auto* header = realized ? realized->GetRowHeader() : nullptr;
		const bool hasRowHeader = HasRowHeaders(_headersVisibility)
			&& header && header->GetIsVisible();
		if (locator.Kind == AccessibilityNodeKind::RowHeader)
		{
			if (!hasRowHeader) return false;
			index = 0;
		}
		else if (locator.Kind == AccessibilityNodeKind::Cell)
			index = locator.ColumnIndex + (hasRowHeader ? 1 : 0);
		else return false;
	}
	const size_t count = GetAccessibilityVirtualChildCount(parentId);
	if (index == DataGridCellInfo::InvalidIndex
		|| (!next && index == 0) || (next && index + 1 >= count))
		return false;
	return TryGetAccessibilityVirtualChildAt(
		parentId, next ? index + 1 : index - 1, result);
}

bool DataGrid::TryHitTestAccessibilityVirtualNode(
	float localX, float localY, uint32_t& result) const
{
	result = 0;
	if (!EnsureAccessibilityVirtualIdentities()) return false;
	const ControlWeakReference ownerLifetime(
		const_cast<DataGrid*>(this));
	std::vector<uint32_t> ids;
	ids.reserve(_accessibilityColumns.size()
		+ GetRealizedItems().size() * 16);
	if (HasColumnHeaders(_headersVisibility))
		for (const auto& column : _accessibilityColumns)
			ids.push_back(column.HeaderId);
	for (const auto& [rowIndex, visual] : GetRealizedItems())
	{
		(void)visual;
		auto* row = EnsureAccessibilityRowIdentity(rowIndex, false);
		if (!row) continue;
		if (HasRowHeaders(_headersVisibility)) ids.push_back(row->HeaderId);
		auto* realizedRow = ResolveRow(rowIndex);
		if (realizedRow)
			for (auto* realizedCell : realizedRow->GetCells())
				if (realizedCell)
					if (auto* identity = EnsureAccessibilityCellIdentity(
						rowIndex, realizedCell->_columnIndex))
						ids.push_back(identity->Id);
		ids.push_back(row->RowId);
	}
	const auto renderPoint = ToRenderSpace(*this, localX, localY);
	auto hit = [ownerLifetime, renderPoint](uint32_t id)
	{
		auto* owner = dynamic_cast<DataGrid*>(ownerLifetime.Get());
		if (!owner) return false;
		AccessibilityVirtualNode node;
		return owner->TryGetAccessibilityVirtualNode(id, node) && node.Visible
			&& IsPointInside(node.BoundsDip, renderPoint);
	};
	for (const uint32_t id : ids)
	{
		if (!ownerLifetime.Get()) return false;
		if (hit(id))
		{
			result = id;
			return true;
		}
	}
	return false;
}

AccessibilityVirtualContainerInfo
DataGrid::GetAccessibilityVirtualContainerInfo() const noexcept
{
	AccessibilityVirtualContainerInfo result;
	result.Patterns = AutomationPattern::Grid
		| AutomationPattern::Table
		| AutomationPattern::Selection;
	const ControlWeakReference ownerLifetime(
		const_cast<DataGrid*>(this));
	AccessibilityScrollInfo scrollInfo;
	const bool hasScroll = GetAccessibilityScrollInfo(scrollInfo);
	auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live) return result;
	if (hasScroll)
		result.Patterns |= AutomationPattern::Scroll;
	result.CanSelectMultiple =
		live->GetSelectionMode() != SelectionMode::Single;
	result.IsSelectionRequired = false;
	try
	{
		const auto items = live->GetItemsView();
		const size_t rows = items ? items.Get()->Count() : 0;
		live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
		if (!live || live->GetItemsView().Shared() != items.Shared())
			return result;
		result.RowCount = static_cast<int>((std::min)(rows,
			static_cast<size_t>((std::numeric_limits<int>::max)())));
		result.ColumnCount = static_cast<int>((std::min)(
			live->_columns.size(),
			static_cast<size_t>((std::numeric_limits<int>::max)())));
	}
	catch (...)
	{
		result.RowCount = 0;
		result.ColumnCount = 0;
	}
	return result;
}

void DataGrid::GetAccessibilityVirtualSelection(
	std::vector<uint32_t>& result) const
{
	result.clear();
	const ControlWeakReference ownerLifetime(
		const_cast<DataGrid*>(this));
	// One retry lets an observer which performs a single synchronous mutation
	// receive the new complete selection.  A continuously mutating observer
	// gets an empty answer, never a prefix assembled from different revisions.
	for (size_t attempt = 0; attempt != 2; ++attempt)
	{
		auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
		if (!live) return;
		if (!live->EnsureAccessibilityVirtualIdentities())
		{
			if (attempt == 0 && ownerLifetime.Get()) continue;
			return;
		}
		live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
		if (!live) return;

		const auto source = live->_source;
		const auto view = live->GetItemsView();
		const auto unit = live->_selectionUnit;
		const size_t rowRevision = live->_rowSelectionRevision;
		const size_t cellRevision = live->_cellSelectionRevision;
		const size_t accessibilityRevision =
			live->_accessibilityIdentityRevision;
		const SelectedIndexCollection selectedRows =
			live->GetSelectedIndices();
		const DataGridSelectedCellCollection selectedCells =
			live->_selectedCells;

		const auto current = [&]() -> DataGrid*
		{
			auto* owner = dynamic_cast<DataGrid*>(ownerLifetime.Get());
			return owner && owner->_source.Shared() == source.Shared()
				&& owner->GetItemsView().Shared() == view.Shared()
				&& owner->_selectionUnit == unit
				&& owner->_rowSelectionRevision == rowRevision
				&& owner->_cellSelectionRevision == cellRevision
				&& owner->_accessibilityIdentityRevision
					== accessibilityRevision
				? owner : nullptr;
		};

		std::vector<uint32_t> candidate;
		bool invalidated = false;
		try
		{
			if (unit != DataGridSelectionUnit::Cell)
			{
				candidate.reserve(selectedRows.size());
				for (const int index : selectedRows)
				{
					if (index < 0) continue;
					live = current();
					if (!live)
					{
						invalidated = true;
						break;
					}
					auto* row = live->EnsureAccessibilityRowIdentity(
						static_cast<size_t>(index), false);
					live = current();
					if (!live)
					{
						invalidated = true;
						break;
					}
					if (row) candidate.push_back(row->RowId);
				}
			}
			if (!invalidated && unit != DataGridSelectionUnit::FullRow)
			{
				candidate.reserve(candidate.size() + selectedCells.size());
				for (const auto& selected : selectedCells)
				{
					live = current();
					if (!live)
					{
						invalidated = true;
						break;
					}
					auto* row = live->EnsureAccessibilityRowIdentity(
						selected.RowIndex, false);
					live = current();
					if (!live)
					{
						invalidated = true;
						break;
					}
					if (!row || row->Item.Shared() != selected.Item.Shared()
						|| row->Occurrence != selected._itemOccurrence) continue;
					auto* cell = live->EnsureAccessibilityCellIdentity(
						selected.RowIndex, selected.ColumnIndex);
					live = current();
					if (!live)
					{
						invalidated = true;
						break;
					}
					if (cell) candidate.push_back(cell->Id);
				}
			}
		}
		catch (...)
		{
			if (!current()) invalidated = true;
			else throw;
		}
		if (!invalidated && current())
		{
			result.swap(candidate);
			return;
		}
	}
}

bool DataGrid::GetAccessibilityVirtualItemAt(
	int row, int column, uint32_t& result) const
{
	result = 0;
	if (row < 0 || column < 0
		|| !EnsureAccessibilityVirtualIdentities()
		|| static_cast<size_t>(row) >= ItemCount()
		|| static_cast<size_t>(column) >= _accessibilityColumns.size())
		return false;
	auto* cellIdentity = EnsureAccessibilityCellIdentity(
		static_cast<size_t>(row), static_cast<size_t>(column));
	if (!cellIdentity) return false;
	result = cellIdentity->Id;
	(void)const_cast<DataGrid*>(this)
		->ScrollAccessibilityVirtualNodeIntoView(result);
	return result != 0;
}

void DataGrid::GetAccessibilityVirtualColumnHeaders(
	std::vector<uint32_t>& result) const
{
	result.clear();
	if (!HasColumnHeaders(_headersVisibility)
		|| !EnsureAccessibilityVirtualIdentities()) return;
	result.reserve(_accessibilityColumns.size());
	for (const auto& column : _accessibilityColumns)
		result.push_back(column.HeaderId);
}

void DataGrid::GetAccessibilityVirtualRowHeaders(
	std::vector<uint32_t>& result) const
{
	result.clear();
	if (!HasRowHeaders(_headersVisibility)
		|| !EnsureAccessibilityVirtualIdentities()) return;
	result.reserve(GetRealizedItems().size());
	for (const auto& [index, visual] : GetRealizedItems())
	{
		(void)visual;
		auto* row = ResolveRow(index);
		auto* header = row ? row->GetRowHeader() : nullptr;
		if (header && header->GetIsVisible())
			if (auto* identity = EnsureAccessibilityRowIdentity(index, false))
				result.push_back(identity->HeaderId);
	}
}

AutomationOperationResult DataGrid::FocusAccessibilityVirtualNode(uint32_t id)
{
	AccessibilityNodeLocator locator;
	if (!TryResolveAccessibilityVirtualNode(id, locator))
		return AutomationOperationResult::ElementNotAvailable;
	if (locator.Kind != AccessibilityNodeKind::Cell)
		return AutomationOperationResult::InvalidOperation;
	if (!IsEffectivelyEnabled())
		return AutomationOperationResult::ElementNotEnabled;

	const ControlWeakReference ownerLifetime(this);
	auto resolveCell = [id](DataGrid* owner,
		AccessibilityNodeLocator& resolved) -> DataGridCell*
	{
		if (!owner
			|| !owner->TryResolveAccessibilityVirtualNode(id, resolved)
			|| resolved.Kind != AccessibilityNodeKind::Cell)
			return nullptr;
		auto* row = owner->ResolveRow(resolved.RowIndex);
		auto* cell = row ? row->GetCell(resolved.ColumnIndex) : nullptr;
		return row && row->GetDataGridOwner() == owner
			&& cell && cell->GetRowOwner() == row ? cell : nullptr;
	};

	auto* cell = resolveCell(this, locator);
	// Match WPF's item-peer contract: a virtualized cell must be realized before
	// its cell peer can accept keyboard focus.
	if (!cell) return AutomationOperationResult::ElementNotAvailable;
	if (!cell->IsEffectivelyEnabled())
		return AutomationOperationResult::ElementNotEnabled;
	if (!cell->CanReceiveKeyboardFocus())
		return AutomationOperationResult::InvalidOperation;

	const size_t rowIndex = locator.RowIndex;
	const size_t columnIndex = locator.ColumnIndex;
	if (!SetCurrentCell(rowIndex, columnIndex))
		return ownerLifetime.Get()
			? AutomationOperationResult::InvalidOperation
			: AutomationOperationResult::ElementNotAvailable;

	auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	cell = resolveCell(live, locator);
	if (!live || !cell || locator.RowIndex != rowIndex
		|| locator.ColumnIndex != columnIndex)
		return AutomationOperationResult::ElementNotAvailable;
	if (!live->_currentCell.IsValid()
		|| live->_currentCell.RowIndex != rowIndex
		|| live->_currentCell.ColumnIndex != columnIndex)
		return AutomationOperationResult::InvalidOperation;
	if (!cell->IsEffectivelyEnabled())
		return AutomationOperationResult::ElementNotEnabled;
	if (!cell->CanReceiveKeyboardFocus())
		return AutomationOperationResult::InvalidOperation;

	const ControlWeakReference cellLifetime(cell);
	if (!cell->Focus())
		return !ownerLifetime.Get() || !cellLifetime.Get()
			? AutomationOperationResult::ElementNotAvailable
			: AutomationOperationResult::InvalidOperation;

	live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	cell = dynamic_cast<DataGridCell*>(cellLifetime.Get());
	AccessibilityNodeLocator focusedLocator;
	if (!live || !cell
		|| resolveCell(live, focusedLocator) != cell
		|| focusedLocator.RowIndex != rowIndex
		|| focusedLocator.ColumnIndex != columnIndex)
		return AutomationOperationResult::ElementNotAvailable;
	return cell->GetIsKeyboardFocused()
		? AutomationOperationResult::Succeeded
		: AutomationOperationResult::InvalidOperation;
}

bool DataGrid::TryGetAccessibilityVirtualFocusedNode(uint32_t& result) const
{
	result = 0;
	if (!EnsureAccessibilityVirtualIdentities()) return false;
	auto* window = GetPresentationWindow();
	auto* cell = window
		? dynamic_cast<DataGridCell*>(window->GetKeyboardFocusedElement())
		: nullptr;
	auto* row = cell ? cell->GetRowOwner() : nullptr;
	if (!row || row->GetDataGridOwner() != this) return false;
	const size_t rowIndex = row->ItemIndex();
	const size_t columnIndex = cell->_columnIndex;
	auto* identity = EnsureAccessibilityCellIdentity(rowIndex, columnIndex);
	if (!identity || columnIndex >= _accessibilityColumns.size()
		|| ResolveRow(rowIndex) != row
		|| row->GetCell(columnIndex) != cell)
		return false;
	result = identity->Id;
	return result != 0;
}

bool DataGrid::InvokeAccessibilityVirtualNode(uint32_t id)
{
	AccessibilityNodeLocator locator;
	if (!TryResolveAccessibilityVirtualNode(id, locator)
		|| !IsEffectivelyEnabled()) return false;
	const ControlWeakReference ownerLifetime(this);
	if (locator.Kind == AccessibilityNodeKind::ColumnHeader)
	{
		auto* column = locator.ColumnIndex < _accessibilityColumns.size()
			? _accessibilityColumns[locator.ColumnIndex].Column : nullptr;
		const bool invoked = column && _canUserSortColumns
			&& column->GetCanUserSort() && PerformSort(*column, false);
		if (invoked)
			if (auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get()))
				live->NotifyAccessibilityVirtualChanged(
					id, AccessibilityChange::Invoke);
		return invoked;
	}
	if (locator.Kind == AccessibilityNodeKind::RowHeader)
	{
		if (!ScrollAccessibilityVirtualNodeIntoView(id)) return false;
		auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
		if (!live || !live->TryResolveAccessibilityVirtualNode(id, locator)
			|| locator.Kind != AccessibilityNodeKind::RowHeader) return false;
		auto* row = live->ResolveRow(locator.RowIndex);
		if (!row || !live->HandleRowHeaderClick(
			*row, ModifierKeys::None)) return false;
		live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
		if (live) live->NotifyAccessibilityVirtualChanged(
			id, AccessibilityChange::Invoke);
		return live != nullptr;
	}
	if (locator.Kind != AccessibilityNodeKind::Row
		&& locator.Kind != AccessibilityNodeKind::Cell) return false;
	if (_isReadOnly || _columns.empty()) return false;
	if (!ScrollAccessibilityVirtualNodeIntoView(id)) return false;
	auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live || !live->TryResolveAccessibilityVirtualNode(id, locator)
		|| (locator.Kind != AccessibilityNodeKind::Row
			&& locator.Kind != AccessibilityNodeKind::Cell)) return false;
	size_t rowIndex = locator.RowIndex;
	size_t columnIndex = locator.Kind == AccessibilityNodeKind::Cell
		? locator.ColumnIndex : 0;
	if (columnIndex >= live->_columns.size()
		|| live->_columns[columnIndex]->GetIsReadOnly()) return false;
	if (locator.Kind == AccessibilityNodeKind::Row
		&& live->_currentCell.IsValid()
		&& live->_currentCell.RowIndex == rowIndex)
	{
		if (auto* current = live->ResolveCurrentCellContainer();
			current && current->GetIsEditing())
		{
			const bool committed = live->CommitEdit();
			live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
			if (committed && live)
				live->NotifyAccessibilityVirtualChanged(
					id, AccessibilityChange::Invoke);
			return committed && live;
		}
	}
	if (locator.Kind == AccessibilityNodeKind::Row)
	{
		live->ListBox::UnselectAll();
		live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
		if (!live || !live->TryResolveAccessibilityVirtualNode(id, locator)
			|| locator.Kind != AccessibilityNodeKind::Row) return false;
		if (!live->ApplySelectedCells({})) return false;
		live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
		if (!live || !live->TryResolveAccessibilityVirtualNode(id, locator)
			|| locator.Kind != AccessibilityNodeKind::Row) return false;
		rowIndex = locator.RowIndex;
		columnIndex = 0;
	}
	if (!live->SetCurrentCell(rowIndex, columnIndex)) return false;
	live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live) return false;
	if (locator.Kind == AccessibilityNodeKind::Cell)
	{
		(void)live->ApplySelectionForCellInput(
			rowIndex, columnIndex, ModifierKeys::None, false, false);
		live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
		if (!live) return false;
	}
	if (auto* cell = live->ResolveCurrentCellContainer();
		cell && cell->GetIsEditing())
	{
		live->NotifyAccessibilityVirtualChanged(
			id, AccessibilityChange::Invoke);
		return true;
	}
	const bool invoked = live->BeginEdit();
	live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (invoked && live) live->NotifyAccessibilityVirtualChanged(
		id, AccessibilityChange::Invoke);
	return invoked && live;
}

bool DataGrid::SetAccessibilityVirtualNodeValue(
	uint32_t id, const std::wstring& value)
{
	AccessibilityNodeLocator locator;
	if (!TryResolveAccessibilityVirtualNode(id, locator)
		|| locator.Kind != AccessibilityNodeKind::Cell
		|| locator.ColumnIndex >= _columns.size()
		|| _isReadOnly || _columns[locator.ColumnIndex]->GetIsReadOnly()
		|| !IsEffectivelyEnabled()) return false;
	if (const auto* bound = dynamic_cast<const DataGridBoundColumn*>(
		_columns[locator.ColumnIndex].get());
		bound && (bound->GetBindingMode() == BindingMode::OneWay
			|| bound->GetBindingMode() == BindingMode::OneTime)) return false;
	const ControlWeakReference ownerLifetime(this);
	if (!ScrollAccessibilityVirtualNodeIntoView(id)) return false;
	auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live || !live->TryResolveAccessibilityVirtualNode(id, locator)
		|| locator.Kind != AccessibilityNodeKind::Cell
		|| locator.ColumnIndex >= live->_columns.size()
		|| live->_isReadOnly
		|| live->_columns[locator.ColumnIndex]->GetIsReadOnly()) return false;
	if (const auto* bound = dynamic_cast<const DataGridBoundColumn*>(
		live->_columns[locator.ColumnIndex].get());
		bound && (bound->GetBindingMode() == BindingMode::OneWay
			|| bound->GetBindingMode() == BindingMode::OneTime)) return false;
	if (!live->SetCurrentCell(locator.RowIndex, locator.ColumnIndex))
		return false;
	live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live || !live->TryResolveAccessibilityVirtualNode(id, locator)
		|| locator.Kind != AccessibilityNodeKind::Cell
		|| !(live->_currentCell.IsValid()
			&& live->_currentCell.RowIndex == locator.RowIndex
			&& live->_currentCell.ColumnIndex == locator.ColumnIndex)) return false;
	auto* cell = live->ResolveCurrentCellContainer();
	if (!cell) return false;
	if (!cell->GetIsEditing() && !live->BeginEdit()) return false;
	live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	cell = live ? live->ResolveCurrentCellContainer() : nullptr;
	auto* editor = cell ? cell->GetEditingElement() : nullptr;
	if (!live || !editor) return false;
	const ControlWeakReference editorLifetime(editor);
	// WPF's DataGridTemplateColumn uses ClipboardContentBinding as the
	// automation value contract.  With the default null binding SetValue
	// enters edit mode, but is otherwise a successful no-op regardless of
	// which automation patterns the editing template happens to expose.
	if (locator.ColumnIndex < live->_columns.size()
		&& dynamic_cast<DataGridTemplateColumn*>(
			live->_columns[locator.ColumnIndex].get()))
	{
		return ownerLifetime.Get() != nullptr;
	}
	// Value providers are extensible and may synchronously detach/delete the
	// editing element. Keep the peer alive until the call unwinds; the weak
	// owner/editor checks below decide whether the edit can continue.
	auto editorPeer = editor->AcquireAutomationPeer();
	if (!editorPeer) return false;
	bool wroteValue = editorPeer->SetValue(value)
		== AutomationOperationResult::Succeeded;
	live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	editor = dynamic_cast<Control*>(editorLifetime.Get());
	if (!live || !editor
		|| !live->TryResolveAccessibilityVirtualNode(id, locator)
		|| locator.Kind != AccessibilityNodeKind::Cell) return false;
	if (!wroteValue)
	{
		if (auto* checkBox = dynamic_cast<CheckBox*>(editor))
		{
			const auto text = Trim(value);
			NullableBool checked;
			if (EqualsIgnoreCase(text, L"true")) checked = NullableBool(true);
			else if (EqualsIgnoreCase(text, L"false"))
				checked = NullableBool(false);
			else if (text.empty() && checkBox->GetIsThreeState())
				checked = NullableBool{};
			else return false;
			wroteValue = checkBox->TrySetCurrentPropertyValue(
				ToggleButton::IsCheckedProperty(), BindingValue(checked));
		}
	}
	if (!wroteValue) return false;
	live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live) return false;
	if (!editorLifetime.Get())
	{
		live->NotifyAccessibilityVirtualChanged(
			id, AccessibilityChange::Value);
		return true;
	}
	const bool committed = live->CommitEdit();
	live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (committed && live) live->NotifyAccessibilityVirtualChanged(
		id, AccessibilityChange::Value);
	return committed && live;
}

bool DataGrid::SelectAccessibilityVirtualNode(
	uint32_t id, AccessibilitySelectionAction action)
{
	AccessibilityNodeLocator locator;
	if (!TryResolveAccessibilityVirtualNode(id, locator)
		|| !IsEffectivelyEnabled()) return false;
	if (locator.Kind == AccessibilityNodeKind::Row)
	{
		if (_selectionUnit == DataGridSelectionUnit::Cell
			|| locator.RowIndex >= ItemCount()) return false;
		const bool selected = IsIndexSelected(locator.RowIndex);
		if (action == AccessibilitySelectionAction::Add)
		{
			if (selected) return true;
			if (GetSelectionMode() == SelectionMode::Single
				&& !GetSelectedIndices().empty()) return false;
			const ControlWeakReference ownerLifetime(this);
			RequestItemSelection(locator.RowIndex, true);
			return ownerLifetime.Get() != nullptr;
		}
		if (action == AccessibilitySelectionAction::Remove)
		{
			if (!selected) return true;
			const ControlWeakReference ownerLifetime(this);
			RequestItemSelection(locator.RowIndex, false);
			return ownerLifetime.Get() != nullptr;
		}
		const ControlWeakReference ownerLifetime(this);
		NotifyItemClicked(
			locator.RowIndex, MouseButton::Left, ModifierKeys::None);
		return ownerLifetime.Get() != nullptr;
	}
	if (locator.Kind != AccessibilityNodeKind::Cell
		|| _selectionUnit == DataGridSelectionUnit::FullRow
		|| locator.RowIndex >= ItemCount()
		|| locator.ColumnIndex >= _columns.size()) return false;
	DataGridCellInfo target;
	if (!TryCreateCellInfo(
		locator.RowIndex, locator.ColumnIndex, target)) return false;
	const bool selected = _selectedCells.Contains(target);
	if (action == AccessibilitySelectionAction::Remove)
	{
		if (!selected) return true;
		return UnselectCell(locator.RowIndex, locator.ColumnIndex);
	}
	if (action == AccessibilitySelectionAction::Add)
	{
		if (selected) return true;
		if (GetSelectionMode() == SelectionMode::Single
			&& !_selectedCells.empty()) return false;
		if (_selectedCells.IsRegionBacked())
		{
			auto next = _selectedCells;
			if (!next.Include(target))
				next._cells.push_back(std::move(target));
			return ApplySelectedCellCollection(std::move(next));
		}
		auto next = _selectedCells._cells;
		next.push_back(std::move(target));
		return ApplySelectedCells(std::move(next));
	}
	if (_selectionUnit == DataGridSelectionUnit::CellOrRowHeader
		&& !GetSelectedIndices().empty())
	{
		const ControlWeakReference ownerLifetime(this);
		if (!DeselectAllRowsWithoutCellSync()) return false;
		auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
		if (!live || !live->TryResolveAccessibilityVirtualNode(id, locator)
			|| locator.Kind != AccessibilityNodeKind::Cell) return false;
		DataGridCellInfo refreshed;
		if (!live->TryCreateCellInfo(
			locator.RowIndex, locator.ColumnIndex, refreshed)) return false;
		return live->ApplySelectedCells({ std::move(refreshed) });
	}
	return ApplySelectedCells({ std::move(target) });
}

bool DataGrid::ScrollAccessibilityVirtualNodeIntoView(uint32_t id)
{
	AccessibilityNodeLocator locator;
	if (!TryResolveAccessibilityVirtualNode(id, locator)) return false;
	if (locator.Kind == AccessibilityNodeKind::RowHeader
		&& !HasRowHeaders(_headersVisibility)) return false;
	const ControlWeakReference ownerLifetime(this);
	if (locator.RowIndex != DataGridCellInfo::InvalidIndex)
	{
		if (locator.RowIndex >= ItemCount()
			|| !BringItemIntoView(locator.RowIndex)) return false;
		if (!ownerLifetime.Get()) return false;
		if (!TryResolveAccessibilityVirtualNode(id, locator)) return false;
	}
	if (locator.ColumnIndex != DataGridCellInfo::InvalidIndex)
	{
		if (!BringColumnIntoView(locator.ColumnIndex)) return false;
		if (!ownerLifetime.Get()) return false;
	}
	return true;
}

bool DataGrid::GetAccessibilityScrollInfo(
	AccessibilityScrollInfo& result) const noexcept
{
	result = {};
	auto* scroll = dynamic_cast<ScrollViewer*>(_scrollViewer.Get());
	if (!scroll) return false;
	try
	{
		return scroll->GetAutomationPeer()
			.GetAccessibilityScrollInfo(result);
	}
	catch (...)
	{
		result = {};
		return false;
	}
}

bool DataGrid::ScrollAccessibility(
	AccessibilityScrollAmount horizontal,
	AccessibilityScrollAmount vertical)
{
	auto* scroll = dynamic_cast<ScrollViewer*>(_scrollViewer.Get());
	return scroll && scroll->GetAutomationPeer()
		.ScrollAccessibility(horizontal, vertical);
}

bool DataGrid::SetAccessibilityScrollPercent(
	double horizontalPercent, double verticalPercent)
{
	auto* scroll = dynamic_cast<ScrollViewer*>(_scrollViewer.Get());
	return scroll && scroll->GetAutomationPeer()
		.SetAccessibilityScrollPercent(
			horizontalPercent, verticalPercent);
}

bool DataGrid::IsCellSelected(
	size_t rowIndex, size_t columnIndex) const
{
	DataGridCellInfo info;
	return TryCreateCellInfo(rowIndex, columnIndex, info)
		&& ContainsCellIdentity(_selectedCells, info);
}

bool DataGrid::RefreshSelectedCellContainers(
	bool suppressRoutedEvents, bool onlyStaleRows)
{
	ControlWeakReference ownerLifetime(this);
	const size_t projectionRevision = _cellSelectionRevision;
	std::vector<ControlWeakReference> rows;
	std::vector<ControlWeakReference> cells;
	for (const auto& [index, realized] : GetRealizedItems())
	{
		(void)realized;
		auto* row = dynamic_cast<DataGridRow*>(GetGeneratedItem(index));
		if (!row || (onlyStaleRows
			&& row->_appliedCellSelectionRevision == projectionRevision))
			continue;
		rows.emplace_back(row);
		for (auto* cell : row->GetCells())
			if (cell) cells.emplace_back(cell);
	}
	if (!cells.empty())
	{
		_updatingCellSelectionVisuals = true;
		try
		{
			for (auto& reference : cells)
			{
				auto* cell = dynamic_cast<DataGridCell*>(reference.Get());
				if (!cell || !cell->GetRowOwner()) continue;
				// The normal first viewport has no selected cells. Avoid asking
				// IsCellSelected to materialize the full occurrence-identity cache just
				// to prove that each of the few realized cells is false. Re-check the
				// collection for every cell because a routed deselection handler may
				// change selection reentrantly.
				const bool selected = !_selectedCells.empty()
					&& IsCellSelected(
						cell->GetRowOwner()->ItemIndex(), cell->_columnIndex);
				cell->SetCurrentIsSelected(selected,
					suppressRoutedEvents);
				if (!ownerLifetime.Get()) return false;
			}
		}
		catch (...)
		{
			if (auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get()))
				live->_updatingCellSelectionVisuals = false;
			throw;
		}
		_updatingCellSelectionVisuals = false;
	}
	auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (live && live->_cellSelectionRevision == projectionRevision)
		for (const auto& reference : rows)
			if (auto* row = dynamic_cast<DataGridRow*>(reference.Get());
				row && row->GetDataGridOwner() == live)
				row->_appliedCellSelectionRevision = projectionRevision;
	return true;
}

void DataGrid::RefreshSelectedCellContainersAfterRollback() noexcept
{
	if (!_selectedCellsVisualRefreshPending) return;
	ControlWeakReference ownerLifetime(this);
	try
	{
		if (!RefreshSelectedCellContainers(true)) return;
		if (auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get()))
			live->_selectedCellsVisualRefreshPending = false;
	}
	catch (...)
	{
		// Rollback must preserve the original transaction failure.  A later
		// realization pass retries the visual projection if an observer throws.
	}
}

bool DataGrid::RaiseSelectedCellsChanged(
	const DataGridSelectedCellCollection& previous,
	bool ignoreLocators)
{
	return RaiseSelectedCellsChangedCore(
		previous, _selectedCells, ignoreLocators);
}

bool DataGrid::RaiseSelectedCellsChangedCore(
	const DataGridSelectedCellCollection& previous,
	const DataGridSelectedCellCollection& current,
	bool ignoreLocators)
{
	ControlWeakReference ownerLifetime(this);
	DataGridSelectedCellsChangedEventArgs args;
	(void)ignoreLocators;
	args.RemovedCells = previous.Difference(current);
	// RemovedCells describes the old logical domain. Bind every lazy region to
	// its retained immutable snapshot: Reset/source reentrancy may already have
	// retired the live occurrence tokens before a handler enumerates the delta.
	for (auto& region : args.RemovedCells._regions)
		region.UseSnapshotLocators = true;
	args.AddedCells = current.Difference(previous);
	if (args.AddedCells.empty() && args.RemovedCells.empty()) return true;
	auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live) return false;
	cui::framework::EventAccess::RaiseWhile(
		live->SelectedCellsChanged,
		[&]()
		{
			auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
			if (!live) return false;
			const auto columnIsLive = [live](DataGridColumn* column)
			{
				return !column || std::any_of(
					live->_columns.begin(), live->_columns.end(),
					[column](const auto& candidate)
					{ return candidate.get() == column; });
			};
			const auto collectionColumnsAreLive = [&](const auto& cells)
			{
				return std::all_of(cells._cells.begin(), cells._cells.end(),
					[&](const DataGridCellInfo& cell)
					{ return columnIsLive(cell.Column); })
					&& std::all_of(cells._regions.begin(), cells._regions.end(),
						[&](const auto& region)
						{
							return std::all_of(region.Columns.begin(),
								region.Columns.end(), columnIsLive);
						});
			};
			return collectionColumnsAreLive(args.AddedCells)
				&& collectionColumnsAreLive(args.RemovedCells);
		},
		live, args);
	live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live) return false;
	// WPF only enumerates a virtual delta when an automation listener requests
	// per-cell events. CUI does not yet expose UiaClientsAreListening, so publish
	// one authoritative owner-selection invalidation for a region delta. The
	// Selection.GetSelection path below remains exact and enumerates lazily when
	// a client actually asks for the selected providers.
	if (args.AddedCells.IsRegionBacked()
		|| args.RemovedCells.IsRegionBacked())
	{
		live->NotifyAccessibilityStateChanged();
		return ownerLifetime.Get() != nullptr;
	}
	// WPF raises the public event first.  A handler can legally update the
	// selection again, so automation classifies the original delta against the
	// final live selection domain after that callback completes.
	if (!live->EnsureAccessibilityVirtualIdentities())
		return ownerLifetime.Get() != nullptr;
	live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live) return false;
	const auto findId = [ownerLifetime](const DataGridCellInfo& info)
	{
		auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
		if (!live) return uint32_t{ 0 };
		auto row = std::find_if(
			live->_accessibilityRows.begin(), live->_accessibilityRows.end(),
			[&info](const AccessibilityRowIdentity& candidate)
			{
				return candidate.Item.Shared() == info.Item.Shared()
					&& candidate.Occurrence == info._itemOccurrence;
			});
		if (row == live->_accessibilityRows.end())
		{
			size_t rowIndex = DataGridCellInfo::InvalidIndex;
			if (!live->TryResolveItemOccurrence(
				info.Item, info._itemOccurrence, rowIndex)) return uint32_t{ 0 };
			live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
			if (!live) return uint32_t{ 0 };
			if (!live->EnsureAccessibilityRowIdentity(rowIndex, false))
				return uint32_t{ 0 };
			live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
			if (!live) return uint32_t{ 0 };
			row = std::find_if(
				live->_accessibilityRows.begin(),
				live->_accessibilityRows.end(),
				[&info](const AccessibilityRowIdentity& candidate)
				{
					return candidate.Item.Shared() == info.Item.Shared()
						&& candidate.Occurrence == info._itemOccurrence;
				});
			if (row == live->_accessibilityRows.end()) return uint32_t{ 0 };
		}
		const size_t rowIndex = row->ViewIndex;
		auto* cell = live->EnsureAccessibilityCellIdentity(
			rowIndex, info.ColumnIndex);
		return cell ? cell->Id : uint32_t{ 0 };
	};
	std::vector<uint32_t> removedIds;
	std::vector<uint32_t> addedIds;
	removedIds.reserve(args.RemovedCells.size());
	addedIds.reserve(args.AddedCells.size());
	for (const auto& cell : args.RemovedCells)
		if (const uint32_t id = findId(cell); id != 0)
			removedIds.push_back(id);
	for (const auto& cell : args.AddedCells)
		if (const uint32_t id = findId(cell); id != 0)
			addedIds.push_back(id);
	if (live->_selectedCells.size() == 1 && args.AddedCells.size() == 1)
	{
		if (!addedIds.empty())
		{
			live->NotifyAccessibilityVirtualChanged(
				addedIds.front(), AccessibilityChange::SelectionSelected);
			if (!ownerLifetime.Get()) return false;
		}
	}
	else
	{
		for (const uint32_t id : addedIds)
		{
			live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
			if (!live) return false;
			live->NotifyAccessibilityVirtualChanged(
				id, AccessibilityChange::SelectionAdded);
		}
		for (const uint32_t id : removedIds)
		{
			live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
			if (!live) return false;
			live->NotifyAccessibilityVirtualChanged(
				id, AccessibilityChange::SelectionRemoved);
		}
	}
	return ownerLifetime.Get() != nullptr;
}

bool DataGrid::ApplySelectedCells(std::vector<DataGridCellInfo> cells)
{
	const bool outermost = _selectedCellsApplyDepth == 0;
	if (outermost) _selectedCellsApplyOld = _selectedCells;
	++_selectedCellsApplyDepth;
	ControlWeakReference ownerLifetime(this);
	try
	{
	const auto items = GetItemsView();
	std::vector<DataGridCellInfo> normalized;
	normalized.reserve(cells.size());
	for (auto& candidate : cells)
	{
		if (!candidate.Item || !candidate.Column || !items) continue;
		const auto column = std::find_if(
			_columns.begin(), _columns.end(),
			[&candidate](const auto& value)
			{ return value.get() == candidate.Column; });
		if (column == _columns.end()) continue;
		const size_t columnIndex = static_cast<size_t>(
			std::distance(_columns.begin(), column));
		size_t rowIndex = DataGridCellInfo::InvalidIndex;
		if (candidate.RowIndex < items.Get()->Count())
		{
			BindingSourceReference atIndex;
			if (items.Get()->TryGetItem(candidate.RowIndex, atIndex)
				&& atIndex.Shared() == candidate.Item.Shared())
			{
				size_t occurrence = DataGridCellInfo::InvalidIndex;
				if (!TryGetItemOccurrenceAt(
					candidate.RowIndex, occurrence)) continue;
				if (candidate._itemOccurrence == DataGridCellInfo::InvalidIndex
					|| candidate._itemOccurrence == occurrence)
				{
					candidate._itemOccurrence = occurrence;
					rowIndex = candidate.RowIndex;
				}
			}
		}
		if (rowIndex == DataGridCellInfo::InvalidIndex)
		{
			if (candidate._itemOccurrence != DataGridCellInfo::InvalidIndex)
			{
				size_t resolvedIndex = DataGridCellInfo::InvalidIndex;
				if (TryResolveItemOccurrence(
					candidate.Item, candidate._itemOccurrence, resolvedIndex))
					rowIndex = resolvedIndex;
			}
		}
		if (rowIndex == DataGridCellInfo::InvalidIndex)
		{
			for (size_t index = 0; index < items.Get()->Count(); ++index)
			{
				BindingSourceReference item;
				if (items.Get()->TryGetItem(index, item)
					&& item.Shared() == candidate.Item.Shared())
				{
					size_t occurrence = DataGridCellInfo::InvalidIndex;
					if (!TryGetItemOccurrenceAt(index, occurrence)) continue;
					if (candidate._itemOccurrence == DataGridCellInfo::InvalidIndex
						|| candidate._itemOccurrence == occurrence)
					{
						candidate._itemOccurrence = occurrence;
						rowIndex = index;
						break;
					}
				}
			}
		}
		if (rowIndex == DataGridCellInfo::InvalidIndex) continue;
		candidate.RowIndex = rowIndex;
		candidate.ColumnIndex = columnIndex;
		candidate.Column = column->get();
		if (!ContainsCellIdentity(normalized, candidate))
			normalized.push_back(std::move(candidate));
	}
	std::sort(normalized.begin(), normalized.end(),
		[](const DataGridCellInfo& left, const DataGridCellInfo& right)
		{
			return left.RowIndex != right.RowIndex
				? left.RowIndex < right.RowIndex
				: left.ColumnIndex < right.ColumnIndex;
		});
	_selectedCells.SetSparse(this, _source, std::move(normalized));
	if (!RefreshSelectedCellContainers()) return false;
	}
	catch (...)
	{
		if (auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get()))
		{
			if (live->_selectedCellsApplyDepth != 0)
				--live->_selectedCellsApplyDepth;
			if (live->_selectedCellsApplyDepth == 0)
				live->_selectedCellsApplyOld = {};
		}
		throw;
	}
	auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live) return false;
	if (live->_selectedCellsApplyDepth != 0)
		--live->_selectedCellsApplyDepth;
	if (live->_selectedCellsApplyDepth != 0) return true;
	auto previous = std::move(live->_selectedCellsApplyOld);
	live->_selectedCellsApplyOld = {};
	const bool ignoreLocators = live->_reconcilingSelectedCellLocators;
	const bool changed = !SameCellCollection(
		previous, live->_selectedCells, ignoreLocators);
	if (!changed) return true;
	if (++live->_cellSelectionRevision == 0)
		live->_cellSelectionRevision = 1;
	for (const auto& [index, realized] : live->GetRealizedItems())
	{
		(void)realized;
		if (auto* row = dynamic_cast<DataGridRow*>(
			live->GetGeneratedItem(index)))
			row->_appliedCellSelectionRevision = live->_cellSelectionRevision;
	}
	if (live->_settingItemsSource || live->IsItemsSourceUpdateInProgress())
	{
		if (!live->_selectedCellsChangeDeferred)
		{
			live->_selectedCellsChangeDeferred = true;
			live->_deferredSelectedCellsOld = previous;
			live->_deferredSelectedCellsIgnoreLocators = ignoreLocators;
		}
		else live->_deferredSelectedCellsIgnoreLocators =
			live->_deferredSelectedCellsIgnoreLocators && ignoreLocators;
		return true;
	}
	return live->RaiseSelectedCellsChanged(previous, ignoreLocators);
}

bool DataGrid::ApplySelectedCellCollection(
	DataGridSelectedCellCollection cells)
{
	const bool outermost = _selectedCellsApplyDepth == 0;
	if (outermost) _selectedCellsApplyOld = _selectedCells;
	++_selectedCellsApplyDepth;
	const ControlWeakReference ownerLifetime(this);
	try
	{
		cells._ownerLifetime = ownerLifetime;
		cells._ownerBound = true;
		if (!cells._ownerSourceIdentity)
			cells._ownerSourceIdentity = _source;
		if (!cells._source) cells._source = GetItemsView();
		_selectedCells = std::move(cells);
		if (!RefreshSelectedCellContainers()) return false;
	}
	catch (...)
	{
		if (auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get()))
		{
			if (live->_selectedCellsApplyDepth != 0)
				--live->_selectedCellsApplyDepth;
			if (live->_selectedCellsApplyDepth == 0)
				live->_selectedCellsApplyOld = {};
		}
		throw;
	}
	auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live) return false;
	if (live->_selectedCellsApplyDepth != 0)
		--live->_selectedCellsApplyDepth;
	if (live->_selectedCellsApplyDepth != 0) return true;
	auto previous = std::move(live->_selectedCellsApplyOld);
	live->_selectedCellsApplyOld = {};
	const bool ignoreLocators = live->_reconcilingSelectedCellLocators;
	if (SameCellCollection(previous, live->_selectedCells, ignoreLocators))
		return true;
	if (++live->_cellSelectionRevision == 0)
		live->_cellSelectionRevision = 1;
	for (const auto& [index, realized] : live->GetRealizedItems())
	{
		(void)realized;
		if (auto* row = dynamic_cast<DataGridRow*>(
			live->GetGeneratedItem(index)))
			row->_appliedCellSelectionRevision = live->_cellSelectionRevision;
	}
	if (live->_settingItemsSource || live->IsItemsSourceUpdateInProgress())
	{
		if (!live->_selectedCellsChangeDeferred)
		{
			live->_selectedCellsChangeDeferred = true;
			live->_deferredSelectedCellsOld = previous;
			live->_deferredSelectedCellsIgnoreLocators = ignoreLocators;
		}
		else live->_deferredSelectedCellsIgnoreLocators =
			live->_deferredSelectedCellsIgnoreLocators && ignoreLocators;
		return true;
	}
	return live->RaiseSelectedCellsChanged(previous, ignoreLocators);
}

bool DataGrid::SelectCell(size_t rowIndex, size_t columnIndex)
{
	if (_selectionUnit == DataGridSelectionUnit::FullRow) return false;
	DataGridCellInfo info;
	if (!TryCreateCellInfo(rowIndex, columnIndex, info)) return false;
	const bool alreadySelected = ContainsCellIdentity(_selectedCells, info);
	_selectionAnchor = info;
	ResetSelectionRange();
	const bool changed = !alreadySelected
		|| GetSelectionMode() == SelectionMode::Single;
	if (GetSelectionMode() == SelectionMode::Single)
		ApplySelectedCells({ std::move(info) });
	else if (_selectedCells.IsRegionBacked())
	{
		auto selected = _selectedCells;
		if (!selected.Include(info) && !alreadySelected)
			selected._cells.push_back(std::move(info));
		ApplySelectedCellCollection(std::move(selected));
	}
	else
	{
		auto selected = _selectedCells._cells;
		if (!alreadySelected) selected.push_back(std::move(info));
		ApplySelectedCells(std::move(selected));
	}
	return changed;
}

bool DataGrid::UnselectCell(size_t rowIndex, size_t columnIndex)
{
	if (_selectionUnit == DataGridSelectionUnit::FullRow) return false;
	DataGridCellInfo info;
	if (!TryCreateCellInfo(rowIndex, columnIndex, info)
		|| !ContainsCellIdentity(_selectedCells, info)) return false;
	auto selected = _selectedCells;
	if (selected.IsRegionBacked())
	{
		selected._cells.erase(std::remove_if(
			selected._cells.begin(), selected._cells.end(),
			[&info](const DataGridCellInfo& value)
			{ return SameCellIdentity(value, info); }), selected._cells.end());
		(void)selected.Exclude(info);
	}
	else selected._cells.erase(std::remove_if(
		selected._cells.begin(), selected._cells.end(),
		[&info](const DataGridCellInfo& value)
		{ return SameCellIdentity(value, info); }), selected._cells.end());
	if (_selectionUnit == DataGridSelectionUnit::CellOrRowHeader)
	{
		ControlWeakReference ownerLifetime(this);
		const bool previous = _suppressRowSelectionCellSync;
		_suppressRowSelectionCellSync = true;
		try
		{
			if (IsIndexSelected(rowIndex))
				RequestItemSelection(rowIndex, false);
			auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
			if (!live) return false;
			live->_suppressRowSelectionCellSync = previous;
		}
		catch (...)
		{
			if (auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get()))
				live->_suppressRowSelectionCellSync = previous;
			throw;
		}
	}
	ResetSelectionRange();
	if (selected.IsRegionBacked())
		ApplySelectedCellCollection(std::move(selected));
	else ApplySelectedCells(std::move(selected._cells));
	return true;
}

void DataGrid::SelectAllCells()
{
	if (_selectionUnit == DataGridSelectionUnit::FullRow)
	{
		ListBox::SelectAll();
		return;
	}
	// A stable checkpoint gives the region a durable occurrence domain. Sorting
	// only permutes the view, so the one full-grid region keeps selecting the
	// exact same duplicate occurrences without N x columns cells.
	BindingListReference regionSnapshot;
	if (TryGetStableSelectedCellRegionSnapshot(regionSnapshot))
	{
		std::vector<DataGridColumn*> columns;
		columns.reserve(_columns.size());
		for (const auto& column : _columns)
			if (column) columns.push_back(column.get());
		DataGridSelectedCellCollection selected(this);
		selected.SetFullRegion(
			this, std::move(regionSnapshot),
			ItemCount(), std::move(columns));
		ResetSelectionRange();
		(void)ApplySelectedCellCollection(std::move(selected));
		return;
	}
	std::vector<DataGridCellInfo> selected;
	selected.reserve(ItemCount() * _columns.size());
	for (size_t row = 0; row < ItemCount(); ++row)
		for (size_t column = 0; column < _columns.size(); ++column)
		{
			DataGridCellInfo info;
			if (TryCreateCellInfo(row, column, info))
				selected.push_back(std::move(info));
		}
	ResetSelectionRange();
	ApplySelectedCells(std::move(selected));
}

void DataGrid::UnselectAllCells()
{
	if (_selectionUnit != DataGridSelectionUnit::Cell)
		if (!DeselectAllRowsWithoutCellSync()) return;
	_selectionAnchor.reset();
	ResetSelectionRange();
	ApplySelectedCells({});
}

void DataGrid::ResetSelectionRange() noexcept
{
	_selectionRangeActive = false;
	_selectionRangeBase.clear();
}

bool DataGrid::DeselectAllRowsWithoutCellSync()
{
	ControlWeakReference ownerLifetime(this);
	const bool previous = _suppressRowSelectionCellSync;
	_suppressRowSelectionCellSync = true;
	try
	{
		ListBox::UnselectAll();
		auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
		if (!live) return false;
		live->_suppressRowSelectionCellSync = previous;
		live->_selectedRowSnapshot.clear();
		live->_selectedRowIndexSnapshot.Clear();
		return true;
	}
	catch (...)
	{
		if (auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get()))
			live->_suppressRowSelectionCellSync = previous;
		throw;
	}
}

bool DataGrid::ApplySelectionForCellInput(
	size_t rowIndex, size_t columnIndex, ModifierKeys modifiers,
	bool allowsRange, bool allowsToggle)
{
	DataGridCellInfo target;
	if (!TryCreateCellInfo(rowIndex, columnIndex, target)) return false;
	const bool control = HasModifier(modifiers, ModifierKeys::Control);
	const bool shift = HasModifier(modifiers, ModifierKeys::Shift);
	const auto mode = GetSelectionMode();

	if (_selectionUnit == DataGridSelectionUnit::FullRow)
	{
		ControlWeakReference ownerLifetime(this);
		_selectionAnchor = target;
		ResetSelectionRange();
		if (mode == SelectionMode::Single && control && allowsToggle
			&& IsIndexSelected(rowIndex))
			ListBox::UnselectAll();
		else
			NotifyItemClicked(rowIndex, MouseButton::Left, modifiers);
		if (!ownerLifetime.Get()) return false;
		return IsCellSelected(rowIndex, columnIndex);
	}

	std::vector<DataGridCellInfo> selected;
	const bool extended = mode == SelectionMode::Extended;
	const bool multiple = mode == SelectionMode::Multiple;
	const bool canExtend = extended && allowsRange && shift
		&& _selectionAnchor && _selectionAnchor->IsValid();
	if (canExtend && control && _selectedCells.IsRegionBacked())
	{
		// The safe dense representation currently originates from SelectAll.
		// Ctrl+Shift unions a rectangle into that already-full region and is
		// therefore an O(1) no-op while retaining the range transaction state.
		_selectionRangeActive = true;
		return true;
	}
	if (canExtend)
	{
		if (_selectionRangeActive && !control)
			_selectionRangeBase.clear();
		if (!_selectionRangeActive)
		{
			_selectionRangeBase = control
				? _selectedCells._cells : std::vector<DataGridCellInfo>{};
			_selectionRangeActive = true;
		}
		selected = _selectionRangeBase;
		const size_t firstRow = (std::min)(
			_selectionAnchor->RowIndex, rowIndex);
		const size_t lastRow = (std::max)(
			_selectionAnchor->RowIndex, rowIndex);
		const size_t firstColumn = (std::min)(
			_selectionAnchor->ColumnIndex, columnIndex);
		const size_t lastColumn = (std::max)(
			_selectionAnchor->ColumnIndex, columnIndex);
		for (size_t row = firstRow; row <= lastRow; ++row)
			for (size_t column = firstColumn; column <= lastColumn; ++column)
			{
				DataGridCellInfo info;
				if (TryCreateCellInfo(row, column, info)
					&& !ContainsCellIdentity(selected, info))
					selected.push_back(std::move(info));
			}
	}
	else
	{
		ResetSelectionRange();
		const bool toggle = allowsToggle
			&& ((extended && control) || multiple);
		if (toggle)
		{
			if (_selectedCells.IsRegionBacked())
			{
				auto next = _selectedCells;
				const bool wasSelected = next.Contains(target);
				if (wasSelected)
				{
					next._cells.erase(std::remove_if(
						next._cells.begin(), next._cells.end(),
						[&target](const DataGridCellInfo& value)
						{ return SameCellIdentity(value, target); }),
						next._cells.end());
					(void)next.Exclude(target);
				}
				else if (!next.Include(target))
					next._cells.push_back(target);
				_selectionAnchor = target;
				if (!DeselectAllRowsWithoutCellSync()) return false;
				const bool targetSelected = next.Contains(target);
				ApplySelectedCellCollection(std::move(next));
				return targetSelected;
			}
			selected = _selectedCells._cells;
			const auto found = std::find_if(selected.begin(), selected.end(),
				[&target](const DataGridCellInfo& value)
				{ return SameCellIdentity(value, target); });
			if (found == selected.end()) selected.push_back(target);
			else selected.erase(found);
		}
		else if (mode == SelectionMode::Single && control && allowsToggle
			&& ContainsCellIdentity(_selectedCells, target))
			selected.clear();
		else selected = { target };
		_selectionAnchor = target;
	}

	if (_selectionUnit == DataGridSelectionUnit::Cell
		|| !control || !extended)
	{
		if (!DeselectAllRowsWithoutCellSync()) return false;
	}
	else if (!ContainsCellIdentity(selected, target)
		&& IsIndexSelected(rowIndex))
	{
		ControlWeakReference ownerLifetime(this);
		const bool previous = _suppressRowSelectionCellSync;
		_suppressRowSelectionCellSync = true;
		try
		{
			RequestItemSelection(rowIndex, false);
			auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
			if (!live) return false;
			live->_suppressRowSelectionCellSync = previous;
		}
		catch (...)
		{
			if (auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get()))
				live->_suppressRowSelectionCellSync = previous;
			throw;
		}
	}
	const bool targetSelected = ContainsCellIdentity(selected, target);
	ApplySelectedCells(std::move(selected));
	return targetSelected;
}

void DataGrid::HandleRowSelectionChanged(SelectionChangedEventArgs&)
{
	const ControlWeakReference ownerLifetime(this);
	const auto currentRowShape = GetSelectedIndices();
	std::vector<uint32_t> previousIds;
	std::vector<uint32_t> currentIds;
	const bool coalesceFullRowAutomation =
		_selectionUnit != DataGridSelectionUnit::Cell
		&& (currentRowShape.IsRangeBacked()
			|| _selectedRowIndexSnapshot.IsRangeBacked());
	if (_selectionUnit != DataGridSelectionUnit::Cell
		&& !coalesceFullRowAutomation
		&& EnsureAccessibilityVirtualIdentities())
	{
		for (const auto& [item, index] : _selectedRowSnapshot)
			if (auto* row = EnsureAccessibilityRowIdentity(index, false);
				row && row->Item.Shared() == item.Shared())
				previousIds.push_back(row->RowId);
		for (const int index : currentRowShape)
			if (index >= 0
				&& static_cast<size_t>(index) < ItemCount())
				if (auto* row = EnsureAccessibilityRowIdentity(
					static_cast<size_t>(index), false))
					currentIds.push_back(row->RowId);
	}
	auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live) return;
	std::vector<uint32_t> removedIds;
	std::vector<uint32_t> addedIds;
	removedIds.reserve(previousIds.size());
	addedIds.reserve(currentIds.size());
	for (const uint32_t id : previousIds)
		if (std::find(currentIds.begin(), currentIds.end(), id)
			== currentIds.end())
			removedIds.push_back(id);
	for (const uint32_t id : currentIds)
		if (std::find(previousIds.begin(), previousIds.end(), id)
			== previousIds.end())
			addedIds.push_back(id);
	const auto publishChanges = [&]()
	{
		// Match DataGridAutomationPeer: a transaction that leaves exactly one
		// selected row and adds that row raises only ElementSelected.
		auto* currentOwner = dynamic_cast<DataGrid*>(ownerLifetime.Get());
		if (!currentOwner) return false;
		if (coalesceFullRowAutomation)
		{
			currentOwner->NotifyAccessibilityStateChanged();
			return ownerLifetime.Get() != nullptr;
		}
		const auto finalSelectedIndices =
			currentOwner->GetSelectedIndices();
		if (finalSelectedIndices.size() == 1
			&& addedIds.size() == 1)
		{
			if (!currentOwner->EnsureAccessibilityVirtualIdentities())
				return ownerLifetime.Get() != nullptr;
			currentOwner = dynamic_cast<DataGrid*>(ownerLifetime.Get());
			if (!currentOwner
				|| currentOwner->GetSelectedIndices() != finalSelectedIndices)
				return false;
			const int selectedIndex = finalSelectedIndices.front();
			if (selectedIndex < 0) return true;
			auto* selectedRow =
				currentOwner->EnsureAccessibilityRowIdentity(
					static_cast<size_t>(selectedIndex), false);
			currentOwner = dynamic_cast<DataGrid*>(ownerLifetime.Get());
			if (!currentOwner || !selectedRow) return currentOwner != nullptr;
			currentOwner->NotifyAccessibilityVirtualChanged(
				selectedRow->RowId,
				AccessibilityChange::SelectionSelected);
			return ownerLifetime.Get() != nullptr;
		}
		for (const uint32_t id : addedIds)
		{
			auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
			if (!live) return false;
			live->NotifyAccessibilityVirtualChanged(
				id, AccessibilityChange::SelectionAdded);
		}
		for (const uint32_t id : removedIds)
		{
			auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
			if (!live) return false;
			live->NotifyAccessibilityVirtualChanged(
				id, AccessibilityChange::SelectionRemoved);
		}
		return true;
	};
	if (live->_suppressRowSelectionCellSync)
	{
		if (live->_selectionUnit == DataGridSelectionUnit::FullRow
			&& currentRowShape.IsRangeBacked())
		{
			live->_selectedRowSnapshot.clear();
			live->_selectedRowIndexSnapshot = currentRowShape;
			(void)publishChanges();
			return;
		}
		const auto items = live->GetItemsView();
		const auto selectedIndices = currentRowShape;
		const size_t selectionRevision = live->_rowSelectionRevision;
		std::vector<std::pair<BindingSourceReference, size_t>> snapshot;
		snapshot.reserve(selectedIndices.size());
		for (const int index : selectedIndices)
		{
			BindingSourceReference item;
			const bool read = items && index >= 0
				&& items.Get()->TryGetItem(
					static_cast<size_t>(index), item);
			live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
			if (!live || live->GetItemsView().Shared() != items.Shared())
				return;
			if (live->_rowSelectionRevision != selectionRevision) return;
			if (read && item)
				snapshot.emplace_back(
					std::move(item), static_cast<size_t>(index));
		}
		live->_selectedRowSnapshot = std::move(snapshot);
		live->_selectedRowIndexSnapshot = selectedIndices;
		(void)publishChanges();
		return;
	}
	if (!live->SynchronizeSelectedCellsFromRows()) return;
	(void)publishChanges();
}

bool DataGrid::SynchronizeRangeSelectedCellsFromRows(
	SelectedIndexCollection selectedIndices)
{
	const ControlWeakReference ownerLifetime(this);
	const auto items = GetItemsView();
	const size_t selectionRevision = _rowSelectionRevision;
	if (!items || !selectedIndices.IsRangeBacked()
		|| selectedIndices.RangeCount() != items.Get()->Count()) return false;
	const auto resolveCurrent = [&]() -> DataGrid*
	{
		auto* owner = dynamic_cast<DataGrid*>(ownerLifetime.Get());
		return owner
			&& owner->_selectionUnit == DataGridSelectionUnit::FullRow
			&& owner->_rowSelectionRevision == selectionRevision
			&& owner->GetItemsView().Shared() == items.Shared()
			? owner : nullptr;
	};
	auto* live = resolveCurrent();
	if (!live) return false;
	if (live->_columns.empty())
	{
		if (!live->ApplySelectedCells({})) return false;
		live = resolveCurrent();
		if (!live) return ownerLifetime.Get() != nullptr;
		live->_selectedRowSnapshot.clear();
		live->_selectedRowIndexSnapshot = std::move(selectedIndices);
		return true;
	}

	DataGridSelectedCellCollection selected = live->_selectedCells;
	const bool reusableRegion = selected.IsRegionBacked()
		&& selected._regions.size() == 1
		&& selected._regions.front().Top == 0
		&& selected._regions.front().IncludedRowOffsets.empty()
		&& selected._ownerSourceIdentity.Shared() == live->_source.Shared()
		&& selected._source
		&& dynamic_cast<const IBindingListStableSnapshot*>(
			selected._source.Get())
		&& dynamic_cast<const IBindingListOccurrenceIdentity*>(
			selected._source.Get())
		&& dynamic_cast<const IBindingListOccurrenceLookup*>(
			selected._source.Get());
	if (!reusableRegion)
	{
		BindingListReference regionSnapshot;
		if (!live->TryGetStableSelectedCellRegionSnapshot(regionSnapshot))
			return false;
		live = resolveCurrent();
		if (!live) return false;
		std::vector<DataGridColumn*> columns;
		columns.reserve(live->_columns.size());
		for (const auto& column : live->_columns)
			if (column) columns.push_back(column.get());
		selected.SetFullRegion(live, std::move(regionSnapshot),
			selectedIndices.RangeCount(), std::move(columns));
	}
	live = resolveCurrent();
	if (!live || selected._regions.size() != 1) return false;
	auto& region = selected._regions.front();
	const auto* liveOccurrences = dynamic_cast<
		const IBindingListOccurrenceIdentity*>(items.Get());
	const auto* liveLookup = dynamic_cast<
		const IBindingListOccurrenceLookup*>(items.Get());
	const auto* snapshotLookup = dynamic_cast<
		const IBindingListOccurrenceLookup*>(selected._source.Get());
	if (!liveOccurrences || !liveLookup || !snapshotLookup) return false;

	std::vector<DataGridColumn*> columns;
	columns.reserve(live->_columns.size());
	for (const auto& column : live->_columns)
		if (column) columns.push_back(column.get());
	region.Columns = columns;
	region.ColumnIndices.resize(columns.size());
	for (size_t index = 0; index < columns.size(); ++index)
		region.ColumnIndices[index] = index;
	region.UseSnapshotLocators = false;
	selected._excludedCells.clear();

	const auto tryGetLiveOccurrence = [&](size_t index,
		BindingSourceReference& item, size_t& occurrence)
	{
		item = {};
		occurrence = DataGridCellInfo::InvalidIndex;
		if (index >= items.Get()->Count()) return false;
		const bool occurrenceRead =
			liveOccurrences->TryGetItemOccurrenceIdentity(index, occurrence);
		const bool itemRead = items.Get()->TryGetItem(index, item);
		return resolveCurrent() && occurrenceRead && itemRead && item
			&& occurrence != DataGridCellInfo::InvalidIndex;
	};
	const auto tryResolveLiveOccurrence = [&](
		const DataGridSelectedCellCollection::CellRegion::ExcludedRow& row,
		size_t& index)
	{
		index = DataGridCellInfo::InvalidIndex;
		if (!liveLookup->TryGetItemIndexByOccurrenceIdentity(
				row.Occurrence, index)) return resolveCurrent() != nullptr ? 0 : -1;
		BindingSourceReference item;
		size_t occurrence = DataGridCellInfo::InvalidIndex;
		if (!tryGetLiveOccurrence(index, item, occurrence))
			return resolveCurrent() ? 0 : -1;
		return item.Shared() == row.Item.Shared()
			&& occurrence == row.Occurrence ? 1 : 0;
	};

	std::vector<DataGridSelectedCellCollection::CellRegion::ExcludedRow>
		excludedRows;
	excludedRows.reserve(region.ExcludedRows.size()
		+ selectedIndices.ExcludedIndices().size());
	std::unordered_set<size_t> excludedOccurrences;
	excludedOccurrences.reserve(region.ExcludedRows.size()
		+ selectedIndices.ExcludedIndices().size());
	for (const auto& excluded : region.ExcludedRows)
	{
		if (IsRowInIntervals(
			region.ExcludedSnapshotRowIntervals,
			excluded.SnapshotRow)) continue;
		size_t currentIndex = DataGridCellInfo::InvalidIndex;
		const int resolved = tryResolveLiveOccurrence(excluded, currentIndex);
		if (resolved < 0) return false;
		if (resolved == 0 || currentIndex
			> static_cast<size_t>((std::numeric_limits<int>::max)())
			|| !selectedIndices.Contains(static_cast<int>(currentIndex)))
			if (excludedOccurrences.insert(excluded.Occurrence).second)
				excludedRows.push_back(excluded);
	}
	for (const int excludedIndex : selectedIndices.ExcludedIndices())
	{
		if (excludedIndex < 0) continue;
		BindingSourceReference item;
		size_t occurrence = DataGridCellInfo::InvalidIndex;
		if (!tryGetLiveOccurrence(
				static_cast<size_t>(excludedIndex), item, occurrence))
		{
			if (!resolveCurrent()) return false;
			continue;
		}
		size_t snapshotRow = DataGridCellInfo::InvalidIndex;
		if (!snapshotLookup->TryGetItemIndexByOccurrenceIdentity(
				occurrence, snapshotRow))
		{
			if (!resolveCurrent()) return false;
			continue;
		}
		BindingSourceReference snapshotItem;
		const bool snapshotRead = snapshotRow < selected._source.Get()->Count()
			&& selected._source.Get()->TryGetItem(snapshotRow, snapshotItem);
		if (!resolveCurrent()) return false;
		if (!snapshotRead || !snapshotItem
			|| snapshotItem.Shared() != item.Shared()
			|| snapshotRow >= region.Height) continue;
		if (excludedOccurrences.insert(occurrence).second)
			excludedRows.push_back({
				std::move(snapshotItem), occurrence, snapshotRow });
	}
	std::sort(excludedRows.begin(), excludedRows.end(),
		[](const auto& left, const auto& right)
		{ return left.SnapshotRow < right.SnapshotRow; });
	region.ExcludedRows = std::move(excludedRows);
	// Retired snapshot intervals deliberately have no live occurrence to remap.
	// They stay anchored to the immutable region source across sorting and row
	// reconciliation; per-occurrence user holes above still follow the live view.
	selected.InvalidateExcludedOffsets();

	const auto regionSelects = [&](const DataGridCellInfo& cell)
	{
		if (!cell.Item || cell._itemOccurrence
			== DataGridCellInfo::InvalidIndex
			|| std::find(region.Columns.begin(), region.Columns.end(),
				cell.Column) == region.Columns.end()) return false;
		size_t snapshotRow = DataGridCellInfo::InvalidIndex;
		if (!snapshotLookup->TryGetItemIndexByOccurrenceIdentity(
				cell._itemOccurrence, snapshotRow)
			|| snapshotRow >= region.Height) return false;
		return !excludedOccurrences.contains(cell._itemOccurrence);
	};
	selected._cells.erase(std::remove_if(selected._cells.begin(),
		selected._cells.end(), [&](const DataGridCellInfo& cell)
		{
			DataGridSelectedCellCollection::CellRegion::ExcludedRow identity{
				cell.Item, cell._itemOccurrence,
				DataGridCellInfo::InvalidIndex };
			size_t currentIndex = DataGridCellInfo::InvalidIndex;
			const int resolved = tryResolveLiveOccurrence(identity, currentIndex);
			if (resolved < 0) return true;
			return resolved == 0 || currentIndex
				> static_cast<size_t>((std::numeric_limits<int>::max)())
				|| !selectedIndices.Contains(static_cast<int>(currentIndex))
				|| std::find(columns.begin(), columns.end(), cell.Column)
					== columns.end()
				|| regionSelects(cell);
		}), selected._cells.end());
	if (!resolveCurrent()) return false;

	std::vector<int> newlySelected;
	if (live->_selectedRowIndexSnapshot.IsRangeBacked()
		&& live->_selectedRowIndexSnapshot.RangeCount()
			== selectedIndices.RangeCount())
		std::set_difference(
			live->_selectedRowIndexSnapshot.ExcludedIndices().begin(),
			live->_selectedRowIndexSnapshot.ExcludedIndices().end(),
			selectedIndices.ExcludedIndices().begin(),
			selectedIndices.ExcludedIndices().end(),
			std::back_inserter(newlySelected));
	for (const int index : newlySelected)
	{
		if (index < 0 || !selectedIndices.Contains(index)) continue;
		for (size_t column = 0; column < columns.size(); ++column)
		{
			live = resolveCurrent();
			if (!live) return false;
			DataGridCellInfo info;
			if (!live->TryCreateCellInfo(
					static_cast<size_t>(index), column, info))
			{
				if (!resolveCurrent()) return false;
				continue;
			}
			if (!regionSelects(info)
				&& !ContainsCellIdentity(selected._cells, info))
				selected._cells.push_back(std::move(info));
		}
	}

	// Explicit rows are those selected after they were inserted outside the
	// immutable region checkpoint. Keep their projection complete when columns
	// are added, without touching any row represented by the range itself.
	std::unordered_map<size_t, BindingSourceReference> sparseRows;
	sparseRows.reserve(selected._cells.size());
	for (const auto& cell : selected._cells)
		sparseRows.try_emplace(cell._itemOccurrence, cell.Item);
	for (const auto& [occurrence, item] : sparseRows)
	{
		DataGridSelectedCellCollection::CellRegion::ExcludedRow identity{
			item, occurrence, DataGridCellInfo::InvalidIndex };
		size_t currentIndex = DataGridCellInfo::InvalidIndex;
		const int resolved = tryResolveLiveOccurrence(identity, currentIndex);
		if (resolved < 0) return false;
		if (resolved == 0) continue;
		for (size_t column = 0; column < columns.size(); ++column)
		{
			live = resolveCurrent();
			if (!live) return false;
			DataGridCellInfo info;
			if (!live->TryCreateCellInfo(currentIndex, column, info))
			{
				if (!resolveCurrent()) return false;
				continue;
			}
			if (!regionSelects(info)
				&& !ContainsCellIdentity(selected._cells, info))
				selected._cells.push_back(std::move(info));
		}
	}

	live = resolveCurrent();
	if (!live || !live->ApplySelectedCellCollection(std::move(selected)))
		return false;
	live = resolveCurrent();
	if (!live) return ownerLifetime.Get() != nullptr;
	live->_selectedRowSnapshot.clear();
	live->_selectedRowIndexSnapshot = std::move(selectedIndices);
	return true;
}

bool DataGrid::SynchronizeSelectedCellsFromRows()
{
	const ControlWeakReference ownerLifetime(this);
	auto* live = this;
	// Ordinary cell input has no row-to-cell synchronization contract. A
	// generated-row rebuild (for example after adding a column) must retain an
	// existing lazy SelectAll region. Programmatic Selector row selection in Cell
	// mode remains row-shaped for WPF compatibility and therefore still projects
	// that selected row into cells.
	if (live->_selectionUnit == DataGridSelectionUnit::Cell)
	{
		if (!live->GetSelectedIndices().empty()
			&& !live->_selectedCells.IsRegionBacked())
		{
			// Continue into the regular row projection below.
		}
		else
		{
			live->_selectedRowSnapshot.clear();
			live->_selectedRowIndexSnapshot.Clear();
			return true;
		}
	}
	if (live->_selectionUnit == DataGridSelectionUnit::FullRow
		&& live->GetSelectedIndices().IsRangeBacked())
		return live->SynchronizeRangeSelectedCellsFromRows(
			live->GetSelectedIndices());
	const auto items = live->GetItemsView();
	const auto selectedIndices = live->GetSelectedIndices();
	const size_t selectionRevision = live->_rowSelectionRevision;
	std::vector<std::pair<BindingSourceReference, size_t>> currentRows;
	currentRows.reserve(selectedIndices.size());
	for (const int index : selectedIndices)
	{
		BindingSourceReference item;
		const bool read = items && index >= 0
			&& items.Get()->TryGetItem(static_cast<size_t>(index), item);
		live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
		if (!live || live->GetItemsView().Shared() != items.Shared()
			|| live->_rowSelectionRevision != selectionRevision) return false;
		if (read && item)
			currentRows.emplace_back(std::move(item), static_cast<size_t>(index));
	}
	BindingListReference regionSnapshot;
	if (live->_selectionUnit == DataGridSelectionUnit::FullRow
		&& selectedIndices.size() == live->ItemCount()
		&& live->TryGetStableSelectedCellRegionSnapshot(regionSnapshot))
	{
		std::vector<DataGridColumn*> columns;
		columns.reserve(live->_columns.size());
		for (const auto& column : live->_columns)
			if (column) columns.push_back(column.get());
		DataGridSelectedCellCollection selected(live);
		selected.SetFullRegion(live, std::move(regionSnapshot),
			live->ItemCount(), std::move(columns));
		live->_selectedRowSnapshot = std::move(currentRows);
		live->_selectedRowIndexSnapshot = selectedIndices;
		return live->ApplySelectedCellCollection(std::move(selected));
	}

	std::vector<DataGridCellInfo> selected =
		(live->_selectionUnit == DataGridSelectionUnit::FullRow
			|| live->_replaceCellSelectionFromRows)
		? std::vector<DataGridCellInfo>{} : live->_selectedCells._cells;
	if (live->_selectionUnit != DataGridSelectionUnit::FullRow)
	{
		for (const auto& oldRow : live->_selectedRowSnapshot)
		{
			const bool retained = std::any_of(
				currentRows.begin(), currentRows.end(),
				[&oldRow](const auto& value)
				{
					return value.first.Shared() == oldRow.first.Shared()
						&& value.second == oldRow.second;
				});
			if (retained) continue;
			selected.erase(std::remove_if(selected.begin(), selected.end(),
				[&oldRow](const DataGridCellInfo& value)
				{
					return value.Item.Shared() == oldRow.first.Shared()
						&& value.RowIndex == oldRow.second;
				}),
				selected.end());
		}
	}
	for (const auto& row : currentRows)
		for (size_t column = 0; ; ++column)
		{
			live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
			if (!live || live->GetItemsView().Shared() != items.Shared()
				|| live->_rowSelectionRevision != selectionRevision) return false;
			if (column >= live->_columns.size()) break;
			DataGridCellInfo info;
			const bool created = live->TryCreateCellInfo(
				row.second, column, info);
			live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
			if (!live || live->GetItemsView().Shared() != items.Shared()
				|| live->_rowSelectionRevision != selectionRevision || !created)
				return false;
			if (!ContainsCellIdentity(selected, info))
				selected.push_back(std::move(info));
		}
	live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live) return false;
	live->_selectedRowSnapshot = std::move(currentRows);
	live->_selectedRowIndexSnapshot = selectedIndices;
	return live->ApplySelectedCells(std::move(selected));
}

bool DataGrid::ReconcileSelectedCells()
{
	ResetSelectionRange();
	if (_selectionAnchor)
	{
		const auto previousAnchor = *_selectionAnchor;
		const auto column = std::find_if(_columns.begin(), _columns.end(),
			[&previousAnchor](const auto& value)
			{ return value.get() == previousAnchor.Column; });
		bool found = false;
		const auto items = GetItemsView();
		if (column != _columns.end() && items)
		{
			const size_t columnIndex = static_cast<size_t>(
				std::distance(_columns.begin(), column));
			size_t row = DataGridCellInfo::InvalidIndex;
			if (previousAnchor._itemOccurrence != DataGridCellInfo::InvalidIndex
				&& TryResolveItemOccurrence(
					previousAnchor.Item,
					previousAnchor._itemOccurrence,
					row))
			{
				DataGridCellInfo info;
				if (TryCreateCellInfo(row, columnIndex, info)
					&& SameCellIdentity(info, previousAnchor))
				{
					_selectionAnchor = std::move(info);
					found = true;
				}
			}
			if (!found && previousAnchor._itemOccurrence
				== DataGridCellInfo::InvalidIndex)
				for (row = 0; row < items.Get()->Count(); ++row)
				{
					DataGridCellInfo info;
					if (TryCreateCellInfo(row, columnIndex, info)
						&& SameCellIdentity(info, previousAnchor))
					{
						_selectionAnchor = std::move(info);
						found = true;
						break;
					}
				}
		}
		if (!found) _selectionAnchor.reset();
	}
	ControlWeakReference ownerLifetime(this);
	const bool previousReconcile = _reconcilingSelectedCellLocators;
	_reconcilingSelectedCellLocators = true;
	try
	{
		bool result = false;
		if (_selectedCells.IsRegionBacked())
		{
			if (_selectedCells._ownerSourceIdentity.Shared()
				!= _source.Shared())
				result = ApplySelectedCells({});
			else
			{
				auto selected = _selectedCells;
				selected.PruneColumns(_columns);
				selected._cells.erase(std::remove_if(
					selected._cells.begin(), selected._cells.end(),
					[this](DataGridCellInfo& cell)
					{
						const auto column = std::find_if(
							_columns.begin(), _columns.end(),
							[&cell](const auto& candidate)
							{ return candidate.get() == cell.Column; });
						size_t rowIndex = DataGridCellInfo::InvalidIndex;
						if (column == _columns.end()
							|| !TryResolveItemOccurrence(cell.Item,
								cell._itemOccurrence, rowIndex)) return true;
						cell.RowIndex = rowIndex;
						cell.ColumnIndex = static_cast<size_t>(
							std::distance(_columns.begin(), column));
						cell.Column = column->get();
						return false;
					}), selected._cells.end());
				for (auto& excluded : selected._excludedCells)
				{
					size_t rowIndex = DataGridCellInfo::InvalidIndex;
					if (TryResolveItemOccurrence(excluded.Item,
						excluded._itemOccurrence, rowIndex))
						excluded.RowIndex = rowIndex;
				}
				selected.InvalidateExcludedOffsets();
				result = ApplySelectedCellCollection(std::move(selected));
			}
		}
		else result = ApplySelectedCells(_selectedCells._cells);
		if (auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get()))
			live->_reconcilingSelectedCellLocators = previousReconcile;
		return result;
	}
	catch (...)
	{
		if (auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get()))
			live->_reconcilingSelectedCellLocators = previousReconcile;
		throw;
	}
}

void DataGrid::OnSelectionUnitChanged(
	DataGridSelectionUnit oldValue, DataGridSelectionUnit newValue)
{
	if (oldValue == newValue) return;
	_selectionAnchor.reset();
	ResetSelectionRange();
	if (!DeselectAllRowsWithoutCellSync()) return;
	_selectedRowSnapshot.clear();
	_selectedRowIndexSnapshot.Clear();
	ApplySelectedCells({});
}

void DataGrid::OnCellIsSelectedChanged(
	DataGridCell& cell, bool selected)
{
	if (_updatingCellSelectionVisuals || !cell.GetRowOwner()) return;
	const size_t rowIndex = cell.GetRowOwner()->ItemIndex();
	const size_t columnIndex = cell._columnIndex;
	if (_selectionUnit == DataGridSelectionUnit::FullRow)
		throw std::logic_error(
			"DataGridCell.IsSelected cannot be changed in FullRow mode");
	DataGridCellInfo info;
	if (!TryCreateCellInfo(rowIndex, columnIndex, info)) return;
	auto cells = _selectedCells;
	if (selected && GetSelectionMode() == SelectionMode::Single)
		cells.SetSparse(this, _source, { info });
	else if (cells.IsRegionBacked())
	{
		if (selected)
		{
			if (!cells.Include(info) && !cells.Contains(info))
				cells._cells.push_back(info);
		}
		else
		{
			cells._cells.erase(std::remove_if(
				cells._cells.begin(), cells._cells.end(),
				[&info](const DataGridCellInfo& value)
				{ return SameCellIdentity(value, info); }), cells._cells.end());
			(void)cells.Exclude(info);
		}
	}
	else if (selected)
	{
		if (!ContainsCellIdentity(cells._cells, info))
			cells._cells.push_back(info);
	}
	else cells._cells.erase(std::remove_if(
		cells._cells.begin(), cells._cells.end(),
		[&info](const DataGridCellInfo& value)
		{ return SameCellIdentity(value, info); }), cells._cells.end());
	if (!selected
		&& _selectionUnit == DataGridSelectionUnit::CellOrRowHeader
		&& IsIndexSelected(rowIndex))
	{
		ControlWeakReference ownerLifetime(this);
		const bool previous = _suppressRowSelectionCellSync;
		_suppressRowSelectionCellSync = true;
		try
		{
			RequestItemSelection(rowIndex, false);
			if (auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get()))
				live->_suppressRowSelectionCellSync = previous;
			else return;
		}
		catch (...)
		{
			if (auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get()))
				live->_suppressRowSelectionCellSync = previous;
			throw;
		}
	}
	if (cells.IsRegionBacked())
		(void)ApplySelectedCellCollection(std::move(cells));
	else (void)ApplySelectedCells(std::move(cells._cells));
}

bool DataGrid::SetCurrentCell(size_t rowIndex, size_t columnIndex)
{
	ControlWeakReference ownerLifetime(this);
	DataGridCellInfo next;
	if (!TryCreateCellInfo(rowIndex, columnIndex, next)) return false;
	if (_currentCell == next) return true;
	if (auto* current = ResolveCurrentCellContainer();
		current && current->GetIsEditing())
	{
		if (!CommitEdit()) return false;
		auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
		if (!live || !live->TryCreateCellInfo(
				rowIndex, columnIndex, next)) return false;
		if (live->_currentCell == next) return true;
		const auto previous = live->_currentCell;
		live->_currentCell = std::move(next);
		return live->RaiseCurrentCellChanged(previous);
	}
	const auto previous = _currentCell;
	_currentCell = std::move(next);
	return RaiseCurrentCellChanged(previous);
}

bool DataGrid::RaiseCurrentCellChanged(DataGridCellInfo previous)
{
	if (previous == _currentCell) return true;
	if (_settingItemsSource || IsItemsSourceUpdateInProgress())
	{
		if (!_currentCellChangeDeferred)
		{
			_currentCellChangeDeferred = true;
			_deferredCurrentCellOld = std::move(previous);
		}
		return true;
	}
	ControlWeakReference ownerLifetime(this);
	DataGridCurrentCellChangedEventArgs args;
	args.OldCell = std::move(previous);
	args.NewCell = _currentCell;
	bool firstHandler = true;
	cui::framework::EventAccess::RaiseWhile(
		CurrentCellChanged,
		[&]()
		{
			auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
			if (!live) return false;
			if (std::exchange(firstHandler, false)) return true;
			const auto columnIsLive = [live](const DataGridCellInfo& cell)
			{
				return !cell.Column || std::any_of(
					live->_columns.begin(), live->_columns.end(),
					[&cell](const auto& column)
					{ return column.get() == cell.Column; });
			};
			return columnIsLive(args.OldCell)
				&& columnIsLive(args.NewCell);
		},
		this, args);
	return ownerLifetime.Get() != nullptr;
}

bool DataGrid::ReconcileCurrentCellColumn()
{
	if (!_currentCell.IsValid()) return true;
	const auto found = std::find_if(
		_columns.begin(), _columns.end(),
		[this](const auto& column)
		{ return column.get() == _currentCell.Column; });
	const auto previous = _currentCell;
	if (found != _columns.end())
	{
		_currentCell.ColumnIndex = static_cast<size_t>(
			std::distance(_columns.begin(), found));
		_currentCell.Column = found->get();
	}
	else if (_columns.empty()) _currentCell = {};
	else
	{
		_currentCell.ColumnIndex = (std::min)(
			_currentCell.ColumnIndex, _columns.size() - 1);
		_currentCell.Column = _columns[_currentCell.ColumnIndex].get();
	}
	return RaiseCurrentCellChanged(previous);
}

void DataGrid::FlushCommittedItemsSourceState()
{
	ControlWeakReference ownerLifetime(this);
	if (_autoColumnsChangedDuringPreparation)
	{
		_autoColumnsChangedDuringPreparation = false;
		RefreshHeaderPresenter();
		if (!ownerLifetime.Get()) return;
	}
	const bool raiseCurrentCell = _currentCellChangeDeferred;
	auto previousCurrentCell = std::move(_deferredCurrentCellOld);
	const auto committedCurrentCell = _currentCell;
	_currentCellChangeDeferred = false;
	_deferredCurrentCellOld = {};
	const bool raiseSelectedCells = _selectedCellsChangeDeferred;
	auto previousSelectedCells = std::move(_deferredSelectedCellsOld);
	const auto committedSelectedCells = _selectedCells;
	const bool ignoreLocators = _deferredSelectedCellsIgnoreLocators;
	_selectedCellsChangeDeferred = false;
	_deferredSelectedCellsOld = {};
	_deferredSelectedCellsIgnoreLocators = false;
	if (raiseCurrentCell)
	{
		if (!(previousCurrentCell == committedCurrentCell))
		{
			DataGridCurrentCellChangedEventArgs args;
			args.OldCell = std::move(previousCurrentCell);
			args.NewCell = committedCurrentCell;
			bool firstHandler = true;
			cui::framework::EventAccess::RaiseWhile(
				CurrentCellChanged,
				[&]()
				{
					auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
					if (!live) return false;
					if (std::exchange(firstHandler, false)) return true;
					const auto columnIsLive =
						[live](const DataGridCellInfo& cell)
					{
						return !cell.Column || std::any_of(
							live->_columns.begin(), live->_columns.end(),
							[&cell](const auto& column)
							{ return column.get() == cell.Column; });
					};
					return columnIsLive(args.OldCell)
						&& columnIsLive(args.NewCell);
				},
				this, args);
			if (!ownerLifetime.Get()) return;
		}
	}
	if (raiseSelectedCells)
	{
		auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
		if (!live || !SameCellCollection(
			live->_selectedCells, committedSelectedCells, ignoreLocators)) return;
		(void)live->RaiseSelectedCellsChangedCore(
			previousSelectedCells, committedSelectedCells, ignoreLocators);
	}
	if (auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get()))
		live->FlushAutoGeneratedColumnsEvent();
}

bool DataGrid::BeginEdit()
{
	return BeginEdit(nullptr);
}

bool DataGrid::BeginEdit(const RoutedEventArgs* editingEventArgs)
{
	return BeginEditCore(editingEventArgs, false);
}

bool DataGrid::BeginEditCore(
	const RoutedEventArgs* editingEventArgs,
	bool toggleCheckBox)
{
	ControlWeakReference ownerLifetime(this);
	if (_raisingBeginningEdit || _isReadOnly
		|| !_currentCell.IsValid()) return false;
	const DataGridCellInfo editingIdentity = _currentCell;
	(void)BringItemIntoView(_currentCell.RowIndex);
	auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live || !(live->_currentCell == editingIdentity)) return false;
	if (live->_enableColumnVirtualization
		&& !live->ResolveCurrentCellContainer())
	{
		if (!live->BringColumnIntoView(editingIdentity.ColumnIndex)) return false;
		live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
		if (!live || !(live->_currentCell == editingIdentity)) return false;
	}
	auto* cell = live->ResolveCurrentCellContainer();
	auto* row = cell ? cell->GetRowOwner() : nullptr;
	if (!cell || !row || cell->GetIsReadOnly()) return false;
	if (cell->GetIsEditing()) return true;
	ControlWeakReference cellLifetime(cell);
	ControlWeakReference rowLifetime(row);
	DataGridBeginningEditEventArgs beginning;
	beginning.Column = cell->GetColumn();
	beginning.Row = row;
	beginning.Cell = cell;
	beginning.EditingEventArgs = editingEventArgs;
	{
		live->_raisingBeginningEdit = true;
		auto resetBeginning = MakeScopeExit([ownerLifetime]
		{
			if (auto* owner = dynamic_cast<DataGrid*>(ownerLifetime.Get()))
				owner->_raisingBeginningEdit = false;
		});
		cui::framework::EventAccess::RaiseWhile(
			live->BeginningEdit,
			[&]()
			{
				auto* owner = dynamic_cast<DataGrid*>(ownerLifetime.Get());
				return owner && cellLifetime.Get() && rowLifetime.Get()
					&& std::any_of(owner->_columns.begin(),
						owner->_columns.end(), [&](const auto& candidate)
						{ return candidate.get() == beginning.Column; });
			},
			live, beginning);
	}
	if (beginning.Cancel) return false;
	live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	cell = dynamic_cast<DataGridCell*>(cellLifetime.Get());
	if (!live || !cell || live->ResolveCurrentCellContainer() != cell
		|| !(live->_currentCell == editingIdentity)) return false;
	row = cell->GetRowOwner();
	if (!row) return false;
	std::wstring error;
	const bool replaced = cell->ReplaceContent(true, &error);
	live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	cell = dynamic_cast<DataGridCell*>(cellLifetime.Get());
	if (!live || !cell || live->ResolveCurrentCellContainer() != cell
		|| !(live->_currentCell == editingIdentity)) return false;
	if (!replaced)
	{
		live->SetLastTemplateError(std::move(error));
		return false;
	}
	row = cell->GetRowOwner();
	if (!row || !cell->GetIsEditing()) return false;
	auto* editingElement = cell->GetEditingElement();
	if (auto* text = dynamic_cast<TextBox*>(editingElement))
	{
		const ControlWeakReference editorLifetime(text);
		if (auto* window = live->GetPresentationWindow())
			window->SetKeyboardFocus(text, false);
		live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
		cell = dynamic_cast<DataGridCell*>(cellLifetime.Get());
		text = dynamic_cast<TextBox*>(editorLifetime.Get());
		if (!live || !cell || !text
			|| live->ResolveCurrentCellContainer() != cell
			|| cell->GetEditingElement() != text
			|| !(live->_currentCell == editingIdentity)
			|| !cell->GetIsEditing()) return false;
		if (const auto* composition =
			dynamic_cast<const TextCompositionEventArgs*>(editingEventArgs))
		{
			text->SelectAll();
			text = dynamic_cast<TextBox*>(editorLifetime.Get());
			if (!text) return false;
			text->InsertText(composition->Text);
		}
		else if (const auto* mouse =
			dynamic_cast<const MouseEventArgs*>(editingEventArgs))
		{
			D2D1_POINT_2F renderPoint{};
			bool hasPoint = mouse->HasRootPosition;
			if (hasPoint)
				renderPoint = D2D1::Point2F(mouse->RootX, mouse->RootY);
			else if (cell)
			{
				renderPoint = ToRenderSpace(
					*cell,
					static_cast<float>(mouse->X),
					static_cast<float>(mouse->Y));
				hasPoint = true;
			}
			D2D1_POINT_2F local{};
			if (hasPoint && text->TryTransformRenderPointToLocal(
				renderPoint, local))
				text->SetCaretIndex(text->GetCharacterIndexFromPoint(
					local.x, local.y));
			else text->SelectAll();
		}
		else text->SelectAll();
		live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
		cell = dynamic_cast<DataGridCell*>(cellLifetime.Get());
		if (!live || !cell || live->ResolveCurrentCellContainer() != cell
			|| !(live->_currentCell == editingIdentity)
			|| !cell->GetIsEditing()) return false;
		row = cell->GetRowOwner();
		if (!row) return false;
	}
	if (toggleCheckBox)
	{
		auto* checkBox = dynamic_cast<CheckBox*>(cell->GetEditingElement());
		if (checkBox)
		{
			ControlWeakReference editorLifetime(checkBox);
			checkBox->Toggle();
			live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
			cell = dynamic_cast<DataGridCell*>(cellLifetime.Get());
			if (!live || !cell || !editorLifetime.Get()
				|| live->ResolveCurrentCellContainer() != cell
				|| !(live->_currentCell == editingIdentity)
				|| cell->GetEditingElement() != editorLifetime.Get()
				|| !cell->GetIsEditing()) return false;
			row = cell->GetRowOwner();
			if (!row) return false;
		}
	}
	DataGridPreparingCellForEditEventArgs preparing;
	preparing.Column = cell->GetColumn();
	preparing.Row = row;
	preparing.Cell = cell;
	preparing.EditingElement = cell->GetEditingElement();
	preparing.EditingEventArgs = editingEventArgs;
	ControlWeakReference preparingRowLifetime(row);
	ControlWeakReference editorLifetime(preparing.EditingElement);
	cui::framework::EventAccess::RaiseWhile(
		live->PreparingCellForEdit,
		[&]()
		{
			auto* owner = dynamic_cast<DataGrid*>(ownerLifetime.Get());
			return owner && cellLifetime.Get() && preparingRowLifetime.Get()
				&& (!preparing.EditingElement || editorLifetime.Get())
				&& std::any_of(owner->_columns.begin(),
					owner->_columns.end(), [&](const auto& candidate)
					{ return candidate.get() == preparing.Column; });
		},
		live, preparing);
	live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	cell = dynamic_cast<DataGridCell*>(cellLifetime.Get());
	if (!live || !cell || live->ResolveCurrentCellContainer() != cell
		|| !(live->_currentCell == editingIdentity)
		|| !cell->GetIsEditing()) return false;
	if (auto* editor = cell->GetEditingElement())
	{
		const ControlWeakReference finalEditorLifetime(editor);
		if (auto* window = live->GetPresentationWindow();
			window && window->GetKeyboardFocusedElement() != editor)
			window->SetKeyboardFocus(editor, false);
		live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
		cell = dynamic_cast<DataGridCell*>(cellLifetime.Get());
		editor = finalEditorLifetime.Get();
		if (!live || !cell || !editor
			|| live->ResolveCurrentCellContainer() != cell
			|| !(live->_currentCell == editingIdentity)
			|| cell->GetEditingElement() != editor
			|| !cell->GetIsEditing()) return false;
		// Binding transfer, activation-specific caret/selection preparation and
		// PreparingCellForEdit may all happen after the editor's first retained
		// command list was recorded.  Publish one final content revision so the
		// first visible edit frame already contains the bound text and selection,
		// instead of waiting for the user's next pointer/key report.
		editor->InvalidateVisual();
	}
	return ownerLifetime.Get() != nullptr;
}

bool DataGrid::CommitEdit()
{
	ControlWeakReference ownerLifetime(this);
	if (_endingCellEdit) return false;
	auto* cell = ResolveCurrentCellContainer();
	if (!cell || !cell->GetIsEditing()) return false;
	_endingCellEdit = true;
	auto resetEnding = MakeScopeExit([ownerLifetime]
	{
		if (auto* owner = dynamic_cast<DataGrid*>(ownerLifetime.Get()))
			owner->_endingCellEdit = false;
	});
	const DataGridCellInfo editingIdentity = _currentCell;
	ControlWeakReference cellLifetime(cell);
	ControlWeakReference editorLifetime(cell->GetEditingElement());
	DataGridCellEditEndingEventArgs ending;
	ending.Column = cell->GetColumn();
	ending.Row = cell->GetRowOwner();
	ending.Cell = cell;
	ending.EditingElement = cell->GetEditingElement();
	ending.EditAction = DataGridEditAction::Commit;
	ControlWeakReference rowLifetime(ending.Row);
	cui::framework::EventAccess::RaiseWhile(
		CellEditEnding,
		[&]()
		{
			auto* owner = dynamic_cast<DataGrid*>(ownerLifetime.Get());
			return owner && cellLifetime.Get() && rowLifetime.Get()
				&& (!ending.EditingElement || editorLifetime.Get())
				&& std::any_of(owner->_columns.begin(),
					owner->_columns.end(), [&](const auto& candidate)
					{ return candidate.get() == ending.Column; });
		},
		this, ending);
	if (ending.Cancel) return false;
	auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	cell = dynamic_cast<DataGridCell*>(cellLifetime.Get());
	if (!live || !cell || live->ResolveCurrentCellContainer() != cell
		|| !(live->_currentCell == editingIdentity)
		|| !cell->GetIsEditing()) return false;
	if (auto* editor = dynamic_cast<Control*>(editorLifetime.Get()))
	{
		if (cell->GetEditingElement() != editor) return false;
		bool updated = true;
		if (dynamic_cast<TextBox*>(editor))
			updated = editor->DataBindings.UpdateSource(TextBox::TextProperty());
		else if (dynamic_cast<CheckBox*>(editor))
			updated = editor->DataBindings.UpdateSource(
				ToggleButton::IsCheckedProperty());
		if (!updated) return false;
		live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
		cell = dynamic_cast<DataGridCell*>(cellLifetime.Get());
		if (!live || !cell || live->ResolveCurrentCellContainer() != cell
			|| !(live->_currentCell == editingIdentity)
			|| !cell->GetIsEditing()) return false;
	}
	std::wstring error;
	const bool replaced = cell->ReplaceContent(false, &error);
	live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live) return false;
	if (!replaced)
	{
		live->SetLastTemplateError(std::move(error));
		return false;
	}
	cell = dynamic_cast<DataGridCell*>(cellLifetime.Get());
	if (auto* window = live->GetPresentationWindow())
	{
		auto* focusTarget = cell && live->ResolveCurrentCellContainer() == cell
			? static_cast<Control*>(cell) : static_cast<Control*>(live);
		window->SetKeyboardFocus(focusTarget, true);
	}
	live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live) return false;
	// The binding source has just accepted an edited value.  It need not publish
	// a collection Replace notification, so explicitly retire the sampled cell
	// widths and resolve Auto/SizeToCells columns against the committed value.
	live->InvalidateColumnContentWidthCache();
	live->RefreshColumnWidths();
	return ownerLifetime.Get() != nullptr;
}

bool DataGrid::CancelEdit()
{
	ControlWeakReference ownerLifetime(this);
	if (_endingCellEdit) return false;
	auto* cell = ResolveCurrentCellContainer();
	if (!cell || !cell->GetIsEditing()) return false;
	_endingCellEdit = true;
	auto resetEnding = MakeScopeExit([ownerLifetime]
	{
		if (auto* owner = dynamic_cast<DataGrid*>(ownerLifetime.Get()))
			owner->_endingCellEdit = false;
	});
	const DataGridCellInfo editingIdentity = _currentCell;
	ControlWeakReference cellLifetime(cell);
	ControlWeakReference editorLifetime(cell->GetEditingElement());
	DataGridCellEditEndingEventArgs ending;
	ending.Column = cell->GetColumn();
	ending.Row = cell->GetRowOwner();
	ending.Cell = cell;
	ending.EditingElement = cell->GetEditingElement();
	ending.EditAction = DataGridEditAction::Cancel;
	ControlWeakReference rowLifetime(ending.Row);
	cui::framework::EventAccess::RaiseWhile(
		CellEditEnding,
		[&]()
		{
			auto* owner = dynamic_cast<DataGrid*>(ownerLifetime.Get());
			return owner && cellLifetime.Get() && rowLifetime.Get()
				&& (!ending.EditingElement || editorLifetime.Get())
				&& std::any_of(owner->_columns.begin(),
					owner->_columns.end(), [&](const auto& candidate)
					{ return candidate.get() == ending.Column; });
		},
		this, ending);
	if (ending.Cancel) return false;
	auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	cell = dynamic_cast<DataGridCell*>(cellLifetime.Get());
	if (!live || !cell || live->ResolveCurrentCellContainer() != cell
		|| !(live->_currentCell == editingIdentity)
		|| !cell->GetIsEditing()) return false;
	std::wstring error;
	const bool replaced = cell->ReplaceContent(false, &error);
	live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live) return false;
	if (!replaced)
	{
		live->SetLastTemplateError(std::move(error));
		return false;
	}
	cell = dynamic_cast<DataGridCell*>(cellLifetime.Get());
	if (auto* window = live->GetPresentationWindow())
	{
		auto* focusTarget = cell && live->ResolveCurrentCellContainer() == cell
			? static_cast<Control*>(cell) : static_cast<Control*>(live);
		window->SetKeyboardFocus(focusTarget, true);
	}
	return ownerLifetime.Get() != nullptr;
}

CollectionSortDescription DataGrid::MakeSortDescription(
	const DataGridColumn& column,
	CollectionSortDirection direction) const
{
	CollectionSortDescription result;
	result.Direction = direction;
	result.IgnoreCase = true;
#if CUI_ENABLE_DYNAMIC_XAML
	result.PropertyName = column.GetSortMemberPath();
	if (result.PropertyName.empty())
	{
		if (const auto* bound = dynamic_cast<const DataGridBoundColumn*>(&column))
			result.PropertyName = bound->GetBindingPath();
	}
#endif
	result.CompiledPath = column.GetCompiledSortMemberPath();
	if (result.CompiledPath.Empty())
	{
		if (const auto* bound = dynamic_cast<const DataGridBoundColumn*>(&column))
			result.CompiledPath = bound->GetCompiledBindingPath();
	}
	return result;
}

bool DataGrid::PerformSort(DataGridColumn& column, bool multiColumn)
{
	const ControlWeakReference ownerLifetime(this);
	auto* const requestedColumn = &column;
	if (!_canUserSortColumns || !column.GetCanUserSort()
		|| column.GetDataGridOwner() != this) return false;
	const auto items = GetItemsView();
	if (!items) return false;
	const auto direction = column._sortDirection
		== CollectionSortDirection::Ascending
		? CollectionSortDirection::Descending
		: CollectionSortDirection::Ascending;
	if (auto* cell = ResolveCurrentCellContainer();
		cell && cell->GetIsEditing() && !CommitEdit()) return false;
	auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live || live->GetItemsView().Shared() != items.Shared()) return false;
	auto columnIt = std::find_if(live->_columns.begin(), live->_columns.end(),
		[requestedColumn](const auto& candidate)
		{ return candidate.get() == requestedColumn; });
	if (columnIt == live->_columns.end()) return false;
	auto* liveColumn = columnIt->get();
	DataGridSortingEventArgs args;
	args.Column = liveColumn;
	args.Direction = direction;
	args.MultiColumn = multiColumn;
	cui::framework::EventAccess::RaiseWhile(
		live->Sorting,
		[&]()
		{
			auto* owner = dynamic_cast<DataGrid*>(ownerLifetime.Get());
			return owner && std::any_of(owner->_columns.begin(),
				owner->_columns.end(), [requestedColumn](const auto& candidate)
				{ return candidate.get() == requestedColumn; });
		},
		live, args);
	if (args.Handled) return true;
	live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live || live->GetItemsView().Shared() != items.Shared()) return false;
	columnIt = std::find_if(live->_columns.begin(), live->_columns.end(),
		[requestedColumn](const auto& candidate)
		{ return candidate.get() == requestedColumn; });
	if (columnIt == live->_columns.end()) return false;
	liveColumn = columnIt->get();
	auto* itemsView = dynamic_cast<CollectionViewSource*>(
		live->GetItemsView().Get());
	if (!itemsView) return false;
	auto description = live->MakeSortDescription(*liveColumn, args.Direction);
	if (!HasSortPath(description)) return false;
	multiColumn = args.MultiColumn;
	auto descriptions = multiColumn
		? itemsView->SortDescriptions()
		: std::vector<CollectionSortDescription>{};
	auto found = std::find_if(descriptions.begin(), descriptions.end(),
		[&](const auto& candidate)
		{ return SameSortPath(candidate, description); });
	if (found == descriptions.end()) descriptions.push_back(description);
	else *found = description;
	const auto previousDescriptions = itemsView->SortDescriptions();
	std::vector<std::pair<DataGridColumn*,
		std::optional<CollectionSortDirection>>> previousDirections;
	previousDirections.reserve(live->_columns.size());
	for (const auto& candidate : live->_columns)
		previousDirections.emplace_back(
			candidate.get(), candidate->_sortDirection);
	if (!multiColumn)
	{
		for (auto& candidate : live->_columns)
		{
			if (candidate.get() != liveColumn)
				candidate->_sortDirection.reset();
		}
	}
	liveColumn->_sortDirection = args.Direction;
	try
	{
		itemsView->SetSortDescriptions(std::move(descriptions));
	}
	catch (...)
	{
		const auto error = std::current_exception();
		live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
		if (live && live->GetItemsView().Shared() == items.Shared())
		{
			try
			{
				itemsView->SetSortDescriptions(previousDescriptions);
			}
			catch (...) {}
		}
		live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
		if (live)
		{
			for (const auto& [candidate, oldDirection]
				: previousDirections)
			{
				auto foundColumn = std::find_if(
					live->_columns.begin(), live->_columns.end(),
					[candidate](const auto& current)
					{ return current.get() == candidate; });
				if (foundColumn != live->_columns.end())
				{
					(*foundColumn)->_sortDirection = oldDirection;
				}
			}
		}
		std::rethrow_exception(error);
	}
	// Do not replace the headers while DataGridColumnHeader::OnClick is still
	// executing on one of them.  SortDirection is column state (the first theme
	// has no sort glyph); a future glyph can update that header in place.
	return true;
}

bool DataGrid::ResizeColumn(size_t columnIndex, double pixelWidth)
{
	return ResizeColumnCore(columnIndex, pixelWidth, false);
}

bool DataGrid::ResizeColumnCore(
	size_t columnIndex, double pixelWidth,
	bool preserveRealizedColumnRange)
{
	if (!_canUserResizeColumns || columnIndex >= _columns.size()
		|| !std::isfinite(pixelWidth)) return false;
	auto& column = *_columns[columnIndex];
	if (!column.GetCanUserResize()) return false;
	if (!_columnResizeSnapshot.empty()) EndColumnResizeTransaction(true);
	const double width = (std::clamp)(
		pixelWidth, column.GetMinWidth(), column.GetMaxWidth());
	const DataGridLength resized(width);
	if (column._width == resized) return true;
	column._width = resized;
	column._runtimeWidth = {};
	// Native header dragging preserves the existing overscanned horizontal
	// container strip. Recomputing it on every WM_MOUSEMOVE can recreate
	// template content and makes the gripper lag behind the pointer; release
	// performs the one final range reconciliation. Programmatic resize keeps the
	// immediate behavior.
	RefreshColumnWidths(preserveRealizedColumnRange);
	return true;
}

bool DataGrid::AutoSizeColumn(size_t columnIndex)
{
	if (!_canUserResizeColumns || columnIndex >= _columns.size()) return false;
	auto& column = *_columns[columnIndex];
	if (!column.GetCanUserResize()) return false;
	if (!_columnResizeSnapshot.empty()) EndColumnResizeTransaction(true);
	column._width = DataGridLength::Auto();
	column._runtimeWidth = {};
	RefreshColumnWidths();
	return true;
}

GridLength DataGrid::ResolveColumnGridLength(size_t columnIndex) const
{
	if (columnIndex >= _columns.size()) return GridLength::Auto();
	if (_resolvedColumnWidths.size() != _columns.size()
		|| !_resolvedColumnWidths[columnIndex])
		RebuildResolvedColumnWidths();
	return columnIndex < _resolvedColumnWidths.size()
		&& _resolvedColumnWidths[columnIndex]
		? *_resolvedColumnWidths[columnIndex]
		: GridLength::Pixels(0.0f);
}

void DataGrid::InvalidateColumnWidthCache() noexcept
{
	_resolvedColumnWidths.clear();
	_columnWidthPrefix.clear();
	if (++_columnWidthProjectionRevision == 0)
		_columnWidthProjectionRevision = 1;
}

bool DataGrid::BeginColumnResizeTransaction(
	size_t columnIndex, bool compensateLeft)
{
	if (!_canUserResizeColumns || columnIndex >= _columns.size()
		|| !_columns[columnIndex]->GetCanUserResize()
		|| !_columnResizeSnapshot.empty()) return false;

	std::vector<ColumnResizeSnapshot> snapshot;
	snapshot.reserve(_columns.size());
	for (size_t index = 0; index < _columns.size(); ++index)
	{
		auto& column = *_columns[index];
		const auto resolved = ResolveColumnGridLength(index);
		const double display = resolved.IsPixel()
			? static_cast<double>(resolved.Value)
			: EstimateColumnWidth(index, true, true);
		if (!std::isfinite(display)) return false;
		double desired = display;
		if (std::isfinite(column._runtimeWidth.Desired))
			desired = column._runtimeWidth.Desired;
		else if (column._width.UnitType == DataGridLengthUnitType::Pixel)
			desired = column._width.Value;
		else if (column._width.UnitType == DataGridLengthUnitType::Star
			&& column._width.Value > 0.000001)
		{
			// The global water-fill solver does not expose WPF's DesiredValue.
			// Recover its common unconstrained water level from any Star which is
			// not currently clamped, then apply it to every Star below.
			desired = (std::numeric_limits<double>::quiet_NaN)();
		}
		snapshot.push_back(ColumnResizeSnapshot{
			&column, column._width, column._runtimeWidth,
			desired, display });
	}
	double starWaterLevel = (std::numeric_limits<double>::quiet_NaN)();
	for (const auto& state : snapshot)
	{
		if (state.Width.UnitType != DataGridLengthUnitType::Star
			|| state.Width.Value <= 0.000001) continue;
		const auto* column = state.Column;
		if (!column || state.Display <= column->GetMinWidth() + 0.000001
			|| state.Display >= column->GetMaxWidth() - 0.000001) continue;
		starWaterLevel = state.Display / state.Width.Value;
		break;
	}
	if (!std::isfinite(starWaterLevel) || starWaterLevel < 0.0)
	{
		double starDisplay = 0.0;
		double starWeight = 0.0;
		for (const auto& state : snapshot)
		{
			if (state.Width.UnitType != DataGridLengthUnitType::Star
				|| state.Width.Value <= 0.000001) continue;
			starDisplay += state.Display;
			starWeight += state.Width.Value;
		}
		starWaterLevel = starWeight > 0.000001
			? starDisplay / starWeight : 0.0;
	}
	for (auto& state : snapshot)
	{
		if (state.Width.UnitType == DataGridLengthUnitType::Star
			&& state.Width.Value > 0.000001
			&& !std::isfinite(state.Desired))
			state.Desired = starWaterLevel * state.Width.Value;
	}
	auto workingSnapshot = snapshot;
	_columnResizeSnapshot = std::move(snapshot);
	_columnResizeWorkingSnapshot = std::move(workingSnapshot);
	_columnResizeDisplayScratch.resize(_columns.size());
	_columnResizeDesiredScratch.resize(_columns.size());
	_columnResizeFactorScratch.resize(_columns.size());
	_columnResizePreviousDisplayScratch.resize(_columns.size());
	_columnResizeTransactionIndex = columnIndex;
	_columnResizeCompensatesLeft = compensateLeft;
	_columnResizeLastRawWidth = _columnResizeWorkingSnapshot[columnIndex].Display;
	_columnResizeInputBias = 0.0;
	_columnWidthDirtyBegin = DataGridCellInfo::InvalidIndex;
	_columnWidthDirtyEnd = DataGridCellInfo::InvalidIndex;
	_columnWidthDirtyVisualSpan =
		(std::numeric_limits<double>::quiet_NaN)();
	_columnWidthMeasureDirty.clear();
	return true;
}

bool DataGrid::RebaseColumnResizeTransaction()
{
	if (_columnResizeSnapshot.empty()
		|| _columnResizeTransactionIndex >= _columns.size()) return false;
	std::vector<ColumnResizeSnapshot> snapshot;
	snapshot.reserve(_columns.size());
	for (size_t index = 0; index < _columns.size(); ++index)
	{
		auto& column = *_columns[index];
		const auto resolved = ResolveColumnGridLength(index);
		const double display = resolved.IsPixel()
			? static_cast<double>(resolved.Value)
			: EstimateColumnWidth(index, true, true);
		if (!std::isfinite(display)) return false;
		const double desired = std::isfinite(column._runtimeWidth.Desired)
			? column._runtimeWidth.Desired
			: (column._width.UnitType == DataGridLengthUnitType::Pixel
				? column._width.Value : display);
		snapshot.push_back(ColumnResizeSnapshot{
			&column, column._width, column._runtimeWidth,
			desired, display });
	}
	_columnResizeWorkingSnapshot = std::move(snapshot);
	return true;
}

bool DataGrid::ResizeColumnInTransaction(
	size_t columnIndex, double pixelWidth)
{
	if (!std::isfinite(pixelWidth)
		|| columnIndex != _columnResizeTransactionIndex
		|| _columnResizeSnapshot.size() != _columns.size()
		|| _columnResizeWorkingSnapshot.size() != _columns.size()) return false;
	if (!_canUserResizeColumns || !_columns[columnIndex]->GetCanUserResize())
	{
		EndColumnResizeTransaction(true);
		return false;
	}
	_columnResizeLastRawWidth = pixelWidth;
	pixelWidth += _columnResizeInputBias;
	bool previousProjectionDiffers = false;
	for (size_t index = 0; index < _columns.size(); ++index)
	{
		if (_columns[index].get() != _columnResizeSnapshot[index].Column
			|| _columns[index].get()
				!= _columnResizeWorkingSnapshot[index].Column)
		{
			_columnResizeSnapshot.clear();
			_columnResizeWorkingSnapshot.clear();
			_columnResizeTransactionIndex = DataGridCellInfo::InvalidIndex;
			_columnResizeCompensatesLeft = false;
			return false;
		}
		const auto& runtime = _columns[index]->_runtimeWidth;
		const auto& startRuntime =
			_columnResizeWorkingSnapshot[index].RuntimeWidth;
		previousProjectionDiffers = previousProjectionDiffers
			|| _columns[index]->_width
				!= _columnResizeWorkingSnapshot[index].Width
			|| runtime.HasDisplayOverride
				!= startRuntime.HasDisplayOverride
			|| (runtime.HasDisplayOverride
				&& (std::abs(runtime.Display - startRuntime.Display) > 0.000001
					|| std::abs(runtime.Desired - startRuntime.Desired)
						> 0.000001));
	}
	auto& previousDisplay = _columnResizePreviousDisplayScratch;
	previousDisplay.resize(_columns.size());
	for (size_t index = 0; index < _columns.size(); ++index)
	{
		const double current = _columns[index]->_runtimeWidth.Display;
		previousDisplay[index] = std::isfinite(current)
			? current : _columnResizeWorkingSnapshot[index].Display;
	}

	// Pointer moves carry an absolute displacement from drag start. Restore the
	// complete semantic snapshot first so min/max saturation is reversible and
	// repeated input never accumulates rounding error.
	for (size_t index = 0; index < _columns.size(); ++index)
	{
		_columns[index]->_width = _columnResizeWorkingSnapshot[index].Width;
		_columns[index]->_runtimeWidth =
			_columnResizeWorkingSnapshot[index].RuntimeWidth;
	}

	auto& display = _columnResizeDisplayScratch;
	auto& desired = _columnResizeDesiredScratch;
	display.resize(_columnResizeWorkingSnapshot.size());
	desired.resize(_columnResizeWorkingSnapshot.size());
	for (size_t index = 0;
		index < _columnResizeWorkingSnapshot.size(); ++index)
	{
		display[index] = _columnResizeWorkingSnapshot[index].Display;
		desired[index] = _columnResizeWorkingSnapshot[index].Desired;
	}
	const double startTargetWidth = display[columnIndex];
	const auto& target = *_columns[columnIndex];
	const double requestedTargetWidth = (std::clamp)(
		pixelWidth, target.GetMinWidth(), target.GetMaxWidth());
	double delta = requestedTargetWidth - startTargetWidth;
	if (std::abs(delta) <= 0.000001 && !previousProjectionDiffers)
		return true;

	{
		const auto resizeStars = [&](
			double amount, bool shrink, size_t begin, size_t end)
		{
			constexpr double epsilon = 0.000001;
			double perStarWidth = (std::numeric_limits<double>::quiet_NaN)();
			for (size_t index = 0; index < _columns.size(); ++index)
			{
				const auto& state = _columnResizeWorkingSnapshot[index];
				if (state.Width.UnitType != DataGridLengthUnitType::Star
					|| state.Width.Value <= 0.0
					|| !std::isfinite(desired[index])) continue;
				const double candidate = desired[index] / state.Width.Value;
				if (std::isfinite(candidate) && candidate > epsilon)
				{
					perStarWidth = candidate;
					break;
				}
			}
			if (!std::isfinite(perStarWidth) || perStarWidth <= epsilon)
				perStarWidth = 1.0;
			auto& workingFactor = _columnResizeFactorScratch;
			workingFactor.assign(_columns.size(), 0.0);
			for (size_t index = begin; index < end; ++index)
			{
				if (_columnResizeWorkingSnapshot[index].Width.UnitType
					== DataGridLengthUnitType::Star)
					workingFactor[index] = (std::max)(
						0.0,
						_columnResizeWorkingSnapshot[index].Width.Value);
			}
			while (amount > epsilon)
			{
				double totalWeight = 0.0;
				double ratio = (std::numeric_limits<double>::infinity)();
				for (size_t index = begin; index < end; ++index)
				{
					const auto& state =
						_columnResizeWorkingSnapshot[index];
					const auto& column = *_columns[index];
					if (state.Width.UnitType
							!= DataGridLengthUnitType::Star
						|| !column.GetCanUserResize()
						|| workingFactor[index] <= 0.0) continue;
					const double weight = workingFactor[index];
					const double capacity = shrink
						? display[index] - column.GetMinWidth()
						: column.GetMaxWidth() - display[index];
					if (capacity <= epsilon) continue;
					totalWeight += weight;
					ratio = (std::min)(
						ratio, capacity / weight);
				}
				if (totalWeight <= epsilon) break;
				ratio = (std::min)(ratio, amount / totalWeight);
				if (!std::isfinite(ratio)) break;
				if (ratio <= epsilon) break;
				double consumed = 0.0;
				for (size_t index = begin; index < end; ++index)
				{
					const auto& state =
						_columnResizeWorkingSnapshot[index];
					const auto& column = *_columns[index];
					if (state.Width.UnitType
							!= DataGridLengthUnitType::Star
						|| !column.GetCanUserResize()
						|| workingFactor[index] <= 0.0) continue;
					const double weight = workingFactor[index];
					const double capacity = shrink
						? display[index] - column.GetMinWidth()
						: column.GetMaxWidth() - display[index];
					if (capacity <= epsilon) continue;
					const double change = (std::min)(
						capacity, ratio * weight);
					display[index] += shrink ? -change : change;
					desired[index] = display[index];
					workingFactor[index] = display[index] / perStarWidth;
					consumed += change;
				}
				if (consumed <= epsilon) break;
				amount -= consumed;
			}
			return (std::max)(0.0, amount);
		};
		const auto resizeNonStars = [&](
			double amount, bool shrink, bool towardDesired,
			size_t begin, size_t end, bool reverse)
		{
			const auto resizeOne = [&](size_t index)
			{
				const auto& state = _columnResizeWorkingSnapshot[index];
				auto& column = *_columns[index];
				if (state.Width.UnitType == DataGridLengthUnitType::Star
					|| !column.GetCanUserResize()) return;
				const double threshold = towardDesired
					? (std::clamp)(desired[index],
						column.GetMinWidth(), column.GetMaxWidth())
					: (shrink ? column.GetMinWidth() : column.GetMaxWidth());
				const double capacity = shrink
					? display[index] - threshold
					: threshold - display[index];
				const double change = (std::min)(
					amount, (std::max)(0.0, capacity));
				display[index] += shrink ? -change : change;
				amount -= change;
			};
			if (reverse)
			{
				for (size_t index = end;
					index-- > begin && amount > 0.000001;)
					resizeOne(index);
			}
			else
			{
				for (size_t index = begin;
					index < end && amount > 0.000001; ++index)
					resizeOne(index);
			}
			return (std::max)(0.0, amount);
		};
		const size_t compensationBegin = _columnResizeCompensatesLeft
			? 0 : columnIndex + 1;
		const size_t compensationEnd = _columnResizeCompensatesLeft
			? columnIndex : _columns.size();

		if (delta > 0.0)
		{
			double remaining = delta;
			// Only a right-edge gesture can consume unused space on the right
			// without moving the opposite edge away from the pointer.
			if (!_columnResizeCompensatesLeft)
			{
				double totalDisplay = 0.0;
				for (const double width : display) totalDisplay += width;
				const double unused = std::isfinite(_columnViewportWidth)
					? (std::max)(0.0, _columnViewportWidth - totalDisplay)
					: 0.0;
				const double fromUnused = (std::min)(remaining, unused);
				display[columnIndex] += fromUnused;
				remaining -= fromUnused;
			}

			double before = remaining;
			remaining = resizeNonStars(
				remaining, true, true,
				compensationBegin, compensationEnd, true);
			display[columnIndex] += before - remaining;

			const double beforeStars = remaining;
			remaining = resizeStars(
				remaining, true, compensationBegin, compensationEnd);
			display[columnIndex] += beforeStars - remaining;

			before = remaining;
			remaining = resizeNonStars(
				remaining, true, false,
				compensationBegin, compensationEnd, true);
			display[columnIndex] += before - remaining;

			// A right edge can grow the scroll extent after its right-hand donors
			// saturate. A left edge is anchored by the grid origin, so it clamps
			// when its left-hand donors cannot move any farther.
			if (!_columnResizeCompensatesLeft)
				display[columnIndex] += remaining;
		}
		else
		{
			double remaining = -delta;
			const bool reverse = _columnResizeCompensatesLeft;
			double before = remaining;
			remaining = resizeNonStars(
				remaining, false, true,
				compensationBegin, compensationEnd, reverse);
			display[columnIndex] -= before - remaining;

			const double beforeStars = remaining;
			remaining = resizeStars(
				remaining, false, compensationBegin, compensationEnd);
			display[columnIndex] -= beforeStars - remaining;

			before = remaining;
			remaining = resizeNonStars(
				remaining, false, false,
				compensationBegin, compensationEnd, reverse);
			display[columnIndex] -= before - remaining;

			if (!_columnResizeCompensatesLeft)
				display[columnIndex] -= remaining;
		}
	}

	bool changed = false;
	for (size_t index = 0; index < display.size(); ++index)
	{
		if (std::abs(display[index]
			- _columnResizeWorkingSnapshot[index].Display) > 0.000001)
		{
			changed = true;
			break;
		}
	}

	// Record exactly which cell slots change geometry. When a target and donor
	// exchange the same amount, columns after the donor keep both their width and
	// left edge and do not need to participate in the drag-frame Arrange pass.
	double oldLeft = 0.0;
	double newLeft = 0.0;
	size_t dirtyBegin = DataGridCellInfo::InvalidIndex;
	size_t dirtyEnd = DataGridCellInfo::InvalidIndex;
	for (size_t index = 0; index < display.size(); ++index)
	{
		const double oldWidth = previousDisplay[index];
		const double newWidth = display[index];
		if (std::abs(oldLeft - newLeft) > 0.000001
			|| std::abs(oldWidth - newWidth) > 0.000001)
		{
			if (dirtyBegin == DataGridCellInfo::InvalidIndex)
				dirtyBegin = index;
			dirtyEnd = index + 1;
		}
		oldLeft += oldWidth;
		newLeft += newWidth;
	}
	_columnWidthDirtyBegin = dirtyBegin;
	_columnWidthDirtyEnd = dirtyEnd;
	_columnWidthDirtyVisualSpan =
		(std::numeric_limits<double>::quiet_NaN)();
	if (dirtyBegin != DataGridCellInfo::InvalidIndex
		&& dirtyEnd != DataGridCellInfo::InvalidIndex)
	{
		double oldSpan = 0.0;
		double newSpan = 0.0;
		for (size_t index = dirtyBegin; index < dirtyEnd; ++index)
		{
			oldSpan += previousDisplay[index];
			newSpan += display[index];
		}
		_columnWidthDirtyVisualSpan = (std::max)(oldSpan, newSpan);
	}
	_columnWidthMeasureDirty.assign(display.size(), 0);
	for (size_t index = 0; index < display.size(); ++index)
	{
		if (std::abs(display[index]
			- previousDisplay[index]) > 0.000001)
			_columnWidthMeasureDirty[index] = 1;
	}
	if (!changed)
	{
		if (previousProjectionDiffers) RefreshColumnWidths(true);
		return true;
	}

	double perStarWidth = (std::numeric_limits<double>::quiet_NaN)();
	for (size_t index = 0; index < _columns.size(); ++index)
	{
		const auto& state = _columnResizeWorkingSnapshot[index];
		if (state.Width.UnitType != DataGridLengthUnitType::Star
			|| state.Width.Value <= 0.000001) continue;
		const double candidate = state.Desired / state.Width.Value;
		if (std::isfinite(candidate) && candidate > 0.000001)
		{
			perStarWidth = candidate;
			break;
		}
	}
	if (!std::isfinite(perStarWidth) || perStarWidth <= 0.000001)
		perStarWidth = 1.0;

	for (size_t index = 0; index < _columns.size(); ++index)
	{
		auto& column = *_columns[index];
		const auto& start = _columnResizeWorkingSnapshot[index];
		const bool displayChanged = std::abs(
			display[index] - start.Display) > 0.000001;
		if (start.Width.UnitType == DataGridLengthUnitType::Star)
		{
			if (index == columnIndex && displayChanged)
			{
				desired[index] = display[index];
				const double targetPerStarWidth = start.Width.Value > 0.0
					? start.Desired / start.Width.Value : perStarWidth;
				if (std::isfinite(targetPerStarWidth)
					&& targetPerStarWidth > 0.000001)
					column._width = DataGridLength::Star(
						(std::max)(0.0,
							desired[index] / targetPerStarWidth));
			}
			else if (displayChanged)
				column._width = DataGridLength::Star(
					(std::max)(0.0, desired[index] / perStarWidth));
		}
		else if (index == columnIndex && displayChanged)
		{
			column._width = DataGridLength(display[index]);
			desired[index] = display[index];
		}
		column._runtimeWidth.Desired = desired[index];
		column._runtimeWidth.Display = display[index];
		// A user resize is local to its target and the donors selected by the
		// dragged edge. Preserve every current Star display so the viewport solver
		// cannot redistribute the accepted delta into unrelated columns afterward.
		column._runtimeWidth.HasDisplayOverride =
			start.Width.UnitType == DataGridLengthUnitType::Star
			|| displayChanged || start.RuntimeWidth.HasDisplayOverride;
	}
	RefreshColumnWidths(true);
	return true;
}

void DataGrid::EndColumnResizeTransaction(bool cancel)
{
	if (_columnResizeSnapshot.empty())
	{
		_columnResizeWorkingSnapshot.clear();
		_columnResizeTransactionIndex = DataGridCellInfo::InvalidIndex;
		_columnResizeCompensatesLeft = false;
		_columnResizeLastRawWidth =
			(std::numeric_limits<double>::quiet_NaN)();
		_columnResizeInputBias = 0.0;
		return;
	}
	const bool canRestore = cancel;
	if (canRestore)
	{
		for (const auto& state : _columnResizeSnapshot)
		{
			auto found = std::find_if(
				_columns.begin(), _columns.end(),
				[identity = state.Column](const auto& column)
				{ return column.get() == identity; });
			if (found == _columns.end()) continue;
			(*found)->_width = state.Width;
			(*found)->_runtimeWidth = state.RuntimeWidth;
		}
	}
	_columnResizeSnapshot.clear();
	_columnResizeWorkingSnapshot.clear();
	_columnResizeTransactionIndex = DataGridCellInfo::InvalidIndex;
	_columnResizeCompensatesLeft = false;
	_columnResizeLastRawWidth =
		(std::numeric_limits<double>::quiet_NaN)();
	_columnResizeInputBias = 0.0;
	if (canRestore) RefreshColumnWidths(true);
}

void DataGrid::InvalidateColumnContentWidthCache() noexcept
{
	_sampledColumnContentWidths.clear();
	++_columnContentWidthCacheEpoch;
	if (_columnContentWidthCacheEpoch == 0)
		_columnContentWidthCacheEpoch = 1;
	InvalidateColumnWidthCache();
}

void DataGrid::ApplyPendingColumnWidths(bool refreshVirtualMetrics)
{
	if (!_columnWidthRefreshPending) return;
	_columnWidthRefreshPending = false;
	const bool activeResizeFrame = !refreshVirtualMetrics
		&& !_columnResizeSnapshot.empty();
	const bool fixedRowHeight = std::isfinite(_rowHeight);
	const bool fixedHeaderHeight = std::isfinite(_columnHeaderHeight)
		|| !HasColumnHeaders(_headersVisibility);
	bool localLayoutCommitted = activeResizeFrame;
	const ControlWeakReference ownerLifetime(this);
	const ControlWeakReference headersLifetime(
		dynamic_cast<DataGridColumnHeadersPresenter*>(
			_headersPresenter.Get()));
	std::vector<ControlWeakReference> rows;
	rows.reserve(GetRealizedItems().size());
	for (const auto& [index, realized] : GetRealizedItems())
	{
		(void)realized;
		if (auto* row = dynamic_cast<DataGridRow*>(GetGeneratedItem(index)))
			rows.emplace_back(row);
	}
	if (auto* presenter = dynamic_cast<DataGridColumnHeadersPresenter*>(
		headersLifetime.Get()))
	{
		presenter->UpdateColumnWidths(false);
		auto* current = dynamic_cast<DataGrid*>(ownerLifetime.Get());
		auto* currentPresenter = dynamic_cast<
			DataGridColumnHeadersPresenter*>(headersLifetime.Get());
		if (!current || !currentPresenter) return;
		if (activeResizeFrame)
			localLayoutCommitted = currentPresenter
				->TryCommitResizeLayoutLocally(fixedHeaderHeight)
				&& localLayoutCommitted;
	}
	auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live) return;
	for (const auto& rowLifetime : rows)
	{
		auto* row = dynamic_cast<DataGridRow*>(rowLifetime.Get());
		live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
		if (!live) return;
		if (row && row->GetDataGridOwner() == live)
		{
			row->UpdateColumnWidths(false);
			live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
			row = dynamic_cast<DataGridRow*>(rowLifetime.Get());
			if (!live) return;
			if (row && row->GetDataGridOwner() == live
				&& activeResizeFrame)
			{
				auto* cells = row->_cellsGrid;
				localLayoutCommitted = cells
					&& cells->TryCommitResizeLayoutLocally(fixedRowHeight)
					&& localLayoutCommitted;
			}
		}
	}
	live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live) return;
	if (activeResizeFrame && localLayoutCommitted) return;

	// Auto-height content is allowed to take the fast path while its desired
	// height remains stable. If any row/header changed height, promote the whole
	// coalesced frame to the ordinary ancestor layout transaction so following
	// rows and the ScrollViewer viewport are repositioned correctly.
	if (auto* presenter = dynamic_cast<DataGridColumnHeadersPresenter*>(
		headersLifetime.Get()))
		live->InvalidateMeasurePathFromDescendant(presenter);
	for (const auto& rowLifetime : rows)
	{
		auto* row = dynamic_cast<DataGridRow*>(rowLifetime.Get());
		live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
		if (!live) return;
		if (row && row->GetDataGridOwner() == live)
			live->InvalidateMeasurePathFromDescendant(row);
	}
	live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live) return;
	// PrepareMeasureCore or PreparePresentation owns the surrounding layout turn.
	// The leaf presenters dirty only their own measure state; these local paths
	// make the immediately following template layout reach them without root
	// requests or a second frame.
	if (refreshVirtualMetrics) live->RefreshVirtualScrollMetrics();
}

void DataGrid::CommitColumnWidthLayoutToAncestors()
{
	const ControlWeakReference ownerLifetime(this);
	const ControlWeakReference headersLifetime(
		dynamic_cast<DataGridColumnHeadersPresenter*>(
			_headersPresenter.Get()));
	std::vector<ControlWeakReference> rows;
	rows.reserve(GetRealizedItems().size());
	for (const auto& [index, realized] : GetRealizedItems())
	{
		(void)realized;
		if (auto* row = dynamic_cast<DataGridRow*>(GetGeneratedItem(index)))
			rows.emplace_back(row);
	}
	if (auto* presenter = dynamic_cast<DataGridColumnHeadersPresenter*>(
		headersLifetime.Get()))
	{
		presenter->UpdateColumnWidths(false);
		auto* current = dynamic_cast<DataGrid*>(ownerLifetime.Get());
		auto* currentPresenter = dynamic_cast<
			DataGridColumnHeadersPresenter*>(headersLifetime.Get());
		if (current && currentPresenter)
			current->InvalidateMeasurePathFromDescendant(currentPresenter);
	}
	auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live) return;
	for (const auto& rowLifetime : rows)
	{
		auto* row = dynamic_cast<DataGridRow*>(rowLifetime.Get());
		live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
		if (!live) return;
		if (row && row->GetDataGridOwner() == live)
		{
			row->UpdateColumnWidths(false);
			live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
			row = dynamic_cast<DataGridRow*>(rowLifetime.Get());
			if (!live) return;
			if (row && row->GetDataGridOwner() == live)
				live->InvalidateMeasurePathFromDescendant(row);
		}
	}
	live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live) return;
	live->RefreshVirtualScrollMetrics();
	live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live) return;
	// Mouse release needs one public ancestor layout request, not one request per
	// realized row. The local paths preserve final Auto-height and hit-test
	// geometry even when the width revision was already applied locally.
	live->RequestLayout();
}

void DataGrid::InvalidatePendingColumnResizeVisual()
{
	// The resize solver records the exact contiguous geometry interval changed
	// since the previously presented frame. Damage only that interval when its
	// first header is realized; a balanced target/donor exchange then avoids
	// repainting the stable columns after the donor.
	const size_t damageBegin = _columnWidthDirtyBegin;
	if (damageBegin < _columns.size()
		&& std::isfinite(_columnWidthDirtyVisualSpan))
	{
		auto* presenter = dynamic_cast<DataGridColumnHeadersPresenter*>(
			_headersPresenter.Get());
		auto* header = presenter
			? presenter->GetHeader(damageBegin) : nullptr;
		if (header && header->GetPresentationWindow() == GetPresentationWindow())
		{
			const auto gridRect = GetAbsoluteRectDip();
			const auto headerRect = header->GetAbsoluteRectDip();
			const float left = (std::clamp)(
				headerRect.Left() - 2.0f,
				gridRect.Left(), gridRect.Right());
			const double maximum = static_cast<double>(
				(std::numeric_limits<float>::max)());
			const float span = static_cast<float>((std::clamp)(
				_columnWidthDirtyVisualSpan + 4.0, 0.0, maximum));
			const float right = (std::clamp)(
				headerRect.Left() + span,
				gridRect.Left(), gridRect.Right());
			if (left < right)
			{
				InvalidateVisualRect(D2D1::RectF(
					left, gridRect.Top(),
					right, gridRect.Bottom()));
				return;
			}
		}
	}
	// Detached/unrealized headers and semantic-only projection changes use the
	// conservative DataGrid damage path.
	InvalidateVisual();
}

void DataGrid::RefreshColumnWidths(bool preserveRealizedColumnRange)
{
	InvalidateColumnWidthCache();
	if (preserveRealizedColumnRange)
	{
		if (_columnWidthRefreshPending) return;
		_columnWidthRefreshPending = true;
		// The presentation hook consumes only the newest drag delta and commits the
		// header/realized-row paths locally. Scheduling damage for this DataGrid keeps
		// the Window measure root and unrelated retained content out of every frame.
		InvalidatePendingColumnResizeVisual();
		return;
	}
	_columnWidthRefreshPending = false;
	if (!preserveRealizedColumnRange) InvalidateRealizedColumnRange();
	if (auto* presenter = dynamic_cast<DataGridColumnHeadersPresenter*>(
		_headersPresenter.Get())) presenter->UpdateColumnWidths(false);
	for (const auto& [index, realized] : GetRealizedItems())
	{
		(void)realized;
		if (auto* row = dynamic_cast<DataGridRow*>(GetGeneratedItem(index)))
			row->UpdateColumnWidths(false);
	}
	RefreshVirtualScrollMetrics();
	// Every realized leaf above is already dirty. One owner request is enough to
	// reach all of them; propagating once per row turns a viewport update into
	// O(realized rows * visual depth) ancestor work before layout even begins.
	RequestLayout();
	InvalidateVisual();
}

void DataGrid::ProjectColumnWidthsForViewportLayout()
{
	// Viewport width is discovered from Header.Measure, DataGrid.Arrange, or the
	// ScrollViewer layout notification. In all three cases an ancestor layout
	// transaction is already walking this template. Project the new shared column
	// revision locally and let that transaction consume it synchronously instead
	// of scheduling a redundant root measure for every realized row.
	InvalidateColumnWidthCache();
	_columnWidthRefreshPending = false;
	InvalidateRealizedColumnRange();
	if (auto* presenter = dynamic_cast<DataGridColumnHeadersPresenter*>(
		_headersPresenter.Get())) presenter->UpdateColumnWidths(false);
	for (const auto& [index, realized] : GetRealizedItems())
	{
		(void)realized;
		if (auto* row = dynamic_cast<DataGridRow*>(GetGeneratedItem(index)))
			row->UpdateColumnWidths(false);
	}
	// Dense rows publish their new measured extent during the current arrange.
	// Sparse rows have no visuals for offscreen columns, so their logical extent
	// still has to be refreshed explicitly.
	if (_enableColumnVirtualization) RefreshVirtualScrollMetrics();
	InvalidateVisual();
}

bool DataGrid::TryCommitViewportColumnLayoutLocally()
{
	// A late ScrollViewer viewport correction arrives after it has arranged its
	// content. Fixed heights commit directly; Auto presenters first remeasure the
	// width-dirty cells and commit locally while their desired height is unchanged.
	// If wrapping changes any height, the caller promotes the correction to one
	// ordinary owner request so the following vertical items are repositioned.
	const bool fixedRowHeight = std::isfinite(_rowHeight);
	const bool fixedHeaderHeight = std::isfinite(_columnHeaderHeight)
		|| !HasColumnHeaders(_headersVisibility);
	const ControlWeakReference ownerLifetime(this);
	const ControlWeakReference headersLifetime(
		dynamic_cast<DataGridColumnHeadersPresenter*>(
			_headersPresenter.Get()));
	std::vector<ControlWeakReference> rows;
	rows.reserve(GetRealizedItems().size());
	for (const auto& [index, realized] : GetRealizedItems())
	{
		(void)realized;
		if (auto* row = dynamic_cast<DataGridRow*>(GetGeneratedItem(index)))
			rows.emplace_back(row);
	}
	if (auto* presenter = dynamic_cast<DataGridColumnHeadersPresenter*>(
		headersLifetime.Get()))
	{
		if (!presenter->TryCommitResizeLayoutLocally(fixedHeaderHeight))
			return false;
		if (!ownerLifetime.Get()) return false;
	}
	for (const auto& rowLifetime : rows)
	{
		auto* owner = dynamic_cast<DataGrid*>(ownerLifetime.Get());
		auto* row = dynamic_cast<DataGridRow*>(rowLifetime.Get());
		if (!owner) return false;
		if (!row || row->GetDataGridOwner() != owner) continue;
		auto* cells = row->_cellsGrid;
		if (!cells
			|| !cells->TryCommitResizeLayoutLocally(fixedRowHeight)) return false;
	}
	return ownerLifetime.Get() != nullptr;
}

bool DataGrid::EnsureColumnWidthPrefix() const
{
	const size_t count = _columns.size();
	if (_columnWidthPrefix.size() == count + 1) return true;
	const ControlWeakReference ownerLifetime(const_cast<DataGrid*>(this));
	std::vector<std::pair<DataGridColumn*, uint32_t>> columns;
	columns.reserve(count);
	for (const auto& column : _columns)
		columns.emplace_back(column.get(), column->_accessibilityIdentity);
	std::vector<double> prefix;
	prefix.reserve(count + 1);
	prefix.push_back(0.0);
	for (size_t index = 0; index < count; ++index)
	{
		auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
		if (!live || live->_columns.size() != count
			|| live->_columns[index].get() != columns[index].first
			|| live->_columns[index]->_accessibilityIdentity
				!= columns[index].second) return false;
		const auto length = live->ResolveColumnGridLength(index);
		live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
		if (!live || live->_columns.size() != count
			|| live->_columns[index].get() != columns[index].first
			|| live->_columns[index]->_accessibilityIdentity
				!= columns[index].second) return false;
		double width = length.IsPixel() ? length.Value : 0.0;
		if (!std::isfinite(width) || width < 0.0) width = 0.0;
		const double previous = prefix.back();
		prefix.push_back(previous >
			(std::numeric_limits<double>::max)() - width
			? (std::numeric_limits<double>::max)() : previous + width);
	}
	auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live || live->_columns.size() != count) return false;
	for (size_t index = 0; index < count; ++index)
		if (live->_columns[index].get() != columns[index].first
			|| live->_columns[index]->_accessibilityIdentity
				!= columns[index].second) return false;
	live->_columnWidthPrefix = std::move(prefix);
	return true;
}

bool DataGrid::TryResolveColumnBounds(
	size_t columnIndex, double& left, double& right) const
{
	left = 0.0;
	right = 0.0;
	if (columnIndex >= _columns.size() || !EnsureColumnWidthPrefix()
		|| _columnWidthPrefix.size() != _columns.size() + 1) return false;
	left = _columnWidthPrefix[columnIndex];
	right = _columnWidthPrefix[columnIndex + 1];
	return true;
}

std::pair<size_t, size_t> DataGrid::ResolveRealizedColumnRange() const
{
	const size_t count = _columns.size();
	if (!_enableColumnVirtualization || count == 0)
		return { 0, count };
	if (!EnsureColumnWidthPrefix()
		|| _columnWidthPrefix.size() != count + 1)
		return { 0, count };

	const double viewport = std::isfinite(_columnViewportWidth)
		&& _columnViewportWidth > 0.0 ? _columnViewportWidth : 480.0;
	const double offset = std::isfinite(_horizontalScrollOffset)
		? (std::max)(0.0, _horizontalScrollOffset) : 0.0;
	const double visibleEnd = offset >
		(std::numeric_limits<double>::max)() - viewport
		? (std::numeric_limits<double>::max)() : offset + viewport;
	const auto firstBoundary = std::upper_bound(
		_columnWidthPrefix.begin() + 1, _columnWidthPrefix.end(), offset);
	size_t first = firstBoundary == _columnWidthPrefix.end()
		? count - 1
		: static_cast<size_t>(std::distance(
			_columnWidthPrefix.begin(), firstBoundary) - 1);
	size_t last = static_cast<size_t>(std::distance(
		_columnWidthPrefix.begin(), std::lower_bound(
			_columnWidthPrefix.begin(), _columnWidthPrefix.end(), visibleEnd)));
	last = (std::min)(count, (std::max)(first + 1, last));

	// Keep two cached columns in total so small wheel/touchpad deltas do not
	// rebuild every realized row for each ScrollChanged notification. At an
	// extent edge put both on the reachable side; in the interior keep one on
	// either side. "Two per side" would silently double the wide-grid control
	// budget.
	constexpr size_t overscan = 2;
	if (first == 0)
		last = (std::min)(count, last + overscan);
	else if (last == count)
		first = first > overscan ? first - overscan : 0;
	else
	{
		--first;
		last = (std::min)(count, last + 1);
	}
	return { first, last };
}

void DataGrid::InvalidateRealizedColumnRange() noexcept
{
	_realizedColumnBegin = DataGridCellInfo::InvalidIndex;
	_realizedColumnEnd = DataGridCellInfo::InvalidIndex;
}

void DataGrid::RefreshRealizedColumns()
{
	if (!_enableColumnVirtualization || _refreshingRealizedColumns
		|| !_columnResizeSnapshot.empty()) return;
	const ControlWeakReference ownerLifetime(this);
	_refreshingRealizedColumns = true;
	auto reset = MakeScopeExit([ownerLifetime]
	{
		if (auto* owner = dynamic_cast<DataGrid*>(ownerLifetime.Get()))
			owner->_refreshingRealizedColumns = false;
	});
	const size_t previousBegin = _realizedColumnBegin;
	const size_t previousEnd = _realizedColumnEnd;
	auto range = ResolveRealizedColumnRange();
	bool sampledEnteringColumns = false;
	const auto needsContentSample = [this](
		const std::pair<size_t, size_t>& candidate)
	{
		if (candidate.first > candidate.second
			|| candidate.second > _columns.size()) return false;
		for (size_t index = candidate.first;
			index < candidate.second; ++index)
		{
			const auto unit = _columns[index]->GetWidth().UnitType;
			if (unit == DataGridLengthUnitType::Pixel
				|| unit == DataGridLengthUnitType::SizeToHeader
				|| unit == DataGridLengthUnitType::Star) continue;
			if (_sampledColumnContentWidths.size() != _columns.size()
				|| !_sampledColumnContentWidths[index]) return true;
		}
		return false;
	};
	// Unseen Auto/SizeToCells columns have only a cheap header/minimum estimate
	// in the prefix.  Sample just the entering viewport strip, then let the
	// prefix settle before changing containers.  Cached measurements make this
	// monotonic in normal use; the small bound is a reentrancy firewall.
	for (size_t pass = 0; pass < 4 && needsContentSample(range); ++pass)
	{
		_realizedColumnBegin = range.first;
		_realizedColumnEnd = range.second;
		InvalidateColumnWidthCache();
		sampledEnteringColumns = true;
		const auto refined = ResolveRealizedColumnRange();
		if (refined == range) break;
		range = refined;
	}
	if (previousBegin == range.first && previousEnd == range.second
		&& !sampledEnteringColumns) return;
	_realizedColumnBegin = range.first;
	_realizedColumnEnd = range.second;

	bool incrementallyUpdated = true;
	std::wstring error;
	if (auto* presenter = GetColumnHeadersPresenter())
		incrementallyUpdated = presenter->RefreshRealizedColumns(
			range.first, range.second, &error);
	auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live) return;
	std::vector<ControlWeakReference> rows;
	if (incrementallyUpdated)
	{
		rows.reserve(live->GetRealizedItems().size());
		for (const auto& [index, visual] : live->GetRealizedItems())
		{
			(void)visual;
			if (auto* row = dynamic_cast<DataGridRow*>(
				live->GetGeneratedItem(index))) rows.emplace_back(row);
		}
	}
	for (const auto& rowLifetime : rows)
	{
		auto* row = dynamic_cast<DataGridRow*>(rowLifetime.Get());
		live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
		if (!live) return;
		if (!row || row->GetDataGridOwner() != live
			|| !row->RefreshRealizedColumns(
				range.first, range.second, &error))
		{
			incrementallyUpdated = false;
			break;
		}
	}

	live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live) return;
	if (!incrementallyUpdated)
	{
		// Reentrant tree observers can invalidate an incremental transfer.  Keep
		// the established all-or-nothing regeneration path as the correctness
		// fallback; ordinary scrolling stays on the overlap-preserving path.
		live->InvalidateRows();
		live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
		if (!live) return;
		live->RefreshHeaderPresenter();
		live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
		if (!live) return;
		return;
	}
	live->RefreshVirtualScrollMetrics();
	live->RequestLayout();
	live->InvalidateVisual();
}

bool DataGrid::BringColumnIntoView(size_t columnIndex)
{
	if (columnIndex >= _columns.size()) return false;
	auto* scroll = dynamic_cast<ScrollViewer*>(_scrollViewer.Get());
	if (!scroll) return !_enableColumnVirtualization;
	double gutter = HasRowHeaders(_headersVisibility)
		? ResolveRowHeaderWidth() : 0.0;
	if (!std::isfinite(gutter)) gutter = 0.0;
	double dataLeft = 0.0;
	double dataRight = 0.0;
	if (!TryResolveColumnBounds(columnIndex, dataLeft, dataRight)) return false;
	const double left = gutter + dataLeft;
	const double right = gutter + dataRight;
	const double viewport = scroll->ViewportWidth;
	if (left < scroll->HorizontalOffset + gutter)
		scroll->ScrollToHorizontalOffset((std::max)(0.0, left - gutter));
	else if (viewport > 0.0
		&& right > scroll->HorizontalOffset + viewport)
		scroll->ScrollToHorizontalOffset(right - viewport);
	RefreshHorizontalScrollAlignment();
	return true;
}

double DataGrid::ResolveColumnViewportWidth(double fallbackWidth) const noexcept
{
	double viewport = fallbackWidth;
	if (auto* scroll = dynamic_cast<ScrollViewer*>(_scrollViewer.Get()))
	{
		const double published = scroll->GetViewportWidth();
		if (std::isfinite(published) && published > 0.0)
			viewport = published;
	}
	if (!std::isfinite(viewport) || viewport < 0.0)
		viewport = 0.0;
	const double rowHeader = HasRowHeaders(_headersVisibility)
		? ResolveRowHeaderWidth() : 0.0;
	return (std::max)(0.0, viewport - rowHeader);
}

double DataGrid::GetVirtualizedHorizontalExtent() const
{
	// Dense rows already publish their complete measured width. Avoid forcing
	// Auto/SizeToCells sampling merely because the vertical virtual host asks for
	// optional logical metrics.
	if (!_enableColumnVirtualization) return 0.0;
	double result = HasRowHeaders(_headersVisibility)
		? ResolveRowHeaderWidth() : 0.0;
	// A source transaction may invalidate sampled widths more than once while it
	// is still rollback-capable. Do not populate a transient prefix from the
	// candidate projection; the committed row/header pass refreshes this metric.
	if (_settingItemsSource || IsItemsSourceUpdateInProgress())
	{
		if (_columnWidthPrefix.size() == _columns.size() + 1)
			result += _columnWidthPrefix.back();
		return result;
	}
	if (!EnsureColumnWidthPrefix() || _columnWidthPrefix.empty()) return result;
	const double columns = _columnWidthPrefix.back();
	if (result > (std::numeric_limits<double>::max)() - columns)
		return (std::numeric_limits<double>::max)();
	result += columns;
	return result;
}

void DataGrid::RedistributeRuntimeWidthsForViewportChange(
	double oldViewportWidth, double newViewportWidth) noexcept
{
	if (!std::isfinite(oldViewportWidth)
		|| !std::isfinite(newViewportWidth)) return;
	double change = newViewportWidth - oldViewportWidth;
	if (std::abs(change) <= 0.0001) return;
	const auto hasOverride = [](const auto& column)
	{
		return column->_runtimeWidth.HasDisplayOverride
			&& std::isfinite(column->_runtimeWidth.Display)
			&& std::isfinite(column->_runtimeWidth.Desired);
	};
	const bool hasStarDisplayOverrides = std::any_of(
		_columns.begin(), _columns.end(), [&](const auto& column)
		{
			return column->_width.UnitType == DataGridLengthUnitType::Star
				&& hasOverride(column);
		});
	const auto redistributeStarDisplays =
		[&](double amount, bool grow) noexcept
	{
		constexpr double epsilon = 0.000001;
		while (amount > epsilon)
		{
			double totalWeight = 0.0;
			double limitingRatio =
				(std::numeric_limits<double>::infinity)();
			for (const auto& column : _columns)
			{
				if (column->_width.UnitType != DataGridLengthUnitType::Star
					|| !hasOverride(column)
					|| column->_width.Value <= epsilon) continue;
				const double capacity = grow
					? column->GetMaxWidth() - column->_runtimeWidth.Display
					: column->_runtimeWidth.Display - column->GetMinWidth();
				if (capacity <= epsilon) continue;
				totalWeight += column->_width.Value;
				limitingRatio = (std::min)(
					limitingRatio, capacity / column->_width.Value);
			}
			if (totalWeight <= epsilon
				|| std::isnan(limitingRatio)) break;
			const double ratio = (std::min)(
				limitingRatio, amount / totalWeight);
			if (ratio <= epsilon) break;
			double consumed = 0.0;
			for (const auto& column : _columns)
			{
				if (column->_width.UnitType != DataGridLengthUnitType::Star
					|| !hasOverride(column)
					|| column->_width.Value <= epsilon) continue;
				const double capacity = grow
					? column->GetMaxWidth() - column->_runtimeWidth.Display
					: column->_runtimeWidth.Display - column->GetMinWidth();
				if (capacity <= epsilon) continue;
				const double delta = (std::min)(
					capacity, ratio * column->_width.Value);
				column->_runtimeWidth.Display += grow ? delta : -delta;
				column->_runtimeWidth.Desired += grow ? delta : -delta;
				consumed += delta;
			}
			if (consumed <= epsilon) break;
			amount -= consumed;
		}
		return (std::max)(0.0, amount);
	};
	if (change > 0.0)
	{
		while (change > 0.000001)
		{
			size_t participantCount = 0;
			double minimumLag = (std::numeric_limits<double>::infinity)();
			for (const auto& column : _columns)
			{
				if (!hasOverride(column)
					|| column->_width.UnitType == DataGridLengthUnitType::Star)
					continue;
				const double limit = (std::clamp)(
					column->_runtimeWidth.Desired,
					column->GetMinWidth(), column->GetMaxWidth());
				const double lag = limit - column->_runtimeWidth.Display;
				if (lag <= 0.000001) continue;
				++participantCount;
				minimumLag = (std::min)(minimumLag, lag);
			}
			if (participantCount == 0) break;
			const double perColumn = (std::min)(
				minimumLag, change / static_cast<double>(participantCount));
			if (!(perColumn > 0.0) || !std::isfinite(perColumn)) break;
			for (const auto& column : _columns)
			{
				if (!hasOverride(column)
					|| column->_width.UnitType == DataGridLengthUnitType::Star)
					continue;
				const double limit = (std::clamp)(
					column->_runtimeWidth.Desired,
					column->GetMinWidth(), column->GetMaxWidth());
				if (column->_runtimeWidth.Display + 0.000001 >= limit) continue;
				const double amount = (std::min)(
					perColumn, limit - column->_runtimeWidth.Display);
				column->_runtimeWidth.Display += amount;
				change -= amount;
			}
		}
		if (hasStarDisplayOverrides)
			(void)redistributeStarDisplays(change, true);
		return;
	}

	double deficit = -change;
	// Star columns consume the available-space reduction first. Only the part
	// beyond their aggregate room down to MinWidth reaches non-Star displays.
	if (hasStarDisplayOverrides)
		deficit = redistributeStarDisplays(deficit, false);
	else
	{
		double starCapacity = 0.0;
		for (const auto& column : _columns)
		{
			if (column->_width.UnitType != DataGridLengthUnitType::Star) continue;
			const double display = std::isfinite(column->_runtimeWidth.Display)
				? column->_runtimeWidth.Display : column->GetMinWidth();
			starCapacity += (std::max)(0.0, display - column->GetMinWidth());
		}
		deficit = (std::max)(0.0, deficit - starCapacity);
	}
	while (deficit > 0.000001)
	{
		size_t participantCount = 0;
		double minimumExcess = (std::numeric_limits<double>::infinity)();
		for (const auto& column : _columns)
		{
			if (column->_width.UnitType == DataGridLengthUnitType::Star) continue;
			const double display = std::isfinite(column->_runtimeWidth.Display)
				? column->_runtimeWidth.Display
				: (std::clamp)(column->_width.Value,
					column->GetMinWidth(), column->GetMaxWidth());
			const double excess = display - column->GetMinWidth();
			if (excess <= 0.000001) continue;
			++participantCount;
			minimumExcess = (std::min)(minimumExcess, excess);
		}
		if (participantCount == 0) break;
		const double perColumn = (std::min)(
			minimumExcess, deficit / static_cast<double>(participantCount));
		if (!(perColumn > 0.0) || !std::isfinite(perColumn)) break;
		for (const auto& column : _columns)
		{
			if (column->_width.UnitType == DataGridLengthUnitType::Star) continue;
			if (!hasOverride(column))
			{
				const double display = std::isfinite(column->_runtimeWidth.Display)
					? column->_runtimeWidth.Display
					: (std::clamp)(column->_width.Value,
						column->GetMinWidth(), column->GetMaxWidth());
				column->_runtimeWidth.Desired =
					column->_width.UnitType == DataGridLengthUnitType::Pixel
					? column->_width.Value : display;
				column->_runtimeWidth.Display = display;
				column->_runtimeWidth.HasDisplayOverride = true;
			}
			if (column->_runtimeWidth.Display
				<= column->GetMinWidth() + 0.000001) continue;
			const double amount = (std::min)(perColumn,
				column->_runtimeWidth.Display - column->GetMinWidth());
			column->_runtimeWidth.Display -= amount;
			deficit -= amount;
		}
	}
}

void DataGrid::UpdateColumnViewportWidth(
	double availableWidth, bool refreshRealizedColumns)
{
	if (!std::isfinite(availableWidth) || availableWidth < 0.0)
		availableWidth = 0.0;
	if (std::isfinite(_columnViewportWidth)
		&& std::abs(_columnViewportWidth - availableWidth) <= 0.0001)
		return;
	const double oldViewportWidth = _columnViewportWidth;
	const bool hasStarColumns = std::any_of(
		_columns.begin(), _columns.end(), [](const auto& column)
		{
			return column->GetWidth().UnitType
				== DataGridLengthUnitType::Star;
		});
	if (hasStarColumns)
	{
		// Preserve the displayed geometry before changing the Star water-fill.
		// Viewport projection used to leave the resize dirty mask empty, whose
		// conservative meaning is "measure every cell".  In the common mixed
		// Pixel/Star DataGrid that needlessly remeasured all fixed-width templates
		// on every WM_SIZE even though only Star constraints changed.
		std::vector<double> previousWidths;
		if (std::isfinite(oldViewportWidth))
		{
			previousWidths.reserve(_columns.size());
			for (size_t index = 0; index < _columns.size(); ++index)
			{
				const auto resolved = ResolveColumnGridLength(index);
				previousWidths.push_back(resolved.IsPixel()
					? static_cast<double>(resolved.Value) : 0.0);
			}
		}
		_columnViewportWidth = availableWidth;
		RedistributeRuntimeWidthsForViewportChange(
			oldViewportWidth, availableWidth);
		ProjectColumnWidthsForViewportLayout();

		std::vector<double> currentWidths;
		currentWidths.reserve(_columns.size());
		for (size_t index = 0; index < _columns.size(); ++index)
		{
			const auto resolved = ResolveColumnGridLength(index);
			currentWidths.push_back(resolved.IsPixel()
				? static_cast<double>(resolved.Value) : 0.0);
		}
		_columnWidthDirtyBegin = DataGridCellInfo::InvalidIndex;
		_columnWidthDirtyEnd = DataGridCellInfo::InvalidIndex;
		_columnWidthDirtyVisualSpan =
			(std::numeric_limits<double>::quiet_NaN)();
		_columnWidthMeasureDirty.clear();
		if (previousWidths.size() == currentWidths.size())
		{
			_columnWidthMeasureDirty.assign(currentWidths.size(), 0);
			double oldLeft = 0.0;
			double newLeft = 0.0;
			for (size_t index = 0; index < currentWidths.size(); ++index)
			{
				const double oldWidth = previousWidths[index];
				const double newWidth = currentWidths[index];
				if (std::abs(oldWidth - newWidth) > 0.000001)
					_columnWidthMeasureDirty[index] = 1;
				if (std::abs(oldLeft - newLeft) > 0.000001
					|| std::abs(oldWidth - newWidth) > 0.000001)
				{
					if (_columnWidthDirtyBegin
						== DataGridCellInfo::InvalidIndex)
						_columnWidthDirtyBegin = index;
					_columnWidthDirtyEnd = index + 1;
				}
				oldLeft += oldWidth;
				newLeft += newWidth;
			}
			if (_columnWidthDirtyBegin != DataGridCellInfo::InvalidIndex)
			{
				double oldSpan = 0.0;
				double newSpan = 0.0;
				for (size_t index = _columnWidthDirtyBegin;
					index < _columnWidthDirtyEnd; ++index)
				{
					oldSpan += previousWidths[index];
					newSpan += currentWidths[index];
				}
				_columnWidthDirtyVisualSpan =
					(std::max)(oldSpan, newSpan);
			}
		}
		// A viewport change can occur while the gripper owns capture (for example
		// when a scrollbar appears). Preserve the drag-start snapshot for Cancel,
		// but replay later absolute pointer deltas from the new projection.
		if (!_columnResizeSnapshot.empty())
		{
			if (RebaseColumnResizeTransaction()
				&& std::isfinite(_columnResizeLastRawWidth))
				_columnResizeInputBias =
					_columnResizeWorkingSnapshot[
						_columnResizeTransactionIndex].Display
					- _columnResizeLastRawWidth;
		}
	}
	else _columnViewportWidth = availableWidth;
	if (refreshRealizedColumns && _columnResizeSnapshot.empty())
		RefreshRealizedColumns();
}

std::pair<size_t, size_t>
DataGrid::ResolveDeferredColumnSampleRange() const
{
	const size_t count = _columns.size();
	if (!_enableColumnVirtualization || count == 0) return { 0, count };
	if (_realizedColumnBegin != DataGridCellInfo::InvalidIndex
		&& _realizedColumnEnd != DataGridCellInfo::InvalidIndex
		&& _realizedColumnBegin < _realizedColumnEnd
		&& _realizedColumnEnd <= count)
		return { _realizedColumnBegin, _realizedColumnEnd };

	const double viewport = std::isfinite(_columnViewportWidth)
		&& _columnViewportWidth > 0.0 ? _columnViewportWidth : 480.0;
	const double offset = std::isfinite(_horizontalScrollOffset)
		? (std::max)(0.0, _horizontalScrollOffset) : 0.0;
	const double visibleEnd = offset >
		(std::numeric_limits<double>::max)() - viewport
		? (std::numeric_limits<double>::max)() : offset + viewport;
	size_t first = count;
	size_t last = count;
	double left = 0.0;
	for (size_t index = 0; index < count; ++index)
	{
		const auto& column = *_columns[index];
		const auto& length = column.GetWidth();
		double width = column.GetMinWidth();
		if (column._runtimeWidth.HasDisplayOverride
			&& std::isfinite(column._runtimeWidth.Display))
			width = (std::clamp)(column._runtimeWidth.Display,
				column.GetMinWidth(), column.GetMaxWidth());
		else if (length.UnitType == DataGridLengthUnitType::Pixel)
			width = (std::clamp)(length.Value,
				column.GetMinWidth(), column.GetMaxWidth());
		else
		{
			if (length.UnitType != DataGridLengthUnitType::SizeToCells)
				width = (std::max)(width,
					16.0 + 7.0 * static_cast<double>(
						column.GetHeader().ToString().size()));
			const bool widthUsesCellContent =
				length.UnitType == DataGridLengthUnitType::Auto
				|| length.UnitType == DataGridLengthUnitType::SizeToCells;
			if (widthUsesCellContent
				&& _sampledColumnContentWidths.size() == count
				&& _sampledColumnContentWidths[index])
				width = (std::max)(
					width, *_sampledColumnContentWidths[index]);
			width = (std::clamp)(
				width, column.GetMinWidth(), column.GetMaxWidth());
		}
		if (!std::isfinite(width) || width < 0.0) width = 0.0;
		const double right = left >
			(std::numeric_limits<double>::max)() - width
			? (std::numeric_limits<double>::max)() : left + width;
		if (first == count && right > offset) first = index;
		if (first != count && left < visibleEnd) last = index + 1;
		if (first != count && left >= visibleEnd) break;
		left = right;
	}
	if (first == count)
	{
		first = count - 1;
		last = count;
	}
	else if (last <= first) last = first + 1;
	constexpr size_t overscan = 2;
	if (first == 0)
		last = (std::min)(count, last + overscan);
	else if (last == count)
		first = first > overscan ? first - overscan : 0;
	else
	{
		--first;
		last = (std::min)(count, last + 1);
	}
	return { first, last };
}

void DataGrid::RebuildResolvedColumnWidths() const
{
	_resolvedColumnWidths.assign(_columns.size(), std::nullopt);
	if (_columns.empty()) return;
	const auto sampleRange = ResolveDeferredColumnSampleRange();

	struct StarColumn final
	{
		size_t Index = 0;
		double Weight = 0.0;
		double Minimum = 0.0;
		double Maximum = 0.0;
	};
	std::vector<StarColumn> stars;
	stars.reserve(_columns.size());
	double nonStarTotal = 0.0;
	for (size_t index = 0; index < _columns.size(); ++index)
	{
		auto& column = *_columns[index];
		const auto& width = column.GetWidth();
		if (column._runtimeWidth.HasDisplayOverride
			&& std::isfinite(column._runtimeWidth.Display))
		{
			if (width.UnitType != DataGridLengthUnitType::Pixel)
			{
				if (width.UnitType != DataGridLengthUnitType::Star)
				{
					const bool header = width.UnitType
						!= DataGridLengthUnitType::SizeToCells;
					const bool cells = width.UnitType
						!= DataGridLengthUnitType::SizeToHeader
						&& (!_enableColumnVirtualization
							|| (index >= sampleRange.first
								&& index < sampleRange.second));
					column._runtimeWidth.Desired =
						EstimateColumnWidth(index, header, cells);
				}
			}
			const double pixels = (std::clamp)(
				column._runtimeWidth.Display,
				column.GetMinWidth(), column.GetMaxWidth());
			_resolvedColumnWidths[index] = GridLength::Pixels(
				static_cast<float>(pixels));
			nonStarTotal += pixels;
			column._runtimeWidth.Display = pixels;
			continue;
		}
		if (width.UnitType == DataGridLengthUnitType::Star)
		{
			stars.push_back(StarColumn{
				index,
				(std::max)(0.0, width.Value),
				column.GetMinWidth(),
				column.GetMaxWidth() });
			continue;
		}
		double pixels = 0.0;
		if (width.UnitType == DataGridLengthUnitType::Pixel)
			pixels = (std::clamp)(
				width.Value, column.GetMinWidth(), column.GetMaxWidth());
		else
		{
			const bool header = width.UnitType
				!= DataGridLengthUnitType::SizeToCells;
			const bool cells = width.UnitType
				!= DataGridLengthUnitType::SizeToHeader
				&& (!_enableColumnVirtualization
					|| (index >= sampleRange.first
						&& index < sampleRange.second));
			pixels = EstimateColumnWidth(index, header, cells);
		}
		_resolvedColumnWidths[index] = GridLength::Pixels(
			static_cast<float>(pixels));
		nonStarTotal += pixels;
		column._runtimeWidth.Desired = width.UnitType
			== DataGridLengthUnitType::Pixel ? width.Value : pixels;
		column._runtimeWidth.Display = pixels;
	}

	if (stars.empty()) return;
	// Interactive resize persists private Display values. A later viewport
	// change gives/takes only the delta to/from Star columns; it must not restore
	// a compensating Pixel/Auto column to its declaration and recreate the
	// original "right edge stays fixed while left Stars move" defect.
	double remaining = std::isfinite(_columnViewportWidth)
		? (std::max)(0.0, _columnViewportWidth - nonStarTotal)
		: 0.0;
	std::vector<size_t> unresolved;
	unresolved.reserve(stars.size());
	for (size_t starIndex = 0; starIndex < stars.size(); ++starIndex)
	{
		const auto& star = stars[starIndex];
		if (star.Weight > 0.0)
		{
			unresolved.push_back(starIndex);
			continue;
		}
		_resolvedColumnWidths[star.Index] = GridLength::Pixels(
			static_cast<float>(star.Minimum));
		_columns[star.Index]->_runtimeWidth.Desired = 0.0;
		_columns[star.Index]->_runtimeWidth.Display = star.Minimum;
		remaining = (std::max)(0.0, remaining - star.Minimum);
	}
	if (unresolved.empty()) return;
	double minimumTotal = 0.0;
	double maximumTotal = 0.0;
	double totalFactors = 0.0;
	for (const size_t starIndex : unresolved)
	{
		minimumTotal += stars[starIndex].Minimum;
		maximumTotal += stars[starIndex].Maximum;
		totalFactors += stars[starIndex].Weight;
	}
	remaining = (std::max)(remaining, minimumTotal);
	if (std::isfinite(maximumTotal))
		remaining = (std::min)(remaining, maximumTotal);
	std::vector<size_t> partial;
	partial.reserve(unresolved.size());
	while (!unresolved.empty())
	{
		if (!(totalFactors > 0.0) || !std::isfinite(totalFactors)) break;
		const double starValue = remaining / totalFactors;
		for (size_t index = 0; index < unresolved.size();)
		{
			const auto& star = stars[unresolved[index]];
			const double share = remaining * star.Weight / totalFactors;
			if (star.Minimum <= share + 0.000001)
			{
				++index;
				continue;
			}
			remaining = (std::max)(0.0, remaining - star.Minimum);
			totalFactors -= star.Weight;
			partial.push_back(unresolved[index]);
			unresolved.erase(unresolved.begin() + index);
		}

		bool iterate = false;
		if (totalFactors > 0.0)
		{
			for (size_t index = 0; index < unresolved.size(); ++index)
			{
				const auto& star = stars[unresolved[index]];
				const double share = remaining * star.Weight / totalFactors;
				if (star.Maximum + 0.000001 >= share) continue;
				_resolvedColumnWidths[star.Index] = GridLength::Pixels(
					static_cast<float>(star.Maximum));
				_columns[star.Index]->_runtimeWidth.Desired =
					starValue * star.Weight;
				_columns[star.Index]->_runtimeWidth.Display = star.Maximum;
				remaining -= star.Maximum;
				totalFactors -= star.Weight;
				unresolved.erase(unresolved.begin() + index);
				iterate = true;
				break;
			}
		}
		if (iterate)
		{
			for (const size_t starIndex : partial)
			{
				unresolved.push_back(starIndex);
				remaining += stars[starIndex].Minimum;
				totalFactors += stars[starIndex].Weight;
			}
			partial.clear();
			continue;
		}

		for (const size_t starIndex : partial)
		{
			const auto& star = stars[starIndex];
			_resolvedColumnWidths[star.Index] = GridLength::Pixels(
				static_cast<float>(star.Minimum));
			_columns[star.Index]->_runtimeWidth.Desired =
				starValue * star.Weight;
			_columns[star.Index]->_runtimeWidth.Display = star.Minimum;
		}
		partial.clear();
		for (const size_t starIndex : unresolved)
		{
			const auto& star = stars[starIndex];
			const double display = totalFactors > 0.0
				? remaining * star.Weight / totalFactors : star.Minimum;
			_resolvedColumnWidths[star.Index] = GridLength::Pixels(
				static_cast<float>(display));
			_columns[star.Index]->_runtimeWidth.Desired =
				starValue * star.Weight;
			_columns[star.Index]->_runtimeWidth.Display = display;
		}
		unresolved.clear();
	}
}

double DataGrid::GetColumnDisplayWidth(size_t columnIndex) const
{
	if (columnIndex >= _columns.size())
		return (std::numeric_limits<double>::quiet_NaN)();
	const auto& runtimeWidth = _columns[columnIndex]->_runtimeWidth;
	if (runtimeWidth.HasDisplayOverride
		&& std::isfinite(runtimeWidth.Display))
		return runtimeWidth.Display;
	const auto resolved = ResolveColumnGridLength(columnIndex);
	if (resolved.IsPixel()) return resolved.Value;
	if (auto* presenter = dynamic_cast<DataGridColumnHeadersPresenter*>(
		_headersPresenter.Get()))
	{
		if (auto* header = presenter->GetHeader(columnIndex))
		{
			const double width = header->GetActualSizeDip().width;
			if (std::isfinite(width) && width > 0.0) return width;
		}
	}
	return EstimateColumnWidth(columnIndex, true, true);
}

double DataGrid::EstimateColumnWidth(
	size_t columnIndex, bool includeHeader, bool includeCells) const
{
	if (columnIndex >= _columns.size()) return 20.0;
	auto* const columnIdentity = _columns[columnIndex].get();
	const double minimum = columnIdentity->GetMinWidth();
	const double maximum = columnIdentity->GetMaxWidth();
	double width = minimum;
	if (includeHeader)
		width = (std::max)(width,
			16.0 + 7.0 * static_cast<double>(
				columnIdentity->GetHeader().ToString().size()));
	const auto widthUnit = columnIdentity->GetWidth().UnitType;
	const bool widthUsesCellContent =
		widthUnit == DataGridLengthUnitType::Auto
		|| widthUnit == DataGridLengthUnitType::SizeToCells;
	if (widthUsesCellContent
		&& _sampledColumnContentWidths.size() == _columns.size()
		&& _sampledColumnContentWidths[columnIndex])
		width = (std::max)(
			width, *_sampledColumnContentWidths[columnIndex]);
	const auto items = GetItemsView();
	if (includeCells && items)
	{
		if (_sampledColumnContentWidths.size() != _columns.size())
		{
			_sampledColumnContentWidths.assign(
				_columns.size(), std::nullopt);
			++_columnContentWidthCacheEpoch;
			if (_columnContentWidthCacheEpoch == 0)
				_columnContentWidthCacheEpoch = 1;
		}
		auto& cached = _sampledColumnContentWidths[columnIndex];
		if (cached) width = (std::max)(width, *cached);
		else
		{
			const auto* bound = dynamic_cast<const DataGridBoundColumn*>(
				columnIdentity);
#if CUI_ENABLE_DYNAMIC_XAML
			const std::wstring bindingPath = bound
				? bound->GetBindingPath() : std::wstring{};
#endif
			const CompiledBindingPathView compiledBindingPath = bound
				? bound->GetCompiledBindingPath() : CompiledBindingPathView{};
			const size_t cacheEpoch = _columnContentWidthCacheEpoch;
			const ControlWeakReference ownerLifetime(
				const_cast<DataGrid*>(this));
			double sampledWidth = 0.0;
			const size_t count = bound ? (std::min)(
				items.Get()->Count(), size_t{ 1000 }) : 0;
			for (size_t index = 0; index < count; ++index)
			{
				BindingSourceReference item;
				BindingValue value;
				if (!items.Get()->TryGetItem(index, item) || !item) continue;
				bool read = false;
#if CUI_ENABLE_DYNAMIC_XAML
				if (!bindingPath.empty())
					read = TryGetBindingPathValue(
						*item.Get(), bindingPath, value);
#endif
				if (!read && !compiledBindingPath.Empty())
					read = TryGetBindingPathValue(
						*item.Get(), compiledBindingPath, value);
				if (read) sampledWidth = (std::max)(sampledWidth,
					16.0 + 7.0 * static_cast<double>(
						value.ToString().size()));
			}
			width = (std::max)(width, sampledWidth);
			auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
			if (live == this
				&& live->_columnContentWidthCacheEpoch == cacheEpoch
				&& columnIndex < live->_columns.size()
				&& live->_columns[columnIndex].get() == columnIdentity
				&& live->GetItemsView().Shared() == items.Shared()
				&& live->_sampledColumnContentWidths.size()
					== live->_columns.size())
				live->_sampledColumnContentWidths[columnIndex] = sampledWidth;
		}
	}
	return (std::clamp)(width, minimum, maximum);
}

void DataGrid::OnGeneratedItemsRebuilt()
{
	const ControlWeakReference ownerLifetime(this);
	ListBox::OnGeneratedItemsRebuilt();
	if (!ownerLifetime.Get()) return;
	InvalidateItemOccurrenceCache();
	if (_currentCell.IsValid())
	{
		const auto previousCurrent = _currentCell;
		const auto items = GetItemsView();
		bool found = false;
		size_t columnIndex = previousCurrent.ColumnIndex;
		const auto currentColumn = std::find_if(
			_columns.begin(), _columns.end(), [&previousCurrent](const auto& column)
			{ return column.get() == previousCurrent.Column; });
		if (currentColumn != _columns.end())
			columnIndex = static_cast<size_t>(
				std::distance(_columns.begin(), currentColumn));
		if (items && currentColumn != _columns.end())
		{
			size_t index = DataGridCellInfo::InvalidIndex;
			if (previousCurrent._itemOccurrence != DataGridCellInfo::InvalidIndex
				&& TryResolveItemOccurrence(
					previousCurrent.Item,
					previousCurrent._itemOccurrence,
					index))
			{
				DataGridCellInfo candidate;
				if (TryCreateCellInfo(index, columnIndex, candidate)
					&& SameCellIdentity(candidate, previousCurrent))
				{
					_currentCell = std::move(candidate);
					found = true;
				}
			}
			if (!found && previousCurrent._itemOccurrence
				== DataGridCellInfo::InvalidIndex)
				for (index = 0; index < items.Get()->Count(); ++index)
				{
					DataGridCellInfo candidate;
					if (TryCreateCellInfo(index, columnIndex, candidate)
						&& SameCellIdentity(candidate, previousCurrent))
					{
						_currentCell = std::move(candidate);
						found = true;
						break;
					}
				}
		}
		if (!found)
		{
			const auto previous = _currentCell;
			_currentCell = {};
			if (!RaiseCurrentCellChanged(previous)) return;
		}
	}
	if (!ReconcileSelectedCells()) return;
	if (!SynchronizeSelectedCellsFromRows()) return;
	auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (live && !live->IsItemsSourceUpdateInProgress())
		(void)live->RefreshRowHeaderActualWidth();
	live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (live) live->RequestAccessibilityStructureChanged();
}

void DataGrid::OnGeneratedItemsRealized()
{
	const ControlWeakReference ownerLifetime(this);
	ListBox::OnGeneratedItemsRealized();
	auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live) return;
	std::vector<ControlWeakReference> rows;
	rows.reserve(live->GetRealizedItems().size());
	for (const auto& [index, realized] : live->GetRealizedItems())
	{
		(void)realized;
		if (auto* row = dynamic_cast<DataGridRow*>(live->GetGeneratedItem(index)))
			rows.emplace_back(row);
	}
	for (const auto& rowLifetime : rows)
	{
		auto* row = dynamic_cast<DataGridRow*>(rowLifetime.Get());
		if (row && row->GetDataGridOwner() == live)
		{
			row->UpdateColumnWidths();
			live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
			row = dynamic_cast<DataGridRow*>(rowLifetime.Get());
			if (!live || !row || row->GetDataGridOwner() != live) return;
			row->UpdateRowHeader();
			live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
			row = dynamic_cast<DataGridRow*>(rowLifetime.Get());
			if (!live || !row || row->GetDataGridOwner() != live) return;
			row->UpdateHorizontalScrollOffset(live->_horizontalScrollOffset);
		}
		live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
		if (!live) return;
	}
	if (!live->IsItemsSourceUpdateInProgress()
		&& !live->RefreshRowHeaderActualWidth()) return;
	live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live) return;
	if (live->RefreshSelectedCellContainers(false, true))
		live->_selectedCellsVisualRefreshPending = false;
	live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (live && HasRowHeaders(live->_headersVisibility))
		live->RequestAccessibilityStructureChanged();
}

void DataGrid::OnGeneratedItemIndexChanged(
	Control& visual, size_t oldIndex, size_t newIndex)
{
	if (auto* row = dynamic_cast<DataGridRow*>(&visual))
	{
		row->SetItemIndex(newIndex);
		row->UpdateRowHeader();
		if (_currentCell.IsValid()
			&& _currentCell.RowIndex == oldIndex
			&& row->GetItem().Shared() == _currentCell.Item.Shared())
			_currentCell.RowIndex = newIndex;
		for (auto& cell : _selectedCells._cells)
			if (cell.RowIndex == oldIndex
				&& cell.Item.Shared() == row->GetItem().Shared())
				cell.RowIndex = newIndex;
		bool excludedLocatorChanged = false;
		for (auto& cell : _selectedCells._excludedCells)
			if (cell.RowIndex == oldIndex
				&& cell.Item.Shared() == row->GetItem().Shared())
			{
				cell.RowIndex = newIndex;
				excludedLocatorChanged = true;
			}
		if (excludedLocatorChanged)
			_selectedCells.InvalidateExcludedOffsets();
		if (_selectionAnchor
			&& _selectionAnchor->RowIndex == oldIndex
			&& _selectionAnchor->Item.Shared() == row->GetItem().Shared())
			_selectionAnchor->RowIndex = newIndex;
	}
}

void DataGrid::OnSelectedIndexChanged(int oldValue, int newValue)
{
	ListBox::OnSelectedIndexChanged(oldValue, newValue);
}

void DataGrid::OnSelectionChanged(SelectionChangedEventArgs& args)
{
	const ControlWeakReference ownerLifetime(this);
	if (++_rowSelectionRevision == 0) _rowSelectionRevision = 1;
	HandleRowSelectionChanged(args);
	if (auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get()))
		live->Selector::OnSelectionChanged(args);
}

bool DataGrid::HandleCellKey(Key key, ModifierKeys modifiers)
{
	ControlWeakReference ownerLifetime(this);
	if (key == Key::F2)
	{
		KeyEventArgs args(key, modifiers);
		return BeginEdit(&args);
	}
	if (key == Key::Space)
	{
		auto* cell = ResolveCurrentCellContainer();
		if (cell && !cell->GetIsEditing()
			&& dynamic_cast<DataGridCheckBoxColumn*>(cell->GetColumn()))
		{
			KeyEventArgs args(key, modifiers);
			return BeginEditCore(&args, true);
		}
		return false;
	}
	if (key == Key::Escape)
	{
		if (auto* cell = ResolveCurrentCellContainer();
			cell && cell->GetIsEditing()) return CancelEdit();
		return false;
	}
	if (key == Key::A && HasModifier(modifiers, ModifierKeys::Control)
		&& GetSelectionMode() == SelectionMode::Extended)
	{
		HandleSelectAll();
		return true;
	}
	if (_columns.empty() || ItemCount() == 0)
		return false;
	const bool navigation = key == Key::Left || key == Key::Right
		|| key == Key::Up || key == Key::Down || key == Key::Home
		|| key == Key::End || key == Key::Prior || key == Key::Next
		|| key == Key::Tab || key == Key::Return;
	if (!navigation) return false;

	const size_t columnCount = _columns.size();
	const size_t rowCount = ItemCount();
	size_t rowIndex = _currentCell.IsValid() ? _currentCell.RowIndex : 0;
	size_t columnIndex = _currentCell.IsValid() ? _currentCell.ColumnIndex : 0;
	const size_t previousRowIndex = rowIndex;
	const bool wasEditing = [&]
	{
		auto* cell = ResolveCurrentCellContainer();
		return cell && cell->GetIsEditing();
	}();
	const bool control = HasModifier(modifiers, ModifierKeys::Control);
	const bool shift = HasModifier(modifiers, ModifierKeys::Shift);
	if (key == Key::Tab)
	{
		const size_t total = rowCount * columnCount;
		long long flat = _currentCell.IsValid()
			? static_cast<long long>(rowIndex * columnCount + columnIndex)
			: (shift ? static_cast<long long>(total) : -1);
		flat += shift ? -1 : 1;
		if (flat < 0 || flat >= static_cast<long long>(total))
		{
			if (wasEditing && !CommitEdit()) return true;
			if (!ownerLifetime.Get()) return true;
			if (auto* window = GetPresentationWindow())
				(void)window->MoveFocus(shift
					? FocusNavigationDirection::Previous
					: FocusNavigationDirection::Next);
			return true;
		}
		rowIndex = static_cast<size_t>(flat) / columnCount;
		columnIndex = static_cast<size_t>(flat) % columnCount;
	}
	else if (key == Key::Left)
		columnIndex = control ? 0 : (columnIndex == 0 ? 0 : columnIndex - 1);
	else if (key == Key::Right)
		columnIndex = control ? columnCount - 1
			: (std::min)(columnIndex + 1, columnCount - 1);
	else if (key == Key::Up)
		rowIndex = control ? 0 : (rowIndex == 0 ? 0 : rowIndex - 1);
	else if (key == Key::Down)
		rowIndex = control ? rowCount - 1
			: (std::min)(rowIndex + 1, rowCount - 1);
	else if (key == Key::Home)
	{
		columnIndex = 0;
		if (control) rowIndex = 0;
	}
	else if (key == Key::End)
	{
		columnIndex = columnCount - 1;
		if (control) rowIndex = rowCount - 1;
	}
	else if (key == Key::Prior || key == Key::Next)
	{
		const double rowHeight = (std::max)(
			1.0, static_cast<double>(GetVirtualizedItemHeight()));
		const size_t page = (std::max)(size_t{ 1 }, static_cast<size_t>(
			(std::max)(1.0, static_cast<double>(GetActualSizeDip().height))
			/ rowHeight));
		if (key == Key::Prior)
			rowIndex = rowIndex > page ? rowIndex - page : 0;
		else rowIndex = (std::min)(rowIndex + page, rowCount - 1);
	}
	else if (key == Key::Return)
	{
		if (auto* cell = ResolveCurrentCellContainer();
			cell && cell->GetIsEditing())
		{
			if (!CommitEdit()) return true;
			if (!ownerLifetime.Get()) return true;
		}
		if (control) return true;
		if (shift)
		{
			if (rowIndex == 0) return true;
			--rowIndex;
		}
		else
		{
			if (rowIndex + 1 >= rowCount) return true;
			++rowIndex;
		}
	}
	if (_currentCell.IsValid() && rowIndex == _currentCell.RowIndex
		&& columnIndex == _currentCell.ColumnIndex)
	{
		if (key == Key::Left || key == Key::Right
			|| key == Key::Up || key == Key::Down)
		{
			if (auto* window = GetPresentationWindow())
			{
				const auto direction = key == Key::Left
					? FocusNavigationDirection::Left
					: key == Key::Right
						? FocusNavigationDirection::Right
						: key == Key::Up
							? FocusNavigationDirection::Up
							: FocusNavigationDirection::Down;
				(void)window->MoveFocus(direction);
			}
			return true;
		}
		return true;
	}
	if (!SetCurrentCell(rowIndex, columnIndex))
		return ownerLifetime.Get() ? false : true;
	if (!ownerLifetime.Get()) return true;
	(void)BringItemIntoView(rowIndex);
	if (!ownerLifetime.Get()) return true;
	ModifierKeys selectionModifiers = ModifierKeys::None;
	if (key != Key::Tab && key != Key::Return && shift)
		selectionModifiers = control
			? ModifierKeys::Shift | ModifierKeys::Control
			: ModifierKeys::Shift;
	(void)ApplySelectionForCellInput(
		rowIndex, columnIndex, selectionModifiers, true, false);
	if (!ownerLifetime.Get()) return true;
	const bool continueEditing = wasEditing
		&& rowIndex == previousRowIndex
		&& (key == Key::Tab
			|| key == Key::Left || key == Key::Right);
	if (continueEditing)
	{
		(void)BeginEdit();
		return true;
	}
	if (auto* window = GetPresentationWindow())
	{
		auto* target = ResolveCurrentCellContainer();
		window->SetKeyboardFocus(target ? static_cast<Control*>(target) : this, true);
	}
	return true;
}

bool DataGrid::HandlesNavigationKey(Key key) const
{
	if (key == Key::Escape)
	{
		auto* cell = ResolveCurrentCellContainer();
		return cell && cell->GetIsEditing();
	}
	switch (key)
	{
	case Key::Tab:
	case Key::Left:
	case Key::Right:
	case Key::Up:
	case Key::Down:
	case Key::Home:
	case Key::End:
	case Key::Prior:
	case Key::Next:
	case Key::Return:
		return !_columns.empty() && ItemCount() != 0;
	default:
		return ListBox::HandlesNavigationKey(key);
	}
}

bool DataGrid::ApplyTextInput(const TextCompositionEventArgs& input)
{
	const ControlWeakReference ownerLifetime(this);
	if (input.Text.empty() || _isReadOnly)
		return ListBox::ApplyTextInput(input);
	if (!_currentCell.IsValid())
	{
		if (_columns.empty() || ItemCount() == 0
			|| !SetCurrentCell(0, 0))
			return ownerLifetime.Get() ? false : true;
	}
	auto* live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
	if (!live) return true;
	if (!dynamic_cast<DataGridTextColumn*>(live->_currentCell.Column))
		return live->ListBox::ApplyTextInput(input);
	if (!live->BeginEdit(&input))
	{
		live = dynamic_cast<DataGrid*>(ownerLifetime.Get());
		return live ? live->ListBox::ApplyTextInput(input) : true;
	}
	return true;
}

bool DataGrid::ProcessInput(const InputReport& input)
{
	ControlWeakReference ownerLifetime(this);
	if (input.Kind == InputReportKind::KeyDown)
	{
		const bool handled = HandleCellKey(input.Key, input.Modifiers);
		if (!ownerLifetime.Get() || handled) return true;
	}
	return ListBox::ProcessInput(input);
}
