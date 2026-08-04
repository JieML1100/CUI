#include "ChartView.h"
#include "EventInfrastructure.h"
#include "Window.h"
#include "AdvancedControlPropertyRegistration.h"
#include <Colors.h>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace
{
	constexpr float ChartPi = 3.14159265358979323846f;

	D2D1_COLOR_F PaletteColor(int index)
	{
		static const D2D1_COLOR_F palette[] = {
			{ 0.17f, 0.49f, 0.96f, 0.95f },
			{ 0.10f, 0.68f, 0.55f, 0.95f },
			{ 0.94f, 0.53f, 0.18f, 0.95f },
			{ 0.78f, 0.33f, 0.74f, 0.95f },
			{ 0.90f, 0.27f, 0.36f, 0.95f },
			{ 0.48f, 0.62f, 0.25f, 0.95f },
			{ 0.25f, 0.67f, 0.84f, 0.95f },
			{ 0.66f, 0.48f, 0.95f, 0.95f },
		};
		return palette[(std::max)(0, index) % (sizeof(palette) / sizeof(palette[0]))];
	}

	D2D1_COLOR_F WithAlpha(D2D1_COLOR_F color, float alpha)
	{
		color.a = alpha;
		return color;
	}

	bool IsTransparent(D2D1_COLOR_F color)
	{
		return color.a <= 0.0001f;
	}

	bool PointInRect(float x, float y, const D2D1_RECT_F& rect)
	{
		return x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom;
	}

	float NormalizeAngle(float angle)
	{
		while (angle < 0.0f) angle += 360.0f;
		while (angle >= 360.0f) angle -= 360.0f;
		return angle;
	}

	bool AngleInSweep(float angle, float startAngle, float sweepAngle)
	{
		if (std::fabs(sweepAngle) >= 359.9f) return true;
		angle = NormalizeAngle(angle);
		startAngle = NormalizeAngle(startAngle);
		float endAngle = startAngle + sweepAngle;
		if (sweepAngle >= 0.0f)
		{
			if (endAngle < 360.0f) return angle >= startAngle && angle <= endAngle;
			return angle >= startAngle || angle <= NormalizeAngle(endAngle);
		}

		if (endAngle >= 0.0f) return angle <= startAngle && angle >= endAngle;
		return angle <= startAngle || angle >= NormalizeAngle(endAngle);
	}

	float RectWidth(const D2D1_RECT_F& rect)
	{
		return (std::max)(0.0f, rect.right - rect.left);
	}

	float RectHeight(const D2D1_RECT_F& rect)
	{
		return (std::max)(0.0f, rect.bottom - rect.top);
	}

}

ChartPoint::ChartPoint(std::wstring label, double value)
	: Label(std::move(label)), Value(value)
{
}

ChartPoint::ChartPoint(std::wstring label, double value, D2D1_COLOR_F color)
	: Label(std::move(label)), Value(value), Color(color), UseCustomColor(true)
{
}

ChartSeries::ChartSeries(std::wstring name, D2D1_COLOR_F color)
	: Name(std::move(name)), Color(color)
{
}

UIClass ChartView::Type()
{
	return UIClass::UI_ChartView;
}

const DependencyProperty& ChartView::ChartKindProperty()
{
	using namespace cui::advanced_properties;
	static const auto registration = RegisterEnumFieldStatic(
		DependencyPropertyRegistrationLiteral(L"ChartKind"),
		&ChartView::_chartKind, ChartViewKind::Bar,
		{ ChartViewKind::Bar, ChartViewKind::Pie, ChartViewKind::Line }
		CUI_DESIGN_METADATA_ARGUMENTS(
			L"Data", 600, 30,
			{ { L"Bar", ChartViewKind::Bar },
			  { L"Pie", ChartViewKind::Pie },
			  { L"Line", ChartViewKind::Line } }));
	return *registration;
}

const DependencyProperty& ChartView::TitleProperty()
{
	using namespace cui::advanced_properties;
	static const auto registration = RegisterFieldStatic(
		DependencyPropertyRegistrationLiteral(L"Title"),
		&ChartView::_title, std::wstring(L"Chart")
		CUI_DESIGN_METADATA_ARGUMENTS(
			L"Data", 600, 10, DependencyPropertyEditorKind::Text));
	return *registration;
}

const DependencyProperty& ChartView::SubtitleProperty()
{
	using namespace cui::advanced_properties;
	static const auto registration = RegisterFieldStatic(
		DependencyPropertyRegistrationLiteral(L"Subtitle"),
		&ChartView::_subtitle, std::wstring{}
		CUI_DESIGN_METADATA_ARGUMENTS(
			L"Data", 600, 20, DependencyPropertyEditorKind::Text));
	return *registration;
}

const DependencyProperty& ChartView::ValuePrecisionProperty()
{
	using namespace cui::advanced_properties;
	static const auto registration = RegisterIntMetricStatic(
		DependencyPropertyRegistrationLiteral(L"ValuePrecision"),
		&ChartView::_valuePrecision, 0
		CUI_DESIGN_METADATA_ARGUMENTS(L"Data", 600, 40), 0, 8);
	return *registration;
}

const DependencyProperty& ChartView::ShowLegendProperty()
{
	using namespace cui::advanced_properties;
	static const auto registration = RegisterFieldStatic(
		DependencyPropertyRegistrationLiteral(L"ShowLegend"),
		&ChartView::_showLegend, true CUI_DESIGN_METADATA_ARGUMENTS(
			L"Behavior", 110, 10, DependencyPropertyEditorKind::Boolean));
	return *registration;
}

const DependencyProperty& ChartView::ShowTooltipProperty()
{
	using namespace cui::advanced_properties;
	static const auto registration = RegisterFieldStatic(
		DependencyPropertyRegistrationLiteral(L"ShowTooltip"),
		&ChartView::_showTooltip, true CUI_DESIGN_METADATA_ARGUMENTS(
			L"Behavior", 110, 20, DependencyPropertyEditorKind::Boolean));
	return *registration;
}

const DependencyProperty& ChartView::ShowValueLabelsProperty()
{
	using namespace cui::advanced_properties;
	static const auto registration = RegisterFieldStatic(
		DependencyPropertyRegistrationLiteral(L"ShowValueLabels"),
		&ChartView::_showValueLabels, false CUI_DESIGN_METADATA_ARGUMENTS(
			L"Behavior", 110, 30, DependencyPropertyEditorKind::Boolean));
	return *registration;
}

const DependencyProperty& ChartView::ShowGridLinesProperty()
{
	using namespace cui::advanced_properties;
	static const auto registration = RegisterFieldStatic(
		DependencyPropertyRegistrationLiteral(L"ShowGridLines"),
		&ChartView::_showGridLines, true CUI_DESIGN_METADATA_ARGUMENTS(
			L"Behavior", 110, 40, DependencyPropertyEditorKind::Boolean));
	return *registration;
}

const DependencyProperty& ChartView::ShowMarkersProperty()
{
	using namespace cui::advanced_properties;
	static const auto registration = RegisterFieldStatic(
		DependencyPropertyRegistrationLiteral(L"ShowMarkers"),
		&ChartView::_showMarkers, true CUI_DESIGN_METADATA_ARGUMENTS(
			L"Behavior", 110, 50, DependencyPropertyEditorKind::Boolean));
	return *registration;
}

const DependencyProperty& ChartView::EnablePanZoomProperty()
{
	using namespace cui::advanced_properties;
	static const auto registration = RegisterFieldStatic(
		DependencyPropertyRegistrationLiteral(L"EnablePanZoom"),
		&ChartView::_enablePanZoom, true CUI_DESIGN_METADATA_ARGUMENTS(
			L"Behavior", 110, 60, DependencyPropertyEditorKind::Boolean));
	return *registration;
}

void ChartView::RegisterDependencyProperties()
{
	Control::RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	(void)ChartKindProperty();
	(void)TitleProperty();
	(void)SubtitleProperty();
	(void)ValuePrecisionProperty();
	(void)ShowLegendProperty();
	(void)ShowTooltipProperty();
	(void)ShowValueLabelsProperty();
	(void)ShowGridLinesProperty();
	(void)ShowMarkersProperty();
	(void)EnablePanZoomProperty();
#endif
}

GET_CPP(ChartView, ChartViewKind, ChartKind) { return _chartKind; }
SET_CPP(ChartView, ChartViewKind, ChartKind)
{
	(void)TrySetPropertyValue(
		ChartKindProperty(), BindingValue(static_cast<int>(value)));
}
GET_CPP(ChartView, std::wstring, Title) { return _title; }
SET_CPP(ChartView, std::wstring, Title)
{
	(void)SetPropertyField(TitleProperty(), _title, std::move(value));
}
GET_CPP(ChartView, std::wstring, Subtitle) { return _subtitle; }
SET_CPP(ChartView, std::wstring, Subtitle)
{
	(void)SetPropertyField(SubtitleProperty(), _subtitle, std::move(value));
}
GET_CPP(ChartView, int, ValuePrecision) { return _valuePrecision; }
SET_CPP(ChartView, int, ValuePrecision)
{
	(void)SetPropertyField(ValuePrecisionProperty(), _valuePrecision, value);
}
GET_CPP(ChartView, bool, ShowLegend) { return _showLegend; }
SET_CPP(ChartView, bool, ShowLegend)
{
	(void)SetPropertyField(ShowLegendProperty(), _showLegend, value);
}
GET_CPP(ChartView, bool, ShowTooltip) { return _showTooltip; }
SET_CPP(ChartView, bool, ShowTooltip)
{
	(void)SetPropertyField(ShowTooltipProperty(), _showTooltip, value);
}
GET_CPP(ChartView, bool, ShowValueLabels) { return _showValueLabels; }
SET_CPP(ChartView, bool, ShowValueLabels)
{
	(void)SetPropertyField(
		ShowValueLabelsProperty(), _showValueLabels, value);
}
GET_CPP(ChartView, bool, ShowGridLines) { return _showGridLines; }
SET_CPP(ChartView, bool, ShowGridLines)
{
	(void)SetPropertyField(ShowGridLinesProperty(), _showGridLines, value);
}
GET_CPP(ChartView, bool, ShowMarkers) { return _showMarkers; }
SET_CPP(ChartView, bool, ShowMarkers)
{
	(void)SetPropertyField(ShowMarkersProperty(), _showMarkers, value);
}
GET_CPP(ChartView, bool, EnablePanZoom) { return _enablePanZoom; }
SET_CPP(ChartView, bool, EnablePanZoom)
{
	(void)SetPropertyField(EnablePanZoomProperty(), _enablePanZoom, value);
}

ChartView::ChartView()
{
	this->RendererBackgroundColor = D2D1_COLOR_F{ 0, 0, 0, 0 };
	this->RendererBorderColor = cui::theme::palette::Border;
	this->RendererForegroundColor = cui::theme::palette::TextPrimary;
}

void ChartView::Clear()
{
	_series.clear();
	_accessibilityIds.clear();
	HoveredSeriesIndex = -1;
	HoveredPointIndex = -1;
	SelectedSeriesIndex = -1;
	SelectedPointIndex = -1;
	ResetView();
	NotifyAccessibilityStructureChanged();
}

int ChartView::AddSeries(const ChartSeries& series)
{
	_series.push_back(series);
	auto& ids = _accessibilityIds.emplace_back();
	ids.reserve(series.Points.size());
	for (size_t index = 0; index < series.Points.size(); ++index)
		ids.push_back(AllocateAccessibilityVirtualId());
	ClampViewport();
	InvalidateVisual();
	NotifyAccessibilityStructureChanged();
	return static_cast<int>(_series.size()) - 1;
}

void ChartView::SetSingleSeries(const std::vector<ChartPoint>& points, const std::wstring& name)
{
	_series.clear();
	_accessibilityIds.clear();
	ChartSeries series(name, AccentColor);
	series.Points = points;
	_series.push_back(series);
	auto& ids = _accessibilityIds.emplace_back();
	ids.reserve(points.size());
	for (size_t index = 0; index < points.size(); ++index)
		ids.push_back(AllocateAccessibilityVirtualId());
	ResetView();
	NotifyAccessibilityStructureChanged();
}

void ChartView::ResetView()
{
	_scrolling = false;
	ZoomX = 1.0f;
	PanX = 0.0f;
	ClampViewport();
	InvalidateVisual();
	cui::framework::EventAccess::Raise(OnViewportChanged, this);
}

bool ChartView::SelectPoint(int seriesIndex, int pointIndex)
{
	if (seriesIndex < 0 || pointIndex < 0 || seriesIndex >= static_cast<int>(_series.size()))
		return false;
	if (pointIndex >= static_cast<int>(_series[seriesIndex].Points.size()))
		return false;

	if (SelectedSeriesIndex == seriesIndex && SelectedPointIndex == pointIndex)
		return true;

	const uint32_t previousId =
		GetAccessibilityId(SelectedSeriesIndex, SelectedPointIndex);
	const int previousPointIndex = SelectedPointIndex;
	SelectedSeriesIndex = seriesIndex;
	SelectedPointIndex = pointIndex;
	EnsurePointVisible(pointIndex);
	SelectionChangedEventArgs args(previousPointIndex, pointIndex);
	SelectionChanged(this, args);
	if (previousId != 0)
		NotifyAccessibilityVirtualChanged(
			previousId, AccessibilityChange::Selection);
	const uint32_t currentId =
		GetAccessibilityId(seriesIndex, pointIndex);
	if (currentId != 0)
	{
		NotifyAccessibilityVirtualChanged(
			currentId, AccessibilityChange::Selection);
		if (IsKeyboardFocused)
			NotifyAccessibilityVirtualChanged(
				currentId, AccessibilityChange::Focus);
	}
	InvalidateVisual();
	return true;
}

bool ChartView::HitTestPoint(int localX, int localY, int& seriesIndex, int& pointIndex)
{
	RebuildHitRegions();
	return HitTestInternal(localX, localY, seriesIndex, pointIndex);
}

CursorKind ChartView::QueryCursor(int localX, int localY)
{
	if (!IsEnabled) return CursorKind::Arrow;
	if (_scrolling) return CursorKind::SizeWE;

	int s = -1;
	int p = -1;
	if (HitTestPoint(localX, localY, s, p))
		return CursorKind::Hand;
	if (HasHorizontalScrollBar())
	{
		const auto size = GetActualSizeDip();
		if (PointInRect((float)localX, (float)localY, GetHorizontalScrollTrackRect(size.width, size.height)))
			return CursorKind::SizeWE;
	}
	return Control::QueryCursor(localX, localY);
}

bool ChartView::CanHandleMouseWheel(int delta, int localX, int localY)
{
	(void)delta;
	if (!EnablePanZoom || ChartKind == ChartViewKind::Pie || GetPointCount() <= 1)
		return false;
	const auto size = GetActualSizeDip();
	return PointInRect((float)localX, (float)localY, GetPlotRect(size.width, size.height));
}

bool ChartView::HandlesNavigationKey(Key key) const
{
	return key == Key::Left || key == Key::Right
		|| key == Key::Up || key == Key::Down
		|| key == Key::PageUp || key == Key::PageDown
		|| key == Key::Home || key == Key::End
		|| key == Key::Return || key == Key::Space;
}

uint32_t ChartView::GetAccessibilityId(
	int seriesIndex, int pointIndex) const noexcept
{
	if (seriesIndex < 0 || pointIndex < 0
		|| static_cast<size_t>(seriesIndex) >= _accessibilityIds.size()
		|| static_cast<size_t>(pointIndex)
			>= _accessibilityIds[static_cast<size_t>(seriesIndex)].size())
		return 0;
	return _accessibilityIds[static_cast<size_t>(seriesIndex)]
		[static_cast<size_t>(pointIndex)];
}

bool ChartView::FindAccessibilityPoint(
	uint32_t id, int& seriesIndex, int& pointIndex) const noexcept
{
	seriesIndex = -1;
	pointIndex = -1;
	if (id == 0) return false;
	for (size_t series = 0; series < _accessibilityIds.size(); ++series)
	{
		const auto& ids = _accessibilityIds[series];
		const auto found = std::find(ids.begin(), ids.end(), id);
		if (found == ids.end()) continue;
		seriesIndex = static_cast<int>(series);
		pointIndex = static_cast<int>(std::distance(ids.begin(), found));
		return true;
	}
	return false;
}

size_t ChartView::GetAccessibilityVirtualChildCount(
	uint32_t parentId) const noexcept
{
	if (parentId != 0) return 0;
	size_t count = 0;
	for (size_t series = 0; series < _series.size(); ++series)
		if (_series[series].Visible)
			count += _series[series].Points.size();
	return count;
}

bool ChartView::TryGetAccessibilityVirtualChildAt(
	uint32_t parentId, size_t index, uint32_t& result) const noexcept
{
	result = 0;
	if (parentId != 0) return false;
	for (size_t series = 0; series < _series.size(); ++series)
	{
		if (!_series[series].Visible) continue;
		const size_t count = _series[series].Points.size();
		if (index >= count)
		{
			index -= count;
			continue;
		}
		result = GetAccessibilityId(
			static_cast<int>(series), static_cast<int>(index));
		return result != 0;
	}
	return false;
}

bool ChartView::TryGetAccessibilityVirtualSibling(
	uint32_t parentId, uint32_t id, bool next,
	uint32_t& result) const noexcept
{
	result = 0;
	if (parentId != 0) return false;
	const size_t count = GetAccessibilityVirtualChildCount(0);
	for (size_t index = 0; index < count; ++index)
	{
		uint32_t current = 0;
		if (!TryGetAccessibilityVirtualChildAt(0, index, current)
			|| current != id) continue;
		if ((!next && index == 0) || (next && index + 1 >= count))
			return false;
		return TryGetAccessibilityVirtualChildAt(
			0, next ? index + 1 : index - 1, result);
	}
	return false;
}

bool ChartView::TryGetAccessibilityVirtualNode(
	uint32_t id, AccessibilityVirtualNode& result)
{
	int seriesIndex = -1;
	int pointIndex = -1;
	if (!FindAccessibilityPoint(id, seriesIndex, pointIndex)
		|| seriesIndex >= static_cast<int>(_series.size())
		|| pointIndex >= static_cast<int>(_series[seriesIndex].Points.size())
		|| !_series[seriesIndex].Visible)
		return false;

	RebuildHitRegions();
	D2D1_RECT_F bounds{};
	bool visible = false;
	for (const auto& region : _hitRegions)
	{
		if (region.SeriesIndex != seriesIndex
			|| region.PointIndex != pointIndex) continue;
		if (region.IsPie)
			bounds = D2D1::RectF(
				region.Center.x - region.Radius,
				region.Center.y - region.Radius,
				region.Center.x + region.Radius,
				region.Center.y + region.Radius);
		else
			bounds = region.Rect;
		visible = true;
		break;
	}
	const auto snapshot = GetAccessibilitySnapshot();
	const auto& point = _series[seriesIndex].Points[pointIndex];
	const std::wstring prefix = snapshot.AutomationId.empty()
		? L"chart" : snapshot.AutomationId;
	result = {};
	result.Id = id;
	result.ControlType = AutomationControlType::DataItem;
	result.Patterns = AutomationPattern::Invoke
		| AutomationPattern::SelectionItem;
	result.Name = GetPointText(seriesIndex, pointIndex);
	result.Description = _series[seriesIndex].Name;
	result.Value = FormatValue(point.Value);
	result.AutomationId =
		prefix + L".point-" + std::to_wstring(id);
	result.BoundsDip = bounds;
	result.Enabled = IsEffectivelyEnabled();
	result.Visible = IsVisible && visible;
	result.Selected = SelectedSeriesIndex == seriesIndex
		&& SelectedPointIndex == pointIndex;
	result.Row = pointIndex;
	result.Column = seriesIndex;
	return true;
}

bool ChartView::TryHitTestAccessibilityVirtualNode(
	float localX, float localY, uint32_t& result)
{
	result = 0;
	int seriesIndex = -1;
	int pointIndex = -1;
	RebuildHitRegions();
	if (!HitTestInternal(
		static_cast<int>(localX), static_cast<int>(localY),
		seriesIndex, pointIndex))
		return false;
	result = GetAccessibilityId(seriesIndex, pointIndex);
	return result != 0;
}

AccessibilityVirtualContainerInfo
ChartView::GetAccessibilityVirtualContainerInfo() const noexcept
{
	AccessibilityVirtualContainerInfo result;
	result.Patterns = AutomationPattern::Selection;
	result.CanSelectMultiple = false;
	result.IsSelectionRequired = false;
	result.RowCount = GetPointCount();
	result.ColumnCount = GetVisibleSeriesCount();
	return result;
}

void ChartView::GetAccessibilityVirtualSelection(
	std::vector<uint32_t>& result) const
{
	result.clear();
	const uint32_t id =
		GetAccessibilityId(SelectedSeriesIndex, SelectedPointIndex);
	if (id != 0) result.push_back(id);
}

bool ChartView::InvokeAccessibilityVirtualNode(uint32_t id)
{
	if (!IsEffectivelyEnabled()) return false;
	int seriesIndex = -1;
	int pointIndex = -1;
	if (!FindAccessibilityPoint(id, seriesIndex, pointIndex)
		|| seriesIndex >= static_cast<int>(_series.size())
		|| !_series[seriesIndex].Visible
		|| pointIndex >= static_cast<int>(_series[seriesIndex].Points.size())
		|| !SelectPoint(seriesIndex, pointIndex))
		return false;
	cui::framework::EventAccess::Raise(
		OnPointClick, this, seriesIndex, pointIndex);
	NotifyAccessibilityVirtualChanged(id, AccessibilityChange::Invoke);
	return true;
}

bool ChartView::SelectAccessibilityVirtualNode(
	uint32_t id, AccessibilitySelectionAction action)
{
	if (action == AccessibilitySelectionAction::Remove
		|| !IsEffectivelyEnabled())
		return false;
	int seriesIndex = -1;
	int pointIndex = -1;
	if (!FindAccessibilityPoint(id, seriesIndex, pointIndex)
		|| seriesIndex >= static_cast<int>(_series.size())
		|| !_series[seriesIndex].Visible)
		return false;
	return SelectPoint(seriesIndex, pointIndex);
}

void ChartView::OnRender()
{
	if (!IsVisible) return;
	auto d2d = GetDrawingContext();
	if (!d2d) return;
	const auto size = GetActualSizeDip();
	const float width = size.width;
	const float height = size.height;
	ClampViewport();
	RebuildHitRegions();

	BeginRender();
	DrawFrame(d2d, width, height);

	auto content = GetContentRect(width, height);
	if (GetPointCount() <= 0 || GetVisibleSeriesCount() <= 0)
	{
		d2d->DrawStringCentered(L"No data", width * 0.5f, height * 0.5f, LegendTextColor, GetRenderFont());
	}
	else
	{
		switch (ChartKind)
		{
		case ChartViewKind::Pie:
			DrawPieChart(d2d, content);
			break;
		case ChartViewKind::Line:
			DrawLineChart(d2d, GetPlotRect(width, height));
			break;
		case ChartViewKind::Bar:
		default:
			DrawBarChart(d2d, GetPlotRect(width, height));
			break;
		}
		DrawLegend(d2d, content);
		DrawHorizontalScrollBar(d2d, width, height);
		DrawKeyboardFocus(d2d);
		DrawTooltip(d2d, width, height);
	}
	EndRender();
}

bool ChartView::ProcessInput(const InputReport& input)
{
	if (!IsEnabled || !IsVisible) return true;
	_lastMouseX = input.X;
	_lastMouseY = input.Y;

	switch (input.Kind)
	{
	case InputReportKind::PointerMove:
	{
		if (_scrolling)
		{
			const auto size = GetActualSizeDip();
			UpdateHorizontalScrollDrag((float)input.X, size.width, size.height);
		}
		UpdateHover(input.X, input.Y);
		auto eventObj = input.CreateMouseEventArgs();
		OnMouseMove(this, eventObj);
		break;
	}
	case InputReportKind::MouseWheel:
	{
		int delta = input.WheelDelta;
		if (CanHandleMouseWheel(delta, input.X, input.Y))
		{
			const auto size = GetActualSizeDip();
			auto plot = GetPlotRect(size.width, size.height);
			int count = GetPointCount();
			float oldZoom = (std::max)(1.0f, ZoomX);
			float rel = ((float)input.X - plot.left) / (std::max)(1.0f, RectWidth(plot));
			rel = std::clamp(rel, 0.0f, 1.0f);
			float oldVirtualWidth = RectWidth(plot) * oldZoom;
			float anchorRatio = (PanX + RectWidth(plot) * rel) / (std::max)(1.0f, oldVirtualWidth);
			float factor = delta > 0 ? 1.16f : 1.0f / 1.16f;
			ZoomX = std::clamp(ZoomX * factor, 1.0f, (std::max)(1.0f, (float)count / 2.0f));
			float newVirtualWidth = RectWidth(plot) * (std::max)(1.0f, ZoomX);
			PanX = anchorRatio * newVirtualWidth - RectWidth(plot) * rel;
			ClampViewport();
			cui::framework::EventAccess::Raise(OnViewportChanged, this);
			InvalidateVisual();
		}
		auto eventObj = input.CreateMouseEventArgs();
		OnMouseWheel(this, eventObj);
		break;
	}
	case InputReportKind::PointerDown:
	{
		if (input.ChangedButton != MouseButton::Left)
			return Control::ProcessInput(input);
		if (GetPresentationWindow()) GetPresentationWindow()->SetKeyboardFocus(this, false);
		const auto size = GetActualSizeDip();
		float width = size.width;
		float height = size.height;
		int s = -1;
		int p = -1;
		if (HasHorizontalScrollBar() && PointInRect((float)input.X, (float)input.Y, GetHorizontalScrollTrackRect(width, height)))
		{
			auto thumb = GetHorizontalScrollThumbRect(width, height);
			if (PointInRect((float)input.X, (float)input.Y, thumb))
				_scrollGrabOffsetX = (float)input.X - thumb.left;
			else
				_scrollGrabOffsetX = RectWidth(thumb) * 0.5f;
			_scrolling = true;
			(void)CaptureMouse();
			UpdateHorizontalScrollDrag((float)input.X, width, height);
		}
		else if (HitTestPoint(input.X, input.Y, s, p))
		{
			SelectPoint(s, p);
			cui::framework::EventAccess::Raise(OnPointClick, this, s, p);
		}
		auto eventObj = input.CreateMouseEventArgs();
		OnMouseDown(this, eventObj);
		InvalidateVisual();
		break;
	}
	case InputReportKind::PointerUp:
	{
		if (input.ChangedButton != MouseButton::Left)
			return Control::ProcessInput(input);
		_scrolling = false;
		if (IsMouseCaptured()) (void)ReleaseMouseCapture();
		auto eventObj = input.CreateMouseEventArgs();
		OnMouseUp(this, eventObj);
		InvalidateVisual();
		break;
	}
	case InputReportKind::Cancel:
	case InputReportKind::CaptureLost:
		_scrolling = false;
		if (input.Kind == InputReportKind::Cancel && IsMouseCaptured())
			(void)ReleaseMouseCapture();
		InvalidateVisual();
		return Control::ProcessInput(input);
	case InputReportKind::PointerDoubleClick:
	{
		if (input.ChangedButton != MouseButton::Left)
			return Control::ProcessInput(input);
		ResetView();
		auto eventObj = input.CreateMouseEventArgs();
		OnMouseDoubleClick(this, eventObj);
		break;
	}
	case InputReportKind::KeyDown:
	{
		auto firstVisiblePoint = [this](bool last)
			-> std::pair<int, int>
		{
			if (last)
			{
				for (int series = static_cast<int>(_series.size()) - 1;
					series >= 0; --series)
					if (_series[series].Visible
						&& !_series[series].Points.empty())
						return {
							series,
							static_cast<int>(
								_series[series].Points.size()) - 1 };
			}
			else
			{
				for (int series = 0;
					series < static_cast<int>(_series.size()); ++series)
					if (_series[series].Visible
						&& !_series[series].Points.empty())
						return { series, 0 };
			}
			return { -1, -1 };
		};

		int series = SelectedSeriesIndex;
		int point = SelectedPointIndex;
		if (series < 0 || point < 0
			|| series >= static_cast<int>(_series.size())
			|| !_series[series].Visible
			|| point >= static_cast<int>(_series[series].Points.size()))
		{
			const auto first = firstVisiblePoint(false);
			series = first.first;
			point = first.second;
		}
		if (series < 0 || point < 0)
			return Control::ProcessInput(input);

		bool invoke = false;
		switch (input.Key)
		{
		case Key::Left:
			point = (std::max)(0, point - 1);
			break;
		case Key::Right:
			point = (std::min)(
				static_cast<int>(_series[series].Points.size()) - 1,
				point + 1);
			break;
		case Key::PageUp:
			point = (std::max)(0, point - 10);
			break;
		case Key::PageDown:
			point = (std::min)(
				static_cast<int>(_series[series].Points.size()) - 1,
				point + 10);
			break;
		case Key::Home:
			point = 0;
			break;
		case Key::End:
			point = static_cast<int>(_series[series].Points.size()) - 1;
			break;
		case Key::Up:
		case Key::Down:
		{
			const int direction = input.Key == Key::Up ? -1 : 1;
			for (int next = series + direction;
				next >= 0 && next < static_cast<int>(_series.size());
				next += direction)
			{
				if (!_series[next].Visible
					|| _series[next].Points.empty()) continue;
				series = next;
				point = (std::min)(
					point,
					static_cast<int>(_series[next].Points.size()) - 1);
				break;
			}
			break;
		}
		case Key::Return:
		case Key::Space:
			invoke = true;
			break;
		default:
			return Control::ProcessInput(input);
		}
		(void)SelectPoint(series, point);
		if (invoke)
			cui::framework::EventAccess::Raise(
				OnPointClick, this, series, point);
		auto args = input.CreateKeyEventArgs();
		OnKeyDown(this, args);
		break;
	}
	case InputReportKind::KeyUp:
	{
		if (!HandlesNavigationKey(input.Key))
			return Control::ProcessInput(input);
		auto args = input.CreateKeyEventArgs();
		OnKeyUp(this, args);
		break;
	}
	default:
		return Control::ProcessInput(input);
	}
	return true;
}

void ChartView::DrawFrame(D2DGraphics* d2d, float width, float height)
{
	// The theme owns border thickness, border brush and disabled chrome.  The
	// retained native presenter only realizes the themed background beneath
	// its data drawing so template visuals never need to cover the chart.
	d2d->FillRoundRect(
		0.0f, 0.0f, width, height,
		RendererBackgroundColor, CornerRadius);

	if (!Title.empty())
	{
		d2d->DrawString(Title, 14.0f, 10.0f, RendererForegroundColor, GetRenderFont());
	}
	if (!Subtitle.empty())
	{
		auto titleHeight = Title.empty() ? 8.0f : 30.0f;
		d2d->DrawString(Subtitle, 14.0f, titleHeight, LegendTextColor, GetRenderFont());
	}
}

void ChartView::DrawAxes(D2DGraphics* d2d, const D2D1_RECT_F& plotRect, double minValue, double maxValue)
{
	d2d->FillRoundRect(plotRect, PlotBackColor, 5.0f);
	const int gridCount = 4;
	for (int i = 0; i <= gridCount; ++i)
	{
		float t = (float)i / (float)gridCount;
		float y = plotRect.top + RectHeight(plotRect) * t;
		if (ShowGridLines)
			d2d->DrawLine(plotRect.left, y, plotRect.right, y, GridLineColor, 1.0f);

		double value = maxValue - (maxValue - minValue) * t;
		std::wstring text = FormatValue(value);
		d2d->DrawString(text, 8.0f, y - 9.0f, LegendTextColor, GetRenderFont());
	}
	d2d->DrawLine(plotRect.left, plotRect.top, plotRect.left, plotRect.bottom, AxisColor, 1.0f);
	d2d->DrawLine(plotRect.left, plotRect.bottom, plotRect.right, plotRect.bottom, AxisColor, 1.0f);
}

void ChartView::DrawBarChart(D2DGraphics* d2d, const D2D1_RECT_F& plotRect)
{
	double minValue = 0.0;
	double maxValue = 1.0;
	if (!GetValueRange(minValue, maxValue)) return;
	DrawAxes(d2d, plotRect, minValue, maxValue);

	int pointCount = GetPointCount();
	if (pointCount <= 0) return;
	int start = 0;
	int end = 0;
	GetVisibleIndexRange(start, end);
	int visibleSeriesCount = (std::max)(1, GetVisibleSeriesCount());
	float virtualWidth = GetVirtualPlotWidth(plotRect);
	float categoryWidth = virtualWidth / (float)pointCount;
	float baseline = ValueToY(0.0, plotRect, minValue, maxValue);
	int visibleCount = (std::max)(1, (int)std::ceil(RectWidth(plotRect) / (std::max)(1.0f, categoryWidth)));
	int labelStep = (std::max)(1, (int)std::ceil((double)visibleCount / 10.0));

	d2d->PushDrawRect(plotRect.left, (std::max)(0.0f, plotRect.top - 18.0f), RectWidth(plotRect), RectHeight(plotRect) + 42.0f);
	for (int i = start; i < end; ++i)
	{
		float categoryLeft = plotRect.left + (float)i * categoryWidth - PanX;
		int visibleSeriesIndex = 0;
		for (int s = 0; s < (int)_series.size(); ++s)
		{
			if (!_series[s].Visible) continue;
			if (i >= (int)_series[s].Points.size())
			{
				++visibleSeriesIndex;
				continue;
			}
			auto& point = _series[s].Points[i];
			float slotWidth = categoryWidth * 0.72f / (float)visibleSeriesCount;
			float barWidth = (std::min)(34.0f, (std::max)(3.0f, slotWidth - 3.0f));
			float x = categoryLeft + categoryWidth * 0.14f + slotWidth * (float)visibleSeriesIndex + (slotWidth - barWidth) * 0.5f;
			float y = ValueToY(point.Value, plotRect, minValue, maxValue);
			float top = (std::min)(y, baseline);
			float bottom = (std::max)(y, baseline);
			float barHeight = (std::max)(1.0f, bottom - top);
			D2D1_COLOR_F color = GetSeriesColor(s, i);
			bool hot = HoveredSeriesIndex == s && HoveredPointIndex == i;
			bool selected = SelectedSeriesIndex == s && SelectedPointIndex == i;
			d2d->FillRoundRect(x, top, barWidth, barHeight, color, 4.0f);
			if (hot || selected)
				d2d->FillRoundRect(x - 2.0f, top - 2.0f, barWidth + 4.0f, barHeight + 4.0f, selected ? SelectedColor : HoverColor, 5.0f);
			if (ShowValueLabels && barHeight > 18.0f)
				d2d->DrawStringCentered(FormatValue(point.Value), x + barWidth * 0.5f, top - 10.0f, LegendTextColor, GetRenderFont());

			++visibleSeriesIndex;
		}

		if (i % labelStep == 0)
		{
			std::wstring label;
			for (const auto& series : _series)
			{
				if (series.Visible && i < (int)series.Points.size())
				{
					label = series.Points[i].Label;
					break;
				}
			}
			if (!label.empty())
				d2d->DrawStringCentered(label, categoryLeft + categoryWidth * 0.5f, plotRect.bottom + 16.0f, LegendTextColor, GetRenderFont());
		}
	}
	d2d->PopDrawRect();
}

void ChartView::DrawLineChart(D2DGraphics* d2d, const D2D1_RECT_F& plotRect)
{
	double minValue = 0.0;
	double maxValue = 1.0;
	if (!GetValueRange(minValue, maxValue)) return;
	DrawAxes(d2d, plotRect, minValue, maxValue);

	int pointCount = GetPointCount();
	if (pointCount <= 0) return;
	int start = 0;
	int end = 0;
	GetVisibleIndexRange(start, end);
	float virtualWidth = GetVirtualPlotWidth(plotRect);
	float stepX = virtualWidth / (float)pointCount;
	int visibleCount = (std::max)(1, (int)std::ceil(RectWidth(plotRect) / (std::max)(1.0f, stepX)));
	int labelStep = (std::max)(1, (int)std::ceil((double)visibleCount / 10.0));

	d2d->PushDrawRect(plotRect.left, (std::max)(0.0f, plotRect.top - 18.0f), RectWidth(plotRect), RectHeight(plotRect) + 42.0f);
	for (int s = 0; s < (int)_series.size(); ++s)
	{
		if (!_series[s].Visible) continue;
		D2D1_POINT_2F previous{ 0, 0 };
		bool hasPrevious = false;
		D2D1_COLOR_F color = GetSeriesColor(s);

		for (int i = start; i < end && i < (int)_series[s].Points.size(); ++i)
		{
			float x = plotRect.left + ((float)i + 0.5f) * stepX - PanX;
			float y = ValueToY(_series[s].Points[i].Value, plotRect, minValue, maxValue);
			D2D1_POINT_2F current{ x, y };
			if (hasPrevious)
				d2d->DrawLine(previous, current, color, 2.4f);
			hasPrevious = true;
			previous = current;
		}

		if (ShowMarkers)
		{
			for (int i = start; i < end && i < (int)_series[s].Points.size(); ++i)
			{
				float x = plotRect.left + ((float)i + 0.5f) * stepX - PanX;
				float y = ValueToY(_series[s].Points[i].Value, plotRect, minValue, maxValue);
				bool hot = HoveredSeriesIndex == s && HoveredPointIndex == i;
				bool selected = SelectedSeriesIndex == s && SelectedPointIndex == i;
				float r = selected ? 6.0f : (hot ? 5.5f : 4.0f);
				d2d->FillEllipse(x, y, r, r, GetSeriesColor(s, i));
				d2d->DrawEllipse(x, y, r + 1.0f, r + 1.0f, selected ? SelectedColor : WithAlpha(Colors::White, hot ? 0.72f : 0.40f), 1.2f);
			}
		}
	}

	for (int i = start; i < end; ++i)
	{
		if (i % labelStep != 0) continue;
		std::wstring label;
		for (const auto& series : _series)
		{
			if (series.Visible && i < (int)series.Points.size())
			{
				label = series.Points[i].Label;
				break;
			}
		}
		if (!label.empty())
			d2d->DrawStringCentered(label, plotRect.left + ((float)i + 0.5f) * stepX - PanX, plotRect.bottom + 16.0f, LegendTextColor, GetRenderFont());
	}
	d2d->PopDrawRect();
}

void ChartView::DrawPieChart(D2DGraphics* d2d, const D2D1_RECT_F& contentRect)
{
	int seriesIndex = -1;
	for (int s = 0; s < (int)_series.size(); ++s)
	{
		if (_series[s].Visible)
		{
			seriesIndex = s;
			break;
		}
	}
	if (seriesIndex < 0) return;

	float legendSpace = ShowLegend ? 120.0f : 0.0f;
	D2D1_RECT_F pieRect = contentRect;
	pieRect.top += Title.empty() ? 12.0f : 42.0f;
	if (!Subtitle.empty()) pieRect.top += 18.0f;
	pieRect.right -= legendSpace;
	pieRect.bottom -= 14.0f;
	float radius = (std::min)(RectWidth(pieRect), RectHeight(pieRect)) * 0.42f;
	if (radius <= 4.0f) return;
	D2D1_POINT_2F center{ pieRect.left + RectWidth(pieRect) * 0.5f, pieRect.top + RectHeight(pieRect) * 0.5f };
	double total = 0.0;
	for (const auto& point : _series[seriesIndex].Points)
		total += (std::max)(0.0, point.Value);
	if (total <= 0.0) return;

	float startAngle = 90.0f;
	for (int i = 0; i < (int)_series[seriesIndex].Points.size(); ++i)
	{
		auto& point = _series[seriesIndex].Points[i];
		double value = (std::max)(0.0, point.Value);
		if (value <= 0.0) continue;
		float sweep = (float)(value / total * 360.0);
		float mid = startAngle + sweep * 0.5f;
		bool hot = HoveredSeriesIndex == seriesIndex && HoveredPointIndex == i;
		bool selected = SelectedSeriesIndex == seriesIndex && SelectedPointIndex == i;
		float offset = selected ? 8.0f : (hot ? 5.0f : 0.0f);
		float rad = mid * ChartPi / 180.0f;
		D2D1_POINT_2F drawCenter{
			center.x + std::cos(rad) * offset,
			center.y - std::sin(rad) * offset
		};

		d2d->FillPie(drawCenter, radius * 2.0f, radius * 2.0f, startAngle, sweep, GetSeriesColor(seriesIndex, i));
		if (hot || selected)
			d2d->FillPie(drawCenter, radius * 2.0f, radius * 2.0f, startAngle, sweep, selected ? SelectedColor : HoverColor);

		if (ShowValueLabels && sweep > 18.0f)
		{
			float labelRadius = radius * 0.62f;
			float tx = drawCenter.x + std::cos(rad) * labelRadius;
			float ty = drawCenter.y - std::sin(rad) * labelRadius;
			d2d->DrawStringCentered(FormatValue(point.Value), tx, ty, Colors::White, GetRenderFont());
		}

		startAngle += sweep;
	}
	d2d->FillEllipse(center.x, center.y, radius * 0.48f, radius * 0.48f, RendererBackgroundColor);
	d2d->DrawEllipse(center.x, center.y, radius, radius, AxisColor, 1.0f);
}

void ChartView::DrawLegend(D2DGraphics* d2d, const D2D1_RECT_F& contentRect)
{
	if (!ShowLegend) return;
	float x = contentRect.right - 112.0f;
	float y = contentRect.top + (Title.empty() ? 14.0f : 48.0f);
	if (!Subtitle.empty()) y += 18.0f;
	float maxY = contentRect.bottom - 10.0f;

	if (ChartKind == ChartViewKind::Pie)
	{
		int seriesIndex = -1;
		for (int s = 0; s < (int)_series.size(); ++s)
		{
			if (_series[s].Visible)
			{
				seriesIndex = s;
				break;
			}
		}
		if (seriesIndex < 0) return;
		for (int i = 0; i < (int)_series[seriesIndex].Points.size() && y < maxY; ++i)
		{
			d2d->FillRoundRect(x, y + 4.0f, 10.0f, 10.0f, GetSeriesColor(seriesIndex, i), 2.0f);
			d2d->DrawString(_series[seriesIndex].Points[i].Label, x + 16.0f, y, LegendTextColor, GetRenderFont());
			y += 22.0f;
		}
		return;
	}

	for (int s = 0; s < (int)_series.size() && y < maxY; ++s)
	{
		if (!_series[s].Visible) continue;
		d2d->FillRoundRect(x, y + 4.0f, 10.0f, 10.0f, GetSeriesColor(s), 2.0f);
		d2d->DrawString(_series[s].Name.empty() ? L"Series" : _series[s].Name, x + 16.0f, y, LegendTextColor, GetRenderFont());
		y += 22.0f;
	}
}

void ChartView::DrawTooltip(D2DGraphics* d2d, float width, float height)
{
	if (!ShowTooltip || HoveredSeriesIndex < 0 || HoveredPointIndex < 0)
		return;
	std::wstring text = GetPointText(HoveredSeriesIndex, HoveredPointIndex);
	if (text.empty()) return;
	auto textSize = GetRenderFont()->GetTextSize(text);
	float w = (std::min)(width - 16.0f, textSize.width + 16.0f);
	float h = textSize.height + 10.0f;
	float x = (float)_lastMouseX + 14.0f;
	float y = (float)_lastMouseY + 14.0f;
	if (x + w > width - 6.0f) x = (float)_lastMouseX - w - 14.0f;
	if (y + h > height - 6.0f) y = (float)_lastMouseY - h - 14.0f;
	x = (std::max)(6.0f, x);
	y = (std::max)(6.0f, y);
	d2d->FillRoundRect(x, y, w, h, TooltipBackColor, 5.0f);
	d2d->DrawRoundRect(x, y, w, h, TooltipBorderColor, 1.0f, 5.0f);
	d2d->DrawString(text, x + 8.0f, y + 5.0f, TooltipTextColor, GetRenderFont());
}

void ChartView::DrawHorizontalScrollBar(D2DGraphics* d2d, float width, float height)
{
	if (!HasHorizontalScrollBar())
		return;

	auto track = GetHorizontalScrollTrackRect(width, height);
	auto thumb = GetHorizontalScrollThumbRect(width, height);
	d2d->FillRoundRect(track, ScrollBackColor, ScrollBarSize * 0.5f);
	d2d->FillRoundRect(thumb, ScrollForeColor, ScrollBarSize * 0.5f);
}

void ChartView::RebuildHitRegions()
{
	_hitRegions.clear();
	const auto size = GetActualSizeDip();
	const float width = size.width;
	const float height = size.height;
	if (width <= 0.0f || height <= 0.0f || GetPointCount() <= 0)
		return;

	if (ChartKind == ChartViewKind::Pie)
	{
		auto content = GetContentRect(width, height);
		int seriesIndex = -1;
		for (int s = 0; s < (int)_series.size(); ++s)
		{
			if (_series[s].Visible)
			{
				seriesIndex = s;
				break;
			}
		}
		if (seriesIndex < 0) return;
		float legendSpace = ShowLegend ? 120.0f : 0.0f;
		D2D1_RECT_F pieRect = content;
		pieRect.top += Title.empty() ? 12.0f : 42.0f;
		if (!Subtitle.empty()) pieRect.top += 18.0f;
		pieRect.right -= legendSpace;
		pieRect.bottom -= 14.0f;
		float radius = (std::min)(RectWidth(pieRect), RectHeight(pieRect)) * 0.42f;
		if (radius <= 4.0f) return;
		D2D1_POINT_2F center{ pieRect.left + RectWidth(pieRect) * 0.5f, pieRect.top + RectHeight(pieRect) * 0.5f };
		double total = 0.0;
		for (const auto& point : _series[seriesIndex].Points)
			total += (std::max)(0.0, point.Value);
		if (total <= 0.0) return;
		float startAngle = 90.0f;
		for (int i = 0; i < (int)_series[seriesIndex].Points.size(); ++i)
		{
			double value = (std::max)(0.0, _series[seriesIndex].Points[i].Value);
			if (value <= 0.0) continue;
			float sweep = (float)(value / total * 360.0);
			HitRegion r;
			r.SeriesIndex = seriesIndex;
			r.PointIndex = i;
			r.Center = center;
			r.Radius = radius + 8.0f;
			r.InnerRadius = radius * 0.48f;
			r.StartAngle = startAngle;
			r.SweepAngle = sweep;
			r.IsPie = true;
			_hitRegions.push_back(r);
			startAngle += sweep;
		}
		return;
	}

	auto plot = GetPlotRect(width, height);
	double minValue = 0.0;
	double maxValue = 1.0;
	if (!GetValueRange(minValue, maxValue)) return;
	int start = 0;
	int end = 0;
	GetVisibleIndexRange(start, end);
	int pointCount = GetPointCount();
	if (pointCount <= 0) return;
	float stepX = GetVirtualPlotWidth(plot) / (float)pointCount;

	if (ChartKind == ChartViewKind::Line)
	{
		for (int s = 0; s < (int)_series.size(); ++s)
		{
			if (!_series[s].Visible) continue;
			for (int i = start; i < end && i < (int)_series[s].Points.size(); ++i)
			{
				float x = plot.left + ((float)i + 0.5f) * stepX - PanX;
				float y = ValueToY(_series[s].Points[i].Value, plot, minValue, maxValue);
				HitRegion r;
				r.SeriesIndex = s;
				r.PointIndex = i;
				r.Rect = D2D1::RectF(x - 7.0f, y - 7.0f, x + 7.0f, y + 7.0f);
				r.Center = D2D1::Point2F(x, y);
				r.Radius = 8.0f;
				r.IsCircle = true;
				_hitRegions.push_back(r);
			}
		}
		return;
	}

	int visibleSeriesCount = (std::max)(1, GetVisibleSeriesCount());
	float baseline = ValueToY(0.0, plot, minValue, maxValue);
	for (int i = start; i < end; ++i)
	{
		float categoryLeft = plot.left + (float)i * stepX - PanX;
		int visibleSeriesIndex = 0;
		for (int s = 0; s < (int)_series.size(); ++s)
		{
			if (!_series[s].Visible) continue;
			if (i < (int)_series[s].Points.size())
			{
				float slotWidth = stepX * 0.72f / (float)visibleSeriesCount;
				float barWidth = (std::min)(34.0f, (std::max)(3.0f, slotWidth - 3.0f));
				float x = categoryLeft + stepX * 0.14f + slotWidth * (float)visibleSeriesIndex + (slotWidth - barWidth) * 0.5f;
				float y = ValueToY(_series[s].Points[i].Value, plot, minValue, maxValue);
				float top = (std::min)(y, baseline);
				float bottom = (std::max)(y, baseline);
				HitRegion r;
				r.SeriesIndex = s;
				r.PointIndex = i;
				r.Rect = D2D1::RectF(x - 3.0f, top - 3.0f, x + barWidth + 3.0f, bottom + 3.0f);
				_hitRegions.push_back(r);
			}
			++visibleSeriesIndex;
		}
	}
}

bool ChartView::HitTestInternal(int localX, int localY, int& seriesIndex, int& pointIndex)
{
	float x = (float)localX;
	float y = (float)localY;
	for (auto it = _hitRegions.rbegin(); it != _hitRegions.rend(); ++it)
	{
		const auto& r = *it;
		if (r.IsPie)
		{
			float dx = x - r.Center.x;
			float dy = y - r.Center.y;
			float dist = std::sqrt(dx * dx + dy * dy);
			if (dist > r.Radius || dist < r.InnerRadius) continue;
			float angle = std::atan2(-dy, dx) * 180.0f / ChartPi;
			if (!AngleInSweep(angle, r.StartAngle, r.SweepAngle)) continue;
		}
		else if (r.IsCircle)
		{
			float dx = x - r.Center.x;
			float dy = y - r.Center.y;
			if (std::sqrt(dx * dx + dy * dy) > r.Radius) continue;
		}
		else if (!PointInRect(x, y, r.Rect))
		{
			continue;
		}
		seriesIndex = r.SeriesIndex;
		pointIndex = r.PointIndex;
		return true;
	}
	seriesIndex = -1;
	pointIndex = -1;
	return false;
}

void ChartView::UpdateHover(int localX, int localY)
{
	int s = -1;
	int p = -1;
	RebuildHitRegions();
	HitTestInternal(localX, localY, s, p);
	if (HoveredSeriesIndex == s && HoveredPointIndex == p)
		return;
	HoveredSeriesIndex = s;
	HoveredPointIndex = p;
	if (s >= 0 && p >= 0)
		cui::framework::EventAccess::Raise(OnPointHover, this, s, p);
	InvalidateVisual();
}

void ChartView::ClampViewport()
{
	int count = GetPointCount();
	if (count <= 1)
	{
		ZoomX = 1.0f;
		PanX = 0.0f;
		return;
	}
	ZoomX = std::clamp(ZoomX, 1.0f, (std::max)(1.0f, (float)count / 2.0f));
	const auto size = GetActualSizeDip();
	float maxPan = GetMaxViewScrollX(size.width, size.height);
	PanX = std::clamp(PanX, 0.0f, maxPan);
}

void ChartView::UpdateHorizontalScrollDrag(float localX, float width, float height)
{
	if (!HasHorizontalScrollBar())
	{
		_scrolling = false;
		return;
	}

	auto track = GetHorizontalScrollTrackRect(width, height);
	auto thumb = GetHorizontalScrollThumbRect(width, height);
	float travel = (std::max)(1.0f, RectWidth(track) - RectWidth(thumb));
	float t = (localX - _scrollGrabOffsetX - track.left) / travel;
	t = std::clamp(t, 0.0f, 1.0f);

	float maxPan = GetMaxViewScrollX(width, height);
	float oldPan = PanX;
	PanX = maxPan * t;
	ClampViewport();
	if (std::fabs(oldPan - PanX) > 0.01f)
	{
		cui::framework::EventAccess::Raise(OnViewportChanged, this);
		InvalidateVisual();
	}
}

void ChartView::EnsurePointVisible(int pointIndex)
{
	if (pointIndex < 0 || ChartKind == ChartViewKind::Pie
		|| !EnablePanZoom || ZoomX <= 1.001f)
		return;
	const auto size = GetActualSizeDip();
	const auto plot = GetPlotRect(size.width, size.height);
	const int count = GetPointCount();
	if (count <= 0 || RectWidth(plot) <= 0.0f) return;
	const float step = GetVirtualPlotWidth(plot) / static_cast<float>(count);
	const float left = pointIndex * step;
	const float right = left + step;
	const float viewport = RectWidth(plot);
	const float previous = PanX;
	if (left < PanX)
		PanX = left;
	else if (right > PanX + viewport)
		PanX = right - viewport;
	ClampViewport();
	if (std::fabs(previous - PanX) > 0.01f)
	{
		cui::framework::EventAccess::Raise(OnViewportChanged, this);
		NotifyAccessibilityScrollChanged();
	}
}

void ChartView::DrawKeyboardFocus(D2DGraphics* d2d)
{
	if (!d2d || !IsKeyboardFocused
		|| SelectedSeriesIndex < 0 || SelectedPointIndex < 0)
		return;
	for (const auto& region : _hitRegions)
	{
		if (region.SeriesIndex != SelectedSeriesIndex
			|| region.PointIndex != SelectedPointIndex) continue;
		if (region.IsPie)
		{
			d2d->DrawEllipse(
				region.Center.x, region.Center.y,
				region.Radius + 2.0f, region.Radius + 2.0f,
				RendererForegroundColor, 1.5f);
		}
		else if (region.IsCircle)
		{
			d2d->DrawEllipse(
				region.Center.x, region.Center.y,
				region.Radius + 3.0f, region.Radius + 3.0f,
				RendererForegroundColor, 1.5f);
		}
		else
		{
			d2d->DrawRoundRect(
				region.Rect.left - 2.0f, region.Rect.top - 2.0f,
				RectWidth(region.Rect) + 4.0f,
				RectHeight(region.Rect) + 4.0f,
				RendererForegroundColor, 1.5f, 4.0f);
		}
		return;
	}
}

D2D1_RECT_F ChartView::GetContentRect(float width, float height) const
{
	return D2D1::RectF(8.0f, 8.0f, (std::max)(8.0f, width - 8.0f), (std::max)(8.0f, height - 8.0f));
}

D2D1_RECT_F ChartView::GetPlotRect(float width, float height) const
{
	auto content = GetContentRect(width, height);
	float top = content.top + (_title.empty() ? 16.0f : 48.0f);
	if (!_subtitle.empty()) top += 18.0f;
	float right = content.right - (_showLegend ? 126.0f : 14.0f);
	float bottomReserve = HasHorizontalScrollBar() ? 50.0f : 34.0f;
	D2D1_RECT_F rect{
		content.left + 52.0f,
		top,
		(std::max)(content.left + 90.0f, right),
		(std::max)(top + 36.0f, content.bottom - bottomReserve)
	};
	return rect;
}

D2D1_RECT_F ChartView::GetHorizontalScrollTrackRect(float width, float height) const
{
	auto plot = GetPlotRect(width, height);
	auto content = GetContentRect(width, height);
	float barHeight = (std::max)(4.0f, ScrollBarSize);
	float top = content.bottom - barHeight - 8.0f;
	return D2D1::RectF(plot.left, top, plot.right, top + barHeight);
}

D2D1_RECT_F ChartView::GetHorizontalScrollThumbRect(float width, float height) const
{
	auto track = GetHorizontalScrollTrackRect(width, height);
	int count = GetPointCount();
	if (count <= 1 || ZoomX <= 1.001f)
		return D2D1::RectF(track.left, track.top, track.right, track.bottom);

	float zoom = (std::max)(1.0f, ZoomX);
	float maxPan = GetMaxViewScrollX(width, height);
	float thumbWidth = RectWidth(track) / zoom;
	thumbWidth = std::clamp(thumbWidth, (std::min)(28.0f, RectWidth(track)), RectWidth(track));
	float travel = (std::max)(0.0f, RectWidth(track) - thumbWidth);
	float t = maxPan > 0.001f ? std::clamp(PanX / maxPan, 0.0f, 1.0f) : 0.0f;
	float left = track.left + travel * t;
	return D2D1::RectF(left, track.top, left + thumbWidth, track.bottom);
}

D2D1_COLOR_F ChartView::GetSeriesColor(int seriesIndex, int pointIndex) const
{
	if (seriesIndex >= 0 && seriesIndex < (int)_series.size())
	{
		if (pointIndex >= 0 && pointIndex < (int)_series[seriesIndex].Points.size())
		{
			const auto& point = _series[seriesIndex].Points[pointIndex];
			if (point.UseCustomColor) return point.Color;
		}
		if (!IsTransparent(_series[seriesIndex].Color)) return _series[seriesIndex].Color;
	}
	return PaletteColor(pointIndex >= 0 && _chartKind == ChartViewKind::Pie
		? pointIndex : seriesIndex);
}

std::wstring ChartView::GetPointText(int seriesIndex, int pointIndex) const
{
	if (seriesIndex < 0 || pointIndex < 0 || seriesIndex >= (int)_series.size())
		return L"";
	const auto& series = _series[seriesIndex];
	if (pointIndex >= (int)series.Points.size())
		return L"";
	const auto& point = series.Points[pointIndex];
	std::wstring text;
	if (!series.Name.empty() && _chartKind != ChartViewKind::Pie)
	{
		text += series.Name;
		text += L" - ";
	}
	text += point.Label.empty() ? L"Point" : point.Label;
	text += L": ";
	text += FormatValue(point.Value);
	return text;
}

std::wstring ChartView::FormatValue(double value) const
{
	std::wstringstream ss;
	if (_valuePrecision >= 0)
		ss << std::fixed << std::setprecision(_valuePrecision);
	ss << value;
	return ss.str();
}

bool ChartView::HasHorizontalScrollBar() const
{
	return _enablePanZoom && _chartKind != ChartViewKind::Pie
		&& ZoomX > 1.001f && GetPointCount() > 1;
}

float ChartView::GetVirtualPlotWidth(const D2D1_RECT_F& plotRect) const
{
	return RectWidth(plotRect) * (std::max)(1.0f, ZoomX);
}

float ChartView::GetMaxViewScrollX(float width, float height) const
{
	if (!HasHorizontalScrollBar())
		return 0.0f;
	auto plot = GetPlotRect(width, height);
	return (std::max)(0.0f, GetVirtualPlotWidth(plot) - RectWidth(plot));
}

void ChartView::GetVisibleIndexRange(int& start, int& end)
{
	int count = GetPointCount();
	if (count <= 0)
	{
		start = 0;
		end = 0;
		return;
	}
	const auto size = GetActualSizeDip();
	auto plot = GetPlotRect(size.width, size.height);
	float categoryWidth = GetVirtualPlotWidth(plot) / (float)count;
	if (categoryWidth <= 0.001f)
	{
		start = 0;
		end = count;
		return;
	}
	start = (int)std::floor(PanX / categoryWidth) - 1;
	end = (int)std::ceil((PanX + RectWidth(plot)) / categoryWidth) + 1;
	start = std::clamp(start, 0, count);
	end = std::clamp(end, start, count);
}

int ChartView::GetPointCount() const
{
	int count = 0;
	for (const auto& series : _series)
	{
		if (!series.Visible) continue;
		count = (std::max)(count, (int)series.Points.size());
	}
	return count;
}

int ChartView::GetVisibleSeriesCount() const
{
	int count = 0;
	for (const auto& series : _series)
	{
		if (series.Visible) ++count;
	}
	return count;
}

bool ChartView::GetValueRange(double& minValue, double& maxValue) const
{
	bool hasValue = false;
	for (const auto& series : _series)
	{
		if (!series.Visible) continue;
		for (const auto& point : series.Points)
		{
			if (!std::isfinite(point.Value)) continue;
			if (!hasValue)
			{
				minValue = maxValue = point.Value;
				hasValue = true;
			}
			else
			{
				minValue = (std::min)(minValue, point.Value);
				maxValue = (std::max)(maxValue, point.Value);
			}
		}
	}
	if (!hasValue) return false;
	if (_chartKind != ChartViewKind::Line)
	{
		minValue = (std::min)(minValue, 0.0);
		maxValue = (std::max)(maxValue, 0.0);
	}
	if (std::fabs(maxValue - minValue) < 0.000001)
	{
		minValue -= 1.0;
		maxValue += 1.0;
	}
	double padding = (maxValue - minValue) * 0.08;
	minValue -= padding;
	maxValue += padding;
	return true;
}

float ChartView::ValueToY(double value, const D2D1_RECT_F& plotRect, double minValue, double maxValue) const
{
	double den = maxValue - minValue;
	if (std::fabs(den) < 0.000001) return plotRect.bottom;
	double t = (value - minValue) / den;
	t = std::clamp(t, 0.0, 1.0);
	return plotRect.bottom - (float)t * RectHeight(plotRect);
}
