#pragma once

#include "HeaderedContentControl.h"

class ToggleButton;

/** WPF-compatible direction in which Expander content is placed. */
enum class ExpandDirection : unsigned char
{
	Down,
	Up,
	Left,
	Right,
};

/**
 * WPF-style HeaderedContentControl whose content participates in measure only
 * while expanded. Chrome, direction layout and transition policy belong to
 * its framework theme ControlTemplate.
 */
class Expander : public HeaderedContentControl
{
private:
	bool _isExpanded = false;
	::ExpandDirection _expandDirection = ::ExpandDirection::Down;
	ToggleButton* _headerSite = nullptr;

	void ApplyExpandedStateChange(bool oldValue, bool newValue);
	void SetCurrentExpanded(bool value);
	void SynchronizeHeaderSite();

protected:
	const DependencyPropertyMetadata* ResolveExactDependencyPropertyMetadata(
		const DependencyProperty& property) const override;
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override
	{
		return std::make_unique<ExpanderAutomationPeer>(*this);
	}
	void OnControlTemplatePresentationChanged() override;
	bool OnAccessKey(bool isMultiple) override;

public:
	using UIElement::Expanded;
	using UIElement::Collapsed;

	UIClass Type() override;
	static const DependencyProperty& IsExpandedProperty();
	static const DependencyProperty& ExpandDirectionProperty();
	static void RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	void EnsureBindingPropertiesRegistered() override
	{
		RegisterDependencyProperties();
	}
#endif

	Expander();

	PROPERTY(bool, IsExpanded);
	GET(bool, IsExpanded);
	SET(bool, IsExpanded);

	PROPERTY(::ExpandDirection, ExpandDirection);
	GET(::ExpandDirection, ExpandDirection);
	SET(::ExpandDirection, ExpandDirection);

	void SetExpanded(bool value);
	void Toggle();
};
