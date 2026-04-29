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

enum class TabControlAnimationMode
{
	DirectReplace = 0,
	SlideHorizontal = 1,
};

/**
 * @brief TabControl：带标题栏的分页容器。
 *
 * - SelectedIndex 为当前选中页索引（0-based）
 * - Update 内会根据 SelectedIndex 维护各页 Visible，并绘制标题栏
 * - 为兼容 WebBrowser 等“原生子窗口控件”，切换页时会触发一次同步（见 TabControl.cpp）
 */
class TabControl : public Control
{
public:
	virtual UIClass Type();
	D2D1_COLOR_F TitleBackColor = Colors::LightYellow3;
	D2D1_COLOR_F SelectedTitleBackColor = Colors::LightYellow1;
	TabControlAnimationMode AnimationMode = TabControlAnimationMode::DirectReplace;
	/** @brief 当前选中页索引（0-based）。 */
	int SelectedIndex = 0;
	/** @brief 标题栏高度（像素）。 */
	int TitleHeight = 24;
	/** @brief 单个标题宽度（像素）。 */
	int TitleWidth = 120;
	float Boder = 1.5f;
	bool IsAnimationRunning() override;
	UINT GetAnimationIntervalMs() override { return 16; }
	bool GetAnimatedInvalidRect(D2D1_RECT_F& outRect) override;
	READONLY_PROPERTY(int, PageCount);
	GET(int, PageCount);
	READONLY_PROPERTY(std::vector<Control*>&, Pages);
	GET(std::vector<Control*>&, Pages);
	/**
	 * @brief 创建 TabControl。
	 */
	TabControl(int x, int y, int width = 120, int height = 24);
	/**
	 * @brief 新增一个 TabPage。
	 * @return 新建页指针（所有权属于 TabControl）。
	 */
	TabPage* AddPage(std::wstring name);
	/** @brief 插入一个 TabPage，并返回实际插入位置。 */
	int InsertPage(int index, TabPage* page);
	/** @brief 移除指定页；deletePage 为 true 时释放页面对象。 */
	bool RemovePageAt(int index, bool deletePage = true);
	/** @brief 移除指定页；deletePage 为 true 时释放页面对象。 */
	bool RemovePage(TabPage* page, bool deletePage = true);
	/** @brief 清空所有页面。 */
	void ClearPages(bool deletePages = true);
	/** @brief 查找指定标题页索引，未找到返回 -1。 */
	int FindPage(const std::wstring& text) const;
	/** @brief 获取当前选中页。 */
	TabPage* SelectedPage() const;
	/** @brief 程序化选中指定页。 */
	void SelectPage(int index, bool fireEvent = true);
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof) override;

private:
	// 解决“拖动/松开时鼠标移出控件导致事件丢失”的问题：
	// TabControl 需要记住鼠标按下命中的子控件，并在按键按住期间持续转发 mousemove / buttonup。
	Control* _capturedChild = NULL;

	// 记录上一次选择页，用于在 Update 中检测程序切换页并同步原生子窗口控件（如 WebBrowser）
	int _lastSelectIndex = -1;
	int _displayIndex = -1;
	int _animFromIndex = -1;
	int _animToIndex = -1;
	ULONGLONG _animStartTick = 0;
	UINT _animDurationMs = 180;
	float _animProgress = 1.0f;
	bool _animating = false;
	void ClampSelectedIndex();
	void LayoutPage(TabPage* page, int offsetX);
	void SyncPageVisibility();
	void FinishTransition();
	void StartTransitionTo(int newIndex);
	void EnsureSelectionState();
	float CurrentTransitionProgress();
};
