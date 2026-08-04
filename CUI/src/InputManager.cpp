#include "InputManager.h"

#include "Control.h"
#include "InputInfrastructure.h"
#include "RoutedEventInfrastructure.h"
#include "UIElement.h"
#include "Window.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <exception>
#include <mutex>
#include <stdexcept>
#include <unordered_set>

struct RoutedEventHandlerStore::State final
{
	struct Entry final
	{
		std::size_t Token = 0;
		RoutedEventId EventId = RoutedEventId::None;
		RoutedHandlerStorageKind Kind =
			RoutedHandlerStorageKind::TypedFacade;
		bool HandledEventsToo = false;
		ErasedHandler Handler;
	};

	std::size_t NextToken = 1;
	std::vector<Entry> Entries;
};

RoutedEventHandlerStore::RoutedEventHandlerStore()
	: _state(std::make_shared<State>())
{
}

RoutedEventHandlerStore::~RoutedEventHandlerStore() = default;

EventConnection RoutedEventHandlerStore::Subscribe(
	UIElement& owner,
	RoutedEventId eventId,
	RoutedHandlerStorageKind kind,
	ErasedHandler handler,
	bool handledEventsToo)
{
	if (!handler || eventId == RoutedEventId::None
		|| eventId == RoutedEventId::Count) return {};
	if (!owner._routedEventHandlers)
		owner._routedEventHandlers =
			std::make_unique<RoutedEventHandlerStore>();
	auto state = owner._routedEventHandlers->_state;
	const auto token = state->NextToken++;
	state->Entries.push_back(State::Entry{
		token,
		eventId,
		kind,
		handledEventsToo,
		std::move(handler) });
	std::weak_ptr<State> weakState = state;
	return EventConnection([weakState, token]()
	{
		if (auto state = weakState.lock())
		{
			state->Entries.erase(std::remove_if(
				state->Entries.begin(),
				state->Entries.end(),
				[token](const State::Entry& entry)
				{ return entry.Token == token; }),
				state->Entries.end());
		}
	});
}

void RoutedEventHandlerStore::AddPersistent(
	UIElement& owner,
	RoutedEventId eventId,
	RoutedHandlerStorageKind kind,
	ErasedHandler handler,
	bool handledEventsToo)
{
	if (!handler || eventId == RoutedEventId::None
		|| eventId == RoutedEventId::Count) return;
	if (!owner._routedEventHandlers)
		owner._routedEventHandlers =
			std::make_unique<RoutedEventHandlerStore>();
	auto& state = *owner._routedEventHandlers->_state;
	state.Entries.push_back(State::Entry{
		state.NextToken++,
		eventId,
		kind,
		handledEventsToo,
		std::move(handler) });
}

RoutedHandlerInvocationCount RoutedEventHandlerStore::Invoke(
	UIElement& owner,
	RoutedEventId eventId,
	RoutedHandlerStorageKind kind,
	Control* sender,
	RoutedEventArgs& args)
{
	RoutedHandlerInvocationCount result;
	if (!owner._routedEventHandlers) return result;
	auto state = owner._routedEventHandlers->_state;
	if (!state || state->Entries.empty()) return result;
	const ControlWeakReference senderLifetime(sender);
	const auto snapshot = state->Entries;
	for (const auto& entry : snapshot)
	{
		if (sender && !senderLifetime) break;
		if (entry.EventId != eventId || entry.Kind != kind) continue;
		if (args.Handled && !entry.HandledEventsToo)
		{
			++result.Skipped;
			continue;
		}
		if (!entry.Handler) continue;
		entry.Handler(sender, args);
		++result.Invoked;
	}
	return result;
}

std::size_t RoutedEventHandlerStore::Count(
	const UIElement& owner,
	RoutedEventId eventId,
	RoutedHandlerStorageKind kind) noexcept
{
	if (!owner._routedEventHandlers
		|| !owner._routedEventHandlers->_state) return 0;
	const auto& entries = owner._routedEventHandlers->_state->Entries;
	return static_cast<std::size_t>(std::count_if(
		entries.begin(), entries.end(),
		[eventId, kind](const State::Entry& entry)
		{
			return entry.EventId == eventId && entry.Kind == kind;
		}));
}

struct cui::framework::ReverseInheritedProperty::NotificationBatch final
{
	struct Entry final
	{
		ControlWeakReference Target;
		DependencyObject::DeferredPropertyChange Change;
		bool Published = false;
		bool Cancelled = false;
	};

	NotificationBatch* Previous = nullptr;
	std::vector<Entry> Entries;
};

cui::framework::ReverseInheritedProperty::ReverseInheritedProperty(
	Window* owner,
	ReverseInheritedPropertyKind kind) noexcept
	: _window(owner), _kind(kind)
{
}

std::vector<ControlWeakReference>
cui::framework::ReverseInheritedProperty::BuildClosure() const
{
	std::vector<ControlWeakReference> result;
	auto* const window = _window;
	auto* const origin = _origin.Get();
	if (!window || !origin
		|| (origin != window
			&& origin->GetPresentationWindow() != window)) return result;

	std::vector<ControlWeakReference> pending;
	pending.emplace_back(origin);
	std::unordered_set<Control*> visited;
	while (!pending.empty())
	{
		const auto currentReference = pending.back();
		pending.pop_back();
		auto* current = currentReference.Get();
		if (!current || !visited.insert(current).second) continue;
		if (current != window
			&& current->GetPresentationWindow() != window) continue;
		result.emplace_back(current);
		if (current->BlocksReverseInheritance()) continue;

		// LIFO order preserves WPF's core/visual branch before the logical
		// branch. A diamond is collapsed by the identity set above.
		auto* const visualParent = current->GetVisualParent();
		auto* const logicalParent = current->GetLogicalParent();
		if (logicalParent && logicalParent != visualParent)
			pending.emplace_back(logicalParent);
		if (visualParent) pending.emplace_back(visualParent);
	}
	return result;
}

void cui::framework::ReverseInheritedProperty::SetOrigin(
	Control* origin, bool raiseInputEvents)
{
	_origin = origin;
	Reconcile(raiseInputEvents);
}

void cui::framework::ReverseInheritedProperty::Refresh(bool raiseInputEvents)
{
	Reconcile(raiseInputEvents);
}

void cui::framework::ReverseInheritedProperty::Reset(bool raiseInputEvents)
{
	_origin.Reset();
	Reconcile(raiseInputEvents);
}

void cui::framework::ReverseInheritedProperty::Reconcile(
	bool raiseInputEvents)
{
	const ControlWeakReference windowLifetime(_window);
	auto next = BuildClosure();
	auto contains = [](const std::vector<ControlWeakReference>& values,
		const Control* target)
	{
		return std::any_of(values.begin(), values.end(),
			[target](const ControlWeakReference& value)
			{ return value.Get() == target; });
	};

	NotificationBatch batch;
	batch.Previous = _activeBatch;
	auto stage = [&](const ControlWeakReference& reference, bool value)
	{
		auto* target = reference.Get();
		if (!target) return;
		DependencyObject::DeferredPropertyChange change;
		if (!target->StageReverseInheritedPropertyChange(
			_kind, value, change))
			throw std::logic_error(
				"reverse-inherited dependency property commit failed");
		if (!change.HasValue()) return;

		bool previous = false;
		bool current = false;
		if (!change.OldValue().TryGet(previous)
			|| !change.NewValue().TryGet(current))
			throw std::logic_error(
				"reverse-inherited dependency property is not Boolean");
		for (auto* active = _activeBatch; active; active = active->Previous)
		{
			for (auto entry = active->Entries.rbegin();
				entry != active->Entries.rend(); ++entry)
			{
				if (entry->Published || entry->Cancelled
					|| entry->Target.Get() != target) continue;
				bool pendingPrevious = false;
				bool pendingCurrent = false;
				if (entry->Change.OldValue().TryGet(pendingPrevious)
					&& entry->Change.NewValue().TryGet(pendingCurrent)
					&& pendingPrevious == current
					&& pendingCurrent == previous)
				{
					// A nested transition reversed an as-yet-unobserved change.
					// Cancel both notifications like WPF's toggled changed bit.
					entry->Cancelled = true;
					return;
				}
			}
		}
		batch.Entries.push_back(NotificationBatch::Entry{
			reference, std::move(change) });
	};

	// Commit old-only false first, then new-only true. No callbacks can run in
	// this phase, so every public getter is final before publication begins.
	for (const auto& reference : _published)
		if (auto* target = reference.Get(); target && !contains(next, target))
			stage(reference, false);
	for (const auto& reference : next)
		if (auto* target = reference.Get(); target && !contains(_published, target))
			stage(reference, true);
	_published = std::move(next);

	_activeBatch = &batch;
	std::exception_ptr firstError;
	for (auto& entry : batch.Entries)
	{
		if (entry.Cancelled) continue;
		if (!windowLifetime.Get())
		{
			if (firstError) std::rethrow_exception(firstError);
			return;
		}
		auto* target = entry.Target.Get();
		if (!target) continue;
		bool expected = false;
		if (!entry.Change.NewValue().TryGet(expected)) continue;
		const bool stillCurrent = [this, target]
		{
			switch (_kind)
			{
			case ReverseInheritedPropertyKind::KeyboardFocusWithin:
				return target->IsKeyboardFocusWithin;
			case ReverseInheritedPropertyKind::MouseOver:
				return target->IsMouseOver;
			case ReverseInheritedPropertyKind::MouseCaptureWithin:
				return target->IsMouseCaptureWithin;
			}
			return false;
		}();
		// A nested transition may supersede only part of this batch. Keep
		// publishing entries whose committed value is still final; skip genuinely
		// stale entries instead of abandoning unrelated pending notifications.
		if (stillCurrent != expected) continue;
		entry.Published = true;
		try
		{
			target->PublishReverseInheritedPropertyChange(
				_kind, entry.Change);
			if (!windowLifetime.Get())
			{
				if (firstError) std::rethrow_exception(firstError);
				return;
			}
			target = entry.Target.Get();
			bool current = false;
			if (raiseInputEvents
				&& _kind == ReverseInheritedPropertyKind::MouseOver
				&& target && entry.Change.NewValue().TryGet(current)
				&& target->IsMouseOver == current)
			{
				if (auto* window = dynamic_cast<Window*>(
					windowLifetime.Get()))
					window->PublishMouseOverTransition(*target, current);
			}
		}
		catch (...)
		{
			if (!firstError) firstError = std::current_exception();
		}
		if (!windowLifetime.Get())
		{
			if (firstError) std::rethrow_exception(firstError);
			return;
		}
	}
	_activeBatch = batch.Previous;
	if (firstError) std::rethrow_exception(firstError);
}

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
		{ RoutedEventId::Indeterminate, L"Indeterminate",
			RoutedEventRoutingStrategy::Bubble, RoutedEventId::None,
			RoutedInputDeviceKind::None, RoutedEventStage::Bubble },
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
		case RoutedEventId::Indeterminate:
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

	bool IsVisualDescendantOrSelf(Control* candidate, Control* root)
	{
		if (!candidate || !root) return false;
		std::unordered_set<Control*> visited;
		for (auto* current = candidate; current
			&& visited.insert(current).second;
			current = current->GetVisualParent())
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

void InputManager::CompleteMouseCaptureLoss(
	const ControlWeakReference& previous)
{
	if (!previous.HasValue()) return;
	if (auto* previousTarget = previous.Get())
	{
		// CUI controls consume CaptureLost as their internal WPF LostMouseCapture
		// class handling. Run it after the read-only state was cleared, but before
		// public LostMouseCapture observers see the control.
		InputReport captureLost;
		captureLost.Kind = InputReportKind::CaptureLost;
		(void)cui::framework::InputAccess::DispatchInput(
			*previousTarget, captureLost);
		previousTarget = previous.Get();
		if (previousTarget)
		{
			RoutedEventArgs args;
			(void)Route(
				*previousTarget,
				RoutedEventId::LostMouseCapture,
				args,
				nullptr);
		}
	}
	++_statistics.MouseCaptureReleased;
}

bool InputManager::CaptureMouse(Window& window, Control* target)
{
	if (!target || (target != &window && target->GetPresentationWindow() != &window)
		|| target->IsDestroying() || !target->IsVisible
		|| !target->IsEffectivelyEnabled()) return false;
	if (_mouseCaptured == target) return true;

	// SetCapture can synchronously run native callbacks. Freeze both lifetimes
	// before entering USER32, and do not overwrite a nested capture transition.
	const ControlWeakReference requested(target);
	const ControlWeakReference windowLifetime(&window);
	const auto providerVersion = _mouseCaptureVersion;
	const auto windowHandle = window.Handle;
	const auto captureBefore = windowHandle ? ::GetCapture() : nullptr;
	if (windowHandle)
	{
		(void)::SetCapture(windowHandle);
		if (::GetCapture() != windowHandle) return false;
	}
	if (windowLifetime.Get() != &window) return false;
	if (_mouseCaptureVersion != providerVersion)
		return _mouseCaptured == requested;
	target = requested.Get();
	if (!target
		|| (target != &window && target->GetPresentationWindow() != &window)
		|| target->IsDestroying() || !target->IsVisible
		|| !target->IsEffectivelyEnabled())
	{
		if (windowHandle && captureBefore != windowHandle
			&& ::GetCapture() == windowHandle)
			(void)::ReleaseCapture();
		return false;
	}

	const ControlWeakReference previous = _mouseCaptured;
	_mouseCaptured = target;
	const auto transitionVersion = ++_mouseCaptureVersion;
	std::exception_ptr withinError;
	try
	{
		_mouseCaptureWithin.SetOrigin(target);
	}
	catch (...)
	{
		withinError = std::current_exception();
	}
	if (auto* previousTarget = previous.Get())
		cui::framework::InputAccess::PublishMouseCaptureState(
			*previousTarget, false);

	// Property callbacks can synchronously transfer capture or destroy either
	// endpoint. Only this still-current transition may publish the new owner.
	auto* current = requested.Get();
	if (_mouseCaptureVersion == transitionVersion
		&& _mouseCaptured == requested && current)
		cui::framework::InputAccess::PublishMouseCaptureState(*current, true);

	current = requested.Get();
	const bool committed = _mouseCaptureVersion == transitionVersion
		&& _mouseCaptured == requested && current
		&& current->IsMouseCaptured();
	if (!committed && _mouseCaptureVersion == transitionVersion
		&& _mouseCaptured == requested)
	{
		_mouseCaptured.Reset();
		++_mouseCaptureVersion;
		try
		{
			_mouseCaptureWithin.SetOrigin(nullptr);
		}
		catch (...)
		{
			if (!withinError) withinError = std::current_exception();
		}
		if (window.Handle && ::GetCapture() == window.Handle)
			(void)::ReleaseCapture();
	}

	CompleteMouseCaptureLoss(previous);

	// LostMouseCapture can synchronously delete or transfer the requested owner.
	current = requested.Get();
	if (!committed || _mouseCaptureVersion != transitionVersion
		|| _mouseCaptured != requested || !current
		|| !current->IsMouseCaptured())
	{
		if (withinError) std::rethrow_exception(withinError);
		return false;
	}
	RoutedEventArgs args;
	(void)Route(*current, RoutedEventId::GotMouseCapture, args, nullptr);
	++_statistics.MouseCaptureAcquired;
	if (withinError) std::rethrow_exception(withinError);
	return true;
}

bool InputManager::ReleaseMouseCapture(
	Window& window,
	Control* expectedOwner)
{
	auto* captured = _mouseCaptured.Get();
	if (!captured || (expectedOwner && captured != expectedOwner))
		return false;
	const ControlWeakReference previous = _mouseCaptured;
	_mouseCaptured.Reset();
	++_mouseCaptureVersion;
	std::exception_ptr withinError;
	try
	{
		_mouseCaptureWithin.SetOrigin(nullptr);
	}
	catch (...)
	{
		withinError = std::current_exception();
	}
	if (window.Handle && ::GetCapture() == window.Handle)
		(void)::ReleaseCapture();
	if (auto* previousTarget = previous.Get())
		cui::framework::InputAccess::PublishMouseCaptureState(
			*previousTarget, false);
	CompleteMouseCaptureLoss(previous);
	if (withinError) std::rethrow_exception(withinError);
	return true;
}

void InputManager::NotifyCaptureLost(Window& window)
{
	(void)window;
	if (!_mouseCaptured.Get()) return;
	const ControlWeakReference previous = _mouseCaptured;
	_mouseCaptured.Reset();
	++_mouseCaptureVersion;
	std::exception_ptr withinError;
	try
	{
		_mouseCaptureWithin.SetOrigin(nullptr);
	}
	catch (...)
	{
		withinError = std::current_exception();
	}
	if (auto* previousTarget = previous.Get())
		cui::framework::InputAccess::PublishMouseCaptureState(
			*previousTarget, false);
	CompleteMouseCaptureLoss(previous);
	if (withinError) std::rethrow_exception(withinError);
}

void InputManager::SetKeyboardFocusWithinOrigin(Control* target)
{
	_keyboardFocusWithin.SetOrigin(target);
}

void InputManager::SetMouseOverOrigin(
	Control* target, bool raiseInputEvents)
{
	_mouseOver.SetOrigin(target, raiseInputEvents);
}

void InputManager::RefreshReverseInheritedProperties()
{
	const ControlWeakReference windowLifetime(_window);
	std::exception_ptr firstError;
	auto refresh = [&](cui::framework::ReverseInheritedProperty& property)
	{
		try { property.Refresh(); }
		catch (...)
		{
			if (!firstError) firstError = std::current_exception();
		}
	};

	refresh(_keyboardFocusWithin);
	if (!windowLifetime.Get())
	{
		if (firstError) std::rethrow_exception(firstError);
		return;
	}
	refresh(_mouseOver);
	if (!windowLifetime.Get())
	{
		if (firstError) std::rethrow_exception(firstError);
		return;
	}
	refresh(_mouseCaptureWithin);
	if (firstError) std::rethrow_exception(firstError);
}

void InputManager::DetachVisualChild(Window& window, Control* root)
{
	if (IsVisualDescendantOrSelf(_mouseCaptured.Get(), root))
		(void)ReleaseMouseCapture(window);
}

KeyboardFocusTransition InputManager::BeginKeyboardFocusTransition(
	Control* previous,
	Control* current,
	bool cancelable)
{
	KeyboardFocusTransition transition(previous, current);
	const bool hasWindowOwner = _window != nullptr;
	const ControlWeakReference windowLifetime(_window);
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
	auto synchronizeEndpoints = [&]
	{
		transition.Lost.SetFocusEndpoints(
			transition.Previous.Get(), transition.Current.Get());
		transition.Got.SetFocusEndpoints(
			transition.Previous.Get(), transition.Current.Get());
	};
	synchronizeEndpoints();
	if (auto* previousTarget = transition.Previous.Get())
	{
		initialize(transition.Lost, previousTarget);
		(void)DispatchPhase(*previousTarget,
			RoutedEventId::PreviewLostKeyboardFocus,
			transition.Lost, nullptr);
		if (hasWindowOwner && !windowLifetime)
		{
			transition.Completed = true;
			return transition;
		}
		if (cancelable && transition.Lost.Handled)
		{
			++_statistics.KeyboardFocusCanceled;
			transition.Completed = true;
			return transition;
		}
	}
	synchronizeEndpoints();
	if (auto* currentTarget = transition.Current.Get())
	{
		initialize(transition.Got, currentTarget);
		(void)DispatchPhase(*currentTarget,
			RoutedEventId::PreviewGotKeyboardFocus,
			transition.Got, nullptr);
		if (hasWindowOwner && !windowLifetime)
		{
			transition.Completed = true;
			return transition;
		}
		if (transition.Current.HasValue() && !transition.Current)
		{
			transition.Completed = true;
			return transition;
		}
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
	const bool hasWindowOwner = _window != nullptr;
	const ControlWeakReference windowLifetime(_window);
	transition.Completed = true;
	++_statistics.KeyboardFocusTransitions;
	transition.Lost.SetFocusEndpoints(
		transition.Previous.Get(), transition.Current.Get());
	transition.Lost.Handled = false;
	transition.Lost.OriginalSource = transition.Previous.Get();
	transition.Lost.Source = transition.Previous.Get();
	if (auto* previous = transition.Previous.Get())
	{
		(void)DispatchPhase(*previous,
			RoutedEventId::LostKeyboardFocus,
			transition.Lost, nullptr);
		if (auto* liveWindow = dynamic_cast<Window*>(windowLifetime.Get()))
			(void)RoutedCommandManager::InvalidateRequerySuggested(*liveWindow);
	}
	if (hasWindowOwner && !windowLifetime) return;
	// WPF snapshots the requested endpoint for Lost, then reads the committed
	// keyboard focus again before Got. A Lost handler may synchronously move
	// focus, in which case this outer transaction reports old -> final focus
	// after the nested transaction has completed.
	auto* liveWindow = dynamic_cast<Window*>(windowLifetime.Get());
	auto* current = liveWindow
		? liveWindow->GetKeyboardFocusedElement()
		: transition.Current.Get();
	const ControlWeakReference currentLifetime(current);
	transition.Got.SetFocusEndpoints(
		transition.Previous.Get(), currentLifetime.Get());
	transition.Got.Handled = false;
	transition.Got.OriginalSource = currentLifetime.Get();
	transition.Got.Source = currentLifetime.Get();
	if (current = currentLifetime.Get())
	{
		(void)DispatchPhase(*current,
			RoutedEventId::GotKeyboardFocus,
			transition.Got, nullptr);
		if (auto* currentWindow = dynamic_cast<Window*>(windowLifetime.Get()))
			(void)RoutedCommandManager::InvalidateRequerySuggested(*currentWindow);
	}
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
	const bool hasWindowOwner = _window != nullptr;
	const ControlWeakReference windowLifetime(_window);
	if (previous == current) return;
	++_statistics.LogicalFocusTransitions;
	if (previous)
	{
		RoutedEventArgs args;
		(void)Route(*previous, RoutedEventId::LostFocus, args, nullptr);
		if (hasWindowOwner && !windowLifetime) return;
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
	_state.OwnerWindow = owner._window;
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
	if (_state.Owner && (!_state.OwnerWindow.HasValue()
		|| _state.OwnerWindow.Get()))
	{
		if (_state.Completed) ++_state.Owner->_statistics.CompletedReports;
		else ++_state.Owner->_statistics.AbortedReports;
		_state.Owner->_statistics.LastHandled = _state.Handled;
		// Match WPF CommandDevice.PostProcessInput: pointer/key motion and
		// press events do not globally requery every command source.  Requery
		// only after the corresponding input gesture has completed.
		if (auto* liveWindow = dynamic_cast<Window*>(_state.OwnerWindow.Get());
			liveWindow
			&& (_state.BubbleEvent == RoutedEventId::KeyUp
				|| _state.BubbleEvent == RoutedEventId::MouseUp))
			(void)RoutedCommandManager::InvalidateRequerySuggested(
				*liveWindow);
	}
	if (InputManager::CurrentInput == &_state)
		InputManager::CurrentInput = _state.Previous;
}

void InputManager::StagingScope::Preview(MouseEventArgs& args)
{
	if (_state.Owner && (!_state.OwnerWindow.HasValue()
		|| _state.OwnerWindow.Get())) _state.Owner->Preview(_state, args);
}

void InputManager::StagingScope::Preview(KeyEventArgs& args)
{
	if (_state.Owner && (!_state.OwnerWindow.HasValue()
		|| _state.OwnerWindow.Get())) _state.Owner->Preview(_state, args);
}

void InputManager::StagingScope::Preview(TextCompositionEventArgs& args)
{
	if (_state.Owner && (!_state.OwnerWindow.HasValue()
		|| _state.OwnerWindow.Get())) _state.Owner->Preview(_state, args);
}

void InputManager::StagingScope::Complete(MouseEventArgs& args)
{
	if (_state.Owner && (!_state.OwnerWindow.HasValue()
		|| _state.OwnerWindow.Get())) _state.Owner->Complete(_state, args);
}

void InputManager::StagingScope::Complete(KeyEventArgs& args)
{
	if (_state.Owner && (!_state.OwnerWindow.HasValue()
		|| _state.OwnerWindow.Get())) _state.Owner->Complete(_state, args);
}

void InputManager::StagingScope::Complete(TextCompositionEventArgs& args)
{
	if (_state.Owner && (!_state.OwnerWindow.HasValue()
		|| _state.OwnerWindow.Get())) _state.Owner->Complete(_state, args);
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
	const bool hasWindowOwner = _window != nullptr;
	const ControlWeakReference windowLifetime(_window);
	auto managerAlive = [&]
	{
		return !hasWindowOwner || static_cast<bool>(windowLifetime);
	};
	auto abandonManager = [&]
	{
		if (active) active->Owner = nullptr;
		return false;
	};
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
		if (!managerAlive()) return abandonManager();
		_statistics.ClassHandlersInvoked += commandBindingCount.Invoked;
		_statistics.HandlersSkippedAfterHandled += commandBindingCount.Skipped;
		if (!currentReference || !sourceLifetime) return false;
		const auto classCount =
			RoutedEventManager::InvokeClassHandlers(*current, args);
		if (!managerAlive()) return abandonManager();
		_statistics.ClassHandlersInvoked += classCount.Invoked;
		_statistics.HandlersSkippedAfterHandled += classCount.Skipped;
		if (!currentReference || !sourceLifetime) return false;
		const auto genericCount =
			cui::framework::RoutedEventAccess::InvokeGenericHandlers(
				*current, current, args);
		if (!managerAlive()) return abandonManager();
		_statistics.InstanceHandlersInvoked += genericCount.Invoked;
		_statistics.HandlersSkippedAfterHandled += genericCount.Skipped;
		if (!currentReference || !sourceLifetime) return false;
		const auto instanceCount = InvokeInstanceHandlers(*current, args);
		if (!managerAlive()) return abandonManager();
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
	if (!managerAlive()) return abandonManager();
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
	const bool hasWindowOwner = _window != nullptr;
	const ControlWeakReference windowLifetime(_window);
	const auto& metadata = GetRoutedEventMetadata(eventId);
	if (metadata.Id == RoutedEventId::None || sourceToRootRoute.empty()
		|| sourceToRootRoute.front().Get() != &source) return false;
	const auto sourceLifetime = sourceToRootRoute.front();
	args.OriginalSource = &source;
	args.Source = &source;
	args.Sequence = StandaloneSequence.fetch_add(1, std::memory_order_relaxed);
	if (metadata.Stage == RoutedEventStage::Bubble
		&& metadata.PairedEvent != RoutedEventId::None)
	{
		(void)DispatchPhase(source, metadata.PairedEvent, args, nullptr,
			sourceToRootRoute);
		if (hasWindowOwner && !windowLifetime) return false;
	}
	if (!sourceLifetime) return true;
	(void)DispatchPhase(source, eventId, args, nullptr, sourceToRootRoute);
	if (hasWindowOwner && !windowLifetime) return false;
	return true;
}

bool InputManager::Route(
	UIElement& owner,
	RoutedEventId eventId,
	RoutedEventArgs& args,
	ActiveInput* active)
{
	const bool hasWindowOwner = _window != nullptr;
	const ControlWeakReference windowLifetime(_window);
	auto ownerExpired = [&]
	{
		if (!hasWindowOwner || windowLifetime) return false;
		if (active) active->Owner = nullptr;
		return true;
	};
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
		if (ownerExpired()) return false;
	}
	if (!sourceLifetime) return true;
	(void)DispatchPhase(*source, eventId, args, staged ? active : nullptr);
	if (ownerExpired()) return false;
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
