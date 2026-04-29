#pragma once
#define NOMINMAX
#include "GridView.h"
#include "Form.h"
#include <algorithm>
#include <cmath>
#include <cwchar>
#pragma comment(lib, "Imm32.lib")

CellValue::CellValue() : Kind(CellValueKind::Empty), _storage(std::monostate{})
{
}
CellValue::CellValue(std::wstring s) : Kind(CellValueKind::Text), _storage(std::move(s))
{
}
CellValue::CellValue(wchar_t* s) : CellValue(std::wstring(s ? s : L""))
{
}
CellValue::CellValue(const wchar_t* s) : CellValue(std::wstring(s ? s : L""))
{
}
CellValue::CellValue(std::shared_ptr<BitmapSource> img) : Kind(CellValueKind::Image), _storage(std::move(img))
{
}
CellValue::CellValue(__int64 tag) : Kind(CellValueKind::Integer), _storage(tag)
{
}
CellValue::CellValue(bool tag) : Kind(CellValueKind::Boolean), _storage(tag)
{
}
CellValue::CellValue(__int32 tag) : CellValue(static_cast<__int64>(tag))
{
}
CellValue::CellValue(unsigned __int32 tag) : CellValue(static_cast<__int64>(tag))
{
}
CellValue::CellValue(unsigned __int64 tag) : CellValue(static_cast<__int64>(tag))
{
}
CellValue::CellValue(PVOID tag) : Kind(CellValueKind::Pointer), _storage(tag)
{
}

std::wstring CellValue::GetText() const
{
	if (const auto* text = std::get_if<std::wstring>(&_storage))
		return *text;
	if (const auto* combo = std::get_if<ComboSelectionValue>(&_storage))
		return combo->Text;
	if (const auto* tagged = std::get_if<TaggedTextValue>(&_storage))
		return tagged->Text;
	if (const auto* value = std::get_if<__int64>(&_storage))
		return std::to_wstring(*value);
	if (const auto* value = std::get_if<bool>(&_storage))
		return *value ? L"true" : L"false";
	if (const auto* pointer = std::get_if<PVOID>(&_storage))
	{
		wchar_t buffer[(sizeof(PVOID) * 2) + 3]{};
		swprintf_s(buffer, L"%p", *pointer);
		return buffer;
	}
	return L"";
}

void CellValue::SetText(std::wstring text)
{
	if (const auto* tagged = std::get_if<TaggedTextValue>(&_storage))
	{
		_storage = TaggedTextValue{ std::move(text), tagged->Tag };
		return;
	}
	Kind = CellValueKind::Text;
	_storage = std::move(text);
}

std::shared_ptr<BitmapSource> CellValue::GetImage() const
{
	if (const auto* image = std::get_if<std::shared_ptr<BitmapSource>>(&_storage))
		return *image;
	return nullptr;
}

void CellValue::SetImage(std::shared_ptr<BitmapSource> image)
{
	Kind = CellValueKind::Image;
	_storage = std::move(image);
}

__int64 CellValue::GetTag() const
{
	if (const auto* value = std::get_if<__int64>(&_storage))
		return *value;
	if (const auto* value = std::get_if<bool>(&_storage))
		return *value ? 1 : 0;
	if (const auto* pointer = std::get_if<PVOID>(&_storage))
		return reinterpret_cast<__int64>(*pointer);
	if (const auto* combo = std::get_if<ComboSelectionValue>(&_storage))
		return combo->SelectedIndex;
	if (const auto* tagged = std::get_if<TaggedTextValue>(&_storage))
		return tagged->Tag;
	return 0;
}

void CellValue::SetTag(__int64 tag)
{
	const auto text = GetText();
	if (!text.empty())
	{
		_storage = TaggedTextValue{ text, tag };
		return;
	}
	Kind = CellValueKind::Integer;
	_storage = tag;
}

PVOID CellValue::GetPointer() const
{
	if (const auto* pointer = std::get_if<PVOID>(&_storage))
		return *pointer;
	if (const auto* value = std::get_if<__int64>(&_storage))
		return reinterpret_cast<PVOID>(*value);
	if (const auto* tagged = std::get_if<TaggedTextValue>(&_storage))
		return reinterpret_cast<PVOID>(tagged->Tag);
	return nullptr;
}

void CellValue::SetPointer(PVOID pointer)
{
	Kind = CellValueKind::Pointer;
	const auto text = GetText();
	if (!text.empty())
		_storage = TaggedTextValue{ text, reinterpret_cast<__int64>(pointer) };
	else
		_storage = pointer;
}

bool CellValue::GetBool() const
{
	return GetTag() != 0;
}

void CellValue::SetBool(bool value)
{
	Kind = CellValueKind::Boolean;
	_storage = value;
}

int CellValue::GetComboIndex() const
{
	if (const auto* combo = std::get_if<ComboSelectionValue>(&_storage))
		return combo->SelectedIndex;
	return static_cast<int>(GetTag());
}

void CellValue::SetComboSelection(int selectedIndex, const std::wstring& selectedText)
{
	Kind = CellValueKind::ComboSelection;
	_storage = ComboSelectionValue{ selectedIndex, selectedText };
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
	for (int i = 0; i < (int)this->_columns.size(); i++)
		sum += this->_columns[i].Width;
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
		float newRowAreaHeight = (this->AllowUserToAddRows && this->_columns.size() > 0) ? l.RowHeight : 0.0f;
		float totalRowsH = (l.RowHeight > 0.0f) ? (l.RowHeight * (float)this->_rows.size()) : 0.0f;
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
			l.MaxScrollRow = std::max(0, (int)this->_rows.size() - visibleRows);
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
	float newRowAreaHeight = (this->AllowUserToAddRows && this->_columns.size() > 0) ? l.RowHeight : 0.0f;
	l.TotalRowsHeight = (l.RowHeight > 0.0f) ? (l.RowHeight * (float)this->_rows.size()) : 0.0f;
	l.TotalRowsHeight += newRowAreaHeight;  // 加上新行区域高度
	l.MaxScrollY = std::max(0.0f, l.TotalRowsHeight - contentH);
	l.VisibleRows = (l.RowHeight > 0.0f && contentH > 0.0f) ? ((int)std::ceil(contentH / l.RowHeight) + 1) : 0;
	if (l.VisibleRows < 0) l.VisibleRows = 0;
	l.MaxScrollRow = std::max(0, (int)this->_rows.size() - l.VisibleRows);
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
			undermouseIndex.y < static_cast<LONG>(this->_rows.size()) && undermouseIndex.x < static_cast<LONG>(this->_columns.size()))
		{
			if (this->_columns[static_cast<size_t>(undermouseIndex.x)].Type == ColumnType::Button)
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
	return _rows[idx];
}
GridViewRow& GridView::SelectedRow()
{
	static GridViewRow default_;
	if (this->SelectedRowIndex >= 0 && this->SelectedRowIndex < static_cast<int>(this->_rows.size()))
	{
		return this->_rows[static_cast<size_t>(this->SelectedRowIndex)];
	}
	return default_;
}
std::wstring GridView::SelectedValue() const
{
	if (this->SelectedRowIndex >= 0 && this->SelectedRowIndex < static_cast<int>(this->_rows.size()) &&
		this->SelectedColumnIndex >= 0 &&
		this->SelectedColumnIndex < static_cast<int>(this->_rows[static_cast<size_t>(this->SelectedRowIndex)].Cells.size()))
	{
		return this->_rows[static_cast<size_t>(this->SelectedRowIndex)].Cells[static_cast<size_t>(SelectedColumnIndex)].GetText();
	}
	return L"";
}

int GridView::RowCount() const
{
	return static_cast<int>(this->_rows.size());
}

int GridView::ColumnCount() const
{
	return static_cast<int>(this->_columns.size());
}

const std::vector<GridViewColumn>& GridView::GetColumns() const
{
	return this->_columns;
}

const std::vector<GridViewRow>& GridView::GetRows() const
{
	return this->_rows;
}

GridViewColumn* GridView::GetColumn(int index)
{
	if (index < 0 || index >= static_cast<int>(this->_columns.size())) return nullptr;
	return &this->_columns[static_cast<size_t>(index)];
}

const GridViewColumn* GridView::GetColumn(int index) const
{
	if (index < 0 || index >= static_cast<int>(this->_columns.size())) return nullptr;
	return &this->_columns[static_cast<size_t>(index)];
}

GridViewColumn& GridView::ColumnAt(int index)
{
	return this->_columns[static_cast<size_t>(index)];
}

const GridViewColumn& GridView::ColumnAt(int index) const
{
	return this->_columns[static_cast<size_t>(index)];
}

GridViewRow* GridView::GetRow(int index)
{
	if (index < 0 || index >= static_cast<int>(this->_rows.size())) return nullptr;
	return &this->_rows[static_cast<size_t>(index)];
}

const GridViewRow* GridView::GetRow(int index) const
{
	if (index < 0 || index >= static_cast<int>(this->_rows.size())) return nullptr;
	return &this->_rows[static_cast<size_t>(index)];
}

GridViewRow& GridView::RowAt(int index)
{
	return this->_rows[static_cast<size_t>(index)];
}

const GridViewRow& GridView::RowAt(int index) const
{
	return this->_rows[static_cast<size_t>(index)];
}

int GridView::AddColumn(const GridViewColumn& column)
{
	CloseComboBoxEditor();
	this->_columns.push_back(column);
	for (auto& row : this->_rows)
	{
		if (row.Cells.size() < this->_columns.size())
			row.Cells.resize(this->_columns.size());
	}
	this->PostRender();
	return static_cast<int>(this->_columns.size()) - 1;
}

void GridView::ClearColumns()
{
	CancelEditing(false);
	CloseComboBoxEditor();
	this->_columns.clear();
	for (auto& row : this->_rows)
		row.Cells.clear();
	this->SelectedColumnIndex = -1;
	this->UnderMouseColumnIndex = -1;
	this->SortedColumnIndex = -1;
	this->ScrollXOffset = 0.0f;
	ClearImageCache();
	this->PostRender();
}

bool GridView::RemoveColumnAt(int index)
{
	if (index < 0 || index >= static_cast<int>(this->_columns.size())) return false;
	CloseComboBoxEditor();
	if (this->Editing && this->EditingColumnIndex == index)
		CancelEditing(true);
	else if (this->Editing && index < this->EditingColumnIndex)
	{
		SaveCurrentEditingCell(true);
		this->EditingColumnIndex--;
	}
	this->_columns.erase(this->_columns.begin() + index);
	for (auto& row : this->_rows)
	{
		if (index < static_cast<int>(row.Cells.size()))
			row.Cells.erase(row.Cells.begin() + index);
	}
	if (this->SelectedColumnIndex == index)
		this->SelectedColumnIndex = this->_columns.empty() ? -1 : std::min(index, static_cast<int>(this->_columns.size()) - 1);
	else if (index < this->SelectedColumnIndex)
		this->SelectedColumnIndex--;
	if (this->UnderMouseColumnIndex == index)
		this->UnderMouseColumnIndex = -1;
	else if (index < this->UnderMouseColumnIndex)
		this->UnderMouseColumnIndex--;
	if (this->SortedColumnIndex == index)
		this->SortedColumnIndex = -1;
	else if (index < this->SortedColumnIndex)
		this->SortedColumnIndex--;
	this->ScrollXOffset = std::min(this->ScrollXOffset, this->CalcScrollLayout().MaxScrollX);
	ClearImageCache();
	this->PostRender();
	return true;
}

int GridView::AddRow(const GridViewRow& row)
{
	CloseComboBoxEditor();
	GridViewRow newRow = row;
	if (newRow.Cells.size() < this->_columns.size())
		newRow.Cells.resize(this->_columns.size());
	this->_rows.push_back(std::move(newRow));
	ClearImageCache();
	this->PostRender();
	return static_cast<int>(this->_rows.size()) - 1;
}

void GridView::ClearRows()
{
	CancelEditing(false);
	CloseComboBoxEditor();
	this->_rows.clear();
	this->ScrollYOffset = 0.0f;
	this->ScrollRowPosition = 0;
	this->SelectedRowIndex = -1;
	this->UnderMouseRowIndex = -1;
	ClearImageCache();
	this->PostRender();
}

bool GridView::SwapRows(int first, int second)
{
	if (first < 0 || second < 0) return false;
	if (first >= static_cast<int>(this->_rows.size()) || second >= static_cast<int>(this->_rows.size())) return false;
	if (first == second) return true;
	CancelEditing(true);
	CloseComboBoxEditor();
	std::swap(this->_rows[static_cast<size_t>(first)], this->_rows[static_cast<size_t>(second)]);
	auto swapIndex = [first, second](int& value)
		{
			if (value == first) value = second;
			else if (value == second) value = first;
		};
	swapIndex(this->SelectedRowIndex);
	swapIndex(this->UnderMouseRowIndex);
	ClearImageCache();
	this->PostRender();
	return true;
}

bool GridView::RemoveRowAt(int index)
{
	if (index < 0 || index >= static_cast<int>(this->_rows.size())) return false;
	CloseComboBoxEditor();
	if (this->Editing && this->EditingRowIndex == index)
		CancelEditing(true);
	else if (this->Editing && index < this->EditingRowIndex)
	{
		SaveCurrentEditingCell(true);
		this->EditingRowIndex--;
	}
	this->_rows.erase(this->_rows.begin() + index);
	if (this->SelectedRowIndex == index)
	{
		this->SelectedRowIndex = this->_rows.empty() ? -1 : std::min(index, static_cast<int>(this->_rows.size()) - 1);
		EditClearHistory();
		this->SelectionChanged(this);
	}
	else if (index < this->SelectedRowIndex)
	{
		this->SelectedRowIndex--;
	}
	if (this->UnderMouseRowIndex == index)
		this->UnderMouseRowIndex = -1;
	else if (index < this->UnderMouseRowIndex)
		this->UnderMouseRowIndex--;
	AdjustScrollPosition();
	ClearImageCache();
	this->PostRender();
	return true;
}

CellValue* GridView::GetCell(int col, int row)
{
	if (row < 0 || row >= static_cast<int>(this->_rows.size())) return nullptr;
	auto& cells = this->_rows[static_cast<size_t>(row)].Cells;
	if (col < 0 || col >= static_cast<int>(cells.size())) return nullptr;
	return &cells[static_cast<size_t>(col)];
}

const CellValue* GridView::GetCell(int col, int row) const
{
	if (row < 0 || row >= static_cast<int>(this->_rows.size())) return nullptr;
	const auto& cells = this->_rows[static_cast<size_t>(row)].Cells;
	if (col < 0 || col >= static_cast<int>(cells.size())) return nullptr;
	return &cells[static_cast<size_t>(col)];
}

bool GridView::SetCellValue(int col, int row, const CellValue& value)
{
	if (row < 0 || row >= static_cast<int>(this->_rows.size())) return false;
	if (col < 0 || col >= static_cast<int>(this->_columns.size())) return false;
	auto& cells = this->_rows[static_cast<size_t>(row)].Cells;
	if (cells.size() <= static_cast<size_t>(col))
		cells.resize(static_cast<size_t>(col) + 1);
	cells[static_cast<size_t>(col)] = value;
	this->PostRender();
	return true;
}

void GridView::SelectCell(int col, int row, bool fireEvent)
{
	if (this->_rows.empty() || this->_columns.empty())
	{
		this->SelectedColumnIndex = -1;
		this->SelectedRowIndex = -1;
		this->PostRender();
		return;
	}
	const int newCol = std::clamp(col, 0, static_cast<int>(this->_columns.size()) - 1);
	const int newRow = std::clamp(row, 0, static_cast<int>(this->_rows.size()) - 1);
	const bool changed = (this->SelectedColumnIndex != newCol || this->SelectedRowIndex != newRow);
	this->SelectedColumnIndex = newCol;
	this->SelectedRowIndex = newRow;
	AdjustScrollPosition();
	if (changed && fireEvent)
	{
		EditClearHistory();
		this->SelectionChanged(this);
	}
	this->PostRender();
}

void GridView::SelectRow(int row, bool fireEvent)
{
	if (this->_rows.empty())
	{
		this->SelectedRowIndex = -1;
		this->SelectedColumnIndex = -1;
		this->PostRender();
		return;
	}
	int activeCol = this->SelectedColumnIndex;
	if (activeCol < 0 && !this->_columns.empty()) activeCol = 0;
	if (!this->_columns.empty())
		activeCol = std::clamp(activeCol, 0, static_cast<int>(this->_columns.size()) - 1);
	else
		activeCol = -1;
	SelectCell(activeCol, row, fireEvent);
}

void GridView::Clear()
{
	CancelEditing(false);
	CloseComboBoxEditor();
	this->_rows.clear();
	this->_columns.clear();
	this->ScrollYOffset = 0.0f;
	this->ScrollRowPosition = 0;
	this->ScrollXOffset = 0.0f;
	this->SelectedColumnIndex = -1;
	this->SelectedRowIndex = -1;
	this->UnderMouseColumnIndex = -1;
	this->UnderMouseRowIndex = -1;
	this->SortedColumnIndex = -1;
	ClearImageCache();
	this->PostRender();
}

static int CompareWStringDefault(const std::wstring& a, const std::wstring& b)
{
	if (a == b) return 0;
	return (a < b) ? -1 : 1;
}

static std::wstring CellToStringDefault(const CellValue* v)
{
	if (!v) return L"";
	auto text = v->GetText();
	if (!text.empty()) return text;
	return std::to_wstring(v->GetTag());
}

void GridView::SortByColumn(int col, bool ascending)
{
	if (col < 0 || col >= static_cast<int>(this->_columns.size())) return;
	if (this->_rows.size() <= 1) return;

	CloseComboBoxEditor();
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

	const auto sortFunc = this->_columns[col].SortFunc;
	std::stable_sort(this->_rows.begin(), this->_rows.end(),
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
	for (; s_x < ct->_columns.size(); s_x++)
	{
		float c_width = ct->_columns[s_x].Width;
		if (virtualX >= acc && virtualX < acc + c_width)
		{
			xindex = s_x;
			break;
		}
		acc += ct->_columns[s_x].Width;
	}
	const float virtualY = ((float)y - head_height) + ct->ScrollYOffset;
	if (virtualY >= 0.0f && row_height > 0.0f)
	{
		const int idx = (int)(virtualY / row_height);
		if (idx >= 0 && idx < static_cast<int>(ct->_rows.size())) yindex = idx;
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
	for (int i = 0; i < static_cast<int>(this->_columns.size()); i++)
	{
		float cWidth = this->_columns[static_cast<size_t>(i)].Width;
		if (virtualX >= xf && virtualX < xf + cWidth)
			return i;
		xf += this->_columns[static_cast<size_t>(i)].Width;
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
	for (int i = 0; i < static_cast<int>(this->_columns.size()); i++)
	{
		const float cWidth = this->_columns[static_cast<size_t>(i)].Width;
		const float rightEdge = xf + cWidth;
		if (std::abs(virtualX - rightEdge) <= hitPx)
			return i;

		xf += this->_columns[static_cast<size_t>(i)].Width;
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
	const float contentH = l.ContentHeight;
	const float totalH = l.TotalRowsHeight;
	if (totalH > contentH && contentH > 0.0f)
	{
		float thumbH = _render_height * (contentH / totalH);
		const float minThumbH = _render_height * 0.1f;
		if (thumbH < minThumbH) thumbH = minThumbH;
		if (thumbH > _render_height) thumbH = _render_height;

		const float maxScrollY = l.MaxScrollY;
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
	auto font = this->Font;
	auto size = this->ActualSize();

	auto l = this->CalcScrollLayout();

	if (l.NeedV && l.TotalRowsHeight > 0.0f)
	{
		float _render_width = l.RenderWidth;
		float _render_height = l.RenderHeight;
		const float contentH = l.ContentHeight;
		const float totalH = l.TotalRowsHeight;
		if (totalH > contentH && contentH > 0.0f)
		{
			float thumbH = _render_height * (contentH / totalH);
			const float minThumbH = _render_height * 0.1f;
			if (thumbH < minThumbH) thumbH = minThumbH;
			if (thumbH > _render_height) thumbH = _render_height;

			const float maxScrollY = l.MaxScrollY;
			const float moveSpace = std::max(0.0f, _render_height - thumbH);
			float per = 0.0f;
			if (maxScrollY > 0.0f)
				per = std::clamp(this->ScrollYOffset / maxScrollY, 0.0f, 1.0f);
			const float thumbTop = per * moveSpace;

			d2d->FillRoundRect(_render_width, 0, l.ScrollBarSize, _render_height, this->ScrollBackColor, 4.0f);
			d2d->FillRoundRect(_render_width, thumbTop, l.ScrollBarSize, thumbH, this->ScrollForeColor, 4.0f);
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

	d2d->FillRoundRect(barX, barY, barW, barH, this->ScrollBackColor, 4.0f);
	d2d->FillRoundRect(thumbX, barY, thumbW, barH, this->ScrollForeColor, 4.0f);
}

void GridView::DrawCorner(const ScrollLayout& l)
{
	auto d2d = this->ParentForm->Render;
	const float x = l.RenderWidth;
	const float y = l.RenderHeight;
	d2d->FillRect(x, y, l.ScrollBarSize, l.ScrollBarSize, this->ScrollBackColor);
}

void GridView::SetScrollByPos(float yof)
{
	auto l = this->CalcScrollLayout();
	const float renderingHeight = l.RenderHeight;
	const float rowHeight = this->GetRowHeightPx();
	const float contentHeight = l.ContentHeight;
	const float totalHeight = l.TotalRowsHeight;
	const float maxScrollY = l.MaxScrollY;

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
			if (l.TotalRowsHeight <= 0.0f)
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
				if (!this->_rows.empty() && this->ScrollRowPosition >= static_cast<int>(this->_rows.size()))
					this->ScrollRowPosition = static_cast<int>(this->_rows.size()) - 1;
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
			for (; i < static_cast<int>(this->_columns.size()); i++)
			{
				float colW = this->_columns[i].Width;
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

				auto ht = head_font->GetTextSize(this->_columns[i].Name);
				float draw_x_offset = (c_width - ht.width) / 2.0f;
				if (draw_x_offset < 0)draw_x_offset = 0;
				float draw_y_offset = (head_height - head_font_height) / 2.0f;
				if (draw_y_offset < 0)draw_y_offset = 0;
				d2d->PushDrawRect(clipX, 0, clipW, head_height);
				{
					d2d->FillRect(drawX, 0, c_width, head_height, this->HeadBackColor);
					d2d->DrawRect(drawX, 0, c_width, head_height, this->HeadForeColor, 2.f);
					d2d->DrawString(this->_columns[i].Name,
						drawX + draw_x_offset,
						draw_y_offset,
						this->HeadForeColor, head_font);
				}
				d2d->PopDrawRect();
				xf += colW;
			}

			const int maxRows = l.VisibleRows;
			i = 0;
	for (int r = s_y; r < static_cast<int>(this->_rows.size()) && i < maxRows; r++, i++)
	{
		GridViewRow& row = this->_rows[static_cast<size_t>(r)];
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
				const bool isSelectedRow = this->FullRowSelect && (r == this->SelectedRowIndex);
				const bool isHotRow = !isSelectedRow && (r == this->UnderMouseRowIndex);
				if (isSelectedRow || isHotRow)
				{
					d2d->PushDrawRect(0.0f, clipY, _render_width, clipH);
					d2d->FillRect(0.0f, yf, _render_width, row_height,
						isSelectedRow ? this->SelectedItemBackColor : this->UnderMouseItemBackColor);
					d2d->PopDrawRect();
				}
				float xf = -this->ScrollXOffset;
		for (int c = s_x; c < static_cast<int>(this->_columns.size()); c++)
		{
			float colW = this->_columns[static_cast<size_t>(c)].Width;
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
						const bool isActiveCell = (c == this->SelectedColumnIndex && r == this->SelectedRowIndex);
						const bool isHotCell = (c == this->UnderMouseColumnIndex && r == this->UnderMouseRowIndex);
						switch (this->_columns[c].Type)
						{
						case ColumnType::Text:
						{
							if (isActiveCell || isSelectedRow)
							{
								if (isActiveCell && this->Editing && this->EditingColumnIndex == c && this->EditingRowIndex == r && this->ParentForm->Selected == this)
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

										d2d->FillRect(drawX, yf, c_width, _r_height, this->EditBackColor);
										d2d->DrawRect(drawX, yf, c_width, _r_height, this->SelectedItemForeColor,
											r == this->UnderMouseRowIndex ? 1.0f : 0.5f);

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
									d2d->FillRect(drawX, yf, c_width, _r_height, this->SelectedItemBackColor);
									d2d->DrawRect(drawX, yf, c_width, _r_height, this->SelectedItemForeColor,
										r == this->UnderMouseRowIndex ? 1.0f : 0.5f);
									if (row.Cells.size() > static_cast<size_t>(c))
										d2d->DrawString(row.Cells[static_cast<size_t>(c)].GetText(),
											drawX + 1.0f,
											yf + text_top,
											this->SelectedItemForeColor, font);
								}
							}
							else if (isHotCell)
							{
								d2d->FillRect(drawX, yf, c_width, _r_height, this->UnderMouseItemBackColor);
								d2d->DrawRect(drawX, yf, c_width, _r_height, this->UnderMouseItemForeColor,
									r == this->UnderMouseRowIndex ? 1.0f : 0.5f);
								if (row.Cells.size() > static_cast<size_t>(c))
									d2d->DrawString(row.Cells[static_cast<size_t>(c)].GetText(),
										drawX + 1.0f,
										yf + text_top,
										this->UnderMouseItemForeColor, font);
							}
							else
							{
								d2d->DrawRect(drawX, yf, c_width, _r_height, this->ForeColor,
									r == this->UnderMouseRowIndex ? 1.0f : 0.5f);
								if (row.Cells.size() > static_cast<size_t>(c))
									d2d->DrawString(row.Cells[static_cast<size_t>(c)].GetText(),
										drawX + 1.0f,
										yf + text_top,
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

							d2d->FillRect(drawX, yf, c_width, _r_height, back);

							// 3D Border: raised vs sunken
							const float px = 1.0f;
							d2d->DrawRect(drawX, yf, c_width, _r_height, this->ButtonBorderDarkColor, 1.0f);
							if (c_width > 2.0f && _r_height > 2.0f)
							{
								auto innerColor = isPressed ? this->ScrollForeColor : this->ButtonBorderLightColor;
								d2d->DrawRect(drawX + px, yf + px,
									c_width - (px * 2.0f), _r_height - (px * 2.0f),
									innerColor, 1.0f);
							}

							// Text center (+ pressed offset)
							// 使用列的ButtonText作为按钮文字
							const std::wstring& buttonText = this->_columns[c].ButtonText;
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
									this->ForeColor, font);
							}
						}
						break;
						case ColumnType::ComboBox:
						{
							EnsureComboBoxCellDefaultSelection(c, r);
							const bool comboOwnsForeground =
								this->ParentForm &&
								this->ParentForm->ForegroundControl == this->_cellComboBox;
							const bool suppressCellText =
								this->_cellComboBox &&
								comboOwnsForeground &&
								(this->_cellComboBox->Expand || this->_cellComboBox->IsAnimationRunning()) &&
								this->_cellComboBoxColumnIndex == c &&
								this->_cellComboBoxRowIndex == r;
							D2D1_COLOR_F back = D2D1_COLOR_F{ 0,0,0,0 };
							D2D1_COLOR_F border = this->ForeColor;
							D2D1_COLOR_F fore = this->ForeColor;
							bool fill = false;

							if (isActiveCell || isSelectedRow)
							{
								back = this->SelectedItemBackColor;
								border = this->SelectedItemForeColor;
								fore = this->SelectedItemForeColor;
								fill = true;
							}
							else if (isHotCell)
							{
								back = this->UnderMouseItemBackColor;
								border = this->UnderMouseItemForeColor;
								fore = this->UnderMouseItemForeColor;
								fill = true;
							}

							if (fill)
								d2d->FillRect(drawX, yf, c_width, _r_height, back);
							d2d->DrawRect(drawX, yf, c_width, _r_height, border,
								r == this->UnderMouseRowIndex ? 1.0f : 0.5f);
							if (!suppressCellText && row.Cells.size() > static_cast<size_t>(c))
							{
								d2d->DrawString(row.Cells[static_cast<size_t>(c)].GetText(),
									drawX + 4.0f,
									yf + text_top,
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
							if (isSelectedRow)
							{
								d2d->DrawRect(drawX, yf, c_width, _r_height, this->SelectedItemForeColor,
									r == this->UnderMouseRowIndex ? 1.0f : 0.5f);
								if (row.Cells.size() > static_cast<size_t>(c))
								{
									if (auto* bmp = GetImageBitmap(row.Cells[static_cast<size_t>(c)].GetImage(), d2d))
										d2d->DrawBitmap(bmp,
											drawX + left,
											yf + top,
											_size, _size
										);
								}
							}
							else if (isHotCell)
							{
								d2d->FillRect(drawX, yf, c_width, _r_height, this->UnderMouseItemBackColor);
								d2d->DrawRect(drawX, yf, c_width, _r_height, this->UnderMouseItemForeColor,
									r == this->UnderMouseRowIndex ? 1.0f : 0.5f);
								if (row.Cells.size() > static_cast<size_t>(c))
								{
									if (auto* bmp = GetImageBitmap(row.Cells[static_cast<size_t>(c)].GetImage(), d2d))
										d2d->DrawBitmap(bmp,
											drawX + left,
											yf + top,
											_size, _size
										);
								}
							}
							else
							{
								d2d->DrawRect(drawX, yf, c_width, _r_height, this->ForeColor,
									r == this->UnderMouseRowIndex ? 1.0f : 0.5f);
								if (row.Cells.size() > static_cast<size_t>(c))
								{
									if (auto* bmp = GetImageBitmap(row.Cells[static_cast<size_t>(c)].GetImage(), d2d))
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
							if (_size > 24)_size = 24;
							float left = (c_width - _size) / 2.0f;
							float top = (row_height - _size) / 2.0f;
							float _rsize = _size;
							if (isSelectedRow)
							{
								d2d->DrawRect(drawX, yf, c_width, _r_height, this->SelectedItemForeColor,
									r == this->UnderMouseRowIndex ? 1.0f : 0.5f);
								if (row.Cells.size() > static_cast<size_t>(c))
								{
									d2d->DrawRect(
										drawX + left + (_rsize * 0.2f),
										yf + top + (_rsize * 0.2f),
										_rsize * 0.6f, _rsize * 0.6f,
										this->SelectedItemForeColor);
									if (row.Cells[static_cast<size_t>(c)].GetBool())
									{
										d2d->FillRect(
											drawX + left + (_rsize * 0.35f),
											yf + top + (_rsize * 0.35f),
											_rsize * 0.3f, _rsize * 0.3f,
											this->SelectedItemForeColor);
									}
								}
							}
							else if (isHotCell)
							{
								d2d->FillRect(drawX, yf, c_width, _r_height, this->UnderMouseItemBackColor);
								d2d->DrawRect(drawX, yf, c_width, _r_height, this->UnderMouseItemForeColor,
									r == this->UnderMouseRowIndex ? 1.0f : 0.5f);
								if (row.Cells.size() > static_cast<size_t>(c))
								{
									d2d->DrawRect(
										drawX + left + (_rsize * 0.2f),
										yf + top + (_rsize * 0.2f),
										_rsize * 0.6f, _rsize * 0.6f,
										this->ForeColor);
									if (row.Cells[static_cast<size_t>(c)].GetBool())
									{
										d2d->FillRect(
											drawX + left + (_rsize * 0.35f),
											yf + top + (_rsize * 0.35f),
											_rsize * 0.3f, _rsize * 0.3f,
											this->ForeColor);
									}
								}
							}
							else
							{
								d2d->DrawRect(drawX, yf, c_width, _r_height, this->ForeColor,
									r == this->UnderMouseRowIndex ? 1.0f : 0.5f);
								if (row.Cells.size() > static_cast<size_t>(c))
								{
									d2d->DrawRect(
										drawX + left + (_rsize * 0.2f),
										yf + top + (_rsize * 0.2f),
										_rsize * 0.6f, _rsize * 0.6f,
										this->ForeColor);
									if (row.Cells[static_cast<size_t>(c)].GetBool())
									{
										d2d->FillRect(
											drawX + left + (_rsize * 0.35f),
											yf + top + (_rsize * 0.35f),
											_rsize * 0.3f, _rsize * 0.3f,
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
			if (this->AllowUserToAddRows && this->_columns.size() > 0)
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
						for (int c = 0; c < static_cast<int>(this->_columns.size()); c++)
						{
							float colW = this->_columns[static_cast<size_t>(c)].Width;
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
								// 绘制新行背景
								d2d->FillRect(drawX, newRowY, c_width, newRowHeight, this->NewRowBackColor);

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

								// 绘制单元格边框
								d2d->DrawRect(drawX, newRowY, c_width, newRowHeight, this->NewRowForeColor, 1.0f);
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
					d2d->DrawRect(0, 0, actualWidth, actualHeight, this->BolderColor, 4);
				}
				else
				{
					d2d->DrawRect(0, 0, actualWidth, actualHeight, this->BolderColor, 2);
				}
			}
			d2d->PopDrawRect();
			this->DrawScroll();
		}
		d2d->DrawRect(0, 0, actualWidth, actualHeight, this->BolderColor, this->Boder);
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
	if (this->_columns.size() <= 0) return false;

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
	const float totalRowsHeight = rowHeight * (float)this->_rows.size();
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
	if (this->_columns.size() <= 0) return -1;

	auto l = this->CalcScrollLayout();
	const float headHeight = l.HeadHeight;
	const float renderWidth = l.RenderWidth;

	if (x < 0 || x >= (int)renderWidth) return -1;
	if (y <= (int)headHeight) return -1;

	const float rowHeight = this->GetRowHeightPx();
	const float totalRowsHeight = rowHeight * (float)this->_rows.size();
	const float virtualY = ((float)y - headHeight) + this->ScrollYOffset;

	// 检查是否在新行区域内
	if (virtualY < totalRowsHeight || virtualY >= totalRowsHeight + rowHeight)
		return -1;

	// 确定鼠标在哪一列
	const float virtualX = (float)x + this->ScrollXOffset;
	float acc = 0.0f;
	for (int i = 0; i < static_cast<int>(this->_columns.size()); i++)
	{
		if (virtualX >= acc && virtualX < acc + this->_columns[static_cast<size_t>(i)].Width)
		{
			outColumnIndex = i;
			return static_cast<int>(this->_rows.size());  // 返回Rows.size()作为新行的索引
		}
		acc += this->_columns[static_cast<size_t>(i)].Width;
	}

	return -1;
}

void GridView::AddNewRow()
{
	if (!this->AllowUserToAddRows) return;
	bool cancel = false;
	this->OnUserAddingRow(this, cancel);
	if (cancel) return;

	// 创建新行
	GridViewRow newRow;
	for (int i = 0; i < static_cast<int>(this->_columns.size()); i++)
	{
		CellValue cell;
		newRow.Cells.push_back(cell);
	}

	// 添加到Rows列表
	int newRowIndex = AddRow(newRow);

	// 触发新行添加事件
	this->OnUserAddedRow(this, newRowIndex);

	// 自动选中新行的第一列并开始编辑
	if (this->_columns.size() > 0)
	{
		this->SelectedColumnIndex = 0;
		this->SelectedRowIndex = newRowIndex;
		EditClearHistory();
		this->SelectionChanged(this);
		StartEditingCell(0, newRowIndex);
	}

	this->PostRender();
}

void GridView::ReSizeRows(int count)
{
	if (count < 0) count = 0;
	CancelEditing(false);
	CloseComboBoxEditor();
	this->_rows.resize((size_t)count);
	NormalizeRows();
	AdjustScrollPosition();
	ClearImageCache();
	this->PostRender();
}

void GridView::NormalizeRows()
{
	const size_t columnCount = this->_columns.size();
	for (auto& row : this->_rows)
	{
		if (row.Cells.size() < columnCount)
			row.Cells.resize(columnCount);
		else if (row.Cells.size() > columnCount)
			row.Cells.resize(columnCount);
	}
}

void GridView::ClearImageCache()
{
	this->_imageCacheIndex.clear();
	this->_imageCacheLru.clear();
}

ID2D1Bitmap* GridView::GetImageBitmap(const std::shared_ptr<BitmapSource>& image, D2DGraphics* render)
{
	if (!render || !image)
		return nullptr;
	auto* target = render->GetRenderTargetRaw();
	if (!target)
		return nullptr;

	ImageCacheKey key{ image.get(), target };
	auto found = this->_imageCacheIndex.find(key);
	if (found != this->_imageCacheIndex.end())
	{
		this->_imageCacheLru.splice(this->_imageCacheLru.begin(), this->_imageCacheLru, found->second);
		return found->second->Bitmap.Get();
	}

	auto* bmp = render->CreateBitmap(image);
	if (!bmp)
		return nullptr;

	ImageCacheEntry entry;
	entry.Key = key;
	entry.Bitmap.Attach(bmp);
	this->_imageCacheLru.push_front(std::move(entry));
	this->_imageCacheIndex[this->_imageCacheLru.front().Key] = this->_imageCacheLru.begin();

	while (this->_imageCacheLru.size() > this->_imageCacheLimit)
	{
		auto last = std::prev(this->_imageCacheLru.end());
		this->_imageCacheIndex.erase(last->Key);
		this->_imageCacheLru.pop_back();
	}

	return this->_imageCacheLru.front().Bitmap.Get();
}

void GridView::SetImageCacheLimit(size_t limit)
{
	if (limit == 0)
		limit = 1;
	this->_imageCacheLimit = limit;
	while (this->_imageCacheLru.size() > this->_imageCacheLimit)
	{
		auto last = std::prev(this->_imageCacheLru.end());
		this->_imageCacheIndex.erase(last->Key);
		this->_imageCacheLru.pop_back();
	}
}

size_t GridView::GetImageCacheLimit() const
{
	return this->_imageCacheLimit;
}

void GridView::AutoSizeColumn(int col)
{
	if (col >= 0 && col < static_cast<int>(this->_columns.size()))
	{
		auto font = this->Font;
		float font_height = font->FontHeight;
		float row_height = font_height + 2.0f;
		if (RowHeight != 0.0f)
		{
			row_height = RowHeight;
		}
		auto& column = this->_columns[static_cast<size_t>(col)];
		column.Width = 10.0f;
		for (int i = 0; i < static_cast<int>(this->_rows.size()); i++)
		{
			auto& r = this->_rows[static_cast<size_t>(i)];
			if (r.Cells.size() > static_cast<size_t>(col))
			{
				if (this->_columns[static_cast<size_t>(col)].Type == ColumnType::Text ||
					this->_columns[static_cast<size_t>(col)].Type == ColumnType::Button ||
					this->_columns[static_cast<size_t>(col)].Type == ColumnType::ComboBox)
				{
					// Button列使用列的ButtonText来计算宽度
					std::wstring textToMeasure;
					if (this->_columns[static_cast<size_t>(col)].Type == ColumnType::Button && !this->_columns[static_cast<size_t>(col)].ButtonText.empty())
					{
						textToMeasure = this->_columns[static_cast<size_t>(col)].ButtonText;
					}
					else
					{
						textToMeasure = r.Cells[static_cast<size_t>(col)].GetText();
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
	auto& cell = this->_rows[row].Cells[col];
	cell.SetBool(!cell.GetBool());
	this->OnGridViewCheckStateChanged(this, col, row, cell.GetBool());
}

void GridView::EnsureComboBoxCellDefaultSelection(int col, int row)
{
	if (col < 0 || row < 0) return;
	if (col >= static_cast<int>(this->_columns.size()) || row >= static_cast<int>(this->_rows.size())) return;
	if (this->_columns[static_cast<size_t>(col)].Type != ColumnType::ComboBox) return;

	auto& column = this->_columns[static_cast<size_t>(col)];
	if (column.ComboBoxItems.size() <= 0) return;
	auto& rowObj = this->_rows[static_cast<size_t>(row)];
	if (rowObj.Cells.size() <= static_cast<size_t>(col))
		rowObj.Cells.resize((size_t)col + 1);
	auto& cell = rowObj.Cells[static_cast<size_t>(col)];

	const __int64 idx = cell.GetTag();
	if (idx < 0 || idx >= static_cast<__int64>(column.ComboBoxItems.size()))
	{
		cell.SetComboSelection(0, column.ComboBoxItems[0]);
	}
	else
	{
		// Keep Text in sync with index if needed
		const auto& t = column.ComboBoxItems[static_cast<size_t>(idx)];
		if (cell.GetText() != t)
			cell.SetComboSelection(static_cast<int>(idx), t);
	}
}

void GridView::CloseComboBoxEditor()
{
	if (!this->_cellComboBox) return;

	if (this->ParentForm && this->ParentForm->ForegroundControl == this->_cellComboBox)
		this->ParentForm->ForegroundControl = NULL;

	this->_cellComboBox->SetExpanded(false);
	this->_cellComboBoxColumnIndex = -1;
	this->_cellComboBoxRowIndex = -1;
}

void GridView::ToggleComboBoxEditor(int col, int row)
{
	if (col < 0 || row < 0) return;
	if (col >= static_cast<int>(this->_columns.size()) || row >= static_cast<int>(this->_rows.size())) return;
	if (!this->ParentForm) return;
	if (this->_columns[static_cast<size_t>(col)].Type != ColumnType::ComboBox) return;

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
		EditClearHistory();
	}

	this->SelectedColumnIndex = col;
	this->SelectedRowIndex = row;
	EditClearHistory();
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

	auto& column = this->_columns[static_cast<size_t>(col)];
	auto& rowObj = this->_rows[static_cast<size_t>(row)];
	if (rowObj.Cells.size() <= static_cast<size_t>(col))
		rowObj.Cells.resize((size_t)col + 1);
	auto& cell = rowObj.Cells[static_cast<size_t>(col)];

	this->_cellComboBox->ParentForm = this->ParentForm;
	this->_cellComboBox->SetFontEx(this->Font, false);
	this->_cellComboBox->SetRuntimeLocation(POINT{ x, y });
	this->_cellComboBox->Size = SIZE{ (w > 0 ? w : 1), (h > 0 ? h : 1) };
	this->_cellComboBox->Items = column.ComboBoxItems;
	this->_cellComboBox->SelectedIndex = cell.GetComboIndex();
	if (this->_cellComboBox->SelectedIndex < 0) this->_cellComboBox->SelectedIndex = 0;
	if (this->_cellComboBox->SelectedIndex >= static_cast<int>(this->_cellComboBox->Items.size()))
		this->_cellComboBox->SelectedIndex = (this->_cellComboBox->Items.size() > 0) ? (static_cast<int>(this->_cellComboBox->Items.size()) - 1) : 0;
	this->_cellComboBox->Text = (this->_cellComboBox->Items.size() > 0) ? this->_cellComboBox->Items[static_cast<size_t>(this->_cellComboBox->SelectedIndex)] : L"";

	int expandCount = 4;
	if (this->_cellComboBox->Items.size() > 0)
		expandCount = std::min(4, (int)this->_cellComboBox->Items.size());
	if (expandCount < 1) expandCount = 1;
	this->_cellComboBox->ExpandCount = expandCount;

	this->_cellComboBox->OnSelectionChanged.Clear();
	this->_cellComboBox->OnSelectionChanged += [this, col, row](Control* sender)
		{
			(void)sender;
			if (col < 0 || row < 0) return;
			if (col >= static_cast<int>(this->_columns.size()) || row >= static_cast<int>(this->_rows.size())) return;
			if (this->_columns[static_cast<size_t>(col)].Type != ColumnType::ComboBox) return;
			auto& column2 = this->_columns[static_cast<size_t>(col)];
			if (!this->_cellComboBox) return;
			if (column2.ComboBoxItems.size() <= 0) return;
			int idx = this->_cellComboBox->SelectedIndex;
			if (idx < 0) idx = 0;
			if (idx >= static_cast<int>(column2.ComboBoxItems.size())) idx = static_cast<int>(column2.ComboBoxItems.size()) - 1;
			auto& cell2 = this->_rows[static_cast<size_t>(row)].Cells[static_cast<size_t>(col)];
			cell2.SetComboSelection(idx, column2.ComboBoxItems[static_cast<size_t>(idx)]);
			this->OnGridViewComboBoxSelectionChanged(this, col, row, idx, cell2.GetText());
			this->PostRender();
		};

	this->_cellComboBoxColumnIndex = col;
	this->_cellComboBoxRowIndex = row;
	this->_cellComboBox->SetExpanded(true);
	this->ParentForm->Invalidate(true);
	this->PostRender();
}
void GridView::StartEditingCell(int col, int row)
{
	if (col < 0 || row < 0) return;
	if (col >= static_cast<int>(this->_columns.size()) || row >= static_cast<int>(this->_rows.size())) return;

	if (this->Editing && (this->EditingColumnIndex != col || this->EditingRowIndex != row))
	{
		SaveCurrentEditingCell(true);
		EditClearHistory();
	}

	this->SelectedColumnIndex = col;
	this->SelectedRowIndex = row;
	EditClearHistory();
	this->SelectionChanged(this);

	if (IsEditableTextCell(col, row))
	{
		this->Editing = true;
		this->EditingColumnIndex = col;
		this->EditingRowIndex = row;
		this->EditingText = this->_rows[static_cast<size_t>(row)].Cells[static_cast<size_t>(col)].GetText();
		this->EditingOriginalText = this->EditingText;
		this->EditSelectionStart = 0;
		this->EditSelectionEnd = (int)this->EditingText.size();
		this->EditOffsetX = 0.0f;
		EditClearHistory();
		this->ParentForm->Selected = this;
		EditSetImeCompositionWindow();
	}
	else
	{
		this->Editing = false;
		this->EditingColumnIndex = -1;
		this->EditingRowIndex = -1;
		EditClearHistory();
	}
}
void GridView::CancelEditing(bool revert)
{
	if (this->Editing)
	{
		if (revert && this->EditingRowIndex >= 0 && this->EditingColumnIndex >= 0 &&
			this->EditingRowIndex < static_cast<int>(this->_rows.size()) && this->EditingColumnIndex < static_cast<int>(this->_columns.size()))
		{
			this->_rows[static_cast<size_t>(this->EditingRowIndex)].Cells[static_cast<size_t>(this->EditingColumnIndex)].SetText(this->EditingOriginalText);
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
	EditClearHistory();
	this->ParentForm->Selected = this;
	this->SelectedColumnIndex = -1;
	this->SelectedRowIndex = -1;
}
void GridView::SaveCurrentEditingCell(bool commit)
{
	if (!this->Editing) return;
	if (!commit) return;
	if (this->EditingColumnIndex < 0 || this->EditingRowIndex < 0) return;
	if (this->EditingRowIndex >= static_cast<int>(this->_rows.size())) return;
	if (this->EditingColumnIndex >= static_cast<int>(this->_columns.size())) return;
	this->_rows[static_cast<size_t>(this->EditingRowIndex)].Cells[static_cast<size_t>(this->EditingColumnIndex)].SetText(this->EditingText);
}
void GridView::AdjustScrollPosition()
{
	auto l = this->CalcScrollLayout();
	const float rowH = this->GetRowHeightPx();
	const float contentH = l.ContentHeight;
	const float maxScrollY = l.MaxScrollY;

	if (this->SelectedRowIndex < 0 || this->SelectedRowIndex >= static_cast<int>(this->_rows.size())) return;
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
		if (this->_resizeColumnIndex >= 0 && this->_resizeColumnIndex < static_cast<int>(this->_columns.size()))
		{
			if (this->_columns[static_cast<size_t>(this->_resizeColumnIndex)].Width != newWidth)
			{
				this->_columns[static_cast<size_t>(this->_resizeColumnIndex)].Width = newWidth;
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
		if (l.TotalRowsHeight > 0.0f && l.MaxScrollY > 0.0f && l.RenderHeight > 0.0f && l.ContentHeight > 0.0f)
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
			this->_resizeStartWidth = this->_columns[divCol].Width;
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
			undermouseIndex.y < static_cast<LONG>(this->_rows.size()) && undermouseIndex.x < static_cast<LONG>(this->_columns.size()))
		{
			// Keep hover index in sync even if we didn't get a prior WM_MOUSEMOVE.
			this->UnderMouseColumnIndex = undermouseIndex.x;
			this->UnderMouseRowIndex = undermouseIndex.y;

			if (this->_columns[static_cast<size_t>(undermouseIndex.x)].Type == ColumnType::Button)
			{
				if (this->Editing)
					SaveCurrentEditingCell(true);
				CloseComboBoxEditor();

				this->SelectedColumnIndex = undermouseIndex.x;
				this->SelectedRowIndex = undermouseIndex.y;
				EditClearHistory();
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
				BeginEditSelectionFromMouse(xof, yof);
				SetCapture(this->ParentForm->Handle);
			}
			else
			{
				HandleCellClick(undermouseIndex.x, undermouseIndex.y);
				if (this->Editing && undermouseIndex.x == this->EditingColumnIndex && undermouseIndex.y == this->EditingRowIndex)
				{
					BeginEditSelectionFromMouse(xof, yof);
					SetCapture(this->ParentForm->Handle);
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
			if (hitResult >= 0 && newRowCol >= 0 && newRowCol < static_cast<int>(this->_columns.size()))
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
			undermouseIndex.x < static_cast<LONG>(this->_columns.size()) && undermouseIndex.y < static_cast<LONG>(this->_rows.size()));
		const bool isButtonCell = validCell && (this->_columns[static_cast<size_t>(undermouseIndex.x)].Type == ColumnType::Button);

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
			EditClearHistory();
			if (this->SelectedRowIndex < static_cast<int>(this->_rows.size()) - 1)
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
				EditClearHistory();
			}
			this->PostRender();
			return;
		}

		const bool ctrlDown = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
		if (ctrlDown && wParam == 'Z')
		{
			if (EditUndo())
				this->PostRender();
			return;
		}
		if (ctrlDown && wParam == 'Y')
		{
			if (EditRedo())
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

	const int oldCol = this->SelectedColumnIndex;
	const int oldRow = this->SelectedRowIndex;
	switch (wParam)
	{
	case VK_RIGHT:
		if (SelectedColumnIndex < static_cast<int>(this->_columns.size()) - 1) SelectedColumnIndex++;
		break;
	case VK_LEFT:
		if (SelectedColumnIndex > 0) SelectedColumnIndex--;
		break;
	case VK_DOWN:
		if (SelectedRowIndex < static_cast<int>(this->_rows.size()) - 1) SelectedRowIndex++;
		break;
	case VK_UP:
		if (SelectedRowIndex > 0) SelectedRowIndex--;
		break;
	default:
		break;
	}

	AdjustScrollPosition();
	if (oldCol != this->SelectedColumnIndex || oldRow != this->SelectedRowIndex)
	{
		EditClearHistory();
		this->SelectionChanged(this);
	}
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
	if (this->_columns[col].Type == ColumnType::Check)
	{
		ToggleCheckState(col, row);
	}
	else if (this->_columns[col].Type == ColumnType::Button)
	{
		// Button click is handled on mouse-up (WinForms-like)
		if (this->Editing)
			SaveCurrentEditingCell(true);
		this->SelectedColumnIndex = col;
		this->SelectedRowIndex = row;
		EditClearHistory();
		this->SelectionChanged(this);
	}
	else if (this->_columns[col].Type == ColumnType::ComboBox)
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
	if (col >= static_cast<int>(this->_columns.size()) || row >= static_cast<int>(this->_rows.size())) return false;

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
	for (int i = 0; i < col; i++) left += this->_columns[static_cast<size_t>(i)].Width;
	float width = this->_columns[static_cast<size_t>(col)].Width;
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
	if (col >= static_cast<int>(this->_columns.size()) || row >= static_cast<int>(this->_rows.size())) return false;
	return this->_columns[static_cast<size_t>(col)].Type == ColumnType::Text && this->_columns[static_cast<size_t>(col)].CanEdit;
}
void GridView::EditEnsureSelectionInRange()
{
	if (this->EditSelectionStart < 0) this->EditSelectionStart = 0;
	if (this->EditSelectionEnd < 0) this->EditSelectionEnd = 0;
	int maxLen = (int)this->EditingText.size();
	if (this->EditSelectionStart > maxLen) this->EditSelectionStart = maxLen;
	if (this->EditSelectionEnd > maxLen) this->EditSelectionEnd = maxLen;
}

void GridView::EditPushUndoState()
{
	if (!this->Editing) return;
	EditEnsureSelectionInRange();
	_editUndoStack.push_back(EditHistoryState{ this->EditingText, this->EditSelectionStart, this->EditSelectionEnd });
	if (_editUndoStack.size() > _editHistoryLimit)
		_editUndoStack.erase(_editUndoStack.begin());
	_editRedoStack.clear();
}

void GridView::EditClearHistory()
{
	_editUndoStack.clear();
	_editRedoStack.clear();
}

void GridView::EditRestoreState(const EditHistoryState& state)
{
	this->EditingText = state.Text;
	this->EditSelectionStart = state.SelectionStart;
	this->EditSelectionEnd = state.SelectionEnd;
	EditEnsureSelectionInRange();
	D2D1_RECT_F rect{};
	if (TryGetCellRectLocal(this->EditingColumnIndex, this->EditingRowIndex, rect))
		EditUpdateScroll(rect.right - rect.left);
	EditSyncCellText();
}

void GridView::EditSyncCellText()
{
	if (this->EditingRowIndex >= 0 && this->EditingColumnIndex >= 0 &&
		this->EditingRowIndex < static_cast<int>(this->_rows.size()) && this->EditingColumnIndex < static_cast<int>(this->_columns.size()))
	{
		this->_rows[static_cast<size_t>(this->EditingRowIndex)].Cells[static_cast<size_t>(this->EditingColumnIndex)].SetText(this->EditingText);
	}
}

bool GridView::EditUndo()
{
	if (!this->Editing || _editUndoStack.empty()) return false;
	EditEnsureSelectionInRange();
	_editRedoStack.push_back(EditHistoryState{ this->EditingText, this->EditSelectionStart, this->EditSelectionEnd });
	auto state = _editUndoStack.back();
	_editUndoStack.pop_back();
	EditRestoreState(state);
	return true;
}

bool GridView::EditRedo()
{
	if (!this->Editing || _editRedoStack.empty()) return false;
	EditEnsureSelectionInRange();
	_editUndoStack.push_back(EditHistoryState{ this->EditingText, this->EditSelectionStart, this->EditSelectionEnd });
	if (_editUndoStack.size() > _editHistoryLimit)
		_editUndoStack.erase(_editUndoStack.begin());
	auto state = _editRedoStack.back();
	_editRedoStack.pop_back();
	EditRestoreState(state);
	return true;
}

void GridView::EditInputText(const std::wstring& input)
{
	if (!this->Editing) return;
	if (input.empty()) return;

	EditEnsureSelectionInRange();
	int sels = (this->EditSelectionStart <= this->EditSelectionEnd) ? this->EditSelectionStart : this->EditSelectionEnd;
	int sele = (this->EditSelectionEnd >= this->EditSelectionStart) ? this->EditSelectionEnd : this->EditSelectionStart;
	int selLen = sele - sels;

	EditPushUndoState();

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

	EditSyncCellText();
}
void GridView::EditInputBack()
{
	if (!this->Editing) return;
	EditEnsureSelectionInRange();
	int sels = (this->EditSelectionStart <= this->EditSelectionEnd) ? this->EditSelectionStart : this->EditSelectionEnd;
	int sele = (this->EditSelectionEnd >= this->EditSelectionStart) ? this->EditSelectionEnd : this->EditSelectionStart;
	int selLen = sele - sels;

	if (selLen <= 0 && sels <= 0)
		return;

	EditPushUndoState();

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

	EditSyncCellText();
}
void GridView::EditInputDelete()
{
	if (!this->Editing) return;
	EditEnsureSelectionInRange();
	int sels = (this->EditSelectionStart <= this->EditSelectionEnd) ? this->EditSelectionStart : this->EditSelectionEnd;
	int sele = (this->EditSelectionEnd >= this->EditSelectionStart) ? this->EditSelectionEnd : this->EditSelectionStart;
	int selLen = sele - sels;

	if (selLen <= 0 && sels >= (int)this->EditingText.size())
		return;

	EditPushUndoState();

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

	EditSyncCellText();
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
bool GridView::BeginEditSelectionFromMouse(int xof, int yof)
{
	if (!this->Editing || this->EditingColumnIndex < 0 || this->EditingRowIndex < 0)
		return false;

	D2D1_RECT_F rect{};
	if (!TryGetCellRectLocal(this->EditingColumnIndex, this->EditingRowIndex, rect))
		return false;

	float cellWidth = rect.right - rect.left;
	float cellHeight = rect.bottom - rect.top;
	float lx = (float)xof - rect.left;
	float ly = (float)yof - rect.top;
	int pos = EditHitTestTextPosition(cellWidth, cellHeight, lx, ly);
	this->EditSelectionStart = this->EditSelectionEnd = pos;
	EditUpdateScroll(cellWidth);
	return true;
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
