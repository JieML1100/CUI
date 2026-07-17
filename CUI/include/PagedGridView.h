#pragma once
#include "Panel.h"
#include "GridView.h"
#include "Button.h"
#include "Label.h"

/**
 * @file PagedGridView.h
 * @brief PagedGridView：基于 GridView 的分页数据表格。
 */

typedef Event<void(class PagedGridView*, int oldPageIndex, int newPageIndex)> PagedGridViewPageChangedEvent;

class PagedGridView : public Panel
{
private:
	int _pageIndex = 0;
	int _pageSize = 50;
	bool _pageDirty = true;
	bool _syncingPage = false;
	bool _changingPageSize = false;
	int _updateDepth = 0;
	size_t _lastSourceSize = 0;
	int _lastPageSize = 50;
	float _pagerHeight = 42.0f;
	float _pagerGap = 6.0f;
	float _pagerButtonWidth = 38.0f;
	float _pagerButtonHeight = 26.0f;
	float _pagerPaddingX = 8.0f;
	bool _showPager = true;
	bool _syncGridEditsOnPageChange = true;
	std::vector<uint32_t> _sourceColumnIds;
	EventConnection _columnsChangedConnection;
	D2D1_COLOR_F _pagerBackColor = cui::theme::palette::SurfaceSubtle;
	D2D1_COLOR_F _pagerBorderColor = cui::theme::palette::Border;
	D2D1_COLOR_F _pagerButtonBackColor = cui::theme::palette::Surface;
	D2D1_COLOR_F _pagerButtonHoverColor = cui::theme::palette::AccentSoft;
	D2D1_COLOR_F _pagerButtonCheckedColor = cui::theme::palette::AccentSelected;
	D2D1_COLOR_F _pagerTextColor = cui::theme::palette::TextPrimary;
	D2D1_COLOR_F _accentColor = cui::theme::palette::Accent;

	Button* _firstButton = nullptr;
	Button* _prevButton = nullptr;
	Button* _nextButton = nullptr;
	Button* _lastButton = nullptr;
	Label* _pageInfoLabel = nullptr;

	void CreatePagerControls();
	void LayoutChildren();
	void UpdatePagerState();
	void StylePagerButton(Button* button);
	void MarkPageDirty();
	void SyncPageToSource(int pageIndex, int pageSize);
	void SetCurrentPageIndex(int value);
	void OnRowsCollectionChanged(const CollectionChangedEventArgs& change);
	void OnColumnsCollectionChanged(const CollectionChangedEventArgs& change);
	void EnsureSourceAccessibilityIds();

public:
	class UpdateScope
	{
	public:
		explicit UpdateScope(PagedGridView& owner) noexcept;
		~UpdateScope();
		UpdateScope(const UpdateScope&) = delete;
		UpdateScope& operator=(const UpdateScope&) = delete;
		UpdateScope(UpdateScope&& other) noexcept;
		UpdateScope& operator=(UpdateScope&& other) noexcept;
		void Commit() noexcept;

	private:
		PagedGridView* _owner = nullptr;
	};

	UIClass Type() override;
	void EnsureBindingPropertiesRegistered() override;
	PagedGridView(int x = 0, int y = 0, int width = 520, int height = 320);

	GridView* Grid = nullptr;
	using RowCollection = ObservableCollection<GridViewRow>;
	using ColumnCollection = GridView::ColumnCollection;
	RowCollection Rows;
	__declspec(property(get = GetColumns)) ColumnCollection& Columns;
	ColumnCollection& GetColumns();
	const ColumnCollection& GetColumns() const;

	PagedGridViewPageChangedEvent OnPageChanged;


#define CUI_PAGED_GRID_PROPERTY(type, name) \
	PROPERTY(type, name); \
	GET(type, name); \
	SET(type, name)

	CUI_PAGED_GRID_PROPERTY(float, PagerHeight);
	CUI_PAGED_GRID_PROPERTY(float, PagerGap);
	CUI_PAGED_GRID_PROPERTY(float, PagerButtonWidth);
	CUI_PAGED_GRID_PROPERTY(float, PagerButtonHeight);
	CUI_PAGED_GRID_PROPERTY(float, PagerPaddingX);
	CUI_PAGED_GRID_PROPERTY(bool, ShowPager);
	CUI_PAGED_GRID_PROPERTY(bool, SyncGridEditsOnPageChange);
	CUI_PAGED_GRID_PROPERTY(D2D1_COLOR_F, PagerBackColor);
	CUI_PAGED_GRID_PROPERTY(D2D1_COLOR_F, PagerBorderColor);
	CUI_PAGED_GRID_PROPERTY(D2D1_COLOR_F, PagerButtonBackColor);
	CUI_PAGED_GRID_PROPERTY(D2D1_COLOR_F, PagerButtonHoverColor);
	CUI_PAGED_GRID_PROPERTY(D2D1_COLOR_F, PagerButtonCheckedColor);
	CUI_PAGED_GRID_PROPERTY(D2D1_COLOR_F, PagerTextColor);
	CUI_PAGED_GRID_PROPERTY(D2D1_COLOR_F, AccentColor);

#undef CUI_PAGED_GRID_PROPERTY

	PROPERTY(int, PageIndex);
	GET(int, PageIndex);
	SET(int, PageIndex);

	PROPERTY(int, PageSize);
	GET(int, PageSize);
	SET(int, PageSize);

	READONLY_PROPERTY(int, PageCount);
	GET(int, PageCount);

	READONLY_PROPERTY(int, TotalRowCount);
	GET(int, TotalRowCount);

	void Clear();
	void ClearRows();
	/** @brief 原子替换分页数据源，最外层更新结束时只刷新一次当前页。 */
	void SetRows(std::vector<GridViewRow> rows);
	void ClearColumns();
	void SetColumns(std::vector<GridViewColumn> columns);
	void AddRow(const GridViewRow& row);
	void AddColumn(const GridViewColumn& column);
	size_t RowCount() const;
	size_t ColumnCount() const;
	GridViewRow& RowAt(int index);
	const GridViewRow& RowAt(int index) const;
	GridViewColumn& ColumnAt(int index);
	const GridViewColumn& ColumnAt(int index) const;
	bool RemoveRowAt(int index);
	bool RemoveColumnAt(int index);
	void BeginUpdate() noexcept;
	void EndUpdate() noexcept;
	bool IsUpdating() const noexcept { return _updateDepth > 0; }
	[[nodiscard]] UpdateScope DeferUpdates() noexcept { return UpdateScope(*this); }
	void FirstPage();
	void PreviousPage();
	void NextPage();
	void LastPage();
	void SortByColumn(int col, bool ascending = true);
	void SyncCurrentPageToSource();
	void RefreshPage();

	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY) override;
};
