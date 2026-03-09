#pragma once
#include "Switch.h"
#include "Form.h"
#pragma comment(lib, "Imm32.lib")
UIClass Switch::Type() { return UIClass::UI_Switch; }
Switch::Switch(int x, int y, int width, int height)
{
	this->Location = POINT{ x,y };
	this->Size = SIZE{ width,height };
	auto bc = this->BackColor;
	bc.a = 0.0f;
	this->BackColor = bc;
	this->Cursor = CursorKind::Hand;
}
void Switch::Update()
{
	if (this->IsVisual == false)return;
	auto d2d = this->ParentForm->Render;
	auto size = this->ActualSize();
	float clipW = last_width > size.cx ? (float)last_width : (float)size.cx;
	this->BeginRender(clipW, (float)size.cy);
	{
		float r = size.cy / 2.0f;
		float x1 = r;
		float x2 = size.cx - r;
		float y = r;
		d2d->FillEllipse({ x1,y }, r, r, this->Checked ? Colors::Green : Colors::Red);
		d2d->FillEllipse({ x2,y }, r, r, this->Checked ? Colors::Green : Colors::Red);
		d2d->FillRect(x1, 0, x2 - x1, size.cy, this->Checked ? Colors::Green : Colors::Red);
		d2d->FillEllipse({ this->Checked ? x2 : x1, y }, r - 2.0f, r - 2.0f, Colors::White);
	}
	if (!this->Enable)
	{
		d2d->FillRect(0, 0, clipW, (float)size.cy, { 1.0f ,1.0f ,1.0f ,0.5f });
	}
	this->EndRender();
	last_width = size.cx;
}
bool Switch::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof)
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
			this->Checked = !this->Checked;
			this->OnChecked(this);
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
		if (lastSelected && lastSelected == this)
		{
			this->Checked = !this->Checked;
			this->OnChecked(this);
			MouseEventArgs event_obj = MouseEventArgs(FromParamToMouseButtons(message), 0, xof, yof, HIWORD(wParam));
			this->OnMouseDoubleClick(this, event_obj);
			this->PostRender();
		}
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