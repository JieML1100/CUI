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

	auto abslocation = AbsLocation;
	auto size = ActualSize();
	auto absRect = AbsRect;
	bool isSelected = ParentForm->Selected == this;
	this->_caretRectCacheValid = false;

	d2d->PushDrawRect(absRect.left, absRect.top, absRect.right - absRect.left, absRect.bottom - absRect.top);
	{
		d2d->FillRoundRect(abslocation.x, abslocation.y, size.cx, size.cy, isSelected ? FocusedColor : BackColor, TextMargin);
		RenderImage();
		d2d->PushDrawRect(absRect.left + TextMargin, absRect.top, (absRect.right - absRect.left) - (TextMargin * 2.0f), (absRect.bottom - absRect.top));

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
					d2d->FillRect(sr.left + abslocation.x + TextMargin - OffsetX, (sr.top + abslocation.y) + OffsetY, sr.width, sr.height, this->SelectedBackColor);
				}
				if (selLen == 0 && !selRange.empty())
				{
					const auto caret = selRange[0];
					const float cx = caret.left + (float)abslocation.x + TextMargin - OffsetX;
					const float cy = caret.top + (float)abslocation.y + OffsetY;
					const float ch = caret.height > 0 ? caret.height : font->FontHeight;
					this->_caretRectCache = { cx - 2.0f, cy - 2.0f, cx + 2.0f, cy + ch + 2.0f };
					this->_caretRectCacheValid = true;
				}
				if (selLen == 0 && !selRange.empty())
					d2d->DrawLine({ selRange[0].left + abslocation.x + TextMargin - OffsetX,(selRange[0].top + abslocation.y) + OffsetY },
						{ selRange[0].left + abslocation.x + TextMargin - OffsetX,(selRange[0].top + abslocation.y + selRange[0].height) + OffsetY }, Colors::Black);
				auto lot = Factory::CreateStringLayout(this->Text, FLT_MAX, render_height, font->FontObject);
				d2d->DrawStringLayoutEffect(lot,
					(float)abslocation.x + TextMargin - OffsetX,
					((float)abslocation.y) + OffsetY,
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
					(float)abslocation.x + TextMargin - OffsetX, ((float)abslocation.y) + OffsetY,
					this->ForeColor);
				lot->Release();
			}
		}
		else if (isSelected)
		{
			const float cx = (float)TextMargin + (float)abslocation.x - OffsetX;
			const float cy = (float)abslocation.y + OffsetY;
			const float ch = (font->FontHeight > 16.0f) ? font->FontHeight : 16.0f;
			this->_caretRectCache = { cx - 2.0f, cy - 2.0f, cx + 2.0f, cy + ch + 2.0f };
			this->_caretRectCacheValid = true;
			d2d->DrawLine({ (float)TextMargin + (float)abslocation.x - OffsetX, (float)abslocation.y + OffsetY },
				{ (float)TextMargin + (float)abslocation.x - OffsetX, (float)abslocation.y + OffsetY + 16.0f }, Colors::Black);
		}
		d2d->PopDrawRect();
	}

	if (!Enable)
	{
		d2d->FillRect(abslocation.x, abslocation.y, size.cx, size.cy, { 1.0f ,1.0f ,1.0f ,0.5f });
	}

	d2d->PopDrawRect();
}