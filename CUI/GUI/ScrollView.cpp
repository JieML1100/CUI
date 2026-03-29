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

static bool FindDeepestWheelTarget(Control* root, int xof, int yof, Control*& outTarget, int& outX, int& outY)
{
	if (!root || !root->Visible || !root->Enable) return false;
	if (!root->ShouldHitTestChildrenAt(xof, yof))
	{
		outTarget = root;
		outX = xof;
		outY = yof;
		return true;
	}

	auto childOffset = root->GetChildrenRenderOffset();
	for (int i = root->Count - 1; i >= 0; --i)
	{
		auto child = root->operator[](i);
		if (!child || !child->Visible || !child->Enable) continue;
		auto loc = child->ActualLocation;
		auto size = child->ActualSize();
		const int drawX = loc.x + childOffset.x;
		const int drawY = loc.y + childOffset.y;
		if (xof < drawX || yof < drawY || xof > (drawX + size.cx) || yof > (drawY + size.cy))
			continue;
		if (FindDeepestWheelTarget(child, xof - drawX, yof - drawY, outTarget, outX, outY))
			return true;
	}

	outTarget = root;
	outX = xof;
	outY = yof;
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

void ScrollView::PerformScrollContentLayout()
{
	constexpr float scrollBarSize = 8.0f;
	auto performLayoutPass = [&](bool reserveVScroll, bool reserveHScroll)
		{
			SIZE containerSize = this->Size;
			Thickness padding = this->Padding;
			float contentLeft = padding.Left;
			float contentTop = padding.Top;
			float contentWidth = (float)containerSize.cx - padding.Left - padding.Right - (reserveVScroll ? scrollBarSize : 0.0f);
			float contentHeight = (float)containerSize.cy - padding.Top - padding.Bottom - (reserveHScroll ? scrollBarSize : 0.0f);
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

			for (int i = 0; i < this->Children.size(); i++)
			{
				auto child = this->Children[i];
				if (!child || !child->Visible) continue;

				POINT location = child->Location;
				Thickness margin = child->Margin;
				uint8_t anchor = child->AnchorStyles;
				HorizontalAlignment hAlign = child->HAlign;
				VerticalAlignment vAlign = child->VAlign;
				SIZE size = child->MeasureCore({ INT_MAX, INT_MAX });

				float x = contentLeft + margin.Left;
				float y = contentTop + margin.Top;
				float w = (float)size.cx;
				float h = (float)size.cy;

				if (anchor != AnchorStyles::None)
				{
					if ((anchor & AnchorStyles::Left) && (anchor & AnchorStyles::Right))
					{
						x = contentLeft + (float)location.x;
						w = contentWidth - (float)location.x - margin.Right;
						if (w < 0) w = 0;
					}
					else if (anchor & AnchorStyles::Right)
					{
						x = contentLeft + contentWidth - margin.Right - w;
					}
					else
					{
						x = contentLeft + (float)location.x;
					}

					if ((anchor & AnchorStyles::Top) && (anchor & AnchorStyles::Bottom))
					{
						y = contentTop + (float)location.y;
						h = contentHeight - (float)location.y - margin.Bottom;
						if (h < 0) h = 0;
					}
					else if (anchor & AnchorStyles::Bottom)
					{
						y = contentTop + contentHeight - margin.Bottom - h;
					}
					else
					{
						y = contentTop + (float)location.y;
					}
				}
				else
				{
					if (hAlign == HorizontalAlignment::Stretch)
					{
						x = contentLeft + margin.Left;
						w = contentWidth - margin.Left - margin.Right;
					}
					else if (hAlign == HorizontalAlignment::Center)
					{
						float availableWidth = contentWidth - margin.Left - margin.Right;
						if (availableWidth < 0) availableWidth = 0;
						x = contentLeft + margin.Left + (availableWidth - w) / 2.0f;
					}
					else if (hAlign == HorizontalAlignment::Right)
					{
						x = contentLeft + contentWidth - margin.Right - w;
					}
					else
					{
						x = contentLeft + (float)location.x;
					}

					if (vAlign == VerticalAlignment::Stretch)
					{
						y = contentTop + margin.Top;
						h = contentHeight - margin.Top - margin.Bottom;
					}
					else if (vAlign == VerticalAlignment::Top)
					{
						y = contentTop + (float)location.y;
					}
					else if (vAlign == VerticalAlignment::Center)
					{
						float availableHeight = contentHeight - margin.Top - margin.Bottom;
						if (availableHeight < 0) availableHeight = 0;
						y = contentTop + margin.Top + (availableHeight - h) / 2.0f;
					}
					else if (vAlign == VerticalAlignment::Bottom)
					{
						y = contentTop + contentHeight - margin.Bottom - h;
					}
				}

				if (w < 0) w = 0;
				if (h < 0) h = 0;

				POINT finalLoc = { (LONG)x, (LONG)y };
				SIZE finalSize = { (LONG)w, (LONG)h };
				child->ApplyLayout(finalLoc, finalSize);
			}
		};

	bool needV = this->AlwaysShowVScroll;
	bool needH = this->AlwaysShowHScroll;
	for (int iter = 0; iter < 3; ++iter)
	{
		performLayoutPass(needV, needH);

		SIZE content = this->AutoContentSize ? MeasureContentSize() : this->ContentSize;
		content.cx = std::max<LONG>(0, content.cx);
		content.cy = std::max<LONG>(0, content.cy);

		float viewportW = (std::max)(0.0f, (float)this->Width - (needV ? scrollBarSize : 0.0f));
		float viewportH = (std::max)(0.0f, (float)this->Height - (needH ? scrollBarSize : 0.0f));
		bool nextNeedH = this->AlwaysShowHScroll || ((float)content.cx > viewportW);
		bool nextNeedV = this->AlwaysShowVScroll || ((float)content.cy > viewportH);
		if (nextNeedH == needH && nextNeedV == needV)
		{
			break;
		}

		needH = nextNeedH;
		needV = nextNeedV;
	}

	this->_needsLayout = false;
}

SIZE ScrollView::MeasureContentSize()
{
	SIZE measured{};
	float maxRight = this->_padding.Left;
	float maxBottom = this->_padding.Top;
	for (int i = 0; i < this->Children.size(); i++)
	{
		auto child = this->Children[i];
		if (!child || !child->Visible) continue;
		auto loc = child->ActualLocation;
		auto sz = child->ActualSize();
		maxRight = (std::max)(maxRight, (float)loc.x + (float)sz.cx);
		maxBottom = (std::max)(maxBottom, (float)loc.y + (float)sz.cy);
	}

	measured.cx = std::max<LONG>(0, (LONG)std::ceil(maxRight + this->_padding.Right));
	measured.cy = std::max<LONG>(0, (LONG)std::ceil(maxBottom + this->_padding.Bottom));
	return measured;
}

ScrollView::ScrollLayout ScrollView::CalcScrollLayout()
{
	ScrollLayout layout{};
	layout.ScrollBarSize = 8.0f;

	SIZE content = this->AutoContentSize ? MeasureContentSize() : this->ContentSize;
	content.cx = std::max<LONG>(0, content.cx);
	content.cy = std::max<LONG>(0, content.cy);

	bool needV = this->AlwaysShowVScroll;
	bool needH = this->AlwaysShowHScroll;
	for (int iter = 0; iter < 3; ++iter)
	{
		float viewportW = (float)this->Width - (needV ? layout.ScrollBarSize : 0.0f);
		float viewportH = (float)this->Height - (needH ? layout.ScrollBarSize : 0.0f);
		if (viewportW < 0.0f) viewportW = 0.0f;
		if (viewportH < 0.0f) viewportH = 0.0f;

		bool nextNeedH = this->AlwaysShowHScroll || ((float)content.cx > viewportW);
		bool nextNeedV = this->AlwaysShowVScroll || ((float)content.cy > viewportH);
		if (nextNeedH == needH && nextNeedV == needV)
		{
			layout.NeedH = needH;
			layout.NeedV = needV;
			layout.ViewportWidth = viewportW;
			layout.ViewportHeight = viewportH;
			layout.ContentWidth = (float)content.cx;
			layout.ContentHeight = (float)content.cy;
			layout.MaxScrollX = (std::max)(0.0f, layout.ContentWidth - viewportW);
			layout.MaxScrollY = (std::max)(0.0f, layout.ContentHeight - viewportH);
			return layout;
		}
		needH = nextNeedH;
		needV = nextNeedV;
	}

	layout.NeedH = needH;
	layout.NeedV = needV;
	layout.ViewportWidth = (std::max)(0.0f, (float)this->Width - (needV ? layout.ScrollBarSize : 0.0f));
	layout.ViewportHeight = (std::max)(0.0f, (float)this->Height - (needH ? layout.ScrollBarSize : 0.0f));
	layout.ContentWidth = (float)content.cx;
	layout.ContentHeight = (float)content.cy;
	layout.MaxScrollX = (std::max)(0.0f, layout.ContentWidth - layout.ViewportWidth);
	layout.MaxScrollY = (std::max)(0.0f, layout.ContentHeight - layout.ViewportHeight);
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

void ScrollView::ScrollBy(int dx, int dy)
{
	SetScrollOffset(this->ScrollXOffset + dx, this->ScrollYOffset + dy);
}

void ScrollView::SetScrollOffset(int x, int y)
{
	auto layout = this->CalcScrollLayout();
	int newX = std::clamp(x, 0, (int)std::ceil(layout.MaxScrollX));
	int newY = std::clamp(y, 0, (int)std::ceil(layout.MaxScrollY));
	if (newX == this->ScrollXOffset && newY == this->ScrollYOffset)
		return;
	this->ScrollXOffset = newX;
	this->ScrollYOffset = newY;
	this->OnScrollChanged(this);
	this->PostRender();
}

bool ScrollView::HitVScrollBar(int xof, int yof, const ScrollLayout& layout) const
{
	if (!layout.NeedV) return false;
	return xof >= (int)layout.ViewportWidth && xof < this->_size.cx && yof >= 0 && yof < (int)layout.ViewportHeight;
}

bool ScrollView::HitHScrollBar(int xof, int yof, const ScrollLayout& layout) const
{
	if (!layout.NeedH) return false;
	return yof >= (int)layout.ViewportHeight && yof < this->_size.cy && xof >= 0 && xof < (int)layout.ViewportWidth;
}

CursorKind ScrollView::QueryCursor(int xof, int yof)
{
	if (!this->Enable) return CursorKind::Arrow;
	auto layout = this->CalcScrollLayout();
	if (HitVScrollBar(xof, yof, layout)) return CursorKind::SizeNS;
	if (HitHScrollBar(xof, yof, layout)) return CursorKind::SizeWE;
	return this->Cursor;
}

bool ScrollView::ShouldHitTestChildrenAt(int xof, int yof) const
{
	if (!this->HitTestChildren()) return false;
	auto layout = const_cast<ScrollView*>(this)->CalcScrollLayout();
	if (xof < 0 || yof < 0) return false;
	if (xof >= (int)layout.ViewportWidth || yof >= (int)layout.ViewportHeight) return false;
	return true;
}

bool ScrollView::HitChild(Control* child, int xof, int yof, int& childX, int& childY) const
{
	if (!child || !child->Visible || !child->Enable) return false;
	auto loc = child->ActualLocation;
	auto size = child->ActualSize();
	int drawX = loc.x - this->ScrollXOffset;
	int drawY = loc.y - this->ScrollYOffset;
	if (xof < drawX || yof < drawY || xof > drawX + size.cx || yof > drawY + size.cy)
		return false;
	childX = xof - drawX;
	childY = yof - drawY;
	return true;
}

void ScrollView::DrawScrollBars(const ScrollLayout& layout)
{
	auto d2d = this->ParentForm->Render;
	if (layout.NeedV && layout.ViewportHeight > 0.0f && layout.ContentHeight > layout.ViewportHeight)
	{
		float thumbH = (layout.ViewportHeight * layout.ViewportHeight) / layout.ContentHeight;
		float minThumbH = (std::max)(16.0f, layout.ViewportHeight * 0.1f);
		thumbH = std::clamp(thumbH, minThumbH, layout.ViewportHeight);
		float moveSpace = (std::max)(0.0f, layout.ViewportHeight - thumbH);
		float per = (layout.MaxScrollY > 0.0f) ? std::clamp((float)this->ScrollYOffset / layout.MaxScrollY, 0.0f, 1.0f) : 0.0f;
		float thumbTop = per * moveSpace;
		d2d->FillRoundRect(layout.ViewportWidth, 0.0f, layout.ScrollBarSize, layout.ViewportHeight, this->ScrollBackColor, 4.0f);
		d2d->FillRoundRect(layout.ViewportWidth, thumbTop, layout.ScrollBarSize, thumbH, this->ScrollForeColor, 4.0f);
	}

	if (layout.NeedH && layout.ViewportWidth > 0.0f && layout.ContentWidth > layout.ViewportWidth)
	{
		float thumbW = (layout.ViewportWidth * layout.ViewportWidth) / layout.ContentWidth;
		float minThumbW = (std::max)(16.0f, layout.ViewportWidth * 0.1f);
		thumbW = std::clamp(thumbW, minThumbW, layout.ViewportWidth);
		float moveSpace = (std::max)(0.0f, layout.ViewportWidth - thumbW);
		float per = (layout.MaxScrollX > 0.0f) ? std::clamp((float)this->ScrollXOffset / layout.MaxScrollX, 0.0f, 1.0f) : 0.0f;
		float thumbLeft = per * moveSpace;
		d2d->FillRoundRect(0.0f, layout.ViewportHeight, layout.ViewportWidth, layout.ScrollBarSize, this->ScrollBackColor, 4.0f);
		d2d->FillRoundRect(thumbLeft, layout.ViewportHeight, thumbW, layout.ScrollBarSize, this->ScrollForeColor, 4.0f);
	}

	if (layout.NeedH && layout.NeedV)
	{
		d2d->FillRect(layout.ViewportWidth, layout.ViewportHeight, layout.ScrollBarSize, layout.ScrollBarSize, this->ScrollBackColor);
	}
}

void ScrollView::UpdateScrollByThumbY(float localY, const ScrollLayout& layout)
{
	if (!layout.NeedV || layout.ContentHeight <= layout.ViewportHeight || layout.ViewportHeight <= 0.0f)
		return;
	float thumbH = (layout.ViewportHeight * layout.ViewportHeight) / layout.ContentHeight;
	float minThumbH = (std::max)(16.0f, layout.ViewportHeight * 0.1f);
	thumbH = std::clamp(thumbH, minThumbH, layout.ViewportHeight);
	float moveSpace = (std::max)(0.0f, layout.ViewportHeight - thumbH);
	if (moveSpace <= 0.0f) return;
	float grab = std::clamp(this->_vScrollThumbGrabOffset, 0.0f, thumbH);
	if (grab <= 0.0f) grab = thumbH * 0.5f;
	float target = std::clamp(localY - grab, 0.0f, moveSpace);
	float per = target / moveSpace;
	SetScrollOffset(this->ScrollXOffset, (int)std::lround(per * layout.MaxScrollY));
}

void ScrollView::UpdateScrollByThumbX(float localX, const ScrollLayout& layout)
{
	if (!layout.NeedH || layout.ContentWidth <= layout.ViewportWidth || layout.ViewportWidth <= 0.0f)
		return;
	float thumbW = (layout.ViewportWidth * layout.ViewportWidth) / layout.ContentWidth;
	float minThumbW = (std::max)(16.0f, layout.ViewportWidth * 0.1f);
	thumbW = std::clamp(thumbW, minThumbW, layout.ViewportWidth);
	float moveSpace = (std::max)(0.0f, layout.ViewportWidth - thumbW);
	if (moveSpace <= 0.0f) return;
	float grab = std::clamp(this->_hScrollThumbGrabOffset, 0.0f, thumbW);
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
	auto layout = this->CalcScrollLayout();
	ClampScrollOffsets(layout);

	this->BeginRender();
	{
		d2d->FillRect(0, 0, size.cx, size.cy, this->BackColor);
		if (this->Image)
		{
			this->RenderImage();
		}

		d2d->PushDrawRect(0.0f, 0.0f, layout.ViewportWidth, layout.ViewportHeight);
		for (int i = 0; i < this->Count; i++)
		{
			auto c = this->operator[](i);
			if (!c || !c->Visible) continue;
			c->Update();
		}
		d2d->PopDrawRect();

		DrawScrollBars(layout);
		d2d->DrawRect(0, 0, size.cx, size.cy, this->BolderColor, this->Boder);
	}
	if (!this->Enable)
	{
		d2d->FillRect(0, 0, size.cx, size.cy, { 1.0f ,1.0f ,1.0f ,0.5f });
	}
	this->EndRender();
}

bool ScrollView::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof)
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

	if (_dragVScroll && message == WM_MOUSEMOVE)
	{
		UpdateScrollByThumbY((float)yof, layout);
		return true;
	}
	if (_dragHScroll && message == WM_MOUSEMOVE)
	{
		UpdateScrollByThumbX((float)xof, layout);
		return true;
	}
	if ((_dragVScroll || _dragHScroll) && (message == WM_LBUTTONUP || message == WM_RBUTTONUP || message == WM_MBUTTONUP))
	{
		_dragVScroll = false;
		_dragHScroll = false;
	}

	switch (message)
	{
	case WM_MOUSEWHEEL:
	{
		if (xof >= 0 && yof >= 0 && xof < (int)layout.ViewportWidth && yof < (int)layout.ViewportHeight)
		{
			Control* wheelTarget = NULL;
			int localX = xof;
			int localY = yof;
			if (FindDeepestWheelTarget(this, xof, yof, wheelTarget, localX, localY) && wheelTarget && wheelTarget != this && wheelTarget->HandlesMouseWheel())
			{
				wheelTarget->ProcessMessage(message, wParam, lParam, localX, localY);
				return true;
			}
		}

		int steps = GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA;
		if (steps != 0 && layout.MaxScrollY > 0.0f)
		{
			SetScrollOffset(this->ScrollXOffset, this->ScrollYOffset - (steps * this->MouseWheelStep));
		}
		MouseEventArgs event_obj = MouseEventArgs(MouseButtons::None, 0, xof, yof, GET_WHEEL_DELTA_WPARAM(wParam));
		this->OnMouseWheel(this, event_obj);
		return true;
	}
	case WM_LBUTTONDOWN:
	{
		if (HitVScrollBar(xof, yof, layout) && layout.ContentHeight > layout.ViewportHeight)
		{
			float thumbH = (layout.ViewportHeight * layout.ViewportHeight) / layout.ContentHeight;
			float minThumbH = (std::max)(16.0f, layout.ViewportHeight * 0.1f);
			thumbH = std::clamp(thumbH, minThumbH, layout.ViewportHeight);
			float moveSpace = (std::max)(0.0f, layout.ViewportHeight - thumbH);
			float per = (layout.MaxScrollY > 0.0f) ? std::clamp((float)this->ScrollYOffset / layout.MaxScrollY, 0.0f, 1.0f) : 0.0f;
			float thumbTop = per * moveSpace;
			float localY = (float)yof;
			bool hitThumb = localY >= thumbTop && localY <= (thumbTop + thumbH);
			this->_vScrollThumbGrabOffset = hitThumb ? (localY - thumbTop) : (thumbH * 0.5f);
			this->_dragVScroll = true;
			UpdateScrollByThumbY(localY, layout);
			return true;
		}
		if (HitHScrollBar(xof, yof, layout) && layout.ContentWidth > layout.ViewportWidth)
		{
			float thumbW = (layout.ViewportWidth * layout.ViewportWidth) / layout.ContentWidth;
			float minThumbW = (std::max)(16.0f, layout.ViewportWidth * 0.1f);
			thumbW = std::clamp(thumbW, minThumbW, layout.ViewportWidth);
			float moveSpace = (std::max)(0.0f, layout.ViewportWidth - thumbW);
			float per = (layout.MaxScrollX > 0.0f) ? std::clamp((float)this->ScrollXOffset / layout.MaxScrollX, 0.0f, 1.0f) : 0.0f;
			float thumbLeft = per * moveSpace;
			float localX = (float)xof;
			bool hitThumb = localX >= thumbLeft && localX <= (thumbLeft + thumbW);
			this->_hScrollThumbGrabOffset = hitThumb ? (localX - thumbLeft) : (thumbW * 0.5f);
			this->_dragHScroll = true;
			UpdateScrollByThumbX(localX, layout);
			return true;
		}
	}
	break;
	case WM_KEYDOWN:
	{
		const bool ctrlDown = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
		const int lineStepY = (std::max)(16, this->MouseWheelStep);
		const int lineStepX = (std::max)(16, this->MouseWheelStep);
		const int pageStepY = (std::max)(16, (int)layout.ViewportHeight - lineStepY);
		const int pageStepX = (std::max)(16, (int)layout.ViewportWidth - lineStepX);
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
			KeyEventArgs event_obj = KeyEventArgs((Keys)(wParam | 0));
			this->OnKeyDown(this, event_obj);
			return true;
		}
	}
	break;
	}

	if (xof >= 0 && yof >= 0 && xof < (int)layout.ViewportWidth && yof < (int)layout.ViewportHeight)
	{
		for (int i = this->Count - 1; i >= 0; --i)
		{
			auto child = this->operator[](i);
			int childX = 0;
			int childY = 0;
			if (!HitChild(child, xof, yof, childX, childY)) continue;
			child->ProcessMessage(message, wParam, lParam, childX, childY);
			break;
		}
	}

	switch (message)
	{
	case WM_DROPFILES:
	{
		HDROP hDropInfo = HDROP(wParam);
		UINT uFileNum = DragQueryFile(hDropInfo, 0xffffffff, NULL, 0);
		TCHAR strFileName[MAX_PATH];
		std::vector<std::wstring> files;
		for (int i = 0; i < (int)uFileNum; i++)
		{
			DragQueryFile(hDropInfo, i, strFileName, MAX_PATH);
			files.push_back(strFileName);
		}
		DragFinish(hDropInfo);
		if (!files.empty())
		{
			this->OnDropFile(this, files);
		}
	}
	break;
	case WM_MOUSEMOVE:
	{
		MouseEventArgs event_obj = MouseEventArgs(MouseButtons::None, 0, xof, yof, HIWORD(wParam));
		this->OnMouseMove(this, event_obj);
	}
	break;
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_MBUTTONDOWN:
	{
		MouseEventArgs event_obj = MouseEventArgs(FromParamToMouseButtons(message), 0, xof, yof, HIWORD(wParam));
		this->OnMouseDown(this, event_obj);
	}
	break;
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
	case WM_MBUTTONUP:
	{
		MouseEventArgs event_obj = MouseEventArgs(FromParamToMouseButtons(message), 0, xof, yof, HIWORD(wParam));
		this->OnMouseUp(this, event_obj);
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
