#pragma once
#include "../Control.h"
#include "LayoutTypes.h"
#include <vector>

// 布局引擎基类
class LayoutEngine {
public:
    virtual ~LayoutEngine() = default;
    
    // 测量阶段：计算控件期望尺寸
    // container: 包含子控件的容器
    // availableSize: 可用空间大小
    // 返回: 期望的尺寸
    virtual SIZE Measure(class Control* container, SIZE availableSize) = 0;
    
    // 排列阶段：设置子控件的实际位置和尺寸
    // container: 包含子控件的容器
    // finalRect: 容器的最终矩形区域
    virtual void Arrange(class Control* container, D2D1_RECT_F finalRect) = 0;
    
    // 布局失效，需要重新布局
    virtual void Invalidate() { 
        _needsLayout = true; 
    }
    
    // 检查是否需要布局
    bool NeedsLayout() const { 
        return _needsLayout; 
    }
    
protected:
    bool _needsLayout = true;
};
