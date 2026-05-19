#include "Slider.h"
#include "Form.h"
#include <cmath>

UIClass Slider::Type() { return UIClass::UI_Slider; }

namespace
{
	D2D1_COLOR_F LerpColor(const D2D1_COLOR_F& from, const D2D1_COLOR_F& to, float t)
	{
		t = std::clamp(t, 0.0f, 1.0f);
		return D2D1_COLOR_F{
			from.r + (to.r - from.r) * t,
			from.g + (to.g - from.g) * t,
			from.b + (to.b - from.b) * t,
			from.a + (to.a - from.a) * t
		};
	}

	D2D1_COLOR_F WithAlpha(D2D1_COLOR_F color, float alpha)
	{
		color.a *= std::clamp(alpha, 0.0f, 1.0f);
		return color;
	}
}

Slider::Slider(int x, int y, int width, int height)
{
	this->Location = POINT{ x,y };
	this->Size = SIZE{ width,height };
	this->BackColor = D2D1_COLOR_F{ 0,0,0,0 };
	this->BorderColor = D2D1_COLOR_F{ 0,0,0,0 };
	this->Cursor = CursorKind::SizeWE;
}

GET_CPP(Slider, float, Min)
{
	return this->_min;
}
SET_CPP(Slider, float, Min)
{
	this->_min = value;
	this->SetValueInternal(this->_value, false);
	this->InvalidateVisual();
}

GET_CPP(Slider, float, Max)
{
	return this->_max;
}
SET_CPP(Slider, float, Max)
{
	this->_max = value;
	this->SetValueInternal(this->_value, false);
	this->InvalidateVisual();
}

GET_CPP(Slider, float, Value)
{
	return this->_value;
}
SET_CPP(Slider, float, Value)
{
	this->SetValueInternal(value, true);
}

CursorKind Slider::QueryCursor(int localX, int localY)
{
	(void)localY;
	if (!this->Enable) return CursorKind::Arrow;
	const float trackLeft = TrackLeftLocal();
	const float trackRight = TrackRightLocal();
	if ((float)localX >= trackLeft && (float)localX <= trackRight) return CursorKind::SizeWE;
	return this->Cursor;
}

void Slider::Update()
{
	if (!this->IsVisual) return;
	auto d2d = this->ParentForm->Render;
	auto size = this->ActualSize();
	const float actualWidth = static_cast<float>(size.cx);
	const float actualHeight = static_cast<float>(size.cy);
	this->BeginRender();
	{
		const bool hover = this->ParentForm && this->ParentForm->UnderMouse == this;
		const bool active = _dragging || (this->ParentForm && this->ParentForm->Selected == this);
		const float state = active ? 1.0f : (hover ? 0.55f : 0.0f);
		float trackLeft = TrackLeftLocal();
		float trackRight = TrackRightLocal();
		if (trackRight < trackLeft) trackRight = trackLeft;
		float trackCenterY = TrackYLocal();
		float trackHeight = TrackHeight + (active ? 1.0f : 0.0f);
		float trackTop = trackCenterY - trackHeight * 0.5f;
		float trackWidth = (trackRight - trackLeft);
		if (trackWidth < 0) trackWidth = 0;

		d2d->FillRoundRect(trackLeft, trackTop, trackWidth, trackHeight, TrackBackColor, trackHeight * 0.5f);
		if (state > 0.0f && TrackHoverColor.a > 0.0f)
			d2d->FillRoundRect(trackLeft, trackTop - 1.0f, trackWidth, trackHeight + 2.0f, WithAlpha(TrackHoverColor, state), (trackHeight + 2.0f) * 0.5f);
		if (TrackBorderColor.a > 0.0f)
			d2d->DrawRoundRect(trackLeft, trackTop, trackWidth, trackHeight, TrackBorderColor, 1.0f, trackHeight * 0.5f);

		float valueRatio = std::clamp(ValueToT(), 0.0f, 1.0f);
		float filledWidth = trackWidth * valueRatio;
		if (filledWidth > 0.0f)
			d2d->FillRoundRect(trackLeft, trackTop, filledWidth, trackHeight, TrackForeColor, trackHeight * 0.5f);

		float thumbCenterX = trackLeft + trackWidth * valueRatio;
		float thumbRadius = ThumbRadius + (active ? ThumbDragRadiusDelta : (hover ? ThumbHoverRadiusDelta : 0.0f));
		if (ThumbShadowColor.a > 0.0f)
			d2d->FillEllipse(thumbCenterX, trackCenterY + 1.5f, thumbRadius + 0.8f, thumbRadius + 0.8f, WithAlpha(ThumbShadowColor, active ? 0.38f : 0.22f));
		d2d->FillEllipse(thumbCenterX, trackCenterY, thumbRadius, thumbRadius, LerpColor(ThumbColor, ThumbHoverColor, state));
		d2d->DrawEllipse(thumbCenterX, trackCenterY, thumbRadius, thumbRadius, ThumbBorderColor, active ? 1.5f : 1.0f);

		(void)size;
	}
	if (!this->Enable)
		d2d->FillRoundRect(0.0f, 0.0f, actualWidth, actualHeight, DisabledOverlayColor, 6.0f);
	this->EndRender();
}

bool Slider::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY)
{
	if (!this->Enable || !this->Visible) return true;
	switch (message)
	{
	case WM_MOUSEMOVE:
	{
		this->ParentForm->UnderMouse = this;
		if (_dragging)
		{
			SetValueInternal(XToValue(localX), true);
		}
		MouseEventArgs eventArgs = MouseEventArgs(MouseButtons::None, 0, localX, localY, HIWORD(wParam));
		this->OnMouseMove(this, eventArgs);
	}
	break;
	case WM_LBUTTONDOWN:
	{
		this->ParentForm->Selected = this;
		_dragging = true;
		SetValueInternal(XToValue(localX), true);
		MouseEventArgs eventArgs = MouseEventArgs(MouseButtons::Left, 0, localX, localY, HIWORD(wParam));
		this->OnMouseDown(this, eventArgs);
		this->InvalidateVisual();
	}
	break;
	case WM_LBUTTONUP:
	{
		_dragging = false;
		if (this->ParentForm->Selected == this)
		{
			MouseEventArgs eventArgs = MouseEventArgs(MouseButtons::Left, 0, localX, localY, HIWORD(wParam));
			this->OnMouseUp(this, eventArgs);
		}
		this->ParentForm->Selected = nullptr;
		this->InvalidateVisual();
	}
	break;
	default:
		return Control::ProcessMessage(message, wParam, lParam, localX, localY);
	}
	return true;
}

