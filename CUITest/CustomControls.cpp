#define NOMINMAX
#include "CustomControls.h"
#include "../CUI/GUI/Form.h"

CustomTextBox1::CustomTextBox1(std::wstring text, int x, int y, int width, int height) :TextBox(text, x, y, width, height)
{
	this->TextMargin = this->Height * 0.5f;
	Stops.push_back({ 0.0f, D2D1::ColorF(227.0f / 255.0f, 9.0f / 255.0f, 64.0f / 255.0f, 1.0f) });
	Stops.push_back({ 0.33f, D2D1::ColorF(231.0f / 255.0f, 215.0f / 255.0f, 2.0f / 255.0f, 1.0f) });
	Stops.push_back({ 0.66f, D2D1::ColorF(15.0f / 255.0f, 168.0f / 255.0f, 149.0f / 255.0f, 1.0f) });
	Stops.push_back({ 1.0f, D2D1::ColorF(19.0f / 255.0f, 115.0f / 255.0f, 232.0f / 255.0f, 1.0f) });
}
void CustomTextBox1::Update()
{
	if (this->IsVisual == false)return;
	bool isUnderMouse = this->ParentForm->UnderMouse == this;
	auto d2d = this->ParentForm->Render;
	auto font = this->Font;
	float render_height = this->Height - (TextMargin * 2.0f);
	textSize = font->GetTextSize(this->Text, FLT_MAX, render_height);
	float OffsetY = (this->Height - textSize.height) * 0.5f;
	if (OffsetY < 0.0f)OffsetY = 0.0f;
	auto size = this->ActualSize();
	bool isSelected = this->ParentForm->Selected == this;
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
		d2d->FillRoundRect(0, 0, size.cx, size.cy, backColor, TextMargin);
		if (this->Image)
		{
			this->RenderImage();
		}
		d2d->PushDrawRect(this->TextMargin, 0, size.cx - this->TextMargin * 2.0f, size.cy);
		auto brush = d2d->CreateLinearGradientBrush(this->Stops.data(), this->Stops.size());
		brush->SetStartPoint(D2D1::Point2F(0, 0));
		brush->SetEndPoint(D2D1::Point2F((float)this->Width, (float)this->Height));
		if (this->Text.size() > 0)
		{
			auto font = this->Font;
			if (isSelected)
			{
				int sels = SelectionStart <= SelectionEnd ? SelectionStart : SelectionEnd;
				int sele = SelectionEnd >= SelectionStart ? SelectionEnd : SelectionStart;
				int selLen = sele - sels;
				auto selRange = font->HitTestTextRange(this->Text, (UINT32)sels, (UINT32)selLen);
				if (selLen != 0)
				{
					for (auto sr : selRange)
					{
						d2d->FillRect(sr.left + TextMargin - OffsetX, sr.top + OffsetY, sr.width, sr.height, this->SelectedBackColor);
					}
				}
				else
				{
					if (!selRange.empty())
					{
						const auto caret = selRange[0];
						const float cx = caret.left + TextMargin - OffsetX;
						const float cy = caret.top + OffsetY;
						const float ch = caret.height > 0 ? caret.height : font->FontHeight;
						auto abs = this->AbsLocation;
						this->_caretRectCache = { abs.x + cx - 2.0f, abs.y + cy - 2.0f, abs.x + cx + 2.0f, abs.y + cy + ch + 2.0f };
						this->_caretRectCacheValid = true;
						shouldDrawCaret = true;
						caretStart = { selRange[0].left + TextMargin - OffsetX, selRange[0].top + OffsetY };
						caretEnd = { selRange[0].left + TextMargin - OffsetX, selRange[0].top + selRange[0].height + OffsetY };
					}
				}
				auto lot = Factory::CreateStringLayout(this->Text, FLT_MAX, render_height, font->FontObject);
				if (lot) {
					d2d->DrawStringLayoutEffect(lot,
						TextMargin - OffsetX, OffsetY,
						brush,
						DWRITE_TEXT_RANGE{ (UINT32)sels, (UINT32)selLen },
						this->SelectedForeColor,
						font);
					lot->Release();
				}
			}
			else
			{
				auto lot = Factory::CreateStringLayout(this->Text, FLT_MAX, render_height, font->FontObject);
				if (lot) {
					d2d->DrawStringLayout(lot,
						TextMargin - OffsetX, OffsetY,
						brush);
					lot->Release();
				}
			}
		}
		else
		{
			if (isSelected)
			{
				const float cx = (float)TextMargin - OffsetX;
				const float cy = OffsetY;
				const float ch = (font->FontHeight > 16.0f) ? font->FontHeight : 16.0f;
				auto abs = this->AbsLocation;
				this->_caretRectCache = { abs.x + cx - 2.0f, abs.y + cy - 2.0f, abs.x + cx + 2.0f, abs.y + cy + ch + 2.0f };
				this->_caretRectCacheValid = true;
				shouldDrawCaret = true;
				caretStart = { (float)TextMargin - OffsetX, OffsetY };
				caretEnd = { (float)TextMargin - OffsetX, OffsetY + 16.0f };
			}
		}
		UpdateCaretBlinkState(isSelected, this->SelectionStart, this->SelectionEnd, this->_caretRectCacheValid, this->_caretRectCacheValid ? &this->_caretRectCache : nullptr);
		if (shouldDrawCaret && IsCaretBlinkVisible())
		{
			d2d->DrawLine(caretStart, caretEnd, this->ForeColor);
		}
		d2d->DrawLine(
			{ this->TextMargin, this->textSize.height + OffsetY },
			{ (float)(this->Width - this->TextMargin), this->textSize.height + OffsetY },
			brush,
			1.0f
		);
		brush->Release();
		d2d->PopDrawRect();
	}
	if (!this->Enable)
	{
		d2d->FillRoundRect(0, 0, size.cx, size.cy, { 1.0f ,1.0f ,1.0f ,0.5f }, this->TextMargin);
	}
	this->EndRender();
}
CustomLabel1::CustomLabel1(std::wstring text, int x, int y) :Label(text, x, y)
{
	Stops.push_back({ 0.0f, D2D1::ColorF(227.0f / 255.0f, 9.0f / 255.0f, 64.0f / 255.0f, 1.0f) });
	Stops.push_back({ 0.33f, D2D1::ColorF(231.0f / 255.0f, 215.0f / 255.0f, 2.0f / 255.0f, 1.0f) });
	Stops.push_back({ 0.66f, D2D1::ColorF(15.0f / 255.0f, 168.0f / 255.0f, 149.0f / 255.0f, 1.0f) });
	Stops.push_back({ 1.0f, D2D1::ColorF(19.0f / 255.0f, 115.0f / 255.0f, 232.0f / 255.0f, 1.0f) });
}
void CustomLabel1::Update()
{
	if (this->IsVisual == false)return;
	auto d2d = this->ParentForm->Render;
	auto size = this->ActualSize();
	if (last_width > size.cx)
	{
		size.cx = last_width;
	}
	this->BeginRender(FLT_MAX, FLT_MAX);
	{
		auto brush = d2d->CreateLinearGradientBrush(this->Stops.data(), this->Stops.size());
		brush->SetStartPoint(D2D1::Point2F(0, 0));
		brush->SetEndPoint(D2D1::Point2F((float)this->Width, (float)this->Height));
		if (this->Image)
		{
			this->RenderImage();
		}
		d2d->DrawString(this->Text, 0, 0, brush, this->Font);
		brush->Release();
	}
	this->EndRender();
	last_width = size.cx;
}
