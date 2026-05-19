#include "Control.h"
#include "Binding.h"
#include "Form.h"
#include "Panel.h"
#include <algorithm>
#include <cmath>

#pragma warning(disable: 4267)
#pragma warning(disable: 4244)
#pragma warning(disable: 4018)

Control::Control()
	:
	Enable(true),
	Checked(false),
	ParentForm(nullptr),
	Parent(nullptr),
	Tag(0),
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
	this->_font = nullptr;
	this->_ownsFont = false;
	for (auto child : this->Children)
	{
		delete child;
	}
}
UIClass Control::Type() { return UIClass::UI_Base; }

void Control::SetTextInternal(std::wstring text)
{
	this->_text = std::move(text);
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
	auto actualSize = this->ActualSize();
	BeginRender((float)actualSize.cx, (float)actualSize.cy);
}
void Control::BeginRender(float clipW, float clipH)
{
	if (!this->ParentForm || !this->ParentForm->Render) return;
	auto absoluteLocation = this->AbsLocation;
	// HeadHeight is physical; divide by dpiScale to get logical units that match AbsLocation.
	const float dpiScale = this->ParentForm->GetDpiScale();
	const float titleBarOffset = (this->ParentForm->VisibleHead ? this->ParentForm->HeadHeight / dpiScale : 0.0f);
	this->ParentForm->Render->PushLocalTransform((float)absoluteLocation.x, (float)absoluteLocation.y + titleBarOffset, clipW, clipH);
}
void Control::EndRender()
{
	if (!this->ParentForm || !this->ParentForm->Render) return;
	this->ParentForm->Render->PopLocalTransform();
}

void Control::InvalidateVisual()
{
	if (!this->IsVisual || !this->ParentForm) return;
	const float titleBarOffset = (this->ParentForm->VisibleHead ? (float)this->ParentForm->HeadHeight : 0.0f);
	auto currentRect = this->AbsRect;
	currentRect.top += titleBarOffset;
	currentRect.bottom += titleBarOffset;

	if (_hasLastInvalidatedClientRect)
	{
		D2D1_RECT_F unionRect{};
		unionRect.left = (std::min)(_lastInvalidatedClientRect.left, currentRect.left);
		unionRect.top = (std::min)(_lastInvalidatedClientRect.top, currentRect.top);
		unionRect.right = (std::max)(_lastInvalidatedClientRect.right, currentRect.right);
		unionRect.bottom = (std::max)(_lastInvalidatedClientRect.bottom, currentRect.bottom);
		this->ParentForm->Invalidate(unionRect, false);
	}
	else
	{
		this->ParentForm->Invalidate(currentRect, false);
	}

	_lastInvalidatedClientRect = currentRect;
	_hasLastInvalidatedClientRect = true;
}

void Control::UpdateCaretBlinkState(bool focused, int selectionStart, int selectionEnd, bool caretRectValid, const D2D1_RECT_F* caretRect)
{
	bool shouldResetBlink = false;
	if (focused != _caretBlinkFocused)
		shouldResetBlink = focused;
	if (selectionStart != _caretBlinkSelectionStart || selectionEnd != _caretBlinkSelectionEnd)
		shouldResetBlink = true;
	if (caretRectValid != _caretBlinkRectValid)
		shouldResetBlink = true;
	if (caretRectValid && caretRect)
	{
		if (!_caretBlinkRectValid ||
			std::fabs(_caretBlinkRect.left - caretRect->left) > 0.1f ||
			std::fabs(_caretBlinkRect.top - caretRect->top) > 0.1f ||
			std::fabs(_caretBlinkRect.right - caretRect->right) > 0.1f ||
			std::fabs(_caretBlinkRect.bottom - caretRect->bottom) > 0.1f)
		{
			shouldResetBlink = true;
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

	if (shouldResetBlink || _caretBlinkResetTick == 0)
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
	this->InvalidateVisual();
}

GET_CPP(Control, BindingCollection&, DataBindings)
{
	if (!this->_dataBindings)
		this->_dataBindings = std::make_unique<BindingCollection>(this);
	return *this->_dataBindings;
}

GET_CPP(Control, int, Count)
{
	return this->Children.size();
}
Control* Control::operator[](int index)
{
	return this->Children[index];
}
Control* Control::GetChild(int index)
{
	if (this->Children.size() <= index)
		return nullptr;
	return this->Children[index];
}

std::vector<Control*> Control::GetChildrenInZOrder() const
{
	std::vector<Control*> result = this->Children;
	std::stable_sort(result.begin(), result.end(), [](Control* left, Control* right)
		{
			if (!left || !right) return left != nullptr;
			return left->ZIndex < right->ZIndex;
		});
	return result;
}

std::vector<Control*> Control::GetChildrenInReverseZOrder() const
{
	auto result = GetChildrenInZOrder();
	std::reverse(result.begin(), result.end());
	return result;
}
void Control::RemoveControl(Control* child)
{
	this->Children.erase(
		std::remove(this->Children.begin(), this->Children.end(), child),
		this->Children.end());
	child->Parent = nullptr;
	child->ParentForm = nullptr;
	if (!this->ParentForm) return;
	if (this->ParentForm->ForegroundControl == child)
		this->ParentForm->ForegroundControl = nullptr;
	if (this->ParentForm->MainMenu == child)
		this->ParentForm->MainMenu = nullptr;
	if (this->ParentForm->MainToolBar == child)
		this->ParentForm->MainToolBar = nullptr;
	if (this->ParentForm->MainStatusBar == child)
		this->ParentForm->MainStatusBar = nullptr;
	if (this->ParentForm->UnderMouse == child)
		this->ParentForm->UnderMouse = nullptr;
}
GET_CPP(Control, POINT, AbsLocation)
{
	Control* ancestor = this;
	POINT absoluteLocation = ancestor->ActualLocation;
	while (ancestor->Parent)
	{
		ancestor = ancestor->Parent;
		auto parentLocation = ancestor->ActualLocation;
		auto childOffset = ancestor->GetChildrenRenderOffset();
		absoluteLocation.x += parentLocation.x;
		absoluteLocation.y += parentLocation.y;
		absoluteLocation.x += childOffset.x;
		absoluteLocation.y += childOffset.y;
	}
	return absoluteLocation;
}
GET_CPP(Control, POINT, ActualLocation)
{
	return _runtimeLocation;
}
GET_CPP(Control, D2D1_RECT_F, AbsRect)
{
	auto absoluteLocation = this->AbsLocation;
	auto actualSize = this->ActualSize();
	return D2D1_RECT_F{
		(float)absoluteLocation.x,
		(float)absoluteLocation.y,
		(float)absoluteLocation.x + (float)actualSize.cx,
		(float)absoluteLocation.y + (float)actualSize.cy
	};
}
GET_CPP(Control, bool, IsVisual)
{
	if (!this->_visible) return false;
	Control* ancestor = this;
	while (ancestor->Parent)
	{
		ancestor = ancestor->Parent;
		if (!ancestor->Visible) return false;
	}
	return true;
}
GET_CPP(Control, bool, Visible)
{
	return this->_visible;
}
SET_CPP(Control, bool, Visible)
{
	if (this->_visible == value)
		return;

	this->_visible = value;
	this->RequestLayout();

	if (this->ParentForm)
	{
		this->ParentForm->Invalidate(false);
	}
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
	this->InvalidateVisual();
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
	this->InvalidateVisual();
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
	this->InvalidateVisual();
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
	this->InvalidateVisual();
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
		this->InvalidateVisual();
	}
	_text = value;
}
GET_CPP(Control, D2D1_COLOR_F, BorderColor)
{
	return _bordercolor;
}
SET_CPP(Control, D2D1_COLOR_F, BorderColor)
{
	_bordercolor = value;
	this->InvalidateVisual();
}
GET_CPP(Control, D2D1_COLOR_F, BackColor)
{
	return _backcolor;
}
SET_CPP(Control, D2D1_COLOR_F, BackColor)
{
	_backcolor = value;
	this->InvalidateVisual();
}
GET_CPP(Control, D2D1_COLOR_F, ForeColor)
{
	return _forecolor;
}
SET_CPP(Control, D2D1_COLOR_F, ForeColor)
{
	_forecolor = value;
	this->InvalidateVisual();
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
	this->InvalidateVisual();
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
void Control::RenderImage(float cornerRadius)
{
	auto* bitmap = this->EnsureImageCache();
	if (bitmap)
	{
		auto imageSize = bitmap->GetSize();
		if (imageSize.width > 0 && imageSize.height > 0)
		{
			auto actualSize = this->ActualSize();
			const float clipRadius = (std::clamp)(cornerRadius, 0.0f, (std::min)((float)actualSize.cx, (float)actualSize.cy) * 0.5f);
			const bool clipPushed = clipRadius > 0.0f && this->ParentForm && this->ParentForm->Render &&
				this->ParentForm->Render->PushRoundClip(0.0f, 0.0f, (float)actualSize.cx, (float)actualSize.cy, clipRadius);
			switch (this->SizeMode)
			{
			case ImageSizeMode::Normal:
			{
				this->ParentForm->Render->DrawBitmap(bitmap, 0.0f, 0.0f, imageSize.width, imageSize.height);
			}
			break;
			case ImageSizeMode::CenterImage:
			{
				this->ParentForm->Render->DrawBitmap(bitmap, (actualSize.cx - imageSize.width) / 2.0f, (actualSize.cy - imageSize.height) / 2.0f, imageSize.width, imageSize.height);
			}
			break;
			case ImageSizeMode::StretchImage:
			{
				this->ParentForm->Render->DrawBitmap(bitmap, 0.0f, 0.0f, (float)actualSize.cx, (float)actualSize.cy);
			}
			break;
			case ImageSizeMode::Zoom:
			{
				float scaleX = actualSize.cx / imageSize.width;
				float scaleY = actualSize.cy / imageSize.height;
				float scale = scaleX < scaleY ? scaleX : scaleY;
				float renderWidth = imageSize.width * scale;
				float renderHeight = imageSize.height * scale;
				float renderX = (actualSize.cx - renderWidth) / 2.0f;
				float renderY = (actualSize.cy - renderHeight) / 2.0f;
				this->ParentForm->Render->DrawBitmap(bitmap, renderX, renderY, renderWidth, renderHeight);
			}
			break;
			default:
				break;
			}
			if (clipPushed)
				this->ParentForm->Render->PopRoundClip();
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
bool Control::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY)
{
	if (!this->Enable || !this->Visible) return true;
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
		}
	}
	break;
	case WM_MOUSEWHEEL:
	{
		MouseEventArgs eventArgs = MouseEventArgs(MouseButtons::None, 0, localX, localY, GET_WHEEL_DELTA_WPARAM(wParam));
		this->OnMouseWheel(this, eventArgs);
	}
	break;
	case WM_MOUSEMOVE:
	{
		MouseEventArgs eventArgs = MouseEventArgs(MouseButtons::None, 0, localX, localY, HIWORD(wParam));
		if (this->ParentForm && this->DefaultTrackUnderMouse())
			this->ParentForm->UnderMouse = this;
		this->BeforeDefaultMouseMove(eventArgs);
		this->OnMouseMove(this, eventArgs);
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
		MouseEventArgs eventArgs = MouseEventArgs(FromParamToMouseButtons(message), 0, localX, localY, HIWORD(wParam));
		this->BeforeDefaultMouseDown(message, eventArgs);
		this->OnMouseDown(this, eventArgs);
		if (this->DefaultInvalidateVisualOnMouseDown(message))
			this->InvalidateVisual();
	}
	break;
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
	case WM_MBUTTONUP:
	{
		bool wasSelected = this->ParentForm && this->ParentForm->Selected == this;
		MouseEventArgs eventArgs = MouseEventArgs(FromParamToMouseButtons(message), 0, localX, localY, HIWORD(wParam));
		this->BeforeDefaultMouseUp(message, eventArgs, wasSelected);
		if (WM_LBUTTONUP == message && wasSelected && this->DefaultRaiseClickOnLeftButtonUp())
		{
			this->BeforeDefaultClick(message, eventArgs);
			this->OnMouseClick(this, eventArgs);
		}
		if (wasSelected && this->DefaultClearSelectionOnMouseUp() && this->ParentForm && this->ParentForm->Selected == this)
		{
			this->ParentForm->SetSelectedControl(nullptr, false);
		}
		this->OnMouseUp(this, eventArgs);
		if (this->DefaultInvalidateVisualOnMouseUp(message))
			this->InvalidateVisual();
	}
	break;
	case WM_LBUTTONDBLCLK:
	{
		bool wasSelected = this->ParentForm && this->ParentForm->Selected == this;
		if (this->ParentForm && this->DefaultSelectOnLeftButtonDoubleClick())
		{
			this->ParentForm->SetSelectedControl(this, false);
		}
		MouseEventArgs eventArgs = MouseEventArgs(FromParamToMouseButtons(message), 0, localX, localY, HIWORD(wParam));
		this->BeforeDefaultMouseDoubleClick(message, eventArgs, wasSelected);
		if (this->DefaultRaiseMouseDoubleClick(message, wasSelected))
			this->OnMouseDoubleClick(this, eventArgs);
		if (this->DefaultInvalidateVisualOnMouseDoubleClick(message, wasSelected))
			this->InvalidateVisual();
	}
	break;
	case WM_KEYDOWN:
	{
		KeyEventArgs eventArgs = KeyEventArgs((Keys)(wParam | 0));
		this->OnKeyDown(this, eventArgs);
	}
	break;
	case WM_KEYUP:
	{
		KeyEventArgs eventArgs = KeyEventArgs((Keys)(wParam | 0));
		this->OnKeyUp(this, eventArgs);
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
		this->InvalidateVisual();
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
		this->InvalidateVisual();
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
	this->InvalidateVisual();
}

GET_CPP(Control, VerticalAlignment, VAlign)
{
	return _verticalAlignment;
}
SET_CPP(Control, VerticalAlignment, VAlign)
{
	_verticalAlignment = value;
	this->RequestLayout();
	this->InvalidateVisual();
}

GET_CPP(Control, uint8_t, AnchorStyles)
{
	return _anchorStyles;
}
SET_CPP(Control, uint8_t, AnchorStyles)
{
	_anchorStyles = value;
	this->RequestLayout();
	this->InvalidateVisual();
}

GET_CPP(Control, int, GridRow)
{
	return _gridRow;
}
SET_CPP(Control, int, GridRow)
{
	_gridRow = value;
	this->RequestLayout();
	this->InvalidateVisual();
}

GET_CPP(Control, int, GridColumn)
{
	return _gridColumn;
}
SET_CPP(Control, int, GridColumn)
{
	_gridColumn = value;
	this->RequestLayout();
	this->InvalidateVisual();
}

GET_CPP(Control, int, GridRowSpan)
{
	return _gridRowSpan;
}
SET_CPP(Control, int, GridRowSpan)
{
	_gridRowSpan = value;
	this->RequestLayout();
	this->InvalidateVisual();
}

GET_CPP(Control, int, GridColumnSpan)
{
	return _gridColumnSpan;
}
SET_CPP(Control, int, GridColumnSpan)
{
	_gridColumnSpan = value;
	this->RequestLayout();
	this->InvalidateVisual();
}

GET_CPP(Control, Dock, DockPosition)
{
	return _dock;
}
SET_CPP(Control, Dock, DockPosition)
{
	_dock = value;
	this->RequestLayout();
	this->InvalidateVisual();
}

GET_CPP(Control, SIZE, MinSize)
{
	return _minSize;
}
SET_CPP(Control, SIZE, MinSize)
{
	_minSize = value;
	this->RequestLayout();
	this->InvalidateVisual();
}

GET_CPP(Control, SIZE, MaxSize)
{
	return _maxSize;
}
SET_CPP(Control, SIZE, MaxSize)
{
	_maxSize = value;
	this->RequestLayout();
	this->InvalidateVisual();
}

// 测量控件期望尺寸
SIZE Control::MeasureCore(SIZE availableSize)
{
	(void)availableSize;
	EnsureLayoutBase();
	SIZE desiredSize = this->_layoutBaseSize;

	// 应用约束
	if (desiredSize.cx < _minSize.cx) desiredSize.cx = _minSize.cx;
	if (desiredSize.cy < _minSize.cy) desiredSize.cy = _minSize.cy;
	if (desiredSize.cx > _maxSize.cx) desiredSize.cx = _maxSize.cx;
	if (desiredSize.cy > _maxSize.cy) desiredSize.cy = _maxSize.cy;

	return desiredSize;
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
		this->InvalidateVisual();
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
	this->InvalidateVisual();
}
