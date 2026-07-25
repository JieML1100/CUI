#pragma once
#include "Binding.h"
#include "ControlWeakReference.h"
#include "Core/EventConnection.h"
#include "Core/Geometry.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

class Control;
class UIElement;
enum class UIClass : int;
template<typename Func>
class Event;
template<typename T>
class ObservableCollection;

namespace cui::framework
{
	struct EventAccess;
	struct RoutedEventAccess;
}

/** WPF-style identity of the button whose state changed. */
enum class MouseButton : std::uint8_t
{
	None,
	Left,
	Right,
	Middle,
	XButton1,
	XButton2,
};

enum class MouseButtonState : std::uint8_t
{
	Released,
	Pressed,
};

/** Independent WPF button-state snapshot. */
struct MouseButtonStates final
{
	MouseButtonState LeftButton = MouseButtonState::Released;
	MouseButtonState RightButton = MouseButtonState::Released;
	MouseButtonState MiddleButton = MouseButtonState::Released;
	MouseButtonState XButton1 = MouseButtonState::Released;
	MouseButtonState XButton2 = MouseButtonState::Released;

	[[nodiscard]] MouseButtonState Get(MouseButton button) const noexcept
	{
		switch (button)
		{
		case MouseButton::Left: return LeftButton;
		case MouseButton::Right: return RightButton;
		case MouseButton::Middle: return MiddleButton;
		case MouseButton::XButton1: return XButton1;
		case MouseButton::XButton2: return XButton2;
		default: return MouseButtonState::Released;
		}
	}

	[[nodiscard]] bool IsPressed(MouseButton button) const noexcept
	{
		return Get(button) == MouseButtonState::Pressed;
	}

	void Set(MouseButton button, MouseButtonState state) noexcept
	{
		switch (button)
		{
		case MouseButton::Left: LeftButton = state; break;
		case MouseButton::Right: RightButton = state; break;
		case MouseButton::Middle: MiddleButton = state; break;
		case MouseButton::XButton1: XButton1 = state; break;
		case MouseButton::XButton2: XButton2 = state; break;
		default: break;
		}
	}

	[[nodiscard]] static MouseButtonStates WithPressed(
		MouseButton button) noexcept
	{
		MouseButtonStates result;
		result.Set(button, MouseButtonState::Pressed);
		return result;
	}
};

/**
 * WPF keyboard identity. Values intentionally follow the semantic Key enum,
 * not Win32 virtual-key numbers; the platform host performs explicit mapping.
 */
enum class Key : std::uint16_t
{
	None,
	Cancel,
	Back,
	Tab,
	LineFeed,
	Clear,
	Return,
	Enter = Return,
	Pause,
	Capital,
	CapsLock = Capital,
	KanaMode,
	HangulMode = KanaMode,
	JunjaMode,
	FinalMode,
	HanjaMode,
	KanjiMode = HanjaMode,
	Escape,
	ImeConvert,
	ImeNonConvert,
	ImeAccept,
	ImeModeChange,
	Space,
	Prior,
	PageUp = Prior,
	Next,
	PageDown = Next,
	End,
	Home,
	Left,
	Up,
	Right,
	Down,
	Select,
	Print,
	Execute,
	Snapshot,
	PrintScreen = Snapshot,
	Insert,
	Delete,
	Help,
	D0,
	D1,
	D2,
	D3,
	D4,
	D5,
	D6,
	D7,
	D8,
	D9,
	A,
	B,
	C,
	D,
	E,
	F,
	G,
	H,
	I,
	J,
	K,
	L,
	M,
	N,
	O,
	P,
	Q,
	R,
	S,
	T,
	U,
	V,
	W,
	X,
	Y,
	Z,
	LWin,
	RWin,
	Apps,
	Sleep,
	NumPad0,
	NumPad1,
	NumPad2,
	NumPad3,
	NumPad4,
	NumPad5,
	NumPad6,
	NumPad7,
	NumPad8,
	NumPad9,
	Multiply,
	Add,
	Separator,
	Subtract,
	Decimal,
	Divide,
	F1,
	F2,
	F3,
	F4,
	F5,
	F6,
	F7,
	F8,
	F9,
	F10,
	F11,
	F12,
	F13,
	F14,
	F15,
	F16,
	F17,
	F18,
	F19,
	F20,
	F21,
	F22,
	F23,
	F24,
	NumLock,
	Scroll,
	LeftShift,
	RightShift,
	LeftCtrl,
	RightCtrl,
	LeftAlt,
	RightAlt,
	BrowserBack,
	BrowserForward,
	BrowserRefresh,
	BrowserStop,
	BrowserSearch,
	BrowserFavorites,
	BrowserHome,
	VolumeMute,
	VolumeDown,
	VolumeUp,
	MediaNextTrack,
	MediaPreviousTrack,
	MediaStop,
	MediaPlayPause,
	LaunchMail,
	SelectMedia,
	LaunchApplication1,
	LaunchApplication2,
	Oem1,
	OemSemicolon = Oem1,
	OemPlus,
	OemComma,
	OemMinus,
	OemPeriod,
	Oem2,
	OemQuestion = Oem2,
	Oem3,
	OemTilde = Oem3,
	AbntC1,
	AbntC2,
	Oem4,
	OemOpenBrackets = Oem4,
	Oem5,
	OemPipe = Oem5,
	Oem6,
	OemCloseBrackets = Oem6,
	Oem7,
	OemQuotes = Oem7,
	Oem8,
	Oem102,
	OemBackslash = Oem102,
	ImeProcessed,
	/** WPF sentinel; inspect KeyEventArgs::SystemKey for the physical key. */
	System,
	OemAttn,
	DbeAlphanumeric = OemAttn,
	OemFinish,
	DbeKatakana = OemFinish,
	OemCopy,
	DbeHiragana = OemCopy,
	OemAuto,
	DbeSbcsChar = OemAuto,
	OemEnlw,
	DbeDbcsChar = OemEnlw,
	OemBackTab,
	DbeRoman = OemBackTab,
	Attn,
	DbeNoRoman = Attn,
	CrSel,
	DbeEnterWordRegisterMode = CrSel,
	ExSel,
	DbeEnterImeConfigureMode = ExSel,
	EraseEof,
	DbeFlushString = EraseEof,
	Play,
	DbeCodeInput = Play,
	Zoom,
	DbeNoCodeInput = Zoom,
	NoName,
	DbeDetermineString = NoName,
	Pa1,
	DbeEnterDialogConversionMode = Pa1,
	OemClear,
	DeadCharProcessed,
};

/** WPF-style keyboard modifier snapshot, never combined with Key. */
enum class ModifierKeys : std::uint8_t
{
	None = 0,
	Alt = 1,
	Control = 2,
	Shift = 4,
	Windows = 8,
};

inline constexpr ModifierKeys operator|(
	ModifierKeys left, ModifierKeys right) noexcept
{
	return static_cast<ModifierKeys>(
		static_cast<std::uint8_t>(left)
		| static_cast<std::uint8_t>(right));
}

inline constexpr ModifierKeys operator&(
	ModifierKeys left, ModifierKeys right) noexcept
{
	return static_cast<ModifierKeys>(
		static_cast<std::uint8_t>(left)
		& static_cast<std::uint8_t>(right));
}

inline constexpr ModifierKeys& operator|=(
	ModifierKeys& left, ModifierKeys right) noexcept
{
	left = left | right;
	return left;
}

inline constexpr bool HasModifier(
	ModifierKeys value, ModifierKeys modifier) noexcept
{
	return (value & modifier) == modifier;
}
template<typename Func>
class Event {
public:
	using function_type = typename std::remove_pointer<Func>::type;
	using std_function_type = std::function<function_type>;

private:
	struct Entry final {
		size_t Token = 0;
		std_function_type Handler;
	};

	struct State final {
		size_t NextToken = 1;
		std::vector<Entry> Entries;
	};

	std::shared_ptr<State> _state;
	friend struct cui::framework::EventAccess;
	template<typename>
	friend class ObservableCollection;

	size_t Add(std_function_type handler) {
		if (!handler) return 0;
		if (!_state) _state = std::make_shared<State>();
		const size_t token = _state->NextToken++;
		_state->Entries.push_back(Entry{ token, std::move(handler) });
		return token;
	}

	static void RemoveToken(State& state, size_t token) {
		if (token == 0) return;
		state.Entries.erase(
			std::remove_if(state.Entries.begin(), state.Entries.end(),
				[token](const Entry& entry) { return entry.Token == token; }),
			state.Entries.end());
	}

	template <typename... Args>
	void InvokeCore(Args&&... args) {
		if (!_state || _state->Entries.empty()) return;
		const auto snapshot = _state->Entries;
		for (const auto& entry : snapshot) {
			if (entry.Handler) entry.Handler(std::forward<Args>(args)...);
		}
	}

	void ClearCore() noexcept {
		if (_state) _state->Entries.clear();
	}

public:
	Event() = default;
	~Event() = default;

	Event(const Event&) = delete;
	Event& operator=(const Event&) = delete;
	Event(Event&&) = default;
	Event& operator=(Event&&) = default;

	template<typename F>
	EventConnection Subscribe(F&& fn) {
		std_function_type handler(std::forward<F>(fn));
		const size_t token = Add(std::move(handler));
		if (token == 0) return {};
		std::weak_ptr<State> weakState = _state;
		return EventConnection([weakState, token]() {
			if (auto state = weakState.lock()) RemoveToken(*state, token);
		});
	}

	/**
	 * @brief 以弱引用方式订阅：仅当 target 仍存活时才会调用其成员/operator()。
	 *
	 * 用于打破"控件持有事件 → 事件持有处理器 → 处理器持有控件"的循环引用。
	 * T 必须能以 std::weak_ptr<T> 构造（即由 shared_ptr 管理）。返回的连接仍可用
	 * EventConnection 手动断开；target 销毁后处理器自动不再触发。
	 *
	 * 用法：event.SubscribeWeak(shared_from_this(), &MyClass::OnEvent);
	 */
	template<typename T, typename Method>
	EventConnection SubscribeWeak(const std::shared_ptr<T>& target, Method method) {
		std::weak_ptr<T> weakTarget = target;
		return Subscribe([weakTarget, method](auto&&... args) {
			if (auto strong = weakTarget.lock()) {
				(strong->*method)(std::forward<decltype(args)>(args)...);
			}
		});
	}

	/// 重载：针对可调用对象（lambda/函数对象）的弱订阅。
	template<typename T, typename Callable>
	EventConnection SubscribeWeak(const std::weak_ptr<T>& weakTarget, Callable&& callable) {
		return Subscribe([weakTarget, fn = std::forward<Callable>(callable)](auto&&... args) mutable {
			if (auto strong = weakTarget.lock()) {
				fn(*strong, std::forward<decltype(args)>(args)...);
			}
		});
	}

	template<typename F>
	void operator+=(F&& fn) {
		std_function_type func(std::forward<F>(fn));

		if constexpr (std::is_pointer_v<std::decay_t<F>>) {
			if (_state) for (const auto& entry : _state->Entries) {
				if (entry.Handler.template target<function_type*>() ==
					func.template target<function_type*>()) {
					return;
				}
			}
		}
		Add(std::move(func));
	}

	template<typename F>
	void operator-=(F&& fn) {
		// 退订只对"函数指针"有效：lambda / std::function / 函数对象无法被可靠
		// 匹配，过去这里是静默 no-op，导致"+= 了 lambda 之后 -= 不掉"的经典陷阱。
		// 现在改为编译期报错，引导改用 Subscribe() 返回的 EventConnection 来退订。
		static_assert(std::is_pointer_v<std::decay_t<F>>,
			"Event::operator-= only supports function pointers. "
			"For lambdas/functors, use EventConnection returned by Subscribe() to unsubscribe.");

		if (!_state || _state->Entries.empty()) return;

		std_function_type func(std::forward<F>(fn));

		if constexpr (std::is_pointer_v<std::decay_t<F>>) {
			auto it = std::remove_if(_state->Entries.begin(), _state->Entries.end(),
				[&](const Entry& entry) {
					return entry.Handler.template target<function_type*>() ==
						func.template target<function_type*>();
				});
			_state->Entries.erase(it, _state->Entries.end());
		}
	}

	size_t Count() const {
		return _state ? _state->Entries.size() : 0;
	}

	bool Empty() const {
		return !_state || _state->Entries.empty();
	}

};

class EventArgs {
public:
	EventArgs() = default;
	virtual ~EventArgs() = default;
};

/** Cancelable lifecycle payload shared by Closing-style events. */
class CancelEventArgs : public EventArgs {
public:
	bool Cancel = false;
};

/** WPF-compatible route direction shared by built-in and XAML-defined events. */
enum class RoutedEventRoutingStrategy : unsigned char
{
	Direct,
	Bubble,
	Tunnel,
};

/** WPF-compatible drag/drop result flags, independent from OLE constants. */
enum class DragDropEffects : std::uint32_t
{
	None = 0,
	Copy = 1,
	Move = 2,
	Link = 4,
	Scroll = 0x80000000u,
};

inline constexpr DragDropEffects operator|(
	DragDropEffects left, DragDropEffects right) noexcept
{
	return static_cast<DragDropEffects>(
		static_cast<std::uint32_t>(left)
		| static_cast<std::uint32_t>(right));
}

inline constexpr DragDropEffects operator&(
	DragDropEffects left, DragDropEffects right) noexcept
{
	return static_cast<DragDropEffects>(
		static_cast<std::uint32_t>(left)
		& static_cast<std::uint32_t>(right));
}

/** Modifier/button snapshot carried by WPF DragEventArgs. */
enum class DragDropKeyStates : std::uint32_t
{
	None = 0,
	LeftMouseButton = 1,
	RightMouseButton = 2,
	ShiftKey = 4,
	ControlKey = 8,
	MiddleMouseButton = 16,
	AltKey = 32,
};

/** Platform-neutral data projection for one external drag transaction. */
class DragDataObject final
{
public:
	DragDataObject() = default;
	DragDataObject(
		std::vector<std::wstring> files,
		std::optional<std::wstring> text = {})
		: _files(std::move(files)), _text(std::move(text)) {}

	[[nodiscard]] bool HasFiles() const noexcept { return !_files.empty(); }
	[[nodiscard]] bool HasText() const noexcept
	{
		return _text.has_value();
	}
	[[nodiscard]] const std::vector<std::wstring>& Files() const noexcept
	{
		return _files;
	}
	[[nodiscard]] const std::optional<std::wstring>& Text() const noexcept
	{
		return _text;
	}

private:
	std::vector<std::wstring> _files;
	std::optional<std::wstring> _text;
};

/** Stable framework identity for every built-in routed input event. */
enum class RoutedEventId : unsigned char
{
	None,
	PreviewMouseWheel,
	MouseWheel,
	PreviewMouseMove,
	MouseMove,
	PreviewMouseDown,
	MouseDown,
	PreviewMouseUp,
	MouseUp,
	PreviewMouseDoubleClick,
	MouseDoubleClick,
	Click,
	SizeChanged,
	TextChanged,
	PasswordChanged,
	ScrollChanged,
	SelectionChanged,
	SelectedDatesChanged,
	SelectedItemChanged,
	ValueChanged,
	Selected,
	Unselected,
	Checked,
	Unchecked,
	Expanded,
	Collapsed,
	SubmenuOpened,
	SubmenuClosed,
	MouseEnter,
	MouseLeave,
	GotMouseCapture,
	LostMouseCapture,
	PreviewKeyDown,
	KeyDown,
	PreviewKeyUp,
	KeyUp,
	PreviewTextInputStart,
	TextInputStart,
	PreviewTextInputUpdate,
	TextInputUpdate,
	PreviewTextInput,
	TextInput,
	PreviewGotKeyboardFocus,
	GotKeyboardFocus,
	PreviewLostKeyboardFocus,
	LostKeyboardFocus,
	GotFocus,
	LostFocus,
	PreviewCanExecute,
	CanExecute,
	PreviewExecuted,
	Executed,
	PreviewDragEnter,
	DragEnter,
	PreviewDragOver,
	DragOver,
	PreviewDragLeave,
	DragLeave,
	PreviewDrop,
	Drop,
	Count,
};

enum class RoutedInputDeviceKind : unsigned char
{
	None,
	Mouse,
	MouseCapture,
	Keyboard,
	Text,
	KeyboardFocus,
	Focus,
	Command,
	DragDrop,
};

enum class RoutedEventStage : unsigned char
{
	None,
	Preview,
	Bubble,
	Direct,
};

struct RoutedEventMetadata final
{
	RoutedEventId Id = RoutedEventId::None;
	const wchar_t* Name = L"";
	RoutedEventRoutingStrategy RoutingStrategy =
		RoutedEventRoutingStrategy::Direct;
	RoutedEventId PairedEvent = RoutedEventId::None;
	RoutedInputDeviceKind Device = RoutedInputDeviceKind::None;
	RoutedEventStage Stage = RoutedEventStage::None;
};

const RoutedEventMetadata& GetRoutedEventMetadata(
	RoutedEventId eventId) noexcept;

/** Shared mutable state carried through one immutable route snapshot. */
class RoutedEventArgs : public EventArgs
{
public:
	RoutedEventArgs() = default;
	RoutedEventArgs(const RoutedEventArgs&) = delete;
	RoutedEventArgs& operator=(const RoutedEventArgs&) = delete;
	RoutedEventArgs(RoutedEventArgs&&) noexcept = default;
	RoutedEventArgs& operator=(RoutedEventArgs&&) noexcept = default;

	RoutedEventId EventId = RoutedEventId::None;
	RoutedEventRoutingStrategy RoutingStrategy =
		RoutedEventRoutingStrategy::Direct;
	RoutedEventStage Stage = RoutedEventStage::None;
	Control* OriginalSource = nullptr;
	Control* Source = nullptr;
	Control* CurrentTarget = nullptr;
	bool Handled = false;
	std::uint64_t Sequence = 0;
};

/** WPF FrameworkElement.SizeChanged payload for its direct routed event. */
class SizeChangedEventArgs : public RoutedEventArgs
{
public:
	cui::core::Size PreviousSize;
	cui::core::Size NewSize;
	bool WidthChanged = false;
	bool HeightChanged = false;

	SizeChangedEventArgs() = default;
	SizeChangedEventArgs(
		cui::core::Size previousSize,
		cui::core::Size newSize) noexcept
		: PreviousSize(previousSize),
		NewSize(newSize),
		WidthChanged(previousSize.width != newSize.width),
		HeightChanged(previousSize.height != newSize.height) {}
};

/** WPF-style old/new payload used by range and selected-item events. */
template<typename TValue>
class RoutedPropertyChangedEventArgs : public RoutedEventArgs
{
public:
	TValue OldValue{};
	TValue NewValue{};

	RoutedPropertyChangedEventArgs() = default;
	RoutedPropertyChangedEventArgs(TValue oldValue, TValue newValue)
		: OldValue(std::move(oldValue)), NewValue(std::move(newValue)) {}
};

/** Item deltas for Selector and Calendar selection routes. */
class SelectionChangedEventArgs : public RoutedEventArgs
{
public:
	int OldIndex = -1;
	int NewIndex = -1;
	std::vector<BindingValue> RemovedItems;
	std::vector<BindingValue> AddedItems;

	SelectionChangedEventArgs() = default;
	SelectionChangedEventArgs(
		int oldIndex,
		int newIndex,
		std::vector<BindingValue> removedItems = {},
		std::vector<BindingValue> addedItems = {})
		: OldIndex(oldIndex),
		NewIndex(newIndex),
		RemovedItems(std::move(removedItems)),
		AddedItems(std::move(addedItems)) {}
};

/** WPF-style payload for TextBoxBase.TextChanged. */
class TextChangedEventArgs : public RoutedEventArgs
{
public:
	std::wstring OldText;
	std::wstring NewText;

	TextChangedEventArgs() = default;
	TextChangedEventArgs(std::wstring oldText, std::wstring newText)
		: OldText(std::move(oldText)), NewText(std::move(newText)) {}
};

/** WPF ScrollViewer.ScrollChanged snapshot and per-raise deltas. */
class ScrollChangedEventArgs : public RoutedEventArgs
{
public:
	double HorizontalOffset = 0.0;
	double HorizontalChange = 0.0;
	double VerticalOffset = 0.0;
	double VerticalChange = 0.0;
	double ExtentWidth = 0.0;
	double ExtentWidthChange = 0.0;
	double ExtentHeight = 0.0;
	double ExtentHeightChange = 0.0;
	double ViewportWidth = 0.0;
	double ViewportWidthChange = 0.0;
	double ViewportHeight = 0.0;
	double ViewportHeightChange = 0.0;
};

/** Old/new element pair shared by one keyboard-focus routed transition. */
class KeyboardFocusChangedEventArgs : public RoutedEventArgs
{
public:
	Control* OldFocus = nullptr;
	Control* NewFocus = nullptr;

	KeyboardFocusChangedEventArgs() = default;
	KeyboardFocusChangedEventArgs(Control* oldFocus, Control* newFocus)
		: OldFocus(oldFocus), NewFocus(newFocus) {}
};

class MouseEventArgs : public RoutedEventArgs {
public:
	MouseButton ChangedButton = MouseButton::None;
	MouseButtonState ButtonState = MouseButtonState::Released;
	MouseButtonStates ButtonStates;
	int ClickCount = 0;
	int WheelDelta = 0;
	int X = 0;
	int Y = 0;
	float RootX = 0.0f;
	float RootY = 0.0f;
	bool HasRootPosition = false;

	MouseEventArgs() = default;
	MouseEventArgs(
		MouseButton changedButton,
		MouseButtonState buttonState,
		int clickCount,
		int x,
		int y,
		int wheelDelta)
		: ChangedButton(changedButton), ButtonState(buttonState),
		ButtonStates(buttonState == MouseButtonState::Pressed
			? MouseButtonStates::WithPressed(changedButton)
			: MouseButtonStates{}),
		ClickCount(clickCount), WheelDelta(wheelDelta), X(x), Y(y) {}
	MouseEventArgs(
		MouseButton changedButton,
		MouseButtonState buttonState,
		MouseButtonStates buttonStates,
		int clickCount,
		int x,
		int y,
		int wheelDelta)
		: ChangedButton(changedButton), ButtonState(buttonState),
		ButtonStates(buttonStates), ClickCount(clickCount),
		WheelDelta(wheelDelta), X(x), Y(y) {}

	[[nodiscard]] bool IsButtonPressed(MouseButton button) const noexcept
	{
		return ButtonStates.IsPressed(button);
	}
};

/** One mutable WPF-style drag payload shared by preview and bubble handlers. */
class DragEventArgs : public RoutedEventArgs
{
public:
	std::shared_ptr<const DragDataObject> Data;
	DragDropKeyStates KeyStates = DragDropKeyStates::None;
	DragDropEffects AllowedEffects = DragDropEffects::None;
	DragDropEffects Effects = DragDropEffects::None;
	int X = 0;
	int Y = 0;
	float RootX = 0.0f;
	float RootY = 0.0f;
	bool HasRootPosition = false;

	DragEventArgs() = default;
	DragEventArgs(
		std::shared_ptr<const DragDataObject> data,
		DragDropKeyStates keyStates,
		DragDropEffects allowedEffects,
		float rootX,
		float rootY)
		: Data(std::move(data)),
		KeyStates(keyStates),
		AllowedEffects(allowedEffects),
		RootX(rootX), RootY(rootY), HasRootPosition(true) {}
};

class KeyEventArgs : public RoutedEventArgs {
public:
	::Key Key = ::Key::None;
	::Key SystemKey = ::Key::None;
	ModifierKeys Modifiers = ModifierKeys::None;
	bool IsRepeat = false;

	KeyEventArgs() = default;
	explicit KeyEventArgs(
		::Key key,
		ModifierKeys modifiers = ModifierKeys::None,
		::Key systemKey = ::Key::None)
		: Key(systemKey == ::Key::None ? key : ::Key::System),
		SystemKey(systemKey), Modifiers(modifiers) {}

	[[nodiscard]] bool HasModifier(ModifierKeys modifier) const noexcept
	{
		return ::HasModifier(Modifiers, modifier);
	}
};

/** Lifecycle state shared by one WPF-style text-composition transaction. */
enum class TextCompositionStage : unsigned char
{
	None,
	Started,
	Updated,
	Completed,
	Canceled,
};

/** Native source normalized by TextCompositionManager. */
enum class TextCompositionInputKind : unsigned char
{
	Keyboard,
	Unicode,
	Ime,
	System,
	Programmatic,
};

/** Reason why a composition ended without committed text. */
enum class TextCompositionCancelReason : unsigned char
{
	None,
	Explicit,
	FocusChanged,
	WindowDeactivated,
	SourceDetached,
	NativeCanceled,
	InvalidUnicode,
};

/**
 * WPF-style text-composition payload. Text is populated for committed input;
 * CompositionText carries the current pre-edit string during start/update.
 */
class TextCompositionEventArgs : public RoutedEventArgs {
public:
	std::wstring Text;
	std::wstring CompositionText;
	std::wstring SystemText;
	std::wstring ControlText;
	std::vector<unsigned char> CompositionAttributes;
	std::vector<std::uint32_t> CompositionClauses;
	TextCompositionStage CompositionStage = TextCompositionStage::None;
	TextCompositionInputKind InputKind = TextCompositionInputKind::Keyboard;
	TextCompositionCancelReason CancelReason =
		TextCompositionCancelReason::None;
	/** Platform-neutral modifier snapshot captured with this text report. */
	ModifierKeys Modifiers = ModifierKeys::None;
	std::uint64_t CompositionId = 0;
	int CaretIndex = -1;
	bool TextApplied = false;

	[[nodiscard]] bool HasModifier(ModifierKeys modifier) const noexcept
	{
		return ::HasModifier(Modifiers, modifier);
	}

	TextCompositionEventArgs() = default;
	explicit TextCompositionEventArgs(std::wstring text)
		: Text(std::move(text)),
		CompositionStage(TextCompositionStage::Completed) {}
	TextCompositionEventArgs(
		TextCompositionStage stage,
		std::wstring text,
		std::wstring compositionText,
		TextCompositionInputKind inputKind,
		std::uint64_t compositionId,
		int caretIndex = -1,
		TextCompositionCancelReason cancelReason =
			TextCompositionCancelReason::None)
		: Text(std::move(text)),
		CompositionText(std::move(compositionText)),
		CompositionStage(stage),
		InputKind(inputKind),
		CancelReason(cancelReason),
		CompositionId(compositionId),
		CaretIndex(caretIndex) {}
};

/** Counts instance handlers invoked/skipped while preserving Handled semantics. */
struct RoutedHandlerInvocationCount final
{
	std::size_t Invoked = 0;
	std::size_t Skipped = 0;
};

/** Implemented by InputManager; direct calls outside native input still route. */
bool RaiseRoutedEvent(
	UIElement& owner,
	RoutedEventId eventId,
	RoutedEventArgs& args);

/**
 * CLR-event-shaped facade over a WPF-style routed event.
 *
 * Existing `OnX.Subscribe(...)` syntax remains the authoring wrapper, while
 * invocation always enters the central route. Handlers receive one shared
 * args object by reference; handledEventsToo is per registration.
 */
template<typename TArgs>
class RoutedEvent final
{
	static_assert(std::is_base_of_v<RoutedEventArgs, TArgs>);

public:
	using function_type = void(Control*, TArgs&);
	using std_function_type = std::function<function_type>;

private:
	struct Entry final
	{
		std::size_t Token = 0;
		bool HandledEventsToo = false;
		std_function_type Handler;
	};

	struct State final
	{
		std::size_t NextToken = 1;
		std::vector<Entry> Entries;
	};

	UIElement* _owner = nullptr;
	RoutedEventId _eventId = RoutedEventId::None;
	std::shared_ptr<State> _state;

	template<typename>
	static constexpr bool AlwaysFalse = false;

	template<typename F>
	static std_function_type AdaptHandler(F&& fn)
	{
		using Callable = std::decay_t<F>;
		if constexpr (std::is_invocable_r_v<void, Callable&, Control*, TArgs&>)
		{
			return std_function_type(std::forward<F>(fn));
		}
		else if constexpr (std::is_same_v<TArgs, RoutedEventArgs>
			&& std::is_invocable_r_v<void, Callable&, Control*>)
		{
			return [callback = Callable(std::forward<F>(fn))](
				Control* sender, TArgs&) mutable { callback(sender); };
		}
		else
		{
			static_assert(AlwaysFalse<Callable>,
				"RoutedEvent handler has an incompatible signature");
		}
	}

	static void RemoveToken(State& state, std::size_t token)
	{
		if (token == 0) return;
		state.Entries.erase(std::remove_if(
			state.Entries.begin(), state.Entries.end(),
			[token](const Entry& entry) { return entry.Token == token; }),
			state.Entries.end());
	}

public:
	RoutedEvent() = default;
	RoutedEvent(UIElement* owner, RoutedEventId eventId) noexcept
		: _owner(owner), _eventId(eventId) {}

	RoutedEvent(const RoutedEvent&) = delete;
	RoutedEvent& operator=(const RoutedEvent&) = delete;
	RoutedEvent(RoutedEvent&&) = delete;
	RoutedEvent& operator=(RoutedEvent&&) = delete;

	RoutedEventId Id() const noexcept { return _eventId; }

	template<typename F>
	EventConnection Subscribe(F&& fn, bool handledEventsToo = false)
	{
		auto handler = AdaptHandler(std::forward<F>(fn));
		if (!handler) return {};
		if (!_state) _state = std::make_shared<State>();
		const auto token = _state->NextToken++;
		_state->Entries.push_back(Entry{
			token, handledEventsToo, std::move(handler) });
		std::weak_ptr<State> weakState = _state;
		return EventConnection([weakState, token]()
		{
			if (auto state = weakState.lock()) RemoveToken(*state, token);
		});
	}

	template<typename F>
	EventConnection SubscribeHandledEventsToo(F&& fn)
	{
		return Subscribe(std::forward<F>(fn), true);
	}

	template<typename F>
	void operator+=(F&& fn)
	{
		auto handler = AdaptHandler(std::forward<F>(fn));
		if (!handler) return;
		if (!_state) _state = std::make_shared<State>();
		const auto token = _state->NextToken++;
		_state->Entries.push_back(Entry{ token, false, std::move(handler) });
	}

	void operator()(Control*, TArgs& args)
	{
		if (_owner) (void)RaiseRoutedEvent(*_owner, _eventId, args);
	}

	void operator()(Control*, TArgs&& args)
	{
		if (_owner) (void)RaiseRoutedEvent(*_owner, _eventId, args);
	}

	void Invoke(Control* sender, TArgs& args) { (*this)(sender, args); }
	void Invoke(Control* sender, TArgs&& args) { (*this)(sender, std::move(args)); }

	template<typename U = TArgs>
	std::enable_if_t<std::is_default_constructible_v<U>, void>
	operator()(Control* sender)
	{
		U args;
		(*this)(sender, args);
	}


private:
    friend class UIElement;
    friend struct cui::framework::RoutedEventAccess;
	RoutedHandlerInvocationCount InvokeHandlers(
		Control* sender, TArgs& args)
	{
		RoutedHandlerInvocationCount result;
		if (!_state || _state->Entries.empty()) return result;
		const ControlWeakReference senderLifetime(sender);
		const auto snapshot = _state->Entries;
		for (const auto& entry : snapshot)
		{
			if (sender && !senderLifetime) break;
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

public:
	std::size_t Count() const noexcept
	{
		return _state ? _state->Entries.size() : 0;
	}

	bool Empty() const noexcept { return Count() == 0; }

};
