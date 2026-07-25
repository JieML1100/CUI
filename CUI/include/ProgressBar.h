#pragma once
#include "RangeBase.h"

/**
 * @file ProgressBar.h
 * @brief ProgressBar：进度条控件。
 *
 * 使用方式：
 * - 通过 Minimum/Maximum/Value 表示进度
 * - Background/Foreground Brush 决定背景与进度外观
 */
class ProgressBar : public RangeBase
{
protected:
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override
	{
		return std::make_unique<RangeBaseAutomationPeer>(
			*this, AutomationControlType::ProgressBar, L"ProgressBar", true);
	}

public:
	virtual UIClass Type();
	static void RegisterDependencyProperties();
	void EnsureBindingPropertiesRegistered() override { RegisterDependencyProperties(); }
	/** @brief 创建进度条。 */
	ProgressBar();
	/** @brief 在当前值上递增 delta（可为负），并触发 routed ValueChanged。 */
	void Increment(double delta);
	/** @brief 将当前值重置为 Minimum，并触发 routed ValueChanged。 */
	void Reset();
protected:
	void OnRender() override;
};
