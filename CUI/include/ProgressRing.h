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
	static void RegisterDependencyProperties();
	void EnsureBindingPropertiesRegistered() override { RegisterDependencyProperties(); }
	PROPERTY(bool, ShowPercentage);
	GET(bool, ShowPercentage);
	SET(bool, ShowPercentage);

	ProgressRing();
protected:
	void OnRender() override;
};
