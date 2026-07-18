#pragma once
#include "Control.h"
#include <utility>
#include <vector>

enum class ChartViewKind
{
	Bar,
	Pie,
	Line
};

struct ChartPoint
{
	std::wstring Label;
	double Value = 0.0;
	UINT64 Tag = 0;
	D2D1_COLOR_F Color = D2D1_COLOR_F{ 0, 0, 0, 0 };
	bool UseCustomColor = false;

	ChartPoint() = default;
	ChartPoint(std::wstring label, double value);
	ChartPoint(std::wstring label, double value, D2D1_COLOR_F color);
};

struct ChartSeries
{
	std::wstring Name;
	std::vector<ChartPoint> Points;
	D2D1_COLOR_F Color = D2D1_COLOR_F{ 0, 0, 0, 0 };
	bool Visible = true;

	ChartSeries() = default;
	ChartSeries(std::wstring name, D2D1_COLOR_F color);
};

typedef Event<void(class ChartView*, int seriesIndex, int pointIndex)> ChartPointEvent;
typedef Event<void(class ChartView*)> ChartViewportChangedEvent;

class ChartView : public Control
{
public:
	UIClass Type() override;
	void EnsureBindingPropertiesRegistered() override;
	ChartView(int x = 0, int y = 0, int width = 360, int height = 240);

	std::vector<ChartSeries> Series;
	ChartViewKind ChartKind = ChartViewKind::Bar;

	std::wstring Title = L"Chart";
	std::wstring Subtitle = L"";
	int ValuePrecision = 0;

	bool ShowLegend = true;
	bool ShowTooltip = true;
	bool ShowValueLabels = false;
	bool ShowGridLines = true;
	bool ShowMarkers = true;
	bool EnablePanZoom = true;

	float Border = 1.0f;
	float CornerRadius = 8.0f;
	float ZoomX = 1.0f;
	float PanX = 0.0f;

	int HoveredSeriesIndex = -1;
	int HoveredPointIndex = -1;
	int SelectedSeriesIndex = -1;
	int SelectedPointIndex = -1;

	D2D1_COLOR_F PlotBackColor = cui::theme::palette::SurfaceSubtle;
	D2D1_COLOR_F GridLineColor = cui::theme::palette::Border;
	D2D1_COLOR_F AxisColor = cui::theme::palette::BorderStrong;
	D2D1_COLOR_F AccentColor = cui::theme::palette::Accent;
	D2D1_COLOR_F HoverColor = cui::theme::palette::AccentSoft;
	D2D1_COLOR_F SelectedColor = cui::theme::palette::AccentSelected;
	D2D1_COLOR_F TooltipBackColor = cui::theme::palette::TooltipSurface;
	D2D1_COLOR_F TooltipBorderColor = cui::theme::palette::BorderStrong;
	D2D1_COLOR_F TooltipTextColor = cui::theme::palette::OnAccent;
	D2D1_COLOR_F LegendTextColor = cui::theme::palette::TextSecondary;
	D2D1_COLOR_F ScrollBackColor = cui::theme::palette::ScrollTrack;
	D2D1_COLOR_F ScrollForeColor = cui::theme::palette::ScrollThumb;
	float ScrollBarSize = 8.0f;

	ChartPointEvent OnPointClick;
	ChartPointEvent OnPointHover;
	SelectionChangedEvent SelectionChanged;
	ChartViewportChangedEvent OnViewportChanged;

	void Clear();
	int AddSeries(const ChartSeries& series);
	void SetSingleSeries(const std::vector<ChartPoint>& points, const std::wstring& name = L"Series");
	void ResetView();
	bool SelectPoint(int seriesIndex, int pointIndex);
	bool HitTestPoint(int localX, int localY, int& seriesIndex, int& pointIndex);

	CursorKind QueryCursor(int localX, int localY) override;
	bool HandlesMouseWheel() const override { return true; }
	bool CanHandleMouseWheel(int delta, int localX, int localY) override;
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY) override;

private:
	struct HitRegion
	{
		int SeriesIndex = -1;
		int PointIndex = -1;
		D2D1_RECT_F Rect{ 0,0,0,0 };
		D2D1_POINT_2F Center{ 0,0 };
		float Radius = 0.0f;
		float StartAngle = 0.0f;
		float SweepAngle = 0.0f;
		bool IsPie = false;
		bool IsCircle = false;
	};

	std::vector<HitRegion> _hitRegions;
	bool _scrolling = false;
	float _scrollGrabOffsetX = 0.0f;
	int _lastMouseX = 0;
	int _lastMouseY = 0;

	void DrawFrame(D2DGraphics* d2d, float width, float height);
	void DrawAxes(D2DGraphics* d2d, const D2D1_RECT_F& plotRect, double minValue, double maxValue);
	void DrawBarChart(D2DGraphics* d2d, const D2D1_RECT_F& plotRect);
	void DrawLineChart(D2DGraphics* d2d, const D2D1_RECT_F& plotRect);
	void DrawPieChart(D2DGraphics* d2d, const D2D1_RECT_F& contentRect);
	void DrawLegend(D2DGraphics* d2d, const D2D1_RECT_F& contentRect);
	void DrawTooltip(D2DGraphics* d2d, float width, float height);
	void DrawHorizontalScrollBar(D2DGraphics* d2d, float width, float height);
	void RebuildHitRegions();
	bool HitTestInternal(int localX, int localY, int& seriesIndex, int& pointIndex);
	void UpdateHover(int localX, int localY);
	void ClampViewport();
	void UpdateHorizontalScrollDrag(float localX, float width, float height);

	D2D1_RECT_F GetContentRect(float width, float height) const;
	D2D1_RECT_F GetPlotRect(float width, float height) const;
	D2D1_RECT_F GetHorizontalScrollTrackRect(float width, float height) const;
	D2D1_RECT_F GetHorizontalScrollThumbRect(float width, float height) const;
	D2D1_COLOR_F GetSeriesColor(int seriesIndex, int pointIndex = -1) const;
	std::wstring GetPointText(int seriesIndex, int pointIndex) const;
	std::wstring FormatValue(double value) const;
	bool HasHorizontalScrollBar() const;
	float GetVirtualPlotWidth(const D2D1_RECT_F& plotRect) const;
	float GetMaxViewScrollX(float width, float height) const;
	void GetVisibleIndexRange(int& start, int& end);
	int GetPointCount() const;
	int GetVisibleSeriesCount() const;
	bool GetValueRange(double& minValue, double& maxValue) const;
	float ValueToY(double value, const D2D1_RECT_F& plotRect, double minValue, double maxValue) const;
};
