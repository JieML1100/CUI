#pragma once
#include "Control.h"

/**
 * @file CheckBox.h
 * @brief CheckBox：复选框控件。
 *
 * 使用方式：
 * - 通过基类字段 Control::Checked 表示勾选状态
 * - 通常由鼠标点击触发状态切换，并通过基类事件对外通知（如 OnMouseClick/OnChecked 等）
 */
class CheckBox : public Control
{
	float last_width = 0.0f;
public:
	virtual UIClass Type();
	/** @brief 鼠标悬停时的高亮色。 */
	D2D1_COLOR_F UnderMouseColor = Colors::DarkSlateGray;
	/** @brief 边框宽度（像素）。 */
	float Border = 1.5f;
	/** @brief 创建复选框。 */
	CheckBox(std::wstring text, int x, int y);
	SIZE ActualSize() override;
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof) override;
};
