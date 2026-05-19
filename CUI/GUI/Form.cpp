#include "Form.h"
#include "NotifyIcon.h"
#include "DCompLayeredHost.h"
#include <algorithm>
#include <functional>
#include <cmath>
#include <unordered_map>
#include <oleidl.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <dcomp.h>
#include <windowsx.h>

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif

#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif

namespace
{
	constexpr int DCompSceneLayerBand = 1000;

	RECT GetPrimaryWorkArea()
	{
		RECT workArea{ 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
		SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
		return workArea;
	}

	RECT GetWindowWorkArea(HWND hWnd, POINT fallbackPoint)
	{
		RECT workArea = GetPrimaryWorkArea();
		HMONITOR monitor = nullptr;
		if (hWnd)
		{
			monitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
		}
		else
		{
			monitor = MonitorFromPoint(fallbackPoint, MONITOR_DEFAULTTONEAREST);
		}
		if (!monitor)
			return workArea;

		MONITORINFO monitorInfo{};
		monitorInfo.cbSize = sizeof(monitorInfo);
		if (GetMonitorInfoW(monitor, &monitorInfo))
			return monitorInfo.rcWork;
		return workArea;
	}

	POINT ClampWindowOriginToWorkArea(POINT origin, SIZE size, const RECT& workArea)
	{
		POINT clamped = origin;
		const int maxX = (std::max)(workArea.left, workArea.right - size.cx);
		const int maxY = (std::max)(workArea.top, workArea.bottom - size.cy);
		clamped.x = (std::clamp)(clamped.x, workArea.left, (LONG)maxX);
		clamped.y = (std::clamp)(clamped.y, workArea.top, (LONG)maxY);
		return clamped;
	}

	float LayoutMainTopBar(Form* form, const SIZE& clientSize)
	{
		auto* menu = form ? form->MainMenu : nullptr;
		LONG width = (std::max)(0L, clientSize.cx);
		LONG top = 0;

		if (menu && menu->Visible)
		{
			LONG height = (std::max)(0, menu->BarHeight);
			if (height > clientSize.cy)
				height = (std::max)(0L, clientSize.cy);
			menu->ApplyLayout(POINT{ 0, top }, SIZE{ width, height });
			top += height;
		}

		auto* toolBar = form ? form->MainToolBar : nullptr;
		if (toolBar && toolBar->Visible)
		{
			LONG availableHeight = clientSize.cy - top;
			if (availableHeight < 0)
				availableHeight = 0;
			SIZE measured = toolBar->MeasureCore(SIZE{ width, availableHeight });
			LONG height = (std::max)(0L, measured.cy);
			if (height > availableHeight)
				height = availableHeight;
			toolBar->ApplyLayout(POINT{ 0, top }, SIZE{ width, height });
			top += height;
		}

		return (float)top;
	}

	float LayoutMainStatusBar(Form* form, const SIZE& clientSize)
	{
		auto* statusBar = form ? form->MainStatusBar : nullptr;
		if (!statusBar || !statusBar->TopMost || !statusBar->Visible)
		{
			return 0.0f;
		}

		SIZE measured = statusBar->MeasureCore(clientSize);
		LONG width = (std::max)(0L, clientSize.cx);
		LONG height = (std::max)(0L, measured.cy);
		LONG top = clientSize.cy - height;
		if (top < 0)
		{
			top = 0;
			height = (std::min)(height, clientSize.cy);
		}

		statusBar->ApplyLayout(POINT{ 0, top }, SIZE{ width, height });
		return (float)height;
	}

	HICON LoadProcessIcon(bool wantSmall)
	{
		static HICON largeIcon = nullptr;
		static HICON smallIcon = nullptr;
		HICON& cached = wantSmall ? smallIcon : largeIcon;
		if (cached) return cached;

		wchar_t exePath[MAX_PATH]{};
		if (GetModuleFileNameW(nullptr, exePath, MAX_PATH) > 0)
		{
			HICON large = nullptr;
			HICON smallHandle = nullptr;
			if (ExtractIconExW(exePath, 0, &large, &smallHandle, 1) > 0)
			{
				if (wantSmall)
				{
					cached = smallHandle ? smallHandle : large;
					if (large && large != cached) DestroyIcon(large);
				}
				else
				{
					cached = large ? large : smallHandle;
					if (smallHandle && smallHandle != cached) DestroyIcon(smallHandle);
				}
			}
		}

		if (!cached)
		{
			cached = (HICON)LoadImageW(
				nullptr,
				IDI_APPLICATION,
				IMAGE_ICON,
				wantSmall ? GetSystemMetrics(SM_CXSMICON) : GetSystemMetrics(SM_CXICON),
				wantSmall ? GetSystemMetrics(SM_CYSMICON) : GetSystemMetrics(SM_CYICON),
				LR_SHARED);
		}

		return cached;
	}
}

HCURSOR Form::GetSystemCursor(CursorKind kind)
{
	static std::unordered_map<CursorKind, HCURSOR> cache;
	auto it = cache.find(kind);
	if (it != cache.end() && it->second) return it->second;

	LPCWSTR id = IDC_ARROW;
	switch (kind)
	{
	case CursorKind::Arrow: id = IDC_ARROW; break;
	case CursorKind::Cross: id = IDC_CROSS; break;
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
	HCURSOR h = LoadCursorW(nullptr, id);
	cache.emplace(kind, h);
	return h;
}

void Form::SetImeCompositionWindowFromLogicalRect(const D2D1_RECT_F& logicalRect)
{
	if (!this->Handle || !::IsWindow(this->Handle)) return;

	HIMC hImc = ImmGetContext(this->Handle);
	if (!hImc) return;

	float dpiScale = GetDpiScale();
	if (dpiScale <= 0.0f) dpiScale = 1.0f;
	float headLogical = this->VisibleHead ? ((float)this->HeadHeight / dpiScale) : 0.0f;

	LONG left = (LONG)std::lround(logicalRect.left * dpiScale);
	LONG top = (LONG)std::lround((logicalRect.top + headLogical) * dpiScale);
	LONG right = (LONG)std::lround(logicalRect.right * dpiScale);
	LONG bottom = (LONG)std::lround((logicalRect.bottom + headLogical) * dpiScale);
	if (right < left) right = left;
	if (bottom < top) bottom = top;

	POINT anchor{ left, bottom };
	COMPOSITIONFORM composition{};
	composition.dwStyle = CFS_POINT;
	composition.ptCurrentPos = anchor;
	ImmSetCompositionWindow(hImc, &composition);

	CANDIDATEFORM candidate{};
	candidate.dwStyle = CFS_EXCLUDE;
	candidate.ptCurrentPos = anchor;
	candidate.rcArea = RECT{
		left,
		top,
		(std::max)(left + 1, right),
		(std::max)(top + 1, bottom)
	};
	for (DWORD index = 0; index < 4; ++index)
	{
		candidate.dwIndex = index;
		ImmSetCandidateWindow(hImc, &candidate);
	}

	ImmReleaseContext(this->Handle, hImc);
}

void Form::ApplyCursor(CursorKind kind)
{
	HCURSOR desired = GetSystemCursor(kind);
	if (kind == _currentCursor && ::GetCursor() == desired) return;
	_currentCursor = kind;
	::SetCursor(desired);
}

bool Form::ApplySystemCursorId(UINT32 cursorId)
{
	if (cursorId == 0) return false;
	HCURSOR cursor = LoadCursorW(nullptr, MAKEINTRESOURCEW((ULONG_PTR)cursorId));
	if (!cursor) return false;
	::SetCursor(cursor);
	return true;
}

static Control* HitTestDeepestChild(Control* root, POINT contentMouse)
{
	if (!root) return nullptr;
	if (!root->Visible || !root->Enable) return nullptr;
	auto rootAbs = root->AbsLocation;
	int localX = contentMouse.x - rootAbs.x;
	int localY = contentMouse.y - rootAbs.y;
	if (!root->ShouldHitTestChildrenAt(localX, localY))
		return root;

	for (auto child : root->GetChildrenInReverseZOrder())
	{
		if (!child || !child->Visible || !child->Enable) continue;
		auto childLocation = child->AbsLocation;
		auto childSize = child->ActualSize();
		if (contentMouse.x >= childLocation.x && contentMouse.y >= childLocation.y &&
			contentMouse.x <= childLocation.x + childSize.cx && contentMouse.y <= childLocation.y + childSize.cy)
		{
			auto deeperChild = HitTestDeepestChild(child, contentMouse);
			return deeperChild ? deeperChild : child;
		}
	}
	return root;
}

static bool PointInControlRect(Control* control, POINT contentMouse)
{
	if (!control) return false;
	if (!control->Visible || !control->Enable) return false;
	auto location = control->AbsLocation;
	return control->ContainsPoint(contentMouse.x - location.x, contentMouse.y - location.y);
}

static void SyncFormWindowStyles(HWND hWnd, bool showInTaskBar, bool minBox, bool maxBox, bool closeBox, bool allowResize)
{
	if (!hWnd)
		return;

	LONG_PTR style = GetWindowLongPtrW(hWnd, GWL_STYLE);
	style &= ~(WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU | WS_THICKFRAME);
	if (minBox) style |= WS_MINIMIZEBOX;
	if (maxBox) style |= WS_MAXIMIZEBOX;
	if (closeBox) style |= WS_SYSMENU;
	if (allowResize) style |= WS_THICKFRAME;
	SetWindowLongPtrW(hWnd, GWL_STYLE, style);

	LONG_PTR exStyle = GetWindowLongPtrW(hWnd, GWL_EXSTYLE);
	if (showInTaskBar)
	{
		exStyle &= ~WS_EX_TOOLWINDOW;
		exStyle |= WS_EX_APPWINDOW;
	}
	else
	{
		exStyle &= ~WS_EX_APPWINDOW;
		exStyle |= WS_EX_TOOLWINDOW;
	}
	SetWindowLongPtrW(hWnd, GWL_EXSTYLE, exStyle);

	SetWindowPos(hWnd, nullptr, 0, 0, 0, 0,
		SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);

	const int cornerPreference = 1;
	DwmSetWindowAttribute(hWnd, DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPreference, sizeof(cornerPreference));
}

static int GetCustomFrameInset()
{
	return GetSystemMetrics(SM_CXSIZEFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
}

static std::vector<Control*> GetRootControlsInZOrder(Form* form)
{
	std::vector<Control*> result;
	if (!form) return result;
	result = form->Controls;
	std::stable_sort(result.begin(), result.end(), [](Control* left, Control* right)
		{
			if (!left || !right) return left != nullptr;
			return left->ZIndex < right->ZIndex;
		});
	return result;
}

static std::vector<Control*> GetRootControlsInReverseZOrder(Form* form)
{
	auto result = GetRootControlsInZOrder(form);
	std::reverse(result.begin(), result.end());
	return result;
}

Control* Form::HitTestControlAt(POINT contentMouse)
{
	// 1) 置顶控件优先命中（ComboBox 下拉等）
	if (this->ForegroundControl && this->ForegroundControl->Visible && this->ForegroundControl->Enable)
	{
		auto* foregroundControl = this->ForegroundControl;
		if (PointInControlRect(foregroundControl, contentMouse))
		{
			return HitTestDeepestChild(foregroundControl, contentMouse);
		}
	}

	// 2) 主菜单单独优先命中（包含下拉区域）
	if (this->MainMenu && this->MainMenu->Visible && this->MainMenu->Enable)
	{
		auto* mainMenu = this->MainMenu;
		if (PointInControlRect(mainMenu, contentMouse))
		{
			return HitTestDeepestChild(mainMenu, contentMouse);
		}
	}

	// 3) 状态栏：置顶于普通控件（但优先级低于主菜单与前景控件）
	if (this->MainStatusBar && this->MainStatusBar->TopMost && this->MainStatusBar->Visible && this->MainStatusBar->Enable)
	{
		auto* statusBar = this->MainStatusBar;
		if (PointInControlRect(statusBar, contentMouse))
		{
			return HitTestDeepestChild(statusBar, contentMouse);
		}
	}

	// 4) 普通控件：按绘制顺序倒序命中（后绘制者优先）
	for (auto control : GetRootControlsInReverseZOrder(this))
	{
		if (!control || !control->Visible || !control->Enable) continue;
		if (control == this->ForegroundControl) continue;
		if (control == this->MainMenu) continue;
		if (this->MainStatusBar && this->MainStatusBar->TopMost && control == this->MainStatusBar) continue;
		if (!PointInControlRect(control, contentMouse)) continue;
		return HitTestDeepestChild(control, contentMouse);
	}
	return nullptr;
}

static bool IsScrollViewFallbackKey(WPARAM key)
{
	switch (key)
	{
	case VK_HOME:
	case VK_END:
	case VK_PRIOR:
	case VK_NEXT:
		return true;
	default:
		return false;
	}
}

static Control* FindAncestorScrollViewForFallback(Control* start, WPARAM key)
{
	if (!start) return nullptr;
	for (Control* parent = start->Parent; parent; parent = parent->Parent)
	{
		if (parent->Type() == UIClass::UI_ScrollView && parent->HandlesNavigationKey(key))
			return parent;
	}
	return nullptr;
}

static Control* GetScrollViewFallbackTarget(Control* selected, WPARAM key)
{
	if (!selected) return nullptr;
	if (!selected->IsVisual) return nullptr;
	if (!IsScrollViewFallbackKey(key)) return nullptr;
	if (selected->HandlesNavigationKey(key)) return nullptr;
	return FindAncestorScrollViewForFallback(selected, key);
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

static std::optional<std::vector<std::wstring>> TryExtractDroppedFiles(IDataObject* pDataObj)
{
	if (!pDataObj) return std::nullopt;
	FORMATETC fmt{};
	fmt.cfFormat = CF_HDROP;
	fmt.dwAspect = DVASPECT_CONTENT;
	fmt.lindex = -1;
	fmt.tymed = TYMED_HGLOBAL;
	STGMEDIUM stg{};
	if (FAILED(pDataObj->GetData(&fmt, &stg))) return std::nullopt;

	std::vector<std::wstring> files;
	HDROP hDrop = (HDROP)GlobalLock(stg.hGlobal);
	if (hDrop)
	{
		UINT count = DragQueryFile(hDrop, 0xFFFFFFFF, nullptr, 0);
		WCHAR buf[MAX_PATH];
		for (UINT i = 0; i < count; i++)
		{
			buf[0] = 0;
			DragQueryFileW(hDrop, i, buf, MAX_PATH);
			files.push_back(buf);
		}
		GlobalUnlock(stg.hGlobal);
	}
	ReleaseStgMedium(&stg);
	if (files.size() <= 0) return std::nullopt;
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
			int wlen = MultiByteToWideChar(CP_ACP, 0, s, len, nullptr, 0);
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
		// physical→logical: OS gives physical px; controls live in logical (96-DPI) coords
		const float sc_ = _form->GetDpiScale();
		POINT contentMouse{ (LONG)(client.x / sc_), (LONG)((client.y - _form->ClientTop()) / sc_) };
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
		const float sc_ = _form->GetDpiScale();
		POINT contentMouse{ (LONG)(client.x / sc_), (LONG)((client.y - _form->ClientTop()) / sc_) };
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

static Control* HitTestRootControlAt(Form* form, POINT contentMouse)
{
	if (!form) return nullptr;

	// 1) ForegroundControl 顶层优先
	if (form->ForegroundControl && form->ForegroundControl->Visible && form->ForegroundControl->Enable)
	{
		auto* foregroundControl = form->ForegroundControl;
		if (PointInControlRect(foregroundControl, contentMouse))
			return foregroundControl;
	}

	// 2) 主菜单次优先
	if (form->MainMenu && form->MainMenu->Visible && form->MainMenu->Enable)
	{
		auto* mainMenu = form->MainMenu;
		if (PointInControlRect(mainMenu, contentMouse))
			return mainMenu;
	}

	// 3) 状态栏（TopMost=true）
	if (form->MainStatusBar && form->MainStatusBar->TopMost && form->MainStatusBar->Visible && form->MainStatusBar->Enable)
	{
		auto* statusBar = form->MainStatusBar;
		if (PointInControlRect(statusBar, contentMouse))
			return statusBar;
	}

	// 4) 普通控件按绘制顺序倒序命中
	for (auto control : GetRootControlsInReverseZOrder(form))
	{
		if (!control || !control->Visible || !control->Enable) continue;
		if (control == form->ForegroundControl) continue;
		if (control == form->MainMenu) continue;
		if (form->MainStatusBar && form->MainStatusBar->TopMost && control == form->MainStatusBar) continue;
		if (!PointInControlRect(control, contentMouse)) continue;
		return control;
	}
	return nullptr;
}

static void DismissForegroundOnOutsideMouseDown(Form* form, POINT contentMouse, UINT message)
{
	if (!form) return;
	if (message != WM_LBUTTONDOWN && message != WM_RBUTTONDOWN && message != WM_MBUTTONDOWN) return;
	bool wasDismissed = false;
	if (form->ForegroundControl && form->ForegroundControl->Visible && form->ForegroundControl->Enable)
	{
		if (!PointInControlRect(form->ForegroundControl, contentMouse) && form->ForegroundControl->AutoCloseOnOutsideClick())
		{
			form->ForegroundControl->ClosePopup();
			wasDismissed = true;
		}
	}
	if (form->MainMenu && form->MainMenu->Visible && form->MainMenu->Enable)
	{
		if (!PointInControlRect(form->MainMenu, contentMouse) && form->MainMenu->AutoCloseOnOutsideClick())
		{
			form->MainMenu->ClosePopup();
			wasDismissed = true;
		}
	}
	if (wasDismissed)
		form->Invalidate(true);
}

CursorKind Form::QueryCursorAt(POINT mouseClient, POINT contentMouse)
{
	const int titleBarHeight = ClientTop();
	if (this->VisibleHead && mouseClient.y < titleBarHeight)
	{
		return CursorKind::Arrow;
	}

	auto hitControl = HitTestControlAt(contentMouse);

	if (this->Selected && this->Selected->IsVisual && (GetAsyncKeyState(VK_LBUTTON) & 0x8000))
	{
		bool keepSelectedCursor = (::GetCapture() == this->Handle);
		if (!keepSelectedCursor)
		{
			keepSelectedCursor = (hitControl == this->Selected) || PointInControlRect(this->Selected, contentMouse);
		}
		if (keepSelectedCursor)
		{
			auto selectedLocation = this->Selected->AbsLocation;
			int localX = contentMouse.x - selectedLocation.x;
			int localY = contentMouse.y - selectedLocation.y;
			return this->Selected->QueryCursor(localX, localY);
		}
	}

	if (!hitControl) return CursorKind::Arrow;
	auto hitLocation = hitControl->AbsLocation;
	int localX = contentMouse.x - hitLocation.x;
	int localY = contentMouse.y - hitLocation.y;
	return hitControl->QueryCursor(localX, localY);
}

void Form::UpdateCursor(POINT mouseClient, POINT contentMouse)
{
	const int titleBarHeight = ClientTop();
	if (!(this->VisibleHead && mouseClient.y < titleBarHeight))
	{
		auto hitControl = HitTestControlAt(contentMouse);

		if (this->Selected && this->Selected->IsVisual && (GetAsyncKeyState(VK_LBUTTON) & 0x8000))
		{
			bool keepSelectedCursor = (::GetCapture() == this->Handle);
			if (!keepSelectedCursor)
			{
				keepSelectedCursor = (hitControl == this->Selected) || PointInControlRect(this->Selected, contentMouse);
			}
			if (keepSelectedCursor)
			{
				UINT32 cursorId = 0;
				if (this->Selected->TryGetSystemCursorId(cursorId) && ApplySystemCursorId(cursorId))
					return;
			}
		}

		for (Control* target = hitControl; target; target = target->Parent)
		{
			UINT32 cursorId = 0;
			if (target->TryGetSystemCursorId(cursorId) && ApplySystemCursorId(cursorId))
				return;
		}
	}

	ApplyCursor(QueryCursorAt(mouseClient, contentMouse));
}

void Form::UpdateCursorFromCurrentMouse()
{
	if (!this->Handle) return;
	POINT mouse{};
	GetCursorPos(&mouse);
	ScreenToClient(this->Handle, &mouse);
	const float dpiScale = GetDpiScale();
	POINT contentMouse{ (LONG)(mouse.x / dpiScale), (LONG)((mouse.y - ClientTop()) / dpiScale) };
	UpdateCursor(mouse, contentMouse);
}

void Form::SetSelectedControl(Control* value, bool invalidateVisual)
{
	auto* previousSelection = this->Selected;
	if (previousSelection == value) return;
	this->Selected = value;
	if (previousSelection)
	{
		previousSelection->OnLostFocus(previousSelection);
		if (invalidateVisual) previousSelection->InvalidateVisual();
	}
	if (value)
	{
		value->OnGotFocus(value);
		if (invalidateVisual) value->InvalidateVisual();
	}
	this->_focusNotifiedSelected = this->Selected;
}

static void RaiseControlMouseEnterLeave(Form* form, Control* previousHover, Control* newHover, POINT contentMouse)
{
	if (!form) return;
	if (previousHover == newHover) return;

	auto makeArgs = [&](Control* control) -> MouseEventArgs
		{
			if (!control) return MouseEventArgs(MouseButtons::None, 0, 0, 0, 0);
			auto controlLocation = control->AbsLocation;
			return MouseEventArgs(MouseButtons::None, 0, contentMouse.x - controlLocation.x, contentMouse.y - controlLocation.y, 0);
		};

	if (previousHover)
	{
		auto args = makeArgs(previousHover);
		previousHover->OnMouseLeave(previousHover, args);
		previousHover->InvalidateVisual();
	}
	if (newHover)
	{
		auto args = makeArgs(newHover);
		newHover->OnMouseEnter(newHover, args);
		newHover->InvalidateVisual();
	}
}

bool Form::TryGetCaptionButtonRect(CaptionButtonKind kind, RECT& out)
{
	if (!this->VisibleHead || this->HeadHeight <= 0) return false;

	const float dpiScale = GetDpiScale();
	int rightEdge = (int)(this->Size.cx / dpiScale);  // logical width
	int buttonHeight = (int)(this->HeadHeight / dpiScale);    // logical = _headHeightBase96
	int buttonWidth = buttonHeight;

	auto place = [&](CaptionButtonKind k, bool enabled) -> std::optional<RECT>
		{
			if (!enabled) return std::nullopt;
			RECT rect{ rightEdge - buttonWidth, 0, rightEdge, buttonHeight };
			rightEdge -= buttonWidth;
			return rect;
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

	auto rect = pick(kind);
	if (!rect.has_value()) return false;
	out = rect.value();
	return true;
}

bool Form::HitTestCaptionButtons(POINT clientPoint, CaptionButtonKind& outKind)
{
	// clientPoint is in physical pixels (from OS); TryGetCaptionButtonRect returns logical rects.
	const float dpiScale = GetDpiScale();
	POINT logicalPoint{ (LONG)(clientPoint.x / dpiScale), (LONG)(clientPoint.y / dpiScale) };
	RECT rect{};
	if (TryGetCaptionButtonRect(CaptionButtonKind::Close, rect) && PtInRect(&rect, logicalPoint))
	{
		outKind = CaptionButtonKind::Close;
		return true;
	}
	if (TryGetCaptionButtonRect(CaptionButtonKind::Maximize, rect) && PtInRect(&rect, logicalPoint))
	{
		outKind = CaptionButtonKind::Maximize;
		return true;
	}
	if (TryGetCaptionButtonRect(CaptionButtonKind::Minimize, rect) && PtInRect(&rect, logicalPoint))
	{
		outKind = CaptionButtonKind::Minimize;
		return true;
	}
	return false;
}

bool Form::HitTestCaptionButtonResizeExclusion(POINT clientPoint)
{
	if (!this->VisibleHead || this->HeadHeight <= 0) return false;

	const float dpiScale = GetDpiScale();
	POINT logicalPoint{ (LONG)(clientPoint.x / dpiScale), (LONG)(clientPoint.y / dpiScale) };
	const int padding = (std::max)(2, (int)std::ceil((float)GetCustomFrameInset() / dpiScale));

	RECT unionRect{};
	bool hasRect = false;
	const CaptionButtonKind kinds[] = { CaptionButtonKind::Close, CaptionButtonKind::Maximize, CaptionButtonKind::Minimize };
	for (auto kind : kinds)
	{
		RECT rect{};
		if (!TryGetCaptionButtonRect(kind, rect)) continue;
		if (!hasRect)
		{
			unionRect = rect;
			hasRect = true;
		}
		else
		{
			unionRect.left = (std::min)(unionRect.left, rect.left);
			unionRect.top = (std::min)(unionRect.top, rect.top);
			unionRect.right = (std::max)(unionRect.right, rect.right);
			unionRect.bottom = (std::max)(unionRect.bottom, rect.bottom);
		}
	}
	if (!hasRect) return false;

	InflateRect(&unionRect, padding, padding);
	return PtInRect(&unionRect, logicalPoint) != FALSE;
}

void Form::ClearCaptionStates()
{
	_capMinState = CaptionButtonState::None;
	_capMaxState = CaptionButtonState::None;
	_capCloseState = CaptionButtonState::None;
	_capPressed = false;
	_capTracking = false;
}

void Form::UpdateCaptionHover(POINT clientPoint)
{
	if (!this->VisibleHead) return;
	CaptionButtonKind hit{};
	bool isButtonHovered = HitTestCaptionButtons(clientPoint, hit);

	auto previousMinState = _capMinState;
	auto previousMaxState = _capMaxState;
	auto previousCloseState = _capCloseState;

	_capMinState = (isButtonHovered && hit == CaptionButtonKind::Minimize) ? CaptionButtonState::Hover : CaptionButtonState::None;
	_capMaxState = (isButtonHovered && hit == CaptionButtonKind::Maximize) ? CaptionButtonState::Hover : CaptionButtonState::None;
	_capCloseState = (isButtonHovered && hit == CaptionButtonKind::Close) ? CaptionButtonState::Hover : CaptionButtonState::None;

	if (_capPressed)
	{
		if (_capPressedKind == CaptionButtonKind::Minimize) _capMinState = CaptionButtonState::Pressed;
		if (_capPressedKind == CaptionButtonKind::Maximize) _capMaxState = CaptionButtonState::Pressed;
		if (_capPressedKind == CaptionButtonKind::Close) _capCloseState = CaptionButtonState::Pressed;
	}

	if (previousMinState != _capMinState || previousMaxState != _capMaxState || previousCloseState != _capCloseState)
	{
		RECT titleBarRect = TitleBarRectClient();
		Invalidate(titleBarRect, false);
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
	::InvalidateRect(this->Handle, nullptr, FALSE);
	// When the window is disabled/hidden (e.g. during a modal dialog), forcing
	// UpdateWindow can create excessive WM_PAINT churn. Let the system schedule paint.
	if (immediate && ::IsWindowVisible(this->Handle) && ::IsWindowEnabled(this->Handle))
		::UpdateWindow(this->Handle);
}

void Form::Invalidate(const RECT& rect, bool immediate)
{
	if (!this->Handle) return;
	this->ControlChanged = true;
	::InvalidateRect(this->Handle, &rect, FALSE);
	if (immediate && ::IsWindowVisible(this->Handle) && ::IsWindowEnabled(this->Handle))
		::UpdateWindow(this->Handle);
}

void Form::Invalidate(D2D1_RECT_F rect, bool immediate)
{
	RECT clientRect = ToRECT(rect, 2);
	Invalidate(clientRect, immediate);
}

bool Form::RectIntersects(const RECT& a, const RECT& b)
{
	RECT out{};
	return ::IntersectRect(&out, &a, &b) != 0;
}

RECT Form::ToRECT(D2D1_RECT_F rect, int inflatePx)
{
	RECT result{};
	result.left = (LONG)std::floor(rect.left) - inflatePx;
	result.top = (LONG)std::floor(rect.top) - inflatePx;
	result.right = (LONG)std::ceil(rect.right) + inflatePx;
	result.bottom = (LONG)std::ceil(rect.bottom) + inflatePx;
	return result;
}

void Form::InvalidateControl(Control* control, int inflatePx, bool immediate)
{
	if (!control || !this->Handle) return;
	if (!control->IsVisual) return;
	// AbsRect is in logical (96-DPI) coords; convert to physical client pixels for InvalidateRect
	RECT logicalRect = ToRECT(control->AbsRect, inflatePx);
	const float dpiScale = GetDpiScale();
	RECT physicalRect;
	physicalRect.left = (LONG)std::floor(logicalRect.left * dpiScale);
	physicalRect.top = (LONG)std::floor(logicalRect.top * dpiScale) + ClientTop();  // ClientTop is physical
	physicalRect.right = (LONG)std::ceil(logicalRect.right * dpiScale);
	physicalRect.bottom = (LONG)std::ceil(logicalRect.bottom * dpiScale) + ClientTop();
	Invalidate(physicalRect, immediate);
}

void Form::RefreshAnimationTimer()
{
	if (!this->Handle) return;

	bool hasActiveAnimation = false;
	UINT desiredIntervalMs = 0;

	std::function<void(Control*)> consider;
	consider = [&](Control* control)
		{
			if (!control || !control->Visible || !control->IsVisual) return;
			if (control->IsAnimationRunning())
			{
				hasActiveAnimation = true;
				UINT interval = control->GetAnimationIntervalMs();
				if (interval == 0) interval = 16;
				desiredIntervalMs = desiredIntervalMs == 0 ? interval : (std::min)(desiredIntervalMs, interval);
			}
			for (int i = 0; i < control->Count; i++)
				consider(control->operator[](i));
		};

	for (auto control : this->Controls) consider(control);
	if (this->ForegroundControl) consider(this->ForegroundControl);
	if (this->MainMenu) consider(this->MainMenu);
	if (this->MainStatusBar) consider(this->MainStatusBar);

	if (!hasActiveAnimation)
	{
		if (_animIntervalMs != 0)
		{
			::KillTimer(this->Handle, _animTimerId);
			_animIntervalMs = 0;
		}
		return;
	}

	if (_animIntervalMs != desiredIntervalMs)
	{
		if (_animIntervalMs != 0)
			::KillTimer(this->Handle, _animTimerId);
		_animIntervalMs = desiredIntervalMs;
		::SetTimer(this->Handle, _animTimerId, _animIntervalMs, nullptr);
	}
}

void Form::InvalidateAnimatedControls(bool immediate)
{
	std::function<void(Control*)> consider;
	consider = [&](Control* control)
		{
			if (!control) return;
			if (!control->Visible || !control->IsVisual) return;
			if (control->IsAnimationRunning())
			{
				D2D1_RECT_F rect{};
				if (control->GetAnimatedInvalidRect(rect))
				{
					RECT logicalRect = ToRECT(rect, 2);
					const float dpiScale = GetDpiScale();
					RECT physicalRect;
					physicalRect.left = (LONG)std::floor(logicalRect.left * dpiScale);
					physicalRect.top = (LONG)std::floor(logicalRect.top * dpiScale) + ClientTop();
					physicalRect.right = (LONG)std::ceil(logicalRect.right * dpiScale);
					physicalRect.bottom = (LONG)std::ceil(logicalRect.bottom * dpiScale) + ClientTop();
					Invalidate(physicalRect, false);
				}
				else
				{
					InvalidateControl(control, 2, false);
				}
			}
			for (int i = 0; i < control->Count; i++)
				consider(control->operator[](i));
		};
	for (auto control : this->Controls) consider(control);
	// 单一置顶控件 / 主菜单（有可能不在 Controls 容器里，保险起见单独考虑）
	if (this->ForegroundControl) consider(this->ForegroundControl);
	if (this->MainMenu) consider(this->MainMenu);
	if (this->MainStatusBar) consider(this->MainStatusBar);
	RefreshAnimationTimer();
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
		return this->_initialLocation;
	}
}
SET_CPP(Form, POINT, Location)
{
	if (this->Handle)
	{
		SetWindowPos(this->Handle, nullptr, value.x, value.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
	}
	this->_initialLocation = value;
}
GET_CPP(Form, SIZE, Size)
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
		return this->_initialSize;
	}
}
SET_CPP(Form, SIZE, Size)
{
	if (this->Handle)
	{
		SetWindowPos(this->Handle, nullptr, 0, 0, value.cx, value.cy, SWP_NOMOVE | SWP_NOZORDER);
	}
	this->_initialSize = value;
	this->ControlChanged = true;
	// 触发布局
	_needsLayout = true;
}

GET_CPP(Form, SIZE, ClientSize)
{
	auto clientSize = this->Size;
	clientSize.cy -= this->HeadHeight;
	return clientSize;
}
GET_CPP(Form, std::wstring, Text) {
	return _text;
}
SET_CPP(Form, std::wstring, Text) {
	_text = value;
	this->ControlChanged = true;
}

GET_CPP(Form, std::shared_ptr<BitmapSource>, Image)
{
	return _imageSource;
}

SET_CPP(Form, std::shared_ptr<BitmapSource>, Image)
{
	if (value == _imageSource)
		return;
	_imageSource = std::move(value);
	ResetImageCache();
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

FormThemeFrame Form::GetThemeFrame() const
{
	FormThemeFrame theme;
	theme.WindowBackColor = this->BackColor;
	theme.WindowForeColor = this->ForeColor;
	theme.WindowBorderLightColor = this->BorderLightColor;
	theme.WindowBorderDarkColor = this->BorderDarkColor;
	theme.TitleBarBackColor = this->HeadBackColor;
	theme.CaptionHoverColor = this->CaptionHoverColor;
	theme.CaptionPressedColor = this->CaptionPressedColor;
	theme.CloseHoverColor = this->CloseHoverColor;
	theme.ClosePressedColor = this->ClosePressedColor;
	return theme;
}

void Form::ApplyThemeFrame(const FormThemeFrame& theme, const std::wstring& themeName)
{
	std::wstring oldTheme = this->_themeName;
	if (!themeName.empty())
	{
		this->_themeName = themeName;
	}

	this->BackColor = theme.WindowBackColor;
	this->ForeColor = theme.WindowForeColor;
	this->BorderLightColor = theme.WindowBorderLightColor;
	this->BorderDarkColor = theme.WindowBorderDarkColor;
	this->HeadBackColor = theme.TitleBarBackColor;
	this->CaptionHoverColor = theme.CaptionHoverColor;
	this->CaptionPressedColor = theme.CaptionPressedColor;
	this->CloseHoverColor = theme.CloseHoverColor;
	this->ClosePressedColor = theme.ClosePressedColor;
	this->ControlChanged = true;

	if (oldTheme != this->_themeName)
	{
		this->OnThemeChanged(this, oldTheme, this->_themeName);
	}

	this->Invalidate(true);
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

	SyncFormWindowStyles(this->Handle, this->_showInTaskBar, this->MinBox, this->MaxBox, this->CloseBox, this->AllowResize);

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
	SyncFormWindowStyles(this->Handle, this->_showInTaskBar, this->MinBox, this->MaxBox, this->CloseBox, this->AllowResize);
}

Form::Form(std::wstring text, POINT _location, SIZE _size)
{
	Application::EnsureDpiAwareness();

	this->_text = text;
	this->_autoCenterOnCreate = (_location.x == 0 && _location.y == 0);
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
		wndclass.hInstance = GetModuleHandleA(nullptr);
		wndclass.hIcon = LoadProcessIcon(false);
		wndclass.hCursor = LoadCursorW(GetModuleHandle(nullptr), IDC_ARROW);
		wndclass.lpszMenuName = nullptr;
		wndclass.lpszClassName = L"CoreNativeWindow";
		if (!RegisterClassW(&wndclass))
		{
			return;
		}
		ClassInited = true;
	}
	RECT workArea = GetWindowWorkArea(nullptr, _location);
	POINT initialOrigin = _location;
	if (this->_autoCenterOnCreate)
	{
		initialOrigin.x = workArea.left + ((workArea.right - workArea.left) - this->Size.cx) / 2;
		initialOrigin.y = workArea.top + ((workArea.bottom - workArea.top) - this->Size.cy) / 2;
	}
	initialOrigin = ClampWindowOriginToWorkArea(initialOrigin, this->Size, workArea);
	this->Handle = CreateWindowExW(
		0L,
		L"CoreNativeWindow",
		_text.c_str(),
		WS_POPUP,
		initialOrigin.x,
		initialOrigin.y,
		this->Size.cx,
		this->Size.cy,
		nullptr,
		nullptr,
		GetModuleHandleW(0),
		0);
	SyncFormWindowStyles(this->Handle, this->_showInTaskBar, this->MinBox, this->MaxBox, this->CloseBox, this->AllowResize);
	SetWindowLongPtrW(this->Handle, GWLP_USERDATA, (LONG_PTR)this ^ 0xFFFFFFFFFFFFFFFF);

	DragAcceptFiles(this->Handle, TRUE);
	EnsureDropTargetRegistered();


	Application::Forms[this->Handle] = this;

	Render = new HwndGraphics(this->Handle);
	OverlayRender = nullptr;
	ResetImageCache();
	ClearCaptionStates();
	ApplyWindowIcon();
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
		SyncFormWindowStyles(this->Handle, this->_showInTaskBar, this->MinBox, this->MaxBox, this->CloseBox, this->AllowResize);
			if (!node) return false;
			for (auto c : this->Controls)
			{
				if (c == node) return true;
				if (isDescendant(c, node, isDescendant)) return true;
			}
			for (Control* parent = node->Parent; parent; parent = parent->Parent)
			{
				for (auto c : this->Controls)
				{
					if (c == parent) return true;
					if (isDescendant(c, parent, isDescendant)) return true;
				}
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
	this->Controls.clear();

	this->Selected = nullptr;
	this->UnderMouse = nullptr;
	this->MainMenu = nullptr;
	this->MainToolBar = nullptr;
	this->MainStatusBar = nullptr;

	this->_imageSource.reset();
	ResetImageCache();

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
	ReleaseDCompD2DLayers();
	if (Render)
	{
		delete Render;
		Render = nullptr;
	}
	if (_dcompHost)
	{
		delete _dcompHost;
		_dcompHost = nullptr;
	}
	if (_layoutEngine)
	{
		delete _layoutEngine;
		_layoutEngine = nullptr;
	}
}

bool Form::EnsureDCompInitialized()
{
#ifdef CUI_ENABLE_WEBVIEW2
		if (_dcompHost) return _dcompHost->IsInitialized();
		if (!this->Handle || !::IsWindow(this->Handle)) return false;

	RECT rc{};
	::GetClientRect(this->Handle, &rc);
	UINT w = (UINT)std::max<LONG>(1, rc.right - rc.left);
	UINT h = (UINT)std::max<LONG>(1, rc.bottom - rc.top);

	_dcompHost = new DCompLayeredHost();
	if (_dcompHost->Initialize(this->Handle, w, h))
	{
		auto* swapChain = static_cast<IDXGISwapChain1*>(_dcompHost->GetSwapChain());
		if (swapChain)
		{
			if (Render)
			{
				delete Render;
				Render = nullptr;
			}
			Render = new CompositionSwapChainGraphics(swapChain);
			Render->SetDpi((FLOAT)_dpi, (FLOAT)_dpi);
			Render->ReSize(w, h);
			auto* overlaySwapChain = static_cast<IDXGISwapChain1*>(_dcompHost->GetOverlaySwapChain());
			if (overlaySwapChain)
			{
				if (OverlayRender)
				{
					delete OverlayRender;
					OverlayRender = nullptr;
				}
				OverlayRender = new CompositionSwapChainGraphics(overlaySwapChain);
				OverlayRender->SetDpi((FLOAT)_dpi, (FLOAT)_dpi);
				OverlayRender->ReSize(w, h);
			}
		}
	}
	else
	{
		delete _dcompHost;
		_dcompHost = nullptr;
	}
		return _dcompHost && _dcompHost->IsInitialized();
#else
		return false;
#endif
}

D2DGraphics* Form::GetDCompD2DLayerRender(size_t index, int layer, int order)
{
#ifdef CUI_ENABLE_WEBVIEW2
	if (!_dcompHost)
		return nullptr;
	while (_dcompD2DLayers.size() <= index)
	{
		void* swapChainPtr = nullptr;
		IDCompositionVisual* visual = nullptr;
		if (!_dcompHost->CreateD2DLayer(&swapChainPtr, &visual, layer, order))
			return nullptr;
		auto* swapChain = static_cast<IDXGISwapChain1*>(swapChainPtr);
		auto* graphics = new CompositionSwapChainGraphics(swapChain);
		if (swapChain)
			swapChain->Release();
		graphics->SetDpi((FLOAT)_dpi, (FLOAT)_dpi);
		_dcompD2DLayers.push_back({ visual, graphics });
	}

	auto& item = _dcompD2DLayers[index];
	if (item.Visual)
		_dcompHost->UpdateVisualOrder(item.Visual, layer, order);
	if (item.Render)
		item.Render->SetDpi((FLOAT)_dpi, (FLOAT)_dpi);
	return item.Render;
#else
	(void)index;
	(void)layer;
	(void)order;
	return nullptr;
#endif
}

void Form::ReleaseDCompD2DLayers()
{
#ifdef CUI_ENABLE_WEBVIEW2
	for (auto& item : _dcompD2DLayers)
	{
		if (item.Render)
		{
			delete item.Render;
			item.Render = nullptr;
		}
		if (item.Visual)
		{
			if (_dcompHost)
				_dcompHost->DestroyD2DLayer(item.Visual);
			item.Visual->Release();
			item.Visual = nullptr;
		}
	}
	_dcompD2DLayers.clear();
#endif
}

void Form::ClearUnusedDCompD2DLayers(size_t usedCount, float logW, float logH)
{
#ifdef CUI_ENABLE_WEBVIEW2
	for (size_t i = usedCount; i < _dcompD2DLayers.size(); i++)
	{
		auto* layerRender = _dcompD2DLayers[i].Render;
		if (!layerRender) continue;
		layerRender->BeginRender();
		layerRender->ClearTransform();
		layerRender->Clear(D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 0.0f });
		layerRender->PushDrawRect(0.0f, 0.0f, logW, logH);
		layerRender->PopDrawRect();
		layerRender->EndRender();
	}
#else
	(void)usedCount;
	(void)logW;
	(void)logH;
#endif
}

void Form::RenderDCompRootLayers(const RECT& contentDirty, int titleBarOffset, float dpiScale)
{
#ifdef CUI_ENABLE_WEBVIEW2
	(void)titleBarOffset;
	if (!_dcompHost || contentDirty.right <= contentDirty.left || contentDirty.bottom <= contentDirty.top)
		return;

	RECT fullClient{};
	::GetClientRect(this->Handle, &fullClient);
	const float logW = (fullClient.right - fullClient.left) / dpiScale;
	const float logH = (fullClient.bottom - fullClient.top) / dpiScale;
	const auto roots = GetRootControlsInZOrder(this);
	DCompSceneBuildState state{};
	state.ContentDirty = contentDirty;
	state.LogW = logW;
	state.LogH = logH;
	state.OldRender = this->Render;
	_dcompSceneOrderCounter = 0;
	_dcompSceneRenderActive = true;

	for (auto control : roots)
	{
		if (ShouldSkipRootDCompSceneControl(control))
			continue;
		RenderDCompControlTree(control, state);
	}

	EndDCompD2DSegment(state);
	_dcompSceneRenderActive = false;
	ClearUnusedDCompD2DLayers(state.LayerIndex, logW, logH);
#else
	(void)contentDirty;
	(void)titleBarOffset;
	(void)dpiScale;
#endif
}

int Form::NextDCompSceneOrder()
{
	return _dcompSceneOrderCounter++;
}

bool Form::ShouldSkipRootDCompSceneControl(Control* control) const
{
	if (!control || !control->Visible)
		return true;
	if (control == this->ForegroundControl)
		return true;
	if (control == this->MainMenu)
		return true;
	if (this->MainStatusBar && this->MainStatusBar->TopMost && control == this->MainStatusBar)
		return true;
	return false;
}

bool Form::IsNativeDCompControl(Control* control) const
{
	return control && control->Type() == UIClass::UI_WebBrowser;
}

bool Form::GetDCompSceneClientClip(Control* control, const RECT& contentDirty, RECT& outClip)
{
	if (!control)
		return false;
	const int top = (int)(ClientTop() / GetDpiScale());
	outClip = contentDirty;
	outClip.top += top;
	outClip.bottom += top;

	Control* current = control->Parent;
	while (current)
	{
		if (current->ClipsChildren())
		{
			auto clip = current->GetChildrenClipRect();
			auto parentAbs = current->AbsLocation;
			RECT clipRect{
				(LONG)std::floor(clip.left + parentAbs.x),
				(LONG)std::floor(clip.top + parentAbs.y + top),
				(LONG)std::ceil(clip.right + parentAbs.x),
				(LONG)std::ceil(clip.bottom + parentAbs.y + top)
			};
			RECT intersection{};
			if (!::IntersectRect(&intersection, &outClip, &clipRect))
				return false;
			outClip = intersection;
		}
		current = current->Parent;
	}
	return outClip.right > outClip.left && outClip.bottom > outClip.top;
}

std::vector<Control*> Form::GetDCompSceneChildren(Control* control)
{
	std::vector<Control*> children;
	if (!control)
		return children;

	if (control->Type() == UIClass::UI_TabControl)
	{
		auto* tab = static_cast<TabControl*>(control);
		for (auto child : tab->GetVisibleScenePages())
		{
			if (child && child->Visible)
				children.push_back(child);
		}
		return children;
	}

	children = control->GetChildrenInZOrder();
	children.erase(std::remove_if(children.begin(), children.end(), [](Control* child)
		{
			return !child || !child->Visible;
		}), children.end());
	return children;
}

void Form::BeginDCompD2DSegment(DCompSceneBuildState& state, int order)
{
#ifdef CUI_ENABLE_WEBVIEW2
	if (state.SegmentOpen)
		return;
	auto* layerRender = GetDCompD2DLayerRender(state.LayerIndex++, DCompSceneLayerBand, order);
	if (!layerRender)
		return;
	state.SegmentRender = layerRender;
	state.SegmentOrder = order;
	state.SegmentOpen = true;
	layerRender->BeginRender();
	layerRender->ClearTransform();
	layerRender->Clear(D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 0.0f });
	layerRender->PushDrawRect(0.0f, 0.0f, state.LogW, state.LogH);
	this->Render = layerRender;
#else
	(void)state;
	(void)order;
#endif
}

void Form::EndDCompD2DSegment(DCompSceneBuildState& state)
{
#ifdef CUI_ENABLE_WEBVIEW2
	if (!state.SegmentOpen || !state.SegmentRender)
		return;
	state.SegmentRender->ClearTransform();
	state.SegmentRender->PopDrawRect();
	state.SegmentRender->EndRender();
	this->Render = state.OldRender;
	state.SegmentRender = nullptr;
	state.SegmentOpen = false;
#else
	(void)state;
#endif
}

void Form::RenderDCompD2DControlInSegment(Control* control, DCompSceneBuildState& state)
{
#ifdef CUI_ENABLE_WEBVIEW2
	if (!control || !state.SegmentOpen || !state.SegmentRender)
		return;
	RECT crc = ToRECT(control->AbsRect, 2);
	const int top = (int)(ClientTop() / GetDpiScale());
	RECT clientControlRc = crc;
	clientControlRc.top += top;
	clientControlRc.bottom += top;
	RECT clientClip{};
	if (!GetDCompSceneClientClip(control, state.ContentDirty, clientClip) || !RectIntersects(clientClip, clientControlRc))
		return;

	state.SegmentRender->PushDrawRect((float)clientClip.left, (float)clientClip.top, (float)(clientClip.right - clientClip.left), (float)(clientClip.bottom - clientClip.top));
	control->Update();
	state.SegmentRender->PopDrawRect();
#else
	(void)control;
	(void)state;
#endif
}

void Form::RenderDCompControlTree(Control* control, DCompSceneBuildState& state)
{
#ifdef CUI_ENABLE_WEBVIEW2
	if (!control || !control->Visible)
		return;

	const int order = NextDCompSceneOrder();
	if (IsNativeDCompControl(control))
	{
		EndDCompD2DSegment(state);
		control->SetDCompSceneOrderOverride(order);
		control->Update();
		control->ClearDCompSceneOrderOverride();
	}
	else
	{
		if (!state.SegmentOpen)
			BeginDCompD2DSegment(state, order);
		RenderDCompD2DControlInSegment(control, state);
	}

	for (auto child : GetDCompSceneChildren(control))
	{
		RenderDCompControlTree(child, state);
	}
#else
	(void)control;
	(void)state;
#endif
}

IDCompositionDevice* Form::GetDCompDevice() const
{
#ifdef CUI_ENABLE_WEBVIEW2
		auto* self = const_cast<Form*>(this);
		if (!self->_dcompHost && !self->EnsureDCompInitialized()) return nullptr;
		return self->_dcompHost ? self->_dcompHost->GetDCompDevice() : nullptr;
#else
	return nullptr;
#endif
}

IDCompositionVisual* Form::GetWebContainerVisual() const
{
#ifdef CUI_ENABLE_WEBVIEW2
		auto* self = const_cast<Form*>(this);
		if (!self->_dcompHost && !self->EnsureDCompInitialized()) return nullptr;
		return self->_dcompHost ? self->_dcompHost->GetWebContainerVisual() : nullptr;
#else
	return nullptr;
#endif
}

bool Form::RegisterDCompVisual(IDCompositionVisual* visual, int layer, int order)
{
#ifdef CUI_ENABLE_WEBVIEW2
		if (!_dcompHost && !EnsureDCompInitialized()) return false;
	return _dcompHost ? _dcompHost->RegisterVisual(visual, layer, order) : false;
#else
	(void)visual;
	(void)layer;
	(void)order;
	return false;
#endif
}

void Form::UpdateDCompVisualOrder(IDCompositionVisual* visual, int layer, int order)
{
#ifdef CUI_ENABLE_WEBVIEW2
	if (_dcompHost) _dcompHost->UpdateVisualOrder(visual, layer, order);
#else
	(void)visual;
	(void)layer;
	(void)order;
#endif
}

void Form::UnregisterDCompVisual(IDCompositionVisual* visual)
{
#ifdef CUI_ENABLE_WEBVIEW2
	if (_dcompHost) _dcompHost->UnregisterVisual(visual);
#else
	(void)visual;
#endif
}

int Form::GetDCompVisualOrder(Control* control) const
{
	if (!control)
		return 0;

	int order = 0;
	int result = -1;
	std::function<bool(Control*)> visit = [&](Control* current) -> bool
		{
			if (!current)
				return false;
			if (current == control)
			{
				result = order++;
				return true;
			}
			order++;
			for (auto child : current->GetChildrenInZOrder())
			{
				if (visit(child))
					return true;
			}
			return false;
		};

	auto roots = GetRootControlsInZOrder(const_cast<Form*>(this));
	for (auto root : roots)
	{
		if (visit(root))
			return result;
	}
	return order;
}

void Form::CommitComposition()
{
#ifdef CUI_ENABLE_WEBVIEW2
	if (_dcompHost) _dcompHost->CommitComposition();
#endif
}

Font* Form::GetScaledDefaultFont()
{
	// D2D 通过 SetDpi 已在物理像素层面正确缩放，字体大小保持 96-DPI 设计值即可
	return GetDefaultFontObject();
}

void Form::ApplyDpiChange(UINT newDpi)
{
	if (newDpi == 0) newDpi = 96;
	if (this->_dpi == newDpi) return;
	this->_dpi = newDpi;

	// 标题栏高度保持物理像素（OS 命中测试使用），从 96-DPI 基准重新计算避免累积误差
	this->HeadHeight = Application::ScaleInt(this->_headHeightBase96, 96, newDpi);

	// 通过 D2D SetDpi 让渲染引擎在逻辑坐标系（96-DPI 设计值）中工作，
	// 无需再对控件树的位置/大小/字体进行缩放——D2D 内部映射到正确的物理像素。
	if (this->Render)        this->Render->SetDpi((FLOAT)newDpi, (FLOAT)newDpi);
	if (this->OverlayRender) this->OverlayRender->SetDpi((FLOAT)newDpi, (FLOAT)newDpi);
	for (auto& layer : this->_dcompD2DLayers)
		if (layer.Render) layer.Render->SetDpi((FLOAT)newDpi, (FLOAT)newDpi);

	this->InvalidateLayout();
	this->_hasRenderedOnce = false;
	this->Invalidate(false);
}

void Form::SyncRenderSizeToClient()
{
	if (!this->Handle || !this->Render) return;
	RECT rc{};
	::GetClientRect(this->Handle, &rc);
	UINT width = (UINT)std::max<LONG>(1, rc.right - rc.left);
	UINT height = (UINT)std::max<LONG>(1, rc.bottom - rc.top);
	if (this->_dcompHost) this->_dcompHost->UpdateD2DLayerSize(width, height);
	this->Render->ReSize(width, height);
	if (this->OverlayRender) this->OverlayRender->ReSize(width, height);
	for (auto& layer : this->_dcompD2DLayers)
		if (layer.Render) layer.Render->ReSize(width, height);
	if (this->Render)        this->Render->SetDpi((FLOAT)_dpi, (FLOAT)_dpi);
	if (this->OverlayRender) this->OverlayRender->SetDpi((FLOAT)_dpi, (FLOAT)_dpi);
	for (auto& layer : this->_dcompD2DLayers)
		if (layer.Render) layer.Render->SetDpi((FLOAT)_dpi, (FLOAT)_dpi);
}

void Form::EnsureInitialDpiApplied()
{
	if (_initialDpiApplied) return;
	_initialDpiApplied = true;
	if (!this->Handle) return;

	Application::EnsureDpiAwareness();
	UINT dpi = Application::GetDpiForWindow(this->Handle);
	if (dpi == 0) dpi = 96;
	if (this->_headHeightBase96 <= 0) this->_headHeightBase96 = 24;

	// 窗口物理尺寸：按 96→dpi 缩放，使窗口在屏幕上占据与设计値相同的视角
	// 控件树保持 96-DPI 逻辑坐标，D2D 通过 SetDpi 完成物理像素映射，无需缩放控件树
	if (!this->_initialWindowRectApplied)
	{
		RECT wr{};
		GetWindowRect(this->Handle, &wr);
		const int newW = Application::ScaleInt(this->_initialSize.cx, 96, dpi);
		const int newH = Application::ScaleInt(this->_initialSize.cy, 96, dpi);
		RECT workArea = GetWindowWorkArea(this->Handle, POINT{ wr.left, wr.top });
		POINT origin{};
		if (this->_autoCenterOnCreate)
		{
			origin.x = workArea.left + ((workArea.right - workArea.left) - newW) / 2;
			origin.y = workArea.top + ((workArea.bottom - workArea.top) - newH) / 2;
		}
		else
		{
			origin.x = wr.left;
			origin.y = wr.top;
		}
		origin = ClampWindowOriginToWorkArea(origin, SIZE{ newW, newH }, workArea);
		SetWindowPos(this->Handle, nullptr, origin.x, origin.y, newW, newH, SWP_NOZORDER | SWP_NOACTIVATE);
		SyncRenderSizeToClient();
		this->_hasRenderedOnce = false;
		this->Invalidate(false);
	}

	// 更新 HeadHeight（物理像素）并为渲染目标设置 DPI
	ApplyDpiChange(dpi);
}

void Form::EnsureOleInitialized()
{
	static bool inited = false;
	if (inited) return;
	inited = true;
	OleInitialize(nullptr);
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

ID2D1Bitmap* Form::EnsureImageCache()
{
	if (!_imageSource || !this->Render)
		return nullptr;
	auto* target = this->Render->GetRenderTargetRaw();
	if (!target)
		return nullptr;
	if (_imageCache && _imageCacheTarget == target)
		return _imageCache.Get();
	_imageCache.Reset();
	_imageCacheTarget = target;
	auto* bmp = this->Render->CreateBitmap(_imageSource);
	if (!bmp)
		return nullptr;
	_imageCache.Attach(bmp);
	return _imageCache.Get();
}

void Form::ResetImageCache()
{
	_imageCache.Reset();
	_imageCacheTarget = nullptr;
}

void Form::RecoverRenderIfNeeded()
{
	if (_recoveringDeviceLost)
		return;
	_recoveringDeviceLost = true;

	// 只有在句柄存在时才尝试恢复
	if (!this->Handle || !::IsWindow(this->Handle))
	{
		_recoveringDeviceLost = false;
		return;
	}

	bool need = false;
	if (this->Render && this->Render->IsDeviceLost()) need = true;
	if (this->OverlayRender && this->OverlayRender->IsDeviceLost()) need = true;
	for (const auto& layer : this->_dcompD2DLayers)
	{
		if (layer.Render && layer.Render->IsDeviceLost()) need = true;
	}
	if (!need)
	{
		_recoveringDeviceLost = false;
		return;
	}

	// 先释放旧渲染对象
	if (this->OverlayRender)
	{
		delete this->OverlayRender;
		this->OverlayRender = nullptr;
	}
	ReleaseDCompD2DLayers();
	if (this->Render)
	{
		delete this->Render;
		this->Render = nullptr;
	}

		bool hadDCompHost = _dcompHost != nullptr;
		if (_dcompHost)
	{
		delete _dcompHost;
		_dcompHost = nullptr;
	}
		if (hadDCompHost)
		{
				EnsureDCompInitialized();
		}
	if (!Render)
	{
		Render = new HwndGraphics(this->Handle);
		OverlayRender = nullptr;
	}

	SyncRenderSizeToClient();
	this->_hasRenderedOnce = false;
	this->Invalidate(false);
	_recoveringDeviceLost = false;
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
		// physical→logical: layout coords match D2D logical (96-DPI) space
		const float dpiScL = GetDpiScale();
		clientSize.cx = (LONG)(clientSize.cx / dpiScL);
		clientSize.cy = (LONG)(clientSize.cy / dpiScL);
		LayoutMainTopBar(this, clientSize);
		float contentLeft = 0.0f;
		float contentTop = 0.0f;
		float contentWidth = (float)clientSize.cx;
		float statusBarHeight = LayoutMainStatusBar(this, clientSize);
		float contentHeight = (float)clientSize.cy - statusBarHeight;
		if (contentHeight < 0.0f) contentHeight = 0.0f;
		
		for (size_t i = 0; i < this->Controls.size(); i++)
		{
			auto control = this->Controls[i];
			if (!control || !control->Visible) continue;
			if (control->Type() == UIClass::UI_Menu) continue;
			if (control == this->MainToolBar) continue;
			if (control == this->MainStatusBar) continue;
			
			POINT location = control->Location;
			Thickness margin = control->Margin;
			uint8_t anchor = control->AnchorStyles;
			HorizontalAlignment hAlign = control->HAlign;
			VerticalAlignment vAlign = control->VAlign;
			SIZE size = control->MeasureCore({ (LONG)contentWidth, (LONG)contentHeight });

			float x = contentLeft + margin.Left;
			float y = contentTop + margin.Top;
			float w = (float)size.cx;
			float h = (float)size.cy;
			
			// 应用 Anchor
			if (anchor != AnchorStyles::None)
			{
				// 左右都锚定：宽度随窗口变化
				if ((anchor & AnchorStyles::Left) && (anchor & AnchorStyles::Right))
				{
					x = (float)location.x;
					w = contentWidth - (float)location.x - margin.Right;
					if (w < 0) w = 0;
				}
				// 只锚定右边：跟随右边缘
				else if (anchor & AnchorStyles::Right)
				{
					x = (float)clientSize.cx - margin.Right - w;
				}
				else
				{
					x = (float)location.x;
				}
				
				// 上下都锚定：高度随窗口变化
				if ((anchor & AnchorStyles::Top) && (anchor & AnchorStyles::Bottom))
				{
					y = (float)location.y;
					h = contentHeight - (float)location.y - margin.Bottom;
					if (h < 0) h = 0;
				}
				// 只锚定下边：跟随下边缘
				else if (anchor & AnchorStyles::Bottom)
				{
					y = (float)clientSize.cy - margin.Bottom - h;
				}
				else
				{
					y = (float)location.y;
				}
			}
			else
			{
				if (hAlign == HorizontalAlignment::Stretch)
				{
					x = margin.Left;
					w = (float)clientSize.cx - margin.Left - margin.Right;
				}
				else if (hAlign == HorizontalAlignment::Center)
				{
					float availableWidth = (float)clientSize.cx - margin.Left - margin.Right;
					if (availableWidth < 0) availableWidth = 0;
					x = margin.Left + (availableWidth - w) / 2.0f;
				}
				else if (hAlign == HorizontalAlignment::Right)
				{
					x = (float)clientSize.cx - margin.Right - w;
				}
				else
				{
					x = (float)location.x;
				}
				
				if (vAlign == VerticalAlignment::Stretch)
				{
					y = margin.Top;
					h = (float)clientSize.cy - margin.Top - margin.Bottom;
				}
				else if (vAlign == VerticalAlignment::Top)
				{
					y = (float)location.y;
				}
				else if (vAlign == VerticalAlignment::Center)
				{
					float availableHeight = (float)clientSize.cy - margin.Top - margin.Bottom;
					if (availableHeight < 0) availableHeight = 0;
					y = margin.Top + (availableHeight - h) / 2.0f;
				}
				else if (vAlign == VerticalAlignment::Bottom)
				{
					y = (float)clientSize.cy - margin.Bottom - h;
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
			// physical→logical: layout coords match D2D logical (96-DPI) space
			const float dpiScL = GetDpiScale();
			clientSize.cx = (LONG)(clientSize.cx / dpiScL);
			clientSize.cy = (LONG)(clientSize.cy / dpiScL);
			LayoutMainTopBar(this, clientSize);
			float statusBarHeight = LayoutMainStatusBar(this, clientSize);
			SIZE contentSize = clientSize;
			contentSize.cy = (LONG)(std::max)(0.0f, (float)clientSize.cy - statusBarHeight);
			_layoutEngine->Measure(nullptr, contentSize);
			
			D2D1_RECT_F finalRect = { 
				0, 0, 
				(float)contentSize.cx, 
				(float)contentSize.cy 
			};
			_layoutEngine->Arrange(nullptr, finalRect);
		}
	}
	
	_needsLayout = false;
}

void Form::ApplyWindowIcon()
{
	if (!this->Handle) return;
	HICON largeIcon = this->Icon ? this->Icon : LoadProcessIcon(false);
	HICON smallIcon = this->Icon ? this->Icon : LoadProcessIcon(true);
	if (largeIcon) SendMessage(this->Handle, WM_SETICON, ICON_BIG, (LPARAM)largeIcon);
	if (smallIcon) SendMessage(this->Handle, WM_SETICON, ICON_SMALL, (LPARAM)smallIcon);
}

void Form::Show()
{
	EnsureInitialDpiApplied();
	ApplyWindowIcon();
	SyncFormWindowStyles(this->Handle, this->_showInTaskBar, this->MinBox, this->MaxBox, this->CloseBox, this->AllowResize);
	ShowWindow(this->Handle, SW_SHOWNORMAL);
	this->OnSizeChanged(this);
	this->Invalidate(true);
}
static HWND GetBestOwnerWindowInCurrentProcess(HWND exclude = nullptr)
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

	return nullptr;
}

void Form::ShowDialog(HWND parent)
{
	EnsureInitialDpiApplied();
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

	ApplyWindowIcon();
	SyncFormWindowStyles(this->Handle, this->_showInTaskBar, this->MinBox, this->MaxBox, this->CloseBox, this->AllowResize);
	ShowWindow(this->Handle, SW_SHOWNORMAL);
	this->OnSizeChanged(this);
	this->Invalidate(true);
	SetForegroundWindow(this->Handle);
	SetActiveWindow(this->Handle);

	MSG messageRecord;
	while (IsWindow(this->Handle))
	{
		BOOL r = GetMessageW(&messageRecord, nullptr, 0, 0);
		if (r <= 0) break;
		TranslateMessage(&messageRecord);
		DispatchMessageW(&messageRecord);
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
	bool canceled = false;
	this->OnClosing(this, canceled);
	if (!canceled)
		PostMessageW(this->Handle, WM_CLOSE, 0, 0);
}
bool Form::DoEvent()
{
	bool hasMessage = false;
	MSG messageRecord;
	while (PeekMessage(&messageRecord, nullptr, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&messageRecord);
		DispatchMessage(&messageRecord);
		hasMessage = true;
	}
	if (!hasMessage && Application::Forms.size() > 0)
	{
		WaitMessage();
	}
	return hasMessage;
}
bool Form::WaitEvent()
{
	MSG messageRecord;
	if (GetMessageW(&messageRecord, nullptr, 0, 0))
	{
		TranslateMessage(&messageRecord);
		DispatchMessageW(&messageRecord);
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
	// dirty rect from OS is in physical pixels; D2D after SetDpi works in logical coords
	const float dpiSc = GetDpiScale();
	auto toLogical = [&](RECT r) -> RECT {
		return RECT{ (LONG)std::floor(r.left / dpiSc), (LONG)std::floor(r.top / dpiSc),
		             (LONG)std::ceil (r.right / dpiSc), (LONG)std::ceil (r.bottom / dpiSc) };
	};
	RECT logClientRc = toLogical(clientRc);
	RECT drawRc = toLogical(dirty);
	if (force || !this->_hasRenderedOnce)
	{
		drawRc = logClientRc;
	}

	this->Render->BeginRender();
	this->Render->ClearTransform();
	this->Render->PushDrawRect((float)drawRc.left, (float)drawRc.top, (float)(drawRc.right - drawRc.left), (float)(drawRc.bottom - drawRc.top));
	this->Render->FillRect((float)drawRc.left, (float)drawRc.top, (float)(drawRc.right - drawRc.left), (float)(drawRc.bottom - drawRc.top), this->BackColor);
	this->Render->DrawRect((float)drawRc.left, (float)drawRc.top, (float)(drawRc.right - drawRc.left), (float)(drawRc.bottom - drawRc.top), this->BorderLightColor, 2.0f);
	this->Render->DrawRect((float)drawRc.left, (float)drawRc.top, (float)(drawRc.right - drawRc.left), (float)(drawRc.bottom - drawRc.top), this->BorderDarkColor, 1.0f);

	if (this->Image)
	{
		this->Render->PushDrawRect((float)drawRc.left, (float)drawRc.top, (float)(drawRc.right - drawRc.left), (float)(drawRc.bottom - drawRc.top));
		this->RenderImage();
		this->Render->PopDrawRect();
	}

	if (VisibleHead)
	{
		const float logW = this->Size.cx / dpiSc;
		const float logH = this->HeadHeight / dpiSc;
		RECT headRc{ 0, 0, (LONG)logW, (LONG)logH };
		if (RectIntersects(drawRc, headRc))
		{
			this->Render->FillRect(0, 0, logW, logH, this->HeadBackColor);
			auto font = this->GetFont();
			float headTextTop = (logH - font->FontHeight) * 0.5f;
			if (headTextTop < 0.0f)
				headTextTop = 0.0f;
			this->Render->PushDrawRect(0, 0, logW, logH);
			if (this->CenterTitle)
			{
				auto tSize = font->GetTextSize(this->Text);
				float textRangeWidth = logW;
				int buttonCount = 0;
				if (this->MinBox) buttonCount++;
				if (this->MaxBox) buttonCount++;
				if (this->CloseBox) buttonCount++;
				textRangeWidth -= (logH * buttonCount);
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
							const float restoreStroke = (std::min)(stroke, (std::max)(1.0f, s * 0.055f));
							const float total = s * 0.46f;
							const float rect = total * 0.68f;
							const float off = total - rect;
							const float xFront = cx - total * 0.5f;
							const float yFront = cy - total * 0.5f + off;
							const float xBack = xFront + off;
							const float yBack = yFront - off;
							this->Render->DrawLine({ xBack, yBack }, { xBack + rect, yBack }, this->ForeColor, restoreStroke);
							this->Render->DrawLine({ xBack + rect, yBack }, { xBack + rect, yBack + rect }, this->ForeColor, restoreStroke);
							this->Render->DrawLine({ xBack, yBack }, { xBack, yFront }, this->ForeColor, restoreStroke);
							this->Render->DrawLine({ xFront + rect, yBack + rect }, { xBack + rect, yBack + rect }, this->ForeColor, restoreStroke);
							this->Render->DrawRect(xFront, yFront, rect, rect, this->ForeColor, restoreStroke);
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
	const int top = (int)(ClientTop() / dpiSc);  // logical head height
	const int logContentW = (int)(this->Size.cx / dpiSc);
	const int logContentH = (int)(this->Size.cy / dpiSc);
	RECT contentDirty = drawRc;
	contentDirty.top -= top;
	contentDirty.bottom -= top;
	if (contentDirty.top < 0) contentDirty.top = 0;
	if (contentDirty.left < 0) contentDirty.left = 0;
	if (contentDirty.right > logContentW) contentDirty.right = logContentW;
	if (contentDirty.bottom > (logContentH - top)) contentDirty.bottom = (logContentH - top);

	if (contentDirty.right > contentDirty.left && contentDirty.bottom > contentDirty.top)
	{
		if (_dcompHost)
		{
			RenderDCompRootLayers(contentDirty, top, dpiSc);
		}
		else
		{
			this->Render->SetTransform(D2D1::Matrix3x2F::Translation(0.0f, (float)top));
			this->Render->PushDrawRect((float)contentDirty.left, (float)contentDirty.top, (float)(contentDirty.right - contentDirty.left), (float)(contentDirty.bottom - contentDirty.top));

			for (auto c : GetRootControlsInZOrder(this))
			{
				if (!c || !c->Visible) continue;
				if (c == this->ForegroundControl) continue;
				if (c == this->MainMenu) continue;
				if (this->MainStatusBar && this->MainStatusBar->TopMost && c == this->MainStatusBar)
					continue;
				RECT crc = ToRECT(c->AbsRect, 2);
				if (!RectIntersects(contentDirty, crc)) continue;
				if (c->ParentForm->Render == nullptr)
					c->ParentForm->Render = this->Render;
				c->Update();
			}

			if (this->MainStatusBar && this->MainStatusBar->TopMost && this->MainStatusBar->Visible)
			{
				this->MainStatusBar->Update();
			}

			if (!this->OverlayRender)
			{
				if (this->MainMenu && this->MainMenu->Visible)
				{
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
	}

	this->OnPaint(this);

	this->Render->PopDrawRect();
	this->Render->EndRender();
	CommitComposition();
	RecoverRenderIfNeeded();

	if (this->OverlayRender)
	{
		auto* oldRender = this->Render;
		RECT fullClient{};
		::GetClientRect(this->Handle, &fullClient);
		// physical→logical for D2D overlay
		const float ovLogW = (fullClient.right  - fullClient.left) / dpiSc;
		const float ovLogH = (fullClient.bottom - fullClient.top)  / dpiSc;

		this->OverlayRender->BeginRender();
		this->OverlayRender->ClearTransform();
		this->OverlayRender->Clear(D2D1_COLOR_F{ 0.0f,0.0f,0.0f,0.0f });
		this->OverlayRender->PushDrawRect(0.0f, 0.0f, ovLogW, ovLogH);

		const int ovTop = (int)(ClientTop() / dpiSc);  // logical head height
		RECT overlayContent{};
		overlayContent.left   = 0;
		overlayContent.top    = 0;
		overlayContent.right  = (LONG)ovLogW;
		overlayContent.bottom = (LONG)ovLogH - ovTop;

		if (overlayContent.right > overlayContent.left && overlayContent.bottom > overlayContent.top)
		{
			this->OverlayRender->SetTransform(D2D1::Matrix3x2F::Translation(0.0f, (float)ovTop));
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
		RecoverRenderIfNeeded();
	}

	this->ControlChanged = false;
	this->_hasRenderedOnce = true;
	RefreshAnimationTimer();
	return true;
}
bool Form::ForceUpdate()
{
	this->Invalidate(true);
	return true;
}

bool Form::RemoveControl(Control* control)
{
	if (std::find(this->Controls.begin(), this->Controls.end(), control) == this->Controls.end())
		return false;
	this->Controls.erase(
		std::remove(this->Controls.begin(), this->Controls.end(), control),
		this->Controls.end());
	if (this->ForegroundControl == control)
		this->ForegroundControl = nullptr;
	if (this->MainMenu == control)
		this->MainMenu = nullptr;
	if (this->MainToolBar == control)
		this->MainToolBar = nullptr;
	if (this->MainStatusBar == control)
		this->MainStatusBar = nullptr;
	if (this->UnderMouse == control)
		this->UnderMouse = nullptr;
	if (this->Selected == control)
		this->SetSelectedControl(nullptr, true);
	if (this->_hoverControl == control)
		this->_hoverControl = nullptr;
	if (this->_mouseCaptureControl == control)
	{
		this->_mouseCaptureControl = nullptr;
		if (this->Handle && GetCapture() == this->Handle)
			ReleaseCapture();
	}
	control->Parent = nullptr;
	control->ParentForm = nullptr;
	return true;
}
bool Form::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY)
{
	if (!this->Enable || !this->Visible) return true;
	POINT mouse;
	GetCursorPos(&mouse);
	ScreenToClient(this->Handle, &mouse);
	const int titleBarHeight = ClientTop();  // physical HeadHeight, for title bar (OS-level) comparisons
	const float dpiScale = GetDpiScale();
	POINT contentMouse{ (LONG)(mouse.x / dpiScale), (LONG)((mouse.y - titleBarHeight) / dpiScale) };
	Control* hitControl = nullptr;
	auto anyMouseButtonDown = []() -> bool
		{
			return (GetAsyncKeyState(VK_LBUTTON) & 0x8000) ||
				(GetAsyncKeyState(VK_RBUTTON) & 0x8000) ||
				(GetAsyncKeyState(VK_MBUTTON) & 0x8000);
		};
	auto forwardToCapturedControl = [&](UINT messageId, WPARAM wParamValue, LPARAM lParamValue) -> bool
		{
			if (!this->_mouseCaptureControl || !this->_mouseCaptureControl->IsVisual)
				return false;
			hitControl = this->_mouseCaptureControl;
			auto location = this->_mouseCaptureControl->AbsLocation;
			this->_mouseCaptureControl->ProcessMessage(messageId, wParamValue, lParamValue, contentMouse.x - location.x, contentMouse.y - location.y);
			return true;
		};
	auto releaseCapturedControlIfIdle = [&]()
		{
			if (this->_mouseCaptureControl && !anyMouseButtonDown())
			{
				this->_mouseCaptureControl = nullptr;
				if (this->Handle && GetCapture() == this->Handle)
					ReleaseCapture();
			}
		};
	switch (message)
	{
	case WM_DROPFILES:
	{
		HDROP hDropInfo = HDROP(wParam);
		UINT fileCount = DragQueryFile(hDropInfo, 0xffffffff, nullptr, 0);
		TCHAR fileName[MAX_PATH];
		std::vector<std::wstring> files;
		for (UINT fileIndex = 0; fileIndex < fileCount; fileIndex++)
		{
			DragQueryFile(hDropInfo, fileIndex, fileName, MAX_PATH);
			files.push_back(fileName);
		}
		DragFinish(hDropInfo);
		if (files.size() > 0)
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

		if (this->VisibleHead && mouse.y < titleBarHeight)
		{
			if (this->_mouseCaptureControl && forwardToCapturedControl(message, wParam, lParam))
			{
				UpdateCursor(mouse, contentMouse);
				break;
			}
			RaiseControlMouseEnterLeave(this, this->_hoverControl, nullptr, contentMouse);
			this->_hoverControl = nullptr;
			this->UnderMouse = nullptr;
			this->OnMouseMove(this, MouseEventArgs(MouseButtons::None, 0, contentMouse.x, contentMouse.y, 0));
			ApplyCursor(CursorKind::Arrow);
			break;
		}

		if (this->_mouseCaptureControl && forwardToCapturedControl(message, wParam, lParam))
		{
			UpdateCursor(mouse, contentMouse);
			break;
		}

		if (this->Selected && (GetAsyncKeyState(VK_LBUTTON) & 0x8000))
		{
			if (this->Selected->IsVisual)
			{
				RaiseControlMouseEnterLeave(this, this->_hoverControl, this->Selected, contentMouse);
				this->_hoverControl = this->Selected;
				this->UnderMouse = this->Selected;
				hitControl = this->Selected;
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
			hitControl = hit;
			auto hitLocation = hit->AbsLocation;
			hit->ProcessMessage(message, wParam, lParam, contentMouse.x - hitLocation.x, contentMouse.y - hitLocation.y);
		}
		this->UnderMouse = this->_hoverControl;
		UpdateCursor(mouse, contentMouse);
		this->OnMouseMove(this, MouseEventArgs(MouseButtons::None, 0, contentMouse.x, contentMouse.y, 0));
	}
	break;
	case WM_MOUSELEAVE:
	{
		this->_mouseLeaveTracking = false;
		if (this->_mouseCaptureControl)
		{
			UpdateCursorFromCurrentMouse();
			break;
		}
		RaiseControlMouseEnterLeave(this, this->_hoverControl, nullptr, contentMouse);
		this->_hoverControl = nullptr;
		this->UnderMouse = nullptr;
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
		Control* selectedBeforeLeftDown = (message == WM_LBUTTONDOWN) ? this->Selected : nullptr;
		Control* pointerHover = nullptr;
		if (!(this->VisibleHead && mouse.y < titleBarHeight))
		{
			pointerHover = HitTestControlAt(contentMouse);
		}
		RaiseControlMouseEnterLeave(this, this->_hoverControl, pointerHover, contentMouse);
		this->_hoverControl = pointerHover;
		this->UnderMouse = pointerHover;

		DismissForegroundOnOutsideMouseDown(this, contentMouse, message);

		if (message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN || message == WM_MBUTTONDOWN)
		{
			if (!(this->VisibleHead && mouse.y < titleBarHeight))
			{
				Control* hit = HitTestControlAt(contentMouse);
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

				if (mouse.y < titleBarHeight)
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
					hitControl = this->Selected;
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
		if (this->VisibleHead && mouse.y < titleBarHeight)
		{
			if ((message == WM_LBUTTONUP || message == WM_RBUTTONUP || message == WM_MBUTTONUP) &&
				this->_mouseCaptureControl && forwardToCapturedControl(message, wParam, lParam))
			{
				releaseCapturedControlIfIdle();
				UpdateCursor(mouse, contentMouse);
				break;
			}
			break;
		}

		if ((message == WM_MOUSEWHEEL || message == WM_LBUTTONUP || message == WM_RBUTTONUP || message == WM_MBUTTONUP) &&
			this->_mouseCaptureControl && forwardToCapturedControl(message, wParam, lParam))
		{
			if (message == WM_LBUTTONUP || message == WM_RBUTTONUP || message == WM_MBUTTONUP)
				releaseCapturedControlIfIdle();
			UpdateCursor(mouse, contentMouse);
			break;
		}

		if (message == WM_MOUSEWHEEL)
		{
			const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
			Control* wheelHit = HitTestControlAt(contentMouse);
			for (Control* target = wheelHit; target; target = target->Parent)
			{
				if (!target->HandlesMouseWheel()) continue;
				POINT targetAbs = target->AbsLocation;
				const int targetX = contentMouse.x - targetAbs.x;
				const int targetY = contentMouse.y - targetAbs.y;
				if (!target->CanHandleMouseWheel(delta, targetX, targetY)) continue;
				if (target->ProcessMessage(message, wParam, lParam, targetX, targetY))
				{
					hitControl = target;
					break;
				}
			}
			this->OnMouseWheel(this, MouseEventArgs(MouseButtons::None, 0, contentMouse.x, contentMouse.y, delta));
			break;
		}

		auto hit = HitTestRootControlAt(this, contentMouse);
		if (hit)
		{
			hitControl = hit;
			auto hitLocation = hit->AbsLocation;
			if (message == WM_MOUSEWHEEL)
			{
				const int controlLocalX = contentMouse.x - hitLocation.x;
				const int controlLocalY = contentMouse.y - hitLocation.y;
				const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
				if (!hit->CanHandleMouseWheel(delta, controlLocalX, controlLocalY))
					hitControl = nullptr;
				else if (!hit->ProcessMessage(message, wParam, lParam, controlLocalX, controlLocalY))
					hitControl = nullptr;
			}
			else
			{
				hit->ProcessMessage(message, wParam, lParam, contentMouse.x - hitLocation.x, contentMouse.y - hitLocation.y);
			}
			if (message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN || message == WM_MBUTTONDOWN)
			{
				this->_mouseCaptureControl = hit;
				if (this->Handle)
					SetCapture(this->Handle);
			}
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

		if (message == WM_LBUTTONDOWN && selectedBeforeLeftDown && this->Selected == selectedBeforeLeftDown && pointerHover != selectedBeforeLeftDown)
		{
			this->SetSelectedControl(nullptr, true);
		}

		if (message == WM_LBUTTONUP || message == WM_RBUTTONUP || message == WM_MBUTTONUP)
		{
			releaseCapturedControlIfIdle();
			UpdateCursor(mouse, contentMouse);
		}
	}
	break;
	case WM_CAPTURECHANGED:
	{
		if ((HWND)lParam != this->Handle)
			this->_mouseCaptureControl = nullptr;
	}
	break;
	case WM_KEYDOWN:
	{
		if (this->Selected)
		{
			if (this->Selected->ProcessMessage(message, wParam, lParam, localX, localY))
			{
				if (this->Selected->IsVisual)
				{
					hitControl = this->Selected;
					KeyEventArgs eventArgs = KeyEventArgs((Keys)(wParam | 0));
					this->OnKeyDown(this, eventArgs);
				}
			}
			else
			{
				auto fallbackTarget = GetScrollViewFallbackTarget(this->Selected, wParam);
				if (fallbackTarget && fallbackTarget->ProcessMessage(message, wParam, lParam, localX, localY))
				{
					hitControl = fallbackTarget;
					KeyEventArgs eventArgs = KeyEventArgs((Keys)(wParam | 0));
					this->OnKeyDown(this, eventArgs);
				}
			}
		}
		else
		{
			KeyEventArgs eventArgs = KeyEventArgs((Keys)(wParam | 0));
			this->OnKeyDown(this, eventArgs);
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
		if (this->ForegroundControl && this->ForegroundControl->Visible && this->ForegroundControl->AutoCloseOnFormFocusLoss())
		{
			this->ForegroundControl->ClosePopup();
			this->Invalidate(true);
		}
		if (this->MainMenu && this->MainMenu->Visible && this->MainMenu->AutoCloseOnFormFocusLoss())
		{
			this->MainMenu->ClosePopup();
			this->Invalidate(true);
		}
		this->OnLostFocus(this);
	}
	break;
	case WM_KEYUP:
	{
		if (this->Selected)
		{
			if (this->Selected->ProcessMessage(message, wParam, lParam, localX, localY))
			{
				hitControl = this->Selected;
				KeyEventArgs eventArgs = KeyEventArgs((Keys)(wParam | 0));
				this->OnKeyUp(this, eventArgs);
			}
		}
		else
		{
			KeyEventArgs eventArgs = KeyEventArgs((Keys)(wParam | 0));
			this->OnKeyUp(this, eventArgs);
		}
	}
	break;
	case WM_SIZE:
	{
		RECT rec;
		GetClientRect(this->Handle, &rec);
		UINT width = (UINT)std::max<LONG>(1, rec.right - rec.left);
		UINT height = (UINT)std::max<LONG>(1, rec.bottom - rec.top);
		if (this->_dcompHost) this->_dcompHost->UpdateD2DLayerSize(width, height);
		this->Render->ReSize(width, height);
		if (this->OverlayRender) this->OverlayRender->ReSize(width, height);
		for (auto& layer : this->_dcompD2DLayers)
			if (layer.Render) layer.Render->ReSize(width, height);
		if (this->Render)        this->Render->SetDpi((FLOAT)this->_dpi, (FLOAT)this->_dpi);
		if (this->OverlayRender) this->OverlayRender->SetDpi((FLOAT)this->_dpi, (FLOAT)this->_dpi);
		for (auto& layer : this->_dcompD2DLayers)
			if (layer.Render) layer.Render->SetDpi((FLOAT)this->_dpi, (FLOAT)this->_dpi);
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
			if (this->Selected->ProcessMessage(message, wParam, lParam, localX, localY))
			{
				hitControl = this->Selected;
				this->OnCharInput(this, (wchar_t)(wParam));
			}
		}
		else
		{
			hitControl = nullptr;
			this->OnCharInput(this, (wchar_t)(wParam));
		}
	}
	break;
	case WM_IME_COMPOSITION:
	{
		if (this->Selected)
		{
			hitControl = this->Selected;
			this->Selected->ProcessMessage(message, wParam, lParam, localX, localY);
		}
	}
	break;
	case WM_CLOSE:
	{
		this->OnFormClosing(this);
		delete this->Render;
		this->Render = nullptr;
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
			old->InvalidateVisual();
		}
		if (now)
		{
			now->OnGotFocus(now);
			now->InvalidateVisual();
		}
	}
	if (WM_LBUTTONDOWN == message && hitControl == nullptr && this->Selected && hitControl != this->Selected)
	{
		this->SetSelectedControl(nullptr, true);
		UpdateCursor(mouse, contentMouse);
	}
	return true;
}
void Form::RenderImage()
{
	auto* bmp = EnsureImageCache();
	if (bmp)
	{
		auto size = bmp->GetSize();
		if (size.width > 0 && size.height > 0)
		{
			// 自绘标题栏属于 client 区域的一部分：背景图应铺满整个窗口区域（D2D 逻辑坐标）
			const float rSc = GetDpiScale();
			const struct { float cx, cy; } asize = { this->Size.cx / rSc, this->Size.cy / rSc };
			switch (this->SizeMode)
			{
			case ImageSizeMode::Normal:
			{
				this->Render->DrawBitmap(bmp, 0, 0, size.width, size.height);
			}
			break;
			case ImageSizeMode::CenterImage:
			{
				float xf = (asize.cx - size.width) / 2.0f;
				float yf = (asize.cy - size.height) / 2.0f;
				this->Render->DrawBitmap(bmp, xf, yf, size.width, size.height);
			}
			break;
			case ImageSizeMode::StretchImage:
			{
				this->Render->DrawBitmap(bmp, 0, 0, (float)asize.cx, (float)asize.cy);
			}
			break;
			case ImageSizeMode::Zoom:
			{
				float xp = asize.cx / size.width, yp = asize.cy / size.height;
				float tp = xp < yp ? xp : yp;
				float tw = size.width * tp, th = size.height * tp;
				float xf = (asize.cx - tw) / 2.0f, yf = (asize.cy - th) / 2.0f;
				this->Render->DrawBitmap(bmp, xf, yf, tw, th);
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
	if (this->Controls.size())
	{
		return this->Controls.back();
	}
	return nullptr;
}
D2D1_RECT_F Form::ChildRect()
{
	if (this->Controls.size() == 0)
		return D2D1_RECT_F{ 0,0,0,0 };
	float left = FLT_MAX;
	float top = FLT_MAX;
	float right = FLT_MIN;
	float bottom = FLT_MIN;
	if (this->Controls.size())
	{
		for (auto control : this->Controls)
		{
			auto location = control->ActualLocation;
			auto size = control->ActualSize();
			auto bottomRight = D2D1_POINT_2F{ (float)location.x + size.cx,(float)location.y + size.cy };
			if (bottomRight.x < left)left = bottomRight.x;
			if (bottomRight.x > right)right = bottomRight.x;

			if (bottomRight.y < top)top = bottomRight.y;
			if (bottomRight.y > bottom)bottom = bottomRight.y;
		}
	}
	return D2D1_RECT_F{ left,top,right,bottom };
}
LRESULT CustomFrameHitTest(HWND _hWnd, WPARAM wParam, LPARAM lParam, int captionHeight, UINT dpi)
{
	const int scalerWidth = (std::max)(1, Application::ScaleInt(8, 96, dpi));
	RECT wr, cr;
	const POINT ptMouse = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
	GetWindowRect(_hWnd, &wr);
	cr.left = wr.left + scalerWidth;
	cr.right = wr.right - scalerWidth;
	cr.bottom = wr.bottom - scalerWidth;
	cr.top = wr.top + scalerWidth;

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
	if ((ULONG64)form != 0xFFFFFFFFFFFFFFFF && Application::Forms.find(form->Handle) != Application::Forms.end())
	{
		if (message == WM_DPICHANGED)
		{
			UINT newDpi = HIWORD(wParam);
			RECT* suggested = (RECT*)lParam;
			if (suggested)
			{
				SetWindowPos(hWnd, nullptr,
					suggested->left,
					suggested->top,
					suggested->right - suggested->left,
					suggested->bottom - suggested->top,
					SWP_NOZORDER | SWP_NOACTIVATE);
				form->_initialWindowRectApplied = !form->_initialDpiApplied;
			}
			// 尺寸/DPI 变化后，强制同步渲染目标尺寸并安排一次重绘，避免出现新区域未刷新。
			form->SyncRenderSizeToClient();
			form->_hasRenderedOnce = false;
			form->Invalidate(false);
			// 若窗口尚未首次显示，控件树可能还未构造完成：此时只记录 DPI，真正缩放留到 Show 前。
			if (!form->_initialDpiApplied)
			{
				form->_dpi = newDpi;
				return 0;
			}
			form->ApplyDpiChange(newDpi);
			return 0;
		}

		form->ProcessMessage(message, wParam, lParam, 0, 0);

		// After any button-up, release lingering mouse capture from child controls
		// (TabControl / GridView call SetCapture for drag tracking; if the mouse moves
		// to the resize border before the button is released, the hit-test in
		// ProcessMessage finds no control there, so the child never receives its
		// WM_LBUTTONUP and never calls ReleaseCapture — leaving capture stuck and
		// blocking the OS from starting a window-resize drag via WM_NCHITTEST).
		if ((message == WM_LBUTTONUP || message == WM_RBUTTONUP || message == WM_MBUTTONUP)
			&& GetCapture() == hWnd && !form->_capTracking)
		{
			ReleaseCapture();
		}

		switch (message)
		{
		case WM_NCCALCSIZE:
		{
			if (wParam)
			{
				NCCALCSIZE_PARAMS* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
				if (params)
				{
					if (IsZoomed(hWnd))
					{
						const int inset = GetCustomFrameInset();
						params->rgrc[0].left += inset;
						params->rgrc[0].right -= inset;
						params->rgrc[0].top += inset;
						params->rgrc[0].bottom -= inset;
					}
					return 0;
				}
			}
			break;
		}
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
			LRESULT hitTestResult;
			if (!DwmDefWindowProc(hWnd, message, wParam, lParam, &hitTestResult))
			{
				POINT ptClient{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
				ScreenToClient(hWnd, &ptClient);
				if (form->HitTestCaptionButtonResizeExclusion(ptClient))
					return HTCLIENT;

				hitTestResult = CustomFrameHitTest(hWnd, wParam, lParam, form->ClientTop(), form->_dpi);
				if (hitTestResult == HTCAPTION)
				{
					CaptionButtonKind k{};
					if (form->HitTestCaptionButtons(ptClient, k))
						return HTCLIENT;
				}
				if (IsZoomed(hWnd))
				{
					// 最大化状态下禁止鼠标拖拽边缘/角落调整窗口大小
					if (hitTestResult == HTLEFT || hitTestResult == HTRIGHT || hitTestResult == HTTOP || hitTestResult == HTBOTTOM ||
						hitTestResult == HTTOPLEFT || hitTestResult == HTTOPRIGHT || hitTestResult == HTBOTTOMLEFT || hitTestResult == HTBOTTOMRIGHT)
					{
						return HTCLIENT;
					}
				}
				if (!form->AllowResize)
				{
					// 禁用边缘/角落 resize，只保留标题栏拖动与正常客户区
					if (hitTestResult == HTLEFT || hitTestResult == HTRIGHT || hitTestResult == HTTOP || hitTestResult == HTBOTTOM ||
						hitTestResult == HTTOPLEFT || hitTestResult == HTTOPRIGHT || hitTestResult == HTBOTTOMLEFT || hitTestResult == HTBOTTOMRIGHT)
					{
						return HTCLIENT;
					}
				}
				if (hitTestResult != HTCAPTION)
				{
					return hitTestResult;
				}
			}
		}
		break;
		case WM_NCDESTROY:
		{
			form->OnFormClosed(form);
			Application::Forms.erase(form->Handle);
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
				}
			}
		}
		break;
	}
	}
	return DefWindowProc(hWnd, message, wParam, lParam);
}
