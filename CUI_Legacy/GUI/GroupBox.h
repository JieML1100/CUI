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

	GroupBox();
	GroupBox(std::wstring text, int x, int y, int width, int height);

	UIClass Type() override;
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof) override;

private:
	void PerformGroupLayoutIfNeeded();
	float GetCaptionBandHeight();
};
