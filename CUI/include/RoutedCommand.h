#pragma once

#include "ControlWeakReference.h"
#include "CuiBuildFeatures.h"
#include "Event.h"

#include <any>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <variant>

class Control;
class InputManager;
class UIElement;
class Window;
struct ComponentTypeToken;
#if CUI_ENABLE_DYNAMIC_XAML
struct RuntimeTypeId;
#endif

namespace cui::framework
{
	struct InputAccess;
}

/** Value identity authored by XAML; no C++ type registration is involved. */
class RoutedCommand final
{
public:
	RoutedCommand() = default;
	explicit RoutedCommand(std::wstring name) : _name(std::move(name)) {}

	const std::wstring& Name() const noexcept { return _name; }
	bool Empty() const noexcept { return _name.empty(); }
	bool operator==(const RoutedCommand&) const = default;

private:
	std::wstring _name;
};

/** Canonical WPF application-command identities shared across controls. */
class ApplicationCommands final
{
public:
	static const RoutedCommand& Copy();
	static const RoutedCommand& Delete();
	static const RoutedCommand& SelectAll();
};

/** Shared args for PreviewCanExecute/CanExecute. */
class CanExecuteRoutedEventArgs final : public RoutedEventArgs
{
public:
	RoutedCommand Command;
	std::any Parameter;
	bool CanExecute = false;
	bool ContinueRouting = false;
	bool CommandBindingMatched = false;
	std::uint64_t CommandTransactionId = 0;
	std::uint64_t CommandRouteId = 0;

	CanExecuteRoutedEventArgs() = default;
	CanExecuteRoutedEventArgs(RoutedCommand command, std::any parameter = {})
		: Command(std::move(command)), Parameter(std::move(parameter)) {}
};

/** Shared args for PreviewExecuted/Executed. */
class ExecutedRoutedEventArgs final : public RoutedEventArgs
{
public:
	RoutedCommand Command;
	std::any Parameter;
	bool Executed = false;
	std::uint64_t CommandTransactionId = 0;
	std::uint64_t CommandRouteId = 0;

	ExecutedRoutedEventArgs() = default;
	ExecutedRoutedEventArgs(RoutedCommand command, std::any parameter = {})
		: Command(std::move(command)), Parameter(std::move(parameter)) {}
};

/** Key + modifier identity used by XAML KeyBinding. */
struct KeyGesture final
{
	::Key Key = ::Key::None;
	ModifierKeys Modifiers = ModifierKeys::None;

	bool IsValid() const noexcept;
	bool Matches(::Key key, ModifierKeys modifiers) const noexcept;
	bool operator==(const KeyGesture&) const = default;
};

/** Parses canonical WPF-like gestures such as Ctrl+Shift+S and Alt+F4. */
bool TryParseKeyGesture(
	std::wstring_view text,
	KeyGesture& result,
	std::wstring* error = nullptr);
std::wstring FormatKeyGesture(const KeyGesture& gesture);

/** Declarative input mapping stored on any UIElement. */
struct KeyBinding final
{
	RoutedCommand Command;
	KeyGesture Gesture;
	std::any CommandParameter;
	ControlWeakReference CommandTarget;
};

/** WPF-compatible mouse actions accepted by XAML MouseBinding. */
enum class MouseAction : unsigned char
{
	None,
	LeftClick,
	RightClick,
	MiddleClick,
	WheelClick,
	LeftDoubleClick,
	RightDoubleClick,
	MiddleDoubleClick
};

/** Mouse action + keyboard modifier identity used by XAML MouseBinding. */
struct MouseGesture final
{
	MouseAction Action = MouseAction::None;
	ModifierKeys Modifiers = ModifierKeys::None;

	bool IsValid() const noexcept;
	bool Matches(
		const MouseEventArgs& input,
		ModifierKeys modifiers) const noexcept;
	bool operator==(const MouseGesture&) const = default;
};

/** Parses canonical WPF-like gestures such as Ctrl+LeftClick. */
bool TryParseMouseGesture(
	std::wstring_view text,
	MouseGesture& result,
	std::wstring* error = nullptr);
std::wstring FormatMouseGesture(const MouseGesture& gesture);

struct MouseBinding final
{
	RoutedCommand Command;
	MouseGesture Gesture;
	std::any CommandParameter;
	ControlWeakReference CommandTarget;
};

/** One polymorphic XAML InputBinding owned by a UIElement. */
using InputBinding = std::variant<KeyBinding, MouseBinding>;

/**
 * Native handler projection of one XAML CommandBinding.
 * The identity remains a string from XAML; these callbacks only attach behavior.
 */
struct CommandBinding final
{
	using CanExecuteHandler =
		std::function<void(Control*, CanExecuteRoutedEventArgs&)>;
	using ExecutedHandler =
		std::function<void(Control*, ExecutedRoutedEventArgs&)>;

	RoutedCommand Command;
	CanExecuteHandler PreviewCanExecute;
	CanExecuteHandler CanExecute;
	ExecutedHandler PreviewExecuted;
	ExecutedHandler Executed;
};

/** Immutable summary produced by the one CanExecute routed transaction. */
struct RoutedCommandCanExecuteResult final
{
	bool CanExecute = false;
	bool ContinueRouting = false;
	bool Handled = false;
	bool CommandBindingMatched = false;
	Control* Target = nullptr;
	std::uint64_t TransactionId = 0;
	std::uint64_t RouteId = 0;
	std::uint64_t RequeryGeneration = 0;
	std::size_t RouteDepth = 0;
};

/** Query and execution outcomes from one route snapshot. */
struct RoutedCommandExecutionResult final
{
	RoutedCommandCanExecuteResult Query;
	bool Executed = false;
	bool Handled = false;
	std::uint64_t TransactionId = 0;
	std::uint64_t RouteId = 0;
};

/** WPF CommandSource projection consumed by automatic CanExecute observers. */
struct RoutedCommandSourceQuery final
{
	RoutedCommand Command;
	std::any Parameter;
	ControlWeakReference CommandTarget;
};

/** One coalesced invalidation in a Window command domain. */
struct RoutedCommandRequeryEventArgs final
{
	Window* Scope = nullptr;
	std::uint64_t Generation = 0;
};

/**
 * Per-Window execution/requery coordinator.
 *
 * Public operations stay static so command sources have one entry point. The
 * mutable requery state itself is owned by Window; there is no process-global
 * fallback event and therefore no cross-window CanExecute traffic.
 */
class RoutedCommandManager final
{
public:
	using CanExecuteObserver = std::function<void(
		Control&, const RoutedCommandCanExecuteResult&)>;
	using RequeryHandler = std::function<void(
		const RoutedCommandRequeryEventArgs&)>;

	explicit RoutedCommandManager(Window& owner);
	~RoutedCommandManager();
	RoutedCommandManager(const RoutedCommandManager&) = delete;
	RoutedCommandManager& operator=(const RoutedCommandManager&) = delete;

	static RoutedCommandCanExecuteResult QueryCanExecute(
		const RoutedCommand& command,
		Control& target,
		std::any parameter = {});
	static RoutedCommandCanExecuteResult QueryCommandSource(
		Control& source,
		const RoutedCommandSourceQuery& query);
	static bool CanExecute(
		const RoutedCommand& command,
		Control& target,
		std::any parameter = {});
	static RoutedCommandExecutionResult ExecuteCommand(
		const RoutedCommand& command,
		Control& target,
		std::any parameter = {});
	static RoutedCommandExecutionResult ExecuteCommandSource(
		Control& source,
		const RoutedCommandSourceQuery& query);
	static bool Execute(
		const RoutedCommand& command,
		Control& target,
		std::any parameter = {});

private:
	friend class Window;
	friend struct cui::framework::InputAccess;
	static bool ProcessInput(Control& source, const KeyEventArgs& input);
	static bool ProcessInput(
		Control& source,
		const MouseEventArgs& input,
		ModifierKeys modifiers = ModifierKeys::None);

public:

	/** Registers one exact compiled component class behavior. */
	static EventConnection RegisterClassCommandBinding(
		ComponentTypeToken ownerType,
		CommandBinding binding);
#if CUI_ENABLE_DYNAMIC_XAML
	/** Design compatibility overload that lowers the QName to a token. */
	static EventConnection RegisterClassCommandBinding(
		const RuntimeTypeId& ownerType,
		CommandBinding binding);
#endif
	/** Registers a native behavior-host class fallback (derived before base). */
	static EventConnection RegisterClassCommandBinding(
		UIClass ownerClass,
		CommandBinding binding);

	/** Registers one immutable WPF-style class input binding. */
	static EventConnection RegisterClassInputBinding(
		ComponentTypeToken ownerType,
		InputBinding binding);
#if CUI_ENABLE_DYNAMIC_XAML
	/** Design compatibility overload that lowers the QName to a token. */
	static EventConnection RegisterClassInputBinding(
		const RuntimeTypeId& ownerType,
		InputBinding binding);
#endif
	/** Registers a native class gesture (derived classes are matched first). */
	static EventConnection RegisterClassInputBinding(
		UIClass ownerClass,
		InputBinding binding);

	/**
	 * Publishes the initial unified query result immediately and one fresh
	 * result after each coalesced requery in the source's current Window.
	 */
	static EventConnection ObserveCanExecute(
		Control& source,
		RoutedCommandSourceQuery query,
		CanExecuteObserver observer);
	/**
	 * Defers observer publication while one command source is being moved
	 * through transient presentation parents. Window-domain registration still
	 * follows every transition; disposal publishes once only when the final
	 * domain differs from the domain at entry.
	 */
	static EventConnection DeferSourceScopeTransitions(Control& source);

	static EventConnection SubscribeRequerySuggested(
		Control& scope,
		RequeryHandler handler);
	static bool InvalidateRequerySuggested(Control& scope);
	static std::uint64_t GetRequeryGeneration(
		const Control& scope) noexcept;

private:
	struct RequeryState;
	std::shared_ptr<RequeryState> _requeryState;

	friend class InputManager;
	friend class UIElement;
	friend class Window;
	friend class Control;
	static RoutedHandlerInvocationCount InvokeCommandBindings(
		Control& target,
		RoutedEventArgs& args);
	static void RefreshCanExecuteObservers(
		Control& root,
		std::uint64_t generation);
	static Control* ResolveCommandTarget(
		Control& source,
		const ControlWeakReference& requested) noexcept;
	static void NotifySourceScopeChanged(Control& source);
	void ProcessPendingRequery();
};
