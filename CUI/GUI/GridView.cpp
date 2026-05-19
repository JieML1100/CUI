#pragma once
#define NOMINMAX
#include "GridView.h"
#include "DropDownPopup.h"
#include "Form.h"
#include <algorithm>
#include <cmath>
#include <cwchar>
#pragma comment(lib, "Imm32.lib")

CellValue::CellValue() : Text(L"null"), Image(nullptr), Tag(NULL)
{
}
CellValue::CellValue(std::wstring s) : Text(s), Tag(NULL), Image(nullptr)
{
}
CellValue::CellValue(wchar_t* s) :Text(s), Tag(NULL), Image(nullptr)
{
}
CellValue::CellValue(const wchar_t* s) : Text(s), Tag(NULL), Image(nullptr)
{
}
CellValue::CellValue(std::shared_ptr<BitmapSource> img) : Text(L""), Tag(NULL), Image(std::move(img))
{
}
CellValue::CellValue(__int64 tag) : Text(std::to_wstring(tag)), Tag(tag), Image(nullptr)
{
}
CellValue::CellValue(bool tag) : Text(std::to_wstring(tag)), Tag(tag), Image(nullptr)
{
}
CellValue::CellValue(__int32 tag) : Text(std::to_wstring(tag)), Tag(tag), Image(nullptr)
{
}
CellValue::CellValue(unsigned __int32 tag) : Text(std::to_wstring(tag)), Tag(tag), Image(nullptr)
{
}
CellValue::CellValue(unsigned __int64 tag) : Text(std::to_wstring(tag)), Tag(tag), Image(nullptr)
{
}
CellValue::CellValue(PVOID tag) : Image(nullptr)
{
	Tag = reinterpret_cast<__int64>(tag);
	Text.resize(sizeof(PVOID) * 2);
	swprintf_s(&Text[0], Text.size(), L"%p", tag);
}

std::wstring CellValue::GetText() const
{
	return Text;
}

void CellValue::SetText(const std::wstring& text)
{
	Text = text;
}

__int64 CellValue::GetTag() const
{
	return Tag;
}

void CellValue::SetTag(__int64 tag)
{
	Tag = tag;
	Text = std::to_wstring(tag);
}

bool CellValue::GetBool() const
{
	return Tag != 0;
}

void CellValue::SetBool(bool value)
{
	Tag = value ? 1 : 0;
	Text = value ? L"1" : L"0";
}

PVOID CellValue::GetPointer() const
{
	return reinterpret_cast<PVOID>(Tag);
}

void CellValue::SetPointer(PVOID value)
{
	Tag = reinterpret_cast<__int64>(value);
	Text.resize(sizeof(PVOID) * 2);
	swprintf_s(&Text[0], Text.size(), L"%p", value);
}

void CellValue::SetComboSelection(int selectedIndex, const std::wstring& selectedText)
{
	Tag = selectedIndex;
	Text = selectedText;
}

ID2D1Bitmap* CellValue::GetImageBitmap(D2DGraphics* render)
{
	if (!render || !Image)
		return nullptr;
	auto* target = render->GetRenderTargetRaw();
	if (!target)
		return nullptr;
	if (ImageCache && ImageCacheTarget == target && ImageCacheSource == Image.get())
		return ImageCache.Get();
	ImageCache.Reset();
	ImageCacheTarget = target;
	ImageCacheSource = Image.get();
	auto* bmp = render->CreateBitmap(Image);
	if (!bmp)
		return nullptr;
	ImageCache.Attach(bmp);
	return ImageCache.Get();
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

static D2D1_RECT_F GridInsetRect(float x, float y, float w, float h, float insetX, float insetY)
{
	return D2D1::RectF(x + insetX, y + insetY, x + w - insetX, y + h - insetY);
}

static void DrawGridCellLines(D2DGraphics* d2d, float x, float y, float w, float h, D2D1_COLOR_F color)
{
	if (!d2d || w <= 0.0f || h <= 0.0f) return;
	d2d->DrawLine(x, y + h - 0.5f, x + w, y + h - 0.5f, color, 1.0f);
	d2d->DrawLine(x + w - 0.5f, y + 5.0f, x + w - 0.5f, y + h - 5.0f, color, 1.0f);
}

static void DrawGridCellState(GridView* grid, D2DGraphics* d2d, float x, float y, float w, float h, bool selected, bool hovered)
{
	if (!grid || !d2d || w <= 0.0f || h <= 0.0f) return;
	const float insetX = (std::max)(2.0f, grid->CellHorizontalPadding * 0.35f);
	const float insetY = (std::max)(2.0f, grid->CellVerticalPadding);
	D2D1_RECT_F stateRect = GridInsetRect(x, y, w, h, insetX, insetY);
	if (stateRect.right <= stateRect.left || stateRect.bottom <= stateRect.top) return;
	if (selected)
	{
		d2d->FillRoundRect(stateRect, grid->SelectedItemBackColor, grid->CellCornerRadius);
		const float accentW = (std::max)(2.0f, grid->SelectedAccentWidth);
		const float accentH = (std::max)(5.0f, (stateRect.bottom - stateRect.top) - 10.0f);
		d2d->FillRoundRect(stateRect.left, stateRect.top + 5.0f, accentW, accentH, grid->AccentColor, accentW * 0.5f);
	}
	else if (hovered)
	{
		d2d->FillRoundRect(stateRect, grid->UnderMouseItemBackColor, grid->CellCornerRadius);
	}
}

static void DrawGridLinkedText(GridView* grid, D2DGraphics* d2d, Font* font,
	const std::wstring& text, float x, float y, float w, float h, float textTop, bool hovered)
{
	if (!grid || !d2d || !font || text.empty() || w <= 0.0f || h <= 0.0f) return;
	const float textX = x + grid->CellHorizontalPadding;
	const float maxTextWidth = (std::max)(1.0f, w - grid->CellHorizontalPadding * 2.0f);
	const auto color = hovered ? grid->LinkedTextHoverColor : grid->LinkedTextColor;
	d2d->DrawString(text, textX, y + textTop, maxTextWidth, font->FontHeight + 2.0f, color, font);

	auto textSize = font->GetTextSize(text);
	const float underlineWidth = (std::min)(maxTextWidth, textSize.width);
	if (underlineWidth <= 0.0f) return;
	float underlineY = y + textTop + textSize.height - 1.0f;
	underlineY = (std::min)(underlineY, y + h - 3.0f);
	d2d->DrawLine(textX, underlineY, textX + underlineWidth, underlineY, color, 1.0f);
}

static D2D1_POINT_2F RotateGridPoint(const D2D1_POINT_2F& point, float cx, float cy, float angle)
{
	const float dx = point.x - cx;
	const float dy = point.y - cy;
	const float s = std::sin(angle);
	const float c = std::cos(angle);
	return D2D1::Point2F(cx + dx * c - dy * s, cy + dx * s + dy * c);
}

static void DrawGridChevron(D2DGraphics* d2d, float cx, float cy, float size, float progress, D2D1_COLOR_F color)
{
	if (!d2d) return;
	progress = std::clamp(progress, 0.0f, 1.0f);
	const float halfW = size * 0.42f;
	const float halfH = size * 0.26f;
	const float angle = progress * 3.14159265359f;
	auto p1 = D2D1::Point2F(cx - halfW, cy - halfH);
	auto p2 = D2D1::Point2F(cx, cy + halfH);
	auto p3 = D2D1::Point2F(cx + halfW, cy - halfH);
	p1 = RotateGridPoint(p1, cx, cy, angle);
	p2 = RotateGridPoint(p2, cx, cy, angle);
	p3 = RotateGridPoint(p3, cx, cy, angle);
	d2d->DrawLine(p1, p2, color, 1.7f);
	d2d->DrawLine(p2, p3, color, 1.7f);
}

static void DrawGridChevron(D2DGraphics* d2d, float cx, float cy, float size, bool up, D2D1_COLOR_F color)
{
	DrawGridChevron(d2d, cx, cy, size, up ? 1.0f : 0.0f, color);
}

bool GridView::HandlesNavigationKey(WPARAM key) const
{
	if (this->Editing)
	{
		switch (key)
		{
		case VK_LEFT:
		case VK_RIGHT:
		case VK_HOME:
		case VK_END:
		case VK_PRIOR:
		case VK_NEXT:
			return true;
		default:
			return false;
		}
	}

	switch (key)
	{
	case VK_LEFT:
	case VK_RIGHT:
	case VK_UP:
	case VK_DOWN:
	case VK_HOME:
	case VK_END:
		return true;
	default:
		return false;
	}
}
GridView::GridView(int x, int y, int width, int height)
{
	this->Location = POINT{ x,y };
	this->Size = SIZE{ width,height };
}

GridView::~GridView()
{
	CloseDropDownEditor(true);
	if (this->_dropDownPopup)
	{
		delete this->_dropDownPopup;
		this->_dropDownPopup = NULL;
	}
	this->_dropDownPopupColumnIndex = -1;
	this->_dropDownPopupRowIndex = -1;

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
	for (int i = 0; i < (int)this->Columns.size(); i++)
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
		float newRowAreaHeight = (this->AllowUserToAddRows && this->Columns.size() > 0) ? l.RowHeight : 0.0f;
		float totalRowsH = (l.RowHeight > 0.0f) ? (l.RowHeight * (float)this->Rows.size()) : 0.0f;
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
			l.MaxScrollRow = std::max(0, (int)this->Rows.size() - visibleRows);
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
	float newRowAreaHeight = (this->AllowUserToAddRows && this->Columns.size() > 0) ? l.RowHeight : 0.0f;
	l.TotalRowsHeight = (l.RowHeight > 0.0f) ? (l.RowHeight * (float)this->Rows.size()) : 0.0f;
	l.TotalRowsHeight += newRowAreaHeight;  // 加上新行区域高度
	l.MaxScrollY = std::max(0.0f, l.TotalRowsHeight - contentH);
	l.VisibleRows = (l.RowHeight > 0.0f && contentH > 0.0f) ? ((int)std::ceil(contentH / l.RowHeight) + 1) : 0;
	if (l.VisibleRows < 0) l.VisibleRows = 0;
	l.MaxScrollRow = std::max(0, (int)this->Rows.size() - l.VisibleRows);
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
			undermouseIndex.y < static_cast<LONG>(this->Rows.size()) && undermouseIndex.x < static_cast<LONG>(this->Columns.size()))
		{
			auto type = this->Columns[static_cast<size_t>(undermouseIndex.x)].Type;
			if (type == ColumnType::Button || type == ColumnType::Check || type == ColumnType::ComboBox || type == ColumnType::LinkedText)
				return CursorKind::Hand;
			if (IsEditableTextCell(undermouseIndex.x, undermouseIndex.y))
				return CursorKind::IBeam;
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

void GridView::ClearRows()
{
	CancelEditing(false);
	this->Rows.clear();
	this->SelectedRowIndex = -1;
	this->UnderMouseRowIndex = -1;
	this->ScrollYOffset = 0.0f;
	this->ScrollRowPosition = 0;
	AdjustScrollPosition();
	this->PostRender();
}

void GridView::ClearColumns()
{
	CancelEditing(false);
	this->Columns.clear();
	this->SelectedColumnIndex = -1;
	this->UnderMouseColumnIndex = -1;
	this->SortedColumnIndex = -1;
	this->ScrollXOffset = 0.0f;
	AdjustScrollPosition();
	this->PostRender();
}

void GridView::AddRow(const GridViewRow& row)
{
	this->Rows.push_back(row);
	AdjustScrollPosition();
	this->PostRender();
}

void GridView::AddColumn(const GridViewColumn& column)
{
	this->Columns.push_back(column);
	AdjustScrollPosition();
	this->PostRender();
}

size_t GridView::RowCount() const
{
	return this->Rows.size();
}

size_t GridView::ColumnCount() const
{
	return this->Columns.size();
}

GridViewRow& GridView::RowAt(int index)
{
	return this->Rows.at((size_t)index);
}

GridViewColumn& GridView::ColumnAt(int index)
{
	return this->Columns.at((size_t)index);
}

void GridView::SwapRows(int indexA, int indexB)
{
	if (indexA < 0 || indexB < 0) return;
	if (indexA >= (int)this->Rows.size() || indexB >= (int)this->Rows.size()) return;
	if (indexA == indexB) return;
	CancelEditing(false);
	std::swap(this->Rows[(size_t)indexA], this->Rows[(size_t)indexB]);
	if (this->SelectedRowIndex == indexA)
		this->SelectedRowIndex = indexB;
	else if (this->SelectedRowIndex == indexB)
		this->SelectedRowIndex = indexA;
	this->PostRender();
}

void GridView::RemoveRowAt(int index)
{
	if (index < 0 || index >= (int)this->Rows.size()) return;
	CancelEditing(false);
	this->Rows.erase(this->Rows.begin() + index);
	if (this->SelectedRowIndex == index)
		this->SelectedRowIndex = -1;
	else if (this->SelectedRowIndex > index)
		this->SelectedRowIndex--;
	if (this->UnderMouseRowIndex == index)
		this->UnderMouseRowIndex = -1;
	else if (this->UnderMouseRowIndex > index)
		this->UnderMouseRowIndex--;
	AdjustScrollPosition();
	this->PostRender();
}

GridViewRow& GridView::SelectedRow()
{
	static GridViewRow default_;
	if (this->SelectedRowIndex >= 0 && this->SelectedRowIndex < static_cast<int>(this->Rows.size()))
	{
		return this->Rows[static_cast<size_t>(this->SelectedRowIndex)];
	}
	return default_;
}
std::wstring& GridView::SelectedValue()
{
	static std::wstring default_;
	if (this->SelectedRowIndex >= 0 && this->SelectedRowIndex < static_cast<int>(this->Rows.size()))
	{
		return this->Rows[static_cast<size_t>(this->SelectedRowIndex)].Cells[static_cast<size_t>(SelectedColumnIndex)].Text;
	}
	return default_;
}
void GridView::Clear()
{
	ClearRows();
	ClearColumns();
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
	if (col < 0 || col >= static_cast<int>(this->Columns.size())) return;

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

	if (this->SortRequestHandler && this->SortRequestHandler(this, col, ascending))
		return;

	if (this->Rows.size() <= 1) return;

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

	float row_height = l.RowHeight;
	float head_height = l.HeadHeight;
	if (y < head_height)
	{
		return { -1,-1 };
	}
	unsigned int s_x = 0;
	unsigned int s_y = 0;
	float yf = head_height;
	float virtualX = (float)x + ct->ScrollXOffset;
	int xindex = -1;
	int yindex = -1;
	float acc = 0.0f;
	for (; s_x < ct->Columns.size(); s_x++)
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
		if (idx >= 0 && idx < static_cast<int>(ct->Rows.size())) yindex = idx;
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
	for (int i = 0; i < static_cast<int>(this->Columns.size()); i++)
	{
		float cWidth = this->Columns[static_cast<size_t>(i)].Width;
		if (virtualX >= xf && virtualX < xf + cWidth)
			return i;
		xf += this->Columns[static_cast<size_t>(i)].Width;
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
	for (int i = 0; i < static_cast<int>(this->Columns.size()); i++)
	{
		const float cWidth = this->Columns[static_cast<size_t>(i)].Width;
		const float rightEdge = xf + cWidth;
		if (std::abs(virtualX - rightEdge) <= hitPx)
			return i;

		xf += this->Columns[static_cast<size_t>(i)].Width;
	}
	return -1;
}
D2D1_RECT_F GridView::GetGridViewScrollBlockRect(GridView* ct)
{
	auto absloc = ct->AbsLocation;
	auto l = ct->CalcScrollLayout();
	float _render_width = l.RenderWidth;
	float _render_height = l.RenderHeight;
	float row_height = l.RowHeight;
	float head_height = l.HeadHeight;
	const float contentH = std::max(0.0f, _render_height - head_height);
	const float totalH = (row_height > 0.0f) ? (row_height * (float)ct->Rows.size()) : 0.0f;
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

	auto l = this->CalcScrollLayout();

	if (l.NeedV && this->Rows.size() > 0)
	{
		float _render_width = l.RenderWidth;
		float _render_height = l.RenderHeight;
		const float row_height = this->GetRowHeightPx();
		const float head_height = this->GetHeadHeightPx();
		const float contentH = std::max(0.0f, _render_height - head_height);
		const float totalH = (row_height > 0.0f) ? (row_height * (float)this->Rows.size()) : 0.0f;
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

			const float barW = (std::max)(5.0f, l.ScrollBarSize - 2.0f);
			const float barX = _render_width + (l.ScrollBarSize - barW) * 0.5f;
			d2d->FillRoundRect(barX, 4.0f, barW, (std::max)(0.0f, _render_height - 8.0f), this->ScrollBackColor, barW * 0.5f);
			d2d->FillRoundRect(barX, thumbTop + 2.0f, barW, (std::max)(4.0f, thumbH - 4.0f), this->ScrollForeColor, barW * 0.5f);
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

	const float barX = 0.0f;
	const float barY = l.RenderHeight;
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

	const float trackH = (std::max)(5.0f, barH - 2.0f);
	const float trackY = barY + (barH - trackH) * 0.5f;
	d2d->FillRoundRect(barX + 4.0f, trackY, (std::max)(0.0f, barW - 8.0f), trackH, this->ScrollBackColor, trackH * 0.5f);
	d2d->FillRoundRect(thumbX + 2.0f, trackY, (std::max)(4.0f, thumbW - 4.0f), trackH, this->ScrollForeColor, trackH * 0.5f);
}

void GridView::DrawCorner(const ScrollLayout& l)
{
	auto d2d = this->ParentForm->Render;
	const float x = l.RenderWidth;
	const float y = l.RenderHeight;
	d2d->FillRoundRect(x + 1.0f, y + 1.0f, l.ScrollBarSize - 2.0f, l.ScrollBarSize - 2.0f, this->ScrollBackColor, 3.0f);
}

void GridView::SetScrollByPos(float yof)
{
	const int rowCount = static_cast<int>(this->Rows.size());
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
	auto size = this->ActualSize();
	const float actualWidth = static_cast<float>(size.cx);
	const float actualHeight = static_cast<float>(size.cy);
	bool caretBlinkStateUpdated = false;
	this->BeginRender();
	{
		d2d->FillRect(0, 0, actualWidth, actualHeight, this->BackColor);
		if (this->Image)
		{
			this->RenderImage(this->CellCornerRadius);
		}
		auto font = this->Font;
		auto head_font = HeadFont ? HeadFont : font;
		{
			auto l = this->CalcScrollLayout();
			float _render_width = l.RenderWidth;
			float _render_height = l.RenderHeight;
			float font_height = font->FontHeight;
			float head_font_height = head_font->FontHeight;
			float row_height = l.RowHeight;
			float text_top = (row_height - font_height) * 0.5f;
			if (text_top < 0) text_top = 0;
			if (this->Rows.size() <= 0)
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
				if (this->ScrollRowPosition >= static_cast<int>(this->Rows.size())) this->ScrollRowPosition = static_cast<int>(this->Rows.size()) - 1;
			}
			if (this->ScrollXOffset < 0.0f) this->ScrollXOffset = 0.0f;
			if (this->ScrollXOffset > l.MaxScrollX) this->ScrollXOffset = l.MaxScrollX;

			int s_x = 0;
			int s_y = this->ScrollRowPosition;
			float head_height = l.HeadHeight;
			float row_offset = (row_height > 0.0f) ? std::fmod(this->ScrollYOffset, row_height) : 0.0f;
			float yf = head_height - row_offset;
			float xf = -this->ScrollXOffset;
			int i = s_x;
			for (; i < static_cast<int>(this->Columns.size()); i++)
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

				float draw_y_offset = (head_height - head_font_height) / 2.0f;
				if (draw_y_offset < 0)draw_y_offset = 0;
				d2d->PushDrawRect(clipX, 0, clipW, head_height);
				{
					const bool sorted = (i == this->SortedColumnIndex);
					D2D1_RECT_F headRect = D2D1::RectF(drawX + 2.0f, 3.0f, drawX + c_width - 2.0f, head_height - 3.0f);
					d2d->FillRoundRect(headRect, sorted ? this->HeadHoverBackColor : this->HeadBackColor, this->CellCornerRadius);
					if (sorted)
					{
						d2d->FillRoundRect(headRect.left + 6.0f, headRect.bottom - 3.0f,
							(std::max)(6.0f, (headRect.right - headRect.left) - 12.0f), 2.0f,
							this->AccentColor, 1.0f);
					}
					DrawGridCellLines(d2d, drawX, 0.0f, c_width, head_height, this->GridLineColor);
					const float sortReserve = sorted ? 16.0f : 0.0f;
					const float textWidth = (std::max)(1.0f, c_width - 16.0f - sortReserve);
					d2d->DrawString(this->Columns[i].Name,
						drawX + 8.0f,
						draw_y_offset,
						textWidth,
						head_font_height + 2.0f,
						this->HeadForeColor, head_font);
					if (sorted)
					{
						DrawGridChevron(d2d, drawX + c_width - 11.0f, head_height * 0.5f, 9.0f,
							this->SortAscending, this->AccentColor);
					}
				}
				d2d->PopDrawRect();
				xf += colW;
			}

			const int maxRows = l.VisibleRows;
			i = 0;
	for (int r = s_y; r < static_cast<int>(this->Rows.size()) && i < maxRows; r++, i++)
	{
		GridViewRow& row = this->Rows[static_cast<size_t>(r)];
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
		for (int c = s_x; c < static_cast<int>(this->Columns.size()); c++)
		{
			float colW = this->Columns[static_cast<size_t>(c)].Width;
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
					d2d->PushDrawRect(clipX, clipY, clipW, clipH);
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

										auto editRect = GridInsetRect(drawX, yf, c_width, _r_height, 3.0f, 3.0f);
										d2d->FillRoundRect(editRect, this->EditBackColor, this->CellCornerRadius);
										d2d->DrawRoundRect(editRect.left, editRect.top, editRect.right - editRect.left,
											editRect.bottom - editRect.top, this->AccentColor, 1.2f, this->CellCornerRadius);
										DrawGridCellLines(d2d, drawX, yf, c_width, _r_height, this->GridLineColor);

										int sels = EditSelectionStart <= EditSelectionEnd ? EditSelectionStart : EditSelectionEnd;
										int sele = EditSelectionEnd >= EditSelectionStart ? EditSelectionEnd : EditSelectionStart;
										int selLen = sele - sels;
										auto selRange = font->HitTestTextRange(this->EditingText, (UINT32)sels, (UINT32)selLen);
										bool caretRectValid = false;
										D2D1_RECT_F caretRect{};

										if (selLen != 0)
										{
											for (auto sr : selRange)
											{
												d2d->FillRect(
													sr.left + drawX + this->EditTextMargin - this->EditOffsetX,
													(sr.top + yf) + offsetY,
													sr.width, sr.height,
													this->EditSelectedBackColor);
											}
										}
										else
										{
											if (!selRange.empty())
											{
												const float caretX = selRange[0].left + drawX + this->EditTextMargin - this->EditOffsetX;
												const float caretTop = (selRange[0].top + yf) + offsetY;
												const float caretBottom = (selRange[0].top + yf + selRange[0].height) + offsetY;
												auto abs = this->AbsLocation;
												caretRect = { abs.x + caretX - 2.0f, abs.y + caretTop - 2.0f, abs.x + caretX + 2.0f, abs.y + caretBottom + 2.0f };
												caretRectValid = true;
											}
										}

										UpdateCaretBlinkState(isSelected, this->EditSelectionStart, this->EditSelectionEnd, caretRectValid, caretRectValid ? &caretRect : nullptr);
										caretBlinkStateUpdated = true;
										if (caretRectValid && IsCaretBlinkVisible())
										{
											d2d->DrawLine(
												{ caretRect.left - this->AbsLocation.x + 2.0f, caretRect.top - this->AbsLocation.y + 2.0f },
												{ caretRect.left - this->AbsLocation.x + 2.0f, caretRect.bottom - this->AbsLocation.y - 2.0f },
												Colors::Black);
										}

										if (!caretBlinkStateUpdated)
										{
											UpdateCaretBlinkState(false, 0, 1, false, nullptr);
										}
										auto lot = Factory::CreateStringLayout(this->EditingText, FLT_MAX, renderHeight, font->FontObject);
										if (lot) {
											if (selLen != 0)
											{
												d2d->DrawStringLayoutEffect(lot,
													drawX + this->EditTextMargin - this->EditOffsetX, (yf)+offsetY,
													this->EditForeColor,
													DWRITE_TEXT_RANGE{ (UINT32)sels, (UINT32)selLen },
													this->EditSelectedForeColor,
													font);
											}
											else
											{
												d2d->DrawStringLayout(lot,
													drawX + this->EditTextMargin - this->EditOffsetX, (yf)+offsetY,
													this->EditForeColor);
											}
											lot->Release();
										}
									}
								}
								else
								{
									DrawGridCellState(this, d2d, drawX, yf, c_width, _r_height, true, false);
									DrawGridCellLines(d2d, drawX, yf, c_width, _r_height, this->GridLineColor);
									if (row.Cells.size() > static_cast<size_t>(c))
										d2d->DrawString(row.Cells[static_cast<size_t>(c)].Text,
											drawX + this->CellHorizontalPadding,
											yf + text_top,
											(std::max)(1.0f, c_width - this->CellHorizontalPadding * 2.0f),
											font_height + 2.0f,
											this->SelectedItemForeColor, font);
								}
							}
							else if (c == this->UnderMouseColumnIndex && r == this->UnderMouseRowIndex)
							{
								DrawGridCellState(this, d2d, drawX, yf, c_width, _r_height, false, true);
								DrawGridCellLines(d2d, drawX, yf, c_width, _r_height, this->GridLineColor);
								if (row.Cells.size() > static_cast<size_t>(c))
									d2d->DrawString(row.Cells[static_cast<size_t>(c)].Text,
										drawX + this->CellHorizontalPadding,
										yf + text_top,
										(std::max)(1.0f, c_width - this->CellHorizontalPadding * 2.0f),
										font_height + 2.0f,
										this->UnderMouseItemForeColor, font);
							}
							else
							{
								DrawGridCellLines(d2d, drawX, yf, c_width, _r_height, this->GridLineColor);
								if (row.Cells.size() > static_cast<size_t>(c))
									d2d->DrawString(row.Cells[static_cast<size_t>(c)].Text,
										drawX + this->CellHorizontalPadding,
										yf + text_top,
										(std::max)(1.0f, c_width - this->CellHorizontalPadding * 2.0f),
										font_height + 2.0f,
										this->ForeColor, font);
							}
						}
						break;
						case ColumnType::LinkedText:
						{
							const bool selectedCell = (c == this->SelectedColumnIndex && r == this->SelectedRowIndex);
							const bool hoverCell = (c == this->UnderMouseColumnIndex && r == this->UnderMouseRowIndex);
							if (selectedCell || hoverCell)
								DrawGridCellState(this, d2d, drawX, yf, c_width, _r_height, selectedCell, hoverCell);
							DrawGridCellLines(d2d, drawX, yf, c_width, _r_height, this->GridLineColor);
							if (row.Cells.size() > static_cast<size_t>(c))
							{
								DrawGridLinkedText(this, d2d, font, row.Cells[static_cast<size_t>(c)].Text,
									drawX, yf, c_width, _r_height, text_top, hoverCell);
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

							DrawGridCellLines(d2d, drawX, yf, c_width, _r_height, this->GridLineColor);
							D2D1_RECT_F buttonRect = GridInsetRect(drawX, yf, c_width, _r_height, 6.0f, 4.0f);
							d2d->FillRoundRect(buttonRect, back, this->CellCornerRadius);
							d2d->DrawRoundRect(buttonRect.left, buttonRect.top, buttonRect.right - buttonRect.left,
								buttonRect.bottom - buttonRect.top,
								isPressed ? this->AccentColor : this->ButtonBorderDarkColor, 1.0f, this->CellCornerRadius);

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
									drawX + tx,
									yf + ty,
									(std::max)(1.0f, c_width - 12.0f),
									font_height + 2.0f,
									this->ForeColor, font);
							}
						}
						break;
						case ColumnType::ComboBox:
						{
							EnsureComboBoxCellDefaultSelection(c, r);
							float dropDownProgress = 0.0f;
							if (this->_dropDownPopup &&
								this->_dropDownPopupColumnIndex == c &&
								this->_dropDownPopupRowIndex == r)
							{
								dropDownProgress = this->_dropDownPopup->CurrentDropProgress();
							}
							D2D1_COLOR_F fore = this->ForeColor;
							bool fill = false;

							if (c == this->SelectedColumnIndex && r == this->SelectedRowIndex)
							{
								fore = this->SelectedItemForeColor;
								fill = true;
							}
							else if (c == this->UnderMouseColumnIndex && r == this->UnderMouseRowIndex)
							{
								fore = this->UnderMouseItemForeColor;
								fill = true;
							}

							if (fill)
								DrawGridCellState(this, d2d, drawX, yf, c_width, _r_height,
									c == this->SelectedColumnIndex && r == this->SelectedRowIndex,
									c == this->UnderMouseColumnIndex && r == this->UnderMouseRowIndex);
							DrawGridCellLines(d2d, drawX, yf, c_width, _r_height, this->GridLineColor);
							if (row.Cells.size() > static_cast<size_t>(c))
							{
								d2d->DrawString(row.Cells[static_cast<size_t>(c)].Text,
									drawX + this->CellHorizontalPadding,
									yf + text_top,
									(std::max)(1.0f, c_width - this->CellHorizontalPadding * 2.0f - 14.0f),
									font_height + 2.0f,
									fore, font);
							}

							// Draw drop arrow on right
							{
								const float h = _r_height;
								float iconSize = h * 0.38f;
								if (iconSize < 8.0f) iconSize = 8.0f;
								if (iconSize > 14.0f) iconSize = 14.0f;
								const float padRight = 8.0f;
								const float cx = drawX + c_width - padRight - iconSize * 0.5f;
								const float cy = yf + h * 0.5f;
								DrawGridChevron(d2d, cx, cy, iconSize, dropDownProgress, fore);
							}
						}
						break;
						case ColumnType::Image:
						{
							float _size = c_width < row_height ? c_width : row_height;
							if (_size > 22.0f) _size = 22.0f;
							float left = (c_width - _size) / 2.0f;
							float top = (row_height - _size) / 2.0f;
							const bool selectedCell = (c == this->SelectedColumnIndex && r == this->SelectedRowIndex);
							const bool hoverCell = (c == this->UnderMouseColumnIndex && r == this->UnderMouseRowIndex);
							if (selectedCell || hoverCell)
							{
								DrawGridCellState(this, d2d, drawX, yf, c_width, _r_height, selectedCell, hoverCell);
								DrawGridCellLines(d2d, drawX, yf, c_width, _r_height, this->GridLineColor);
								if (row.Cells.size() > static_cast<size_t>(c))
								{
									if (auto* bmp = row.Cells[static_cast<size_t>(c)].GetImageBitmap(d2d))
										d2d->DrawBitmap(bmp,
											drawX + left,
											yf + top,
											_size, _size
										);
								}
							}
							else
							{
								DrawGridCellLines(d2d, drawX, yf, c_width, _r_height, this->GridLineColor);
								if (row.Cells.size() > static_cast<size_t>(c))
								{
									if (auto* bmp = row.Cells[static_cast<size_t>(c)].GetImageBitmap(d2d))
										d2d->DrawBitmap(bmp,
											drawX + left,
											yf + top,
											_size, _size
										);
								}
							}
						}
						break;
						case ColumnType::Check:
						{
							float _size = c_width < row_height ? c_width : row_height;
							if (_size > 22)_size = 22;
							float left = (c_width - _size) / 2.0f;
							float top = (row_height - _size) / 2.0f;
							float _rsize = _size;
							const bool selectedCell = (c == this->SelectedColumnIndex && r == this->SelectedRowIndex);
							const bool hoverCell = (c == this->UnderMouseColumnIndex && r == this->UnderMouseRowIndex);
							if (selectedCell || hoverCell)
								DrawGridCellState(this, d2d, drawX, yf, c_width, _r_height, selectedCell, hoverCell);
							DrawGridCellLines(d2d, drawX, yf, c_width, _r_height, this->GridLineColor);
							if (row.Cells.size() > static_cast<size_t>(c))
							{
								D2D1_RECT_F box = D2D1::RectF(
									drawX + left + (_rsize * 0.18f),
									yf + top + (_rsize * 0.18f),
									drawX + left + (_rsize * 0.82f),
									yf + top + (_rsize * 0.82f));
								const bool checked = row.Cells[static_cast<size_t>(c)].Tag != 0;
								d2d->FillRoundRect(box, checked ? this->AccentColor : this->EditBackColor, 4.0f);
								d2d->DrawRoundRect(box.left, box.top, box.right - box.left, box.bottom - box.top,
									checked ? this->AccentColor : this->GridLineColor, 1.2f, 4.0f);
								if (checked)
								{
									d2d->DrawLine(
										D2D1::Point2F(box.left + (box.right - box.left) * 0.26f, box.top + (box.bottom - box.top) * 0.53f),
										D2D1::Point2F(box.left + (box.right - box.left) * 0.44f, box.top + (box.bottom - box.top) * 0.70f),
										Colors::White, 1.8f);
									d2d->DrawLine(
										D2D1::Point2F(box.left + (box.right - box.left) * 0.44f, box.top + (box.bottom - box.top) * 0.70f),
										D2D1::Point2F(box.left + (box.right - box.left) * 0.76f, box.top + (box.bottom - box.top) * 0.32f),
										Colors::White, 1.8f);
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
			if (this->AllowUserToAddRows && this->Columns.size() > 0)
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
						for (int c = 0; c < static_cast<int>(this->Columns.size()); c++)
						{
							float colW = this->Columns[static_cast<size_t>(c)].Width;
							if (xf >= _render_width) break;
							if (xf + colW <= 0.0f) { xf += colW; continue; }

							float drawX = xf;
							float c_width = colW;
							if (drawX < 0.0f) { c_width += drawX; drawX = 0.0f; }
							if (drawX + c_width > _render_width) c_width = _render_width - drawX;
							if (c_width <= 0.0f) { xf += colW; continue; }

							const float clipX = drawX;
							const float clipW = c_width;

							d2d->PushDrawRect(clipX, newRowY, clipW, newRowHeight);
							{
								D2D1_RECT_F rowRect = GridInsetRect(drawX, newRowY, c_width, newRowHeight, 3.0f, 3.0f);
								d2d->FillRoundRect(rowRect, this->_isUnderNewRow ? this->UnderMouseItemBackColor : this->NewRowBackColor, this->CellCornerRadius);

								// 绘制新行单元格内容（空单元格样式）
								if (c == 0)
								{
									// 在第一列显示新行指示符 (*)
									float asteriskSize = font_height * 0.5f;
									float asteriskX = drawX + text_top;
									float asteriskY = newRowY + text_top;

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

								DrawGridCellLines(d2d, drawX, newRowY, c_width, newRowHeight, this->GridLineColor);
							}
							d2d->PopDrawRect();
							xf += colW;
						}
					}
				}
			}

			d2d->PushDrawRect(
				0.0f,
				0.0f,
				actualWidth,
				actualHeight);
			{
				if (this->ParentForm->UnderMouse == this)
				{
					d2d->DrawRoundRect(1.0f, 1.0f, actualWidth - 2.0f, actualHeight - 2.0f, this->BolderColor, 1.5f, this->CellCornerRadius);
				}
				else
				{
					d2d->DrawRoundRect(1.0f, 1.0f, actualWidth - 2.0f, actualHeight - 2.0f, this->BolderColor, 1.0f, this->CellCornerRadius);
				}
			}
			d2d->PopDrawRect();
			this->DrawScroll();
		}
		d2d->DrawRoundRect(this->Boder * 0.5f, this->Boder * 0.5f,
			actualWidth - this->Boder, actualHeight - this->Boder,
			this->BolderColor, this->Boder, this->CellCornerRadius);
	}
	if (!this->Enable)
	{
		d2d->FillRect(0, 0, actualWidth, actualHeight, { 1.0f ,1.0f ,1.0f ,0.5f });
	}
	this->EndRender();
}

bool GridView::IsNewRowArea(int x, int y)
{
	if (!this->AllowUserToAddRows) return false;
	if (this->Columns.size() <= 0) return false;

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
	const float totalRowsHeight = rowHeight * (float)this->Rows.size();
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
	if (this->Columns.size() <= 0) return -1;

	auto l = this->CalcScrollLayout();
	const float headHeight = l.HeadHeight;
	const float renderWidth = l.RenderWidth;

	if (x < 0 || x >= (int)renderWidth) return -1;
	if (y <= (int)headHeight) return -1;

	const float rowHeight = this->GetRowHeightPx();
	const float totalRowsHeight = rowHeight * (float)this->Rows.size();
	const float virtualY = ((float)y - headHeight) + this->ScrollYOffset;

	// 检查是否在新行区域内
	if (virtualY < totalRowsHeight || virtualY >= totalRowsHeight + rowHeight)
		return -1;

	// 确定鼠标在哪一列
	const float virtualX = (float)x + this->ScrollXOffset;
	float acc = 0.0f;
	for (int i = 0; i < static_cast<int>(this->Columns.size()); i++)
	{
		if (virtualX >= acc && virtualX < acc + this->Columns[static_cast<size_t>(i)].Width)
		{
			outColumnIndex = i;
			return static_cast<int>(this->Rows.size());  // 返回Rows.size()作为新行的索引
		}
		acc += this->Columns[static_cast<size_t>(i)].Width;
	}

	return -1;
}

void GridView::AddNewRow()
{
	if (!this->AllowUserToAddRows) return;

	// 创建新行
	GridViewRow newRow;
	for (int i = 0; i < static_cast<int>(this->Columns.size()); i++)
	{
		CellValue cell;
		newRow.Cells.push_back(cell);
	}

	// 添加到Rows列表
	int newRowIndex = static_cast<int>(this->Rows.size());
	this->Rows.push_back(newRow);

	// 触发新行添加事件
	this->OnUserAddedRow(this, newRowIndex);

	// 自动选中新行的第一列并开始编辑
	if (this->Columns.size() > 0)
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
	if (col >= 0 && col < static_cast<int>(this->Columns.size()))
	{
		auto font = this->Font;
		float font_height = font->FontHeight;
		float row_height = font_height + 2.0f;
		if (RowHeight != 0.0f)
		{
			row_height = RowHeight;
		}
		auto& column = this->Columns[static_cast<size_t>(col)];
		column.Width = 10.0f;
		for (int i = 0; i < static_cast<int>(this->Rows.size()); i++)
		{
			auto& r = this->Rows[static_cast<size_t>(i)];
			if (r.Cells.size() > static_cast<size_t>(col))
			{
				if (this->Columns[static_cast<size_t>(col)].Type == ColumnType::Text ||
					this->Columns[static_cast<size_t>(col)].Type == ColumnType::LinkedText ||
					this->Columns[static_cast<size_t>(col)].Type == ColumnType::Button ||
					this->Columns[static_cast<size_t>(col)].Type == ColumnType::ComboBox)
				{
					// Button列使用列的ButtonText来计算宽度
					std::wstring textToMeasure;
					if (this->Columns[static_cast<size_t>(col)].Type == ColumnType::Button && !this->Columns[static_cast<size_t>(col)].ButtonText.empty())
					{
						textToMeasure = this->Columns[static_cast<size_t>(col)].ButtonText;
					}
					else
					{
						textToMeasure = r.Cells[static_cast<size_t>(col)].Text;
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

void GridView::RaiseLinkedTextClick(int col, int row)
{
	if (col < 0 || row < 0) return;
	if (col >= static_cast<int>(this->Columns.size()) || row >= static_cast<int>(this->Rows.size())) return;
	if (this->Columns[static_cast<size_t>(col)].Type != ColumnType::LinkedText) return;

	std::wstring text;
	auto& rowObj = this->Rows[static_cast<size_t>(row)];
	if (rowObj.Cells.size() > static_cast<size_t>(col))
		text = rowObj.Cells[static_cast<size_t>(col)].Text;
	this->OnGridViewLinkedTextClick(this, col, row, text);
}

void GridView::EnsureComboBoxCellDefaultSelection(int col, int row)
{
	if (col < 0 || row < 0) return;
	if (col >= static_cast<int>(this->Columns.size()) || row >= static_cast<int>(this->Rows.size())) return;
	if (this->Columns[static_cast<size_t>(col)].Type != ColumnType::ComboBox) return;

	auto& column = this->Columns[static_cast<size_t>(col)];
	if (column.ComboBoxItems.size() <= 0) return;
	auto& rowObj = this->Rows[static_cast<size_t>(row)];
	if (rowObj.Cells.size() <= static_cast<size_t>(col))
		rowObj.Cells.resize((size_t)col + 1);
	auto& cell = rowObj.Cells[static_cast<size_t>(col)];

	const __int64 idx = cell.Tag;
	if (idx < 0 || idx >= static_cast<__int64>(column.ComboBoxItems.size()))
	{
		cell.Tag = 0;
		cell.Text = column.ComboBoxItems[0];
	}
	else
	{
		// Keep Text in sync with index if needed
		const auto& t = column.ComboBoxItems[static_cast<size_t>(idx)];
		if (cell.Text != t)
			cell.Text = t;
	}
}

void GridView::CloseDropDownEditor(bool immediate)
{
	if (!this->_dropDownPopup) return;

	this->_dropDownPopup->Hide(!immediate, immediate);
	if (immediate)
	{
		this->_dropDownPopupColumnIndex = -1;
		this->_dropDownPopupRowIndex = -1;
	}
}

void GridView::ToggleDropDownEditor(int col, int row)
{
	if (col < 0 || row < 0) return;
	if (col >= static_cast<int>(this->Columns.size()) || row >= static_cast<int>(this->Rows.size())) return;
	if (!this->ParentForm) return;
	if (this->Columns[static_cast<size_t>(col)].Type != ColumnType::ComboBox) return;

	EnsureComboBoxCellDefaultSelection(col, row);

	if (this->_dropDownPopup &&
		this->_dropDownPopup->IsOpen() &&
		this->_dropDownPopupColumnIndex == col &&
		this->_dropDownPopupRowIndex == row)
	{
		CloseDropDownEditor();
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

	if (!this->_dropDownPopup)
	{
		this->_dropDownPopup = new DropDownPopup();
	}

	D2D1_RECT_F cellLocal{};
	if (!TryGetCellRectLocal(col, row, cellLocal)) return;

	const auto abs = this->AbsLocation;
	const int x = (int)std::round((float)abs.x + cellLocal.left);
	const int y = (int)std::round((float)abs.y + cellLocal.top);
	const int w = (int)std::round(cellLocal.right - cellLocal.left);
	const int h = (int)std::round(cellLocal.bottom - cellLocal.top);

	auto& column = this->Columns[static_cast<size_t>(col)];
	auto& rowObj = this->Rows[static_cast<size_t>(row)];
	if (rowObj.Cells.size() <= static_cast<size_t>(col))
		rowObj.Cells.resize((size_t)col + 1);
	auto& cell = rowObj.Cells[static_cast<size_t>(col)];

	int selectedIndex = (int)cell.Tag;
	if (selectedIndex < 0) selectedIndex = 0;
	if (selectedIndex >= static_cast<int>(column.ComboBoxItems.size()))
		selectedIndex = column.ComboBoxItems.empty() ? -1 : static_cast<int>(column.ComboBoxItems.size()) - 1;

	this->_dropDownPopup->SetFontEx(this->Font, false);
	this->_dropDownPopup->DropBackColor = this->EditBackColor;
	this->_dropDownPopup->ForeColor = this->EditForeColor;
	this->_dropDownPopup->DropBorderColor = D2D1_COLOR_F{ 0.74f, 0.77f, 0.84f, 0.95f };
	this->_dropDownPopup->AccentColor = this->AccentColor;
	this->_dropDownPopup->SelectedItemBackColor = this->SelectedItemBackColor;
	this->_dropDownPopup->SelectedItemForeColor = this->SelectedItemForeColor;
	this->_dropDownPopup->UnderMouseBackColor = this->UnderMouseItemBackColor;
	this->_dropDownPopup->UnderMouseForeColor = this->UnderMouseItemForeColor;
	this->_dropDownPopup->ScrollBackColor = this->ScrollBackColor;
	this->_dropDownPopup->ScrollForeColor = this->ScrollForeColor;
	this->_dropDownPopup->MinWidth = 80.0f;
	this->_dropDownPopup->CornerRadius = 6.0f;
	this->_dropDownPopup->ItemHeight = (float)(h > 0 ? h : 24);

	this->_dropDownPopup->SelectionChanged.Clear();
	this->_dropDownPopup->SelectionChanged += [this, col, row](DropDownPopup* sender, int selectedIndex, std::wstring selectedText)
		{
			(void)sender;
			(void)selectedText;
			if (col < 0 || row < 0) return;
			if (col >= static_cast<int>(this->Columns.size()) || row >= static_cast<int>(this->Rows.size())) return;
			if (this->Columns[static_cast<size_t>(col)].Type != ColumnType::ComboBox) return;
			auto& column2 = this->Columns[static_cast<size_t>(col)];
			if (column2.ComboBoxItems.size() <= 0) return;
			int idx = selectedIndex;
			if (idx < 0) idx = 0;
			if (idx >= static_cast<int>(column2.ComboBoxItems.size())) idx = static_cast<int>(column2.ComboBoxItems.size()) - 1;
			auto& cell2 = this->Rows[static_cast<size_t>(row)].Cells[static_cast<size_t>(col)];
			cell2.Tag = (__int64)idx;
			cell2.Text = column2.ComboBoxItems[static_cast<size_t>(idx)];
			this->OnGridViewComboBoxSelectionChanged(this, col, row, idx, cell2.Text);
			this->PostRender();
		};
	this->_dropDownPopup->Closed.Clear();
	this->_dropDownPopup->Closed += [this](DropDownPopup* sender)
		{
			(void)sender;
			this->_dropDownPopupColumnIndex = -1;
			this->_dropDownPopupRowIndex = -1;
			this->PostRender();
		};

	this->_dropDownPopupColumnIndex = col;
	this->_dropDownPopupRowIndex = row;
	this->_dropDownPopup->ShowAt(this->ParentForm, this,
		D2D1::RectF((float)x, (float)y, (float)(x + (w > 0 ? w : 1)), (float)(y + (h > 0 ? h : 1))),
		column.ComboBoxItems, selectedIndex, (float)(w > 0 ? w : 1), (float)(h > 0 ? h : 24), 4);
	this->ParentForm->Invalidate(true);
	this->PostRender();
}
void GridView::StartEditingCell(int col, int row)
{
	if (col < 0 || row < 0) return;
	if (col >= static_cast<int>(this->Columns.size()) || row >= static_cast<int>(this->Rows.size())) return;

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
		this->EditingText = this->Rows[static_cast<size_t>(row)].Cells[static_cast<size_t>(col)].Text;
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
			this->EditingRowIndex < static_cast<int>(this->Rows.size()) && this->EditingColumnIndex < static_cast<int>(this->Columns.size()))
		{
			this->Rows[static_cast<size_t>(this->EditingRowIndex)].Cells[static_cast<size_t>(this->EditingColumnIndex)].Text = this->EditingOriginalText;
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
	if (this->EditingRowIndex >= static_cast<int>(this->Rows.size())) return;
	if (this->EditingColumnIndex >= static_cast<int>(this->Columns.size())) return;
	this->Rows[static_cast<size_t>(this->EditingRowIndex)].Cells[static_cast<size_t>(this->EditingColumnIndex)].Text = this->EditingText;
}
void GridView::AdjustScrollPosition()
{
	auto l = this->CalcScrollLayout();
	const float rowH = this->GetRowHeightPx();
	const float headH = this->GetHeadHeightPx();
	const float contentH = std::max(0.0f, l.RenderHeight - headH);
	const float totalH = (rowH > 0.0f) ? (rowH * (float)this->Rows.size()) : 0.0f;
	const float maxScrollY = std::max(0.0f, totalH - contentH);

	if (this->SelectedRowIndex < 0 || this->SelectedRowIndex >= static_cast<int>(this->Rows.size())) return;
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
bool GridView::CanHandleMouseWheel(int delta, int xof, int yof)
{
	(void)xof;
	(void)yof;
	if (delta == 0) return false;
	auto l = this->CalcScrollLayout();
	if ((GetKeyState(VK_SHIFT) & 0x8000) && l.NeedH && l.MaxScrollX > 0.0f)
	{
		return delta > 0
			? this->ScrollXOffset > 0.0f
			: this->ScrollXOffset < l.MaxScrollX;
	}
	if (!l.NeedV || l.MaxScrollY <= 0.0f)
		return false;
	return delta > 0
		? this->ScrollYOffset > 0.0f
		: this->ScrollYOffset < l.MaxScrollY;
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
	std::vector<std::wstring> files;

	for (UINT i = 0; i < uFileNum; i++)
	{
		DragQueryFile(hDropInfo, i, strFileName, MAX_PATH);
		files.push_back(strFileName);
	}
	DragFinish(hDropInfo);

	if (files.size() > 0)
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
		if (this->_resizeColumnIndex >= 0 && this->_resizeColumnIndex < static_cast<int>(this->Columns.size()))
		{
			if (this->Columns[static_cast<size_t>(this->_resizeColumnIndex)].Width != newWidth)
			{
				this->Columns[static_cast<size_t>(this->_resizeColumnIndex)].Width = newWidth;
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
		SetScrollByPos(static_cast<float>(yof));
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
		if (this->Rows.size() > 0 && l.MaxScrollY > 0.0f && l.RenderHeight > 0.0f && l.ContentHeight > 0.0f)
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
			undermouseIndex.y < static_cast<LONG>(this->Rows.size()) && undermouseIndex.x < static_cast<LONG>(this->Columns.size()))
		{
			// Keep hover index in sync even if we didn't get a prior WM_MOUSEMOVE.
			this->UnderMouseColumnIndex = undermouseIndex.x;
			this->UnderMouseRowIndex = undermouseIndex.y;

			if (this->Columns[static_cast<size_t>(undermouseIndex.x)].Type == ColumnType::Button)
			{
				if (this->Editing)
					SaveCurrentEditingCell(true);
				CloseDropDownEditor();

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

			if (this->Columns[static_cast<size_t>(undermouseIndex.x)].Type == ColumnType::LinkedText)
			{
				if (this->Editing)
					SaveCurrentEditingCell(true);
				CloseDropDownEditor();

				this->SelectedColumnIndex = undermouseIndex.x;
				this->SelectedRowIndex = undermouseIndex.y;
				this->SelectionChanged(this);

				this->_linkedTextMouseDown = true;
				this->_linkedTextDownColumnIndex = undermouseIndex.x;
				this->_linkedTextDownRowIndex = undermouseIndex.y;
				SetCapture(this->ParentForm->Handle);

				MouseEventArgs event_obj(MouseButtons::Left, 0, xof, yof, 0);
				this->OnMouseDown(this, event_obj);
				this->PostRender();
				return;
			}

			if (this->Editing && undermouseIndex.x == this->EditingColumnIndex && undermouseIndex.y == this->EditingRowIndex)
			{
				SetEditingCaretFromMousePoint(xof, yof);
			}
			else
			{
				HandleCellClick(undermouseIndex.x, undermouseIndex.y);
				if (this->Editing && undermouseIndex.x == this->EditingColumnIndex && undermouseIndex.y == this->EditingRowIndex)
				{
					SetEditingCaretFromMousePoint(xof, yof);
				}
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
			if (hitResult >= 0 && newRowCol >= 0 && newRowCol < static_cast<int>(this->Columns.size()))
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
			undermouseIndex.x < static_cast<LONG>(this->Columns.size()) && undermouseIndex.y < static_cast<LONG>(this->Rows.size()));
		const bool isButtonCell = validCell && (this->Columns[static_cast<size_t>(undermouseIndex.x)].Type == ColumnType::Button);

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

	if (this->_linkedTextMouseDown)
	{
		POINT undermouseIndex = GetGridViewUnderMouseItem(xof, yof, this);
		const bool hitSameCell = (undermouseIndex.x == this->_linkedTextDownColumnIndex && undermouseIndex.y == this->_linkedTextDownRowIndex);
		const bool validCell = (undermouseIndex.x >= 0 && undermouseIndex.y >= 0 &&
			undermouseIndex.x < static_cast<LONG>(this->Columns.size()) && undermouseIndex.y < static_cast<LONG>(this->Rows.size()));
		const bool isLinkedTextCell = validCell && (this->Columns[static_cast<size_t>(undermouseIndex.x)].Type == ColumnType::LinkedText);

		this->_linkedTextMouseDown = false;
		this->_linkedTextDownColumnIndex = -1;
		this->_linkedTextDownRowIndex = -1;

		this->InScroll = false;
		this->InHScroll = false;
		ReleaseCapture();
		MouseEventArgs event_obj(MouseButtons::Left, 0, xof, yof, 0);
		this->OnMouseUp(this, event_obj);

		if (hitSameCell && isLinkedTextCell)
		{
			RaiseLinkedTextClick(undermouseIndex.x, undermouseIndex.y);
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
void GridView::HandleLeftButtonDoubleClick(WPARAM wParam, int xof, int yof)
{
	POINT undermouseIndex = GetGridViewUnderMouseItem(xof, yof, this);
	if (undermouseIndex.x >= 0 && undermouseIndex.y >= 0 &&
		undermouseIndex.x < static_cast<LONG>(this->Columns.size()) &&
		undermouseIndex.y < static_cast<LONG>(this->Rows.size()) &&
		IsEditableTextCell(undermouseIndex.x, undermouseIndex.y))
	{
		StartEditingCell(undermouseIndex.x, undermouseIndex.y);
		if (this->Editing &&
			this->EditingColumnIndex == undermouseIndex.x &&
			this->EditingRowIndex == undermouseIndex.y)
		{
			this->EditSelectionStart = 0;
			this->EditSelectionEnd = (int)this->EditingText.size();
			this->EditOffsetX = 0.0f;
			if (this->ParentForm)
				this->ParentForm->Selected = this;
			EditSetImeCompositionWindow();
			this->PostRender();
		}
	}

	MouseEventArgs event_obj(MouseButtons::Left, 2, xof, yof, HIWORD(wParam));
	this->OnMouseDoubleClick(this, event_obj);
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
			if (this->SelectedRowIndex < static_cast<int>(this->Rows.size()) - 1)
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
		if (SelectedColumnIndex < static_cast<int>(this->Columns.size()) - 1) SelectedColumnIndex++;
		break;
	case VK_LEFT:
		if (SelectedColumnIndex > 0) SelectedColumnIndex--;
		break;
	case VK_DOWN:
		if (SelectedRowIndex < static_cast<int>(this->Rows.size()) - 1) SelectedRowIndex++;
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
		ToggleDropDownEditor(col, row);
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

	case WM_LBUTTONDBLCLK:
		HandleLeftButtonDoubleClick(wParam, xof, yof);
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
	float rowHeight = font->FontHeight + 10.0f;
	if (this->RowHeight != 0.0f) rowHeight = this->RowHeight;
	return rowHeight;
}
float GridView::GetHeadHeightPx()
{
	auto font = this->Font;
	auto headFont = this->HeadFont ? this->HeadFont : font;
	float headHeight = (this->HeadHeight == 0.0f) ? headFont->FontHeight + 12.0f : this->HeadHeight;
	return headHeight;
}
bool GridView::TryGetCellRectLocal(int col, int row, D2D1_RECT_F& outRect)
{
	if (col < 0 || row < 0) return false;
	if (col >= static_cast<int>(this->Columns.size()) || row >= static_cast<int>(this->Rows.size())) return false;

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
	for (int i = 0; i < col; i++) left += this->Columns[static_cast<size_t>(i)].Width;
	float width = this->Columns[static_cast<size_t>(col)].Width;
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
	if (col >= static_cast<int>(this->Columns.size()) || row >= static_cast<int>(this->Rows.size())) return false;
	return this->Columns[static_cast<size_t>(col)].Type == ColumnType::Text && this->Columns[static_cast<size_t>(col)].CanEdit;
}
bool GridView::SetEditingCaretFromMousePoint(int xof, int yof)
{
	if (!this->Editing) return false;
	D2D1_RECT_F rect{};
	if (!TryGetCellRectLocal(this->EditingColumnIndex, this->EditingRowIndex, rect)) return false;

	float cellWidth = rect.right - rect.left;
	float cellHeight = rect.bottom - rect.top;
	float lx = (float)xof - rect.left;
	float ly = (float)yof - rect.top;
	int pos = EditHitTestTextPosition(cellWidth, cellHeight, lx, ly);
	this->EditSelectionStart = this->EditSelectionEnd = pos;
	EditUpdateScroll(cellWidth);
	return true;
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
		this->EditingRowIndex < static_cast<int>(this->Rows.size()) && this->EditingColumnIndex < static_cast<int>(this->Columns.size()))
	{
		this->Rows[static_cast<size_t>(this->EditingRowIndex)].Cells[static_cast<size_t>(this->EditingColumnIndex)].Text = this->EditingText;
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
		this->EditingRowIndex < static_cast<int>(this->Rows.size()) && this->EditingColumnIndex < static_cast<int>(this->Columns.size()))
	{
		this->Rows[static_cast<size_t>(this->EditingRowIndex)].Cells[static_cast<size_t>(this->EditingColumnIndex)].Text = this->EditingText;
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
		this->EditingRowIndex < static_cast<int>(this->Rows.size()) && this->EditingColumnIndex < static_cast<int>(this->Columns.size()))
	{
		this->Rows[static_cast<size_t>(this->EditingRowIndex)].Cells[static_cast<size_t>(this->EditingColumnIndex)].Text = this->EditingText;
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
	this->ParentForm->SetImeCompositionWindowFromLogicalRect(
		D2D1_RECT_F{
			(float)pos.x + rect.left,
			(float)pos.y + rect.top,
			(float)pos.x + rect.right,
			(float)pos.y + rect.bottom
		});
}
#pragma endregion
