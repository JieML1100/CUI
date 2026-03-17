#include "Control.h"
#include "Form.h"
#include "Panel.h"
#include <cmath>

#pragma warning(disable: 4267)
#pragma warning(disable: 4244)
#pragma warning(disable: 4018)

Control::Control()
	:
	Enable(true),
	Visible(true),
	Checked(false),
	ParentForm(nullptr),
	Parent(nullptr),
	Tag(NULL),
	SizeMode(ImageSizeMode::Zoom),
	_text(L"")
{
	this->_layoutBaseSize = this->_size;
	this->_layoutBaseInitialized = true;
	this->_location = POINT{ 0, 0 };
	this->_runtimeLocation = POINT{ 0, 0 };
}
Control::~Control()
{
	this->_imageCache.Reset();
	this->_imageCacheTarget = nullptr;
	this->_imageSource.reset();
	if (this->_font && this->_ownsFont)
	{
		delete this->_font;
	}
	this->_font = NULL;
	this->_ownsFont = false;
	for (auto c : this->Children)
	{
		delete c;
	}
}
UIClass Control::Type() { return UIClass::UI_Base; }

void Control::setTextPrivate(std::wstring s)
{
	this->_text = s;
}
void Control::Update() {}

void Control::RequestLayout()
{
	if (this->Parent)
	{
		auto* panelParent = dynamic_cast<Panel*>(this->Parent);
		if (panelParent)
		{
			panelParent->InvalidateLayout();
		}
		return;
	}

	if (this->ParentForm)
	{
		this->ParentForm->InvalidateLayout();
	}
}
void Control::BeginRender()
{
	auto size = this->ActualSize();
	BeginRender((float)size.cx, (float)size.cy);
}
void Control::BeginRender(float clipW, float clipH)
{
	if (!this->ParentForm || !this->ParentForm->Render) return;
	auto abs = this->AbsLocation;
	// HeadHeight is physical; divide by dpiScale to get logical units that match abs.y (logical)
	const float dpiSc = this->ParentForm->GetDpiScale();
	const float top = (this->ParentForm->VisibleHead ? this->ParentForm->HeadHeight / dpiSc : 0.0f);
	this->ParentForm->Render->PushLocalTransform((float)abs.x, (float)abs.y + top, clipW, clipH);
}
void Control::EndRender()
{
	if (!this->ParentForm || !this->ParentForm->Render) return;
	this->ParentForm->Render->PopLocalTransform();
}

void Control::PostRender()
{
	if (!this->IsVisual || !this->ParentForm) return;
	const float top = (this->ParentForm->VisibleHead ? (float)this->ParentForm->HeadHeight : 0.0f);
	auto r = this->AbsRect;
	r.top += top;
	r.bottom += top;

	if (_hasLastPostRenderClientRect)
	{
		D2D1_RECT_F u{};
		u.left = (std::min)(_lastPostRenderClientRect.left, r.left);
		u.top = (std::min)(_lastPostRenderClientRect.top, r.top);
		u.right = (std::max)(_lastPostRenderClientRect.right, r.right);
		u.bottom = (std::max)(_lastPostRenderClientRect.bottom, r.bottom);
		this->ParentForm->Invalidate(u, false);
	}
	else
	{
		this->ParentForm->Invalidate(r, false);
	}

	_lastPostRenderClientRect = r;
	_hasLastPostRenderClientRect = true;
}

void Control::UpdateCaretBlinkState(bool focused, int selectionStart, int selectionEnd, bool caretRectValid, const D2D1_RECT_F* caretRect)
{
	bool needReset = false;
	if (focused != _caretBlinkFocused)
		needReset = focused;
	if (selectionStart != _caretBlinkSelectionStart || selectionEnd != _caretBlinkSelectionEnd)
		needReset = true;
	if (caretRectValid != _caretBlinkRectValid)
		needReset = true;
	if (caretRectValid && caretRect)
	{
		if (!_caretBlinkRectValid ||
			std::fabs(_caretBlinkRect.left - caretRect->left) > 0.1f ||
			std::fabs(_caretBlinkRect.top - caretRect->top) > 0.1f ||
			std::fabs(_caretBlinkRect.right - caretRect->right) > 0.1f ||
			std::fabs(_caretBlinkRect.bottom - caretRect->bottom) > 0.1f)
		{
			needReset = true;
		}
		_caretBlinkRect = *caretRect;
	}
	else
	{
		_caretBlinkRect = { 0,0,0,0 };
	}

	_caretBlinkFocused = focused;
	_caretBlinkSelectionStart = selectionStart;
	_caretBlinkSelectionEnd = selectionEnd;
	_caretBlinkRectValid = caretRectValid;

	if (needReset || _caretBlinkResetTick == 0)
		_caretBlinkResetTick = ::GetTickCount64();
}

bool Control::IsCaretBlinkVisible() const
{
	if (!_caretBlinkFocused) return false;
	if (!_caretBlinkRectValid) return false;
	if (_caretBlinkSelectionStart != _caretBlinkSelectionEnd) return false;

	const UINT blinkTime = ::GetCaretBlinkTime();
	if (blinkTime == INFINITE || blinkTime == 0)
		return true;

	const ULONGLONG elapsed = ::GetTickCount64() - _caretBlinkResetTick;
	return ((elapsed / blinkTime) % 2ULL) == 0;
}

bool Control::IsCaretBlinkAnimating() const
{
	if (!_caretBlinkFocused) return false;
	if (!_caretBlinkRectValid) return false;
	if (_caretBlinkSelectionStart != _caretBlinkSelectionEnd) return false;

	const UINT blinkTime = ::GetCaretBlinkTime();
	return blinkTime != 0 && blinkTime != INFINITE;
}

bool Control::GetCaretBlinkInvalidRect(D2D1_RECT_F& outRect) const
{
	if (!_caretBlinkFocused) return false;
	if (!_caretBlinkRectValid) return false;
	if (_caretBlinkSelectionStart != _caretBlinkSelectionEnd) return false;
	outRect = _caretBlinkRect;
	return true;
}

GET_CPP(Control, class Font*, Font)
{
	if (this->_font)
		return this->_font;
	if (this->ParentForm)
		return this->ParentForm->GetFont();
	return GetDefaultFontObject();
}
SET_CPP(Control, class Font*, Font)
{
	this->SetFontEx(value, true);
}

void Control::SetFontEx(class Font* value, bool takeOwnership)
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
	this->PostRender();
}

GET_CPP(Control, int, Count)
{
	return this->Children.Count;
}
Control* Control::operator[](int index)
{
	return this->Children[index];
}
Control* Control::get(int index)
{
	if (this->Children.Count <= index)
		return NULL;
	return this->Children[index];
}
void Control::RemoveControl(Control* c)
{
	this->Children.Remove(c);
	c->Parent = NULL;
	c->ParentForm = NULL;
	if (!this->ParentForm) return;
	if (this->ParentForm->ForegroundControl == c)
		this->ParentForm->ForegroundControl = NULL;
	if (this->ParentForm->MainMenu == c)
		this->ParentForm->MainMenu = NULL;
	if (this->ParentForm->MainToolBar == c)
		this->ParentForm->MainToolBar = NULL;
	if (this->ParentForm->MainStatusBar == c)
		this->ParentForm->MainStatusBar = NULL;
	if (this->ParentForm->UnderMouse == c)
		this->ParentForm->UnderMouse = NULL;
}
GET_CPP(Control, POINT, AbsLocation)
{
	Control* tmpc = this;
	POINT tmpl = tmpc->ActualLocation;
	while (tmpc->Parent)
	{
		tmpc = tmpc->Parent;
		auto loc = tmpc->ActualLocation;
		auto childOffset = tmpc->GetChildrenRenderOffset();
		tmpl.x += loc.x;
		tmpl.y += loc.y;
		tmpl.x += childOffset.x;
		tmpl.y += childOffset.y;
	}
	return tmpl;
}
GET_CPP(Control, POINT, ActualLocation)
{
	return _runtimeLocation;
}
GET_CPP(Control, D2D1_RECT_F, AbsRect)
{
	Control* tmpc = this;
	auto absMin = this->AbsLocation;
	auto asize = this->ActualSize();
	return D2D1_RECT_F{
		(float)absMin.x,
		(float)absMin.y,
		(float)absMin.x + (float)asize.cx,
		(float)absMin.y + (float)asize.cy
	};
}
GET_CPP(Control, bool, IsVisual)
{
	if (this->Visible == false) return false;
	Control* tmpc = this;
	while (tmpc->Parent)
	{
		tmpc = tmpc->Parent;
		if (tmpc->Visible == false) return false;
	}
	return true;
}
GET_CPP(Control, POINT, Location)
{
	return _location;
}
SET_CPP(Control, POINT, Location)
{
	POINT oldConfiguredLocation = this->_location;
	POINT oldLocation = this->_runtimeLocation;
	_location = value;
	_runtimeLocation = value;
	this->RequestLayout();
	if (oldConfiguredLocation.x != _location.x || oldConfiguredLocation.y != _location.y ||
		oldLocation.x != _runtimeLocation.x || oldLocation.y != _runtimeLocation.y)
	{
		this->OnMoved(this);
	}
	this->PostRender();
}
GET_CPP(Control, SIZE, Size)
{
	return _size;
}
SET_CPP(Control, SIZE, Size)
{
	this->OnSizeChanged(this);
	_size = value;
	this->UpdateLayoutBaseSize(value);
	this->RequestLayout();
	this->PostRender();
}
GET_CPP(Control, int, Left)
{
	return this->_location.x;
}
SET_CPP(Control, int, Left)
{
	this->Location = POINT{ value, this->_location.y };
}
GET_CPP(Control, int, Top)
{
	return this->_location.y;
}
SET_CPP(Control, int, Top)
{
	this->Location = POINT{ this->_location.x, value };
}
GET_CPP(Control, int, Width)
{
	return this->_size.cx;
}
SET_CPP(Control, int, Width)
{
	this->OnSizeChanged(this);
	this->_size.cx = value;
	this->UpdateLayoutBaseSize(this->_size);
	this->RequestLayout();
	this->PostRender();
}
GET_CPP(Control, int, Height)
{
	return this->_size.cy;
}
SET_CPP(Control, int, Height)
{
	this->OnSizeChanged(this);
	_size.cy = value;
	this->UpdateLayoutBaseSize(this->_size);
	this->RequestLayout();
	this->PostRender();
}
GET_CPP(Control, float, Right)
{
	return this->Left + this->Width;
}
GET_CPP(Control, float, Bottom)
{
	return this->Top + this->Height;
}
GET_CPP(Control, std::wstring, Text)
{
	return _text;
}
SET_CPP(Control, std::wstring, Text)
{
	if (value != _text)
	{
		this->TextChanged = true;
		this->OnTextChanged(this, _text, value);
		this->PostRender();
	}
	_text = value;
}
GET_CPP(Control, D2D1_COLOR_F, BolderColor)
{
	return _boldercolor;
}
SET_CPP(Control, D2D1_COLOR_F, BolderColor)
{
	_boldercolor = value;
	this->PostRender();
}
GET_CPP(Control, D2D1_COLOR_F, BackColor)
{
	return _backcolor;
}
SET_CPP(Control, D2D1_COLOR_F, BackColor)
{
	_backcolor = value;
	this->PostRender();
}
GET_CPP(Control, D2D1_COLOR_F, ForeColor)
{
	return _forecolor;
}
SET_CPP(Control, D2D1_COLOR_F, ForeColor)
{
	_forecolor = value;
	this->PostRender();
}
GET_CPP(Control, std::shared_ptr<BitmapSource>, Image)
{
	return _imageSource;
}
SET_CPP(Control, std::shared_ptr<BitmapSource>, Image)
{
	this->SetImageEx(std::move(value));
}

void Control::SetImageEx(std::shared_ptr<BitmapSource> value)
{
	if (value == this->_imageSource)
		return;
	this->_imageSource = std::move(value);
	this->_imageCache.Reset();
	this->_imageCacheTarget = nullptr;
	this->PostRender();
}

ID2D1Bitmap* Control::EnsureImageCache()
{
	if (!this->_imageSource || !this->ParentForm || !this->ParentForm->Render)
		return nullptr;
	auto* target = this->ParentForm->Render->GetRenderTargetRaw();
	if (!target)
		return nullptr;
	if (this->_imageCache && this->_imageCacheTarget == target)
		return this->_imageCache.Get();
	this->_imageCache.Reset();
	this->_imageCacheTarget = target;
	auto* bmp = this->ParentForm->Render->CreateBitmap(this->_imageSource);
	if (!bmp)
		return nullptr;
	this->_imageCache.Attach(bmp);
	return this->_imageCache.Get();
}
void Control::RenderImage()
{
	auto* bmp = this->EnsureImageCache();
	if (bmp)
	{
		auto size = bmp->GetSize();
		if (size.width > 0 && size.height > 0)
		{
			auto asize = this->ActualSize();
			switch (this->SizeMode)
			{
			case ImageSizeMode::Normal:
			{
				this->ParentForm->Render->DrawBitmap(bmp, 0.0f, 0.0f, size.width, size.height);
			}
			break;
			case ImageSizeMode::CenterImage:
			{
				this->ParentForm->Render->DrawBitmap(bmp, (asize.cx - size.width) / 2.0f, (asize.cy - size.height) / 2.0f, size.width, size.height);
			}
			break;
			case ImageSizeMode::StretchIamge:
			{
				this->ParentForm->Render->DrawBitmap(bmp, 0.0f, 0.0f, (float)asize.cx, (float)asize.cy);
			}
			break;
			case ImageSizeMode::Zoom:
			{
				float xp = asize.cx / size.width, yp = asize.cy / size.height;
				float tp = xp < yp ? xp : yp;
				float tw = size.width * tp, th = size.height * tp;
				float xf = (asize.cx - tw) / 2.0f, yf = (asize.cy - th) / 2.0f;
				this->ParentForm->Render->DrawBitmap(bmp, xf, yf, tw, th);
			}
			break;
			default:
				break;
			}
		}
	}
}
SIZE Control::ActualSize()
{
	return this->_size;
}

bool Control::IsSelected()
{
	return this->ParentForm && this->ParentForm->Selected == this;
}
bool Control::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof)
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
		MouseEventArgs event_obj = MouseEventArgs(MouseButtons::None, 0, xof, yof, HIWORD(wParam));
		if (this->ParentForm && this->DefaultTrackUnderMouse())
			this->ParentForm->UnderMouse = this;
		this->BeforeDefaultMouseMove(event_obj);
		this->OnMouseMove(this, event_obj);
	}
	break;
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_MBUTTONDOWN:
	{
		if (WM_LBUTTONDOWN == message && this->ParentForm && this->DefaultSelectOnLeftButtonDown())
		{
			this->ParentForm->SetSelectedControl(this, false);
		}
		MouseEventArgs event_obj = MouseEventArgs(FromParamToMouseButtons(message), 0, xof, yof, HIWORD(wParam));
		this->BeforeDefaultMouseDown(message, event_obj);
		this->OnMouseDown(this, event_obj);
		if (this->DefaultPostRenderOnMouseDown(message))
			this->PostRender();
	}
	break;
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
	case WM_MBUTTONUP:
	{
		bool wasSelected = this->ParentForm && this->ParentForm->Selected == this;
		MouseEventArgs event_obj = MouseEventArgs(FromParamToMouseButtons(message), 0, xof, yof, HIWORD(wParam));
		this->BeforeDefaultMouseUp(message, event_obj, wasSelected);
		if (WM_LBUTTONUP == message && wasSelected && this->DefaultRaiseClickOnLeftButtonUp())
		{
			this->BeforeDefaultClick(message, event_obj);
			this->OnMouseClick(this, event_obj);
		}
		if (wasSelected && this->DefaultClearSelectionOnMouseUp() && this->ParentForm && this->ParentForm->Selected == this)
		{
			this->ParentForm->SetSelectedControl(NULL, false);
		}
		this->OnMouseUp(this, event_obj);
		if (this->DefaultPostRenderOnMouseUp(message))
			this->PostRender();
	}
	break;
	case WM_LBUTTONDBLCLK:
	{
		bool wasSelected = this->ParentForm && this->ParentForm->Selected == this;
		if (this->ParentForm && this->DefaultSelectOnLeftButtonDoubleClick())
		{
			this->ParentForm->SetSelectedControl(this, false);
		}
		MouseEventArgs event_obj = MouseEventArgs(FromParamToMouseButtons(message), 0, xof, yof, HIWORD(wParam));
		this->BeforeDefaultMouseDoubleClick(message, event_obj, wasSelected);
		if (this->DefaultRaiseMouseDoubleClick(message, wasSelected))
			this->OnMouseDoubleClick(this, event_obj);
		if (this->DefaultPostRenderOnMouseDoubleClick(message, wasSelected))
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

// 布局属性实现
GET_CPP(Control, Thickness, Margin)
{
	return _margin;
}
SET_CPP(Control, Thickness, Margin)
{
	if (_margin != value)
	{
		_margin = value;
		this->RequestLayout();
		this->PostRender();
	}
}

GET_CPP(Control, Thickness, Padding)
{
	return _padding;
}
SET_CPP(Control, Thickness, Padding)
{
	if (_padding != value)
	{
		_padding = value;
		this->RequestLayout();
		this->PostRender();
	}
}

GET_CPP(Control, HorizontalAlignment, HAlign)
{
	return _horizontalAlignment;
}
SET_CPP(Control, HorizontalAlignment, HAlign)
{
	_horizontalAlignment = value;
	this->RequestLayout();
	this->PostRender();
}

GET_CPP(Control, VerticalAlignment, VAlign)
{
	return _verticalAlignment;
}
SET_CPP(Control, VerticalAlignment, VAlign)
{
	_verticalAlignment = value;
	this->RequestLayout();
	this->PostRender();
}

GET_CPP(Control, uint8_t, AnchorStyles)
{
	return _anchorStyles;
}
SET_CPP(Control, uint8_t, AnchorStyles)
{
	_anchorStyles = value;
	this->RequestLayout();
	this->PostRender();
}

GET_CPP(Control, int, GridRow)
{
	return _gridRow;
}
SET_CPP(Control, int, GridRow)
{
	_gridRow = value;
	this->RequestLayout();
	this->PostRender();
}

GET_CPP(Control, int, GridColumn)
{
	return _gridColumn;
}
SET_CPP(Control, int, GridColumn)
{
	_gridColumn = value;
	this->RequestLayout();
	this->PostRender();
}

GET_CPP(Control, int, GridRowSpan)
{
	return _gridRowSpan;
}
SET_CPP(Control, int, GridRowSpan)
{
	_gridRowSpan = value;
	this->RequestLayout();
	this->PostRender();
}

GET_CPP(Control, int, GridColumnSpan)
{
	return _gridColumnSpan;
}
SET_CPP(Control, int, GridColumnSpan)
{
	_gridColumnSpan = value;
	this->RequestLayout();
	this->PostRender();
}

GET_CPP(Control, Dock, DockPosition)
{
	return _dock;
}
SET_CPP(Control, Dock, DockPosition)
{
	_dock = value;
	this->RequestLayout();
	this->PostRender();
}

GET_CPP(Control, SIZE, MinSize)
{
	return _minSize;
}
SET_CPP(Control, SIZE, MinSize)
{
	_minSize = value;
	this->RequestLayout();
	this->PostRender();
}

GET_CPP(Control, SIZE, MaxSize)
{
	return _maxSize;
}
SET_CPP(Control, SIZE, MaxSize)
{
	_maxSize = value;
	this->RequestLayout();
	this->PostRender();
}

// 测量控件期望尺寸
SIZE Control::MeasureCore(SIZE availableSize)
{
	EnsureLayoutBase();
	SIZE desired = this->_layoutBaseSize;

	// 应用约束
	if (desired.cx < _minSize.cx) desired.cx = _minSize.cx;
	if (desired.cy < _minSize.cy) desired.cy = _minSize.cy;
	if (desired.cx > _maxSize.cx) desired.cx = _maxSize.cx;
	if (desired.cy > _maxSize.cy) desired.cy = _maxSize.cy;

	// 考虑可用空间
	if (desired.cx > availableSize.cx) desired.cx = availableSize.cx;
	if (desired.cy > availableSize.cy) desired.cy = availableSize.cy;

	return desired;
}

// 应用布局结果
void Control::ApplyLayout(POINT location, SIZE size)
{
	bool locationChanged = (_runtimeLocation.x != location.x || _runtimeLocation.y != location.y);
	bool sizeChanged = (_size.cx != size.cx || _size.cy != size.cy);

	if (locationChanged)
	{
		_runtimeLocation = location;
		this->OnMoved(this);
	}

	if (sizeChanged)
	{
		_size = size;
		this->OnSizeChanged(this);
	}

	if (locationChanged || sizeChanged)
	{
		this->PostRender();
	}
}

void Control::SetRuntimeLocation(POINT value)
{
	if (_runtimeLocation.x == value.x && _runtimeLocation.y == value.y)
	{
		return;
	}

	_runtimeLocation = value;
	this->OnMoved(this);
	this->PostRender();
}