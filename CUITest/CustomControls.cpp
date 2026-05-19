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
	float renderHeight = this->Height - (TextMargin * 2.0f);
	textSize = font->GetTextSize(this->Text, FLT_MAX, renderHeight);
	float textOffsetY = (this->Height - textSize.height) * 0.5f;
	if (textOffsetY < 0.0f)textOffsetY = 0.0f;
	auto size = this->ActualSize();
	const float actualWidth = static_cast<float>(size.cx);
	const float actualHeight = static_cast<float>(size.cy);
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
		d2d->FillRoundRect(0, 0, actualWidth, actualHeight, backColor, TextMargin);
		if (this->Image)
		{
			this->RenderImage();
		}
		d2d->PushDrawRect(this->TextMargin, 0, actualWidth - this->TextMargin * 2.0f, actualHeight);
		auto brush = d2d->CreateLinearGradientBrush(this->Stops.data(), static_cast<UINT32>(this->Stops.size()));
		brush->SetStartPoint(D2D1::Point2F(0, 0));
		brush->SetEndPoint(D2D1::Point2F((float)this->Width, (float)this->Height));
		if (this->Text.size() > 0)
		{
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
						d2d->FillRect(sr.left + TextMargin - HorizontalScrollOffset, sr.top + textOffsetY, sr.width, sr.height, this->SelectedBackColor);
					}
				}
				else
				{
					if (!selRange.empty())
					{
						const auto caret = selRange[0];
						const float cx = caret.left + TextMargin - HorizontalScrollOffset;
						const float cy = caret.top + textOffsetY;
						const float ch = caret.height > 0 ? caret.height : font->FontHeight;
						auto absoluteLocation = this->AbsLocation;
						this->_caretRectCache = { static_cast<float>(absoluteLocation.x) + cx - 2.0f, static_cast<float>(absoluteLocation.y) + cy - 2.0f, static_cast<float>(absoluteLocation.x) + cx + 2.0f, static_cast<float>(absoluteLocation.y) + cy + ch + 2.0f };
						this->_caretRectCacheValid = true;
						shouldDrawCaret = true;
						caretStart = { selRange[0].left + TextMargin - HorizontalScrollOffset, selRange[0].top + textOffsetY };
						caretEnd = { selRange[0].left + TextMargin - HorizontalScrollOffset, selRange[0].top + selRange[0].height + textOffsetY };
					}
				}
				auto textLayout = Factory::CreateStringLayout(this->Text, FLT_MAX, renderHeight, font->FontObject);
				if (textLayout) {
					d2d->DrawStringLayoutEffect(textLayout,
						TextMargin - HorizontalScrollOffset, textOffsetY,
						brush,
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
						brush);
					textLayout->Release();
				}
			}
		}
		else
		{
			if (isSelected)
			{
				const float cx = (float)TextMargin - HorizontalScrollOffset;
				const float cy = textOffsetY;
				const float ch = (font->FontHeight > 16.0f) ? font->FontHeight : 16.0f;
				auto absoluteLocation = this->AbsLocation;
				this->_caretRectCache = { static_cast<float>(absoluteLocation.x) + cx - 2.0f, static_cast<float>(absoluteLocation.y) + cy - 2.0f, static_cast<float>(absoluteLocation.x) + cx + 2.0f, static_cast<float>(absoluteLocation.y) + cy + ch + 2.0f };
				this->_caretRectCacheValid = true;
				shouldDrawCaret = true;
				caretStart = { (float)TextMargin - HorizontalScrollOffset, textOffsetY };
				caretEnd = { (float)TextMargin - HorizontalScrollOffset, textOffsetY + 16.0f };
			}
		}
		UpdateCaretBlinkState(isSelected, this->SelectionStart, this->SelectionEnd, this->_caretRectCacheValid, this->_caretRectCacheValid ? &this->_caretRectCache : nullptr);
		if (shouldDrawCaret && IsCaretBlinkVisible())
		{
			d2d->DrawLine(caretStart, caretEnd, this->ForeColor);
		}
		d2d->DrawLine(
			{ this->TextMargin, this->textSize.height + textOffsetY },
			{ (float)(this->Width - this->TextMargin), this->textSize.height + textOffsetY },
			brush,
			1.0f
		);
		brush->Release();
		d2d->PopDrawRect();
	}
	if (!this->Enable)
	{
		d2d->FillRoundRect(0, 0, actualWidth, actualHeight, { 1.0f ,1.0f ,1.0f ,0.5f }, this->TextMargin);
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
	if (lastMeasuredWidth > size.cx)
	{
		size.cx = static_cast<LONG>(lastMeasuredWidth);
	}
	this->BeginRender(FLT_MAX, FLT_MAX);
	{
		auto brush = d2d->CreateLinearGradientBrush(this->Stops.data(), static_cast<UINT32>(this->Stops.size()));
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
	lastMeasuredWidth = static_cast<float>(size.cx);
}
