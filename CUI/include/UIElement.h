#pragma once

#include "Event.h"
#include "InputReport.h"
#include "FocusManager.h"
#include "Layout/LayoutState.h"
#include "RoutedCommand.h"
#include "Visual.h"

#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

class Control;
struct CommandBindingCollectionState;
struct CommandCanExecuteObserverState;

namespace cui::framework
{
	struct RoutedEventAccess;
}

/** WPF visibility semantics: Hidden keeps layout space; Collapsed does not. */
enum class Visibility : unsigned char
{
	Visible,
	Hidden,
	Collapsed
};

inline const wchar_t* VisibilityName(Visibility value) noexcept
{
	switch (value)
	{
	case Visibility::Visible: return L"Visible";
	case Visibility::Hidden: return L"Hidden";
	case Visibility::Collapsed: return L"Collapsed";
	}
	return L"Visible";
}

using MouseWheelEvent = RoutedEvent<MouseEventArgs>;
using MouseMoveEvent = RoutedEvent<MouseEventArgs>;
using MouseUpEvent = RoutedEvent<MouseEventArgs>;
using MouseDownEvent = RoutedEvent<MouseEventArgs>;
using MouseDoubleClickEvent = RoutedEvent<MouseEventArgs>;
using ClickEvent = RoutedEvent<RoutedEventArgs>;
using TextChangedEvent = RoutedEvent<TextChangedEventArgs>;
using PasswordChangedEvent = RoutedEvent<RoutedEventArgs>;
using ScrollChangedEvent = RoutedEvent<ScrollChangedEventArgs>;
using SelectionChangedRoutedEvent = RoutedEvent<SelectionChangedEventArgs>;
using SelectedItemChangedRoutedEvent =
	RoutedEvent<RoutedPropertyChangedEventArgs<BindingValue>>;
using ValueChangedRoutedEvent =
	RoutedEvent<RoutedPropertyChangedEventArgs<double>>;
using SelectionStateEvent = RoutedEvent<RoutedEventArgs>;
using MouseEnterEvent = RoutedEvent<MouseEventArgs>;
using MouseLeaveEvent = RoutedEvent<MouseEventArgs>;
using KeyUpEvent = RoutedEvent<KeyEventArgs>;
using KeyDownEvent = RoutedEvent<KeyEventArgs>;
using TextInputEvent = RoutedEvent<TextCompositionEventArgs>;
using KeyboardFocusEvent = RoutedEvent<KeyboardFocusChangedEventArgs>;
using FocusEvent = RoutedEvent<RoutedEventArgs>;
using CanExecuteEvent = RoutedEvent<CanExecuteRoutedEventArgs>;
using ExecutedEvent = RoutedEvent<ExecutedRoutedEventArgs>;
using DragDropEvent = RoutedEvent<DragEventArgs>;
using SizeChangedEvent = RoutedEvent<SizeChangedEventArgs>;
using GotFocusEvent = FocusEvent;
using LostFocusEvent = FocusEvent;
using GotMouseCaptureEvent = FocusEvent;
using LostMouseCaptureEvent = FocusEvent;
using IsVisibleChangedEvent = DependencyPropertyChangedEvent;

/** Owns layout-pass state, input publication, focus participation and hit testing. */
class UIElement : public Visual
{
protected:
	friend class RoutedCommandManager;
	friend struct cui::framework::RoutedEventAccess;
	cui::layout::LayoutState _layoutState;
	Visibility _visibility = Visibility::Visible;
	bool _focusable = false;
	bool _isTabStop = true;
	int _tabIndex = 0;
	bool _isFocused = false;
	bool _isKeyboardFocused = false;
	bool _isKeyboardFocusWithin = false;
	bool _isMouseOver = false;
	bool _isMouseDirectlyOver = false;
	bool _allowDrop = false;
	/** Framework presentation projection; never replaces authored Visibility. */
	bool _presentationSuppressed = false;
	bool _isFocusScope = false;
	KeyboardNavigationMode _tabNavigation = KeyboardNavigationMode::Continue;
	KeyboardNavigationMode _directionalNavigation = KeyboardNavigationMode::Continue;
	bool _defaultLeftButtonPressActive = false;
	std::shared_ptr<CommandBindingCollectionState> _commandBindings;
	std::shared_ptr<CommandCanExecuteObserverState> _commandCanExecuteObservers;
	std::vector<InputBinding> _inputBindings;
	std::array<std::unique_ptr<RoutedEvent<RoutedEventArgs>>,
		static_cast<std::size_t>(RoutedEventId::Count)>
		_genericRoutedEventHandlers;
	void InvalidateCommandInfrastructureForDestruction() noexcept;

public:
	UIElement()
		: OnPreviewMouseWheel(this, RoutedEventId::PreviewMouseWheel),
		OnMouseWheel(this, RoutedEventId::MouseWheel),
		OnPreviewMouseMove(this, RoutedEventId::PreviewMouseMove),
		OnMouseMove(this, RoutedEventId::MouseMove),
		OnPreviewMouseUp(this, RoutedEventId::PreviewMouseUp),
		OnMouseUp(this, RoutedEventId::MouseUp),
		OnPreviewMouseDown(this, RoutedEventId::PreviewMouseDown),
		OnMouseDown(this, RoutedEventId::MouseDown),
		OnPreviewMouseDoubleClick(this, RoutedEventId::PreviewMouseDoubleClick),
		OnMouseDoubleClick(this, RoutedEventId::MouseDoubleClick),
		Click(this, RoutedEventId::Click),
		SizeChanged(this, RoutedEventId::SizeChanged),
		OnMouseEnter(this, RoutedEventId::MouseEnter),
		OnMouseLeave(this, RoutedEventId::MouseLeave),
		OnGotMouseCapture(this, RoutedEventId::GotMouseCapture),
		OnLostMouseCapture(this, RoutedEventId::LostMouseCapture),
		OnPreviewKeyUp(this, RoutedEventId::PreviewKeyUp),
		OnKeyUp(this, RoutedEventId::KeyUp),
		OnPreviewKeyDown(this, RoutedEventId::PreviewKeyDown),
		OnKeyDown(this, RoutedEventId::KeyDown),
		OnPreviewTextInputStart(this, RoutedEventId::PreviewTextInputStart),
		OnTextInputStart(this, RoutedEventId::TextInputStart),
		OnPreviewTextInputUpdate(this, RoutedEventId::PreviewTextInputUpdate),
		OnTextInputUpdate(this, RoutedEventId::TextInputUpdate),
		OnPreviewTextInput(this, RoutedEventId::PreviewTextInput),
		OnTextInput(this, RoutedEventId::TextInput),
		OnPreviewGotKeyboardFocus(
			this, RoutedEventId::PreviewGotKeyboardFocus),
		OnGotKeyboardFocus(this, RoutedEventId::GotKeyboardFocus),
		OnPreviewLostKeyboardFocus(
			this, RoutedEventId::PreviewLostKeyboardFocus),
		OnLostKeyboardFocus(this, RoutedEventId::LostKeyboardFocus),
		OnGotFocus(this, RoutedEventId::GotFocus),
		OnLostFocus(this, RoutedEventId::LostFocus),
		OnPreviewCanExecute(this, RoutedEventId::PreviewCanExecute),
		OnCanExecute(this, RoutedEventId::CanExecute),
		OnPreviewExecuted(this, RoutedEventId::PreviewExecuted),
		OnExecuted(this, RoutedEventId::Executed),
		OnPreviewDragEnter(this, RoutedEventId::PreviewDragEnter),
		OnDragEnter(this, RoutedEventId::DragEnter),
		OnPreviewDragOver(this, RoutedEventId::PreviewDragOver),
		OnDragOver(this, RoutedEventId::DragOver),
		OnPreviewDragLeave(this, RoutedEventId::PreviewDragLeave),
		OnDragLeave(this, RoutedEventId::DragLeave),
		OnPreviewDrop(this, RoutedEventId::PreviewDrop),
		OnDrop(this, RoutedEventId::Drop),
		OnTextChanged(this, RoutedEventId::TextChanged),
		PasswordChanged(this, RoutedEventId::PasswordChanged),
		OnScrollChanged(this, RoutedEventId::ScrollChanged),
		SelectionChanged(this, RoutedEventId::SelectionChanged),
		SelectedDatesChanged(this, RoutedEventId::SelectedDatesChanged),
		SelectedItemChanged(this, RoutedEventId::SelectedItemChanged),
		ValueChanged(this, RoutedEventId::ValueChanged),
		Selected(this, RoutedEventId::Selected),
		Unselected(this, RoutedEventId::Unselected),
		Checked(this, RoutedEventId::Checked),
		Unchecked(this, RoutedEventId::Unchecked),
		Expanded(this, RoutedEventId::Expanded),
		Collapsed(this, RoutedEventId::Collapsed),
		SubmenuOpened(this, RoutedEventId::SubmenuOpened),
		SubmenuClosed(this, RoutedEventId::SubmenuClosed) {}
	~UIElement() override;

	MouseWheelEvent OnPreviewMouseWheel;
	MouseWheelEvent OnMouseWheel;
	MouseMoveEvent OnPreviewMouseMove;
	MouseMoveEvent OnMouseMove;
	MouseUpEvent OnPreviewMouseUp;
	MouseUpEvent OnMouseUp;
	MouseDownEvent OnPreviewMouseDown;
	MouseDownEvent OnMouseDown;
	MouseDoubleClickEvent OnPreviewMouseDoubleClick;
	MouseDoubleClickEvent OnMouseDoubleClick;
	MouseEnterEvent OnMouseEnter;
	MouseLeaveEvent OnMouseLeave;
	GotMouseCaptureEvent OnGotMouseCapture;
	LostMouseCaptureEvent OnLostMouseCapture;
	KeyUpEvent OnPreviewKeyUp;
	KeyUpEvent OnKeyUp;
	KeyDownEvent OnPreviewKeyDown;
	KeyDownEvent OnKeyDown;
	TextInputEvent OnPreviewTextInputStart;
	TextInputEvent OnTextInputStart;
	TextInputEvent OnPreviewTextInputUpdate;
	TextInputEvent OnTextInputUpdate;
	TextInputEvent OnPreviewTextInput;
	TextInputEvent OnTextInput;
	KeyboardFocusEvent OnPreviewGotKeyboardFocus;
	KeyboardFocusEvent OnGotKeyboardFocus;
	KeyboardFocusEvent OnPreviewLostKeyboardFocus;
	KeyboardFocusEvent OnLostKeyboardFocus;
	SizeChangedEvent SizeChanged;
	GotFocusEvent OnGotFocus;
	LostFocusEvent OnLostFocus;
	CanExecuteEvent OnPreviewCanExecute;
	CanExecuteEvent OnCanExecute;
	ExecutedEvent OnPreviewExecuted;
	ExecutedEvent OnExecuted;
	DragDropEvent OnPreviewDragEnter;
	DragDropEvent OnDragEnter;
	DragDropEvent OnPreviewDragOver;
	DragDropEvent OnDragOver;
	DragDropEvent OnPreviewDragLeave;
	DragDropEvent OnDragLeave;
	DragDropEvent OnPreviewDrop;
	DragDropEvent OnDrop;

protected:
	/** Route storage exposed only by WPF controls that own a Click facade. */
	ClickEvent Click;
	/** Exposed only by TextBox/RichTextBox through a using-declaration. */
	TextChangedEvent OnTextChanged;
	/** Exposed only by PasswordBox through a using-declaration. */
	PasswordChangedEvent PasswordChanged;
	/** Exposed only by ScrollViewer through a using-declaration. */
	ScrollChangedEvent OnScrollChanged;
	/** Exposed by controls that own WPF selection/value semantics. */
	SelectionChangedRoutedEvent SelectionChanged;
	SelectionChangedRoutedEvent SelectedDatesChanged;
	SelectedItemChangedRoutedEvent SelectedItemChanged;
	ValueChangedRoutedEvent ValueChanged;
	/** Exposed only by selector item containers such as TabItem. */
	SelectionStateEvent Selected;
	SelectionStateEvent Unselected;
	/** Exposed only by controls that own the corresponding WPF state. */
	SelectionStateEvent Checked;
	SelectionStateEvent Unchecked;
	SelectionStateEvent Expanded;
	SelectionStateEvent Collapsed;
	SelectionStateEvent SubmenuOpened;
	SelectionStateEvent SubmenuClosed;

	/** Internal route endpoint for owner-specific semantic routed events. */
	RoutedHandlerInvocationCount InvokeSemanticRoutedEventHandlers(
		Control* sender, RoutedEventArgs& args)
	{
		switch (args.EventId)
		{
		case RoutedEventId::Click:
			return Click.InvokeHandlers(sender, args);
		case RoutedEventId::TextChanged:
			return OnTextChanged.InvokeHandlers(
				sender, static_cast<TextChangedEventArgs&>(args));
		case RoutedEventId::PasswordChanged:
			return PasswordChanged.InvokeHandlers(sender, args);
		case RoutedEventId::ScrollChanged:
			return OnScrollChanged.InvokeHandlers(
				sender, static_cast<ScrollChangedEventArgs&>(args));
		case RoutedEventId::SelectionChanged:
			return SelectionChanged.InvokeHandlers(
				sender, static_cast<SelectionChangedEventArgs&>(args));
		case RoutedEventId::SelectedDatesChanged:
			return SelectedDatesChanged.InvokeHandlers(
				sender, static_cast<SelectionChangedEventArgs&>(args));
		case RoutedEventId::SelectedItemChanged:
			return SelectedItemChanged.InvokeHandlers(sender,
				static_cast<RoutedPropertyChangedEventArgs<BindingValue>&>(args));
		case RoutedEventId::ValueChanged:
			return ValueChanged.InvokeHandlers(sender,
				static_cast<RoutedPropertyChangedEventArgs<double>&>(args));
		case RoutedEventId::Selected:
			return Selected.InvokeHandlers(sender, args);
		case RoutedEventId::Unselected:
			return Unselected.InvokeHandlers(sender, args);
		case RoutedEventId::Checked:
			return Checked.InvokeHandlers(sender, args);
		case RoutedEventId::Unchecked:
			return Unchecked.InvokeHandlers(sender, args);
		case RoutedEventId::Expanded:
			return Expanded.InvokeHandlers(sender, args);
		case RoutedEventId::Collapsed:
			return Collapsed.InvokeHandlers(sender, args);
		case RoutedEventId::SubmenuOpened:
			return SubmenuOpened.InvokeHandlers(sender, args);
		case RoutedEventId::SubmenuClosed:
			return SubmenuClosed.InvokeHandlers(sender, args);
		default:
			return {};
		}
	}

public:
	/**
	 * WPF-style untyped routed-event hook. The returned token is the sole
	 * subscription lifetime; handledEventsToo matches AddHandler semantics.
	 */
	[[nodiscard]] EventConnection AddHandler(
		RoutedEventId eventId,
		std::function<void(Control*, RoutedEventArgs&)> handler,
		bool handledEventsToo = false)
	{
		const auto index = static_cast<std::size_t>(eventId);
		if (!handler || eventId == RoutedEventId::None
			|| eventId == RoutedEventId::Count
			|| index >= _genericRoutedEventHandlers.size()) return {};
		auto& event = _genericRoutedEventHandlers[index];
		if (!event)
			event = std::make_unique<RoutedEvent<RoutedEventArgs>>(
				this, eventId);
		return event->Subscribe(std::move(handler), handledEventsToo);
	}

	/** Adds one binding and returns the sole lifetime token for that entry. */
	[[nodiscard]] EventConnection AddCommandBinding(CommandBinding binding);
	bool AddInputBinding(KeyBinding binding);
	bool AddInputBinding(MouseBinding binding);
	bool SetInputBindings(std::vector<InputBinding> bindings);
	void ClearInputBindings() noexcept { _inputBindings.clear(); }
	std::span<const InputBinding> GetInputBindings() const noexcept
	{
		return _inputBindings;
	}

	virtual cui::core::Size MeasureCore(
		const cui::core::Constraints& available) = 0;
	virtual cui::core::Size Measure(
		const cui::core::Constraints& available) = 0;
	virtual void Arrange(cui::core::Rect finalRect) = 0;
	virtual cui::core::Size GetActualSizeDip() const = 0;
	virtual bool ContainsPoint(int localX, int localY) = 0;

protected:
	virtual bool ProcessInput(const InputReport& input) = 0;
};
