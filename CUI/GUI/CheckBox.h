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
protected:
	bool DefaultTrackUnderMouse() const override { return true; }
	bool DefaultRaiseClickOnLeftButtonUp() const override { return true; }
	bool DefaultClearSelectionOnMouseUp() const override { return true; }
	bool DefaultSelectOnLeftButtonDoubleClick() const override { return true; }
	bool DefaultRaiseMouseDoubleClick(UINT message, bool wasSelected) const override;
	bool DefaultPostRenderOnMouseDoubleClick(UINT message, bool wasSelected) const override;
	void BeforeDefaultMouseUp(UINT message, MouseEventArgs& e, bool wasSelected) override;
	void BeforeDefaultMouseDoubleClick(UINT message, MouseEventArgs& e, bool wasSelected) override;
public:
	virtual UIClass Type();
	bool HandlesNavigationKey(WPARAM key) const override;
	/** @brief 鼠标悬停时的高亮色。 */
	D2D1_COLOR_F UnderMouseColor = Colors::DarkSlateGray;
	/** @brief 边框宽度（像素）。 */
	float Border = 1.5f;
	/** @brief 创建复选框。 */
	CheckBox(std::wstring text, int x, int y);
	/** @brief 设置勾选状态，可选择是否触发 OnChecked。 */
	void SetChecked(bool checked, bool fireEvent = true);
	/** @brief 切换勾选状态。 */
	void Toggle(bool fireEvent = true);
	SIZE ActualSize() override;
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof) override;
};
