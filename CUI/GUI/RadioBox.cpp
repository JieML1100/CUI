#pragma once
#include "RadioBox.h"
#include "Form.h"
UIClass RadioBox::Type() { return UIClass::UI_RadioBox; }
bool RadioBox::HandlesNavigationKey(WPARAM key) const
{
	return key == VK_SPACE || key == VK_RETURN;
}

RadioBox::RadioBox(std::wstring text, int x, int y)
{
	this->Text = text;
	this->Location = POINT{ x,y };
	this->BackColor = D2D1_COLOR_F{ 0.75f , 0.75f , 0.75f , 0.75f };
	this->Cursor = CursorKind::Hand;
}

void RadioBox::SetChecked(bool checked, bool fireEvent)
{
	if (this->Checked == checked) return;
	this->Checked = checked;
	if (fireEvent)
		this->OnChecked(this);
	this->PostRender();
}

SIZE RadioBox::ActualSize()
{
	auto d2d = this->ParentForm->Render;
	auto font = this->Font;
	auto text_size = font->GetTextSize(this->Text);
	return SIZE{ (int)text_size.width + int(text_size.height + 2),(int)text_size.height };
}
void RadioBox::Update()
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
		d2d->DrawEllipse(
			textSize.height * 0.5f, textSize.height * 0.5f,
			textSize.height * 0.3f, textSize.height * 0.3f,
			col);
		if (this->Checked)
		{
			d2d->FillEllipse(
				textSize.height * 0.5f, textSize.height * 0.5f,
				textSize.height * 0.2f, textSize.height * 0.2f,
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

bool RadioBox::DefaultRaiseMouseDoubleClick(UINT message, bool wasSelected) const
{
	(void)message;
	return wasSelected;
}

bool RadioBox::DefaultPostRenderOnMouseDoubleClick(UINT message, bool wasSelected) const
{
	(void)message;
	return wasSelected;
}

void RadioBox::BeforeDefaultMouseUp(UINT message, MouseEventArgs& e, bool wasSelected)
{
	(void)e;
	if (message == WM_LBUTTONUP && wasSelected && this->Checked == false)
	{
		this->SetChecked(true, true);
	}
}

void RadioBox::BeforeDefaultMouseDoubleClick(UINT message, MouseEventArgs& e, bool wasSelected)
{
	(void)e;
	if (message == WM_LBUTTONDBLCLK && wasSelected)
	{
		this->SetChecked(true, true);
	}
}

bool RadioBox::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof)
{
	if (message == WM_KEYDOWN && (wParam == VK_SPACE || wParam == VK_RETURN))
	{
		this->SetChecked(true, true);
		KeyEventArgs event_obj = KeyEventArgs((Keys)(wParam | 0));
		this->OnKeyDown(this, event_obj);
		return true;
	}
	return Control::ProcessMessage(message, wParam, lParam, xof, yof);
}
