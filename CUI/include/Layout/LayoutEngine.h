#pragma once
#include "Control.h"
#include "LayoutTypes.h"
#include <span>

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
 * LayoutEngine 是纯逻辑组件，通常由容器（如 Panel/Window）持有并在需要时触发。
 */
/**
 * @brief 一次 Measure/Arrange 调用所使用的布局宿主视图。
 *
 * LayoutContext 不拥有控件。Panel 场景可直接包装真实容器；Window 场景则
 * 提供过滤掉菜单/工具栏/状态栏后的显式子项视图。
 */
class LayoutContext final {
private:
    Control* _owner = nullptr;
    std::span<Control* const> _children {};
    Window* _hostWindow = nullptr;
    bool _isWindowRoot = false;

public:
    explicit LayoutContext(Control* container, bool isWindowRoot = false) noexcept
        : _owner(container),
          _children(container ? container->GetLayoutChildrenView() : std::span<Control* const>{}),
          _hostWindow(container ? container->GetPresentationWindow() : nullptr),
          _isWindowRoot(isWindowRoot)
    {
    }

    LayoutContext(
        Control* owner,
        std::span<Control* const> children,
        Window* hostWindow,
        bool isWindowRoot) noexcept
        : _owner(owner),
          _children(children),
          _hostWindow(hostWindow),
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

    [[nodiscard]] Control* Owner() const noexcept { return _owner; }

    [[nodiscard]] Window* HostWindow() const noexcept { return _hostWindow; }
    [[nodiscard]] bool IsWindowRoot() const noexcept { return _isWindowRoot; }
};

namespace cui::layout
{
    /**
     * WPF FrameworkElement treats an explicit Width/Height as stronger than
     * Stretch.  The effective alignment for that axis becomes centered while
     * the declared alignment value itself remains Stretch.
     *
     * Keep this resolution in the shared layout contract so StackPanel, Grid,
     * DockPanel and content-slot layout cannot drift into different rules.
     */
    [[nodiscard]] inline HorizontalAlignment ResolveHorizontalArrangeAlignment(
        Control& child) noexcept
    {
        const auto alignment = child.HorizontalAlignment;
        return alignment == HorizontalAlignment::Stretch
            && child.GetSpecifiedLayout().width.IsFixed()
            ? HorizontalAlignment::Center
            : alignment;
    }

    [[nodiscard]] inline VerticalAlignment ResolveVerticalArrangeAlignment(
        Control& child) noexcept
    {
        const auto alignment = child.VerticalAlignment;
        return alignment == VerticalAlignment::Stretch
            && child.GetSpecifiedLayout().height.IsFixed()
            ? VerticalAlignment::Center
            : alignment;
    }
}

class LayoutEngine {
public:
    virtual ~LayoutEngine() = default;

    /** Float-DIP Measure contract. Layout engines never receive Win32 SIZEs. */
    virtual cui::core::Size Measure(
        LayoutContext& context,
        const cui::core::Constraints& available) = 0;

    /** Float-DIP Arrange contract in the owner's local coordinate space. */
    virtual void Arrange(
        LayoutContext& context,
        cui::core::Rect finalRect) = 0;
    
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
