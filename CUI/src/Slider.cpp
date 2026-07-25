#include "Slider.h"
#include "Window.h"
#include <cmath>
#include <utility>

UIClass Slider::Type() { return UIClass::UI_Slider; }

namespace
{
	template<typename TValue>
	DependencyPropertyOptions<Slider, TValue> SliderPropertyOptions(
		TValue defaultValue,
		const wchar_t* category,
		int categoryOrder,
		int order,
		DependencyPropertyEditorKind editor,
		DependencyPropertyFlags flags = DependencyPropertyFlags::AffectsRender)
	{
		DependencyPropertyOptions<Slider, TValue> options;
		options.DefaultValue = std::move(defaultValue);
		options.Flags = flags;
		options.Design.Category = category;
		options.Design.CategoryOrder = categoryOrder;
		options.Design.Order = order;
		options.Design.Editor = editor;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		return options;
	}

	auto SliderPropertySubscriber(const wchar_t* propertyName)
	{
		return [propertyName = std::wstring(propertyName)](
			Slider& target,
			DependencyPropertyMetadata::ChangeHandler handler,
			DataSourceUpdateMode)
		{
			return target.OnPropertyValueChanged.Subscribe(
				[propertyName, handler = std::move(handler)](
					DependencyObject*, const DependencyPropertyChangedEventArgs& args)
				{
					if (args.PropertyName == propertyName)
						handler();
				});
		};
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

void Slider::RegisterDependencyProperties()
{
	RangeBase::RegisterDependencyProperties();
	static const bool registered = []
	{
		auto changeOptions = [](double defaultValue, int order)
		{
			auto options = SliderPropertyOptions(
				defaultValue, L"Range", 100, order,
				DependencyPropertyEditorKind::Number);
			options.Coerce = [](
				Slider&, const double& proposed) -> std::optional<double>
			{
				return std::isfinite(proposed)
					? std::optional<double>{ (std::max)(0.0, proposed) }
					: std::nullopt;
			};
			options.Design.Minimum = 0.0;
			options.Design.Step = 0.1;
			return options;
		};
		DependencyPropertyRegistry::Register<Slider, double>(L"SmallChange",
			[](Slider& target) { return target.SmallChange; },
			[](Slider& target, const double& value)
			{ target.SmallChange = value; },
			SliderPropertySubscriber(L"SmallChange"), changeOptions(1.0, 40));
		DependencyPropertyRegistry::Register<Slider, double>(L"LargeChange",
			[](Slider& target) { return target.LargeChange; },
			[](Slider& target, const double& value)
			{ target.LargeChange = value; },
			SliderPropertySubscriber(L"LargeChange"), changeOptions(10.0, 50));
		DependencyPropertyRegistry::Register<Slider, double>(L"TickFrequency",
			[](Slider& target) { return target.TickFrequency; },
			[](Slider& target, const double& value)
			{ target.TickFrequency = value; },
			SliderPropertySubscriber(L"TickFrequency"), changeOptions(1.0, 60));

		auto snapOptions = SliderPropertyOptions(
			false, L"Range", 100, 70,
			DependencyPropertyEditorKind::Boolean);
		DependencyPropertyRegistry::Register<Slider, bool>(
			L"IsSnapToTickEnabled",
			[](Slider& target) { return target.IsSnapToTickEnabled; },
			[](Slider& target, const bool& value)
			{ target.IsSnapToTickEnabled = value; },
			SliderPropertySubscriber(L"IsSnapToTickEnabled"),
			std::move(snapOptions));
		return true;
	}();
	(void)registered;
}

Slider::Slider()
{
	this->RendererBackgroundColor = D2D1_COLOR_F{ 0,0,0,0 };
	this->RendererBorderColor = D2D1_COLOR_F{ 0,0,0,0 };
	(void)TrySetPropertyValue(
		L"Cursor", BindingValue(CursorKind::SizeWE),
		DependencyPropertyValueSource::Theme);
}

GET_CPP(Slider, double, SmallChange) { return _smallChange; }
SET_CPP(Slider, double, SmallChange)
{
	(void)SetPropertyField(L"SmallChange", _smallChange, value);
}

GET_CPP(Slider, double, LargeChange) { return _largeChange; }
SET_CPP(Slider, double, LargeChange)
{
	(void)SetPropertyField(L"LargeChange", _largeChange, value);
}

GET_CPP(Slider, double, TickFrequency) { return _tickFrequency; }
SET_CPP(Slider, double, TickFrequency)
{
	if (!SetPropertyField(L"TickFrequency", _tickFrequency, value)) return;
	ReevaluateRangeValue();
}

GET_CPP(Slider, bool, IsSnapToTickEnabled)
{
	return _isSnapToTickEnabled;
}
SET_CPP(Slider, bool, IsSnapToTickEnabled)
{
	if (!SetPropertyField(
		L"IsSnapToTickEnabled", _isSnapToTickEnabled, value)) return;
	ReevaluateRangeValue();
}

double Slider::CoerceRangeValue(double value) const
{
	double next = RangeBase::CoerceRangeValue(value);
	if (_isSnapToTickEnabled && _tickFrequency > 0.0
		&& std::isfinite(_tickFrequency))
	{
		const double ticks = (next - MinimumCore()) / _tickFrequency;
		next = MinimumCore() + std::round(ticks) * _tickFrequency;
		next = RangeBase::CoerceRangeValue(next);
	}
	return next;
}

void Slider::Increment(double delta)
{
	SetCurrentRangeValue(Value + delta);
}

void Slider::Increment()
{
	Increment(_smallChange > 0.0 ? _smallChange : 1.0);
}

void Slider::Decrement(double delta)
{
	SetCurrentRangeValue(Value - delta);
}

void Slider::Decrement()
{
	Decrement(_smallChange > 0.0 ? _smallChange : 1.0);
}

void Slider::Reset()
{
	SetCurrentRangeValue(Minimum);
}

CursorKind Slider::QueryCursor(int localX, int localY)
{
	(void)localY;
	if (!this->IsEnabled) return CursorKind::Arrow;
	const float trackLeft = TrackLeftLocal();
	const float trackRight = TrackRightLocal();
	if ((float)localX >= trackLeft && (float)localX <= trackRight) return CursorKind::SizeWE;
	return Control::QueryCursor(localX, localY);
}

void Slider::OnRender()
{
	if (!this->IsVisible) return;
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
		const bool hover = this->IsMouseOver;
		const bool active = _dragging || (this->GetPresentationWindow() && this->GetPresentationWindow()->GetKeyboardFocusedElement() == this);
		const float state = active ? 1.0f : (hover ? 0.55f : 0.0f);
		float trackLeft = TrackLeftLocal();
		float trackRight = TrackRightLocal();
		if (trackRight < trackLeft) trackRight = trackLeft;
		float trackCenterY = TrackYLocal();
		float trackHeight = _trackHeight + (active ? 1.0f : 0.0f);
		float trackTop = trackCenterY - trackHeight * 0.5f;
		float trackWidth = (trackRight - trackLeft);
		if (trackWidth < 0) trackWidth = 0;

		d2d->FillRoundRect(trackLeft, trackTop, trackWidth, trackHeight,
			_trackBackColor, trackHeight * 0.5f);
		if (state > 0.0f && _trackHoverColor.a > 0.0f)
			d2d->FillRoundRect(trackLeft, trackTop - 1.0f, trackWidth,
				trackHeight + 2.0f, WithAlpha(_trackHoverColor, state),
				(trackHeight + 2.0f) * 0.5f);
		if (_trackBorderColor.a > 0.0f)
			d2d->DrawRoundRect(trackLeft, trackTop, trackWidth, trackHeight,
				_trackBorderColor, 1.0f, trackHeight * 0.5f);

		float valueRatio = std::clamp(ValueToT(), 0.0f, 1.0f);
		float filledWidth = trackWidth * valueRatio;
		if (filledWidth > 0.0f)
			d2d->FillRoundRect(trackLeft, trackTop, filledWidth, trackHeight,
				_trackForeColor, trackHeight * 0.5f);

		float thumbCenterX = trackLeft + trackWidth * valueRatio;
		float thumbRadius = _thumbRadius + (active
			? _thumbDragRadiusDelta : (hover ? _thumbHoverRadiusDelta : 0.0f));
		if (_thumbShadowColor.a > 0.0f)
			d2d->FillEllipse(thumbCenterX, trackCenterY + 1.5f,
				thumbRadius + 0.8f, thumbRadius + 0.8f,
				WithAlpha(_thumbShadowColor, active ? 0.38f : 0.22f));
		d2d->FillEllipse(thumbCenterX, trackCenterY, thumbRadius, thumbRadius,
			LerpColor(_thumbColor, _thumbHoverColor, state));
		d2d->DrawEllipse(thumbCenterX, trackCenterY, thumbRadius, thumbRadius,
			_thumbBorderColor, active ? 1.5f : 1.0f);

		(void)size;
	}
	if (!this->IsEnabled)
		d2d->FillRoundRect(0.0f, 0.0f, actualWidth, actualHeight,
			_disabledOverlayColor, 6.0f);
	this->EndRender();
}

bool Slider::ProcessInput(const InputReport& input)
{
	if (!this->IsEnabled || !this->IsVisible) return true;
	switch (input.Kind)
	{
	case InputReportKind::PointerMove:
	{
		if (_dragging)
		{
			SetCurrentRangeValue(XToValue(input.X));
		}
		auto eventArgs = input.CreateMouseEventArgs();
		this->OnMouseMove(this, eventArgs);
	}
	break;
	case InputReportKind::PointerDown:
	{
		if (input.ChangedButton != MouseButton::Left)
			return Control::ProcessInput(input);
		this->GetPresentationWindow()->SetKeyboardFocus(this, true);
		_dragging = true;
		(void)CaptureMouse();
		SetCurrentRangeValue(XToValue(input.X));
		auto eventArgs = input.CreateMouseEventArgs();
		this->OnMouseDown(this, eventArgs);
		this->InvalidateVisual();
	}
	break;
	case InputReportKind::PointerUp:
	{
		if (input.ChangedButton != MouseButton::Left)
			return Control::ProcessInput(input);
		_dragging = false;
		if (this->GetPresentationWindow()->GetKeyboardFocusedElement() == this)
		{
			auto eventArgs = input.CreateMouseEventArgs();
			this->OnMouseUp(this, eventArgs);
		}
		if (IsMouseCaptured()) (void)ReleaseMouseCapture();
		this->InvalidateVisual();
	}
	break;
	case InputReportKind::Cancel:
	case InputReportKind::CaptureLost:
		_dragging = false;
		if (input.Kind == InputReportKind::Cancel && IsMouseCaptured())
			(void)ReleaseMouseCapture();
		InvalidateVisual();
		return Control::ProcessInput(input);
	default:
		return Control::ProcessInput(input);
	}
	return true;
}
