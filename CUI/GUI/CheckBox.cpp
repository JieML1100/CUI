#pragma once
#include "CheckBox.h"
#include "Form.h"
UIClass CheckBox::Type() { return UIClass::UI_CheckBox; }
D2D1_COLOR_F UnderMouseColor = Colors::DarkSlateGray;
bool CheckBox::HandlesNavigationKey(WPARAM key) const
{
	return key == VK_SPACE || key == VK_RETURN;
}

CheckBox::CheckBox(std::wstring text, int x, int y)
{
	this->Text = text;
	this->Location = POINT{ x,y };
	this->BackColor = D2D1_COLOR_F{ 0.75f , 0.75f , 0.75f , 0.75f };
	this->Cursor = CursorKind::Hand;
}

void CheckBox::SetChecked(bool checked, bool fireEvent)
{
	if (this->Checked == checked) return;
	this->Checked = checked;
	if (fireEvent)
		this->OnChecked(this);
	this->PostRender();
}

void CheckBox::Toggle(bool fireEvent)
{
	this->SetChecked(!this->Checked, fireEvent);
}

SIZE CheckBox::ActualSize()
{
	auto d2d = this->ParentForm->Render;
	auto font = this->Font;
	auto text_size = font->GetTextSize(this->Text);
	return SIZE{ (int)text_size.width + int(text_size.height + 2),(int)text_size.height };
}
void CheckBox::Update()
{
	if (this->IsVisual == false)return;
	bool isUnderMouse = this->ParentForm->UnderMouse == this;
	auto d2d = this->ParentForm->Render;
	auto size = this->ActualSize();
	float clipW = last_width > size.cx ? (float)last_width : (float)size.cx;
	this->BeginRender(clipW, (float)size.cy);
	{
		auto col = this->ForeColor;
		if (isUnderMouse)
		{
			col = UnderMouseColor;
		}
		auto font = this->Font;
		auto textSize = font->GetTextSize(this->Text);
		d2d->DrawString(this->Text, textSize.height + 2, 0, col, font);
		d2d->DrawRect(
			textSize.height * 0.2f, textSize.height * 0.2f,
			textSize.height * 0.6f, textSize.height * 0.6f,
			col);
		if (this->Checked)
		{
			d2d->FillRect(
				textSize.height * 0.35f, textSize.height * 0.35f,
				textSize.height * 0.3f, textSize.height * 0.3f,
				col);
		}
	}
	if (!this->Enable)
	{
		d2d->FillRect(0, 0, clipW, (float)size.cy, { 1.0f ,1.0f ,1.0f ,0.5f });
	}
	this->EndRender();
	last_width = static_cast<float>(size.cx);
}

bool CheckBox::DefaultRaiseMouseDoubleClick(UINT message, bool wasSelected) const
{
	(void)message;
	return wasSelected;
}

bool CheckBox::DefaultPostRenderOnMouseDoubleClick(UINT message, bool wasSelected) const
{
	(void)message;
	return wasSelected;
}

void CheckBox::BeforeDefaultMouseUp(UINT message, MouseEventArgs& e, bool wasSelected)
{
	(void)e;
	if (message == WM_LBUTTONUP && wasSelected)
	{
		this->Toggle(true);
	}
}

void CheckBox::BeforeDefaultMouseDoubleClick(UINT message, MouseEventArgs& e, bool wasSelected)
{
	(void)e;
	if (message == WM_LBUTTONDBLCLK && wasSelected)
	{
		this->Toggle(true);
	}
}

bool CheckBox::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof)
{
	if (message == WM_KEYDOWN && (wParam == VK_SPACE || wParam == VK_RETURN))
	{
		this->Toggle(true);
		KeyEventArgs event_obj = KeyEventArgs((Keys)(wParam | 0));
		this->OnKeyDown(this, event_obj);
		return true;
	}
	return Control::ProcessMessage(message, wParam, lParam, xof, yof);
}
