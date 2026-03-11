#pragma once
#include "Control.h"
#pragma comment(lib, "Imm32.lib")

/**
 * @file RadioBox.h
 * @brief RadioBox：单选框风格控件。
 *
 * 说明：
 * - 勾选状态使用基类字段 Control::Checked
 * - “互斥分组”通常需要由外部容器或业务逻辑保证（框架未在此头文件层面强制）
 */
class RadioBox : public Control
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
	/** @brief 鼠标悬停时的高亮色。 */
	D2D1_COLOR_F UnderMouseColor = Colors::DarkSlateGray;
	/** @brief 边框宽度（像素）。 */
	float Boder = 1.5f;
	/** @brief 创建单选框。 */
	RadioBox(std::wstring text, int x, int y);
	SIZE ActualSize() override;
	void Update() override;
};