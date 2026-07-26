#pragma once
#include "ProgressBar.h"
#include <algorithm>
#include <cmath>

UIClass ProgressBar::Type() { return UIClass::UI_ProgressBar; }

void ProgressBar::RegisterDependencyProperties()
{
	RangeBase::RegisterDependencyProperties();
	static const bool registered = []
	{
		DependencyPropertyOptions<ProgressBar, ::Orientation>
			orientationOptions;
		orientationOptions.DefaultValue = Orientation::Horizontal;
		orientationOptions.Flags = DependencyPropertyFlags::AffectsMeasure
			| DependencyPropertyFlags::AffectsArrange
			| DependencyPropertyFlags::AffectsRender;
		orientationOptions.Design.Category = L"Layout";
		orientationOptions.Design.CategoryOrder = 50;
		orientationOptions.Design.Order = 10;
		orientationOptions.Design.Editor =
			DependencyPropertyEditorKind::Choice;
		orientationOptions.Design.Persistence =
			DependencyPropertyPersistence::Metadata;
		orientationOptions.Design.Choices = {
			{ L"Horizontal", BindingValue(Orientation::Horizontal) },
			{ L"Vertical", BindingValue(Orientation::Vertical) }
		};
		DependencyPropertyRegistry::Register<ProgressBar, ::Orientation>(
			L"Orientation",
			[](ProgressBar& target) { return target.Orientation; },
			[](ProgressBar& target, const ::Orientation& value)
			{ target.Orientation = value; },
			{}, std::move(orientationOptions));

		DependencyPropertyOptions<ProgressBar, bool>
			indeterminateOptions;
		indeterminateOptions.DefaultValue = false;
		indeterminateOptions.Flags =
			DependencyPropertyFlags::AffectsRender;
		indeterminateOptions.Design.Category = L"Behavior";
		indeterminateOptions.Design.CategoryOrder = 300;
		indeterminateOptions.Design.Order = 10;
		indeterminateOptions.Design.Editor =
			DependencyPropertyEditorKind::Boolean;
		indeterminateOptions.Design.Persistence =
			DependencyPropertyPersistence::Metadata;
		DependencyPropertyRegistry::Register<ProgressBar, bool>(
			L"IsIndeterminate",
			[](ProgressBar& target) { return target.IsIndeterminate; },
			[](ProgressBar& target, const bool& value)
			{ target.IsIndeterminate = value; },
			{}, std::move(indeterminateOptions));
		return true;
	}();
	(void)registered;
}

ProgressBar::ProgressBar()
{
	RegisterDependencyProperties();
}

void ProgressBar::Increment(double delta)
{
	SetCurrentRangeValue(Value + delta);
}

void ProgressBar::Reset()
{
	SetCurrentRangeValue(Minimum);
}

GET_CPP(ProgressBar, ::Orientation, Orientation)
{
	return _orientation;
}

SET_CPP(ProgressBar, ::Orientation, Orientation)
{
	if (!SetPropertyField(L"Orientation", _orientation, value)) return;
	UpdateIndicator();
}

GET_CPP(ProgressBar, bool, IsIndeterminate)
{
	return _isIndeterminate;
}

SET_CPP(ProgressBar, bool, IsIndeterminate)
{
	if (!SetPropertyField(
		L"IsIndeterminate", _isIndeterminate, value)) return;
	UpdateIndicator();
}

void ProgressBar::OnRangeValueChanged(double oldValue, double newValue)
{
	(void)oldValue;
	(void)newValue;
	UpdateIndicator();
}

void ProgressBar::OnComputedLayoutSizeChanged()
{
	UpdateIndicator();
}

void ProgressBar::OnControlTemplatePresentationChanged()
{
	ClearTemplatePartEventConnections();
	if (auto* track = FindDeclarativeTemplatePart(L"PART_Track"))
	{
		const ControlWeakReference lifetime(this);
		RetainTemplatePartEventConnection(track->SizeChanged.Subscribe(
			[lifetime](Control*, SizeChangedEventArgs&)
			{
				auto* progress =
					dynamic_cast<ProgressBar*>(lifetime.Get());
				if (progress) progress->UpdateIndicator();
			}));
	}
	UpdateIndicator();
}

void ProgressBar::UpdateIndicator()
{
	auto* track = FindDeclarativeTemplatePart(L"PART_Track");
	auto* indicator = FindDeclarativeTemplatePart(L"PART_Indicator");
	if (!track || !indicator) return;

	const double range = Maximum - Minimum;
	const double fraction = _isIndeterminate || range <= 0.0000001
		? 1.0
		: (std::clamp)((Value - Minimum) / range, 0.0, 1.0);
	const auto trackSize = track->GetActualSizeDip();
	if (_orientation == Orientation::Horizontal)
	{
		const auto width = cui::layout::Length::Fixed(
			(std::max)(0.0f,
				trackSize.width * static_cast<float>(fraction)));
		if (indicator->Width != width) indicator->Width = width;
		if (!indicator->Height.IsAuto())
			indicator->Height = cui::layout::Length::Auto();
		if (indicator->HorizontalAlignment
			!= HorizontalAlignment::Left)
			indicator->HorizontalAlignment = HorizontalAlignment::Left;
		if (indicator->VerticalAlignment
			!= VerticalAlignment::Stretch)
			indicator->VerticalAlignment = VerticalAlignment::Stretch;
	}
	else
	{
		const auto height = cui::layout::Length::Fixed(
			(std::max)(0.0f,
				trackSize.height * static_cast<float>(fraction)));
		if (indicator->Height != height) indicator->Height = height;
		if (!indicator->Width.IsAuto())
			indicator->Width = cui::layout::Length::Auto();
		if (indicator->HorizontalAlignment
			!= HorizontalAlignment::Stretch)
			indicator->HorizontalAlignment = HorizontalAlignment::Stretch;
		if (indicator->VerticalAlignment
			!= VerticalAlignment::Bottom)
			indicator->VerticalAlignment = VerticalAlignment::Bottom;
	}
}
