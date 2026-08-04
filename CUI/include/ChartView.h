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
protected:
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override
	{
		return std::make_unique<ChartViewAutomationPeer>(*this);
	}

private:
	std::vector<ChartSeries> _series;
	std::vector<std::vector<uint32_t>> _accessibilityIds;
	ChartViewKind _chartKind = ChartViewKind::Bar;
	std::wstring _title = L"Chart";
	std::wstring _subtitle;
	int _valuePrecision = 0;
	bool _showLegend = true;
	bool _showTooltip = true;
	bool _showValueLabels = false;
	bool _showGridLines = true;
	bool _showMarkers = true;
	bool _enablePanZoom = true;

	// Private native presenter defaults. Public appearance belongs to XAML
	// templates/styles rather than to ChartView's semantic property surface.
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

public:
	using UIElement::SelectionChanged;
	UIClass Type() override;
	static void RegisterDependencyProperties();
	static const DependencyProperty& ChartKindProperty();
	static const DependencyProperty& TitleProperty();
	static const DependencyProperty& SubtitleProperty();
	static const DependencyProperty& ValuePrecisionProperty();
	static const DependencyProperty& ShowLegendProperty();
	static const DependencyProperty& ShowTooltipProperty();
	static const DependencyProperty& ShowValueLabelsProperty();
	static const DependencyProperty& ShowGridLinesProperty();
	static const DependencyProperty& ShowMarkersProperty();
	static const DependencyProperty& EnablePanZoomProperty();
#if CUI_ENABLE_DYNAMIC_XAML
	void EnsureBindingPropertiesRegistered() override { RegisterDependencyProperties(); }
#endif
	ChartView();

	PROPERTY(ChartViewKind, ChartKind);
	GET(ChartViewKind, ChartKind);
	SET(ChartViewKind, ChartKind);
	PROPERTY(std::wstring, Title);
	GET(std::wstring, Title);
	SET(std::wstring, Title);
	PROPERTY(std::wstring, Subtitle);
	GET(std::wstring, Subtitle);
	SET(std::wstring, Subtitle);
	PROPERTY(int, ValuePrecision);
	GET(int, ValuePrecision);
	SET(int, ValuePrecision);
	PROPERTY(bool, ShowLegend);
	GET(bool, ShowLegend);
	SET(bool, ShowLegend);
	PROPERTY(bool, ShowTooltip);
	GET(bool, ShowTooltip);
	SET(bool, ShowTooltip);
	PROPERTY(bool, ShowValueLabels);
	GET(bool, ShowValueLabels);
	SET(bool, ShowValueLabels);
	PROPERTY(bool, ShowGridLines);
	GET(bool, ShowGridLines);
	SET(bool, ShowGridLines);
	PROPERTY(bool, ShowMarkers);
	GET(bool, ShowMarkers);
	SET(bool, ShowMarkers);
	PROPERTY(bool, EnablePanZoom);
	GET(bool, EnablePanZoom);
	SET(bool, EnablePanZoom);

	const std::vector<ChartSeries>& GetSeries() const noexcept { return _series; }
	float GetZoomX() const noexcept { return ZoomX; }
	float GetPanX() const noexcept { return PanX; }
	int GetSelectedSeriesIndex() const noexcept { return SelectedSeriesIndex; }
	int GetSelectedPointIndex() const noexcept { return SelectedPointIndex; }

	ChartPointEvent OnPointClick;
	ChartPointEvent OnPointHover;
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
	bool HandlesNavigationKey(Key key) const override;
	bool TryGetAccessibilityVirtualNode(
		uint32_t id, AccessibilityVirtualNode& result);
	size_t GetAccessibilityVirtualChildCount(
		uint32_t parentId) const noexcept;
	bool TryGetAccessibilityVirtualChildAt(
		uint32_t parentId, size_t index, uint32_t& result) const noexcept;
	bool TryGetAccessibilityVirtualSibling(
		uint32_t parentId, uint32_t id, bool next,
		uint32_t& result) const noexcept;
	bool TryHitTestAccessibilityVirtualNode(
		float localX, float localY, uint32_t& result);
	AccessibilityVirtualContainerInfo
		GetAccessibilityVirtualContainerInfo() const noexcept;
	void GetAccessibilityVirtualSelection(
		std::vector<uint32_t>& result) const;
	bool InvokeAccessibilityVirtualNode(uint32_t id);
	bool SelectAccessibilityVirtualNode(
		uint32_t id, AccessibilitySelectionAction action);
protected:
	void OnRender() override;
	bool ProcessInput(const InputReport& input) override;
private:
	struct HitRegion
	{
		int SeriesIndex = -1;
		int PointIndex = -1;
		D2D1_RECT_F Rect{ 0,0,0,0 };
		D2D1_POINT_2F Center{ 0,0 };
		float Radius = 0.0f;
		float InnerRadius = 0.0f;
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
	void EnsurePointVisible(int pointIndex);
	void DrawKeyboardFocus(D2DGraphics* d2d);
	uint32_t GetAccessibilityId(
		int seriesIndex, int pointIndex) const noexcept;
	bool FindAccessibilityPoint(
		uint32_t id, int& seriesIndex, int& pointIndex) const noexcept;

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
