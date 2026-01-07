#include "Form.h"
#include "NotifyIcon.h"
#include <algorithm>
#include <functional>
#include <unordered_map>
#include <oleidl.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <windowsx.h>
#pragma comment(lib, "Dwmapi.lib")

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif

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
	if (!root->HitTestChildren())
		return root;

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

static bool PointInControlRect(Control* c, POINT contentMouse)
{
	if (!c) return false;
	if (!c->Visible || !c->Enable) return false;
	auto loc = c->AbsLocation;
	auto sz = c->ActualSize();
	return (contentMouse.x >= loc.x && contentMouse.y >= loc.y &&
		contentMouse.x <= loc.x + sz.cx && contentMouse.y <= loc.y + sz.cy);
}

Control* Form::HitTestControlAt(POINT contentMouse)
{
	// 1) 置顶控件优先命中（ComboBox 下拉等）
	if (this->ForegroundControl && this->ForegroundControl->Visible && this->ForegroundControl->Enable)
	{
		auto* fc = this->ForegroundControl;
		if (PointInControlRect(fc, contentMouse))
		{
			return HitTestDeepestChild(fc, contentMouse);
		}
	}

	// 2) 主菜单单独优先命中（包含下拉区域）
	if (this->MainMenu && this->MainMenu->Visible && this->MainMenu->Enable)
	{
		auto* m = this->MainMenu;
		if (PointInControlRect(m, contentMouse))
		{
			return HitTestDeepestChild(m, contentMouse);
		}
	}

	// 3) 状态栏：置顶于普通控件（但优先级低于主菜单与前景控件）
	if (this->MainStatusBar && this->MainStatusBar->TopMost && this->MainStatusBar->Visible && this->MainStatusBar->Enable)
	{
		auto* sb = this->MainStatusBar;
		if (PointInControlRect(sb, contentMouse))
		{
			return HitTestDeepestChild(sb, contentMouse);
		}
	}

	// 4) 普通控件：按绘制顺序倒序命中（后绘制者优先）
	for (int i = this->Controls.Count - 1; i >= 0; i--)
	{
		auto c = this->Controls[i];
		if (!c || !c->Visible || !c->Enable) continue;
		if (c == this->ForegroundControl) continue;
		if (c == this->MainMenu) continue;
		if (this->MainStatusBar && this->MainStatusBar->TopMost && c == this->MainStatusBar) continue;
		if (!PointInControlRect(c, contentMouse)) continue;
		return HitTestDeepestChild(c, contentMouse);
	}
	return NULL;
}

static bool DataObjectHasFormat(IDataObject* pDataObj, CLIPFORMAT cf)
{
	if (!pDataObj) return false;
	FORMATETC fmt{};
	fmt.cfFormat = cf;
	fmt.dwAspect = DVASPECT_CONTENT;
	fmt.lindex = -1;
	fmt.tymed = TYMED_HGLOBAL;
	return SUCCEEDED(pDataObj->QueryGetData(&fmt));
}

static std::optional<List<std::wstring>> TryExtractDroppedFiles(IDataObject* pDataObj)
{
	if (!pDataObj) return std::nullopt;
	FORMATETC fmt{};
	fmt.cfFormat = CF_HDROP;
	fmt.dwAspect = DVASPECT_CONTENT;
	fmt.lindex = -1;
	fmt.tymed = TYMED_HGLOBAL;
	STGMEDIUM stg{};
	if (FAILED(pDataObj->GetData(&fmt, &stg))) return std::nullopt;

	List<std::wstring> files;
	HDROP hDrop = (HDROP)GlobalLock(stg.hGlobal);
	if (hDrop)
	{
		UINT count = DragQueryFile(hDrop, 0xFFFFFFFF, NULL, 0);
		WCHAR buf[MAX_PATH];
		for (UINT i = 0; i < count; i++)
		{
			buf[0] = 0;
			DragQueryFileW(hDrop, i, buf, MAX_PATH);
			files.Add(buf);
		}
		GlobalUnlock(stg.hGlobal);
	}
	ReleaseStgMedium(&stg);
	if (files.Count <= 0) return std::nullopt;
	return files;
}

static std::optional<std::wstring> TryExtractDroppedText(IDataObject* pDataObj)
{
	if (!pDataObj) return std::nullopt;
	CLIPFORMAT fmtText = CF_UNICODETEXT;
	if (!DataObjectHasFormat(pDataObj, fmtText))
	{
		fmtText = CF_TEXT;
		if (!DataObjectHasFormat(pDataObj, fmtText))
			return std::nullopt;
	}

	FORMATETC fmt{};
	fmt.cfFormat = fmtText;
	fmt.dwAspect = DVASPECT_CONTENT;
	fmt.lindex = -1;
	fmt.tymed = TYMED_HGLOBAL;
	STGMEDIUM stg{};
	if (FAILED(pDataObj->GetData(&fmt, &stg))) return std::nullopt;

	std::optional<std::wstring> result;
	void* p = GlobalLock(stg.hGlobal);
	if (p)
	{
		if (fmtText == CF_UNICODETEXT)
		{
			result = std::wstring((const wchar_t*)p);
		}
		else
		{
			// ANSI -> UTF-16
			const char* s = (const char*)p;
			int len = (int)strlen(s);
			int wlen = MultiByteToWideChar(CP_ACP, 0, s, len, NULL, 0);
			std::wstring ws;
			ws.resize(wlen);
			if (wlen > 0)
				MultiByteToWideChar(CP_ACP, 0, s, len, ws.data(), wlen);
			result = std::move(ws);
		}
		GlobalUnlock(stg.hGlobal);
	}
	ReleaseStgMedium(&stg);
	if (result && result->empty()) return std::nullopt;
	return result;
}

class FormDropTarget final : public IDropTarget
{
public:
	explicit FormDropTarget(Form* f) : _ref(1), _form(f) {}

	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
	{
		if (!ppvObject) return E_POINTER;
		*ppvObject = nullptr;
		if (riid == IID_IUnknown || riid == IID_IDropTarget)
		{
			*ppvObject = static_cast<IDropTarget*>(this);
			AddRef();
			return S_OK;
		}
		return E_NOINTERFACE;
	}
	ULONG STDMETHODCALLTYPE AddRef(void) override { return InterlockedIncrement(&_ref); }
	ULONG STDMETHODCALLTYPE Release(void) override
	{
		ULONG r = InterlockedDecrement(&_ref);
		if (r == 0) delete this;
		return r;
	}

	HRESULT STDMETHODCALLTYPE DragEnter(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override
	{
		(void)grfKeyState;
		_lastDataObj = pDataObj;
		return DragOver(grfKeyState, pt, pdwEffect);
	}
	HRESULT STDMETHODCALLTYPE DragOver(DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override
	{
		(void)grfKeyState;
		if (!pdwEffect) return E_POINTER;
		*pdwEffect = DROPEFFECT_NONE;
		if (!_form || !_form->Handle) return S_OK;

		POINT client{ pt.x, pt.y };
		ScreenToClient(_form->Handle, &client);
		POINT contentMouse{ client.x, client.y - _form->ClientTop() };
		if (_form->VisibleHead && client.y < _form->ClientTop())
			return S_OK;

		auto* target = _form->HitTestControlAt(contentMouse);
		bool hasFiles = DataObjectHasFormat(_lastDataObj, CF_HDROP);
		bool hasText = DataObjectHasFormat(_lastDataObj, CF_UNICODETEXT) || DataObjectHasFormat(_lastDataObj, CF_TEXT);

		auto canAcceptFiles = [&](Control* c) -> bool { return c && c->OnDropFile.Count() > 0; };
		auto canAcceptText = [&](Control* c) -> bool { return c && c->OnDropText.Count() > 0; };

		bool accept = false;
		if (target)
		{
			if (hasFiles && canAcceptFiles(target)) accept = true;
			else if (hasText && canAcceptText(target)) accept = true;
		}
		else
		{
			if (hasFiles && _form->OnDropFile.Count() > 0) accept = true;
			else if (hasText && _form->OnDropText.Count() > 0) accept = true;
		}

		if (accept)
			*pdwEffect = DROPEFFECT_COPY;
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE DragLeave(void) override
	{
		_lastDataObj = nullptr;
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE Drop(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override
	{
		(void)grfKeyState;
		if (!pdwEffect) return E_POINTER;
		*pdwEffect = DROPEFFECT_NONE;
		if (!_form || !_form->Handle || !pDataObj) return S_OK;

		POINT client{ pt.x, pt.y };
		ScreenToClient(_form->Handle, &client);
		POINT contentMouse{ client.x, client.y - _form->ClientTop() };
		if (_form->VisibleHead && client.y < _form->ClientTop())
			return S_OK;

		auto* target = _form->HitTestControlAt(contentMouse);

		if (auto files = TryExtractDroppedFiles(pDataObj))
		{
			if (target && target->OnDropFile.Count() > 0)
			{
				target->OnDropFile(target, *files);
				*pdwEffect = DROPEFFECT_COPY;
			}
			else if (!target && _form->OnDropFile.Count() > 0)
			{
				_form->OnDropFile(_form, *files);
				*pdwEffect = DROPEFFECT_COPY;
			}
			return S_OK;
		}

		if (auto text = TryExtractDroppedText(pDataObj))
		{
			if (target && target->OnDropText.Count() > 0)
			{
				target->OnDropText(target, *text);
				*pdwEffect = DROPEFFECT_COPY;
			}
			else if (!target && _form->OnDropText.Count() > 0)
			{
				_form->OnDropText(_form, *text);
				*pdwEffect = DROPEFFECT_COPY;
			}
			return S_OK;
		}

		return S_OK;
	}

private:
	volatile LONG _ref;
	Form* _form;
	IDataObject* _lastDataObj = nullptr;
};

static Control* HitTestRootControlAt(Form* f, POINT contentMouse)
{
	if (!f) return NULL;

	// 1) ForegroundControl 顶层优先
	if (f->ForegroundControl && f->ForegroundControl->Visible && f->ForegroundControl->Enable)
	{
		auto* fc = f->ForegroundControl;
		if (PointInControlRect(fc, contentMouse))
			return fc;
	}

	// 2) 主菜单次优先
	if (f->MainMenu && f->MainMenu->Visible && f->MainMenu->Enable)
	{
		auto* m = f->MainMenu;
		if (PointInControlRect(m, contentMouse))
			return (Control*)m;
	}

	// 3) 状态栏（TopMost=true）
	if (f->MainStatusBar && f->MainStatusBar->TopMost && f->MainStatusBar->Visible && f->MainStatusBar->Enable)
	{
		auto* sb = f->MainStatusBar;
		if (PointInControlRect(sb, contentMouse))
			return (Control*)sb;
	}

	// 4) 普通控件按绘制顺序倒序命中
	for (int i = f->Controls.Count - 1; i >= 0; i--)
	{
		auto c = f->Controls[i];
		if (!c || !c->Visible || !c->Enable) continue;
		if (c == f->ForegroundControl) continue;
		if (c == f->MainMenu) continue;
		if (f->MainStatusBar && f->MainStatusBar->TopMost && c == f->MainStatusBar) continue;
		if (!PointInControlRect(c, contentMouse)) continue;
		return c;
	}
	return NULL;
}

static void DismissComboBoxForegroundOnOutsideMouseDown(Form* f, POINT contentMouse, UINT message)
{
	if (!f) return;
	if (message != WM_LBUTTONDOWN && message != WM_RBUTTONDOWN && message != WM_MBUTTONDOWN) return;
	if (!f->ForegroundControl || !f->ForegroundControl->Visible || !f->ForegroundControl->Enable) return;
	if (PointInControlRect(f->ForegroundControl, contentMouse)) return;

	if (f->ForegroundControl->Type() != UIClass::UI_ComboBox) return;
	auto* cb = (ComboBox*)f->ForegroundControl;
	if (!cb->Expand) return;

	cb->Expand = false;
	if (f->ForegroundControl == cb)
		f->ForegroundControl = NULL;

	cb->PostRender();
	f->Invalidate(true);
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

void Form::SetSelectedControl(Control* value, bool postRender)
{
	auto* old = this->Selected;
	if (old == value) return;
	this->Selected = value;
	if (old)
	{
		old->OnLostFocus(old);
		if (postRender) old->PostRender();
	}
	if (value)
	{
		value->OnGotFocus(value);
		if (postRender) value->PostRender();
	}
	this->_focusNotifiedSelected = this->Selected;
}

static void RaiseControlMouseEnterLeave(Form* f, Control* oldHover, Control* newHover, POINT contentMouse)
{
	if (!f) return;
	if (oldHover == newHover) return;

	auto makeArgs = [&](Control* c) -> MouseEventArgs
		{
			if (!c) return MouseEventArgs(MouseButtons::None, 0, 0, 0, 0);
			auto abs = c->AbsLocation;
			return MouseEventArgs(MouseButtons::None, 0, contentMouse.x - abs.x, contentMouse.y - abs.y, 0);
		};

	if (oldHover)
	{
		auto args = makeArgs(oldHover);
		oldHover->OnMouseLeaved(oldHover, args);
		oldHover->PostRender();
	}
	if (newHover)
	{
		auto args = makeArgs(newHover);
		newHover->OnMouseEnter(newHover, args);
		newHover->PostRender();
	}
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
		if (!this->AllowResize)
			break;
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
	// When the window is disabled/hidden (e.g. during a modal dialog), forcing
	// UpdateWindow can create excessive WM_PAINT churn. Let the system schedule paint.
	if (immediate && ::IsWindowVisible(this->Handle) && ::IsWindowEnabled(this->Handle))
		::UpdateWindow(this->Handle);
}

void Form::Invalidate(const RECT& rc, bool immediate)
{
	if (!this->Handle) return;
	this->ControlChanged = true;
	::InvalidateRect(this->Handle, &rc, FALSE);
	if (immediate && ::IsWindowVisible(this->Handle) && ::IsWindowEnabled(this->Handle))
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
	// 单一置顶控件 / 主菜单（有可能不在 Controls 容器里，保险起见单独考虑）
	if (this->ForegroundControl) consider(this->ForegroundControl);
	if (this->MainMenu) consider((Control*)this->MainMenu);
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
	// 触发布局
	_needsLayout = true;
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

class Font* Form::GetFont()
{
	if (this->_font)
		return this->_font;
	return GetScaledDefaultFont();
}

void Form::SetFont(class Font* value)
{
	this->SetFontEx(value, true);
}

void Form::SetFontEx(class Font* value, bool takeOwnership)
{
	if (value == GetDefaultFontObject())
	{
		value = nullptr;
		takeOwnership = false;
	}

	if (value == this->_font)
	{
		this->_ownsFont = takeOwnership;
		return;
	}

	if (this->_font && this->_ownsFont)
	{
		delete this->_font;
	}
	this->_font = value;
	this->_ownsFont = takeOwnership;
	this->ControlChanged = true;
	this->Invalidate(false);
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

GET_CPP(Form, bool, AllowResize)
{
	return this->_allowResize;
}

SET_CPP(Form, bool, AllowResize)
{
	if (this->_allowResize == value)
		return;

	this->_allowResize = value;
	if (!value)
	{
		this->_maxBoxBeforeAllowResize = this->MaxBox;
		this->MaxBox = false;

		if (this->Handle && IsZoomed(this->Handle))
			ShowWindow(this->Handle, SW_RESTORE);
	}
	else
	{
		this->MaxBox = this->_maxBoxBeforeAllowResize;
	}

	ClearCaptionStates();
	Invalidate(TitleBarRectClient(), true);
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
	this->_headHeightBase96 = this->HeadHeight;
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
	EnsureDropTargetRegistered();


	Application::Forms.Add(this->Handle, this);

	// Win7 兼容版：不使用 DirectComposition / swapchain composition
	Render = new HwndGraphics(this->Handle);
	Render->SetDpi(96.0f, 96.0f);
	OverlayRender = nullptr;
	ClearCaptionStates();
}

Font* Form::GetScaledDefaultFont()
{
	UINT dpi = this->_dpi ? this->_dpi : 96;
	if (_scaledDefaultFont && _scaledDefaultFontDpi == dpi)
		return _scaledDefaultFont;
	if (_scaledDefaultFont)
	{
		delete _scaledDefaultFont;
		_scaledDefaultFont = nullptr;
	}
	// 以默认字体为基准按 DPI 缩放字号
	auto base = GetDefaultFontObject();
	const float size = Application::ScaleFloat(base->FontSize, 96, dpi);
	_scaledDefaultFont = new Font(base->FontName, size);
	_scaledDefaultFontDpi = dpi;
	return _scaledDefaultFont;
}

void Form::ScaleControlTreeForDpi(UINT fromDpi, UINT toDpi)
{
	if (fromDpi == 0) fromDpi = 96;
	if (toDpi == 0) toDpi = 96;
	if (fromDpi == toDpi) return;

	std::function<void(Control*)> scale;
	scale = [&](Control* c)
		{
			if (!c) return;
			c->_location.x = Application::ScaleInt(c->_location.x, fromDpi, toDpi);
			c->_location.y = Application::ScaleInt(c->_location.y, fromDpi, toDpi);
			c->_size.cx = Application::ScaleInt(c->_size.cx, fromDpi, toDpi);
			c->_size.cy = Application::ScaleInt(c->_size.cy, fromDpi, toDpi);
			c->_layoutBaseLocation.x = Application::ScaleInt(c->_layoutBaseLocation.x, fromDpi, toDpi);
			c->_layoutBaseLocation.y = Application::ScaleInt(c->_layoutBaseLocation.y, fromDpi, toDpi);
			c->_layoutBaseSize.cx = Application::ScaleInt(c->_layoutBaseSize.cx, fromDpi, toDpi);
			c->_layoutBaseSize.cy = Application::ScaleInt(c->_layoutBaseSize.cy, fromDpi, toDpi);
			c->_minSize.cx = Application::ScaleInt(c->_minSize.cx, fromDpi, toDpi);
			c->_minSize.cy = Application::ScaleInt(c->_minSize.cy, fromDpi, toDpi);
			if (c->_maxSize.cx != INT_MAX) c->_maxSize.cx = Application::ScaleInt(c->_maxSize.cx, fromDpi, toDpi);
			if (c->_maxSize.cy != INT_MAX) c->_maxSize.cy = Application::ScaleInt(c->_maxSize.cy, fromDpi, toDpi);
			c->_margin.Left = Application::ScaleFloat(c->_margin.Left, fromDpi, toDpi);
			c->_margin.Top = Application::ScaleFloat(c->_margin.Top, fromDpi, toDpi);
			c->_margin.Right = Application::ScaleFloat(c->_margin.Right, fromDpi, toDpi);
			c->_margin.Bottom = Application::ScaleFloat(c->_margin.Bottom, fromDpi, toDpi);
			c->_padding.Left = Application::ScaleFloat(c->_padding.Left, fromDpi, toDpi);
			c->_padding.Top = Application::ScaleFloat(c->_padding.Top, fromDpi, toDpi);
			c->_padding.Right = Application::ScaleFloat(c->_padding.Right, fromDpi, toDpi);
			c->_padding.Bottom = Application::ScaleFloat(c->_padding.Bottom, fromDpi, toDpi);

			if (c->_font && c->_ownsFont)
			{
				c->_font->FontSize = Application::ScaleFloat(c->_font->FontSize, fromDpi, toDpi);
			}
			for (int i = 0; i < c->Count; i++)
				scale(c->operator[](i));
		};

	for (auto c : this->Controls)
	{
		// 跳过MainMenu和MainStatusBar，它们会在后面单独处理，避免重复缩放
		if (c == (Control*)this->MainMenu || c == (Control*)this->MainStatusBar)
			continue;
		scale(c);
	}
	//if (this->MainMenu) scale((Control*)this->MainMenu);
	//if (this->MainStatusBar) scale((Control*)this->MainStatusBar);
	if (this->ForegroundControl) scale(this->ForegroundControl);
}

void Form::ApplyDpiChange(UINT newDpi)
{
	if (newDpi == 0) newDpi = 96;
	UINT oldContentDpi = this->_contentDpi ? this->_contentDpi : 96;
	this->_dpi = newDpi;
	if (oldContentDpi == newDpi) return;

	this->HeadHeight = Application::ScaleInt(this->_headHeightBase96, 96, newDpi);
	this->_scaledDefaultFontDpi = 0;

	if (this->_font && this->_ownsFont)
		this->_font->FontSize = Application::ScaleFloat(this->_font->FontSize, oldContentDpi, newDpi);

	ScaleControlTreeForDpi(oldContentDpi, newDpi);
	this->_contentDpi = newDpi;
	this->InvalidateLayout();
	this->_hasRenderedOnce = false;
	this->Invalidate(false);
}

void Form::EnsureInitialDpiApplied()
{
	if (_initialDpiApplied) return;
	_initialDpiApplied = true;
	if (!this->Handle) return;

	Application::EnsureDpiAwareness();
	UINT dpi = Application::GetDpiForWindow(this->Handle);
	if (dpi == 0) dpi = 96;
	this->_dpi = dpi;
	if (this->_contentDpi == 0) this->_contentDpi = 96;
	if (this->_headHeightBase96 <= 0) this->_headHeightBase96 = 24;

	// 1) 首次显示前先把窗口大小按 96->dpi 缩放一次
	if (!this->_initialWindowRectApplied)
	{
		RECT wr{};
		GetWindowRect(this->Handle, &wr);
		const int oldW = wr.right - wr.left;
		const int oldH = wr.bottom - wr.top;
		const int newW = Application::ScaleInt(this->_Size_INTI.cx, 96, dpi);
		const int newH = Application::ScaleInt(this->_Size_INTI.cy, 96, dpi);
		const int x = wr.left + (oldW - newW) / 2;
		const int y = wr.top + (oldH - newH) / 2;
		SetWindowPos(this->Handle, NULL, x, y, newW, newH, SWP_NOZORDER | SWP_NOACTIVATE);
		SyncRenderSizeToClient();
		this->_hasRenderedOnce = false;
		this->Invalidate(false);
	}

	// 2) 控件树/字体/标题栏统一做一次 96->dpi 缩放
	ApplyDpiChange(dpi);
}

Form::~Form()
{
	CleanupResources();
}

void Form::CleanupResources()
{
	if (_resourcesCleaned)
		return;
	_resourcesCleaned = true;
	if (_scaledDefaultFont)
	{
		delete _scaledDefaultFont;
		_scaledDefaultFont = nullptr;
		_scaledDefaultFontDpi = 0;
	}
	if (this->Handle && _dropRegistered)
	{
		RevokeDragDrop(this->Handle);
		_dropRegistered = false;
	}
	if (_dropTarget)
	{
		_dropTarget->Release();
		_dropTarget = nullptr;
	}

	auto isDescendant = [&](Control* root, Control* node, const auto& self) -> bool
		{
			if (!root || !node) return false;
			for (int i = 0; i < root->Count; i++)
			{
				auto child = root->operator[](i);
				if (child == node) return true;
				if (self(child, node, self)) return true;
			}
			return false;
		};

	auto isOwnedByRootControls = [&](Control* node) -> bool
		{
			if (!node) return false;
			for (auto c : this->Controls)
			{
				if (c == node) return true;
				if (isDescendant(c, node, isDescendant)) return true;
			}
			return false;
		};

	if (this->ForegroundControl && !isOwnedByRootControls(this->ForegroundControl))
	{
		delete this->ForegroundControl;
	}
	this->ForegroundControl = nullptr;

	for (auto c : this->Controls)
	{
		delete c;
	}
	this->Controls.Clear();

	this->Selected = nullptr;
	this->UnderMouse = nullptr;
	this->MainMenu = nullptr;

	if (this->Image)
	{
		this->Image->Release();
		this->Image = nullptr;
	}

	if (this->_font && this->_ownsFont)
	{
		delete this->_font;
	}
	this->_font = nullptr;
	this->_ownsFont = false;

	if (OverlayRender)
	{
		delete OverlayRender;
		OverlayRender = nullptr;
	}
	if (Render)
	{
		delete Render;
		Render = nullptr;
	}
	if (_layoutEngine)
	{
		delete _layoutEngine;
		_layoutEngine = nullptr;
	}
}

void Form::EnsureOleInitialized()
{
	static bool inited = false;
	if (inited) return;
	inited = true;
	OleInitialize(NULL);
}

void Form::EnsureDropTargetRegistered()
{
	if (!this->Handle) return;
	if (_dropRegistered) return;
	EnsureOleInitialized();
	if (!_dropTarget)
	{
		_dropTarget = new FormDropTarget(this);
	}
	HRESULT hr = RegisterDragDrop(this->Handle, _dropTarget);
	if (SUCCEEDED(hr))
	{
		_dropRegistered = true;
		DragAcceptFiles(this->Handle, FALSE);
	}
}

void Form::SetLayoutEngine(class LayoutEngine* engine)
{
	if (_layoutEngine)
	{
		delete _layoutEngine;
	}
	_layoutEngine = engine;
	_needsLayout = true;
}

void Form::PerformLayout()
{
	if (!_layoutEngine)
	{
		// 默认布局：支持控件的 Anchor 和 Margin
		SIZE clientSize = this->ClientSize;
		float contentLeft = 0.0f;
		float contentTop = 0.0f;
		float contentWidth = (float)clientSize.cx;
		float contentHeight = (float)clientSize.cy;
		
		for (int i = 0; i < this->Controls.Count; i++)
		{
			auto control = this->Controls[i];
			if (!control || !control->Visible) continue;
			if (control->Type() == UIClass::UI_Menu) continue;
			
			control->EnsureLayoutBase();
			POINT loc = control->_layoutBaseLocation;
			SIZE size = control->_layoutBaseSize;
			Thickness margin = control->Margin;
			uint8_t anchor = control->AnchorStyles;
			HorizontalAlignment hAlign = control->HAlign;
			VerticalAlignment vAlign = control->VAlign;

			float baseLeft = (float)loc.x;
			float baseTop = (float)loc.y;
			if (anchor & AnchorStyles::Left)
			{
				if (margin.Left != 0.0f) baseLeft = margin.Left;
			}
			else
			{
				baseLeft += margin.Left;
			}
			if (anchor & AnchorStyles::Top)
			{
				if (margin.Top != 0.0f) baseTop = margin.Top;
			}
			else
			{
				baseTop += margin.Top;
			}

			float x = contentLeft + baseLeft;
			float y = contentTop + baseTop;
			float w = (float)size.cx;
			float h = (float)size.cy;

			float availableW = contentWidth - margin.Left - margin.Right;
			float availableH = contentHeight - margin.Top - margin.Bottom;
			if (availableW < 0) availableW = 0;
			if (availableH < 0) availableH = 0;
			
			// 应用 Anchor
			if (anchor != AnchorStyles::None)
			{
				// 左右都锚定：宽度随窗口变化
				if ((anchor & AnchorStyles::Left) && (anchor & AnchorStyles::Right))
				{
					w = (float)clientSize.cx - margin.Right - x;
					if (w < 0) w = 0;
				}
				// 只锚定右边：跟随右边缘
				else if (anchor & AnchorStyles::Right)
				{
					x = (float)clientSize.cx - margin.Right - w;
				}
				
				// 上下都锚定：高度随窗口变化
				if ((anchor & AnchorStyles::Top) && (anchor & AnchorStyles::Bottom))
				{
					h = (float)clientSize.cy - margin.Bottom - y;
					if (h < 0) h = 0;
				}
				// 只锚定下边：跟随下边缘
				else if (anchor & AnchorStyles::Bottom)
				{
					y = (float)clientSize.cy - margin.Bottom - h;
				}
			}
			else
			{
				if (hAlign == HorizontalAlignment::Stretch)
				{
					x = margin.Left;
					w = availableW;
				}
				else if (hAlign == HorizontalAlignment::Center)
				{
					x = margin.Left + (availableW - w) / 2.0f;
				}
				else if (hAlign == HorizontalAlignment::Right)
				{
					x = margin.Left + (availableW - w);
				}
				
				if (vAlign == VerticalAlignment::Stretch)
				{
					y = margin.Top;
					h = availableH;
				}
				else if (vAlign == VerticalAlignment::Center)
				{
					y = margin.Top + (availableH - h) / 2.0f;
				}
				else if (vAlign == VerticalAlignment::Bottom)
				{
					y = margin.Top + (availableH - h);
				}
			}

			if (w < 0) w = 0;
			if (h < 0) h = 0;
			
			POINT finalLoc = { (LONG)x, (LONG)y };
			SIZE finalSize = { (LONG)w, (LONG)h };
			control->ApplyLayout(finalLoc, finalSize);
		}
	}
	else
	{
		// 使用布局引擎
		if (_needsLayout || _layoutEngine->NeedsLayout())
		{
			SIZE clientSize = this->ClientSize;
			_layoutEngine->Measure(nullptr, clientSize);
			
			D2D1_RECT_F finalRect = { 
				0, 0, 
				(float)clientSize.cx, 
				(float)clientSize.cy 
			};
			_layoutEngine->Arrange(nullptr, finalRect);
		}
	}
	
	_needsLayout = false;
}

void Form::SyncRenderSizeToClient()
{
	if (!this->Handle || !this->Render) return;
	RECT rc{};
	::GetClientRect(this->Handle, &rc);
	UINT width = (UINT)std::max<LONG>(1, rc.right - rc.left);
	UINT height = (UINT)std::max<LONG>(1, rc.bottom - rc.top);
	this->Render->ReSize(width, height);
	if (this->OverlayRender) this->OverlayRender->ReSize(width, height);
}

void Form::Show()
{
	EnsureInitialDpiApplied();
	if (this->Icon) SendMessage(this->Handle, WM_SETICON, ICON_BIG, (LPARAM)this->Icon);
	SetWindowLong(this->Handle, GWL_STYLE, WS_POPUP);
	ShowWindow(this->Handle, SW_SHOWNORMAL);
	SyncRenderSizeToClient();
	this->OnSizeChanged(this);
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

	EnsureInitialDpiApplied();

	if (this->Icon) SendMessage(this->Handle, WM_SETICON, ICON_BIG, (LPARAM)this->Icon);
	SetWindowLong(this->Handle, GWL_STYLE, WS_POPUP);
	ShowWindow(this->Handle, SW_SHOWNORMAL);
	SyncRenderSizeToClient();
	this->OnSizeChanged(this);
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
	if (!this->Handle) return;
	PostMessageW(this->Handle, WM_CLOSE, 0, 0);
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

	RECT clientRc{};
	::GetClientRect(this->Handle, &clientRc);

	// 在渲染前执行一次布局：否则直接挂在 Form 上的控件不会应用 Margin/Anchor 等布局属性
	if (_needsLayout || (_layoutEngine && _layoutEngine->NeedsLayout()))
	{
		PerformLayout();
	}
	RECT drawRc = dirty;
	if (force || !this->_hasRenderedOnce)
	{
		drawRc = clientRc;
	}

	this->Render->BeginRender();
	this->Render->ClearTransform();
	this->Render->PushDrawRect((float)drawRc.left, (float)drawRc.top, (float)(drawRc.right - drawRc.left), (float)(drawRc.bottom - drawRc.top));
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
			auto font = this->GetFont();
			float headTextTop = (this->HeadHeight - font->FontHeight) * 0.5f;
			if (headTextTop < 0.0f)
				headTextTop = 0.0f;
			this->Render->PushDrawRect(0, 0, this->Size.cx, this->HeadHeight);
			if (this->CenterTitle)
			{
				auto tSize = font->GetTextSize(this->Text);
				float textRangeWidth = this->Size.cx;
				int buttonCount = 0;
				if (this->MinBox) buttonCount++;
				if (this->MaxBox) buttonCount++;
				if (this->CloseBox) buttonCount++;
				textRangeWidth -= (this->HeadHeight * buttonCount);
				float headTextLeft = (textRangeWidth - tSize.width) * 0.5f;
				if (headTextLeft < 0.0f)
					headTextLeft = 0.0f;
				this->Render->DrawString(this->Text, headTextLeft, headTextTop, this->ForeColor, font);
			}
			else
			{
				this->Render->DrawString(this->Text, 5.0f, headTextTop, this->ForeColor, font);
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
			// 主菜单/置顶控件在有 Overlay 时由 Overlay 层单独绘制，避免重复
			if (this->OverlayRender)
			{
				if (c == this->ForegroundControl) continue;
				if (c == this->MainMenu) continue;
				if (this->MainStatusBar && this->MainStatusBar->TopMost && c == this->MainStatusBar) continue;
			}
			// 状态栏（TopMost=true）单独绘制，避免被普通控件覆盖
			if (this->MainStatusBar && this->MainStatusBar->TopMost && c == this->MainStatusBar)
				continue;
			RECT crc = ToRECT(c->AbsRect, 2);
			if (!RectIntersects(contentDirty, crc)) continue;
			if (c->ParentForm->Render == NULL)
				c->ParentForm->Render = this->Render;
			c->Update();
		}

		// 状态栏：在普通控件之后绘制（TopMost=true）
		if (this->MainStatusBar && this->MainStatusBar->TopMost && this->MainStatusBar->Visible)
		{
			this->MainStatusBar->Update();
		}

		// 如果主菜单展开/前景控件可见，它们应覆盖在状态栏之上
		if (!this->OverlayRender)
		{
			if (this->MainMenu && this->MainMenu->Visible)
			{
				auto ms = this->MainMenu->ActualSize();
				if (ms.cy > this->MainMenu->BarHeight)
					this->MainMenu->Update();
			}
			if (this->ForegroundControl && this->ForegroundControl->Visible && this->ForegroundControl != (Control*)this->MainMenu)
			{
				this->ForegroundControl->Update();
			}
		}
		this->Render->PopDrawRect();
		this->Render->ClearTransform();
	}

	this->OnPaint(this);

	this->Render->PopDrawRect();
	this->Render->EndRender();

	if (this->OverlayRender)
	{
		auto* oldRender = this->Render;
		RECT fullClient{};
		::GetClientRect(this->Handle, &fullClient);
		RECT overlayRc = fullClient;

		this->OverlayRender->BeginRender();
		this->OverlayRender->ClearTransform();
		this->OverlayRender->Clear(D2D1_COLOR_F{ 0.0f,0.0f,0.0f,0.0f });
		this->OverlayRender->PushDrawRect((float)overlayRc.left, (float)overlayRc.top, (float)(overlayRc.right - overlayRc.left), (float)(overlayRc.bottom - overlayRc.top));

		RECT overlayContent = fullClient;
		const int top = ClientTop();
		overlayContent.top -= top;
		overlayContent.bottom -= top;
		if (overlayContent.top < 0) overlayContent.top = 0;
		if (overlayContent.left < 0) overlayContent.left = 0;
		if (overlayContent.right > this->Size.cx) overlayContent.right = this->Size.cx;
		if (overlayContent.bottom > (this->Size.cy - top)) overlayContent.bottom = (this->Size.cy - top);

		if (overlayContent.right > overlayContent.left && overlayContent.bottom > overlayContent.top)
		{
			this->OverlayRender->SetTransform(D2D1::Matrix3x2F::Translation(0.0f, (float)top));
			this->OverlayRender->PushDrawRect((float)overlayContent.left, (float)overlayContent.top, (float)(overlayContent.right - overlayContent.left), (float)(overlayContent.bottom - overlayContent.top));

			this->Render = this->OverlayRender;
			if (this->MainStatusBar && this->MainStatusBar->TopMost && this->MainStatusBar->Visible)
			{
				this->MainStatusBar->Update();
			}
			if (this->MainMenu && this->MainMenu->Visible)
			{
				this->MainMenu->Update();
			}
			if (this->ForegroundControl && this->ForegroundControl->Visible && this->ForegroundControl != (Control*)this->MainMenu)
			{
				this->ForegroundControl->Update();
			}
			this->Render = oldRender;

			this->OverlayRender->PopDrawRect();
			this->OverlayRender->ClearTransform();
		}

		this->OverlayRender->PopDrawRect();
		this->OverlayRender->EndRender();
	}

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
		if (this->ForegroundControl == c) 
			this->ForegroundControl = NULL;
		if (this->MainMenu == c) 
			this->MainMenu = NULL;
		if (this->MainStatusBar == c)
			this->MainStatusBar = NULL;
		if (this->UnderMouse == c)
			this->UnderMouse = NULL;
		if (this->Selected == c)
			this->SetSelectedControl(NULL, true);
		if (this->_hoverControl == c)
			this->_hoverControl = NULL;
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
	case WM_DROPFILES:
	{
		HDROP hDropInfo = HDROP(wParam);
		UINT uFileNum = DragQueryFile(hDropInfo, 0xffffffff, NULL, 0);
		TCHAR strFileName[MAX_PATH];
		List<std::wstring> files;
		for (int i = 0; i < (int)uFileNum; i++)
		{
			DragQueryFile(hDropInfo, i, strFileName, MAX_PATH);
			files.Add(strFileName);
		}
		DragFinish(hDropInfo);
		if (files.Count > 0)
		{
			this->OnDropFile(this, files);
			auto* target = HitTestControlAt(contentMouse);
			if (target)
			{
				target->OnDropFile(target, files);
			}
		}
	}
	break;
	case WM_MOUSEMOVE:
	{
		if (!this->_mouseLeaveTracking && this->Handle)
		{
			TRACKMOUSEEVENT tme{};
			tme.cbSize = sizeof(tme);
			tme.dwFlags = TME_LEAVE;
			tme.hwndTrack = this->Handle;
			::TrackMouseEvent(&tme);
			this->_mouseLeaveTracking = true;
		}

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
			RaiseControlMouseEnterLeave(this, this->_hoverControl, NULL, contentMouse);
			this->_hoverControl = NULL;
			this->UnderMouse = NULL;
			this->OnMouseMove(this, MouseEventArgs(MouseButtons::None, 0, contentMouse.x, contentMouse.y, 0));
			ApplyCursor(CursorKind::Arrow);
			break;
		}

		if (this->Selected && (GetAsyncKeyState(VK_LBUTTON) & 0x8000))
		{
			if (this->Selected->IsVisual)
			{
				RaiseControlMouseEnterLeave(this, this->_hoverControl, this->Selected, contentMouse);
				this->_hoverControl = this->Selected;
				this->UnderMouse = this->Selected;
				HitControl = this->Selected;
				auto location = this->Selected->AbsLocation;
				this->Selected->ProcessMessage(message, wParam, lParam, contentMouse.x - location.x, contentMouse.y - location.y);
				UpdateCursor(mouse, contentMouse);
				break;
			}
		}

		Control* newHover = HitTestControlAt(contentMouse);
		RaiseControlMouseEnterLeave(this, this->_hoverControl, newHover, contentMouse);
		this->_hoverControl = newHover;
		this->UnderMouse = newHover;

		auto hit = HitTestRootControlAt(this, contentMouse);
		if (hit)
		{
			HitControl = hit;
			auto abs = hit->AbsLocation;
			hit->ProcessMessage(message, wParam, lParam, contentMouse.x - abs.x, contentMouse.y - abs.y);
		}
		this->UnderMouse = this->_hoverControl;
		UpdateCursor(mouse, contentMouse);
		this->OnMouseMove(this, MouseEventArgs(MouseButtons::None, 0, contentMouse.x, contentMouse.y, 0));
	}
	break;
	case WM_MOUSELEAVE:
	{
		this->_mouseLeaveTracking = false;
		RaiseControlMouseEnterLeave(this, this->_hoverControl, NULL, contentMouse);
		this->_hoverControl = NULL;
		this->UnderMouse = NULL;
		UpdateCursorFromCurrentMouse();
	}
	break;
	case WM_MOUSEWHEEL:
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
	case WM_LBUTTONDBLCLK:
	{
		DismissComboBoxForegroundOnOutsideMouseDown(this, contentMouse, message);

		if (message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN || message == WM_MBUTTONDOWN)
		{
			if (!(this->VisibleHead && mouse.y < top))
			{
				if (::GetFocus() != this->Handle)
					::SetFocus(this->Handle);
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
		auto hit = HitTestRootControlAt(this, contentMouse);
		if (hit)
		{
			HitControl = hit;
			auto abs = hit->AbsLocation;
			hit->ProcessMessage(message, wParam, lParam, contentMouse.x - abs.x, contentMouse.y - abs.y);
		}
		if (message == WM_MOUSEWHEEL)
		{
			this->OnMouseWheel(this, MouseEventArgs(MouseButtons::None, 0, contentMouse.x, contentMouse.y, GET_WHEEL_DELTA_WPARAM(wParam)));
		}
		else if (message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN || message == WM_MBUTTONDOWN)
		{
			this->OnMouseDown(this, MouseEventArgs(FromParamToMouseButtons(message), 0, contentMouse.x, contentMouse.y, HIWORD(wParam)));
		}
		else if (message == WM_LBUTTONUP || message == WM_RBUTTONUP || message == WM_MBUTTONUP)
		{
			this->OnMouseUp(this, MouseEventArgs(FromParamToMouseButtons(message), 0, contentMouse.x, contentMouse.y, HIWORD(wParam)));
		}

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
	case WM_SETFOCUS:
	{
		this->OnGotFocus(this);
	}
	break;
	case WM_KILLFOCUS:
	{
		this->OnLostFocus(this);
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
		RECT rec;
		UINT width = LOWORD(lParam);
		UINT height = HIWORD(lParam);
		GetClientRect(this->Handle, &rec);
		this->Render->ReSize(width, height);
		if (this->OverlayRender) this->OverlayRender->ReSize(width, height);
		this->InvalidateLayout();
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
	// 兼容：旧控件代码路径直接写 Selected，这里补齐焦点事件
	if (this->_focusNotifiedSelected != this->Selected)
	{
		auto* old = this->_focusNotifiedSelected;
		auto* now = this->Selected;
		this->_focusNotifiedSelected = now;
		if (old)
		{
			old->OnLostFocus(old);
			old->PostRender();
		}
		if (now)
		{
			now->OnGotFocus(now);
			now->PostRender();
		}
	}
	if (WM_LBUTTONDOWN == message && HitControl == NULL && this->Selected && HitControl != this->Selected)
	{
		this->SetSelectedControl(NULL, true);
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
LRESULT CustomFrameHitTest(HWND _hWnd, WPARAM wParam, LPARAM lParam, int captionHeight, UINT dpi)
{
	// 边框命中宽度按 DPI 缩放，避免高 DPI 下 resize 命中区域过窄。
	const int SCALER_WIDTH = Application::ScaleInt(8, 96, dpi ? dpi : 96);
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
		if (message == WM_DPICHANGED)
		{
			RECT* suggested = (RECT*)lParam;
			if (suggested)
			{
				SetWindowPos(hWnd, NULL,
					suggested->left,
					suggested->top,
					suggested->right - suggested->left,
					suggested->bottom - suggested->top,
					SWP_NOZORDER | SWP_NOACTIVATE);
			}
			UINT newDpi = HIWORD(wParam);
			// 首次显示前可能控件树未构造完成：先只记录 DPI，真正缩放留到 Show 前。
			if (!form->_initialDpiApplied)
			{
				form->_dpi = newDpi;
				form->_initialWindowRectApplied = true;
				form->SyncRenderSizeToClient();
				form->_hasRenderedOnce = false;
				form->Invalidate(false);
				return 0;
			}
			form->ApplyDpiChange(newDpi);
			form->SyncRenderSizeToClient();
			form->_hasRenderedOnce = false;
			form->Invalidate(false);
			return 0;
		}

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
				if (!::IsWindowEnabled(hWnd))
				{
					EndPaint(hWnd, &ps);
					return 0;
				}

				if (form->ControlChanged || !form->_hasRenderedOnce)
					form->UpdateDirtyRect(ps.rcPaint, true);
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
				lr = CustomFrameHitTest(hWnd, wParam, lParam, form->ClientTop(), form->_dpi);
				if (lr == HTCAPTION)
				{
					POINT ptClient{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
					ScreenToClient(hWnd, &ptClient);
					CaptionButtonKind k{};
					if (form->HitTestCaptionButtons(ptClient, k))
						return HTCLIENT;
				}
				if (IsZoomed(hWnd))
				{
					// 最大化状态下禁止鼠标拖拽边缘/角落调整窗口大小
					if (lr == HTLEFT || lr == HTRIGHT || lr == HTTOP || lr == HTBOTTOM ||
						lr == HTTOPLEFT || lr == HTTOPRIGHT || lr == HTBOTTOMLEFT || lr == HTBOTTOMRIGHT)
					{
						return HTCLIENT;
					}
				}
				if (!form->AllowResize)
				{
					// 禁用边缘/角落 resize，只保留标题栏拖动与正常客户区
					if (lr == HTLEFT || lr == HTRIGHT || lr == HTTOP || lr == HTBOTTOM ||
						lr == HTTOPLEFT || lr == HTTOPRIGHT || lr == HTBOTTOMLEFT || lr == HTBOTTOMRIGHT)
					{
						return HTCLIENT;
					}
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
			form->CleanupResources();
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