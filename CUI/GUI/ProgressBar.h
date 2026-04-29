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
	/** @brief 边框宽度（像素）。 */
	float Boder = 1.5f;
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
	/** @brief 进度值变化事件。 */
	EventHandler OnValueChanged = EventHandler();
	/** @brief 创建进度条。 */
	ProgressBar(int x, int y, int width = 120, int height = 24);
	/** @brief 同时设置最大值和当前值。 */
	void SetRange(float maxValue, float value = 0.0f);
	/** @brief 将当前值增加指定步长，并自动夹取到合法范围。 */
	void Increment(float delta);
	/** @brief 将当前值重置为 0。 */
	void Reset();
	void Update() override;
};
