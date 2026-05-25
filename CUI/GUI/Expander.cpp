#define NOMINMAX
#include "Expander.h"
#include "Form.h"

#include <algorithm>
#include <cmath>

namespace
{
	float RectWidth(const D2D1_RECT_F& rect)
	{
		return rect.right - rect.left;
	}

	float RectHeight(const D2D1_RECT_F& rect)
	{
		return rect.bottom - rect.top;
	}

	float TextTop(Font* font, const D2D1_RECT_F& rect)
	{
		const float fontHeight = font ? font->FontHeight : 16.0f;
		return rect.top + (std::max)(0.0f, (RectHeight(rect) - fontHeight) * 0.5f);
	}

	D2D1_COLOR_F ScaleAlpha(D2D1_COLOR_F color, float scale)
	{
		color.a *= (std::clamp)(scale, 0.0f, 1.0f);
		return color;
	}

	D2D1_POINT_2F RotateAround(const D2D1_POINT_2F& point, float cx, float cy, float angle)
	{
		const float dx = point.x - cx;
		const float dy = point.y - cy;
		const float s = std::sin(angle);
		const float c = std::cos(angle);
		return D2D1::Point2F(cx + dx * c - dy * s, cy + dx * s + dy * c);
	}

	void DrawExpanderChevron(D2DGraphics* d2d, float cx, float cy, float size, float progress, D2D1_COLOR_F color)
	{
		if (!d2d) return;
		progress = (std::clamp)(progress, 0.0f, 1.0f);
		const float angle = progress * 1.57079632679f;
		const float halfW = size * 0.28f;
		const float halfH = size * 0.46f;
		D2D1_POINT_2F p1 = D2D1::Point2F(cx - halfW, cy - halfH);
		D2D1_POINT_2F p2 = D2D1::Point2F(cx + halfW, cy);
		D2D1_POINT_2F p3 = D2D1::Point2F(cx - halfW, cy + halfH);
		p1 = RotateAround(p1, cx, cy, angle);
		p2 = RotateAround(p2, cx, cy, angle);
		p3 = RotateAround(p3, cx, cy, angle);
		d2d->DrawLine(p1, p2, color, 1.8f);
		d2d->DrawLine(p2, p3, color, 1.8f);
	}
}

UIClass Expander::Type()
{
	return UIClass::UI_Expander;
}

Expander::Expander()
	: Panel()
{
	this->Text = L"Expander";
	this->BackColor = D2D1_COLOR_F{ 0, 0, 0, 0 };
	this->BorderColor = D2D1_COLOR_F{ 0.55f, 0.60f, 0.68f, 0.58f };
	this->ForeColor = Colors::Black;
	this->Cursor = CursorKind::Arrow;
	this->OnTextChanged += [this](Control* sender, std::wstring oldText, std::wstring newText)
		{
			(void)sender;
			(void)oldText;
			(void)newText;
			InvalidateVisual();
		};
}

Expander::Expander(std::wstring text, int x, int y, int width, int height)
	: Expander()
{
	this->Text = text;
	this->Location = POINT{ x, y };
	this->Size = SIZE{ width, height };
}

GET_CPP(Expander, bool, IsExpanded)
{
	return _isExpanded;
}

SET_CPP(Expander, bool, IsExpanded)
{
	SetExpandedInternal(value, true);
}

float Expander::CurrentExpandProgress()
{
	if (!_animating)
	{
		_expandProgress = _isExpanded ? 1.0f : 0.0f;
		return _expandProgress;
	}

	ULONGLONG currentTick = ::GetTickCount64();
	ULONGLONG elapsedMs = currentTick >= _animStartTick ? currentTick - _animStartTick : 0;
	float normalizedTime = AnimationDurationMs > 0 ? (float)elapsedMs / (float)AnimationDurationMs : 1.0f;
	if (normalizedTime >= 1.0f)
	{
		_expandProgress = _animTargetProgress;
		_animating = false;
		return _expandProgress;
	}

	normalizedTime = 1.0f - std::pow(1.0f - (std::clamp)(normalizedTime, 0.0f, 1.0f), 3.0f);
	_expandProgress = _animStartProgress + (_animTargetProgress - _animStartProgress) * normalizedTime;
	return _expandProgress;
}

void Expander::PerformExpanderLayoutIfNeeded()
{
	if (!_needsLayout && !(_layoutEngine && _layoutEngine->NeedsLayout()))
		return;
	Thickness originalPadding = this->Padding;
	_padding.Top += HeaderHeight;
	PerformLayout();
	_padding = originalPadding;
}

bool Expander::HeaderHitTest(int localX, int localY) const
{
	return localX >= 0 && localY >= 0 && localX <= _size.cx && localY <= HeaderHeight;
}

void Expander::SetExpandedInternal(bool value, bool fireEvent)
{
	CurrentExpandProgress();
	if (_isExpanded == value && !_animating)
		return;
	_isExpanded = value;
	_animStartProgress = _expandProgress;
	_animTargetProgress = _isExpanded ? 1.0f : 0.0f;
	if (std::fabs(_animTargetProgress - _animStartProgress) <= 0.001f)
	{
		_expandProgress = _animTargetProgress;
		_animating = false;
	}
	else
	{
		_animStartTick = ::GetTickCount64();
		_animating = true;
	}
	if (!_isExpanded && ParentForm && ParentForm->Selected && ParentForm->Selected->Parent == this)
		ParentForm->SetSelectedControl(nullptr, false);
	if (ParentForm)
		ParentForm->Invalidate(true);
	InvalidateVisual();
	if (fireEvent)
		OnExpandedChanged(this, _isExpanded);
}

void Expander::SetExpanded(bool value)
{
	SetExpandedInternal(value, true);
}

void Expander::Toggle()
{
	SetExpandedInternal(!_isExpanded, true);
}

SIZE Expander::ActualSize()
{
	SIZE size = this->_size;
	float headerHeight = (std::clamp)(HeaderHeight, 0.0f, (float)size.cy);
	float contentHeight = (std::max)(0.0f, (float)size.cy - headerHeight);
	size.cy = (LONG)std::ceil(headerHeight + contentHeight * CurrentExpandProgress());
	return size;
}

CursorKind Expander::QueryCursor(int localX, int localY)
{
	if (!Enable) return CursorKind::Arrow;
	if (HeaderHitTest(localX, localY))
		return CursorKind::Hand;
	return this->Cursor;
}

bool Expander::ShouldHitTestChildrenAt(int localX, int localY) const
{
	if (!HitTestChildren())
		return false;
	if (localX < 0 || localX > _size.cx)
		return false;
	const float progress = const_cast<Expander*>(this)->CurrentExpandProgress();
	const float headerHeight = (std::clamp)(HeaderHeight, 0.0f, (float)_size.cy);
	const float visibleContentHeight = (std::max)(0.0f, ((float)_size.cy - headerHeight) * progress);
	return localY >= (int)std::floor(headerHeight) && localY <= (int)std::ceil(headerHeight + visibleContentHeight);
}

D2D1_RECT_F Expander::GetChildrenClipRect()
{
	const float progress = CurrentExpandProgress();
	const float headerHeight = (std::clamp)(HeaderHeight, 0.0f, (float)_size.cy);
	const float visibleContentHeight = (std::max)(0.0f, ((float)_size.cy - headerHeight) * progress);
	return D2D1::RectF(0.0f, headerHeight, (float)_size.cx, headerHeight + visibleContentHeight);
}

bool Expander::HandlesNavigationKey(WPARAM key) const
{
	return key == VK_RETURN || key == VK_SPACE;
}

bool Expander::IsAnimationRunning()
{
	CurrentExpandProgress();
	return _animating;
}

bool Expander::GetAnimatedInvalidRect(D2D1_RECT_F& outRect)
{
	if (!_animating)
		return false;
	outRect = this->AbsRect;
	outRect.bottom = outRect.top + (float)this->_size.cy;
	return true;
}

void Expander::Update()
{
	if (this->IsVisual == false) return;
	PerformExpanderLayoutIfNeeded();
	auto d2d = this->ParentForm ? this->ParentForm->Render : nullptr;
	if (!d2d) return;

	const float progress = CurrentExpandProgress();
	auto size = this->ActualSize();
	const float width = (float)size.cx;
	const float height = (float)size.cy;
	const float fullHeight = (float)this->_size.cy;
	const float headerHeight = (std::clamp)(HeaderHeight, 0.0f, fullHeight);
	const float border = (std::max)(0.0f, Border);
	const float radius = (std::clamp)(CornerRadius, 0.0f, (std::min)(width, (std::max)(headerHeight, height)) * 0.5f);

	this->BeginRender(width, height);
	{
		D2D1_COLOR_F surface = SurfaceColor.a > 0.0f ? SurfaceColor : this->BackColor;
		d2d->FillRoundRect(0.0f, 0.0f, width, height, surface, radius);
		if (ContentBackColor.a > 0.0f && height > headerHeight)
			d2d->FillRect(0.0f, headerHeight, width, height - headerHeight, ContentBackColor);
		d2d->FillRoundRect(0.0f, 0.0f, width, (std::min)(headerHeight, height), HeaderBackColor, radius);
		if (_hoverHeader)
			d2d->FillRoundRect(1.0f, 1.0f, (std::max)(0.0f, width - 2.0f), (std::max)(0.0f, headerHeight - 2.0f),
				HeaderHoverBackColor, (std::max)(0.0f, radius - 1.0f));

		const float chevronCenterX = HeaderPaddingX + ChevronSize * 0.5f;
		const float chevronCenterY = headerHeight * 0.5f;
		DrawExpanderChevron(d2d, chevronCenterX, chevronCenterY, ChevronSize, progress, ForeColor);

		D2D1_RECT_F textRect{
			HeaderPaddingX + ChevronSize + 9.0f,
			0.0f,
			(std::max)(HeaderPaddingX + ChevronSize + 9.0f, width - HeaderPaddingX),
			headerHeight
		};
		d2d->PushDrawRect(textRect.left, textRect.top, (std::max)(1.0f, RectWidth(textRect)), RectHeight(textRect));
		d2d->DrawString(this->Text, textRect.left, TextTop(Font, textRect),
			(std::max)(1.0f, RectWidth(textRect)), RectHeight(textRect), ForeColor, Font);
		d2d->PopDrawRect();

		if (progress > 0.001f && height > headerHeight)
		{
			D2D1_RECT_F clip = GetChildrenClipRect();
			d2d->PushDrawRect(clip.left, clip.top, RectWidth(clip), RectHeight(clip));
			if (!this->ParentForm || !this->ParentForm->IsDCompSceneRenderActive())
			{
				for (auto child : this->GetChildrenInZOrder())
				{
					if (!child || !child->Visible) continue;
					child->Update();
				}
			}
			d2d->PopDrawRect();
		}

		if (headerHeight < height)
			d2d->DrawLine(HeaderPaddingX, headerHeight, (std::max)(HeaderPaddingX, width - HeaderPaddingX), headerHeight, ScaleAlpha(BorderColor, 0.62f), 1.0f);
		if (border > 0.0f && BorderColor.a > 0.0f)
			d2d->DrawRoundRect(border * 0.5f, border * 0.5f,
				(std::max)(0.0f, width - border), (std::max)(0.0f, height - border),
				BorderColor, border, radius);
		if (AccentColor.a > 0.0f)
		{
			float accentHeight = (std::max)(6.0f, headerHeight - 14.0f);
			d2d->FillRoundRect(2.0f, (headerHeight - accentHeight) * 0.5f, 3.0f, accentHeight, AccentColor, 1.5f);
		}
		if (!Enable)
			d2d->FillRoundRect(0.0f, 0.0f, width, height, DisabledOverlayColor, radius);
	}
	this->EndRender();
}

bool Expander::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY)
{
	if (!this->Enable || !this->Visible) return true;
	PerformExpanderLayoutIfNeeded();

	const bool isHeaderHit = HeaderHitTest(localX, localY);
	switch (message)
	{
	case WM_MOUSEMOVE:
		if (ParentForm) ParentForm->UnderMouse = this;
		if (_hoverHeader != isHeaderHit)
		{
			_hoverHeader = isHeaderHit;
			InvalidateVisual();
		}
		break;
	case WM_LBUTTONDOWN:
		if (ParentForm)
			ParentForm->SetSelectedControl(this, false);
		if (isHeaderHit)
		{
			OnMouseDown(this, MouseEventArgs(MouseButtons::Left, 0, localX, localY, HIWORD(wParam)));
			InvalidateVisual();
			return true;
		}
		break;
	case WM_LBUTTONUP:
		if (isHeaderHit)
		{
			Toggle();
			MouseEventArgs eventArgs(MouseButtons::Left, 0, localX, localY, HIWORD(wParam));
			OnMouseUp(this, eventArgs);
			OnMouseClick(this, eventArgs);
			return true;
		}
		break;
	case WM_KEYDOWN:
		if (ParentForm && ParentForm->Selected == this && (wParam == VK_RETURN || wParam == VK_SPACE))
		{
			Toggle();
			OnKeyDown(this, KeyEventArgs((Keys)(wParam | 0)));
			return true;
		}
		break;
	default:
		break;
	}

	return Panel::ProcessMessage(message, wParam, lParam, localX, localY);
}
