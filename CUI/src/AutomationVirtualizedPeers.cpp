#include "AutomationPeer.h"

#include "CalendarView.h"
#include "ChartView.h"
#include "Control.h"
#include "DataGrid.h"
#include "ScrollViewer.h"

#include <algorithm>
#include <cmath>

ScrollViewerAutomationPeer::ScrollViewerAutomationPeer(Control& owner) :
	AutomationPeer(owner, AutomationControlType::Pane, L"ScrollViewer")
{
}

AutomationPattern
ScrollViewerAutomationPeer::GetPatternSet() const noexcept
{
	return AutomationPattern::Scroll;
}

bool ScrollViewerAutomationPeer::GetAccessibilityScrollInfo(
	AccessibilityScrollInfo& result) const noexcept
{
	result = {};
	auto* scroll = dynamic_cast<ScrollViewer*>(&Owner());
	if (!scroll) return false;
	const double horizontalMaximum =
		(std::max)(0.0, scroll->ExtentWidth - scroll->ViewportWidth);
	const double verticalMaximum =
		(std::max)(0.0, scroll->ExtentHeight - scroll->ViewportHeight);
	result.HorizontallyScrollable = horizontalMaximum > 0.0;
	result.VerticallyScrollable = verticalMaximum > 0.0;
	if (result.HorizontallyScrollable)
	{
		result.HorizontalScrollPercent =
			(std::clamp)(scroll->HorizontalOffset
				/ horizontalMaximum * 100.0, 0.0, 100.0);
		result.HorizontalViewSize = scroll->ExtentWidth > 0.0
			? (std::clamp)(scroll->ViewportWidth
				/ scroll->ExtentWidth * 100.0, 0.0, 100.0)
			: 100.0;
	}
	if (result.VerticallyScrollable)
	{
		result.VerticalScrollPercent =
			(std::clamp)(scroll->VerticalOffset
				/ verticalMaximum * 100.0, 0.0, 100.0);
		result.VerticalViewSize = scroll->ExtentHeight > 0.0
			? (std::clamp)(scroll->ViewportHeight
				/ scroll->ExtentHeight * 100.0, 0.0, 100.0)
			: 100.0;
	}
	return true;
}

bool ScrollViewerAutomationPeer::ScrollAccessibility(
	AccessibilityScrollAmount horizontal,
	AccessibilityScrollAmount vertical)
{
	auto* scroll = dynamic_cast<ScrollViewer*>(&Owner());
	if (!scroll) return false;
	auto applyHorizontal = [scroll](AccessibilityScrollAmount amount)
	{
		switch (amount)
		{
		case AccessibilityScrollAmount::LargeDecrement:
			scroll->PageLeft(); break;
		case AccessibilityScrollAmount::SmallDecrement:
			scroll->LineLeft(); break;
		case AccessibilityScrollAmount::LargeIncrement:
			scroll->PageRight(); break;
		case AccessibilityScrollAmount::SmallIncrement:
			scroll->LineRight(); break;
		case AccessibilityScrollAmount::NoAmount: break;
		}
	};
	auto applyVertical = [scroll](AccessibilityScrollAmount amount)
	{
		switch (amount)
		{
		case AccessibilityScrollAmount::LargeDecrement:
			scroll->PageUp(); break;
		case AccessibilityScrollAmount::SmallDecrement:
			scroll->LineUp(); break;
		case AccessibilityScrollAmount::LargeIncrement:
			scroll->PageDown(); break;
		case AccessibilityScrollAmount::SmallIncrement:
			scroll->LineDown(); break;
		case AccessibilityScrollAmount::NoAmount: break;
		}
	};
	applyHorizontal(horizontal);
	applyVertical(vertical);
	return true;
}

bool ScrollViewerAutomationPeer::SetAccessibilityScrollPercent(
	double horizontalPercent,
	double verticalPercent)
{
	auto* scroll = dynamic_cast<ScrollViewer*>(&Owner());
	if (!scroll) return false;
	auto valid = [](double value)
	{
		return value == AccessibilityScrollNoChange
			|| (std::isfinite(value) && value >= 0.0 && value <= 100.0);
	};
	if (!valid(horizontalPercent) || !valid(verticalPercent)) return false;
	if (horizontalPercent != AccessibilityScrollNoChange)
	{
		const double maximum =
			(std::max)(0.0, scroll->ExtentWidth - scroll->ViewportWidth);
		if (maximum <= 0.0 && horizontalPercent != 0.0) return false;
		scroll->ScrollToHorizontalOffset(
			maximum * horizontalPercent / 100.0);
	}
	if (verticalPercent != AccessibilityScrollNoChange)
	{
		const double maximum =
			(std::max)(0.0, scroll->ExtentHeight - scroll->ViewportHeight);
		if (maximum <= 0.0 && verticalPercent != 0.0) return false;
		scroll->ScrollToVerticalOffset(
			maximum * verticalPercent / 100.0);
	}
	return true;
}

CalendarViewAutomationPeer::CalendarViewAutomationPeer(Control& owner) :
	AutomationPeer(owner, AutomationControlType::Calendar,
		owner.Type() == UIClass::UI_Calendar ? L"Calendar" : L"CalendarView")
{
}

AutomationPattern
CalendarViewAutomationPeer::GetPatternSet() const noexcept
{
	return AutomationPattern::Selection
		| AutomationPattern::Grid
		| AutomationPattern::Table;
}

bool CalendarViewAutomationPeer::TryGetAccessibilityVirtualNode(
	uint32_t id, AccessibilityVirtualNode& result)
{
	if (!static_cast<CalendarView&>(Owner())
		.TryGetAccessibilityVirtualNode(id, result)) return false;
	if (result.ClassName.empty()) result.ClassName = L"CUI.VirtualItem";
	result.KeyboardFocusable = result.Enabled && result.Visible
		&& HasAutomationPattern(
			result.Patterns, AutomationPattern::SelectionItem);
	result.IsContentElement =
		result.ControlType != AutomationControlType::HeaderItem;
	return true;
}

size_t CalendarViewAutomationPeer::GetAccessibilityVirtualChildCount(
	uint32_t parentId)
{
	return static_cast<const CalendarView&>(Owner())
		.GetAccessibilityVirtualChildCount(parentId);
}

bool CalendarViewAutomationPeer::TryGetAccessibilityVirtualChildAt(
	uint32_t parentId, size_t index, uint32_t& result)
{
	return static_cast<const CalendarView&>(Owner())
		.TryGetAccessibilityVirtualChildAt(parentId, index, result);
}

bool CalendarViewAutomationPeer::TryGetAccessibilityVirtualSibling(
	uint32_t parentId, uint32_t id, bool next, uint32_t& result)
{
	return static_cast<const CalendarView&>(Owner())
		.TryGetAccessibilityVirtualSibling(parentId, id, next, result);
}

bool CalendarViewAutomationPeer::TryHitTestAccessibilityVirtualNode(
	float localX, float localY, uint32_t& result)
{
	return static_cast<const CalendarView&>(Owner())
		.TryHitTestAccessibilityVirtualNode(localX, localY, result);
}

AccessibilityVirtualContainerInfo
CalendarViewAutomationPeer::GetAccessibilityVirtualContainerInfo()
const noexcept
{
	return static_cast<const CalendarView&>(Owner())
		.GetAccessibilityVirtualContainerInfo();
}

void CalendarViewAutomationPeer::GetAccessibilityVirtualSelection(
	std::vector<uint32_t>& result)
{
	static_cast<const CalendarView&>(Owner())
		.GetAccessibilityVirtualSelection(result);
}

bool CalendarViewAutomationPeer::GetAccessibilityVirtualItemAt(
	int row, int column, uint32_t& result)
{
	return static_cast<const CalendarView&>(Owner())
		.GetAccessibilityVirtualItemAt(row, column, result);
}

void CalendarViewAutomationPeer::GetAccessibilityVirtualColumnHeaders(
	std::vector<uint32_t>& result)
{
	static_cast<const CalendarView&>(Owner())
		.GetAccessibilityVirtualColumnHeaders(result);
}

bool CalendarViewAutomationPeer::InvokeAccessibilityVirtualNode(uint32_t id)
{
	return static_cast<CalendarView&>(Owner())
		.InvokeAccessibilityVirtualNode(id);
}

bool CalendarViewAutomationPeer::SelectAccessibilityVirtualNode(
	uint32_t id, AccessibilitySelectionAction action)
{
	return static_cast<CalendarView&>(Owner())
		.SelectAccessibilityVirtualNode(id, action);
}

DataGridAutomationPeer::DataGridAutomationPeer(Control& owner) :
	AutomationPeer(owner, AutomationControlType::DataGrid, L"DataGrid")
{
}

std::wstring DataGridAutomationPeer::GetAutomationClassName() const
{
	// WPF's DataGridAutomationPeer returns Owner.GetType().Name rather than a
	// framework-qualified automation class name.
	return L"DataGrid";
}

AutomationPattern DataGridAutomationPeer::GetPatternSet() const noexcept
{
	auto result = AutomationPattern::Selection
		| AutomationPattern::Grid
		| AutomationPattern::Table;
	AccessibilityScrollInfo scrollInfo;
	if (static_cast<const DataGrid&>(Owner())
		.GetAccessibilityScrollInfo(scrollInfo))
		result |= AutomationPattern::Scroll;
	return result;
}

bool DataGridAutomationPeer::CanSelectMultiple() const noexcept
{
	return static_cast<const DataGrid&>(Owner()).GetSelectionMode()
		!= SelectionMode::Single;
}

bool DataGridAutomationPeer::TryGetAccessibilityVirtualNode(
	uint32_t id, AccessibilityVirtualNode& result)
{
	return static_cast<DataGrid&>(Owner())
		.TryGetAccessibilityVirtualNode(id, result);
}

size_t DataGridAutomationPeer::GetAccessibilityVirtualChildCount(
	uint32_t parentId)
{
	return static_cast<const DataGrid&>(Owner())
		.GetAccessibilityVirtualChildCount(parentId);
}

bool DataGridAutomationPeer::TryGetAccessibilityVirtualChildAt(
	uint32_t parentId, size_t index, uint32_t& result)
{
	return static_cast<const DataGrid&>(Owner())
		.TryGetAccessibilityVirtualChildAt(parentId, index, result);
}

bool DataGridAutomationPeer::TryGetAccessibilityVirtualSibling(
	uint32_t parentId, uint32_t id, bool next, uint32_t& result)
{
	return static_cast<const DataGrid&>(Owner())
		.TryGetAccessibilityVirtualSibling(parentId, id, next, result);
}

bool DataGridAutomationPeer::TryHitTestAccessibilityVirtualNode(
	float localX, float localY, uint32_t& result)
{
	return static_cast<const DataGrid&>(Owner())
		.TryHitTestAccessibilityVirtualNode(localX, localY, result);
}

AccessibilityVirtualContainerInfo
DataGridAutomationPeer::GetAccessibilityVirtualContainerInfo() const noexcept
{
	return static_cast<const DataGrid&>(Owner())
		.GetAccessibilityVirtualContainerInfo();
}

void DataGridAutomationPeer::GetAccessibilityVirtualSelection(
	std::vector<uint32_t>& result)
{
	auto& owner = static_cast<const DataGrid&>(Owner());
	// Direct peer consumers do not have a COM provider to hold the semantic
	// object. Pin this peer before source callbacks can synchronously delete the
	// DataGrid; the DataGrid operation itself guards owner lifetime/revisions.
	auto operationLease = owner.AcquireAutomationPeer();
	if (!operationLease || operationLease.get() != this)
	{
		result.clear();
		return;
	}
	owner.GetAccessibilityVirtualSelection(result);
}

bool DataGridAutomationPeer::GetAccessibilityVirtualItemAt(
	int row, int column, uint32_t& result)
{
	return static_cast<const DataGrid&>(Owner())
		.GetAccessibilityVirtualItemAt(row, column, result);
}

void DataGridAutomationPeer::GetAccessibilityVirtualColumnHeaders(
	std::vector<uint32_t>& result)
{
	static_cast<const DataGrid&>(Owner())
		.GetAccessibilityVirtualColumnHeaders(result);
}

void DataGridAutomationPeer::GetAccessibilityVirtualRowHeaders(
	std::vector<uint32_t>& result)
{
	static_cast<const DataGrid&>(Owner())
		.GetAccessibilityVirtualRowHeaders(result);
}

AutomationOperationResult
DataGridAutomationPeer::FocusAccessibilityVirtualNode(uint32_t id)
{
	return static_cast<DataGrid&>(Owner())
		.FocusAccessibilityVirtualNode(id);
}

bool DataGridAutomationPeer::TryGetAccessibilityVirtualFocusedNode(
	uint32_t& result)
{
	return static_cast<const DataGrid&>(Owner())
		.TryGetAccessibilityVirtualFocusedNode(result);
}

bool DataGridAutomationPeer::InvokeAccessibilityVirtualNode(uint32_t id)
{
	return static_cast<DataGrid&>(Owner())
		.InvokeAccessibilityVirtualNode(id);
}

bool DataGridAutomationPeer::SetAccessibilityVirtualNodeValue(
	uint32_t id, const std::wstring& value)
{
	return static_cast<DataGrid&>(Owner())
		.SetAccessibilityVirtualNodeValue(id, value);
}

bool DataGridAutomationPeer::SelectAccessibilityVirtualNode(
	uint32_t id, AccessibilitySelectionAction action)
{
	return static_cast<DataGrid&>(Owner())
		.SelectAccessibilityVirtualNode(id, action);
}

bool DataGridAutomationPeer::ScrollAccessibilityVirtualNodeIntoView(
	uint32_t id)
{
	return static_cast<DataGrid&>(Owner())
		.ScrollAccessibilityVirtualNodeIntoView(id);
}

bool DataGridAutomationPeer::GetAccessibilityScrollInfo(
	AccessibilityScrollInfo& result) const noexcept
{
	return static_cast<const DataGrid&>(Owner())
		.GetAccessibilityScrollInfo(result);
}

bool DataGridAutomationPeer::ScrollAccessibility(
	AccessibilityScrollAmount horizontal,
	AccessibilityScrollAmount vertical)
{
	return static_cast<DataGrid&>(Owner())
		.ScrollAccessibility(horizontal, vertical);
}

bool DataGridAutomationPeer::SetAccessibilityScrollPercent(
	double horizontalPercent, double verticalPercent)
{
	return static_cast<DataGrid&>(Owner())
		.SetAccessibilityScrollPercent(horizontalPercent, verticalPercent);
}

ChartViewAutomationPeer::ChartViewAutomationPeer(Control& owner) :
	AutomationPeer(owner, AutomationControlType::Image, L"ChartView")
{
}

AutomationPattern ChartViewAutomationPeer::GetPatternSet() const noexcept
{
	return AutomationPattern::Selection;
}

bool ChartViewAutomationPeer::TryGetAccessibilityVirtualNode(
	uint32_t id, AccessibilityVirtualNode& result)
{
	if (!static_cast<ChartView&>(Owner())
		.TryGetAccessibilityVirtualNode(id, result)) return false;
	if (result.ClassName.empty()) result.ClassName = L"CUI.VirtualItem";
	result.KeyboardFocusable = result.Enabled && result.Visible
		&& HasAutomationPattern(
			result.Patterns, AutomationPattern::SelectionItem);
	result.IsContentElement =
		result.ControlType != AutomationControlType::HeaderItem;
	return true;
}

size_t ChartViewAutomationPeer::GetAccessibilityVirtualChildCount(
	uint32_t parentId)
{
	return static_cast<const ChartView&>(Owner())
		.GetAccessibilityVirtualChildCount(parentId);
}

bool ChartViewAutomationPeer::TryGetAccessibilityVirtualChildAt(
	uint32_t parentId, size_t index, uint32_t& result)
{
	return static_cast<const ChartView&>(Owner())
		.TryGetAccessibilityVirtualChildAt(parentId, index, result);
}

bool ChartViewAutomationPeer::TryGetAccessibilityVirtualSibling(
	uint32_t parentId, uint32_t id, bool next, uint32_t& result)
{
	return static_cast<const ChartView&>(Owner())
		.TryGetAccessibilityVirtualSibling(parentId, id, next, result);
}

bool ChartViewAutomationPeer::TryHitTestAccessibilityVirtualNode(
	float localX, float localY, uint32_t& result)
{
	return static_cast<ChartView&>(Owner())
		.TryHitTestAccessibilityVirtualNode(localX, localY, result);
}

AccessibilityVirtualContainerInfo
ChartViewAutomationPeer::GetAccessibilityVirtualContainerInfo() const noexcept
{
	return static_cast<const ChartView&>(Owner())
		.GetAccessibilityVirtualContainerInfo();
}

void ChartViewAutomationPeer::GetAccessibilityVirtualSelection(
	std::vector<uint32_t>& result)
{
	static_cast<const ChartView&>(Owner())
		.GetAccessibilityVirtualSelection(result);
}

bool ChartViewAutomationPeer::InvokeAccessibilityVirtualNode(uint32_t id)
{
	return static_cast<ChartView&>(Owner())
		.InvokeAccessibilityVirtualNode(id);
}

bool ChartViewAutomationPeer::SelectAccessibilityVirtualNode(
	uint32_t id, AccessibilitySelectionAction action)
{
	return static_cast<ChartView&>(Owner())
		.SelectAccessibilityVirtualNode(id, action);
}
