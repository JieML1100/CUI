#include "Form.h"
#include "NotifyIcon.h"
#include <CppUtils/Graphics/Factory.h>
#include <CppUtils/Graphics/Graphics.h>
#include <algorithm>
#include <functional>
#include <dwmapi.h>
#include <windowsx.h>
#pragma comment(lib, "Dwmapi.lib")

bool Form::TryGetCaptionButtonRect(CaptionButtonKind kind, RECT& out)
{
	if (!this->VisibleHead || this->HeadHeight <= 0) return false;

	// 右侧从右往左排：Close / Max / Min（可按开关隐藏）
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

	// hover state
	_capMinState = (onBtn && hit == CaptionButtonKind::Minimize) ? CaptionButtonState::Hover : CaptionButtonState::None;
	_capMaxState = (onBtn && hit == CaptionButtonKind::Maximize) ? CaptionButtonState::Hover : CaptionButtonState::None;
	_capCloseState = (onBtn && hit == CaptionButtonKind::Close) ? CaptionButtonState::Hover : CaptionButtonState::None;

	// 如果按下中，则保持 pressed 的视觉优先
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

int Form::ComputeDesiredFrameIntervalMs()
{
	int best = 0;
	std::function<void(Control*)> consider;
	consider = [&](Control* c)
	{
		if (!c) return;
		if (!c->IsVisual) return;
		int v = c->DesiredFrameIntervalMs();
		if (v > 0)
		{
			if (best <= 0) best = v;
			else best = std::min(best, v);
		}
		// 递归扫描子控件（Count/operator[] 可用）
		for (int i = 0; i < c->Count; i++)
			consider(c->operator[](i));
	};
	for (auto c : this->Controls) consider(c);
	for (auto c : this->ForegroundControls) consider(c);
	// 避免 0/过小 interval：至少 10ms（给媒体播放器/动画留空间）
	if (best > 0 && best < 10) best = 10;
	return best;
}

void Form::UpdateAnimationTimer()
{
	if (!this->Handle) return;
	UINT newInterval = (UINT)ComputeDesiredFrameIntervalMs();
	if (newInterval == 0)
	{
		if (this->_animIntervalMs != 0)
		{
			KillTimer(this->Handle, this->_animTimerId);
			this->_animIntervalMs = 0;
		}
		return;
	}
	if (this->_animIntervalMs != newInterval)
	{
		// 重新设定最小刷新间隔（所有需要持续刷新的控件取 min）
		if (this->_animIntervalMs != 0)
			KillTimer(this->Handle, this->_animTimerId);
		SetTimer(this->Handle, this->_animTimerId, newInterval, NULL);
		this->_animIntervalMs = newInterval;
	}
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

// 适配 Control::PostRender 传入的 D2D1_RECT_F
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
	// 控件坐标系：以“客户区内容(标题栏下方)”为原点，需要转换到窗口客户区坐标
	OffsetRect(&rc, 0, ClientTop());
	Invalidate(rc, immediate);
}

void Form::InvalidateAnimatedControls(bool immediate)
{
	// 只无效化“需要持续刷新”的控件区域（例如：输入光标/动画/未来的视频播放器控件）
	std::function<void(Control*)> consider;
	consider = [&](Control* c)
	{
		if (!c) return;
		if (!c->IsVisual) return;
		if (c->DesiredFrameIntervalMs() > 0)
		{
			D2D1_RECT_F anim{};
			if (c->GetAnimatedInvalidRect(anim))
			{
				RECT rc = ToRECT(anim, 2);
				OffsetRect(&rc, 0, ClientTop());
				Invalidate(rc, false);
			}
			else
			{
				InvalidateControl(c, 2, false);
			}
		}
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
		// 重要：不要把窗口设为其他窗口的 owner（owned window 默认不出现在任务栏，
		// 且最小化/还原会跟随 owner，导致你描述的“只能还原一个窗口”等异常）
		NULL,
		NULL,
		GetModuleHandleW(0),
		0);
	SetWindowLongPtrW(this->Handle, GWLP_USERDATA, (LONG_PTR)this ^ 0xFFFFFFFFFFFFFFFF);
	
	DragAcceptFiles(this->Handle, TRUE);


	Application::Forms.Add(this->Handle, this);

	Render = new HwndGraphics(this->Handle);
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
	// 1) 优先使用当前前台窗口（且属于本进程）
	HWND fg = GetForegroundWindow();
	if (fg && fg != exclude)
	{
		DWORD pid = 0;
		GetWindowThreadProcessId(fg, &pid);
		if (pid == GetCurrentProcessId() && IsWindowVisible(fg))
			return fg;
	}

	// 2) 其次使用当前激活窗口（且属于本进程）
	HWND active = GetActiveWindow();
	if (active && active != exclude)
	{
		DWORD pid = 0;
		GetWindowThreadProcessId(active, &pid);
		if (pid == GetCurrentProcessId() && IsWindowVisible(active))
			return active;
	}

	// 3) 再从 Application::Forms 里找任意一个（不是自己）
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
	// 选择 owner：未指定则自动选取（如果没有其他窗口则保持 NULL）
	HWND owner = parent;
	if (!owner)
		owner = GetBestOwnerWindowInCurrentProcess(this->Handle);

	// 将当前窗口设置为 owned window（对话框语义：不单独占任务栏、跟随 owner）
	if (owner && IsWindow(owner))
		SetWindowLongPtrW(this->Handle, GWLP_HWNDPARENT, (LONG_PTR)owner);
	else
		SetWindowLongPtrW(this->Handle, GWLP_HWNDPARENT, 0);

	// 禁用 owner，形成真正的“模态”
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

	// 模态消息循环：需要分发所有窗口消息（否则托盘/其他窗体绘制会异常）
	MSG msg;
	while (IsWindow(this->Handle))
	{
		BOOL r = GetMessageW(&msg, NULL, 0, 0);
		if (r <= 0) break; // WM_QUIT or error

		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}

	// 恢复 owner
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
	// 关键：如果窗口都关了，不能阻塞，否则 main.cpp 无法退出循环
	if (!hasMessage && Application::Forms.size() > 0)
	{
		// 进入等待，避免空转占用 CPU；WM_TIMER/输入/绘制都会唤醒
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
	// 默认 Update：从系统获取当前脏区，然后走 UpdateDirtyRect
	if (!IsWindow(this->Handle)) return false;
	if (!(force || ControlChanged)) return false;

	RECT dirty{};
	if (!GetUpdateRect(this->Handle, &dirty, FALSE))
		return false;
	return UpdateDirtyRect(dirty, force);
}

bool Form::UpdateDirtyRect(const RECT& dirty, bool force)
{
	if (!IsWindow(this->Handle) || !this->Render) return false;

	// 空矩形直接跳过
	if (dirty.right <= dirty.left || dirty.bottom <= dirty.top)
		return false;

	UpdateAnimationTimer();

	this->Render->BeginRender();
	// 局部刷新：只填充脏区背景（不要用 Clear，它会清全屏）
	this->Render->FillRect((float)dirty.left, (float)dirty.top, (float)(dirty.right - dirty.left), (float)(dirty.bottom - dirty.top), this->BackColor);

	if (this->Image)
	{
		// 图片绘制通常覆盖全窗，使用脏区 clip 降低开销
		this->Render->PushDrawRect((float)dirty.left, (float)dirty.top, (float)(dirty.right - dirty.left), (float)(dirty.bottom - dirty.top));
		this->RenderImage();
		this->Render->PopDrawRect();
	}

	if (VisibleHead)
	{
		// head 如果与脏区相交再画（否则跳过）
		RECT headRc{ 0, 0, this->Size.cx, this->HeadHeight };
		if (RectIntersects(dirty, headRc))
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
				// 标题文本区域：去掉右侧 caption buttons 区域
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

			// 画 caption buttons
			auto drawBtn = [&](CaptionButtonKind kind, CaptionButtonState st, const wchar_t* glyph, D2D1_COLOR_F hover, D2D1_COLOR_F pressed)
			{
				RECT r{};
				if (!TryGetCaptionButtonRect(kind, r)) return;
				if (st == CaptionButtonState::Hover)
					this->Render->FillRect((float)r.left, (float)r.top, (float)(r.right - r.left), (float)(r.bottom - r.top), hover);
				else if (st == CaptionButtonState::Pressed)
					this->Render->FillRect((float)r.left, (float)r.top, (float)(r.right - r.left), (float)(r.bottom - r.top), pressed);

				// 简单居中绘制 glyph（后续可换成矢量图标）
				auto sz = defaultFont->GetTextSize(glyph);
				float cx = r.left + ((r.right - r.left) - sz.width) * 0.5f;
				float cy = r.top + ((r.bottom - r.top) - sz.height) * 0.5f;
				if (cx < (float)r.left) cx = (float)r.left;
				if (cy < (float)r.top) cy = (float)r.top;
				this->Render->DrawString(glyph, cx, cy, this->ForeColor);
			};

			// close
			drawBtn(CaptionButtonKind::Close, _capCloseState, L"✕", this->CloseHoverColor, this->ClosePressedColor);
			// max/restore glyph
			drawBtn(CaptionButtonKind::Maximize, _capMaxState, IsZoomed(this->Handle) ? L"⬜" : L"❐", this->CaptionHoverColor, this->CaptionPressedColor);
			// min
			drawBtn(CaptionButtonKind::Minimize, _capMinState, L"―", this->CaptionHoverColor, this->CaptionPressedColor);

			this->Render->PopDrawRect();
		}
	}

	// -------- 绘制客户区内容（控件坐标系：0,0 从标题栏下方开始）--------
	const int top = ClientTop();
	RECT contentDirty = dirty;
	contentDirty.top -= top;
	contentDirty.bottom -= top;
	// 裁剪到内容区域
	if (contentDirty.top < 0) contentDirty.top = 0;
	if (contentDirty.left < 0) contentDirty.left = 0;
	if (contentDirty.right > this->Size.cx) contentDirty.right = this->Size.cx;
	if (contentDirty.bottom > (this->Size.cy - top)) contentDirty.bottom = (this->Size.cy - top);

	if (contentDirty.right > contentDirty.left && contentDirty.bottom > contentDirty.top)
	{
		// 注意：clip 需要在设置 transform 之后 push，避免坐标系错位
		this->Render->SetTransform(D2D1::Matrix3x2F::Translation(0.0f, (float)top));
		this->Render->PushDrawRect((float)contentDirty.left, (float)contentDirty.top, (float)(contentDirty.right - contentDirty.left), (float)(contentDirty.bottom - contentDirty.top));

		for (int i = 0; i < this->Controls.Count; i++)
		{
			auto c = this->Controls[i]; if (!c->Visible)continue;
			RECT crc = ToRECT(c->AbsRect, 2);
			if (!RectIntersects(contentDirty, crc)) continue;
			if (c->ParentForm->Render == NULL)
				c->ParentForm->Render = this->Render;
			c->Update();
		}
		for (auto fc : this->ForegroundControls)
		{
			RECT crc = ToRECT(fc->AbsRect, 2);
			if (!RectIntersects(contentDirty, crc)) continue;
			fc->Update();
		}

		this->Render->PopDrawRect();
		this->Render->ClearTransform();
	}

	// OnPaint 维持“窗口客户区坐标系”（包含标题栏），避免破坏现有用法
	this->OnPaint(this);

	this->Render->EndRender();
	this->ControlChanged = false;
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
		// 标题栏按钮 hover/pressed 状态更新（即使不在控件上也需要）
		if (this->VisibleHead && mouse.y < this->HeadHeight)
		{
			UpdateCaptionHover(mouse);
		}
		else if (this->_capMinState != CaptionButtonState::None || this->_capMaxState != CaptionButtonState::None || this->_capCloseState != CaptionButtonState::None)
		{
			// 离开标题栏后清 hover
			if (!this->_capPressed)
			{
				ClearCaptionStates();
				Invalidate(TitleBarRectClient(), false);
			}
		}

		// 鼠标在标题栏区域：不参与控件命中（避免与标题栏逻辑冲突）
		if (this->VisibleHead && mouse.y < top)
		{
			break;
		}

		if (this->Selected && (GetAsyncKeyState(VK_LBUTTON) & 0x8000))
		{
			if (this->Selected->IsVisual)
			{
				HitControl = this->Selected;
				auto location = this->Selected->AbsLocation;
				this->Selected->ProcessMessage(message, wParam, lParam, contentMouse.x - location.x, contentMouse.y - location.y);
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
				// 1) 若点在 caption button 上：按下状态
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

				// 2) 标题栏空白区域：拖动窗口（排除按钮区域）
				if (mouse.y < top)
				{
					ReleaseCapture();
					PostMessage(this->Handle, WM_SYSCOMMAND, SC_MOVE | HTCAPTION, 0);
				}
			}
		}
		else if (WM_LBUTTONUP == message)
		{
			// 释放 caption button
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
					break;
				}
				_capPressed = false;
				ClearCaptionStates();
				Invalidate(TitleBarRectClient(), false);
				break;
			}

			if (this->Selected)
			{
				if (this->Selected->IsVisual)
				{
					HitControl = this->Selected;
					auto location = this->Selected->AbsLocation;
					this->Selected->ProcessMessage(message, wParam, lParam, contentMouse.x - location.x, contentMouse.y - location.y);
					break;
				}
			}
		}
		else if (WM_LBUTTONDBLCLK == message)
		{
			// 标题栏双击：切换最大化/还原（排除按钮区域）
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
		// 点击在标题栏区域（且不是 caption buttons）：不再向控件派发
		if (this->VisibleHead && mouse.y < top)
		{
			break;
		}
		bool is_First = true;
	reExc1:
		for (int i = 0; i < this->Controls.Count; i++)
		{
			auto c = this->Controls[i]; if (!c->Visible)continue;
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
		// WM_CLOSE 之后窗口仍可能收到少量消息；这里必须把 Render 置空，避免悬空指针被再次使用
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
	// 统一走 Invalidate + WM_PAINT，避免在消息处理过程中直接绘制导致重入/闪烁
	UpdateAnimationTimer();
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
		case WM_ERASEBKGND:
			// D2D 自己全量绘制，避免 GDI 擦背景导致闪烁
			return 1;
		case WM_PAINT:
		{
			PAINTSTRUCT ps{};
			BeginPaint(hWnd, &ps);
			// 关键修复：WM_PAINT 内不能再用 GetUpdateRect（BeginPaint 后脏区会被验证）
			// 直接使用 ps.rcPaint 做局部绘制，避免窗口“永远不显示/像最小化”
			if (form->Render)
				form->UpdateDirtyRect(ps.rcPaint, true);
			EndPaint(hWnd, &ps);
			return 0;
		}
		case WM_TIMER:
		{
			if (wParam == form->_animTimerId)
			{
				// 局部刷新：只无效化需要持续刷新的控件区域
				// 重要：WM_PAINT 属于“空闲时”消息；在某些消息循环/高频消息场景下可能被饿死，
				// 导致光标闪烁等动画只能在其它事件触发重绘时才更新。
				// 这里主动 UpdateWindow，确保定时刷新可见。
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
				// 鼠标在自绘 caption buttons 上时，必须返回 HTCLIENT，否则会被当作 HTCAPTION 导致无法点击
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