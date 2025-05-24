#pragma once
#include "ProgressBar.h"
#include "Form.h"
UIClass ProgressBar::Type() { return UIClass::UI_ProgressBar; }

GET_CPP(ProgressBar, float, PercentageValue)
{
	return this->_percentageValue;
}
SET_CPP(ProgressBar, float, PercentageValue)
{
	this->_percentageValue = value;
	this->PostRender();
}
ProgressBar::ProgressBar(int x, int y, int width, int height)
{
	this->Location = POINT{ x,y };
	this->Size = SIZE{ width,height };
	this->BackColor = Colors::LightYellow4;
	this->ForeColor = Colors::LawnGreen;
}
void ProgressBar::Update()
{
	if (this->IsVisual == false)return;
	auto d2d = this->ParentForm->Render;
	bool isUnderMouse = this->ParentForm->UnderMouse == this;
	bool isSelected = this->ParentForm->Selected == this;
	auto abslocation = this->AbsLocation;
	auto size = this->ActualSize();
	auto absRect = this->AbsRect;
	d2d->PushDrawRect(absRect.left, absRect.top, absRect.right - absRect.left, absRect.bottom - absRect.top);
	{
		d2d->FillRect(abslocation.x, abslocation.y, size.cx, size.cy, this->BackColor);
		if (this->Image)
		{
			this->RenderImage();
		}
		d2d->FillRect(abslocation.x, abslocation.y, size.cx * this->PercentageValue, size.cy, this->ForeColor);
		
	}
	if (!this->Enable)
	{
		d2d->FillRect(abslocation.x, abslocation.y, size.cx, size.cy, { 1.0f ,1.0f ,1.0f ,0.5f });
	}
	d2d->PopDrawRect();
}