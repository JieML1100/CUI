#pragma once

#include "Control.h"
#include "ControlWeakReference.h"

/** WPF-compatible placement modes supported by the in-window popup layer. */
enum class PlacementMode : int
{
	Absolute = 0,
	Bottom,
	Top,
	Left,
	Right,
	Center
};

class Popup;
using PopupEvent = Event<void(Popup*)>;

/**
 * Transient presentation boundary.
 *
 * Popup remains in the logical/template ownership graph, but its visual
 * subtree is omitted from the retained content scene and presented through
 * Window's transient foreground layer. Window never owns or deletes it.
 */
class Popup : public Control
{
public:
	Popup();
	~Popup() override;
	UIClass Type() override { return UIClass::UI_Popup; }
	static void RegisterDependencyProperties();
	void EnsureBindingPropertiesRegistered() override
	{
		RegisterDependencyProperties();
	}

	bool GetIsOpen() const noexcept { return _isOpen; }
	void SetIsOpen(bool value);
	__declspec(property(get = GetIsOpen, put = SetIsOpen)) bool IsOpen;

	bool GetStaysOpen() const noexcept { return _staysOpen; }
	void SetStaysOpen(bool value);
	__declspec(property(get = GetStaysOpen, put = SetStaysOpen)) bool StaysOpen;

	PlacementMode GetPlacement() const noexcept { return _placement; }
	void SetPlacement(PlacementMode value);
	__declspec(property(get = GetPlacement, put = SetPlacement))
		PlacementMode Placement;

	Control* GetPlacementTarget() const noexcept
	{
		return _placementTarget.Get();
	}
	void SetPlacementTarget(Control* value);
	__declspec(property(get = GetPlacementTarget, put = SetPlacementTarget))
		Control* PlacementTarget;

	float GetHorizontalOffset() const noexcept { return _horizontalOffset; }
	void SetHorizontalOffset(float value);
	__declspec(property(get = GetHorizontalOffset, put = SetHorizontalOffset))
		float HorizontalOffset;

	float GetVerticalOffset() const noexcept { return _verticalOffset; }
	void SetVerticalOffset(float value);
	__declspec(property(get = GetVerticalOffset, put = SetVerticalOffset))
		float VerticalOffset;

	/** Returns the single authored Popup.Child visual. */
	Control* GetChild() const noexcept;
	/** Replaces Popup.Child and transfers ownership. */
	Control* SetChild(std::unique_ptr<Control> value);
	/** Detaches Popup.Child and returns its ownership. */
	std::unique_ptr<Control> DetachChild();

	PopupEvent Opened;
	PopupEvent Closed;

	bool ParticipatesInPresentationScene() const override { return false; }
	bool ClipsChildren() override { return true; }
	void UpdatePlacement();
protected:
	void PreparePresentation() override;
	void OnRender() override;
	cui::core::Size MeasureCore(
		const cui::core::Constraints& available) override;
	void Arrange(cui::core::Rect finalRect) override;
	void OnPresentationWindowChanged(
		Window* previousWindow, Window* currentWindow) override;
	bool ValidateVisualChildCollection(
		std::span<Control* const> children,
		std::string& error) const override;

private:
	bool _isOpen = false;
	bool _staysOpen = true;
	PlacementMode _placement = PlacementMode::Bottom;
	ControlWeakReference _placementTarget;
	float _horizontalOffset = 0.0f;
	float _verticalOffset = 0.0f;
	bool _applyingPlacement = false;

	void ApplyIsOpenChange(bool oldValue, bool newValue);
	void SynchronizeTransientPresentation();
	cui::core::Size MeasurePopupContent(
		const cui::core::Constraints& available);
};
