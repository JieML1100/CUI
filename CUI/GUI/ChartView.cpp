#include "ChartView.h"
#include "Form.h"
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

ChartView::ChartView(int x, int y, int width, int height)
{
	this->Location = POINT{ x, y };
	this->Size = SIZE{ width, height };
	this->BackColor = D2D1_COLOR_F{ 0, 0, 0, 0 };
	this->BolderColor = D2D1_COLOR_F{ 0.60f, 0.66f, 0.76f, 0.52f };
	this->ForeColor = D2D1_COLOR_F{ 0.90f, 0.92f, 0.96f, 1.0f };
	this->Cursor = CursorKind::Arrow;
}

void ChartView::Clear()
{
	Series.clear();
	HoveredSeriesIndex = -1;
	HoveredPointIndex = -1;
	SelectedSeriesIndex = -1;
	SelectedPointIndex = -1;
	ResetView();
}

int ChartView::AddSeries(const ChartSeries& series)
{
	Series.push_back(series);
	ClampViewport();
	PostRender();
	return static_cast<int>(Series.size()) - 1;
}

void ChartView::SetSingleSeries(const std::vector<ChartPoint>& points, const std::wstring& name)
{
	Series.clear();
	ChartSeries series(name, AccentColor);
	series.Points = points;
	Series.push_back(series);
	ResetView();
}

void ChartView::ResetView()
{
	_scrolling = false;
	ZoomX = 1.0f;
	PanX = 0.0f;
	ClampViewport();
	PostRender();
	OnViewportChanged(this);
}

bool ChartView::SelectPoint(int seriesIndex, int pointIndex)
{
	if (seriesIndex < 0 || pointIndex < 0 || seriesIndex >= static_cast<int>(Series.size()))
		return false;
	if (pointIndex >= static_cast<int>(Series[seriesIndex].Points.size()))
		return false;

	if (SelectedSeriesIndex == seriesIndex && SelectedPointIndex == pointIndex)
		return true;

	SelectedSeriesIndex = seriesIndex;
	SelectedPointIndex = pointIndex;
	SelectionChanged(this);
	PostRender();
	return true;
}

bool ChartView::HitTestPoint(int xof, int yof, int& seriesIndex, int& pointIndex)
{
	RebuildHitRegions();
	return HitTestInternal(xof, yof, seriesIndex, pointIndex);
}

CursorKind ChartView::QueryCursor(int xof, int yof)
{
	if (!Enable) return CursorKind::Arrow;
	if (_scrolling) return CursorKind::SizeWE;

	int s = -1;
	int p = -1;
	if (HitTestPoint(xof, yof, s, p))
		return CursorKind::Hand;
	if (HasHorizontalScrollBar())
	{
		auto size = ActualSize();
		if (PointInRect((float)xof, (float)yof, GetHorizontalScrollTrackRect((float)size.cx, (float)size.cy)))
			return CursorKind::SizeWE;
	}
	return Cursor;
}

bool ChartView::CanHandleMouseWheel(int delta, int xof, int yof)
{
	(void)delta;
	if (!EnablePanZoom || ChartKind == ChartViewKind::Pie || GetPointCount() <= 1)
		return false;
	auto size = ActualSize();
	return PointInRect((float)xof, (float)yof, GetPlotRect((float)size.cx, (float)size.cy));
}

void ChartView::Update()
{
	if (!IsVisual) return;
	auto d2d = ParentForm->Render;
	auto size = ActualSize();
	const float width = (float)size.cx;
	const float height = (float)size.cy;
	ClampViewport();
	RebuildHitRegions();

	BeginRender();
	DrawFrame(d2d, width, height);

	auto content = GetContentRect(width, height);
	if (GetPointCount() <= 0 || GetVisibleSeriesCount() <= 0)
	{
		d2d->DrawStringCentered(L"No data", width * 0.5f, height * 0.5f, LegendTextColor, Font);
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
		DrawTooltip(d2d, width, height);
	}

	if (!Enable)
		d2d->FillRoundRect(0, 0, width, height, D2D1_COLOR_F{ 1,1,1,0.45f }, CornerRadius);
	EndRender();
}

bool ChartView::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof)
{
	if (!Enable || !Visible) return true;
	(void)lParam;
	_lastMouseX = xof;
	_lastMouseY = yof;

	switch (message)
	{
	case WM_MOUSEMOVE:
	{
		if (ParentForm) ParentForm->UnderMouse = this;
		if (_scrolling)
		{
			auto size = ActualSize();
			UpdateHorizontalScrollDrag((float)xof, (float)size.cx, (float)size.cy);
		}
		UpdateHover(xof, yof);
		MouseEventArgs eventObj(MouseButtons::None, 0, xof, yof, HIWORD(wParam));
		OnMouseMove(this, eventObj);
		break;
	}
	case WM_MOUSEWHEEL:
	{
		int delta = GET_WHEEL_DELTA_WPARAM(wParam);
		if (CanHandleMouseWheel(delta, xof, yof))
		{
			auto size = ActualSize();
			auto plot = GetPlotRect((float)size.cx, (float)size.cy);
			int count = GetPointCount();
			float oldZoom = (std::max)(1.0f, ZoomX);
			float rel = ((float)xof - plot.left) / (std::max)(1.0f, RectWidth(plot));
			rel = std::clamp(rel, 0.0f, 1.0f);
			float oldVirtualWidth = RectWidth(plot) * oldZoom;
			float anchorRatio = (PanX + RectWidth(plot) * rel) / (std::max)(1.0f, oldVirtualWidth);
			float factor = delta > 0 ? 1.16f : 1.0f / 1.16f;
			ZoomX = std::clamp(ZoomX * factor, 1.0f, (std::max)(1.0f, (float)count / 2.0f));
			float newVirtualWidth = RectWidth(plot) * (std::max)(1.0f, ZoomX);
			PanX = anchorRatio * newVirtualWidth - RectWidth(plot) * rel;
			ClampViewport();
			OnViewportChanged(this);
			PostRender();
		}
		MouseEventArgs eventObj(MouseButtons::None, 0, xof, yof, delta);
		OnMouseWheel(this, eventObj);
		break;
	}
	case WM_LBUTTONDOWN:
	{
		if (ParentForm) ParentForm->SetSelectedControl(this, false);
		auto size = ActualSize();
		float width = (float)size.cx;
		float height = (float)size.cy;
		int s = -1;
		int p = -1;
		if (HasHorizontalScrollBar() && PointInRect((float)xof, (float)yof, GetHorizontalScrollTrackRect(width, height)))
		{
			auto thumb = GetHorizontalScrollThumbRect(width, height);
			if (PointInRect((float)xof, (float)yof, thumb))
				_scrollGrabOffsetX = (float)xof - thumb.left;
			else
				_scrollGrabOffsetX = RectWidth(thumb) * 0.5f;
			_scrolling = true;
			UpdateHorizontalScrollDrag((float)xof, width, height);
		}
		else if (HitTestPoint(xof, yof, s, p))
		{
			SelectPoint(s, p);
			OnPointClick(this, s, p);
		}
		MouseEventArgs eventObj(MouseButtons::Left, 0, xof, yof, HIWORD(wParam));
		OnMouseDown(this, eventObj);
		PostRender();
		break;
	}
	case WM_LBUTTONUP:
	{
		_scrolling = false;
		MouseEventArgs eventObj(MouseButtons::Left, 0, xof, yof, HIWORD(wParam));
		OnMouseUp(this, eventObj);
		if (ParentForm && ParentForm->Selected == this)
			ParentForm->SetSelectedControl(NULL, false);
		PostRender();
		break;
	}
	case WM_LBUTTONDBLCLK:
	{
		ResetView();
		MouseEventArgs eventObj(MouseButtons::Left, 2, xof, yof, HIWORD(wParam));
		OnMouseDoubleClick(this, eventObj);
		break;
	}
	default:
		return Control::ProcessMessage(message, wParam, lParam, xof, yof);
	}
	return true;
}

void ChartView::DrawFrame(D2DGraphics* d2d, float width, float height)
{
	d2d->FillRoundRect(Border * 0.5f, Border * 0.5f, width - Border, height - Border, BackColor, CornerRadius);
	d2d->DrawRoundRect(Border * 0.5f, Border * 0.5f, width - Border, height - Border, BolderColor, Border, CornerRadius);

	if (!Title.empty())
	{
		d2d->DrawString(Title, 14.0f, 10.0f, ForeColor, Font);
	}
	if (!Subtitle.empty())
	{
		auto titleHeight = Title.empty() ? 8.0f : 30.0f;
		d2d->DrawString(Subtitle, 14.0f, titleHeight, LegendTextColor, Font);
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
		d2d->DrawString(text, 8.0f, y - 9.0f, LegendTextColor, Font);
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
		for (int s = 0; s < (int)Series.size(); ++s)
		{
			if (!Series[s].Visible) continue;
			if (i >= (int)Series[s].Points.size())
			{
				++visibleSeriesIndex;
				continue;
			}
			auto& point = Series[s].Points[i];
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
				d2d->DrawStringCentered(FormatValue(point.Value), x + barWidth * 0.5f, top - 10.0f, LegendTextColor, Font);

			++visibleSeriesIndex;
		}

		if (i % labelStep == 0)
		{
			std::wstring label;
			for (const auto& series : Series)
			{
				if (series.Visible && i < (int)series.Points.size())
				{
					label = series.Points[i].Label;
					break;
				}
			}
			if (!label.empty())
				d2d->DrawStringCentered(label, categoryLeft + categoryWidth * 0.5f, plotRect.bottom + 16.0f, LegendTextColor, Font);
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
	for (int s = 0; s < (int)Series.size(); ++s)
	{
		if (!Series[s].Visible) continue;
		D2D1_POINT_2F previous{ 0, 0 };
		bool hasPrevious = false;
		D2D1_COLOR_F color = GetSeriesColor(s);

		for (int i = start; i < end && i < (int)Series[s].Points.size(); ++i)
		{
			float x = plotRect.left + ((float)i + 0.5f) * stepX - PanX;
			float y = ValueToY(Series[s].Points[i].Value, plotRect, minValue, maxValue);
			D2D1_POINT_2F current{ x, y };
			if (hasPrevious)
				d2d->DrawLine(previous, current, color, 2.4f);
			hasPrevious = true;
			previous = current;
		}

		if (ShowMarkers)
		{
			for (int i = start; i < end && i < (int)Series[s].Points.size(); ++i)
			{
				float x = plotRect.left + ((float)i + 0.5f) * stepX - PanX;
				float y = ValueToY(Series[s].Points[i].Value, plotRect, minValue, maxValue);
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
		for (const auto& series : Series)
		{
			if (series.Visible && i < (int)series.Points.size())
			{
				label = series.Points[i].Label;
				break;
			}
		}
		if (!label.empty())
			d2d->DrawStringCentered(label, plotRect.left + ((float)i + 0.5f) * stepX - PanX, plotRect.bottom + 16.0f, LegendTextColor, Font);
	}
	d2d->PopDrawRect();
}

void ChartView::DrawPieChart(D2DGraphics* d2d, const D2D1_RECT_F& contentRect)
{
	int seriesIndex = -1;
	for (int s = 0; s < (int)Series.size(); ++s)
	{
		if (Series[s].Visible)
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
	for (const auto& point : Series[seriesIndex].Points)
		total += (std::max)(0.0, point.Value);
	if (total <= 0.0) return;

	float startAngle = 90.0f;
	for (int i = 0; i < (int)Series[seriesIndex].Points.size(); ++i)
	{
		auto& point = Series[seriesIndex].Points[i];
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
			d2d->DrawStringCentered(FormatValue(point.Value), tx, ty, Colors::White, Font);
		}

		startAngle += sweep;
	}
	d2d->FillEllipse(center.x, center.y, radius * 0.48f, radius * 0.48f, BackColor);
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
		for (int s = 0; s < (int)Series.size(); ++s)
		{
			if (Series[s].Visible)
			{
				seriesIndex = s;
				break;
			}
		}
		if (seriesIndex < 0) return;
		for (int i = 0; i < (int)Series[seriesIndex].Points.size() && y < maxY; ++i)
		{
			d2d->FillRoundRect(x, y + 4.0f, 10.0f, 10.0f, GetSeriesColor(seriesIndex, i), 2.0f);
			d2d->DrawString(Series[seriesIndex].Points[i].Label, x + 16.0f, y, LegendTextColor, Font);
			y += 22.0f;
		}
		return;
	}

	for (int s = 0; s < (int)Series.size() && y < maxY; ++s)
	{
		if (!Series[s].Visible) continue;
		d2d->FillRoundRect(x, y + 4.0f, 10.0f, 10.0f, GetSeriesColor(s), 2.0f);
		d2d->DrawString(Series[s].Name.empty() ? L"Series" : Series[s].Name, x + 16.0f, y, LegendTextColor, Font);
		y += 22.0f;
	}
}

void ChartView::DrawTooltip(D2DGraphics* d2d, float width, float height)
{
	if (!ShowTooltip || HoveredSeriesIndex < 0 || HoveredPointIndex < 0)
		return;
	std::wstring text = GetPointText(HoveredSeriesIndex, HoveredPointIndex);
	if (text.empty()) return;
	auto textSize = Font->GetTextSize(text);
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
	d2d->DrawString(text, x + 8.0f, y + 5.0f, TooltipTextColor, Font);
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
	auto size = ActualSize();
	const float width = (float)size.cx;
	const float height = (float)size.cy;
	if (width <= 0.0f || height <= 0.0f || GetPointCount() <= 0)
		return;

	if (ChartKind == ChartViewKind::Pie)
	{
		auto content = GetContentRect(width, height);
		int seriesIndex = -1;
		for (int s = 0; s < (int)Series.size(); ++s)
		{
			if (Series[s].Visible)
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
		for (const auto& point : Series[seriesIndex].Points)
			total += (std::max)(0.0, point.Value);
		if (total <= 0.0) return;
		float startAngle = 90.0f;
		for (int i = 0; i < (int)Series[seriesIndex].Points.size(); ++i)
		{
			double value = (std::max)(0.0, Series[seriesIndex].Points[i].Value);
			if (value <= 0.0) continue;
			float sweep = (float)(value / total * 360.0);
			HitRegion r;
			r.SeriesIndex = seriesIndex;
			r.PointIndex = i;
			r.Center = center;
			r.Radius = radius + 8.0f;
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
		for (int s = 0; s < (int)Series.size(); ++s)
		{
			if (!Series[s].Visible) continue;
			for (int i = start; i < end && i < (int)Series[s].Points.size(); ++i)
			{
				float x = plot.left + ((float)i + 0.5f) * stepX - PanX;
				float y = ValueToY(Series[s].Points[i].Value, plot, minValue, maxValue);
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
		for (int s = 0; s < (int)Series.size(); ++s)
		{
			if (!Series[s].Visible) continue;
			if (i < (int)Series[s].Points.size())
			{
				float slotWidth = stepX * 0.72f / (float)visibleSeriesCount;
				float barWidth = (std::min)(34.0f, (std::max)(3.0f, slotWidth - 3.0f));
				float x = categoryLeft + stepX * 0.14f + slotWidth * (float)visibleSeriesIndex + (slotWidth - barWidth) * 0.5f;
				float y = ValueToY(Series[s].Points[i].Value, plot, minValue, maxValue);
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

bool ChartView::HitTestInternal(int xof, int yof, int& seriesIndex, int& pointIndex)
{
	float x = (float)xof;
	float y = (float)yof;
	for (auto it = _hitRegions.rbegin(); it != _hitRegions.rend(); ++it)
	{
		const auto& r = *it;
		if (r.IsPie)
		{
			float dx = x - r.Center.x;
			float dy = y - r.Center.y;
			float dist = std::sqrt(dx * dx + dy * dy);
			if (dist > r.Radius) continue;
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

void ChartView::UpdateHover(int xof, int yof)
{
	int s = -1;
	int p = -1;
	RebuildHitRegions();
	HitTestInternal(xof, yof, s, p);
	if (HoveredSeriesIndex == s && HoveredPointIndex == p)
		return;
	HoveredSeriesIndex = s;
	HoveredPointIndex = p;
	if (s >= 0 && p >= 0)
		OnPointHover(this, s, p);
	PostRender();
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
	auto size = ActualSize();
	float maxPan = GetMaxViewScrollX((float)size.cx, (float)size.cy);
	PanX = std::clamp(PanX, 0.0f, maxPan);
}

void ChartView::UpdateHorizontalScrollDrag(float xof, float width, float height)
{
	if (!HasHorizontalScrollBar())
	{
		_scrolling = false;
		return;
	}

	auto track = GetHorizontalScrollTrackRect(width, height);
	auto thumb = GetHorizontalScrollThumbRect(width, height);
	float travel = (std::max)(1.0f, RectWidth(track) - RectWidth(thumb));
	float t = (xof - _scrollGrabOffsetX - track.left) / travel;
	t = std::clamp(t, 0.0f, 1.0f);

	float maxPan = GetMaxViewScrollX(width, height);
	float oldPan = PanX;
	PanX = maxPan * t;
	ClampViewport();
	if (std::fabs(oldPan - PanX) > 0.01f)
	{
		OnViewportChanged(this);
		PostRender();
	}
}

D2D1_RECT_F ChartView::GetContentRect(float width, float height) const
{
	return D2D1::RectF(8.0f, 8.0f, (std::max)(8.0f, width - 8.0f), (std::max)(8.0f, height - 8.0f));
}

D2D1_RECT_F ChartView::GetPlotRect(float width, float height) const
{
	auto content = GetContentRect(width, height);
	float top = content.top + (Title.empty() ? 16.0f : 48.0f);
	if (!Subtitle.empty()) top += 18.0f;
	float right = content.right - (ShowLegend ? 126.0f : 14.0f);
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
	if (seriesIndex >= 0 && seriesIndex < (int)Series.size())
	{
		if (pointIndex >= 0 && pointIndex < (int)Series[seriesIndex].Points.size())
		{
			const auto& point = Series[seriesIndex].Points[pointIndex];
			if (point.UseCustomColor) return point.Color;
		}
		if (!IsTransparent(Series[seriesIndex].Color)) return Series[seriesIndex].Color;
	}
	return PaletteColor(pointIndex >= 0 && ChartKind == ChartViewKind::Pie ? pointIndex : seriesIndex);
}

std::wstring ChartView::GetPointText(int seriesIndex, int pointIndex) const
{
	if (seriesIndex < 0 || pointIndex < 0 || seriesIndex >= (int)Series.size())
		return L"";
	const auto& series = Series[seriesIndex];
	if (pointIndex >= (int)series.Points.size())
		return L"";
	const auto& point = series.Points[pointIndex];
	std::wstring text;
	if (!series.Name.empty() && ChartKind != ChartViewKind::Pie)
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
	if (ValuePrecision >= 0)
		ss << std::fixed << std::setprecision(ValuePrecision);
	ss << value;
	return ss.str();
}

bool ChartView::HasHorizontalScrollBar() const
{
	return EnablePanZoom && ChartKind != ChartViewKind::Pie && ZoomX > 1.001f && GetPointCount() > 1;
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
	auto size = ActualSize();
	auto plot = GetPlotRect((float)size.cx, (float)size.cy);
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
	for (const auto& series : Series)
	{
		if (!series.Visible) continue;
		count = (std::max)(count, (int)series.Points.size());
	}
	return count;
}

int ChartView::GetVisibleSeriesCount() const
{
	int count = 0;
	for (const auto& series : Series)
	{
		if (series.Visible) ++count;
	}
	return count;
}

bool ChartView::GetValueRange(double& minValue, double& maxValue) const
{
	bool hasValue = false;
	for (const auto& series : Series)
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
	if (ChartKind != ChartViewKind::Line)
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
