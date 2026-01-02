#pragma once
#include "Control.h"
#pragma comment(lib, "Imm32.lib")

/**
 * @file Switch.h
 * @brief Switch：开关控件。
 *
 * 说明：
 * - 状态通常使用基类字段 Control::Checked 表示（开/关）
 * - 交互由 ProcessMessage 处理（点击、拖动等），绘制由 Update 负责
 */
class Switch : public Control
{
	float last_width = 0.0f;
public:
	virtual UIClass Type();
	/** @brief 鼠标悬停时的高亮色。 */
	D2D1_COLOR_F UnderMouseColor = Colors::DarkSlateGray;
	/** @brief 边框宽度（像素）。 */
	float Boder = 1.5f;
	/** @brief 创建开关。 */
	Switch(int x = 0, int y = 0, int width = 60, int height = 22);
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof) override;
};