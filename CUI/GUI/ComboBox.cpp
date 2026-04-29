#pragma once
#include "ComboBox.h"
#include "Form.h"
#include <algorithm>
#include <cmath>
#pragma comment(lib, "Imm32.lib")
#define COMBO_MIN_SCROLL_BLOCK 16
UIClass ComboBox::Type() { return UIClass::UI_ComboBox; }

bool ComboBox::CanHandleMouseWheel(int delta, int xof, int yof)
{
	(void)xof;
	(void)yof;
	if (delta == 0 || !this->Expand) return false;
	const int visibleCount = VisibleItemCount();
	const int maxScroll = std::max(0, static_cast<int>(this->values.size()) - visibleCount);
	if (maxScroll <= 0) return false;
	EnsureScrollInRange();
	return delta > 0
		? this->ExpandScroll > 0
		: this->ExpandScroll < maxScroll;
}

GET_CPP(ComboBox, std::vector<std::wstring>&, Items)
{
	return this->values;
}
SET_CPP(ComboBox, std::vector<std::wstring>&, Items)
{
	const int oldIndex = this->SelectedIndex;
	const std::wstring oldText = this->Text;
	this->values = value;
	EnsureSelectionInRange();
	EnsureScrollInRange();
	if (this->SelectedIndex != oldIndex || this->Text != oldText)
		this->OnSelectionChanged(this);
	this->PostRender();
}

int ComboBox::VisibleItemCount()
{
	if (this->values.size() <= 0) return 0;
	int maxVisible = this->ExpandCount;
	if (maxVisible < 1) maxVisible = 1;
	return std::min(maxVisible, (int)this->values.size());
}

float ComboBox::FullDropdownHeight()
{
	return (float)(this->Height * VisibleItemCount());
}

float ComboBox::CurrentDropProgress()
{
	if (!_animating)
	{
		_dropProgress = this->Expand ? 1.0f : 0.0f;
		return _dropProgress;
	}

	const ULONGLONG now = ::GetTickCount64();
	const ULONGLONG elapsed = now >= _animStartTick ? (now - _animStartTick) : 0;
	float t = _animDurationMs > 0 ? (float)elapsed / (float)_animDurationMs : 1.0f;
	if (t >= 1.0f)
	{
		const bool wasCollapsing = (_animTargetProgress <= 0.001f && _dropProgress > 0.001f);
		_dropProgress = _animTargetProgress;
		_animating = false;
		if (wasCollapsing)
			_collapseCleanupPending = true;
		if (_dropProgress <= 0.0f && this->ParentForm && this->ParentForm->ForegroundControl == this)
			this->ParentForm->ForegroundControl = NULL;
		return _dropProgress;
	}
	t = 1.0f - std::pow(1.0f - (std::clamp)(t, 0.0f, 1.0f), 3.0f);
	_dropProgress = _animStartProgress + (_animTargetProgress - _animStartProgress) * t;
	return _dropProgress;
}

float ComboBox::CurrentDropdownHeight()
{
	return FullDropdownHeight() * CurrentDropProgress();
}

bool ComboBox::IsDropDownVisible()
{
	return this->Expand || _animating || _dropProgress > 0.001f;
}

bool ComboBox::IsDropDownInteractive()
{
	return this->Expand && CurrentDropdownHeight() > 0.5f;
}

void ComboBox::EnsureSelectionInRange()
{
	if (this->values.empty())
	{
		this->SelectedIndex = 0;
		this->Text = L"";
		return;
	}
	if (this->SelectedIndex < 0) this->SelectedIndex = 0;
	const int lastIndex = static_cast<int>(this->values.size()) - 1;
	if (this->SelectedIndex > lastIndex) this->SelectedIndex = lastIndex;
	this->Text = this->values[static_cast<size_t>(this->SelectedIndex)];
}

void ComboBox::EnsureScrollInRange()
{
	const int visibleCount = VisibleItemCount();
	const int maxScroll = std::max(0, (int)this->values.size() - visibleCount);
	if (this->ExpandScroll < 0) this->ExpandScroll = 0;
	if (this->ExpandScroll > maxScroll) this->ExpandScroll = maxScroll;
	if (_underMouseIndex >= static_cast<int>(this->values.size())) _underMouseIndex = -1;
}
CursorKind ComboBox::QueryCursor(int xof, int yof)
{
	if (!this->Enable) return CursorKind::Arrow;

	const bool hasVScroll = (IsDropDownVisible() && static_cast<int>(this->values.size()) > VisibleItemCount());
	const float dropHeight = CurrentDropdownHeight();
	if (hasVScroll && xof >= (this->Width - 8) && yof >= this->Height && (float)yof <= ((float)this->Height + dropHeight))
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

int ComboBox::ItemCount() const
{
	return static_cast<int>(this->values.size());
}

std::wstring ComboBox::GetSelectedItem() const
{
	if (this->SelectedIndex < 0 || this->SelectedIndex >= static_cast<int>(this->values.size()))
		return L"";
	return this->values[static_cast<size_t>(this->SelectedIndex)];
}

void ComboBox::SetSelectedIndex(int index)
{
	const int oldIndex = this->SelectedIndex;
	const std::wstring oldText = this->Text;
	this->SelectedIndex = index;
	EnsureSelectionInRange();
	EnsureScrollInRange();
	if (this->SelectedIndex != oldIndex || this->Text != oldText)
		this->OnSelectionChanged(this);
	this->PostRender();
}

int ComboBox::FindItem(const std::wstring& text) const
{
	auto it = std::find(this->values.begin(), this->values.end(), text);
	if (it == this->values.end()) return -1;
	return static_cast<int>(std::distance(this->values.begin(), it));
}

int ComboBox::AddItem(const std::wstring& text)
{
	this->values.push_back(text);
	EnsureSelectionInRange();
	EnsureScrollInRange();
	this->PostRender();
	return static_cast<int>(this->values.size()) - 1;
}

int ComboBox::InsertItem(int index, const std::wstring& text)
{
	const int insertIndex = std::clamp(index, 0, static_cast<int>(this->values.size()));
	this->values.insert(this->values.begin() + insertIndex, text);
	if (this->values.size() == 1)
	{
		this->SelectedIndex = 0;
		this->Text = text;
		this->OnSelectionChanged(this);
	}
	else if (insertIndex <= this->SelectedIndex)
	{
		this->SelectedIndex++;
	}
	EnsureSelectionInRange();
	EnsureScrollInRange();
	this->PostRender();
	return insertIndex;
}

bool ComboBox::RemoveItemAt(int index)
{
	if (index < 0 || index >= static_cast<int>(this->values.size())) return false;
	const int oldIndex = this->SelectedIndex;
	const std::wstring oldText = this->Text;
	this->values.erase(this->values.begin() + index);
	if (index < this->SelectedIndex)
		this->SelectedIndex--;
	else if (index == this->SelectedIndex && this->SelectedIndex >= static_cast<int>(this->values.size()))
		this->SelectedIndex = static_cast<int>(this->values.size()) - 1;
	EnsureSelectionInRange();
	EnsureScrollInRange();
	if (this->values.empty())
		SetExpanded(false);
	if (this->SelectedIndex != oldIndex || this->Text != oldText)
		this->OnSelectionChanged(this);
	this->PostRender();
	return true;
}

void ComboBox::ClearItems()
{
	if (this->values.empty()) return;
	const int oldIndex = this->SelectedIndex;
	const std::wstring oldText = this->Text;
	this->values.clear();
	this->SelectedIndex = 0;
	this->Text = L"";
	this->ExpandScroll = 0;
	this->_underMouseIndex = -1;
	SetExpanded(false);
	if (this->SelectedIndex != oldIndex || this->Text != oldText)
		this->OnSelectionChanged(this);
	this->PostRender();
}

bool ComboBox::IsAnimationRunning()
{
	CurrentDropProgress();
	return _animating || _collapseCleanupPending;
}

bool ComboBox::GetAnimatedInvalidRect(D2D1_RECT_F& outRect)
{
	if (!IsDropDownVisible() && !_collapseCleanupPending) return false;
	auto abs = this->AbsRect;
	outRect = abs;
	outRect.bottom += FullDropdownHeight();
	return true;
}

void ComboBox::SetExpanded(bool expanded)
{
	const bool wantExpand = expanded && VisibleItemCount() > 0;
	CurrentDropProgress();
	if (wantExpand)
	{
		EnsureSelectionInRange();
		EnsureScrollInRange();
		if (this->ParentForm)
			this->ParentForm->ForegroundControl = this;
	}
	this->Expand = wantExpand;
	_animStartProgress = _dropProgress;
	_animTargetProgress = wantExpand ? 1.0f : 0.0f;
	_collapseCleanupPending = false;
	if (std::fabs(_animTargetProgress - _animStartProgress) < 0.001f)
	{
		_dropProgress = _animTargetProgress;
		_animating = false;
		if (!wantExpand && this->ParentForm && this->ParentForm->ForegroundControl == this)
			this->ParentForm->ForegroundControl = NULL;
	}
	else
	{
		_animStartTick = ::GetTickCount64();
		_animating = true;
	}
	if (this->ParentForm)
		this->ParentForm->Invalidate(false);
}
SIZE ComboBox::ActualSize()
{
	auto size = this->Size;
	const float dropHeight = CurrentDropdownHeight();
	if (dropHeight > 0.0f)
	{
		size.cy += (LONG)std::ceil(dropHeight);
	}
	return size;
}
void ComboBox::DrawScroll()
{
	auto d2d = this->ParentForm->Render;
	const int render_count = VisibleItemCount();
	const float renderHeight = CurrentDropdownHeight();
	if (this->values.size() > 0 && render_count > 0 && renderHeight > 0.0f)
	{
		const int itemCount = static_cast<int>(this->values.size());
		if (render_count < itemCount)
		{
			int max_scroll = itemCount - render_count;
			float scroll_block_height = ((float)render_count / (float)this->values.size()) * renderHeight;
			if (scroll_block_height < COMBO_MIN_SCROLL_BLOCK)scroll_block_height = COMBO_MIN_SCROLL_BLOCK;
			if (scroll_block_height > renderHeight) scroll_block_height = renderHeight;
			float scroll_block_move_space = renderHeight - scroll_block_height;
			float per = (float)this->ExpandScroll / (float)max_scroll;
			float scroll_block_top = per * scroll_block_move_space;
			// 在局部坐标中：滚动条 X = Width - 8，Y = Height（下拉区起始）
			d2d->FillRoundRect(this->Width - 8.0f, (float)this->Height, 8.0f, renderHeight, this->ScrollBackColor, 4.0f);
			d2d->FillRoundRect(this->Width - 8.0f, scroll_block_top + this->Height, 8.0f, scroll_block_height, this->ScrollForeColor, 4.0f);
		}
	}
}
void ComboBox::UpdateScrollDrag(float posY) {
	if (!isDraggingScroll) return;
	int render_count = VisibleItemCount();
	float _render_height = CurrentDropdownHeight();
	float dxHeight = _render_height;
	if (render_count <= 0 || _render_height <= 0.0f) return;
	int maxScroll = static_cast<int>(this->values.size()) - render_count;
	float scrollBlockHeight = ((float)render_count / (float)this->values.size()) * (float)_render_height;
	if (scrollBlockHeight < COMBO_MIN_SCROLL_BLOCK)scrollBlockHeight = COMBO_MIN_SCROLL_BLOCK;
	if (scrollBlockHeight > _render_height) scrollBlockHeight = _render_height;
	float scrollHeight = dxHeight - scrollBlockHeight;
	if (scrollHeight <= 0.0f) return;
	float grab = std::clamp(_scrollThumbGrabOffsetY, 0.0f, scrollBlockHeight);
	float targetTop = posY - grab;
	float per = targetTop / scrollHeight;
	per = std::clamp(per, 0.0f, 1.0f);
	int newScroll = static_cast<int>(per * maxScroll);
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
	auto d2d = this->ParentForm->Render;
	EnsureSelectionInRange();
	EnsureScrollInRange();
	auto size = this->ActualSize();
	const float actualWidth = static_cast<float>(size.cx);
	const float actualHeight = static_cast<float>(size.cy);
	const float controlWidth = static_cast<float>(this->Width);
	const float controlHeight = static_cast<float>(this->Height);
	this->BeginRender(actualWidth, actualHeight);
	{
		d2d->FillRect(0, 0, actualWidth, actualHeight, this->BackColor);
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
		d2d->DrawString(this->Text, drawLeft, drawTop, this->ForeColor, font);
		// 右侧展开符号：使用图形绘制，避免随 Font 改变，并在展开/收起时显示不同图案
		{
			const float h = controlHeight;
			float iconSize = h * 0.38f;
			if (iconSize < 8.0f) iconSize = 8.0f;
			if (iconSize > 14.0f) iconSize = 14.0f;
			const float padRight = 8.0f;
			const float cx = controlWidth - padRight - iconSize * 0.5f;
			const float cy = h * 0.5f;
			const float half = iconSize * 0.5f;
			const float triH = iconSize * 0.55f;

			D2D1_TRIANGLE tri{};
			if (this->Expand)
			{
				tri.point1 = D2D1::Point2F(cx - half, cy + triH * 0.5f);
				tri.point2 = D2D1::Point2F(cx + half, cy + triH * 0.5f);
				tri.point3 = D2D1::Point2F(cx, cy - triH * 0.5f);
			}
			else
			{
				tri.point1 = D2D1::Point2F(cx - half, cy - triH * 0.5f);
				tri.point2 = D2D1::Point2F(cx + half, cy - triH * 0.5f);
				tri.point3 = D2D1::Point2F(cx, cy + triH * 0.5f);
			}
			d2d->FillTriangle(tri, this->ForeColor);
		}
		const int visibleCount = VisibleItemCount();
		const float dropHeight = CurrentDropdownHeight();
		if (dropHeight > 0.0f && visibleCount > 0)
		{
			d2d->PushDrawRect(0.0f, controlHeight, controlWidth, dropHeight);
			const int itemCount = static_cast<int>(this->values.size());
			for (int i = this->ExpandScroll; i < this->ExpandScroll + visibleCount && i < itemCount; i++)
			{
				if (i == _underMouseIndex)
				{
					int viewIndex = i - this->ExpandScroll;
					d2d->FillRect(0,
						static_cast<float>((viewIndex + 1) * this->Height),
						controlWidth, controlHeight, this->UnderMouseBackColor);
					d2d->DrawString(
						this->values[static_cast<size_t>(i)],
						drawLeft,
						drawTop + static_cast<float>(((i - this->ExpandScroll) + 1) * this->Height),
						UnderMouseForeColor, font);
				}
				else
				{
					d2d->DrawString(
						this->values[static_cast<size_t>(i)],
						drawLeft,
						drawTop + static_cast<float>(((i - this->ExpandScroll) + 1) * this->Height),
						this->ForeColor, font);
				}
			}
			d2d->PopDrawRect();
			this->DrawScroll();
			d2d->DrawRect(0, 0, actualWidth, controlHeight, this->BolderColor, this->Boder);
		}
		d2d->DrawRect(0, 0, actualWidth, actualHeight, this->BolderColor, this->Boder);
	}
	if (!this->Enable)
	{
		d2d->FillRect(0, 0, actualWidth, actualHeight, { 1.0f ,1.0f ,1.0f ,0.5f });
	}
	this->EndRender();
	if (!_animating && _dropProgress <= 0.001f)
		_collapseCleanupPending = false;
}
bool ComboBox::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof)
{
	if (!this->Enable || !this->Visible) return true;
	EnsureSelectionInRange();
	EnsureScrollInRange();
	const int visibleCount = VisibleItemCount();
	const float dropdownHeight = CurrentDropdownHeight();
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
				if (this->ExpandScroll < static_cast<int>(this->values.size()) - visibleCount)
				{
					this->ExpandScroll += 1;
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
		if (IsDropDownVisible())
		{
			bool need_update = false;
			if (isDraggingScroll)
			{
				UpdateScrollDrag(static_cast<float>(yof - this->Height));
				need_update = true;
			}
			else
			{
				if (xof >= 0 && yof >= this->Height && (float)yof < ((float)this->Height + dropdownHeight))
				{
					int _yof = int((yof - this->Height) / this->Height);
					if (_yof < visibleCount)
					{
						int idx = _yof + this->ExpandScroll;
						if (idx < static_cast<int>(this->values.size()))
						{
							if (idx != this->_underMouseIndex)
							{
								need_update = true;
							}
							this->_underMouseIndex = idx;
						}
					}
				}
				else if (this->_underMouseIndex != -1)
				{
					this->_underMouseIndex = -1;
					need_update = true;
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
			if (this->Expand && xof >= (Width - 8) && xof <= Width && yof >= this->Height && (float)yof <= ((float)this->Height + dropdownHeight))
			{
				const int render_count = visibleCount;
				if (render_count > 0 && static_cast<int>(this->values.size()) > render_count)
				{
					const int max_scroll = static_cast<int>(this->values.size()) - render_count;
					const float renderH = dropdownHeight;
					float thumbH = ((float)render_count / (float)this->values.size()) * renderH;
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
				if (yof >= 0)
				{
					if (yof < this->Height)
					{
						SetExpanded(!this->Expand);
						if (this->ParentForm)
							this->ParentForm->Invalidate(true);
						this->PostRender();
						this->ParentForm->Selected = NULL;
						MouseEventArgs event_obj = MouseEventArgs(FromParamToMouseButtons(message), 0, xof, yof, HIWORD(wParam));
						this->OnMouseUp(this, event_obj);
						break;
					}
					else if (this->Expand && (float)yof < ((float)this->Height + dropdownHeight))
					{
						int _yof = int((yof - this->Height) / this->Height);
						if (_yof < visibleCount)
						{
							int idx = _yof + this->ExpandScroll;
							if (idx < static_cast<int>(this->values.size()))
							{
								this->_underMouseIndex = idx;
								this->SelectedIndex = this->_underMouseIndex;
								this->Text = this->values[static_cast<size_t>(this->SelectedIndex)];
								this->OnSelectionChanged(this);
								this->PostRender();
								SetExpanded(false);
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
