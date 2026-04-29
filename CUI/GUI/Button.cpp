#include "Button.h"
#include "Form.h"
UIClass Button::Type() { return UIClass::UI_Button; }
Button::Button(std::wstring text, int x, int y, int width, int height)
{
	this->Text = text;
	this->Location = POINT{ x,y };
	this->Size = SIZE{ width,height };
	this->BackColor = Colors::Snow3;
	this->BolderColor = Colors::Snow4;
	this->Cursor = CursorKind::Hand;
}
void Button::Update()
{
	if (!this->IsVisual) return;
	bool isUnderMouse = this->ParentForm->UnderMouse == this;
	bool isSelected = this->ParentForm->Selected == this;
	auto d2d = this->ParentForm->Render;
	auto size = this->ActualSize();
	const float actualWidth = static_cast<float>(size.cx);
	const float actualHeight = static_cast<float>(size.cy);
	this->BeginRender();
	{
		float roundVal = this->Image ? 0.0f : this->Height * Round;
		d2d->FillRoundRect(this->Boder * 0.5f, this->Boder * 0.5f, actualWidth - this->Boder, actualHeight - this->Boder, this->BackColor, roundVal);
		if (this->Image)
			this->RenderImage();
		D2D1::ColorF color = isUnderMouse ? (isSelected ? D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.7f) : D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.4f)) : D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f);
		d2d->FillRoundRect(0, 0, actualWidth, actualHeight, color, roundVal);
		auto textSize = this->Font->GetTextSize(this->Text);
		float drawLeft = this->Width > textSize.width ? (this->Width - textSize.width) / 2.0f : 0.0f;
		float drawTop = this->Height > textSize.height ? (this->Height - textSize.height) / 2.0f : 0.0f;
		d2d->DrawString(this->Text, drawLeft, drawTop, this->ForeColor, this->Font);
		d2d->DrawRoundRect(this->Boder * 0.5f, this->Boder * 0.5f,
			actualWidth - this->Boder, actualHeight - this->Boder,
			this->BolderColor, this->Boder, roundVal);
	}

	if (!this->Enable)
		d2d->FillRect(0, 0, actualWidth, actualHeight, { 1.0f ,1.0f ,1.0f ,0.5f });
	this->EndRender();
}
