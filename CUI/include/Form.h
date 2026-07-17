#pragma once
#include <vector>
#include <string>
#include "Control.h"
#include "Application.h"
#include "Button.h"
#include "CheckBox.h"
#include "ComboBox.h"
#include "CalendarView.h"
#include "ColorPicker.h"
#include "DateTimePicker.h"
#include "GridView.h"
#include "PagedGridView.h"
#include "Label.h"
#include "LinkLabel.h"
#include "Panel.h"
#include "GroupBox.h"
#include "ScrollView.h"
#include "PasswordBox.h"
#include "PictureBox.h"
#include "LoadingRing.h"
#include "ProgressBar.h"
#include "ProgressRing.h"
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
#include "ToolTip.h"
#include "TreeView.h"
#include "ContextMenu.h"
#include "Taskbar.h"
#include "NotifyIcon.h"
#include "MediaPlayer.h"
#include "NavigationView.h"
#include "SplitContainer.h"
#include "NumericUpDown.h"
#include "Expander.h"

#if defined(_MSC_VER)
#pragma comment(lib, "Dwmapi.lib")
#endif

struct IDCompositionDevice;
struct IDCompositionVisual;
class FormAccessibleObject;
class FormUiaProvider;

typedef Event<void(class Form* sender, int Id, int info)> CommandEvent;
typedef Event<void(class Form*)> FormClosingEvent;
typedef Event<void(class Form*)> FormClosedEvent;
typedef Event<void(class Form*)> FormShownEvent;
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
typedef Event<void(class Form*, std::wstring, std::wstring)> FormThemeChangedEvent;
typedef Event<void(class Form*)> FormGotFocusEvent;
typedef Event<void(class Form*)> FormLostFocusEvent;
typedef Event<void(class Form*, std::vector<std::wstring>)> FormDropFileEvent;
typedef Event<void(class Form*, std::wstring)> FormDropTextEvent;
typedef Event<void(class Form*, MouseEventArgs)> FormMouseClickEvent;
typedef Event<void(class Form*,bool&)> FormCloseEvent;

struct FormThemeFrame
{
	D2D1_COLOR_F WindowBackColor = cui::theme::palette::Window;
	D2D1_COLOR_F WindowForeColor = cui::theme::palette::TextPrimary;
	D2D1_COLOR_F WindowBorderLightColor = cui::theme::palette::Surface;
	D2D1_COLOR_F WindowBorderDarkColor = cui::theme::palette::BorderStrong;
	D2D1_COLOR_F TitleBarBackColor = cui::theme::palette::SurfaceMuted;
	D2D1_COLOR_F CaptionHoverColor = cui::theme::palette::AccentSoft;
	D2D1_COLOR_F CaptionPressedColor = cui::theme::palette::AccentSelected;
	D2D1_COLOR_F CloseHoverColor = D2D1_COLOR_F{ 0.90f, 0.20f, 0.20f, 0.50f };
	D2D1_COLOR_F ClosePressedColor = D2D1_COLOR_F{ 0.90f, 0.20f, 0.20f, 0.70f };
	D2D1_COLOR_F ValidationErrorColor = D2D1_COLOR_F{ 0.90f, 0.20f, 0.24f, 1.0f };
	D2D1_COLOR_F ValidationWarningColor = D2D1_COLOR_F{ 0.95f, 0.62f, 0.12f, 1.0f };
	D2D1_COLOR_F ValidationInfoColor = D2D1_COLOR_F{ 0.12f, 0.52f, 0.88f, 1.0f };
	D2D1_COLOR_F ValidationToolTipBackColor = cui::theme::palette::TooltipSurface;
	D2D1_COLOR_F ValidationToolTipTextColor = cui::theme::palette::OnAccent;
};

/**
 * @file Form.h
 * @brief 顶层窗口(Form)定义：消息循环、控件管理、渲染与输入分发。
 *
 * Form 负责：
 * - 维护 HWND 与窗口状态（大小/位置/标题栏按钮等）
 * - 承载控件树（Controls），并进行命中测试/焦点管理
 * - 触发布局（LayoutEngine）以及渲染（D2DGraphics）
 * - 支持 Overlay 渲染与可选的 DirectComposition 容器（供 WebView2 等使用）
 */
class Form
{
private:
	friend class FormDropTarget;
	friend class Control;
	friend class FormAccessibleObject;
	friend class FormUiaProvider;
	void ClearDetachedControlReferences(Control* root);
	std::vector<Control*> GetAccessibleControls() const;
	void NotifyAccessibilityEvent(Control* control, AccessibilityChange change);
	void NotifyAccessibilityVirtualEvent(
		Control* owner, uint32_t virtualId, AccessibilityChange change);
	LRESULT HandleAccessibleObjectRequest(WPARAM wParam, LPARAM lParam);
	POINT _initialLocation;
	bool _autoCenterOnCreate = false;
	SIZE _initialSize;
	std::wstring _text;
	Font* _font = nullptr;
	bool _ownsFont = false;
	std::unique_ptr<Font> _systemScaledFont;
	Font* _systemScaledFontSource = nullptr;
	float _systemScaledFontSourceSize = 0.0f;
	float _systemScaledFontFactor = 1.0f;
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

	D2D1_COLOR_F HeadBackColor = cui::theme::palette::SurfaceMuted;
	D2D1_COLOR_F CaptionHoverColor = cui::theme::palette::AccentSoft;
	D2D1_COLOR_F CaptionPressedColor = cui::theme::palette::AccentSelected;
	D2D1_COLOR_F CloseHoverColor = { 0.90f, 0.20f, 0.20f, 0.50f };
	D2D1_COLOR_F ClosePressedColor = { 0.90f, 0.20f, 0.20f, 0.70f };
	D2D1_COLOR_F BorderLightColor = cui::theme::palette::Surface;
	D2D1_COLOR_F BorderDarkColor = cui::theme::palette::BorderStrong;
	D2D1_COLOR_F ValidationErrorColor = D2D1_COLOR_F{ 0.90f, 0.20f, 0.24f, 1.0f };
	D2D1_COLOR_F ValidationWarningColor = D2D1_COLOR_F{ 0.95f, 0.62f, 0.12f, 1.0f };
	D2D1_COLOR_F ValidationInfoColor = D2D1_COLOR_F{ 0.12f, 0.52f, 0.88f, 1.0f };
	D2D1_COLOR_F ValidationToolTipBackColor = cui::theme::palette::TooltipSurface;
	D2D1_COLOR_F ValidationToolTipTextColor = cui::theme::palette::OnAccent;
	std::wstring _themeName = L"default";
	bool _showInTaskBar = true;
	UINT_PTR _animTimerId = 0xC001;
	UINT _animIntervalMs = 0;
	bool _hasRenderedOnce = false;
	bool _shownRaised = false;
	CursorKind _currentCursor = CursorKind::Arrow;

	bool TryGetCaptionButtonRect(CaptionButtonKind kind, RECT& out);
	bool HitTestCaptionButtons(POINT ptClient, CaptionButtonKind& outKind);
	bool HitTestCaptionButtonResizeExclusion(POINT ptClient);
	void UpdateCaptionHover(POINT ptClient);
	void ExecuteCaptionButton(CaptionButtonKind kind);
	void ApplyWindowIcon();
	void RaiseShownOnce();
	void ClearCaptionStates();
	void RefreshAnimationTimer();
	void InvalidateControl(class Control* control, float inflateDip = 2.0f, bool immediate = false);
	void InvalidateAnimatedControls(bool immediate = false);
	static bool RectIntersects(const RECT& a, const RECT& b);
	static RECT ToRECT(D2D1_RECT_F rect, int inflatePx = 0);

	void ApplyCursor(CursorKind kind);
	bool ApplySystemCursorId(UINT32 cursorId);
	void UpdateCursor(POINT mouseClient, POINT contentMouse);
	CursorKind QueryCursorAt(POINT mouseClient, POINT contentMouse);
	class Control* HitTestControlAt(POINT contentMouse);
	void RenderValidationToolTip();
	static HCURSOR GetSystemCursor(CursorKind kind);

	// 布局支持
	std::unique_ptr<class LayoutEngine> _layoutEngine;
	bool _needsLayout = false;
	cui::layout::LayoutDeferral _layoutDeferral;
	bool _resourcesCleaned = false;
	class DCompLayeredHost* _dcompHost = nullptr;
	bool EnsureDCompInitialized();
	bool _dcompSceneRenderActive = false;
	int _dcompSceneOrderCounter = 0;
	struct DCompD2DLayer
	{
		IDCompositionVisual* Visual = nullptr;
		D2DGraphics* Render = nullptr;
	};
	struct DCompSceneBuildState
	{
		size_t LayerIndex = 0;
		D2DGraphics* SegmentRender = nullptr;
		int SegmentOrder = 0;
		bool SegmentOpen = false;
		RECT ContentDirty{};
		float LogW = 0.0f;
		float LogH = 0.0f;
		D2DGraphics* OldRender = nullptr;
	};
	std::vector<DCompD2DLayer> _dcompD2DLayers;
	D2DGraphics* GetDCompD2DLayerRender(size_t index, int layer, int order);
	void ReleaseDCompD2DLayers();
	void ClearUnusedDCompD2DLayers(size_t usedCount, float logW, float logH);
	void RenderDCompRootLayers(const RECT& contentDirty, int titleBarOffset, float dpiScale);
	void RenderDCompControlTree(class Control* control, DCompSceneBuildState& state);
	void BeginDCompD2DSegment(DCompSceneBuildState& state, int order);
	void EndDCompD2DSegment(DCompSceneBuildState& state);
	void RenderDCompD2DControlInSegment(class Control* control, DCompSceneBuildState& state);
	bool ShouldSkipRootDCompSceneControl(class Control* control) const;
	std::vector<class Control*> GetDCompSceneChildren(class Control* control);
	bool IsNativeDCompControl(class Control* control) const;
	bool GetDCompSceneClientClip(class Control* control, const RECT& contentDirty, RECT& outClip);
	// ---- DPI ----
	UINT _dpi = 96;
	bool _initialDpiApplied = false;
	bool _initialWindowRectApplied = false;
	int _headHeightBase96 = 24;
	void SyncRenderSizeToClient();
	Font* GetScaledDefaultFont();
	void ApplyDpiChange(UINT newDpi);
	void EnsureInitialDpiApplied();
	// 鼠标 Hover/Leave 跟踪
	bool _mouseLeaveTracking = false;
	class Control* _hoverControl = nullptr;
	class Control* _mouseCaptureControl = nullptr;
	// 焦点通知同步：用于捕获直接写 Selected 的旧代码路径
	class Control* _focusNotifiedSelected = nullptr;
	class Button* _defaultButton = nullptr;
	class Button* _cancelButton = nullptr;
	class FormAccessibleObject* _accessibleObject = nullptr;
	class FormUiaProvider* _uiaProvider = nullptr;
	SystemVisualPreferences _systemVisualPreferences;
	bool _keyboardFocusVisualRequested = false;
	bool _lastKeyboardMessageHandled = false;
	wchar_t _suppressedCharacter = L'\0';
	// OLE Drag&Drop 支持：用于在拖动悬停时返回接受/不接受光标，并支持文本拖放
	struct IDropTarget* _dropTarget = nullptr;
	bool _dropRegistered = false;
	static void EnsureOleInitialized();
	void EnsureDropTargetRegistered();
	void CleanupResources();
	ID2D1Bitmap* EnsureImageCache();
	void ResetImageCache();

public:
	/** @brief 鼠标滚轮事件（窗口级）。 */
	FormMouseWheelEvent OnMouseWheel = FormMouseWheelEvent();
	/** @brief 鼠标移动事件（窗口级）。 */
	FormMouseMoveEvent OnMouseMove = FormMouseMoveEvent();
	/** @brief 鼠标抬起事件（窗口级）。 */
	FormMouseUpEvent OnMouseUp = FormMouseUpEvent();
	/** @brief 鼠标按下事件（窗口级）。 */
	FormMouseDownEvent OnMouseDown = FormMouseDownEvent();
	MouseDoubleClickEvent OnMouseDoubleClick = MouseDoubleClickEvent();
	FormMouseClickEvent OnMouseClick = FormMouseClickEvent();
	MouseEnterEvent OnMouseEnter = MouseEnterEvent();
	MouseLeaveEvent OnMouseLeave = MouseLeaveEvent();
	/** @brief 键盘抬起事件（窗口级）。 */
	FormKeyUpEvent OnKeyUp = FormKeyUpEvent();
	/** @brief 键盘按下事件（窗口级）。 */
	FormKeyDownEvent OnKeyDown = FormKeyDownEvent();
	/** @brief 绘制事件（窗口级）。 */
	FormPaintEvent OnPaint = FormPaintEvent();
	FormCloseEvent OnClosing = FormCloseEvent();
	FormMovedEvent OnMoved = FormMovedEvent();
	FormSizeChangedEvent OnSizeChanged = FormSizeChangedEvent();
	/** @brief 标题文本变化事件。 */
	FormTextChangedEvent OnTextChanged = FormTextChangedEvent();
	/** @brief 字符输入事件（已解析为 wchar_t）。 */
	FormCharInputEvent OnCharInput = FormCharInputEvent();
	/** @brief 主题变更事件。 */
	FormThemeChangedEvent OnThemeChanged = FormThemeChangedEvent();
	FormGotFocusEvent OnGotFocus = FormGotFocusEvent();
	FormLostFocusEvent OnLostFocus = FormLostFocusEvent();
	/** @brief 文件拖放事件。 */
	FormDropFileEvent OnDropFile = FormDropFileEvent();
	/** @brief 文本拖放事件。 */
	FormDropTextEvent OnDropText = FormDropTextEvent();
	/** @brief 关闭中事件（允许外部拦截/取消关闭）。 */
	FormClosingEvent OnFormClosing = FormClosingEvent();
	/** @brief 已关闭事件。 */
	FormClosedEvent OnFormClosed = FormClosedEvent();
	/** @brief 窗体首次显示事件；每个 Form 实例只触发一次。 */
	FormShownEvent OnShown = FormShownEvent();

	CommandEvent OnCommand;

	/** @brief Win32 窗口句柄。 */
	HWND Handle = nullptr;
	bool MinBox = true;
	bool MaxBox = true;
	bool CloseBox = true;
	bool VisibleHead = true;
	bool CenterTitle = true;
	bool ControlChanged = false;
	/** @brief 当前具有键盘焦点的控件。 */
	class Control* Selected = nullptr;
	class Control* UnderMouse = nullptr;
	/** @brief 顶层控件集合（通常包含布局容器与各控件）。 */
	std::vector<class Control*> Controls = std::vector<class Control*>();
	// 置顶控件：最多只允许一个（用于 ComboBox 下拉、临时浮层等）
	class Control* ForegroundControl = nullptr;
	// 主菜单：单独管理（菜单栏/下拉菜单）
	class Menu* MainMenu = nullptr;
	// 主工具栏：单独管理（跟随客户区宽度，位于主菜单下方）
	class ToolBar* MainToolBar = nullptr;
	// 状态栏：单独管理（置底但置顶于普通控件；需要独立渲染与消息处理）
	class StatusBar* MainStatusBar = nullptr;
	/** @brief 主渲染器（控件树渲染）。 */
	D2DGraphics* Render;
	/** @brief 覆盖层渲染器（用于前景控件/临时浮层等）。 */
	D2DGraphics* OverlayRender = nullptr;
	bool _recoveringDeviceLost = false;
	void RecoverRenderIfNeeded();
	int HeadHeight = 24;
	/** @brief Returns the current DPI-to-96 scale factor (e.g., 2.0 at 192 DPI). */
	float GetDpiScale() const { return _dpi > 0 ? (_dpi / 96.0f) : 1.0f; }
	/**
	 * @brief Converts a rectangle from content-space DIPs to Win32 client pixels.
	 * @param contentRect Rectangle relative to the content origin, in 96-DPI logical units.
	 * @param inflateDip Optional logical padding applied before conversion.
	 */
	RECT ContentDipRectToClientPixels(const D2D1_RECT_F& contentRect, float inflateDip = 0.0f) const;
	void SetImeCompositionWindowFromLogicalRect(const D2D1_RECT_F& logicalRect);
	D2D1_COLOR_F BackColor = cui::theme::palette::Window;
	D2D1_COLOR_F ForeColor = cui::theme::palette::TextPrimary;
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
	/** Returns the unscaled explicitly configured font, or nullptr for default. */
	class Font* GetConfiguredFont() noexcept { return _font; }
	const class Font* GetConfiguredFont() const noexcept { return _font; }
	bool UsesDefaultFont() const noexcept { return _font == nullptr; }
	bool OwnsConfiguredFont() const noexcept { return _font && _ownsFont; }
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

	HICON Icon = nullptr;
	/**
	 * @brief 创建一个顶层窗口。
	 * @param _text 窗口标题。
	 * @param _location 初始位置。
	 * @param _size 初始大小。
	 */
	Form(std::wstring _text = L"NativeWindow", POINT _location = { 0,0 }, SIZE _size = { 600,400 });
	~Form();
	const std::wstring& GetThemeName() const { return _themeName; }
	FormThemeFrame GetThemeFrame() const;
	FormThemeFrame GetEffectiveThemeFrame() const;
	void ApplyThemeFrame(const FormThemeFrame& theme, const std::wstring& themeName = L"");
	const SystemVisualPreferences& GetSystemVisualPreferences() const noexcept
	{
		return _systemVisualPreferences;
	}
	void ApplySystemVisualPreferences(SystemVisualPreferences preferences);
	void RefreshSystemVisualPreferences();
	bool AreSystemAnimationsEnabled() const noexcept
	{
		return _systemVisualPreferences.AnimationsEnabled;
	}
	float GetTextScaleFactor() const noexcept
	{
		return _systemVisualPreferences.TextScaleFactor();
	}
	bool ShouldShowKeyboardFocusVisual() const noexcept
	{
		return _systemVisualPreferences.KeyboardCuesAlwaysVisible
			|| _keyboardFocusVisualRequested;
	}
	D2D1_COLOR_F GetEffectiveControlBackColor(D2D1_COLOR_F configured) const;
	D2D1_COLOR_F GetEffectiveControlForeColor(D2D1_COLOR_F configured) const;
	D2D1_COLOR_F GetEffectiveFocusColor(D2D1_COLOR_F configured) const;
	D2D1_COLOR_F GetValidationColor(
		BindingValidationSeverity severity) const noexcept;
	// 统一设置键盘焦点控件（Selected），并触发控件 Got/LostFocus。
	void SetSelectedControl(class Control* value, bool invalidateVisual = true);
	/** Returns the effective Tab order (TabIndex, then stable tree order). */
	std::vector<class Control*> GetTabOrder() const;
	/** Pure helper used by designers/tests without creating an HWND. */
	static std::vector<class Control*> BuildTabOrder(
		std::span<class Control* const> roots);
	/** Moves logical focus, wrapping at either end. */
	bool MoveFocus(bool forward = true);
	/** Focuses/invokes the next control matching an explicit or '&'-derived access key. */
	bool ProcessAccessKey(wchar_t key);
	bool SetDefaultButton(class Button* button);
	class Button* GetDefaultButton() const noexcept { return _defaultButton; }
	bool SetCancelButton(class Button* button);
	class Button* GetCancelButton() const noexcept { return _cancelButton; }
	/** @brief 以非模态方式显示窗口。 */
	void Show();
	/** @brief 以模态方式显示窗口。 */
	void ShowDialog(HWND parent = nullptr);
	/** @brief 请求关闭窗口。 */
	void Close();
	/** @brief 根据当前鼠标位置刷新窗口光标显示。 */
	void UpdateCursorFromCurrentMouse();

	int ClientTop() { return VisibleHead ? HeadHeight : 0; }
	RECT TitleBarRectClient() { auto s = this->Size; return RECT{ 0, 0, s.cx, this->ClientTop() }; }


	template<typename T>
	T InsertControl(int index, T control)
	{
		if (!control)
			throw std::invalid_argument("不能添加空控件");
		if (index < 0 || static_cast<size_t>(index) > this->Controls.size())
			throw std::out_of_range("顶层控件索引超出范围");
		if (std::find(this->Controls.begin(), this->Controls.end(), control) != this->Controls.end())
		{
			return control;
		}
		if (control->Parent)
		{
			throw std::logic_error("该控件已属于其他容器");
		}
		if (control->ParentForm && control->ParentForm != this)
		{
			throw std::logic_error("该控件已属于其他窗口");
		}
		this->Controls.insert(this->Controls.begin() + index, control);
		control->Parent = nullptr;
		control->ParentForm = this;
		control->_isFormRoot = true;

		// 递归设置所有子控件的ParentForm
		Control::SetChildrenParentForm(control, this);

		// 主菜单单独管理
		if (control->Type() == UIClass::UI_Menu)
		{
			this->MainMenu = dynamic_cast<Menu*>(control);
		}
		if (control->Type() == UIClass::UI_ToolBar)
		{
			this->MainToolBar = dynamic_cast<ToolBar*>(control);
		}
		// 状态栏单独管理（TopMost=true 时）
		if (control->Type() == UIClass::UI_StatusBar)
		{
			auto* statusBar = dynamic_cast<StatusBar*>(control);
			if (statusBar && statusBar->TopMost)
				this->MainStatusBar = statusBar;
		}
		// 触发布局并安排一帧；窗口隐藏时 Invalidate 只记录脏区，不会强制刷新。
		InvalidateLayout();
		NotifyAccessibilityEvent(nullptr, AccessibilityChange::Structure);
		return control;
	}

	template<typename T>
	T AddControl(T control)
	{
		return InsertControl(static_cast<int>(this->Controls.size()), control);
	}

	/** @brief 安全接收并挂载一个顶层控件，成功后由 Form 接管所有权。 */
	template<typename T>
	T* AddOwned(std::unique_ptr<T> control)
	{
		static_assert(std::is_base_of_v<Control, T>, "T must derive from Control");
		if (!control)
			throw std::invalid_argument("不能添加空控件");
		T* raw = control.get();
		this->AddControl(raw);
		control.release();
		return raw;
	}

	/**
	 * @brief 尝试在指定槽位接收顶层控件，失败时把所有权保留给调用方。
	 *
	 * 该入口供需要回滚保证的宿主事务使用；它会在挂载后的后续步骤抛出
	 * 异常时重新分离控件，避免 unique_ptr 被部分提交吞掉。
	 */
	bool TryInsertOwned(
		int index, std::unique_ptr<Control>& control) noexcept;
	template<typename T>
	bool TryInsertOwned(int index, std::unique_ptr<T>& control) noexcept
	{
		static_assert(std::is_base_of_v<Control, T>, "T must derive from Control");
		std::unique_ptr<Control> base(control.release());
		const bool inserted = TryInsertOwned(index, base);
		control.reset(static_cast<T*>(base.release()));
		return inserted;
	}
	int IndexOfControl(const Control* control) const noexcept;

	/** @brief 原位构造并添加顶层控件。 */
	template<typename T, typename... Args>
	T* Add(Args&&... args)
	{
		static_assert(std::is_base_of_v<Control, T>, "T must derive from Control");
		return AddOwned(std::make_unique<T>(std::forward<Args>(args)...));
	}
	/**
	 * @brief 分离一个顶层控件，并将所有权交还给调用方。
	 * @return 成功时返回拥有该控件的 unique_ptr；control 不是顶层控件时返回空。
	 */
	std::unique_ptr<Control> DetachControl(Control* control);

	/** @brief 类型安全的 DetachControl 重载。 */
	template<typename T>
	std::unique_ptr<T> DetachControl(T* control)
	{
		static_assert(std::is_base_of_v<Control, T>, "T must derive from Control");
		auto detached = DetachControl(static_cast<Control*>(control));
		return std::unique_ptr<T>(static_cast<T*>(detached.release()));
	}

	/** @brief 移除并销毁一个顶层控件。 */
	bool DeleteControl(Control* control);

	/**
	 * @brief 兼容旧代码：只解除挂载，不销毁；分离后的所有权仍由调用方负责。
	 */
	bool RemoveControl(Control* control);
	virtual bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY);
	virtual bool Update(bool force = false);
	virtual bool UpdateDirtyRect(const RECT& dirty, bool force = false);
	virtual bool ForceUpdate();
	/**
	 * @brief 使整个窗口区域失效（触发重绘）。
	 * @param immediate true 表示尽快立即刷新。
	 */
	void Invalidate(bool immediate = false);
	/**
	 * @brief 使窗口指定区域失效（触发重绘）。
	 * @param rect 需要重绘的矩形（客户区坐标）。
	 */
	void Invalidate(const RECT& rect, bool immediate = false);
	void Invalidate(D2D1_RECT_F rect, bool immediate = false);
	virtual void RenderImage();
	D2D1_RECT_F ChildRect();
	Control* LastChild();

	// 布局管理
	/**
	 * @brief 设置窗口内容区的布局引擎，并接管其所有权。
	 *
	 * 菜单、主工具栏和置顶状态栏仍由 Form 独立布局；引擎收到一个只包含
	 * 普通顶层控件的非拥有 Control 根视图。
	 */
	void SetLayoutEngine(class LayoutEngine* engine);
	void PerformLayout();
	void InvalidateLayout();
	/** @brief 暂停窗口布局与重绘调度；支持嵌套调用。 */
	void SuspendLayout();
	/** @brief 恢复一层暂停；最外层恢复时可立即执行一次待处理布局。 */
	void ResumeLayout(bool performLayout = true);
	bool IsLayoutSuspended() const { return _layoutDeferral.IsSuspended(); }

	IDCompositionDevice* GetDCompDevice() const;
	IDCompositionVisual* GetWebContainerVisual() const;
	bool RegisterDCompVisual(IDCompositionVisual* visual, int layer, int order);
	void UpdateDCompVisualOrder(IDCompositionVisual* visual, int layer, int order);
	void UnregisterDCompVisual(IDCompositionVisual* visual);
	int GetDCompVisualOrder(Control* control) const;
	int NextDCompSceneOrder();
	bool IsDCompSceneRenderActive() const { return _dcompSceneRenderActive; }
	void CommitComposition();

	static bool DoEvent();
	static bool WaitEvent();
	static LRESULT CALLBACK WINMSG_PROCESS(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
};
