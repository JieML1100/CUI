#pragma once
#include "Control.h"
#pragma comment(lib, "Imm32.lib")

/**
 * @file ProgressBar.h
 * @brief ProgressBar：进度条控件。
 *
 * 使用方式：
 * - 通过 PercentageValue 控制填充比例（通常期望在 [0, 1]）
 * - BackColor/ForeColor 决定背景与进度颜色
 */
class ProgressBar : public Control
{
private:
	float _maxValue = 1.0f;
	float _currentValue = 0.5f;
public:
	virtual UIClass Type();
	void EnsureBindingPropertiesRegistered() override;
	/** @brief 边框宽度（像素）。 */
	float BorderThickness = 1.5f;
	/** @brief 轨道边框色。 */
	D2D1_COLOR_F TrackBorderColor = cui::theme::palette::Border;
	/** @brief 进度填充的顶部高光。 */
	D2D1_COLOR_F FillHighlightColor = D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.14f };
	/** @brief 禁用遮罩色。 */
	D2D1_COLOR_F DisabledOverlayColor = cui::theme::palette::DisabledOverlay;
	/** @brief 进度条圆角；小于 0 时自动使用胶囊圆角。 */
	float CornerRadius = -1.0f;
	/** @brief 填充与外框之间的内边距。 */
	float InnerPadding = 2.0f;
	/**
	 * @brief 进度最大值。
	 */
	PROPERTY(float, MaxValue);
	GET(float, MaxValue);
	SET(float, MaxValue);
	/**
	 * @brief 进度当前值。
	 */
	PROPERTY(float, Value);
	GET(float, Value);
	SET(float, Value);

	/**
	 * @brief 进度比例。
	 *
	 * 约定：常用范围为 [0, 1]，渲染时以 `Width * PercentageValue` 计算进度宽度。
	 */
	PROPERTY(float, PercentageValue);
	GET(float, PercentageValue);
	SET(float, PercentageValue);
	/** @brief 创建进度条。 */
	ProgressBar(int x, int y, int width = 120, int height = 24);
	void Update() override;
};
