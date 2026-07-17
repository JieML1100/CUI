#include "Slider.h"
#include "Form.h"
#include <cmath>
#include <utility>

UIClass Slider::Type() { return UIClass::UI_Slider; }

namespace
{
	template<typename TValue>
	ControlPropertyOptions<Slider, TValue> SliderPropertyOptions(
		TValue defaultValue,
		const wchar_t* category,
		int categoryOrder,
		int order,
		ControlPropertyEditorKind editor,
		ControlPropertyFlags flags = ControlPropertyFlags::AffectsRender)
	{
		ControlPropertyOptions<Slider, TValue> options;
		options.DefaultValue = std::move(defaultValue);
		options.Flags = flags | ControlPropertyFlags::TracksLocalValue;
		options.Design.Category = category;
		options.Design.CategoryOrder = categoryOrder;
		options.Design.Order = order;
		options.Design.Editor = editor;
		options.Design.Persistence = ControlPropertyPersistence::Metadata;
		return options;
	}

	auto SliderPropertySubscriber(const wchar_t* propertyName)
	{
		return [propertyName = std::wstring(propertyName)](
			Slider& target,
			BindingPropertyMetadata::ChangeHandler handler,
			DataSourceUpdateMode)
		{
			return target.OnPropertyValueChanged.Subscribe(
				[propertyName, handler = std::move(handler)](
					Control*, const ControlPropertyChangedEventArgs& args)
				{
					if (_wcsicmp(args.PropertyName.c_str(), propertyName.c_str()) == 0)
						handler();
				});
		};
	}

	bool SliderColorsEqual(
		const D2D1_COLOR_F& left,
		const D2D1_COLOR_F& right)
	{
		return left.r == right.r && left.g == right.g
			&& left.b == right.b && left.a == right.a;
	}

	ControlPropertyOptions<Slider, D2D1_COLOR_F> SliderColorOptions(
		D2D1_COLOR_F defaultValue,
		int order)
	{
		auto options = SliderPropertyOptions(
			defaultValue, L"Appearance", 200, order,
			ControlPropertyEditorKind::Color);
		options.Equals = SliderColorsEqual;
		return options;
	}

	ControlPropertyOptions<Slider, float> SliderMetricOptions(
		float defaultValue,
		int order)
	{
		auto options = SliderPropertyOptions(
			defaultValue, L"Appearance", 200, order,
			ControlPropertyEditorKind::Number);
		options.Coerce = [](Slider&, const float& proposed) -> std::optional<float>
		{
			return std::isfinite(proposed)
				? std::optional<float>{ (std::max)(0.0f, proposed) }
				: std::nullopt;
		};
		options.Design.Minimum = 0.0;
		options.Design.Step = 0.5;
		return options;
	}

	D2D1_COLOR_F LerpColor(const D2D1_COLOR_F& from, const D2D1_COLOR_F& to, float t)
	{
		t = std::clamp(t, 0.0f, 1.0f);
		return D2D1_COLOR_F{
			from.r + (to.r - from.r) * t,
			from.g + (to.g - from.g) * t,
			from.b + (to.b - from.b) * t,
			from.a + (to.a - from.a) * t
		};
	}

	D2D1_COLOR_F WithAlpha(D2D1_COLOR_F color, float alpha)
	{
		color.a *= std::clamp(alpha, 0.0f, 1.0f);
		return color;
	}
}

void Slider::EnsureBindingPropertiesRegistered()
{
	Control::EnsureBindingPropertiesRegistered();
	static const bool registered = []
	{
		auto minimumOptions = SliderPropertyOptions(
			0.0f, L"Range", 100, 10, ControlPropertyEditorKind::Number);
		minimumOptions.Coerce = [](
			Slider&, const float& proposed) -> std::optional<float>
		{
			return std::isfinite(proposed)
				? std::optional<float>{ proposed } : std::nullopt;
		};
		minimumOptions.Design.Step = 0.1;
		BindingPropertyRegistry::Register<Slider, float>(L"Min",
			[](Slider& target) { return target.Min; },
			[](Slider& target, const float& value) { target.Min = value; },
			SliderPropertySubscriber(L"Min"), std::move(minimumOptions));

		auto maximumOptions = SliderPropertyOptions(
			100.0f, L"Range", 100, 20, ControlPropertyEditorKind::Number);
		maximumOptions.Coerce = [](
			Slider& target, const float& proposed) -> std::optional<float>
		{
			if (!std::isfinite(proposed)) return std::nullopt;
			return (std::max)(target.Min, proposed);
		};
		maximumOptions.Design.Step = 0.1;
		BindingPropertyRegistry::Register<Slider, float>(L"Max",
			[](Slider& target) { return target.Max; },
			[](Slider& target, const float& value) { target.Max = value; },
			SliderPropertySubscriber(L"Max"), std::move(maximumOptions));

		auto valueOptions = SliderPropertyOptions(
			0.0f, L"Range", 100, 50, ControlPropertyEditorKind::Number);
		valueOptions.Coerce = [](
			Slider& target, const float& proposed) -> std::optional<float>
		{
			return target.CoerceValue(proposed);
		};
		valueOptions.Changed = [](
			Slider& target, const float& oldValue, const float& newValue)
		{
			target.OnValueChanged(&target, oldValue, newValue);
		};
		valueOptions.Design.Step = 0.1;
		BindingPropertyRegistry::Register<Slider, float>(L"Value",
			[](Slider& target) { return target.Value; },
			[](Slider& target, const float& value) { target.Value = value; },
			SliderPropertySubscriber(L"Value"), std::move(valueOptions));

		auto stepOptions = SliderPropertyOptions(
			1.0f, L"Range", 100, 30, ControlPropertyEditorKind::Number);
		stepOptions.Coerce = [](
			Slider&, const float& proposed) -> std::optional<float>
		{
			return std::isfinite(proposed)
				? std::optional<float>{ (std::max)(0.0f, proposed) }
				: std::nullopt;
		};
		stepOptions.Design.Minimum = 0.0;
		stepOptions.Design.Step = 0.1;
		BindingPropertyRegistry::Register<Slider, float>(L"Step",
			[](Slider& target) { return target.Step; },
			[](Slider& target, const float& value) { target.Step = value; },
			SliderPropertySubscriber(L"Step"), std::move(stepOptions));

		auto snapOptions = SliderPropertyOptions(
			false, L"Range", 100, 40, ControlPropertyEditorKind::Boolean);
		BindingPropertyRegistry::Register<Slider, bool>(L"SnapToStep",
			[](Slider& target) { return target.SnapToStep; },
			[](Slider& target, const bool& value) { target.SnapToStep = value; },
			SliderPropertySubscriber(L"SnapToStep"), std::move(snapOptions));

		BindingPropertyRegistry::Register<Slider, D2D1_COLOR_F>(L"TrackBackColor",
			[](Slider& target) { return target.TrackBackColor; },
			[](Slider& target, const D2D1_COLOR_F& value) { target.TrackBackColor = value; },
			SliderPropertySubscriber(L"TrackBackColor"),
			SliderColorOptions(cui::theme::palette::ScrollTrack, 10));
		BindingPropertyRegistry::Register<Slider, D2D1_COLOR_F>(L"TrackForeColor",
			[](Slider& target) { return target.TrackForeColor; },
			[](Slider& target, const D2D1_COLOR_F& value) { target.TrackForeColor = value; },
			SliderPropertySubscriber(L"TrackForeColor"),
			SliderColorOptions(cui::theme::palette::Accent, 20));
		BindingPropertyRegistry::Register<Slider, D2D1_COLOR_F>(L"TrackHoverColor",
			[](Slider& target) { return target.TrackHoverColor; },
			[](Slider& target, const D2D1_COLOR_F& value) { target.TrackHoverColor = value; },
			SliderPropertySubscriber(L"TrackHoverColor"),
			SliderColorOptions(cui::theme::palette::AccentSoft, 30));
		BindingPropertyRegistry::Register<Slider, D2D1_COLOR_F>(L"TrackBorderColor",
			[](Slider& target) { return target.TrackBorderColor; },
			[](Slider& target, const D2D1_COLOR_F& value) { target.TrackBorderColor = value; },
			SliderPropertySubscriber(L"TrackBorderColor"),
			SliderColorOptions(cui::theme::palette::Border, 40));
		BindingPropertyRegistry::Register<Slider, D2D1_COLOR_F>(L"ThumbColor",
			[](Slider& target) { return target.ThumbColor; },
			[](Slider& target, const D2D1_COLOR_F& value) { target.ThumbColor = value; },
			SliderPropertySubscriber(L"ThumbColor"),
			SliderColorOptions(cui::theme::palette::Surface, 50));
		BindingPropertyRegistry::Register<Slider, D2D1_COLOR_F>(L"ThumbHoverColor",
			[](Slider& target) { return target.ThumbHoverColor; },
			[](Slider& target, const D2D1_COLOR_F& value) { target.ThumbHoverColor = value; },
			SliderPropertySubscriber(L"ThumbHoverColor"),
			SliderColorOptions(cui::theme::palette::Surface, 60));
		BindingPropertyRegistry::Register<Slider, D2D1_COLOR_F>(L"ThumbBorderColor",
			[](Slider& target) { return target.ThumbBorderColor; },
			[](Slider& target, const D2D1_COLOR_F& value) { target.ThumbBorderColor = value; },
			SliderPropertySubscriber(L"ThumbBorderColor"),
			SliderColorOptions(cui::theme::palette::BorderStrong, 70));
		BindingPropertyRegistry::Register<Slider, D2D1_COLOR_F>(L"ThumbShadowColor",
			[](Slider& target) { return target.ThumbShadowColor; },
			[](Slider& target, const D2D1_COLOR_F& value) { target.ThumbShadowColor = value; },
			SliderPropertySubscriber(L"ThumbShadowColor"),
			SliderColorOptions(cui::theme::palette::Shadow, 80));
		BindingPropertyRegistry::Register<Slider, D2D1_COLOR_F>(L"DisabledOverlayColor",
			[](Slider& target) { return target.DisabledOverlayColor; },
			[](Slider& target, const D2D1_COLOR_F& value) { target.DisabledOverlayColor = value; },
			SliderPropertySubscriber(L"DisabledOverlayColor"),
			SliderColorOptions(cui::theme::palette::DisabledOverlay, 90));

		BindingPropertyRegistry::Register<Slider, float>(L"TrackHeight",
			[](Slider& target) { return target.TrackHeight; },
			[](Slider& target, const float& value) { target.TrackHeight = value; },
			SliderPropertySubscriber(L"TrackHeight"), SliderMetricOptions(5.0f, 100));
		BindingPropertyRegistry::Register<Slider, float>(L"ThumbRadius",
			[](Slider& target) { return target.ThumbRadius; },
			[](Slider& target, const float& value) { target.ThumbRadius = value; },
			SliderPropertySubscriber(L"ThumbRadius"), SliderMetricOptions(8.0f, 110));
		BindingPropertyRegistry::Register<Slider, float>(L"ThumbHoverRadiusDelta",
			[](Slider& target) { return target.ThumbHoverRadiusDelta; },
			[](Slider& target, const float& value) { target.ThumbHoverRadiusDelta = value; },
			SliderPropertySubscriber(L"ThumbHoverRadiusDelta"), SliderMetricOptions(1.0f, 120));
		BindingPropertyRegistry::Register<Slider, float>(L"ThumbDragRadiusDelta",
			[](Slider& target) { return target.ThumbDragRadiusDelta; },
			[](Slider& target, const float& value) { target.ThumbDragRadiusDelta = value; },
			SliderPropertySubscriber(L"ThumbDragRadiusDelta"), SliderMetricOptions(2.0f, 130));
		return true;
	}();
	(void)registered;
}

Slider::Slider(int x, int y, int width, int height)
{
	this->Location = POINT{ x,y };
	this->Size = SIZE{ width,height };
	this->BackColor = D2D1_COLOR_F{ 0,0,0,0 };
	this->BorderColor = D2D1_COLOR_F{ 0,0,0,0 };
	this->Cursor = CursorKind::SizeWE;
}

GET_CPP(Slider, float, Min)
{
	return this->_min;
}
SET_CPP(Slider, float, Min)
{
	if (!SetPropertyField(L"Min", _min, value)) return;
	(void)ReevaluatePropertyValue(L"Max");
	ReevaluateValue();
}

GET_CPP(Slider, float, Max)
{
	return this->_max;
}
SET_CPP(Slider, float, Max)
{
	if (!SetPropertyField(L"Max", _max, value)) return;
	ReevaluateValue();
}

GET_CPP(Slider, float, Value)
{
	return this->_value;
}
SET_CPP(Slider, float, Value)
{
	(void)SetPropertyField(L"Value", _value, value);
}

GET_CPP(Slider, float, Step) { return _step; }
SET_CPP(Slider, float, Step)
{
	if (!SetPropertyField(L"Step", _step, value)) return;
	ReevaluateValue();
}

GET_CPP(Slider, bool, SnapToStep) { return _snapToStep; }
SET_CPP(Slider, bool, SnapToStep)
{
	if (!SetPropertyField(L"SnapToStep", _snapToStep, value)) return;
	ReevaluateValue();
}

float Slider::CoerceValue(float value) const
{
	if (!std::isfinite(value)) value = _min;
	const float maximum = (std::max)(_min, _max);
	float next = (std::clamp)(value, _min, maximum);
	if (_snapToStep && _step > 0.0f && std::isfinite(_step))
	{
		const float steps = (next - _min) / _step;
		next = _min + std::round(steps) * _step;
		next = (std::clamp)(next, _min, maximum);
	}
	return next;
}

void Slider::SetCurrentValue(float value)
{
	(void)SetCurrentPropertyField(L"Value", _value, value);
}

void Slider::ReevaluateValue()
{
	(void)ReevaluatePropertyValue(L"Value");
}

#define CUI_SLIDER_PROPERTY_IMPL(type, name, field, propertyName) \
	GET_CPP(Slider, type, name) { return field; } \
	SET_CPP(Slider, type, name) { (void)SetPropertyField(propertyName, field, value); }

CUI_SLIDER_PROPERTY_IMPL(D2D1_COLOR_F, TrackBackColor, _trackBackColor, L"TrackBackColor")
CUI_SLIDER_PROPERTY_IMPL(D2D1_COLOR_F, TrackForeColor, _trackForeColor, L"TrackForeColor")
CUI_SLIDER_PROPERTY_IMPL(D2D1_COLOR_F, TrackHoverColor, _trackHoverColor, L"TrackHoverColor")
CUI_SLIDER_PROPERTY_IMPL(D2D1_COLOR_F, TrackBorderColor, _trackBorderColor, L"TrackBorderColor")
CUI_SLIDER_PROPERTY_IMPL(D2D1_COLOR_F, ThumbColor, _thumbColor, L"ThumbColor")
CUI_SLIDER_PROPERTY_IMPL(D2D1_COLOR_F, ThumbHoverColor, _thumbHoverColor, L"ThumbHoverColor")
CUI_SLIDER_PROPERTY_IMPL(D2D1_COLOR_F, ThumbBorderColor, _thumbBorderColor, L"ThumbBorderColor")
CUI_SLIDER_PROPERTY_IMPL(D2D1_COLOR_F, ThumbShadowColor, _thumbShadowColor, L"ThumbShadowColor")
CUI_SLIDER_PROPERTY_IMPL(D2D1_COLOR_F, DisabledOverlayColor, _disabledOverlayColor, L"DisabledOverlayColor")
CUI_SLIDER_PROPERTY_IMPL(float, TrackHeight, _trackHeight, L"TrackHeight")
CUI_SLIDER_PROPERTY_IMPL(float, ThumbRadius, _thumbRadius, L"ThumbRadius")
CUI_SLIDER_PROPERTY_IMPL(float, ThumbHoverRadiusDelta, _thumbHoverRadiusDelta, L"ThumbHoverRadiusDelta")
CUI_SLIDER_PROPERTY_IMPL(float, ThumbDragRadiusDelta, _thumbDragRadiusDelta, L"ThumbDragRadiusDelta")

#undef CUI_SLIDER_PROPERTY_IMPL

void Slider::SetRange(float minValue, float maxValue)
{
	// 先设 Min 再设 Max；两个 setter 都会 ReevaluateValue 保证一致性。
	this->Min = minValue;
	this->Max = (std::max)(maxValue, minValue);
}

void Slider::Increment(float delta)
{
	this->Value = this->_value + delta;
}

void Slider::Increment()
{
	Increment(_step > 0.0f ? _step : 1.0f);
}

void Slider::Decrement(float delta)
{
	this->Value = this->_value - delta;
}

void Slider::Decrement()
{
	Decrement(_step > 0.0f ? _step : 1.0f);
}

void Slider::Reset()
{
	this->Value = this->_min;
}

CursorKind Slider::QueryCursor(int localX, int localY)
{
	(void)localY;
	if (!this->Enable) return CursorKind::Arrow;
	const float trackLeft = TrackLeftLocal();
	const float trackRight = TrackRightLocal();
	if ((float)localX >= trackLeft && (float)localX <= trackRight) return CursorKind::SizeWE;
	return this->Cursor;
}

void Slider::Update()
{
	if (!this->IsVisual) return;
	auto d2d = this->ParentForm->Render;
	const auto size = this->GetActualSizeDip();
	const float actualWidth = size.width;
	const float actualHeight = size.height;
	this->BeginRender();
	{
		const bool hover = this->ParentForm && this->ParentForm->UnderMouse == this;
		const bool active = _dragging || (this->ParentForm && this->ParentForm->Selected == this);
		const float state = active ? 1.0f : (hover ? 0.55f : 0.0f);
		float trackLeft = TrackLeftLocal();
		float trackRight = TrackRightLocal();
		if (trackRight < trackLeft) trackRight = trackLeft;
		float trackCenterY = TrackYLocal();
		float trackHeight = TrackHeight + (active ? 1.0f : 0.0f);
		float trackTop = trackCenterY - trackHeight * 0.5f;
		float trackWidth = (trackRight - trackLeft);
		if (trackWidth < 0) trackWidth = 0;

		d2d->FillRoundRect(trackLeft, trackTop, trackWidth, trackHeight, TrackBackColor, trackHeight * 0.5f);
		if (state > 0.0f && TrackHoverColor.a > 0.0f)
			d2d->FillRoundRect(trackLeft, trackTop - 1.0f, trackWidth, trackHeight + 2.0f, WithAlpha(TrackHoverColor, state), (trackHeight + 2.0f) * 0.5f);
		if (TrackBorderColor.a > 0.0f)
			d2d->DrawRoundRect(trackLeft, trackTop, trackWidth, trackHeight, TrackBorderColor, 1.0f, trackHeight * 0.5f);

		float valueRatio = std::clamp(ValueToT(), 0.0f, 1.0f);
		float filledWidth = trackWidth * valueRatio;
		if (filledWidth > 0.0f)
			d2d->FillRoundRect(trackLeft, trackTop, filledWidth, trackHeight, TrackForeColor, trackHeight * 0.5f);

		float thumbCenterX = trackLeft + trackWidth * valueRatio;
		float thumbRadius = ThumbRadius + (active ? ThumbDragRadiusDelta : (hover ? ThumbHoverRadiusDelta : 0.0f));
		if (ThumbShadowColor.a > 0.0f)
			d2d->FillEllipse(thumbCenterX, trackCenterY + 1.5f, thumbRadius + 0.8f, thumbRadius + 0.8f, WithAlpha(ThumbShadowColor, active ? 0.38f : 0.22f));
		d2d->FillEllipse(thumbCenterX, trackCenterY, thumbRadius, thumbRadius, LerpColor(ThumbColor, ThumbHoverColor, state));
		d2d->DrawEllipse(thumbCenterX, trackCenterY, thumbRadius, thumbRadius, ThumbBorderColor, active ? 1.5f : 1.0f);

		(void)size;
	}
	if (!this->Enable)
		d2d->FillRoundRect(0.0f, 0.0f, actualWidth, actualHeight, DisabledOverlayColor, 6.0f);
	this->EndRender();
}

bool Slider::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY)
{
	if (!this->Enable || !this->Visible) return true;
	switch (message)
	{
	case WM_MOUSEMOVE:
	{
		this->ParentForm->UnderMouse = this;
		if (_dragging)
		{
			SetCurrentValue(XToValue(localX));
		}
		MouseEventArgs eventArgs = MouseEventArgs(MouseButtons::None, 0, localX, localY, HIWORD(wParam));
		this->OnMouseMove(this, eventArgs);
	}
	break;
	case WM_LBUTTONDOWN:
	{
		this->ParentForm->Selected = this;
		_dragging = true;
		SetCurrentValue(XToValue(localX));
		MouseEventArgs eventArgs = MouseEventArgs(MouseButtons::Left, 0, localX, localY, HIWORD(wParam));
		this->OnMouseDown(this, eventArgs);
		this->InvalidateVisual();
	}
	break;
	case WM_LBUTTONUP:
	{
		_dragging = false;
		if (this->ParentForm->Selected == this)
		{
			MouseEventArgs eventArgs = MouseEventArgs(MouseButtons::Left, 0, localX, localY, HIWORD(wParam));
			this->OnMouseUp(this, eventArgs);
		}
		this->ParentForm->Selected = nullptr;
		this->InvalidateVisual();
	}
	break;
	default:
		return Control::ProcessMessage(message, wParam, lParam, localX, localY);
	}
	return true;
}

