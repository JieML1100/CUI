#pragma once
#include "Label.h"
#include "Form.h"
#pragma comment(lib, "Imm32.lib")
UIClass Label::Type() { return UIClass::UI_Label; }
Label::Label(std::wstring text, int x, int y)
{
	this->Text = text;
	this->Location = POINT{ x,y };
	this->BackColor = D2D1_COLOR_F{ .0f,.0f,.0f,.0f };
}
SIZE Label::ActualSize()
{
	auto d2d = this->ParentForm->Render;
	auto font = this->Font;
	auto text_size = font->GetTextSize(this->Text);
	return SIZE{ (int)text_size.width,(int)text_size.height };
}
void Label::Update()
{
	if (this->IsVisual == false)return;
	auto d2d = this->ParentForm->Render;
	auto size = this->ActualSize();
	auto font = this->Font;
	float clipW = last_width > size.cx ? (float)last_width : FLT_MAX;
	this->BeginRender(clipW, FLT_MAX);
	{
		if (this->Image)
		{
			this->RenderImage();
		}
		d2d->DrawString(this->Text, 0, 0, this->ForeColor, font);
	}
	if (!this->Enable)
	{
		float w = last_width > size.cx ? (float)last_width : (float)size.cx;
		d2d->FillRect(0, 0, w, size.cy, { 1.0f ,1.0f ,1.0f ,0.5f });
	}
	this->EndRender();
	last_width = size.cx;
}