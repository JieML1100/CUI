#pragma once
#include "Control.h"

/**
 * @file Label.h
 * @brief Label：文本显示控件（只读）。
 *
 * Label 在测量阶段按文本大小给出期望尺寸，最终显示范围由父布局应用的 Size 决定。
 */
class Label : public Control
{
protected:
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override
	{
		return std::make_unique<AutomationPeer>(
			*this, AutomationControlType::Text, L"TextBlock");
	}
public:
	PROPERTY(std::wstring, Text);
	GET(std::wstring, Text);
	SET(std::wstring, Text);
	virtual UIClass Type();
	static void RegisterDependencyProperties();
	void EnsureBindingPropertiesRegistered() override
	{
		RegisterDependencyProperties();
	}
	/** @brief 创建 Label。 */
	Label();
	cui::core::Size MeasureCore(const cui::core::Constraints& available) override;
protected:
	void OnRender() override;
};
