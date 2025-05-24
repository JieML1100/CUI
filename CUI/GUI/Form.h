#pragma once
#include "Utils/Utils.h"
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
	Button* _minBox;
	Button* _maxBox;
	Button* _closeBox;
	void updateHead();
	bool _showInTaskBar = true;
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
	List<class Control*> ForeGroundControls = List<class Control*>();
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
	void ShowDialog();
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
	virtual bool ForceUpdate();
	virtual void RenderImage();
	D2D1_RECT_F ChildRect();
	Control* LastChild();
	static bool DoEvent();
	
	static bool WaiteEvent();
	static LRESULT CALLBACK WINMSG_PROCESS(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
};