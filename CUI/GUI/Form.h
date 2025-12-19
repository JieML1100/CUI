#pragma once
#include <CppUtils/Utils/Utils.h>
#include "Control.h"
#include "Application.h"
#include "Button.h"
#include "CheckBox.h"
#include "ComboBox.h"
#include "GridView.h"
#include "Label.h"
#include "Panel.h"
#include "PasswordBox.h"
#include "PictureBox.h"
#include "ProgressBar.h"
#include "RadioBox.h"
#include "RichTextBox.h"
#include "RoundTextBox.h"
#include "Switch.h"
#include "TabControl.h"
#include "TextBox.h"
#include "TreeView.h"
#include "Taskbar.h"
#include "NotifyIcon.h"
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
typedef Event<void(class Form*, MouseEventArgs)> FormMouseClickEvent;
class Form
{
private:
	POINT _Location_INIT;
	SIZE _Size_INTI;
	std::wstring _text;
	// 自绘标题栏按钮（避免用 3 个 Button 控件污染控件树/事件分发）
	enum class CaptionButtonKind : uint8_t { Minimize, Maximize, Close };
	enum class CaptionButtonState : uint8_t { None, Hover, Pressed };
	CaptionButtonState _capMinState = CaptionButtonState::None;
	CaptionButtonState _capMaxState = CaptionButtonState::None;
	CaptionButtonState _capCloseState = CaptionButtonState::None;
	bool _capPressed = false;
	CaptionButtonKind _capPressedKind = CaptionButtonKind::Close;
	bool _capTracking = false;

	// 标题栏外观（可后续抽成 Theme）
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
	// 连续刷新（动画/实时）定时器
	UINT_PTR _animTimerId = 0xC001;
	UINT _animIntervalMs = 0;
	void UpdateAnimationTimer();
	int ComputeDesiredFrameIntervalMs();
	void InvalidateControl(class Control* c, int inflatePx = 2, bool immediate = false);
	void InvalidateAnimatedControls(bool immediate = false);
	static bool RectIntersects(const RECT& a, const RECT& b);
	static RECT ToRECT(D2D1_RECT_F r, int inflatePx = 0);
public:
	FormMouseWheelEvent OnMouseWheel;
	FormMouseMoveEvent OnMouseMove;
	FormMouseUpEvent OnMouseUp;
	FormMouseDownEvent OnMouseDown;
	MouseDoubleClickEvent OnMouseDoubleClick;
	FormMouseClickEvent OnMouseClick;
	MouseEnterEvent OnMouseEnter;
	MouseLeavedEvent OnMouseLeaved;
	FormKeyUpEvent OnKeyUp;
	FormKeyDownEvent OnKeyDown;
	FormPaintEvent OnPaint;
	CloseEvent OnClose;
	FormMovedEvent OnMoved;
	FormSizeChangedEvent OnSizeChanged;
	FormTextChangedEvent OnTextChanged;
	FormCharInputEvent OnCharInput;
	FormGotFocusEvent OnGotFocus;
	FormLostFocusEvent OnLostFocus;
	FormDropFileEvent OnDropFile;
	FormClosingEvent OnFormClosing;
	FormClosedEvent OnFormClosed;
	
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
	List<class Control*> ForegroundControls = List<class Control*>();
	D2DGraphics* Render;
	int HeadHeight = 24;
	D2D1_COLOR_F BackColor = Colors::WhiteSmoke;
	D2D1_COLOR_F ForeColor = Colors::Black;
	ID2D1Bitmap* Image = NULL;
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
	PROPERTY(bool, TopMost);
	GET(bool, TopMost);
	SET(bool, TopMost);
	PROPERTY(bool, Enable);
	GET(bool, Enable);
	SET(bool, Enable);
	PROPERTY(bool, Visible);
	GET(bool, Visible);
	SET(bool, Visible);

	HICON Icon = NULL;
	Form(std::wstring _text = L"NativeWindow", POINT _location = { 0,0 }, SIZE _size = { 600,400 });
	void Show();
	// 模态显示：parent==NULL 时自动在当前进程内选择一个合适的 owner（若没有则保持 NULL）
	void ShowDialog(HWND parent = NULL);
	void Close();

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
		return c;
	}
	bool RemoveControl(Control* c);
	virtual bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof);
	virtual bool Update(bool force = false);
	// WM_PAINT 专用：按给定脏区进行局部绘制（客户端坐标）
	virtual bool UpdateDirtyRect(const RECT& dirty, bool force = false);
	virtual bool ForceUpdate();
	// 请求重绘（统一走 WM_PAINT），用于 PostRender/实时控件刷新
	void Invalidate(bool immediate = false);
	// 仅重绘指定区域（客户端坐标）
	void Invalidate(const RECT& rc, bool immediate = false);
	// 仅重绘指定区域（客户端坐标，浮点）
	void Invalidate(D2D1_RECT_F rc, bool immediate = false);
	virtual void RenderImage();
	D2D1_RECT_F ChildRect();
	Control* LastChild();
	static bool DoEvent();
	
	static bool WaiteEvent();
	static LRESULT CALLBACK WINMSG_PROCESS(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
};