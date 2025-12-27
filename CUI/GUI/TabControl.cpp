#pragma once
#include "TabControl.h"
#include "Panel.h"
#include "Form.h"
#include "WebBrowser.h"
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

static void SyncNativeChildWindowsRecursive(Control* root)
{
	if (!root) return;
	// 目前库里只有 WebBrowser 会创建真实 HWND 子窗口；后续有更多原生控件也可在此扩展
	if (root->Type() == UIClass::UI_WebBrowser)
	{
		// 强制跑一次 Update：它会根据 IsVisual/Visible 同步宿主 HWND 的 Show/Hide/Bounds
		root->Update();
	}
	for (int i = 0; i < root->Count; i++)
		SyncNativeChildWindowsRecursive(root->operator[](i));
}

static void SyncNativeChildWindowsForAllPages(TabControl* tc)
{
	if (!tc) return;
	for (int i = 0; i < tc->Count; i++)
	{
		auto page = tc->operator[](i);
		SyncNativeChildWindowsRecursive(page);
	}
}

TabPage* TabControl::AddPage(std::wstring name)
{
	TabPage* result = this->AddControl(new TabPage(name));
	result->BackColor = this->BackColor;
	result->Location = POINT{ 0, this->TitleHeight };
	{
		SIZE s = this->Size;
		s.cy = std::max(0L, s.cy - this->TitleHeight);
		result->Size = s;
	}
	for (int i = 0; i < this->Count; i++)
	{
		this->operator[](i)->Visible = (this->SelectIndex == i);
	}
	// 新增页后也同步一次原生子窗口（避免被隐藏页“遗留显示”）
	SyncNativeChildWindowsForAllPages(this);
	return result;
}
GET_CPP(TabControl, int, PageCount)
{
	return this->Count;
}
GET_CPP(TabControl, List<Control*>&, Pages)
{
	return this->Children;
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
			if (this->SelectIndex < 0)this->SelectIndex = 0;
			if (this->SelectIndex >= this->Count)this->SelectIndex = this->Count - 1;

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
				d2d->DrawString(this->operator[](i)->Text, abslocation.x + (TitleWidth * i) + lf, abslocation.y + tf, this->ForeColor, font);
				d2d->DrawRect(abslocation.x + (TitleWidth * i), abslocation.y, TitleWidth, TitleHeight, this->BolderColor, this->Boder);
				d2d->PopDrawRect();
			}
			TabPage* page = (TabPage*)this->operator[](this->SelectIndex);
			page->Location = POINT{ 0,(int)this->TitleHeight };
			{
				SIZE s = this->Size;
				s.cy = std::max(0L, s.cy - this->TitleHeight);
				page->Size = s;
			}
			page->Update();

			if (this->_lastSelectIndex != this->SelectIndex)
			{
				SyncNativeChildWindowsForAllPages(this);
				this->_lastSelectIndex = this->SelectIndex;
			}
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

		// 先处理标题栏点击（切换页）：
		if (message == WM_LBUTTONDOWN && yof < this->TitleHeight)
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
					this->operator[](i)->Visible = (i == this->SelectIndex);
				}

				// 同步隐藏页的 WebBrowser 之类的原生子窗口
				SyncNativeChildWindowsForAllPages(this);
				this->_lastSelectIndex = this->SelectIndex;

				this->_capturedChild = NULL;
				if (GetCapture() == this->ParentForm->Handle)
					ReleaseCapture();
				this->PostRender();
			}
		}

		// Content 区域坐标
		const int cy = yof - this->TitleHeight;

		auto forwardToChild = [&](Control* c)
			{
				if (!c) return;
				auto location = c->Location;
				c->ProcessMessage(message, wParam, lParam, xof - location.x, cy - location.y);
			};

		// 鼠标按住期间：持续转发到按下时命中的子控件（解决拖动/松开丢失）
		bool mousePressed = (wParam & MK_LBUTTON) || (wParam & MK_RBUTTON) || (wParam & MK_MBUTTON);
		if ((message == WM_MOUSEMOVE || message == WM_LBUTTONUP || message == WM_RBUTTONUP || message == WM_MBUTTONUP) && this->_capturedChild)
		{
			forwardToChild(this->_capturedChild);
			if (message == WM_LBUTTONUP || message == WM_RBUTTONUP || message == WM_MBUTTONUP)
			{
				this->_capturedChild = NULL;
				if (GetCapture() == this->ParentForm->Handle)
					ReleaseCapture();
			}
		}
		else if ((message == WM_MOUSEMOVE && mousePressed) && this->_capturedChild)
		{
			forwardToChild(this->_capturedChild);
		}
		else
		{
			// 按下时：命中哪个子控件就捕获它
			if (message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN || message == WM_MBUTTONDOWN ||
				message == WM_LBUTTONDBLCLK || message == WM_RBUTTONDBLCLK || message == WM_MBUTTONDBLCLK ||
				message == WM_MOUSEMOVE || message == WM_MOUSEWHEEL)
			{
				// 只在 content 区域才命中子控件
				if (cy >= 0)
				{
					Control* hit = NULL;
					for (int i = page->Count - 1; i >= 0; i--)
					{
						auto c = page->operator[](i);
						if (!c || !c->Visible || !c->Enable) continue;
						auto loc = c->Location;
						auto sz = c->ActualSize();
						if (xof >= loc.x && cy >= loc.y && xof <= (loc.x + sz.cx) && cy <= (loc.y + sz.cy))
						{
							hit = c;
							break;
						}
					}

					if (hit)
					{
						// 捕获鼠标，确保鼠标移出窗口也能持续收到 move/up（拖动选中/下拉框等）
						if (message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN || message == WM_MBUTTONDOWN)
						{
							this->_capturedChild = hit;
							if (this->ParentForm && this->ParentForm->Handle)
								SetCapture(this->ParentForm->Handle);
						}
						forwardToChild(hit);
					}
				}
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
		MouseEventArgs event_obj = MouseEventArgs(FromParamToMouseButtons(message), 0, xof, yof, HIWORD(wParam));
		this->OnMouseDown(this, event_obj);
	}
	break;
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
	case WM_MBUTTONUP:
	{
		// 防御：如果捕获还在，释放掉
		if (this->_capturedChild && (GetCapture() == this->ParentForm->Handle))
			ReleaseCapture();
		this->_capturedChild = NULL;
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
