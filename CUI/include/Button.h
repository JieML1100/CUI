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
	bool DefaultSelectOnLeftButtonDoubleClick() const override { return true; }
	bool DefaultInvalidateVisualOnMouseDoubleClick(UINT message, bool wasSelected) const override { (void)message; (void)wasSelected; return true; }
private:
	D2D1_COLOR_F _underMouseColor = D2D1_COLOR_F{ 0.20f, 0.46f, 0.90f, 0.16f };
	D2D1_COLOR_F _checkedColor = D2D1_COLOR_F{ 0.20f, 0.46f, 0.90f, 0.28f };
	D2D1_COLOR_F _highlightColor = D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.04f };
	D2D1_COLOR_F _shadowColor = D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 0.06f };
	D2D1_COLOR_F _disabledOverlayColor = D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.42f };
	bool _raised = false;
	float _borderThickness = 1.5f;
	float _round = 7.0f;
public:
	virtual UIClass Type();
	void EnsureBindingPropertiesRegistered() override;
	/** @brief 鼠标悬停时背景色。 */
	PROPERTY(D2D1_COLOR_F, UnderMouseColor);
	GET(D2D1_COLOR_F, UnderMouseColor);
	SET(D2D1_COLOR_F, UnderMouseColor);
	/** @brief Checked=true 时背景色。 */
	PROPERTY(D2D1_COLOR_F, CheckedColor);
	GET(D2D1_COLOR_F, CheckedColor);
	SET(D2D1_COLOR_F, CheckedColor);
	/** @brief 顶部高光色。 */
	PROPERTY(D2D1_COLOR_F, HighlightColor);
	GET(D2D1_COLOR_F, HighlightColor);
	SET(D2D1_COLOR_F, HighlightColor);
	/** @brief 阴影色。 */
	PROPERTY(D2D1_COLOR_F, ShadowColor);
	GET(D2D1_COLOR_F, ShadowColor);
	SET(D2D1_COLOR_F, ShadowColor);
	/** @brief 禁用遮罩色。 */
	PROPERTY(D2D1_COLOR_F, DisabledOverlayColor);
	GET(D2D1_COLOR_F, DisabledOverlayColor);
	SET(D2D1_COLOR_F, DisabledOverlayColor);
	/** @brief 是否启用轻微立体效果（高光、阴影、按下位移）。false 为扁平按钮。 */
	PROPERTY(bool, Raised);
	GET(bool, Raised);
	SET(bool, Raised);
	PROPERTY(float, BorderThickness);
	GET(float, BorderThickness);
	SET(float, BorderThickness);
	/** @brief 圆角半径（像素）。设置为 0 可得到直角按钮。 */
	PROPERTY(float, Round);
	GET(float, Round);
	SET(float, Round);
	/** @brief 创建按钮。 */
	Button(std::wstring text, int x, int y, int width = 120, int height = 24);
	bool Invoke() override;
	void Update() override;
};
