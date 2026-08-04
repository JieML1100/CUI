#pragma once

#include "ContentControl.h"

#include <cstdint>

/** WPF ButtonBase.ClickMode activation timing. */
enum class ClickMode : unsigned char
{
	Release,
	Press,
	Hover,
};

/** WPF-style semantic base for all mouse, keyboard and automation buttons. */
class ButtonBase : public ContentControl
{
private:
	bool _isPressed = false;
	bool _isSpaceKeyDown = false;
	bool _leftButtonDown = false;
	bool _lastPointerInside = false;
	int _lastPointerX = 0;
	int _lastPointerY = 0;
	::ClickMode _clickMode = ::ClickMode::Release;
	std::wstring _command;
	BindingValue _commandParameter;
	ControlWeakReference _commandTarget;
	EventConnection _commandCanExecuteConnection;
	std::uint64_t _commandSourceRefreshVersion = 0;
	static const DependencyPropertyKey& IsPressedPropertyKey();
	static void EnsureClassHandlers();
	static void HandleDescendantPointerPress(
		Control* sender, RoutedEventArgs& args);
	void BeginPointerPress(MouseEventArgs& args);
	void CancelPress(bool releaseCapture);
	void ApplyCommandTarget(const ControlWeakReference& value);
	void RefreshCommandSource();
	bool ExecuteCommandSource();

protected:
	const DependencyPropertyMetadata* ResolveExactDependencyPropertyMetadata(
		const DependencyProperty& property) const override;
	/** Derived WPF controls may clear the read-only pressed visual state. */
	void SetPressed(bool value);
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override
	{
		return std::make_unique<InvokeAutomationPeer>(
			*this, AutomationControlType::Button, L"Button");
	}
	bool DefaultSelectOnLeftButtonDown() const override { return false; }
	bool DefaultRaiseClickOnLeftButtonUp() const override { return false; }
	void BeforeDefaultMouseMove(MouseEventArgs& args) override;
	void BeforeDefaultMouseDown(
		MouseButton button, MouseEventArgs& args) override;
	void BeforeDefaultMouseUp(
		MouseButton button,
		MouseEventArgs& args,
		bool hasMatchingPress) override;
	void OnIsMouseOverChanged(bool previous, bool current) override;
	void OnEffectiveIsEnabledChanged(
		bool previousValue, bool currentValue) override;
	void OnPresentationWindowChanged(
		PresentationWindow* previousWindow,
		PresentationWindow* currentWindow) override;
	void OnComputedLayoutSizeChanged() override;
	void OnPressedVisualStateChanged(bool value) override;
	bool ProcessInput(const InputReport& input) override;
	bool OnAccessKey(bool isMultiple) override;
	/** Raises Click and then executes this WPF ICommandSource. */
	virtual bool OnClick();

public:
	using UIElement::Click;

	ButtonBase();
	UIClass Type() override { return UIClass::UI_ButtonBase; }
	/** WPF dependency-property identities used by generated/native code. */
	static const DependencyProperty& IsPressedProperty();
	static const DependencyProperty& ClickModeProperty();
	static const DependencyProperty& CommandProperty();
	static const DependencyProperty& CommandParameterProperty();
	static const DependencyProperty& CommandTargetProperty();
	static void RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	void EnsureBindingPropertiesRegistered() override
	{
		RegisterDependencyProperties();
	}
#endif
	READONLY_PROPERTY(bool, IsPressed);
	GET(bool, IsPressed);
	PROPERTY(::ClickMode, ClickMode);
	GET(::ClickMode, ClickMode);
	SET(::ClickMode, ClickMode);
	/** XAML-authored routed-command identity shared by every ButtonBase. */
	PROPERTY(std::wstring, Command);
	GET(std::wstring, Command);
	SET(std::wstring, Command);
	/** WPF object-valued parameter passed unchanged to the routed command. */
	PROPERTY(BindingValue, CommandParameter);
	GET(BindingValue, CommandParameter);
	SET(BindingValue, CommandParameter);
	/** Optional authored routed-command target. An expired target stays authored. */
	PROPERTY(class Control*, CommandTarget);
	GET(class Control*, CommandTarget);
	SET(class Control*, CommandTarget);
	bool HasAuthoredCommandTarget() const noexcept
	{
		return _commandTarget.HasValue();
	}
	/** Removes the authored target so focus-based resolution is used again. */
	void ClearCommandTarget();
	bool HandlesNavigationKey(Key key) const override;
	bool Invoke() override;
};
