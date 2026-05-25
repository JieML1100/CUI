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
protected:
	bool DefaultTrackUnderMouse() const override { return true; }
	bool DefaultRaiseClickOnLeftButtonUp() const override { return true; }
	bool DefaultClearSelectionOnMouseUp() const override { return true; }
	bool DefaultSelectOnLeftButtonDoubleClick() const override { return true; }
	bool DefaultInvalidateVisualOnMouseDoubleClick(UINT message, bool wasSelected) const override { (void)message; (void)wasSelected; return true; }
public:
	virtual UIClass Type();
	/** @brief 鼠标悬停时背景色。 */
	D2D1_COLOR_F UnderMouseColor = D2D1_COLOR_F{ 0.20f, 0.46f, 0.90f, 0.16f };
	/** @brief Checked=true 时背景色。 */
	D2D1_COLOR_F CheckedColor = D2D1_COLOR_F{ 0.20f, 0.46f, 0.90f, 0.28f };
	/** @brief 顶部高光色。 */
	D2D1_COLOR_F HighlightColor = D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.04f };
	/** @brief 阴影色。 */
	D2D1_COLOR_F ShadowColor = D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 0.06f };
	/** @brief 禁用遮罩色。 */
	D2D1_COLOR_F DisabledOverlayColor = D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.42f };
	/** @brief 是否启用轻微立体效果（高光、阴影、按下位移）。false 为扁平按钮。 */
	bool Raised = false;
	float BorderThickness = 1.5f;
	/** @brief 圆角半径（像素）。设置为 0 可得到直角按钮。 */
	float Round = 7.0f;
	/** @brief 创建按钮。 */
	Button(std::wstring text, int x, int y, int width = 120, int height = 24);
	void Update() override;
};
