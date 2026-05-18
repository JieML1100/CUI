#define NOMINMAX
#include "DropDownPopup.h"
#include "Form.h"

#include <algorithm>
#include <cmath>

namespace
{
	static float RectWidth(const D2D1_RECT_F& rect)
	{
		return rect.right - rect.left;
	}

	static float RectHeight(const D2D1_RECT_F& rect)
	{
		return rect.bottom - rect.top;
	}

	static bool PtInRectF(const D2D1_RECT_F& rect, float x, float y)
	{
		return x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom;
	}

	static float TextTop(Font* font, const D2D1_RECT_F& rect)
	{
		const float fontHeight = font ? font->FontHeight : 16.0f;
		return rect.top + std::max(0.0f, (RectHeight(rect) - fontHeight) * 0.5f);
	}

	static float EaseOutCubic(float t)
	{
		t = std::clamp(t, 0.0f, 1.0f);
		return 1.0f - std::pow(1.0f - t, 3.0f);
	}

	static SIZE GetTopLevelContentSize(Form* form)
	{
		if (!form) return SIZE{ 1, 1 };
		if (form->Handle)
		{
			RECT rc{};
			if (::GetClientRect(form->Handle, &rc))
			{
				float scale = form->GetDpiScale();
				if (scale <= 0.0f) scale = 1.0f;
				const int contentW = static_cast<int>(std::floor(static_cast<float>(rc.right - rc.left) / scale));
				const int contentH = static_cast<int>(std::floor(static_cast<float>(std::max<LONG>(0, rc.bottom - rc.top - form->ClientTop())) / scale));
				return SIZE{ std::max(1, contentW), std::max(1, contentH) };
			}
		}

		auto fallback = form->ClientSize;
		float scale = form->GetDpiScale();
		if (scale <= 0.0f) scale = 1.0f;
		return SIZE{
			std::max(1, static_cast<int>(std::floor(static_cast<float>(fallback.cx) / scale))),
			std::max(1, static_cast<int>(std::floor(static_cast<float>(fallback.cy) / scale)))
		};
	}
}

UIClass DropDownPopup::Type()
{
	return UIClass::UI_CUSTOM;
}

DropDownPopup::DropDownPopup()
{
	this->Visible = false;
	this->Enable = true;
	this->BackColor = D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 0.0f };
	this->Cursor = CursorKind::Arrow;
}

bool DropDownPopup::IsOpen() const
{
	return this->_visible && this->ItemCount() > 0 &&
		(this->_expanded || this->_animating || this->_dropProgress > 0.001f);
}

int DropDownPopup::ItemCount() const
{
	return static_cast<int>(this->Items.size());
}

int DropDownPopup::VisibleItemCount() const
{
	if (ItemCount() <= 0) return 0;
	if (_visibleCount > 0) return std::min(_visibleCount, ItemCount());
	return std::min(std::max(1, this->MaxVisibleItems), ItemCount());
}

int DropDownPopup::MaxScrollOffset() const
{
	return std::max(0, ItemCount() - VisibleItemCount());
}

bool DropDownPopup::HasScrollBar() const
{
	return ItemCount() > VisibleItemCount();
}

float DropDownPopup::FullPopupHeight() const
{
	return std::max(0.0f, static_cast<float>(this->_size.cy));
}

float DropDownPopup::CurrentDropProgress()
{
	if (!this->_animating)
	{
		this->_dropProgress = this->_expanded ? 1.0f : 0.0f;
		return this->_dropProgress;
	}

	const UINT64 now = ::GetTickCount64();
	const UINT64 elapsed = now >= this->_animStartTick ? (now - this->_animStartTick) : 0;
	float t = this->_animDurationMs > 0 ? static_cast<float>(elapsed) / static_cast<float>(this->_animDurationMs) : 1.0f;
	if (t >= 1.0f)
	{
		const bool wasCollapsing = this->_animTargetProgress <= 0.001f;
		this->_dropProgress = this->_animTargetProgress;
		this->_animating = false;
		if (wasCollapsing)
			FinishCollapsed(this->_raiseClosedAfterCollapse);
		return this->_dropProgress;
	}

	t = EaseOutCubic(t);
	this->_dropProgress = this->_animStartProgress + (this->_animTargetProgress - this->_animStartProgress) * t;
	return this->_dropProgress;
}

float DropDownPopup::CurrentPopupHeight()
{
	return FullPopupHeight() * CurrentDropProgress();
}

void DropDownPopup::EnsureSelectionInRange()
{
	const int count = ItemCount();
	if (count <= 0)
	{
		this->SelectedIndex = -1;
		this->HoveredIndex = -1;
		return;
	}
	this->SelectedIndex = std::clamp(this->SelectedIndex, 0, count - 1);
	if (this->HoveredIndex < 0 || this->HoveredIndex >= count)
		this->HoveredIndex = this->SelectedIndex;
}

void DropDownPopup::EnsureScrollInRange()
{
	this->_scrollOffset = std::clamp(this->_scrollOffset, 0, MaxScrollOffset());
}

void DropDownPopup::EnsureSelectionVisible()
{
	if (this->SelectedIndex < 0) return;
	const int visible = VisibleItemCount();
	if (visible <= 0) return;
	if (this->SelectedIndex < this->_scrollOffset)
		this->_scrollOffset = this->SelectedIndex;
	else if (this->SelectedIndex >= this->_scrollOffset + visible)
		this->_scrollOffset = this->SelectedIndex - visible + 1;
	EnsureScrollInRange();
}

void DropDownPopup::ShowAt(Form* form, Control* owner, const D2D1_RECT_F& anchorAbsRect,
	const std::vector<std::wstring>& items, int selectedIndex, float preferredWidth,
	float itemHeight, int maxVisibleItems)
{
	if (!form || items.empty())
	{
		Hide(false);
		return;
	}

	this->Parent = owner;
	this->ParentForm = form;
	this->_owner = owner;
	this->_anchorAbsRect = anchorAbsRect;
	this->_preferredWidth = preferredWidth;
	this->Items = items;
	this->SelectedIndex = selectedIndex;
	this->HoveredIndex = selectedIndex;
	this->ItemHeight = std::max(18.0f, itemHeight);
	this->MaxVisibleItems = std::max(1, maxVisibleItems);
	this->_scrollOffset = 0;
	this->_dragScroll = false;
	this->_collapseCleanupPending = false;
	this->_raiseClosedAfterCollapse = false;

	EnsureSelectionInRange();
	Reposition();
	EnsureSelectionVisible();
	Reposition();

	this->Visible = true;
	if (form->ForegroundControl && form->ForegroundControl != this && form->ForegroundControl->AutoCloseOnOutsideClick())
		form->ForegroundControl->ClosePopup();
	SetExpanded(true, false);
	form->ForegroundControl = this;
	form->SetSelectedControl(this, false);
	form->Invalidate(true);
}

void DropDownPopup::Hide(bool raiseClosed, bool immediate)
{
	Form* form = this->ParentForm;
	Control* owner = this->_owner;
	const bool wasOpen = this->_visible || this->_expanded || this->_animating ||
		this->_collapseCleanupPending || (form && form->ForegroundControl == this);

	this->_dragScroll = false;
	if (!wasOpen)
		return;

	if (immediate)
	{
		this->_expanded = false;
		this->_animating = false;
		this->_dropProgress = 0.0f;
		this->_animStartProgress = 0.0f;
		this->_animTargetProgress = 0.0f;
		this->_collapseCleanupPending = false;
		this->_raiseClosedAfterCollapse = false;
		this->Visible = false;
		this->HoveredIndex = -1;
		if (form && form->ForegroundControl == this)
			form->ForegroundControl = nullptr;
		if (form && form->Selected == this)
			form->SetSelectedControl(owner && owner->IsVisual ? owner : nullptr, false);
		if (raiseClosed)
			this->Closed(this);
		if (form)
			form->Invalidate(true);
		return;
	}

	if (!this->_expanded && this->_animating && this->_animTargetProgress <= 0.001f)
		return;

	if (form && form->Selected == this)
		form->SetSelectedControl(owner && owner->IsVisual ? owner : nullptr, false);
	SetExpanded(false, raiseClosed);
}

void DropDownPopup::SetExpanded(bool expanded, bool raiseClosedAfterCollapse)
{
	const bool wantExpand = expanded && ItemCount() > 0;
	CurrentDropProgress();
	if (wantExpand)
	{
		this->Visible = true;
		this->_collapseCleanupPending = false;
		this->_raiseClosedAfterCollapse = false;
	}
	this->_expanded = wantExpand;
	this->_animStartProgress = this->_dropProgress;
	this->_animTargetProgress = wantExpand ? 1.0f : 0.0f;
	this->_collapseCleanupPending = !wantExpand;
	this->_raiseClosedAfterCollapse = !wantExpand && raiseClosedAfterCollapse;

	if (std::fabs(this->_animTargetProgress - this->_animStartProgress) < 0.001f)
	{
		this->_dropProgress = this->_animTargetProgress;
		this->_animating = false;
		if (!wantExpand)
			FinishCollapsed(this->_raiseClosedAfterCollapse);
		else
			this->_collapseCleanupPending = false;
	}
	else
	{
		this->_animStartTick = ::GetTickCount64();
		this->_animating = true;
	}

	if (this->ParentForm)
		this->ParentForm->Invalidate(true);
	this->PostRender();
}

void DropDownPopup::FinishCollapsed(bool raiseClosed)
{
	Form* form = this->ParentForm;
	Control* owner = this->_owner;
	const bool wasOpen = this->_visible || this->_collapseCleanupPending || this->_dropProgress > 0.001f ||
		(form && form->ForegroundControl == this);

	this->_dropProgress = 0.0f;
	this->_animating = false;
	this->_expanded = false;
	this->_collapseCleanupPending = false;
	this->_raiseClosedAfterCollapse = false;
	this->_dragScroll = false;
	this->HoveredIndex = -1;
	this->Visible = false;

	if (form && form->ForegroundControl == this)
		form->ForegroundControl = nullptr;
	if (form && form->Selected == this)
		form->SetSelectedControl(owner && owner->IsVisual ? owner : nullptr, false);
	if (raiseClosed && wasOpen)
		this->Closed(this);
	if (form)
		form->Invalidate(true);
}

void DropDownPopup::Reposition()
{
	if (!this->ParentForm || ItemCount() <= 0) return;

	const SIZE client = GetTopLevelContentSize(this->ParentForm);
	const float clientW = std::max(1.0f, static_cast<float>(client.cx));
	const float clientH = std::max(1.0f, static_cast<float>(client.cy));
	const float edge = 2.0f;
	const float preferredW = this->_preferredWidth > 0.0f ? this->_preferredWidth : RectWidth(this->_anchorAbsRect);
	float popupW = std::max(this->MinWidth, preferredW);
	popupW = std::min(popupW, std::max(1.0f, clientW - edge * 2.0f));

	const int wantedVisible = std::min(std::max(1, this->MaxVisibleItems), ItemCount());
	const float wantedH = this->VerticalPadding * 2.0f + static_cast<float>(wantedVisible) * this->ItemHeight;
	const float availableBelow = clientH - this->_anchorAbsRect.bottom - this->DropGap - edge;
	const float availableAbove = this->_anchorAbsRect.top - this->DropGap - edge;
	const bool openAbove = wantedH > availableBelow && availableAbove > availableBelow;
	const float available = openAbove ? availableAbove : availableBelow;
	int fitVisible = wantedVisible;
	if (available > this->VerticalPadding * 2.0f + 1.0f)
	{
		fitVisible = std::min(wantedVisible,
			std::max(1, static_cast<int>(std::floor((available - this->VerticalPadding * 2.0f) / this->ItemHeight))));
	}
	this->_visibleCount = std::max(1, fitVisible);

	float popupH = this->VerticalPadding * 2.0f + static_cast<float>(VisibleItemCount()) * this->ItemHeight;
	popupH = std::min(popupH, std::max(1.0f, clientH - edge * 2.0f));

	float popupX = std::clamp(this->_anchorAbsRect.left, edge, std::max(edge, clientW - popupW - edge));
	float popupY = openAbove ? this->_anchorAbsRect.top - this->DropGap - popupH : this->_anchorAbsRect.bottom + this->DropGap;
	popupY = std::clamp(popupY, edge, std::max(edge, clientH - popupH - edge));

	POINT parentAbs{ 0, 0 };
	if (this->_owner)
		parentAbs = this->_owner->AbsLocation;
	this->SetRuntimeLocation(POINT{
		static_cast<LONG>(std::round(popupX - (float)parentAbs.x)),
		static_cast<LONG>(std::round(popupY - (float)parentAbs.y))
		});
	this->Size = SIZE{
		static_cast<LONG>(std::ceil(popupW)),
		static_cast<LONG>(std::ceil(popupH))
	};
	EnsureScrollInRange();
}

bool DropDownPopup::IsAnimationRunning()
{
	CurrentDropProgress();
	return this->_animating || this->_collapseCleanupPending;
}

bool DropDownPopup::GetAnimatedInvalidRect(D2D1_RECT_F& outRect)
{
	if (!this->_visible && !this->_collapseCleanupPending) return false;

	auto loc = this->AbsLocation;
	outRect = D2D1::RectF(
		static_cast<float>(loc.x),
		static_cast<float>(loc.y),
		static_cast<float>(loc.x + this->_size.cx),
		static_cast<float>(loc.y + this->_size.cy));
	outRect.left = std::min(outRect.left, this->_anchorAbsRect.left);
	outRect.top = std::min(outRect.top, this->_anchorAbsRect.top);
	outRect.right = std::max(outRect.right, this->_anchorAbsRect.right);
	outRect.bottom = std::max(outRect.bottom, this->_anchorAbsRect.bottom);
	return true;
}

SIZE DropDownPopup::ActualSize()
{
	auto size = this->Size;
	if (this->_visible || this->_animating || this->_collapseCleanupPending)
	{
		const float currentHeight = CurrentPopupHeight();
		size.cy = currentHeight > 0.001f ? static_cast<LONG>(std::ceil(currentHeight)) : 0;
	}
	return size;
}

void DropDownPopup::ScrollBy(int deltaItems)
{
	if (deltaItems == 0) return;
	const int old = this->_scrollOffset;
	this->_scrollOffset = std::clamp(this->_scrollOffset + deltaItems, 0, MaxScrollOffset());
	if (old != this->_scrollOffset)
		this->PostRender();
}

D2D1_RECT_F DropDownPopup::GetScrollTrackRect() const
{
	if (!HasScrollBar()) return D2D1::RectF();
	const float width = static_cast<float>(this->_size.cx);
	const float height = static_cast<float>(this->_size.cy);
	const float barW = std::max(4.0f, this->ScrollBarWidth);
	const float inset = std::max(2.0f, this->ScrollTrackPadding);
	return D2D1::RectF(width - barW - inset, inset, width - inset, std::max(inset, height - inset));
}

D2D1_RECT_F DropDownPopup::GetScrollThumbRect() const
{
	auto track = GetScrollTrackRect();
	if (!HasScrollBar() || RectHeight(track) <= 0.0f) return D2D1::RectF();
	const int visible = VisibleItemCount();
	const int count = ItemCount();
	float thumbH = (static_cast<float>(visible) / static_cast<float>(count)) * RectHeight(track);
	thumbH = std::clamp(thumbH, std::min(RectHeight(track), 16.0f), RectHeight(track));
	const float movable = std::max(0.0f, RectHeight(track) - thumbH);
	const int maxScroll = MaxScrollOffset();
	const float per = maxScroll > 0 ? std::clamp(static_cast<float>(_scrollOffset) / static_cast<float>(maxScroll), 0.0f, 1.0f) : 0.0f;
	const float top = track.top + movable * per;
	return D2D1::RectF(track.left, top, track.right, top + thumbH);
}

bool DropDownPopup::IsOverScrollBar(int xof, int yof) const
{
	if (!HasScrollBar()) return false;
	auto track = GetScrollTrackRect();
	return PtInRectF(track, static_cast<float>(xof), static_cast<float>(yof));
}

void DropDownPopup::UpdateScrollByThumb(float yof)
{
	if (!this->_dragScroll || !HasScrollBar()) return;
	auto track = GetScrollTrackRect();
	auto thumb = GetScrollThumbRect();
	const float thumbH = RectHeight(thumb);
	const float movable = std::max(1.0f, RectHeight(track) - thumbH);
	float top = std::clamp(yof - this->_scrollThumbGrabOffsetY, track.top, track.bottom - thumbH);
	float per = (top - track.top) / movable;
	this->_scrollOffset = std::clamp(static_cast<int>(std::round(per * MaxScrollOffset())), 0, MaxScrollOffset());
	this->PostRender();
}

int DropDownPopup::HitTestItem(int xof, int yof) const
{
	if (!IsOpen()) return -1;
	const float itemRight = static_cast<float>(this->_size.cx) - (HasScrollBar() ? (this->ScrollBarWidth + this->ScrollTrackPadding * 2.0f) : 0.0f);
	if (xof < 0 || static_cast<float>(xof) > itemRight) return -1;
	const float localY = static_cast<float>(yof) - this->VerticalPadding;
	if (localY < 0.0f) return -1;
	const int viewIndex = static_cast<int>(localY / this->ItemHeight);
	if (viewIndex < 0 || viewIndex >= VisibleItemCount()) return -1;
	const int index = this->_scrollOffset + viewIndex;
	return index >= 0 && index < ItemCount() ? index : -1;
}

bool DropDownPopup::CommitSelection(int selectedIndex)
{
	if (selectedIndex < 0 || selectedIndex >= ItemCount()) return false;
	this->SelectedIndex = selectedIndex;
	const std::wstring selectedText = this->Items[static_cast<size_t>(selectedIndex)];
	this->SelectionChanged(this, selectedIndex, selectedText);
	Hide(true);
	return true;
}

bool DropDownPopup::CanHandleMouseWheel(int delta, int xof, int yof)
{
	(void)xof;
	(void)yof;
	if (!IsOpen() || delta == 0 || MaxScrollOffset() <= 0) return false;
	return delta > 0 ? this->_scrollOffset > 0 : this->_scrollOffset < MaxScrollOffset();
}

bool DropDownPopup::HandlesNavigationKey(WPARAM key) const
{
	switch (key)
	{
	case VK_UP:
	case VK_DOWN:
	case VK_PRIOR:
	case VK_NEXT:
	case VK_HOME:
	case VK_END:
	case VK_RETURN:
	case VK_SPACE:
	case VK_ESCAPE:
		return true;
	default:
		return false;
	}
}

CursorKind DropDownPopup::QueryCursor(int xof, int yof)
{
	if (IsOverScrollBar(xof, yof))
		return CursorKind::SizeNS;
	if (HitTestItem(xof, yof) >= 0)
		return CursorKind::Hand;
	return CursorKind::Arrow;
}

void DropDownPopup::Update()
{
	if (!this->IsVisual || !this->Visible || !this->ParentForm || !this->ParentForm->Render) return;
	auto d2d = this->ParentForm->Render;
	EnsureSelectionInRange();
	EnsureScrollInRange();

	const float width = static_cast<float>(this->Width);
	const float height = static_cast<float>(this->Height);
	const float renderHeight = CurrentPopupHeight();
	if (renderHeight <= 0.001f)
		return;
	const float border = std::max(1.0f, this->Border);
	this->BeginRender();
	{
		d2d->FillRect(0.0f, 0.0f, width, height, D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 0.0f });
		d2d->PushDrawRect(0.0f, 0.0f, width, renderHeight);
		const auto popupRect = D2D1::RectF(0.0f, 0.0f, width, renderHeight);
		d2d->FillRoundRect(popupRect, this->DropBackColor, this->CornerRadius);
		d2d->DrawRoundRect(border * 0.5f, border * 0.5f,
			std::max(0.0f, width - border), std::max(0.0f, renderHeight - border),
			this->DropBorderColor, border, this->CornerRadius);

		const bool hasScroll = HasScrollBar();
		const float scrollPad = hasScroll ? (this->ScrollBarWidth + this->ScrollTrackPadding * 2.0f) : 0.0f;
		const float itemRight = std::max(1.0f, width - scrollPad - 2.0f);
		class Font* fontObj = this->Font ? this->Font : GetDefaultFontObject();
		d2d->PushDrawRect(0.0f, 0.0f, width, height);
		for (int i = 0; i < VisibleItemCount(); i++)
		{
			const int index = this->_scrollOffset + i;
			if (index < 0 || index >= ItemCount()) break;
			const float itemTop = this->VerticalPadding + static_cast<float>(i) * this->ItemHeight;
			const auto itemRect = D2D1::RectF(2.0f, itemTop, itemRight, itemTop + this->ItemHeight);
			const auto stateRect = D2D1::RectF(itemRect.left + 4.0f, itemRect.top + 2.0f,
				std::max(itemRect.left + 5.0f, itemRect.right - 2.0f), itemRect.bottom - 2.0f);
			const bool selected = index == this->SelectedIndex;
			const bool hovered = index == this->HoveredIndex;
			if (selected)
			{
				d2d->FillRoundRect(stateRect, this->SelectedItemBackColor, this->ItemCornerRadius);
				const float accentW = 3.0f;
				const float accentH = std::max(5.0f, RectHeight(stateRect) - 10.0f);
				d2d->FillRoundRect(stateRect.left, stateRect.top + 5.0f, accentW, accentH, this->AccentColor, accentW * 0.5f);
			}
			else if (hovered)
			{
				d2d->FillRoundRect(stateRect, this->UnderMouseBackColor, this->ItemCornerRadius);
			}
			const auto textColor = hovered ? this->UnderMouseForeColor : (selected ? this->SelectedItemForeColor : this->ForeColor);
			d2d->DrawString(this->Items[static_cast<size_t>(index)],
				itemRect.left + this->HorizontalPadding,
				TextTop(fontObj, itemRect),
				std::max(1.0f, itemRect.right - itemRect.left - this->HorizontalPadding * 2.0f),
				RectHeight(itemRect),
				textColor,
				fontObj);
		}
		d2d->PopDrawRect();

		if (hasScroll)
		{
			auto track = GetScrollTrackRect();
			auto thumb = GetScrollThumbRect();
			d2d->FillRoundRect(track, this->ScrollBackColor, RectWidth(track) * 0.5f);
			d2d->FillRoundRect(thumb, this->ScrollForeColor, RectWidth(thumb) * 0.5f);
		}
		d2d->PopDrawRect();
	}
	this->EndRender();

	if (IsAnimationRunning())
		this->PostRender();
}

bool DropDownPopup::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof)
{
	if (!this->Enable || !this->Visible) return true;
	switch (message)
	{
	case WM_MOUSEWHEEL:
	{
		const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
		if (delta != 0)
			ScrollBy(delta > 0 ? -1 : 1);
		MouseEventArgs e(MouseButtons::None, 0, xof, yof, delta);
		this->OnMouseWheel(this, e);
		return true;
	}
	case WM_MOUSEMOVE:
	{
		if (this->ParentForm)
			this->ParentForm->UnderMouse = this;
		if (this->_dragScroll)
		{
			UpdateScrollByThumb(static_cast<float>(yof));
		}
		else
		{
			int hover = HitTestItem(xof, yof);
			if (hover != this->HoveredIndex)
			{
				this->HoveredIndex = hover;
				this->PostRender();
			}
		}
		MouseEventArgs e(MouseButtons::None, 0, xof, yof, HIWORD(wParam));
		this->OnMouseMove(this, e);
		return true;
	}
	case WM_LBUTTONDOWN:
	{
		if (this->ParentForm)
			this->ParentForm->SetSelectedControl(this, false);
		if (IsOverScrollBar(xof, yof))
		{
			auto thumb = GetScrollThumbRect();
			const float localY = static_cast<float>(yof);
			const bool hitThumb = PtInRectF(thumb, static_cast<float>(xof), localY);
			this->_scrollThumbGrabOffsetY = hitThumb ? localY - thumb.top : RectHeight(thumb) * 0.5f;
			this->_dragScroll = true;
			UpdateScrollByThumb(localY);
		}
		else
		{
			this->HoveredIndex = HitTestItem(xof, yof);
			this->PostRender();
		}
		MouseEventArgs e(MouseButtons::Left, 0, xof, yof, HIWORD(wParam));
		this->OnMouseDown(this, e);
		return true;
	}
	case WM_LBUTTONUP:
	{
		const bool wasDragging = this->_dragScroll;
		this->_dragScroll = false;
		if (!wasDragging)
			CommitSelection(HitTestItem(xof, yof));
		MouseEventArgs e(MouseButtons::Left, 0, xof, yof, HIWORD(wParam));
		this->OnMouseUp(this, e);
		return true;
	}
	case WM_LBUTTONDBLCLK:
	{
		CommitSelection(HitTestItem(xof, yof));
		MouseEventArgs e(MouseButtons::Left, 2, xof, yof, HIWORD(wParam));
		this->OnMouseDoubleClick(this, e);
		return true;
	}
	case WM_KEYDOWN:
	{
		EnsureSelectionInRange();
		int hover = this->HoveredIndex >= 0 ? this->HoveredIndex : this->SelectedIndex;
		switch (wParam)
		{
		case VK_ESCAPE:
			Hide(true);
			break;
		case VK_RETURN:
		case VK_SPACE:
			CommitSelection(hover);
			break;
		case VK_UP:
			this->HoveredIndex = ItemCount() > 0 ? std::max(0, hover - 1) : -1;
			EnsureSelectionVisible();
			if (this->HoveredIndex < this->_scrollOffset)
				this->_scrollOffset = this->HoveredIndex;
			this->PostRender();
			break;
		case VK_DOWN:
			this->HoveredIndex = ItemCount() > 0 ? std::min(ItemCount() - 1, hover + 1) : -1;
			if (this->HoveredIndex >= this->_scrollOffset + VisibleItemCount())
				this->_scrollOffset = this->HoveredIndex - VisibleItemCount() + 1;
			EnsureScrollInRange();
			this->PostRender();
			break;
		case VK_HOME:
			this->HoveredIndex = ItemCount() > 0 ? 0 : -1;
			this->_scrollOffset = 0;
			this->PostRender();
			break;
		case VK_END:
			this->HoveredIndex = ItemCount() > 0 ? ItemCount() - 1 : -1;
			this->_scrollOffset = MaxScrollOffset();
			this->PostRender();
			break;
		case VK_PRIOR:
			this->HoveredIndex = ItemCount() > 0 ? std::max(0, hover - VisibleItemCount()) : -1;
			if (this->HoveredIndex < this->_scrollOffset)
				this->_scrollOffset = this->HoveredIndex;
			EnsureScrollInRange();
			this->PostRender();
			break;
		case VK_NEXT:
			this->HoveredIndex = ItemCount() > 0 ? std::min(ItemCount() - 1, hover + VisibleItemCount()) : -1;
			if (this->HoveredIndex >= this->_scrollOffset + VisibleItemCount())
				this->_scrollOffset = this->HoveredIndex - VisibleItemCount() + 1;
			EnsureScrollInRange();
			this->PostRender();
			break;
		default:
			break;
		}
		KeyEventArgs e((Keys)(wParam | 0));
		this->OnKeyDown(this, e);
		return true;
	}
	case WM_KEYUP:
	{
		KeyEventArgs e((Keys)(wParam | 0));
		this->OnKeyUp(this, e);
		return true;
	}
	default:
		break;
	}
	return true;
}
