#pragma once
#include "HeaderedContentControl.h"

/**
 * @file GroupBox.h
 * @brief GroupBox: WPF HeaderedContentControl behavior host.
 *
 * Header, content and border presentation belong to the framework theme's
 * ControlTemplate. The native class only supplies semantics and automation.
 */
class GroupBox : public HeaderedContentControl
{
protected:
	const DependencyPropertyMetadata* ResolveExactDependencyPropertyMetadata(
		const DependencyProperty& property) const override;
	bool OnAccessKey(bool isMultiple) override;
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override
	{
		return std::make_unique<AutomationPeer>(
			*this, AutomationControlType::Group, L"GroupBox");
	}

public:
	GroupBox();

	UIClass Type() override;
	static void RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	void EnsureBindingPropertiesRegistered() override { RegisterDependencyProperties(); }
#endif
};
