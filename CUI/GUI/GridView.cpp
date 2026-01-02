#pragma once
#include "GridView.h"
#include "Form.h"
#include <algorithm>
#include <cmath>
#include <cwchar>
#pragma comment(lib, "Imm32.lib")

CellValue::CellValue() : Text(L""), Image(NULL), Tag(NULL)
{
}
CellValue::CellValue(std::wstring s) : Text(s), Tag(NULL), Image(NULL)
{
}
CellValue::CellValue(wchar_t* s) :Text(s), Tag(NULL), Image(NULL)
{
}
CellValue::CellValue(const wchar_t* s) : Text(s), Tag(NULL), Image(NULL)
{
}
CellValue::CellValue(ID2D1Bitmap* img) : Text(L""), Tag(NULL), Image(img)
{
}
CellValue::CellValue(__int64 tag) : Text(L""), Tag(tag), Image(NULL)
{
}
CellValue::CellValue(bool tag) : Text(L""), Tag(tag), Image(NULL)
{
}
CellValue::CellValue(__int32 tag) : Text(L""), Tag(tag), Image(NULL)
{
}
CellValue::CellValue(unsigned __int32 tag) : Text(L""), Tag(tag), Image(NULL)
{
}
CellValue::CellValue(unsigned __int64 tag) : Text(L""), Tag(tag), Image(NULL)
{
}
CellValue& GridViewRow::operator[](int idx)
{
	return Cells[idx];
}
GridViewColumn::GridViewColumn(std::wstring name, float width, ColumnType type, bool canEdit)
{
	Name = name;
	Width = width;
	Type = type;
	CanEdit = canEdit;
}
UIClass GridView::Type() { return UIClass::UI_GridView; }
GridView::GridView(int x, int y, int width, int height)
{
	this->Location = POINT{ x,y };
	this->Size = SIZE{ width,height };
}

GridView::~GridView()
{
	CloseComboBoxEditor();
	if (this->_cellComboBox)
	{
		delete this->_cellComboBox;
		this->_cellComboBox = NULL;
	}
	this->_cellComboBoxColumnIndex = -1;
	this->_cellComboBoxRowIndex = -1;

	auto baseFont = this->Font;
	if (this->HeadFont && this->HeadFont != baseFont && this->HeadFont != GetDefaultFontObject())
	{
		delete this->HeadFont;
	}
	this->HeadFont = NULL;
}

float GridView::GetTotalColumnsWidth()
{
	float sum = 0.0f;
	for (int i = 0; i < this->Columns.Count; i++)
		sum += this->Columns[i].Width;
	return sum;
}

GridView::ScrollLayout GridView::CalcScrollLayout()
{
	ScrollLayout l{};
	l.ScrollBarSize = 8.0f;
	l.HeadHeight = this->GetHeadHeightPx();
	l.RowHeight = this->GetRowHeightPx();
	l.TotalColumnsWidth = GetTotalColumnsWidth();

	bool needV = false;
	bool needH = false;
	for (int iter = 0; iter < 3; iter++)
	{
		float renderW = (float)this->Width - (needV ? l.ScrollBarSize : 0.0f);
		float renderH = (float)this->Height - (needH ? l.ScrollBarSize : 0.0f);
		if (renderW < 0.0f) renderW = 0.0f;
		if (renderH < 0.0f) renderH = 0.0f;

		float contentH = renderH - l.HeadHeight;
		if (contentH < 0.0f) contentH = 0.0f;
		int visibleRows = 0;
		if (l.RowHeight > 0.0f && contentH > 0.0f)
			visibleRows = (int)std::ceil(contentH / l.RowHeight) + 1;
		if (visibleRows < 0) visibleRows = 0;
		
		// 计算新行区域高度（如果有的话）
		float newRowAreaHeight = (this->AllowUserToAddRows && this->Columns.Count > 0) ? l.RowHeight : 0.0f;
		float totalRowsH = (l.RowHeight > 0.0f) ? (l.RowHeight * (float)this->Rows.Count) : 0.0f;
		totalRowsH += newRowAreaHeight;  // 加上新行区域高度

		bool newNeedV = (totalRowsH > contentH);
		bool newNeedH = (l.TotalColumnsWidth > renderW);

		if (newNeedV == needV && newNeedH == needH)
		{
			l.NeedV = needV;
			l.NeedH = needH;
			l.RenderWidth = renderW;
			l.RenderHeight = renderH;
			l.ContentHeight = contentH;
			l.TotalRowsHeight = totalRowsH;
			l.MaxScrollY = std::max(0.0f, totalRowsH - contentH);
			l.VisibleRows = visibleRows;
			l.MaxScrollRow = std::max(0, this->Rows.Count - visibleRows);
			l.MaxScrollX = std::max(0.0f, l.TotalColumnsWidth - renderW);
			return l;
		}
		needV = newNeedV;
		needH = newNeedH;
	}

	l.NeedV = needV;
	l.NeedH = needH;
	l.RenderWidth = (float)this->Width - (needV ? l.ScrollBarSize : 0.0f);
	l.RenderHeight = (float)this->Height - (needH ? l.ScrollBarSize : 0.0f);
	float contentH = l.RenderHeight - l.HeadHeight;
	if (contentH < 0.0f) contentH = 0.0f;
	l.ContentHeight = contentH;
	
	// 计算新行区域高度
	float newRowAreaHeight = (this->AllowUserToAddRows && this->Columns.Count > 0) ? l.RowHeight : 0.0f;
	l.TotalRowsHeight = (l.RowHeight > 0.0f) ? (l.RowHeight * (float)this->Rows.Count) : 0.0f;
	l.TotalRowsHeight += newRowAreaHeight;  // 加上新行区域高度
	l.MaxScrollY = std::max(0.0f, l.TotalRowsHeight - contentH);
	l.VisibleRows = (l.RowHeight > 0.0f && contentH > 0.0f) ? ((int)std::ceil(contentH / l.RowHeight) + 1) : 0;
	if (l.VisibleRows < 0) l.VisibleRows = 0;
	l.MaxScrollRow = std::max(0, this->Rows.Count - l.VisibleRows);
	l.MaxScrollX = std::max(0.0f, l.TotalColumnsWidth - l.RenderWidth);
	return l;
}

CursorKind GridView::QueryCursor(int xof, int yof)
{
	if (!this->Enable) return CursorKind::Arrow;
	if (this->_resizingColumn) return CursorKind::SizeWE;

	{
		auto l = this->CalcScrollLayout();
		const int renderW = (int)l.RenderWidth;
		const int renderH = (int)l.RenderHeight;
		if (l.NeedH && yof >= renderH && xof >= 0 && xof < renderW)
			return CursorKind::SizeWE;
		if (l.NeedV && xof >= renderW && yof >= 0 && yof < renderH)
			return CursorKind::SizeNS;
	}

	if (HitTestHeaderDivider(xof, yof) >= 0)
		return CursorKind::SizeWE;

	{
		POINT undermouseIndex = GetGridViewUnderMouseItem(xof, yof, this);
		if (undermouseIndex.y >= 0 && undermouseIndex.x >= 0 &&
			undermouseIndex.y < this->Rows.Count && undermouseIndex.x < this->Columns.Count)
		{
			if (this->Columns[undermouseIndex.x].Type == ColumnType::Button)
				return CursorKind::Hand;
		}
	}

	if (this->Editing && this->IsSelected())
	{
		D2D1_RECT_F rect{};
		if (TryGetCellRectLocal(this->EditingColumnIndex, this->EditingRowIndex, rect))
		{
			if ((float)xof >= rect.left && (float)xof <= rect.right &&
				(float)yof >= rect.top && (float)yof <= rect.bottom)
			{
				return CursorKind::IBeam;
			}
		}
	}

	// 检查是否在新行区域
	if (this->AllowUserToAddRows)
	{
		int newRowCol = -1;
		if (HitTestNewRow(xof, yof, newRowCol) >= 0 && newRowCol >= 0)
		{
			return CursorKind::IBeam;  // 在新行区域显示IBeam光标表示可以编辑
		}
	}

	return CursorKind::Arrow;
}
GridViewRow& GridView::operator[](int idx)
{
	return Rows[idx];
}
GridViewRow& GridView::SelectedRow()
{
	static GridViewRow default_;
	if (this->SelectedRowIndex >= 0 && this->SelectedRowIndex < this->Rows.Count)
	{
		return this->Rows[this->SelectedRowIndex];
	}
	return default_;
}
std::wstring& GridView::SelectedValue()
{
	static std::wstring default_;
	if (this->SelectedRowIndex >= 0 && this->SelectedRowIndex < this->Rows.Count)
	{
		return this->Rows[this->SelectedRowIndex].Cells[SelectedColumnIndex].Text;
	}
	return default_;
}
void GridView::Clear()
{
	this->Rows.Clear();
	this->ScrollYOffset = 0.0f;
	this->ScrollRowPosition = 0;
}

static int CompareWStringDefault(const std::wstring& a, const std::wstring& b)
{
	if (a == b) return 0;
	return (a < b) ? -1 : 1;
}

static std::wstring CellToStringDefault(const CellValue* v)
{
	if (!v) return L"";
	if (!v->Text.empty()) return v->Text;
	return std::to_wstring((__int64)v->Tag);
}

void GridView::SortByColumn(int col, bool ascending)
{
	if (col < 0 || col >= this->Columns.Count) return;
	if (this->Rows.Count <= 1) return;

	if (this->Editing)
	{
		SaveCurrentEditingCell(true);
		this->Editing = false;
		this->EditingColumnIndex = -1;
		this->EditingRowIndex = -1;
		this->EditingText.clear();
		this->EditingOriginalText.clear();
		this->EditSelectionStart = this->EditSelectionEnd = 0;
		this->EditOffsetX = 0.0f;
	}

	const auto sortFunc = this->Columns[col].SortFunc;
	std::stable_sort(this->Rows.begin(), this->Rows.end(),
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
				cmp = CompareWStringDefault(CellToStringDefault(av), CellToStringDefault(bv));
			}

			if (ascending)
				return cmp < 0;
			return cmp > 0;
		});

	this->SortedColumnIndex = col;
	this->SortAscending = ascending;
	this->PostRender();
}
#pragma region _GridView_
POINT GridView::GetGridViewUnderMouseItem(int x, int y, GridView* ct)
{
	auto l = ct->CalcScrollLayout();
	float _render_width = l.RenderWidth;
	float _render_height = l.RenderHeight;
	if (x < 0 || y < 0) return { -1,-1 };
	if (x >= (int)_render_width || y >= (int)_render_height) return { -1,-1 };

	auto font = ct->Font;
	auto head_font = HeadFont ? HeadFont : font;
	float font_height = font->FontHeight;
	float row_height = font_height + 2.0f;
	if (RowHeight != 0.0f)
	{
		row_height = RowHeight;
	}
	float head_font_height = head_font->FontHeight;
	float head_height = ct->HeadHeight == 0.0f ? head_font_height : ct->HeadHeight;
	if (y < head_height)
	{
		return { -1,-1 };
	}
	unsigned int s_x = 0;
	unsigned int s_y = 0;
	float yf = ct->HeadHeight == 0.0f ? row_height : ct->HeadHeight;
	float virtualX = (float)x + ct->ScrollXOffset;
	int xindex = -1;
	int yindex = -1;
	float acc = 0.0f;
	for (; s_x < ct->Columns.Count; s_x++)
	{
		float c_width = ct->Columns[s_x].Width;
		if (virtualX >= acc && virtualX < acc + c_width)
		{
			xindex = s_x;
			break;
		}
		acc += ct->Columns[s_x].Width;
	}
	const float virtualY = ((float)y - head_height) + ct->ScrollYOffset;
	if (virtualY >= 0.0f && row_height > 0.0f)
	{
		const int idx = (int)(virtualY / row_height);
		if (idx >= 0 && idx < ct->Rows.Count) yindex = idx;
	}
	return { xindex,yindex };
}

int GridView::HitTestHeaderColumn(int x, int y)
{
	auto l = this->CalcScrollLayout();
	const float headHeight = l.HeadHeight;
	const float renderWidth = l.RenderWidth;
	if (y < 0 || y >= (int)headHeight) return -1;
	if (x < 0 || x >= (int)renderWidth) return -1;

	const float virtualX = (float)x + this->ScrollXOffset;
	float xf = 0.0f;
	for (int i = 0; i < this->Columns.Count; i++)
	{
		float cWidth = this->Columns[i].Width;
		if (virtualX >= xf && virtualX < xf + cWidth)
			return i;
		xf += this->Columns[i].Width;
	}
	return -1;
}

int GridView::HitTestHeaderDivider(int x, int y)
{
	auto l = this->CalcScrollLayout();
	const float headHeight = l.HeadHeight;
	const float renderWidth = l.RenderWidth;
	if (y < 0 || y >= (int)headHeight) return -1;
	if (x < 0 || x >= (int)renderWidth) return -1;

	const float hitPx = 3.0f;
	const float virtualX = (float)x + this->ScrollXOffset;
	float xf = 0.0f;
	for (int i = 0; i < this->Columns.Count; i++)
	{
		const float cWidth = this->Columns[i].Width;
		const float rightEdge = xf + cWidth;
		if (std::abs(virtualX - rightEdge) <= hitPx)
			return i;

		xf += this->Columns[i].Width;
	}
	return -1;
}
D2D1_RECT_F GridView::GetGridViewScrollBlockRect(GridView* ct)
{
	auto absloc = ct->AbsLocation;
	auto size = ct->Size;
	auto l = ct->CalcScrollLayout();
	float _render_width = l.RenderWidth;
	float _render_height = l.RenderHeight;
	auto font = ct->Font;
	auto head_font = HeadFont ? HeadFont : font;
	float font_height = font->FontHeight;
	float row_height = font_height + 2.0f;
	if (RowHeight != 0.0f)
	{
		row_height = RowHeight;
	}
	float head_font_height = head_font->FontHeight;
	float head_height = ct->HeadHeight == 0.0f ? head_font_height : ct->HeadHeight;
	const float contentH = std::max(0.0f, _render_height - head_height);
	const float totalH = (row_height > 0.0f) ? (row_height * (float)ct->Rows.Count) : 0.0f;
	if (totalH > contentH && contentH > 0.0f)
	{
		float thumbH = _render_height * (contentH / totalH);
		const float minThumbH = _render_height * 0.1f;
		if (thumbH < minThumbH) thumbH = minThumbH;
		if (thumbH > _render_height) thumbH = _render_height;

		const float maxScrollY = std::max(0.0f, totalH - contentH);
		const float moveSpace = std::max(0.0f, _render_height - thumbH);
		float per = 0.0f;
		if (maxScrollY > 0.0f)
			per = std::clamp(ct->ScrollYOffset / maxScrollY, 0.0f, 1.0f);
		const float thumbTop = per * moveSpace;
		return { (float)absloc.x + _render_width, (float)absloc.y + thumbTop, 8.0f, thumbH };
	}
	return { 0,0,0,0 };
}
int GridView::GetGridViewRenderRowCount(GridView* ct)
{
	auto l = ct->CalcScrollLayout();
	return l.VisibleRows;
}
void GridView::DrawScroll()
{
	auto d2d = this->ParentForm->Render;
	auto abslocation = this->AbsLocation;
	auto font = this->Font;
	auto size = this->ActualSize();

	auto l = this->CalcScrollLayout();

	if (l.NeedV && this->Rows.Count > 0)
	{
		float _render_width = l.RenderWidth;
		float _render_height = l.RenderHeight;
		const float row_height = this->GetRowHeightPx();
		const float head_height = this->GetHeadHeightPx();
		const float contentH = std::max(0.0f, _render_height - head_height);
		const float totalH = (row_height > 0.0f) ? (row_height * (float)this->Rows.Count) : 0.0f;
		if (totalH > contentH && contentH > 0.0f)
		{
			float thumbH = _render_height * (contentH / totalH);
			const float minThumbH = _render_height * 0.1f;
			if (thumbH < minThumbH) thumbH = minThumbH;
			if (thumbH > _render_height) thumbH = _render_height;

			const float maxScrollY = std::max(0.0f, totalH - contentH);
			const float moveSpace = std::max(0.0f, _render_height - thumbH);
			float per = 0.0f;
			if (maxScrollY > 0.0f)
				per = std::clamp(this->ScrollYOffset / maxScrollY, 0.0f, 1.0f);
			const float thumbTop = per * moveSpace;

			d2d->FillRoundRect(abslocation.x + _render_width, abslocation.y, l.ScrollBarSize, _render_height, this->ScrollBackColor, 4.0f);
			d2d->FillRoundRect(abslocation.x + _render_width, abslocation.y + thumbTop, l.ScrollBarSize, thumbH, this->ScrollForeColor, 4.0f);
		}
	}

	if (l.NeedH)
		DrawHScroll(l);
	if (l.NeedH && l.NeedV)
		DrawCorner(l);
}

void GridView::DrawHScroll(const ScrollLayout& l)
{
	auto d2d = this->ParentForm->Render;
	auto abslocation = this->AbsLocation;

	const float barX = (float)abslocation.x;
	const float barY = (float)abslocation.y + l.RenderHeight;
	const float barW = l.RenderWidth;
	const float barH = l.ScrollBarSize;

	if (barW <= 0.0f || barH <= 0.0f) return;
	if (l.TotalColumnsWidth <= barW) return;

	const float maxScrollX = std::max(0.0f, l.TotalColumnsWidth - barW);
	float per = 0.0f;
	if (maxScrollX > 0.0f)
		per = std::clamp(this->ScrollXOffset / maxScrollX, 0.0f, 1.0f);

	float thumbW = (barW * barW) / l.TotalColumnsWidth;
	const float minThumbW = barW * 0.1f;
	if (thumbW < minThumbW) thumbW = minThumbW;
	if (thumbW > barW) thumbW = barW;

	const float moveSpace = barW - thumbW;
	const float thumbX = barX + (per * moveSpace);

	d2d->FillRoundRect(barX, barY, barW, barH, this->ScrollBackColor, 4.0f);
	d2d->FillRoundRect(thumbX, barY, thumbW, barH, this->ScrollForeColor, 4.0f);
}

void GridView::DrawCorner(const ScrollLayout& l)
{
	auto d2d = this->ParentForm->Render;
	auto abslocation = this->AbsLocation;
	const float x = (float)abslocation.x + l.RenderWidth;
	const float y = (float)abslocation.y + l.RenderHeight;
	d2d->FillRect(x, y, l.ScrollBarSize, l.ScrollBarSize, this->ScrollBackColor);
}

void GridView::SetScrollByPos(float yof)
{
	const auto d2d = this->ParentForm->Render;
	const auto absLocation = this->AbsLocation;
	const auto font = this->Font;
	const auto size = this->ActualSize();

	const int rowCount = this->Rows.Count;
	if (rowCount == 0) return;

	auto l = this->CalcScrollLayout();
	const float renderingHeight = l.RenderHeight;
	const float rowHeight = this->GetRowHeightPx();
	const float headHeight = this->GetHeadHeightPx();
	const float contentHeight = std::max(0.0f, renderingHeight - headHeight);
	const float totalHeight = (rowHeight > 0.0f) ? (rowHeight * (float)rowCount) : 0.0f;
	const float maxScrollY = std::max(0.0f, totalHeight - contentHeight);

	if (maxScrollY > 0.0f && contentHeight > 0.0f)
	{
		float thumbH = renderingHeight * (contentHeight / totalHeight);
		const float minThumbH = renderingHeight * 0.1f;
		if (thumbH < minThumbH) thumbH = minThumbH;
		if (thumbH > renderingHeight) thumbH = renderingHeight;

		const float moveSpace = std::max(0.0f, renderingHeight - thumbH);
		float grab = std::clamp(_vScrollThumbGrabOffsetY, 0.0f, thumbH);
		if (grab <= 0.0f) grab = thumbH * 0.5f;
		float target = yof - grab;
		target = std::clamp(target, 0.0f, moveSpace);
		const float per = (moveSpace > 0.0f) ? (target / moveSpace) : 0.0f;
		this->ScrollYOffset = std::clamp(per * maxScrollY, 0.0f, maxScrollY);
	}
	else
	{
		this->ScrollYOffset = 0.0f;
	}

	this->ScrollRowPosition = (rowHeight > 0.0f) ? (int)std::floor(this->ScrollYOffset / rowHeight) : 0;
	this->ScrollChanged(this);
}

void GridView::SetHScrollByPos(float xof)
{
	auto l = this->CalcScrollLayout();
	if (!l.NeedH) return;
	if (l.TotalColumnsWidth <= l.RenderWidth) { this->ScrollXOffset = 0.0f; return; }

	const float barW = l.RenderWidth;
	const float maxScrollX = std::max(0.0f, l.TotalColumnsWidth - barW);
	if (maxScrollX <= 0.0f) { this->ScrollXOffset = 0.0f; return; }

	float thumbW = (barW * barW) / l.TotalColumnsWidth;
	const float minThumbW = barW * 0.1f;
	if (thumbW < minThumbW) thumbW = minThumbW;
	if (thumbW > barW) thumbW = barW;

	const float moveSpace = barW - thumbW;
	if (moveSpace <= 0.0f) { this->ScrollXOffset = 0.0f; return; }

	float grab = std::clamp(_hScrollThumbGrabOffsetX, 0.0f, thumbW);
	if (grab <= 0.0f) grab = thumbW * 0.5f;
	float target = xof - grab;
	target = std::clamp(target, 0.0f, moveSpace);
	float per = target / moveSpace;
	this->ScrollXOffset = std::clamp(per * maxScrollX, 0.0f, maxScrollX);
}

void GridView::Update()
{
	if (this->IsVisual == false)return;
	bool isUnderMouse = this->ParentForm->UnderMouse == this;
	bool isSelected = this->ParentForm->Selected == this;
	auto d2d = this->ParentForm->Render;
	auto abslocation = this->AbsLocation;
	auto size = this->ActualSize();
	auto absRect = this->AbsRect;
	d2d->PushDrawRect(absRect.left, absRect.top, absRect.right - absRect.left, absRect.bottom - absRect.top);
	{
		d2d->FillRect(abslocation.x, abslocation.y, size.cx, size.cy, this->BackColor);
		if (this->Image)
		{
			this->RenderImage();
		}
		auto font = this->Font;
		auto head_font = HeadFont ? HeadFont : font;
		{
			auto l = this->CalcScrollLayout();
			float _render_width = l.RenderWidth;
			float _render_height = l.RenderHeight;
			float font_height = font->FontHeight;
			float head_font_height = head_font->FontHeight;
			float row_height = font_height + 2.0f;
			if (RowHeight != 0.0f)
			{
				row_height = RowHeight;
			}
			float text_top = (row_height - font_height) * 0.5f;
			if (text_top < 0) text_top = 0;
			if (this->Rows.Count <= 0)
			{
				this->ScrollYOffset = 0.0f;
				this->ScrollRowPosition = 0;
			}
			else
			{
				if (this->ScrollYOffset < 0.0f) this->ScrollYOffset = 0.0f;
				if (this->ScrollYOffset > l.MaxScrollY) this->ScrollYOffset = l.MaxScrollY;
				this->ScrollRowPosition = (row_height > 0.0f) ? (int)std::floor(this->ScrollYOffset / row_height) : 0;
				if (this->ScrollRowPosition < 0) this->ScrollRowPosition = 0;
				if (this->ScrollRowPosition >= this->Rows.Count) this->ScrollRowPosition = this->Rows.Count - 1;
			}
			if (this->ScrollXOffset < 0.0f) this->ScrollXOffset = 0.0f;
			if (this->ScrollXOffset > l.MaxScrollX) this->ScrollXOffset = l.MaxScrollX;

			int s_x = 0;
			int s_y = this->ScrollRowPosition;
			float head_height = this->HeadHeight == 0.0f ? head_font_height : this->HeadHeight;
			float row_offset = (row_height > 0.0f) ? std::fmod(this->ScrollYOffset, row_height) : 0.0f;
			float yf = head_height - row_offset;
			float xf = -this->ScrollXOffset;
			int i = s_x;
			for (; i < this->Columns.Count; i++)
			{
				float colW = this->Columns[i].Width;
				if (xf >= _render_width) break;
				if (xf + colW <= 0.0f) { xf += colW; continue; }

				float drawX = xf;
				float c_width = colW;
				if (drawX < 0.0f) { c_width += drawX; drawX = 0.0f; }
				if (drawX + c_width > _render_width) c_width = _render_width - drawX;
				if (c_width <= 0.0f) { xf += colW; continue; }
				const float clipX = drawX;
				const float clipW = c_width;
				drawX = xf;
				c_width = colW;

				auto ht = head_font->GetTextSize(this->Columns[i].Name);
				float draw_x_offset = (c_width - ht.width) / 2.0f;
				if (draw_x_offset < 0)draw_x_offset = 0;
				float draw_y_offset = (head_height - head_font_height) / 2.0f;
				if (draw_y_offset < 0)draw_y_offset = 0;
				d2d->PushDrawRect(abslocation.x + clipX, abslocation.y, clipW, head_height);
				{
					d2d->FillRect(abslocation.x + drawX, abslocation.y, c_width, head_height, this->HeadBackColor);
					d2d->DrawRect(abslocation.x + drawX, abslocation.y, c_width, head_height, this->HeadForeColor, 2.f);
					d2d->DrawString(this->Columns[i].Name,
						abslocation.x + drawX + draw_x_offset,
						abslocation.y + draw_y_offset,
						this->HeadForeColor, head_font);
				}
				d2d->PopDrawRect();
				xf += colW;
			}

			const int maxRows = l.VisibleRows;
			i = 0;
			for (int r = s_y; r < this->Rows.Count && i < maxRows; r++, i++)
			{
				GridViewRow& row = this->Rows[r];
				float clipY = yf;
				float clipH = row_height;
				if (clipY < head_height)
				{
					clipH -= (head_height - clipY);
					clipY = head_height;
				}
				if (clipY + clipH > _render_height)
					clipH = _render_height - clipY;
				if (clipH <= 0.0f)
				{
					yf += row_height;
					continue;
				}
				float xf = -this->ScrollXOffset;
				for (int c = s_x; c < this->Columns.Count; c++)
				{
					float colW = this->Columns[c].Width;
					if (xf >= _render_width) break;
					if (xf + colW <= 0.0f) { xf += colW; continue; }

					float drawX = xf;
					float c_width = colW;
					if (drawX < 0.0f) { c_width += drawX; drawX = 0.0f; }
					if (drawX + c_width > _render_width) c_width = _render_width - drawX;
					if (c_width <= 0.0f) { xf += colW; continue; }
					const float clipX = drawX;
					const float clipW = c_width;
					drawX = xf;
					c_width = colW;

					float _r_height = row_height;
					d2d->PushDrawRect(abslocation.x + clipX, abslocation.y + clipY, clipW, clipH);
					{
						switch (this->Columns[c].Type)
						{
						case ColumnType::Text:
						{
							if (c == this->SelectedColumnIndex && r == this->SelectedRowIndex)
							{
								if (this->Editing && this->EditingColumnIndex == c && this->EditingRowIndex == r && this->ParentForm->Selected == this)
								{
									D2D1_RECT_F cellLocal{};
									if (!TryGetCellRectLocal(c, r, cellLocal))
									{
										SaveCurrentEditingCell(true);
										this->Editing = false;
									}
									else
									{
										float renderHeight = _r_height - (this->EditTextMargin * 2.0f);
										if (renderHeight < 0.0f) renderHeight = 0.0f;

										EditEnsureSelectionInRange();
										EditUpdateScroll(clipW);

										auto textSize = font->GetTextSize(this->EditingText, FLT_MAX, renderHeight);
										float offsetY = (_r_height - textSize.height) * 0.5f;
										if (offsetY < 0.0f) offsetY = 0.0f;

										d2d->FillRect(abslocation.x + drawX, abslocation.y + yf, c_width, _r_height, this->EditBackColor);
										d2d->DrawRect(abslocation.x + drawX, abslocation.y + yf, c_width, _r_height, this->SelectedItemForeColor,
											r == this->UnderMouseRowIndex ? 1.0f : 0.5f);

										int sels = EditSelectionStart <= EditSelectionEnd ? EditSelectionStart : EditSelectionEnd;
										int sele = EditSelectionEnd >= EditSelectionStart ? EditSelectionEnd : EditSelectionStart;
										int selLen = sele - sels;
										auto selRange = font->HitTestTextRange(this->EditingText, (UINT32)sels, (UINT32)selLen);

										if (selLen != 0)
										{
											for (auto sr : selRange)
											{
												d2d->FillRect(
													sr.left + abslocation.x + drawX + this->EditTextMargin - this->EditOffsetX,
													(sr.top + abslocation.y + yf) + offsetY,
													sr.width, sr.height,
													this->EditSelectedBackColor);
											}
										}
										else
										{
											d2d->DrawLine(
												{ selRange[0].left + abslocation.x + drawX + this->EditTextMargin - this->EditOffsetX,(selRange[0].top + abslocation.y + yf) - offsetY },
												{ selRange[0].left + abslocation.x + drawX + this->EditTextMargin - this->EditOffsetX,(selRange[0].top + abslocation.y + yf + selRange[0].height) + offsetY },
												Colors::Black);
										}

										auto lot = Factory::CreateStringLayout(this->EditingText, FLT_MAX, renderHeight, font->FontObject);
										if (selLen != 0)
										{
											d2d->DrawStringLayoutEffect(lot,
												(float)abslocation.x + drawX + this->EditTextMargin - this->EditOffsetX, ((float)abslocation.y + yf) + offsetY,
												this->EditForeColor,
												DWRITE_TEXT_RANGE{ (UINT32)sels, (UINT32)selLen },
												this->EditSelectedForeColor,
												font);
										}
										else
										{
											d2d->DrawStringLayout(lot,
												(float)abslocation.x + drawX + this->EditTextMargin - this->EditOffsetX, ((float)abslocation.y + yf) + offsetY,
												this->EditForeColor);
										}
										lot->Release();
									}
								}
								else
								{
									d2d->FillRect(abslocation.x + drawX, abslocation.y + yf, c_width, _r_height, this->SelectedItemBackColor);
									d2d->DrawRect(abslocation.x + drawX, abslocation.y + yf, c_width, _r_height, this->SelectedItemForeColor,
										r == this->UnderMouseRowIndex ? 1.0f : 0.5f);
									if (row.Cells.Count > c)
										d2d->DrawString(row.Cells[c].Text,
											abslocation.x + drawX + 1.0f,
											abslocation.y + yf + text_top,
											this->SelectedItemForeColor, font);
								}
							}
							else if (c == this->UnderMouseColumnIndex && r == this->UnderMouseRowIndex)
							{
								d2d->FillRect(abslocation.x + drawX, abslocation.y + yf, c_width, _r_height, this->UnderMouseItemBackColor);
								d2d->DrawRect(abslocation.x + drawX, abslocation.y + yf, c_width, _r_height, this->UnderMouseItemForeColor,
									r == this->UnderMouseRowIndex ? 1.0f : 0.5f);
								if (row.Cells.Count > c)
									d2d->DrawString(row.Cells[c].Text,
										abslocation.x + drawX + 1.0f,
										abslocation.y + yf + text_top,
										this->UnderMouseItemForeColor, font);
							}
							else
							{
								d2d->DrawRect(abslocation.x + drawX, abslocation.y + yf, c_width, _r_height, this->ForeColor,
									r == this->UnderMouseRowIndex ? 1.0f : 0.5f);
								if (row.Cells.Count > c)
									d2d->DrawString(row.Cells[c].Text,
										abslocation.x + drawX + 1.0f,
										abslocation.y + yf + text_top,
										this->ForeColor, font);
							}
						}
						break;
						case ColumnType::Button:
						{
							// Button：独立样式（WinForms-like），不使用普通单元格的"选中底色"
							const bool isHot = (c == this->UnderMouseColumnIndex && r == this->UnderMouseRowIndex);
							const bool isPressed = (this->_buttonMouseDown && isHot &&
								this->_buttonDownColumnIndex == c && this->_buttonDownRowIndex == r &&
								(GetAsyncKeyState(VK_LBUTTON) & 0x8000));

							D2D1_COLOR_F back = this->ButtonBackColor;
							if (isPressed) back = this->ButtonPressedBackColor;
							else if (isHot) back = this->ButtonHoverBackColor;

							d2d->FillRect(abslocation.x + drawX, abslocation.y + yf, c_width, _r_height, back);

							// 3D Border: raised vs sunken
							const float px = 1.0f;
							d2d->DrawRect(abslocation.x + drawX, abslocation.y + yf, c_width, _r_height, this->ButtonBorderDarkColor, 1.0f);
							if (c_width > 2.0f && _r_height > 2.0f)
							{
								auto innerColor = isPressed ? this->ScrollForeColor : this->ButtonBorderLightColor;
								d2d->DrawRect(abslocation.x + drawX + px, abslocation.y + yf + px,
									c_width - (px * 2.0f), _r_height - (px * 2.0f),
									innerColor, 1.0f);
							}

							// Text center (+ pressed offset)
							// 使用列的ButtonText作为按钮文字
							const std::wstring& buttonText = this->Columns[c].ButtonText;
							if (!buttonText.empty())
							{
								auto textSize = font->GetTextSize(buttonText);
								float tx = (c_width - textSize.width) * 0.5f;
								float ty = (_r_height - textSize.height) * 0.5f;
								if (tx < 0.0f) tx = 0.0f;
								if (ty < 0.0f) ty = 0.0f;
								if (isPressed) { tx += 1.0f; ty += 1.0f; }
								d2d->DrawString(buttonText,
									abslocation.x + drawX + tx,
									abslocation.y + yf + ty,
									this->ForeColor, font);
							}
						}
						break;
						case ColumnType::ComboBox:
						{
							EnsureComboBoxCellDefaultSelection(c, r);
							D2D1_COLOR_F back = D2D1_COLOR_F{ 0,0,0,0 };
							D2D1_COLOR_F border = this->ForeColor;
							D2D1_COLOR_F fore = this->ForeColor;
							bool fill = false;

							if (c == this->SelectedColumnIndex && r == this->SelectedRowIndex)
							{
								back = this->SelectedItemBackColor;
								border = this->SelectedItemForeColor;
								fore = this->SelectedItemForeColor;
								fill = true;
							}
							else if (c == this->UnderMouseColumnIndex && r == this->UnderMouseRowIndex)
							{
								back = this->UnderMouseItemBackColor;
								border = this->UnderMouseItemForeColor;
								fore = this->UnderMouseItemForeColor;
								fill = true;
							}

							if (fill)
								d2d->FillRect(abslocation.x + drawX, abslocation.y + yf, c_width, _r_height, back);
							d2d->DrawRect(abslocation.x + drawX, abslocation.y + yf, c_width, _r_height, border,
								r == this->UnderMouseRowIndex ? 1.0f : 0.5f);
							if (row.Cells.Count > c)
							{
								d2d->DrawString(row.Cells[c].Text,
									abslocation.x + drawX + 4.0f,
									abslocation.y + yf + text_top,
									fore, font);
							}

							// Draw drop arrow on right
							{
								const float h = _r_height;
								float iconSize = h * 0.38f;
								if (iconSize < 8.0f) iconSize = 8.0f;
								if (iconSize > 14.0f) iconSize = 14.0f;
								const float padRight = 8.0f;
								const float cx = abslocation.x + drawX + c_width - padRight - iconSize * 0.5f;
								const float cy = abslocation.y + yf + h * 0.5f;
								const float half = iconSize * 0.5f;
								const float triH = iconSize * 0.55f;
								D2D1_TRIANGLE tri{};
								tri.point1 = D2D1::Point2F(cx - half, cy - triH * 0.5f);
								tri.point2 = D2D1::Point2F(cx + half, cy - triH * 0.5f);
								tri.point3 = D2D1::Point2F(cx, cy + triH * 0.5f);
								d2d->FillTriangle(tri, fore);
							}
						}
						break;
						case ColumnType::Image:
						{
							float _size = c_width < row_height ? c_width : row_height;
							float left = (c_width - _size) / 2.0f;
							float top = (row_height - _size) / 2.0f;
							if (c == this->UnderMouseColumnIndex && r == this->UnderMouseRowIndex)
							{
								d2d->FillRect(abslocation.x + drawX, abslocation.y + yf, c_width, _r_height, this->UnderMouseItemBackColor);
								d2d->DrawRect(abslocation.x + drawX, abslocation.y + yf, c_width, _r_height, this->UnderMouseItemForeColor,
									r == this->UnderMouseRowIndex ? 1.0f : 0.5f);
								if (row.Cells.Count > c)
								{
									if (row.Cells[c].Image)
										d2d->DrawBitmap(row.Cells[c].Image,
											abslocation.x + drawX + left,
											abslocation.y + yf + top,
											_size, _size
										);
								}
							}
							else
							{
								d2d->DrawRect(abslocation.x + drawX, abslocation.y + yf, c_width, _r_height, this->ForeColor,
									r == this->UnderMouseRowIndex ? 1.0f : 0.5f);
								if (row.Cells.Count > c)
								{
									if (row.Cells[c].Image)
										d2d->DrawBitmap(row.Cells[c].Image,
											abslocation.x + drawX + left,
											abslocation.y + yf + top,
											_size, _size
										);
								}
							}
						}
						break;
						case ColumnType::Check:
						{
							float _size = c_width < row_height ? c_width : row_height;
							if (_size > 24)_size = 24;
							float left = (c_width - _size) / 2.0f;
							float top = (row_height - _size) / 2.0f;
							float _rsize = _size;
							if (c == this->UnderMouseColumnIndex && r == this->UnderMouseRowIndex)
							{
								d2d->FillRect(abslocation.x + drawX, abslocation.y + yf, c_width, _r_height, this->UnderMouseItemBackColor);
								d2d->DrawRect(abslocation.x + drawX, abslocation.y + yf, c_width, _r_height, this->UnderMouseItemForeColor,
									r == this->UnderMouseRowIndex ? 1.0f : 0.5f);
								if (row.Cells.Count > c)
								{
									d2d->DrawRect(
										abslocation.x + drawX + left + (_rsize * 0.2),
										abslocation.y + yf + top + (_rsize * 0.2),
										_rsize * 0.6, _rsize * 0.6,
										this->ForeColor);
									if (row.Cells[c].Tag)
									{
										d2d->FillRect(
											abslocation.x + drawX + left + (_rsize * 0.35),
											abslocation.y + yf + top + (_rsize * 0.35),
											_rsize * 0.3, _rsize * 0.3,
											this->ForeColor);
									}
								}
							}
							else
							{
								d2d->DrawRect(abslocation.x + drawX, abslocation.y + yf, c_width, _r_height, this->ForeColor,
									r == this->UnderMouseRowIndex ? 1.0f : 0.5f);
								if (row.Cells.Count > c)
								{
									d2d->DrawRect(
										abslocation.x + drawX + left + (_rsize * 0.2),
										abslocation.y + yf + top + (_rsize * 0.2),
										_rsize * 0.6, _rsize * 0.6,
										this->ForeColor);
									if (row.Cells[c].Tag)
									{
										d2d->FillRect(
											abslocation.x + drawX + left + (_rsize * 0.35),
											abslocation.y + yf + top + (_rsize * 0.35),
											_rsize * 0.3, _rsize * 0.3,
											this->ForeColor);
									}
								}
							}
						}
						break;
						default:
							break;
						}
					}
					d2d->PopDrawRect();
					xf += colW;
				}
				yf += row_height;
			}
			
			// 渲染新行区域（如果启用）
			if (this->AllowUserToAddRows && this->Columns.Count > 0)
			{
				float newRowY = yf;
				if (newRowY < head_height) newRowY = head_height;
				
				// 确保新行在可视区域内
				if (newRowY < _render_height)
				{
					float newRowHeight = row_height;
					if (newRowY + newRowHeight > _render_height)
						newRowHeight = _render_height - newRowY;
					
					if (newRowHeight > 0.0f)
					{
						float xf = -this->ScrollXOffset;
						for (int c = 0; c < this->Columns.Count; c++)
						{
							float colW = this->Columns[c].Width;
							if (xf >= _render_width) break;
							if (xf + colW <= 0.0f) { xf += colW; continue; }

							float drawX = xf;
							float c_width = colW;
							if (drawX < 0.0f) { c_width += drawX; drawX = 0.0f; }
							if (drawX + c_width > _render_width) c_width = _render_width - drawX;
							if (c_width <= 0.0f) { xf += colW; continue; }

							const float clipX = drawX;
							const float clipW = c_width;

							d2d->PushDrawRect(abslocation.x + clipX, abslocation.y + newRowY, clipW, newRowHeight);
							{
								// 绘制新行背景
								d2d->FillRect(abslocation.x + drawX, abslocation.y + newRowY, c_width, newRowHeight, this->NewRowBackColor);
								
								// 绘制新行单元格内容（空单元格样式）
								if (c == 0)
								{
									// 在第一列显示新行指示符 (*)
									float asteriskSize = font_height * 0.5f;
									float asteriskX = abslocation.x + drawX + text_top;
									float asteriskY = abslocation.y + newRowY + text_top;
									
									// 绘制星号
									d2d->DrawString(L"*",
										asteriskX,
										asteriskY,
										this->NewRowIndicatorColor, font);
									
									// 绘制提示文字
									std::wstring hintText = L"点击添加新行";
									auto hintSize = font->GetTextSize(hintText);
									d2d->DrawString(hintText,
										asteriskX + asteriskSize + 4.0f,
										asteriskY,
										this->NewRowForeColor, font);
								}
								
								// 绘制单元格边框
								d2d->DrawRect(abslocation.x + drawX, abslocation.y + newRowY, c_width, newRowHeight, this->NewRowForeColor, 1.0f);
							}
							d2d->PopDrawRect();
							xf += colW;
						}
					}
				}
			}
			
			d2d->PushDrawRect(
				(float)abslocation.x,
				(float)abslocation.y,
				(float)size.cx,
				(float)size.cy);
			{
				if (this->ParentForm->UnderMouse == this)
				{
					d2d->DrawRect(abslocation.x, abslocation.y, size.cx, size.cy, this->BolderColor, 4);
				}
				else
				{
					d2d->DrawRect(abslocation.x, abslocation.y, size.cx, size.cy, this->BolderColor, 2);
				}
			}
			d2d->PopDrawRect();
			this->DrawScroll();
		}
		d2d->DrawRect(abslocation.x, abslocation.y, size.cx, size.cy, this->BolderColor, this->Boder);
	}
	if (!this->Enable)
	{
		d2d->FillRect(abslocation.x, abslocation.y, size.cx, size.cy, { 1.0f ,1.0f ,1.0f ,0.5f });
	}
	d2d->PopDrawRect();
}

bool GridView::IsNewRowArea(int x, int y)
{
	if (!this->AllowUserToAddRows) return false;
	if (this->Columns.Count <= 0) return false;

	auto l = this->CalcScrollLayout();
	const float headHeight = l.HeadHeight;
	const float renderWidth = l.RenderWidth;
	const float renderHeight = l.RenderHeight;

	// 检查是否在渲染区域内
	if (x < 0 || x >= (int)renderWidth) return false;
	if (y < 0 || y >= (int)renderHeight) return false;

	// 检查是否在表头下方
	if (y <= (int)headHeight) return false;

	// 计算新行区域的位置
	const float rowHeight = this->GetRowHeightPx();
	const float totalRowsHeight = rowHeight * (float)this->Rows.Count;
	const float newRowY = headHeight + totalRowsHeight;

	// 检查鼠标是否在新行区域内
	const float virtualY = ((float)y - headHeight) + this->ScrollYOffset;
	if (virtualY >= totalRowsHeight && virtualY < totalRowsHeight + rowHeight)
	{
		return true;
	}

	return false;
}

int GridView::HitTestNewRow(int x, int y, int& outColumnIndex)
{
	if (!this->AllowUserToAddRows) return -1;
	if (this->Columns.Count <= 0) return -1;

	auto l = this->CalcScrollLayout();
	const float headHeight = l.HeadHeight;
	const float renderWidth = l.RenderWidth;

	if (x < 0 || x >= (int)renderWidth) return -1;
	if (y <= (int)headHeight) return -1;

	const float rowHeight = this->GetRowHeightPx();
	const float totalRowsHeight = rowHeight * (float)this->Rows.Count;
	const float virtualY = ((float)y - headHeight) + this->ScrollYOffset;

	// 检查是否在新行区域内
	if (virtualY < totalRowsHeight || virtualY >= totalRowsHeight + rowHeight)
		return -1;

	// 确定鼠标在哪一列
	const float virtualX = (float)x + this->ScrollXOffset;
	float acc = 0.0f;
	for (int i = 0; i < this->Columns.Count; i++)
	{
		if (virtualX >= acc && virtualX < acc + this->Columns[i].Width)
		{
			outColumnIndex = i;
			return this->Rows.Count;  // 返回Rows.Count作为新行的索引
		}
		acc += this->Columns[i].Width;
	}

	return -1;
}

void GridView::AddNewRow()
{
	if (!this->AllowUserToAddRows) return;

	// 创建新行
	GridViewRow newRow;
	for (int i = 0; i < this->Columns.Count; i++)
	{
		CellValue cell;
		newRow.Cells.Add(cell);
	}
	
	// 添加到Rows列表
	int newRowIndex = this->Rows.Count;
	this->Rows.Add(newRow);

	// 触发新行添加事件
	this->OnUserAddedRow(this, newRowIndex);

	// 自动选中新行的第一列并开始编辑
	if (this->Columns.Count > 0)
	{
		this->SelectedColumnIndex = 0;
		this->SelectedRowIndex = newRowIndex;
		this->SelectionChanged(this);
		StartEditingCell(0, newRowIndex);
	}

	this->PostRender();
}

void GridView::ReSizeRows(int count)
{
	if (count < 0) count = 0;
	this->Rows.resize((size_t)count);
}
void GridView::AutoSizeColumn(int col)
{
	if (this->Columns.Count > col)
	{
		auto font = this->Font;
		float font_height = font->FontHeight;
		float row_height = font_height + 2.0f;
		if (RowHeight != 0.0f)
		{
			row_height = RowHeight;
		}
		auto& column = this->Columns[col];
		column.Width = 10.0f;
		for (int i = 0; i < this->Rows.Count; i++)
		{
			auto& r = this->Rows[i];
			if (r.Cells.Count > col)
			{
				if (this->Columns[col].Type == ColumnType::Text ||
					this->Columns[col].Type == ColumnType::Button ||
					this->Columns[col].Type == ColumnType::ComboBox)
				{
					// Button列使用列的ButtonText来计算宽度
					std::wstring textToMeasure;
					if (this->Columns[col].Type == ColumnType::Button && !this->Columns[col].ButtonText.empty())
					{
						textToMeasure = this->Columns[col].ButtonText;
					}
					else
					{
						textToMeasure = r.Cells[col].Text;
					}
					auto width = font->GetTextSize(textToMeasure.c_str()).width;
					if (column.Width < width)
					{
						column.Width = width;
					}
				}
				else
				{
					column.Width = row_height;
				}
			}
		}
	}
}
void GridView::ToggleCheckState(int col, int row)
{
	auto& cell = this->Rows[row].Cells[col];
	cell.Tag = __int64(!cell.Tag);
	this->OnGridViewCheckStateChanged(this, col, row, cell.Tag != 0);
}

void GridView::EnsureComboBoxCellDefaultSelection(int col, int row)
{
	if (col < 0 || row < 0) return;
	if (col >= this->Columns.Count || row >= this->Rows.Count) return;
	if (this->Columns[col].Type != ColumnType::ComboBox) return;

	auto& column = this->Columns[col];
	if (column.ComboBoxItems.Count <= 0) return;
	auto& rowObj = this->Rows[row];
	if (rowObj.Cells.Count <= col)
		rowObj.Cells.resize((size_t)col + 1);
	auto& cell = rowObj.Cells[col];

	const __int64 idx = cell.Tag;
	if (idx < 0 || idx >= column.ComboBoxItems.Count)
	{
		cell.Tag = 0;
		cell.Text = column.ComboBoxItems[0];
	}
	else
	{
		// Keep Text in sync with index if needed
		const auto& t = column.ComboBoxItems[(int)idx];
		if (cell.Text != t)
			cell.Text = t;
	}
}

void GridView::CloseComboBoxEditor()
{
	if (!this->_cellComboBox) return;

	if (this->ParentForm && this->ParentForm->ForegroundControl == this->_cellComboBox)
		this->ParentForm->ForegroundControl = NULL;

	this->_cellComboBox->Expand = false;
	this->_cellComboBoxColumnIndex = -1;
	this->_cellComboBoxRowIndex = -1;
}

void GridView::ToggleComboBoxEditor(int col, int row)
{
	if (col < 0 || row < 0) return;
	if (col >= this->Columns.Count || row >= this->Rows.Count) return;
	if (!this->ParentForm) return;
	if (this->Columns[col].Type != ColumnType::ComboBox) return;

	EnsureComboBoxCellDefaultSelection(col, row);

	// If same cell and already open => close
	if (this->_cellComboBox &&
		this->ParentForm->ForegroundControl == this->_cellComboBox &&
		this->_cellComboBox->Expand &&
		this->_cellComboBoxColumnIndex == col &&
		this->_cellComboBoxRowIndex == row)
	{
		CloseComboBoxEditor();
		this->ParentForm->Invalidate(true);
		this->PostRender();
		return;
	}

	// Commit text edit when switching modes
	if (this->Editing)
	{
		SaveCurrentEditingCell(true);
		this->Editing = false;
		this->EditingColumnIndex = -1;
		this->EditingRowIndex = -1;
		this->EditingText.clear();
		this->EditingOriginalText.clear();
		this->EditSelectionStart = this->EditSelectionEnd = 0;
		this->EditOffsetX = 0.0f;
	}

	this->SelectedColumnIndex = col;
	this->SelectedRowIndex = row;
	this->SelectionChanged(this);

	if (!this->_cellComboBox)
	{
		this->_cellComboBox = new ComboBox(L"", 0, 0, 120, 24);
	}

	D2D1_RECT_F cellLocal{};
	if (!TryGetCellRectLocal(col, row, cellLocal)) return;

	const auto abs = this->AbsLocation;
	const int x = (int)std::round((float)abs.x + cellLocal.left);
	const int y = (int)std::round((float)abs.y + cellLocal.top);
	const int w = (int)std::round(cellLocal.right - cellLocal.left);
	const int h = (int)std::round(cellLocal.bottom - cellLocal.top);

	auto& column = this->Columns[col];
	auto& rowObj = this->Rows[row];
	if (rowObj.Cells.Count <= col)
		rowObj.Cells.resize((size_t)col + 1);
	auto& cell = rowObj.Cells[col];

	this->_cellComboBox->ParentForm = this->ParentForm;
	this->_cellComboBox->Font = this->Font;
	this->_cellComboBox->Location = POINT{ x, y };
	this->_cellComboBox->Size = SIZE{ (w > 0 ? w : 1), (h > 0 ? h : 1) };
	this->_cellComboBox->Items = column.ComboBoxItems;
	this->_cellComboBox->SelectedIndex = (int)cell.Tag;
	if (this->_cellComboBox->SelectedIndex < 0) this->_cellComboBox->SelectedIndex = 0;
	if (this->_cellComboBox->SelectedIndex >= this->_cellComboBox->Items.Count)
		this->_cellComboBox->SelectedIndex = (this->_cellComboBox->Items.Count > 0) ? (this->_cellComboBox->Items.Count - 1) : 0;
	this->_cellComboBox->Text = (this->_cellComboBox->Items.Count > 0) ? this->_cellComboBox->Items[this->_cellComboBox->SelectedIndex] : L"";

	int expandCount = 4;
	if (this->_cellComboBox->Items.Count > 0)
		expandCount = std::min(4, this->_cellComboBox->Items.Count);
	if (expandCount < 1) expandCount = 1;
	this->_cellComboBox->ExpandCount = expandCount;

	this->_cellComboBox->OnSelectionChanged.Clear();
	this->_cellComboBox->OnSelectionChanged += [this, col, row](Control* sender)
	{
		(void)sender;
		if (col < 0 || row < 0) return;
		if (col >= this->Columns.Count || row >= this->Rows.Count) return;
		if (this->Columns[col].Type != ColumnType::ComboBox) return;
		auto& column2 = this->Columns[col];
		if (!this->_cellComboBox) return;
		if (column2.ComboBoxItems.Count <= 0) return;
		int idx = this->_cellComboBox->SelectedIndex;
		if (idx < 0) idx = 0;
		if (idx >= column2.ComboBoxItems.Count) idx = column2.ComboBoxItems.Count - 1;
		auto& cell2 = this->Rows[row].Cells[col];
		cell2.Tag = (__int64)idx;
		cell2.Text = column2.ComboBoxItems[idx];
		this->OnGridViewComboBoxSelectionChanged(this, col, row, idx, cell2.Text);
		this->PostRender();
	};

	this->_cellComboBoxColumnIndex = col;
	this->_cellComboBoxRowIndex = row;
	this->_cellComboBox->Expand = true;
	this->ParentForm->ForegroundControl = this->_cellComboBox;
	this->ParentForm->Invalidate(true);
	this->PostRender();
}
void GridView::StartEditingCell(int col, int row)
{
	if (col < 0 || row < 0) return;
	if (col >= this->Columns.Count || row >= this->Rows.Count) return;

	if (this->Editing && (this->EditingColumnIndex != col || this->EditingRowIndex != row))
	{
		SaveCurrentEditingCell(true);
	}

	this->SelectedColumnIndex = col;
	this->SelectedRowIndex = row;
	this->SelectionChanged(this);

	if (IsEditableTextCell(col, row))
	{
		this->Editing = true;
		this->EditingColumnIndex = col;
		this->EditingRowIndex = row;
		this->EditingText = this->Rows[row].Cells[col].Text;
		this->EditingOriginalText = this->EditingText;
		this->EditSelectionStart = 0;
		this->EditSelectionEnd = (int)this->EditingText.size();
		this->EditOffsetX = 0.0f;
		this->ParentForm->Selected = this;
		EditSetImeCompositionWindow();
	}
	else
	{
		this->Editing = false;
		this->EditingColumnIndex = -1;
		this->EditingRowIndex = -1;
	}
}
void GridView::CancelEditing(bool revert)
{
	if (this->Editing)
	{
		if (revert && this->EditingRowIndex >= 0 && this->EditingColumnIndex >= 0 &&
			this->EditingRowIndex < this->Rows.Count && this->EditingColumnIndex < this->Columns.Count)
		{
			this->Rows[this->EditingRowIndex].Cells[this->EditingColumnIndex].Text = this->EditingOriginalText;
		}
		else
		{
			SaveCurrentEditingCell(true);
		}
	}
	this->Editing = false;
	this->EditingColumnIndex = -1;
	this->EditingRowIndex = -1;
	this->EditingText.clear();
	this->EditingOriginalText.clear();
	this->EditSelectionStart = this->EditSelectionEnd = 0;
	this->EditOffsetX = 0.0f;
	this->ParentForm->Selected = this;
	this->SelectedColumnIndex = -1;
	this->SelectedRowIndex = -1;
}
void GridView::SaveCurrentEditingCell(bool commit)
{
	if (!this->Editing) return;
	if (!commit) return;
	if (this->EditingColumnIndex < 0 || this->EditingRowIndex < 0) return;
	if (this->EditingRowIndex >= this->Rows.Count) return;
	if (this->EditingColumnIndex >= this->Columns.Count) return;
	this->Rows[this->EditingRowIndex].Cells[this->EditingColumnIndex].Text = this->EditingText;
}
void GridView::AdjustScrollPosition()
{
	auto l = this->CalcScrollLayout();
	const float rowH = this->GetRowHeightPx();
	const float headH = this->GetHeadHeightPx();
	const float contentH = std::max(0.0f, l.RenderHeight - headH);
	const float totalH = (rowH > 0.0f) ? (rowH * (float)this->Rows.Count) : 0.0f;
	const float maxScrollY = std::max(0.0f, totalH - contentH);

	if (this->SelectedRowIndex < 0 || this->SelectedRowIndex >= this->Rows.Count) return;
	if (rowH <= 0.0f) return;

	const float rowTop = rowH * (float)this->SelectedRowIndex;
	const float rowBottom = rowTop + rowH;
	const float viewTop = this->ScrollYOffset;
	const float viewBottom = this->ScrollYOffset + contentH;

	if (rowTop < viewTop)
		this->ScrollYOffset = rowTop;
	else if (rowBottom > viewBottom)
		this->ScrollYOffset = rowBottom - contentH;

	this->ScrollYOffset = std::clamp(this->ScrollYOffset, 0.0f, maxScrollY);
	this->ScrollRowPosition = (int)std::floor(this->ScrollYOffset / rowH);
}
bool GridView::CanScrollDown()
{
	auto l = this->CalcScrollLayout();
	return this->ScrollYOffset < l.MaxScrollY;
}
void GridView::UpdateUnderMouseIndices(int xof, int yof)
{
	POINT undermouseIndex = GetGridViewUnderMouseItem(xof, yof, this);
	this->UnderMouseColumnIndex = undermouseIndex.x;
	this->UnderMouseRowIndex = undermouseIndex.y;
}
void GridView::ChangeEditionSelected(int col, int row)
{
	if (this->Editing)
	{
		SaveCurrentEditingCell(true);
	}
	StartEditingCell(col, row);
}
void GridView::HandleDropFiles(WPARAM wParam)
{
	HDROP hDropInfo = HDROP(wParam);
	UINT uFileNum = DragQueryFile(hDropInfo, 0xffffffff, NULL, 0);
	TCHAR strFileName[MAX_PATH];
	List<std::wstring> files;

	for (UINT i = 0; i < uFileNum; i++)
	{
		DragQueryFile(hDropInfo, i, strFileName, MAX_PATH);
		files.Add(strFileName);
	}
	DragFinish(hDropInfo);

	if (files.Count > 0)
	{
		this->OnDropFile(this, files);
	}
}
void GridView::HandleMouseWheel(WPARAM wParam, int xof, int yof)
{
	bool needUpdate = false;
	int delta = GET_WHEEL_DELTA_WPARAM(wParam);
	auto l = this->CalcScrollLayout();

	if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) && l.NeedH)
	{
		float step = 40.0f;
		if (delta < 0) this->ScrollXOffset += step;
		else this->ScrollXOffset -= step;
		if (this->ScrollXOffset < 0.0f) this->ScrollXOffset = 0.0f;
		if (this->ScrollXOffset > l.MaxScrollX) this->ScrollXOffset = l.MaxScrollX;
		needUpdate = true;

		UpdateUnderMouseIndices(xof, yof);
		MouseEventArgs event_obj(MouseButtons::None, 0, xof, yof, delta);
		this->OnMouseWheel(this, event_obj);
		if (needUpdate) this->PostRender();
		return;
	}

	if (delta < 0)
	{
		if (CanScrollDown())
		{
			needUpdate = true;
			const float rowH = this->GetRowHeightPx();
			const float step = (rowH > 0.0f) ? rowH : 16.0f;
			this->ScrollYOffset = std::min(this->ScrollYOffset + step, l.MaxScrollY);
			this->ScrollRowPosition = (rowH > 0.0f) ? (int)std::floor(this->ScrollYOffset / rowH) : 0;
			this->ScrollChanged(this);
		}
	}
	else
	{
		if (this->ScrollYOffset > 0.0f)
		{
			needUpdate = true;
			const float rowH = this->GetRowHeightPx();
			const float step = (rowH > 0.0f) ? rowH : 16.0f;
			this->ScrollYOffset = std::max(0.0f, this->ScrollYOffset - step);
			this->ScrollRowPosition = (rowH > 0.0f) ? (int)std::floor(this->ScrollYOffset / rowH) : 0;
			this->ScrollChanged(this);
		}
	}

	UpdateUnderMouseIndices(xof, yof);
	MouseEventArgs event_obj(MouseButtons::None, 0, xof, yof, delta);
	this->OnMouseWheel(this, event_obj);

	if (needUpdate)
	{
		this->PostRender();
	}
}
void GridView::HandleMouseMove(int xof, int yof)
{
	this->ParentForm->UnderMouse = this;
	bool needUpdate = false;

	if (this->_resizingColumn)
	{
		float dx = (float)xof - this->_resizeStartX;
		float newWidth = this->_resizeStartWidth + dx;
		if (newWidth < this->_minColumnWidth) newWidth = this->_minColumnWidth;
		if (this->_resizeColumnIndex >= 0 && this->_resizeColumnIndex < this->Columns.Count)
		{
			if (this->Columns[this->_resizeColumnIndex].Width != newWidth)
			{
				this->Columns[this->_resizeColumnIndex].Width = newWidth;
				needUpdate = true;
			}
		}
		MouseEventArgs event_obj(MouseButtons::None, 0, xof, yof, 0);
		this->OnMouseMove(this, event_obj);
		if (needUpdate) this->PostRender();
		return;
	}

	if (this->InScroll)
	{
		needUpdate = true;
		SetScrollByPos(yof);
	}
	else if (this->InHScroll)
	{
		needUpdate = true;
		SetHScrollByPos((float)xof);
	}
	else
	{
		if (this->Editing && this->ParentForm->Selected == this && (GetAsyncKeyState(VK_LBUTTON) & 0x8000))
		{
			D2D1_RECT_F rect{};
			if (TryGetCellRectLocal(this->EditingColumnIndex, this->EditingRowIndex, rect))
			{
				float cellWidth = rect.right - rect.left;
				float cellHeight = rect.bottom - rect.top;
				float lx = (float)xof - rect.left;
				float ly = (float)yof - rect.top;
				this->EditSelectionEnd = EditHitTestTextPosition(cellWidth, cellHeight, lx, ly);
				EditUpdateScroll(cellWidth);
				needUpdate = true;
			}
		}
		POINT undermouseIndex = GetGridViewUnderMouseItem(xof, yof, this);
		if (this->UnderMouseColumnIndex != undermouseIndex.x ||
			this->UnderMouseRowIndex != undermouseIndex.y)
		{
			needUpdate = true;
		}
		this->UnderMouseColumnIndex = undermouseIndex.x;
		this->UnderMouseRowIndex = undermouseIndex.y;

		// 检查是否在新行区域
		if (this->AllowUserToAddRows)
		{
			int newRowCol = -1;
			int hitResult = HitTestNewRow(xof, yof, newRowCol);
			bool isUnderNewRow = (hitResult >= 0 && newRowCol >= 0);
			if (this->_isUnderNewRow != isUnderNewRow)
			{
				this->_isUnderNewRow = isUnderNewRow;
				this->_newRowAreaHitTest = newRowCol;
				needUpdate = true;
			}
		}
	}

	MouseEventArgs event_obj(MouseButtons::None, 0, xof, yof, 0);
	this->OnMouseMove(this, event_obj);

	if (needUpdate)
	{
		this->PostRender();
	}
}
void GridView::HandleLeftButtonDown(int xof, int yof)
{
	auto lastSelected = this->ParentForm->Selected;
	this->ParentForm->Selected = this;

	if (lastSelected && lastSelected != this)
	{
		lastSelected->PostRender();
	}

	auto l = this->CalcScrollLayout();
	const int renderW = (int)l.RenderWidth;
	const int renderH = (int)l.RenderHeight;

	if (l.NeedH && yof >= renderH && xof >= 0 && xof < renderW)
	{
		CancelEditing(true);
		this->InHScroll = true;
		if (l.TotalColumnsWidth > l.RenderWidth && l.RenderWidth > 0.0f)
		{
			const float barW = l.RenderWidth;
			const float maxScrollX = std::max(0.0f, l.TotalColumnsWidth - barW);
			float thumbW = (barW * barW) / l.TotalColumnsWidth;
			const float minThumbW = barW * 0.1f;
			if (thumbW < minThumbW) thumbW = minThumbW;
			if (thumbW > barW) thumbW = barW;
			const float moveSpace = std::max(0.0f, barW - thumbW);
			float per = 0.0f;
			if (maxScrollX > 0.0f) per = std::clamp(this->ScrollXOffset / maxScrollX, 0.0f, 1.0f);
			const float thumbX = per * moveSpace;
			const float localX = (float)xof;
			const bool hitThumb = (localX >= thumbX && localX <= (thumbX + thumbW));
			_hScrollThumbGrabOffsetX = hitThumb ? (localX - thumbX) : (thumbW * 0.5f);
		}
		else
		{
			_hScrollThumbGrabOffsetX = 0.0f;
		}
		SetHScrollByPos((float)xof);
		SetCapture(this->ParentForm->Handle);
		MouseEventArgs event_obj(MouseButtons::Left, 0, xof, yof, 0);
		this->OnMouseDown(this, event_obj);
		this->PostRender();
		return;
	}

	if (l.NeedV && xof >= renderW && yof >= 0 && yof < renderH)
	{
		CancelEditing(true);
		this->InScroll = true;
		if (this->Rows.Count > 0 && l.MaxScrollY > 0.0f && l.RenderHeight > 0.0f && l.ContentHeight > 0.0f)
		{
			const float renderingHeight = l.RenderHeight;
			const float totalHeight = l.TotalRowsHeight;
			float thumbH = renderingHeight * (l.ContentHeight / totalHeight);
			const float minThumbH = renderingHeight * 0.1f;
			if (thumbH < minThumbH) thumbH = minThumbH;
			if (thumbH > renderingHeight) thumbH = renderingHeight;
			const float moveSpace = std::max(0.0f, renderingHeight - thumbH);
			float per = std::clamp(this->ScrollYOffset / l.MaxScrollY, 0.0f, 1.0f);
			const float thumbTop = per * moveSpace;
			const float localY = (float)yof;
			const bool hitThumb = (localY >= thumbTop && localY <= (thumbTop + thumbH));
			_vScrollThumbGrabOffsetY = hitThumb ? (localY - thumbTop) : (thumbH * 0.5f);
		}
		else
		{
			_vScrollThumbGrabOffsetY = 0.0f;
		}
		SetScrollByPos((float)yof);
		SetCapture(this->ParentForm->Handle);
		MouseEventArgs event_obj(MouseButtons::Left, 0, xof, yof, 0);
		this->OnMouseDown(this, event_obj);
		this->PostRender();
		return;
	}

	if (xof < renderW && yof < renderH)
	{
		int divCol = HitTestHeaderDivider(xof, yof);
		if (divCol >= 0)
		{
			CancelEditing(true);
			this->_resizingColumn = true;
			this->_resizeColumnIndex = divCol;
			this->_resizeStartX = (float)xof;
			this->_resizeStartWidth = this->Columns[divCol].Width;
			SetCapture(this->ParentForm->Handle);
			MouseEventArgs event_obj(MouseButtons::Left, 0, xof, yof, 0);
			this->OnMouseDown(this, event_obj);
			return;
		}

		int headCol = HitTestHeaderColumn(xof, yof);
		if (headCol >= 0)
		{
			CancelEditing(true);
			bool ascending = true;
			if (this->SortedColumnIndex == headCol)
				ascending = !this->SortAscending;
			SortByColumn(headCol, ascending);

			MouseEventArgs event_obj(MouseButtons::Left, 0, xof, yof, 0);
			this->OnMouseDown(this, event_obj);
			return;
		}

		POINT undermouseIndex = GetGridViewUnderMouseItem(xof, yof, this);
		if (undermouseIndex.y >= 0 && undermouseIndex.x >= 0 &&
			undermouseIndex.y < this->Rows.Count && undermouseIndex.x < this->Columns.Count)
		{
			// Keep hover index in sync even if we didn't get a prior WM_MOUSEMOVE.
			this->UnderMouseColumnIndex = undermouseIndex.x;
			this->UnderMouseRowIndex = undermouseIndex.y;

			if (this->Columns[undermouseIndex.x].Type == ColumnType::Button)
			{
				if (this->Editing)
					SaveCurrentEditingCell(true);
				CloseComboBoxEditor();

				this->SelectedColumnIndex = undermouseIndex.x;
				this->SelectedRowIndex = undermouseIndex.y;
				this->SelectionChanged(this);

				this->_buttonMouseDown = true;
				this->_buttonDownColumnIndex = undermouseIndex.x;
				this->_buttonDownRowIndex = undermouseIndex.y;
				SetCapture(this->ParentForm->Handle);

				MouseEventArgs event_obj(MouseButtons::Left, 0, xof, yof, 0);
				this->OnMouseDown(this, event_obj);
				this->PostRender();
				return;
			}

			if (this->Editing && undermouseIndex.x == this->EditingColumnIndex && undermouseIndex.y == this->EditingRowIndex)
			{
				D2D1_RECT_F rect{};
				if (TryGetCellRectLocal(this->EditingColumnIndex, this->EditingRowIndex, rect))
				{
					float cellWidth = rect.right - rect.left;
					float cellHeight = rect.bottom - rect.top;
					float lx = (float)xof - rect.left;
					float ly = (float)yof - rect.top;
					int pos = EditHitTestTextPosition(cellWidth, cellHeight, lx, ly);
					this->EditSelectionStart = this->EditSelectionEnd = pos;
					EditUpdateScroll(cellWidth);
				}
			}
			else
			{
				HandleCellClick(undermouseIndex.x, undermouseIndex.y);
			}
		}
		else
		{
			CancelEditing(false);
		}

		// 处理新行点击
		if (this->AllowUserToAddRows && undermouseIndex.y < 0 && undermouseIndex.x >= 0)
		{
			int newRowCol = -1;
			int hitResult = HitTestNewRow(xof, yof, newRowCol);
			if (hitResult >= 0 && newRowCol >= 0 && newRowCol < this->Columns.Count)
			{
				CancelEditing(true);
				AddNewRow();
				return;
			}
		}
	}

	MouseEventArgs event_obj(MouseButtons::Left, 0, xof, yof, 0);
	this->OnMouseDown(this, event_obj);
	this->PostRender();
}
void GridView::HandleLeftButtonUp(int xof, int yof)
{
	if (this->_resizingColumn)
	{
		this->_resizingColumn = false;
		this->_resizeColumnIndex = -1;
		ReleaseCapture();
		MouseEventArgs event_obj(MouseButtons::Left, 0, xof, yof, 0);
		this->OnMouseUp(this, event_obj);
		this->PostRender();
		return;
	}

	if (this->_buttonMouseDown)
	{
		POINT undermouseIndex = GetGridViewUnderMouseItem(xof, yof, this);
		const bool hitSameCell = (undermouseIndex.x == this->_buttonDownColumnIndex && undermouseIndex.y == this->_buttonDownRowIndex);
		const bool validCell = (undermouseIndex.x >= 0 && undermouseIndex.y >= 0 &&
			undermouseIndex.x < this->Columns.Count && undermouseIndex.y < this->Rows.Count);
		const bool isButtonCell = validCell && (this->Columns[undermouseIndex.x].Type == ColumnType::Button);

		this->_buttonMouseDown = false;
		this->_buttonDownColumnIndex = -1;
		this->_buttonDownRowIndex = -1;

		this->InScroll = false;
		this->InHScroll = false;
		ReleaseCapture();
		MouseEventArgs event_obj(MouseButtons::Left, 0, xof, yof, 0);
		this->OnMouseUp(this, event_obj);

		if (hitSameCell && isButtonCell)
		{
			this->OnGridViewButtonClick(this, undermouseIndex.x, undermouseIndex.y);
		}
		this->PostRender();
		return;
	}

	this->InScroll = false;
	this->InHScroll = false;
	ReleaseCapture();
	MouseEventArgs event_obj(MouseButtons::Left, 0, xof, yof, 0);
	this->OnMouseUp(this, event_obj);
	this->PostRender();
}
void GridView::HandleKeyDown(WPARAM wParam)
{
	if (this->Editing && this->ParentForm->Selected == this)
	{
		EditSetImeCompositionWindow();
		EditEnsureSelectionInRange();

		if (wParam == VK_ESCAPE)
		{
			CancelEditing(true);
			this->PostRender();
			return;
		}
		if (wParam == VK_RETURN)
		{
			SaveCurrentEditingCell(true);
			if (this->SelectedRowIndex < this->Rows.Count - 1)
			{
				int nextRow = this->SelectedRowIndex + 1;
				StartEditingCell(this->SelectedColumnIndex, nextRow);
				this->EditSelectionStart = 0;
				this->EditSelectionEnd = (int)this->EditingText.size();
				AdjustScrollPosition();
			}
			else
			{
				this->Editing = false;
				this->EditingColumnIndex = -1;
				this->EditingRowIndex = -1;
			}
			this->PostRender();
			return;
		}

		if (wParam == VK_DELETE)
		{
			EditInputDelete();
			this->PostRender();
			return;
		}
		if (wParam == VK_RIGHT)
		{
			if (this->EditSelectionEnd < (int)this->EditingText.size())
			{
				this->EditSelectionEnd += 1;
				if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) == false)
					this->EditSelectionStart = this->EditSelectionEnd;
			}
			this->PostRender();
			return;
		}
		if (wParam == VK_LEFT)
		{
			if (this->EditSelectionEnd > 0)
			{
				this->EditSelectionEnd -= 1;
				if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) == false)
					this->EditSelectionStart = this->EditSelectionEnd;
			}
			this->PostRender();
			return;
		}
		if (wParam == VK_HOME)
		{
			this->EditSelectionEnd = 0;
			if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) == false)
				this->EditSelectionStart = this->EditSelectionEnd;
			this->PostRender();
			return;
		}
		if (wParam == VK_END)
		{
			this->EditSelectionEnd = (int)this->EditingText.size();
			if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) == false)
				this->EditSelectionStart = this->EditSelectionEnd;
			this->PostRender();
			return;
		}

		KeyEventArgs event_obj(static_cast<Keys>(wParam));
		this->OnKeyDown(this, event_obj);
		this->PostRender();
		return;
	}

	switch (wParam)
	{
	case VK_RIGHT:
		if (SelectedColumnIndex < this->Columns.Count - 1) SelectedColumnIndex++;
		break;
	case VK_LEFT:
		if (SelectedColumnIndex > 0) SelectedColumnIndex--;
		break;
	case VK_DOWN:
		if (SelectedRowIndex < this->Rows.Count - 1) SelectedRowIndex++;
		break;
	case VK_UP:
		if (SelectedRowIndex > 0) SelectedRowIndex--;
		break;
	default:
		break;
	}

	AdjustScrollPosition();
	KeyEventArgs event_obj(static_cast<Keys>(wParam));
	this->OnKeyDown(this, event_obj);
	this->PostRender();
}
void GridView::HandleKeyUp(WPARAM wParam)
{
	KeyEventArgs event_obj(static_cast<Keys>(wParam));
	this->OnKeyUp(this, event_obj);
}
void GridView::HandleCharInput(WPARAM wParam)
{
	if (!this->Enable || !this->Visible) return;
	wchar_t ch = (wchar_t)wParam;

	if (!this->Editing)
	{
		if (ch >= 32 && ch <= 126 && this->SelectedColumnIndex >= 0 && this->SelectedRowIndex >= 0)
		{
			if (IsEditableTextCell(this->SelectedColumnIndex, this->SelectedRowIndex))
			{
				StartEditingCell(this->SelectedColumnIndex, this->SelectedRowIndex);
				this->EditSelectionStart = this->EditSelectionEnd = 0;
			}
		}
	}

	if (!this->Editing || this->ParentForm->Selected != this) return;

	if (ch >= 32 && ch <= 126)
	{
		const wchar_t buf[2] = { ch, L'\0' };
		EditInputText(buf);
	}
	else if (ch == 1) {
		this->EditSelectionStart = 0;
		this->EditSelectionEnd = (int)this->EditingText.size();
	}
	else if (ch == 8) {
		EditInputBack();
	}
	else if (ch == 22) {
		if (OpenClipboard(this->ParentForm->Handle))
		{
			if (IsClipboardFormatAvailable(CF_UNICODETEXT))
			{
				HANDLE hClip = GetClipboardData(CF_UNICODETEXT);
				if (hClip)
				{
					const wchar_t* pBuf = (const wchar_t*)GlobalLock(hClip);
					if (pBuf)
					{
						EditInputText(std::wstring(pBuf));
						GlobalUnlock(hClip);
					}
				}
			}
			CloseClipboard();
		}
	}
	else if (ch == 3 || ch == 24) {
		std::wstring s = EditGetSelectedString();
		if (!s.empty() && OpenClipboard(this->ParentForm->Handle))
		{
			EmptyClipboard();
			size_t bytes = (s.size() + 1) * sizeof(wchar_t);
			HGLOBAL hData = GlobalAlloc(GMEM_MOVEABLE, bytes);
			if (hData)
			{
				wchar_t* pData = (wchar_t*)GlobalLock(hData);
				if (pData)
				{
					memcpy(pData, s.c_str(), bytes);
					GlobalUnlock(hData);
					SetClipboardData(CF_UNICODETEXT, hData);
				}
			}
			CloseClipboard();
		}
		if (ch == 24) {
			EditInputBack();
		}
	}

	this->PostRender();
}
void GridView::HandleImeComposition(LPARAM lParam)
{
	if (!this->Editing || this->ParentForm->Selected != this) return;
	if (lParam & GCS_RESULTSTR)
	{
		HIMC hIMC = ImmGetContext(this->ParentForm->Handle);
		if (hIMC)
		{
			LONG bytes = ImmGetCompositionStringW(hIMC, GCS_RESULTSTR, NULL, 0);
			if (bytes > 0)
			{
				int wcharCount = bytes / (int)sizeof(wchar_t);
				std::wstring buffer;
				buffer.resize(wcharCount);
				ImmGetCompositionStringW(hIMC, GCS_RESULTSTR, buffer.data(), bytes);

				std::wstring filtered;
				filtered.reserve(buffer.size());
				for (wchar_t c : buffer)
				{
					if (c > 0xFF)
						filtered.push_back(c);
				}
				if (!filtered.empty())
				{
					EditInputText(filtered);
				}
			}
			ImmReleaseContext(this->ParentForm->Handle, hIMC);
		}
		this->PostRender();
	}
}
void GridView::HandleCellClick(int col, int row)
{
	if (this->Columns[col].Type == ColumnType::Check)
	{
		ToggleCheckState(col, row);
	}
	else if (this->Columns[col].Type == ColumnType::Button)
	{
		// Button click is handled on mouse-up (WinForms-like)
		if (this->Editing)
			SaveCurrentEditingCell(true);
		this->SelectedColumnIndex = col;
		this->SelectedRowIndex = row;
		this->SelectionChanged(this);
	}
	else if (this->Columns[col].Type == ColumnType::ComboBox)
	{
		ToggleComboBoxEditor(col, row);
	}
	else
	{
		StartEditingCell(col, row);
	}
}
bool GridView::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof)
{
	if (!this->Enable || !this->Visible) return true;
	switch (message)
	{
	case WM_DROPFILES:
		HandleDropFiles(wParam);
		break;

	case WM_MOUSEWHEEL:
		HandleMouseWheel(wParam, xof, yof);
		break;

	case WM_MOUSEMOVE:
		HandleMouseMove(xof, yof);
		break;

	case WM_LBUTTONDOWN:
		HandleLeftButtonDown(xof, yof);
		break;

	case WM_LBUTTONUP:
		HandleLeftButtonUp(xof, yof);
		break;

	case WM_KEYDOWN:
		HandleKeyDown(wParam);
		break;

	case WM_KEYUP:
		HandleKeyUp(wParam);
		break;

	case WM_CHAR:
		HandleCharInput(wParam);
		break;

	case WM_IME_COMPOSITION:
		HandleImeComposition(lParam);
		break;

	default:
		break;
	}
	return true;
}

float GridView::GetRowHeightPx()
{
	auto font = this->Font;
	float rowHeight = font->FontHeight + 2.0f;
	if (this->RowHeight != 0.0f) rowHeight = this->RowHeight;
	return rowHeight;
}
float GridView::GetHeadHeightPx()
{
	auto font = this->Font;
	auto headFont = this->HeadFont ? this->HeadFont : font;
	float headHeight = (this->HeadHeight == 0.0f) ? headFont->FontHeight : this->HeadHeight;
	return headHeight;
}
bool GridView::TryGetCellRectLocal(int col, int row, D2D1_RECT_F& outRect)
{
	if (col < 0 || row < 0) return false;
	if (col >= this->Columns.Count || row >= this->Rows.Count) return false;

	auto l = this->CalcScrollLayout();
	float renderWidth = l.RenderWidth;
	float rowHeight = GetRowHeightPx();
	float headHeight = GetHeadHeightPx();
	if (rowHeight <= 0.0f) return false;

	const int firstRow = (int)std::floor(this->ScrollYOffset / rowHeight);
	const float rowOffsetY = std::fmod(this->ScrollYOffset, rowHeight);
	int drawIndex = row - firstRow;
	if (drawIndex < 0) return false;
	float top = headHeight + (rowHeight * (float)drawIndex) - rowOffsetY;
	float bottom = top + rowHeight;
	if (bottom <= headHeight || top >= l.RenderHeight) return false;

	float left = -this->ScrollXOffset;
	for (int i = 0; i < col; i++) left += this->Columns[i].Width;
	float width = this->Columns[col].Width;
	const float clipLeft = std::max(0.0f, left);
	const float clipRight = std::min(renderWidth, left + width);
	const float clipTop = std::max(headHeight, top);
	const float clipBottom = std::min(l.RenderHeight, bottom);
	if (clipRight <= clipLeft) return false;
	if (clipBottom <= clipTop) return false;

	outRect = D2D1_RECT_F{ clipLeft, clipTop, clipRight, clipBottom };
	return true;
}
bool GridView::IsEditableTextCell(int col, int row)
{
	if (col < 0 || row < 0) return false;
	if (col >= this->Columns.Count || row >= this->Rows.Count) return false;
	return this->Columns[col].Type == ColumnType::Text && this->Columns[col].CanEdit;
}
void GridView::EditEnsureSelectionInRange()
{
	if (this->EditSelectionStart < 0) this->EditSelectionStart = 0;
	if (this->EditSelectionEnd < 0) this->EditSelectionEnd = 0;
	int maxLen = (int)this->EditingText.size();
	if (this->EditSelectionStart > maxLen) this->EditSelectionStart = maxLen;
	if (this->EditSelectionEnd > maxLen) this->EditSelectionEnd = maxLen;
}
void GridView::EditInputText(const std::wstring& input)
{
	if (!this->Editing) return;
	std::wstring old = this->EditingText;

	EditEnsureSelectionInRange();
	int sels = (this->EditSelectionStart <= this->EditSelectionEnd) ? this->EditSelectionStart : this->EditSelectionEnd;
	int sele = (this->EditSelectionEnd >= this->EditSelectionStart) ? this->EditSelectionEnd : this->EditSelectionStart;
	int selLen = sele - sels;

	if (selLen > 0)
	{
		this->EditingText.erase((size_t)sels, (size_t)selLen);
	}
	this->EditingText.insert((size_t)sels, input);
	this->EditSelectionStart = this->EditSelectionEnd = sels + (int)input.size();

	for (auto& ch : this->EditingText)
	{
		if (ch == L'\r' || ch == L'\n') ch = L' ';
	}

	if (this->EditingRowIndex >= 0 && this->EditingColumnIndex >= 0 &&
		this->EditingRowIndex < this->Rows.Count && this->EditingColumnIndex < this->Columns.Count)
	{
		this->Rows[this->EditingRowIndex].Cells[this->EditingColumnIndex].Text = this->EditingText;
	}
}
void GridView::EditInputBack()
{
	if (!this->Editing) return;
	EditEnsureSelectionInRange();
	int sels = (this->EditSelectionStart <= this->EditSelectionEnd) ? this->EditSelectionStart : this->EditSelectionEnd;
	int sele = (this->EditSelectionEnd >= this->EditSelectionStart) ? this->EditSelectionEnd : this->EditSelectionStart;
	int selLen = sele - sels;

	if (selLen > 0)
	{
		this->EditingText.erase((size_t)sels, (size_t)selLen);
		this->EditSelectionStart = this->EditSelectionEnd = sels;
	}
	else if (sels > 0)
	{
		this->EditingText.erase((size_t)sels - 1, 1);
		this->EditSelectionStart = this->EditSelectionEnd = sels - 1;
	}

	if (this->EditingRowIndex >= 0 && this->EditingColumnIndex >= 0 &&
		this->EditingRowIndex < this->Rows.Count && this->EditingColumnIndex < this->Columns.Count)
	{
		this->Rows[this->EditingRowIndex].Cells[this->EditingColumnIndex].Text = this->EditingText;
	}
}
void GridView::EditInputDelete()
{
	if (!this->Editing) return;
	EditEnsureSelectionInRange();
	int sels = (this->EditSelectionStart <= this->EditSelectionEnd) ? this->EditSelectionStart : this->EditSelectionEnd;
	int sele = (this->EditSelectionEnd >= this->EditSelectionStart) ? this->EditSelectionEnd : this->EditSelectionStart;
	int selLen = sele - sels;

	if (selLen > 0)
	{
		this->EditingText.erase((size_t)sels, (size_t)selLen);
		this->EditSelectionStart = this->EditSelectionEnd = sels;
	}
	else if (sels < (int)this->EditingText.size())
	{
		this->EditingText.erase((size_t)sels, 1);
		this->EditSelectionStart = this->EditSelectionEnd = sels;
	}

	if (this->EditingRowIndex >= 0 && this->EditingColumnIndex >= 0 &&
		this->EditingRowIndex < this->Rows.Count && this->EditingColumnIndex < this->Columns.Count)
	{
		this->Rows[this->EditingRowIndex].Cells[this->EditingColumnIndex].Text = this->EditingText;
	}
}
void GridView::EditUpdateScroll(float cellWidth)
{
	if (!this->Editing) return;
	float renderWidth = cellWidth - (this->EditTextMargin * 2.0f);
	if (renderWidth <= 1.0f) return;

	EditEnsureSelectionInRange();
	auto font = this->Font;
	auto hit = font->HitTestTextRange(this->EditingText, (UINT32)this->EditSelectionEnd, (UINT32)0);
	if (hit.empty()) return;
	auto caret = hit[0];
	if ((caret.left + caret.width) - this->EditOffsetX > renderWidth)
	{
		this->EditOffsetX = (caret.left + caret.width) - renderWidth;
	}
	if (caret.left - this->EditOffsetX < 0.0f)
	{
		this->EditOffsetX = caret.left;
	}
	if (this->EditOffsetX < 0.0f) this->EditOffsetX = 0.0f;
}
int GridView::EditHitTestTextPosition(float cellWidth, float cellHeight, float x, float y)
{
	auto font = this->Font;
	float renderHeight = cellHeight - (this->EditTextMargin * 2.0f);
	if (renderHeight < 0.0f) renderHeight = 0.0f;
	return font->HitTestTextPosition(this->EditingText, FLT_MAX, renderHeight, (x - this->EditTextMargin) + this->EditOffsetX, y - this->EditTextMargin);
}
std::wstring GridView::EditGetSelectedString()
{
	int sels = (this->EditSelectionStart <= this->EditSelectionEnd) ? this->EditSelectionStart : this->EditSelectionEnd;
	int sele = (this->EditSelectionEnd >= this->EditSelectionStart) ? this->EditSelectionEnd : this->EditSelectionStart;
	if (sele > sels && sels >= 0 && sele <= (int)this->EditingText.size())
	{
		return this->EditingText.substr((size_t)sels, (size_t)(sele - sels));
	}
	return L"";
}
void GridView::EditSetImeCompositionWindow()
{
	if (!this->ParentForm || !this->ParentForm->Handle) return;
	if (!this->Editing) return;
	D2D1_RECT_F rect{};
	if (!TryGetCellRectLocal(this->EditingColumnIndex, this->EditingRowIndex, rect)) return;

	auto pos = this->AbsLocation;
	POINT pt{ pos.x + (int)rect.left, pos.y + (int)rect.top };
	HIMC hImc = ImmGetContext(this->ParentForm->Handle);
	if (!hImc) return;
	COMPOSITIONFORM form;
	form.dwStyle = CFS_RECT;
	form.ptCurrentPos = pt;
	form.rcArea = RECT{ pt.x, pt.y + (LONG)(rect.bottom - rect.top), pt.x + 400, pt.y + 240 };
	ImmSetCompositionWindow(hImc, &form);
	ImmReleaseContext(this->ParentForm->Handle, hImc);
}
#pragma endregion