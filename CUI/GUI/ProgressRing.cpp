#pragma once

#include "ProgressRing.h"
#include "Form.h"
#include <algorithm>
#include <cmath>

namespace
{
	constexpr float kPi = 3.14159265358979323846f;

	D2D1_POINT_2F RingPoint(D2D1_POINT_2F center, float radius, float angleDeg)
	{
		const float radians = angleDeg * kPi / 180.0f;
		return D2D1::Point2F(
			center.x + std::sin(radians) * radius,
			center.y - std::cos(radians) * radius);
	}

	D2D1_COLOR_F ResolveTextColor(D2D1_COLOR_F preferred, D2D1_COLOR_F fallback)
	{
		return preferred.a > 0.0f ? preferred : fallback;
	}
}

UIClass ProgressRing::Type() { return UIClass::UI_ProgressRing; }

GET_CPP(ProgressRing, float, PercentageValue)
{
	return this->_percentageValue;
}

SET_CPP(ProgressRing, float, PercentageValue)
{
	const float clamped = (std::clamp)(value, 0.0f, 1.0f);
	if (std::fabs(this->_percentageValue - clamped) < 0.0001f)
		return;
	this->_percentageValue = clamped;
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
		const float thickness = RingThickness > 0.0f ? RingThickness : (std::clamp)(diameter * 0.105f, 4.0f, 13.0f);
		const float radius = (std::max)(4.0f, diameter * 0.5f - thickness * 0.65f - 1.0f);
		const D2D1_POINT_2F center = D2D1::Point2F(actualWidth * 0.5f, actualHeight * 0.5f);
		const float progress = (std::clamp)(this->_percentageValue, 0.0f, 1.0f);

		if (CenterBackColor.a > 0.0f)
		{
			const float innerRadius = (std::max)(0.0f, radius - thickness * 1.05f);
			if (innerRadius > 1.0f)
				d2d->FillEllipse(center, innerRadius, innerRadius, CenterBackColor);
		}

		d2d->DrawArc(center, radius, 0.0f, 359.9f, this->BackColor, thickness);
		if (progress > 0.0001f)
		{
			const float startAngle = -90.0f;
			const float endAngle = startAngle + progress * 360.0f;
			if (ProgressGlowColor.a > 0.0f)
				d2d->DrawArc(center, radius, startAngle, endAngle, ProgressGlowColor, thickness + 3.0f);
			d2d->DrawArc(center, radius, startAngle, endAngle, this->ForeColor, thickness);

			if (ShowCaps)
			{
				const float capRadius = thickness * 0.5f;
				auto startPoint = RingPoint(center, radius, startAngle);
				d2d->FillEllipse(startPoint, capRadius, capRadius, this->ForeColor);
				if (progress < 0.9999f)
				{
					auto endPoint = RingPoint(center, radius, endAngle);
					d2d->FillEllipse(endPoint, capRadius, capRadius, this->ForeColor);
				}
			}
		}

		if (this->_showPercentage)
		{
			std::wstring centerText = this->Text;
			if (centerText.empty())
			{
				centerText = std::to_wstring((int)std::lround(progress * 100.0f)) + L"%";
			}
			d2d->DrawStringCentered(centerText, center.x, center.y, ResolveTextColor(CenterTextColor, this->ForeColor), this->Font);
		}
	}

	if (!this->Enable)
	{
		d2d->FillRoundRect(0.0f, 0.0f, actualWidth, actualHeight, DisabledOverlayColor, 8.0f);
	}
	this->EndRender();
}
