#include "FakeWebBrowser.h"
#include "../CUI_Legacy/GUI/Form.h"

FakeWebBrowser::FakeWebBrowser(int x, int y, int width, int height)
{
	this->Location = POINT{ x, y };
	this->Size = SIZE{ width, height };
	this->BackColor = Colors::Black;
}

void FakeWebBrowser::Update()
{
	if (!this->IsVisual || !this->Visible) return;
	if (!this->ParentForm || !this->ParentForm->Render) return;

	auto sz = this->ActualSize();
	auto* d2d = this->ParentForm->Render;
	this->BeginRender();
	{
		d2d->FillRect(0.0f, 0.0f, (float)sz.cx, (float)sz.cy, Colors::Black);
		d2d->DrawRect(0.0f, 0.0f, (float)sz.cx, (float)sz.cy, Colors::DimGrey, 1.0f);

		auto labelFont = this->Font ? this->Font : GetDefaultFontObject();
		std::wstring label = L"WebBrowser";
		auto textSize = labelFont->GetTextSize(label);
		float textX = ((float)sz.cx - textSize.width) * 0.5f;
		float textY = ((float)sz.cy - textSize.height) * 0.5f;
		if (textX < 8.0f) textX = 8.0f;
		if (textY < 8.0f) textY = 8.0f;
		d2d->DrawString(label, textX, textY, Colors::WhiteSmoke, labelFont);
	}
	this->EndRender();
}
