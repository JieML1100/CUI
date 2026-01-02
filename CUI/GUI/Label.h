#pragma once
#include "Control.h"
#pragma comment(lib, "Imm32.lib")

/**
 * @file Label.h
 * @brief Label：文本显示控件（只读）。
 *
 * Label 会根据文本测量自动调整显示（ActualSize 可能与 Size 不同，见实现）。
 */
class Label : public Control
{
public:
	/** @brief 上一次渲染/测量使用的宽度（用于缓存/重算）。 */
	float last_width = 0.0f;
	virtual UIClass Type();
	/** @brief 创建 Label。 */
	Label(std::wstring text, int x, int y);
	SIZE ActualSize() override;
	void Update() override;
};