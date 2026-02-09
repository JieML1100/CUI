#pragma once

/**
 * @file Form.h
 * @brief Form：顶层窗口与消息分发（Legacy）。
 */
#include "Control.h"
#include "Application.h"
#include "Button.h"
#include "CheckBox.h"
#include "ComboBox.h"
#include "DateTimePicker.h"
#include "GridView.h"
#include "Label.h"
#include "LinkLabel.h"
#include "Panel.h"
#include "PasswordBox.h"
#include "PictureBox.h"
#include "ProgressBar.h"
#include "RadioBox.h"
#include "RichTextBox.h"
#include "RoundTextBox.h"
#include "Switch.h"
#include "Menu.h"
#include "ToolBar.h"
#include "StatusBar.h"
#include "Slider.h"
#include "TabControl.h"
#include "TextBox.h"
#include "TreeView.h"
#include "Taskbar.h"
#include "NotifyIcon.h"
#include "MediaPlayer.h"

typedef Event<void(class Form* sender, int Id, int info)> CommandEvent;
typedef Event<void(class Form*)> FormClosingEvent;
typedef Event<void(class Form*)> FormClosedEvent;
typedef Event<void(class Form*, wchar_t)> FormCharInputEvent;
typedef Event<void(class Form*)> FormPaintEvent;
typedef Event<void(class Form*, MouseEventArgs)> FormMouseWheelEvent;
typedef Event<void(class Form*, MouseEventArgs)> FormMouseMoveEvent;
typedef Event<void(class Form*, MouseEventArgs)> FormMouseUpEvent;
typedef Event<void(class Form*, MouseEventArgs)> FormMouseDownEvent;
typedef Event<void(class Form*, KeyEventArgs)> FormKeyUpEvent;
typedef Event<void(class Form*, KeyEventArgs)> FormKeyDownEvent;
typedef Event<void(class Form*)> FormMovedEvent;
typedef Event<void(class Form*)> FormSizeChangedEvent;
typedef Event<void(class Form*, std::wstring, std::wstring)> FormTextChangedEvent;
typedef Event<void(class Form*)> FormGotFocusEvent;
typedef Event<void(class Form*)> FormLostFocusEvent;
typedef Event<void(class Form*, List<std::wstring>)> FormDropFileEvent;
typedef Event<void(class Form*, std::wstring)> FormDropTextEvent;
typedef Event<void(class Form*, MouseEventArgs)> FormMouseClickEvent;
class Form
{
private:
	friend class FormDropTarget;
	POINT _Location_INIT;
	SIZE _Size_INTI;
	std::wstring _text;
	Font* _font = NULL;
	bool _ownsFont = false;
	std::shared_ptr<BitmapSource> _imageSource;
	Microsoft::WRL::ComPtr<ID2D1Bitmap> _imageCache;
	ID2D1RenderTarget* _imageCacheTarget = nullptr;
	bool _allowResize = true;
	bool _maxBoxBeforeAllowResize = true;
	enum class CaptionButtonKind : uint8_t { Minimize, Maximize, Close };
	enum class CaptionButtonState : uint8_t { None, Hover, Pressed };
	CaptionButtonState _capMinState = CaptionButtonState::None;
	CaptionButtonState _capMaxState = CaptionButtonState::None;
	CaptionButtonState _capCloseState = CaptionButtonState::None;
	bool _capPressed = false;
	CaptionButtonKind _capPressedKind = CaptionButtonKind::Close;
	bool _capTracking = false;

	D2D1_COLOR_F HeadBackColor = { 0.5f ,0.5f ,0.5f ,0.25f };
	D2D1_COLOR_F CaptionHoverColor = { 1.f, 1.f, 1.f, 0.15f };
	D2D1_COLOR_F CaptionPressedColor = { 1.f, 1.f, 1.f, 0.25f };
	D2D1_COLOR_F CloseHoverColor = { 0.90f, 0.20f, 0.20f, 0.50f };
	D2D1_COLOR_F ClosePressedColor = { 0.90f, 0.20f, 0.20f, 0.70f };

	int ClientTop() { return VisibleHead ? HeadHeight : 0; }
	RECT TitleBarRectClient() { auto s = this->Size; return RECT{ 0, 0, s.cx, this->ClientTop() }; }
	bool TryGetCaptionButtonRect(CaptionButtonKind kind, RECT& out);
	bool HitTestCaptionButtons(POINT ptClient, CaptionButtonKind& outKind);
	void UpdateCaptionHover(POINT ptClient);
	void ExecuteCaptionButton(CaptionButtonKind kind);
	void ClearCaptionStates();
	bool _showInTaskBar = true;
	UINT_PTR _animTimerId = 0xC001;
	UINT _animIntervalMs = 0;
	bool _hasRenderedOnce = false;
	void SyncRenderSizeToClient();
	// ---- DPI ----
	UINT _dpi = 96;
	UINT _contentDpi = 96; // 控件树/布局当前已应用的 DPI
	bool _initialDpiApplied = false;
	bool _initialWindowRectApplied = false;
	int _headHeightBase96 = 24;
	Font* _scaledDefaultFont = nullptr;
	UINT _scaledDefaultFontDpi = 0;
	Font* GetScaledDefaultFont();
	void ApplyDpiChange(UINT newDpi);
	void ScaleControlTreeForDpi(UINT fromDpi, UINT toDpi);
	void EnsureInitialDpiApplied();
	void InvalidateControl(class Control* c, int inflatePx = 2, bool immediate = false);
	void InvalidateAnimatedControls(bool immediate = false);
	static bool RectIntersects(const RECT& a, const RECT& b);
	static RECT ToRECT(D2D1_RECT_F r, int inflatePx = 0);

	CursorKind _currentCursor = CursorKind::Arrow;
	void ApplyCursor(CursorKind kind);
	void UpdateCursor(POINT mouseClient, POINT contentMouse);
	CursorKind QueryCursorAt(POINT mouseClient, POINT contentMouse);
	class Control* HitTestControlAt(POINT contentMouse);
	static HCURSOR GetSystemCursor(CursorKind kind);

	// 布局支持
	class LayoutEngine* _layoutEngine = nullptr;
	bool _needsLayout = false;
	bool _resourcesCleaned = false;
	// 鼠标 Hover/Leave 跟踪
	bool _mouseLeaveTracking = false;
	class Control* _hoverControl = NULL;
	// 焦点通知同步：用于捕获直接写 Selected 的旧代码路径
	class Control* _focusNotifiedSelected = NULL;
	// OLE Drag&Drop 支持：用于在拖动悬停时返回接受/不接受光标，并支持文本拖放
	struct IDropTarget* _dropTarget = nullptr;
	bool _dropRegistered = false;
	static void EnsureOleInitialized();
	void EnsureDropTargetRegistered();
	void CleanupResources();
	ID2D1Bitmap* EnsureImageCache();
	void ResetImageCache();

public:
	FormMouseWheelEvent OnMouseWheel = FormMouseWheelEvent();
	FormMouseMoveEvent OnMouseMove = FormMouseMoveEvent();
	FormMouseUpEvent OnMouseUp = FormMouseUpEvent();
	FormMouseDownEvent OnMouseDown = FormMouseDownEvent();
	MouseDoubleClickEvent OnMouseDoubleClick = MouseDoubleClickEvent();
	FormMouseClickEvent OnMouseClick = FormMouseClickEvent();
	MouseEnterEvent OnMouseEnter = MouseEnterEvent();
	MouseLeavedEvent OnMouseLeaved = MouseLeavedEvent();
	FormKeyUpEvent OnKeyUp = FormKeyUpEvent();
	FormKeyDownEvent OnKeyDown = FormKeyDownEvent();
	FormPaintEvent OnPaint = FormPaintEvent();
	CloseEvent OnClose = CloseEvent();
	FormMovedEvent OnMoved = FormMovedEvent();
	FormSizeChangedEvent OnSizeChanged = FormSizeChangedEvent();
	FormTextChangedEvent OnTextChanged = FormTextChangedEvent();
	FormCharInputEvent OnCharInput = FormCharInputEvent();
	FormGotFocusEvent OnGotFocus = FormGotFocusEvent();
	FormLostFocusEvent OnLostFocus = FormLostFocusEvent();
	FormDropFileEvent OnDropFile = FormDropFileEvent();
	FormDropTextEvent OnDropText = FormDropTextEvent();
	FormClosingEvent OnFormClosing = FormClosingEvent();
	FormClosedEvent OnFormClosed = FormClosedEvent();

	CommandEvent OnCommand;

	HWND Handle = NULL;
	bool MinBox = true;
	bool MaxBox = true;
	bool CloseBox = true;
	bool VisibleHead = true;
	bool CenterTitle = true;
	bool ControlChanged = false;
	class Control* Selected = NULL;
	class Control* UnderMouse = NULL;
	List<class Control*> Controls = List<class Control*>();
	// 置顶控件：最多只允许一个（用于 ComboBox 下拉、临时浮层等）
	class Control* ForegroundControl = NULL;
	// 主菜单：单独管理（菜单栏/下拉菜单）
	class Menu* MainMenu = NULL;
	// 状态栏：单独管理（置底但置顶于普通控件；需要独立渲染与消息处理）
	class StatusBar* MainStatusBar = NULL;
	D2DGraphics* Render;
	D2DGraphics* OverlayRender = nullptr;
	int HeadHeight = 24;
	D2D1_COLOR_F BackColor = Colors::WhiteSmoke;
	D2D1_COLOR_F ForeColor = Colors::Black;
	PROPERTY(std::shared_ptr<BitmapSource>, Image);
	GET(std::shared_ptr<BitmapSource>, Image);
	SET(std::shared_ptr<BitmapSource>, Image);
	ImageSizeMode SizeMode = ImageSizeMode::Normal;
	PROPERTY(POINT, Location);
	GET(POINT, Location);
	SET(POINT, Location);

	PROPERTY(bool, ShowInTaskBar);
	GET(bool, ShowInTaskBar);
	SET(bool, ShowInTaskBar);

	PROPERTY(SIZE, Size);
	GET(SIZE, Size);
	SET(SIZE, Size);
	READONLY_PROPERTY(SIZE, ClientSize);
	GET(SIZE, ClientSize);
	PROPERTY(std::wstring, Text);
	GET(std::wstring, Text);
	SET(std::wstring, Text);
	class Font* GetFont();
	void SetFont(class Font* value);
	// 显式设置是否由 Form 释放 Font（默认：通过属性 Font 设置时视为“拥有”）
	void SetFontEx(class Font* value, bool takeOwnership);
	PROPERTY(bool, TopMost);
	GET(bool, TopMost);
	SET(bool, TopMost);
	PROPERTY(bool, Enable);
	GET(bool, Enable);
	SET(bool, Enable);
	PROPERTY(bool, Visible);
	GET(bool, Visible);
	SET(bool, Visible);

	PROPERTY(bool, AllowResize);
	GET(bool, AllowResize);
	SET(bool, AllowResize);

	HICON Icon = NULL;
	Form(std::wstring _text = L"NativeWindow", POINT _location = { 0,0 }, SIZE _size = { 600,400 });
	~Form();
	// 统一设置键盘焦点控件（Selected），并触发控件 Got/LostFocus。
	void SetSelectedControl(class Control* value, bool postRender = true);
	void Show();
	void ShowDialog(HWND parent = NULL);
	void Close();
	void UpdateCursorFromCurrentMouse();

	template<typename T>
	T AddControl(T c)
	{
		if (c->Parent)
		{
			throw "该控件已属于其他容器!";
			return NULL;
		}
		if (this->Controls.Contains(c))
		{
			return c;
		}
		this->Controls.Add(c);
		c->Parent = NULL;
		c->ParentForm = this;

		// 递归设置所有子控件的ParentForm
		Control::SetChildrenParentForm(c, this);

		// 主菜单单独管理
		if (c->Type() == UIClass::UI_Menu)
		{
			this->MainMenu = (Menu*)c;
		}
		// 状态栏单独管理（TopMost=true 时）
		if (c->Type() == UIClass::UI_StatusBar)
		{
			auto* sb = (StatusBar*)c;
			if (sb && sb->TopMost)
				this->MainStatusBar = sb;
		}
		// 触发布局
		_needsLayout = true;
		return c;
	}
	bool RemoveControl(Control* c);
	virtual bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof);
	virtual bool Update(bool force = false);
	virtual bool UpdateDirtyRect(const RECT& dirty, bool force = false);
	virtual bool ForceUpdate();
	void Invalidate(bool immediate = false);
	void Invalidate(const RECT& rc, bool immediate = false);
	void Invalidate(D2D1_RECT_F rc, bool immediate = false);
	virtual void RenderImage();
	D2D1_RECT_F ChildRect();
	Control* LastChild();

	// 布局管理
	void SetLayoutEngine(class LayoutEngine* engine);
	void PerformLayout();
	void InvalidateLayout() { _needsLayout = true; if (_layoutEngine) _layoutEngine->Invalidate(); }

	static bool DoEvent();
	static bool WaiteEvent();
	static LRESULT CALLBACK WINMSG_PROCESS(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
};