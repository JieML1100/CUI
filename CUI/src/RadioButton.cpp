#include "RadioButton.h"
#include "TreeInfrastructure.h"
#include <algorithm>
#include <functional>
#include <unordered_set>
#include <vector>

UIClass RadioButton::Type() { return UIClass::UI_RadioButton; }

RadioButton::RadioButton()
{
	RegisterDependencyProperties();
	const ControlWeakReference lifetime(this);
	RetainEventConnection(
		cui::framework::TreeAccess::SubscribeLogicalParentChanged(
			*this, [lifetime](Control*, Control*, Control*)
			{
				auto* radio = dynamic_cast<RadioButton*>(lifetime.Get());
				if (radio && radio->IsChecked)
					radio->UpdateRadioButtonGroup();
			}));
	RetainEventConnection(
		cui::framework::TreeAccess::SubscribeVisualParentChanged(
			*this, [lifetime](Control*, Control*, Control*)
			{
				auto* radio = dynamic_cast<RadioButton*>(lifetime.Get());
				if (radio && radio->IsChecked
					&& !radio->GetLogicalParent())
					radio->UpdateRadioButtonGroup();
			}));
}

void RadioButton::RegisterDependencyProperties()
{
	ToggleButton::RegisterDependencyProperties();
	static const bool registered = []
	{
		DependencyPropertyOptions<RadioButton, std::wstring> options;
		options.DefaultValue = std::wstring{};
		options.Design.Category = L"Behavior";
		options.Design.CategoryOrder = 300;
		options.Design.Order = 20;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		options.Changed = [](RadioButton& target,
			const std::wstring&, const std::wstring&)
		{
			if (target.IsChecked)
				target.UpdateRadioButtonGroup();
		};
		DependencyPropertyRegistry::Register<RadioButton, std::wstring>(
			L"GroupName",
			[](RadioButton& target) { return target.GroupName; },
			[](RadioButton& target, const std::wstring& value)
			{ target.GroupName = value; },
			{}, std::move(options));
		return true;
	}();
	(void)registered;
}

void RadioButton::OnIsCheckedChanged(bool oldValue, bool newValue)
{
	(void)oldValue;
	if (newValue)
		UpdateRadioButtonGroup();
}

void RadioButton::UpdateRadioButtonGroup()
{
	if (!IsChecked) return;

	const ControlWeakReference selfReference(this);
	std::vector<ControlWeakReference> peers;
	auto remember = [&](Control* candidate)
	{
		auto* radio = dynamic_cast<RadioButton*>(candidate);
		if (radio && radio != this && radio->IsChecked
			&& radio->GroupName == _groupName)
			peers.emplace_back(radio);
	};

	if (_groupName.empty())
	{
		if (auto* parent = GetLogicalParent())
		{
			for (auto* child : parent->GetLogicalChildrenView())
				remember(child);
		}
		else if (auto* parent = GetVisualParent())
		{
			for (auto* child : parent->GetVisualChildrenView())
				remember(child);
		}
	}
	else
	{
		Control* root = this;
		while (auto* parent = root->GetRoutedParent())
			root = parent;
		std::unordered_set<Control*> visited;
		std::function<void(Control*)> collect = [&](Control* node)
		{
			if (!node || !visited.insert(node).second) return;
			remember(node);
			for (auto* child : node->GetVisualChildrenView())
				collect(child);
			for (auto* child : node->GetLogicalChildrenView())
				collect(child);
		};
		collect(root);
	}

	for (const auto& peerReference : peers)
	{
		if (!selfReference.Get()) return;
		auto* peer = dynamic_cast<RadioButton*>(peerReference.Get());
		if (!peer || !peer->IsChecked || peer->GroupName != _groupName)
			continue;
		peer->SetChecked(false);
	}
}

void RadioButton::BeforeDefaultMouseUp(MouseButton button, MouseEventArgs& e, bool hasMatchingPress)
{
	(void)e;
	if (button == MouseButton::Left && hasMatchingPress && this->IsChecked == false)
		SetChecked(true);
}

bool RadioButton::Invoke()
{
	if (!IsEnabled || !IsVisible) return false;
	if (!IsChecked)
		SetChecked(true);
	RoutedEventArgs eventArgs;
	Click(this, eventArgs);
	return true;
}

void RadioButton::SetChecked(bool checked)
{
	if (IsChecked == checked) return;
	(void)TrySetCurrentPropertyValue(
		L"IsChecked", BindingValue(checked));
}

GET_CPP(RadioButton, std::wstring, GroupName)
{
	return _groupName;
}

SET_CPP(RadioButton, std::wstring, GroupName)
{
	(void)SetPropertyField(L"GroupName", _groupName, std::move(value));
}
