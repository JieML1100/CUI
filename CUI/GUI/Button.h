#pragma once
#include "Control.h"

/**
 * @file Button.h
 * @brief Button：基础按钮控件。
 *
 * 主要行为：
 * - 根据鼠标悬停/按下/Checked 状态绘制不同背景
 * - 通过 Control 事件（OnMouseDown/OnMouseUp/OnMouseClick/OnChecked 等）对外通知
 */
class Button : public Control
{
public:
	virtual UIClass Type();
	/** @brief 鼠标悬停时背景色。 */
	D2D1_COLOR_F UnderMouseColor = Colors::SkyBlue;
	/** @brief Checked=true 时背景色。 */
	D2D1_COLOR_F CheckedColor = Colors::SteelBlue;
	float Boder = 1.5f;
	/** @brief 圆角半径（像素）。 */
	float Round = 0.0f;
	/** @brief 创建按钮。 */
	Button(std::wstring text, int x, int y, int width = 120, int height = 24);
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof) override;
};
