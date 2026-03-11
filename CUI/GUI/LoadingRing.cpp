#pragma once

#include "LoadingRing.h"
#include "Form.h"
#include <algorithm>
#include <cmath>

namespace
{
	constexpr float kPi = 3.14159265358979323846f;

	D2D1_POINT_2F PointOnCircle(D2D1_POINT_2F center, float radius, float angleDeg)
	{
		const float radians = angleDeg * kPi / 180.0f;
		return D2D1::Point2F(
			center.x + std::sin(radians) * radius,
			center.y - std::cos(radians) * radius);
	}
}

UIClass LoadingRing::Type() { return UIClass::UI_LoadingRing; }

GET_CPP(LoadingRing, bool, Active)
{
	return this->_active;
}

SET_CPP(LoadingRing, bool, Active)
{
	if (this->_active == value)
		return;
	this->_active = value;
	this->_animStartTick = ::GetTickCount64();
	this->PostRender();
}

LoadingRing::LoadingRing(int x, int y, int width, int height)
{
	this->Location = POINT{ x, y };
	this->Size = SIZE{ width, height };
	this->BackColor = D2D1::ColorF(0.0f, 0.48f, 0.85f, 0.12f);
	this->ForeColor = D2D1::ColorF(0.0f, 0.48f, 0.85f, 1.0f);
	this->BolderColor = D2D1::ColorF(0, 0, 0, 0);
	this->_animStartTick = ::GetTickCount64();
}

float LoadingRing::GetAnimationPhase() const
{
	if (!_active)
		return 0.0f;

	const ULONGLONG now = ::GetTickCount64();
	const ULONGLONG elapsed = now >= _animStartTick ? (now - _animStartTick) : 0;
	const UINT period = _animationPeriodMs == 0 ? 1 : _animationPeriodMs;
	return (float)(elapsed % period) / (float)period;
}

bool LoadingRing::IsAnimationRunning()
{
	return this->_active && this->IsVisual;
}

bool LoadingRing::GetAnimatedInvalidRect(D2D1_RECT_F& outRect)
{
	if (!IsAnimationRunning()) return false;
	outRect = this->AbsRect;
	return true;
}

void LoadingRing::Update()
{
	if (this->IsVisual == false) return;

	auto d2d = this->ParentForm->Render;
	auto size = this->ActualSize();
	this->BeginRender();
	{
		const float diameter = (float)(std::min)(size.cx, size.cy);
		const D2D1_POINT_2F center = D2D1::Point2F(size.cx * 0.5f, size.cy * 0.5f);
		const float orbitRadius = (std::max)(6.0f, diameter * 0.31f);
		const float trackWidth = (std::max)(1.5f, diameter * 0.055f);
		const float phase = GetAnimationPhase();
		const float baseAngle = phase * 360.0f;
		const int dotCount = 5;
		const float spreadDeg = 22.0f;

		if (this->BackColor.a > 0.001f)
		{
			d2d->DrawArc(center, orbitRadius, 0.0f, 359.9f, this->BackColor, trackWidth);
		}

		for (int index = 0; index < dotCount; ++index)
		{
			const float trail = 1.0f - ((float)index / (float)dotCount);
			const float angle = baseAngle - spreadDeg * index - 90.0f;
			auto dotCenter = PointOnCircle(center, orbitRadius, angle);
			auto dotColor = this->ForeColor;
			dotColor.a *= 0.22f + trail * 0.78f;
			const float dotRadius = (std::max)(2.0f, diameter * (0.055f + trail * 0.02f));
			d2d->FillEllipse(dotCenter, dotRadius, dotRadius, dotColor);
		}
	}

	if (!this->Enable)
	{
		d2d->FillRect(0, 0, size.cx, size.cy, { 1.0f, 1.0f, 1.0f, 0.5f });
	}
	this->EndRender();
}