#include "Form.h"
#include "NotifyIcon.h"
#include "Graphics/Factory.h"
#include "Graphics/Graphics.h"
#include "NotifyIcon.h"
#include <dwmapi.h>
#include <windowsx.h>
#pragma comment(lib, "Dwmapi.lib")
GET_CPP(Form, POINT, Location)
{
	if (this->Handle)
	{
		RECT rect;
		GetWindowRect(this->Handle, &rect);
		POINT point = { rect.left,rect.top };
		return point;
	}
	else
	{
		return this->_Location_INIT;
	}
}
SET_CPP(Form, POINT, Location)
{
	if (this->Handle)
	{
		SetWindowPos(this->Handle, NULL, value.x, value.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
	}
	this->_Location_INIT = value;
}
GET_CPP(Form, SIZE, Size)
{
	if (this->Handle)
	{
		RECT rect;
		GetWindowRect(this->Handle, &rect);
		SIZE size = { rect.right - rect.left,rect.bottom - rect.top };
		return size;
	}
	else
	{
		return this->_Size_INTI;
	}
}
SET_CPP(Form, SIZE, Size)
{
	if (this->Handle)
	{
		SetWindowPos(this->Handle, NULL, 0, 0, value.cx, value.cy, SWP_NOMOVE | SWP_NOZORDER);
	}
	this->_Size_INTI = value;
	this->ControlChanged = true;
}

GET_CPP(Form, SIZE, ClientSize)
{
	if (this->Handle)
	{
		RECT rect;
		GetClientRect(this->Handle, &rect);
		SIZE size = { rect.right - rect.left,rect.bottom - rect.top };
		return size;
	}
	else
	{
		return this->_Size_INTI;
	}
}
GET_CPP(Form, std::wstring, Text) {
	return _text;
}
SET_CPP(Form, std::wstring, Text) {
	_text = value;
	this->ControlChanged = true;
}
GET_CPP(Form, bool, TopMost)
{
	return (GetWindowLong(this->Handle, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
}
SET_CPP(Form, bool, TopMost)
{
	if (value)
	{
		SetWindowPos(this->Handle, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
	}
	else
	{
		SetWindowPos(this->Handle, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
	}
}
GET_CPP(Form, bool, Enable)
{
	return IsWindowEnabled(this->Handle);
}
SET_CPP(Form, bool, Enable)
{
	EnableWindow(this->Handle, value);
}
GET_CPP(Form, bool, Visible)
{
	return IsWindowVisible(this->Handle);
}
SET_CPP(Form, bool, Visible)
{
	ShowWindow(this->Handle, value ? SW_SHOW : SW_HIDE);
}

GET_CPP(Form, bool, ShowInTaskBar)
{
	return this->_showInTaskBar;
}
SET_CPP(Form, bool, ShowInTaskBar)
{
	this->_showInTaskBar = value;
	if (this->Handle)
	{
		LONG_PTR exStyle = GetWindowLongPtr(this->Handle, GWL_EXSTYLE);

		if (value)
		{
			exStyle &= ~WS_EX_TOOLWINDOW;
			exStyle |= WS_EX_APPWINDOW;
		}
		else
		{
			exStyle |= WS_EX_TOOLWINDOW;
			exStyle &= ~WS_EX_APPWINDOW;
		}
		SetWindowLongPtr(this->Handle, GWL_EXSTYLE, exStyle);
	}
}

Form::Form(std::wstring text, POINT _location, SIZE _size)
{
	this->_text = text;
	static bool ClassInited = false;
	this->Location = _location;
	this->Size = _size;
	WNDCLASSW wndclass = { 0 };
	if (!ClassInited)
	{
		wndclass.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
		wndclass.cbClsExtra = 0;
		wndclass.cbWndExtra = 0;
		wndclass.lpfnWndProc = WINMSG_PROCESS;
		wndclass.hInstance = GetModuleHandleA(NULL);
		wndclass.hIcon = LoadIconW(NULL, MAKEINTRESOURCEW(32512));
		wndclass.hCursor = LoadCursorW(NULL, MAKEINTRESOURCEW(32512));
		wndclass.lpszMenuName = NULL;
		wndclass.lpszClassName = L"CoreNativeWindow";
		if (!RegisterClassW(&wndclass))
		{
			return;
		}
		ClassInited = true;
	}
	int desktopWidth = GetSystemMetrics(SM_CXSCREEN);
	int desktopHeight = GetSystemMetrics(SM_CYSCREEN);
	this->Handle = CreateWindowExW(
		0L,
		L"CoreNativeWindow",
		_text.c_str(),
		WS_POPUP,
		this->Location.x == 0 ? ((int)(desktopWidth - this->Size.cx) / 2) : this->Location.x,
		this->Location.y == 0 ? ((int)(desktopHeight - this->Size.cy) / 2) : this->Location.y,
		this->Size.cx,
		this->Size.cy,
		GetTopMostWindowInCurrentProcess(),
		NULL,
		GetModuleHandleW(0),
		0);
	PROPERTY
	SetWindowLongPtrW(this->Handle, GWLP_USERDATA, (LONG_PTR)this ^ 0xFFFFFFFFFFFFFFFF);
	
	DragAcceptFiles,(this->Handle, TRUE);


	Application::Forms.Add(this->Handle, this);

	float xtmp = this->Size.cx - (this->HeadHeight * 3);
	_minBox = (Button*)this->AddControl(new Button(L"―", xtmp, 0.0f, this->HeadHeight, this->HeadHeight));
	xtmp += this->HeadHeight;
	_maxBox = (Button*)this->AddControl(new Button(L"⬜", xtmp, 0.0f, this->HeadHeight, this->HeadHeight));
	xtmp += this->HeadHeight;
	_closeBox = (Button*)this->AddControl(new Button(L"✕", xtmp, 0.0f, this->HeadHeight, this->HeadHeight));

	_minBox->OnMouseClick += [](class Control* sender, MouseEventArgs)
		{
			ShowWindow(((Button*)sender)->ParentForm->Handle, SW_MINIMIZE); 
		};
	_maxBox->OnMouseClick += [](class Control* sender, MouseEventArgs)
		{
			if (IsZoomed(((Button*)sender)->ParentForm->Handle))
				((Button*)sender)->ParentForm->Handle; 
			if (IsZoomed(((Button*)sender)->ParentForm->Handle)) 
			{
				((Button*)sender)->Text = L"⬜"; 
				ShowWindow(((Button*)sender)->ParentForm->Handle, SW_RESTORE); 
			}
			else
			{
				((Button*)sender)->Text = L"❐"; 
				ShowWindow(((Button*)sender)->ParentForm->Handle, SW_MAXIMIZE); 
			}
		};
	_closeBox->OnMouseClick += [](class Control* sender, MouseEventArgs)
		{
			((Button*)sender)->ParentForm->Close();
			((Button*)sender)->ParentForm->Close(); 
		};
	_minBox->Boder = 0.0f; _minBox->Round = 0.0f; _minBox->BackColor = D2D1_COLOR_F{ 0.0f,0.0f,0.0f,0.0f };
	_maxBox->Boder = 0.0f; _maxBox->Round = 0.0f; _maxBox->BackColor = D2D1_COLOR_F{ 0.0f,0.0f,0.0f,0.0f };
	_closeBox->Boder = 0.0f; _closeBox->Round = 0.0f; _closeBox->BackColor = D2D1_COLOR_F{ 0.0f,0.0f,0.0f,0.0f };

	Render = new HwndGraphics(this->Handle);
}

void Form::updateHead()
{
	float xtmp = this->Size.cx - this->HeadHeight;
	if (_closeBox && CloseBox)
	{
		_closeBox->Left = xtmp; _closeBox->Size = SIZE{ this->HeadHeight ,this->HeadHeight };
		xtmp -= this->HeadHeight;
	}
	if (_maxBox && MaxBox)
	{
		_maxBox->Left = xtmp; _maxBox->Size = SIZE{ this->HeadHeight ,this->HeadHeight };
		xtmp -= this->HeadHeight;
	}
	if (_minBox && MinBox)
	{
		_minBox->Left = xtmp; _minBox->Size = SIZE{ this->HeadHeight ,this->HeadHeight };
	}
	_minBox->Visible = this->MinBox;
	_maxBox->Visible = this->MaxBox;
	_closeBox->Visible = this->CloseBox;
}
void Form::Show()
{
	updateHead();
	if (this->Icon) SendMessage(this->Handle, WM_SETICON, ICON_BIG, (LPARAM)this->Icon);
	SetWindowLong(this->Handle, GWL_STYLE, WS_POPUP);
	ShowWindow(this->Handle, SW_SHOWNORMAL);
}
void Form::ShowDialog()
{
	if (this->Icon) SendMessage(this->Handle, WM_SETICON, ICON_BIG, (LPARAM)this->Icon);
	SetWindowLong(this->Handle, GWL_STYLE, WS_POPUP);
	updateHead();
	ShowWindow(this->Handle, SW_SHOWNORMAL);
	while (IsWindow(this->Handle))
	{
		MSG msg;
		if (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			ULONG64 form = (ULONG64)(GetWindowLongPtrW(msg.hwnd, GWLP_USERDATA) ^ 0xFFFFFFFFFFFFFFFF);
			if (msg.hwnd == this->Handle ||
				form == 0xFFFFFFFFFFFFFFFF ||
				(msg.message == WM_SYSCOMMAND && msg.wParam == SC_RESTORE))
			{
				if (IsWindow(this->Handle))
				{
					DispatchMessageW(&msg);
					this->Update();
				}
				else
				{
					DefWindowProc(msg.hwnd, msg.message, msg.wParam, msg.lParam);
				}
			}
			else
			{
				DefWindowProc(msg.hwnd, msg.message, msg.wParam, msg.lParam);
			}
		}
		else
		{
			Sleep(1);
		}
	}
}
void Form::Close()
{
	SendMessageA(this->Handle, WM_CLOSE, 0, 0);
}
bool Form::DoEvent()
{
	bool hasMessage = false;
	MSG msg;
	if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		hasMessage = true;
	}
	static LONGLONG lastRender = 0;
	LONGLONG now = GetTickCount64();
	if (now - lastRender > 25)
	{
		lastRender = now;
		for (auto& form : Application::Forms)
		{
			form.second->ForceUpdate();
		}
	}
	else
	{
		for (auto& form : Application::Forms)
		{
			form.second->Update();
		}
	}
	if(!hasMessage)
		Sleep(1);
	return hasMessage;
}
bool Form::WaiteEvent()
{
	MSG msg;
	if (GetMessageW(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
		return true;
	}
	return false;
}
bool Form::Update(bool force)
{
	if (ControlChanged && IsWindow(this->Handle))
	{
		this->Render->BeginRender();
		this->Render->Clear(this->BackColor);
		if (this->Image)
		{
			this->RenderImage();
		}
		if (VisibleHead)
		{
			this->Render->FillRect(0, 0, this->Size.cx, this->HeadHeight, { 0.5f ,0.5f ,0.5f ,0.25f });
			float headTextTop = (this->HeadHeight - this->Render->DefaultFontObject->FontHeight) * 0.5f;
			if (headTextTop < 0.0f)
				headTextTop = 0.0f;
			this->Render->PushDrawRect(0, 0, this->Size.cx, this->HeadHeight);
			if (this->CenterTitle)
			{
				auto tSize = this->Render->DefaultFontObject->GetTextSize(this->Text);
				float textRangeWidth = this->Size.cx;
				if (tSize.width > (this->Size.cx - (this->HeadHeight * 3)))
				{
					textRangeWidth -= this->HeadHeight * 3;
				}
				float headTextLeft = (textRangeWidth - tSize.width) * 0.5f;
				if (headTextLeft < 0.0f)
					headTextLeft = 0.0f;
				this->Render->DrawString(this->Text, headTextLeft, headTextTop, this->ForeColor);
			}
			else
			{
				this->Render->DrawString(this->Text, 5.0f, headTextTop, this->ForeColor);
			}
			this->Render->PopDrawRect();
		}
		for (int i = 0; i < this->Controls.Count; i++)
		{
			auto c = this->Controls[i]; if (!c->Visible)continue;
			auto location = c->Location;
			if (c->ParentForm->Render == NULL)
			{
				c->ParentForm->Render = this->Render;
			}
			c->Update();
		}
		for (auto fc : this->ForeGroundControls)
		{
			fc->Update();
		}
		this->OnPaint(this);
		
		this->Render->EndRender();
		this->ControlChanged = false;
		return true;
	}
	return false;
}
bool Form::ForceUpdate()
{
	this->ControlChanged = true;
	return Update(true);
}

bool Form::RemoveControl(Control* c)
{
	if (this->Controls.Contains(c))
	{
		this->Controls.Remove(c);
		this->ForeGroundControls.Remove(c);
		c->Parent = NULL;
		c->ParentForm = NULL;
		return true;
	}
	return false;
}
bool Form::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof)
{
	if (!this->Enable || !this->Visible) return true;
	POINT mouse;
	GetCursorPos(&mouse);
	ScreenToClient(this->Handle, &mouse);
	Control* HitControl = NULL;
	switch (message)
	{ 
	case WM_MOUSEMOVE:
	{
		if (this->Selected && (GetAsyncKeyState(VK_LBUTTON) & 0x8000))
		{
			if (this->Selected->IsVisual)
			{
				HitControl = this->Selected;
				auto location = this->Selected->AbsLocation;
				this->Selected->ProcessMessage(message, wParam, lParam, mouse.x - location.x, mouse.y - location.y);
				break;
			}
		}
		auto lastUnderMouse = this->UnderMouse;
		this->UnderMouse = NULL;
		bool is_First = true;
	reExc:
		for (int i = 0; i < this->Controls.Count; i++)
		{
			auto c = this->Controls[i];
			if (!c->Visible || !c->Enable)continue;
			auto location = c->Location;
			auto size = c->ActualSize();
			if (
				mouse.x >= location.x &&
				mouse.y >= location.y &&
				mouse.x <= (location.x + size.cx) &&
				mouse.y <= (location.y + size.cy)
				)
			{
				if (is_First)
				{
					if (c->Type() == UIClass::UI_ComboBox)
					{
						HitControl = c;
						this->UnderMouse = c;
						c->ProcessMessage(message, wParam, lParam, mouse.x - location.x, mouse.y - location.y);
						goto ext;
					}
				}
				else
				{
					HitControl = c;
					this->UnderMouse = c;
					c->ProcessMessage(message, wParam, lParam, mouse.x - location.x, mouse.y - location.y);
				}
			}
		}
		if (is_First)
		{
			is_First = false;
			goto reExc;
		}
	ext:
		if (lastUnderMouse != this->UnderMouse)
		{
			if (this->UnderMouse)this->UnderMouse->PostRender();
			if (lastUnderMouse)lastUnderMouse->PostRender();
		}
	}
	break;
	case WM_DROPFILES:
	case WM_MOUSEWHEEL:
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
	case WM_LBUTTONDBLCLK:
	{
		if (WM_LBUTTONDOWN == message)
		{
			if (VisibleHead)
			{
				float xtmp = this->Size.cx;
				if (_minBox->Visible) xtmp -= this->HeadHeight;
				if (_maxBox->Visible) xtmp -= this->HeadHeight;
				if (_closeBox->Visible) xtmp -= this->HeadHeight;
				if (((int)(short)HIWORD(lParam)) < this->HeadHeight && ((int)(short)LOWORD(lParam)) < xtmp)
				{
					ReleaseCapture();
					PostMessage(this->Handle, WM_SYSCOMMAND, SC_MOVE | HTCAPTION, NULL);
				}
			}
		}
		else if (WM_LBUTTONUP == message)
		{
			if (this->Selected)
			{
				if (this->Selected->IsVisual)
				{
					HitControl = this->Selected;
					auto location = this->Selected->AbsLocation;
					this->Selected->ProcessMessage(message, wParam, lParam, mouse.x - location.x, mouse.y - location.y);
					break;
				}
			}
		}
		else if (WM_LBUTTONDBLCLK == message)
		{
			if (mouse.y < this->HeadHeight)
				this->_maxBox->OnMouseClick(this->_maxBox, MouseEventArgs{});
		}
		bool is_First = true;
	reExc1:
		for (int i = 0; i < this->Controls.Count; i++)
		{
			auto c = this->Controls[i]; if (!c->Visible)continue;
			auto location = c->Location;
			auto size = c->ActualSize();
			if (
				mouse.x >= location.x &&
				mouse.y >= location.y &&
				mouse.x <= (location.x + size.cx) &&
				mouse.y <= (location.y + size.cy)
				)
			{
				if (is_First)
				{
					if (c->Type() == UIClass::UI_ComboBox)
					{
						HitControl = c;
						c->ProcessMessage(message, wParam, lParam, mouse.x - location.x, mouse.y - location.y);
						goto ext1;
					}
				}
				else
				{
					HitControl = c;
					c->ProcessMessage(message, wParam, lParam, mouse.x - location.x, mouse.y - location.y);
				}
			}
		}
		if (is_First)
		{
			is_First = false;
			goto reExc1;
		}
	ext1:;
	}
	break;
	case WM_KEYDOWN:
	{
		if (this->Selected)
		{
			if (this->Selected->ProcessMessage(message, wParam, lParam, xof, yof))
			{
				if (this->Selected->IsVisual)
				{
					HitControl = this->Selected;
					KeyEventArgs event_obj = KeyEventArgs((Keys)(wParam | 0));
					this->OnKeyDown(this, event_obj);
				}
			}
		}
		else
		{
			KeyEventArgs event_obj = KeyEventArgs((Keys)(wParam | 0));
			this->OnKeyDown(this, event_obj);
		}
	}
	break;
	case WM_KEYUP:
	{
		if (this->Selected)
		{
			if (this->Selected->ProcessMessage(message, wParam, lParam, xof, yof))
			{
				HitControl = this->Selected;
				KeyEventArgs event_obj = KeyEventArgs((Keys)(wParam | 0));
				this->OnKeyUp(this, event_obj);
			}
		}
		else
		{
			KeyEventArgs event_obj = KeyEventArgs((Keys)(wParam | 0));
			this->OnKeyUp(this, event_obj);
		}
	}
	break;
	case WM_SIZE:
	{
		UINT width = LOWORD(lParam);
		UINT height = HIWORD(lParam);
		RECT rec;
		GetClientRect(this->Handle, &rec);
		updateHead();
		this->Render->ReSize(width, height);
		this->OnSizeChanged(this);
		this->ControlChanged = true;
	}
	break;
	case WM_MOVE:
	{
		RECT client_rectangle;
		GetClientRect(this->Handle, &client_rectangle);
		this->OnMoved(this);
	}
	break;
	case WM_PAINT:
	{
		
	}
	break;
	case WM_CHAR:
	{
		if (this->Selected)
		{
			if (this->Selected->ProcessMessage(message, wParam, lParam, xof, yof))
			{
				HitControl = this->Selected;
				this->OnCharInput(this, (wchar_t)(wParam));
			}
		}
		else
		{
			HitControl = NULL;
			this->OnCharInput(this, (wchar_t)(wParam));
		}
	}
	break;
	case WM_IME_COMPOSITION:
	{
		if (this->Selected)
		{
			HitControl = this->Selected;
			this->Selected->ProcessMessage(message, wParam, lParam, xof, yof);
		}
	}
	break;
	case WM_CLOSE:
	{
		this->OnFormClosing(this);
		delete this->Render;
		return true;
	}
	break;
	case WM_COMMAND:
	{
		int id = LOWORD(wParam);
		int additionalInfo = HIWORD(wParam);
		this->OnCommand(this, id, additionalInfo);
	}
	break;
	};
	if (WM_LBUTTONDOWN == message && HitControl == NULL && this->Selected && HitControl != this->Selected)
	{
		auto se = this->Selected;
		this->Selected = NULL;
		se->PostRender();
	}
	if (IsWindow(this->Handle) && this->Render)this->Update();
	return true;
}
void Form::RenderImage()
{
	if (this->Image)
	{
		auto size = this->Image->GetSize();
		if (size.width > 0 && size.height > 0)
		{
			auto asize = this->ClientSize;
			switch (this->SizeMode)
			{
			case ImageSizeMode::Normal:
			{
				this->Render->DrawBitmap(this->Image, 0, 0, size.width, size.height);
			}
			break;
			case ImageSizeMode::CenterImage:
			{
				float xf = (asize.cx - size.width) / 2.0f;
				float yf = (asize.cy - size.height) / 2.0f;
				this->Render->DrawBitmap(this->Image, xf, yf, size.width, size.height);
			}
			break;
			case ImageSizeMode::StretchIamge:
			{
				this->Render->DrawBitmap(this->Image, 0, 0, (float)asize.cx, (float)asize.cy);
			}
			break;
			case ImageSizeMode::Zoom:
			{
				float xp = asize.cx / size.width, yp = asize.cy / size.height;
				float tp = xp < yp ? xp : yp;
				float tw = size.width * tp, th = size.height * tp;
				float xf = (asize.cx - tw) / 2.0f, yf = (asize.cy - th) / 2.0f;
				this->Render->DrawBitmap(this->Image, xf, yf, tw, th);
			}
			break;
			default:
				break;
			}
		}
	}
}
Control* Form::LastChild()
{
	if (this->Controls.Count)
	{
		return this->Controls.Last();
	}
	return NULL;
}
D2D1_RECT_F Form::ChildRect()
{
	if (this->Controls.Count == 0)
		return D2D1_RECT_F{ 0,0,0,0 };
	float left = FLT_MAX;
	float top = FLT_MAX;
	float right = FLT_MIN;
	float bottom = FLT_MIN;
	if (this->Controls.Count)
	{
		for (auto c : this->Controls)
		{
			auto last = this->Controls.Last();
			auto loc = last->Location;
			auto siz = last->ActualSize();
			auto tmp = D2D1_POINT_2F{ (float)loc.x + siz.cy,(float)loc.y + siz.cy };
			if (tmp.x < left)left = tmp.x;
			if (tmp.x > right)right = tmp.x;

			if (tmp.y < top)top = tmp.y;
			if (tmp.y > bottom)bottom = tmp.y;
		}
	}
	return D2D1_RECT_F{ left,top,right,bottom };
}
LRESULT CustomFrameHitTest(HWND _hWnd, WPARAM wParam, LPARAM lParam)
{
#define CAPTION_HEIGHT 30
#define SCALER_WIDTH 8
#define BORDER_WIDTH 1
	RECT wr, cr;
	const POINT ptMouse = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
	GetWindowRect(_hWnd, &wr);
	cr.left = wr.left + SCALER_WIDTH;
	cr.right = wr.right - SCALER_WIDTH;
	cr.bottom = wr.bottom - SCALER_WIDTH;
	cr.top = wr.top + SCALER_WIDTH;

	uint8_t pos_code = 0;
	if (ptMouse.x < wr.left || ptMouse.x > wr.right || ptMouse.y < wr.top || ptMouse.y > wr.bottom)
		return HTNOWHERE;

	if (ptMouse.x < cr.left)
		pos_code |= 0b01;
	else if (ptMouse.x > cr.right)
		pos_code |= 0b11;
	else
		pos_code |= 0b10;

	if (ptMouse.y < cr.top)
		pos_code |= 0b0100;
	else if (ptMouse.y < wr.top + CAPTION_HEIGHT)
		return HTCAPTION;
	else if (ptMouse.y > cr.bottom)
		pos_code |= 0b1100;
	else
		pos_code |= 0b1000;

	switch (pos_code)
	{
	case 0b0101:
		return HTTOPLEFT;
	case 0b0110:
		return HTTOP;
	case 0b0111:
		return HTTOPRIGHT;
	case 0b1001:
		return HTLEFT;
	case 0b1010:
		return HTCLIENT;
	case 0b1011:
		return HTRIGHT;
	case 0b1101:
		return HTBOTTOMLEFT;
	case 0b1110:
		return HTBOTTOM;
	case 0b1111:
		return HTBOTTOMRIGHT;
	}
	return HTNOWHERE;
}
LRESULT CALLBACK Form::WINMSG_PROCESS(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	Form* form = (Form*)(GetWindowLongPtrW(hWnd, GWLP_USERDATA) ^ 0xFFFFFFFFFFFFFFFF);
	if ((ULONG64)form != 0xFFFFFFFFFFFFFFFF && Application::Forms.ContainsKey(form->Handle))
	{
		form->ProcessMessage(message, wParam, lParam, 0, 0);

		switch (message)
		{
		case WM_ACTIVATE:
		{
			constexpr MARGINS margins{ 1, 1, 1, 1 };
			DwmExtendFrameIntoClientArea(hWnd, &margins);
		}
		break;
		case WM_NCHITTEST:
		{
			LRESULT lr;
			if (!DwmDefWindowProc(hWnd, message, wParam, lParam, &lr))
			{
				lr = CustomFrameHitTest(hWnd, wParam, lParam);
				if (lr != HTCAPTION)
				{
					return lr;
				}
			}
		}
		break;
		case WM_NCDESTROY:
		{
			form->OnFormClosed(form);
			Application::Forms.Remove(form->Handle);
			
			
			
			
			
		}
		break;
		case (WM_USER + 1):
		{
			if (lParam == WM_LBUTTONDOWN || lParam == WM_RBUTTONDOWN)
			{
				if (NotifyIcon::Instance)
				{
					POINT mouseLocation;
					GetCursorPos(&mouseLocation);
					NotifyIcon::Instance->OnNotifyIconMouseDown(NotifyIcon::Instance, MouseEventArgs(
						lParam == WM_LBUTTONDOWN ? MouseButtons::Left : MouseButtons::Right,
						0, mouseLocation.x, mouseLocation.y, 0
					));

					
					if (lParam == WM_RBUTTONDOWN)
					{
						NotifyIcon::Instance->ShowContextMenu(mouseLocation.x, mouseLocation.y);
					}
				}
			}
		}
		break;
		}
	}
	return DefWindowProc(hWnd, message, wParam, lParam);
}