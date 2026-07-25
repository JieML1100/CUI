#pragma once
#include "Control.h"
#include "Layout/LayoutEngine.h"
#include <algorithm>
#include <cmath>
#include <utility>

/**
 * @file Panel.h
 * @brief WPF-style Panel behavior host for multi-child layout.
 *
 * The native behavior host shares the common element storage implementation;
 * its authoritative XAML type derives from FrameworkElement and owns only
 * Panel.Background. In particular, Panel has no Padding, border chrome or
 * ControlTemplate.
 *
 * Panel provides layout behavior:
 * - 默认使用 Canvas.Left/Top/Right/Bottom 附加值定位
 * - 派生 Panel 通过内部 LayoutEngine 实现各自 Measure/Arrange 策略
 */

class Panel : public Control
{
protected:
	friend class Control;
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override
	{
		return std::make_unique<AutomationPeer>(
			*this, AutomationControlType::Pane, L"Panel");
	}
	std::unique_ptr<class LayoutEngine> _layoutEngine;
	bool _needsMeasure = true;
	bool _needsArrange = true;
	void RequestLayout() override;
	void OnComputedLayoutSizeChanged() override;
	void PerformPendingLayout() override;
	cui::core::Size MeasureCore(const cui::core::Constraints& available) override;
	/** Installs the private layout policy owned by this Panel subtype. */
	void SetLayoutEngine(class LayoutEngine* engine);
	/** Invalidates only the panel's child-arrangement policy. */
	void InvalidateArrangeLayout();
	void PerformLayout();
public:
	virtual UIClass Type();
	static void RegisterDependencyProperties();
	void EnsureBindingPropertiesRegistered() override { RegisterDependencyProperties(); }

	Panel();
	virtual ~Panel();
	void Arrange(cui::core::Rect finalRect) override;
	
protected:
	void OnRender() override;
	
public:
	/** @brief 标记布局失效，下一帧重新布局。 */
	void InvalidateLayout();
	
};
