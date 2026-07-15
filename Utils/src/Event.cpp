#include "Event.h"

#pragma warning(disable: 4267)
#pragma warning(disable: 4244)
#pragma warning(disable: 4018)
MouseEventArgs::MouseEventArgs(MouseButtons button, int clicks, int x, int y, int delta) {
	this->Buttons = button;
	this->Clicks = clicks;
	this->Delta = delta;
	this->X = x;
	this->Y = y;
}
KeyEventArgs::KeyEventArgs(Keys keyData) {
	this->KeyData = keyData;
	this->EventHandled = false;
}
bool KeyEventArgs::Alt() {
	return (((int)this->KeyData & (int)Keys::Alt) != 0);
}
bool KeyEventArgs::Control() {
	return (((int)this->KeyData & (int)Keys::Control) != 0);
}
bool KeyEventArgs::Shift() {
	return (((int)this->KeyData & (int)Keys::Shift) != 0);
}
Keys KeyEventArgs::Modifiers() {
	return (Keys)((int)this->KeyData & (int)Keys::Modifiers);
}
Keys KeyEventArgs::KeyCode() {
	return (Keys)((int)this->KeyData & (int)Keys::KeyCode);
}
int KeyEventArgs::KeyValue() {
	return (int)this->KeyData;
}
KeyEventArgs KeyEventArgs::ProcessKeyEventArgs(MSG m) {
	return KeyEventArgs((Keys)(m.wParam | 0));
}
MouseButtons FromParamToMouseButtons(UINT message) {
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