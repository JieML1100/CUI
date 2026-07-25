#pragma once

#include "ControlWeakReference.h"
#include "Event.h"

#include <bitset>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <vector>

class Control;
class UIElement;
class Window;

/** Stable counters for the normalized native-input staging pipeline. */
struct InputStagingStatistics final
{
	std::uint64_t Sequence = 0;
	std::uint64_t RawReports = 0;
	std::uint64_t CompletedReports = 0;
	std::uint64_t AbortedReports = 0;
	std::uint64_t PreviewRoutes = 0;
	std::uint64_t BubbleRoutes = 0;
	std::uint64_t DirectRoutes = 0;
	std::uint64_t ClassHandlersInvoked = 0;
	std::uint64_t InstanceHandlersInvoked = 0;
	std::uint64_t HandlersSkippedAfterHandled = 0;
	std::uint64_t DuplicateRaisesSuppressed = 0;
	std::uint64_t MouseCaptureAcquired = 0;
	std::uint64_t MouseCaptureReleased = 0;
	std::uint64_t KeyboardFocusRequests = 0;
	std::uint64_t KeyboardFocusTransitions = 0;
	std::uint64_t KeyboardFocusCanceled = 0;
	std::uint64_t LogicalFocusTransitions = 0;
	std::size_t MaxRouteDepth = 0;
	std::size_t LastRouteDepth = 0;
	RoutedEventId LastEvent = RoutedEventId::None;
	bool LastHandled = false;
};

/** One class-handler invocation summary used by InputManager diagnostics. */
struct RoutedClassHandlerInvocationCount final
{
	std::size_t Invoked = 0;
	std::size_t Skipped = 0;
};

/**
 * Registers framework class handlers without C++ custom-control type
 * registration. UI_Base applies to every built-in/XAML element; other
 * UIClass handlers follow the represented native base-class closure.
 */
class RoutedEventManager final
{
public:
	using ClassHandler = std::function<void(Control*, RoutedEventArgs&)>;

	static EventConnection RegisterClassHandler(
		UIClass ownerClass,
		RoutedEventId eventId,
		ClassHandler handler,
		bool handledEventsToo = false);

private:
	friend class InputManager;
	static RoutedClassHandlerInvocationCount InvokeClassHandlers(
		Control& target,
		RoutedEventArgs& args);
};

/** Builds one cycle-safe route snapshot; tunnel reverses the same snapshot. */
using RoutedEventRoute = std::vector<ControlWeakReference>;

RoutedEventRoute BuildRoutedEventRoute(
	Control* source,
	RoutedEventRoutingStrategy strategy);

/** Raises one routed-event pair over an already captured source-to-root path. */
bool RaiseRoutedEventOnRoute(
	Control& source,
	RoutedEventId eventId,
	RoutedEventArgs& args,
	std::span<const ControlWeakReference> sourceToRootRoute);

/** Routed data retained across preview, state commit and bubble phases. */
struct KeyboardFocusTransition final
{
	explicit KeyboardFocusTransition(Control* previous, Control* current)
		: Lost(previous, current), Got(previous, current) {}
	KeyboardFocusTransition(const KeyboardFocusTransition&) = delete;
	KeyboardFocusTransition& operator=(const KeyboardFocusTransition&) = delete;
	KeyboardFocusTransition(KeyboardFocusTransition&&) noexcept = default;
	KeyboardFocusTransition& operator=(KeyboardFocusTransition&&) noexcept = default;

	KeyboardFocusChangedEventArgs Lost;
	KeyboardFocusChangedEventArgs Got;
	bool Accepted = false;
	bool Completed = false;
};

/**
 * Per-Window normalized input coordinator.
 *
 * A native message creates one StagingScope. Preview is raised before C++
 * behavior sees the message; the source behavior then raises the bubble
 * wrapper, and Complete guarantees a bubble even when no behavior emitted it.
 */
class InputManager final
{
private:
	static constexpr std::size_t RoutedEventCount =
		static_cast<std::size_t>(RoutedEventId::Count);

	struct ActiveInput final
	{
		InputManager* Owner = nullptr;
		ActiveInput* Previous = nullptr;
		Control* OriginalSource = nullptr;
		ControlWeakReference OriginalSourceLifetime;
		RoutedEventId BubbleEvent = RoutedEventId::None;
		RoutedInputDeviceKind Device = RoutedInputDeviceKind::None;
		float RootX = 0.0f;
		float RootY = 0.0f;
		std::uint64_t Sequence = 0;
		std::bitset<RoutedEventCount> Raised;
		bool Handled = false;
		bool Previewed = false;
		bool Completed = false;
	};

	static thread_local ActiveInput* CurrentInput;
	InputStagingStatistics _statistics;
	Window* _window = nullptr;
	Control* _mouseCaptured = nullptr;

	bool Route(
		UIElement& owner,
		RoutedEventId eventId,
		RoutedEventArgs& args,
		ActiveInput* active);
	bool DispatchPhase(
		Control& source,
		RoutedEventId eventId,
		RoutedEventArgs& args,
		ActiveInput* active,
		std::span<const ControlWeakReference> sourceToRootRoute = {});
	bool RouteSnapshot(
		Control& source,
		RoutedEventId eventId,
		RoutedEventArgs& args,
		std::span<const ControlWeakReference> sourceToRootRoute);
	void Preview(ActiveInput& active, RoutedEventArgs& args);
	void Complete(ActiveInput& active, RoutedEventArgs& args);

	friend bool RaiseRoutedEvent(
		UIElement& owner,
		RoutedEventId eventId,
		RoutedEventArgs& args);
	friend bool RaiseRoutedEventOnRoute(
		Control& source,
		RoutedEventId eventId,
		RoutedEventArgs& args,
		std::span<const ControlWeakReference> sourceToRootRoute);

public:
	class StagingScope final
	{
	public:
		StagingScope(
			InputManager& owner,
			Control* originalSource,
			RoutedEventId bubbleEvent,
			float rootX = 0.0f,
			float rootY = 0.0f) noexcept;
		~StagingScope();

		StagingScope(const StagingScope&) = delete;
		StagingScope& operator=(const StagingScope&) = delete;
		StagingScope(StagingScope&&) = delete;
		StagingScope& operator=(StagingScope&&) = delete;

		void Preview(MouseEventArgs& args);
		void Preview(KeyEventArgs& args);
		void Preview(TextCompositionEventArgs& args);
		void Complete(MouseEventArgs& args);
		void Complete(KeyEventArgs& args);
		void Complete(TextCompositionEventArgs& args);

		bool Handled() const noexcept { return _state.Handled; }
		std::uint64_t Sequence() const noexcept { return _state.Sequence; }

	private:
		ActiveInput _state;
	};

	explicit InputManager(Window* owner = nullptr) noexcept : _window(owner) {}
	InputManager(const InputManager&) = delete;
	InputManager& operator=(const InputManager&) = delete;

	Control* MouseCaptured() const noexcept { return _mouseCaptured; }
	bool CaptureMouse(Window& window, Control* target);
	bool ReleaseMouseCapture(Window& window, Control* expectedOwner = nullptr);
	void NotifyCaptureLost(Window& window);
	void DetachVisualChild(Window& window, Control* root);
	KeyboardFocusTransition BeginKeyboardFocusTransition(
		Control* previous,
		Control* current,
		bool cancelable = true);
	void CompleteKeyboardFocusTransition(
		KeyboardFocusTransition& transition);
	void CancelKeyboardFocusTransition(
		KeyboardFocusTransition& transition) noexcept;
	void NotifyLogicalFocusChanged(Control* previous, Control* current);

	InputStagingStatistics Statistics() const noexcept { return _statistics; }
};
