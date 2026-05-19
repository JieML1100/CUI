#pragma once
#include "Control.h"
#pragma comment(lib, "Imm32.lib")

/**
 * @file Label.h
 * @brief Label：文本显示控件（只读）。
 *
 * Label 在测量阶段按文本大小给出期望尺寸，最终显示范围由父布局应用的 Size 决定。
 */
class Label : public Control
{
public:
	/** @brief 上一次渲染/测量使用的宽度（用于缓存/重算）。 */
	float lastMeasuredWidth = 0.0f;
	virtual UIClass Type();
	/** @brief 创建 Label。 */
	Label(std::wstring text, int x, int y);
	SIZE MeasureCore(SIZE availableSize) override;
	void Update() override;
};
