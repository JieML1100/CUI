#pragma once
#include "ProgressBar.h"
#include "Window.h"
#include <algorithm>
#include <cmath>

UIClass ProgressBar::Type() { return UIClass::UI_ProgressBar; }

void ProgressBar::RegisterDependencyProperties()
{
	RangeBase::RegisterDependencyProperties();
	RegisterControlBorderThicknessMetadata<ProgressBar>(1.5f, 60);
}

ProgressBar::ProgressBar()
{
	RegisterDependencyProperties();
	InitializeControlBorderThicknessDefault(1.5f);
	this->RendererBackgroundColor = cui::theme::palette::ScrollTrack;
	this->RendererForegroundColor = cui::theme::palette::Accent;
	this->RendererBorderColor = cui::theme::palette::Border;
	(void)TrySetPropertyValue(
		L"Padding", BindingValue(Thickness{ 2.0f }),
		DependencyPropertyValueSource::Theme);
}

void ProgressBar::Increment(double delta)
{
	SetCurrentRangeValue(Value + delta);
}

void ProgressBar::Reset()
{
	SetCurrentRangeValue(Minimum);
}

void ProgressBar::OnRender()
{
	if (this->IsVisible == false)return;
	auto d2d = this->GetDrawingContext();
	const auto size = this->GetActualSizeDip();
	const float actualWidth = size.width;
	const float actualHeight = size.height;
	const float radius = actualHeight * 0.5f;
	this->BeginRender();
	if (GetControlTemplateRoot())
	{
		this->EndRender();
		return;
	}
	{
		d2d->FillRoundRect(0.0f, 0.0f, actualWidth, actualHeight, this->RendererBackgroundColor, radius);
		const float border = BorderThickness.MaxEdge();
		if (border > 0.0f && RendererBorderColor.a > 0.0f)
			d2d->DrawRoundRect(0.5f, 0.5f,
				(std::max)(0.0f, actualWidth - 1.0f),
				(std::max)(0.0f, actualHeight - 1.0f),
				RendererBorderColor, border, radius);

		const float fillLeft = (std::max)(0.0f, Padding.Left);
		const float fillTop = (std::max)(0.0f, Padding.Top);
		const float fillRight = (std::max)(0.0f, Padding.Right);
		const float fillBottom = (std::max)(0.0f, Padding.Bottom);
		const float fillH = (std::max)(0.0f,
			actualHeight - fillTop - fillBottom);
		const double range = Maximum - Minimum;
		const float progress = range > 0.0
			? static_cast<float>((Value - Minimum) / range) : 0.0f;
		const float fillW = (std::max)(0.0f,
			actualWidth - fillLeft - fillRight)
			* (std::clamp)(progress, 0.0f, 1.0f);
		if (fillW > 0.25f && fillH > 0.25f)
		{
			const float fillRadius = fillH * 0.5f;
			d2d->FillRoundRect(fillLeft, fillTop,
				fillW, fillH, this->RendererForegroundColor, fillRadius);
		}
	}
	if (!this->IsEnabled)
	{
		d2d->FillRoundRect(0.0f, 0.0f, actualWidth, actualHeight,
			cui::theme::palette::DisabledOverlay, radius);
	}
	this->EndRender();
}
