#pragma once

#include "Control.h"

class ProgressRing : public Control
{
private:
	float _percentageValue = 0.5f;
	bool _showPercentage = true;

public:
	virtual UIClass Type();
	float Boder = 0.0f;
	/** @brief 进度弧后方的柔光色。 */
	D2D1_COLOR_F ProgressGlowColor = D2D1_COLOR_F{ 0.20f, 0.55f, 0.95f, 0.14f };
	/** @brief 中心文字颜色；默认跟随 ForeColor。 */
	D2D1_COLOR_F CenterTextColor = D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 0.0f };
	/** @brief 中心区域背景色。 */
	D2D1_COLOR_F CenterBackColor = D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.06f };
	/** @brief 禁用遮罩色。 */
	D2D1_COLOR_F DisabledOverlayColor = D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.42f };
	/** @brief 环形线宽；小于等于 0 时自动计算。 */
	float RingThickness = -1.0f;
	/** @brief 是否绘制圆形端点。 */
	bool ShowCaps = true;

	PROPERTY(float, PercentageValue);
	GET(float, PercentageValue);
	SET(float, PercentageValue);

	PROPERTY(bool, ShowPercentage);
	GET(bool, ShowPercentage);
	SET(bool, ShowPercentage);

	ProgressRing(int x, int y, int width = 72, int height = 72);
	void Update() override;
};
