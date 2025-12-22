#pragma once
#include "../Panel.h"
#include "LayoutEngine.h"
#include "LayoutTypes.h"

// WrapPanel 布局引擎
class WrapLayoutEngine : public LayoutEngine {
private:
    Orientation _orientation = Orientation::Horizontal;
    float _itemWidth = 0.0f;   // 0表示使用控件自身宽度
    float _itemHeight = 0.0f;  // 0表示使用控件自身高度
    
public:
    void SetOrientation(Orientation value) {
        _orientation = value;
        Invalidate();
    }
    
    Orientation GetOrientation() const {
        return _orientation;
    }
    
    void SetItemWidth(float value) {
        _itemWidth = value;
        Invalidate();
    }
    
    float GetItemWidth() const {
        return _itemWidth;
    }
    
    void SetItemHeight(float value) {
        _itemHeight = value;
        Invalidate();
    }
    
    float GetItemHeight() const {
        return _itemHeight;
    }
    
    SIZE Measure(Control* container, SIZE availableSize) override;
    void Arrange(Control* container, D2D1_RECT_F finalRect) override;
};

// WrapPanel 控件类
class WrapPanel : public Panel {
private:
    WrapLayoutEngine* _wrapEngine;
    
public:
    WrapPanel();
    WrapPanel(int x, int y, int width, int height);
    virtual ~WrapPanel();
    
    UIClass Type() override { return UIClass::UI_WrapPanel; }
    
    void SetOrientation(Orientation value) {
        _wrapEngine->SetOrientation(value);
    }
    
    Orientation GetOrientation() const {
        return _wrapEngine->GetOrientation();
    }
    
    void SetItemWidth(float value) {
        _wrapEngine->SetItemWidth(value);
    }
    
    float GetItemWidth() const {
        return _wrapEngine->GetItemWidth();
    }
    
    void SetItemHeight(float value) {
        _wrapEngine->SetItemHeight(value);
    }
    
    float GetItemHeight() const {
        return _wrapEngine->GetItemHeight();
    }
};
