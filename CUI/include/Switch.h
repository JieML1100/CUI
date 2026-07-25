#pragma once
#include "ToggleButton.h"

/**
 * @file Switch.h
 * @brief Switch：开关控件。
 *
 * 说明：
 * - 开关状态由 ToggleButton 拥有
 * - 交互沿 routed input/default behavior 管线处理，绘制由 OnRender 负责
 */
class Switch : public ToggleButton
{
	float lastMeasuredWidth = 0.0f;
	float _thumbProgress = 0.0f;
	float _animStartProgress = 0.0f;
	float _animTargetProgress = 0.0f;
	ULONGLONG _animStartTick = 0;
	UINT _animDurationMs = 140;
	bool _animating = false;
	D2D1_COLOR_F UnderMouseColor = cui::theme::palette::AccentSoft;
	D2D1_COLOR_F TrackOffColor = cui::theme::palette::SurfaceMuted;
	D2D1_COLOR_F TrackOnColor = cui::theme::palette::Accent;
	D2D1_COLOR_F TrackBorderColor = cui::theme::palette::BorderStrong;
	D2D1_COLOR_F ThumbColor = cui::theme::palette::Surface;
	D2D1_COLOR_F ThumbShadowColor = cui::theme::palette::Shadow;
	D2D1_COLOR_F DisabledOverlayColor = cui::theme::palette::DisabledOverlay;
	float TrackPadding = 3.0f;
	void SyncAnimationState();
	void StartToggleAnimation(bool checked);
	float CurrentThumbProgress();
protected:
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override
	{
		return std::make_unique<ToggleAutomationPeer>(
			*this, AutomationControlType::CheckBox, L"Switch");
	}
	bool DefaultRaiseClickOnLeftButtonUp() const override { return true; }
	void BeforeDefaultMouseUp(MouseButton button, MouseEventArgs& e, bool hasMatchingPress) override;
public:
	virtual UIClass Type();
	/** @brief 创建开关。 */
	Switch();
	/** @brief 以程序方式设置开关状态，带动画并触发 Checked/Unchecked。 */
	void SetChecked(bool checked);
	/** @brief 切换开关状态，带动画并触发 Checked/Unchecked。 */
	void Toggle();
	bool IsAnimationRunning() override;
	UINT GetAnimationIntervalMs() override { return 16; }
	bool GetAnimatedInvalidRect(D2D1_RECT_F& outRect) override;
	bool Invoke() override;
protected:
	void OnRender() override;
};
