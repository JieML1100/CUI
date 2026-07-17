#pragma once
#include "ProgressBar.h"
#include "Form.h"
#include <algorithm>
#include <cmath>

UIClass ProgressBar::Type() { return UIClass::UI_ProgressBar; }

void ProgressBar::EnsureBindingPropertiesRegistered()
{
	Control::EnsureBindingPropertiesRegistered();
	static const bool registered = []
	{
		auto transientValue = []
		{
			ControlPropertyOptions<ProgressBar, float> options;
			options.Design.Browsable = false;
			options.Design.Category = L"Data";
			options.Design.CategoryOrder = 600;
			options.Design.Persistence = ControlPropertyPersistence::Transient;
			return options;
		};
		BindingPropertyRegistry::Register<ProgressBar, float>(L"Value",
			[](ProgressBar& target) { return target.Value; },
			[](ProgressBar& target, const float& value) { target.Value = value; },
			{}, transientValue());
		BindingPropertyRegistry::Register<ProgressBar, float>(L"MaxValue",
			[](ProgressBar& target) { return target.MaxValue; },
			[](ProgressBar& target, const float& value) { target.MaxValue = value; },
			{}, transientValue());
		ControlPropertyOptions<ProgressBar, float> percentageOptions;
		percentageOptions.DefaultValue = 0.5f;
		percentageOptions.Flags = ControlPropertyFlags::AffectsRender;
		percentageOptions.Coerce = [](ProgressBar&, const float& value) -> std::optional<float>
		{
			return (std::clamp)(value, 0.0f, 1.0f);
		};
		percentageOptions.Design.Category = L"Data";
		percentageOptions.Design.CategoryOrder = 600;
		percentageOptions.Design.Order = 10;
		percentageOptions.Design.Editor = ControlPropertyEditorKind::Number;
		percentageOptions.Design.Minimum = 0.0;
		percentageOptions.Design.Maximum = 1.0;
		percentageOptions.Design.Step = 0.01;
		percentageOptions.Design.Persistence = ControlPropertyPersistence::Legacy;
		BindingPropertyRegistry::Register<ProgressBar, float>(L"PercentageValue",
			[](ProgressBar& target) { return target.PercentageValue; },
			[](ProgressBar& target, const float& value) { target.PercentageValue = value; },
			{}, std::move(percentageOptions));
		return true;
	}();
	(void)registered;
}

GET_CPP(ProgressBar, float, MaxValue)
{
	return this->_maxValue;
}

SET_CPP(ProgressBar, float, MaxValue)
{
	if (value > 0.0f)
	{
		this->_maxValue = value;
		if (this->_currentValue > this->_maxValue)
			this->_currentValue = this->_maxValue;
	}
	this->InvalidateVisual();
}

GET_CPP(ProgressBar, float, Value)
{
	return this->_currentValue;
}

SET_CPP(ProgressBar, float, Value)
{
	this->_currentValue = (std::clamp)(value, 0.0f, this->_maxValue);
	this->InvalidateVisual();
}

GET_CPP(ProgressBar, float, PercentageValue)
{
	if (this->_maxValue <= 0.0f)
		return 0.0f;
	return this->_currentValue / this->_maxValue;
}

SET_CPP(ProgressBar, float, PercentageValue)
{
	this->_currentValue = this->_maxValue * (std::clamp)(value, 0.0f, 1.0f);
	this->InvalidateVisual();
}

ProgressBar::ProgressBar(int x, int y, int width, int height)
{
	this->Location = POINT{ x,y };
	this->Size = SIZE{ width,height };
	this->BackColor = cui::theme::palette::ScrollTrack;
	this->ForeColor = cui::theme::palette::Accent;
}

void ProgressBar::Update()
{
	if (this->IsVisual == false)return;
	auto d2d = this->ParentForm->Render;
	const auto size = this->GetActualSizeDip();
	const float actualWidth = size.width;
	const float actualHeight = size.height;
	const float radius = CornerRadius < 0.0f ? actualHeight * 0.5f : (std::min)(CornerRadius, actualHeight * 0.5f);
	this->BeginRender();
	{
		d2d->FillRoundRect(0.0f, 0.0f, actualWidth, actualHeight, this->BackColor, radius);
		if (this->Image)
		{
			this->RenderImage(radius);
		}
		if (BorderThickness > 0.0f && TrackBorderColor.a > 0.0f)
			d2d->DrawRoundRect(0.5f, 0.5f, (std::max)(0.0f, actualWidth - 1.0f), (std::max)(0.0f, actualHeight - 1.0f), TrackBorderColor, BorderThickness, radius);

		const float inset = (std::clamp)(InnerPadding, 0.0f, actualHeight * 0.35f);
		const float fillH = (std::max)(0.0f, actualHeight - inset * 2.0f);
		const float fillW = (std::max)(0.0f, actualWidth - inset * 2.0f) * (std::clamp)(this->PercentageValue, 0.0f, 1.0f);
		if (fillW > 0.25f && fillH > 0.25f)
		{
			const float fillRadius = fillH * 0.5f;
			d2d->FillRoundRect(inset, inset, fillW, fillH, this->ForeColor, fillRadius);
			if (FillHighlightColor.a > 0.0f)
			{
				const float highlightH = (std::max)(1.0f, fillH * 0.42f);
				d2d->FillRoundRect(inset + 1.0f, inset + 1.0f, (std::max)(0.0f, fillW - 2.0f), highlightH, FillHighlightColor, highlightH * 0.5f);
			}
		}
	}
	if (!this->Enable)
	{
		d2d->FillRoundRect(0.0f, 0.0f, actualWidth, actualHeight, DisabledOverlayColor, radius);
	}
	this->EndRender();
}
