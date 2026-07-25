#include "ButtonBase.h"

#include "InputManager.h"

#include <vector>

void ButtonBase::EnsureClassHandlers()
{
	static const std::vector<EventConnection> handlers = []
	{
		std::vector<EventConnection> result;
		result.push_back(RoutedEventManager::RegisterClassHandler(
			UIClass::UI_ButtonBase, RoutedEventId::MouseDown,
			&ButtonBase::HandleDescendantPointerPress));
		result.push_back(RoutedEventManager::RegisterClassHandler(
			UIClass::UI_ButtonBase, RoutedEventId::MouseDoubleClick,
			&ButtonBase::HandleDescendantPointerPress));
		return result;
	}();
	(void)handlers;
}

void ButtonBase::HandleDescendantPointerPress(
	Control* sender, RoutedEventArgs& args)
{
	auto* button = dynamic_cast<ButtonBase*>(sender);
	auto& mouse = static_cast<MouseEventArgs&>(args);
	if (!button || args.OriginalSource == button
		|| mouse.ChangedButton != MouseButton::Left
		|| !button->IsEffectivelyEnabled() || !button->IsVisible) return;
	button->_defaultLeftButtonPressActive = true;
	(void)button->Focus();
	button->BeforeDefaultMouseDown(MouseButton::Left, mouse);
	button->SetStyleState(ControlStyleState::Pressed, true);
	button->InvalidateVisual();
	(void)button->CaptureMouse();
}

ButtonBase::ButtonBase()
	: ContentControl()
{
	EnsureClassHandlers();
	(void)TrySetPropertyValue(
		L"Cursor", BindingValue(CursorKind::Hand),
		DependencyPropertyValueSource::Theme);
}

void ButtonBase::RegisterDependencyProperties()
{
	ContentControl::RegisterDependencyProperties();
	static const bool registered = []
	{
		DependencyPropertyOptions<ButtonBase, bool> options;
		options.DefaultValue = false;
		options.Flags = DependencyPropertyFlags::AffectsRender;
		options.IsReadOnly = true;
		options.Design.Category = L"State";
		options.Design.CategoryOrder = 70;
		options.Design.Order = 60;
		options.Design.Editor = DependencyPropertyEditorKind::Boolean;
		options.Design.Persistence = DependencyPropertyPersistence::Transient;
		options.Design.Browsable = false;
		DependencyPropertyRegistry::Register<ButtonBase, bool>(
			L"IsPressed",
			[](ButtonBase& target) { return target.IsPressed; },
			[](ButtonBase& target, const bool& value)
			{
				(void)target.SetReadOnlyPropertyField(
					L"IsPressed", target._isPressed, value);
			}, {}, std::move(options));
		return true;
	}();
	(void)registered;
}

GET_CPP(ButtonBase, bool, IsPressed)
{
	return _isPressed;
}

void ButtonBase::OnPressedVisualStateChanged(bool value)
{
	if (_isPressed == value) return;
	(void)SetReadOnlyPropertyField(L"IsPressed", _isPressed, value);
}
