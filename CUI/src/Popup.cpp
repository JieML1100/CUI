#include "Popup.h"
#include "EventInfrastructure.h"

#include "Window.h"
#include "WindowInfrastructure.h"
#include "Layout/OverlayLayout.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <typeindex>
#include <utility>

namespace
{
	template<typename TValue>
	DependencyPropertyOptions<Popup, TValue> PopupOptions(
		TValue defaultValue
		CUI_DESIGN_METADATA_ARGUMENTS(
			int order,
			DependencyPropertyEditorKind editor),
		DependencyPropertyFlags flags = DependencyPropertyFlags::None)
	{
		DependencyPropertyOptions<Popup, TValue> options;
		options.DefaultValue = std::move(defaultValue);
		options.Flags = flags;
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Behavior";
		options.Design.CategoryOrder = 110;
		options.Design.Order = order;
		options.Design.Editor = editor;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		)
		return options;
	}

	float FiniteOrZero(float value) noexcept
	{
		return std::isfinite(value) ? value : 0.0f;
	}

	DependencyPropertyOptions<Popup, float> PopupOffsetOptions(
		float defaultValue CUI_DESIGN_METADATA_ARGUMENTS(int order))
	{
		auto options = PopupOptions(
			defaultValue CUI_DESIGN_METADATA_ARGUMENTS(
				order, DependencyPropertyEditorKind::Number),
			DependencyPropertyFlags::AffectsArrange);
		options.Validate = [](const float& proposed)
		{
			return std::isfinite(proposed);
		};
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Step = 0.5;
		)
		return options;
	}
}

const DependencyProperty& Popup::IsOpenProperty()
{
	static const auto registration = []
	{
		auto options = PopupOptions(
			false CUI_DESIGN_METADATA_ARGUMENTS(
				10, DependencyPropertyEditorKind::Boolean),
			DependencyPropertyFlags::AffectsArrange
				| DependencyPropertyFlags::AffectsRender);
		options.Changed = [](
			Popup& target, const bool& oldValue, const bool& newValue)
		{
			target.ApplyIsOpenChange(oldValue, newValue);
		};
		return DependencyPropertyRegistry::RegisterStatic<Popup, bool>(
			DependencyPropertyRegistrationLiteral(L"IsOpen"),
			[](Popup& target) { return target.GetIsOpen(); },
			[](Popup& target, const bool& value) { target.SetIsOpen(value); },
			{}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& Popup::StaysOpenProperty()
{
	static const auto registration =
		DependencyPropertyRegistry::RegisterStatic<Popup, bool>(
			DependencyPropertyRegistrationLiteral(L"StaysOpen"),
			[](Popup& target) { return target.GetStaysOpen(); },
			[](Popup& target, const bool& value)
			{ target.SetStaysOpen(value); }, {},
			PopupOptions(true CUI_DESIGN_METADATA_ARGUMENTS(
				20, DependencyPropertyEditorKind::Boolean)));
	return *registration;
}

const DependencyProperty& Popup::PlacementProperty()
{
	static const auto registration = []
	{
		auto options = PopupOptions(
			PlacementMode::Bottom CUI_DESIGN_METADATA_ARGUMENTS(
				30, DependencyPropertyEditorKind::Choice),
			DependencyPropertyFlags::AffectsArrange);
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Choices = {
			{ L"Absolute", BindingValue(PlacementMode::Absolute) },
			{ L"Bottom", BindingValue(PlacementMode::Bottom) },
			{ L"Top", BindingValue(PlacementMode::Top) },
			{ L"Left", BindingValue(PlacementMode::Left) },
			{ L"Right", BindingValue(PlacementMode::Right) },
			{ L"Center", BindingValue(PlacementMode::Center) },
			{ L"MousePoint", BindingValue(PlacementMode::MousePoint) }
		};
		)
		return DependencyPropertyRegistry::RegisterStatic<Popup, PlacementMode>(
			DependencyPropertyRegistrationLiteral(L"Placement"),
			[](Popup& target) { return target.GetPlacement(); },
			[](Popup& target, const PlacementMode& value)
			{ target.SetPlacement(value); }, {}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& Popup::PlacementTargetProperty()
{
	static const auto registration = []
	{
		auto options = PopupOptions(
			ControlWeakReference{} CUI_DESIGN_METADATA_ARGUMENTS(
				40, DependencyPropertyEditorKind::Auto),
			DependencyPropertyFlags::AffectsArrange);
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Browsable = false;
		options.Design.Persistence = DependencyPropertyPersistence::Native;
		)
		return DependencyPropertyRegistry::RegisterStatic<
			Popup, ControlWeakReference>(
				DependencyPropertyRegistrationLiteral(L"PlacementTarget"),
				[](Popup& target) { return target._placementTarget; },
				[](Popup& target, const ControlWeakReference& value)
				{ target.SetPlacementTarget(value.Get()); }, {},
				std::move(options));
	}();
	return *registration;
}

const DependencyProperty& Popup::HorizontalOffsetProperty()
{
	static const auto registration =
		DependencyPropertyRegistry::RegisterStatic<Popup, float>(
			DependencyPropertyRegistrationLiteral(L"HorizontalOffset"),
			[](Popup& target) { return target.GetHorizontalOffset(); },
			[](Popup& target, const float& value)
			{ target.SetHorizontalOffset(value); }, {},
			PopupOffsetOptions(0.0f CUI_DESIGN_METADATA_ARGUMENTS(50)));
	return *registration;
}

const DependencyProperty& Popup::VerticalOffsetProperty()
{
	static const auto registration =
		DependencyPropertyRegistry::RegisterStatic<Popup, float>(
			DependencyPropertyRegistrationLiteral(L"VerticalOffset"),
			[](Popup& target) { return target.GetVerticalOffset(); },
			[](Popup& target, const float& value)
			{ target.SetVerticalOffset(value); }, {},
			PopupOffsetOptions(0.0f CUI_DESIGN_METADATA_ARGUMENTS(60)));
	return *registration;
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
#if CUI_ENABLE_DYNAMIC_XAML
	(void)IsOpenProperty();
	(void)StaysOpenProperty();
	(void)PlacementProperty();
	(void)PlacementTargetProperty();
	(void)HorizontalOffsetProperty();
	(void)VerticalOffsetProperty();
#endif
}

void Popup::SetIsOpen(bool value)
{
	(void)SetPropertyField(IsOpenProperty(), _isOpen, value);
}

void Popup::SetStaysOpen(bool value)
{
	if (SetPropertyField(StaysOpenProperty(), _staysOpen, value)
		&& _isOpen && GetPresentationWindow())
		SynchronizeTransientPresentation();
}

void Popup::SetPlacement(PlacementMode value)
{
	(void)SetPropertyField(PlacementProperty(), _placement, value);
}

void Popup::SetPlacementTarget(Control* value)
{
	const ControlWeakReference proposed(value);
	if (_placementTarget == proposed) return;
	(void)SetPropertyField(
		PlacementTargetProperty(), _placementTarget, proposed);
	if (_isOpen) UpdatePlacement();
}

void Popup::SetHorizontalOffset(float value)
{
	(void)SetPropertyField(
		HorizontalOffsetProperty(), _horizontalOffset, FiniteOrZero(value));
}

void Popup::SetVerticalOffset(float value)
{
	(void)SetPropertyField(
		VerticalOffsetProperty(), _verticalOffset, FiniteOrZero(value));
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
		auto* child = AddOwned(std::move(value));
		_placementContentDirty = true;
		_hasPlacementSnapshot = false;
		return child;
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
	if (!child) return {};
	auto result = DetachVisualChild(child);
	_placementContentDirty = true;
	_hasPlacementSnapshot = false;
	return result;
}

void Popup::ApplyIsOpenChange(bool oldValue, bool newValue)
{
	if (oldValue == newValue) return;
	if (newValue)
	{
		_placementContentDirty = true;
		_hasPlacementSnapshot = false;
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
				Popup::IsOpenProperty(), BindingValue(false));
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
		const auto viewportSnapshot = cui::core::Size{
			viewportWidth, viewportHeight };
		const bool viewportChanged = !_hasPlacementSnapshot
			|| _placementViewport.width != viewportSnapshot.width
			|| _placementViewport.height != viewportSnapshot.height;
		const bool targetChanged = !_hasPlacementSnapshot
			|| _placementTargetRect.x != targetRect.x
			|| _placementTargetRect.y != targetRect.y
			|| _placementTargetRect.width != targetRect.width
			|| _placementTargetRect.height != targetRect.height;
		if (_placementContentDirty || viewportChanged || targetChanged)
		{
			_measuredPopupContent = MeasurePopupContent(
				cui::core::Constraints{
					{ 0.0f, 0.0f },
					{ (std::max)(1.0f, maximumWidth),
						(std::max)(1.0f, maximumHeight) } });
			_placementViewport = viewportSnapshot;
			_placementTargetRect = targetRect;
			_placementContentDirty = false;
			_hasPlacementSnapshot = true;
		}
		const auto desired = _measuredPopupContent;

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
		case PlacementMode::MousePoint:
			// Popup itself has no input-service anchor.  Its owner supplies the
			// pointer point as a zero-sized PlacementTarget rectangle.
			x = targetRect.x;
			y = targetRect.y;
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

		// Popup is projected as an independent transient presentation root.  Its
		// arranged origin therefore belongs to the window content viewport, not
		// to the template owner's local coordinate space.  Subtracting the visual
		// parent's origin here places nested menu popups off-screen after the
		// transient scene severs their main-tree presentation inheritance.
		Control::Arrange({
			x,
			y,
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

void Popup::RequestLayout()
{
	_placementContentDirty = true;
	_hasPlacementSnapshot = false;
	Control::RequestLayout();
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
		_placementContentDirty = true;
		_hasPlacementSnapshot = false;
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
