#pragma once
#include "../Panel.h"
#include "LayoutEngine.h"
#include "LayoutTypes.h"
#include <algorithm>
#include <cmath>

/**
 * @file StackPanel.h
 * @brief StackPanel：按主轴方向依次堆叠子控件的容器。
 */

/**
 * @brief StackPanel 布局引擎。
 *
 * Orientation 决定主轴方向（Horizontal/Vertical）。与 WPF 一致，
 * 间距和交叉轴对齐由子元素自身的 Margin/Alignment 表达。
 */
class StackLayoutEngine : public LayoutEngine {
private:
    Orientation _orientation = Orientation::Vertical;
    
public:
    /** @brief 设置主轴方向。 */
    void SetOrientation(Orientation value) { 
        _orientation = value; 
        Invalidate(); 
    }
    
    Orientation GetOrientation() const { 
        return _orientation; 
    }
    
    cui::core::Size Measure(LayoutContext& context, const cui::core::Constraints& available) override;
    void Arrange(LayoutContext& context, cui::core::Rect finalRect) override;
};

/**
 * @brief StackPanel 控件类。
 *
 * 作为 Panel 的一种，实现“线性布局”。子控件的 Margin/Padding/对齐等规则由布局引擎解释。
 */
class StackPanel : public Panel {
private:
    StackLayoutEngine* _stackEngine;
    
public:
	StackPanel();
    virtual ~StackPanel();
    
    UIClass Type() override { return UIClass::UI_StackPanel; }
    static void RegisterDependencyProperties();
    void EnsureBindingPropertiesRegistered() override { RegisterDependencyProperties(); }
    
    /** @brief 设置/获取主轴方向。 */
    void SetOrientation(Orientation value) { 
        _stackEngine->SetOrientation(value);
        InvalidateLayout();
    }
    
    Orientation GetOrientation() const { 
        return _stackEngine->GetOrientation(); 
    }
    
};
