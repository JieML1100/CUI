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
	size_t _lastSourceSize = 0;
	int _lastPageSize = 50;

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

public:
	UIClass Type() override;
	PagedGridView(int x = 0, int y = 0, int width = 520, int height = 320);

	GridView* Grid = nullptr;
	std::vector<GridViewRow> Rows;

	PagedGridViewPageChangedEvent OnPageChanged;

	float PagerHeight = 42.0f;
	float PagerGap = 6.0f;
	float PagerButtonWidth = 38.0f;
	float PagerButtonHeight = 26.0f;
	float PagerPaddingX = 8.0f;
	float CornerRadius = 8.0f;
	bool ShowPager = true;
	bool SyncGridEditsOnPageChange = true;

	D2D1_COLOR_F PagerBackColor = D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.72f };
	D2D1_COLOR_F PagerBorderColor = D2D1_COLOR_F{ 0.72f, 0.75f, 0.82f, 0.55f };
	D2D1_COLOR_F PagerButtonBackColor = D2D1_COLOR_F{ 0.97f, 0.98f, 0.99f, 1.0f };
	D2D1_COLOR_F PagerButtonHoverColor = D2D1_COLOR_F{ 0.20f, 0.46f, 0.90f, 0.16f };
	D2D1_COLOR_F PagerButtonCheckedColor = D2D1_COLOR_F{ 0.20f, 0.46f, 0.90f, 0.28f };
	D2D1_COLOR_F PagerTextColor = Colors::Black;
	D2D1_COLOR_F AccentColor = D2D1_COLOR_F{ 0.20f, 0.55f, 0.95f, 0.95f };

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
	void ClearColumns();
	void AddRow(const GridViewRow& row);
	void AddColumn(const GridViewColumn& column);
	size_t RowCount() const;
	size_t ColumnCount() const;
	GridViewRow& RowAt(int index);
	GridViewColumn& ColumnAt(int index);
	void RemoveRowAt(int index);
	void FirstPage();
	void PreviousPage();
	void NextPage();
	void LastPage();
	void SortByColumn(int col, bool ascending = true);
	void SyncCurrentPageToSource();
	void RefreshPage();

	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof) override;
};
