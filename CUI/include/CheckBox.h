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
protected:
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override
	{
		return std::make_unique<ToggleAutomationPeer>(
			*this, AutomationControlType::CheckBox, L"CheckBox");
	}
	bool DefaultRaiseClickOnLeftButtonUp() const override { return true; }
	void BeforeDefaultMouseUp(MouseButton button, MouseEventArgs& e, bool hasMatchingPress) override;
public:
	virtual UIClass Type();
	/** @brief 创建复选框。 */
	CheckBox() = default;
	/** @brief 以程序方式设置勾选状态并触发 Checked/Unchecked。 */
	void SetChecked(bool checked);
	/** @brief 切换勾选状态。 */
	void Toggle();
	bool Invoke() override;
};
