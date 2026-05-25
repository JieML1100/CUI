#pragma once
#include "Form.h"

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

class MessageDialog : public Form
{
public:
	MessageDialog(std::wstring title, std::wstring message,
		MessageDialogButtons buttons = MessageDialogButtons::OK,
		MessageDialogIcon icon = MessageDialogIcon::Info);

	MessageDialogResult Result = MessageDialogResult::None;
	std::wstring Message;
	MessageDialogButtons Buttons = MessageDialogButtons::OK;
	MessageDialogIcon Icon = MessageDialogIcon::Info;

	MessageDialogResult ShowModal(HWND parent = nullptr);

	static MessageDialogResult Show(const std::wstring& title, const std::wstring& message,
		MessageDialogButtons buttons = MessageDialogButtons::OK,
		MessageDialogIcon icon = MessageDialogIcon::Info,
		HWND parent = nullptr);

private:
	void BuildContent();
	void Finish(MessageDialogResult result);
};
