#pragma once
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
	float render_height = Height - (TextMargin * 2.0f);
	textSize = font->GetTextSize(Text, FLT_MAX, render_height);
	float OffsetY = std::max((Height - textSize.height) * 0.5f, 0.0f);

	auto size = ActualSize();
	bool isSelected = ParentForm->Selected == this;
	this->_caretRectCacheValid = false;

	this->BeginRender();
	{
		d2d->FillRoundRect(0, 0, size.cx, size.cy, isSelected ? FocusedColor : BackColor, TextMargin);
		RenderImage();
		// 内部文本裁剪：本地坐标下从 TextMargin 开始
		d2d->PushDrawRect(TextMargin, 0, (float)size.cx - TextMargin * 2.0f, (float)size.cy);

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
					d2d->FillRect(sr.left + TextMargin - OffsetX, sr.top + OffsetY, sr.width, sr.height, this->SelectedBackColor);
				}
				if (selLen == 0 && !selRange.empty())
				{
					const auto caret = selRange[0];
					const float cx = caret.left + TextMargin - OffsetX;
					const float cy = caret.top + OffsetY;
					const float ch = caret.height > 0 ? caret.height : font->FontHeight;
					auto abs = this->AbsLocation;
					this->_caretRectCache = { abs.x + cx - 2.0f, abs.y + cy - 2.0f, abs.x + cx + 2.0f, abs.y + cy + ch + 2.0f };
					this->_caretRectCacheValid = true;
				}
				if (selLen == 0 && !selRange.empty())
					d2d->DrawLine({ selRange[0].left + TextMargin - OffsetX, selRange[0].top + OffsetY },
						{ selRange[0].left + TextMargin - OffsetX, selRange[0].top + selRange[0].height + OffsetY }, Colors::Black);
				auto lot = Factory::CreateStringLayout(this->Text, FLT_MAX, render_height, font->FontObject);
				d2d->DrawStringLayoutEffect(lot,
					TextMargin - OffsetX,
					OffsetY,
					this->ForeColor,
					DWRITE_TEXT_RANGE{ (UINT32)sels, (UINT32)selLen },
					this->SelectedForeColor,
					font);
				lot->Release();
			}
			else
			{
				auto lot = Factory::CreateStringLayout(this->Text, FLT_MAX, render_height, font->FontObject);
				d2d->DrawStringLayout(lot,
					TextMargin - OffsetX, OffsetY,
					this->ForeColor);
				lot->Release();
			}
		}
		else if (isSelected)
		{
			const float cx = (float)TextMargin - OffsetX;
			const float cy = OffsetY;
			const float ch = (font->FontHeight > 16.0f) ? font->FontHeight : 16.0f;
			auto abs = this->AbsLocation;
			this->_caretRectCache = { abs.x + cx - 2.0f, abs.y + cy - 2.0f, abs.x + cx + 2.0f, abs.y + cy + ch + 2.0f };
			this->_caretRectCacheValid = true;
			d2d->DrawLine({ (float)TextMargin - OffsetX, OffsetY },
				{ (float)TextMargin - OffsetX, OffsetY + 16.0f }, Colors::Black);
		}
		d2d->PopDrawRect();
	}

	if (!Enable)
	{
		d2d->FillRect(0, 0, size.cx, size.cy, { 1.0f ,1.0f ,1.0f ,0.5f });
	}

	this->EndRender();
}