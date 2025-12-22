#pragma once
#include "Control.h"
#include "Layout/LayoutEngine.h"
#pragma comment(lib, "Imm32.lib")

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
	void SetLayoutEngine(class LayoutEngine* engine);
	class LayoutEngine* GetLayoutEngine() { return _layoutEngine; }
	
	// 触发布局
	void PerformLayout();
	void InvalidateLayout();
	
	// 重写 AddControl 以支持布局触发
	template<typename T>
	T AddControl(T c) {
		Control::AddControl(c);
		InvalidateLayout();
		return c;
	}
};