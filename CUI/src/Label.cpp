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
	this->OnTextChanged += [&](Control* sender, std::wstring oldText, std::wstring newText) {
		(void)sender;
		(void)oldText;
		(void)newText;
		this->RequestLayout();
	};
}
cui::core::Size Label::MeasureCore(const cui::core::Constraints& available)
{
	(void)available;
	auto font = this->Font;
	auto textSize = font->GetTextSize(this->Text);
	return cui::core::Size{
		textSize.width + _padding.Left + _padding.Right,
		textSize.height + _padding.Top + _padding.Bottom };
}
void Label::Update()
{
	if (this->IsVisual == false)return;
	auto d2d = this->ParentForm->Render;
	const auto size = this->GetActualSizeDip();
	auto font = this->Font;
	float clipW = lastMeasuredWidth > size.width ? lastMeasuredWidth : FLT_MAX;
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
		float w = lastMeasuredWidth > size.width ? lastMeasuredWidth : size.width;
		d2d->FillRect(0, 0, w, size.height, { 1.0f ,1.0f ,1.0f ,0.5f });
	}
	this->EndRender();
	lastMeasuredWidth = size.width;
}
