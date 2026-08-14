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
	void UpdateRadioButtonGroup();
protected:
	bool OnAccessKey(bool isMultiple) override;
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override
	{
		return std::make_unique<RadioButtonAutomationPeer>(*this);
	}
	void OnIsCheckedChanged(
		NullableBool oldValue, NullableBool newValue) override;
	void OnToggle() override;
public:
	virtual UIClass Type();
	/** @brief 创建单选框。 */
	RadioButton();
	static void RegisterDependencyProperties();
	static const DependencyProperty& GroupNameProperty();
#if CUI_ENABLE_DYNAMIC_XAML
	void EnsureBindingPropertiesRegistered() override
	{
		RegisterDependencyProperties();
	}
#endif
	PROPERTY(std::wstring, GroupName);
	GET(std::wstring, GroupName);
	SET(std::wstring, GroupName);
	/** @brief 以程序方式设置选中状态并触发 Checked/Unchecked。 */
	void SetChecked(bool checked);
	bool Invoke() override;
};
