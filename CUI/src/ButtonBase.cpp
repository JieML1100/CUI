#include "ButtonBase.h"

#include "InputManager.h"

#include <stdexcept>
#include <typeindex>
#include <utility>
#include <vector>

namespace
{
	const DependencyPropertyMetadataRegistration&
		ButtonBaseFocusableMetadataRelation()
	{
		static const DependencyPropertyMetadataRegistration relation = []
		{
			const auto& property = Control::FocusableProperty();
			DependencyPropertyOptions<ButtonBase, bool> options;
			options.DefaultValue = true;
			CUI_DESIGN_METADATA_ONLY(
			const std::type_index ownerTypes[] = {
				std::type_index(typeid(Control))
			};
			const auto* base =
				DependencyPropertyRegistry::FindRegistered(
					ownerTypes, L"Focusable");
			if (!base)
				throw std::logic_error(
					"Control.Focusable must be registered before ButtonBase");
			options.Design = base->Design();
			)
			return DependencyPropertyRegistry::OverrideMetadataStatic<
				ButtonBase, ContentControl, bool>(
					property, std::move(options));
		}();
		return relation;
	}

	bool IsPointerInside(
		ButtonBase& button, int x, int y) noexcept
	{
		return button.ContainsPoint(x, y);
	}

}

const DependencyProperty& ButtonBase::IsPressedProperty()
{
	return IsPressedPropertyKey().Property();
}

const DependencyPropertyKey& ButtonBase::IsPressedPropertyKey()
{
	static const auto registration = []
	{
		DependencyPropertyOptions<ButtonBase, bool> options;
		options.DefaultValue = false;
		options.Flags = DependencyPropertyFlags::AffectsRender;
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"State";
		options.Design.CategoryOrder = 70;
		options.Design.Order = 60;
		options.Design.Editor = DependencyPropertyEditorKind::Boolean;
		options.Design.Persistence = DependencyPropertyPersistence::Transient;
		options.Design.Browsable = false;
		)
		return DependencyPropertyRegistry::RegisterReadOnlyStatic<
			ButtonBase, bool>(
				DependencyPropertyRegistrationLiteral(L"IsPressed"),
				[](ButtonBase& target) { return target.IsPressed; },
				[](ButtonBase& target, const bool& value)
				{
					(void)target.SetReadOnlyPropertyField(
						IsPressedPropertyKey(), target._isPressed, value);
				}, {}, std::move(options));
	}();
	return registration.Key();
}

const DependencyProperty& ButtonBase::ClickModeProperty()
{
	static const auto registration = []
	{
		DependencyPropertyOptions<ButtonBase, ::ClickMode> options;
		options.DefaultValue = ::ClickMode::Release;
		options.Validate = [](const ::ClickMode& value)
		{
			return value == ::ClickMode::Release
				|| value == ::ClickMode::Press
				|| value == ::ClickMode::Hover;
		};
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Behavior";
		options.Design.CategoryOrder = 300;
		options.Design.Order = 10;
		options.Design.Editor = DependencyPropertyEditorKind::Choice;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		options.Design.Choices = {
			{ L"Release", BindingValue(::ClickMode::Release) },
			{ L"Press", BindingValue(::ClickMode::Press) },
			{ L"Hover", BindingValue(::ClickMode::Hover) },
		};
		)
		return DependencyPropertyRegistry::RegisterStatic<
			ButtonBase, ::ClickMode>(
				DependencyPropertyRegistrationLiteral(L"ClickMode"),
				[](ButtonBase& target) { return target.ClickMode; },
				[](ButtonBase& target, const ::ClickMode& value)
				{ target.ClickMode = value; }, {}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& ButtonBase::CommandProperty()
{
	static const auto registration = []
	{
		auto options = DependencyPropertyOptions<ButtonBase, std::wstring>{
			std::wstring{}, DependencyPropertyFlags::None };
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Behavior";
		options.Design.CategoryOrder = 300;
		options.Design.Order = 20;
		options.Design.Editor = DependencyPropertyEditorKind::Text;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		)
		options.Changed = [](
			ButtonBase& target, const std::wstring&, const std::wstring&)
		{
			target.RefreshCommandSource();
		};
		return DependencyPropertyRegistry::RegisterStatic<
			ButtonBase, std::wstring>(
				DependencyPropertyRegistrationLiteral(L"Command"),
				[](ButtonBase& target) { return target.Command; },
				[](ButtonBase& target, const std::wstring& value)
				{ target.Command = value; }, {}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& ButtonBase::CommandParameterProperty()
{
	static const auto registration = []
	{
		DependencyPropertyOptions<ButtonBase, BindingValue> options;
		options.DefaultValue = BindingValue{};
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Behavior";
		options.Design.CategoryOrder = 300;
		options.Design.Order = 30;
		options.Design.Editor = DependencyPropertyEditorKind::Auto;
		options.Design.Persistence = DependencyPropertyPersistence::Native;
		)
		options.Changed = [](
			ButtonBase& target, const BindingValue&, const BindingValue&)
		{
			target.RefreshCommandSource();
		};
		return DependencyPropertyRegistry::RegisterStatic<
			ButtonBase, BindingValue>(
				DependencyPropertyRegistrationLiteral(L"CommandParameter"),
				[](ButtonBase& target) { return target.CommandParameter; },
				[](ButtonBase& target, const BindingValue& value)
				{ target.CommandParameter = value; }, {}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& ButtonBase::CommandTargetProperty()
{
	static const auto registration = []
	{
		auto options =
			DependencyPropertyOptions<ButtonBase, ControlWeakReference>{
				ControlWeakReference{}, DependencyPropertyFlags::None };
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Behavior";
		options.Design.CategoryOrder = 300;
		options.Design.Order = 40;
		options.Design.Editor = DependencyPropertyEditorKind::Auto;
		options.Design.Persistence = DependencyPropertyPersistence::Native;
		)
		return DependencyPropertyRegistry::RegisterStatic<
			ButtonBase, ControlWeakReference>(
				DependencyPropertyRegistrationLiteral(L"CommandTarget"),
				[](ButtonBase& target) { return target._commandTarget; },
				[](ButtonBase& target, const ControlWeakReference& value)
				{ target.ApplyCommandTarget(value); }, {}, std::move(options));
	}();
	return *registration;
}

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
	button->BeginPointerPress(mouse);
}

ButtonBase::ButtonBase()
	: ContentControl()
{
	EnsureClassHandlers();
	RegisterDependencyProperties();
	RetainEventConnection(OnLogicalParentChanged.Subscribe(
		[this](Control*, Control*, Control*) { RefreshCommandSource(); }));
	RetainEventConnection(OnVisualParentChanged.Subscribe(
		[this](Control*, Control*, Control*) { RefreshCommandSource(); }));
}

void ButtonBase::RegisterDependencyProperties()
{
	ContentControl::RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	(void)IsPressedProperty();
	(void)ClickModeProperty();
	(void)CommandProperty();
	(void)CommandParameterProperty();
	(void)CommandTargetProperty();
#endif
	CUI_DESIGN_METADATA_ONLY(
	(void)ButtonBaseFocusableMetadataRelation();
	)
}

const DependencyPropertyMetadata*
ButtonBase::ResolveExactDependencyPropertyMetadata(
	const DependencyProperty& property) const
{
	if (&property == &Control::FocusableProperty())
		return &ButtonBaseFocusableMetadataRelation().Metadata();
	return ContentControl::ResolveExactDependencyPropertyMetadata(property);
}

GET_CPP(ButtonBase, bool, IsPressed)
{
	return _isPressed;
}

void ButtonBase::OnPressedVisualStateChanged(bool value)
{
	if (_isPressed == value) return;
	(void)SetReadOnlyPropertyField(
		IsPressedPropertyKey(), _isPressed, value);
}

void ButtonBase::SetPressed(bool value)
{
	SetStyleState(ControlStyleState::Pressed, value);
}

GET_CPP(ButtonBase, ::ClickMode, ClickMode)
{
	return _clickMode;
}

SET_CPP(ButtonBase, ::ClickMode, ClickMode)
{
	(void)SetPropertyField(ClickModeProperty(), _clickMode, value);
}

GET_CPP(ButtonBase, std::wstring, Command)
{
	return _command;
}

SET_CPP(ButtonBase, std::wstring, Command)
{
	(void)SetPropertyField(
		CommandProperty(), _command, std::move(value));
}

GET_CPP(ButtonBase, BindingValue, CommandParameter)
{
	return _commandParameter;
}

SET_CPP(ButtonBase, BindingValue, CommandParameter)
{
	(void)SetPropertyField(
		CommandParameterProperty(),
		_commandParameter, std::move(value));
}

GET_CPP(ButtonBase, Control*, CommandTarget)
{
	return _commandTarget.Get();
}

SET_CPP(ButtonBase, Control*, CommandTarget)
{
	const ControlWeakReference lifetime(this);
	if (value)
	{
		(void)TrySetPropertyValue(
			CommandTargetProperty(),
			BindingValue(ControlWeakReference(value)),
			DependencyPropertyValueSource::Local);
		return;
	}
	if (ClearPropertyValue(CommandTargetProperty()))
		return;
	if (auto* source = dynamic_cast<ButtonBase*>(lifetime.Get()))
		source->ApplyCommandTarget({});
}

void ButtonBase::ClearCommandTarget()
{
	SetCommandTarget(nullptr);
}

void ButtonBase::ApplyCommandTarget(
	const ControlWeakReference& value)
{
	const ControlWeakReference lifetime(this);
	if (_commandTarget == value) return;
	_commandTarget = value;
	if (auto* source = dynamic_cast<ButtonBase*>(lifetime.Get()))
		source->RefreshCommandSource();
}

void ButtonBase::RefreshCommandSource()
{
	const auto refreshVersion = ++_commandSourceRefreshVersion;
	_commandCanExecuteConnection.Disconnect();
	if (_command.empty())
	{
		ClearCommandCanExecuteState();
		return;
	}

	const ControlWeakReference lifetime(this);
	auto connection = RoutedCommandManager::ObserveCanExecute(
		*this,
		RoutedCommandSourceQuery{
			RoutedCommand(_command), _commandParameter.ToAny(), _commandTarget },
		[lifetime, refreshVersion](
			Control& source,
			const RoutedCommandCanExecuteResult& result)
		{
			auto* current =
				dynamic_cast<ButtonBase*>(lifetime.Get());
			if (current != &source
				|| current->_commandSourceRefreshVersion
					!= refreshVersion)
				return;
			current->SetCommandCanExecuteState(result.CanExecute);
		});

	auto* source = dynamic_cast<ButtonBase*>(lifetime.Get());
	if (!source
		|| source->_commandSourceRefreshVersion != refreshVersion)
		return;
	source->_commandCanExecuteConnection = std::move(connection);
}

bool ButtonBase::ExecuteCommandSource()
{
	if (_command.empty()) return true;
	return RoutedCommandManager::ExecuteCommandSource(
		*this,
		RoutedCommandSourceQuery{
			RoutedCommand(_command), _commandParameter.ToAny(),
			_commandTarget }).Executed;
}

bool ButtonBase::OnClick()
{
	const ControlWeakReference lifetime(this);
	RoutedEventArgs eventArgs;
	Click(this, eventArgs);
	auto* source = dynamic_cast<ButtonBase*>(lifetime.Get());
	return source ? source->ExecuteCommandSource() : true;
}

bool ButtonBase::Invoke()
{
	const auto snapshot = GetAccessibilitySnapshot();
	if (!snapshot.Enabled || !snapshot.Visible) return false;
	return OnClick();
}

bool ButtonBase::OnAccessKey(bool isMultiple)
{
	return isMultiple
		? ContentControl::OnAccessKey(isMultiple)
		: OnClick();
}

bool ButtonBase::HandlesNavigationKey(Key key) const
{
	// WPF KeyboardNavigation.AcceptsReturn defaults to false for ButtonBase
	// (and RadioButton explicitly preserves that metadata). Enter must remain
	// available to Window's default-button routing; Space is the direct
	// ButtonBase activation gesture.
	return key == Key::Space;
}

void ButtonBase::BeginPointerPress(MouseEventArgs& args)
{
	if (_clickMode == ::ClickMode::Hover) return;

	const ControlWeakReference lifetime(this);
	(void)Focus();
	auto* source = dynamic_cast<ButtonBase*>(lifetime.Get());
	if (!source || !source->IsEffectivelyEnabled()
		|| !source->IsVisible) return;

	source->_lastPointerX = args.X;
	source->_lastPointerY = args.Y;
	source->_lastPointerInside =
		IsPointerInside(*source, args.X, args.Y);
	source->_leftButtonDown = true;
	source->_defaultLeftButtonPressActive = true;
	(void)source->CaptureMouse();

	source = dynamic_cast<ButtonBase*>(lifetime.Get());
	if (!source) return;
	source->SetPressed(
		source->IsMouseCaptured() && source->_lastPointerInside);

	source = dynamic_cast<ButtonBase*>(lifetime.Get());
	if (source && source->_clickMode == ::ClickMode::Press)
		(void)source->OnClick();
}

void ButtonBase::CancelPress(bool releaseCapture)
{
	const ControlWeakReference lifetime(this);
	_leftButtonDown = false;
	_isSpaceKeyDown = false;
	_defaultLeftButtonPressActive = false;
	SetPressed(false);

	auto* source = dynamic_cast<ButtonBase*>(lifetime.Get());
	if (source && releaseCapture && source->IsMouseCaptured())
		(void)source->ReleaseMouseCapture();
}

void ButtonBase::BeforeDefaultMouseMove(MouseEventArgs& args)
{
	_lastPointerX = args.X;
	_lastPointerY = args.Y;
	_lastPointerInside = IsPointerInside(*this, args.X, args.Y);
	_leftButtonDown = args.IsButtonPressed(MouseButton::Left);
	if (_clickMode != ::ClickMode::Hover
		&& IsMouseCaptured() && _leftButtonDown
		&& !_isSpaceKeyDown)
		SetPressed(_lastPointerInside);
}

void ButtonBase::BeforeDefaultMouseDown(
	MouseButton button, MouseEventArgs& args)
{
	if (button == MouseButton::Left)
		BeginPointerPress(args);
}

void ButtonBase::BeforeDefaultMouseUp(
	MouseButton button,
	MouseEventArgs& args,
	bool hasMatchingPress)
{
	if (button != MouseButton::Left
		|| _clickMode == ::ClickMode::Hover) return;

	_lastPointerX = args.X;
	_lastPointerY = args.Y;
	_lastPointerInside = IsPointerInside(*this, args.X, args.Y);
	const bool shouldClick = hasMatchingPress && _leftButtonDown
		&& !_isSpaceKeyDown && _isPressed
		&& _clickMode == ::ClickMode::Release;
	_leftButtonDown = false;
	_defaultLeftButtonPressActive = false;

	const ControlWeakReference lifetime(this);
	if (!_isSpaceKeyDown)
	{
		SetPressed(false);
		auto* source = dynamic_cast<ButtonBase*>(lifetime.Get());
		if (!source) return;
		if (source->IsMouseCaptured())
			(void)source->ReleaseMouseCapture();
	}

	auto* source = dynamic_cast<ButtonBase*>(lifetime.Get());
	if (source && shouldClick) (void)source->OnClick();
}

void ButtonBase::OnIsMouseOverChanged(bool previous, bool current)
{
	ContentControl::OnIsMouseOverChanged(previous, current);
	if (_clickMode != ::ClickMode::Hover) return;
	if (!current || !IsEffectivelyEnabled() || !IsVisible)
	{
		SetPressed(false);
		return;
	}

	const ControlWeakReference lifetime(this);
	SetPressed(true);
	if (auto* source = dynamic_cast<ButtonBase*>(lifetime.Get()))
		(void)source->OnClick();
}

void ButtonBase::OnEffectiveIsEnabledChanged(
	bool previousValue, bool currentValue)
{
	ContentControl::OnEffectiveIsEnabledChanged(
		previousValue, currentValue);
	if (!currentValue) CancelPress(true);
}

void ButtonBase::OnPresentationWindowChanged(
	PresentationWindow* previousWindow,
	PresentationWindow* currentWindow)
{
	const ControlWeakReference lifetime(this);
	ContentControl::OnPresentationWindowChanged(
		previousWindow, currentWindow);
	auto* source = dynamic_cast<ButtonBase*>(lifetime.Get());
	if (!source) return;
	if (!currentWindow)
	{
		source->CancelPress(false);
		source = dynamic_cast<ButtonBase*>(lifetime.Get());
		if (!source) return;
	}
	source->RefreshCommandSource();
}

void ButtonBase::OnComputedLayoutSizeChanged()
{
	ContentControl::OnComputedLayoutSizeChanged();
	if (_clickMode == ::ClickMode::Hover
		|| !IsMouseCaptured() || !_leftButtonDown
		|| _isSpaceKeyDown) return;
	_lastPointerInside = IsPointerInside(
		*this, _lastPointerX, _lastPointerY);
	SetPressed(_lastPointerInside);
}

bool ButtonBase::ProcessInput(const InputReport& input)
{
	const ControlWeakReference lifetime(this);
	(void)ContentControl::ProcessInput(input);
	auto* source = dynamic_cast<ButtonBase*>(lifetime.Get());
	if (!source) return true;

	if (!source->IsEffectivelyEnabled() || !source->IsVisible)
	{
		source->CancelPress(true);
		return true;
	}

	switch (input.Kind)
	{
	case InputReportKind::KeyDown:
		if (source->_clickMode == ::ClickMode::Hover) break;
		if (input.Key == Key::Space)
		{
			const auto commandModifiers =
				input.Modifiers
				& (ModifierKeys::Control | ModifierKeys::Alt);
			if (commandModifiers == ModifierKeys::Alt) break;
			if (!source->_isSpaceKeyDown)
			{
				source->_isSpaceKeyDown = true;
				source->SetPressed(true);
				source = dynamic_cast<ButtonBase*>(lifetime.Get());
				if (!source) return true;
				(void)source->CaptureMouse();
				source = dynamic_cast<ButtonBase*>(lifetime.Get());
				if (source
					&& source->_clickMode == ::ClickMode::Press)
					(void)source->OnClick();
			}
			return true;
		}
		if (source->_isSpaceKeyDown)
			source->CancelPress(true);
		break;

	case InputReportKind::KeyUp:
		if (input.Key == Key::Space
			&& source->_isSpaceKeyDown
			&& source->_clickMode != ::ClickMode::Hover)
		{
			source->_isSpaceKeyDown = false;
			if (!source->_leftButtonDown)
			{
				const bool shouldClick =
					source->_isPressed
					&& source->_clickMode
						== ::ClickMode::Release;
				source->SetPressed(false);
				source = dynamic_cast<ButtonBase*>(lifetime.Get());
				if (!source) return true;
				if (source->IsMouseCaptured())
					(void)source->ReleaseMouseCapture();
				source = dynamic_cast<ButtonBase*>(lifetime.Get());
				if (source && shouldClick)
					(void)source->OnClick();
			}
			else if (source->IsMouseCaptured())
			{
				source->SetPressed(
					source->_lastPointerInside);
			}
			return true;
		}
		break;

	case InputReportKind::FocusLost:
	case InputReportKind::Cancel:
		source->CancelPress(true);
		break;

	case InputReportKind::CaptureLost:
		source->_leftButtonDown = false;
		source->_defaultLeftButtonPressActive = false;
		if (!source->_isSpaceKeyDown)
			source->SetPressed(false);
		break;

	default:
		break;
	}
	return true;
}
