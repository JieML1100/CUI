#include "ToggleButton.h"
#include "Window.h"

namespace
{
	auto IsCheckedSubscriber()
	{
		return [](ToggleButton& target,
			DependencyPropertyMetadata::ChangeHandler handler,
			DataSourceUpdateMode)
		{
			return target.OnPropertyValueChanged.Subscribe(
				[handler = std::move(handler)](
					DependencyObject*,
					const DependencyPropertyChangedEventArgs& args)
				{
					if (args.PropertyName == L"IsChecked")
						handler();
				});
		};
	}
}

ToggleButton::ToggleButton()
	: ButtonBase()
{
	RegisterDependencyProperties();
}

void ToggleButton::RegisterDependencyProperties()
{
	ButtonBase::RegisterDependencyProperties();
	static const bool registered = []
	{
		DependencyPropertyOptions<ToggleButton, bool> options;
		options.DefaultValue = false;
		options.Flags = DependencyPropertyFlags::AffectsRender;
		options.Design.Category = L"Behavior";
		options.Design.CategoryOrder = 300;
		options.Design.Order = 10;
		options.Design.Editor = DependencyPropertyEditorKind::Boolean;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		options.Changed = [](ToggleButton& target, const bool& oldValue,
			const bool& value)
		{
			const ControlWeakReference lifetime(&target);
			target.SetStyleState(ControlStyleState::Checked, value);
			auto* live = dynamic_cast<ToggleButton*>(lifetime.Get());
			if (!live) return;
			live->OnIsCheckedChanged(oldValue, value);
			live = dynamic_cast<ToggleButton*>(lifetime.Get());
			if (!live) return;
			RoutedEventArgs args;
			if (value) live->Checked(live, args);
			else live->Unchecked(live, args);
			if (auto* source = dynamic_cast<ToggleButton*>(lifetime.Get()))
				source->NotifyAccessibilityStateChanged();
		};
		DependencyPropertyRegistry::Register<ToggleButton, bool>(L"IsChecked",
			[](ToggleButton& target) { return target.IsChecked; },
			[](ToggleButton& target, const bool& value)
			{ target.IsChecked = value; },
			IsCheckedSubscriber(), std::move(options));
		return true;
	}();
	(void)registered;
}

GET_CPP(ToggleButton, bool, IsChecked)
{
	return _isChecked;
}

SET_CPP(ToggleButton, bool, IsChecked)
{
	(void)SetPropertyField(L"IsChecked", _isChecked, value);
}
