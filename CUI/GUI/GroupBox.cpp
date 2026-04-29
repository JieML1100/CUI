#include "GroupBox.h"
#include "Form.h"

#include <algorithm>

UIClass GroupBox::Type() { return UIClass::UI_GroupBox; }

GroupBox::GroupBox()
	: Panel()
{
	this->Text = L"GroupBox";
	this->BackColor = D2D1_COLOR_F{ 0, 0, 0, 0 };
	this->OnTextChanged += [&](Control* sender, std::wstring oldText, std::wstring newText)
		{
			(void)sender;
			(void)oldText;
			(void)newText;
			this->InvalidateLayout();
		};
}

GroupBox::GroupBox(std::wstring text, int x, int y, int width, int height)
	: GroupBox()
{
	this->Text = text;
	this->Location = POINT{ x, y };
	this->Size = SIZE{ width, height };
}

float GroupBox::GetCaptionBandHeight()
{
	auto font = this->Font;
	if (!font) return 20.0f;
	auto textSize = font->GetTextSize(this->Text);
	return (std::max)(20.0f, textSize.height + CaptionPaddingY * 2.0f);
}

void GroupBox::PerformGroupLayoutIfNeeded()
{
	if (!_needsLayout && !(_layoutEngine && _layoutEngine->NeedsLayout()))
		return;

	Thickness originalPadding = this->Padding;
	_padding.Top += GetCaptionBandHeight() * 0.5f + CaptionPaddingY + 6.0f;
	PerformLayout();
	_padding = originalPadding;
}

void GroupBox::Update()
{
	if (this->IsVisual == false) return;

	PerformGroupLayoutIfNeeded();

	auto d2d = this->ParentForm->Render;
	auto size = this->ActualSize();
	const float actualWidth = static_cast<float>(size.cx);
	const float actualHeight = static_cast<float>(size.cy);
	auto font = this->Font;
	float textWidth = 0.0f;
	if (font)
	{
		auto textSize = font->GetTextSize(this->Text);
		textWidth = textSize.width;
	}
	float captionBandHeight = GetCaptionBandHeight();
	float borderY = captionBandHeight * 0.5f;
	float gapLeft = CaptionMarginLeft;
	float gapRight = CaptionMarginLeft + textWidth + CaptionPaddingX * 2.0f;
	gapLeft = (std::max)(0.0f, gapLeft);
	gapRight = (std::min)(actualWidth, gapRight);

	this->BeginRender();
	{
		d2d->FillRect(0, 0, actualWidth, actualHeight, this->BackColor);
		if (this->Image)
		{
			this->RenderImage();
		}
		for (int i = 0; i < this->Count; i++)
		{
			auto c = this->operator[](i);
			c->Update();
		}

		if (!this->Text.empty())
		{
			d2d->FillRect(gapLeft, 0.0f, gapRight - gapLeft, captionBandHeight, this->BackColor);
			d2d->DrawString(this->Text, CaptionMarginLeft + CaptionPaddingX, CaptionPaddingY, this->ForeColor, font);
		}

		if (gapLeft > 0.0f)
			d2d->DrawLine(D2D1::Point2F(0.0f, borderY), D2D1::Point2F(gapLeft, borderY), this->BolderColor, this->Boder);
		if (gapRight < actualWidth)
			d2d->DrawLine(D2D1::Point2F(gapRight, borderY), D2D1::Point2F(actualWidth, borderY), this->BolderColor, this->Boder);
		d2d->DrawLine(D2D1::Point2F(0.0f, borderY), D2D1::Point2F(0.0f, actualHeight), this->BolderColor, this->Boder);
		d2d->DrawLine(D2D1::Point2F(actualWidth, borderY), D2D1::Point2F(actualWidth, actualHeight), this->BolderColor, this->Boder);
		d2d->DrawLine(D2D1::Point2F(0.0f, actualHeight), D2D1::Point2F(actualWidth, actualHeight), this->BolderColor, this->Boder);
	}
	if (!this->Enable)
	{
		d2d->FillRect(0, 0, actualWidth, actualHeight, { 1.0f ,1.0f ,1.0f ,0.5f });
	}
	this->EndRender();
}

bool GroupBox::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof)
{
	PerformGroupLayoutIfNeeded();
	return Panel::ProcessMessage(message, wParam, lParam, xof, yof);
}
