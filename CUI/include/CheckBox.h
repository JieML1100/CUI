#pragma once
#include "ToggleButton.h"

/**
 * @file CheckBox.h
 * @brief CheckBox：复选框控件。
 *
 * 使用方式：
 * - 通过 ToggleButton::IsChecked 表示勾选状态
 * - 通常由鼠标点击触发状态切换，并通过 ToggleButton 事件对外通知
 */
class CheckBox : public ToggleButton
{
	float lastMeasuredWidth = 0.0f;
	float _checkProgress = 0.0f;
	float _animStartProgress = 0.0f;
	float _animTargetProgress = 0.0f;
	ULONGLONG _animStartTick = 0;
	UINT _animDurationMs = 120;
	bool _animating = false;
	D2D1_COLOR_F UnderMouseColor = cui::theme::palette::AccentSoft;
	D2D1_COLOR_F BoxBackColor = cui::theme::palette::Surface;
	D2D1_COLOR_F BoxBorderColor = cui::theme::palette::BorderStrong;
	D2D1_COLOR_F CheckedBackColor = cui::theme::palette::Accent;
	D2D1_COLOR_F CheckMarkColor = cui::theme::palette::OnAccent;
	D2D1_COLOR_F DisabledOverlayColor = cui::theme::palette::DisabledOverlay;
	float BoxCornerRadius = 4.0f;
	float TextGap = 8.0f;
	void StartCheckAnimation(bool checked);
	float CurrentCheckProgress();
protected:
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override
	{
		return std::make_unique<ToggleAutomationPeer>(
			*this, AutomationControlType::CheckBox, L"CheckBox");
	}
	void ConfigureContentVisual(Control& child) override;
	void PerformPendingLayout() override;
	bool DefaultRaiseClickOnLeftButtonUp() const override { return true; }
	void BeforeDefaultMouseUp(MouseButton button, MouseEventArgs& e, bool hasMatchingPress) override;
public:
	virtual UIClass Type();
	/** @brief 创建复选框。 */
	CheckBox();
	/** @brief 以程序方式设置勾选状态，带动画并触发 Checked/Unchecked。 */
	void SetChecked(bool checked);
	/** @brief 切换勾选状态，带动画并触发 Checked/Unchecked。 */
	void Toggle();
	bool IsAnimationRunning() override;
	UINT GetAnimationIntervalMs() override { return 16; }
	bool GetAnimatedInvalidRect(D2D1_RECT_F& outRect) override;
	cui::core::Size MeasureCore(
		const cui::core::Constraints& available) override;
	bool Invoke() override;
protected:
	void OnRender() override;
};
