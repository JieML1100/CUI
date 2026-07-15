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
 * - Orientation 决定主轴方向（Horizontal/Vertical）
 * - Spacing 控制相邻子控件之间的间距
 */
class StackLayoutEngine : public LayoutEngine {
private:
    Orientation _orientation = Orientation::Vertical;
    float _spacing = 0.0f;
    HorizontalAlignment _horizontalContentAlignment = HorizontalAlignment::Stretch;
    VerticalAlignment _verticalContentAlignment = VerticalAlignment::Stretch;
    
public:
    /** @brief 设置主轴方向。 */
    void SetOrientation(Orientation value) { 
        _orientation = value; 
        Invalidate(); 
    }
    
    Orientation GetOrientation() const { 
        return _orientation; 
    }
    
    /** @brief 设置子项间距（DIP）。 */
    void SetSpacing(float value) {
        _spacing = std::isfinite(value) && value > 0.0f ? value : 0.0f;
        Invalidate(); 
    }
    
    float GetSpacing() const { 
        return _spacing; 
    }
    
    void SetHorizontalContentAlignment(HorizontalAlignment value) {
        _horizontalContentAlignment = value;
        Invalidate();
    }

    HorizontalAlignment GetHorizontalContentAlignment() const {
        return _horizontalContentAlignment;
    }
    
    void SetVerticalContentAlignment(VerticalAlignment value) {
        _verticalContentAlignment = value;
        Invalidate();
    }

    VerticalAlignment GetVerticalContentAlignment() const {
        return _verticalContentAlignment;
    }
    
    SIZE Measure(Control* container, SIZE availableSize) override;
    void Arrange(Control* container, D2D1_RECT_F finalRect) override;
    cui::core::Size Measure(LayoutContext& context, const cui::core::Constraints& available) override;
    void Arrange(LayoutContext& context, D2D1_RECT_F finalRect) override;
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
    StackPanel(int x, int y, int width, int height);
    virtual ~StackPanel();
    
    UIClass Type() override { return UIClass::UI_StackPanel; }
    void EnsureBindingPropertiesRegistered() override;
    
    /** @brief 设置/获取主轴方向。 */
    void SetOrientation(Orientation value) { 
        _stackEngine->SetOrientation(value);
        InvalidateLayout();
    }
    
    Orientation GetOrientation() const { 
        return _stackEngine->GetOrientation(); 
    }
    
    /** @brief 设置/获取子项间距（DIP）。 */
    void SetSpacing(float value) { 
        _stackEngine->SetSpacing(value);
        InvalidateLayout();
    }
    
    float GetSpacing() const { 
        return _stackEngine->GetSpacing(); 
    }

    /**
     * @brief 控制整个内容带在水平方向的位置。
     * Vertical Stack 中 Stretch 让内容带占满宽度；Horizontal Stack 中
     * Left/Center/Right 控制整行在剩余空间中的位置。
     */
    void SetHorizontalContentAlignment(HorizontalAlignment value) {
        _stackEngine->SetHorizontalContentAlignment(value);
        InvalidateLayout();
    }

    HorizontalAlignment GetHorizontalContentAlignment() const {
        return _stackEngine->GetHorizontalContentAlignment();
    }

    /**
     * @brief 控制整个内容带在垂直方向的位置。
     * Horizontal Stack 中 Stretch 让内容带占满高度；Vertical Stack 中
     * Top/Center/Bottom 控制整列在剩余空间中的位置。
     */
    void SetVerticalContentAlignment(VerticalAlignment value) {
        _stackEngine->SetVerticalContentAlignment(value);
        InvalidateLayout();
    }

    VerticalAlignment GetVerticalContentAlignment() const {
        return _stackEngine->GetVerticalContentAlignment();
    }
};
