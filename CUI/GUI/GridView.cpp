#pragma once
#include "GridView.h"
#include "Form.h"
#include "TextBox.h"
#pragma comment(lib, "Imm32.lib")

CellValue::CellValue() : str(L"")
{}
CellValue::CellValue(std::wstring s) : str(s), Tag(NULL), Image(NULL)
{}
CellValue::CellValue(wchar_t* s) :str(s), Tag(NULL), Image(NULL)
{}
CellValue::CellValue(const wchar_t* s) : str(s), Tag(NULL), Image(NULL)
{}
CellValue::CellValue(ID2D1Bitmap* img) : str(L""), Tag(NULL), Image(img)
{}
CellValue::CellValue(__int64 tag) : str(L""), Tag(tag), Image(NULL)
{}
CellValue::CellValue(bool tag) : str(L""), Tag(tag), Image(NULL)
{}
CellValue::CellValue(__int32 tag) : str(L""), Tag(tag), Image(NULL)
{}
CellValue::CellValue(unsigned __int32 tag) : str(L""), Tag(tag), Image(NULL)
{}
CellValue::CellValue(unsigned __int64 tag) : str(L""), Tag(tag), Image(NULL)
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
		return this->Rows[this->SelectedRowIndex].Cells[SelectedColumnIndex].str;
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
		
		const float scrollBlockHeight = max(static_cast<float>(renderingHeight * 0.1f),
			(renderingHeight * renderCount) / static_cast<float>(rowCount));

		const float topPosition = scrollBlockHeight * 0.5f;
		const float bottomPosition = renderingHeight - topPosition;

		
		if (bottomPosition > topPosition)
		{
			const float percent = (yof - topPosition) / (bottomPosition - topPosition);
			this->ScrollRowPosition = std::clamp(maxScroll * percent, 0.0f, static_cast<float>(maxScroll));
		}
	}

	
	this->ScrollRowPosition = max(min(this->ScrollRowPosition, static_cast<float>(rowCount -
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
			if (ScrollRowPosition < 0)
				ScrollRowPosition = 0;
			if (ScrollRowPosition >= this->Rows.Count)
				ScrollRowPosition = this->Rows.Count - 1;
			unsigned int s_x = 0;
			unsigned int s_y = this->ScrollRowPosition;
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
			for (uint32_t r = s_y; r < this->Rows.Count && i < (int)(_render_height / row_height); r++, i++)
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
								float _size = c_width < row_height ? c_width : row_height;
								if (c == this->SelectedColumnIndex && r == this->SelectedRowIndex)
								{
									d2d->FillRect(abslocation.x + xf, abslocation.y + yf, c_width, _r_height, this->SelectedItemBackColor);
									d2d->DrawRect(abslocation.x + xf, abslocation.y + yf, c_width, _r_height, this->SelectedItemForeColor,
										r == this->UnderMouseRowIndex ? 1.0f : 0.5f);
									if (row.Cells.Count > c)
										d2d->DrawString(row.Cells[c].str,
											abslocation.x + xf + 1.0f,
											abslocation.y + yf + text_top,
											this->SelectedItemForeColor, font);
								}
								else if (c == this->UnderMouseColumnIndex && r == this->UnderMouseRowIndex)
								{
									d2d->FillRect(abslocation.x + xf, abslocation.y + yf, c_width, _r_height, this->UnderMouseItemBackColor);
									d2d->DrawRect(abslocation.x + xf, abslocation.y + yf, c_width, _r_height, this->UnderMouseItemForeColor,
										r == this->UnderMouseRowIndex ? 1.0f : 0.5f);
									if (row.Cells.Count > c)
										d2d->DrawString(row.Cells[c].str,
											abslocation.x + xf + 1.0f,
											abslocation.y + yf + text_top,
											this->UnderMouseItemForeColor, font);
								}
								else
								{
									d2d->DrawRect(abslocation.x + xf, abslocation.y + yf, c_width, _r_height, this->ForeColor,
										r == this->UnderMouseRowIndex ? 1.0f : 0.5f);
									if (row.Cells.Count > c)
										d2d->DrawString(row.Cells[c].str,
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
	if (this->Count == 0)
	{
		this->AddControl(new TextBox(L"", 10, 10))->Visible = true;
		this->get(0)->Tag = 0xFFFFFFFFFFFFFFFF;
		this->get(0)->OnTextChanged += [](class Control* sender, std::wstring o, std::wstring n)
			{
				GridView* gd = (GridView*)sender->Parent;
				int xx = sender->Tag >> 32;
				int yy = sender->Tag & 0xffffffff;
				if (xx >= 0 && yy >= 0)
				{
					std::wstring str = sender->Text.c_str();
					gd->Rows[yy].Cells[xx] = str;
				}

			};
		this->get(0)->OnKeyDown += [](class Control* sender, KeyEventArgs e)
			{
				GridView* gd = (GridView*)sender->Parent;
				if (e.KeyData == Keys::Return)
				{
					if (gd->SelectedRowIndex < gd->Rows.Count - 1)
					{
						TextBox* tb = (TextBox*)sender;
						gd->ChangeEditionSelected(gd->SelectedColumnIndex, gd->SelectedRowIndex + 1);
						tb->SelectionStart = 0;
						tb->SelectionEnd = tb->Text.length();
						if (tb->Bottom > gd->Height)
						{
							gd->ScrollRowPosition += 1;
						}
					}
				}
			};
	}
	TextBox* c = (TextBox*)this->get(0);
	if (this->UpdateEdit() && (isSelected || this->ParentForm->Selected == c))
	{
		c->Update();
	}
}
void GridView::ReSizeRows(int count)
{
	if (this->Rows.Count < count)
	{
		for (int i = 0; i < (count - this->Rows.Count); i++)
			this->Rows.Add(GridViewRow());
	}
	else if (count < this->Rows.Count)
	{
		for (int i = count; i < this->Rows.Count; i++)
		{
			this->Rows[i].Cells.~List<CellValue>();
		}
		this->Rows.resize(count);
	}
}
bool GridView::UpdateEdit()
{
	TextBox* c = (TextBox*)this->get(0);
	if (this->SelectedColumnIndex >= 0 &&
		this->Columns[this->SelectedColumnIndex].Type == ColumnType::Text &&
		this->Columns[this->SelectedColumnIndex].CanEdit)
	{
		if (this->SelectedRowIndex >= 0)
		{

			int topIndex = this->ScrollRowPosition;
			int drawIndex = this->SelectedRowIndex - topIndex;
			auto d2d = this->ParentForm->Render;
			auto font = this->Font;
			float font_height = font->FontHeight;
			float row_height = font_height + 2.0f;

			float renderLeft = 0.0f;
			for (int i = 0; i < this->SelectedColumnIndex; i++)
			{
				renderLeft += this->Columns[i].Width;
			}
			auto head_font = HeadFont ? HeadFont : font;
			float head_height = this->HeadHeight == 0.0f ? head_font->FontHeight : this->HeadHeight;
			float rendertop = head_height + (row_height * drawIndex);
			if (rendertop >= head_height && rendertop <= this->Height)
			{
				c->Location = POINT{ (int)renderLeft ,(int)rendertop };
				c->Size = SIZE{ (int)this->Columns[this->SelectedColumnIndex].Width ,(int)row_height };
				c->Visible = true;
				return true;
			}
			return false;
		}
	}
	c->Visible = false;
	return false;
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
					auto width = font->GetTextSize(r.Cells[col].str.c_str()).width;
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
	if (this->SelectedColumnIndex != col || this->SelectedRowIndex != row)
	{
		SaveCurrentEditingCell();

		if (this->Columns[col].Type == ColumnType::Text && this->Columns[col].CanEdit)
		{
			TextBox* tb = static_cast<TextBox*>(this->get(0));
			tb->Visible = true;
			tb->Tag = (static_cast<ULONG64>(col) << 32) | static_cast<ULONG64>(row);
			tb->Text = this->Rows[row].Cells[col].str;
			tb->SelectionStart = 0;
			this->ParentForm->Selected = tb;
		}

		this->SelectedColumnIndex = col;
		this->SelectedRowIndex = row;
		this->SelectionChanged(this);
	}
}
void GridView::CancelEditing()
{
	TextBox* c = static_cast<TextBox*>(this->get(0));
	if (c && c->Visible)
	{
		SaveCurrentEditingCell();
		c->Visible = false;
		this->ParentForm->Selected = this;
	}

	this->SelectedColumnIndex = -1;
	this->SelectedRowIndex = -1;
}
void GridView::SaveCurrentEditingCell()
{
	TextBox* c = static_cast<TextBox*>(this->get(0));
	if (c && c->Visible)
	{
		int oldCol = c->Tag >> 32;
		int oldRow = c->Tag & 0xFFFFFFFF;
		if (oldCol >= 0 && oldRow >= 0)
		{
			this->Rows[oldRow].Cells[oldCol].str = c->Text;
		}
	}
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
	if (this->get(0)->Visible)
	{
		int oldx = this->get(0)->Tag >> 32;
		int oldy = this->get(0)->Tag & 0xffffffff;
		if (oldx >= 0 && oldy >= 0)
		{
			std::wstring str = this->get(0)->Text.c_str();
			this->Rows[oldy].Cells[oldx] = str;
		}
	}
	if (this->Columns[col].Type == ColumnType::Text && this->Columns[col].CanEdit)
	{
		TextBox* tb = (TextBox*)this->get(0);
		tb->Visible = true;
		tb->Tag = (ULONG64)col << 32 | (ULONG64)row;
		tb->Text = this->Rows[row].Cells[col].str;
		tb->SelectionStart = 0;
		this->ParentForm->Selected = tb;
	}
	this->SelectedColumnIndex = col;
	this->SelectedRowIndex = row;
	this->SelectionChanged(this);
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
void GridView::HandleLeftButtonDown(int xof, int yof, bool hitEdit)
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
			HandleCellClick(undermouseIndex.x, undermouseIndex.y);
		}
		else
		{
			CancelEditing();
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

	TextBox* c = static_cast<TextBox*>(this->get(0));
	POINT input_location = { 0, 0 };
	bool hitEdit = false;

	if (c)
	{
		input_location = c->Location;
		auto size = c->ActualSize();
		hitEdit = (xof >= input_location.x &&
			yof >= input_location.y &&
			xof <= (input_location.x + size.cx) &&
			yof <= (input_location.y + size.cy)) && c->Visible;
	}

	
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
		HandleLeftButtonDown(xof, yof, hitEdit);
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

	default:
		break;
	}

	
	if (hitEdit && c)
	{
		c->ProcessMessage(message, wParam, lParam, xof - input_location.x, yof - input_location.y);
	}

	return true;
}
#pragma endregion