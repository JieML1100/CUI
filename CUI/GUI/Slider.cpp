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
	auto abs = this->AbsLocation;
	auto size = this->ActualSize();
	auto absRect = this->AbsRect;
	d2d->PushDrawRect(absRect.left, absRect.top, absRect.right - absRect.left, absRect.bottom - absRect.top);
	{
		// track
		float l = abs.x + TrackLeftLocal();
		float r = abs.x + TrackRightLocal();
		float cy = abs.y + TrackYLocal();
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
		d2d->FillRect(abs.x, abs.y, size.cx, size.cy, { 1.0f ,1.0f ,1.0f ,0.5f });
	d2d->PopDrawRect();
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

