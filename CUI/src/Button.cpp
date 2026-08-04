#include "Button.h"
#include "Window.h"

#include <optional>
#include <stdexcept>
#include <typeindex>

UIClass Button::Type()
{
	return UIClass::UI_Button;
}

const DependencyProperty& Button::IsDefaultProperty()
{
	static const auto registration = []
	{
		auto options = DependencyPropertyOptions<Button, bool>{
			false, DependencyPropertyFlags::AffectsRender };
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Behavior";
		options.Design.CategoryOrder = 300;
		options.Design.Order = 10;
		options.Design.Editor = DependencyPropertyEditorKind::Boolean;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		)
		options.Changed = [](
			Button& target, const bool&, const bool&)
		{
			auto* window = target.GetPresentationWindow();
			target.UpdateIsDefaulted(
				window ? window->GetKeyboardFocusedElement() : nullptr);
		};
		return DependencyPropertyRegistry::RegisterStatic<Button, bool>(
			DependencyPropertyRegistrationLiteral(L"IsDefault"),
			[](Button& target) { return target.IsDefault; },
			[](Button& target, const bool& value) { target.IsDefault = value; },
			{}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& Button::IsCancelProperty()
{
	static const auto registration = []
	{
		auto options = DependencyPropertyOptions<Button, bool>{
			false, DependencyPropertyFlags::None };
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Behavior";
		options.Design.CategoryOrder = 300;
		options.Design.Order = 20;
		options.Design.Editor = DependencyPropertyEditorKind::Boolean;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		)
		return DependencyPropertyRegistry::RegisterStatic<Button, bool>(
			DependencyPropertyRegistrationLiteral(L"IsCancel"),
			[](Button& target) { return target.IsCancel; },
			[](Button& target, const bool& value) { target.IsCancel = value; },
			{}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& Button::IsDefaultedProperty()
{
	return IsDefaultedPropertyKey().Property();
}

const DependencyPropertyKey& Button::IsDefaultedPropertyKey()
{
	static const auto registration = []
	{
		DependencyPropertyOptions<Button, bool> options;
		options.DefaultValue = false;
		options.Flags = DependencyPropertyFlags::AffectsRender;
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"State";
		options.Design.CategoryOrder = 70;
		options.Design.Order = 70;
		options.Design.Editor = DependencyPropertyEditorKind::Boolean;
		options.Design.Persistence = DependencyPropertyPersistence::Transient;
		options.Design.Browsable = false;
		)
		return DependencyPropertyRegistry::RegisterReadOnlyStatic<Button, bool>(
			DependencyPropertyRegistrationLiteral(L"IsDefaulted"),
			[](Button& target) { return target.IsDefaulted; },
			[](Button& target, const bool& value)
			{
				(void)target.SetReadOnlyPropertyField(
					IsDefaultedPropertyKey(), target._isDefaulted, value);
			}, {}, std::move(options));
	}();
	return registration.Key();
}

void Button::RegisterDependencyProperties()
{
	ButtonBase::RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	(void)IsDefaultProperty();
	(void)IsCancelProperty();
	(void)IsDefaultedProperty();
#endif
}

GET_CPP(Button, bool, IsDefault) { return _isDefault; }
SET_CPP(Button, bool, IsDefault)
{
	(void)SetPropertyField(IsDefaultProperty(), _isDefault, value);
}
GET_CPP(Button, bool, IsCancel) { return _isCancel; }
SET_CPP(Button, bool, IsCancel)
{
	(void)SetPropertyField(IsCancelProperty(), _isCancel, value);
}
GET_CPP(Button, bool, IsDefaulted) { return _isDefaulted; }

Button::Button()
	: ButtonBase()
{
	RegisterDependencyProperties();
}

bool Button::OnClick()
{
	return ButtonBase::OnClick();
}

void Button::OnEffectiveIsEnabledChanged(
	bool previousValue, bool currentValue)
{
	const ControlWeakReference lifetime(this);
	ButtonBase::OnEffectiveIsEnabledChanged(
		previousValue, currentValue);
	auto* source = dynamic_cast<Button*>(lifetime.Get());
	if (!source) return;
	auto* window = source->GetPresentationWindow();
	source->UpdateIsDefaulted(
		window ? window->GetKeyboardFocusedElement() : nullptr);
}

void Button::OnPresentationWindowChanged(
	PresentationWindow* previousWindow,
	PresentationWindow* currentWindow)
{
	const ControlWeakReference lifetime(this);
	ButtonBase::OnPresentationWindowChanged(
		previousWindow, currentWindow);
	auto* source = dynamic_cast<Button*>(lifetime.Get());
	if (!source) return;
	auto* window = source->GetPresentationWindow();
	source->UpdateIsDefaulted(
		window ? window->GetKeyboardFocusedElement() : nullptr);
}

void Button::UpdateIsDefaulted(Control* focused)
{
	auto* window = GetPresentationWindow();
	bool isDefaulted = _isDefault && focused && window
		&& focused->GetPresentationWindow() == window
		&& IsEffectivelyEnabled() && IsVisible
		&& !focused->HandlesNavigationKey(Key::Return)
		&& window->GetFocusScope(this)
			== window->GetFocusScope(focused);
	if (_isDefaulted == isDefaulted) return;
	(void)SetReadOnlyPropertyField(
		IsDefaultedPropertyKey(),
		_isDefaulted, isDefaulted);
}
