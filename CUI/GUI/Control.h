#pragma once


/*---如果Utils和Graphics源代码包含在此项目中则直接引用本地项目---*/
//#define _LIB
#include <CppUtils/Utils/Event.h>
#include <CppUtils/Utils/Utils.h>
#include <CppUtils/Graphics/Colors.h>
#include <CppUtils/Graphics/Font.h>
#include <CppUtils/Graphics/Factory.h>
#include <CppUtils/Graphics/Graphics1.h>

/*---如果Utils和Graphics被编译成lib则引用外部头文件---*/
// (using external CppUtils)


#include <string>
#include <vector>
#include <cstdint>
#include "Layout/LayoutTypes.h"

struct ID2D1Bitmap;

inline Font* GetDefaultFontObject()
{
	static Font defaultFont(L"Arial", 18.0f);
	return &defaultFont;
}

enum class ImageSizeMode : char
{
	Normal,
	CenterImage,
	StretchIamge,
	Zoom
};
enum class UIClass : int
{
	UI_Base,
	UI_Label,
	UI_Button,
	UI_PictureBox,
	UI_TextBox,
	UI_RichTextBox,
	UI_PasswordBox,
	UI_ComboBox,
	UI_GridView,
	UI_CheckBox,
	UI_RadioBox,
	UI_ProgressBar,
	UI_TreeView,
	UI_Panel,
	UI_TabPage,
	UI_TabControl,
	UI_Switch,
	UI_Menu,
	UI_MenuItem,
	UI_ToolBar,
	UI_StatusBar,
	UI_Slider,
	UI_WebBrowser,
	UI_StackPanel,
	UI_GridPanel,
	UI_DockPanel,
	UI_WrapPanel,
	UI_RelativePanel,
	UI_CUSTOM
};

enum class CursorKind : uint8_t
{
	Arrow,
	Hand,
	IBeam,
	SizeWE,
	SizeNS,
	SizeNWSE,
	SizeNESW,
	SizeAll,
	No
};

typedef Event<void(class Control*, EventArgs)> EventHandler;
typedef Event<void(class Control*)> CheckedEvent;
typedef Event<void(class Control*, MouseEventArgs)> MouseWheelEvent;
typedef Event<void(class Control*, MouseEventArgs)> MouseMoveEvent;
typedef Event<void(class Control*, MouseEventArgs)> MouseUpEvent;
typedef Event<void(class Control*, MouseEventArgs)> MouseDownEvent;
typedef Event<void(class Control*, MouseEventArgs)> MouseDoubleClickEvent;
typedef Event<void(class Control*, MouseEventArgs)> MouseClickEvent;
typedef Event<void(class Control*, MouseEventArgs)> MouseEnterEvent;
typedef Event<void(class Control*, MouseEventArgs)> MouseLeavedEvent;
typedef Event<void(class Control*, KeyEventArgs)> KeyUpEvent;
typedef Event<void(class Control*, KeyEventArgs)> KeyDownEvent;
typedef Event<void(class Control*)> PaintEvent;
typedef Event<void(class Control*, int column, int row, bool value)> GridViewCheckStateChangedEvent;
typedef Event<void(class Control*)> CloseEvent;
typedef Event<void(class Control*)> MovedEvent;
typedef Event<void(class Control*)> SizeChangedEvent;
typedef Event<void(class Control*)> SelectedChangedEvent;
typedef Event<void(class Control*)> ScrollChangedEvent;
typedef Event<void(class Control*, std::wstring, std::wstring)> TextChangedEvent;
typedef Event<void(class Control*, wchar_t)> CharInputEvent;
typedef Event<void(class Control*)> GotFocusEvent;
typedef Event<void(class Control*)> LostFocusEvent;
typedef Event<void(class Control*, List<std::wstring>)> DropFileEvent;
typedef Event<void(class Control*)> SelectionChangedEvent;

class Control
{
protected:
	POINT _location = { 0,0 };
	SIZE _size = { 120,20 };
	D2D1_COLOR_F _backcolor = Colors::gray91;
	D2D1_COLOR_F _forecolor = Colors::Black;
	D2D1_COLOR_F _boldercolor = Colors::Black;
	ID2D1Bitmap* _image = NULL;
	bool _ownsImage = false;
	std::wstring _text;
	Font* _font = NULL;
	bool _ownsFont = false;
	// 上一次 PostRender 触发的 client 坐标系矩形（包含标题栏 top 偏移）
	// 用于处理控件尺寸变小（如 Menu/ComboBox 收起）时，旧绘制区域未被失效导致的残影。
	D2D1_RECT_F _lastPostRenderClientRect{ 0,0,0,0 };
	bool _hasLastPostRenderClientRect = false;
	
	// 布局属性
	Thickness _margin;
	Thickness _padding;
	HorizontalAlignment _horizontalAlignment = HorizontalAlignment::Left;
	VerticalAlignment _verticalAlignment = VerticalAlignment::Top;
	uint8_t _anchorStyles = AnchorStyles::None;
	
	// Grid 布局专用属性
	int _gridRow = 0;
	int _gridColumn = 0;
	int _gridRowSpan = 1;
	int _gridColumnSpan = 1;
	
	// Dock 布局专用属性
	Dock _dock = Dock::Fill;
	
	// 尺寸约束
	SIZE _minSize = {0, 0};
	SIZE _maxSize = {INT_MAX, INT_MAX};
public:
	CheckedEvent OnChecked = CheckedEvent();
	MouseWheelEvent OnMouseWheel = MouseWheelEvent();
	MouseMoveEvent OnMouseMove = MouseMoveEvent();
	MouseUpEvent OnMouseUp = MouseUpEvent();
	MouseDownEvent OnMouseDown = MouseDownEvent();
	MouseDoubleClickEvent OnMouseDoubleClick = MouseDoubleClickEvent();
	MouseClickEvent OnMouseClick = MouseClickEvent();
	MouseEnterEvent OnMouseEnter = MouseEnterEvent();
	MouseLeavedEvent OnMouseLeaved = MouseLeavedEvent();
	KeyUpEvent OnKeyUp = KeyUpEvent();
	KeyDownEvent OnKeyDown = KeyDownEvent();
	PaintEvent OnPaint = PaintEvent();
	GridViewCheckStateChangedEvent OnGridViewCheckStateChanged = GridViewCheckStateChangedEvent();
	CloseEvent OnClose = CloseEvent();
	MovedEvent OnMoved = MovedEvent();
	SizeChangedEvent OnSizeChanged = SizeChangedEvent();
	SelectedChangedEvent OnSelectedChanged = SelectedChangedEvent();
	ScrollChangedEvent OnScrollChanged = ScrollChangedEvent();
	TextChangedEvent OnTextChanged = TextChangedEvent();
	CharInputEvent OnCharInput = CharInputEvent();
	GotFocusEvent OnGotFocus = GotFocusEvent();
	LostFocusEvent OnLostFocus = LostFocusEvent();
	DropFileEvent OnDropFile = DropFileEvent();
	class Form* ParentForm;
	class Control* Parent;
	bool TextChanged = true;
	bool Enable;
	bool Visible;
	bool Checked;
	UINT64 Tag;
	List<Control*> Children;
	ImageSizeMode SizeMode = ImageSizeMode::Zoom;
	Control();
	virtual ~Control();
	virtual UIClass Type();
	virtual void Update();
	virtual void PostRender();
	virtual bool GetAnimatedInvalidRect(D2D1_RECT_F& outRect) { (void)outRect; return false; }
	PROPERTY(class Font*, Font);
	GET(class Font*, Font);
	SET(class Font*, Font);
	// 显式设置是否由 Control 释放 Font（默认：通过属性 Font 设置时视为“拥有”）
	void SetFontEx(class Font* value, bool takeOwnership);
	READONLY_PROPERTY(int, Count);
	GET(int, Count);
	Control* operator[](int index);
	Control* get(int index);

	template<typename T>
	T AddControl(T c) {
		if (c->Parent) {
			throw std::exception("该控件已属于其他容器!");
			return NULL;
		}
		if (this->Children.Contains(c)) {
			return c;
		}
		c->Parent = this;
		c->ParentForm = this->ParentForm;
		this->Children.Add(c);
		
		// 递归设置所有子控件的ParentForm
		SetChildrenParentForm(c, this->ParentForm);
		return c;
	}
	
	// 递归设置所有子控件的ParentForm
	static void SetChildrenParentForm(Control* c, Form* form) {
		if (!c) return;
		c->ParentForm = form;
		for (int i = 0; i < c->Children.Count; i++) {
			SetChildrenParentForm(c->Children[i], form);
		}
	}
	void RemoveControl(Control* c);
	READONLY_PROPERTY(POINT, AbsLocation);
	GET(POINT, AbsLocation);
	READONLY_PROPERTY(D2D1_RECT_F, AbsRect);
	GET(D2D1_RECT_F, AbsRect);
	READONLY_PROPERTY(bool, IsVisual);
	GET(bool, IsVisual);
	PROPERTY(POINT, Location);
	GET(POINT, Location);
	SET(POINT, Location);
	PROPERTY(SIZE, Size);
	GET(SIZE, Size);
	SET(SIZE, Size);
	PROPERTY(int, Left);
	GET(int, Left);
	SET(int, Left);
	PROPERTY(int, Top);
	GET(int, Top);
	SET(int, Top);
	PROPERTY(int, Width);
	GET(int, Width);
	SET(int, Width);
	PROPERTY(int, Height);
	GET(int, Height);
	SET(int, Height);
	READONLY_PROPERTY(float, Right);
	GET(float, Right);
	READONLY_PROPERTY(float, Bottom);
	GET(float, Bottom);
	PROPERTY(std::wstring, Text);
	GET(std::wstring, Text);
	SET(std::wstring, Text);
	PROPERTY(D2D1_COLOR_F, BolderColor);
	GET(D2D1_COLOR_F, BolderColor);
	SET(D2D1_COLOR_F, BolderColor);
	PROPERTY(D2D1_COLOR_F, BackColor);
	GET(D2D1_COLOR_F, BackColor);
	SET(D2D1_COLOR_F, BackColor);
	PROPERTY(D2D1_COLOR_F, ForeColor);
	GET(D2D1_COLOR_F, ForeColor);
	SET(D2D1_COLOR_F, ForeColor);
	PROPERTY(ID2D1Bitmap*, Image);
	GET(ID2D1Bitmap*, Image);
	SET(ID2D1Bitmap*, Image);
	// 显式设置 Image 所有权（默认：通过属性 Image 设置时视为“拥有”并在析构/替换时 Release）
	void SetImageEx(ID2D1Bitmap* value, bool takeOwnership);

	// 布局属性访问器
	PROPERTY(Thickness, Margin);
	GET(Thickness, Margin);
	SET(Thickness, Margin);
	PROPERTY(Thickness, Padding);
	GET(Thickness, Padding);
	SET(Thickness, Padding);
	PROPERTY(::HorizontalAlignment, HAlign);
	GET(::HorizontalAlignment, HAlign);
	SET(::HorizontalAlignment, HAlign);
	PROPERTY(::VerticalAlignment, VAlign);
	GET(::VerticalAlignment, VAlign);
	SET(::VerticalAlignment, VAlign);
	PROPERTY(uint8_t, AnchorStyles);
	GET(uint8_t, AnchorStyles);
	SET(uint8_t, AnchorStyles);
	PROPERTY(int, GridRow);
	GET(int, GridRow);
	SET(int, GridRow);
	PROPERTY(int, GridColumn);
	GET(int, GridColumn);
	SET(int, GridColumn);
	PROPERTY(int, GridRowSpan);
	GET(int, GridRowSpan);
	SET(int, GridRowSpan);
	PROPERTY(int, GridColumnSpan);
	GET(int, GridColumnSpan);
	SET(int, GridColumnSpan);
	PROPERTY(::Dock, DockPosition);
	GET(::Dock, DockPosition);
	SET(::Dock, DockPosition);
	PROPERTY(SIZE, MinSize);
	GET(SIZE, MinSize);
	SET(SIZE, MinSize);
	PROPERTY(SIZE, MaxSize);
	GET(SIZE, MaxSize);
	SET(SIZE, MaxSize);
	
	// 布局相关方法
	virtual SIZE MeasureCore(SIZE availableSize);
	void ApplyLayout(POINT location, SIZE size);

	CursorKind Cursor = CursorKind::Arrow;
	virtual CursorKind QueryCursor(int xof, int yof) { (void)xof; (void)yof; return this->Cursor; }
	// Whether Form-level hit-testing should descend into children.
	// Default: true (normal containers). DesignerCanvas overrides this to false
	// so that clicks on child controls are handled by the canvas for selection/drag/resize.
	virtual bool HitTestChildren() const { return true; }
	virtual void RenderImage();
	virtual SIZE ActualSize();
	void setTextPrivate(std::wstring);
	bool IsSelected();
	virtual bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof);
};