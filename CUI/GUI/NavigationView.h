#pragma once
#include "Control.h"
#include <vector>

/**
 * @file NavigationView.h
 * @brief NavigationView/SideBar/BreadcrumbBar：侧栏导航与面包屑路径控件。
 */

enum class NavigationViewDisplayMode
{
	Expanded,
	Compact
};

enum class NavigationViewItemKind
{
	Item,
	Header,
	Separator
};

class NavigationViewItem
{
public:
	std::wstring Text;
	std::wstring Value;
	std::wstring BadgeText;
	std::shared_ptr<BitmapSource> Icon;
	Microsoft::WRL::ComPtr<ID2D1Bitmap> IconCache;
	ID2D1RenderTarget* IconCacheTarget = nullptr;
	const BitmapSource* IconCacheSource = nullptr;
	NavigationViewItemKind Kind = NavigationViewItemKind::Item;
	UINT64 Tag = 0;
	bool Enabled = true;
	bool Selected = false;

	NavigationViewItem() = default;
	NavigationViewItem(std::wstring text, std::wstring value = L"", std::shared_ptr<BitmapSource> icon = nullptr);
	static NavigationViewItem Header(std::wstring text);
	static NavigationViewItem Separator();
	ID2D1Bitmap* GetIconBitmap(D2DGraphics* render);
};

typedef Event<void(class NavigationView*, int index)> NavigationViewItemEvent;

class NavigationView : public Control
{
public:
	UIClass Type() override;
	NavigationView(int x = 0, int y = 0, int width = 220, int height = 360);

	std::vector<NavigationViewItem> Items;

	NavigationViewDisplayMode DisplayMode = NavigationViewDisplayMode::Expanded;
	bool IsPaneOpen = true;
	bool ShowToggleButton = true;
	bool ShowHeader = true;
	bool ShowIconPlaceholder = true;
	bool AutoSelectOnClick = true;

	std::wstring HeaderText = L"Navigation";
	std::wstring FooterText;

	float Border = 1.0f;
	float CornerRadius = 8.0f;
	float HeaderHeight = 44.0f;
	float ItemHeight = 34.0f;
	float HeaderItemHeight = 24.0f;
	float SeparatorHeight = 10.0f;
	float ItemGap = 4.0f;
	float ItemPaddingX = 8.0f;
	float IconSize = 18.0f;
	float SelectedAccentWidth = 3.0f;
	float ScrollBarSize = 8.0f;
	int MouseWheelStep = 48;

	int SelectedIndex = -1;
	int HoveredIndex = -1;
	int FocusedIndex = -1;
	float ScrollYOffset = 0.0f;

	D2D1_COLOR_F SurfaceColor = D2D1_COLOR_F{ 0.98f, 0.985f, 0.995f, 0.96f };
	D2D1_COLOR_F HeaderBackColor = D2D1_COLOR_F{ 0.92f, 0.94f, 0.98f, 0.72f };
	D2D1_COLOR_F MutedTextColor = D2D1_COLOR_F{ 0.42f, 0.47f, 0.56f, 1.0f };
	D2D1_COLOR_F SelectedItemBackColor = D2D1_COLOR_F{ 0.20f, 0.52f, 0.95f, 0.16f };
	D2D1_COLOR_F SelectedItemForeColor = Colors::Black;
	D2D1_COLOR_F UnderMouseItemBackColor = D2D1_COLOR_F{ 0.20f, 0.52f, 0.95f, 0.10f };
	D2D1_COLOR_F AccentColor = D2D1_COLOR_F{ 0.20f, 0.52f, 0.95f, 0.95f };
	D2D1_COLOR_F IconPlaceholderColor = D2D1_COLOR_F{ 0.20f, 0.52f, 0.95f, 0.18f };
	D2D1_COLOR_F BadgeBackColor = D2D1_COLOR_F{ 0.20f, 0.52f, 0.95f, 0.86f };
	D2D1_COLOR_F BadgeForeColor = Colors::White;
	D2D1_COLOR_F SeparatorColor = D2D1_COLOR_F{ 0.52f, 0.58f, 0.68f, 0.32f };
	D2D1_COLOR_F ScrollBackColor = Colors::LightGray;
	D2D1_COLOR_F ScrollForeColor = Colors::DimGrey;

	NavigationViewItemEvent OnItemClick;
	NavigationViewItemEvent OnItemDoubleClick;
	SelectionChangedEvent SelectionChanged;
	ScrollChangedEvent ScrollChanged;

	int AddItem(const NavigationViewItem& item);
	int AddItem(const std::wstring& text, const std::wstring& value = L"", std::shared_ptr<BitmapSource> icon = nullptr);
	int AddHeader(const std::wstring& text);
	int AddSeparator();
	void ClearItems();
	bool RemoveItemAt(int index);
	NavigationViewItem* SelectedItem();
	const NavigationViewItem* SelectedItem() const;
	bool SelectItem(int index);
	void ClearSelection();
	void SetPaneOpen(bool value);
	void TogglePane();
	void SetScrollOffset(float offsetY);
	int HitTestItem(int xof, int yof) const;

	CursorKind QueryCursor(int xof, int yof) override;
	bool HandlesMouseWheel() const override { return true; }
	bool CanHandleMouseWheel(int delta, int xof, int yof) override;
	bool HandlesNavigationKey(WPARAM key) const override;
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof) override;

protected:
	bool IsCompactMode() const;

private:
	struct RowInfo
	{
		int Index = -1;
		float Top = 0.0f;
		float Height = 0.0f;
	};

	struct Layout
	{
		D2D1_RECT_F HeaderRect{ 0,0,0,0 };
		D2D1_RECT_F ToggleRect{ 0,0,0,0 };
		D2D1_RECT_F ContentRect{ 0,0,0,0 };
		D2D1_RECT_F FooterRect{ 0,0,0,0 };
		D2D1_RECT_F ScrollTrackRect{ 0,0,0,0 };
		D2D1_RECT_F ScrollThumbRect{ 0,0,0,0 };
		float ContentHeight = 0.0f;
		float MaxScrollY = 0.0f;
		bool NeedVScroll = false;
	};

	bool _dragVScroll = false;
	float _scrollThumbGrabOffsetY = 0.0f;

	Layout CalcLayout(std::vector<RowInfo>* rows = nullptr) const;
	float GetRowHeight(const NavigationViewItem& item) const;
	D2D1_RECT_F GetRowRect(const RowInfo& row, const Layout& layout) const;
	void ClampScroll(Layout& layout);
	bool HitTestToggle(const Layout& layout, int xof, int yof) const;
	void DrawHeader(D2DGraphics* d2d, const Layout& layout);
	void DrawRows(D2DGraphics* d2d, const std::vector<RowInfo>& rows, const Layout& layout);
	void DrawScrollBar(D2DGraphics* d2d, const Layout& layout);
	void UpdateHover(int xof, int yof);
	void UpdateScrollByThumb(float yof);
	void MoveSelectionBy(int delta);
	void SyncSelectedIndexFromItems();
};

class SideBar : public NavigationView
{
public:
	UIClass Type() override;
	SideBar(int x = 0, int y = 0, int width = 200, int height = 360);
};

class BreadcrumbBarItem
{
public:
	std::wstring Text;
	std::wstring Value;
	UINT64 Tag = 0;
	bool Enabled = true;

	BreadcrumbBarItem() = default;
	BreadcrumbBarItem(std::wstring text, std::wstring value = L"");
};

typedef Event<void(class BreadcrumbBar*, int index)> BreadcrumbBarItemEvent;

class BreadcrumbBar : public Control
{
public:
	UIClass Type() override;
	BreadcrumbBar(int x = 0, int y = 0, int width = 320, int height = 32);

	std::vector<BreadcrumbBarItem> Items;
	int SelectedIndex = -1;
	int HoveredIndex = -1;

	float Border = 1.0f;
	float CornerRadius = 7.0f;
	float ItemPaddingX = 10.0f;
	float ItemGap = 4.0f;
	float SeparatorWidth = 18.0f;

	D2D1_COLOR_F SurfaceColor = D2D1_COLOR_F{ 0.98f, 0.985f, 0.995f, 0.86f };
	D2D1_COLOR_F HoverBackColor = D2D1_COLOR_F{ 0.20f, 0.52f, 0.95f, 0.10f };
	D2D1_COLOR_F SelectedBackColor = D2D1_COLOR_F{ 0.20f, 0.52f, 0.95f, 0.16f };
	D2D1_COLOR_F MutedTextColor = D2D1_COLOR_F{ 0.42f, 0.47f, 0.56f, 1.0f };
	D2D1_COLOR_F AccentColor = D2D1_COLOR_F{ 0.20f, 0.52f, 0.95f, 0.95f };

	BreadcrumbBarItemEvent OnItemClick;
	SelectionChangedEvent SelectionChanged;

	int AddItem(const BreadcrumbBarItem& item);
	int AddItem(const std::wstring& text, const std::wstring& value = L"");
	void SetPath(const std::vector<std::wstring>& path);
	void ClearItems();
	bool SelectItem(int index);
	int HitTestItem(int xof, int yof) const;

	CursorKind QueryCursor(int xof, int yof) override;
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof) override;

private:
	struct ItemRegion
	{
		int Index = -1;
		D2D1_RECT_F Rect{ 0,0,0,0 };
	};

	std::vector<ItemRegion> BuildLayout() const;
	void DrawChevron(D2DGraphics* d2d, float cx, float cy, D2D1_COLOR_F color);
};
