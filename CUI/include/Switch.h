#pragma once
#include "ToggleButton.h"

/**
 * @file Switch.h
 * @brief Switch：开关控件。
 *
 * 说明：
 * - 开关状态由 ToggleButton 拥有
 * - 交互沿 routed input/default behavior 管线处理
 * - 默认轨道、滑块和视觉状态完全由 framework theme 提供
 */
class Switch : public ToggleButton
{
protected:
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override
	{
		return std::make_unique<ToggleAutomationPeer>(
			*this, AutomationControlType::CheckBox, L"Switch");
	}
public:
	virtual UIClass Type();
	/** @brief 创建开关。 */
	Switch();
	/** @brief 以程序方式设置开关状态并触发 Checked/Unchecked。 */
	void SetChecked(bool checked);
	/** @brief 切换开关状态并触发 Checked/Unchecked。 */
	void Toggle();
	bool IsAnimationRunning() override;
	bool GetAnimatedInvalidRect(D2D1_RECT_F& outRect) override;
	bool Invoke() override;
};
