#include "Slider.h"
#include "Canvas.h"
#include "InputManager.h"
#include <cmath>
#include <utility>
#include <vector>

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

	float FixedOrActualExtent(
		const Control* control,
		bool horizontal,
		float fallback)
	{
		if (!control) return fallback;
		const auto& layout = control->GetSpecifiedLayout();
		const auto specified = horizontal
			? layout.width : layout.height;
		if (specified.IsFixed())
			return (std::max)(0.0f, specified.value);
		const auto actual = control->GetActualSizeDip();
		const float extent = horizontal ? actual.width : actual.height;
		return extent > 0.0f ? extent : fallback;
	}

	void SetFixedExtent(Control* control, bool horizontal, float value)
	{
		if (!control) return;
		const auto length =
			cui::layout::Length::Fixed((std::max)(0.0f, value));
		if (horizontal)
		{
			if (control->Width != length) control->Width = length;
		}
		else if (control->Height != length)
			control->Height = length;
	}

	void SetCanvasPosition(Control* control, float left, float top)
	{
		if (!control) return;
		if (Canvas::GetLeft(*control) != left)
			Canvas::SetLeft(*control, left);
		if (Canvas::GetTop(*control) != top)
			Canvas::SetTop(*control, top);
	}
}

void Slider::EnsureClassHandlers()
{
	static const std::vector<EventConnection> handlers = []
	{
		std::vector<EventConnection> result;
		result.push_back(RoutedEventManager::RegisterClassHandler(
			UIClass::UI_Slider, RoutedEventId::MouseDown,
			&Slider::HandleDescendantPointerPress));
		result.push_back(RoutedEventManager::RegisterClassHandler(
			UIClass::UI_Slider, RoutedEventId::MouseDoubleClick,
			&Slider::HandleDescendantPointerPress));
		return result;
	}();
	(void)handlers;
}

void Slider::HandleDescendantPointerPress(
	Control* sender, RoutedEventArgs& args)
{
	auto* slider = dynamic_cast<Slider*>(sender);
	auto& mouse = static_cast<MouseEventArgs&>(args);
	if (!slider || args.OriginalSource == slider
		|| mouse.ChangedButton != MouseButton::Left
		|| !slider->IsEffectivelyEnabled() || !slider->IsVisible) return;
	slider->BeginPointerInteraction(mouse.X, mouse.Y);
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

		auto orientationOptions = SliderPropertyOptions(
			Orientation::Horizontal, L"Layout", 50, 10,
			DependencyPropertyEditorKind::Choice,
			DependencyPropertyFlags::AffectsMeasure
				| DependencyPropertyFlags::AffectsArrange
				| DependencyPropertyFlags::AffectsRender);
		orientationOptions.Design.Choices = {
			{ L"Horizontal", BindingValue(Orientation::Horizontal) },
			{ L"Vertical", BindingValue(Orientation::Vertical) }
		};
		DependencyPropertyRegistry::Register<Slider, ::Orientation>(
			L"Orientation",
			[](Slider& target) { return target.Orientation; },
			[](Slider& target, const ::Orientation& value)
			{ target.Orientation = value; },
			SliderPropertySubscriber(L"Orientation"),
			std::move(orientationOptions));

		auto directionOptions = SliderPropertyOptions(
			false, L"Behavior", 300, 80,
			DependencyPropertyEditorKind::Boolean,
			DependencyPropertyFlags::AffectsRender);
		DependencyPropertyRegistry::Register<Slider, bool>(
			L"IsDirectionReversed",
			[](Slider& target) { return target.IsDirectionReversed; },
			[](Slider& target, const bool& value)
			{ target.IsDirectionReversed = value; },
			SliderPropertySubscriber(L"IsDirectionReversed"),
			std::move(directionOptions));
		return true;
	}();
	(void)registered;
}

Slider::Slider()
{
	EnsureClassHandlers();
	RegisterDependencyProperties();
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

GET_CPP(Slider, ::Orientation, Orientation)
{
	return _orientation;
}

SET_CPP(Slider, ::Orientation, Orientation)
{
	if (!SetPropertyField(L"Orientation", _orientation, value)) return;
	UpdateTemplateParts();
}

GET_CPP(Slider, bool, IsDirectionReversed)
{
	return _isDirectionReversed;
}

SET_CPP(Slider, bool, IsDirectionReversed)
{
	if (!SetPropertyField(
		L"IsDirectionReversed", _isDirectionReversed, value)) return;
	UpdateTemplateParts();
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
	(void)localX;
	(void)localY;
	if (!this->IsEnabled) return CursorKind::Arrow;
	return _orientation == Orientation::Horizontal
		? CursorKind::SizeWE : CursorKind::SizeNS;
}

bool Slider::HandlesNavigationKey(Key key) const
{
	switch (key)
	{
	case Key::Left:
	case Key::Right:
	case Key::Up:
	case Key::Down:
	case Key::Home:
	case Key::End:
	case Key::PageUp:
	case Key::PageDown:
		return true;
	default:
		return false;
	}
}

bool Slider::ProcessInput(const InputReport& input)
{
	if (!this->IsEnabled || !this->IsVisible) return true;
	switch (input.Kind)
	{
	case InputReportKind::PointerMove:
	{
		if (_dragging)
			SetCurrentRangeValue(PointToValue(input.X, input.Y));
		auto eventArgs = input.CreateMouseEventArgs();
		this->OnMouseMove(this, eventArgs);
	}
	break;
	case InputReportKind::PointerDown:
	{
		if (input.ChangedButton != MouseButton::Left)
			return Control::ProcessInput(input);
		BeginPointerInteraction(input.X, input.Y);
		auto eventArgs = input.CreateMouseEventArgs();
		this->OnMouseDown(this, eventArgs);
	}
	break;
	case InputReportKind::PointerUp:
	{
		if (input.ChangedButton != MouseButton::Left)
			return Control::ProcessInput(input);
		if (_dragging)
			SetCurrentRangeValue(PointToValue(input.X, input.Y));
		_dragging = false;
		auto eventArgs = input.CreateMouseEventArgs();
		this->OnMouseUp(this, eventArgs);
		if (IsMouseCaptured()) (void)ReleaseMouseCapture();
		UpdateTemplateParts();
	}
	break;
	case InputReportKind::KeyDown:
	{
		if (input.Modifiers != ModifierKeys::None)
			return Control::ProcessInput(input);
		bool handled = true;
		const bool horizontal =
			_orientation == Orientation::Horizontal;
		const bool inverted = _isDirectionReversed;
		switch (input.Key)
		{
		case Key::Left:
			if (!horizontal) handled = false;
			else if (inverted) Increment();
			else Decrement();
			break;
		case Key::Right:
			if (!horizontal) handled = false;
			else if (inverted) Decrement();
			else Increment();
			break;
		case Key::Up:
			if (horizontal) handled = false;
			else if (inverted) Decrement();
			else Increment();
			break;
		case Key::Down:
			if (horizontal) handled = false;
			else if (inverted) Increment();
			else Decrement();
			break;
		case Key::PageUp:
			Increment(_largeChange > 0.0 ? _largeChange : 10.0);
			break;
		case Key::PageDown:
			Decrement(_largeChange > 0.0 ? _largeChange : 10.0);
			break;
		case Key::Home:
			SetCurrentRangeValue(Minimum);
			break;
		case Key::End:
			SetCurrentRangeValue(Maximum);
			break;
		default:
			handled = false;
			break;
		}
		const bool routed = Control::ProcessInput(input);
		return handled || routed;
	}
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

Slider::TrackGeometry Slider::ResolveTrackGeometry() const
{
	TrackGeometry result;
	const auto* track =
		FindDeclarativeTemplatePart(L"PART_Track");
	if (track)
	{
		const auto trackSize = track->GetActualSizeDip();
		const auto trackOrigin = track->GetAbsoluteLocationDip();
		const auto ownerOrigin = GetAbsoluteLocationDip();
		result.Left = trackOrigin.x - ownerOrigin.x;
		result.Top = trackOrigin.y - ownerOrigin.y;
		result.Width = trackSize.width;
		result.Height = trackSize.height;
		if (result.Width > 0.0f && result.Height > 0.0f)
			return result;
	}

	const auto ownerSize = GetActualSizeDip();
	const auto padding = GetSpecifiedLayout().padding;
	result.Left = (std::max)(0.0f, padding.left);
	result.Top = (std::max)(0.0f, padding.top);
	result.Width = (std::max)(0.0f,
		ownerSize.width - padding.left - padding.right);
	result.Height = (std::max)(0.0f,
		ownerSize.height - padding.top - padding.bottom);
	return result;
}

float Slider::ValueRatio() const
{
	const double range = MaximumCore() - MinimumCore();
	if (range <= 0.0000001) return 0.0f;
	return static_cast<float>((std::clamp)(
		(ValueCore() - MinimumCore()) / range, 0.0, 1.0));
}

float Slider::PositionRatio() const
{
	const float value = ValueRatio();
	if (_orientation == Orientation::Horizontal)
		return _isDirectionReversed ? 1.0f - value : value;
	return _isDirectionReversed ? value : 1.0f - value;
}

double Slider::PointToValue(int localX, int localY) const
{
	const auto geometry = ResolveTrackGeometry();
	const auto* thumb =
		FindDeclarativeTemplatePart(L"PART_Thumb");
	const bool horizontal =
		_orientation == Orientation::Horizontal;
	const float thumbExtent = FixedOrActualExtent(
		thumb, horizontal, 18.0f);
	const float axisStart = horizontal
		? geometry.Left : geometry.Top;
	const float axisExtent = horizontal
		? geometry.Width : geometry.Height;
	const float usable = (std::max)(0.0f,
		axisExtent - thumbExtent);
	float ratio = usable > 0.0f
		? ((horizontal ? static_cast<float>(localX)
			: static_cast<float>(localY))
			- axisStart - thumbExtent * 0.5f) / usable
		: 0.0f;
	ratio = (std::clamp)(ratio, 0.0f, 1.0f);
	if (horizontal)
	{
		if (_isDirectionReversed) ratio = 1.0f - ratio;
	}
	else if (_isDirectionReversed)
	{
		// Reversed vertical tracks increase from top to bottom.
	}
	else
		ratio = 1.0f - ratio;
	return MinimumCore() + static_cast<double>(ratio)
		* (MaximumCore() - MinimumCore());
}

void Slider::BeginPointerInteraction(int localX, int localY)
{
	(void)Focus();
	_dragging = true;
	(void)CaptureMouse();
	SetCurrentRangeValue(PointToValue(localX, localY));
	UpdateTemplateParts();
}

void Slider::OnRangeValueChanged(double oldValue, double newValue)
{
	(void)oldValue;
	(void)newValue;
	UpdateTemplateParts();
}

void Slider::OnComputedLayoutSizeChanged()
{
	UpdateTemplateParts();
}

void Slider::OnControlTemplatePresentationChanged()
{
	ClearTemplatePartEventConnections();
	if (auto* track = FindDeclarativeTemplatePart(L"PART_Track"))
	{
		const ControlWeakReference lifetime(this);
		RetainTemplatePartEventConnection(track->SizeChanged.Subscribe(
			[lifetime](Control*, SizeChangedEventArgs&)
			{
				auto* slider =
					dynamic_cast<Slider*>(lifetime.Get());
				if (slider) slider->UpdateTemplateParts();
			}));
	}
	UpdateTemplateParts();
}

void Slider::UpdateTemplateParts()
{
	auto* track = FindDeclarativeTemplatePart(L"PART_Track");
	auto* trackBackground =
		FindDeclarativeTemplatePart(L"PART_TrackBackground");
	auto* selection =
		FindDeclarativeTemplatePart(L"PART_SelectionRange");
	auto* thumb = FindDeclarativeTemplatePart(L"PART_Thumb");
	if (!track || !trackBackground || !selection || !thumb) return;

	const auto trackSize = track->GetActualSizeDip();
	const float thumbWidth =
		FixedOrActualExtent(thumb, true, 18.0f);
	const float thumbHeight =
		FixedOrActualExtent(thumb, false, 18.0f);
	const float lineThickness = 5.0f;
	const float position = PositionRatio();

	if (_orientation == Orientation::Horizontal)
	{
		const float lineStart = thumbWidth * 0.5f;
		const float lineLength = (std::max)(
			0.0f, trackSize.width - thumbWidth);
		const float center =
			lineStart + lineLength * position;
		const float lineTop = (std::max)(
			0.0f, (trackSize.height - lineThickness) * 0.5f);
		SetCanvasPosition(
			trackBackground, lineStart, lineTop);
		SetFixedExtent(trackBackground, true, lineLength);
		SetFixedExtent(trackBackground, false, lineThickness);

		const float selectionLeft = _isDirectionReversed
			? center : lineStart;
		const float selectionWidth = _isDirectionReversed
			? lineStart + lineLength - center
			: center - lineStart;
		SetCanvasPosition(selection, selectionLeft, lineTop);
		SetFixedExtent(selection, true, selectionWidth);
		SetFixedExtent(selection, false, lineThickness);
		SetCanvasPosition(thumb,
			center - thumbWidth * 0.5f,
			(trackSize.height - thumbHeight) * 0.5f);
	}
	else
	{
		const float lineStart = thumbHeight * 0.5f;
		const float lineLength = (std::max)(
			0.0f, trackSize.height - thumbHeight);
		const float center =
			lineStart + lineLength * position;
		const float lineLeft = (std::max)(
			0.0f, (trackSize.width - lineThickness) * 0.5f);
		SetCanvasPosition(
			trackBackground, lineLeft, lineStart);
		SetFixedExtent(trackBackground, true, lineThickness);
		SetFixedExtent(trackBackground, false, lineLength);

		const float selectionTop = _isDirectionReversed
			? lineStart : center;
		const float selectionHeight = _isDirectionReversed
			? center - lineStart
			: lineStart + lineLength - center;
		SetCanvasPosition(selection, lineLeft, selectionTop);
		SetFixedExtent(selection, true, lineThickness);
		SetFixedExtent(selection, false, selectionHeight);
		SetCanvasPosition(thumb,
			(trackSize.width - thumbWidth) * 0.5f,
			center - thumbHeight * 0.5f);
	}
}
