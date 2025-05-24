#pragma once
#include "TabControl.h"
#include "Panel.h"
#include "Form.h"
#pragma comment(lib, "Imm32.lib")

UIClass TabPage::Type() { return UIClass::UI_TabPage; }
TabPage::TabPage()
{
	this->Text = L"Page";
}
TabPage::TabPage(std::wstring text)
{
	this->Text = text;
}

UIClass TabControl::Type() { return UIClass::UI_TabControl; }
TabControl::TabControl(int x, int y, int width, int height)
{
	this->Location = POINT{ x,y };
	this->Size = SIZE{ width,height };
}
TabPage* TabControl::AddPage(std::wstring name)
{
	TabPage* result = this->AddControl(new TabPage(name));
	result->BackColor = this->BackColor;
	return result;
}
void TabControl::Update()
{
	if (this->IsVisual == false)return;
	bool isUnderMouse = this->ParentForm->UnderMouse == this;
	bool isSelected = this->ParentForm->Selected == this;
	auto d2d = this->ParentForm->Render;
	auto font = this->Font;
	auto abslocation = this->AbsLocation;
	auto size = this->ActualSize();
	auto absRect = this->AbsRect;
	d2d->PushDrawRect(absRect.left, absRect.top, absRect.right - absRect.left, absRect.bottom - absRect.top);
	{
		d2d->FillRect(abslocation.x, abslocation.y, size.cx, size.cy, this->BackColor);
		if (this->Image)
		{
			this->RenderImage();
		}
		
		if (this->Count > 0)
		{
			for (int i = 0; i < this->Count; i++)
			{
				this->operator[](i)->Visible = this->SelectIndex == i;
				auto textsize = font->GetTextSize(this->operator[](i)->Text);
				float lf = (TitleWidth - textsize.width) / 2.0f;
				if (lf < 0)lf = 0;
				float tf = (TitleHeight - textsize.height) / 2.0f;
				if (tf < 0)tf = 0;
				d2d->PushDrawRect(abslocation.x + (TitleWidth * i), abslocation.y, TitleWidth, TitleHeight);
				if (i == this->SelectIndex)
					d2d->FillRect(abslocation.x + (TitleWidth * i), abslocation.y, TitleWidth, TitleHeight, this->SelectedTitleBackColor);
				else
					d2d->FillRect(abslocation.x + (TitleWidth * i), abslocation.y, TitleWidth, TitleHeight, this->TitleBackColor);
				d2d->DrawString(this->operator[](i)->Text, abslocation.x + (TitleWidth * i) + lf, abslocation.y + tf, this->ForeColor);
				d2d->DrawRect(abslocation.x + (TitleWidth * i), abslocation.y, TitleWidth, TitleHeight, this->BolderColor, this->Boder);
				d2d->PopDrawRect();
			}
			TabPage* page = (TabPage*)this->operator[](this->SelectIndex);
			page->Location = POINT{ 0,(int)this->TitleHeight };
			page->Size = this->Size;
			page->Update();
		}
		d2d->DrawRect(abslocation.x, abslocation.y + this->TitleHeight, size.cx, size.cy - this->TitleHeight, this->BolderColor, this->Boder);
	}
	if (!this->Enable)
	{
		d2d->FillRect(abslocation.x, abslocation.y, size.cx, size.cy, { 1.0f ,1.0f ,1.0f ,0.5f });
	}
	d2d->PopDrawRect();
}
bool TabControl::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof)
{
	if (!this->Enable || !this->Visible) return true;
	if (WM_LBUTTONDOWN == message)
	{
		if (this->ParentForm->Selected && this->ParentForm->Selected != this)
		{
			auto se = this->ParentForm->Selected;
			this->ParentForm->Selected = this;
			se->PostRender();
		}
	}
	if (this->Count > 0)
	{
		if (this->SelectIndex < 0)this->SelectIndex = 0;
		if (this->SelectIndex >= this->Count)this->SelectIndex = this->Count - 1;
		TabPage* page = (TabPage*)this->operator[](this->SelectIndex);
		for (int i = 0; i < page->Count; i++)
		{
			float _yof = yof - TitleHeight;
			auto c = page->operator[](i);
			auto location = c->Location;
			auto size = c->ActualSize();
			if (
				xof >= location.x &&
				_yof >= location.y &&
				xof <= (location.x + size.cx) &&
				_yof <= (location.y + size.cy)
				)
			{
				c->ProcessMessage(message, wParam, lParam, xof - location.x, _yof - location.y);
			}
		}
	}
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
			if (yof < this->TitleHeight)
			{
				if (xof < (this->Count * this->TitleWidth))
				{
					int newSelected = xof / this->TitleWidth;
					if (this->SelectIndex != newSelected)
					{
						this->SelectIndex = newSelected;
						this->OnSelectedChanged(this);
					}
					for (int i = 0; i < this->Count; i++)
					{
						if (i != this->SelectIndex)
						{
							this->operator[](i)->Visible = false;
						}
						else
						{
							this->operator[](i)->Visible = true;
						}
					}
					this->PostRender();
				}
			}
		}
		MouseEventArgs event_obj = MouseEventArgs(FromParamToMouseButtons(message), 0, xof, yof, HIWORD(wParam));
		this->OnMouseDown(this, event_obj);
	}
	break;
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
	case WM_MBUTTONUP:
	{
		MouseEventArgs event_obj = MouseEventArgs(FromParamToMouseButtons(message), 0, xof, yof, HIWORD(wParam));
		this->OnMouseUp(this, event_obj);
	}
	break;
	case WM_LBUTTONDBLCLK:
	{
		MouseEventArgs event_obj = MouseEventArgs(FromParamToMouseButtons(message), 0, xof, yof, HIWORD(wParam));
		this->OnMouseDoubleClick(this, event_obj);
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
