#define NOMINMAX
#include "MessageDialog.h"
#include "Window.h"

namespace
{
	UINT NativeButtons(MessageDialogButtons buttons) noexcept
	{
		switch (buttons)
		{
		case MessageDialogButtons::OKCancel: return MB_OKCANCEL;
		case MessageDialogButtons::YesNo: return MB_YESNO;
		case MessageDialogButtons::YesNoCancel: return MB_YESNOCANCEL;
		case MessageDialogButtons::OK:
		default: return MB_OK;
		}
	}

	UINT NativeIcon(MessageDialogIcon icon) noexcept
	{
		switch (icon)
		{
		case MessageDialogIcon::Info:
		case MessageDialogIcon::Success: return MB_ICONINFORMATION;
		case MessageDialogIcon::Warning: return MB_ICONWARNING;
		case MessageDialogIcon::Error: return MB_ICONERROR;
		case MessageDialogIcon::Question: return MB_ICONQUESTION;
		case MessageDialogIcon::None:
		default: return 0;
		}
	}

	MessageDialogResult ResultFromNative(int result) noexcept
	{
		switch (result)
		{
		case IDOK: return MessageDialogResult::OK;
		case IDCANCEL: return MessageDialogResult::Cancel;
		case IDYES: return MessageDialogResult::Yes;
		case IDNO: return MessageDialogResult::No;
		default: return MessageDialogResult::Close;
		}
	}
}

MessageDialogResult MessageDialog::Show(
	const std::wstring& title,
	const std::wstring& message,
	MessageDialogButtons buttons,
	MessageDialogIcon icon,
	Window* owner)
{
	const HWND ownerHandle = owner ? owner->Handle : nullptr;
	const UINT flags = NativeButtons(buttons) | NativeIcon(icon)
		| MB_SETFOREGROUND;
	return ResultFromNative(::MessageBoxW(
		ownerHandle, message.c_str(), title.c_str(), flags));
}
