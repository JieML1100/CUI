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

CursorKind Slider::QueryCursor(int xof, int yof)
{
	(void)yof;
	if (!this->Enable) return CursorKind::Arrow;
	const float l = TrackLeftLocal();
	const float r = TrackRightLocal();
	if ((float)xof >= l && (float)xof <= r) return CursorKind::SizeWE;
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
		float l = TrackLeftLocal();
		float r = TrackRightLocal();
		if (r < l) r = l;
		float cy = TrackYLocal();
		float th = TrackHeight + (active ? 1.0f : 0.0f);
		float top = cy - th * 0.5f;
		float w = (r - l);
		if (w < 0) w = 0;

		d2d->FillRoundRect(l, top, w, th, TrackBackColor, th * 0.5f);
		if (state > 0.0f && TrackHoverColor.a > 0.0f)
			d2d->FillRoundRect(l, top - 1.0f, w, th + 2.0f, WithAlpha(TrackHoverColor, state), (th + 2.0f) * 0.5f);
		if (TrackBorderColor.a > 0.0f)
			d2d->DrawRoundRect(l, top, w, th, TrackBorderColor, 1.0f, th * 0.5f);

		float t = std::clamp(ValueToT(), 0.0f, 1.0f);
		float fw = w * t;
		if (fw > 0.0f)
			d2d->FillRoundRect(l, top, fw, th, TrackForeColor, th * 0.5f);

		float cx = l + w * t;
		float rad = ThumbRadius + (active ? ThumbDragRadiusDelta : (hover ? ThumbHoverRadiusDelta : 0.0f));
		if (ThumbShadowColor.a > 0.0f)
			d2d->FillEllipse(cx, cy + 1.5f, rad + 0.8f, rad + 0.8f, WithAlpha(ThumbShadowColor, active ? 0.38f : 0.22f));
		d2d->FillEllipse(cx, cy, rad, rad, LerpColor(ThumbColor, ThumbHoverColor, state));
		d2d->DrawEllipse(cx, cy, rad, rad, ThumbBorderColor, active ? 1.5f : 1.0f);

		(void)size;
	}
	if (!this->Enable)
		d2d->FillRoundRect(0.0f, 0.0f, actualWidth, actualHeight, DisabledOverlayColor, 6.0f);
	this->EndRender();
}

bool Slider::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof)
{
	if (!this->Enable || !this->Visible) return true;
	switch (message)
	{
	case WM_MOUSEMOVE:
	{
		this->ParentForm->UnderMouse = this;
		if (_dragging)
		{
			SetValueInternal(XToValue(xof), true);
		}
		MouseEventArgs event_obj = MouseEventArgs(MouseButtons::None, 0, xof, yof, HIWORD(wParam));
		this->OnMouseMove(this, event_obj);
	}
	break;
	case WM_LBUTTONDOWN:
	{
		this->ParentForm->Selected = this;
		_dragging = true;
		SetValueInternal(XToValue(xof), true);
		MouseEventArgs event_obj = MouseEventArgs(MouseButtons::Left, 0, xof, yof, HIWORD(wParam));
		this->OnMouseDown(this, event_obj);
		this->InvalidateVisual();
	}
	break;
	case WM_LBUTTONUP:
	{
		_dragging = false;
		if (this->ParentForm->Selected == this)
		{
			MouseEventArgs event_obj = MouseEventArgs(MouseButtons::Left, 0, xof, yof, HIWORD(wParam));
			this->OnMouseUp(this, event_obj);
		}
		this->ParentForm->Selected = NULL;
		this->InvalidateVisual();
	}
	break;
	default:
		return Control::ProcessMessage(message, wParam, lParam, xof, yof);
	}
	return true;
}

