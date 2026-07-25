#include "Popup.h"
#include "EventInfrastructure.h"

#include "Window.h"
#include "WindowInfrastructure.h"
#include "Layout/OverlayLayout.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace
{
	template<typename TValue>
	DependencyPropertyOptions<Popup, TValue> PopupOptions(
		TValue defaultValue,
		int order,
		DependencyPropertyEditorKind editor,
		DependencyPropertyFlags flags = DependencyPropertyFlags::None)
	{
		DependencyPropertyOptions<Popup, TValue> options;
		options.DefaultValue = std::move(defaultValue);
		options.Flags = flags;
		options.Design.Category = L"Behavior";
		options.Design.CategoryOrder = 110;
		options.Design.Order = order;
		options.Design.Editor = editor;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		return options;
	}

	auto PopupSubscriber(const wchar_t* propertyName)
	{
		return [propertyName = std::wstring(propertyName)](
			Popup& target,
			DependencyPropertyMetadata::ChangeHandler handler,
			DataSourceUpdateMode)
		{
			return target.OnPropertyValueChanged.Subscribe(
				[propertyName, handler = std::move(handler)](
					DependencyObject*,
					const DependencyPropertyChangedEventArgs& args)
				{
					if (args.PropertyName == propertyName) handler();
				});
		};
	}

	float FiniteOrZero(float value) noexcept
	{
		return std::isfinite(value) ? value : 0.0f;
	}
}

Popup::Popup()
	: Control()
{
	RegisterDependencyProperties();
	SetPresentationSuppressed(true);
}

Popup::~Popup()
{
	if (GetPresentationWindow())
		(void)cui::framework::WindowAccess::CloseTransientPresentation(
			*GetPresentationWindow(), this);
}

void Popup::RegisterDependencyProperties()
{
	Control::RegisterDependencyProperties();
	static const bool registered = []
	{
		auto openOptions = PopupOptions(
			false, 10, DependencyPropertyEditorKind::Boolean,
			DependencyPropertyFlags::AffectsArrange
				| DependencyPropertyFlags::AffectsRender);
		openOptions.Changed = [](
			Popup& target, const bool& oldValue, const bool& newValue)
		{
			target.ApplyIsOpenChange(oldValue, newValue);
		};
		DependencyPropertyRegistry::Register<Popup, bool>(L"IsOpen",
			[](Popup& target) { return target.GetIsOpen(); },
			[](Popup& target, const bool& value) { target.SetIsOpen(value); },
			PopupSubscriber(L"IsOpen"), std::move(openOptions));

		DependencyPropertyRegistry::Register<Popup, bool>(L"StaysOpen",
			[](Popup& target) { return target.GetStaysOpen(); },
			[](Popup& target, const bool& value) { target.SetStaysOpen(value); },
			PopupSubscriber(L"StaysOpen"),
			PopupOptions(true, 20, DependencyPropertyEditorKind::Boolean));

		auto placementOptions = PopupOptions(
			PlacementMode::Bottom, 30,
			DependencyPropertyEditorKind::Choice,
			DependencyPropertyFlags::AffectsArrange);
		placementOptions.Design.Choices = {
			{ L"Absolute", BindingValue(PlacementMode::Absolute) },
			{ L"Bottom", BindingValue(PlacementMode::Bottom) },
			{ L"Top", BindingValue(PlacementMode::Top) },
			{ L"Left", BindingValue(PlacementMode::Left) },
			{ L"Right", BindingValue(PlacementMode::Right) },
			{ L"Center", BindingValue(PlacementMode::Center) }
		};
		DependencyPropertyRegistry::Register<Popup, PlacementMode>(L"Placement",
			[](Popup& target) { return target.GetPlacement(); },
			[](Popup& target, const PlacementMode& value)
			{ target.SetPlacement(value); },
			PopupSubscriber(L"Placement"), std::move(placementOptions));

		auto targetOptions = PopupOptions(
			ControlWeakReference{}, 40,
			DependencyPropertyEditorKind::Auto,
			DependencyPropertyFlags::AffectsArrange);
		targetOptions.Design.Browsable = false;
		targetOptions.Design.Persistence =
			DependencyPropertyPersistence::Native;
		DependencyPropertyRegistry::Register<Popup, ControlWeakReference>(
			L"PlacementTarget",
			[](Popup& target) { return target._placementTarget; },
			[](Popup& target, const ControlWeakReference& value)
			{ target.SetPlacementTarget(value.Get()); },
			PopupSubscriber(L"PlacementTarget"), std::move(targetOptions));

		auto offsetOptions = [](float defaultValue, int order)
		{
			auto options = PopupOptions(
				defaultValue, order, DependencyPropertyEditorKind::Number,
				DependencyPropertyFlags::AffectsArrange);
			options.Coerce = [](Popup&, const float& proposed)
				-> std::optional<float>
			{
				return std::isfinite(proposed)
					? std::optional<float>{ proposed } : std::nullopt;
			};
			options.Design.Step = 0.5;
			return options;
		};
		DependencyPropertyRegistry::Register<Popup, float>(L"HorizontalOffset",
			[](Popup& target) { return target.GetHorizontalOffset(); },
			[](Popup& target, const float& value)
			{ target.SetHorizontalOffset(value); },
			PopupSubscriber(L"HorizontalOffset"), offsetOptions(0.0f, 50));
		DependencyPropertyRegistry::Register<Popup, float>(L"VerticalOffset",
			[](Popup& target) { return target.GetVerticalOffset(); },
			[](Popup& target, const float& value)
			{ target.SetVerticalOffset(value); },
			PopupSubscriber(L"VerticalOffset"), offsetOptions(0.0f, 60));
		return true;
	}();
	(void)registered;
}

void Popup::SetIsOpen(bool value)
{
	(void)SetPropertyField(L"IsOpen", _isOpen, value);
}

void Popup::SetStaysOpen(bool value)
{
	if (SetPropertyField(L"StaysOpen", _staysOpen, value)
		&& _isOpen && GetPresentationWindow())
		SynchronizeTransientPresentation();
}

void Popup::SetPlacement(PlacementMode value)
{
	(void)SetPropertyField(L"Placement", _placement, value);
}

void Popup::SetPlacementTarget(Control* value)
{
	const ControlWeakReference proposed(value);
	if (_placementTarget == proposed) return;
	(void)SetPropertyField(
		L"PlacementTarget", _placementTarget, proposed);
	if (_isOpen) UpdatePlacement();
}

void Popup::SetHorizontalOffset(float value)
{
	(void)SetPropertyField(
		L"HorizontalOffset", _horizontalOffset, FiniteOrZero(value));
}

void Popup::SetVerticalOffset(float value)
{
	(void)SetPropertyField(
		L"VerticalOffset", _verticalOffset, FiniteOrZero(value));
}

Control* Popup::GetChild() const noexcept
{
	const auto children = GetVisualChildrenView();
	return children.size() == 1 ? children.front() : nullptr;
}

Control* Popup::SetChild(std::unique_ptr<Control> value)
{
	if (value.get() == GetChild()) return value.release();
	auto previous = DetachChild();
	if (!value) return nullptr;
	try
	{
		return AddOwned(std::move(value));
	}
	catch (...)
	{
		if (previous) AddOwned(std::move(previous));
		throw;
	}
}

std::unique_ptr<Control> Popup::DetachChild()
{
	auto* child = GetChild();
	return child ? DetachVisualChild(child) : std::unique_ptr<Control>{};
}

void Popup::ApplyIsOpenChange(bool oldValue, bool newValue)
{
	if (oldValue == newValue) return;
	if (newValue)
	{
		SetPresentationSuppressed(false);
		UpdatePlacement();
		SynchronizeTransientPresentation();
		cui::framework::EventAccess::Raise(Opened, this);
	}
	else
	{
		auto* window = GetPresentationWindow();
		if (window)
			(void)cui::framework::WindowAccess::CloseTransientPresentation(
				*window, this);
		SetPresentationSuppressed(true);
		cui::framework::EventAccess::Raise(Closed, this);
	}
}

void Popup::SynchronizeTransientPresentation()
{
	if (!_isOpen || !GetPresentationWindow()) return;
	TransientPresentationOptions options;
	options.DismissOnOutsidePointerDown = !_staysOpen;
	options.DismissOnWindowDeactivation = !_staysOpen;
	options.CloseExistingDismissiblePresentation = false;
	(void)cui::framework::WindowAccess::OpenTransientPresentation(
		*GetPresentationWindow(), this, options,
		[](Control& root)
		{
			(void)static_cast<Popup&>(root).TrySetCurrentPropertyValue(
				L"IsOpen", BindingValue(false));
		});
}

cui::core::Size Popup::MeasurePopupContent(
	const cui::core::Constraints& available)
{
	return cui::layout::MeasureOverlayChildren(
		GetLayoutChildrenView(), available);
}

cui::core::Size Popup::MeasureCore(const cui::core::Constraints&)
{
	// A Popup never contributes its transient subtree to its placement
	// parent's measure pass.
	return {};
}

void Popup::Arrange(cui::core::Rect finalRect)
{
	(void)finalRect;
	if (!_isOpen)
	{
		Control::Arrange({ 0.0f, 0.0f, 0.0f, 0.0f });
		return;
	}
	UpdatePlacement();
}

void Popup::UpdatePlacement()
{
	if (_applyingPlacement || !_isOpen || !GetPresentationWindow()) return;
	_applyingPlacement = true;

	auto finish = [this] { _applyingPlacement = false; };
	try
	{
		const auto viewport = GetPresentationWindow()->GetContentViewportSizeDip();
		const float edge = 2.0f;
		const float viewportWidth = (std::max)(1.0f, viewport.width);
		const float viewportHeight = (std::max)(1.0f, viewport.height);
		const auto target = _placementTarget.Get();
		const auto targetRect = target
			? target->GetAbsoluteRectDip()
			: cui::core::Rect{};
		const auto style = GetSpecifiedLayout();
		const auto limits = style.SizeConstraints();
		const float maximumWidth = (std::min)(
			viewportWidth - edge * 2.0f, limits.maximum.width);
		const float maximumHeight = (std::min)(
			viewportHeight - edge * 2.0f, limits.maximum.height);
		const auto desired = MeasurePopupContent(cui::core::Constraints{
			{ 0.0f, 0.0f },
			{ (std::max)(1.0f, maximumWidth),
				(std::max)(1.0f, maximumHeight) } });

		float width = style.width.IsFixed()
			? style.width.value
			: (std::max)(desired.width, targetRect.width);
		float height = style.height.IsFixed()
			? style.height.value : desired.height;
		width = (std::clamp)(width,
			limits.minimum.width, (std::max)(limits.minimum.width, maximumWidth));
		height = (std::clamp)(height,
			limits.minimum.height, (std::max)(limits.minimum.height, maximumHeight));

		float x = targetRect.x;
		float y = targetRect.y + targetRect.height;
		switch (_placement)
		{
		case PlacementMode::Absolute:
			x = 0.0f;
			y = 0.0f;
			break;
		case PlacementMode::Top:
			y = targetRect.y - height;
			if (y < edge && targetRect.y + targetRect.height + height
				<= viewportHeight - edge)
				y = targetRect.y + targetRect.height;
			break;
		case PlacementMode::Left:
			x = targetRect.x - width;
			y = targetRect.y;
			if (x < edge && targetRect.x + targetRect.width + width
				<= viewportWidth - edge)
				x = targetRect.x + targetRect.width;
			break;
		case PlacementMode::Right:
			x = targetRect.x + targetRect.width;
			y = targetRect.y;
			if (x + width > viewportWidth - edge
				&& targetRect.x - width >= edge)
				x = targetRect.x - width;
			break;
		case PlacementMode::Center:
			x = targetRect.x + (targetRect.width - width) * 0.5f;
			y = targetRect.y + (targetRect.height - height) * 0.5f;
			break;
		case PlacementMode::Bottom:
		default:
			if (y + height > viewportHeight - edge
				&& targetRect.y - height >= edge)
				y = targetRect.y - height;
			break;
		}
		x += _horizontalOffset;
		y += _verticalOffset;
		x = (std::clamp)(x, edge,
			(std::max)(edge, viewportWidth - width - edge));
		y = (std::clamp)(y, edge,
			(std::max)(edge, viewportHeight - height - edge));

		cui::core::Point parentAbsolute{};
		if (auto* parent = GetVisualParent())
			parentAbsolute = parent->GetAbsoluteLocationDip();
		Control::Arrange({
			x - parentAbsolute.x,
			y - parentAbsolute.y,
			width,
			height });
		cui::layout::ArrangeOverlayChildren(
			GetLayoutChildrenView(),
			cui::core::Rect{ 0.0f, 0.0f, width, height });
		finish();
	}
	catch (...)
	{
		finish();
		throw;
	}
}

void Popup::PreparePresentation()
{
	Control::PreparePresentation();
	if (_isOpen && GetPresentationWindow()
		&& cui::framework::WindowAccess::IsTransientPresentationOpen(
			*GetPresentationWindow(), this))
		UpdatePlacement();
}

void Popup::OnRender()
{
	if (!_isOpen || !GetPresentationWindow()
		|| !cui::framework::WindowAccess::IsTransientPresentationOpen(
			*GetPresentationWindow(), this)) return;
	if (!IsVisible || !GetDrawingContext()) return;
	BeginRender();
	EndRender();
}

void Popup::OnPresentationWindowChanged(
	Window* previousWindow, Window* currentWindow)
{
	Control::OnPresentationWindowChanged(previousWindow, currentWindow);
	if (previousWindow)
		(void)cui::framework::WindowAccess::CloseTransientPresentation(
			*previousWindow, this);
	if (_isOpen && currentWindow)
	{
		SetPresentationSuppressed(false);
		UpdatePlacement();
		SynchronizeTransientPresentation();
	}
}

bool Popup::ValidateVisualChildCollection(
	std::span<Control* const> children,
	std::string& error) const
{
	const auto authoredCount = std::count_if(
		children.begin(), children.end(),
		[](const Control* child) { return child != nullptr; });
	if (authoredCount <= 1) return true;
	error = "Popup accepts exactly one Child slot";
	return false;
}
