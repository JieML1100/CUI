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

	D2D1_COLOR_F SurfaceColor = D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.055f };
	D2D1_COLOR_F ActiveBackColor = D2D1_COLOR_F{ 0.20f, 0.52f, 0.95f, 0.16f };
	D2D1_COLOR_F HoverColor = D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.10f };
	D2D1_COLOR_F AccentColor = D2D1_COLOR_F{ 0.20f, 0.55f, 0.95f, 0.95f };
	D2D1_COLOR_F MutedTextColor = D2D1_COLOR_F{ 0.72f, 0.76f, 0.82f, 1.0f };
	D2D1_COLOR_F PositiveColor = D2D1_COLOR_F{ 0.14f, 0.70f, 0.50f, 1.0f };
	D2D1_COLOR_F NegativeColor = D2D1_COLOR_F{ 0.92f, 0.28f, 0.34f, 1.0f };
	D2D1_COLOR_F NeutralColor = D2D1_COLOR_F{ 0.66f, 0.70f, 0.78f, 1.0f };
	D2D1_COLOR_F SparklineFillColor = D2D1_COLOR_F{ 0.20f, 0.55f, 0.95f, 0.12f };

	KpiCardEvent OnCardClick;

	CursorKind QueryCursor(int xof, int yof) override;
	void SetSparkline(const std::vector<double>& values);
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof) override;

private:
	bool _pressed = false;

	D2D1_COLOR_F GetTrendColor() const;
	void DrawSparkline(D2DGraphics* d2d, const D2D1_RECT_F& rect);
};
