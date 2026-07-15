#pragma once
#include "Panel.h"

/**
 * @file GroupBox.h
 * @brief GroupBox: lightweight container with a captioned border.
 */
class GroupBox : public Panel
{
private:
	float _captionMarginLeft = 12.0f;
	float _captionPaddingX = 6.0f;
	float _captionPaddingY = 2.0f;
	float _captionCornerRadius = 6.0f;
	D2D1_COLOR_F _captionBackColor = D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 0.0f };
	D2D1_COLOR_F _captionBorderColor = D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 0.0f };

protected:
	void PerformPendingLayout() override;

public:
	GroupBox();
	GroupBox(std::wstring text, int x, int y, int width, int height);

	UIClass Type() override;
	void EnsureBindingPropertiesRegistered() override;

#define CUI_GROUP_BOX_PROPERTY(type, name) \
	PROPERTY(type, name); \
	GET(type, name); \
	SET(type, name)

	CUI_GROUP_BOX_PROPERTY(float, CaptionMarginLeft);
	CUI_GROUP_BOX_PROPERTY(float, CaptionPaddingX);
	CUI_GROUP_BOX_PROPERTY(float, CaptionPaddingY);
	CUI_GROUP_BOX_PROPERTY(float, CaptionCornerRadius);
	CUI_GROUP_BOX_PROPERTY(D2D1_COLOR_F, CaptionBackColor);
	CUI_GROUP_BOX_PROPERTY(D2D1_COLOR_F, CaptionBorderColor);

#undef CUI_GROUP_BOX_PROPERTY

	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY) override;

private:
	void PerformGroupLayoutIfNeeded();
	float GetCaptionBandHeight();
};
