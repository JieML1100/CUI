#include "Slider.h"
#include "Form.h"
#include <cmath>

UIClass Slider::Type() { return UIClass::UI_Slider; }

Slider::Slider(int x, int y, int width, int height)
{
	this->Location = POINT{ x,y };
	this->Size = SIZE{ width,height };
	this->BackColor = D2D1_COLOR_F{ 0,0,0,0 };
	this->BolderColor = D2D1_COLOR_F{ 0,0,0,0 };
	this->Cursor = CursorKind::SizeWE;
}

GET_CPP(Slider, float, Min)
{
	return this->_min;
}
SET_CPP(Slider, float, Min)
{
	this->_min = value;
	if (this->_max < this->_min) this->_max = this->_min;
	this->SetValueInternal(this->_value, false);
	this->PostRender();
}

GET_CPP(Slider, float, Max)
{
	return this->_max;
}
SET_CPP(Slider, float, Max)
{
	this->_max = value;
	if (this->_min > this->_max) this->_min = this->_max;
	this->SetValueInternal(this->_value, false);
	this->PostRender();
}

GET_CPP(Slider, float, Value)
{
	return this->_value;
}
SET_CPP(Slider, float, Value)
{
	this->SetValueInternal(value, true);
}

void Slider::SetRange(float minValue, float maxValue, float value)
{
	if (maxValue < minValue)
		std::swap(minValue, maxValue);
	this->_min = minValue;
	this->_max = maxValue;
	this->SetValueInternal(value, true);
	this->PostRender();
}

void Slider::Increment(float delta)
{
	const float step = (delta != 0.0f) ? delta : (this->Step > 0.0f ? this->Step : 1.0f);
	this->SetValueInternal(this->_value + step, true);
}

void Slider::Decrement(float delta)
{
	const float step = (delta != 0.0f) ? delta : (this->Step > 0.0f ? this->Step : 1.0f);
	this->SetValueInternal(this->_value - step, true);
}

void Slider::Reset()
{
	this->SetValueInternal(this->_min, true);
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
		// track (TrackLeftLocal/RightLocal/YLocal 均为相对控件左上角的局部偏移)
		float l = TrackLeftLocal();
		float r = TrackRightLocal();
		float cy = TrackYLocal();
		float th = TrackHeight;
		float top = cy - th * 0.5f;
		float w = (r - l);
		if (w < 0) w = 0;

		d2d->FillRoundRect(l, top, w, th, TrackBackColor, th * 0.5f);

		float t = std::clamp(ValueToT(), 0.0f, 1.0f);
		float fw = w * t;
		if (fw > 0.0f)
			d2d->FillRoundRect(l, top, fw, th, TrackForeColor, th * 0.5f);

		// thumb
		float cx = l + w * t;
		float rad = ThumbRadius;
		d2d->FillEllipse(cx, cy, rad, rad, ThumbColor);
		d2d->DrawEllipse(cx, cy, rad, rad, ThumbBorderColor, 1.0f);

		(void)size;
	}
	if (!this->Enable)
		d2d->FillRect(0, 0, actualWidth, actualHeight, { 1.0f ,1.0f ,1.0f ,0.5f });
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
		this->PostRender();
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
		this->PostRender();
	}
	break;
	default:
		return Control::ProcessMessage(message, wParam, lParam, xof, yof);
	}
	return true;
}

