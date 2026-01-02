#pragma once
#include "Control.h"
#include "Layout/LayoutEngine.h"
#pragma comment(lib, "Imm32.lib")

/**
 * @file Panel.h
 * @brief Panel：通用容器控件（可承载子控件并驱动布局）。
 *
 * Panel 本身继承自 Control，并额外提供布局能力：
 * - 未设置 LayoutEngine 时，使用默认 Anchor/Margin/Align 逻辑（见 Panel.cpp）
 * - 设置 LayoutEngine 后，按 Measure/Arrange 两阶段布局
 *
 * 所有权：SetLayoutEngine 会接管传入指针并负责 delete。
 */

class Panel : public Control
{
protected:
	class LayoutEngine* _layoutEngine = nullptr;
	bool _needsLayout = false;
	
public:
	virtual UIClass Type();
	float Boder = 1.5f;
	Panel();
	Panel(int x, int y, int width, int height);
	virtual ~Panel();
	
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof) override;
	
	// 布局引擎管理
	/**
	 * @brief 设置布局引擎。
	 *
	 * 传入 nullptr 表示恢复为默认布局（Anchor/Margin/Align）。
	 * @param engine 布局引擎指针（由 Panel 接管并在析构/替换时 delete）。
	 */
	void SetLayoutEngine(class LayoutEngine* engine);
	/** @brief 获取当前布局引擎（不转移所有权）。 */
	class LayoutEngine* GetLayoutEngine() { return _layoutEngine; }
	
	// 触发布局
	/**
	 * @brief 立即执行一次布局（必要时会 Measure/Arrange）。
	 *
	 * 通常由 Update 在检测到布局脏标记时调用。
	 */
	void PerformLayout();
	/** @brief 标记布局失效，下一帧重新布局。 */
	void InvalidateLayout();
	
	// 重写 AddControl 以支持布局触发
	template<typename T>
	T AddControl(T c) {
		Control::AddControl(c);
		InvalidateLayout();
		return c;
	}
};