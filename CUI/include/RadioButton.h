#pragma once
#include "ToggleButton.h"

/**
 * @file RadioButton.h
 * @brief WPF RadioButton behavior host.
 *
 * Empty GroupName groups direct logical siblings. A non-empty GroupName
 * groups matching RadioButton instances in the same presentation tree.
 */
class RadioButton : public ToggleButton
{
	std::wstring _groupName;
	float lastMeasuredWidth = 0.0f;
	float _selectProgress = 0.0f;
	float _animStartProgress = 0.0f;
	float _animTargetProgress = 0.0f;
	ULONGLONG _animStartTick = 0;
	UINT _animDurationMs = 120;
	bool _animating = false;
	D2D1_COLOR_F UnderMouseColor = cui::theme::palette::AccentSoft;
	D2D1_COLOR_F CircleBackColor = cui::theme::palette::Surface;
	D2D1_COLOR_F CircleBorderColor = cui::theme::palette::BorderStrong;
	D2D1_COLOR_F SelectedColor = cui::theme::palette::Accent;
	D2D1_COLOR_F DotColor = cui::theme::palette::OnAccent;
	D2D1_COLOR_F DisabledOverlayColor = cui::theme::palette::DisabledOverlay;
	float TextGap = 8.0f;
	void StartSelectionAnimation(bool checked);
	float CurrentSelectionProgress();
	void UpdateRadioButtonGroup();
protected:
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override
	{
		return std::make_unique<RadioButtonAutomationPeer>(*this);
	}
	void ConfigureContentVisual(Control& child) override;
	void PerformPendingLayout() override;
	void OnIsCheckedChanged(bool oldValue, bool newValue) override;
	bool DefaultRaiseClickOnLeftButtonUp() const override { return true; }
	void BeforeDefaultMouseUp(MouseButton button, MouseEventArgs& e, bool hasMatchingPress) override;
public:
	virtual UIClass Type();
	/** @brief 创建单选框。 */
	RadioButton();
	static void RegisterDependencyProperties();
	void EnsureBindingPropertiesRegistered() override
	{
		RegisterDependencyProperties();
	}
	PROPERTY(std::wstring, GroupName);
	GET(std::wstring, GroupName);
	SET(std::wstring, GroupName);
	/** @brief 以程序方式设置选中状态，带动画并触发 Checked/Unchecked。 */
	void SetChecked(bool checked);
	bool IsAnimationRunning() override;
	UINT GetAnimationIntervalMs() override { return 16; }
	bool GetAnimatedInvalidRect(D2D1_RECT_F& outRect) override;
	cui::core::Size MeasureCore(
		const cui::core::Constraints& available) override;
	bool Invoke() override;
protected:
	void OnRender() override;
};
