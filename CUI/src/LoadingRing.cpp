#pragma once

#include "LoadingRing.h"
#include "Window.h"
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

const DependencyProperty& LoadingRing::IsActiveProperty()
{
	static const auto registration = []
	{
		DependencyPropertyOptions<LoadingRing, bool> options;
		options.DefaultValue = true;
		options.Flags = DependencyPropertyFlags::AffectsRender;
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Behavior";
		options.Design.CategoryOrder = 300;
		options.Design.Order = 10;
		options.Design.Editor = DependencyPropertyEditorKind::Boolean;
		options.Design.Persistence = DependencyPropertyPersistence::Native;
		)
		return DependencyPropertyRegistry::RegisterStatic<LoadingRing, bool>(
			DependencyPropertyRegistrationLiteral(L"IsActive"),
			[](LoadingRing& target) { return target.IsActive; },
			[](LoadingRing& target, const bool& value)
			{ target.IsActive = value; },
			{}, std::move(options));
	}();
	return *registration;
}

void LoadingRing::RegisterDependencyProperties()
{
	Control::RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	(void)IsActiveProperty();
#endif
}

GET_CPP(LoadingRing, bool, IsActive)
{
	return this->_active;
}

SET_CPP(LoadingRing, bool, IsActive)
{
	const auto& property = IsActiveProperty();
	const bool applyingMetadata = _applyingPropertyMetadata
		&& &_applyingPropertyMetadata->Property() == &property;
	if (!SetPropertyField(property, _active, value)) return;
	if (!applyingMetadata) return;
	this->_animStartTick = ::GetTickCount64();
	this->InvalidateVisual();
}

LoadingRing::LoadingRing()
{
	this->RendererBackgroundColor = D2D1::ColorF(0.0f, 0.48f, 0.85f, 0.12f);
	this->RendererForegroundColor = D2D1::ColorF(0.0f, 0.48f, 0.85f, 1.0f);
	this->RendererBorderColor = D2D1::ColorF(0, 0, 0, 0);
	this->_animStartTick = ::GetTickCount64();
}

float LoadingRing::GetAnimationPhase() const
{
	if (!_active || !AreSystemAnimationsEnabled())
		return 0.0f;

	const ULONGLONG now = ::GetTickCount64();
	const ULONGLONG elapsed = now >= _animStartTick ? (now - _animStartTick) : 0;
	const UINT period = _animationPeriodMs == 0 ? 1 : _animationPeriodMs;
	return (float)(elapsed % period) / (float)period;
}

bool LoadingRing::IsAnimationRunning()
{
	return this->_active && this->IsVisible && AreSystemAnimationsEnabled();
}

bool LoadingRing::GetAnimatedInvalidRect(D2D1_RECT_F& outRect)
{
	if (!IsAnimationRunning()) return false;
	outRect = GetAbsoluteBoundsDip();
	return true;
}

void LoadingRing::OnRender()
{
	if (this->IsVisible == false) return;

	auto d2d = this->GetDrawingContext();
	const auto size = this->GetActualSizeDip();
	const float actualWidth = size.width;
	const float actualHeight = size.height;
	this->BeginRender();
	{
		const float diameter = (std::min)(actualWidth, actualHeight);
		const D2D1_POINT_2F center = D2D1::Point2F(actualWidth * 0.5f, actualHeight * 0.5f);
		const float orbitRadius = (std::max)(6.0f, diameter * 0.31f);
		const float trackWidth = (std::max)(1.5f, diameter * 0.055f);
		const float phase = GetAnimationPhase();
		const float baseAngle = phase * 360.0f;
		const int dotCount = 5;
		const float spreadDeg = 22.0f;

		if (this->RendererBackgroundColor.a > 0.001f)
		{
			d2d->DrawArc(center, orbitRadius, 0.0f, 359.9f, this->RendererBackgroundColor, trackWidth);
		}

		for (int index = 0; index < dotCount; ++index)
		{
			const float trail = 1.0f - ((float)index / (float)dotCount);
			const float angle = baseAngle - spreadDeg * index - 90.0f;
			auto dotCenter = PointOnCircle(center, orbitRadius, angle);
			auto dotColor = this->RendererForegroundColor;
			dotColor.a *= 0.22f + trail * 0.78f;
			const float dotRadius = (std::max)(2.0f, diameter * (0.055f + trail * 0.02f));
			d2d->FillEllipse(dotCenter, dotRadius, dotRadius, dotColor);
		}
	}

	if (!this->IsEnabled)
	{
		d2d->FillRect(0, 0, actualWidth, actualHeight, { 1.0f, 1.0f, 1.0f, 0.5f });
	}
	this->EndRender();
}
