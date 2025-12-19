#pragma once
#include "GridView.h"
#include "Form.h"
#include <CppUtils/Graphics/Factory.h>
#pragma comment(lib, "Imm32.lib")

CellValue::CellValue() : Text(L"")
{}
CellValue::CellValue(std::wstring s) : Text(s), Tag(NULL), Image(NULL)
{}
CellValue::CellValue(wchar_t* s) :Text(s), Tag(NULL), Image(NULL)
{}
CellValue::CellValue(const wchar_t* s) : Text(s), Tag(NULL), Image(NULL)
{}
CellValue::CellValue(ID2D1Bitmap* img) : Text(L""), Tag(NULL), Image(img)
{}
CellValue::CellValue(__int64 tag) : Text(L""), Tag(tag), Image(NULL)
{}
CellValue::CellValue(bool tag) : Text(L""), Tag(tag), Image(NULL)
{}
CellValue::CellValue(__int32 tag) : Text(L""), Tag(tag), Image(NULL)
{}
CellValue::CellValue(unsigned __int32 tag) : Text(L""), Tag(tag), Image(NULL)
{}
CellValue::CellValue(unsigned __int64 tag) : Text(L""), Tag(tag), Image(NULL)
{}
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
	this->ScrollRowPosition = 0;
}
#pragma region _GridView_
POINT GridView::GetGridViewUnderMouseItem(int x, int y, GridView* ct)
{
	float _render_width = ct->Width - 8;
	float _render_height = ct->Height;
	if (x > _render_width || y > _render_height)return { -1,-1 };
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
	unsigned int s_y = ct->ScrollRowPosition;
	float yf = ct->HeadHeight == 0.0f ? row_height : ct->HeadHeight;
	float xf = 0.0f;
	int xindex = -1;
	int yindex = -1;
	for (; s_x < ct->Columns.Count; s_x++)
	{
		float c_width = ct->Columns[s_x].Width;
		if (c_width + xf > _render_width)
		{
			c_width = _render_width - xf;
		}
		if (xf<x && xf + c_width>x)
		{
			xindex = s_x;
			break;
		}
		xf += ct->Columns[s_x].Width;
		if (xf > _render_width)
		{
			break;
		}
	}
	if (((y - head_height) / row_height) + s_y < ct->Rows.Count)
	{
		yindex = ((y - head_height) / row_height) + s_y;
	}
	return { xindex,yindex };
}
D2D1_RECT_F GridView::GetGridViewScrollBlockRect(GridView* ct)
{
	auto absloc = ct->AbsLocation;
	auto size = ct->Size;
	float _render_width = ct->Width - 8;
	float _render_height = ct->Height;
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
	float render_items_height = _render_height - head_height;
	int render_items_count = render_items_height / row_height;
	if (render_items_count < ct->Rows.Count)
	{
		float scroll_block_height = (float)ct->Height * (float)render_items_count / (float)ct->Rows.Count;
		float scroll_block_top = ((float)ct->ScrollRowPosition / ((float)ct->Rows.Count)) * (float)ct->Height;
		return { absloc.x + (ct->Width - 8.0f), absloc.y + scroll_block_top, 8.0f, scroll_block_height };
	}
	return { 0,0,0,0 };
}
int GridView::GetGridViewRenderRowCount(GridView* ct)
{
	float _render_height = ct->Height;
	float font_height = (ct->Font)->FontHeight;
	float row_height = font_height + 2.0f;
	if (RowHeight != 0.0f)
	{
		row_height = RowHeight;
	}
	auto font = ct->Font;
	auto head_font = HeadFont ? HeadFont : font;
	float head_font_height = head_font->FontHeight;
	float head_height = ct->HeadHeight == 0.0f ? head_font_height : ct->HeadHeight;
	_render_height -= head_height;
	return (int)(_render_height / row_height);
}
void GridView::DrawScroll()
{
	auto d2d = this->ParentForm->Render;
	auto abslocation = this->AbsLocation;
	auto font = this->Font;
	auto size = this->ActualSize();
	if (this->Rows.Count > 0)
	{
		float _render_width = this->Width - 8;
		float _render_height = this->Height;
		float font_height = font->FontHeight;
		float row_height = font_height + 2.0f;
		if (RowHeight != 0.0f)
		{
			row_height = RowHeight;
		}
		auto head_font = HeadFont ? HeadFont : font;
		float head_font_height = head_font->FontHeight;
		float head_height = this->HeadHeight == 0.0f ? head_font_height : this->HeadHeight;
		float render_items_height = _render_height - head_height;
		int render_items_count = render_items_height / row_height;
		if (render_items_count < this->Rows.Count)
		{
			int render_count = GetGridViewRenderRowCount(this);
			int max_scroll = this->Rows.Count - render_count;
			float scroll_block_height = ((float)render_count / (float)this->Rows.Count) * (float)this->Height;
			if (scroll_block_height < this->Height * 0.1)scroll_block_height = this->Height * 0.1;
			float scroll_block_move_space = this->Height - scroll_block_height;
			float yt = scroll_block_height * 0.5f;
			float yb = this->Height - (scroll_block_height * 0.5f);
			float per = (float)this->ScrollRowPosition / (float)max_scroll;
			float scroll_tmp_y = per * scroll_block_move_space;
			float scroll_block_top = scroll_tmp_y;
			d2d->FillRoundRect(abslocation.x + (this->Width - 8.0f), abslocation.y, 8.0f, this->Height, this->ScrollBackColor, 4.0f);
			d2d->FillRoundRect(abslocation.x + (this->Width - 8.0f), abslocation.y + scroll_block_top, 8.0f, scroll_block_height, this->ScrollForeColor, 4.0f);
		}
	}
}
void GridView::SetScrollByPos(float yof)
{
	const auto d2d = this->ParentForm->Render;
	const auto absLocation = this->AbsLocation;
	const auto font = this->Font;
	const auto size = this->ActualSize();

	const int rowCount = this->Rows.Count;
	if (rowCount == 0) return;

	const int renderCount = GetGridViewRenderRowCount(this);
	const int maxScroll = rowCount - renderCount;

	
	const float renderingWidth = this->Width - 8.0f;
	const float renderingHeight = this->Height;

	
	float rowHeight = font->FontHeight + 2.0f;
	if (RowHeight != 0.0f)
		rowHeight = RowHeight;

	
	const auto headFont = HeadFont ? HeadFont : font;
	const float headHeight = (this->HeadHeight == 0.0f) ? headFont->FontHeight : this->HeadHeight;
	const float contentHeight = renderingHeight - headHeight;

	
	const int visibleRowsCount = static_cast<int>(contentHeight / rowHeight);

	if (visibleRowsCount < rowCount)
	{
		
		const float scrollBlockHeight = std::max(static_cast<float>(renderingHeight * 0.1f),
			(renderingHeight * renderCount) / static_cast<float>(rowCount));

		const float topPosition = scrollBlockHeight * 0.5f;
		const float bottomPosition = renderingHeight - topPosition;

		
		if (bottomPosition > topPosition)
		{
			const float percent = (yof - topPosition) / (bottomPosition - topPosition);
			this->ScrollRowPosition = std::clamp(maxScroll * percent, 0.0f, static_cast<float>(maxScroll));
		}
	}

	
	this->ScrollRowPosition = std::max(std::min(static_cast<float>(this->ScrollRowPosition), static_cast<float>(rowCount -
		renderCount)), 0.0f);

	this->ScrollChanged(this);
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
			float _render_width = this->Width - 8;
			float _render_height = this->Height;
			float font_height = font->FontHeight;
			float head_font_height = head_font->FontHeight;
			float row_height = font_height + 2.0f;
			if (RowHeight != 0.0f)
			{
				row_height = RowHeight;
			}
			float text_top = (row_height - font_height) * 0.5f;
			if (text_top < 0) text_top = 0;
			// Clamp scroll position，避免 Rows.Count==0 时出现 -1/unsigned wrap
			if (this->Rows.Count <= 0)
			{
				this->ScrollRowPosition = 0;
			}
			else
			{
				if (ScrollRowPosition < 0) ScrollRowPosition = 0;
				if (ScrollRowPosition >= this->Rows.Count) ScrollRowPosition = this->Rows.Count - 1;
			}
			int s_x = 0;
			int s_y = this->ScrollRowPosition;
			float head_height = this->HeadHeight == 0.0f ? head_font_height : this->HeadHeight;
			float yf = head_height;
			float xf = 0.0f;
			int i = s_x;
			for (; i < this->Columns.Count; i++)
			{
				float c_width = this->Columns[i].Width;
				if (c_width + xf > _render_width)
				{
					c_width = _render_width - xf;
				}
				auto ht = head_font->GetTextSize(this->Columns[i].Name);
				float draw_x_offset = (c_width - ht.width) / 2.0f;
				if (draw_x_offset < 0)draw_x_offset = 0;
				float draw_y_offset = (head_height - head_font_height) / 2.0f;
				if (draw_y_offset < 0)draw_y_offset = 0;
				d2d->PushDrawRect(abslocation.x + xf, abslocation.y, c_width, head_height);
				{
					d2d->FillRect(abslocation.x + xf, abslocation.y, c_width, head_height, this->HeadBackColor);
					d2d->DrawRect(abslocation.x + xf, abslocation.y, c_width, head_height, this->HeadForeColor, 2.f);
					d2d->DrawString(this->Columns[i].Name,
						abslocation.x + xf + draw_x_offset,
						abslocation.y + draw_y_offset,
						this->HeadForeColor, head_font);
				}
				d2d->PopDrawRect();
				xf += this->Columns[i].Width;
				if (xf > _render_width)
				{
					break;
				}
			}
			xf = 0;
			i = 0;
			for (int r = s_y; r < this->Rows.Count && i < (int)(_render_height / row_height); r++, i++)
			{
				GridViewRow& row = this->Rows[r];
				float xf = 0.0f;
				for (int c = s_x; c < this->Columns.Count; c++)
				{
					{
						float c_width = this->Columns[c].Width;
						if (c_width + xf > _render_width)
						{
							c_width = _render_width - xf;
						}
						float _r_height = row_height;
						d2d->PushDrawRect(abslocation.x + xf, abslocation.y + yf, c_width, _r_height);
						{
							switch (this->Columns[c].Type)
							{
							case ColumnType::Text:
							{
								if (c == this->SelectedColumnIndex && r == this->SelectedRowIndex)
								{
									// 编辑状态：在单元格内绘制编辑器（选区/光标/文本）
									if (this->Editing && this->EditingColumnIndex == c && this->EditingRowIndex == r && this->ParentForm->Selected == this)
									{
										D2D1_RECT_F cellLocal{};
										if (!TryGetCellRectLocal(c, r, cellLocal))
										{
											// 当前编辑单元格已不可见：提交并退出编辑，避免“看不见但仍在输入”
											SaveCurrentEditingCell(true);
											this->Editing = false;
										}
										else
										{
											float renderHeight = _r_height - (this->EditTextMargin * 2.0f);
											if (renderHeight < 0.0f) renderHeight = 0.0f;

											EditEnsureSelectionInRange();
											EditUpdateScroll(c_width);

											auto textSize = font->GetTextSize(this->EditingText, FLT_MAX, renderHeight);
											float offsetY = (_r_height - textSize.height) * 0.5f;
											if (offsetY < 0.0f) offsetY = 0.0f;

											d2d->FillRect(abslocation.x + xf, abslocation.y + yf, c_width, _r_height, this->EditBackColor);
											d2d->DrawRect(abslocation.x + xf, abslocation.y + yf, c_width, _r_height, this->SelectedItemForeColor,
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
														sr.left + abslocation.x + xf + this->EditTextMargin - this->EditOffsetX,
														(sr.top + abslocation.y + yf) + offsetY,
														sr.width, sr.height,
														this->EditSelectedBackColor);
												}
											}
											else
											{
												if (!selRange.empty() && (GetTickCount64() / 200) % 2 == 0)
												{
													d2d->DrawLine(
														{ selRange[0].left + abslocation.x + xf + this->EditTextMargin - this->EditOffsetX,(selRange[0].top + abslocation.y + yf) - offsetY },
														{ selRange[0].left + abslocation.x + xf + this->EditTextMargin - this->EditOffsetX,(selRange[0].top + abslocation.y + yf + selRange[0].height) + offsetY },
														Colors::Black);
												}
											}

											auto lot = Factory::CreateStringLayout(this->EditingText, FLT_MAX, renderHeight, font->FontObject);
											if (selLen != 0)
											{
												d2d->DrawStringLayoutEffect(lot,
													(float)abslocation.x + xf + this->EditTextMargin - this->EditOffsetX, ((float)abslocation.y + yf) + offsetY,
													this->EditForeColor,
													DWRITE_TEXT_RANGE{ (UINT32)sels, (UINT32)selLen },
													this->EditSelectedForeColor,
													font);
											}
											else
											{
												d2d->DrawStringLayout(lot,
													(float)abslocation.x + xf + this->EditTextMargin - this->EditOffsetX, ((float)abslocation.y + yf) + offsetY,
													this->EditForeColor);
											}
											lot->Release();
										}
									}
									else
									{
										d2d->FillRect(abslocation.x + xf, abslocation.y + yf, c_width, _r_height, this->SelectedItemBackColor);
										d2d->DrawRect(abslocation.x + xf, abslocation.y + yf, c_width, _r_height, this->SelectedItemForeColor,
											r == this->UnderMouseRowIndex ? 1.0f : 0.5f);
										if (row.Cells.Count > c)
											d2d->DrawString(row.Cells[c].Text,
												abslocation.x + xf + 1.0f,
												abslocation.y + yf + text_top,
												this->SelectedItemForeColor, font);
									}
								}
								else if (c == this->UnderMouseColumnIndex && r == this->UnderMouseRowIndex)
								{
									d2d->FillRect(abslocation.x + xf, abslocation.y + yf, c_width, _r_height, this->UnderMouseItemBackColor);
									d2d->DrawRect(abslocation.x + xf, abslocation.y + yf, c_width, _r_height, this->UnderMouseItemForeColor,
										r == this->UnderMouseRowIndex ? 1.0f : 0.5f);
									if (row.Cells.Count > c)
										d2d->DrawString(row.Cells[c].Text,
											abslocation.x + xf + 1.0f,
											abslocation.y + yf + text_top,
											this->UnderMouseItemForeColor, font);
								}
								else
								{
									d2d->DrawRect(abslocation.x + xf, abslocation.y + yf, c_width, _r_height, this->ForeColor,
										r == this->UnderMouseRowIndex ? 1.0f : 0.5f);
									if (row.Cells.Count > c)
										d2d->DrawString(row.Cells[c].Text,
											abslocation.x + xf + 1.0f,
											abslocation.y + yf + text_top,
											this->ForeColor, font);
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
									d2d->FillRect(abslocation.x + xf, abslocation.y + yf, c_width, _r_height, this->UnderMouseItemBackColor);
									d2d->DrawRect(abslocation.x + xf, abslocation.y + yf, c_width, _r_height, this->UnderMouseItemForeColor,
										r == this->UnderMouseRowIndex ? 1.0f : 0.5f);
									if (row.Cells.Count > c)
									{
										if (row.Cells[c].Image)
											d2d->DrawBitmap(row.Cells[c].Image,
												abslocation.x + xf + left,
												abslocation.y + yf + top,
												_size, _size
											);
									}
								}
								else
								{
									d2d->DrawRect(abslocation.x + xf, abslocation.y + yf, c_width, _r_height, this->ForeColor,
										r == this->UnderMouseRowIndex ? 1.0f : 0.5f);
									if (row.Cells.Count > c)
									{
										if (row.Cells[c].Image)
											d2d->DrawBitmap(row.Cells[c].Image,
												abslocation.x + xf + left,
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
									d2d->FillRect(abslocation.x + xf, abslocation.y + yf, c_width, _r_height, this->UnderMouseItemBackColor);
									d2d->DrawRect(abslocation.x + xf, abslocation.y + yf, c_width, _r_height, this->UnderMouseItemForeColor,
										r == this->UnderMouseRowIndex ? 1.0f : 0.5f);
									if (row.Cells.Count > c)
									{
										d2d->DrawRect(
											abslocation.x + xf + left + (_rsize * 0.2),
											abslocation.y + yf + top + (_rsize * 0.2),
											_rsize * 0.6, _rsize * 0.6,
											this->ForeColor);
										if (row.Cells[c].Tag)
										{
											d2d->FillRect(
												abslocation.x + xf + left + (_rsize * 0.35),
												abslocation.y + yf + top + (_rsize * 0.35),
												_rsize * 0.3, _rsize * 0.3,
												this->ForeColor);
										}
									}
								}
								else
								{
									d2d->DrawRect(abslocation.x + xf, abslocation.y + yf, c_width, _r_height, this->ForeColor,
										r == this->UnderMouseRowIndex ? 1.0f : 0.5f);
									if (row.Cells.Count > c)
									{
										d2d->DrawRect(
											abslocation.x + xf + left + (_rsize * 0.2),
											abslocation.y + yf + top + (_rsize * 0.2),
											_rsize * 0.6, _rsize * 0.6,
											this->ForeColor);
										if (row.Cells[c].Tag)
										{
											d2d->FillRect(
												abslocation.x + xf + left + (_rsize * 0.35),
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
					}
					xf += this->Columns[c].Width;
					if (xf > _render_width)
					{
						break;
					}
				}
				yf += row_height;
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
void GridView::ReSizeRows(int count)
{
	// List<GridViewRow> 基于 std::vector，缩容时会自动析构元素；
	// 这里手工调用 Cells 的析构会导致双重析构/内存破坏。
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
				if (this->Columns[col].Type == ColumnType::Text)
				{
					auto width = font->GetTextSize(r.Cells[col].Text.c_str()).width;
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
void GridView::StartEditingCell(int col, int row)
{
	if (col < 0 || row < 0) return;
	if (col >= this->Columns.Count || row >= this->Rows.Count) return;

	// 切换编辑单元格时，先提交上一格
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
			// 恢复原始内容
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
	int renderCount = GetGridViewRenderRowCount(this) - 1;

	if (SelectedRowIndex < this->ScrollRowPosition)
	{
		this->ScrollRowPosition = SelectedRowIndex;
	}
	if (SelectedRowIndex > this->ScrollRowPosition + renderCount)
	{
		this->ScrollRowPosition += 1;
	}
}
bool GridView::CanScrollDown()
{
	int renderItemCount = GetGridViewRenderRowCount(this);
	return this->ScrollRowPosition < this->Rows.Count - renderItemCount;
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

	if (delta < 0)
	{
		if (CanScrollDown())
		{
			needUpdate = true;
			this->ScrollRowPosition += 1;
			this->ScrollChanged(this);
		}
	}
	else
	{
		if (this->ScrollRowPosition > 0)
		{
			needUpdate = true;
			this->ScrollRowPosition -= 1;
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

	if (this->InScroll)
	{
		needUpdate = true;
		SetScrollByPos(yof);
	}
	else
	{
		// 编辑状态下，按住左键拖动可更新选区
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

	if (xof < this->Width - 8)
	{
		POINT undermouseIndex = GetGridViewUnderMouseItem(xof, yof, this);
		if (undermouseIndex.y >= 0 && undermouseIndex.x >= 0 &&
			undermouseIndex.y < this->Rows.Count && undermouseIndex.x < this->Columns.Count)
		{
			// 如果点击的是当前编辑单元格，则仅移动光标/更新选区
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
			CancelEditing(true);
		}
	}
	else
	{
		this->InScroll = true;
		SetScrollByPos(yof);
	}

	MouseEventArgs event_obj(MouseButtons::Left, 0, xof, yof,0);
	this->OnMouseDown(this, event_obj);
	this->PostRender();
}
void GridView::HandleLeftButtonUp(int xof, int yof)
{
	this->InScroll = false;
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
			// 回车：同列下移一行继续编辑（保持原有交互）
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

		// 其它按键交给 GridView 的 KeyDown 事件（不做单元格移动）
		KeyEventArgs event_obj(static_cast<Keys>(wParam));
		this->OnKeyDown(this, event_obj);
		this->PostRender();
		return;
	}

	// 非编辑状态：方向键移动选中单元格
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

	// Ctrl+A / Ctrl+C / Ctrl+V / Ctrl+X / Backspace 等
	if (!this->Editing)
	{
		// 在非编辑状态直接输入字符时，自动进入编辑
		// 注意：IME 开启时 WM_CHAR 可能会发 VK_PROCESSKEY(229) 等“过程键”，这里与 TextBox 保持一致：只对 ASCII 可见字符启动编辑。
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

	// 与 TextBox 行为对齐：WM_CHAR 只处理 ASCII 可见字符，中文等由 WM_IME_COMPOSITION(GCS_RESULTSTR) 提交。
	if (ch >= 32 && ch <= 126)
	{
		const wchar_t buf[2] = { ch, L'\0' };
		EditInputText(buf);
	}
	else if (ch == 1) // Ctrl+A
	{
		this->EditSelectionStart = 0;
		this->EditSelectionEnd = (int)this->EditingText.size();
	}
	else if (ch == 8) // Backspace
	{
		EditInputBack();
	}
	else if (ch == 22) // Ctrl+V
	{
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
	else if (ch == 3 || ch == 24) // Ctrl+C / Ctrl+X
	{
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
		if (ch == 24) // Ctrl+X
		{
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

				// 关键：IME 开启时，某些输入（如半角数字/字母）会同时触发 WM_CHAR 和 GCS_RESULTSTR。
				// 为避免重复插入，这里只接受“非 ASCII”结果（中文/全角字符等）。该策略与 TextBox 原实现一致。
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

	float renderWidth = (float)this->Width - 8.0f;
	float rowHeight = GetRowHeightPx();
	float headHeight = GetHeadHeightPx();

	int drawIndex = row - this->ScrollRowPosition;
	if (drawIndex < 0) return false;
	float top = headHeight + (rowHeight * (float)drawIndex);
	if (top < headHeight || top > (float)this->Height) return false;

	float left = 0.0f;
	for (int i = 0; i < col; i++) left += this->Columns[i].Width;
	float width = this->Columns[col].Width;
	if (left >= renderWidth) return false;
	if (left + width > renderWidth) width = renderWidth - left;
	if (width <= 0.0f) return false;

	outRect = D2D1_RECT_F{ left, top, left + width, top + rowHeight };
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

	// 编辑过程中实时同步到数据（与旧实现一致）
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