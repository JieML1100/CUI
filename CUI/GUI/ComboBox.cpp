#pragma once
#include "ComboBox.h"
#include "Form.h"
#pragma comment(lib, "Imm32.lib")
#define COMBO_MIN_SCROLL_BLOCK 16
UIClass ComboBox::Type() { return UIClass::UI_ComboBox; }
ComboBox::ComboBox(std::wstring text, int x, int y, int width, int height)
{
	this->Text = text;
	this->Location = POINT{ x,y };
	this->Size = SIZE{ width,height };
	this->BackColor = D2D1_COLOR_F{ 0.75f , 0.75f , 0.75f , 0.75f };
}
SIZE ComboBox::ActualSize()
{
	auto size = this->Size;
	if (this->Expand)
	{
		size.cy += this->Height * this->ExpandCount;
	}
	return size;
}
void ComboBox::DrawScroll()
{
	auto d2d = this->ParentForm->Render;
	auto abslocation = this->AbsLocation;
	auto font = this->Font;
	auto size = this->ActualSize();
	if (this->values.Count > 0)
	{
		float _render_width = this->Width - 8;
		float _render_height = this->Height * this->ExpandCount;
		float font_height = font->FontHeight;
		float row_height = font_height + 2.0f;
		int render_count = this->ExpandCount;
		if (render_count < this->values.Count)
		{
			int max_scroll = this->values.Count - render_count;
			float scroll_block_height = ((float)render_count / (float)this->values.Count) * (float)_render_height;
			if (scroll_block_height < COMBO_MIN_SCROLL_BLOCK)scroll_block_height = COMBO_MIN_SCROLL_BLOCK;
			float scroll_block_move_space = _render_height - scroll_block_height;
			float yt = scroll_block_height * 0.5f;
			float yb = _render_height - (scroll_block_height * 0.5f);
			float per = (float)this->ExpandScroll / (float)max_scroll;
			float scroll_tmp_y = per * scroll_block_move_space;
			float scroll_block_top = scroll_tmp_y;
			d2d->FillRoundRect(abslocation.x + (this->Width - 8.0f), abslocation.y + this->Height, 8.0f, _render_height, this->ScrollBackColor, 4.0f);
			d2d->FillRoundRect(abslocation.x + (this->Width - 8.0f), abslocation.y + scroll_block_top + this->Height, 8.0f, scroll_block_height, this->ScrollForeColor, 4.0f);
		}
	}
}
void ComboBox::UpdateScrollDrag(float posY) {
	if (!isDraggingScroll) return;
	auto d2d = this->ParentForm->Render;
	auto font = this->Font;
	float font_height = font->FontHeight;
	float dxHeight = this->ActualSize().cy - this->Height;
	int render_count = this->ExpandCount;
	float _render_height = this->Height * this->ExpandCount;
	int maxScroll = this->values.Count - render_count;
	float fontHeight = this->Font->FontHeight;
	float scrollBlockHeight = ((float)render_count / (float)this->values.Count) * (float)_render_height;
	float scrollTop = scrollBlockHeight * 0.5f;
	float scrollHeight = dxHeight - scrollBlockHeight;
	float chrckPos = posY - scrollTop;
	float per = chrckPos / scrollHeight;
	int newScroll = per * maxScroll;
	{
		ExpandScroll = newScroll;
		if (ExpandScroll < 0)
		{
			ExpandScroll = 0;
		}
		if (ExpandScroll > maxScroll)
		{
			ExpandScroll = maxScroll;
		}
		PostRender();
	}
}
void ComboBox::Update()
{
	if (this->IsVisual == false)return;
	bool isUnderMouse = this->ParentForm->UnderMouse == this;
	bool isSelected = this->ParentForm->Selected == this;
	auto d2d = this->ParentForm->Render;
	auto abslocation = this->AbsLocation;
	auto size = this->ActualSize();
	auto absRect = this->AbsRect;
	if (this->Expand)
	{
		absRect.bottom = absRect.top + size.cy;
	}
	d2d->PushDrawRect(absRect.left, absRect.top, absRect.right - absRect.left, absRect.bottom - absRect.top);
	{
		d2d->FillRect(abslocation.x, abslocation.y, size.cx, size.cy, this->BackColor);
		if (this->Image)
		{
			this->RenderImage();
		}
		auto font = this->Font;
		auto textSize = font->GetTextSize(this->Text);
		float drawLeft = 0.0f;
		float drawTop = 0.0f;
		if (this->Height > textSize.height)
		{
			drawLeft = drawTop = (this->Height - textSize.height) / 2.0f;
		}
		d2d->DrawString(this->Text, abslocation.x + drawLeft, abslocation.y + drawTop, this->ForeColor, this->Font);
		auto tSize = font->GetTextSize(L'﹀');
		float tLeft = this->Width - (tSize.width * 1.5f);
		float tTop = (this->Height - tSize.height) * 0.5f;
		d2d->DrawString(L"﹀", abslocation.x + tLeft, abslocation.y + tTop, this->ForeColor);
		if (this->Expand)
		{
			for (int i = this->ExpandScroll; i < this->ExpandScroll + this->ExpandCount && i < this->values.Count; i++)
			{
				if (i == _underMouseIndex)
				{
					int viewIndex = i - this->ExpandScroll;
					d2d->FillRect(abslocation.x,
						abslocation.y + ((viewIndex + 1) * this->Height),
						this->Width, this->Height, this->UnderMouseBackColor);
					d2d->DrawString(
						this->values[i],
						abslocation.x + drawLeft,
						abslocation.y + drawTop + (((i - this->ExpandScroll) + 1) * this->Height),
						UnderMouseForeColor, this->Font);
				}
				else
				{
					d2d->DrawString(
						this->values[i],
						abslocation.x + drawLeft,
						abslocation.y + drawTop + (((i - this->ExpandScroll) + 1) * this->Height),
						this->ForeColor, this->Font);
				}
			}
			this->DrawScroll();
			d2d->DrawRect(abslocation.x, abslocation.y, size.cx, this->Height, this->BolderColor, this->Boder);
		}
		d2d->DrawRect(abslocation.x, abslocation.y, size.cx, size.cy, this->BolderColor, this->Boder);
	}
	if (!this->Enable)
	{
		d2d->FillRect(abslocation.x, abslocation.y, size.cx, size.cy, { 1.0f ,1.0f ,1.0f ,0.5f });
	}
	d2d->PopDrawRect();
}
bool ComboBox::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof)
{
	if (!this->Enable || !this->Visible) return true;
	if (WM_LBUTTONDOWN == message)
	{
		if (this->ParentForm->Selected && this->ParentForm->Selected != this)
		{
			auto se = this->ParentForm->Selected;
			this->ParentForm->Selected = this;
			se->PostRender();
		}
	}
	switch (message)
	{
	case WM_DROPFILES:
	{
		HDROP hDropInfo = HDROP(wParam);
		UINT uFileNum = DragQueryFile(hDropInfo, 0xFFFFFFFF, NULL, 0);
		TCHAR strFileName[MAX_PATH];
		List<std::wstring> files;
		for (int i = 0; i < uFileNum; i++)
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
	break;
	case WM_MOUSEWHEEL:
	{
		if (this->Expand)
		{
			if (GET_WHEEL_DELTA_WPARAM(wParam) > 0)
			{
				if (this->ExpandScroll > 0)
				{
					this->ExpandScroll -= 1;
					this->PostRender();
				}
			}
			else
			{
				if (this->ExpandScroll < this->values.Count - this->ExpandCount)
				{
					this->ExpandScroll += 1;
					this->PostRender();
				}
			}
		}
		else
		{
			if (GET_WHEEL_DELTA_WPARAM(wParam) > 0)
			{
				if (this->SelectedIndex > 0)
				{
					this->SelectedIndex -= 1;
					this->Text = this->values[this->SelectedIndex];
					this->OnSelectionChanged(this);
					this->PostRender();
				}
			}
			else
			{
				if (this->SelectedIndex < this->values.Count - 1)
				{
					this->SelectedIndex += 1;
					this->Text = this->values[this->SelectedIndex];
					this->OnSelectionChanged(this);
					this->PostRender();
				}
			}
		}
		MouseEventArgs event_obj = MouseEventArgs(MouseButtons::None, 0, xof, yof, GET_WHEEL_DELTA_WPARAM(wParam));
		this->OnMouseWheel(this, event_obj);
	}
	break;
	case WM_MOUSEMOVE:
	{
		this->ParentForm->UnderMouse = this;
		if (this->Expand)
		{
			bool need_update = false;
			if (isDraggingScroll)
			{
				UpdateScrollDrag(yof - this->Height);
				need_update = true;
			}
			else
			{
				if (xof >= 0 && yof >= this->Height)
				{
					int _yof = int((yof - this->Height) / this->Height);
					if (_yof <= this->ExpandCount)
					{
						int idx = _yof + this->ExpandScroll;
						if (idx < this->values.Count)
						{
							if (idx != this->_underMouseIndex)
							{
								need_update = true;
							}
							this->_underMouseIndex = idx;
						}
					}
				}
			}
			if (need_update)this->PostRender();
		}
		MouseEventArgs event_obj = MouseEventArgs(MouseButtons::None, 0, xof, yof, HIWORD(wParam));
		this->OnMouseMove(this, event_obj);
	}
	break;
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_MBUTTONDOWN:
	{
		if (WM_LBUTTONDOWN == message)
		{
			// 仅在“展开状态 + 鼠标在下拉区滚动条轨道”时才进入滚动拖拽
			//（避免点击右侧下拉按钮/文本区时被误判为滚动条拖拽，导致无法展开/收起）
			if (this->Expand && xof >= (Width - 8) && xof <= Width && yof >= this->Height)
			{
				isDraggingScroll = true;
				// 点击滚动条轨道时，立即定位到相对位置（和 GridView 一致）
				UpdateScrollDrag((float)(yof - this->Height));
			}
			this->ParentForm->Selected = this;
		}
		MouseEventArgs event_obj = MouseEventArgs(FromParamToMouseButtons(message), 0, xof, yof, HIWORD(wParam));
		this->OnMouseDown(this, event_obj);
	}
	break;
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
	case WM_MBUTTONUP:
	{
		if (WM_LBUTTONUP == message && this->ParentForm->Selected == this)
		{
			if (isDraggingScroll) {
				isDraggingScroll = false;
			}
			else if (xof >= 0 && yof >= 0)
			{
				if (yof > 0)
				{
					if (yof < this->Height)
					{
						this->PostRender();

						this->Expand = !this->Expand;
						if (this->Expand)
							this->ParentForm->ForegroundControls.Add(this);
						else
							this->ParentForm->ForegroundControls.Remove(this);

						this->PostRender();
						this->ParentForm->Selected = NULL;
						MouseEventArgs event_obj = MouseEventArgs(FromParamToMouseButtons(message), 0, xof, yof, HIWORD(wParam));
						this->OnMouseUp(this, event_obj);
						break;
					}
					else if (this->Expand)
					{
						int _yof = int((yof - this->Height) / this->Height);
						if (_yof <= this->ExpandCount)
						{
							int idx = _yof + this->ExpandScroll;
							if (idx < this->values.Count)
							{
								this->_underMouseIndex = idx;
								this->SelectedIndex = this->_underMouseIndex;
								this->Text = this->values[this->SelectedIndex];
								this->OnSelectionChanged(this);
								this->PostRender();
								this->Expand = false;
								this->ParentForm->ForegroundControls.Remove(this);
								this->PostRender();
							}
						}
					}
				}
			}
		}
		this->ParentForm->Selected = NULL;
		MouseEventArgs event_obj = MouseEventArgs(FromParamToMouseButtons(message), 0, xof, yof, HIWORD(wParam));
		this->OnMouseUp(this, event_obj);
		this->PostRender();
	}
	break;
	case WM_LBUTTONDBLCLK:
	{
		MouseEventArgs event_obj = MouseEventArgs(FromParamToMouseButtons(message), 0, xof, yof, HIWORD(wParam));
		this->OnMouseDoubleClick(this, event_obj);
	}
	break;
	case WM_KEYDOWN:
	{
		KeyEventArgs event_obj = KeyEventArgs((Keys)(wParam | 0));
		this->OnKeyDown(this, event_obj);
	}
	break;
	case WM_KEYUP:
	{
		KeyEventArgs event_obj = KeyEventArgs((Keys)(wParam | 0));
		this->OnKeyUp(this, event_obj);
	}
	break;
	}
	return true;
}