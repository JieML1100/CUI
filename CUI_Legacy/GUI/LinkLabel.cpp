#include "LinkLabel.h"
#include "Form.h"

UIClass LinkLabel::Type() { return UIClass::UI_LinkLabel; }

LinkLabel::LinkLabel(std::wstring text, int x, int y)
	: Label(text, x, y)
{
	this->BackColor = D2D1_COLOR_F{ 0.f, 0.f, 0.f, 0.f };
	this->ForeColor = Colors::DeepSkyBlue;
	this->UnderlineColor = this->ForeColor;
}

void LinkLabel::Update()
{
	if (!this->IsVisual) return;
	auto d2d = this->ParentForm->Render;
	auto abslocation = this->AbsLocation;
	auto size = this->ActualSize();
	auto absRect = this->AbsRect;
	auto font = this->Font;

	if (last_width > size.cx)
	{
		absRect.right += last_width - size.cx;
		size.cx = last_width;
	}

	d2d->PushDrawRect(absRect.left, absRect.top, FLT_MAX, FLT_MAX);
	{
		if (this->Image)
		{
			this->RenderImage();
		}

		bool hover = this->ParentForm && this->ParentForm->UnderMouse == this;
		auto textColor = hover ? this->HoverColor : (this->Visited ? this->VisitedColor : this->ForeColor);
		auto underlineColor = hover ? this->HoverColor : this->UnderlineColor;
		d2d->DrawString(this->Text, abslocation.x, abslocation.y, textColor, font);

		auto textSize = font->GetTextSize(this->Text);
		float underlineY = abslocation.y + textSize.height - 1.0f;
		d2d->DrawLine({ (float)abslocation.x, underlineY },
			{ (float)abslocation.x + textSize.width, underlineY },
			underlineColor, 1.0f);
	}

	if (!this->Enable)
	{
		d2d->FillRect(abslocation.x, abslocation.y, size.cx, size.cy, { 1.0f ,1.0f ,1.0f ,0.5f });
	}

	d2d->PopDrawRect();
	last_width = size.cx;
}

CursorKind LinkLabel::QueryCursor(int xof, int yof)
{
	(void)xof;
	(void)yof;
	return CursorKind::Hand;
}

bool LinkLabel::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof)
{
	if (!this->Enable || !this->Visible) return true;
	bool handled = Label::ProcessMessage(message, wParam, lParam, xof, yof);

	if (message == WM_LBUTTONUP)
	{
		if (this->ParentForm && this->ParentForm->Selected == this)
		{
			this->Visited = true;
			MouseEventArgs event_obj = MouseEventArgs(FromParamToMouseButtons(message), 0, xof, yof, HIWORD(wParam));
			this->OnMouseClick(this, event_obj);
			this->ParentForm->Selected = NULL;
			this->PostRender();
		}
	}

	return handled;
}
