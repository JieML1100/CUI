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

	D2D1_COLOR_F PlotBackColor = D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.06f };
	D2D1_COLOR_F GridLineColor = D2D1_COLOR_F{ 0.55f, 0.60f, 0.68f, 0.28f };
	D2D1_COLOR_F AxisColor = D2D1_COLOR_F{ 0.55f, 0.60f, 0.68f, 0.70f };
	D2D1_COLOR_F AccentColor = D2D1_COLOR_F{ 0.20f, 0.55f, 0.95f, 0.92f };
	D2D1_COLOR_F HoverColor = D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.28f };
	D2D1_COLOR_F SelectedColor = D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.48f };
	D2D1_COLOR_F TooltipBackColor = D2D1_COLOR_F{ 0.08f, 0.10f, 0.13f, 0.94f };
	D2D1_COLOR_F TooltipBorderColor = D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.24f };
	D2D1_COLOR_F TooltipTextColor = Colors::White;
	D2D1_COLOR_F LegendTextColor = D2D1_COLOR_F{ 0.78f, 0.82f, 0.88f, 1.0f };
	D2D1_COLOR_F ScrollBackColor = D2D1_COLOR_F{ 0.45f, 0.49f, 0.56f, 0.26f };
	D2D1_COLOR_F ScrollForeColor = D2D1_COLOR_F{ 0.76f, 0.81f, 0.90f, 0.88f };
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
	bool HitTestPoint(int xof, int yof, int& seriesIndex, int& pointIndex);

	CursorKind QueryCursor(int xof, int yof) override;
	bool HandlesMouseWheel() const override { return true; }
	bool CanHandleMouseWheel(int delta, int xof, int yof) override;
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof) override;

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
	bool HitTestInternal(int xof, int yof, int& seriesIndex, int& pointIndex);
	void UpdateHover(int xof, int yof);
	void ClampViewport();
	void UpdateHorizontalScrollDrag(float xof, float width, float height);

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
