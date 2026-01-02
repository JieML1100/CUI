#pragma once
#include "../Panel.h"
#include "LayoutEngine.h"
#include "LayoutTypes.h"

/**
 * @file DockPanel.h
 * @brief DockPanel：按 Dock 方向停靠子控件的容器。
 */

/**
 * @brief DockPanel 布局引擎。
 *
 * 子控件通过 Control::DockPosition 指定停靠方向。
 * LastChildFill=true 时，最后一个子控件会占用剩余空间。
 */
class DockLayoutEngine : public LayoutEngine {
private:
    bool _lastChildFill = true;
    
public:
    /** @brief 设置最后一个子控件是否填充剩余空间。 */
    void SetLastChildFill(bool value) { 
        _lastChildFill = value; 
        Invalidate(); 
    }
    
    bool GetLastChildFill() const { 
        return _lastChildFill; 
    }
    
    SIZE Measure(Control* container, SIZE availableSize) override;
    void Arrange(Control* container, D2D1_RECT_F finalRect) override;
};

/**
 * @brief DockPanel 控件类。
 */
class DockPanel : public Panel {
private:
    DockLayoutEngine* _dockEngine;
    
public:
    DockPanel();
    DockPanel(int x, int y, int width, int height);
    virtual ~DockPanel();
    
    UIClass Type() override { return UIClass::UI_DockPanel; }
    
    /** @brief 设置/获取 LastChildFill。 */
    void SetLastChildFill(bool value) { 
        _dockEngine->SetLastChildFill(value); 
    }
    
    bool GetLastChildFill() const { 
        return _dockEngine->GetLastChildFill(); 
    }
};
