#pragma once
#include "Core/EventConnection.h"
#include <Windows.h>
#include <algorithm>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

enum class MouseButtons {
	None = 0x00000000,
	Left = 0x00100000,
	Right = 0x00200000,
	Middle = 0x00400000,
	XButton1 = 0x00800000,
	XButton2 = 0x01000000
};
enum class Keys {
	None = 0x00000000,
	LButton = 0x00000001,
	RButton = 0x00000002,
	Cancel = 0x00000003,
	MButton = 0x00000004,
	XButton1 = 0x00000005,
	XButton2 = 0x00000006,
	Back = 0x00000008,
	Tab = 0x00000009,
	LineFeed = 0x0000000A,
	Clear = 0x0000000C,
	Return = 0x0000000D,
	Enter = 0x0000000D,
	ShiftKey = 0x00000010,
	ControlKey = 0x00000011,
	Menu = 0x00000012,
	Pause = 0x00000013,
	CapsLock = 0x00000014,
	Capital = 0x00000014,
	KanaMode = 0x00000015,
	HanguelMode = 0x00000015,
	HangulMode = 0x00000015,
	JunjaMode = 0x00000017,
	FinalMode = 0x00000018,
	KanjiMode = 0x00000019,
	HanjaMode = 0x00000019,
	Escape = 0x0000001B,
	IMEConvert = 0x0000001C,
	IMENonconvert = 0x0000001D,
	IMEAceept = 0x0000001E,
	IMEModeChange = 0x0000001F,
	Space = 0x00000020,
	PageUp = 0x00000021,
	Prior = 0x00000021,
	PageDown = 0x00000022,
	Next = 0x00000022,
	End = 0x00000023,
	Home = 0x00000024,
	Left = 0x00000025,
	Up = 0x00000026,
	Right = 0x00000027,
	Down = 0x00000028,
	Select = 0x00000029,
	Print = 0x0000002A,
	Execute = 0x0000002B,
	PrintScreen = 0x0000002C,
	Snapshot = 0x0000002C,
	Insert = 0x0000002D,
	Delete = 0x0000002E,
	Help = 0x0000002F,
	D0 = 0x00000030,
	D1 = 0x00000031,
	D2 = 0x00000032,
	D3 = 0x00000033,
	D4 = 0x00000034,
	D5 = 0x00000035,
	D6 = 0x00000036,
	D7 = 0x00000037,
	D8 = 0x00000038,
	D9 = 0x00000039,
	A = 0x00000041,
	B = 0x00000042,
	C = 0x00000043,
	D = 0x00000044,
	E = 0x00000045,
	F = 0x00000046,
	G = 0x00000047,
	H = 0x00000048,
	I = 0x00000049,
	J = 0x0000004A,
	K = 0x0000004B,
	L = 0x0000004C,
	M = 0x0000004D,
	N = 0x0000004E,
	O = 0x0000004F,
	P = 0x00000050,
	Q = 0x00000051,
	R = 0x00000052,
	S = 0x00000053,
	T = 0x00000054,
	U = 0x00000055,
	V = 0x00000056,
	W = 0x00000057,
	X = 0x00000058,
	Y = 0x00000059,
	Z = 0x0000005A,
	LWin = 0x0000005B,
	RWin = 0x0000005C,
	Apps = 0x0000005D,
	NumPad0 = 0x00000060,
	NumPad1 = 0x00000061,
	NumPad2 = 0x00000062,
	NumPad3 = 0x00000063,
	NumPad4 = 0x00000064,
	NumPad5 = 0x00000065,
	NumPad6 = 0x00000066,
	NumPad7 = 0x00000067,
	NumPad8 = 0x00000068,
	NumPad9 = 0x00000069,
	Multiply = 0x0000006A,
	Add = 0x0000006B,
	Separator = 0x0000006C,
	Subtract = 0x0000006D,
	Decimal = 0x0000006E,
	Divide = 0x0000006F,
	F1 = 0x00000070,
	F2 = 0x00000071,
	F3 = 0x00000072,
	F4 = 0x00000073,
	F5 = 0x00000074,
	F6 = 0x00000075,
	F7 = 0x00000076,
	F8 = 0x00000077,
	F9 = 0x00000078,
	F10 = 0x00000079,
	F11 = 0x0000007A,
	F12 = 0x0000007B,
	F13 = 0x0000007C,
	F14 = 0x0000007D,
	F15 = 0x0000007E,
	F16 = 0x0000007F,
	F17 = 0x00000080,
	F18 = 0x00000081,
	F19 = 0x00000082,
	F20 = 0x00000083,
	F21 = 0x00000084,
	F22 = 0x00000085,
	F23 = 0x00000086,
	F24 = 0x00000087,
	NumLock = 0x00000090,
	Scroll = 0x00000091,
	LShiftKey = 0x000000A0,
	RShiftKey = 0x000000A1,
	LControlKey = 0x000000A2,
	RControlKey = 0x000000A3,
	LMenu = 0x000000A4,
	RMenu = 0x000000A5,
	BrowserBack = 0x000000A6,
	BrowserForward = 0x000000A7,
	BrowserRefresh = 0x000000A8,
	BrowserStop = 0x000000A9,
	BrowserSearch = 0x000000AA,
	BrowserFavorites = 0x000000AB,
	BrowserHome = 0x000000AC,
	VolumeMute = 0x000000AD,
	VolumeDown = 0x000000AE,
	VolumeUp = 0x000000AF,
	MediaNextTrack = 0x000000B0,
	MediaPreviousTrack = 0x000000B1,
	MediaStop = 0x000000B2,
	MediaPlayPause = 0x000000B3,
	LaunchMail = 0x000000B4,
	SelectMedia = 0x000000B5,
	LaunchApplication1 = 0x000000B6,
	LaunchApplication2 = 0x000000B7,
	OemSemicolon = 0x000000BA,
	Oemplus = 0x000000BB,
	Oemcomma = 0x000000BC,
	OemMinus = 0x000000BD,
	OemPeriod = 0x000000BE,
	OemQuestion = 0x000000BF,
	Oemtilde = 0x000000C0,
	OemOpenBrackets = 0x000000DB,
	OemPipe = 0x000000DC,
	OemCloseBrackets = 0x000000DD,
	OemQuotes = 0x000000DE,
	Oem8 = 0x000000DF,
	OemBackslash = 0x000000E2,
	ProcessKey = 0x000000E5,
	Attn = 0x000000F6,
	Crsel = 0x000000F7,
	Exsel = 0x000000F8,
	EraseEof = 0x000000F9,
	Play = 0x000000FA,
	Zoom = 0x000000FB,
	NoName = 0x000000FC,
	Pa1 = 0x000000FD,
	OemClear = 0x000000FE,
	KeyCode = 0x0000FFFF,
	Shift = 0x00010000,
	Control = 0x00020000,
	Alt = 0x00040000,
	Modifiers = ((int)0xFFFF0000),
	IMEAccept = 0x0000001E,
	Oem1 = 0x000000BA,
	Oem102 = 0x000000E2,
	Oem2 = 0x000000BF,
	Oem3 = 0x000000C0,
	Oem4 = 0x000000DB,
	Oem5 = 0x000000DC,
	Oem6 = 0x000000DD,
	Oem7 = 0x000000DE,
	Packet = 0x000000E7,
	Sleep = 0x0000005F
};
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

public:
	Event() = default;
	~Event() = default;

	Event(const Event&) = delete;
	Event& operator=(const Event&) = delete;
	Event(Event&&) = default;
	Event& operator=(Event&&) = default;

	template <typename... Args>
	void Invoke(Args&&... args) {
		if (!_state || _state->Entries.empty()) return;
		const auto snapshot = _state->Entries;
		for (const auto& entry : snapshot) {
			if (entry.Handler) entry.Handler(std::forward<Args>(args)...);
		}
	}

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

	template <typename... Args>
	void operator()(Args&&... args) {
		Invoke(std::forward<Args>(args)...);
	}

	size_t Count() const {
		return _state ? _state->Entries.size() : 0;
	}

	bool Empty() const {
		return !_state || _state->Entries.empty();
	}

	void Clear() {
		if (_state) _state->Entries.clear();
	}
};
class EventArgs {
public:
	EventArgs() {}
};
class MouseEventArgs : public EventArgs {
public:
	MouseButtons	Buttons;
	int		Clicks;
	int		Delta;
	int		X;
	int		Y;
	MouseEventArgs() {}
	MouseEventArgs(MouseButtons button, int clicks, int x, int y, int delta) {
		this->Buttons = button;
		this->Clicks = clicks;
		this->Delta = delta;
		this->X = x;
		this->Y = y;
	}
};
class KeyEventArgs : EventArgs {
public:
	Keys	KeyData;
	bool	EventHandled;
	bool	SupressKeyPress;
	KeyEventArgs() {}
	KeyEventArgs(Keys keyData) {
		this->KeyData = keyData;
		this->EventHandled = false;
	}
	bool Alt() {
		return (((int)this->KeyData & (int)Keys::Alt) != 0);
	}
	bool Control() {
		return (((int)this->KeyData & (int)Keys::Control) != 0);
	}
	bool Shift() {
		return (((int)this->KeyData & (int)Keys::Shift) != 0);
	}
	Keys Modifiers() {
		return (Keys)((int)this->KeyData & (int)Keys::Modifiers);
	}
	Keys KeyCode() {
		return (Keys)((int)this->KeyData & (int)Keys::KeyCode);
	}
	int KeyValue() {
		return (int)this->KeyData;
	}
	KeyEventArgs ProcessKeyEventArgs(MSG m) {
		return KeyEventArgs((Keys)(m.wParam | 0));
	}
};
static MouseButtons FromParamToMouseButtons(UINT message) {
	switch (message) {
	case WM_MOUSEWHEEL: return MouseButtons::Middle;
	case WM_LBUTTONDOWN:return MouseButtons::Left;
	case WM_RBUTTONDOWN:return MouseButtons::Right;
	case WM_MBUTTONDOWN:return MouseButtons::Middle;
	case WM_LBUTTONUP:return MouseButtons::Left;
	case WM_RBUTTONUP:return MouseButtons::Right;
	case WM_MBUTTONUP:return MouseButtons::Middle;
	case WM_LBUTTONDBLCLK:return MouseButtons::Left;
	}
	return MouseButtons::None;
}
