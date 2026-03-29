#pragma once
#include <Windows.h>
#include <memory>
#include <functional>
#include <string>
#include <vector>
#include <algorithm>
#include <type_traits>

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
	Return = 0x0000000D,
	Enter = 0x0000000D,
	ShiftKey = 0x00000010,
	ControlKey = 0x00000011,
	Menu = 0x00000012,
	Escape = 0x0000001B,
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
	Insert = 0x0000002D,
	Delete = 0x0000002E,
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
	KeyCode = 0x0000FFFF,
	Shift = 0x00010000,
	Control = 0x00020000,
	Alt = 0x00040000,
	Modifiers = ((int)0xFFFF0000)
};

template<typename Func>
class Event {
public:
	using function_type = typename std::remove_pointer<Func>::type;
	using std_function_type = std::function<function_type>;

private:
	std::unique_ptr<std::vector<std_function_type>> _events;

public:
	template <typename... Args>
	void Invoke(Args&&... args) {
		if (!_events) return;
		for (auto& event : *_events) event(std::forward<Args>(args)...);
	}

	template<typename F>
	void operator+=(F&& fn) {
		if (!_events) _events = std::make_unique<std::vector<std_function_type>>();
		_events->push_back(std_function_type(std::forward<F>(fn)));
	}

	template <typename... Args>
	void operator()(Args&&... args) {
		Invoke(std::forward<Args>(args)...);
	}

	size_t Count() const { return _events ? _events->size() : 0; }
	void Clear() { _events.reset(); }
};

class EventArgs {
public:
	EventArgs() {}
};

class MouseEventArgs : public EventArgs {
public:
	MouseButtons Buttons;
	int Clicks;
	int Delta;
	int X;
	int Y;
	MouseEventArgs() {}
	MouseEventArgs(MouseButtons button, int clicks, int x, int y, int delta)
		: Buttons(button), Clicks(clicks), Delta(delta), X(x), Y(y) {}
};

class KeyEventArgs : EventArgs {
public:
	Keys KeyData;
	bool EventHandled;
	bool SupressKeyPress;
	KeyEventArgs() : KeyData(Keys::None), EventHandled(false), SupressKeyPress(false) {}
	KeyEventArgs(Keys keyData) : KeyData(keyData), EventHandled(false), SupressKeyPress(false) {}
};

static MouseButtons FromParamToMouseButtons(UINT message) {
	switch (message) {
	case WM_MOUSEWHEEL: return MouseButtons::Middle;
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_LBUTTONDBLCLK: return MouseButtons::Left;
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP: return MouseButtons::Right;
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP: return MouseButtons::Middle;
	default: return MouseButtons::None;
	}
}
