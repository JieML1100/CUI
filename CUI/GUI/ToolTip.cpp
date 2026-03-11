#include "ToolTip.h"
#include "Form.h"
#include <algorithm>

UIClass ToolTip::Type() { return UIClass::UI_ToolTip; }

ToolTip::ToolTip(std::wstring text)
{
	this->Text = text;
	this->BackColor = D2D1_COLOR_F{ 0, 0, 0, 0 };
	this->BolderColor = D2D1_COLOR_F{ 0, 0, 0, 0 };
	this->ForeColor = PopupTextColor;
	this->Enable = false;
	this->Visible = true;
	this->Location = POINT{ 0, 0 };
	this->Size = SIZE{ 0, 0 };
}

SIZE ToolTip::ActualSize()
{
	if (!this->ParentForm)
		return SIZE{ 0, 0 };
	return this->ParentForm->ClientSize;
}

POINT ToolTip::CalcPopupOrigin()
{
	POINT origin{ 0, 0 };
	if (!_target || !this->ParentForm)
		return origin;

	auto targetRect = _target->AbsRect;
	auto font = this->Font;
	auto textSize = font->GetTextSize(this->Text);
	float popupW = textSize.width + PaddingX * 2.0f;
	float popupH = textSize.height + PaddingY * 2.0f;
	float x = targetRect.left + (float)OffsetX;
	float y = targetRect.bottom + (float)OffsetY;
	float maxW = (float)this->ParentForm->ClientSize.cx;
	float maxH = (float)this->ParentForm->ClientSize.cy;
	if (x + popupW > maxW)
		x = (std::max)(0.0f, maxW - popupW - 8.0f);
	if (y + popupH > maxH)
		y = (std::max)(0.0f, targetRect.top - popupH - 6.0f);
	if (x < 0.0f) x = 0.0f;
	if (y < 0.0f) y = 0.0f;
	origin.x = (LONG)x;
	origin.y = (LONG)y;
	return origin;
}

void ToolTip::Update()
{
	if (!this->IsVisual || !_popupVisible || !this->ParentForm || !_target || !this->_target->IsVisual)
		return;

	auto d2d = this->ParentForm->Render;
	auto size = this->ActualSize();
	auto font = this->Font;
	auto textSize = font->GetTextSize(this->Text);
	float popupW = textSize.width + PaddingX * 2.0f;
	float popupH = textSize.height + PaddingY * 2.0f;
	POINT origin = CalcPopupOrigin();

	this->BeginRender((float)size.cx, (float)size.cy);
	{
		d2d->FillRoundRect((float)origin.x, (float)origin.y, popupW, popupH, PopupBackColor, 5.0f);
		d2d->DrawRoundRect((float)origin.x, (float)origin.y, popupW, popupH, PopupBorderColor, Border, 5.0f);
		d2d->DrawString(this->Text, (float)origin.x + PaddingX, (float)origin.y + PaddingY, PopupTextColor, font);
	}
	this->EndRender();
}

void ToolTip::Bind(class Control* target, const std::wstring& text)
{
	_target = target;
	this->Text = text;
	if (!_target)
		return;

	_target->OnMouseEnter += [this](class Control* sender, MouseEventArgs e)
		{
			(void)sender;
			(void)e;
			this->Show();
		};
	_target->OnMouseLeaved += [this](class Control* sender, MouseEventArgs e)
		{
			(void)sender;
			(void)e;
			this->Hide();
		};
	_target->OnMouseDown += [this](class Control* sender, MouseEventArgs e)
		{
			(void)sender;
			(void)e;
			this->Hide();
		};
	_target->OnMouseWheel += [this](class Control* sender, MouseEventArgs e)
		{
			(void)sender;
			(void)e;
			this->Hide();
		};
}

void ToolTip::Show()
{
	if (!_target || !_target->ParentForm || !_target->IsVisual)
		return;
	_popupVisible = true;
	if (this->ParentForm)
		this->ParentForm->Invalidate(false);
}

void ToolTip::Hide()
{
	if (!_popupVisible)
		return;
	_popupVisible = false;
	if (this->ParentForm)
		this->ParentForm->Invalidate(false);
}