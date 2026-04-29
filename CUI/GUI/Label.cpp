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
SIZE Label::MeasureCore(SIZE availableSize)
{
	auto font = this->Font;
	auto text_size = font->GetTextSize(this->Text);
	SIZE desired = {
		(LONG)text_size.width,
		(LONG)text_size.height
	};

	desired.cx += (LONG)(_padding.Left + _padding.Right);
	desired.cy += (LONG)(_padding.Top + _padding.Bottom);

	if (desired.cx < _minSize.cx) desired.cx = _minSize.cx;
	if (desired.cy < _minSize.cy) desired.cy = _minSize.cy;
	if (desired.cx > _maxSize.cx) desired.cx = _maxSize.cx;
	if (desired.cy > _maxSize.cy) desired.cy = _maxSize.cy;
	if (desired.cx > availableSize.cx) desired.cx = availableSize.cx;
	if (desired.cy > availableSize.cy) desired.cy = availableSize.cy;

	return desired;
}
SIZE Label::ActualSize()
{
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
	float clipW = last_width > static_cast<float>(size.cx) ? last_width : FLT_MAX;
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
		float w = last_width > static_cast<float>(size.cx) ? last_width : static_cast<float>(size.cx);
		d2d->FillRect(0, 0, w, static_cast<float>(size.cy), { 1.0f ,1.0f ,1.0f ,0.5f });
	}
	this->EndRender();
	last_width = static_cast<float>(size.cx);
}
