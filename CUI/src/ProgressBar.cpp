#pragma once
#include "ProgressBar.h"
#include <algorithm>
#include <cmath>

UIClass ProgressBar::Type() { return UIClass::UI_ProgressBar; }

const DependencyProperty& ProgressBar::OrientationProperty()
{
	static const auto registration = []
	{
		DependencyPropertyOptions<ProgressBar, ::Orientation> options;
		options.DefaultValue = Orientation::Horizontal;
		options.Flags = DependencyPropertyFlags::AffectsMeasure
			| DependencyPropertyFlags::AffectsArrange
			| DependencyPropertyFlags::AffectsRender;
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Layout";
		options.Design.CategoryOrder = 50;
		options.Design.Order = 10;
		options.Design.Editor = DependencyPropertyEditorKind::Choice;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		options.Design.Choices = {
			{ L"Horizontal", BindingValue(Orientation::Horizontal) },
			{ L"Vertical", BindingValue(Orientation::Vertical) }
		};
		)
		return DependencyPropertyRegistry::RegisterStatic<
			ProgressBar, ::Orientation>(
				DependencyPropertyRegistrationLiteral(L"Orientation"),
				[](ProgressBar& target) { return target.Orientation; },
				[](ProgressBar& target, const ::Orientation& value)
				{ target.Orientation = value; },
				{}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& ProgressBar::IsIndeterminateProperty()
{
	static const auto registration = []
	{
		DependencyPropertyOptions<ProgressBar, bool> options;
		options.DefaultValue = false;
		options.Flags = DependencyPropertyFlags::AffectsRender;
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Behavior";
		options.Design.CategoryOrder = 300;
		options.Design.Order = 10;
		options.Design.Editor = DependencyPropertyEditorKind::Boolean;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		)
		return DependencyPropertyRegistry::RegisterStatic<ProgressBar, bool>(
			DependencyPropertyRegistrationLiteral(L"IsIndeterminate"),
			[](ProgressBar& target) { return target.IsIndeterminate; },
			[](ProgressBar& target, const bool& value)
			{ target.IsIndeterminate = value; },
			{}, std::move(options));
	}();
	return *registration;
}

void ProgressBar::RegisterDependencyProperties()
{
	RangeBase::RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	(void)OrientationProperty();
	(void)IsIndeterminateProperty();
#endif
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
	if (!SetPropertyField(OrientationProperty(), _orientation, value)) return;
	UpdateIndicator();
}

GET_CPP(ProgressBar, bool, IsIndeterminate)
{
	return _isIndeterminate;
}

SET_CPP(ProgressBar, bool, IsIndeterminate)
{
	if (!SetPropertyField(
		IsIndeterminateProperty(), _isIndeterminate, value)) return;
	UpdateIndicator();
}

void ProgressBar::OnMinimumChanged(double oldValue, double newValue)
{
	(void)oldValue;
	(void)newValue;
	UpdateIndicator();
	NotifyAccessibilityValueChanged();
}

void ProgressBar::OnMaximumChanged(double oldValue, double newValue)
{
	(void)oldValue;
	(void)newValue;
	UpdateIndicator();
	NotifyAccessibilityValueChanged();
}

void ProgressBar::OnRangeValueChanged(double oldValue, double newValue)
{
	(void)oldValue;
	(void)newValue;
	UpdateIndicator();
	NotifyAccessibilityValueChanged();
}

void ProgressBar::OnComputedLayoutSizeChanged()
{
	UpdateIndicator();
}

void ProgressBar::OnControlTemplatePresentationChanged()
{
	ClearTemplatePartEventConnections();
	if (auto* track = FindDeclarativeTemplatePart(
		MakeTemplatePartToken(L"PART_Track")))
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
	auto* track = FindDeclarativeTemplatePart(
		MakeTemplatePartToken(L"PART_Track"));
	auto* indicator = FindDeclarativeTemplatePart(
		MakeTemplatePartToken(L"PART_Indicator"));
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
