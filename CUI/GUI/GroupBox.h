#pragma once
#include "Panel.h"

/**
 * @file GroupBox.h
 * @brief GroupBox: lightweight container with a captioned border.
 */
class GroupBox : public Panel
{
public:
	float CaptionMarginLeft = 12.0f;
	float CaptionPaddingX = 6.0f;
	float CaptionPaddingY = 2.0f;
	float CaptionCornerRadius = 6.0f;
	D2D1_COLOR_F CaptionBackColor = D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 0.0f };
	D2D1_COLOR_F CaptionBorderColor = D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 0.0f };

	GroupBox();
	GroupBox(std::wstring text, int x, int y, int width, int height);

	UIClass Type() override;
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY) override;

private:
	void PerformGroupLayoutIfNeeded();
	float GetCaptionBandHeight();
};
