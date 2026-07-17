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
	float lastMeasuredWidth = 0.0f;
	float _checkProgress = 0.0f;
	float _animStartProgress = 0.0f;
	float _animTargetProgress = 0.0f;
	ULONGLONG _animStartTick = 0;
	UINT _animDurationMs = 120;
	bool _animating = false;
	void StartCheckAnimation(bool checked);
	float CurrentCheckProgress();
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
	D2D1_COLOR_F UnderMouseColor = cui::theme::palette::AccentSoft;
	/** @brief 未选中框背景色。 */
	D2D1_COLOR_F BoxBackColor = cui::theme::palette::Surface;
	/** @brief 未选中框边框色。 */
	D2D1_COLOR_F BoxBorderColor = cui::theme::palette::BorderStrong;
	/** @brief 选中框背景色。 */
	D2D1_COLOR_F CheckedBackColor = cui::theme::palette::Accent;
	/** @brief 勾选标记颜色。 */
	D2D1_COLOR_F CheckMarkColor = cui::theme::palette::OnAccent;
	/** @brief 禁用遮罩色。 */
	D2D1_COLOR_F DisabledOverlayColor = cui::theme::palette::DisabledOverlay;
	/** @brief 勾选框圆角。 */
	float BoxCornerRadius = 4.0f;
	/** @brief 文本与勾选框间距。 */
	float TextGap = 8.0f;
	/** @brief 边框宽度（像素）。 */
	float Border = 1.5f;
	/** @brief 创建复选框。 */
	CheckBox(std::wstring text, int x, int y);
	/** @brief 以程序方式设置勾选状态，带动画并触发 OnChecked。 */
	void SetChecked(bool checked);
	/** @brief 切换勾选状态，带动画并触发 OnChecked。 */
	void Toggle();
	bool IsAnimationRunning() override;
	UINT GetAnimationIntervalMs() override { return 16; }
	bool GetAnimatedInvalidRect(D2D1_RECT_F& outRect) override;
	SIZE ActualSize() override;
	bool Invoke() override;
	void Update() override;
};
