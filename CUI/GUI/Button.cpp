#include "Button.h"
#include "Form.h"
UIClass Button::Type() { return UIClass::UI_Button; }
Button::Button(std::wstring text, int x, int y, int width, int height)
{
	this->Text = text;
	this->Location = POINT{ x,y };
	this->Size = SIZE{ width,height };
	this->BackColor = Colors::Snow3;
	this->BolderColor = Colors::Snow4;
}
void Button::Update()
{
	if (!this->IsVisual) return;
	bool isUnderMouse = this->ParentForm->UnderMouse == this;
	bool isSelected = this->ParentForm->Selected == this;
	auto d2d = this->ParentForm->Render;
	auto abslocation = this->AbsLocation;
	auto size = this->ActualSize();
	auto absRect = this->AbsRect;
	d2d->PushDrawRect(absRect.left, absRect.top, absRect.right - absRect.left, absRect.bottom - absRect.top);
	{
		float roundVal = this->Image ? 0.0f : this->Height * Round;
		d2d->FillRoundRect(abslocation.x + (this->Boder * 0.5f), abslocation.y + (this->Boder * 0.5f), size.cx - this->Boder, size.cy - this->Boder, this->BackColor, roundVal);
		if (this->Image)
			this->RenderImage();
		D2D1::ColorF color = isUnderMouse ? (isSelected ? D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.7f) : D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.4f)) : D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f);
		d2d->FillRoundRect(abslocation.x, abslocation.y, size.cx, size.cy, color, roundVal);
		auto font = this->Font;
		auto textSize = font->GetTextSize(this->Text);
		float drawLeft = this->Width > textSize.width ? (this->Width - textSize.width) / 2.0f : 0.0f;
		float drawTop = this->Height > textSize.height ? (this->Height - textSize.height) / 2.0f : 0.0f;
		d2d->DrawString(this->Text, abslocation.x + drawLeft, abslocation.y + drawTop, this->ForeColor, this->Font);
		d2d->DrawRoundRect(abslocation.x + (this->Boder * 0.5f), abslocation.y + (this->Boder * 0.5f),
			size.cx - this->Boder, size.cy - this->Boder,
			this->BolderColor, this->Boder, roundVal);
	}

	if (!this->Enable)
		d2d->FillRect(abslocation.x, abslocation.y, size.cx, size.cy, { 1.0f ,1.0f ,1.0f ,0.5f });
	d2d->PopDrawRect();
}
bool Button::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof)
{
	if (!this->Enable || !this->Visible) return true;
	switch (message)
	{
	case WM_DROPFILES:
	{
		HDROP hDropInfo = HDROP(wParam);
		UINT uFileNum = DragQueryFile(hDropInfo, 0xffffffff, NULL, 0);
		TCHAR strFileName[MAX_PATH];
		List<std::wstring> files;
		for (int i = 0; i < uFileNum; i++)
		{
			DragQueryFile(hDropInfo, i, strFileName, MAX_PATH);
			files.Add(strFileName);
		}
		DragFinish(hDropInfo);
		if (files.Count > 0)
		{
			this->OnDropFile(this, files);
		}
	}
	break;
	case WM_MOUSEWHEEL:
	{
		MouseEventArgs event_obj = MouseEventArgs(MouseButtons::None, 0, xof, yof, GET_WHEEL_DELTA_WPARAM(wParam));
		this->OnMouseWheel(this, event_obj);
	}
	break;
	case WM_MOUSEMOVE:
	{
		this->ParentForm->UnderMouse = this;
		MouseEventArgs event_obj = MouseEventArgs(MouseButtons::None, 0, xof, yof, HIWORD(wParam));
		this->OnMouseMove(this, event_obj);
	}
	break;
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_MBUTTONDOWN:
	{
		if (WM_LBUTTONDOWN == message)
		{
			auto lastSelected = this->ParentForm->Selected;
			this->ParentForm->Selected = this;
			if (lastSelected && lastSelected != this)
			{
				lastSelected->PostRender();
			}
		}
		MouseEventArgs event_obj = MouseEventArgs(FromParamToMouseButtons(message), 0, xof, yof, HIWORD(wParam));
		this->OnMouseDown(this, event_obj);
		this->PostRender();
	}
	break;
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
	case WM_MBUTTONUP:
	{
		if (WM_LBUTTONUP == message && this->ParentForm->Selected == this)
		{
			MouseEventArgs event_obj = MouseEventArgs(FromParamToMouseButtons(message), 0, xof, yof, HIWORD(wParam));
			this->OnMouseClick(this, event_obj);
		}
		this->ParentForm->Selected = NULL;
		MouseEventArgs event_obj = MouseEventArgs(FromParamToMouseButtons(message), 0, xof, yof, HIWORD(wParam));
		this->OnMouseUp(this, event_obj);
		this->PostRender();
	}
	break;
	case WM_LBUTTONDBLCLK:
	{
		auto lastSelected = this->ParentForm->Selected;
		this->ParentForm->Selected = this;
		MouseEventArgs event_obj = MouseEventArgs(FromParamToMouseButtons(message), 0, xof, yof, HIWORD(wParam));
		this->OnMouseDoubleClick(this, event_obj);
		this->PostRender();
	}
	break;
	case WM_KEYDOWN:
	{
		KeyEventArgs event_obj = KeyEventArgs((Keys)(wParam | 0));
		this->OnKeyDown(this, event_obj);
	}
	break;
	case WM_KEYUP:
	{
		KeyEventArgs event_obj = KeyEventArgs((Keys)(wParam | 0));
		this->OnKeyUp(this, event_obj);
	}
	break;
	}
	return true;
}