#pragma once
#include "Control.h"
#include "Panel.h"
#pragma comment(lib, "Imm32.lib")

/**
 * @file TabControl.h
 * @brief TabControl/TabPage：分页容器控件。
 *
 * TabControl 自身继承自 Control，通过 Children 管理多个 TabPage。
 * SelectedChanged 事件沿用 Control::OnSelectedChanged。
 */

class TabPage : public Panel
{
public:
	virtual UIClass Type();
	TabPage();
	TabPage(std::wstring text);
};

/**
 * @brief TabControl：带标题栏的分页容器。
 *
 * - SelectIndex 为当前选中页索引（0-based）
 * - Update 内会根据 SelectIndex 维护各页 Visible，并绘制标题栏
 * - 为兼容 WebBrowser 等“原生子窗口控件”，切换页时会触发一次同步（见 TabControl.cpp）
 */
class TabControl : public Control
{
public:
	virtual UIClass Type();
	D2D1_COLOR_F TitleBackColor = Colors::LightYellow3;
	D2D1_COLOR_F SelectedTitleBackColor = Colors::LightYellow1;
	/** @brief 当前选中页索引（0-based）。 */
	int SelectIndex = 0;
	/** @brief 标题栏高度（像素）。 */
	int TitleHeight = 24;
	/** @brief 单个标题宽度（像素）。 */
	int TitleWidth = 120;
	float Boder = 1.5f;
	READONLY_PROPERTY(int, PageCount);
	GET(int, PageCount);
	READONLY_PROPERTY(List<Control*>&, Pages);
	GET(List<Control*>&, Pages);
	/**
	 * @brief 创建 TabControl。
	 */
	TabControl(int x, int y, int width = 120, int height = 24);
	/**
	 * @brief 新增一个 TabPage。
	 * @return 新建页指针（所有权属于 TabControl）。
	 */
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