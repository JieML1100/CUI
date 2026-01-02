#pragma once
#include "Control.h"
#pragma comment(lib, "Imm32.lib")

/**
 * @file ComboBox.h
 * @brief ComboBox：下拉选择控件（支持滚动、展开/收起）。
 *
 * 说明：
 * - Items 为下拉项列表（std::wstring）
 * - SelectedIndex 为当前选中项索引
 * - Expand=true 表示展开下拉面板
 * - 展开后的高度与 ExpandCount/ExpandScroll 相关（见实现）
 */
class ComboBox : public Control
{
#define COMBO_MIN_SCROLL_BLOCK 16
private:
	int _underMouseIndex = -1;
	bool isDraggingScroll = false;
	float _scrollThumbGrabOffsetY = 0.0f;
	void UpdateScrollDrag(float posY);
	List<std::wstring> values;
public:
	virtual UIClass Type();
	CursorKind QueryCursor(int xof, int yof) override;
	D2D1_COLOR_F UnderMouseBackColor = Colors::SkyBlue;
	D2D1_COLOR_F UnderMouseForeColor = Colors::White;
	D2D1_COLOR_F ScrollBackColor = Colors::LightGray;
	D2D1_COLOR_F ScrollForeColor = Colors::DimGrey;
	D2D1_COLOR_F ButtonBackColor = Colors::SkyBlue;
	/** @brief 选择变化事件。 */
	SelectionChangedEvent OnSelectionChanged;
	/** @brief 展开状态下最多显示的条目数量（不含滚动）。 */
	int ExpandCount = 4;
	/** @brief 展开状态下的滚动偏移（按条目计）。 */
	int ExpandScroll = 0;
	/** @brief 是否展开下拉面板。 */
	bool Expand = false;
	/** @brief 当前选中索引（0-based）。 */
	int SelectedIndex = 0;
	PROPERTY(List<std::wstring>&, Items);
	GET(List<std::wstring>&, Items);
	SET(List<std::wstring>&, Items);
	float Boder = 1.5f;
	/** @brief 创建 ComboBox。 */
	ComboBox(std::wstring text, int x, int y, int width = 120, int height = 24);
	SIZE ActualSize() override;
	void DrawScroll();
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof) override;
};
