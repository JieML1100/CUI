#pragma once
#define NOMINMAX
#include "RoundTextBox.h"
#include "TextBox.h"
#include "Form.h"
#pragma comment(lib, "Imm32.lib")
RoundTextBox::RoundTextBox(std::wstring text, int x, int y, int width, int height) :TextBox(text, x, y, width, height)
{
	this->TextMargin = this->Height * 0.5f;
}
void RoundTextBox::Update()
{
	if (!IsVisual) return;

	bool isUnderMouse = this->ParentForm->UnderMouse == this;
	auto d2d = this->ParentForm->Render;
	auto font = this->Font;
	float renderHeight = Height - (TextMargin * 2.0f);
	textSize = font->GetTextSize(Text, FLT_MAX, renderHeight);
	float textOffsetY = std::max((Height - textSize.height) * 0.5f, 0.0f);

	const auto size = GetActualSizeDip();
	const float actualWidth = size.width;
	const float actualHeight = size.height;
	bool isSelected = ParentForm->Selected == this;
	this->_caretRectCacheValid = false;
	bool shouldDrawCaret = false;
	D2D1_POINT_2F caretStart{};
	D2D1_POINT_2F caretEnd{};

	this->BeginRender();
	{
		auto backColor = this->BackColor;
		if (isUnderMouse || isSelected)
		{
			backColor.r = std::min(1.0f, backColor.r * 1.2f);
			backColor.g = std::min(1.0f, backColor.g * 1.2f);
			backColor.b = std::min(1.0f, backColor.b * 1.2f);
		}
		d2d->FillRoundRect(0, 0, actualWidth, actualHeight, backColor, TextMargin);
		RenderImage(TextMargin);
		// 内部文本裁剪：本地坐标下从 TextMargin 开始
		d2d->PushDrawRect(TextMargin, 0, actualWidth - TextMargin * 2.0f, actualHeight);

		if (Text.size() > 0)
		{

			if (isSelected)
			{
				int sels = SelectionStart <= SelectionEnd ? SelectionStart : SelectionEnd;
				int sele = SelectionEnd >= SelectionStart ? SelectionEnd : SelectionStart;
				int selLen = sele - sels;
				auto selRange = font->HitTestTextRange(this->Text, (UINT32)sels, (UINT32)selLen);
				for (auto sr : selRange)
				{
					d2d->FillRect(sr.left + TextMargin - HorizontalScrollOffset, sr.top + textOffsetY, sr.width, sr.height, this->SelectedBackColor);
				}
				if (selLen == 0 && !selRange.empty())
				{
					const auto caret = selRange[0];
					const float cx = caret.left + TextMargin - HorizontalScrollOffset;
					const float cy = caret.top + textOffsetY;
					const float ch = caret.height > 0 ? caret.height : font->FontHeight;
					const auto absoluteLocation = this->GetAbsoluteLocationDip();
					this->_caretRectCache = { static_cast<float>(absoluteLocation.x) + cx - 2.0f, static_cast<float>(absoluteLocation.y) + cy - 2.0f, static_cast<float>(absoluteLocation.x) + cx + 2.0f, static_cast<float>(absoluteLocation.y) + cy + ch + 2.0f };
					this->_caretRectCacheValid = true;
					shouldDrawCaret = true;
					caretStart = { selRange[0].left + TextMargin - HorizontalScrollOffset, selRange[0].top + textOffsetY };
					caretEnd = { selRange[0].left + TextMargin - HorizontalScrollOffset, selRange[0].top + selRange[0].height + textOffsetY };
				}
				auto textLayout = Factory::CreateStringLayout(this->Text, FLT_MAX, renderHeight, font->FontObject);
				if (textLayout) {
					d2d->DrawStringLayoutEffect(textLayout,
						TextMargin - HorizontalScrollOffset,
						textOffsetY,
						this->ForeColor,
						DWRITE_TEXT_RANGE{ (UINT32)sels, (UINT32)selLen },
						this->SelectedForeColor,
						font);
					textLayout->Release();
				}
			}
			else
			{
				auto textLayout = Factory::CreateStringLayout(this->Text, FLT_MAX, renderHeight, font->FontObject);
				if (textLayout) {
					d2d->DrawStringLayout(textLayout,
						TextMargin - HorizontalScrollOffset, textOffsetY,
						this->ForeColor);
					textLayout->Release();
				}
			}
		}
		else if (isSelected)
		{
			const float cx = (float)TextMargin - HorizontalScrollOffset;
			const float cy = textOffsetY;
			const float ch = (font->FontHeight > 16.0f) ? font->FontHeight : 16.0f;
			const auto absoluteLocation = this->GetAbsoluteLocationDip();
			this->_caretRectCache = { static_cast<float>(absoluteLocation.x) + cx - 2.0f, static_cast<float>(absoluteLocation.y) + cy - 2.0f, static_cast<float>(absoluteLocation.x) + cx + 2.0f, static_cast<float>(absoluteLocation.y) + cy + ch + 2.0f };
			this->_caretRectCacheValid = true;
			shouldDrawCaret = true;
			caretStart = { (float)TextMargin - HorizontalScrollOffset, textOffsetY };
			caretEnd = { (float)TextMargin - HorizontalScrollOffset, textOffsetY + 16.0f };
		}
		UpdateCaretBlinkState(isSelected, this->SelectionStart, this->SelectionEnd, this->_caretRectCacheValid, this->_caretRectCacheValid ? &this->_caretRectCache : nullptr);
		if (shouldDrawCaret && IsCaretBlinkVisible())
		{
			d2d->DrawLine(caretStart, caretEnd, this->ForeColor);
		}
		d2d->PopDrawRect();
	}

	if (!Enable)
	{
		d2d->FillRect(0, 0, actualWidth, actualHeight, { 1.0f ,1.0f ,1.0f ,0.5f });
	}

	this->EndRender();
}
