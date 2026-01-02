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
	float _percentageValue = 0.5f;
public:
	virtual UIClass Type();
	/** @brief 边框宽度（像素）。 */
	float Boder = 1.5f;

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