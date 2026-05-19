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
	const float border = (std::max)(0.0f, this->BorderThickness);
	const float radius = (std::clamp)(this->CornerRadius, 0.0f, (std::min)(actualWidth, actualHeight) * 0.5f);

	this->BeginRender();
	{
		if (radius > 0.0f)
			d2d->FillRoundRect(0, 0, actualWidth, actualHeight, this->BackColor, radius);
		else
			d2d->FillRect(0, 0, actualWidth, actualHeight, this->BackColor);
		if (this->Image)
		{
			this->RenderImage(radius);
		}
		if (!this->ParentForm || !this->ParentForm->IsDCompSceneRenderActive())
		{
			for (auto child : this->GetChildrenInZOrder())
			{
				if (!child || !child->Visible) continue;
				child->Update();
			}
		}

		if (border > 0.0f && this->BorderColor.a > 0.0f)
		{
			const float drawW = (std::max)(0.0f, actualWidth - border);
			const float drawH = (std::max)(0.0f, actualHeight - border);
			if (radius > 0.0f)
				d2d->DrawRoundRect(border * 0.5f, border * 0.5f, drawW, drawH, this->BorderColor, border, radius);
			else
				d2d->DrawRect(border * 0.5f, border * 0.5f, drawW, drawH, this->BorderColor, border);
		}

		if (!this->Text.empty())
		{
			const float captionH = (std::min)(captionBandHeight, actualHeight);
			const float maxCaptionW = (std::max)(0.0f, actualWidth - CaptionMarginLeft * 2.0f);
			const float captionW = (std::min)(maxCaptionW, textWidth + CaptionPaddingX * 2.0f);
			const float captionX = (std::min)(CaptionMarginLeft, (std::max)(0.0f, actualWidth - captionW));
			const float captionY = (std::min)(4.0f, (std::max)(0.0f, actualHeight - captionH));
			const float captionRadius = (std::clamp)(CaptionCornerRadius, 0.0f, captionH * 0.5f);
			const auto captionBack = CaptionBackColor.a > 0.0f ? CaptionBackColor : this->BackColor;
			if (captionBack.a > 0.0f)
				d2d->FillRoundRect(captionX, captionY, captionW, captionH, captionBack, captionRadius);
			if (CaptionBorderColor.a > 0.0f)
				d2d->DrawRoundRect(captionX + 0.5f, captionY + 0.5f, (std::max)(0.0f, captionW - 1.0f), (std::max)(0.0f, captionH - 1.0f), CaptionBorderColor, 1.0f, captionRadius);
			d2d->DrawString(this->Text, captionX + CaptionPaddingX, captionY + CaptionPaddingY, this->ForeColor, font);
		}
	}
	if (!this->Enable)
	{
		if (radius > 0.0f)
			d2d->FillRoundRect(0, 0, actualWidth, actualHeight, DisabledOverlayColor, radius);
		else
			d2d->FillRect(0, 0, actualWidth, actualHeight, DisabledOverlayColor);
	}
	this->EndRender();
}

bool GroupBox::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY)
{
	PerformGroupLayoutIfNeeded();
	return Panel::ProcessMessage(message, wParam, lParam, localX, localY);
}
