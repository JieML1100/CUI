#pragma once
#include "../Panel.h"
#include "LayoutEngine.h"
#include "LayoutTypes.h"

/**
 * @file WrapPanel.h
 * @brief WrapPanel：按行/列自动换行(换列)排列子控件的容器。
 */

/**
 * @brief WrapPanel 布局引擎。
 *
 * Orientation=Horizontal：按行从左到右排列，空间不足则换到下一行。
 * Orientation=Vertical：按列从上到下排列，空间不足则换到下一列。
 * ItemWidth/ItemHeight 为 0 时使用子控件自身测量尺寸。
 */
class WrapLayoutEngine : public LayoutEngine {
private:
    Orientation _orientation = Orientation::Horizontal;
    float _itemWidth = 0.0f;   // 0表示使用控件自身宽度
    float _itemHeight = 0.0f;  // 0表示使用控件自身高度
    
public:
    /** @brief 设置换行方向（主轴）。 */
    void SetOrientation(Orientation value) {
        _orientation = value;
        Invalidate();
    }
    
    Orientation GetOrientation() const {
        return _orientation;
    }
    
    /**
     * @brief 设置统一子项宽度（像素）。
     * 0 表示使用子控件自身宽度。
     */
    void SetItemWidth(float value) {
        _itemWidth = value;
        Invalidate();
    }
    
    float GetItemWidth() const {
        return _itemWidth;
    }
    
    /**
     * @brief 设置统一子项高度（像素）。
     * 0 表示使用子控件自身高度。
     */
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

/**
 * @brief WrapPanel 控件类。
 */
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
