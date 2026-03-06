#include "CustomControls.h"
#include "../CUI_Legacy/GUI/Form.h"

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
	this->BeginRender();
	{
		d2d->FillRoundRect(0, 0, size.cx, size.cy, isSelected ? this->FocusedColor : this->BackColor, this->TextMargin);
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
					d2d->DrawLine(
						{ selRange[0].left + TextMargin - OffsetX, selRange[0].top - OffsetY },
						{ selRange[0].left + TextMargin - OffsetX, selRange[0].top + selRange[0].height + OffsetY },
						Colors::Black);
				}
				auto lot = Factory::CreateStringLayout(this->Text, FLT_MAX, render_height, font->FontObject);
				d2d->DrawStringLayoutEffect(lot,
					TextMargin - OffsetX, OffsetY,
					brush,
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
					brush);
				lot->Release();
			}
		}
		else
		{
			if (isSelected)
				d2d->DrawLine(
					{ TextMargin - OffsetX, OffsetY },
					{ TextMargin - OffsetX, OffsetY + 16.0f },
					Colors::Red);
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
