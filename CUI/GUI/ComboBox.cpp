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

bool ComboBox::CanHandleMouseWheel(int delta, int localX, int localY)
{
	(void)localX;
	(void)localY;
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
			this->ParentForm->ForegroundControl = nullptr;
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

bool ComboBox::IsHeaderHit(int localX, int localY)
{
	return localX >= 0 && localX <= this->Width && localY >= 0 && localY < this->Height;
}

bool ComboBox::IsDropdownHit(int localX, int localY, float dropdownHeight)
{
	const float top = DropdownTop();
	return localX >= 0 && localX <= this->Width &&
		(float)localY >= top &&
		(float)localY < (top + dropdownHeight);
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
CursorKind ComboBox::QueryCursor(int localX, int localY)
{
	if (!this->Enable) return CursorKind::Arrow;

	const bool hasVScroll = (IsDropDownVisible() && static_cast<int>(this->values.size()) > VisibleItemCount());
	const float dropHeight = CurrentDropdownHeight();
	const float dropTop = DropdownTop();
	if (hasVScroll && localX >= (this->Width - 12) && localY >= dropTop && (float)localY <= (dropTop + dropHeight))
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
			this->ParentForm->ForegroundControl = nullptr;
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
	return this->Size;
}

bool ComboBox::ContainsForegroundPoint(int localX, int localY)
{
	return IsDropDownVisible() && IsDropdownHit(localX, localY, CurrentDropdownHeight());
}

void ComboBox::InvalidateVisual()
{
	if (!this->IsVisual || !this->ParentForm) return;
	const float titleBarOffset = (this->ParentForm->VisibleHead ? (float)this->ParentForm->HeadHeight : 0.0f);
	auto currentRect = this->AbsRect;
	if (IsDropDownVisible() || _collapseCleanupPending)
	{
		currentRect.bottom += (LONG)std::ceil((std::max)(0.0f, this->DropGap) + FullDropdownHeight());
	}
	currentRect.top += titleBarOffset;
	currentRect.bottom += titleBarOffset;

	if (_hasLastInvalidatedClientRect)
	{
		D2D1_RECT_F unionRect{};
		unionRect.left = (std::min)(_lastInvalidatedClientRect.left, currentRect.left);
		unionRect.top = (std::min)(_lastInvalidatedClientRect.top, currentRect.top);
		unionRect.right = (std::max)(_lastInvalidatedClientRect.right, currentRect.right);
		unionRect.bottom = (std::max)(_lastInvalidatedClientRect.bottom, currentRect.bottom);
		this->ParentForm->Invalidate(unionRect, false);
	}
	else
	{
		this->ParentForm->Invalidate(currentRect, false);
	}

	_lastInvalidatedClientRect = currentRect;
	_hasLastInvalidatedClientRect = true;
}

void ComboBox::DrawScroll()
{
	auto d2d = this->ParentForm->Render;
	const int visibleItemCount = VisibleItemCount();
	const float renderHeight = CurrentDropdownHeight();
	if (this->values.size() > 0 && visibleItemCount > 0 && renderHeight > 0.0f)
	{
		const int itemCount = static_cast<int>(this->values.size());
		if (visibleItemCount < itemCount)
		{
			int maxScroll = itemCount - visibleItemCount;
			float scrollThumbHeight = ((float)visibleItemCount / (float)this->values.size()) * renderHeight;
			if (scrollThumbHeight < COMBO_MIN_SCROLL_BLOCK)scrollThumbHeight = COMBO_MIN_SCROLL_BLOCK;
			if (scrollThumbHeight > renderHeight) scrollThumbHeight = renderHeight;
			float scrollThumbMoveSpace = renderHeight - scrollThumbHeight;
			float scrollRatio = (float)this->ExpandScroll / (float)maxScroll;
			float scrollThumbTop = scrollRatio * scrollThumbMoveSpace;
			const float barW = (std::max)(4.0f, this->ScrollBarWidth);
			const float barX = (float)this->Width - barW - 5.0f;
			const float barY = DropdownTop() + 5.0f;
			const float barH = (std::max)(0.0f, renderHeight - 10.0f);
			if (barH <= 0.0f) return;
			if (scrollThumbHeight > barH) scrollThumbHeight = barH;
			const float moveSpace = (std::max)(0.0f, barH - scrollThumbHeight);
			scrollThumbTop = scrollRatio * moveSpace;
			d2d->FillRoundRect(barX, barY, barW, barH, this->ScrollBackColor, barW * 0.5f);
			d2d->FillRoundRect(barX, barY + scrollThumbTop, barW, scrollThumbHeight, this->ScrollForeColor, barW * 0.5f);
		}
	}
}
void ComboBox::UpdateScrollDrag(float posY) {
	if (!isDraggingScroll) return;
	int visibleItemCount = VisibleItemCount();
	float renderHeight = CurrentDropdownHeight();
	if (visibleItemCount <= 0 || renderHeight <= 0.0f) return;
	int maxScroll = static_cast<int>(this->values.size()) - visibleItemCount;
	const float barInset = 5.0f;
	const float barHeight = (std::max)(0.0f, renderHeight - barInset * 2.0f);
	if (barHeight <= 0.0f) return;
	float scrollBlockHeight = ((float)visibleItemCount / (float)this->values.size()) * barHeight;
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
		InvalidateVisual();
	}
}
void ComboBox::Update()
{
	if (this->IsVisual == false)return;
	auto d2d = this->ParentForm->Render;
	EnsureSelectionInRange();
	EnsureScrollInRange();
	const float actualWidth = static_cast<float>(this->Width);
	const float actualHeight = static_cast<float>(this->Height);
	const float controlWidth = static_cast<float>(this->Width);
	const float controlHeight = static_cast<float>(this->Height);
	CurrentDropProgress();
	this->BeginRender(actualWidth, actualHeight);
	{
		d2d->FillRect(0, 0, actualWidth, actualHeight, D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 0.0f });
		const float border = (std::max)(1.0f, this->BorderThickness);
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
		const auto borderColor = IsDropDownVisible() ? this->AccentColor : this->BorderColor;
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

void ComboBox::UpdateForeground()
{
	if (this->IsVisual == false)return;
	auto d2d = this->ParentForm->Render;
	EnsureSelectionInRange();
	EnsureScrollInRange();

	const float controlWidth = static_cast<float>(this->Width);
	const float controlHeight = static_cast<float>(this->Height);
	const float dropHeight = CurrentDropdownHeight();
	const int visibleCount = VisibleItemCount();
	if (dropHeight <= 0.0f || visibleCount <= 0)
	{
		if (!_animating && _dropProgress <= 0.001f)
			_collapseCleanupPending = false;
		return;
	}

	const float dropTop = DropdownTop();
	this->BeginRender(controlWidth, dropTop + dropHeight);
	{
		const float border = (std::max)(1.0f, this->BorderThickness);
		const bool hasScroll = static_cast<int>(this->values.size()) > visibleCount;
		const float itemRight = hasScroll ? controlWidth - (std::max)(4.0f, this->ScrollBarWidth) - 11.0f : controlWidth;
		const D2D1_RECT_F dropRect = D2D1::RectF(0.0f, dropTop, controlWidth, dropTop + dropHeight);
		const float drawLeft = (std::max)(4.0f, this->ItemHorizontalPadding);
		auto font = this->Font;

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
	this->EndRender();
}

bool ComboBox::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY)
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
			se->InvalidateVisual();
		}
	}
	switch (message)
	{
	case WM_DROPFILES:
	{
		HDROP hDropInfo = HDROP(wParam);
		UINT fileCount = DragQueryFile(hDropInfo, 0xFFFFFFFF, nullptr, 0);
		TCHAR fileName[MAX_PATH];
		std::vector<std::wstring> files;
		for (UINT i = 0; i < fileCount; i++)
		{
			DragQueryFile(hDropInfo, i, fileName, MAX_PATH);
			files.push_back(fileName);
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
					this->InvalidateVisual();
				}
			}
			else
			{
				if (this->ExpandScroll < static_cast<int>(this->values.size()) - visibleCount)
				{
					this->ExpandScroll += 1;
					this->InvalidateVisual();
				}
			}
		}
		MouseEventArgs eventArgs = MouseEventArgs(MouseButtons::None, 0, localX, localY, GET_WHEEL_DELTA_WPARAM(wParam));
		this->OnMouseWheel(this, eventArgs);
	}
	break;
	case WM_MOUSEMOVE:
	{
		this->ParentForm->UnderMouse = this;
		if (IsDropDownVisible())
		{
			bool needsUpdate = false;
			if (isDraggingScroll)
			{
				UpdateScrollDrag(static_cast<float>(localY) - DropdownTop());
				needsUpdate = true;
			}
			else
			{
				if (IsDropdownHit(localX, localY, dropdownHeight))
				{
					int visibleItemIndex = int(((float)localY - DropdownTop()) / (float)this->Height);
					if (visibleItemIndex < visibleCount)
					{
						int itemIndex = visibleItemIndex + this->ExpandScroll;
						if (itemIndex < static_cast<int>(this->values.size()))
						{
							if (itemIndex != this->_underMouseIndex)
							{
								needsUpdate = true;
							}
							this->_underMouseIndex = itemIndex;
						}
					}
				}
				else if (this->_underMouseIndex != -1)
				{
					this->_underMouseIndex = -1;
					needsUpdate = true;
				}
			}
			if (needsUpdate)this->InvalidateVisual();
		}
		MouseEventArgs eventArgs = MouseEventArgs(MouseButtons::None, 0, localX, localY, HIWORD(wParam));
		this->OnMouseMove(this, eventArgs);
	}
	break;
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_MBUTTONDOWN:
	{
		if (WM_LBUTTONDOWN == message)
		{
			const float dropTop = DropdownTop();
			if (this->Expand && localX >= (Width - 12) && localX <= Width && (float)localY >= dropTop && (float)localY <= (dropTop + dropdownHeight))
			{
				const int visibleItemCount = visibleCount;
				if (visibleItemCount > 0 && static_cast<int>(this->values.size()) > visibleItemCount)
				{
					const int maxScroll = static_cast<int>(this->values.size()) - visibleItemCount;
					const float barInset = 5.0f;
					const float renderH = (std::max)(0.0f, dropdownHeight - barInset * 2.0f);
					float thumbH = ((float)visibleItemCount / (float)this->values.size()) * renderH;
					if (thumbH < COMBO_MIN_SCROLL_BLOCK) thumbH = COMBO_MIN_SCROLL_BLOCK;
					if (thumbH > renderH) thumbH = renderH;
					const float moveSpace = std::max(0.0f, renderH - thumbH);
					float per = 0.0f;
					if (maxScroll > 0) per = std::clamp((float)this->ExpandScroll / (float)maxScroll, 0.0f, 1.0f);
					const float thumbTop = per * moveSpace;
					const float dropdownLocalY = (float)localY - dropTop;
					const float barLocalY = dropdownLocalY - barInset;
					const bool hitThumb = (barLocalY >= thumbTop && barLocalY <= (thumbTop + thumbH));
					_scrollThumbGrabOffsetY = hitThumb ? (barLocalY - thumbTop) : (thumbH * 0.5f);
					isDraggingScroll = true;
					UpdateScrollDrag(dropdownLocalY);
				}
			}
			this->ParentForm->Selected = this;
		}
		MouseEventArgs eventArgs = MouseEventArgs(FromParamToMouseButtons(message), 0, localX, localY, HIWORD(wParam));
		this->OnMouseDown(this, eventArgs);
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
				if (IsHeaderHit(localX, localY))
				{
					SetExpanded(!this->Expand);
					if (this->ParentForm)
						this->ParentForm->Invalidate(true);
					this->InvalidateVisual();
					this->ParentForm->Selected = nullptr;
					MouseEventArgs eventArgs = MouseEventArgs(FromParamToMouseButtons(message), 0, localX, localY, HIWORD(wParam));
					this->OnMouseUp(this, eventArgs);
					break;
				}
				else if (this->Expand && IsDropdownHit(localX, localY, dropdownHeight))
				{
					int visibleItemIndex = int(((float)localY - DropdownTop()) / (float)this->Height);
					if (visibleItemIndex < visibleCount)
					{
						int itemIndex = visibleItemIndex + this->ExpandScroll;
						if (itemIndex < static_cast<int>(this->values.size()))
						{
							this->_underMouseIndex = itemIndex;
							this->SelectedIndex = this->_underMouseIndex;
							this->Text = this->values[static_cast<size_t>(this->SelectedIndex)];
							this->OnSelectionChanged(this);
							this->InvalidateVisual();
							SetExpanded(false);
							if (this->ParentForm)
								this->ParentForm->Invalidate(true);
							this->InvalidateVisual();
						}
					}
				}
			}
		}
		this->ParentForm->Selected = nullptr;
		MouseEventArgs eventArgs = MouseEventArgs(FromParamToMouseButtons(message), 0, localX, localY, HIWORD(wParam));
		this->OnMouseUp(this, eventArgs);
		this->InvalidateVisual();
	}
	break;
	case WM_LBUTTONDBLCLK:
	{
		MouseEventArgs eventArgs = MouseEventArgs(FromParamToMouseButtons(message), 0, localX, localY, HIWORD(wParam));
		this->OnMouseDoubleClick(this, eventArgs);
	}
	break;
	case WM_KEYDOWN:
	{
		KeyEventArgs eventArgs = KeyEventArgs((Keys)(wParam | 0));
		this->OnKeyDown(this, eventArgs);
	}
	break;
	case WM_KEYUP:
	{
		KeyEventArgs eventArgs = KeyEventArgs((Keys)(wParam | 0));
		this->OnKeyUp(this, eventArgs);
	}
	break;
	}
	return true;
}
