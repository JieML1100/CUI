#pragma once
#include "HeaderedContentControl.h"

/**
 * @file GroupBox.h
 * @brief GroupBox: lightweight container with a captioned border.
 */
class GroupBox : public HeaderedContentControl
{
private:
	float _captionMarginLeft = 12.0f;
	float _captionPaddingX = 6.0f;
	float _captionPaddingY = 2.0f;
	float _captionCornerRadius = 6.0f;
	D2D1_COLOR_F _captionBackColor = D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 0.0f };
	D2D1_COLOR_F _captionBorderColor = D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 0.0f };

protected:
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override
	{
		return std::make_unique<AutomationPeer>(
			*this, AutomationControlType::Group, L"GroupBox");
	}
	void PerformPendingLayout() override;
	cui::core::Insets GetHeaderPresentationInsets() const noexcept override;
	float GetHeaderSlotHeightDip(float availableWidth) override;

public:
	GroupBox();

	UIClass Type() override;
	static void RegisterDependencyProperties();
	void EnsureBindingPropertiesRegistered() override { RegisterDependencyProperties(); }

protected:
	void OnRender() override;

private:
	void PerformGroupLayoutIfNeeded();
	float GetCaptionBandHeight();
};
