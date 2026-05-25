#define NOMINMAX
#include "ScrollView.h"
#include "Form.h"

#include <algorithm>
#include <cmath>

UIClass ScrollView::Type() { return UIClass::UI_ScrollView; }

bool ScrollView::HandlesNavigationKey(WPARAM key) const
{
	switch (key)
	{
	case VK_LEFT:
	case VK_RIGHT:
	case VK_UP:
	case VK_DOWN:
	case VK_HOME:
	case VK_END:
	case VK_PRIOR:
	case VK_NEXT:
		return true;
	default:
		return false;
	}
}

bool ScrollView::CanHandleMouseWheel(int delta, int localX, int localY)
{
	if (delta == 0) return false;
	if (_needsLayout || (_layoutEngine && _layoutEngine->NeedsLayout()))
	{
		PerformScrollContentLayout();
	}

	auto layout = this->CalcScrollLayout();
	ClampScrollOffsets(layout);
	if (localX < 0 || localY < 0 || localX >= this->Width || localY >= this->Height)
		return false;
	if (!layout.HasVerticalScroll || layout.MaxScrollY <= 0.0f)
		return false;
	return delta > 0
		? this->ScrollYOffset > 0
		: this->ScrollYOffset < (int)std::ceil(layout.MaxScrollY);
}

static bool FindDeepestWheelTarget(Control* root, int localX, int localY, Control*& outTarget, int& outX, int& outY)
{
	if (!root || !root->Visible || !root->Enable) return false;
	if (!root->ShouldHitTestChildrenAt(localX, localY))
	{
		outTarget = root;
		outX = localX;
		outY = localY;
		return true;
	}

	auto childOffset = root->GetChildrenRenderOffset();
	for (auto child : root->GetChildrenInReverseZOrder())
	{
		if (!child || !child->Visible || !child->Enable) continue;
		auto childLocation = child->ActualLocation;
		auto childSize = child->ActualSize();
		const int drawX = childLocation.x + childOffset.x;
		const int drawY = childLocation.y + childOffset.y;
		if (localX < drawX || localY < drawY || localX > (drawX + childSize.cx) || localY > (drawY + childSize.cy))
			continue;
		if (FindDeepestWheelTarget(child, localX - drawX, localY - drawY, outTarget, outX, outY))
			return true;
	}

	outTarget = root;
	outX = localX;
	outY = localY;
	return true;
}

ScrollView::ScrollView()
	: Panel()
{
}

ScrollView::ScrollView(int x, int y, int width, int height)
	: Panel(x, y, width, height)
{
}

POINT ScrollView::GetChildrenRenderOffset() const
{
	return POINT{ -this->ScrollXOffset, -this->ScrollYOffset };
}

D2D1_RECT_F ScrollView::GetChildrenClipRect()
{
	auto layout = this->CalcScrollLayout();
	return D2D1_RECT_F{ 0.0f, 0.0f, layout.ViewportWidth, layout.ViewportHeight };
}

void ScrollView::PerformScrollContentLayout()
{
	constexpr float scrollBarThickness = 8.0f;
	auto performLayoutPass = [&](bool reserveVerticalScrollBar, bool reserveHorizontalScrollBar)
		{
			SIZE containerSize = this->Size;
			Thickness padding = this->Padding;
			float contentLeft = padding.Left;
			float contentTop = padding.Top;
			float contentWidth = (float)containerSize.cx - padding.Left - padding.Right - (reserveVerticalScrollBar ? scrollBarThickness : 0.0f);
			float contentHeight = (float)containerSize.cy - padding.Top - padding.Bottom - (reserveHorizontalScrollBar ? scrollBarThickness : 0.0f);
			if (contentWidth < 0) contentWidth = 0;
			if (contentHeight < 0) contentHeight = 0;

			if (this->_layoutEngine)
			{
				SIZE availableSize = SIZE{ (LONG)contentWidth, (LONG)contentHeight };
				this->_layoutEngine->Measure(this, availableSize);

				D2D1_RECT_F finalRect = {
					padding.Left,
					padding.Top,
					padding.Left + (float)availableSize.cx,
					padding.Top + (float)availableSize.cy
				};
				this->_layoutEngine->Arrange(this, finalRect);
				return;
			}

			for (size_t i = 0; i < this->Children.size(); i++)
			{
				auto child = this->Children[i];
				if (!child || !child->Visible) continue;

				POINT childLocation = child->Location;
				Thickness childMargin = child->Margin;
				uint8_t anchorStyles = child->AnchorStyles;
				HorizontalAlignment horizontalAlignment = child->HAlign;
				VerticalAlignment verticalAlignment = child->VAlign;
				SIZE measuredSize = child->MeasureCore({ INT_MAX, INT_MAX });

				float finalX = contentLeft + childMargin.Left;
				float finalY = contentTop + childMargin.Top;
				float finalWidth = (float)measuredSize.cx;
				float finalHeight = (float)measuredSize.cy;

				if (anchorStyles != AnchorStyles::None)
				{
					if ((anchorStyles & AnchorStyles::Left) && (anchorStyles & AnchorStyles::Right))
					{
						finalX = contentLeft + (float)childLocation.x;
						finalWidth = contentWidth - (float)childLocation.x - childMargin.Right;
						if (finalWidth < 0) finalWidth = 0;
					}
					else if (anchorStyles & AnchorStyles::Right)
					{
						finalX = contentLeft + contentWidth - childMargin.Right - finalWidth;
					}
					else
					{
						finalX = contentLeft + (float)childLocation.x;
					}

					if ((anchorStyles & AnchorStyles::Top) && (anchorStyles & AnchorStyles::Bottom))
					{
						finalY = contentTop + (float)childLocation.y;
						finalHeight = contentHeight - (float)childLocation.y - childMargin.Bottom;
						if (finalHeight < 0) finalHeight = 0;
					}
					else if (anchorStyles & AnchorStyles::Bottom)
					{
						finalY = contentTop + contentHeight - childMargin.Bottom - finalHeight;
					}
					else
					{
						finalY = contentTop + (float)childLocation.y;
					}
				}
				else
				{
					if (horizontalAlignment == HorizontalAlignment::Stretch)
					{
						finalX = contentLeft + childMargin.Left;
						finalWidth = contentWidth - childMargin.Left - childMargin.Right;
					}
					else if (horizontalAlignment == HorizontalAlignment::Center)
					{
						float availableWidth = contentWidth - childMargin.Left - childMargin.Right;
						if (availableWidth < 0) availableWidth = 0;
						finalX = contentLeft + childMargin.Left + (availableWidth - finalWidth) / 2.0f;
					}
					else if (horizontalAlignment == HorizontalAlignment::Right)
					{
						finalX = contentLeft + contentWidth - childMargin.Right - finalWidth;
					}
					else
					{
						finalX = contentLeft + (float)childLocation.x;
					}

					if (verticalAlignment == VerticalAlignment::Stretch)
					{
						finalY = contentTop + childMargin.Top;
						finalHeight = contentHeight - childMargin.Top - childMargin.Bottom;
					}
					else if (verticalAlignment == VerticalAlignment::Top)
					{
						finalY = contentTop + (float)childLocation.y;
					}
					else if (verticalAlignment == VerticalAlignment::Center)
					{
						float availableHeight = contentHeight - childMargin.Top - childMargin.Bottom;
						if (availableHeight < 0) availableHeight = 0;
						finalY = contentTop + childMargin.Top + (availableHeight - finalHeight) / 2.0f;
					}
					else if (verticalAlignment == VerticalAlignment::Bottom)
					{
						finalY = contentTop + contentHeight - childMargin.Bottom - finalHeight;
					}
				}

				if (finalWidth < 0) finalWidth = 0;
				if (finalHeight < 0) finalHeight = 0;

				POINT finalLocation = { (LONG)finalX, (LONG)finalY };
				SIZE finalSize = { (LONG)finalWidth, (LONG)finalHeight };
				child->ApplyLayout(finalLocation, finalSize);
			}
		};

	bool needsVerticalScroll = this->AlwaysShowVScroll;
	bool needsHorizontalScroll = this->AlwaysShowHScroll;
	for (int iter = 0; iter < 3; ++iter)
	{
		performLayoutPass(needsVerticalScroll, needsHorizontalScroll);

		SIZE content = this->AutoContentSize ? MeasureContentSize() : this->ContentSize;
		content.cx = std::max<LONG>(0, content.cx);
		content.cy = std::max<LONG>(0, content.cy);

		float viewportWidth = std::max(0.0f, (float)this->Width - (needsVerticalScroll ? scrollBarThickness : 0.0f));
		float viewportHeight = std::max(0.0f, (float)this->Height - (needsHorizontalScroll ? scrollBarThickness : 0.0f));
		bool nextNeedsHorizontalScroll = this->AlwaysShowHScroll || ((float)content.cx > viewportWidth);
		bool nextNeedsVerticalScroll = this->AlwaysShowVScroll || ((float)content.cy > viewportHeight);
		if (nextNeedsHorizontalScroll == needsHorizontalScroll && nextNeedsVerticalScroll == needsVerticalScroll)
		{
			break;
		}

		needsHorizontalScroll = nextNeedsHorizontalScroll;
		needsVerticalScroll = nextNeedsVerticalScroll;
	}

	this->_needsLayout = false;
}

SIZE ScrollView::MeasureContentSize()
{
	SIZE measured{};
	float maxRight = this->_padding.Left;
	float maxBottom = this->_padding.Top;
	for (size_t i = 0; i < this->Children.size(); i++)
	{
		auto child = this->Children[i];
		if (!child || !child->Visible) continue;
		auto childLocation = child->ActualLocation;
		auto childSize = child->ActualSize();
		maxRight = std::max(maxRight, (float)childLocation.x + (float)childSize.cx);
		maxBottom = std::max(maxBottom, (float)childLocation.y + (float)childSize.cy);
	}

	measured.cx = std::max<LONG>(0, (LONG)std::ceil(maxRight + this->_padding.Right));
	measured.cy = std::max<LONG>(0, (LONG)std::ceil(maxBottom + this->_padding.Bottom));
	return measured;
}

ScrollView::ScrollLayout ScrollView::CalcScrollLayout()
{
	ScrollLayout layout{};
	layout.ScrollBarThickness = 8.0f;

	SIZE content = this->AutoContentSize ? MeasureContentSize() : this->ContentSize;
	content.cx = std::max<LONG>(0, content.cx);
	content.cy = std::max<LONG>(0, content.cy);

	bool needsVerticalScroll = this->AlwaysShowVScroll;
	bool needsHorizontalScroll = this->AlwaysShowHScroll;
	for (int iter = 0; iter < 3; ++iter)
	{
		float viewportWidth = (float)this->Width - (needsVerticalScroll ? layout.ScrollBarThickness : 0.0f);
		float viewportHeight = (float)this->Height - (needsHorizontalScroll ? layout.ScrollBarThickness : 0.0f);
		if (viewportWidth < 0.0f) viewportWidth = 0.0f;
		if (viewportHeight < 0.0f) viewportHeight = 0.0f;

		bool nextNeedsHorizontalScroll = this->AlwaysShowHScroll || ((float)content.cx > viewportWidth);
		bool nextNeedsVerticalScroll = this->AlwaysShowVScroll || ((float)content.cy > viewportHeight);
		if (nextNeedsHorizontalScroll == needsHorizontalScroll && nextNeedsVerticalScroll == needsVerticalScroll)
		{
			layout.HasHorizontalScroll = needsHorizontalScroll;
			layout.HasVerticalScroll = needsVerticalScroll;
			layout.ViewportWidth = viewportWidth;
			layout.ViewportHeight = viewportHeight;
			layout.ContentWidth = (float)content.cx;
			layout.ContentHeight = (float)content.cy;
			layout.MaxScrollX = std::max(0.0f, layout.ContentWidth - viewportWidth);
			layout.MaxScrollY = std::max(0.0f, layout.ContentHeight - viewportHeight);
			return layout;
		}
		needsHorizontalScroll = nextNeedsHorizontalScroll;
		needsVerticalScroll = nextNeedsVerticalScroll;
	}

	layout.HasHorizontalScroll = needsHorizontalScroll;
	layout.HasVerticalScroll = needsVerticalScroll;
	layout.ViewportWidth = std::max(0.0f, (float)this->Width - (needsVerticalScroll ? layout.ScrollBarThickness : 0.0f));
	layout.ViewportHeight = std::max(0.0f, (float)this->Height - (needsHorizontalScroll ? layout.ScrollBarThickness : 0.0f));
	layout.ContentWidth = (float)content.cx;
	layout.ContentHeight = (float)content.cy;
	layout.MaxScrollX = std::max(0.0f, layout.ContentWidth - layout.ViewportWidth);
	layout.MaxScrollY = std::max(0.0f, layout.ContentHeight - layout.ViewportHeight);
	return layout;
}

void ScrollView::ClampScrollOffsets(const ScrollLayout& layout)
{
	int oldX = this->ScrollXOffset;
	int oldY = this->ScrollYOffset;
	this->ScrollXOffset = std::clamp(this->ScrollXOffset, 0, (int)std::ceil(layout.MaxScrollX));
	this->ScrollYOffset = std::clamp(this->ScrollYOffset, 0, (int)std::ceil(layout.MaxScrollY));
	if (oldX != this->ScrollXOffset || oldY != this->ScrollYOffset)
	{
		this->OnScrollChanged(this);
	}
}

void ScrollView::ScrollBy(int deltaX, int deltaY)
{
	SetScrollOffset(this->ScrollXOffset + deltaX, this->ScrollYOffset + deltaY);
}

void ScrollView::SetScrollOffset(int offsetX, int offsetY)
{
	auto layout = this->CalcScrollLayout();
	int newX = std::clamp(offsetX, 0, (int)std::ceil(layout.MaxScrollX));
	int newY = std::clamp(offsetY, 0, (int)std::ceil(layout.MaxScrollY));
	if (newX == this->ScrollXOffset && newY == this->ScrollYOffset)
		return;
	this->ScrollXOffset = newX;
	this->ScrollYOffset = newY;
	this->OnScrollChanged(this);
	this->InvalidateVisual();
}

bool ScrollView::HitVerticalScrollBar(int localX, int localY, const ScrollLayout& layout) const
{
	if (!layout.HasVerticalScroll) return false;
	return localX >= (int)layout.ViewportWidth && localX < this->_size.cx && localY >= 0 && localY < (int)layout.ViewportHeight;
}

bool ScrollView::HitHorizontalScrollBar(int localX, int localY, const ScrollLayout& layout) const
{
	if (!layout.HasHorizontalScroll) return false;
	return localY >= (int)layout.ViewportHeight && localY < this->_size.cy && localX >= 0 && localX < (int)layout.ViewportWidth;
}

CursorKind ScrollView::QueryCursor(int localX, int localY)
{
	if (!this->Enable) return CursorKind::Arrow;
	auto layout = this->CalcScrollLayout();
	if (HitVerticalScrollBar(localX, localY, layout)) return CursorKind::SizeNS;
	if (HitHorizontalScrollBar(localX, localY, layout)) return CursorKind::SizeWE;
	return this->Cursor;
}

bool ScrollView::ShouldHitTestChildrenAt(int localX, int localY) const
{
	if (!this->HitTestChildren()) return false;
	auto layout = const_cast<ScrollView*>(this)->CalcScrollLayout();
	if (localX < 0 || localY < 0) return false;
	if (localX >= (int)layout.ViewportWidth || localY >= (int)layout.ViewportHeight) return false;
	return true;
}

bool ScrollView::HitChild(Control* child, int localX, int localY, int& childX, int& childY) const
{
	if (!child || !child->Visible || !child->Enable) return false;
	auto childLocation = child->ActualLocation;
	auto childSize = child->ActualSize();
	int drawX = childLocation.x - this->ScrollXOffset;
	int drawY = childLocation.y - this->ScrollYOffset;
	if (localX < drawX || localY < drawY || localX > drawX + childSize.cx || localY > drawY + childSize.cy)
		return false;
	childX = localX - drawX;
	childY = localY - drawY;
	return true;
}

void ScrollView::DrawScrollBars(const ScrollLayout& layout)
{
	auto d2d = this->ParentForm->Render;
	if (layout.HasVerticalScroll && layout.ViewportHeight > 0.0f && layout.ContentHeight > layout.ViewportHeight)
	{
		float thumbH = (layout.ViewportHeight * layout.ViewportHeight) / layout.ContentHeight;
		float minThumbH = std::max(16.0f, layout.ViewportHeight * 0.1f);
		thumbH = std::clamp(thumbH, minThumbH, layout.ViewportHeight);
		float moveSpace = std::max(0.0f, layout.ViewportHeight - thumbH);
		float per = (layout.MaxScrollY > 0.0f) ? std::clamp((float)this->ScrollYOffset / layout.MaxScrollY, 0.0f, 1.0f) : 0.0f;
		float thumbTop = per * moveSpace;
		d2d->FillRoundRect(layout.ViewportWidth, 0.0f, layout.ScrollBarThickness, layout.ViewportHeight, this->ScrollBackColor, 4.0f);
		d2d->FillRoundRect(layout.ViewportWidth, thumbTop, layout.ScrollBarThickness, thumbH, this->ScrollForeColor, 4.0f);
	}

	if (layout.HasHorizontalScroll && layout.ViewportWidth > 0.0f && layout.ContentWidth > layout.ViewportWidth)
	{
		float thumbW = (layout.ViewportWidth * layout.ViewportWidth) / layout.ContentWidth;
		float minThumbW = std::max(16.0f, layout.ViewportWidth * 0.1f);
		thumbW = std::clamp(thumbW, minThumbW, layout.ViewportWidth);
		float moveSpace = std::max(0.0f, layout.ViewportWidth - thumbW);
		float per = (layout.MaxScrollX > 0.0f) ? std::clamp((float)this->ScrollXOffset / layout.MaxScrollX, 0.0f, 1.0f) : 0.0f;
		float thumbLeft = per * moveSpace;
		d2d->FillRoundRect(0.0f, layout.ViewportHeight, layout.ViewportWidth, layout.ScrollBarThickness, this->ScrollBackColor, 4.0f);
		d2d->FillRoundRect(thumbLeft, layout.ViewportHeight, thumbW, layout.ScrollBarThickness, this->ScrollForeColor, 4.0f);
	}

	if (layout.HasHorizontalScroll && layout.HasVerticalScroll)
	{
		d2d->FillRect(layout.ViewportWidth, layout.ViewportHeight, layout.ScrollBarThickness, layout.ScrollBarThickness, this->ScrollBackColor);
	}
}

void ScrollView::UpdateVerticalScrollByThumb(float localY, const ScrollLayout& layout)
{
	if (!layout.HasVerticalScroll || layout.ContentHeight <= layout.ViewportHeight || layout.ViewportHeight <= 0.0f)
		return;
	float thumbH = (layout.ViewportHeight * layout.ViewportHeight) / layout.ContentHeight;
	float minThumbH = std::max(16.0f, layout.ViewportHeight * 0.1f);
	thumbH = std::clamp(thumbH, minThumbH, layout.ViewportHeight);
	float moveSpace = std::max(0.0f, layout.ViewportHeight - thumbH);
	if (moveSpace <= 0.0f) return;
	float grab = std::clamp(this->_verticalScrollThumbGrabOffset, 0.0f, thumbH);
	if (grab <= 0.0f) grab = thumbH * 0.5f;
	float target = std::clamp(localY - grab, 0.0f, moveSpace);
	float per = target / moveSpace;
	SetScrollOffset(this->ScrollXOffset, (int)std::lround(per * layout.MaxScrollY));
}

void ScrollView::UpdateHorizontalScrollByThumb(float localX, const ScrollLayout& layout)
{
	if (!layout.HasHorizontalScroll || layout.ContentWidth <= layout.ViewportWidth || layout.ViewportWidth <= 0.0f)
		return;
	float thumbW = (layout.ViewportWidth * layout.ViewportWidth) / layout.ContentWidth;
	float minThumbW = std::max(16.0f, layout.ViewportWidth * 0.1f);
	thumbW = std::clamp(thumbW, minThumbW, layout.ViewportWidth);
	float moveSpace = std::max(0.0f, layout.ViewportWidth - thumbW);
	if (moveSpace <= 0.0f) return;
	float grab = std::clamp(this->_horizontalScrollThumbGrabOffset, 0.0f, thumbW);
	if (grab <= 0.0f) grab = thumbW * 0.5f;
	float target = std::clamp(localX - grab, 0.0f, moveSpace);
	float per = target / moveSpace;
	SetScrollOffset((int)std::lround(per * layout.MaxScrollX), this->ScrollYOffset);
}

void ScrollView::Update()
{
	if (this->IsVisual == false) return;
	if (_needsLayout || (_layoutEngine && _layoutEngine->NeedsLayout()))
	{
		PerformScrollContentLayout();
	}

	auto d2d = this->ParentForm->Render;
	auto size = this->ActualSize();
	const float actualWidth = static_cast<float>(size.cx);
	const float actualHeight = static_cast<float>(size.cy);
	const float border = (std::max)(0.0f, this->BorderThickness);
	const float radius = (std::clamp)(this->CornerRadius, 0.0f, (std::min)(actualWidth, actualHeight) * 0.5f);
	auto layout = this->CalcScrollLayout();
	ClampScrollOffsets(layout);

	this->BeginRender();
	{
		if (radius > 0.0f)
			d2d->FillRoundRect(0, 0, actualWidth, actualHeight, this->BackColor, radius);
		else
			d2d->FillRect(0, 0, actualWidth, actualHeight, this->BackColor);
		if (this->Image)
		{
			this->RenderImage(radius);
		}

		d2d->PushDrawRect(0.0f, 0.0f, layout.ViewportWidth, layout.ViewportHeight);
		if (!this->ParentForm || !this->ParentForm->IsDCompSceneRenderActive())
		{
			for (auto c : this->GetChildrenInZOrder())
			{
				if (!c || !c->Visible) continue;
				c->Update();
			}
		}
		d2d->PopDrawRect();

		DrawScrollBars(layout);
		if (border > 0.0f && this->BorderColor.a > 0.0f)
		{
			const float drawW = (std::max)(0.0f, actualWidth - border);
			const float drawH = (std::max)(0.0f, actualHeight - border);
			if (radius > 0.0f)
				d2d->DrawRoundRect(border * 0.5f, border * 0.5f, drawW, drawH, this->BorderColor, border, radius);
			else
				d2d->DrawRect(border * 0.5f, border * 0.5f, drawW, drawH, this->BorderColor, border);
		}
	}
	if (!this->Enable)
	{
		if (radius > 0.0f)
			d2d->FillRoundRect(0, 0, actualWidth, actualHeight, DisabledOverlayColor, radius);
		else
			d2d->FillRect(0, 0, actualWidth, actualHeight, DisabledOverlayColor);
	}
	this->EndRender();
}

bool ScrollView::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY)
{
	if (!this->Enable || !this->Visible) return true;
	if (_needsLayout || (_layoutEngine && _layoutEngine->NeedsLayout()))
	{
		PerformScrollContentLayout();
	}

	auto layout = this->CalcScrollLayout();
	ClampScrollOffsets(layout);

	if (WM_LBUTTONDOWN == message && this->ParentForm)
	{
		this->ParentForm->SetSelectedControl(this, false);
	}

	if (_draggingVerticalScrollBar && message == WM_MOUSEMOVE)
	{
		UpdateVerticalScrollByThumb((float)localY, layout);
		return true;
	}
	if (_draggingHorizontalScrollBar && message == WM_MOUSEMOVE)
	{
		UpdateHorizontalScrollByThumb((float)localX, layout);
		return true;
	}
	if ((_draggingVerticalScrollBar || _draggingHorizontalScrollBar) && (message == WM_LBUTTONUP || message == WM_RBUTTONUP || message == WM_MBUTTONUP))
	{
		_draggingVerticalScrollBar = false;
		_draggingHorizontalScrollBar = false;
	}

	switch (message)
	{
	case WM_MOUSEWHEEL:
	{
		const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
		if (localX >= 0 && localY >= 0 && localX < (int)layout.ViewportWidth && localY < (int)layout.ViewportHeight)
		{
			Control* wheelTarget = nullptr;
			int targetLocalX = localX;
			int targetLocalY = localY;
			if (FindDeepestWheelTarget(this, localX, localY, wheelTarget, targetLocalX, targetLocalY) && wheelTarget && wheelTarget != this)
			{
				POINT viewLocation = this->AbsLocation;
				POINT mouseInForm{ viewLocation.x + localX, viewLocation.y + localY };
				for (Control* target = wheelTarget; target && target != this; target = target->Parent)
				{
					if (!target->HandlesMouseWheel()) continue;
					POINT targetLocation = target->AbsLocation;
					const int targetX = mouseInForm.x - targetLocation.x;
					const int targetY = mouseInForm.y - targetLocation.y;
					if (target->CanHandleMouseWheel(delta, targetX, targetY))
					{
						target->ProcessMessage(message, wParam, lParam, targetX, targetY);
						return true;
					}
				}
			}
		}

		if (!this->CanHandleMouseWheel(delta, localX, localY))
			return false;

		int steps = delta / WHEEL_DELTA;
		if (steps != 0)
		{
			SetScrollOffset(this->ScrollXOffset, this->ScrollYOffset - (steps * this->MouseWheelStep));
		}
		MouseEventArgs eventArgs = MouseEventArgs(MouseButtons::None, 0, localX, localY, delta);
		this->OnMouseWheel(this, eventArgs);
		return true;
	}
	case WM_LBUTTONDOWN:
	{
		if (HitVerticalScrollBar(localX, localY, layout) && layout.ContentHeight > layout.ViewportHeight)
		{
			float thumbH = (layout.ViewportHeight * layout.ViewportHeight) / layout.ContentHeight;
			float minThumbH = std::max(16.0f, layout.ViewportHeight * 0.1f);
			thumbH = std::clamp(thumbH, minThumbH, layout.ViewportHeight);
			float moveSpace = std::max(0.0f, layout.ViewportHeight - thumbH);
			float scrollRatio = (layout.MaxScrollY > 0.0f) ? std::clamp((float)this->ScrollYOffset / layout.MaxScrollY, 0.0f, 1.0f) : 0.0f;
			float thumbTop = scrollRatio * moveSpace;
			float pointerY = (float)localY;
			bool hitThumb = pointerY >= thumbTop && pointerY <= (thumbTop + thumbH);
			this->_verticalScrollThumbGrabOffset = hitThumb ? (pointerY - thumbTop) : (thumbH * 0.5f);
			this->_draggingVerticalScrollBar = true;
			UpdateVerticalScrollByThumb(pointerY, layout);
			return true;
		}
		if (HitHorizontalScrollBar(localX, localY, layout) && layout.ContentWidth > layout.ViewportWidth)
		{
			float thumbW = (layout.ViewportWidth * layout.ViewportWidth) / layout.ContentWidth;
			float minThumbW = std::max(16.0f, layout.ViewportWidth * 0.1f);
			thumbW = std::clamp(thumbW, minThumbW, layout.ViewportWidth);
			float moveSpace = std::max(0.0f, layout.ViewportWidth - thumbW);
			float scrollRatio = (layout.MaxScrollX > 0.0f) ? std::clamp((float)this->ScrollXOffset / layout.MaxScrollX, 0.0f, 1.0f) : 0.0f;
			float thumbLeft = scrollRatio * moveSpace;
			float pointerX = (float)localX;
			bool hitThumb = pointerX >= thumbLeft && pointerX <= (thumbLeft + thumbW);
			this->_horizontalScrollThumbGrabOffset = hitThumb ? (pointerX - thumbLeft) : (thumbW * 0.5f);
			this->_draggingHorizontalScrollBar = true;
			UpdateHorizontalScrollByThumb(pointerX, layout);
			return true;
		}
	}
	break;
	case WM_KEYDOWN:
	{
		const bool ctrlDown = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
		const int lineStepY = std::max(16, this->MouseWheelStep);
		const int lineStepX = std::max(16, this->MouseWheelStep);
		const int pageStepY = std::max(16, (int)layout.ViewportHeight - lineStepY);
		const int pageStepX = std::max(16, (int)layout.ViewportWidth - lineStepX);
		bool handledScroll = false;

		switch (wParam)
		{
		case VK_UP:
			if (layout.MaxScrollY > 0.0f)
			{
				SetScrollOffset(this->ScrollXOffset, this->ScrollYOffset - lineStepY);
				handledScroll = true;
			}
			break;
		case VK_DOWN:
			if (layout.MaxScrollY > 0.0f)
			{
				SetScrollOffset(this->ScrollXOffset, this->ScrollYOffset + lineStepY);
				handledScroll = true;
			}
			break;
		case VK_LEFT:
			if (layout.MaxScrollX > 0.0f)
			{
				SetScrollOffset(this->ScrollXOffset - lineStepX, this->ScrollYOffset);
				handledScroll = true;
			}
			break;
		case VK_RIGHT:
			if (layout.MaxScrollX > 0.0f)
			{
				SetScrollOffset(this->ScrollXOffset + lineStepX, this->ScrollYOffset);
				handledScroll = true;
			}
			break;
		case VK_PRIOR:
			if (layout.MaxScrollY > 0.0f)
			{
				SetScrollOffset(this->ScrollXOffset, this->ScrollYOffset - pageStepY);
				handledScroll = true;
			}
			break;
		case VK_NEXT:
			if (layout.MaxScrollY > 0.0f)
			{
				SetScrollOffset(this->ScrollXOffset, this->ScrollYOffset + pageStepY);
				handledScroll = true;
			}
			break;
		case VK_HOME:
			if (ctrlDown)
			{
				if (layout.MaxScrollX > 0.0f || layout.MaxScrollY > 0.0f)
				{
					SetScrollOffset(0, 0);
					handledScroll = true;
				}
			}
			else if (layout.MaxScrollY > 0.0f)
			{
				SetScrollOffset(this->ScrollXOffset, 0);
				handledScroll = true;
			}
			else if (layout.MaxScrollX > 0.0f)
			{
				SetScrollOffset(0, this->ScrollYOffset);
				handledScroll = true;
			}
			break;
		case VK_END:
			if (ctrlDown)
			{
				if (layout.MaxScrollX > 0.0f || layout.MaxScrollY > 0.0f)
				{
					SetScrollOffset((int)std::ceil(layout.MaxScrollX), (int)std::ceil(layout.MaxScrollY));
					handledScroll = true;
				}
			}
			else if (layout.MaxScrollY > 0.0f)
			{
				SetScrollOffset(this->ScrollXOffset, (int)std::ceil(layout.MaxScrollY));
				handledScroll = true;
			}
			else if (layout.MaxScrollX > 0.0f)
			{
				SetScrollOffset((int)std::ceil(layout.MaxScrollX), this->ScrollYOffset);
				handledScroll = true;
			}
			break;
		}

		if (handledScroll)
		{
			KeyEventArgs eventArgs = KeyEventArgs((Keys)(wParam | 0));
			this->OnKeyDown(this, eventArgs);
			return true;
		}
	}
	break;
	}

	if (localX >= 0 && localY >= 0 && localX < (int)layout.ViewportWidth && localY < (int)layout.ViewportHeight)
	{
		for (auto child : this->GetChildrenInReverseZOrder())
		{
			int childX = 0;
			int childY = 0;
			if (!HitChild(child, localX, localY, childX, childY)) continue;
			child->ProcessMessage(message, wParam, lParam, childX, childY);
			break;
		}
	}

	switch (message)
	{
	case WM_DROPFILES:
	{
		HDROP hDropInfo = HDROP(wParam);
		UINT fileCount = DragQueryFile(hDropInfo, 0xffffffff, nullptr, 0);
		TCHAR fileName[MAX_PATH];
		std::vector<std::wstring> files;
		for (int fileIndex = 0; fileIndex < (int)fileCount; fileIndex++)
		{
			DragQueryFile(hDropInfo, fileIndex, fileName, MAX_PATH);
			files.push_back(fileName);
		}
		DragFinish(hDropInfo);
		if (files.size() > 0)
		{
			this->OnDropFile(this, files);
		}
	}
	break;
	case WM_MOUSEMOVE:
	{
		MouseEventArgs eventArgs = MouseEventArgs(MouseButtons::None, 0, localX, localY, HIWORD(wParam));
		this->OnMouseMove(this, eventArgs);
	}
	break;
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_MBUTTONDOWN:
	{
		MouseEventArgs eventArgs = MouseEventArgs(FromParamToMouseButtons(message), 0, localX, localY, HIWORD(wParam));
		this->OnMouseDown(this, eventArgs);
	}
	break;
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
	case WM_MBUTTONUP:
	{
		MouseEventArgs eventArgs = MouseEventArgs(FromParamToMouseButtons(message), 0, localX, localY, HIWORD(wParam));
		this->OnMouseUp(this, eventArgs);
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
