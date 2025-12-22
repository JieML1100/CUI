#pragma once
#include "../Panel.h"
#include "LayoutEngine.h"
#include "LayoutTypes.h"
#include <algorithm>

// StackPanel 布局引擎
class StackLayoutEngine : public LayoutEngine {
private:
    Orientation _orientation = Orientation::Vertical;
    float _spacing = 0.0f;
    HorizontalAlignment _horizontalContentAlignment = HorizontalAlignment::Stretch;
    VerticalAlignment _verticalContentAlignment = VerticalAlignment::Stretch;
    
public:
    void SetOrientation(Orientation value) { 
        _orientation = value; 
        Invalidate(); 
    }
    
    Orientation GetOrientation() const { 
        return _orientation; 
    }
    
    void SetSpacing(float value) { 
        _spacing = value; 
        Invalidate(); 
    }
    
    float GetSpacing() const { 
        return _spacing; 
    }
    
    void SetHorizontalContentAlignment(HorizontalAlignment value) {
        _horizontalContentAlignment = value;
        Invalidate();
    }
    
    void SetVerticalContentAlignment(VerticalAlignment value) {
        _verticalContentAlignment = value;
        Invalidate();
    }
    
    SIZE Measure(Control* container, SIZE availableSize) override;
    void Arrange(Control* container, D2D1_RECT_F finalRect) override;
};

// StackPanel 控件类
class StackPanel : public Panel {
private:
    StackLayoutEngine* _stackEngine;
    
public:
    StackPanel();
    StackPanel(int x, int y, int width, int height);
    virtual ~StackPanel();
    
    UIClass Type() override { return UIClass::UI_StackPanel; }
    
    // Orientation 属性
    void SetOrientation(Orientation value) { 
        _stackEngine->SetOrientation(value); 
    }
    
    Orientation GetOrientation() const { 
        return _stackEngine->GetOrientation(); 
    }
    
    // Spacing 属性
    void SetSpacing(float value) { 
        _stackEngine->SetSpacing(value); 
    }
    
    float GetSpacing() const { 
        return _stackEngine->GetSpacing(); 
    }
};
