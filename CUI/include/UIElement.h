#ifndef CUI_UI_ELEMENT_H_INCLUDED
#define CUI_UI_ELEMENT_H_INCLUDED
#pragma once

#include "Event.h"
#include "InputReport.h"
#include "FocusManager.h"
#include "Layout/LayoutState.h"
#include "RoutedCommand.h"
#include "Visual.h"

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

using PreviewMouseWheelEvent = RoutedEvent<
	MouseEventArgs, RoutedEventId::PreviewMouseWheel>;
using MouseWheelEvent = RoutedEvent<MouseEventArgs, RoutedEventId::MouseWheel>;
using PreviewMouseMoveEvent = RoutedEvent<
	MouseEventArgs, RoutedEventId::PreviewMouseMove>;
using MouseMoveEvent = RoutedEvent<MouseEventArgs, RoutedEventId::MouseMove>;
using PreviewMouseUpEvent = RoutedEvent<
	MouseEventArgs, RoutedEventId::PreviewMouseUp>;
using MouseUpEvent = RoutedEvent<MouseEventArgs, RoutedEventId::MouseUp>;
using PreviewMouseDownEvent = RoutedEvent<
	MouseEventArgs, RoutedEventId::PreviewMouseDown>;
using MouseDownEvent = RoutedEvent<MouseEventArgs, RoutedEventId::MouseDown>;
using PreviewMouseDoubleClickEvent = RoutedEvent<
	MouseEventArgs, RoutedEventId::PreviewMouseDoubleClick>;
using MouseDoubleClickEvent = RoutedEvent<
	MouseEventArgs, RoutedEventId::MouseDoubleClick>;
using ClickEvent = RoutedEvent<RoutedEventArgs, RoutedEventId::Click>;
using TextChangedEvent = RoutedEvent<
	TextChangedEventArgs, RoutedEventId::TextChanged>;
using PasswordChangedEvent = RoutedEvent<
	RoutedEventArgs, RoutedEventId::PasswordChanged>;
using ScrollChangedEvent = RoutedEvent<
	ScrollChangedEventArgs, RoutedEventId::ScrollChanged>;
using SelectionChangedRoutedEvent = RoutedEvent<
	SelectionChangedEventArgs, RoutedEventId::SelectionChanged>;
using SelectedDatesChangedRoutedEvent = RoutedEvent<
	SelectionChangedEventArgs, RoutedEventId::SelectedDatesChanged>;
using SelectedItemChangedRoutedEvent = RoutedEvent<
	RoutedPropertyChangedEventArgs<BindingValue>,
	RoutedEventId::SelectedItemChanged>;
using ValueChangedRoutedEvent = RoutedEvent<
	RoutedPropertyChangedEventArgs<double>, RoutedEventId::ValueChanged>;
using SelectedEvent = RoutedEvent<RoutedEventArgs, RoutedEventId::Selected>;
using UnselectedEvent = RoutedEvent<RoutedEventArgs, RoutedEventId::Unselected>;
using CheckedEvent = RoutedEvent<RoutedEventArgs, RoutedEventId::Checked>;
using UncheckedEvent = RoutedEvent<RoutedEventArgs, RoutedEventId::Unchecked>;
using ExpandedEvent = RoutedEvent<RoutedEventArgs, RoutedEventId::Expanded>;
using CollapsedEvent = RoutedEvent<RoutedEventArgs, RoutedEventId::Collapsed>;
using SubmenuOpenedEvent = RoutedEvent<
	RoutedEventArgs, RoutedEventId::SubmenuOpened>;
using SubmenuClosedEvent = RoutedEvent<
	RoutedEventArgs, RoutedEventId::SubmenuClosed>;
using IndeterminateEvent = RoutedEvent<
	RoutedEventArgs, RoutedEventId::Indeterminate>;
using MouseEnterEvent = RoutedEvent<MouseEventArgs, RoutedEventId::MouseEnter>;
using MouseLeaveEvent = RoutedEvent<MouseEventArgs, RoutedEventId::MouseLeave>;
using GotMouseCaptureEvent = RoutedEvent<
	RoutedEventArgs, RoutedEventId::GotMouseCapture>;
using LostMouseCaptureEvent = RoutedEvent<
	RoutedEventArgs, RoutedEventId::LostMouseCapture>;
using PreviewKeyUpEvent = RoutedEvent<KeyEventArgs, RoutedEventId::PreviewKeyUp>;
using KeyUpEvent = RoutedEvent<KeyEventArgs, RoutedEventId::KeyUp>;
using PreviewKeyDownEvent = RoutedEvent<
	KeyEventArgs, RoutedEventId::PreviewKeyDown>;
using KeyDownEvent = RoutedEvent<KeyEventArgs, RoutedEventId::KeyDown>;
using PreviewTextInputStartEvent = RoutedEvent<
	TextCompositionEventArgs, RoutedEventId::PreviewTextInputStart>;
using TextInputStartEvent = RoutedEvent<
	TextCompositionEventArgs, RoutedEventId::TextInputStart>;
using PreviewTextInputUpdateEvent = RoutedEvent<
	TextCompositionEventArgs, RoutedEventId::PreviewTextInputUpdate>;
using TextInputUpdateEvent = RoutedEvent<
	TextCompositionEventArgs, RoutedEventId::TextInputUpdate>;
using PreviewTextInputEvent = RoutedEvent<
	TextCompositionEventArgs, RoutedEventId::PreviewTextInput>;
using TextInputEvent = RoutedEvent<
	TextCompositionEventArgs, RoutedEventId::TextInput>;
using PreviewGotKeyboardFocusEvent = RoutedEvent<
	KeyboardFocusChangedEventArgs,
	RoutedEventId::PreviewGotKeyboardFocus>;
using GotKeyboardFocusEvent = RoutedEvent<
	KeyboardFocusChangedEventArgs, RoutedEventId::GotKeyboardFocus>;
using PreviewLostKeyboardFocusEvent = RoutedEvent<
	KeyboardFocusChangedEventArgs,
	RoutedEventId::PreviewLostKeyboardFocus>;
using LostKeyboardFocusEvent = RoutedEvent<
	KeyboardFocusChangedEventArgs, RoutedEventId::LostKeyboardFocus>;
using GotFocusEvent = RoutedEvent<RoutedEventArgs, RoutedEventId::GotFocus>;
using LostFocusEvent = RoutedEvent<RoutedEventArgs, RoutedEventId::LostFocus>;
using PreviewCanExecuteEvent = RoutedEvent<
	CanExecuteRoutedEventArgs, RoutedEventId::PreviewCanExecute>;
using CanExecuteEvent = RoutedEvent<
	CanExecuteRoutedEventArgs, RoutedEventId::CanExecute>;
using PreviewExecutedEvent = RoutedEvent<
	ExecutedRoutedEventArgs, RoutedEventId::PreviewExecuted>;
using ExecutedEvent = RoutedEvent<
	ExecutedRoutedEventArgs, RoutedEventId::Executed>;
using PreviewDragEnterEvent = RoutedEvent<
	DragEventArgs, RoutedEventId::PreviewDragEnter>;
using DragEnterEvent = RoutedEvent<DragEventArgs, RoutedEventId::DragEnter>;
using PreviewDragOverEvent = RoutedEvent<
	DragEventArgs, RoutedEventId::PreviewDragOver>;
using DragOverEvent = RoutedEvent<DragEventArgs, RoutedEventId::DragOver>;
using PreviewDragLeaveEvent = RoutedEvent<
	DragEventArgs, RoutedEventId::PreviewDragLeave>;
using DragLeaveEvent = RoutedEvent<DragEventArgs, RoutedEventId::DragLeave>;
using PreviewDropEvent = RoutedEvent<DragEventArgs, RoutedEventId::PreviewDrop>;
using DropEvent = RoutedEvent<DragEventArgs, RoutedEventId::Drop>;
using SizeChangedEvent = RoutedEvent<
	SizeChangedEventArgs, RoutedEventId::SizeChanged>;
using IsVisibleChangedEvent = DependencyPropertyChangedEvent;

/** Owns layout-pass state, input publication, focus participation and hit testing. */
class UIElement : public Visual
{
protected:
	friend class RoutedCommandManager;
	friend class RoutedEventHandlerStore;
	friend struct cui::framework::RoutedEventAccess;
	cui::layout::LayoutState _layoutState;
	Visibility _visibility = Visibility::Visible;
	bool _isFocused = false;
	bool _isKeyboardFocused = false;
	bool _isKeyboardFocusVisible = false;
	bool _isKeyboardFocusWithin = false;
	bool _isMouseOver = false;
	bool _isMouseDirectlyOver = false;
	bool _isMouseCaptured = false;
	bool _isMouseCaptureWithin = false;
	/** Framework presentation projection; never replaces authored Visibility. */
	bool _presentationSuppressed = false;
	bool _defaultLeftButtonPressActive = false;
	std::shared_ptr<CommandBindingCollectionState> _commandBindings;
	std::shared_ptr<CommandCanExecuteObserverState> _commandCanExecuteObservers;
	std::vector<InputBinding> _inputBindings;
	std::unique_ptr<RoutedEventHandlerStore> _routedEventHandlers;
	void InvalidateCommandInfrastructureForDestruction() noexcept;

public:
	UIElement()
		: OnPreviewMouseWheel(this),
		OnMouseWheel(this),
		OnPreviewMouseMove(this),
		OnMouseMove(this),
		OnPreviewMouseUp(this),
		OnMouseUp(this),
		OnPreviewMouseDown(this),
		OnMouseDown(this),
		OnPreviewMouseDoubleClick(this),
		OnMouseDoubleClick(this),
		Click(this),
		SizeChanged(this),
		OnMouseEnter(this),
		OnMouseLeave(this),
		OnGotMouseCapture(this),
		OnLostMouseCapture(this),
		OnPreviewKeyUp(this),
		OnKeyUp(this),
		OnPreviewKeyDown(this),
		OnKeyDown(this),
		OnPreviewTextInputStart(this),
		OnTextInputStart(this),
		OnPreviewTextInputUpdate(this),
		OnTextInputUpdate(this),
		OnPreviewTextInput(this),
		OnTextInput(this),
		OnPreviewGotKeyboardFocus(this),
		OnGotKeyboardFocus(this),
		OnPreviewLostKeyboardFocus(this),
		OnLostKeyboardFocus(this),
		OnGotFocus(this),
		OnLostFocus(this),
		OnPreviewCanExecute(this),
		OnCanExecute(this),
		OnPreviewExecuted(this),
		OnExecuted(this),
		OnPreviewDragEnter(this),
		OnDragEnter(this),
		OnPreviewDragOver(this),
		OnDragOver(this),
		OnPreviewDragLeave(this),
		OnDragLeave(this),
		OnPreviewDrop(this),
		OnDrop(this),
		OnTextChanged(this),
		PasswordChanged(this),
		OnScrollChanged(this),
		SelectionChanged(this),
		SelectedDatesChanged(this),
		SelectedItemChanged(this),
		ValueChanged(this),
		Selected(this),
		Unselected(this),
		Checked(this),
		Unchecked(this),
		Expanded(this),
		Collapsed(this),
		SubmenuOpened(this),
		SubmenuClosed(this),
		Indeterminate(this) {}
	~UIElement() override;

	PreviewMouseWheelEvent OnPreviewMouseWheel;
	MouseWheelEvent OnMouseWheel;
	PreviewMouseMoveEvent OnPreviewMouseMove;
	MouseMoveEvent OnMouseMove;
	PreviewMouseUpEvent OnPreviewMouseUp;
	MouseUpEvent OnMouseUp;
	PreviewMouseDownEvent OnPreviewMouseDown;
	MouseDownEvent OnMouseDown;
	PreviewMouseDoubleClickEvent OnPreviewMouseDoubleClick;
	MouseDoubleClickEvent OnMouseDoubleClick;
	MouseEnterEvent OnMouseEnter;
	MouseLeaveEvent OnMouseLeave;
	GotMouseCaptureEvent OnGotMouseCapture;
	LostMouseCaptureEvent OnLostMouseCapture;
	PreviewKeyUpEvent OnPreviewKeyUp;
	KeyUpEvent OnKeyUp;
	PreviewKeyDownEvent OnPreviewKeyDown;
	KeyDownEvent OnKeyDown;
	PreviewTextInputStartEvent OnPreviewTextInputStart;
	TextInputStartEvent OnTextInputStart;
	PreviewTextInputUpdateEvent OnPreviewTextInputUpdate;
	TextInputUpdateEvent OnTextInputUpdate;
	PreviewTextInputEvent OnPreviewTextInput;
	TextInputEvent OnTextInput;
	PreviewGotKeyboardFocusEvent OnPreviewGotKeyboardFocus;
	GotKeyboardFocusEvent OnGotKeyboardFocus;
	PreviewLostKeyboardFocusEvent OnPreviewLostKeyboardFocus;
	LostKeyboardFocusEvent OnLostKeyboardFocus;
	SizeChangedEvent SizeChanged;
	GotFocusEvent OnGotFocus;
	LostFocusEvent OnLostFocus;
	PreviewCanExecuteEvent OnPreviewCanExecute;
	CanExecuteEvent OnCanExecute;
	PreviewExecutedEvent OnPreviewExecuted;
	ExecutedEvent OnExecuted;
	PreviewDragEnterEvent OnPreviewDragEnter;
	DragEnterEvent OnDragEnter;
	PreviewDragOverEvent OnPreviewDragOver;
	DragOverEvent OnDragOver;
	PreviewDragLeaveEvent OnPreviewDragLeave;
	DragLeaveEvent OnDragLeave;
	PreviewDropEvent OnPreviewDrop;
	DropEvent OnDrop;

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
	SelectedDatesChangedRoutedEvent SelectedDatesChanged;
	SelectedItemChangedRoutedEvent SelectedItemChanged;
	ValueChangedRoutedEvent ValueChanged;
	/** Exposed only by selector item containers such as TabItem. */
	SelectedEvent Selected;
	UnselectedEvent Unselected;
	/** Exposed only by controls that own the corresponding WPF state. */
	CheckedEvent Checked;
	UncheckedEvent Unchecked;
	ExpandedEvent Expanded;
	CollapsedEvent Collapsed;
	SubmenuOpenedEvent SubmenuOpened;
	SubmenuClosedEvent SubmenuClosed;
	IndeterminateEvent Indeterminate;

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
		case RoutedEventId::Indeterminate:
			return Indeterminate.InvokeHandlers(sender, args);
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
		if (!handler || eventId == RoutedEventId::None
			|| eventId == RoutedEventId::Count) return {};
		return RoutedEventHandlerStore::Subscribe(
			*this,
			eventId,
			RoutedHandlerStorageKind::Generic,
			std::move(handler),
			handledEventsToo);
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

#endif // CUI_UI_ELEMENT_H_INCLUDED
