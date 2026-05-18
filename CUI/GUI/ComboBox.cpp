#pragma once
#include "ComboBox.h"
#include "Form.h"
#include <algorithm>
#include <cmath>
#pragma comment(lib, "Imm32.lib")
#define COMBO_MIN_SCROLL_BLOCK 16
UIClass ComboBox::Type() { return UIClass::UI_ComboBox; }

static D2D1_POINT_2F RotatePoint(const D2D1_POINT_2F& point, float cx, float cy, float angle)
{
	const float dx = point.x - cx;
	const float dy = point.y - cy;
	const float s = std::sin(angle);
	const float c = std::cos(angle);
	return D2D1::Point2F(cx + dx * c - dy * s, cy + dx * s + dy * c);
}

static void DrawComboChevron(D2DGraphics* d2d, float cx, float cy, float size, float progress, D2D1_COLOR_F color)
{
	if (!d2d) return;
	progress = (std::clamp)(progress, 0.0f, 1.0f);
	const float halfW = size * 0.42f;
	const float halfH = size * 0.26f;
	const float angle = progress * 3.14159265359f;
	D2D1_POINT_2F p1 = D2D1::Point2F(cx - halfW, cy - halfH);
	D2D1_POINT_2F p2 = D2D1::Point2F(cx, cy + halfH);
	D2D1_POINT_2F p3 = D2D1::Point2F(cx + halfW, cy - halfH);
	p1 = RotatePoint(p1, cx, cy, angle);
	p2 = RotatePoint(p2, cx, cy, angle);
	p3 = RotatePoint(p3, cx, cy, angle);
	d2d->DrawLine(p1, p2, color, 1.8f);
	d2d->DrawLine(p2, p3, color, 1.8f);
}

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
	this->values = value;
	EnsureSelectionInRange();
	EnsureScrollInRange();
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

float ComboBox::DropdownTop()
{
	return (float)this->Height + (std::max)(0.0f, this->DropGap);
}

bool ComboBox::IsDropDownVisible()
{
	return this->Expand || _animating || _dropProgress > 0.001f;
}

bool ComboBox::IsDropDownInteractive()
{
	return this->Expand && CurrentDropdownHeight() > 0.5f;
}

bool ComboBox::IsHeaderHit(int xof, int yof)
{
	return xof >= 0 && xof <= this->Width && yof >= 0 && yof < this->Height;
}

bool ComboBox::IsDropdownHit(int xof, int yof, float dropdownHeight)
{
	const float top = DropdownTop();
	return xof >= 0 && xof <= this->Width &&
		(float)yof >= top &&
		(float)yof < (top + dropdownHeight);
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
	const float dropTop = DropdownTop();
	if (hasVScroll && xof >= (this->Width - 12) && yof >= dropTop && (float)yof <= (dropTop + dropHeight))
		return CursorKind::SizeNS;

	return this->Cursor;
}
ComboBox::ComboBox(std::wstring text, int x, int y, int width, int height)
{
	this->Text = text;
	this->Location = POINT{ x,y };
	this->Size = SIZE{ width,height };
	this->BackColor = D2D1_COLOR_F{ 1.0f , 1.0f , 1.0f , 0.98f };
	this->Cursor = CursorKind::Hand;
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
	outRect.bottom += (LONG)std::ceil((std::max)(0.0f, this->DropGap) + FullDropdownHeight());
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
		size.cy += (LONG)std::ceil((std::max)(0.0f, this->DropGap) + dropHeight);
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
			const float barW = (std::max)(4.0f, this->ScrollBarWidth);
			const float barX = (float)this->Width - barW - 5.0f;
			const float barY = DropdownTop() + 5.0f;
			const float barH = (std::max)(0.0f, renderHeight - 10.0f);
			if (barH <= 0.0f) return;
			if (scroll_block_height > barH) scroll_block_height = barH;
			const float moveSpace = (std::max)(0.0f, barH - scroll_block_height);
			scroll_block_top = per * moveSpace;
			d2d->FillRoundRect(barX, barY, barW, barH, this->ScrollBackColor, barW * 0.5f);
			d2d->FillRoundRect(barX, barY + scroll_block_top, barW, scroll_block_height, this->ScrollForeColor, barW * 0.5f);
		}
	}
}
void ComboBox::UpdateScrollDrag(float posY) {
	if (!isDraggingScroll) return;
	int render_count = VisibleItemCount();
	float _render_height = CurrentDropdownHeight();
	if (render_count <= 0 || _render_height <= 0.0f) return;
	int maxScroll = static_cast<int>(this->values.size()) - render_count;
	const float barInset = 5.0f;
	const float barHeight = (std::max)(0.0f, _render_height - barInset * 2.0f);
	if (barHeight <= 0.0f) return;
	float scrollBlockHeight = ((float)render_count / (float)this->values.size()) * barHeight;
	if (scrollBlockHeight < COMBO_MIN_SCROLL_BLOCK)scrollBlockHeight = COMBO_MIN_SCROLL_BLOCK;
	if (scrollBlockHeight > barHeight) scrollBlockHeight = barHeight;
	float scrollHeight = barHeight - scrollBlockHeight;
	if (scrollHeight <= 0.0f) return;
	float grab = std::clamp(_scrollThumbGrabOffsetY, 0.0f, scrollBlockHeight);
	float targetTop = posY - barInset - grab;
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
	const float dropHeight = CurrentDropdownHeight();
	this->BeginRender(actualWidth, actualHeight);
	{
		d2d->FillRect(0, 0, actualWidth, actualHeight, D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 0.0f });
		const float border = (std::max)(1.0f, this->Boder);
		const D2D1_RECT_F headerRect = D2D1::RectF(border * 0.5f, border * 0.5f, controlWidth - border * 0.5f, controlHeight - border * 0.5f);
		d2d->FillRoundRect(headerRect, this->BackColor, this->CornerRadius);
		if ((this->ParentForm && this->ParentForm->UnderMouse == this) || IsDropDownVisible())
			d2d->FillRoundRect(headerRect, this->HeaderHoverBackColor, this->CornerRadius);
		if (this->Image)
		{
			this->RenderImage(this->CornerRadius);
		}
		auto font = this->Font;
		auto textSize = font->GetTextSize(this->Text);
		const float drawLeft = (std::max)(4.0f, this->ItemHorizontalPadding);
		const float drawTop = (std::max)(0.0f, (controlHeight - textSize.height) * 0.5f);
		const float textRightPad = 30.0f;
		d2d->DrawString(this->Text, drawLeft, drawTop, (std::max)(1.0f, controlWidth - drawLeft - textRightPad), textSize.height + 2.0f, this->ForeColor, font);
		{
			float iconSize = (std::max)(6.0f, this->ChevronSize);
			if (iconSize < 8.0f) iconSize = 8.0f;
			if (iconSize > 14.0f) iconSize = 14.0f;
			const float cx = controlWidth - 16.0f;
			const float cy = controlHeight * 0.5f;
			DrawComboChevron(d2d, cx, cy, iconSize, _dropProgress, this->ForeColor);
		}
		const int visibleCount = VisibleItemCount();
		if (dropHeight > 0.0f && visibleCount > 0)
		{
			const float dropTop = DropdownTop();
			const bool hasScroll = static_cast<int>(this->values.size()) > visibleCount;
			const float itemRight = hasScroll ? controlWidth - (std::max)(4.0f, this->ScrollBarWidth) - 11.0f : controlWidth;
			const D2D1_RECT_F dropRect = D2D1::RectF(0.0f, dropTop, controlWidth, dropTop + dropHeight);
			d2d->PushDrawRect(0.0f, dropTop, controlWidth, dropHeight);
			d2d->FillRoundRect(dropRect, this->DropBackColor, this->DropCornerRadius);
			d2d->DrawRoundRect(dropRect.left + border * 0.5f, dropRect.top + border * 0.5f,
				(dropRect.right - dropRect.left) - border, (dropRect.bottom - dropRect.top) - border,
				this->DropBorderColor, border, this->DropCornerRadius);
			const int itemCount = static_cast<int>(this->values.size());
			for (int i = this->ExpandScroll; i < this->ExpandScroll + visibleCount && i < itemCount; i++)
			{
				const int viewIndex = i - this->ExpandScroll;
				const float itemTop = dropTop + static_cast<float>(viewIndex) * controlHeight;
				const float itemBottom = itemTop + controlHeight;
				const D2D1_RECT_F itemRect = D2D1::RectF(6.0f, itemTop + this->ItemVerticalPadding,
					(std::max)(7.0f, itemRight - 6.0f), itemBottom - this->ItemVerticalPadding);
				const bool isSelected = i == this->SelectedIndex;
				const bool isHovered = i == this->_underMouseIndex;
				if (isSelected)
				{
					d2d->FillRoundRect(itemRect, this->SelectedItemBackColor, this->CornerRadius);
					const float accentW = 3.0f;
					const float accentTop = itemRect.top + 5.0f;
					const float accentH = (std::max)(5.0f, (itemRect.bottom - itemRect.top) - 10.0f);
					d2d->FillRoundRect(itemRect.left, accentTop, accentW, accentH, this->AccentColor, accentW * 0.5f);
				}
				if (isHovered)
				{
					d2d->FillRoundRect(itemRect, this->UnderMouseBackColor, this->CornerRadius);
				}
				auto itemTextSize = font->GetTextSize(this->values[static_cast<size_t>(i)]);
				const float itemTextY = itemTop + (std::max)(0.0f, (controlHeight - itemTextSize.height) * 0.5f);
				const auto itemTextColor = isHovered ? this->UnderMouseForeColor : (isSelected ? this->SelectedItemForeColor : this->ForeColor);
				d2d->DrawString(
					this->values[static_cast<size_t>(i)],
					drawLeft,
					itemTextY,
					(std::max)(1.0f, itemRight - drawLeft - 8.0f),
					itemTextSize.height + 2.0f,
					itemTextColor, font);
			}
			d2d->PopDrawRect();
			this->DrawScroll();
		}
		const auto borderColor = IsDropDownVisible() ? this->AccentColor : this->BolderColor;
		d2d->DrawRoundRect(headerRect.left, headerRect.top, headerRect.right - headerRect.left,
			headerRect.bottom - headerRect.top, borderColor, border, this->CornerRadius);
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
				UpdateScrollDrag(static_cast<float>(yof) - DropdownTop());
				need_update = true;
			}
			else
			{
				if (IsDropdownHit(xof, yof, dropdownHeight))
				{
					int _yof = int(((float)yof - DropdownTop()) / (float)this->Height);
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
			const float dropTop = DropdownTop();
			if (this->Expand && xof >= (Width - 12) && xof <= Width && (float)yof >= dropTop && (float)yof <= (dropTop + dropdownHeight))
			{
				const int render_count = visibleCount;
				if (render_count > 0 && static_cast<int>(this->values.size()) > render_count)
				{
					const int max_scroll = static_cast<int>(this->values.size()) - render_count;
					const float barInset = 5.0f;
					const float renderH = (std::max)(0.0f, dropdownHeight - barInset * 2.0f);
					float thumbH = ((float)render_count / (float)this->values.size()) * renderH;
					if (thumbH < COMBO_MIN_SCROLL_BLOCK) thumbH = COMBO_MIN_SCROLL_BLOCK;
					if (thumbH > renderH) thumbH = renderH;
					const float moveSpace = std::max(0.0f, renderH - thumbH);
					float per = 0.0f;
					if (max_scroll > 0) per = std::clamp((float)this->ExpandScroll / (float)max_scroll, 0.0f, 1.0f);
					const float thumbTop = per * moveSpace;
					const float localY = (float)yof - dropTop;
					const float barLocalY = localY - barInset;
					const bool hitThumb = (barLocalY >= thumbTop && barLocalY <= (thumbTop + thumbH));
					_scrollThumbGrabOffsetY = hitThumb ? (barLocalY - thumbTop) : (thumbH * 0.5f);
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
			else
			{
				if (IsHeaderHit(xof, yof))
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
				else if (this->Expand && IsDropdownHit(xof, yof, dropdownHeight))
				{
					int _yof = int(((float)yof - DropdownTop()) / (float)this->Height);
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
