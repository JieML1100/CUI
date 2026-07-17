#define NOMINMAX
#include "MessageDialog.h"

#include <algorithm>
#include <utility>

namespace
{
	static D2D1_COLOR_F DialogAccent(MessageDialogIcon icon)
	{
		switch (icon)
		{
		case MessageDialogIcon::Success: return D2D1_COLOR_F{ 0.10f, 0.68f, 0.48f, 1.0f };
		case MessageDialogIcon::Warning: return D2D1_COLOR_F{ 0.95f, 0.62f, 0.18f, 1.0f };
		case MessageDialogIcon::Error: return D2D1_COLOR_F{ 0.90f, 0.20f, 0.24f, 1.0f };
		case MessageDialogIcon::Question: return D2D1_COLOR_F{ 0.46f, 0.38f, 0.88f, 1.0f };
		case MessageDialogIcon::Info:
		default: return D2D1_COLOR_F{ 0.20f, 0.52f, 0.95f, 1.0f };
		}
	}

	static std::wstring IconText(MessageDialogIcon icon)
	{
		switch (icon)
		{
		case MessageDialogIcon::Success: return L"OK";
		case MessageDialogIcon::Warning: return L"!";
		case MessageDialogIcon::Error: return L"X";
		case MessageDialogIcon::Question: return L"?";
		case MessageDialogIcon::Info: return L"i";
		default: return L"";
		}
	}
}

MessageDialog::MessageDialog(std::wstring title, std::wstring message, MessageDialogButtons buttons, MessageDialogIcon icon)
	: Form(title, POINT{ 0,0 }, SIZE{ 460, 230 })
{
	this->Message = std::move(message);
	this->Buttons = buttons;
	this->Icon = icon;
	this->AllowResize = false;
	this->MinBox = false;
	this->MaxBox = false;
	this->ShowInTaskBar = false;
	this->CenterTitle = false;
	this->BackColor = cui::theme::palette::Window;
	this->ForeColor = cui::theme::palette::TextPrimary;
	BuildContent();
}

void MessageDialog::BuildContent()
{
	const auto accent = DialogAccent(this->Icon);
	auto iconText = IconText(this->Icon);
	if (!iconText.empty())
	{
		auto badge = this->AddControl(new Button(iconText, 24, 54, 46, 46));
		badge->BackColor = D2D1_COLOR_F{ accent.r, accent.g, accent.b, 0.16f };
		badge->ForeColor = accent;
		badge->BorderColor = D2D1_COLOR_F{ accent.r, accent.g, accent.b, 0.32f };
		badge->Round = 23.0f;
	}

	auto title = this->AddControl(new Label(this->Text, 88, 48));
	title->Font = new Font(L"Arial", 20.0f);
	title->ForeColor = this->ForeColor;

	auto message = this->AddControl(new RichTextBox(this->Message, 88, 82, 340, 70));
	message->ReadOnly = true;
	message->BackColor = D2D1_COLOR_F{ 1, 1, 1, 0 };
	message->BorderColor = D2D1_COLOR_F{ 1, 1, 1, 0 };
	message->ForeColor = D2D1_COLOR_F{ 0.20f, 0.23f, 0.30f, 1.0f };

	std::vector<std::pair<std::wstring, MessageDialogResult>> specs;
	switch (this->Buttons)
	{
	case MessageDialogButtons::OKCancel:
		specs = { { L"确定", MessageDialogResult::OK }, { L"取消", MessageDialogResult::Cancel } };
		break;
	case MessageDialogButtons::YesNo:
		specs = { { L"是", MessageDialogResult::Yes }, { L"否", MessageDialogResult::No } };
		break;
	case MessageDialogButtons::YesNoCancel:
		specs = { { L"是", MessageDialogResult::Yes }, { L"否", MessageDialogResult::No }, { L"取消", MessageDialogResult::Cancel } };
		break;
	case MessageDialogButtons::OK:
	default:
		specs = { { L"确定", MessageDialogResult::OK } };
		break;
	}

	const int buttonW = 88;
	const int gap = 10;
	const int totalW = (int)specs.size() * buttonW + ((int)specs.size() - 1) * gap;
	int x = this->Size.cx - totalW - 24;
	for (size_t i = 0; i < specs.size(); i++)
	{
		auto btn = this->AddControl(new Button(specs[i].first, x + (int)i * (buttonW + gap), 170, buttonW, 32));
		btn->Round = 6.0f;
		if (i == 0)
		{
			btn->BackColor = accent;
			btn->ForeColor = Colors::White;
			btn->BorderColor = accent;
		}
		MessageDialogResult result = specs[i].second;
		btn->OnMouseClick += [this, result](Control*, MouseEventArgs) { Finish(result); };
	}

	this->OnClosing += [this](Form*, bool&) {
		if (this->Result == MessageDialogResult::None)
			this->Result = MessageDialogResult::Close;
		};
}

void MessageDialog::Finish(MessageDialogResult result)
{
	this->Result = result;
	this->Close();
}

MessageDialogResult MessageDialog::ShowModal(HWND parent)
{
	this->ShowDialog(parent);
	return this->Result;
}

MessageDialogResult MessageDialog::Show(const std::wstring& title, const std::wstring& message,
	MessageDialogButtons buttons, MessageDialogIcon icon, HWND parent)
{
	MessageDialog dialog(title, message, buttons, icon);
	return dialog.ShowModal(parent);
}
