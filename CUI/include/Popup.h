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
	Center,
	/** WPF ContextMenu default: use the pointer anchor supplied by its owner. */
	MousePoint
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
	/** WPF dependency-property identities used by generated/native code. */
	static const DependencyProperty& IsOpenProperty();
	static const DependencyProperty& StaysOpenProperty();
	static const DependencyProperty& PlacementProperty();
	static const DependencyProperty& PlacementTargetProperty();
	static const DependencyProperty& HorizontalOffsetProperty();
	static const DependencyProperty& VerticalOffsetProperty();
	static void RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	void EnsureBindingPropertiesRegistered() override
	{
		RegisterDependencyProperties();
	}
#endif

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
	bool PresentationSuppressionAffectsLayout() const noexcept override
	{
		return false;
	}
	bool BreaksVisualPresentationInheritance() const noexcept override
	{
		return true;
	}
	bool BlocksReverseInheritance() const noexcept override
	{
		return GetTemplatedParent() == nullptr;
	}
	bool ClipsChildren() override { return true; }
	void UpdatePlacement();
protected:
	void PreparePresentation() override;
	void OnRender() override;
	void RequestLayout() override;
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
	// A Popup is retained as a transient scene root.  Re-measuring its child
	// during every paint turns an otherwise cached frame into a full submenu
	// layout pass, so keep the measured content until layout invalidates it.
	bool _placementContentDirty = true;
	bool _hasPlacementSnapshot = false;
	cui::core::Size _measuredPopupContent{};
	cui::core::Size _placementViewport{};
	cui::core::Rect _placementTargetRect{};

	void ApplyIsOpenChange(bool oldValue, bool newValue);
	void SynchronizeTransientPresentation();
	cui::core::Size MeasurePopupContent(
		const cui::core::Constraints& available);
};
