#pragma once
#include "Control.h"
#include "ObservableCollection.h"
#include <unordered_map>
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
class ComboBox : public Control, public IAccessibilityVirtualizedControl
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
	int _animationDurationMs = 180;
	bool _animating = false;
	bool _collapseCleanupPending = false;
	int _expandCount = 4;
	int _expandScroll = 0;
	bool _expand = false;
	int _selectedIndex = 0;
	float _cornerRadius = 6.0f;
	float _dropCornerRadius = 7.0f;
	float _dropGap = 4.0f;
	float _itemHorizontalPadding = 10.0f;
	float _itemVerticalPadding = 3.0f;
	float _chevronSize = 10.0f;
	float _scrollBarWidth = 6.0f;
	float _borderThickness = 1.5f;
	D2D1_COLOR_F _accentColor = cui::theme::palette::Accent;
	D2D1_COLOR_F _headerHoverBackColor = cui::theme::palette::AccentSoft;
	D2D1_COLOR_F _dropBackColor = cui::theme::palette::Surface;
	D2D1_COLOR_F _dropBorderColor = cui::theme::palette::Border;
	D2D1_COLOR_F _selectedItemBackColor = cui::theme::palette::AccentSelected;
	D2D1_COLOR_F _underMouseBackColor = cui::theme::palette::AccentSoft;
	D2D1_COLOR_F _selectedItemForeColor = cui::theme::palette::TextPrimary;
	D2D1_COLOR_F _underMouseForeColor = cui::theme::palette::TextPrimary;
	D2D1_COLOR_F _scrollBackColor = cui::theme::palette::ScrollTrack;
	D2D1_COLOR_F _scrollForeColor = cui::theme::palette::ScrollThumb;
	D2D1_COLOR_F _buttonBackColor = cui::theme::palette::SurfaceMuted;
	void UpdateScrollDrag(float posY);
	int VisibleItemCount();
	float FullDropdownHeight();
	float CurrentDropProgress();
	float CurrentDropdownHeight();
	bool IsDropDownVisible();
	bool IsDropDownInteractive();
	bool IsHeaderHit(int localX, int localY);
	bool IsDropdownHit(int localX, int localY, float dropdownHeight);
	float DropdownTop();
	void EnsureSelectionInRange();
	void EnsureScrollInRange();
	void EnsureSelectedItemVisible();
	void SyncTextWithSelection();
	void ApplySelectedIndexChange(int oldValue, int newValue);
	void ApplyExpandedStateChange(bool oldValue, bool newValue);
	void SetCurrentSelectedIndex(int value);
	void SetCurrentExpandScroll(int value);
	void SetCurrentExpanded(bool value);
	ObservableCollection<std::wstring> values;
	std::vector<uint32_t> _accessibilityItemIds;
	std::vector<std::wstring> _accessibilityItemTexts;
	std::unordered_map<uint32_t, size_t> _accessibilityItemIndexById;
	uint32_t _selectedAccessibilityItemId = 0;
	void OnItemsCollectionChanged(const CollectionChangedEventArgs& change);
	void ReconcileAccessibilityItemIds();
	void RebuildAccessibilityItemIndex();
	int FindAccessibilityItem(uint32_t id);
public:
	virtual UIClass Type();
	void EnsureBindingPropertiesRegistered() override;
	CursorKind QueryCursor(int localX, int localY) override;
	bool AutoCloseOnOutsideClick() const override { return true; }
	bool AutoCloseOnFormFocusLoss() const override { return true; }
	void ClosePopup() override { SetExpanded(false); }
	bool HandlesMouseWheel() const override { return true; }
	bool CanHandleMouseWheel(int delta, int localX, int localY) override;
	bool HandlesNavigationKey(WPARAM key) const override;
	bool IsAnimationRunning() override;
	UINT GetAnimationIntervalMs() override { return 16; }
	bool GetAnimatedInvalidRect(D2D1_RECT_F& outRect) override;
	bool ContainsForegroundPoint(int localX, int localY) override;
	bool RenderNormalWhenForeground() const override { return true; }
	void InvalidateVisual() override;
	PROPERTY(float, CornerRadius); GET(float, CornerRadius); SET(float, CornerRadius);
	PROPERTY(float, DropCornerRadius); GET(float, DropCornerRadius); SET(float, DropCornerRadius);
	PROPERTY(float, DropGap); GET(float, DropGap); SET(float, DropGap);
	PROPERTY(float, ItemHorizontalPadding); GET(float, ItemHorizontalPadding); SET(float, ItemHorizontalPadding);
	PROPERTY(float, ItemVerticalPadding); GET(float, ItemVerticalPadding); SET(float, ItemVerticalPadding);
	PROPERTY(float, ChevronSize); GET(float, ChevronSize); SET(float, ChevronSize);
	PROPERTY(float, ScrollBarWidth); GET(float, ScrollBarWidth); SET(float, ScrollBarWidth);
	PROPERTY(D2D1_COLOR_F, AccentColor); GET(D2D1_COLOR_F, AccentColor); SET(D2D1_COLOR_F, AccentColor);
	PROPERTY(D2D1_COLOR_F, HeaderHoverBackColor); GET(D2D1_COLOR_F, HeaderHoverBackColor); SET(D2D1_COLOR_F, HeaderHoverBackColor);
	PROPERTY(D2D1_COLOR_F, DropBackColor); GET(D2D1_COLOR_F, DropBackColor); SET(D2D1_COLOR_F, DropBackColor);
	PROPERTY(D2D1_COLOR_F, DropBorderColor); GET(D2D1_COLOR_F, DropBorderColor); SET(D2D1_COLOR_F, DropBorderColor);
	PROPERTY(D2D1_COLOR_F, SelectedItemBackColor); GET(D2D1_COLOR_F, SelectedItemBackColor); SET(D2D1_COLOR_F, SelectedItemBackColor);
	PROPERTY(D2D1_COLOR_F, UnderMouseBackColor); GET(D2D1_COLOR_F, UnderMouseBackColor); SET(D2D1_COLOR_F, UnderMouseBackColor);
	PROPERTY(D2D1_COLOR_F, SelectedItemForeColor); GET(D2D1_COLOR_F, SelectedItemForeColor); SET(D2D1_COLOR_F, SelectedItemForeColor);
	PROPERTY(D2D1_COLOR_F, UnderMouseForeColor); GET(D2D1_COLOR_F, UnderMouseForeColor); SET(D2D1_COLOR_F, UnderMouseForeColor);
	PROPERTY(D2D1_COLOR_F, ScrollBackColor); GET(D2D1_COLOR_F, ScrollBackColor); SET(D2D1_COLOR_F, ScrollBackColor);
	PROPERTY(D2D1_COLOR_F, ScrollForeColor); GET(D2D1_COLOR_F, ScrollForeColor); SET(D2D1_COLOR_F, ScrollForeColor);
	PROPERTY(D2D1_COLOR_F, ButtonBackColor); GET(D2D1_COLOR_F, ButtonBackColor); SET(D2D1_COLOR_F, ButtonBackColor);
	/** @brief 选择变化事件。 */
	SelectionChangedEvent OnSelectionChanged;
	/** @brief 下拉状态下最多显示的条目数量。实际可见项数会被 Items.Count 截断。 */
	PROPERTY(int, ExpandCount); GET(int, ExpandCount); SET(int, ExpandCount);
	/** @brief 展开状态下的滚动偏移（按条目计）。 */
	PROPERTY(int, ExpandScroll); GET(int, ExpandScroll); SET(int, ExpandScroll);
	/** @brief 是否展开下拉面板。 */
	PROPERTY(bool, Expand); GET(bool, Expand); SET(bool, Expand);
	/** @brief 当前选中索引（0-based）。 */
	PROPERTY(int, SelectedIndex); GET(int, SelectedIndex); SET(int, SelectedIndex);
	/** @brief 展开/收起动画时长（毫秒，0 表示禁用动画）。 */
	PROPERTY(UINT, AnimationDurationMs); GET(UINT, AnimationDurationMs); SET(UINT, AnimationDurationMs);
	using ItemCollection = ObservableCollection<std::wstring>;
	__declspec(property(put = SetItems, get = GetItems)) ItemCollection& Items;
	ItemCollection& GetItems();
	void SetItems(const std::vector<std::wstring>& value);
	PROPERTY(float, BorderThickness); GET(float, BorderThickness); SET(float, BorderThickness);
	/** @brief 创建 ComboBox。 */
	ComboBox(std::wstring text, int x, int y, int width = 120, int height = 24);
	void SetExpanded(bool expanded);
	/** @brief 通过交互语义选择条目，并保留现有 Binding 值来源。 */
	bool SelectItem(int index);
	/** @brief 通过交互语义滚动指定条目数，并保留现有 Binding 值来源。 */
	void ScrollBy(int itemDelta);
	void GetAccessibilityVirtualChildren(
		uint32_t parentId, std::vector<uint32_t>& result) override;
	bool TryGetAccessibilityVirtualNode(
		uint32_t id, AccessibilityVirtualNode& result) override;
	size_t GetAccessibilityVirtualChildCount(uint32_t parentId) override;
	bool TryGetAccessibilityVirtualChildAt(
		uint32_t parentId, size_t index, uint32_t& result) override;
	bool TryGetAccessibilityVirtualSibling(
		uint32_t parentId, uint32_t id, bool next, uint32_t& result) override;
	bool TryHitTestAccessibilityVirtualNode(
		float localX, float localY, uint32_t& result) override;
	AccessibilityVirtualContainerInfo
		GetAccessibilityVirtualContainerInfo() const noexcept override;
	void GetAccessibilityVirtualSelection(
		std::vector<uint32_t>& result) override;
	bool SelectAccessibilityVirtualNode(
		uint32_t id, AccessibilitySelectionAction action) override;
	bool ScrollAccessibilityVirtualNodeIntoView(uint32_t id) override;
	bool GetAccessibilityScrollInfo(
		AccessibilityScrollInfo& result) const noexcept override;
	bool ScrollAccessibility(
		AccessibilityScrollAmount horizontal,
		AccessibilityScrollAmount vertical) override;
	bool SetAccessibilityScrollPercent(
		double horizontalPercent, double verticalPercent) override;
	SIZE ActualSize() override;
	void DrawScroll();
	void Update() override;
	void UpdateForeground() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY) override;
};
