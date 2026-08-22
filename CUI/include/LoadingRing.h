#pragma once

#include "Control.h"

class LoadingRing : public Control
{
protected:
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override
	{
		return std::make_unique<AutomationPeer>(
			*this, AutomationControlType::ProgressBar, L"LoadingRing");
	}

private:
	bool _active = true;
	ULONGLONG _animStartTick = 0;
	UINT _animationPeriodMs = 900;

	float GetAnimationPhase() const;

public:
	virtual UIClass Type();
	static void RegisterDependencyProperties();
	static const DependencyProperty& IsActiveProperty();
#if CUI_ENABLE_DYNAMIC_XAML
	void EnsureBindingPropertiesRegistered() override { RegisterDependencyProperties(); }
#endif

	PROPERTY(bool, IsActive);
	GET(bool, IsActive);
	SET(bool, IsActive);

	LoadingRing();
	bool IsAnimationRunning() override;
	bool HasRetainedNativeAnimation() override { return _active; }
	UINT GetAnimationIntervalMs() override { return 16; }
	bool GetAnimatedInvalidRect(D2D1_RECT_F& outRect) override;
protected:
	void OnRender() override;
};
