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
	bool OnAccessKey(bool isMultiple) override;
	bool ProcessInput(const InputReport& input) override;
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override
	{
		return std::make_unique<ToggleAutomationPeer>(
			*this, AutomationControlType::CheckBox, L"CheckBox");
	}
public:
	virtual UIClass Type();
	/** @brief 创建复选框。 */
	CheckBox();
	static void RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	void EnsureBindingPropertiesRegistered() override
	{
		RegisterDependencyProperties();
	}
#endif
	/** @brief 以程序方式设置勾选状态并触发 Checked/Unchecked。 */
	void SetChecked(bool checked);
	/** @brief 设置 WPF 三态复选框的 Indeterminate 状态。 */
	void SetIndeterminate();
	/** @brief 切换勾选状态。 */
	void Toggle();
	bool Invoke() override;
};
