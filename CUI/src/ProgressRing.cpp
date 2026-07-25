#pragma once

#include "ProgressRing.h"
#include "Window.h"
#include <algorithm>
#include <cmath>

UIClass ProgressRing::Type() { return UIClass::UI_ProgressRing; }

void ProgressRing::RegisterDependencyProperties()
{
	RangeBase::RegisterDependencyProperties();
	static const bool registered = []
	{
		DependencyPropertyOptions<ProgressRing, bool> showPercentage;
		showPercentage.DefaultValue = true;
		showPercentage.Flags = DependencyPropertyFlags::AffectsRender;
		showPercentage.Design.Category = L"Behavior";
		showPercentage.Design.CategoryOrder = 300;
		showPercentage.Design.Order = 10;
		showPercentage.Design.Editor = DependencyPropertyEditorKind::Boolean;
		showPercentage.Design.Persistence = DependencyPropertyPersistence::Native;
		DependencyPropertyRegistry::Register<ProgressRing, bool>(L"ShowPercentage",
			[](ProgressRing& target) { return target.ShowPercentage; },
			[](ProgressRing& target, const bool& value) { target.ShowPercentage = value; },
			{}, std::move(showPercentage));
		return true;
	}();
	(void)registered;
}

GET_CPP(ProgressRing, bool, ShowPercentage)
{
	return this->_showPercentage;
}

SET_CPP(ProgressRing, bool, ShowPercentage)
{
	(void)SetPropertyField(
		L"ShowPercentage", _showPercentage, value);
}

ProgressRing::ProgressRing()
{
	this->RendererBackgroundColor = D2D1::ColorF(0.0f, 0.48f, 0.85f, 0.16f);
	this->RendererForegroundColor = D2D1::ColorF(0.0f, 0.48f, 0.85f, 1.0f);
	this->RendererBorderColor = D2D1::ColorF(0, 0, 0, 0);
	(void)TrySetPropertyValue(
		L"FontSize", BindingValue(16.0),
		DependencyPropertyValueSource::Theme);
}

void ProgressRing::OnRender()
{
	if (this->IsVisible == false) return;

	auto d2d = this->GetDrawingContext();
	const auto size = this->GetActualSizeDip();
	const float actualWidth = size.width;
	const float actualHeight = size.height;
	this->BeginRender();
	if (GetControlTemplateRoot())
	{
		this->EndRender();
		return;
	}
	{
		const float diameter = (std::min)(actualWidth, actualHeight);
		const float thickness = (std::clamp)(
			diameter * 0.105f, 4.0f, 13.0f);
		const float radius = (std::max)(4.0f, diameter * 0.5f - thickness * 0.65f - 1.0f);
		const D2D1_POINT_2F center = D2D1::Point2F(actualWidth * 0.5f, actualHeight * 0.5f);
		const double minimum = MinimumCore();
		const double maximum = MaximumCore();
		const double span = maximum - minimum;
		const float progress = span > 0.0
			? static_cast<float>((ValueCore() - minimum) / span)
			: 0.0f;

		d2d->DrawArc(center, radius, 0.0f, 359.9f, this->RendererBackgroundColor, thickness);
		if (progress > 0.0001f)
		{
			const float startAngle = -90.0f;
			const float endAngle = startAngle + progress * 360.0f;
			d2d->DrawArc(center, radius, startAngle, endAngle, this->RendererForegroundColor, thickness);
		}

		if (this->_showPercentage)
		{
			const std::wstring centerText =
				std::to_wstring((int)std::lround(progress * 100.0f)) + L"%";
			d2d->DrawStringCentered(centerText, center.x, center.y,
				this->RendererForegroundColor, this->GetRenderFont());
		}
	}

	if (!this->IsEnabled)
	{
		d2d->FillRoundRect(0.0f, 0.0f, actualWidth, actualHeight,
			cui::theme::palette::DisabledOverlay, 8.0f);
	}
	this->EndRender();
}
