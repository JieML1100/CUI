#pragma once
#include "Control.h"

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
	std::wstring Header;
	float Width = 120.0f;
	ListViewCellAlign Align = ListViewCellAlign::Left;

	ListViewColumn() = default;
	ListViewColumn(std::wstring header, float width = 120.0f, ListViewCellAlign align = ListViewCellAlign::Left);
};

class ListViewItem
{
public:
	std::wstring Text;
	std::wstring SubText;
	std::vector<std::wstring> SubItems;
	std::shared_ptr<BitmapSource> Image;
	Microsoft::WRL::ComPtr<ID2D1Bitmap> ImageCache;
	ID2D1RenderTarget* ImageCacheTarget = nullptr;
	const BitmapSource* ImageCacheSource = nullptr;
	UINT64 Tag = 0;
	bool Checked = false;
	bool Selected = false;
	bool Enabled = true;

	ListViewItem() = default;
	explicit ListViewItem(std::wstring text);
	ListViewItem(std::wstring text, std::wstring subText);
	ID2D1Bitmap* GetImageBitmap(D2DGraphics* render);
};

typedef Event<void(class ListView*, int index)> ListViewItemEvent;
typedef Event<void(class ListView*, int index, bool checked)> ListViewCheckChangedEvent;

class ListView : public Control
{
public:
	UIClass Type() override;
	ListView(int x = 0, int y = 0, int width = 240, int height = 180);

	std::vector<ListViewColumn> Columns;
	std::vector<ListViewItem> Items;

	ListViewViewMode ViewMode = ListViewViewMode::List;
	ListViewSelectionMode SelectionMode = ListViewSelectionMode::Single;
	bool ShowCheckBoxes = false;
	bool ShowColumnHeaders = true;
	bool AlternatingRows = false;
	bool FullRowSelect = true;
	bool HideSelectionWhenLostFocus = false;

	float Border = 1.0f;
	float CornerRadius = 6.0f;
	float HeaderHeight = 30.0f;
	float RowHeight = 30.0f;
	float TileHeight = 58.0f;
	float IconItemWidth = 96.0f;
	float IconItemHeight = 82.0f;
	float IconSize = 32.0f;
	float CheckBoxSize = 14.0f;
	float ItemPaddingX = 8.0f;
	float ItemPaddingY = 3.0f;
	float ItemGap = 8.0f;
	float SelectedAccentWidth = 3.0f;
	float ScrollBarSize = 8.0f;
	int MouseWheelStep = 48;

	int SelectedIndex = -1;
	int HoveredIndex = -1;
	int FocusedIndex = -1;
	float ScrollYOffset = 0.0f;

	D2D1_COLOR_F HeaderBackColor = D2D1_COLOR_F{ 0.18f, 0.22f, 0.28f, 0.95f };
	D2D1_COLOR_F HeaderForeColor = D2D1_COLOR_F{ 0.90f, 0.93f, 0.98f, 1.0f };
	D2D1_COLOR_F GridLineColor = D2D1_COLOR_F{ 0.55f, 0.60f, 0.68f, 0.24f };
	D2D1_COLOR_F AlternateItemBackColor = D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.04f };
	D2D1_COLOR_F SelectedItemBackColor = D2D1_COLOR_F{ 0.20f, 0.52f, 0.95f, 0.32f };
	D2D1_COLOR_F SelectedItemForeColor = Colors::Black;
	D2D1_COLOR_F UnderMouseItemBackColor = D2D1_COLOR_F{ 0.20f, 0.52f, 0.95f, 0.12f };
	D2D1_COLOR_F DisabledItemForeColor = D2D1_COLOR_F{ 0.50f, 0.52f, 0.58f, 1.0f };
	D2D1_COLOR_F MutedTextColor = D2D1_COLOR_F{ 0.58f, 0.62f, 0.70f, 1.0f };
	D2D1_COLOR_F AccentColor = D2D1_COLOR_F{ 0.20f, 0.52f, 0.95f, 0.95f };
	D2D1_COLOR_F CheckBackColor = Colors::White;
	D2D1_COLOR_F CheckBorderColor = D2D1_COLOR_F{ 0.45f, 0.48f, 0.55f, 1.0f };
	D2D1_COLOR_F ScrollBackColor = Colors::LightGray;
	D2D1_COLOR_F ScrollForeColor = Colors::DimGrey;

	ListViewItemEvent OnItemClick;
	ListViewItemEvent OnItemDoubleClick;
	ListViewCheckChangedEvent OnItemCheckChanged;
	SelectionChangedEvent SelectionChanged;
	ScrollChangedEvent ScrollChanged;

	void Clear();
	void ClearItems();
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
	int HitTestItem(int xof, int yof) const;

	CursorKind QueryCursor(int xof, int yof) override;
	bool HandlesMouseWheel() const override { return true; }
	bool CanHandleMouseWheel(int delta, int xof, int yof) override;
	bool HandlesNavigationKey(WPARAM key) const override;
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof) override;

protected:
	virtual bool IsListBox() const { return false; }

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
	int _anchorIndex = -1;

	Layout CalcLayout() const;
	float GetEffectiveRowHeight() const;
	float GetItemPrimaryExtent() const;
	float GetItemSecondaryExtent() const;
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
	void UpdateHover(int xof, int yof);
	void UpdateScrollByThumb(float yof);
	void ToggleCheckAt(int index);
	void MoveSelectionBy(int delta);
	void PageSelection(int direction);
	void SyncSelectedIndexFromItems();
};
/**
 * @file ListView.h
 * @brief ListBox：与ListView等价, 占位符, 防止无意义的额外实现。
 */
class ListBox : public ListView
{
public:
	UIClass Type() override;
	ListBox(int x = 0, int y = 0, int width = 200, int height = 160);

protected:
	bool IsListBox() const override { return true; }
};
