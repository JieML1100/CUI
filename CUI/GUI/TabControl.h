#pragma once
#include "Control.h"
#include "Panel.h"
#pragma comment(lib, "Imm32.lib")
class TabPage : public Panel
{
public:
	virtual UIClass Type();
	TabPage();
	TabPage(std::wstring text);
};
class TabControl : public Control
{
public:
	virtual UIClass Type();
	D2D1_COLOR_F TitleBackColor = Colors::LightYellow3;
	D2D1_COLOR_F SelectedTitleBackColor = Colors::LightYellow1;
	int SelectIndex = 0;
	int TitleHeight = 24;
	int TitleWidth = 120;
	float Boder = 1.5f;
	READONLY_PROPERTY(int, PageCount);
	GET(int, PageCount);
	READONLY_PROPERTY(List<Control*>&, Pages);
	GET(List<Control*>&, Pages);
	TabControl(int x, int y, int width = 120, int height = 24);
	TabPage* AddPage(std::wstring name);
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof) override;

private:
	// 解决“拖动/松开时鼠标移出控件导致事件丢失”的问题：
	// TabControl 需要记住鼠标按下命中的子控件，并在按键按住期间持续转发 mousemove / buttonup。
	Control* _capturedChild = NULL;

	// 记录上一次选择页，用于在 Update 中检测程序切换页并同步原生子窗口控件（如 WebBrowser）
	int _lastSelectIndex = -1;
};