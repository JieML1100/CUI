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
	float lastMeasuredWidth = 0.0f;
	float _selectProgress = 0.0f;
	float _animStartProgress = 0.0f;
	float _animTargetProgress = 0.0f;
	ULONGLONG _animStartTick = 0;
	UINT _animDurationMs = 120;
	bool _animating = false;
	void StartSelectionAnimation(bool checked);
	float CurrentSelectionProgress();
protected:
	bool DefaultTrackUnderMouse() const override { return true; }
	bool DefaultRaiseClickOnLeftButtonUp() const override { return true; }
	bool DefaultSelectOnLeftButtonDoubleClick() const override { return true; }
	bool DefaultRaiseMouseDoubleClick(UINT message, bool wasSelected) const override;
	bool DefaultInvalidateVisualOnMouseDoubleClick(UINT message, bool wasSelected) const override;
	void BeforeDefaultMouseUp(UINT message, MouseEventArgs& e, bool wasSelected) override;
	void BeforeDefaultMouseDoubleClick(UINT message, MouseEventArgs& e, bool wasSelected) override;
public:
	virtual UIClass Type();
	/** @brief 鼠标悬停时的高亮色。 */
	D2D1_COLOR_F UnderMouseColor = Colors::DarkSlateGray;
	/** @brief 外圈背景色。 */
	D2D1_COLOR_F CircleBackColor = D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.08f };
	/** @brief 外圈边框色。 */
	D2D1_COLOR_F CircleBorderColor = D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.32f };
	/** @brief 选中 accent 色。 */
	D2D1_COLOR_F SelectedColor = D2D1_COLOR_F{ 0.28f, 0.63f, 0.98f, 0.92f };
	/** @brief 内部圆点颜色。 */
	D2D1_COLOR_F DotColor = D2D1_COLOR_F{ 0.98f, 0.99f, 1.0f, 1.0f };
	/** @brief 禁用遮罩色。 */
	D2D1_COLOR_F DisabledOverlayColor = D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.42f };
	/** @brief 文本与圆形控件间距。 */
	float TextGap = 8.0f;
	/** @brief 边框宽度（像素）。 */
	float BorderThickness = 1.5f;
	/** @brief 创建单选框。 */
	RadioBox(std::wstring text, int x, int y);
	bool IsAnimationRunning() override;
	UINT GetAnimationIntervalMs() override { return 16; }
	bool GetAnimatedInvalidRect(D2D1_RECT_F& outRect) override;
	SIZE ActualSize() override;
	bool Invoke() override;
	void Update() override;
};
