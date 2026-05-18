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
	float _dropProgress = 0.0f;
	float _animStartProgress = 0.0f;
	float _animTargetProgress = 0.0f;
	ULONGLONG _animStartTick = 0;
	UINT _animDurationMs = 180;
	bool _animating = false;
	bool _collapseCleanupPending = false;
	void UpdateScrollDrag(float posY);
	int VisibleItemCount();
	float FullDropdownHeight();
	float CurrentDropProgress();
	float CurrentDropdownHeight();
	bool IsDropDownVisible();
	bool IsDropDownInteractive();
	bool IsHeaderHit(int xof, int yof);
	bool IsDropdownHit(int xof, int yof, float dropdownHeight);
	float DropdownTop();
	void EnsureSelectionInRange();
	void EnsureScrollInRange();
	std::vector<std::wstring> values;
public:
	virtual UIClass Type();
	CursorKind QueryCursor(int xof, int yof) override;
	bool AutoCloseOnOutsideClick() const override { return true; }
	bool AutoCloseOnFormFocusLoss() const override { return true; }
	void ClosePopup() override { SetExpanded(false); }
	bool HandlesMouseWheel() const override { return true; }
	bool CanHandleMouseWheel(int delta, int xof, int yof) override;
	bool IsAnimationRunning() override;
	UINT GetAnimationIntervalMs() override { return 16; }
	bool GetAnimatedInvalidRect(D2D1_RECT_F& outRect) override;
	float CornerRadius = 6.0f;
	float DropCornerRadius = 7.0f;
	float DropGap = 4.0f;
	float ItemHorizontalPadding = 10.0f;
	float ItemVerticalPadding = 3.0f;
	float ChevronSize = 10.0f;
	float ScrollBarWidth = 6.0f;
	D2D1_COLOR_F AccentColor = { 0.3882f, 0.4000f, 0.9451f, 1.0f };
	D2D1_COLOR_F HeaderHoverBackColor = { 0.3882f, 0.4000f, 0.9451f, 0.06f };
	D2D1_COLOR_F DropBackColor = { 1.0f, 1.0f, 1.0f, 0.98f };
	D2D1_COLOR_F DropBorderColor = { 0.74f, 0.77f, 0.84f, 0.95f };
	D2D1_COLOR_F SelectedItemBackColor = { 0.3882f, 0.4000f, 0.9451f, 0.14f };
	D2D1_COLOR_F UnderMouseBackColor = { 0.3882f, 0.4000f, 0.9451f, 0.09f };
	D2D1_COLOR_F SelectedItemForeColor = Colors::Black;
	D2D1_COLOR_F UnderMouseForeColor = Colors::Black;
	D2D1_COLOR_F ScrollBackColor = Colors::LightGray;
	D2D1_COLOR_F ScrollForeColor = Colors::DimGrey;
	D2D1_COLOR_F ButtonBackColor = Colors::SkyBlue;
	/** @brief 选择变化事件。 */
	SelectionChangedEvent OnSelectionChanged;
	/** @brief 下拉状态下最多显示的条目数量。实际可见项数会被 Items.Count 截断。 */
	int ExpandCount = 4;
	/** @brief 展开状态下的滚动偏移（按条目计）。 */
	int ExpandScroll = 0;
	/** @brief 是否展开下拉面板。 */
	bool Expand = false;
	/** @brief 当前选中索引（0-based）。 */
	int SelectedIndex = 0;
	PROPERTY(std::vector<std::wstring>&, Items);
	GET(std::vector<std::wstring>&, Items);
	SET(std::vector<std::wstring>&, Items);
	float Boder = 1.5f;
	/** @brief 创建 ComboBox。 */
	ComboBox(std::wstring text, int x, int y, int width = 120, int height = 24);
	void SetExpanded(bool expanded);
	SIZE ActualSize() override;
	void DrawScroll();
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof) override;
};
