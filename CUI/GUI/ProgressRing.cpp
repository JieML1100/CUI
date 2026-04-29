#pragma once

#include "ProgressRing.h"
#include "Form.h"
#include <algorithm>
#include <cmath>

namespace
{
	constexpr float kPi = 3.14159265358979323846f;
	constexpr float ProgressRingMinMaxValue = 0.0001f;

	D2D1_POINT_2F RingPoint(D2D1_POINT_2F center, float radius, float angleDeg)
	{
		const float radians = angleDeg * kPi / 180.0f;
		return D2D1::Point2F(
			center.x + std::sin(radians) * radius,
			center.y - std::cos(radians) * radius);
	}
}

UIClass ProgressRing::Type() { return UIClass::UI_ProgressRing; }

GET_CPP(ProgressRing, float, MaxValue)
{
	return this->_maxValue;
}

SET_CPP(ProgressRing, float, MaxValue)
{
	const float oldValue = this->_currentValue;
	this->_maxValue = (std::max)(value, ProgressRingMinMaxValue);
	this->_currentValue = (std::clamp)(this->_currentValue, 0.0f, this->_maxValue);
	if (this->_currentValue != oldValue)
		this->OnValueChanged(this, EventArgs());
	this->PostRender();
}

GET_CPP(ProgressRing, float, Value)
{
	return this->_currentValue;
}

SET_CPP(ProgressRing, float, Value)
{
	const float newValue = (std::clamp)(value, 0.0f, this->_maxValue);
	if (this->_currentValue != newValue)
	{
		this->_currentValue = newValue;
		this->OnValueChanged(this, EventArgs());
	}
	this->PostRender();
}

GET_CPP(ProgressRing, float, PercentageValue)
{
	if (this->_maxValue <= 0.0f) return 0.0f;
	return this->_currentValue / this->_maxValue;
}

SET_CPP(ProgressRing, float, PercentageValue)
{
	const float clamped = (std::clamp)(value, 0.0f, 1.0f);
	const float newValue = this->_maxValue * clamped;
	if (std::fabs(this->_currentValue - newValue) < 0.0001f) return;
	this->_currentValue = newValue;
	this->OnValueChanged(this, EventArgs());
	this->PostRender();
}

GET_CPP(ProgressRing, bool, ShowPercentage)
{
	return this->_showPercentage;
}

SET_CPP(ProgressRing, bool, ShowPercentage)
{
	if (this->_showPercentage == value)
		return;
	this->_showPercentage = value;
	this->PostRender();
}

ProgressRing::ProgressRing(int x, int y, int width, int height)
{
	this->Location = POINT{ x, y };
	this->Size = SIZE{ width, height };
	this->BackColor = D2D1::ColorF(0.0f, 0.48f, 0.85f, 0.16f);
	this->ForeColor = D2D1::ColorF(0.0f, 0.48f, 0.85f, 1.0f);
	this->BolderColor = D2D1::ColorF(0, 0, 0, 0);
	this->Font = new ::Font(L"Segoe UI", 16.0f);
}

void ProgressRing::SetRange(float maxValue, float value)
{
	const float oldValue = this->_currentValue;
	this->_maxValue = (std::max)(maxValue, ProgressRingMinMaxValue);
	this->_currentValue = (std::clamp)(value, 0.0f, this->_maxValue);
	if (this->_currentValue != oldValue)
		this->OnValueChanged(this, EventArgs());
	this->PostRender();
}

void ProgressRing::Increment(float delta)
{
	this->Value = this->_currentValue + delta;
}

void ProgressRing::Reset()
{
	this->Value = 0.0f;
}

void ProgressRing::Update()
{
	if (this->IsVisual == false) return;

	auto d2d = this->ParentForm->Render;
	auto size = this->ActualSize();
	const float actualWidth = static_cast<float>(size.cx);
	const float actualHeight = static_cast<float>(size.cy);
	this->BeginRender();
	{
		const float diameter = (std::min)(actualWidth, actualHeight);
		const float thickness = (std::clamp)(diameter * 0.12f, 4.0f, 14.0f);
		const float radius = (std::max)(4.0f, diameter * 0.5f - thickness * 0.65f - 1.0f);
		const D2D1_POINT_2F center = D2D1::Point2F(actualWidth * 0.5f, actualHeight * 0.5f);
		const float progress = (std::clamp)(this->PercentageValue, 0.0f, 1.0f);

		d2d->DrawArc(center, radius, 0.0f, 359.9f, this->BackColor, thickness);
		if (progress > 0.0001f)
		{
			const float startAngle = -90.0f;
			const float endAngle = startAngle + progress * 360.0f;
			d2d->DrawArc(center, radius, startAngle, endAngle, this->ForeColor, thickness);

			const float capRadius = thickness * 0.5f;
			auto startPoint = RingPoint(center, radius, startAngle);
			d2d->FillEllipse(startPoint, capRadius, capRadius, this->ForeColor);
			if (progress < 0.9999f)
			{
				auto endPoint = RingPoint(center, radius, endAngle);
				d2d->FillEllipse(endPoint, capRadius, capRadius, this->ForeColor);
			}
		}

		if (this->_showPercentage)
		{
			std::wstring centerText = this->Text;
			if (centerText.empty())
			{
				centerText = std::to_wstring((int)std::lround(progress * 100.0f)) + L"%";
			}
			d2d->DrawStringCentered(centerText, center.x, center.y, this->ForeColor, this->Font);
		}
	}

	if (!this->Enable)
	{
		d2d->FillRect(0, 0, actualWidth, actualHeight, { 1.0f, 1.0f, 1.0f, 0.5f });
	}
	this->EndRender();
}
