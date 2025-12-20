#include "Form.h"
#include "NotifyIcon.h"
#include <CppUtils/Graphics/Factory.h>
#include <CppUtils/Graphics/Graphics1.h>
#include <algorithm>
#include <functional>
#include <unordered_map>
#include <dwmapi.h>
#include <windowsx.h>
#pragma comment(lib, "Dwmapi.lib")

HCURSOR Form::GetSystemCursor(CursorKind kind)
{
	static std::unordered_map<CursorKind, HCURSOR> cache;
	auto it = cache.find(kind);
	if (it != cache.end() && it->second) return it->second;

	LPCWSTR id = IDC_ARROW;
	switch (kind)
	{
	case CursorKind::Arrow: id = IDC_ARROW; break;
	case CursorKind::Hand: id = IDC_HAND; break;
	case CursorKind::IBeam: id = IDC_IBEAM; break;
	case CursorKind::SizeWE: id = IDC_SIZEWE; break;
	case CursorKind::SizeNS: id = IDC_SIZENS; break;
	case CursorKind::SizeNWSE: id = IDC_SIZENWSE; break;
	case CursorKind::SizeNESW: id = IDC_SIZENESW; break;
	case CursorKind::SizeAll: id = IDC_SIZEALL; break;
	case CursorKind::No: id = IDC_NO; break;
	default: id = IDC_ARROW; break;
	}
	HCURSOR h = LoadCursorW(NULL, id);
	cache.emplace(kind, h);
	return h;
}

void Form::ApplyCursor(CursorKind kind)
{
	HCURSOR desired = GetSystemCursor(kind);
	if (kind == _currentCursor && ::GetCursor() == desired) return;
	_currentCursor = kind;
	::SetCursor(desired);
}

static Control* HitTestDeepestChild(Control* root, POINT contentMouse)
{
	if (!root) return NULL;
	if (!root->Visible || !root->Enable) return NULL;

	for (int i = root->Count - 1; i >= 0; i--)
	{
		auto c = root->operator[](i);
		if (!c || !c->Visible || !c->Enable) continue;
		auto abs = c->AbsLocation;
		auto sz = c->ActualSize();
		if (contentMouse.x >= abs.x && contentMouse.y >= abs.y &&
			contentMouse.x <= abs.x + sz.cx && contentMouse.y <= abs.y + sz.cy)
		{
			auto deeper = HitTestDeepestChild(c, contentMouse);
			return deeper ? deeper : c;
		}
	}
	return root;
}

Control* Form::HitTestControlAt(POINT contentMouse)
{
	for (auto fc : this->ForegroundControls)
	{
		if (!fc || !fc->Visible || !fc->Enable) continue;
		auto loc = fc->Location;
		auto sz = fc->ActualSize();
		if (contentMouse.x >= loc.x && contentMouse.y >= loc.y &&
			contentMouse.x <= (loc.x + sz.cx) && contentMouse.y <= (loc.y + sz.cy))
		{
			return HitTestDeepestChild(fc, contentMouse);
		}
	}

	for (int pass = 0; pass < 2; pass++)
	{
		for (int i = 0; i < this->Controls.Count; i++)
		{
			auto c = this->Controls[i];
			if (!c || !c->Visible || !c->Enable) continue;
			if (this->ForegroundControls.Contains(c)) continue;
			if (pass == 0 && c->Type() != UIClass::UI_ComboBox) continue;
			if (pass == 1 && c->Type() == UIClass::UI_ComboBox) continue;

			auto loc = c->Location;
			auto sz = c->ActualSize();
			if (contentMouse.x >= loc.x && contentMouse.y >= loc.y &&
				contentMouse.x <= (loc.x + sz.cx) && contentMouse.y <= (loc.y + sz.cy))
			{
				return HitTestDeepestChild(c, contentMouse);
			}
		}
	}
	return NULL;
}

CursorKind Form::QueryCursorAt(POINT mouseClient, POINT contentMouse)
{
	const int top = ClientTop();
	if (this->VisibleHead && mouseClient.y < top)
	{
		return CursorKind::Arrow;
	}

	if (this->Selected && this->Selected->IsVisual && (GetAsyncKeyState(VK_LBUTTON) & 0x8000))
	{
		auto abs = this->Selected->AbsLocation;
		int xof = contentMouse.x - abs.x;
		int yof = contentMouse.y - abs.y;
		return this->Selected->QueryCursor(xof, yof);
	}

	auto hit = HitTestControlAt(contentMouse);
	if (!hit) return CursorKind::Arrow;
	auto abs = hit->AbsLocation;
	int xof = contentMouse.x - abs.x;
	int yof = contentMouse.y - abs.y;
	return hit->QueryCursor(xof, yof);
}

void Form::UpdateCursor(POINT mouseClient, POINT contentMouse)
{
	ApplyCursor(QueryCursorAt(mouseClient, contentMouse));
}

void Form::UpdateCursorFromCurrentMouse()
{
	if (!this->Handle) return;
	POINT mouse{};
	GetCursorPos(&mouse);
	ScreenToClient(this->Handle, &mouse);
	POINT contentMouse{ mouse.x, mouse.y - ClientTop() };
	UpdateCursor(mouse, contentMouse);
}

bool Form::TryGetCaptionButtonRect(CaptionButtonKind kind, RECT& out)
{
	if (!this->VisibleHead || this->HeadHeight <= 0) return false;

	int xRight = this->Size.cx;
	int h = this->HeadHeight;
	int w = this->HeadHeight;

	auto place = [&](CaptionButtonKind k, bool enabled) -> std::optional<RECT>
		{
			if (!enabled) return std::nullopt;
			RECT r{ xRight - w, 0, xRight, h };
			xRight -= w;
			return r;
		};

	auto closeR = place(CaptionButtonKind::Close, this->CloseBox);
	auto maxR = place(CaptionButtonKind::Maximize, this->MaxBox);
	auto minR = place(CaptionButtonKind::Minimize, this->MinBox);

	auto pick = [&](CaptionButtonKind k) -> std::optional<RECT>
		{
			if (k == CaptionButtonKind::Close) return closeR;
			if (k == CaptionButtonKind::Maximize) return maxR;
			return minR;
		};

	auto r = pick(kind);
	if (!r.has_value()) return false;
	out = r.value();
	return true;
}

bool Form::HitTestCaptionButtons(POINT ptClient, CaptionButtonKind& outKind)
{
	RECT r{};
	if (TryGetCaptionButtonRect(CaptionButtonKind::Close, r) && PtInRect(&r, ptClient))
	{
		outKind = CaptionButtonKind::Close;
		return true;
	}
	if (TryGetCaptionButtonRect(CaptionButtonKind::Maximize, r) && PtInRect(&r, ptClient))
	{
		outKind = CaptionButtonKind::Maximize;
		return true;
	}
	if (TryGetCaptionButtonRect(CaptionButtonKind::Minimize, r) && PtInRect(&r, ptClient))
	{
		outKind = CaptionButtonKind::Minimize;
		return true;
	}
	return false;
}

void Form::ClearCaptionStates()
{
	_capMinState = CaptionButtonState::None;
	_capMaxState = CaptionButtonState::None;
	_capCloseState = CaptionButtonState::None;
	_capPressed = false;
	_capTracking = false;
}

void Form::UpdateCaptionHover(POINT ptClient)
{
	if (!this->VisibleHead) return;
	CaptionButtonKind hit{};
	bool onBtn = HitTestCaptionButtons(ptClient, hit);

	auto oldMin = _capMinState;
	auto oldMax = _capMaxState;
	auto oldClose = _capCloseState;

	_capMinState = (onBtn && hit == CaptionButtonKind::Minimize) ? CaptionButtonState::Hover : CaptionButtonState::None;
	_capMaxState = (onBtn && hit == CaptionButtonKind::Maximize) ? CaptionButtonState::Hover : CaptionButtonState::None;
	_capCloseState = (onBtn && hit == CaptionButtonKind::Close) ? CaptionButtonState::Hover : CaptionButtonState::None;

	if (_capPressed)
	{
		if (_capPressedKind == CaptionButtonKind::Minimize) _capMinState = CaptionButtonState::Pressed;
		if (_capPressedKind == CaptionButtonKind::Maximize) _capMaxState = CaptionButtonState::Pressed;
		if (_capPressedKind == CaptionButtonKind::Close) _capCloseState = CaptionButtonState::Pressed;
	}

	if (oldMin != _capMinState || oldMax != _capMaxState || oldClose != _capCloseState)
	{
		RECT tr = TitleBarRectClient();
		Invalidate(tr, false);
	}
}

void Form::ExecuteCaptionButton(CaptionButtonKind kind)
{
	switch (kind)
	{
	case CaptionButtonKind::Minimize:
		ShowWindow(this->Handle, SW_MINIMIZE);
		break;
	case CaptionButtonKind::Maximize:
		if (IsZoomed(this->Handle))
			ShowWindow(this->Handle, SW_RESTORE);
		else
			ShowWindow(this->Handle, SW_MAXIMIZE);
		break;
	case CaptionButtonKind::Close:
		this->Close();
		break;
	}
	this->Invalidate(true);
}


void Form::Invalidate(bool immediate)
{
	if (!this->Handle) return;
	this->ControlChanged = true;
	::InvalidateRect(this->Handle, NULL, FALSE);
	if (immediate)
		::UpdateWindow(this->Handle);
}

void Form::Invalidate(const RECT& rc, bool immediate)
{
	if (!this->Handle) return;
	this->ControlChanged = true;
	::InvalidateRect(this->Handle, &rc, FALSE);
	if (immediate)
		::UpdateWindow(this->Handle);
}

void Form::Invalidate(D2D1_RECT_F rc, bool immediate)
{
	RECT r = ToRECT(rc, 2);
	Invalidate(r, immediate);
}

bool Form::RectIntersects(const RECT& a, const RECT& b)
{
	RECT out{};
	return ::IntersectRect(&out, &a, &b) != 0;
}

RECT Form::ToRECT(D2D1_RECT_F r, int inflatePx)
{
	RECT rc{};
	rc.left = (LONG)std::floor(r.left) - inflatePx;
	rc.top = (LONG)std::floor(r.top) - inflatePx;
	rc.right = (LONG)std::ceil(r.right) + inflatePx;
	rc.bottom = (LONG)std::ceil(r.bottom) + inflatePx;
	return rc;
}

void Form::InvalidateControl(Control* c, int inflatePx, bool immediate)
{
	if (!c || !this->Handle) return;
	if (!c->IsVisual) return;
	RECT rc = ToRECT(c->AbsRect, inflatePx);
	OffsetRect(&rc, 0, ClientTop());
	Invalidate(rc, immediate);
}

void Form::InvalidateAnimatedControls(bool immediate)
{
	std::function<void(Control*)> consider;
	consider = [&](Control* c)
		{
			if (!c) return;
			if (!c->IsVisual) return;
			for (int i = 0; i < c->Count; i++)
				consider(c->operator[](i));
		};
	for (auto c : this->Controls) consider(c);
	for (auto c : this->ForegroundControls) consider(c);
	if (immediate)
		::UpdateWindow(this->Handle);
}
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
	auto siz = this->Size;
	siz.cy -= this->HeadHeight;
	return siz;
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
		NULL,
		NULL,
		GetModuleHandleW(0),
		0);
	SetWindowLongPtrW(this->Handle, GWLP_USERDATA, (LONG_PTR)this ^ 0xFFFFFFFFFFFFFFFF);

	DragAcceptFiles(this->Handle, TRUE);


	Application::Forms.Add(this->Handle, this);

	Render = new HwndGraphics1(this->Handle);
	ClearCaptionStates();
}
void Form::Show()
{
	if (this->Icon) SendMessage(this->Handle, WM_SETICON, ICON_BIG, (LPARAM)this->Icon);
	SetWindowLong(this->Handle, GWL_STYLE, WS_POPUP);
	ShowWindow(this->Handle, SW_SHOWNORMAL);
	this->Invalidate(true);
}
static HWND GetBestOwnerWindowInCurrentProcess(HWND exclude = NULL)
{
	HWND fg = GetForegroundWindow();
	if (fg && fg != exclude)
	{
		DWORD pid = 0;
		GetWindowThreadProcessId(fg, &pid);
		if (pid == GetCurrentProcessId() && IsWindowVisible(fg))
			return fg;
	}

	HWND active = GetActiveWindow();
	if (active && active != exclude)
	{
		DWORD pid = 0;
		GetWindowThreadProcessId(active, &pid);
		if (pid == GetCurrentProcessId() && IsWindowVisible(active))
			return active;
	}

	for (auto& kv : Application::Forms)
	{
		HWND h = kv.first;
		if (h && h != exclude && IsWindow(h) && IsWindowVisible(h))
			return h;
	}

	return NULL;
}

void Form::ShowDialog(HWND parent)
{
	HWND owner = parent;
	if (!owner)
		owner = GetBestOwnerWindowInCurrentProcess(this->Handle);

	if (owner && IsWindow(owner))
		SetWindowLongPtrW(this->Handle, GWLP_HWNDPARENT, (LONG_PTR)owner);
	else
		SetWindowLongPtrW(this->Handle, GWLP_HWNDPARENT, 0);

	if (owner && IsWindow(owner))
	{
		EnableWindow(owner, FALSE);
	}

	if (this->Icon) SendMessage(this->Handle, WM_SETICON, ICON_BIG, (LPARAM)this->Icon);
	SetWindowLong(this->Handle, GWL_STYLE, WS_POPUP);
	ShowWindow(this->Handle, SW_SHOWNORMAL);
	this->Invalidate(true);
	SetForegroundWindow(this->Handle);
	SetActiveWindow(this->Handle);

	MSG msg;
	while (IsWindow(this->Handle))
	{
		BOOL r = GetMessageW(&msg, NULL, 0, 0);
		if (r <= 0) break;
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}

	if (owner && IsWindow(owner))
	{
		EnableWindow(owner, TRUE);
		SetForegroundWindow(owner);
		SetActiveWindow(owner);
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
	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		hasMessage = true;
	}
	if (!hasMessage && Application::Forms.size() > 0)
	{
		WaitMessage();
	}
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
	if (!IsWindow(this->Handle)) return false;

	// 旧版 WebBrowser 依赖“抓帧贴图”需要持续 Update；
	// 现在 WebBrowser 使用原生 WebView2 子窗口渲染，不再需要强制刷新。
	if (!force && !ControlChanged) return false;

	RECT dirty{};
	if (!GetUpdateRect(this->Handle, &dirty, FALSE))
		return false;
	return UpdateDirtyRect(dirty, force);
}

bool Form::UpdateDirtyRect(const RECT& dirty, bool force)
{
	if (!IsWindow(this->Handle) || !this->Render) return false;

	if (dirty.right <= dirty.left || dirty.bottom <= dirty.top)
		return false;

	// 注意：
	// - 当前渲染采用“按脏矩形局部重绘”的策略；
	// - 但底层 D2D 渲染目标在很多场景下并不保证保留上一帧内容（尤其是 Resize 之后）；
	// - 同时标题栏/按钮存在半透明填充，如果绘制越界到 dirty 外，会对未清理区域产生“叠加污染”，表现为残影。
	// 因此：本次绘制全程裁剪到 drawRc，并在需要时强制全量重绘。
	RECT clientRc{};
	::GetClientRect(this->Handle, &clientRc);
	RECT drawRc = dirty;
	if (force || !this->_hasRenderedOnce)
	{
		drawRc = clientRc;
	}

	this->Render->BeginRender();
	// 防御：避免上一次绘制遗留 Transform 影响背景/标题栏
	this->Render->ClearTransform();
	// 全程裁剪到本次需要更新的区域，避免对 dirty 外区域产生半透明叠加污染
	this->Render->PushDrawRect((float)drawRc.left, (float)drawRc.top, (float)(drawRc.right - drawRc.left), (float)(drawRc.bottom - drawRc.top));
	// 先用不透明底色清空 drawRc，保证接下来的半透明绘制不叠加历史内容
	this->Render->FillRect((float)drawRc.left, (float)drawRc.top, (float)(drawRc.right - drawRc.left), (float)(drawRc.bottom - drawRc.top), this->BackColor);

	if (this->Image)
	{
		this->Render->PushDrawRect((float)drawRc.left, (float)drawRc.top, (float)(drawRc.right - drawRc.left), (float)(drawRc.bottom - drawRc.top));
		this->RenderImage();
		this->Render->PopDrawRect();
	}

	if (VisibleHead)
	{
		RECT headRc{ 0, 0, this->Size.cx, this->HeadHeight };
		if (RectIntersects(drawRc, headRc))
		{
			this->Render->FillRect(0, 0, this->Size.cx, this->HeadHeight, this->HeadBackColor);
			auto defaultFont = GetDefaultFontObject();
			float headTextTop = (this->HeadHeight - defaultFont->FontHeight) * 0.5f;
			if (headTextTop < 0.0f)
				headTextTop = 0.0f;
			this->Render->PushDrawRect(0, 0, this->Size.cx, this->HeadHeight);
			if (this->CenterTitle)
			{
				auto tSize = defaultFont->GetTextSize(this->Text);
				float textRangeWidth = this->Size.cx;
				int buttonCount = 0;
				if (this->MinBox) buttonCount++;
				if (this->MaxBox) buttonCount++;
				if (this->CloseBox) buttonCount++;
				textRangeWidth -= (this->HeadHeight * buttonCount);
				float headTextLeft = (textRangeWidth - tSize.width) * 0.5f;
				if (headTextLeft < 0.0f)
					headTextLeft = 0.0f;
				this->Render->DrawString(this->Text, headTextLeft, headTextTop, this->ForeColor);
			}
			else
			{
				this->Render->DrawString(this->Text, 5.0f, headTextTop, this->ForeColor);
			}

			auto drawBtn = [&](CaptionButtonKind kind, CaptionButtonState st, D2D1_COLOR_F hover, D2D1_COLOR_F pressed)
				{
					RECT r{};
					if (!TryGetCaptionButtonRect(kind, r)) return;
					if (st == CaptionButtonState::Hover)
						this->Render->FillRect((float)r.left, (float)r.top, (float)(r.right - r.left), (float)(r.bottom - r.top), hover);
					else if (st == CaptionButtonState::Pressed)
						this->Render->FillRect((float)r.left, (float)r.top, (float)(r.right - r.left), (float)(r.bottom - r.top), pressed);

					const float left = (float)r.left;
					const float top = (float)r.top;
					const float bw = (float)(r.right - r.left);
					const float bh = (float)(r.bottom - r.top);
					const float s = (bw < bh) ? bw : bh;
					const float cx = left + bw * 0.5f;
					const float cy = top + bh * 0.5f;

					const float icon = s * 0.42f;
					const float half = icon * 0.5f;
					float stroke = s * 0.08f;
					if (stroke < 1.0f) stroke = 1.0f;

					auto drawMinimize = [&]()
						{
							const float y = cy + half * 0.35f;
							this->Render->DrawLine({ cx - half, y }, { cx + half, y }, this->ForeColor, stroke);
						};
					auto drawMaximize = [&]()
						{
							const float x = cx - half;
							const float y = cy - half;
							this->Render->DrawRect(x, y, icon, icon, this->ForeColor, stroke);
						};
					auto drawRestore = [&]()
						{
							const float off = stroke * 1.2f;
							const float xBack = (cx - half) - off;
							const float yBack = (cy - half) - off;
							const float xFront = (cx - half) + off;
							const float yFront = (cy - half) + off;
							this->Render->DrawRect(xBack, yBack, icon, icon, this->ForeColor, stroke);
							this->Render->DrawRect(xFront, yFront, icon, icon, this->ForeColor, stroke);
						};
					auto drawClose = [&]()
						{
							this->Render->DrawLine({ cx - half, cy - half }, { cx + half, cy + half }, this->ForeColor, stroke);
							this->Render->DrawLine({ cx + half, cy - half }, { cx - half, cy + half }, this->ForeColor, stroke);
						};

					switch (kind)
					{
					case CaptionButtonKind::Minimize:
						drawMinimize();
						break;
					case CaptionButtonKind::Maximize:
						if (IsZoomed(this->Handle))
							drawRestore();
						else
							drawMaximize();
						break;
					case CaptionButtonKind::Close:
						drawClose();
						break;
					}
				};

			drawBtn(CaptionButtonKind::Close, _capCloseState, this->CloseHoverColor, this->ClosePressedColor);
			drawBtn(CaptionButtonKind::Maximize, _capMaxState, this->CaptionHoverColor, this->CaptionPressedColor);
			drawBtn(CaptionButtonKind::Minimize, _capMinState, this->CaptionHoverColor, this->CaptionPressedColor);

			this->Render->PopDrawRect();
		}
	}
	const int top = ClientTop();
	RECT contentDirty = drawRc;
	contentDirty.top -= top;
	contentDirty.bottom -= top;
	if (contentDirty.top < 0) contentDirty.top = 0;
	if (contentDirty.left < 0) contentDirty.left = 0;
	if (contentDirty.right > this->Size.cx) contentDirty.right = this->Size.cx;
	if (contentDirty.bottom > (this->Size.cy - top)) contentDirty.bottom = (this->Size.cy - top);

	if (contentDirty.right > contentDirty.left && contentDirty.bottom > contentDirty.top)
	{
		this->Render->SetTransform(D2D1::Matrix3x2F::Translation(0.0f, (float)top));
		this->Render->PushDrawRect((float)contentDirty.left, (float)contentDirty.top, (float)(contentDirty.right - contentDirty.left), (float)(contentDirty.bottom - contentDirty.top));

		for (int i = 0; i < this->Controls.Count; i++)
		{
			auto c = this->Controls[i]; if (!c->Visible)continue;
			if (this->ForegroundControls.Contains(c)) continue;
			RECT crc = ToRECT(c->AbsRect, 2);
			if (!RectIntersects(contentDirty, crc)) continue;
			if (c->ParentForm->Render == NULL)
				c->ParentForm->Render = this->Render;
			c->Update();
		}
		for (int i = 0; i < this->ForegroundControls.Count; i++)
		{
			auto fc = this->ForegroundControls[i];
			RECT crc = ToRECT(fc->AbsRect, 2);
			if (!RectIntersects(contentDirty, crc)) continue;
			fc->Update();
		}

		this->Render->PopDrawRect();
		this->Render->ClearTransform();
	}

	this->OnPaint(this);

	// 结束本次全程裁剪
	this->Render->PopDrawRect();
	this->Render->EndRender();
	this->ControlChanged = false;
	this->_hasRenderedOnce = true;
	return true;
}
bool Form::ForceUpdate()
{
	this->Invalidate(true);
	return true;
}

bool Form::RemoveControl(Control* c)
{
	if (this->Controls.Contains(c))
	{
		this->Controls.Remove(c);
		this->ForegroundControls.Remove(c);
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
	const int top = ClientTop();
	POINT contentMouse{ mouse.x, mouse.y - top };
	Control* HitControl = NULL;
	switch (message)
	{
	case WM_MOUSEMOVE:
	{
		if (this->VisibleHead && mouse.y < this->HeadHeight)
		{
			UpdateCaptionHover(mouse);
		}
		else if (this->_capMinState != CaptionButtonState::None || this->_capMaxState != CaptionButtonState::None || this->_capCloseState != CaptionButtonState::None)
		{
			if (!this->_capPressed)
			{
				ClearCaptionStates();
				Invalidate(TitleBarRectClient(), false);
			}
		}

		if (this->VisibleHead && mouse.y < top)
		{
			ApplyCursor(CursorKind::Arrow);
			break;
		}

		if (this->Selected && (GetAsyncKeyState(VK_LBUTTON) & 0x8000))
		{
			if (this->Selected->IsVisual)
			{
				HitControl = this->Selected;
				auto location = this->Selected->AbsLocation;
				this->Selected->ProcessMessage(message, wParam, lParam, contentMouse.x - location.x, contentMouse.y - location.y);
				UpdateCursor(mouse, contentMouse);
				break;
			}
		}
		auto lastUnderMouse = this->UnderMouse;
		this->UnderMouse = NULL;
		bool is_First = true;

		// ForegroundControls: 顶层优先命中（用于 Menu 下拉、ToolBar 等）
		for (int i = this->ForegroundControls.Count - 1; i >= 0; i--)
		{
			auto fc = this->ForegroundControls[i];
			if (!fc || !fc->Visible || !fc->Enable) continue;
			auto loc = fc->Location;
			auto size = fc->ActualSize();
			if (contentMouse.x >= loc.x && contentMouse.y >= loc.y &&
				contentMouse.x <= (loc.x + size.cx) && contentMouse.y <= (loc.y + size.cy))
			{
				HitControl = fc;
				this->UnderMouse = fc;
				fc->ProcessMessage(message, wParam, lParam, contentMouse.x - loc.x, contentMouse.y - loc.y);
				goto ext;
			}
		}

	reExc:
		for (int i = 0; i < this->Controls.Count; i++)
		{
			auto c = this->Controls[i];
			if (!c->Visible || !c->Enable)continue;
			if (this->ForegroundControls.Contains(c)) continue;
			auto location = c->Location;
			auto size = c->ActualSize();
			if (
				contentMouse.x >= location.x &&
				contentMouse.y >= location.y &&
				contentMouse.x <= (location.x + size.cx) &&
				contentMouse.y <= (location.y + size.cy)
				)
			{
				if (is_First)
				{
					if (c->Type() == UIClass::UI_ComboBox)
					{
						HitControl = c;
						this->UnderMouse = c;
						c->ProcessMessage(message, wParam, lParam, contentMouse.x - location.x, contentMouse.y - location.y);
						goto ext;
					}
				}
				else
				{
					HitControl = c;
					this->UnderMouse = c;
					c->ProcessMessage(message, wParam, lParam, contentMouse.x - location.x, contentMouse.y - location.y);
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

		UpdateCursor(mouse, contentMouse);
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
		// WebBrowser(WebView2) 是真实 HWND 子窗口：当它获得焦点后，键盘/IME 消息会发往子窗口，
		// 这会导致框架内的自绘控件（GridView 编辑等）无法继续收到 WM_CHAR/WM_IME_*。
		// 解决：当鼠标点击目标不是 WebBrowser 时，显式把焦点抢回到 Form。
		if (message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN || message == WM_MBUTTONDOWN)
		{
			// 标题栏区域不参与控件输入焦点（但不影响拖动/系统按钮）
			if (!(this->VisibleHead && mouse.y < top))
			{
				Control* hit = HitTestControlAt(contentMouse);
				if (!(hit && hit->Type() == UIClass::UI_WebBrowser))
				{
					if (::GetFocus() != this->Handle)
						::SetFocus(this->Handle);
				}
			}
		}

		if (WM_LBUTTONDOWN == message)
		{
			if (VisibleHead)
			{
				CaptionButtonKind kind{};
				if (HitTestCaptionButtons(mouse, kind))
				{
					_capPressed = true;
					_capPressedKind = kind;
					_capTracking = true;
					UpdateCaptionHover(mouse);
					SetCapture(this->Handle);
					break;
				}

				if (mouse.y < top)
				{
					ReleaseCapture();
					PostMessage(this->Handle, WM_SYSCOMMAND, SC_MOVE | HTCAPTION, 0);
				}
			}
		}
		else if (WM_LBUTTONUP == message)
		{
			if (_capTracking)
			{
				ReleaseCapture();
				_capTracking = false;
				CaptionButtonKind kind{};
				bool hit = HitTestCaptionButtons(mouse, kind);
				if (_capPressed && hit && kind == _capPressedKind)
				{
					_capPressed = false;
					ClearCaptionStates();
					ExecuteCaptionButton(kind);
					UpdateCursor(mouse, contentMouse);
					break;
				}
				_capPressed = false;
				ClearCaptionStates();
				Invalidate(TitleBarRectClient(), false);
				UpdateCursor(mouse, contentMouse);
				break;
			}

			if (this->Selected)
			{
				if (this->Selected->IsVisual)
				{
					HitControl = this->Selected;
					auto location = this->Selected->AbsLocation;
					this->Selected->ProcessMessage(message, wParam, lParam, contentMouse.x - location.x, contentMouse.y - location.y);
					UpdateCursor(mouse, contentMouse);
					break;
				}
			}
		}
		else if (WM_LBUTTONDBLCLK == message)
		{
			if (VisibleHead && mouse.y < this->HeadHeight)
			{
				CaptionButtonKind kind{};
				if (!HitTestCaptionButtons(mouse, kind))
				{
					ExecuteCaptionButton(CaptionButtonKind::Maximize);
					break;
				}
			}
		}
		if (this->VisibleHead && mouse.y < top)
		{
			break;
		}
		bool is_First = true;

		// ForegroundControls: 顶层优先命中并吞掉事件（用于 Menu 下拉、ToolBar 等）
		for (int i = this->ForegroundControls.Count - 1; i >= 0; i--)
		{
			auto fc = this->ForegroundControls[i];
			if (!fc || !fc->Visible || !fc->Enable) continue;
			auto loc = fc->Location;
			auto size = fc->ActualSize();
			if (contentMouse.x >= loc.x && contentMouse.y >= loc.y &&
				contentMouse.x <= (loc.x + size.cx) && contentMouse.y <= (loc.y + size.cy))
			{
				HitControl = fc;
				fc->ProcessMessage(message, wParam, lParam, contentMouse.x - loc.x, contentMouse.y - loc.y);
				goto ext1;
			}
		}

	reExc1:
		for (int i = 0; i < this->Controls.Count; i++)
		{
			auto c = this->Controls[i]; if (!c->Visible)continue;
			if (this->ForegroundControls.Contains(c)) continue;
			auto location = c->Location;
			auto size = c->ActualSize();
			if (
				contentMouse.x >= location.x &&
				contentMouse.y >= location.y &&
				contentMouse.x <= (location.x + size.cx) &&
				contentMouse.y <= (location.y + size.cy)
				)
			{
				if (is_First)
				{
					if (c->Type() == UIClass::UI_ComboBox)
					{
						HitControl = c;
						c->ProcessMessage(message, wParam, lParam, contentMouse.x - location.x, contentMouse.y - location.y);
						goto ext1;
					}
				}
				else
				{
					HitControl = c;
					c->ProcessMessage(message, wParam, lParam, contentMouse.x - location.x, contentMouse.y - location.y);
				}
			}
		}
		if (is_First)
		{
			is_First = false;
			goto reExc1;
		}
	ext1:;
		if (message == WM_LBUTTONUP || message == WM_RBUTTONUP || message == WM_MBUTTONUP)
			UpdateCursor(mouse, contentMouse);
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
		this->Render->ReSize(width, height);
		// Resize 后 D2D 的后台缓冲内容通常不再可信，强制下一帧全量重绘
		this->_hasRenderedOnce = false;
		this->OnSizeChanged(this);
		this->Invalidate(false);
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
		this->Render = NULL;
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
	return true;
}
void Form::RenderImage()
{
	if (this->Image)
	{
		auto size = this->Image->GetSize();
		if (size.width > 0 && size.height > 0)
		{
			// 自绘标题栏属于 client 区域的一部分：背景图应铺满整个窗口区域（0..Size）
			auto asize = this->Size;
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
			auto loc = c->Location;
			auto siz = c->ActualSize();
			auto tmp = D2D1_POINT_2F{ (float)loc.x + siz.cx,(float)loc.y + siz.cy };
			if (tmp.x < left)left = tmp.x;
			if (tmp.x > right)right = tmp.x;

			if (tmp.y < top)top = tmp.y;
			if (tmp.y > bottom)bottom = tmp.y;
		}
	}
	return D2D1_RECT_F{ left,top,right,bottom };
}
LRESULT CustomFrameHitTest(HWND _hWnd, WPARAM wParam, LPARAM lParam, int captionHeight)
{
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
	else if (captionHeight > 0 && ptMouse.y < wr.top + captionHeight)
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
		case WM_SETCURSOR:
		{
			if (LOWORD(lParam) == HTCLIENT)
			{
				form->UpdateCursorFromCurrentMouse();
				return TRUE;
			}
		}
		break;
		case WM_ERASEBKGND:
			return 1;
		case WM_PAINT:
		{
			PAINTSTRUCT ps{};
			BeginPaint(hWnd, &ps);
			if (form->Render)
			{
				// 检查是否有可见的 WebBrowser 控件
				bool hasVisibleWebBrowser = false;
				std::function<void(Control*)> checkWebBrowser;
				checkWebBrowser = [&](Control* c) {
					if (!c || !c->Visible) return;
					if (c->Type() == UIClass::UI_WebBrowser) {
						hasVisibleWebBrowser = true;
						return;
					}
					for (int i = 0; i < c->Count && !hasVisibleWebBrowser; i++)
						checkWebBrowser(c->operator[](i));
				};
				
				for (auto c : form->Controls)
					if (!hasVisibleWebBrowser)
						checkWebBrowser(c);
				for (auto c : form->ForegroundControls)
					if (!hasVisibleWebBrowser)
						checkWebBrowser(c);
				
				// 如果有 WebBrowser 或 ControlChanged，则更新
				if (hasVisibleWebBrowser || form->ControlChanged || !form->_hasRenderedOnce)
				{
					form->UpdateDirtyRect(ps.rcPaint, true);
				}
				else if (form->_hasRenderedOnce)
				{
					// 无 WebBrowser 且无变化时，只验证区域
					form->UpdateDirtyRect(ps.rcPaint, true);
				}
			}
			EndPaint(hWnd, &ps);
			return 0;
		}
		case WM_TIMER:
		{
			if (wParam == form->_animTimerId)
			{
				form->InvalidateAnimatedControls(true);
				return 0;
			}
		}
		break;
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
				lr = CustomFrameHitTest(hWnd, wParam, lParam, form->ClientTop());
				if (lr == HTCAPTION)
				{
					POINT ptClient{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
					ScreenToClient(hWnd, &ptClient);
					CaptionButtonKind k{};
					if (form->HitTestCaptionButtons(ptClient, k))
						return HTCLIENT;
				}
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