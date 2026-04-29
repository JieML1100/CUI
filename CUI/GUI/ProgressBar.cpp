#pragma once
#include "ProgressBar.h"
#include "Form.h"
#include <algorithm>
UIClass ProgressBar::Type() { return UIClass::UI_ProgressBar; }

namespace
{
	constexpr float ProgressBarMinMaxValue = 0.0001f;
}

GET_CPP(ProgressBar, float, MaxValue)
{
	return this->_maxValue;
}
SET_CPP(ProgressBar, float, MaxValue)
{
	const float oldValue = this->_currentValue;
	this->_maxValue = std::max(value, ProgressBarMinMaxValue);
	this->_currentValue = std::clamp(this->_currentValue, 0.0f, this->_maxValue);
	if (this->_currentValue != oldValue)
	{
		this->OnValueChanged(this, EventArgs());
	}
	this->PostRender();
}
GET_CPP(ProgressBar, float, Value)
{
	return this->_currentValue;
}
SET_CPP(ProgressBar, float, Value)
{
	const float newValue = std::clamp(value, 0.0f, this->_maxValue);
	if (this->_currentValue != newValue)
	{
		this->_currentValue = newValue;
		this->OnValueChanged(this, EventArgs());
	}
	this->PostRender();
}
GET_CPP(ProgressBar, float, PercentageValue)
{
	if (this->_maxValue <= 0.0f) return 0.0f;
	return this->_currentValue / this->_maxValue;
}
SET_CPP(ProgressBar, float, PercentageValue)
{
	const float percent = std::clamp(value, 0.0f, 1.0f);
	const float newValue = this->_maxValue * percent;
	if (this->_currentValue != newValue)
	{
		this->_currentValue = newValue;
		this->OnValueChanged(this, EventArgs());
	}
	this->PostRender();
}
ProgressBar::ProgressBar(int x, int y, int width, int height)
{
	this->Location = POINT{ x,y };
	this->Size = SIZE{ width,height };
	this->BackColor = Colors::LightYellow4;
	this->ForeColor = Colors::LawnGreen;
}

void ProgressBar::SetRange(float maxValue, float value)
{
	const float oldValue = this->_currentValue;
	this->_maxValue = std::max(maxValue, ProgressBarMinMaxValue);
	this->_currentValue = std::clamp(value, 0.0f, this->_maxValue);
	if (this->_currentValue != oldValue)
	{
		this->OnValueChanged(this, EventArgs());
	}
	this->PostRender();
}

void ProgressBar::Increment(float delta)
{
	this->Value = this->_currentValue + delta;
}

void ProgressBar::Reset()
{
	this->Value = 0.0f;
}

void ProgressBar::Update()
{
	if (this->IsVisual == false)return;
	auto d2d = this->ParentForm->Render;
	auto size = this->ActualSize();
	const float actualWidth = static_cast<float>(size.cx);
	const float actualHeight = static_cast<float>(size.cy);
	this->BeginRender();
	{
		d2d->FillRect(0, 0, actualWidth, actualHeight, this->BackColor);
		if (this->Image)
		{
			this->RenderImage();
		}
		d2d->FillRect(0, 0, actualWidth * this->PercentageValue, actualHeight, this->ForeColor);
	}
	if (!this->Enable)
	{
		d2d->FillRect(0, 0, actualWidth, actualHeight, { 1.0f ,1.0f ,1.0f ,0.5f });
	}
	this->EndRender();
}
