#pragma once
#include "../Control.h"
#include "LayoutTypes.h"
#include <vector>

/**
 * @file LayoutEngine.h
 * @brief 布局引擎接口定义。
 *
 * 布局通常分两阶段：
 * - Measure：测量阶段，计算容器/子控件所需尺寸
 * - Arrange：排列阶段，确定每个子控件最终的位置与尺寸
 */

/**
 * @brief 布局引擎基类。
 *
 * LayoutEngine 是纯逻辑组件，通常由容器（如 Panel/Form）持有并在需要时触发。
 */
class LayoutEngine {
public:
    virtual ~LayoutEngine() = default;
    
    /**
     * @brief 测量阶段：计算容器期望尺寸。
     * @param container 包含子控件的容器。
     * @param availableSize 可用空间大小（单位：像素）。
     * @return 容器期望的尺寸（单位：像素）。
     */
    virtual SIZE Measure(class Control* container, SIZE availableSize) = 0;
    
    /**
     * @brief 排列阶段：为子控件计算并应用最终位置/尺寸。
     * @param container 包含子控件的容器。
     * @param finalRect 容器最终矩形区域（容器本地坐标系）。
     */
    virtual void Arrange(class Control* container, D2D1_RECT_F finalRect) = 0;
    
    /** @brief 标记布局失效，需要重新布局。 */
    virtual void Invalidate() { 
        _needsLayout = true; 
    }
    
    /** @brief 检查是否需要重新布局。 */
    bool NeedsLayout() const { 
        return _needsLayout; 
    }
    
protected:
    bool _needsLayout = true;
};
