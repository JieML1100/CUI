#define NOMINMAX
#include "PagedGridView.h"
#include "Form.h"

#include <algorithm>
#include <cmath>

static int ComparePagedGridStringDefault(const std::wstring& a, const std::wstring& b)
{
	if (a == b) return 0;
	return (a < b) ? -1 : 1;
}

static std::wstring PagedGridCellToStringDefault(const CellValue* value)
{
	if (!value) return L"";
	if (!value->Text.empty()) return value->Text;
	return std::to_wstring((__int64)value->Tag);
}

UIClass PagedGridView::Type()
{
	return UIClass::UI_PagedGridView;
}

PagedGridView::PagedGridView(int x, int y, int width, int height)
	: Panel(x, y, width, height)
{
	this->BackColor = D2D1_COLOR_F{ 0, 0, 0, 0 };
	this->BorderColor = D2D1_COLOR_F{ 0, 0, 0, 0 };
	this->BorderThickness = 0.0f;
	this->CornerRadius = 8.0f;

	Grid = this->AddControl(new GridView(0, 0, width, (std::max)(1, height - (int)PagerHeight)));
	Grid->AllowUserToAddRows = false;
	Grid->SortRequestHandler = [this](GridView*, int columnIndex, bool ascending) -> bool
		{
			SortByColumn(columnIndex, ascending);
			return true;
		};
	Grid->OnUserAddedRow += [this](GridView* sender, int newRowIndex)
		{
			if (_syncingPage || !sender) return;
			int insertIndex = std::clamp(_pageIndex * _pageSize + newRowIndex, 0, (int)Rows.size());
			if (newRowIndex >= 0 && newRowIndex < (int)sender->Rows.size())
				Rows.insert(Rows.begin() + insertIndex, sender->Rows[newRowIndex]);
			MarkPageDirty();
			RefreshPage();
		};

	CreatePagerControls();
	_lastSourceSize = Rows.size();
	_lastPageSize = _pageSize;
}

GET_CPP(PagedGridView, int, PageIndex)
{
	return _pageIndex;
}

SET_CPP(PagedGridView, int, PageIndex)
{
	int pageCount = GetPageCount();
	int next = std::clamp(value, 0, pageCount - 1);
	if (_pageIndex == next && !_pageDirty) return;
	if (SyncGridEditsOnPageChange)
		SyncCurrentPageToSource();
	int old = _pageIndex;
	_pageIndex = next;
	MarkPageDirty();
	RefreshPage();
	if (old != _pageIndex)
		OnPageChanged(this, old, _pageIndex);
}

GET_CPP(PagedGridView, int, PageSize)
{
	return _pageSize;
}

SET_CPP(PagedGridView, int, PageSize)
{
	int nextSize = std::max(1, value);
	if (_pageSize == nextSize) return;
	if (SyncGridEditsOnPageChange)
		SyncCurrentPageToSource();
	_pageSize = nextSize;
	int clamped = std::clamp(_pageIndex, 0, GetPageCount() - 1);
	int oldPage = _pageIndex;
	_pageIndex = clamped;
	MarkPageDirty();
	if (oldPage != _pageIndex)
		OnPageChanged(this, oldPage, _pageIndex);
	RefreshPage();
}

GET_CPP(PagedGridView, int, PageCount)
{
	int size = std::max(1, _pageSize);
	int total = (int)Rows.size();
	return std::max(1, (total + size - 1) / size);
}

GET_CPP(PagedGridView, int, TotalRowCount)
{
	return (int)Rows.size();
}

void PagedGridView::CreatePagerControls()
{
	_firstButton = this->AddControl(new Button(L"<<", 0, 0, 38, 26));
	_prevButton = this->AddControl(new Button(L"<", 0, 0, 38, 26));
	_nextButton = this->AddControl(new Button(L">", 0, 0, 38, 26));
	_lastButton = this->AddControl(new Button(L">>", 0, 0, 38, 26));
	_pageInfoLabel = this->AddControl(new Label(L"Page 1 / 1", 0, 0));
	_pageInfoLabel->Size = { 220, 24 };
	_pageInfoLabel->BackColor = D2D1_COLOR_F{ 0, 0, 0, 0 };

	_firstButton->OnMouseClick += [this](Control*, MouseEventArgs) { FirstPage(); };
	_prevButton->OnMouseClick += [this](Control*, MouseEventArgs) { PreviousPage(); };
	_nextButton->OnMouseClick += [this](Control*, MouseEventArgs) { NextPage(); };
	_lastButton->OnMouseClick += [this](Control*, MouseEventArgs) { LastPage(); };
}

void PagedGridView::StylePagerButton(Button* button)
{
	if (!button) return;
	button->BackColor = PagerButtonBackColor;
	button->ForeColor = PagerTextColor;
	button->BorderColor = PagerBorderColor;
	button->UnderMouseColor = PagerButtonHoverColor;
	button->CheckedColor = PagerButtonCheckedColor;
	button->DisabledOverlayColor = D2D1_COLOR_F{ 1, 1, 1, 0.38f };
	button->Round = 6.0f;
	button->BorderThickness = 1.0f;
}

void PagedGridView::LayoutChildren()
{
	if (!Grid) return;
	int w = this->Width;
	int h = this->Height;
	int pagerH = ShowPager ? (int)std::round(std::clamp(PagerHeight, 0.0f, (float)h)) : 0;
	int gridH = std::max(1, h - pagerH);
	Grid->Location = { 0, 0 };
	Grid->Size = { w, gridH };

	int buttonW = (int)std::round(std::max(24.0f, PagerButtonWidth));
	int buttonH = (int)std::round(std::max(22.0f, PagerButtonHeight));
	int gap = (int)std::round(std::max(0.0f, PagerGap));
	int padX = (int)std::round(std::max(0.0f, PagerPaddingX));
	int footerTop = gridH;
	int buttonY = footerTop + std::max(0, (pagerH - buttonH) / 2);
	int x = std::max(padX, w - padX - buttonW * 4 - gap * 3);
	Button* buttons[] = { _firstButton, _prevButton, _nextButton, _lastButton };
	for (auto* button : buttons)
	{
		if (!button) continue;
		button->Visible = ShowPager;
		button->Location = { x, buttonY };
		button->Size = { buttonW, buttonH };
		x += buttonW + gap;
	}
	if (_pageInfoLabel)
	{
		_pageInfoLabel->Visible = ShowPager;
		_pageInfoLabel->Location = { padX, footerTop + std::max(0, (pagerH - 22) / 2) };
		_pageInfoLabel->Size = { std::max(1, w - padX * 2 - buttonW * 4 - gap * 4), 24 };
	}
}

void PagedGridView::UpdatePagerState()
{
	const int pageCount = GetPageCount();
	_pageIndex = std::clamp(_pageIndex, 0, pageCount - 1);
	if (_pageInfoLabel)
	{
		int start = Rows.empty() ? 0 : _pageIndex * _pageSize + 1;
		int end = Rows.empty() ? 0 : std::min((int)Rows.size(), (_pageIndex + 1) * _pageSize);
		_pageInfoLabel->Text = L"Page " + std::to_wstring(_pageIndex + 1) + L" / " + std::to_wstring(pageCount) +
			L"   " + std::to_wstring(start) + L"-" + std::to_wstring(end) +
			L" of " + std::to_wstring((int)Rows.size());
		_pageInfoLabel->ForeColor = PagerTextColor;
	}
	bool canPrev = _pageIndex > 0;
	bool canNext = _pageIndex < pageCount - 1;
	if (_firstButton) _firstButton->Enable = canPrev;
	if (_prevButton) _prevButton->Enable = canPrev;
	if (_nextButton) _nextButton->Enable = canNext;
	if (_lastButton) _lastButton->Enable = canNext;
}

void PagedGridView::MarkPageDirty()
{
	_pageDirty = true;
	InvalidateVisual();
}

void PagedGridView::Clear()
{
	ClearRows();
	ClearColumns();
}

void PagedGridView::ClearRows()
{
	Rows.clear();
	_pageIndex = 0;
	MarkPageDirty();
	RefreshPage();
}

void PagedGridView::ClearColumns()
{
	if (Grid) Grid->ClearColumns();
	MarkPageDirty();
}

void PagedGridView::AddRow(const GridViewRow& row)
{
	Rows.push_back(row);
	MarkPageDirty();
}

void PagedGridView::AddColumn(const GridViewColumn& column)
{
	if (Grid) Grid->AddColumn(column);
}

size_t PagedGridView::RowCount() const
{
	return Rows.size();
}

size_t PagedGridView::ColumnCount() const
{
	return Grid ? Grid->ColumnCount() : 0;
}

GridViewRow& PagedGridView::RowAt(int index)
{
	return Rows.at((size_t)index);
}

GridViewColumn& PagedGridView::ColumnAt(int index)
{
	return Grid->ColumnAt(index);
}

void PagedGridView::RemoveRowAt(int index)
{
	if (index < 0 || index >= (int)Rows.size()) return;
	Rows.erase(Rows.begin() + index);
	_pageIndex = std::clamp(_pageIndex, 0, GetPageCount() - 1);
	MarkPageDirty();
	RefreshPage();
}

void PagedGridView::FirstPage()
{
	SetPageIndex(0);
}

void PagedGridView::PreviousPage()
{
	SetPageIndex(_pageIndex - 1);
}

void PagedGridView::NextPage()
{
	SetPageIndex(_pageIndex + 1);
}

void PagedGridView::LastPage()
{
	SetPageIndex(GetPageCount() - 1);
}

void PagedGridView::SortByColumn(int col, bool ascending)
{
	if (!Grid) return;
	if (col < 0 || col >= (int)Grid->Columns.size()) return;

	if (SyncGridEditsOnPageChange)
		SyncCurrentPageToSource();

	if (Rows.size() > 1)
	{
		const auto sortFunc = Grid->Columns[(size_t)col].SortFunc;
		std::stable_sort(Rows.begin(), Rows.end(),
			[&](const GridViewRow& a, const GridViewRow& b) -> bool
			{
				const int aCount = (int)a.Cells.size();
				const int bCount = (int)b.Cells.size();
				const CellValue* av = (aCount > col) ? (a.Cells.data() + col) : nullptr;
				const CellValue* bv = (bCount > col) ? (b.Cells.data() + col) : nullptr;

				int cmp = 0;
				if (sortFunc)
				{
					static CellValue empty;
					cmp = sortFunc(av ? *av : empty, bv ? *bv : empty);
				}
				else
				{
					cmp = ComparePagedGridStringDefault(PagedGridCellToStringDefault(av), PagedGridCellToStringDefault(bv));
				}

				return ascending ? (cmp < 0) : (cmp > 0);
			});
	}

	Grid->SortedColumnIndex = col;
	Grid->SortAscending = ascending;
	MarkPageDirty();
	RefreshPage();
}

void PagedGridView::SyncCurrentPageToSource()
{
	if (!Grid || _syncingPage || _pageSize <= 0) return;
	int start = _pageIndex * _pageSize;
	for (int i = 0; i < (int)Grid->Rows.size(); i++)
	{
		int sourceIndex = start + i;
		if (sourceIndex >= 0 && sourceIndex < (int)Rows.size())
			Rows[(size_t)sourceIndex] = Grid->Rows[(size_t)i];
	}
}

void PagedGridView::RefreshPage()
{
	if (!Grid) return;
	_syncingPage = true;
	Grid->Rows.clear();
	_pageIndex = std::clamp(_pageIndex, 0, GetPageCount() - 1);
	int start = _pageIndex * _pageSize;
	int end = std::min((int)Rows.size(), start + _pageSize);
	for (int i = start; i < end; i++)
		Grid->Rows.push_back(Rows[(size_t)i]);
	Grid->ScrollYOffset = 0.0f;
	Grid->ScrollRowPosition = 0;
	Grid->SelectedColumnIndex = -1;
	Grid->SelectedRowIndex = -1;
	Grid->UnderMouseColumnIndex = -1;
	Grid->UnderMouseRowIndex = -1;
	_syncingPage = false;
	_pageDirty = false;
	_lastSourceSize = Rows.size();
	_lastPageSize = _pageSize;
	UpdatePagerState();
	InvalidateVisual();
}

void PagedGridView::Update()
{
	if (!this->IsVisual) return;
	if (Rows.size() != _lastSourceSize || _pageSize != _lastPageSize)
		MarkPageDirty();
	if (_pageDirty)
		RefreshPage();

	LayoutChildren();
	StylePagerButton(_firstButton);
	StylePagerButton(_prevButton);
	StylePagerButton(_nextButton);
	StylePagerButton(_lastButton);
	UpdatePagerState();

	auto d2d = this->ParentForm ? this->ParentForm->Render : nullptr;
	if (d2d && ShowPager)
	{
		const float w = (float)this->Width;
		const float h = (float)this->Height;
		const float pagerH = std::clamp(PagerHeight, 0.0f, h);
		this->BeginRender(w, h);
		{
			const float top = h - pagerH;
			d2d->FillRoundRect(0.0f, top, w, pagerH, PagerBackColor, std::clamp(CornerRadius, 0.0f, pagerH * 0.5f));
			d2d->DrawLine(0.0f, top + 0.5f, w, top + 0.5f, PagerBorderColor, 1.0f);
			if (!this->ParentForm || !this->ParentForm->IsDCompSceneRenderActive())
			{
				for (auto c : this->GetChildrenInZOrder())
				{
					if (!c || !c->Visible) continue;
					c->Update();
				}
			}
		}
		if (!Enable)
			d2d->FillRoundRect(0.0f, 0.0f, w, h, DisabledOverlayColor, CornerRadius);
		this->EndRender();
		return;
	}

	Panel::Update();
}

bool PagedGridView::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof)
{
	if (message == WM_KEYDOWN)
	{
		if (wParam == VK_PRIOR)
		{
			PreviousPage();
			return true;
		}
		if (wParam == VK_NEXT)
		{
			NextPage();
			return true;
		}
	}
	return Panel::ProcessMessage(message, wParam, lParam, xof, yof);
}
