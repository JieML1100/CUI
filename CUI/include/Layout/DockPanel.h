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
 * 子控件通过 DockPanel.Dock attached property 指定停靠方向。
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
    
    cui::core::Size Measure(LayoutContext& context, const cui::core::Constraints& available) override;
    void Arrange(LayoutContext& context, cui::core::Rect finalRect) override;
};

/**
 * @brief DockPanel 控件类。
 */
class DockPanel : public Panel {
private:
    DockLayoutEngine* _dockEngine;
    bool _lastChildFill = true;
    
public:
	DockPanel();
    virtual ~DockPanel();
    
	UIClass Type() override { return UIClass::UI_DockPanel; }
	static Dock GetDock(Control& element) noexcept
	{
		return element.GetDockPosition();
	}
	static void SetDock(Control& element, Dock value)
	{
		element.SetDockPosition(value);
	}
	static void RegisterDependencyProperties();
    void EnsureBindingPropertiesRegistered() override { RegisterDependencyProperties(); }
    
    /** @brief 设置/获取 LastChildFill。 */
    void SetLastChildFill(bool value);
    
    bool GetLastChildFill() const { return _lastChildFill; }
};
