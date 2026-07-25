#include "FakeWebBrowser.h"
#include "../CUI/include/Canvas.h"
#include "../CUI/include/Window.h"

FakeWebBrowser::FakeWebBrowser(int x, int y, int width, int height)
	: WebBrowser()
{
	Canvas::SetLeft(*this, static_cast<float>(x));
	Canvas::SetTop(*this, static_cast<float>(y));
	Width = static_cast<float>(width);
	Height = static_cast<float>(height);
	this->Background = Colors::Black;
}

void FakeWebBrowser::OnRender()
{
	if (!this->IsVisible) return;
	if (!this->GetPresentationWindow() || !this->GetDrawingContext()) return;

	auto sz = this->GetActualSizeDip();
	auto* d2d = this->GetDrawingContext();
	this->BeginRender();
	{
		d2d->FillRect(0.0f, 0.0f, sz.width, sz.height, Colors::Black);
		d2d->DrawRect(0.0f, 0.0f, sz.width, sz.height, Colors::DimGrey, 1.0f);

		auto* labelFont = this->GetRenderFont();
		if (!labelFont)
		{
			this->EndRender();
			return;
		}
		std::wstring label = L"WebBrowser";
		auto textSize = labelFont->GetTextSize(label);
		float textX = (sz.width - textSize.width) * 0.5f;
		float textY = (sz.height - textSize.height) * 0.5f;
		if (textX < 8.0f) textX = 8.0f;
		if (textY < 8.0f) textY = 8.0f;
		d2d->DrawString(label, textX, textY, Colors::WhiteSmoke, labelFont);
	}
	this->EndRender();
}
