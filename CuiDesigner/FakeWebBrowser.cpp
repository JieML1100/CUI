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

	auto abs = this->AbsLocation;
	auto sz = this->ActualSize();
	auto absRect = this->AbsRect;

	this->ParentForm->Render->PushDrawRect(absRect.left, absRect.top, absRect.right - absRect.left, absRect.bottom - absRect.top);
	this->ParentForm->Render->FillRect((float)abs.x, (float)abs.y, (float)sz.cx, (float)sz.cy, Colors::Black);
	this->ParentForm->Render->PopDrawRect();
}
