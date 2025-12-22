#pragma once
#include "../Panel.h"
#include "LayoutEngine.h"
#include "LayoutTypes.h"

// DockPanel 布局引擎
class DockLayoutEngine : public LayoutEngine {
private:
    bool _lastChildFill = true;
    
public:
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

// DockPanel 控件类
class DockPanel : public Panel {
private:
    DockLayoutEngine* _dockEngine;
    
public:
    DockPanel();
    DockPanel(int x, int y, int width, int height);
    virtual ~DockPanel();
    
    UIClass Type() override { return UIClass::UI_DockPanel; }
    
    // LastChildFill 属性
    void SetLastChildFill(bool value) { 
        _dockEngine->SetLastChildFill(value); 
    }
    
    bool GetLastChildFill() const { 
        return _dockEngine->GetLastChildFill(); 
    }
};
