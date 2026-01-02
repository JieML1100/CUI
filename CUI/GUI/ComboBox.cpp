#pragma once
#include "ComboBox.h"
#include "Form.h"
#pragma comment(lib, "Imm32.lib")
#define COMBO_MIN_SCROLL_BLOCK 16
UIClass ComboBox::Type() { return UIClass::UI_ComboBox; }

GET_CPP(ComboBox, List<std::wstring>&, Items)
{
	return this->values;
}
SET_CPP(ComboBox, List<std::wstring>&, Items)
{
	this->values = value;
}
CursorKind ComboBox::QueryCursor(int xof, int yof)
{
	if (!this->Enable) return CursorKind::Arrow;

	const bool hasVScroll = (this->Expand && this->values.Count > this->ExpandCount);
	if (hasVScroll && xof >= (this->Width - 8) && yof >= this->Height)
		return CursorKind::SizeNS;

	return this->Cursor;
}
ComboBox::ComboBox(std::wstring text, int x, int y, int width, int height)
{
	this->Text = text;
	this->Location = POINT{ x,y };
	this->Size = SIZE{ width,height };
	this->BackColor = D2D1_COLOR_F{ 0.75f , 0.75f , 0.75f , 0.75f };
	this->Cursor = CursorKind::Hand;
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
	float fontHeight = font->FontHeight;
	float scrollBlockHeight = ((float)render_count / (float)this->values.Count) * (float)_render_height;
	if (scrollBlockHeight < COMBO_MIN_SCROLL_BLOCK)scrollBlockHeight = COMBO_MIN_SCROLL_BLOCK;
	float scrollHeight = dxHeight - scrollBlockHeight;
	if (scrollHeight <= 0.0f) return;
	float grab = std::clamp(_scrollThumbGrabOffsetY, 0.0f, scrollBlockHeight);
	float targetTop = posY - grab;
	float per = targetTop / scrollHeight;
	per = std::clamp(per, 0.0f, 1.0f);
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
		d2d->DrawString(this->Text, abslocation.x + drawLeft, abslocation.y + drawTop, this->ForeColor, font);
		// 右侧展开符号：使用图形绘制，避免随 Font 改变，并在展开/收起时显示不同图案
		{
			const float h = (float)this->Height;
			float iconSize = h * 0.38f;
			if (iconSize < 8.0f) iconSize = 8.0f;
			if (iconSize > 14.0f) iconSize = 14.0f;
			const float padRight = 8.0f;
			const float cx = abslocation.x + (float)this->Width - padRight - iconSize * 0.5f;
			const float cy = abslocation.y + h * 0.5f;
			const float half = iconSize * 0.5f;
			const float triH = iconSize * 0.55f;

			D2D1_TRIANGLE tri{};
			if (this->Expand)
			{
				// 已展开：上三角（提示可收起）
				tri.point1 = D2D1::Point2F(cx - half, cy + triH * 0.5f);
				tri.point2 = D2D1::Point2F(cx + half, cy + triH * 0.5f);
				tri.point3 = D2D1::Point2F(cx, cy - triH * 0.5f);
			}
			else
			{
				// 未展开：下三角
				tri.point1 = D2D1::Point2F(cx - half, cy - triH * 0.5f);
				tri.point2 = D2D1::Point2F(cx + half, cy - triH * 0.5f);
				tri.point3 = D2D1::Point2F(cx, cy + triH * 0.5f);
			}
			d2d->FillTriangle(tri, this->ForeColor);
		}
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
						UnderMouseForeColor, font);
				}
				else
				{
					d2d->DrawString(
						this->values[i],
						abslocation.x + drawLeft,
						abslocation.y + drawTop + (((i - this->ExpandScroll) + 1) * this->Height),
						this->ForeColor, font);
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
			if (this->Expand && xof >= (Width - 8) && xof <= Width && yof >= this->Height)
			{
				const int render_count = this->ExpandCount;
				if (render_count > 0 && this->values.Count > render_count)
				{
					const int max_scroll = this->values.Count - render_count;
					const float renderH = (float)(this->Height * this->ExpandCount);
					float thumbH = ((float)render_count / (float)this->values.Count) * renderH;
					if (thumbH < COMBO_MIN_SCROLL_BLOCK) thumbH = COMBO_MIN_SCROLL_BLOCK;
					if (thumbH > renderH) thumbH = renderH;
					const float moveSpace = std::max(0.0f, renderH - thumbH);
					float per = 0.0f;
					if (max_scroll > 0) per = std::clamp((float)this->ExpandScroll / (float)max_scroll, 0.0f, 1.0f);
					const float thumbTop = per * moveSpace;
					const float localY = (float)(yof - this->Height);
					const bool hitThumb = (localY >= thumbTop && localY <= (thumbTop + thumbH));
					_scrollThumbGrabOffsetY = hitThumb ? (localY - thumbTop) : (thumbH * 0.5f);
					isDraggingScroll = true;
					UpdateScrollDrag(localY);
				}
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
						// 置顶控件改为单指针管理
						if (this->ParentForm)
						{
							if (this->Expand)
								this->ParentForm->ForegroundControl = this;
							else if (this->ParentForm->ForegroundControl == this)
								this->ParentForm->ForegroundControl = NULL;
						}

					// 收起/展开时：强制立即重绘（UpdateWindow），避免 WM_PAINT 被延后导致“残影直到 Resize 才消失”
					if (this->ParentForm)
						this->ParentForm->Invalidate(true);
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
								if (this->ParentForm && this->ParentForm->ForegroundControl == this)
									this->ParentForm->ForegroundControl = NULL;
								// 选择后收起：立即重绘，清理 Overlay 残影
								if (this->ParentForm)
									this->ParentForm->Invalidate(true);
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