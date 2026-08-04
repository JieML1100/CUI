#include "Slider.h"
#include "Canvas.h"
#include "InputManager.h"
#include <cmath>
#include <optional>
#include <stdexcept>
#include <typeindex>
#include <utility>
#include <vector>

UIClass Slider::Type() { return UIClass::UI_Slider; }

namespace
{
	template<typename TValue>
	DependencyPropertyOptions<Slider, TValue> SliderPropertyOptions(
		TValue defaultValue
		CUI_DESIGN_METADATA_ARGUMENTS(
			const wchar_t* category,
			int categoryOrder,
			int order,
			DependencyPropertyEditorKind editor),
		DependencyPropertyFlags flags = DependencyPropertyFlags::AffectsRender)
	{
		DependencyPropertyOptions<Slider, TValue> options;
		options.DefaultValue = std::move(defaultValue);
		options.Flags = flags;
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = category;
		options.Design.CategoryOrder = categoryOrder;
		options.Design.Order = order;
		options.Design.Editor = editor;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		)
		return options;
	}

	DependencyPropertyOptions<Slider, double> SliderChangeOptions(
		double defaultValue CUI_DESIGN_METADATA_ARGUMENTS(int order))
	{
		auto options = SliderPropertyOptions(
			defaultValue CUI_DESIGN_METADATA_ARGUMENTS(
				L"Range", 100, order,
				DependencyPropertyEditorKind::Number));
		options.Validate = [](const double& proposed)
		{
			return std::isfinite(proposed);
		};
		options.Coerce = [](
			Slider&, const double& proposed) -> std::optional<double>
		{
			return (std::max)(0.0, proposed);
		};
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Minimum = 0.0;
		options.Design.Step = 0.1;
		)
		return options;
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

	const DependencyPropertyMetadataRegistration&
		SliderMaximumMetadataRelation()
	{
		static const DependencyPropertyMetadataRegistration relation = []
		{
			const auto& property = RangeBase::MaximumProperty();
			DependencyPropertyOptions<Slider, double> options;
			options.DefaultValue = 10.0;
			options.Flags = DependencyPropertyFlags::AffectsMeasure
				| DependencyPropertyFlags::AffectsRender;
			CUI_DESIGN_METADATA_ONLY(
			const std::type_index rangeOwner[] = {
				std::type_index(typeid(RangeBase))
			};
			const auto* base =
				DependencyPropertyRegistry::FindRegistered(
					rangeOwner, L"Maximum");
			if (!base)
				throw std::logic_error(
					"RangeBase.Maximum must be registered before Slider");
			options.Design = base->Design();
			)
			return DependencyPropertyRegistry::OverrideMetadataStatic<
				Slider, RangeBase, double>(property, std::move(options));
		}();
		return relation;
	}

	const DependencyPropertyMetadataRegistration&
		SliderFocusableMetadataRelation()
	{
		static const DependencyPropertyMetadataRegistration relation = []
		{
			const auto& property = Control::FocusableProperty();
			DependencyPropertyOptions<Slider, bool> options;
			options.DefaultValue = true;
			CUI_DESIGN_METADATA_ONLY(
			const std::type_index controlOwner[] = {
				std::type_index(typeid(Control))
			};
			const auto* base =
				DependencyPropertyRegistry::FindRegistered(
					controlOwner, L"Focusable");
			if (!base)
				throw std::logic_error(
					"Control.Focusable must be registered before Slider");
			options.Design = base->Design();
			)
			return DependencyPropertyRegistry::OverrideMetadataStatic<
				Slider, RangeBase, bool>(property, std::move(options));
		}();
		return relation;
	}
}

const DependencyProperty& Slider::TickFrequencyProperty()
{
	static const auto registration = []
	{
		auto options = SliderChangeOptions(1.0
			CUI_DESIGN_METADATA_ARGUMENTS(60));
		options.Coerce = {};
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Minimum.reset();
		)
		return DependencyPropertyRegistry::RegisterStatic<Slider, double>(
			DependencyPropertyRegistrationLiteral(L"TickFrequency"),
			[](Slider& target) { return target.TickFrequency; },
			[](Slider& target, const double& value)
			{ target.TickFrequency = value; }, {}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& Slider::OrientationProperty()
{
	static const auto registration = []
	{
		auto options = SliderPropertyOptions(
			Orientation::Horizontal
			CUI_DESIGN_METADATA_ARGUMENTS(
				L"Layout", 50, 10, DependencyPropertyEditorKind::Choice),
			DependencyPropertyFlags::AffectsMeasure
				| DependencyPropertyFlags::AffectsArrange
				| DependencyPropertyFlags::AffectsRender);
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Choices = {
			{ L"Horizontal", BindingValue(Orientation::Horizontal) },
			{ L"Vertical", BindingValue(Orientation::Vertical) }
		};
		)
		return DependencyPropertyRegistry::RegisterStatic<Slider, ::Orientation>(
			DependencyPropertyRegistrationLiteral(L"Orientation"),
			[](Slider& target) { return target.Orientation; },
			[](Slider& target, const ::Orientation& value)
			{ target.Orientation = value; }, {}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& Slider::IsSelectionRangeEnabledProperty()
{
	static const auto registration = []
	{
		auto options = SliderPropertyOptions(
			false CUI_DESIGN_METADATA_ARGUMENTS(
				L"Appearance", 200, 10,
				DependencyPropertyEditorKind::Boolean));
		return DependencyPropertyRegistry::RegisterStatic<Slider, bool>(
			DependencyPropertyRegistrationLiteral(L"IsSelectionRangeEnabled"),
			[](Slider& target) { return target.IsSelectionRangeEnabled; },
			[](Slider& target, const bool& value)
			{ target.IsSelectionRangeEnabled = value; }, {}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& Slider::IsThumbDraggingProperty()
{
	return IsThumbDraggingPropertyKey().Property();
}

const DependencyPropertyKey& Slider::IsThumbDraggingPropertyKey()
{
	static const auto registration = []
	{
		auto options = SliderPropertyOptions(
			false CUI_DESIGN_METADATA_ARGUMENTS(
				L"State", 70, 10,
				DependencyPropertyEditorKind::Boolean));
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Persistence = DependencyPropertyPersistence::Transient;
		options.Design.Browsable = false;
		)
		return DependencyPropertyRegistry::RegisterReadOnlyStatic<Slider, bool>(
			DependencyPropertyRegistrationLiteral(L"IsThumbDragging"),
			[](Slider& target) { return target.IsThumbDragging; },
			[](Slider& target, const bool& value)
			{
				(void)target.SetReadOnlyPropertyField(
					IsThumbDraggingPropertyKey(),
					target._isThumbDragging, value);
			}, {}, std::move(options));
	}();
	return registration.Key();
}

const DependencyProperty& Slider::IsSnapToTickEnabledProperty()
{
	static const auto registration = []
	{
		auto options = SliderPropertyOptions(
			false CUI_DESIGN_METADATA_ARGUMENTS(
				L"Range", 100, 70,
				DependencyPropertyEditorKind::Boolean));
		return DependencyPropertyRegistry::RegisterStatic<Slider, bool>(
			DependencyPropertyRegistrationLiteral(L"IsSnapToTickEnabled"),
			[](Slider& target) { return target.IsSnapToTickEnabled; },
			[](Slider& target, const bool& value)
			{ target.IsSnapToTickEnabled = value; }, {}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& Slider::IsMoveToPointEnabledProperty()
{
	static const auto registration = []
	{
		auto options = SliderPropertyOptions(
			false CUI_DESIGN_METADATA_ARGUMENTS(
				L"Behavior", 300, 10,
				DependencyPropertyEditorKind::Boolean));
		return DependencyPropertyRegistry::RegisterStatic<Slider, bool>(
			DependencyPropertyRegistrationLiteral(L"IsMoveToPointEnabled"),
			[](Slider& target) { return target.IsMoveToPointEnabled; },
			[](Slider& target, const bool& value)
			{ target.IsMoveToPointEnabled = value; }, {}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& Slider::SelectionStartProperty()
{
	static const auto registration = []
	{
		auto options = SliderChangeOptions(0.0
			CUI_DESIGN_METADATA_ARGUMENTS(80));
		options.Flags = DependencyPropertyFlags::AffectsRender;
		options.Coerce = [](
			Slider& target, const double& proposed) -> std::optional<double>
		{
			return (std::clamp)(proposed, target.Minimum, target.Maximum);
		};
		options.Changed = [](
			Slider& target, const double&, const double&)
		{
			(void)target.ReevaluatePropertyValue(SelectionEndProperty());
			target.UpdateTemplateParts();
		};
		return DependencyPropertyRegistry::RegisterStatic<Slider, double>(
			DependencyPropertyRegistrationLiteral(L"SelectionStart"),
			[](Slider& target) { return target.SelectionStart; },
			[](Slider& target, const double& value)
			{ target.SelectionStart = value; }, {}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& Slider::SelectionEndProperty()
{
	static const auto registration = []
	{
		auto options = SliderChangeOptions(0.0
			CUI_DESIGN_METADATA_ARGUMENTS(90));
		options.Flags = DependencyPropertyFlags::AffectsRender;
		options.Coerce = [](
			Slider& target, const double& proposed) -> std::optional<double>
		{
			return (std::clamp)(
				proposed, target.SelectionStart, target.Maximum);
		};
		options.Changed = [](
			Slider& target, const double&, const double&)
		{
			target.UpdateTemplateParts();
		};
		return DependencyPropertyRegistry::RegisterStatic<Slider, double>(
			DependencyPropertyRegistrationLiteral(L"SelectionEnd"),
			[](Slider& target) { return target.SelectionEnd; },
			[](Slider& target, const double& value)
			{ target.SelectionEnd = value; }, {}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& Slider::IsDirectionReversedProperty()
{
	static const auto registration = []
	{
		auto options = SliderPropertyOptions(
			false CUI_DESIGN_METADATA_ARGUMENTS(
				L"Behavior", 300, 80,
				DependencyPropertyEditorKind::Boolean),
			DependencyPropertyFlags::AffectsRender);
		return DependencyPropertyRegistry::RegisterStatic<Slider, bool>(
			DependencyPropertyRegistrationLiteral(L"IsDirectionReversed"),
			[](Slider& target) { return target.IsDirectionReversed; },
			[](Slider& target, const bool& value)
			{ target.IsDirectionReversed = value; }, {}, std::move(options));
	}();
	return *registration;
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
	slider->HandlePointerPress(
		mouse.X, mouse.Y,
		slider->IsOriginalSourceWithinThumb(args.OriginalSource));
}

void Slider::RegisterDependencyProperties()
{
	RangeBase::RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	(void)TickFrequencyProperty();
	(void)OrientationProperty();
	(void)IsSelectionRangeEnabledProperty();
	(void)IsThumbDraggingProperty();
	(void)IsSnapToTickEnabledProperty();
	(void)IsMoveToPointEnabledProperty();
	(void)SelectionStartProperty();
	(void)SelectionEndProperty();
	(void)IsDirectionReversedProperty();
#endif
	CUI_DESIGN_METADATA_ONLY(
	(void)SliderMaximumMetadataRelation();
	(void)SliderFocusableMetadataRelation();
	)
}

Slider::Slider()
	: RangeBase(10.0)
{
	EnsureClassHandlers();
	RegisterDependencyProperties();
}

const DependencyPropertyMetadata*
Slider::ResolveExactDependencyPropertyMetadata(
	const DependencyProperty& property) const
{
	if (&property == &RangeBase::MaximumProperty())
		return &SliderMaximumMetadataRelation().Metadata();
	if (&property == &Control::FocusableProperty())
		return &SliderFocusableMetadataRelation().Metadata();
	return RangeBase::ResolveExactDependencyPropertyMetadata(property);
}

GET_CPP(Slider, double, TickFrequency) { return _tickFrequency; }
SET_CPP(Slider, double, TickFrequency)
{
	(void)SetPropertyField(TickFrequencyProperty(), _tickFrequency, value);
}

GET_CPP(Slider, bool, IsSnapToTickEnabled)
{
	return _isSnapToTickEnabled;
}
SET_CPP(Slider, bool, IsSnapToTickEnabled)
{
	(void)SetPropertyField(
		IsSnapToTickEnabledProperty(), _isSnapToTickEnabled, value);
}

GET_CPP(Slider, bool, IsMoveToPointEnabled)
{
	return _isMoveToPointEnabled;
}

SET_CPP(Slider, bool, IsMoveToPointEnabled)
{
	(void)SetPropertyField(
		IsMoveToPointEnabledProperty(), _isMoveToPointEnabled, value);
}

GET_CPP(Slider, bool, IsSelectionRangeEnabled)
{
	return _isSelectionRangeEnabled;
}

SET_CPP(Slider, bool, IsSelectionRangeEnabled)
{
	(void)SetPropertyField(
		IsSelectionRangeEnabledProperty(),
		_isSelectionRangeEnabled, value);
}

GET_CPP(Slider, double, SelectionStart)
{
	return _selectionStart;
}

SET_CPP(Slider, double, SelectionStart)
{
	(void)SetPropertyField(
		SelectionStartProperty(), _selectionStart, value);
}

GET_CPP(Slider, double, SelectionEnd)
{
	return _selectionEnd;
}

SET_CPP(Slider, double, SelectionEnd)
{
	(void)SetPropertyField(
		SelectionEndProperty(), _selectionEnd, value);
}

GET_CPP(Slider, ::Orientation, Orientation)
{
	return _orientation;
}

SET_CPP(Slider, ::Orientation, Orientation)
{
	if (!SetPropertyField(OrientationProperty(), _orientation, value)) return;
	UpdateTemplateParts();
}

GET_CPP(Slider, bool, IsDirectionReversed)
{
	return _isDirectionReversed;
}

SET_CPP(Slider, bool, IsDirectionReversed)
{
	if (!SetPropertyField(
		IsDirectionReversedProperty(), _isDirectionReversed, value)) return;
	UpdateTemplateParts();
}

GET_CPP(Slider, bool, IsThumbDragging)
{
	return _isThumbDragging;
}

void Slider::Increment(double delta)
{
	MoveToNextTick(delta);
}

void Slider::Increment()
{
	MoveToNextTick(SmallChange);
}

void Slider::Decrement(double delta)
{
	MoveToNextTick(-delta);
}

void Slider::Decrement()
{
	MoveToNextTick(-SmallChange);
}

void Slider::Reset()
{
	SetCurrentRangeValue(Minimum);
}

CursorKind Slider::QueryCursor(int localX, int localY)
{
	(void)localX;
	(void)localY;
	return CursorKind::Arrow;
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
	bool handled = false;
	switch (input.Kind)
	{
	case InputReportKind::PointerMove:
		if (_isThumbDragging)
		{
			UpdateValueFromInput(
				ValueFromDragDelta(input.X, input.Y));
			handled = true;
		}
		break;
	case InputReportKind::PointerDown:
		if (input.ChangedButton != MouseButton::Left)
			return Control::ProcessInput(input);
		HandlePointerPress(
			input.X, input.Y,
			IsPointOverThumb(input.X, input.Y));
		handled = true;
		break;
	case InputReportKind::PointerUp:
		if (input.ChangedButton != MouseButton::Left)
			return Control::ProcessInput(input);
		if (_isThumbDragging)
		{
			UpdateValueFromInput(
				ValueFromDragDelta(input.X, input.Y));
			SetThumbDragging(false);
			handled = true;
		}
		if (IsMouseCaptured()) (void)ReleaseMouseCapture();
		break;
	case InputReportKind::KeyDown:
	{
		if (input.Modifiers != ModifierKeys::None)
			return Control::ProcessInput(input);
		handled = true;
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
			MoveToNextTick(LargeChange);
			break;
		case Key::PageDown:
			MoveToNextTick(-LargeChange);
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
		break;
	}
	case InputReportKind::Cancel:
	case InputReportKind::CaptureLost:
		if (_isThumbDragging)
		{
			SetThumbDragging(false);
			handled = true;
		}
		if (input.Kind == InputReportKind::Cancel && IsMouseCaptured())
			(void)ReleaseMouseCapture();
		break;
	default:
		return Control::ProcessInput(input);
	}
	const bool routed = Control::ProcessInput(input);
	return handled || routed;
}

Slider::TrackGeometry Slider::ResolveTrackGeometry() const
{
	TrackGeometry result;
	const auto* track =
		FindDeclarativeTemplatePart(
			MakeTemplatePartToken(L"PART_Track"));
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
	return PositionRatio(ValueCore());
}

float Slider::PositionRatio(double value) const
{
	const double range = MaximumCore() - MinimumCore();
	const float normalized = range <= 0.0000001
		? 0.0f
		: static_cast<float>((std::clamp)(
			(value - MinimumCore()) / range, 0.0, 1.0));
	if (_orientation == Orientation::Horizontal)
		return _isDirectionReversed
			? 1.0f - normalized : normalized;
	return _isDirectionReversed
		? normalized : 1.0f - normalized;
}

double Slider::PointToValue(int localX, int localY) const
{
	const auto geometry = ResolveTrackGeometry();
	const auto* thumb =
		FindDeclarativeTemplatePart(
			MakeTemplatePartToken(L"PART_Thumb"));
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

double Slider::ValueFromDragDelta(
	int localX, int localY) const
{
	const auto geometry = ResolveTrackGeometry();
	const auto* thumb =
		FindDeclarativeTemplatePart(
			MakeTemplatePartToken(L"PART_Thumb"));
	const bool horizontal =
		_orientation == Orientation::Horizontal;
	const float thumbExtent = FixedOrActualExtent(
		thumb, horizontal, 18.0f);
	const float axisExtent = horizontal
		? geometry.Width : geometry.Height;
	const double usable = static_cast<double>(
		(std::max)(0.0f, axisExtent - thumbExtent));
	if (usable <= 0.0000001)
		return _dragStartValue;

	const double pixelDelta = horizontal
		? static_cast<double>(localX - _dragStartX)
		: static_cast<double>(localY - _dragStartY);
	double direction = 1.0;
	if (horizontal)
		direction = _isDirectionReversed ? -1.0 : 1.0;
	else
		direction = _isDirectionReversed ? 1.0 : -1.0;
	const double range = MaximumCore() - MinimumCore();
	return _dragStartValue
		+ direction * pixelDelta * range / usable;
}

double Slider::SnapToTick(double value) const
{
	double next = RangeBase::CoerceRangeValue(value);
	if (!_isSnapToTickEnabled
		|| _tickFrequency <= 0.0
		|| !std::isfinite(_tickFrequency))
		return next;

	const double ticks =
		(next - MinimumCore()) / _tickFrequency;
	next = MinimumCore()
		+ std::round(ticks) * _tickFrequency;
	return RangeBase::CoerceRangeValue(next);
}

void Slider::UpdateValueFromInput(double value)
{
	SetCurrentRangeValue(SnapToTick(value));
}

void Slider::MoveToNextTick(double direction)
{
	if (direction == 0.0 || !std::isfinite(direction))
		return;

	const double current = ValueCore();
	double next = SnapToTick(
		RangeBase::CoerceRangeValue(current + direction));
	if (std::fabs(next - current) <= 0.0000001
		&& ((_isSnapToTickEnabled
			&& _tickFrequency > 0.0
			&& std::isfinite(_tickFrequency))))
	{
		const double tick = direction > 0.0
			? std::floor(
				(current - MinimumCore()) / _tickFrequency
				+ 0.0000001) + 1.0
			: std::ceil(
				(current - MinimumCore()) / _tickFrequency
				- 0.0000001) - 1.0;
		next = RangeBase::CoerceRangeValue(
			MinimumCore() + tick * _tickFrequency);
	}
	SetCurrentRangeValue(next);
}

bool Slider::IsPointOverThumb(
	int localX, int localY) const
{
	const auto* thumb =
		FindDeclarativeTemplatePart(
			MakeTemplatePartToken(L"PART_Thumb"));
	if (!thumb) return false;
	const auto thumbSize = thumb->GetActualSizeDip();
	const auto thumbOrigin = thumb->GetAbsoluteLocationDip();
	const auto ownerOrigin = GetAbsoluteLocationDip();
	const float left =
		thumbOrigin.x - ownerOrigin.x;
	const float top =
		thumbOrigin.y - ownerOrigin.y;
	return static_cast<float>(localX) >= left
		&& static_cast<float>(localX) <= left + thumbSize.width
		&& static_cast<float>(localY) >= top
		&& static_cast<float>(localY) <= top + thumbSize.height;
}

bool Slider::IsOriginalSourceWithinThumb(
	Control* source) const
{
	const auto* thumb =
		FindDeclarativeTemplatePart(
			MakeTemplatePartToken(L"PART_Thumb"));
	for (auto* current = source;
		current; current = current->GetVisualParent())
	{
		if (current == thumb) return true;
		if (current == this) break;
	}
	return false;
}

void Slider::HandlePointerPress(
	int localX, int localY, bool isThumbPress)
{
	(void)Focus();
	if (isThumbPress)
	{
		BeginThumbDrag(localX, localY);
		return;
	}

	const double target = PointToValue(localX, localY);
	if (_isMoveToPointEnabled)
		UpdateValueFromInput(target);
	else if (target > ValueCore())
		MoveToNextTick(LargeChange);
	else if (target < ValueCore())
		MoveToNextTick(-LargeChange);
}

void Slider::BeginThumbDrag(int localX, int localY)
{
	_dragStartValue = ValueCore();
	_dragStartX = localX;
	_dragStartY = localY;
	(void)CaptureMouse();
	SetThumbDragging(true);
}

void Slider::SetThumbDragging(bool value)
{
	if (_isThumbDragging == value) return;
	(void)SetReadOnlyPropertyField(
		IsThumbDraggingPropertyKey(),
		_isThumbDragging, value);
}

void Slider::Arrange(cui::core::Rect finalRect)
{
	RangeBase::Arrange(finalRect);
	UpdateTemplateParts();
}

void Slider::PreparePresentation()
{
	RangeBase::PreparePresentation();
	UpdateTemplateParts();
}

void Slider::OnMinimumChanged(
	double oldValue, double newValue)
{
	(void)oldValue;
	(void)newValue;
	(void)ReevaluatePropertyValue(SelectionStartProperty());
	(void)ReevaluatePropertyValue(SelectionEndProperty());
	UpdateTemplateParts();
}

void Slider::OnMaximumChanged(
	double oldValue, double newValue)
{
	(void)oldValue;
	(void)newValue;
	(void)ReevaluatePropertyValue(SelectionStartProperty());
	(void)ReevaluatePropertyValue(SelectionEndProperty());
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
	RangeBase::OnControlTemplatePresentationChanged();
	for (const auto part : {
		MakeTemplatePartToken(L"PART_Track"),
		MakeTemplatePartToken(L"PART_Thumb") })
	{
		auto* partControl = FindDeclarativeTemplatePart(part);
		if (!partControl) continue;
		const ControlWeakReference lifetime(this);
		RetainTemplatePartEventConnection(partControl->SizeChanged.Subscribe(
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
	auto* track = FindDeclarativeTemplatePart(
		MakeTemplatePartToken(L"PART_Track"));
	auto* trackBackground =
		FindDeclarativeTemplatePart(
			MakeTemplatePartToken(L"PART_TrackBackground"));
	auto* selectionRange =
		FindDeclarativeTemplatePart(
			MakeTemplatePartToken(L"PART_SelectionRange"));
	auto* selectedRange =
		FindDeclarativeTemplatePart(
			MakeTemplatePartToken(L"PART_SelectedRange"));
	auto* thumb = FindDeclarativeTemplatePart(
		MakeTemplatePartToken(L"PART_Thumb"));
	if (!track || !thumb) return;

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
		SetCanvasPosition(trackBackground, lineStart, lineTop);
		SetFixedExtent(trackBackground, true, lineLength);
		SetFixedExtent(trackBackground, false, lineThickness);

		const auto placeRange = [this, lineStart, lineLength, lineTop](
			Control* part, double start, double end)
		{
			if (!part) return;
			const float first = PositionRatio(start);
			const float second = PositionRatio(end);
			const float left = lineStart
				+ lineLength * (std::min)(first, second);
			const float width =
				lineLength * std::fabs(second - first);
			SetCanvasPosition(part, left, lineTop);
			SetFixedExtent(part, true, width);
			SetFixedExtent(part, false, 5.0f);
		};
		placeRange(
			selectedRange, MinimumCore(), ValueCore());
		placeRange(
			selectionRange, _selectionStart, _selectionEnd);
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
		SetCanvasPosition(trackBackground, lineLeft, lineStart);
		SetFixedExtent(trackBackground, true, lineThickness);
		SetFixedExtent(trackBackground, false, lineLength);

		const auto placeRange = [this, lineStart, lineLength, lineLeft](
			Control* part, double start, double end)
		{
			if (!part) return;
			const float first = PositionRatio(start);
			const float second = PositionRatio(end);
			const float top = lineStart
				+ lineLength * (std::min)(first, second);
			const float height =
				lineLength * std::fabs(second - first);
			SetCanvasPosition(part, lineLeft, top);
			SetFixedExtent(part, true, 5.0f);
			SetFixedExtent(part, false, height);
		};
		placeRange(
			selectedRange, MinimumCore(), ValueCore());
		placeRange(
			selectionRange, _selectionStart, _selectionEnd);
		SetCanvasPosition(thumb,
			(trackSize.width - thumbWidth) * 0.5f,
			center - thumbHeight * 0.5f);
	}
}
