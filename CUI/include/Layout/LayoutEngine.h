#pragma once
#include "../Control.h"
#include "LayoutTypes.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <span>
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
/**
 * @brief 一次 Measure/Arrange 调用所使用的布局宿主视图。
 *
 * LayoutContext 不拥有控件。Panel 场景可直接包装真实容器；Form 场景则
 * 提供过滤掉菜单/工具栏/状态栏后的显式子项视图。
 */
class LayoutContext final {
private:
    Control* _legacyContainer = nullptr;
    std::span<Control* const> _children {};
    Form* _hostForm = nullptr;
    bool _isWindowRoot = false;

public:
    explicit LayoutContext(Control* container, bool isWindowRoot = false) noexcept
        : _legacyContainer(container),
          _children(container ? container->GetLayoutChildrenView() : std::span<Control* const>{}),
          _hostForm(container ? container->ParentForm : nullptr),
          _isWindowRoot(isWindowRoot)
    {
    }

    LayoutContext(
        Control* legacyContainer,
        std::span<Control* const> children,
        Form* hostForm,
        bool isWindowRoot) noexcept
        : _legacyContainer(legacyContainer),
          _children(children),
          _hostForm(hostForm),
          _isWindowRoot(isWindowRoot)
    {
    }

    [[nodiscard]] int ChildCount() const noexcept
    {
        return static_cast<int>(_children.size());
    }

    [[nodiscard]] Control* ChildAt(int index) const noexcept
    {
        if (index < 0 || index >= ChildCount()) return nullptr;
        return _children[static_cast<std::size_t>(index)];
    }

    [[nodiscard]] Control* LegacyContainer() const noexcept
    {
        return _legacyContainer;
    }

    [[nodiscard]] Form* HostForm() const noexcept { return _hostForm; }
    [[nodiscard]] bool IsWindowRoot() const noexcept { return _isWindowRoot; }
};

class LayoutEngine {
public:
    virtual ~LayoutEngine() = default;
    
    /**
     * @brief 测量阶段：计算容器期望尺寸。
     * @param container 包含子控件的容器。用于 Form 时，这是仅在本次
     * Measure/Arrange 调用期间有效的非拥有根视图；布局引擎不得保存该指针，
     * 也不得通过它增删控件。
     * @param availableSize 可用空间大小（单位：DIP）。
     * @return 容器期望的尺寸（单位：DIP）。
     */
    virtual SIZE Measure(class Control* container, SIZE availableSize)
    {
        (void)container;
        (void)availableSize;
        return SIZE{ 0, 0 };
    }
    
    /**
     * @brief 排列阶段：为子控件计算并应用最终位置/尺寸。
     * @param container 包含子控件的容器；生命周期约束同 Measure。
     * @param finalRect 容器最终矩形区域（容器本地坐标系）。
     */
    virtual void Arrange(class Control* container, D2D1_RECT_F finalRect)
    {
        (void)container;
        (void)finalRect;
    }

    /** @brief 新布局入口；默认转发到旧 Control* 虚函数，兼容现有自定义引擎。 */
    virtual SIZE Measure(LayoutContext& context, SIZE availableSize)
    {
        return Measure(context.LegacyContainer(), availableSize);
    }

    /**
     * @brief Float-DIP measurement entry point used by built-in containers.
     *
     * The default bridge invokes the legacy virtual overload, preserving
     * existing custom LayoutEngine implementations while the complete
     * constraint and result remain floating point for migrated engines.
     */
    virtual cui::core::Size Measure(
        LayoutContext& context,
        const cui::core::Constraints& available)
    {
        const auto maximum = available.Normalized().maximum;
        const auto project = [](cui::core::Dip value)
        {
            if (!(value > 0.0f)) return 0L;
            const auto limit = static_cast<cui::core::Dip>((std::numeric_limits<LONG>::max)());
            return value >= limit
                ? (std::numeric_limits<LONG>::max)()
                : static_cast<LONG>(std::ceil(value));
        };
        const SIZE desired = Measure(context, SIZE{
            project(maximum.width), project(maximum.height) });
        return cui::core::Size{
            static_cast<float>((std::max)(0L, desired.cx)),
            static_cast<float>((std::max)(0L, desired.cy)) };
    }

    /** @brief 新排列入口；默认转发到旧 Control* 虚函数。 */
    virtual void Arrange(LayoutContext& context, D2D1_RECT_F finalRect)
    {
        Arrange(context.LegacyContainer(), finalRect);
    }
    
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
