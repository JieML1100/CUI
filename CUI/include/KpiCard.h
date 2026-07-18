#pragma once
#include "Control.h"
#include <vector>

enum class KpiTrendDirection
{
	Neutral,
	Up,
	Down
};

typedef Event<void(class KpiCard*)> KpiCardEvent;

class KpiCard : public Control
{
public:
	UIClass Type() override;
	void EnsureBindingPropertiesRegistered() override;
	KpiCard(int x = 0, int y = 0, int width = 220, int height = 132);

	std::wstring Title = L"Metric";
	std::wstring Value = L"0";
	std::wstring Unit = L"";
	std::wstring TrendText = L"";
	std::wstring Caption = L"";
	std::vector<double> SparklineValues;
	KpiTrendDirection TrendDirection = KpiTrendDirection::Neutral;

	bool Clickable = true;
	bool Active = false;
	bool ShowSparkline = true;
	bool ToggleActiveOnClick = true;

	float Border = 1.0f;
	float CornerRadius = 8.0f;

	D2D1_COLOR_F SurfaceColor = cui::theme::palette::Surface;
	D2D1_COLOR_F ActiveBackColor = cui::theme::palette::AccentSelected;
	D2D1_COLOR_F HoverColor = cui::theme::palette::AccentSoft;
	D2D1_COLOR_F AccentColor = cui::theme::palette::Accent;
	D2D1_COLOR_F MutedTextColor = cui::theme::palette::TextMuted;
	D2D1_COLOR_F PositiveColor = cui::theme::palette::Positive;
	D2D1_COLOR_F NegativeColor = cui::theme::palette::Negative;
	D2D1_COLOR_F NeutralColor = cui::theme::palette::TextMuted;
	D2D1_COLOR_F SparklineFillColor = cui::theme::palette::AccentSoft;

	KpiCardEvent OnCardClick;

	CursorKind QueryCursor(int localX, int localY) override;
	void SetSparkline(const std::vector<double>& values);
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY) override;

private:
	bool _pressed = false;

	D2D1_COLOR_F GetTrendColor() const;
	void DrawSparkline(D2DGraphics* d2d, const D2D1_RECT_F& rect);
};
