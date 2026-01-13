#pragma once


/*---如果Utils和Graphics源代码包含在此项目中则直接引用本地项目---*/
//#define _LIB
#include <CppUtils/Utils/Event.h>
#include <CppUtils/Utils/Utils.h>
#include <CppUtils/Graphics/Colors.h>
#include <CppUtils/Graphics/Font.h>
#include <CppUtils/Graphics/Factory.h>
#include <CppUtils/Graphics/Graphics.h>

/*---如果Utils和Graphics被编译成lib则引用外部头文件---*/
// (using external CppUtils)


#include <string>
#include <vector>
#include <cstdint>
#include "Layout/LayoutTypes.h"

struct ID2D1Bitmap;

/**
 * @file Control.h
 * @brief CUI 控件基类及基础事件/枚举定义。
 *
 * 约定：
 * - UI 对象通常具有线程亲和性（应在创建它的 UI 线程访问/更新）。
 * - 资源所有权需显式：Font/Image 默认由控件“接管并释放”，也可通过 Set*Ex 关闭接管。
 * - 布局相关属性（Margin/Padding/Anchor/Grid/Dock/MinSize/MaxSize）由布局引擎与容器协同使用。
 */

inline Font* GetDefaultFontObject()
{
	static Font defaultFont(L"Arial", 14.0f);
	return &defaultFont;
}

/**
 * @brief 图片在控件区域内的绘制方式。
 */
enum class ImageSizeMode : char
{
	Normal,
	CenterImage,
	StretchIamge,
	Zoom
};

/**
 * @brief 运行时 UI 类型标识，用于 RTTI/序列化/设计器等场景。
 */
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
	UI_MediaPlayer,
	UI_StackPanel,
	UI_GridPanel,
	UI_DockPanel,
	UI_WrapPanel,
	UI_RelativePanel,
	UI_CUSTOM
};

/**
 * @brief 光标类型。
 */
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

/**
 * @brief 控件通用事件回调类型别名。
 *
 * sender 一般为触发事件的控件指针。
 */
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
typedef Event<void(class Control*, std::wstring)> DropTextEvent;
typedef Event<void(class Control*)> SelectionChangedEvent;

/**
 * @brief 所有可视控件的基类。
 *
 * 控件是轻量对象，主要职责：
 * - 保存几何（Location/Size）、颜色（Back/Fore/Border）、文本、资源指针（Font/Image）等属性
 * - 处理输入消息（ProcessMessage）并触发相应事件
 * - 参与布局：提供 MeasureCore/ApplyLayout，且通过 RequestLayout 通知父容器重排
 *
 * 所有权说明：
 * - Font：通过属性 Font 设置时默认由控件接管并在替换/析构时释放；可用 SetFontEx 指定不接管。
 * - Image：通过属性 Image 设置时默认由控件接管并在替换/析构时 Release；可用 SetImageEx 指定不接管。
 */
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

	// 默认 Anchor/Margin 布局使用的“基准”位置/尺寸（不随 ApplyLayout 写回而变化）
	// 用于保证重复 PerformLayout 幂等，避免位置/尺寸在多次布局时累计偏移。
	POINT _layoutBaseLocation = { 0,0 };
	SIZE _layoutBaseSize = { 120,20 };
	bool _layoutBaseInitialized = false;

	void EnsureLayoutBase()
	{
		if (_layoutBaseInitialized) return;
		_layoutBaseLocation = _location;
		_layoutBaseSize = _size;
		_layoutBaseInitialized = true;
	}

	void UpdateLayoutBaseLocation(POINT value)
	{
		_layoutBaseLocation = value;
		_layoutBaseInitialized = true;
	}

	void UpdateLayoutBaseSize(SIZE value)
	{
		_layoutBaseSize = value;
		_layoutBaseInitialized = true;
	}

	// 通知父容器（Panel 或 Form）需要重新布局
	void RequestLayout();

	friend class Panel;
	friend class Form;
public:
	/** @brief 勾选状态变化事件（CheckBox/RadioBox 等）。 */
	CheckedEvent OnChecked = CheckedEvent();
	/** @brief 鼠标滚轮事件。 */
	MouseWheelEvent OnMouseWheel = MouseWheelEvent();
	/** @brief 鼠标移动事件。 */
	MouseMoveEvent OnMouseMove = MouseMoveEvent();
	/** @brief 鼠标抬起事件。 */
	MouseUpEvent OnMouseUp = MouseUpEvent();
	/** @brief 鼠标按下事件。 */
	MouseDownEvent OnMouseDown = MouseDownEvent();
	/** @brief 鼠标双击事件。 */
	MouseDoubleClickEvent OnMouseDoubleClick = MouseDoubleClickEvent();
	/** @brief 鼠标单击事件。 */
	MouseClickEvent OnMouseClick = MouseClickEvent();
	/** @brief 鼠标进入控件事件。 */
	MouseEnterEvent OnMouseEnter = MouseEnterEvent();
	/** @brief 鼠标离开控件事件。 */
	MouseLeavedEvent OnMouseLeaved = MouseLeavedEvent();
	/** @brief 键盘抬起事件。 */
	KeyUpEvent OnKeyUp = KeyUpEvent();
	/** @brief 键盘按下事件。 */
	KeyDownEvent OnKeyDown = KeyDownEvent();
	/** @brief 绘制事件（通常由窗口的渲染循环触发）。 */
	PaintEvent OnPaint = PaintEvent();
	GridViewCheckStateChangedEvent OnGridViewCheckStateChanged = GridViewCheckStateChangedEvent();
	/** @brief 关闭事件（如按钮触发关闭请求）。 */
	CloseEvent OnClose = CloseEvent();
	/** @brief 位置移动事件。 */
	MovedEvent OnMoved = MovedEvent();
	/** @brief 尺寸变化事件。 */
	SizeChangedEvent OnSizeChanged = SizeChangedEvent();
	SelectedChangedEvent OnSelectedChanged = SelectedChangedEvent();
	ScrollChangedEvent OnScrollChanged = ScrollChangedEvent();
	/** @brief 文本变化事件。 */
	TextChangedEvent OnTextChanged = TextChangedEvent();
	/** @brief 字符输入事件（已解析为 wchar_t）。 */
	CharInputEvent OnCharInput = CharInputEvent();
	/** @brief 获得焦点事件。 */
	GotFocusEvent OnGotFocus = GotFocusEvent();
	/** @brief 失去焦点事件。 */
	LostFocusEvent OnLostFocus = LostFocusEvent();
	/** @brief 文件拖放事件。 */
	DropFileEvent OnDropFile = DropFileEvent();
	/** @brief 文本拖放事件。 */
	DropTextEvent OnDropText = DropTextEvent();
	class Form* ParentForm;
	class Control* Parent;
	/** @brief 文本是否发生变化（用于渲染或布局的脏标记）。 */
	bool TextChanged = true;
	bool Enable;
	bool Visible;
	bool Checked;
	/** @brief 用户自定义数据槽（不由框架解释）。 */
	UINT64 Tag;
	/** @brief 子控件集合。 */
	List<Control*> Children;
	/** @brief 图片绘制模式。 */
	ImageSizeMode SizeMode = ImageSizeMode::Zoom;
	/** @brief 创建基础控件。 */
	Control();
	/** @brief 虚析构：释放由控件接管的资源（Font/Image）等。 */
	virtual ~Control();
	/** @brief 返回运行时类型标识。 */
	virtual UIClass Type();
	/** @brief 更新控件状态（逻辑更新）。 */
	virtual void Update();
	/** @brief 渲染完成后的后处理（例如动画/失效区域上报）。 */
	virtual void PostRender();
	/**
	 * @brief 获取动画导致的额外无效区域。
	 * @param outRect 输出需要重绘的区域。
	 * @return true 表示 outRect 有效。
	 */
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
	/**
	 * @brief 从子控件列表移除一个控件。
	 * @param c 需要移除的控件。
	 */
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
	
	/**
	 * @brief 测量阶段：返回控件期望尺寸。
	 * @param availableSize 可用空间（由父布局提供）。
	 */
	virtual SIZE MeasureCore(SIZE availableSize);
	/**
	 * @brief 布局应用：由布局引擎/父容器设置最终位置与尺寸。
	 */
	void ApplyLayout(POINT location, SIZE size);

	CursorKind Cursor = CursorKind::Arrow;
	/**
	 * @brief 根据命中区域返回光标类型。
	 * @param xof 相对于控件客户区的 X。
	 * @param yof 相对于控件客户区的 Y。
	 */
	virtual CursorKind QueryCursor(int xof, int yof) { (void)xof; (void)yof; return this->Cursor; }
	virtual bool HitTestChildren() const { return true; }
	// 当底层 render target 因设备变化被重建时调用（例如 RDP 断开/重连）。
	// 默认实现会清空缓存的 Image，避免继续使用旧的 ID2D1Bitmap。
	virtual void OnRenderTargetRecreated();
	virtual void RenderImage();
	virtual SIZE ActualSize();
	void setTextPrivate(std::wstring);
	bool IsSelected();
	/**
	 * @brief 处理窗口消息并分发到控件。
	 * @return true 表示已处理。
	 */
	virtual bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof);
};