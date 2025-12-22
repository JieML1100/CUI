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
	auto abslocation = this->AbsLocation;
	auto size = this->ActualSize();
	auto absRect = this->AbsRect;
	bool isSelected = this->ParentForm->Selected == this;
	d2d->PushDrawRect(absRect.left, absRect.top, absRect.right - absRect.left, absRect.bottom - absRect.top);
	{
		d2d->FillRoundRect(abslocation.x, abslocation.y, size.cx, size.cy, isSelected ? this->FocusedColor : this->BackColor, this->TextMargin);
		if (this->Image)
		{
			this->RenderImage();
		}
		d2d->PushDrawRect(absRect.left + this->TextMargin, absRect.top, (absRect.right - absRect.left) - (this->TextMargin * 2.0f), (absRect.bottom - absRect.top));
		auto brush = d2d->CreateLinearGradientBrush(this->Stops.data(), this->Stops.size());
		brush->SetStartPoint(D2D1::Point2F(this->Left, this->Top));
		brush->SetEndPoint(D2D1::Point2F(this->Left + this->Width, this->Top + this->Height));
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
						d2d->FillRect(sr.left + abslocation.x + TextMargin - OffsetX, (sr.top + abslocation.y) + OffsetY, sr.width, sr.height, this->SelectedBackColor);
					}
				}
				else
				{
					d2d->DrawLine(
						{ selRange[0].left + abslocation.x + TextMargin - OffsetX,(selRange[0].top + abslocation.y) - OffsetY },
						{ selRange[0].left + abslocation.x + TextMargin - OffsetX,(selRange[0].top + abslocation.y + selRange[0].height) + OffsetY },
						Colors::Black);
				}
				auto lot = Factory::CreateStringLayout(this->Text, FLT_MAX, render_height, font->FontObject);
				d2d->DrawStringLayoutEffect(lot,
					(float)abslocation.x + TextMargin - OffsetX, ((float)abslocation.y) + OffsetY,
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
					(float)abslocation.x + TextMargin - OffsetX, ((float)abslocation.y) + OffsetY,
					brush);
				lot->Release();
			}
		}
		else
		{
			if (isSelected)
				d2d->DrawLine(
					{ (float)TextMargin + (float)abslocation.x - OffsetX, (float)abslocation.y + OffsetY },
					{ (float)TextMargin + (float)abslocation.x - OffsetX, (float)abslocation.y + OffsetY + 16.0f },
					Colors::Red);
		}
		d2d->DrawLine(
			{ abslocation.x + this->TextMargin, abslocation.y + this->textSize.height + OffsetY },
			{ abslocation.x + (this->Width - this->TextMargin), abslocation.y + this->textSize.height + OffsetY },
			brush,
			1.0f
		);
		brush->Release();
		d2d->PopDrawRect();
	}
	if (!this->Enable)
	{
		d2d->FillRoundRect(abslocation.x, abslocation.y, size.cx, size.cy, { 1.0f ,1.0f ,1.0f ,0.5f }, this->TextMargin);
	}
	d2d->PopDrawRect();
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
	auto abslocation = this->AbsLocation;
	auto size = this->ActualSize();
	auto absRect = this->AbsRect;
	if (last_width > size.cx)
	{
		absRect.right += last_width - size.cx;
		size.cx = last_width;
	}
	d2d->PushDrawRect(absRect.left, absRect.top, FLT_MAX, FLT_MAX);
	{
		auto brush = d2d->CreateLinearGradientBrush(this->Stops.data(), this->Stops.size());
		brush->SetStartPoint(D2D1::Point2F(this->Left, this->Top));
		brush->SetEndPoint(D2D1::Point2F(this->Left + this->Width, this->Top + this->Height));
		if (this->Image)
		{
			this->RenderImage();
		}
		d2d->DrawString(this->Text, abslocation.x, abslocation.y, brush, this->Font);
		brush->Release();
	}
	d2d->PopDrawRect();
	last_width = size.cx;
}
