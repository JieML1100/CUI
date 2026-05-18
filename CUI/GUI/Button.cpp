#include "Button.h"
#include "Form.h"
#include <algorithm>
UIClass Button::Type() { return UIClass::UI_Button; }
Button::Button(std::wstring text, int x, int y, int width, int height)
{
	this->Text = text;
	this->Location = POINT{ x,y };
	this->Size = SIZE{ width,height };
	this->BackColor = D2D1_COLOR_F{ 0.97f, 0.98f, 0.99f, 1.0f };
	this->BolderColor = D2D1_COLOR_F{ 0.70f, 0.76f, 0.86f, 1.0f };
	this->ForeColor = D2D1_COLOR_F{ 0.12f, 0.16f, 0.22f, 1.0f };
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
		const float border = (std::max)(0.0f, this->Boder);
		const bool hasSurface = this->BackColor.a > 0.0f || this->Checked;
		const bool raised = this->Raised;
		const float pressOffset = (raised && isSelected) ? 1.0f : 0.0f;
		const float surfaceX = border * 0.5f;
		const float surfaceY = border * 0.5f + pressOffset;
		const float surfaceW = (std::max)(0.0f, actualWidth - border);
		const float surfaceH = (std::max)(0.0f, actualHeight - border - pressOffset);
		const float roundVal = (std::min)((std::max)(0.0f, this->Round), (std::min)(surfaceW, surfaceH) * 0.5f);
		const auto baseColor = this->Checked ? this->CheckedColor : this->BackColor;
		if (raised && hasSurface && ShadowColor.a > 0.0f && !isSelected)
			d2d->FillRoundRect(surfaceX, surfaceY + 1.0f, surfaceW, surfaceH, ShadowColor, roundVal);
		if (hasSurface)
		{
			d2d->FillRoundRect(surfaceX, surfaceY, surfaceW, surfaceH, baseColor, roundVal);
			if (raised && HighlightColor.a > 0.0f)
			{
				const float highlightH = (std::max)(1.0f, surfaceH * 0.34f);
				d2d->FillRoundRect(surfaceX + 1.0f, surfaceY + 1.0f, (std::max)(0.0f, surfaceW - 2.0f), highlightH, HighlightColor, (std::min)(roundVal, highlightH * 0.5f));
			}
		}
		if (isUnderMouse && this->UnderMouseColor.a > 0.0f)
			d2d->FillRoundRect(surfaceX, surfaceY, surfaceW, surfaceH, isSelected ? this->CheckedColor : this->UnderMouseColor, roundVal);
		if (this->Image)
			this->RenderImage(roundVal);
		auto textSize = this->Font->GetTextSize(this->Text);
		const float horizontalPad = 6.0f;
		const float textWidth = (std::max)(1.0f, actualWidth - horizontalPad * 2.0f);
		float drawLeft = actualWidth > textSize.width ? (actualWidth - textSize.width) / 2.0f : horizontalPad;
		if (drawLeft < horizontalPad) drawLeft = horizontalPad;
		float drawTop = actualHeight > textSize.height ? (actualHeight - textSize.height) / 2.0f + pressOffset * 0.5f : 0.0f;
		d2d->DrawString(this->Text, drawLeft, drawTop, textWidth, textSize.height + 2.0f, this->ForeColor, this->Font);
		if (border > 0.0f && this->BolderColor.a > 0.0f)
		{
			d2d->DrawRoundRect(surfaceX, surfaceY,
				surfaceW, surfaceH,
				this->BolderColor, border, roundVal);
		}
	}

	if (!this->Enable)
		d2d->FillRoundRect(0.0f, 0.0f, actualWidth, actualHeight, DisabledOverlayColor, actualHeight * 0.28f);
	this->EndRender();
}
