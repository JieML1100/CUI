#include "Control.h"
#include "Form.h"

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
	_image(NULL),
	_text(L"")
{
}
Control::~Control()
{
	if (this->_image && this->_ownsImage)
	{
		this->_image->Release();
	}
	this->_image = NULL;
	this->_ownsImage = false;
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

GET_CPP(Control, class Font*, Font)
{
	if (this->_font)
		return this->_font;
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
	if (this->ParentForm->ForegroundControl == c)
		this->ParentForm->ForegroundControl = NULL;
	if (this->ParentForm->MainMenu == c)
		this->ParentForm->MainMenu = NULL;
	if (this->ParentForm->MainStatusBar == c)
		this->ParentForm->MainStatusBar = NULL;
	if (this->ParentForm->UnderMouse == c)
		this->ParentForm->UnderMouse = NULL;
}
GET_CPP(Control, POINT, AbsLocation)
{
	Control* tmpc = this;
	POINT tmpl = tmpc->_location;
	while (tmpc->Parent)
	{
		tmpc = tmpc->Parent;
		auto loc = tmpc->_location;
		tmpl.x += loc.x;
		tmpl.y += loc.y;
	}
	return tmpl;
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
	this->OnMoved(this);
	_location = value;
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
	this->PostRender();
}
GET_CPP(Control, int, Left)
{
	return this->Location.x;
}
SET_CPP(Control, int, Left)
{
	this->_location = POINT{ value,this->_location.y };
	this->PostRender();
}
GET_CPP(Control, int, Top)
{
	return this->Location.y;
}
SET_CPP(Control, int, Top)
{
	this->_location = POINT{ this->_location.x,value };
	this->PostRender();
}
GET_CPP(Control, int, Width)
{
	return this->_size.cx;
}
SET_CPP(Control, int, Width)
{
	this->OnSizeChanged(this);
	this->_size.cx = value;
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
GET_CPP(Control, ID2D1Bitmap*, Image)
{
	return _image;
}
SET_CPP(Control, ID2D1Bitmap*, Image)
{
	this->SetImageEx(value, true);
}

void Control::SetImageEx(ID2D1Bitmap* value, bool takeOwnership)
{
	if (value == this->_image)
	{
		this->_ownsImage = takeOwnership;
		return;
	}
	if (this->_image && this->_ownsImage)
	{
		this->_image->Release();
	}
	this->_image = value;
	this->_ownsImage = takeOwnership;
	this->PostRender();
}
void Control::RenderImage()
{
	if (this->_image)
	{
		auto absLocation = this->AbsLocation;
		auto size = this->_image->GetSize();
		if (size.width > 0 && size.height > 0)
		{
			auto asize = this->ActualSize();
			switch (this->SizeMode)
			{
			case ImageSizeMode::Normal:
			{
				this->ParentForm->Render->DrawBitmap(this->_image, (float)absLocation.x, (float)absLocation.y, size.width, size.height);
			}
			break;
			case ImageSizeMode::CenterImage:
			{
				this->ParentForm->Render->DrawBitmap(this->_image, absLocation.x + ((asize.cx - size.width) / 2.0f), absLocation.y + ((asize.cy - size.height) / 2.0f), size.width, size.height);
			}
			break;
			case ImageSizeMode::StretchIamge:
			{
				this->ParentForm->Render->DrawBitmap(this->_image, (float)absLocation.x, (float)absLocation.y, (float)asize.cx, (float)asize.cy);
			}
			break;
			case ImageSizeMode::Zoom:
			{
				float xp = asize.cx / size.width, yp = asize.cy / size.height;
				float tp = xp < yp ? xp : yp;
				float tw = size.width * tp, th = size.height * tp;
				float xf = (asize.cx - tw) / 2.0f, yf = (asize.cy - th) / 2.0f;
				this->ParentForm->Render->DrawBitmap(this->_image, absLocation.x + xf, absLocation.y + yf, tw, th);
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
	return this->Size;
}

bool Control::IsSelected()
{
	return this->ParentForm && this->ParentForm->Selected == this;
}
bool Control::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof)
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
			this->ParentForm->Selected = this;
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
		MouseEventArgs event_obj = MouseEventArgs(FromParamToMouseButtons(message), 0, xof, yof, HIWORD(wParam));
		this->OnMouseUp(this, event_obj);
		this->PostRender();
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
	this->PostRender();
}

GET_CPP(Control, VerticalAlignment, VAlign)
{
	return _verticalAlignment;
}
SET_CPP(Control, VerticalAlignment, VAlign)
{
	_verticalAlignment = value;
	this->PostRender();
}

GET_CPP(Control, uint8_t, AnchorStyles)
{
	return _anchorStyles;
}
SET_CPP(Control, uint8_t, AnchorStyles)
{
	_anchorStyles = value;
	this->PostRender();
}

GET_CPP(Control, int, GridRow)
{
	return _gridRow;
}
SET_CPP(Control, int, GridRow)
{
	_gridRow = value;
	this->PostRender();
}

GET_CPP(Control, int, GridColumn)
{
	return _gridColumn;
}
SET_CPP(Control, int, GridColumn)
{
	_gridColumn = value;
	this->PostRender();
}

GET_CPP(Control, int, GridRowSpan)
{
	return _gridRowSpan;
}
SET_CPP(Control, int, GridRowSpan)
{
	_gridRowSpan = value;
	this->PostRender();
}

GET_CPP(Control, int, GridColumnSpan)
{
	return _gridColumnSpan;
}
SET_CPP(Control, int, GridColumnSpan)
{
	_gridColumnSpan = value;
	this->PostRender();
}

GET_CPP(Control, Dock, DockPosition)
{
	return _dock;
}
SET_CPP(Control, Dock, DockPosition)
{
	_dock = value;
	this->PostRender();
}

GET_CPP(Control, SIZE, MinSize)
{
	return _minSize;
}
SET_CPP(Control, SIZE, MinSize)
{
	_minSize = value;
	this->PostRender();
}

GET_CPP(Control, SIZE, MaxSize)
{
	return _maxSize;
}
SET_CPP(Control, SIZE, MaxSize)
{
	_maxSize = value;
	this->PostRender();
}

// 测量控件期望尺寸
SIZE Control::MeasureCore(SIZE availableSize)
{
	SIZE desired = this->_size;

	// 应用 Padding
	desired.cx += (LONG)(_padding.Left + _padding.Right);
	desired.cy += (LONG)(_padding.Top + _padding.Bottom);

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
	bool changed = false;

	if (_location.x != location.x || _location.y != location.y)
	{
		_location = location;
		changed = true;
	}

	if (_size.cx != size.cx || _size.cy != size.cy)
	{
		_size = size;
		changed = true;
	}

	if (changed)
	{
		this->PostRender();
	}
}