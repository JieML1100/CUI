#pragma once

#include "RangeBase.h"

class ProgressRing : public RangeBase
{
protected:
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override
	{
		return std::make_unique<RangeBaseAutomationPeer>(
			*this, AutomationControlType::ProgressBar, L"ProgressRing", true);
	}

private:
	bool _showPercentage = true;

public:
	virtual UIClass Type();
	static const DependencyProperty& ShowPercentageProperty();
	static void RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	void EnsureBindingPropertiesRegistered() override { RegisterDependencyProperties(); }
#endif
	PROPERTY(bool, ShowPercentage);
	GET(bool, ShowPercentage);
	SET(bool, ShowPercentage);

	ProgressRing();
protected:
	void OnRender() override;
};
