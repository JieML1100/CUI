#pragma once
#include <string>

class Window;

enum class MessageDialogButtons
{
	OK,
	OKCancel,
	YesNo,
	YesNoCancel
};

enum class MessageDialogIcon
{
	None,
	Info,
	Success,
	Warning,
	Error,
	Question
};

enum class MessageDialogResult
{
	None,
	OK,
	Cancel,
	Yes,
	No,
	Close
};

/** Native modal message service; it is not a Control or an authored XAML type. */
class MessageDialog final
{
public:
	static MessageDialogResult Show(const std::wstring& title, const std::wstring& message,
		MessageDialogButtons buttons = MessageDialogButtons::OK,
		MessageDialogIcon icon = MessageDialogIcon::Info,
		Window* owner = nullptr);
};
