#include "InputManager.h"

#include "Control.h"
#include "RoutedEventInfrastructure.h"
#include "UIElement.h"
#include "Window.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <mutex>
#include <unordered_set>

namespace
{
	constexpr std::size_t EventIndex(RoutedEventId eventId) noexcept
	{
		return static_cast<std::size_t>(eventId);
	}

	constexpr std::array<RoutedEventMetadata,
		static_cast<std::size_t>(RoutedEventId::Count)> RoutedEvents{
		RoutedEventMetadata{},
		{ RoutedEventId::PreviewMouseWheel, L"PreviewMouseWheel",
			RoutedEventRoutingStrategy::Tunnel, RoutedEventId::MouseWheel,
			RoutedInputDeviceKind::Mouse, RoutedEventStage::Preview },
		{ RoutedEventId::MouseWheel, L"MouseWheel",
			RoutedEventRoutingStrategy::Bubble, RoutedEventId::PreviewMouseWheel,
			RoutedInputDeviceKind::Mouse, RoutedEventStage::Bubble },
		{ RoutedEventId::PreviewMouseMove, L"PreviewMouseMove",
			RoutedEventRoutingStrategy::Tunnel, RoutedEventId::MouseMove,
			RoutedInputDeviceKind::Mouse, RoutedEventStage::Preview },
		{ RoutedEventId::MouseMove, L"MouseMove",
			RoutedEventRoutingStrategy::Bubble, RoutedEventId::PreviewMouseMove,
			RoutedInputDeviceKind::Mouse, RoutedEventStage::Bubble },
		{ RoutedEventId::PreviewMouseDown, L"PreviewMouseDown",
			RoutedEventRoutingStrategy::Tunnel, RoutedEventId::MouseDown,
			RoutedInputDeviceKind::Mouse, RoutedEventStage::Preview },
		{ RoutedEventId::MouseDown, L"MouseDown",
			RoutedEventRoutingStrategy::Bubble, RoutedEventId::PreviewMouseDown,
			RoutedInputDeviceKind::Mouse, RoutedEventStage::Bubble },
		{ RoutedEventId::PreviewMouseUp, L"PreviewMouseUp",
			RoutedEventRoutingStrategy::Tunnel, RoutedEventId::MouseUp,
			RoutedInputDeviceKind::Mouse, RoutedEventStage::Preview },
		{ RoutedEventId::MouseUp, L"MouseUp",
			RoutedEventRoutingStrategy::Bubble, RoutedEventId::PreviewMouseUp,
			RoutedInputDeviceKind::Mouse, RoutedEventStage::Bubble },
		{ RoutedEventId::PreviewMouseDoubleClick, L"PreviewMouseDoubleClick",
			RoutedEventRoutingStrategy::Tunnel, RoutedEventId::MouseDoubleClick,
			RoutedInputDeviceKind::Mouse, RoutedEventStage::Preview },
		{ RoutedEventId::MouseDoubleClick, L"MouseDoubleClick",
			RoutedEventRoutingStrategy::Bubble,
			RoutedEventId::PreviewMouseDoubleClick,
			RoutedInputDeviceKind::Mouse, RoutedEventStage::Bubble },
		{ RoutedEventId::Click, L"Click",
			RoutedEventRoutingStrategy::Bubble, RoutedEventId::None,
			RoutedInputDeviceKind::None, RoutedEventStage::Bubble },
		{ RoutedEventId::SizeChanged, L"SizeChanged",
			RoutedEventRoutingStrategy::Direct, RoutedEventId::None,
			RoutedInputDeviceKind::None, RoutedEventStage::Direct },
		{ RoutedEventId::TextChanged, L"TextChanged",
			RoutedEventRoutingStrategy::Bubble, RoutedEventId::None,
			RoutedInputDeviceKind::None, RoutedEventStage::Bubble },
		{ RoutedEventId::PasswordChanged, L"PasswordChanged",
			RoutedEventRoutingStrategy::Bubble, RoutedEventId::None,
			RoutedInputDeviceKind::None, RoutedEventStage::Bubble },
		{ RoutedEventId::ScrollChanged, L"ScrollChanged",
			RoutedEventRoutingStrategy::Bubble, RoutedEventId::None,
			RoutedInputDeviceKind::None, RoutedEventStage::Bubble },
		{ RoutedEventId::SelectionChanged, L"SelectionChanged",
			RoutedEventRoutingStrategy::Bubble, RoutedEventId::None,
			RoutedInputDeviceKind::None, RoutedEventStage::Bubble },
		{ RoutedEventId::SelectedDatesChanged, L"SelectedDatesChanged",
			RoutedEventRoutingStrategy::Bubble, RoutedEventId::None,
			RoutedInputDeviceKind::None, RoutedEventStage::Bubble },
		{ RoutedEventId::SelectedItemChanged, L"SelectedItemChanged",
			RoutedEventRoutingStrategy::Bubble, RoutedEventId::None,
			RoutedInputDeviceKind::None, RoutedEventStage::Bubble },
		{ RoutedEventId::ValueChanged, L"ValueChanged",
			RoutedEventRoutingStrategy::Bubble, RoutedEventId::None,
			RoutedInputDeviceKind::None, RoutedEventStage::Bubble },
		{ RoutedEventId::Selected, L"Selected",
			RoutedEventRoutingStrategy::Bubble, RoutedEventId::None,
			RoutedInputDeviceKind::None, RoutedEventStage::Bubble },
		{ RoutedEventId::Unselected, L"Unselected",
			RoutedEventRoutingStrategy::Bubble, RoutedEventId::None,
			RoutedInputDeviceKind::None, RoutedEventStage::Bubble },
		{ RoutedEventId::Checked, L"Checked",
			RoutedEventRoutingStrategy::Bubble, RoutedEventId::None,
			RoutedInputDeviceKind::None, RoutedEventStage::Bubble },
		{ RoutedEventId::Unchecked, L"Unchecked",
			RoutedEventRoutingStrategy::Bubble, RoutedEventId::None,
			RoutedInputDeviceKind::None, RoutedEventStage::Bubble },
		{ RoutedEventId::Expanded, L"Expanded",
			RoutedEventRoutingStrategy::Bubble, RoutedEventId::None,
			RoutedInputDeviceKind::None, RoutedEventStage::Bubble },
		{ RoutedEventId::Collapsed, L"Collapsed",
			RoutedEventRoutingStrategy::Bubble, RoutedEventId::None,
			RoutedInputDeviceKind::None, RoutedEventStage::Bubble },
		{ RoutedEventId::SubmenuOpened, L"SubmenuOpened",
			RoutedEventRoutingStrategy::Bubble, RoutedEventId::None,
			RoutedInputDeviceKind::None, RoutedEventStage::Bubble },
		{ RoutedEventId::SubmenuClosed, L"SubmenuClosed",
			RoutedEventRoutingStrategy::Bubble, RoutedEventId::None,
			RoutedInputDeviceKind::None, RoutedEventStage::Bubble },
		{ RoutedEventId::MouseEnter, L"MouseEnter",
			RoutedEventRoutingStrategy::Direct, RoutedEventId::None,
			RoutedInputDeviceKind::Mouse, RoutedEventStage::Direct },
		{ RoutedEventId::MouseLeave, L"MouseLeave",
			RoutedEventRoutingStrategy::Direct, RoutedEventId::None,
			RoutedInputDeviceKind::Mouse, RoutedEventStage::Direct },
		{ RoutedEventId::GotMouseCapture, L"GotMouseCapture",
			RoutedEventRoutingStrategy::Bubble, RoutedEventId::None,
			RoutedInputDeviceKind::MouseCapture, RoutedEventStage::Bubble },
		{ RoutedEventId::LostMouseCapture, L"LostMouseCapture",
			RoutedEventRoutingStrategy::Bubble, RoutedEventId::None,
			RoutedInputDeviceKind::MouseCapture, RoutedEventStage::Bubble },
		{ RoutedEventId::PreviewKeyDown, L"PreviewKeyDown",
			RoutedEventRoutingStrategy::Tunnel, RoutedEventId::KeyDown,
			RoutedInputDeviceKind::Keyboard, RoutedEventStage::Preview },
		{ RoutedEventId::KeyDown, L"KeyDown",
			RoutedEventRoutingStrategy::Bubble, RoutedEventId::PreviewKeyDown,
			RoutedInputDeviceKind::Keyboard, RoutedEventStage::Bubble },
		{ RoutedEventId::PreviewKeyUp, L"PreviewKeyUp",
			RoutedEventRoutingStrategy::Tunnel, RoutedEventId::KeyUp,
			RoutedInputDeviceKind::Keyboard, RoutedEventStage::Preview },
		{ RoutedEventId::KeyUp, L"KeyUp",
			RoutedEventRoutingStrategy::Bubble, RoutedEventId::PreviewKeyUp,
			RoutedInputDeviceKind::Keyboard, RoutedEventStage::Bubble },
		{ RoutedEventId::PreviewTextInputStart, L"PreviewTextInputStart",
			RoutedEventRoutingStrategy::Tunnel, RoutedEventId::TextInputStart,
			RoutedInputDeviceKind::Text, RoutedEventStage::Preview },
		{ RoutedEventId::TextInputStart, L"TextInputStart",
			RoutedEventRoutingStrategy::Bubble,
			RoutedEventId::PreviewTextInputStart,
			RoutedInputDeviceKind::Text, RoutedEventStage::Bubble },
		{ RoutedEventId::PreviewTextInputUpdate, L"PreviewTextInputUpdate",
			RoutedEventRoutingStrategy::Tunnel, RoutedEventId::TextInputUpdate,
			RoutedInputDeviceKind::Text, RoutedEventStage::Preview },
		{ RoutedEventId::TextInputUpdate, L"TextInputUpdate",
			RoutedEventRoutingStrategy::Bubble,
			RoutedEventId::PreviewTextInputUpdate,
			RoutedInputDeviceKind::Text, RoutedEventStage::Bubble },
		{ RoutedEventId::PreviewTextInput, L"PreviewTextInput",
			RoutedEventRoutingStrategy::Tunnel, RoutedEventId::TextInput,
			RoutedInputDeviceKind::Text, RoutedEventStage::Preview },
		{ RoutedEventId::TextInput, L"TextInput",
			RoutedEventRoutingStrategy::Bubble, RoutedEventId::PreviewTextInput,
			RoutedInputDeviceKind::Text, RoutedEventStage::Bubble },
		{ RoutedEventId::PreviewGotKeyboardFocus, L"PreviewGotKeyboardFocus",
			RoutedEventRoutingStrategy::Tunnel,
			RoutedEventId::GotKeyboardFocus,
			RoutedInputDeviceKind::KeyboardFocus, RoutedEventStage::Preview },
		{ RoutedEventId::GotKeyboardFocus, L"GotKeyboardFocus",
			RoutedEventRoutingStrategy::Bubble,
			RoutedEventId::PreviewGotKeyboardFocus,
			RoutedInputDeviceKind::KeyboardFocus, RoutedEventStage::Bubble },
		{ RoutedEventId::PreviewLostKeyboardFocus, L"PreviewLostKeyboardFocus",
			RoutedEventRoutingStrategy::Tunnel,
			RoutedEventId::LostKeyboardFocus,
			RoutedInputDeviceKind::KeyboardFocus, RoutedEventStage::Preview },
		{ RoutedEventId::LostKeyboardFocus, L"LostKeyboardFocus",
			RoutedEventRoutingStrategy::Bubble,
			RoutedEventId::PreviewLostKeyboardFocus,
			RoutedInputDeviceKind::KeyboardFocus, RoutedEventStage::Bubble },
		{ RoutedEventId::GotFocus, L"GotFocus",
			RoutedEventRoutingStrategy::Bubble, RoutedEventId::None,
			RoutedInputDeviceKind::Focus, RoutedEventStage::Bubble },
		{ RoutedEventId::LostFocus, L"LostFocus",
			RoutedEventRoutingStrategy::Bubble, RoutedEventId::None,
			RoutedInputDeviceKind::Focus, RoutedEventStage::Bubble },
		{ RoutedEventId::PreviewCanExecute, L"PreviewCanExecute",
			RoutedEventRoutingStrategy::Tunnel, RoutedEventId::CanExecute,
			RoutedInputDeviceKind::Command, RoutedEventStage::Preview },
		{ RoutedEventId::CanExecute, L"CanExecute",
			RoutedEventRoutingStrategy::Bubble, RoutedEventId::PreviewCanExecute,
			RoutedInputDeviceKind::Command, RoutedEventStage::Bubble },
		{ RoutedEventId::PreviewExecuted, L"PreviewExecuted",
			RoutedEventRoutingStrategy::Tunnel, RoutedEventId::Executed,
			RoutedInputDeviceKind::Command, RoutedEventStage::Preview },
		{ RoutedEventId::Executed, L"Executed",
			RoutedEventRoutingStrategy::Bubble, RoutedEventId::PreviewExecuted,
			RoutedInputDeviceKind::Command, RoutedEventStage::Bubble },
		{ RoutedEventId::PreviewDragEnter, L"PreviewDragEnter",
			RoutedEventRoutingStrategy::Tunnel, RoutedEventId::DragEnter,
			RoutedInputDeviceKind::DragDrop, RoutedEventStage::Preview },
		{ RoutedEventId::DragEnter, L"DragEnter",
			RoutedEventRoutingStrategy::Bubble, RoutedEventId::PreviewDragEnter,
			RoutedInputDeviceKind::DragDrop, RoutedEventStage::Bubble },
		{ RoutedEventId::PreviewDragOver, L"PreviewDragOver",
			RoutedEventRoutingStrategy::Tunnel, RoutedEventId::DragOver,
			RoutedInputDeviceKind::DragDrop, RoutedEventStage::Preview },
		{ RoutedEventId::DragOver, L"DragOver",
			RoutedEventRoutingStrategy::Bubble, RoutedEventId::PreviewDragOver,
			RoutedInputDeviceKind::DragDrop, RoutedEventStage::Bubble },
		{ RoutedEventId::PreviewDragLeave, L"PreviewDragLeave",
			RoutedEventRoutingStrategy::Tunnel, RoutedEventId::DragLeave,
			RoutedInputDeviceKind::DragDrop, RoutedEventStage::Preview },
		{ RoutedEventId::DragLeave, L"DragLeave",
			RoutedEventRoutingStrategy::Bubble, RoutedEventId::PreviewDragLeave,
			RoutedInputDeviceKind::DragDrop, RoutedEventStage::Bubble },
		{ RoutedEventId::PreviewDrop, L"PreviewDrop",
			RoutedEventRoutingStrategy::Tunnel, RoutedEventId::Drop,
			RoutedInputDeviceKind::DragDrop, RoutedEventStage::Preview },
		{ RoutedEventId::Drop, L"Drop",
			RoutedEventRoutingStrategy::Bubble, RoutedEventId::PreviewDrop,
			RoutedInputDeviceKind::DragDrop, RoutedEventStage::Bubble },
	};

	std::atomic<std::uint64_t> StandaloneSequence{ 1 };

	struct ClassHandlerEntry final
	{
		std::uint64_t Token = 0;
		UIClass OwnerClass = UIClass::UI_Base;
		RoutedEventId EventId = RoutedEventId::None;
		bool HandledEventsToo = false;
		RoutedEventManager::ClassHandler Handler;
	};

	struct ClassHandlerState final
	{
		std::mutex Mutex;
		std::uint64_t NextToken = 1;
		std::vector<ClassHandlerEntry> Entries;
	};

	ClassHandlerState& ClassHandlers()
	{
		static ClassHandlerState state;
		return state;
	}

	RoutedHandlerInvocationCount InvokeInstanceHandlers(
		Control& target,
		RoutedEventArgs& args)
	{
		switch (args.EventId)
		{
		case RoutedEventId::PreviewMouseWheel:
			return cui::framework::RoutedEventAccess::InvokeHandlers(target.OnPreviewMouseWheel,
				&target, static_cast<MouseEventArgs&>(args));
		case RoutedEventId::MouseWheel:
			return cui::framework::RoutedEventAccess::InvokeHandlers(target.OnMouseWheel,
				&target, static_cast<MouseEventArgs&>(args));
		case RoutedEventId::PreviewMouseMove:
			return cui::framework::RoutedEventAccess::InvokeHandlers(target.OnPreviewMouseMove,
				&target, static_cast<MouseEventArgs&>(args));
		case RoutedEventId::MouseMove:
			return cui::framework::RoutedEventAccess::InvokeHandlers(target.OnMouseMove,
				&target, static_cast<MouseEventArgs&>(args));
		case RoutedEventId::PreviewMouseDown:
			return cui::framework::RoutedEventAccess::InvokeHandlers(target.OnPreviewMouseDown,
				&target, static_cast<MouseEventArgs&>(args));
		case RoutedEventId::MouseDown:
			return cui::framework::RoutedEventAccess::InvokeHandlers(target.OnMouseDown,
				&target, static_cast<MouseEventArgs&>(args));
		case RoutedEventId::PreviewMouseUp:
			return cui::framework::RoutedEventAccess::InvokeHandlers(target.OnPreviewMouseUp,
				&target, static_cast<MouseEventArgs&>(args));
		case RoutedEventId::MouseUp:
			return cui::framework::RoutedEventAccess::InvokeHandlers(target.OnMouseUp,
				&target, static_cast<MouseEventArgs&>(args));
		case RoutedEventId::PreviewMouseDoubleClick:
			return cui::framework::RoutedEventAccess::InvokeHandlers(target.OnPreviewMouseDoubleClick,
				&target, static_cast<MouseEventArgs&>(args));
		case RoutedEventId::MouseDoubleClick:
			return cui::framework::RoutedEventAccess::InvokeHandlers(target.OnMouseDoubleClick,
				&target, static_cast<MouseEventArgs&>(args));
		case RoutedEventId::SizeChanged:
			return cui::framework::RoutedEventAccess::InvokeHandlers(
				target.SizeChanged, &target,
				static_cast<SizeChangedEventArgs&>(args));
		case RoutedEventId::Click:
		case RoutedEventId::TextChanged:
		case RoutedEventId::PasswordChanged:
		case RoutedEventId::ScrollChanged:
		case RoutedEventId::SelectionChanged:
		case RoutedEventId::SelectedDatesChanged:
		case RoutedEventId::SelectedItemChanged:
		case RoutedEventId::ValueChanged:
		case RoutedEventId::Selected:
		case RoutedEventId::Unselected:
		case RoutedEventId::Checked:
		case RoutedEventId::Unchecked:
		case RoutedEventId::Expanded:
		case RoutedEventId::Collapsed:
		case RoutedEventId::SubmenuOpened:
		case RoutedEventId::SubmenuClosed:
			return cui::framework::RoutedEventAccess::InvokeSemanticHandlers(
				target, &target, args);
		case RoutedEventId::MouseEnter:
			return cui::framework::RoutedEventAccess::InvokeHandlers(target.OnMouseEnter,
				&target, static_cast<MouseEventArgs&>(args));
		case RoutedEventId::MouseLeave:
			return cui::framework::RoutedEventAccess::InvokeHandlers(target.OnMouseLeave,
				&target, static_cast<MouseEventArgs&>(args));
		case RoutedEventId::GotMouseCapture:
			return cui::framework::RoutedEventAccess::InvokeHandlers(target.OnGotMouseCapture,&target, args);
		case RoutedEventId::LostMouseCapture:
			return cui::framework::RoutedEventAccess::InvokeHandlers(target.OnLostMouseCapture,&target, args);
		case RoutedEventId::PreviewKeyDown:
			return cui::framework::RoutedEventAccess::InvokeHandlers(target.OnPreviewKeyDown,
				&target, static_cast<KeyEventArgs&>(args));
		case RoutedEventId::KeyDown:
			return cui::framework::RoutedEventAccess::InvokeHandlers(target.OnKeyDown,
				&target, static_cast<KeyEventArgs&>(args));
		case RoutedEventId::PreviewKeyUp:
			return cui::framework::RoutedEventAccess::InvokeHandlers(target.OnPreviewKeyUp,
				&target, static_cast<KeyEventArgs&>(args));
		case RoutedEventId::KeyUp:
			return cui::framework::RoutedEventAccess::InvokeHandlers(target.OnKeyUp,
				&target, static_cast<KeyEventArgs&>(args));
		case RoutedEventId::PreviewTextInputStart:
			return cui::framework::RoutedEventAccess::InvokeHandlers(target.OnPreviewTextInputStart,
				&target, static_cast<TextCompositionEventArgs&>(args));
		case RoutedEventId::TextInputStart:
			return cui::framework::RoutedEventAccess::InvokeHandlers(target.OnTextInputStart,
				&target, static_cast<TextCompositionEventArgs&>(args));
		case RoutedEventId::PreviewTextInputUpdate:
			return cui::framework::RoutedEventAccess::InvokeHandlers(target.OnPreviewTextInputUpdate,
				&target, static_cast<TextCompositionEventArgs&>(args));
		case RoutedEventId::TextInputUpdate:
			return cui::framework::RoutedEventAccess::InvokeHandlers(target.OnTextInputUpdate,
				&target, static_cast<TextCompositionEventArgs&>(args));
		case RoutedEventId::PreviewTextInput:
			return cui::framework::RoutedEventAccess::InvokeHandlers(target.OnPreviewTextInput,
				&target, static_cast<TextCompositionEventArgs&>(args));
		case RoutedEventId::TextInput:
			return cui::framework::RoutedEventAccess::InvokeHandlers(target.OnTextInput,
				&target, static_cast<TextCompositionEventArgs&>(args));
		case RoutedEventId::PreviewGotKeyboardFocus:
			return cui::framework::RoutedEventAccess::InvokeHandlers(target.OnPreviewGotKeyboardFocus,
				&target, static_cast<KeyboardFocusChangedEventArgs&>(args));
		case RoutedEventId::GotKeyboardFocus:
			return cui::framework::RoutedEventAccess::InvokeHandlers(target.OnGotKeyboardFocus,
				&target, static_cast<KeyboardFocusChangedEventArgs&>(args));
		case RoutedEventId::PreviewLostKeyboardFocus:
			return cui::framework::RoutedEventAccess::InvokeHandlers(target.OnPreviewLostKeyboardFocus,
				&target, static_cast<KeyboardFocusChangedEventArgs&>(args));
		case RoutedEventId::LostKeyboardFocus:
			return cui::framework::RoutedEventAccess::InvokeHandlers(target.OnLostKeyboardFocus,
				&target, static_cast<KeyboardFocusChangedEventArgs&>(args));
		case RoutedEventId::GotFocus:
			return cui::framework::RoutedEventAccess::InvokeHandlers(target.OnGotFocus,&target, args);
		case RoutedEventId::LostFocus:
			return cui::framework::RoutedEventAccess::InvokeHandlers(target.OnLostFocus,&target, args);
		case RoutedEventId::PreviewCanExecute:
			return cui::framework::RoutedEventAccess::InvokeHandlers(target.OnPreviewCanExecute,
				&target, static_cast<CanExecuteRoutedEventArgs&>(args));
		case RoutedEventId::CanExecute:
			return cui::framework::RoutedEventAccess::InvokeHandlers(target.OnCanExecute,
				&target, static_cast<CanExecuteRoutedEventArgs&>(args));
		case RoutedEventId::PreviewExecuted:
			return cui::framework::RoutedEventAccess::InvokeHandlers(target.OnPreviewExecuted,
				&target, static_cast<ExecutedRoutedEventArgs&>(args));
		case RoutedEventId::Executed:
			return cui::framework::RoutedEventAccess::InvokeHandlers(target.OnExecuted,
				&target, static_cast<ExecutedRoutedEventArgs&>(args));
		case RoutedEventId::PreviewDragEnter:
			return cui::framework::RoutedEventAccess::InvokeHandlers(
				target.OnPreviewDragEnter, &target,
				static_cast<DragEventArgs&>(args));
		case RoutedEventId::DragEnter:
			return cui::framework::RoutedEventAccess::InvokeHandlers(
				target.OnDragEnter, &target,
				static_cast<DragEventArgs&>(args));
		case RoutedEventId::PreviewDragOver:
			return cui::framework::RoutedEventAccess::InvokeHandlers(
				target.OnPreviewDragOver, &target,
				static_cast<DragEventArgs&>(args));
		case RoutedEventId::DragOver:
			return cui::framework::RoutedEventAccess::InvokeHandlers(
				target.OnDragOver, &target,
				static_cast<DragEventArgs&>(args));
		case RoutedEventId::PreviewDragLeave:
			return cui::framework::RoutedEventAccess::InvokeHandlers(
				target.OnPreviewDragLeave, &target,
				static_cast<DragEventArgs&>(args));
		case RoutedEventId::DragLeave:
			return cui::framework::RoutedEventAccess::InvokeHandlers(
				target.OnDragLeave, &target,
				static_cast<DragEventArgs&>(args));
		case RoutedEventId::PreviewDrop:
			return cui::framework::RoutedEventAccess::InvokeHandlers(
				target.OnPreviewDrop, &target,
				static_cast<DragEventArgs&>(args));
		case RoutedEventId::Drop:
			return cui::framework::RoutedEventAccess::InvokeHandlers(
				target.OnDrop, &target,
				static_cast<DragEventArgs&>(args));
		default:
			return {};
		}
	}

	void EnsureRootMousePosition(
		Control& source,
		MouseEventArgs& args,
		float stagedRootX,
		float stagedRootY,
		bool hasStagedPosition)
	{
		if (hasStagedPosition)
		{
			args.RootX = stagedRootX;
			args.RootY = stagedRootY;
			args.HasRootPosition = true;
			return;
		}
		if (args.HasRootPosition) return;
		const auto value = source.GetLocalToRenderTransform();
		const auto transform = D2D1::Matrix3x2F(
			value._11, value._12, value._21,
			value._22, value._31, value._32);
		const auto root = transform.TransformPoint(D2D1::Point2F(
			static_cast<float>(args.X), static_cast<float>(args.Y)));
		args.RootX = root.x;
		args.RootY = root.y;
		args.HasRootPosition = true;
	}

	void SetCurrentMousePosition(Control& target, MouseEventArgs& args)
	{
		if (!args.HasRootPosition) return;
		D2D1_POINT_2F local{};
		if (!target.TryTransformRenderPointToLocal(
			D2D1::Point2F(args.RootX, args.RootY), local)) return;
		args.X = static_cast<int>(std::floor(local.x));
		args.Y = static_cast<int>(std::floor(local.y));
	}

	void EnsureRootDragPosition(Control& source, DragEventArgs& args)
	{
		if (args.HasRootPosition) return;
		const auto value = source.GetLocalToRenderTransform();
		const auto transform = D2D1::Matrix3x2F(
			value._11, value._12, value._21,
			value._22, value._31, value._32);
		const auto root = transform.TransformPoint(D2D1::Point2F(
			static_cast<float>(args.X), static_cast<float>(args.Y)));
		args.RootX = root.x;
		args.RootY = root.y;
		args.HasRootPosition = true;
	}

	void SetCurrentDragPosition(Control& target, DragEventArgs& args)
	{
		if (!args.HasRootPosition) return;
		D2D1_POINT_2F local{};
		if (!target.TryTransformRenderPointToLocal(
			D2D1::Point2F(args.RootX, args.RootY), local)) return;
		args.X = static_cast<int>(std::floor(local.x));
		args.Y = static_cast<int>(std::floor(local.y));
	}

	bool IsRoutedDescendantOrSelf(Control* candidate, Control* root)
	{
		if (!candidate || !root) return false;
		std::unordered_set<Control*> visited;
		for (auto* current = candidate; current
			&& visited.insert(current).second;
			current = current->GetRoutedParent())
		{
			if (current == root) return true;
		}
		return false;
	}
}

thread_local InputManager::ActiveInput* InputManager::CurrentInput = nullptr;

const RoutedEventMetadata& GetRoutedEventMetadata(
	RoutedEventId eventId) noexcept
{
	const auto index = EventIndex(eventId);
	return index < RoutedEvents.size() ? RoutedEvents[index] : RoutedEvents[0];
}

RoutedEventRoute BuildRoutedEventRoute(
	Control* source,
	RoutedEventRoutingStrategy strategy)
{
	RoutedEventRoute route;
	if (!source) return route;
	route.emplace_back(source);
	if (strategy != RoutedEventRoutingStrategy::Direct)
	{
		std::unordered_set<Control*> visited;
		visited.insert(source);
		for (auto* current = source->GetRoutedParent(); current;
			current = current->GetRoutedParent())
		{
			if (!visited.insert(current).second) break;
			route.emplace_back(current);
		}
		if (strategy == RoutedEventRoutingStrategy::Tunnel)
			std::reverse(route.begin(), route.end());
	}
	return route;
}

EventConnection RoutedEventManager::RegisterClassHandler(
	UIClass ownerClass,
	RoutedEventId eventId,
	ClassHandler handler,
	bool handledEventsToo)
{
	if (!handler || eventId == RoutedEventId::None
		|| eventId == RoutedEventId::Count) return {};
	auto& state = ClassHandlers();
	std::uint64_t token = 0;
	{
		std::scoped_lock lock(state.Mutex);
		token = state.NextToken++;
		state.Entries.push_back(ClassHandlerEntry{
			token, ownerClass, eventId,
			handledEventsToo, std::move(handler) });
	}
	return EventConnection([token]()
	{
		auto& current = ClassHandlers();
		std::scoped_lock lock(current.Mutex);
		current.Entries.erase(std::remove_if(
			current.Entries.begin(), current.Entries.end(),
			[token](const ClassHandlerEntry& entry)
			{ return entry.Token == token; }), current.Entries.end());
	});
}

RoutedClassHandlerInvocationCount RoutedEventManager::InvokeClassHandlers(
	Control& target,
	RoutedEventArgs& args)
{
	const ControlWeakReference targetLifetime(&target);
	std::vector<ClassHandlerEntry> snapshot;
	{
		auto& state = ClassHandlers();
		std::scoped_lock lock(state.Mutex);
		for (const auto& entry : state.Entries)
			if (entry.EventId == args.EventId
				&& IsUIClassAssignableFrom(entry.OwnerClass, target.Type()))
				snapshot.push_back(entry);
	}
	std::stable_sort(snapshot.begin(), snapshot.end(),
		[&](const auto& left, const auto& right)
		{
			return GetUIClassInheritanceDistance(
				left.OwnerClass, target.Type())
				< GetUIClassInheritanceDistance(
					right.OwnerClass, target.Type());
		});
	RoutedClassHandlerInvocationCount result;
	for (const auto& entry : snapshot)
	{
		if (!targetLifetime) break;
		if (args.Handled && !entry.HandledEventsToo)
		{
			++result.Skipped;
			continue;
		}
		if (!entry.Handler) continue;
		entry.Handler(&target, args);
		++result.Invoked;
	}
	return result;
}

bool InputManager::CaptureMouse(Window& window, Control* target)
{
	if (!target || (target != &window && target->GetPresentationWindow() != &window)
		|| !target->IsVisible) return false;
	if (_mouseCaptured == target) return true;
	if (window.Handle)
	{
		(void)::SetCapture(window.Handle);
		if (::GetCapture() != window.Handle) return false;
	}

	auto* previous = _mouseCaptured;
	_mouseCaptured = target;
	if (previous)
	{
		RoutedEventArgs args;
		(void)Route(*previous, RoutedEventId::LostMouseCapture, args, nullptr);
		++_statistics.MouseCaptureReleased;
	}
	RoutedEventArgs args;
	(void)Route(*target, RoutedEventId::GotMouseCapture, args, nullptr);
	++_statistics.MouseCaptureAcquired;
	return true;
}

bool InputManager::ReleaseMouseCapture(
	Window& window,
	Control* expectedOwner)
{
	if (!_mouseCaptured || (expectedOwner && _mouseCaptured != expectedOwner))
		return false;
	auto* previous = _mouseCaptured;
	_mouseCaptured = nullptr;
	if (window.Handle && ::GetCapture() == window.Handle)
		(void)::ReleaseCapture();
	RoutedEventArgs args;
	(void)Route(*previous, RoutedEventId::LostMouseCapture, args, nullptr);
	++_statistics.MouseCaptureReleased;
	return true;
}

void InputManager::NotifyCaptureLost(Window& window)
{
	if (!_mouseCaptured) return;
	auto* previous = _mouseCaptured;
	_mouseCaptured = nullptr;
	RoutedEventArgs args;
	(void)Route(*previous, RoutedEventId::LostMouseCapture, args, nullptr);
	++_statistics.MouseCaptureReleased;
}

void InputManager::DetachVisualChild(Window& window, Control* root)
{
	if (IsRoutedDescendantOrSelf(_mouseCaptured, root))
		(void)ReleaseMouseCapture(window);
}

KeyboardFocusTransition InputManager::BeginKeyboardFocusTransition(
	Control* previous,
	Control* current,
	bool cancelable)
{
	KeyboardFocusTransition transition(previous, current);
	if (previous == current)
	{
		transition.Accepted = true;
		transition.Completed = true;
		return transition;
	}

	++_statistics.KeyboardFocusRequests;
	const auto sequence = StandaloneSequence.fetch_add(
		1, std::memory_order_relaxed);
	auto initialize = [sequence](
		KeyboardFocusChangedEventArgs& args,
		Control* source)
	{
		args.OriginalSource = source;
		args.Source = source;
		args.Sequence = sequence;
	};
	if (previous)
	{
		initialize(transition.Lost, previous);
		(void)DispatchPhase(*previous,
			RoutedEventId::PreviewLostKeyboardFocus,
			transition.Lost, nullptr);
		if (cancelable && transition.Lost.Handled)
		{
			++_statistics.KeyboardFocusCanceled;
			transition.Completed = true;
			return transition;
		}
	}
	if (current)
	{
		initialize(transition.Got, current);
		(void)DispatchPhase(*current,
			RoutedEventId::PreviewGotKeyboardFocus,
			transition.Got, nullptr);
		if (cancelable && transition.Got.Handled)
		{
			++_statistics.KeyboardFocusCanceled;
			transition.Completed = true;
			return transition;
		}
	}
	transition.Accepted = true;
	return transition;
}

void InputManager::CompleteKeyboardFocusTransition(
	KeyboardFocusTransition& transition)
{
	if (!transition.Accepted || transition.Completed) return;
	transition.Completed = true;
	++_statistics.KeyboardFocusTransitions;
	if (transition.Lost.OldFocus)
		(void)DispatchPhase(*transition.Lost.OldFocus,
			RoutedEventId::LostKeyboardFocus,
			transition.Lost, nullptr);
	if (transition.Got.NewFocus)
		(void)DispatchPhase(*transition.Got.NewFocus,
			RoutedEventId::GotKeyboardFocus,
			transition.Got, nullptr);
}

void InputManager::CancelKeyboardFocusTransition(
	KeyboardFocusTransition& transition) noexcept
{
	if (transition.Completed) return;
	transition.Accepted = false;
	transition.Completed = true;
	++_statistics.KeyboardFocusCanceled;
}

void InputManager::NotifyLogicalFocusChanged(
	Control* previous,
	Control* current)
{
	if (previous == current) return;
	++_statistics.LogicalFocusTransitions;
	if (previous)
	{
		RoutedEventArgs args;
		(void)Route(*previous, RoutedEventId::LostFocus, args, nullptr);
	}
	if (current)
	{
		RoutedEventArgs args;
		(void)Route(*current, RoutedEventId::GotFocus, args, nullptr);
	}
}

InputManager::StagingScope::StagingScope(
	InputManager& owner,
	Control* originalSource,
	RoutedEventId bubbleEvent,
	float rootX,
	float rootY) noexcept
{
	_state.Owner = &owner;
	_state.Previous = InputManager::CurrentInput;
	_state.OriginalSource = originalSource;
	_state.OriginalSourceLifetime = originalSource;
	_state.BubbleEvent = bubbleEvent;
	_state.Device = GetRoutedEventMetadata(bubbleEvent).Device;
	_state.RootX = rootX;
	_state.RootY = rootY;
	_state.Sequence = ++owner._statistics.Sequence;
	++owner._statistics.RawReports;
	InputManager::CurrentInput = &_state;
}

InputManager::StagingScope::~StagingScope()
{
	if (_state.Owner)
	{
		if (_state.Completed) ++_state.Owner->_statistics.CompletedReports;
		else ++_state.Owner->_statistics.AbortedReports;
		_state.Owner->_statistics.LastHandled = _state.Handled;
		if (_state.Owner->_window
			&& (_state.Device == RoutedInputDeviceKind::Keyboard
				|| _state.Device == RoutedInputDeviceKind::Mouse))
			(void)RoutedCommandManager::InvalidateRequerySuggested(
				*_state.Owner->_window);
	}
	if (InputManager::CurrentInput == &_state)
		InputManager::CurrentInput = _state.Previous;
}

void InputManager::StagingScope::Preview(MouseEventArgs& args)
{
	if (_state.Owner) _state.Owner->Preview(_state, args);
}

void InputManager::StagingScope::Preview(KeyEventArgs& args)
{
	if (_state.Owner) _state.Owner->Preview(_state, args);
}

void InputManager::StagingScope::Preview(TextCompositionEventArgs& args)
{
	if (_state.Owner) _state.Owner->Preview(_state, args);
}

void InputManager::StagingScope::Complete(MouseEventArgs& args)
{
	if (_state.Owner) _state.Owner->Complete(_state, args);
}

void InputManager::StagingScope::Complete(KeyEventArgs& args)
{
	if (_state.Owner) _state.Owner->Complete(_state, args);
}

void InputManager::StagingScope::Complete(TextCompositionEventArgs& args)
{
	if (_state.Owner) _state.Owner->Complete(_state, args);
}

void InputManager::Preview(ActiveInput& active, RoutedEventArgs& args)
{
	auto* originalSource = active.OriginalSourceLifetime.Get();
	if (!originalSource || active.Previewed) return;
	active.Previewed = true;
	const auto paired = GetRoutedEventMetadata(active.BubbleEvent).PairedEvent;
	if (paired == RoutedEventId::None) return;
	(void)Route(*originalSource, paired, args, &active);
}

void InputManager::Complete(ActiveInput& active, RoutedEventArgs& args)
{
	auto* originalSource = active.OriginalSourceLifetime.Get();
	if (!originalSource || active.Completed) return;
	(void)Route(*originalSource, active.BubbleEvent, args, &active);
	active.Completed = true;
}

bool InputManager::DispatchPhase(
	Control& source,
	RoutedEventId eventId,
	RoutedEventArgs& args,
	ActiveInput* active,
	std::span<const ControlWeakReference> sourceToRootRoute)
{
	const auto& metadata = GetRoutedEventMetadata(eventId);
	args.EventId = eventId;
	args.RoutingStrategy = metadata.RoutingStrategy;
	args.Stage = metadata.Stage;
	args.CurrentTarget = nullptr;
	RoutedEventRoute builtRoute;
	if (sourceToRootRoute.empty())
	{
		builtRoute = BuildRoutedEventRoute(
			&source, RoutedEventRoutingStrategy::Bubble);
		sourceToRootRoute = builtRoute;
	}
	const std::size_t routeDepth = metadata.RoutingStrategy
		== RoutedEventRoutingStrategy::Direct
		? (sourceToRootRoute.empty() ? 0U : 1U)
		: sourceToRootRoute.size();
	_statistics.LastRouteDepth = routeDepth;
	_statistics.MaxRouteDepth = (std::max)(
		_statistics.MaxRouteDepth, routeDepth);
	_statistics.LastEvent = eventId;
	switch (metadata.Stage)
	{
	case RoutedEventStage::Preview: ++_statistics.PreviewRoutes; break;
	case RoutedEventStage::Bubble: ++_statistics.BubbleRoutes; break;
	case RoutedEventStage::Direct: ++_statistics.DirectRoutes; break;
	default: break;
	}

	const ControlWeakReference sourceLifetime(&source);
	auto dispatchTarget = [&](const ControlWeakReference& currentReference)
	{
		if (!sourceLifetime) return false;
		auto* current = currentReference.Get();
		if (!current) return true;
		args.CurrentTarget = current;
		if (metadata.Device == RoutedInputDeviceKind::Mouse)
			SetCurrentMousePosition(*current,
				static_cast<MouseEventArgs&>(args));
		else if (metadata.Device == RoutedInputDeviceKind::DragDrop)
			SetCurrentDragPosition(*current,
				static_cast<DragEventArgs&>(args));
		const auto commandBindingCount =
			RoutedCommandManager::InvokeCommandBindings(*current, args);
		_statistics.ClassHandlersInvoked += commandBindingCount.Invoked;
		_statistics.HandlersSkippedAfterHandled += commandBindingCount.Skipped;
		if (!currentReference || !sourceLifetime) return false;
		const auto classCount =
			RoutedEventManager::InvokeClassHandlers(*current, args);
		_statistics.ClassHandlersInvoked += classCount.Invoked;
		_statistics.HandlersSkippedAfterHandled += classCount.Skipped;
		if (!currentReference || !sourceLifetime) return false;
		const auto instanceCount = InvokeInstanceHandlers(*current, args);
		_statistics.InstanceHandlersInvoked += instanceCount.Invoked;
		_statistics.HandlersSkippedAfterHandled += instanceCount.Skipped;
		return static_cast<bool>(sourceLifetime);
	};
	if (metadata.RoutingStrategy == RoutedEventRoutingStrategy::Tunnel)
	{
		for (auto current = sourceToRootRoute.rbegin();
			current != sourceToRootRoute.rend(); ++current)
			if (!dispatchTarget(*current)) break;
	}
	else if (metadata.RoutingStrategy == RoutedEventRoutingStrategy::Direct)
	{
		if (!sourceToRootRoute.empty())
			(void)dispatchTarget(sourceToRootRoute.front());
	}
	else
	{
		for (const auto& current : sourceToRootRoute)
			if (!dispatchTarget(current)) break;
	}
	args.CurrentTarget = nullptr;
	if (!sourceLifetime)
	{
		args.OriginalSource = nullptr;
		args.Source = nullptr;
	}
	_statistics.LastHandled = args.Handled;
	if (active)
	{
		active->Handled = active->Handled || args.Handled;
	}
	return true;
}

bool InputManager::RouteSnapshot(
	Control& source,
	RoutedEventId eventId,
	RoutedEventArgs& args,
	std::span<const ControlWeakReference> sourceToRootRoute)
{
	const auto& metadata = GetRoutedEventMetadata(eventId);
	if (metadata.Id == RoutedEventId::None || sourceToRootRoute.empty()
		|| sourceToRootRoute.front().Get() != &source) return false;
	const auto sourceLifetime = sourceToRootRoute.front();
	args.OriginalSource = &source;
	args.Source = &source;
	args.Sequence = StandaloneSequence.fetch_add(1, std::memory_order_relaxed);
	if (metadata.Stage == RoutedEventStage::Bubble
		&& metadata.PairedEvent != RoutedEventId::None)
		(void)DispatchPhase(source, metadata.PairedEvent, args, nullptr,
			sourceToRootRoute);
	if (!sourceLifetime) return true;
	(void)DispatchPhase(source, eventId, args, nullptr, sourceToRootRoute);
	return true;
}

bool InputManager::Route(
	UIElement& owner,
	RoutedEventId eventId,
	RoutedEventArgs& args,
	ActiveInput* active)
{
	const auto& metadata = GetRoutedEventMetadata(eventId);
	if (metadata.Id == RoutedEventId::None) return false;
	auto* ownerControl = dynamic_cast<Control*>(&owner);
	if (!ownerControl) return false;

	const bool staged = active
		&& active->Device == metadata.Device
		&& metadata.Stage != RoutedEventStage::Direct;
	if (staged && active->Raised.test(EventIndex(eventId)))
	{
		++_statistics.DuplicateRaisesSuppressed;
		return true;
	}
	if (staged)
	{
		active->Raised.set(EventIndex(eventId));
		args.Handled = args.Handled || active->Handled;
	}

	Control* source = staged
		? active->OriginalSourceLifetime.Get() : ownerControl;
	if (!source) return false;
	const ControlWeakReference sourceLifetime(source);
	args.OriginalSource = source;
	args.Source = source;
	args.Sequence = staged ? active->Sequence
		: StandaloneSequence.fetch_add(1, std::memory_order_relaxed);
	if (metadata.Device == RoutedInputDeviceKind::Mouse)
		EnsureRootMousePosition(*source, static_cast<MouseEventArgs&>(args),
			staged ? active->RootX : 0.0f,
			staged ? active->RootY : 0.0f, staged);
	else if (metadata.Device == RoutedInputDeviceKind::DragDrop)
		EnsureRootDragPosition(*source, static_cast<DragEventArgs&>(args));

	if (metadata.Stage == RoutedEventStage::Bubble
		&& metadata.PairedEvent != RoutedEventId::None
		&& (!staged
			|| !active->Raised.test(EventIndex(metadata.PairedEvent))))
	{
		if (staged) active->Raised.set(EventIndex(metadata.PairedEvent));
		(void)DispatchPhase(*source, metadata.PairedEvent, args,
			staged ? active : nullptr);
	}
	if (!sourceLifetime) return true;
	(void)DispatchPhase(*source, eventId, args, staged ? active : nullptr);
	if (staged && eventId == active->BubbleEvent)
		active->Completed = true;
	return true;
}

bool RaiseRoutedEvent(
	UIElement& owner,
	RoutedEventId eventId,
	RoutedEventArgs& args)
{
	if (InputManager::CurrentInput
		&& InputManager::CurrentInput->Owner)
		return InputManager::CurrentInput->Owner->Route(
			owner, eventId, args, InputManager::CurrentInput);
	InputManager standalone;
	return standalone.Route(owner, eventId, args, nullptr);
}

bool RaiseRoutedEventOnRoute(
	Control& source,
	RoutedEventId eventId,
	RoutedEventArgs& args,
	std::span<const ControlWeakReference> sourceToRootRoute)
{
	if (InputManager::CurrentInput
		&& InputManager::CurrentInput->Owner)
		return InputManager::CurrentInput->Owner->RouteSnapshot(
			source, eventId, args, sourceToRootRoute);
	InputManager standalone;
	return standalone.RouteSnapshot(
		source, eventId, args, sourceToRootRoute);
}
