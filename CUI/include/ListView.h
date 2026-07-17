#pragma once
#include "Control.h"
#include "ObservableCollection.h"
#include <unordered_map>
#include <unordered_set>

/**
 * @file ListView.h
 * @brief ListView：列表控件（列表、图标、磁贴、明细列、选择、滚动）。
 */

enum class ListViewViewMode
{
	List,
	Details,
	Tile,
	Icon
};

enum class ListViewSelectionMode
{
	Single,
	Multiple
};

enum class ListViewCellAlign
{
	Left,
	Center,
	Right
};

struct ListViewColumn
{
	/** Stable identity used by UI Automation in Details view. */
	mutable uint32_t AccessibilityId = 0;
	std::wstring Header;
	float Width = 120.0f;
	ListViewCellAlign Align = ListViewCellAlign::Left;

	ListViewColumn() = default;
	ListViewColumn(std::wstring header, float width = 120.0f, ListViewCellAlign align = ListViewCellAlign::Left);
};

class ListViewItem
{
public:
	/** Stable identity used by UI Automation; duplicate copies are repaired lazily. */
	mutable uint32_t AccessibilityId = 0;
	std::wstring Text;
	std::wstring SubText;
	std::vector<std::wstring> SubItems;
	std::shared_ptr<BitmapSource> Image;
	Microsoft::WRL::ComPtr<ID2D1Bitmap> ImageCache;
	ID2D1RenderTarget* ImageCacheTarget = nullptr;
	const BitmapSource* ImageCacheSource = nullptr;
	UINT64 Tag = 0;
	bool Checked = false;
	/** Compatibility flag; call owner.Items.NotifyReset() after direct edits. */
	bool Selected = false;
	bool Enabled = true;

	ListViewItem() = default;
	explicit ListViewItem(std::wstring text);
	ListViewItem(std::wstring text, std::wstring subText);
	ID2D1Bitmap* GetImageBitmap(D2DGraphics* render);
};

typedef Event<void(class ListView*, int index)> ListViewItemEvent;
typedef Event<void(class ListView*, int index, bool checked)> ListViewCheckChangedEvent;

class ListView : public Control, public IAccessibilityVirtualizedControl
{
public:
	class UpdateScope
	{
	public:
		explicit UpdateScope(ListView& owner) noexcept;
		~UpdateScope();
		UpdateScope(const UpdateScope&) = delete;
		UpdateScope& operator=(const UpdateScope&) = delete;
		UpdateScope(UpdateScope&& other) noexcept;
		UpdateScope& operator=(UpdateScope&& other) noexcept;
		void Commit() noexcept;

	private:
		ListView* _owner = nullptr;
	};

	UIClass Type() override;
	void EnsureBindingPropertiesRegistered() override;
	ListView(int x = 0, int y = 0, int width = 240, int height = 180);

	using ColumnCollection = ObservableCollection<ListViewColumn>;
	using ItemCollection = ObservableCollection<ListViewItem>;
	ColumnCollection Columns;
	ItemCollection Items;

#define CUI_LIST_VIEW_PROPERTY(type, name) \
	PROPERTY(type, name); \
	GET(type, name); \
	SET(type, name)

	CUI_LIST_VIEW_PROPERTY(ListViewViewMode, ViewMode);
	CUI_LIST_VIEW_PROPERTY(ListViewSelectionMode, SelectionMode);
	CUI_LIST_VIEW_PROPERTY(bool, ShowCheckBoxes);
	CUI_LIST_VIEW_PROPERTY(bool, ShowColumnHeaders);
	CUI_LIST_VIEW_PROPERTY(bool, AlternatingRows);
	CUI_LIST_VIEW_PROPERTY(bool, FullRowSelect);
	CUI_LIST_VIEW_PROPERTY(bool, HideSelectionWhenLostFocus);

	CUI_LIST_VIEW_PROPERTY(float, Border);
	CUI_LIST_VIEW_PROPERTY(float, CornerRadius);
	CUI_LIST_VIEW_PROPERTY(float, HeaderHeight);
	CUI_LIST_VIEW_PROPERTY(float, RowHeight);
	CUI_LIST_VIEW_PROPERTY(float, TileHeight);
	CUI_LIST_VIEW_PROPERTY(float, IconItemWidth);
	CUI_LIST_VIEW_PROPERTY(float, IconItemHeight);
	CUI_LIST_VIEW_PROPERTY(float, IconSize);
	CUI_LIST_VIEW_PROPERTY(float, CheckBoxSize);
	CUI_LIST_VIEW_PROPERTY(float, ItemPaddingX);
	CUI_LIST_VIEW_PROPERTY(float, ItemPaddingY);
	CUI_LIST_VIEW_PROPERTY(float, ItemGap);
	CUI_LIST_VIEW_PROPERTY(float, SelectedAccentWidth);
	CUI_LIST_VIEW_PROPERTY(float, ScrollBarSize);
	CUI_LIST_VIEW_PROPERTY(int, MouseWheelStep);

	CUI_LIST_VIEW_PROPERTY(int, SelectedIndex);
	CUI_LIST_VIEW_PROPERTY(int, HoveredIndex);
	CUI_LIST_VIEW_PROPERTY(int, FocusedIndex);
	CUI_LIST_VIEW_PROPERTY(float, ScrollYOffset);

	CUI_LIST_VIEW_PROPERTY(D2D1_COLOR_F, HeaderBackColor);
	CUI_LIST_VIEW_PROPERTY(D2D1_COLOR_F, HeaderForeColor);
	CUI_LIST_VIEW_PROPERTY(D2D1_COLOR_F, GridLineColor);
	CUI_LIST_VIEW_PROPERTY(D2D1_COLOR_F, AlternateItemBackColor);
	CUI_LIST_VIEW_PROPERTY(D2D1_COLOR_F, SelectedItemBackColor);
	CUI_LIST_VIEW_PROPERTY(D2D1_COLOR_F, SelectedItemForeColor);
	CUI_LIST_VIEW_PROPERTY(D2D1_COLOR_F, UnderMouseItemBackColor);
	CUI_LIST_VIEW_PROPERTY(D2D1_COLOR_F, DisabledItemForeColor);
	CUI_LIST_VIEW_PROPERTY(D2D1_COLOR_F, MutedTextColor);
	CUI_LIST_VIEW_PROPERTY(D2D1_COLOR_F, AccentColor);
	CUI_LIST_VIEW_PROPERTY(D2D1_COLOR_F, CheckBackColor);
	CUI_LIST_VIEW_PROPERTY(D2D1_COLOR_F, CheckBorderColor);
	CUI_LIST_VIEW_PROPERTY(D2D1_COLOR_F, ScrollBackColor);
	CUI_LIST_VIEW_PROPERTY(D2D1_COLOR_F, ScrollForeColor);

#undef CUI_LIST_VIEW_PROPERTY

	ListViewItemEvent OnItemClick;
	ListViewItemEvent OnItemDoubleClick;
	ListViewCheckChangedEvent OnItemCheckChanged;
	SelectionChangedEvent SelectionChanged;
	ScrollChangedEvent ScrollChanged;

	void Clear();
	void ClearItems();
	/** Replaces the structural item collection and reconciles selected flags. */
	void SetItems(std::vector<ListViewItem> items);
	void ClearColumns();
	int AddItem(const ListViewItem& item);
	void AddColumn(const ListViewColumn& column);
	bool RemoveItemAt(int index);
	bool SwapItems(int indexA, int indexB);
	size_t ItemCount() const;
	size_t ColumnCount() const;
	ListViewItem* SelectedItem();
	const ListViewItem* SelectedItem() const;
	bool SelectItem(int index, bool additive = false, bool range = false);
	void ClearSelection();
	std::vector<int> GetSelectedIndices() const;
	void EnsureVisible(int index);
	void SetScrollOffset(float offsetY);
	/** Returns the current viewport's contiguous candidate range as [start, end). */
	void GetVisibleItemRange(int& start, int& end) const noexcept;
	int HitTestItem(int localX, int localY) const;
	/** Starts/ends a nested structural batch with one public Reset at the edge. */
	void BeginUpdate() noexcept;
	void EndUpdate() noexcept;
	bool IsUpdating() const noexcept { return _updateDepth != 0; }
	[[nodiscard]] UpdateScope DeferUpdates() noexcept { return UpdateScope(*this); }
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
	/** Number of Details cell identities created by UIA queries so far. */
	size_t MaterializedAccessibilityCellCount() const noexcept
	{
		return _accessibilityCellIds.size();
	}
	/** Number of item-index entries touched by the latest structural mutation. */
	size_t LastAccessibilityIndexUpdateWork() const noexcept
	{
		return _lastAccessibilityIndexUpdateWork;
	}
	/** Number of selection entries inspected by the latest selection operation. */
	size_t LastSelectionUpdateWork() const noexcept
	{
		return _lastSelectionUpdateWork;
	}
	AccessibilityVirtualContainerInfo
		GetAccessibilityVirtualContainerInfo() const noexcept override;
	void GetAccessibilityVirtualSelection(
		std::vector<uint32_t>& result) override;
	bool GetAccessibilityVirtualItemAt(
		int row, int column, uint32_t& id) override;
	void GetAccessibilityVirtualColumnHeaders(
		std::vector<uint32_t>& result) override;
	bool SelectAccessibilityVirtualNode(
		uint32_t id, AccessibilitySelectionAction action) override;
	bool ToggleAccessibilityVirtualNode(uint32_t id) override;
	bool ScrollAccessibilityVirtualNodeIntoView(uint32_t id) override;
	bool GetAccessibilityScrollInfo(
		AccessibilityScrollInfo& result) const noexcept override;
	bool ScrollAccessibility(
		AccessibilityScrollAmount horizontal,
		AccessibilityScrollAmount vertical) override;
	bool SetAccessibilityScrollPercent(
		double horizontalPercent, double verticalPercent) override;

	CursorKind QueryCursor(int localX, int localY) override;
	bool HandlesMouseWheel() const override { return true; }
	bool CanHandleMouseWheel(int delta, int localX, int localY) override;
	bool HandlesNavigationKey(WPARAM key) const override;
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY) override;

protected:
	virtual bool IsListBox() const { return false; }
	void InitializeListBoxDefaults() noexcept;
	void OnComputedLayoutSizeChanged() override;

private:
	struct Layout
	{
		D2D1_RECT_F HeaderRect{ 0,0,0,0 };
		D2D1_RECT_F ContentRect{ 0,0,0,0 };
		D2D1_RECT_F ScrollTrackRect{ 0,0,0,0 };
		D2D1_RECT_F ScrollThumbRect{ 0,0,0,0 };
		float ContentWidth = 0.0f;
		float ContentHeight = 0.0f;
		float MaxScrollY = 0.0f;
		float ScrollBarSize = 8.0f;
		int ColumnsPerRow = 1;
		bool NeedVScroll = false;
	};

	bool _dragVScroll = false;
	float _scrollThumbGrabOffsetY = 0.0f;
	unsigned int _updateDepth = 0;
	bool _updatePendingCollectionRefresh = false;
	int _anchorIndex = -1;
	bool _selectionItemsPrepared = false;
	bool _selectedIndexFollowsItems = false;
	ControlPropertyValueSource _selectedIndexProjectionSource =
		ControlPropertyValueSource::Default;
	int _viewMode = static_cast<int>(ListViewViewMode::List);
	int _selectionMode = static_cast<int>(ListViewSelectionMode::Single);
	bool _showCheckBoxes = false;
	bool _showColumnHeaders = true;
	bool _alternatingRows = false;
	bool _fullRowSelect = true;
	bool _hideSelectionWhenLostFocus = false;
	float _border = 1.0f;
	float _cornerRadius = 6.0f;
	float _headerHeight = 30.0f;
	float _rowHeight = 30.0f;
	float _tileHeight = 58.0f;
	float _iconItemWidth = 96.0f;
	float _iconItemHeight = 82.0f;
	float _iconSize = 32.0f;
	float _checkBoxSize = 14.0f;
	float _itemPaddingX = 8.0f;
	float _itemPaddingY = 3.0f;
	float _itemGap = 8.0f;
	float _selectedAccentWidth = 3.0f;
	float _scrollBarSize = 8.0f;
	int _mouseWheelStep = 48;
	int _selectedIndex = -1;
	int _hoveredIndex = -1;
	int _focusedIndex = -1;
	float _scrollYOffset = 0.0f;
	mutable uint32_t _accessibilityImplicitColumnId = 0;
	mutable std::vector<uint32_t> _accessibilityItemIdsByIndex;
	mutable std::unordered_map<uint32_t, size_t> _accessibilityItemIndexById;
	mutable std::unordered_map<uint32_t, size_t> _accessibilityColumnIndexById;
	mutable std::unordered_map<uint64_t, uint32_t> _accessibilityCellIds;
	mutable std::unordered_map<uint32_t, uint64_t> _accessibilityCellKeyById;
	mutable bool _accessibilityItemIdsDirty = true;
	mutable bool _accessibilityDetailsIdsDirty = true;
	std::unordered_set<uint32_t> _selectedItemIds;
	mutable size_t _lastAccessibilityIndexUpdateWork = 0;
	mutable size_t _lastSelectionUpdateWork = 0;
	D2D1_COLOR_F _headerBackColor = D2D1_COLOR_F{ 0.18f, 0.22f, 0.28f, 0.95f };
	D2D1_COLOR_F _headerForeColor = D2D1_COLOR_F{ 0.90f, 0.93f, 0.98f, 1.0f };
	D2D1_COLOR_F _gridLineColor = D2D1_COLOR_F{ 0.55f, 0.60f, 0.68f, 0.24f };
	D2D1_COLOR_F _alternateItemBackColor = D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.04f };
	D2D1_COLOR_F _selectedItemBackColor = D2D1_COLOR_F{ 0.20f, 0.52f, 0.95f, 0.32f };
	D2D1_COLOR_F _selectedItemForeColor = Colors::Black;
	D2D1_COLOR_F _underMouseItemBackColor = D2D1_COLOR_F{ 0.20f, 0.52f, 0.95f, 0.12f };
	D2D1_COLOR_F _disabledItemForeColor = D2D1_COLOR_F{ 0.50f, 0.52f, 0.58f, 1.0f };
	D2D1_COLOR_F _mutedTextColor = D2D1_COLOR_F{ 0.58f, 0.62f, 0.70f, 1.0f };
	D2D1_COLOR_F _accentColor = D2D1_COLOR_F{ 0.20f, 0.52f, 0.95f, 0.95f };
	D2D1_COLOR_F _checkBackColor = Colors::White;
	D2D1_COLOR_F _checkBorderColor = D2D1_COLOR_F{ 0.45f, 0.48f, 0.55f, 1.0f };
	D2D1_COLOR_F _scrollBackColor = Colors::LightGray;
	D2D1_COLOR_F _scrollForeColor = Colors::DimGrey;

	Layout CalcLayout() const;
	float GetEffectiveRowHeight() const;
	float GetItemPrimaryExtent() const;
	float GetItemSecondaryExtent() const;
	void GetVisibleItemRange(
		const Layout& layout, int& start, int& end) const noexcept;
	D2D1_RECT_F GetItemRect(int index, const Layout& layout) const;
	D2D1_RECT_F GetCheckRect(const D2D1_RECT_F& itemRect) const;
	void ClampScroll(Layout& layout);
	void DrawHeader(D2DGraphics* d2d, const Layout& layout);
	void DrawItems(D2DGraphics* d2d, const Layout& layout);
	void DrawListItem(D2DGraphics* d2d, int index, const D2D1_RECT_F& rect);
	void DrawDetailsItem(D2DGraphics* d2d, int index, const D2D1_RECT_F& rect);
	void DrawTileItem(D2DGraphics* d2d, int index, const D2D1_RECT_F& rect);
	void DrawIconItem(D2DGraphics* d2d, int index, const D2D1_RECT_F& rect);
	void DrawCheckBox(D2DGraphics* d2d, const D2D1_RECT_F& rect, bool checked, bool enabled);
	void DrawScrollBar(D2DGraphics* d2d, const Layout& layout);
	void UpdateHover(int localX, int localY);
	void UpdateScrollByThumb(float localY);
	void ToggleCheckAt(int index);
	void MoveSelectionBy(int delta);
	void PageSelection(int direction);
	void SyncSelectedIndexFromItems(bool raiseEvent = true);
	void ApplySelectedIndexChange(int oldValue, int newValue);
	void NormalizeSelectionForMode();
	void CommitPreparedSelection(
		int selectedIndex,
		int focusedIndex,
		bool selectionItemsChanged);
	void SetCurrentSelectedIndex(int value);
	void SetCurrentFocusedIndex(int value);
	void SetCurrentHoveredIndex(int value);
	void SetCurrentScrollYOffset(float value);
	void ClampScrollToRange();
	void RequestCollectionRefresh();
	void OnItemsCollectionChanged(const CollectionChangedEventArgs& change);
	void OnColumnsCollectionChanged(const CollectionChangedEventArgs& change);
	bool ShouldDrawSelection(const ListViewItem& item) const;
	void EnsureAccessibilityItemIds() const;
	bool ApplyAccessibilityItemCollectionChange(
		const CollectionChangedEventArgs& change);
	void PruneAccessibilityCellsForMissingItems() const;
	void EnsureAccessibilityDetailsIds() const;
	uint32_t EnsureAccessibilityCellId(
		uint32_t rowId, uint32_t columnId) const;
	int FindAccessibilityItem(uint32_t id) const;
	bool SetCachedItemSelected(size_t index, bool selected);
	bool ClearCachedSelection();
	int FindFirstCachedSelectedIndex() const;
};
/**
 * @file ListView.h
 * @brief ListBox：与ListView等价, 占位符, 防止无意义的额外实现。
 */
class ListBox : public ListView
{
public:
	UIClass Type() override;
	void EnsureBindingPropertiesRegistered() override;
	ListBox(int x = 0, int y = 0, int width = 200, int height = 160);

protected:
	bool IsListBox() const override { return true; }
};
