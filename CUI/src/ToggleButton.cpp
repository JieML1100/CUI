#include "ToggleButton.h"

ToggleButton::ToggleButton()
	: ButtonBase()
{
	RegisterDependencyProperties();
}

const DependencyProperty& ToggleButton::IsCheckedProperty()
{
	static const auto registration = []
	{
		DependencyPropertyOptions<ToggleButton, NullableBool> options;
		options.DefaultValue = NullableBool(false);
		options.Flags = DependencyPropertyFlags::AffectsRender
			| DependencyPropertyFlags::BindsTwoWayByDefault;
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Behavior";
		options.Design.CategoryOrder = 300;
		options.Design.Order = 10;
		options.Design.Editor = DependencyPropertyEditorKind::Choice;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		options.Design.Choices = {
			{ L"False", BindingValue(NullableBool(false)) },
			{ L"True", BindingValue(NullableBool(true)) },
			{ L"Indeterminate", BindingValue(NullableBool{}) }
		};
		)
		options.Changed = [](ToggleButton& target,
			const NullableBool& oldValue,
			const NullableBool& value)
		{
			const ControlWeakReference lifetime(&target);
			target.SetStyleState(
				ControlStyleState::Checked, value == true);
			auto* live = dynamic_cast<ToggleButton*>(lifetime.Get());
			if (!live) return;
			live->OnIsCheckedChanged(oldValue, value);
			live = dynamic_cast<ToggleButton*>(lifetime.Get());
			if (!live) return;
			RoutedEventArgs args;
			if (value == true)
				live->Checked(live, args);
			else if (value == false)
				live->Unchecked(live, args);
			else
				live->Indeterminate(live, args);
			if (auto* source = dynamic_cast<ToggleButton*>(lifetime.Get()))
				source->NotifyAccessibilityStateChanged();
		};
		return DependencyPropertyRegistry::RegisterStatic<
			ToggleButton, NullableBool>(
				DependencyPropertyRegistrationLiteral(L"IsChecked"),
				[](ToggleButton& target) { return target.IsChecked; },
				[](ToggleButton& target, const NullableBool& value)
				{ target.IsChecked = value; },
				{}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& ToggleButton::IsThreeStateProperty()
{
	static const auto registration = []
	{
		DependencyPropertyOptions<ToggleButton, bool> options;
		options.DefaultValue = false;
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Behavior";
		options.Design.CategoryOrder = 300;
		options.Design.Order = 20;
		options.Design.Editor = DependencyPropertyEditorKind::Boolean;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		)
		return DependencyPropertyRegistry::RegisterStatic<ToggleButton, bool>(
			DependencyPropertyRegistrationLiteral(L"IsThreeState"),
			[](ToggleButton& target) { return target.IsThreeState; },
			[](ToggleButton& target, const bool& value)
			{
				target.IsThreeState = value;
			},
			{}, std::move(options));
	}();
	return *registration;
}

void ToggleButton::RegisterDependencyProperties()
{
	ButtonBase::RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	(void)IsCheckedProperty();
	(void)IsThreeStateProperty();
#endif
}

GET_CPP(ToggleButton, NullableBool, IsChecked)
{
	return _isChecked;
}

SET_CPP(ToggleButton, NullableBool, IsChecked)
{
	(void)SetPropertyField(IsCheckedProperty(), _isChecked, value);
}

GET_CPP(ToggleButton, bool, IsThreeState)
{
	return _isThreeState;
}

SET_CPP(ToggleButton, bool, IsThreeState)
{
	(void)SetPropertyField(IsThreeStateProperty(), _isThreeState, value);
}

void ToggleButton::OnToggle()
{
	NullableBool next;
	if (_isChecked == true)
		next = _isThreeState ? NullableBool{} : NullableBool(false);
	else if (_isChecked == false)
		next = NullableBool(true);
	else
		next = NullableBool(false);
	(void)TrySetCurrentPropertyValue(
		IsCheckedProperty(), BindingValue(next));
}

bool ToggleButton::OnClick()
{
	OnToggle();
	return ButtonBase::OnClick();
}
