#pragma once

#include "HeaderedContentControl.h"

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
 * while expanded. Chrome and transition policy belong to its template; the
 * native implementation contains only a private fallback presenter.
 */
class Expander : public HeaderedContentControl
{
private:
	bool _isExpanded = true;
	::ExpandDirection _expandDirection = ::ExpandDirection::Down;
	bool _hoverHeader = false;
	bool _headerPressActive = false;

	void ApplyExpandedStateChange(bool oldValue, bool newValue);
	void SetCurrentExpanded(bool value);
	void SynchronizeContentPresentation();
	bool HeaderHitTest(int localX, int localY) const;
	cui::core::Rect HeaderRect() const noexcept;
	cui::core::Rect ContentRect() const noexcept;

protected:
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override
	{
		return std::make_unique<ExpanderAutomationPeer>(*this);
	}
	void ConfigureContentVisual(Control& child) override;
	cui::core::Insets GetHeaderPresentationInsets() const noexcept override;
	float GetHeaderSlotHeightDip(float availableWidth) override;
	void PerformPendingLayout() override;

public:
	using UIElement::Expanded;
	using UIElement::Collapsed;

	UIClass Type() override;
	static void RegisterDependencyProperties();
	void EnsureBindingPropertiesRegistered() override
	{
		RegisterDependencyProperties();
	}

	Expander();

	PROPERTY(bool, IsExpanded);
	GET(bool, IsExpanded);
	SET(bool, IsExpanded);

	PROPERTY(::ExpandDirection, ExpandDirection);
	GET(::ExpandDirection, ExpandDirection);
	SET(::ExpandDirection, ExpandDirection);

	void SetExpanded(bool value);
	void Toggle();

	cui::core::Size MeasureCore(
		const cui::core::Constraints& available) override;
	CursorKind QueryCursor(int localX, int localY) override;
	bool ClipsChildren() override { return true; }
	bool ShouldHitTestChildrenAt(int localX, int localY) const override;
	D2D1_RECT_F GetVisualChildrenClipRect() override;
	bool HandlesNavigationKey(Key key) const override;
protected:
	void OnRender() override;
	bool ProcessInput(const InputReport& input) override;
};
