#pragma once
#define NOMINMAX
#include "Event.h"
#include "Binding.h"
#include "ObservableCollection.h"
#include <Colors.h>
#include <Font.h>
#include <Factory.h>
#include <Graphics.h>


#include <string>
#include <vector>
#include <array>
#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <wrl/client.h>
#include "Layout/LayoutTypes.h"
#include "Layout/LayoutState.h"
#include "Layout/LayoutDeferral.h"

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
	StretchImage,
	Zoom
};

/**
 * @brief 运行时 UI 类型标识，用于 RTTI/序列化/设计器等场景。
 */
enum class UIClass : int
{
	UI_Base,
	UI_Label,
	UI_LinkLabel,
	UI_Button,
	UI_PictureBox,
	UI_TextBox,
	UI_RichTextBox,
	UI_PasswordBox,
	UI_ComboBox,
	UI_ListView,
	UI_ListBox,
	UI_GridView,
	UI_PropertyGrid,
	UI_CheckBox,
	UI_RadioBox,
	UI_ProgressBar,
	UI_LoadingRing,
	UI_ProgressRing,
	UI_TreeView,
	UI_Panel,
	UI_GroupBox,
	UI_ScrollView,
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
	UI_SplitContainer,
	UI_DateTimePicker,
	UI_ToolTip,
	UI_ContextMenu,
	UI_ToastHost,
	UI_ChartView,
	UI_ReportView,
	UI_KpiCard,
	UI_FilterBar,
	UI_NavigationView,
	UI_SideBar,
	UI_BreadcrumbBar,
	UI_CalendarView,
	UI_DateRangePicker,
	UI_ColorPicker,
	UI_PagedGridView,
	UI_NumericUpDown,
	UI_Expander,
	UI_CUSTOM
};

/** Semantic role exposed to assistive technologies. Default is inferred from UIClass. */
enum class AccessibleRole : int
{
	Default,
	Window,
	Pane,
	Group,
	Text,
	Link,
	Button,
	CheckBox,
	RadioButton,
	Switch,
	TextBox,
	PasswordBox,
	ComboBox,
	List,
	ListItem,
	Table,
	Tree,
	Tab,
	TabItem,
	Menu,
	MenuItem,
	ToolBar,
	StatusBar,
	Slider,
	ProgressBar,
	ScrollBar,
	Image,
	Document,
	Custom,
	TreeItem,
	DataItem,
	HeaderItem
};

/** Stable, value-only accessibility view used by native bridges and tests. */
struct AccessibilitySnapshot
{
	AccessibleRole Role = AccessibleRole::Default;
	std::wstring Name;
	std::wstring Description;
	std::wstring HelpText;
	std::wstring Value;
	std::wstring AutomationId;
	std::wstring KeyboardShortcut;
	bool Enabled = true;
	bool Visible = true;
	bool Focusable = false;
	bool Focused = false;
	bool Checked = false;
	bool Selected = false;
	bool Password = false;
	bool ReadOnly = false;
};

enum class AccessibilityChange : uint8_t
{
	Name,
	Description,
	Help,
	Value,
	State,
	Focus,
	Structure,
	Invoke,
	Toggle,
	ExpandCollapse,
	Selection,
	Scroll
};

/** Allocates an identity that remains unique for the lifetime of the process. */
uint32_t AllocateAccessibilityVirtualId() noexcept;

/** UI-agnostic capabilities that a virtual accessibility node can expose. */
enum class AccessibilityVirtualPattern : uint32_t
{
	None = 0,
	Invoke = 1u << 0,
	Toggle = 1u << 1,
	Value = 1u << 2,
	ExpandCollapse = 1u << 3,
	SelectionItem = 1u << 4,
	ScrollItem = 1u << 5,
	VirtualizedItem = 1u << 6,
	GridItem = 1u << 7,
	TableItem = 1u << 8,
	Selection = 1u << 9,
	Grid = 1u << 10,
	Table = 1u << 11,
	Scroll = 1u << 12
};

inline AccessibilityVirtualPattern operator|(
	AccessibilityVirtualPattern left,
	AccessibilityVirtualPattern right) noexcept
{
	return static_cast<AccessibilityVirtualPattern>(
		static_cast<uint32_t>(left) | static_cast<uint32_t>(right));
}

inline AccessibilityVirtualPattern& operator|=(
	AccessibilityVirtualPattern& left,
	AccessibilityVirtualPattern right) noexcept
{
	left = left | right;
	return left;
}

inline bool HasAccessibilityVirtualPattern(
	AccessibilityVirtualPattern value,
	AccessibilityVirtualPattern pattern) noexcept
{
	return (static_cast<uint32_t>(value) & static_cast<uint32_t>(pattern))
		== static_cast<uint32_t>(pattern);
}

enum class AccessibilitySelectionAction : uint8_t
{
	Select,
	Add,
	Remove
};

/** UI Automation-compatible logical scroll increments without COM dependencies. */
enum class AccessibilityScrollAmount : uint8_t
{
	LargeDecrement,
	SmallDecrement,
	NoAmount,
	LargeIncrement,
	SmallIncrement
};

inline constexpr double AccessibilityScrollNoChange = -1.0;

/** Percentage-based snapshot used by native and test accessibility adapters. */
struct AccessibilityScrollInfo
{
	bool HorizontallyScrollable = false;
	double HorizontalScrollPercent = AccessibilityScrollNoChange;
	double HorizontalViewSize = 100.0;
	bool VerticallyScrollable = false;
	double VerticalScrollPercent = AccessibilityScrollNoChange;
	double VerticalViewSize = 100.0;
};

/** Value-only snapshot for an item rendered inside a Control rather than owned as a child. */
struct AccessibilityVirtualNode
{
	uint32_t Id = 0;
	uint32_t ParentId = 0;
	AccessibleRole Role = AccessibleRole::ListItem;
	AccessibilityVirtualPattern Patterns = AccessibilityVirtualPattern::None;
	std::wstring Name;
	std::wstring Description;
	std::wstring Value;
	std::wstring AutomationId;
	D2D1_RECT_F BoundsDip{ 0, 0, 0, 0 }; // owner-local DIPs
	bool Enabled = true;
	bool Visible = true;
	bool Selected = false;
	bool Checked = false;
	bool ReadOnly = true;
	bool Expanded = false;
	int Row = -1;
	int Column = -1;
	int RowSpan = 1;
	int ColumnSpan = 1;
	int Level = 0;
};

struct AccessibilityVirtualContainerInfo
{
	AccessibilityVirtualPattern Patterns = AccessibilityVirtualPattern::None;
	bool CanSelectMultiple = false;
	bool IsSelectionRequired = false;
	int RowCount = 0;
	int ColumnCount = 0;
};

/**
 * Implemented by controls that render logical children without creating Control objects.
 * IDs must remain stable while the corresponding logical item remains in the collection.
 */
class IAccessibilityVirtualizedControl
{
public:
	virtual ~IAccessibilityVirtualizedControl() = default;
	virtual void GetAccessibilityVirtualChildren(
		uint32_t parentId, std::vector<uint32_t>& result) = 0;
	virtual bool TryGetAccessibilityVirtualNode(
		uint32_t id, AccessibilityVirtualNode& result) = 0;
	/**
	 * Indexed navigation lets native providers query one edge/sibling without
	 * materializing every virtual child. Existing implementations remain source
	 * compatible through these vector-backed defaults.
	 */
	virtual size_t GetAccessibilityVirtualChildCount(uint32_t parentId)
	{
		std::vector<uint32_t> children;
		GetAccessibilityVirtualChildren(parentId, children);
		return children.size();
	}
	virtual bool TryGetAccessibilityVirtualChildAt(
		uint32_t parentId, size_t index, uint32_t& result)
	{
		result = 0;
		std::vector<uint32_t> children;
		GetAccessibilityVirtualChildren(parentId, children);
		if (index >= children.size()) return false;
		result = children[index];
		return result != 0;
	}
	virtual bool TryGetAccessibilityVirtualSibling(
		uint32_t parentId, uint32_t id, bool next, uint32_t& result)
	{
		result = 0;
		std::vector<uint32_t> children;
		GetAccessibilityVirtualChildren(parentId, children);
		const auto position = std::find(children.begin(), children.end(), id);
		if (position == children.end()) return false;
		if (next)
		{
			if (position + 1 == children.end()) return false;
			result = *(position + 1);
		}
		else
		{
			if (position == children.begin()) return false;
			result = *(position - 1);
		}
		return result != 0;
	}
	/** Point is expressed in owner-local DIPs; the deepest visible node wins. */
	virtual bool TryHitTestAccessibilityVirtualNode(
		float localX, float localY, uint32_t& result)
	{
		result = 0;
		auto hitChildren = [&](uint32_t parentId, auto&& self) -> bool
		{
			std::vector<uint32_t> children;
			GetAccessibilityVirtualChildren(parentId, children);
			for (auto position = children.rbegin(); position != children.rend(); ++position)
			{
				if (self(*position, self)) return true;
				AccessibilityVirtualNode node;
				if (!TryGetAccessibilityVirtualNode(*position, node)
					|| !node.Visible) continue;
				if (localX >= node.BoundsDip.left && localX < node.BoundsDip.right
					&& localY >= node.BoundsDip.top && localY < node.BoundsDip.bottom)
				{
					result = node.Id;
					return result != 0;
				}
			}
			return false;
		};
		return hitChildren(0, hitChildren);
	}
	virtual AccessibilityVirtualContainerInfo
		GetAccessibilityVirtualContainerInfo() const noexcept { return {}; }
	virtual void GetAccessibilityVirtualSelection(
		std::vector<uint32_t>& result) { result.clear(); }
	virtual bool GetAccessibilityVirtualItemAt(
		int, int, uint32_t&) { return false; }
	virtual void GetAccessibilityVirtualColumnHeaders(
		std::vector<uint32_t>& result) { result.clear(); }
	virtual bool InvokeAccessibilityVirtualNode(uint32_t) { return false; }
	virtual bool ToggleAccessibilityVirtualNode(uint32_t) { return false; }
	virtual bool SetAccessibilityVirtualNodeValue(
		uint32_t, const std::wstring&) { return false; }
	virtual bool SetAccessibilityVirtualNodeExpanded(uint32_t, bool) { return false; }
	virtual bool SelectAccessibilityVirtualNode(
		uint32_t, AccessibilitySelectionAction) { return false; }
	virtual bool ScrollAccessibilityVirtualNodeIntoView(uint32_t) { return false; }
	virtual bool GetAccessibilityScrollInfo(
		AccessibilityScrollInfo& result) const noexcept
	{
		result = {};
		return false;
	}
	virtual bool ScrollAccessibility(
		AccessibilityScrollAmount, AccessibilityScrollAmount) { return false; }
	virtual bool SetAccessibilityScrollPercent(double, double) { return false; }
};

/** States addressable by control-style selectors. */
enum class ControlStyleState : uint32_t
{
	None = 0,
	Hovered = 1u << 0,
	Focused = 1u << 1,
	Pressed = 1u << 2,
	Disabled = 1u << 3,
	Checked = 1u << 4,
	Selected = 1u << 5
};

inline ControlStyleState operator|(
	ControlStyleState left, ControlStyleState right) noexcept
{
	return static_cast<ControlStyleState>(
		static_cast<uint32_t>(left) | static_cast<uint32_t>(right));
}

inline ControlStyleState operator&(
	ControlStyleState left, ControlStyleState right) noexcept
{
	return static_cast<ControlStyleState>(
		static_cast<uint32_t>(left) & static_cast<uint32_t>(right));
}

inline ControlStyleState operator~(ControlStyleState value) noexcept
{
	return static_cast<ControlStyleState>(~static_cast<uint32_t>(value));
}

inline ControlStyleState& operator|=(
	ControlStyleState& left, ControlStyleState right) noexcept
{
	left = left | right;
	return left;
}

inline ControlStyleState& operator&=(
	ControlStyleState& left, ControlStyleState right) noexcept
{
	left = left & right;
	return left;
}

inline bool HasControlStyleState(
	ControlStyleState value, ControlStyleState state) noexcept
{
	return (value & state) == state;
}

/**
 * @brief 光标类型。
 */
enum class CursorKind : uint8_t
{
	Arrow,
	Cross,
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
typedef Event<void(class Control*, MouseEventArgs)> MouseLeaveEvent;
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
typedef Event<void(class Control*, std::vector<std::wstring>)> DropFileEvent;
typedef Event<void(class Control*, std::wstring)> DropTextEvent;
typedef Event<void(class Control*)> SelectionChangedEvent;

struct ControlPropertyChangedEventArgs
{
	std::wstring PropertyName;
	BindingValue OldValue;
	BindingValue NewValue;
};

typedef Event<void(class Control*, const ControlPropertyChangedEventArgs&)>
	ControlPropertyChangedEvent;

class ControlStyleSheet;
class Control;

/**
 * Observable, vector-readable owned child collection.
 * Owner synchronization hooks are framework-only so callers cannot replace
 * the Parent/ParentForm/lifetime invariant handler.
 */
class ControlCollection final : public ObservableCollection<Control*>
{
public:
	using CollectionBase = ObservableCollection<Control*>;
	using Base = typename CollectionBase::Base;
	ControlCollection() = default;
	ControlCollection(const ControlCollection&) = delete;
	ControlCollection(ControlCollection&&) = delete;
	ControlCollection& operator=(const ControlCollection&) = delete;
	ControlCollection& operator=(ControlCollection&&) = delete;
	ControlCollection& operator=(const Base&) = delete;
	ControlCollection& operator=(Base&&) = delete;
	ControlCollection& operator=(std::initializer_list<Control*>) = delete;

private:
	using CollectionBase::SetOwnerChangedHandler;
	using CollectionBase::SetOwnerSynchronizationDuringUpdates;
	friend class Control;
};

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
 * - Image：改为存储 BitmapSource（设备无关），渲染时按需创建 ID2D1Bitmap 缓存。
 */
class Control
{
protected:
	std::vector<Control*> _observedChildren;
	SIZE _size = { 120,20 };
	D2D1_COLOR_F _backcolor = Colors::gray91;
	D2D1_COLOR_F _forecolor = Colors::Black;
	D2D1_COLOR_F _bordercolor = Colors::Black;
	std::shared_ptr<BitmapSource> _imageSource;
	Microsoft::WRL::ComPtr<ID2D1Bitmap> _imageCache;
	ID2D1RenderTarget* _imageCacheTarget = nullptr;
	std::wstring _text = std::wstring(L"");
	Font* _font = nullptr;
	bool _ownsFont = false;
	std::unique_ptr<Font> _systemScaledFont;
	Font* _systemScaledFontSource = nullptr;
	float _systemScaledFontSourceSize = 0.0f;
	float _systemScaledFontFactor = 1.0f;
	D2D1_RECT_F _lastInvalidatedClientRect{ 0,0,0,0 };
	bool _hasLastInvalidatedClientRect = false;
	D2D1_RECT_F _caretBlinkRect{ 0,0,0,0 };
	bool _caretBlinkRectValid = false;
	int _caretBlinkSelectionStart = 0;
	int _caretBlinkSelectionEnd = 0;
	bool _caretBlinkFocused = false;
	ULONGLONG _caretBlinkResetTick = 0;
	bool _hasDCompSceneOrderOverride = false;
	int _dcompSceneOrderOverride = 0;
	bool _isFormRoot = false;
	
	// 布局属性
	POINT _location = { 0,0 };
	Thickness _margin;
	POINT _runtimeLocation = { 0,0 };
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
	bool _visible = true;

	// 新布局内核明确区分用户配置与运行时计算结果。旧字段暂时保留为
	// WinForms 风格 API 的兼容存储，逐步迁移到下面两份状态。
	cui::layout::LayoutStyle _layoutStyle;
	cui::layout::LayoutState _layoutState;
	cui::layout::LayoutDeferral _layoutDeferral;
	struct PropertyValueEntry
	{
		BindingValue BaseValue;
		bool HasBaseValue = false;
		std::array<std::optional<BindingValue>, 4> Values;
		std::array<const Binding*, 4> BindingOwners{};

		bool HasSources() const noexcept
		{
			return std::any_of(Values.begin(), Values.end(),
				[](const auto& value) { return value.has_value(); });
		}
	};
	std::unordered_map<const BindingPropertyMetadata*, PropertyValueEntry>
		_propertyValues;
	const BindingPropertyMetadata* _applyingPropertyMetadata = nullptr;
	ControlPropertyValueSource _applyingPropertySource =
		ControlPropertyValueSource::Default;
	std::unique_ptr<BindingCollection> _dataBindings;
	bool _showValidationBorder = true;
	bool _showValidationToolTip = true;
	float _validationBorderThickness = 2.0f;
	float _validationCornerRadius = 4.0f;
	float _validationToolTipMaxWidth = 320.0f;
	bool _isTabStop = true;
	int _tabIndex = 0;
	std::wstring _accessKey;
	std::wstring _accessibleName;
	std::wstring _accessibleDescription;
	std::wstring _accessibleHelpText;
	std::wstring _automationId;
	AccessibleRole _accessibleRole = AccessibleRole::Default;
	uint32_t _accessibilityRuntimeId = 0;
	bool _showFocusVisual = true;
	D2D1_COLOR_F _focusVisualColor{ 0.20f, 0.46f, 0.90f, 0.95f };
	float _focusVisualThickness = 1.5f;
	unsigned long long _propertyChangeVersion = 0;
	bool _isDestroying = false;
	std::wstring _styleId;
	std::vector<std::wstring> _styleClasses;
	ControlStyleState _styleState = ControlStyleState::None;
	std::shared_ptr<const ControlStyleSheet> _themeStyleSheet;
	std::shared_ptr<const ControlStyleSheet> _styleSheet;
	EventConnection _themeStyleConnection;
	EventConnection _styleSheetConnection;
	std::vector<EventConnection> _styleStateConnections;
	std::array<std::vector<std::wstring>, 2> _styleSheetProperties;
	bool _refreshingStyleValues = false;
	bool _styleRefreshPending = false;
	std::function<void(Control&, D2DGraphics&)> _renderDecorator;

	friend class BindingCollection;
	friend class Binding;
	friend class BindingPropertyMetadata;
	void OnBindingValidationChanged(const std::wstring& targetProperty);
	void RenderValidationAdorner();
	void RenderFocusAdorner();
	void NotifyAccessibilityStructureChanged();
	void NotifyAccessibilityScrollChanged();
	void NotifyAccessibilityVirtualChanged(
		uint32_t virtualId, AccessibilityChange change);
	void RequestArrange();
	void ApplyPropertyMetadataChange(
		const BindingPropertyMetadata& metadata,
		const BindingValue& oldValue,
		const BindingValue& newValue);
	bool ApplyEffectivePropertyValue(
		const BindingPropertyMetadata& metadata,
		const BindingValue& value,
		ControlPropertyValueSource source);
	bool TryResolveEffectivePropertyValue(
		const BindingPropertyMetadata& metadata,
		const PropertyValueEntry& entry,
		BindingValue& value,
		ControlPropertyValueSource& source) const;
	bool HasStoredPropertyValues(
		const BindingPropertyMetadata& metadata) const;
	bool CanAcquireBindingPropertyValue(
		const std::wstring& propertyName,
		const Binding* owner);
	bool TrySetBindingPropertyValue(
		const std::wstring& propertyName,
		const BindingValue& value,
		const Binding* owner);
	bool TrySetPropertyValueOwned(
		const std::wstring& propertyName,
		const BindingValue& value,
		ControlPropertyValueSource source,
		const Binding* owner);
	bool ClearPropertyValueOwned(
		const std::wstring& propertyName,
		ControlPropertyValueSource source,
		const Binding* owner);
	bool ClearBindingPropertyValue(
		const std::wstring& propertyName,
		const Binding* owner);
	bool RefreshStyleValuesForSource(
		ControlPropertyValueSource source,
		const std::shared_ptr<const ControlStyleSheet>& sheet,
		std::vector<std::wstring>& appliedProperties);

	template<typename TValue>
	bool SetPropertyField(
		const std::wstring& propertyName,
		TValue& storage,
		TValue value);

	template<typename TValue>
	bool SetCurrentPropertyField(
		const std::wstring& propertyName,
		TValue& storage,
		TValue value);

	/** Re-runs conversion/coercion for the property's current value source. */
	bool ReevaluatePropertyValue(const std::wstring& propertyName);

	void UpdateLayoutBaseSize(SIZE value)
	{
		_layoutStyle.width = cui::layout::Length::Fixed((float)value.cx);
		_layoutStyle.height = cui::layout::Length::Fixed((float)value.cy);
	}

	cui::core::Size ResolveDesiredSize(
		cui::core::Size intrinsicSize,
		const cui::core::Constraints& available) const;

	void SyncComputedLayoutFromCompatibilityGeometry()
	{
		_layoutState.CommitArrange(cui::core::Rect{
			(float)_runtimeLocation.x,
			(float)_runtimeLocation.y,
			_size.cx > 0 ? (float)_size.cx : 0.0f,
			_size.cy > 0 ? (float)_size.cy : 0.0f });
	}

	void UpdateCaretBlinkState(bool focused, int selectionStart, int selectionEnd, bool caretRectValid, const D2D1_RECT_F* caretRect = nullptr);
	bool IsCaretBlinkVisible() const;
	bool IsCaretBlinkAnimating() const;
	bool GetCaretBlinkInvalidRect(D2D1_RECT_F& outRect) const;
	virtual bool DefaultTrackUnderMouse() const { return false; }
	virtual bool DefaultSelectOnLeftButtonDown() const { return true; }
	virtual bool DefaultSelectOnLeftButtonDoubleClick() const { return false; }
	virtual bool DefaultRaiseClickOnLeftButtonUp() const { return false; }
	virtual bool DefaultClearSelectionOnMouseUp() const { return false; }
	virtual bool DefaultRaiseMouseDoubleClick(UINT message, bool wasSelected) const { (void)message; (void)wasSelected; return true; }
	virtual bool DefaultInvalidateVisualOnMouseDown(UINT message) const { (void)message; return true; }
	virtual bool DefaultInvalidateVisualOnMouseUp(UINT message) const { (void)message; return true; }
	virtual bool DefaultInvalidateVisualOnMouseDoubleClick(UINT message, bool wasSelected) const { (void)message; (void)wasSelected; return false; }
	virtual void BeforeDefaultMouseMove(MouseEventArgs& e) { (void)e; }
	virtual void BeforeDefaultMouseDown(UINT message, MouseEventArgs& e) { (void)message; (void)e; }
	virtual void BeforeDefaultMouseUp(UINT message, MouseEventArgs& e, bool wasSelected) { (void)message; (void)e; (void)wasSelected; }
	virtual void BeforeDefaultClick(UINT message, MouseEventArgs& e) { (void)message; (void)e; }
	virtual void BeforeDefaultMouseDoubleClick(UINT message, MouseEventArgs& e, bool wasSelected) { (void)message; (void)e; (void)wasSelected; }
	virtual void OnComputedLayoutSizeChanged() {}
	virtual void PerformPendingLayout() {}
	/** Allows specialized containers to reject structurally invalid child types. */
	virtual bool ValidateChildCollection(
		std::span<Control* const> children,
		std::string& error) const
	{
		(void)children;
		(void)error;
		return true;
	}
	/** Called after Parent/ParentForm synchronization and before public notification. */
	virtual void OnChildCollectionChanged(
		const CollectionChangedEventArgs& change,
		std::span<Control* const> previousChildren)
	{
		(void)change;
		(void)previousChildren;
	}
	void SynchronizeChildCollection(const CollectionChangedEventArgs& change);

	// 通知父容器（Panel 或 Form）需要重新布局
	virtual void RequestLayout();
	void InvalidateMeasureSubtree();
	// 将内容区 DIP 矩形统一转换为窗口客户区物理像素，并与上次区域取并集。
	void InvalidateVisualRect(const D2D1_RECT_F& contentRect);
	void DispatchInvalidatedClientRect(const D2D1_RECT_F& clientRect);

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
	MouseLeaveEvent OnMouseLeave = MouseLeaveEvent();
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
	/** @brief 选中状态变化事件（如列表项被选中）。 */
	SelectedChangedEvent OnSelectedChanged = SelectedChangedEvent();
	/** @brief 滚动位置变化事件（如 ScrollView 滚动）。 */
	ScrollChangedEvent OnScrollChanged = ScrollChangedEvent();
	/** @brief 文本变化事件。 */
	TextChangedEvent OnTextChanged = TextChangedEvent();
	/** @brief 任意元数据属性的有效值发生变化。 */
	ControlPropertyChangedEvent OnPropertyValueChanged = ControlPropertyChangedEvent();
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
	PROPERTY(bool, Visible);
	GET(bool, Visible);
	SET(bool, Visible);
	bool Checked;
	/** @brief 用户自定义数据槽（不由框架解释）。 */
	UINT64 Tag;
	/**
	 * @brief 设计文档分配的稳定 ID；0 表示该控件并非来自设计文档。
	 *
	 * 该值用于生成代码、动态 UI 加载和名称变更后的可靠索引，不参与
	 * 控件所有权或可访问性运行时 ID 分配。
	 */
	int DesignId = 0;
	using ChildCollection = ControlCollection;
	/**
	 * @brief 可观察的拥有型子控件集合。
	 *
	 * 直接 insert/erase/move/swap 会同步 Parent、ParentForm、样式、布局与
	 * 可访问性。erase/clear 只分离对象，不销毁；销毁请使用 DeleteControl。
	 */
	ChildCollection Children;
	/** @brief 同级控件的绘制/命中层级；数值越大越靠上。 */
	int ZIndex = 0;
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
	/** @brief 当前控件作为 ForegroundControl 时的前景层绘制；默认沿用完整控件绘制。 */
	virtual void UpdateForeground() { Update(); }
	/** @brief 同步控件持有的原生渲染/窗口资源；普通控件无需处理。 */
	virtual void SyncNativeSurface() {}
	/**
	 * @brief 在控件局部坐标系中开始渲染（设置平移变换 + 裁剪矩形）。
	 * 结束时必须调用对应的 EndRender()。
	 * clipW/clipH 默认使用 GetActualSizeDip()；可显式传入（如需扩展裁剪区域）。
	 */
	void BeginRender();
	void BeginRender(float clipW, float clipH);
	/**
	 * @brief 设置一个宿主拥有的最终绘制装饰器。
	 *
	 * 回调在控件局部变换/裁剪内、焦点和验证装饰之前执行。
	 * 回调应直接使用传入的 D2DGraphics，不得再调用 BeginRender/EndRender。
	 * 设置空回调可清除装饰器。此状态不参与布局或序列化。
	 */
	void SetRenderDecorator(
		std::function<void(Control&, D2DGraphics&)> decorator);
	bool HasRenderDecorator() const noexcept
	{
		return static_cast<bool>(_renderDecorator);
	}
	/** @brief 结束局部坐标渲染，恢复之前的变换状态。 */
	void EndRender();
	/** @brief 使控件可视区域失效，并请求窗口在下一次绘制中刷新它。 */
	virtual void InvalidateVisual();
	/** @brief 当前控件是否处于活动动画中。 */
	virtual bool IsAnimationRunning() { return false; }
	/** @brief 动画帧间隔（毫秒）。 */
	virtual UINT GetAnimationIntervalMs() { return 16; }
	/**
	 * @brief 获取动画导致的额外无效区域。
	 * @param outRect 输出需要重绘的区域。
	 * @return true 表示 outRect 有效。
	 */
	virtual bool GetAnimatedInvalidRect(D2D1_RECT_F& outRect) { (void)outRect; return false; }
	PROPERTY(class Font*, Font);
	GET(class Font*, Font);
	SET(class Font*, Font);
	READONLY_PROPERTY(BindingCollection&, DataBindings);
	GET(BindingCollection&, DataBindings);
	PROPERTY(bool, ShowValidationBorder);
	GET(bool, ShowValidationBorder);
	SET(bool, ShowValidationBorder);
	PROPERTY(bool, ShowValidationToolTip);
	GET(bool, ShowValidationToolTip);
	SET(bool, ShowValidationToolTip);
	PROPERTY(float, ValidationBorderThickness);
	GET(float, ValidationBorderThickness);
	SET(float, ValidationBorderThickness);
	PROPERTY(float, ValidationCornerRadius);
	GET(float, ValidationCornerRadius);
	SET(float, ValidationCornerRadius);
	PROPERTY(float, ValidationToolTipMaxWidth);
	GET(float, ValidationToolTipMaxWidth);
	SET(float, ValidationToolTipMaxWidth);
	/** Whether this control participates in Form Tab/Shift+Tab navigation. */
	PROPERTY(bool, IsTabStop);
	GET(bool, IsTabStop);
	SET(bool, IsTabStop);
	/** Stable order within a Form; ties preserve tree/insertion order. */
	PROPERTY(int, TabIndex);
	GET(int, TabIndex);
	SET(int, TabIndex);
	/** Explicit access key (normally one character). Empty derives from '&' in Text. */
	PROPERTY(std::wstring, AccessKey);
	GET(std::wstring, AccessKey);
	SET(std::wstring, AccessKey);
	PROPERTY(std::wstring, AccessibleName);
	GET(std::wstring, AccessibleName);
	SET(std::wstring, AccessibleName);
	PROPERTY(std::wstring, AccessibleDescription);
	GET(std::wstring, AccessibleDescription);
	SET(std::wstring, AccessibleDescription);
	PROPERTY(std::wstring, AccessibleHelpText);
	GET(std::wstring, AccessibleHelpText);
	SET(std::wstring, AccessibleHelpText);
	PROPERTY(std::wstring, AutomationId);
	GET(std::wstring, AutomationId);
	SET(std::wstring, AutomationId);
	PROPERTY(::AccessibleRole, AccessibleRole);
	GET(::AccessibleRole, AccessibleRole);
	SET(::AccessibleRole, AccessibleRole);
	PROPERTY(bool, ShowFocusVisual);
	GET(bool, ShowFocusVisual);
	SET(bool, ShowFocusVisual);
	PROPERTY(D2D1_COLOR_F, FocusVisualColor);
	GET(D2D1_COLOR_F, FocusVisualColor);
	SET(D2D1_COLOR_F, FocusVisualColor);
	PROPERTY(float, FocusVisualThickness);
	GET(float, FocusVisualThickness);
	SET(float, FocusVisualThickness);
	std::vector<BindingValidationResult> GetValidationResults() const;
	bool HasValidationIssues() const;
	bool HasValidationErrors() const;
	bool TryGetValidationSeverity(BindingValidationSeverity& severity) const;
	std::wstring GetValidationSummary(size_t maxIssues = 0) const;
	std::wstring GetEffectiveAccessibleName() const;
	/** Text rendered by mnemonic-aware controls (single '&' markers removed). */
	std::wstring GetDisplayText() const;
	std::wstring GetEffectiveAccessibleDescription() const;
	std::wstring GetEffectiveKeyboardShortcut() const;
	wchar_t GetEffectiveAccessKey() const;
	::AccessibleRole GetEffectiveAccessibleRole() const;
	/** Stable per-process id used by native accessibility fragment providers. */
	uint32_t GetAccessibilityRuntimeId() const noexcept
	{
		return _accessibilityRuntimeId;
	}
	AccessibilitySnapshot GetAccessibilitySnapshot() const;
	/** Returns true when this control type is intended to receive keyboard focus. */
	virtual bool IsKeyboardFocusable() const;
	virtual bool IsAccessibilityReadOnly() const { return false; }
	/** Applies visibility/enabled/TabStop and ancestor state to IsKeyboardFocusable(). */
	bool CanReceiveKeyboardFocus() const;
	/** Moves its owning Form's logical keyboard focus to this control. */
	bool Focus();
	/** Performs the control's primary action; overridden by actionable controls. */
	virtual bool Invoke();
	/** Returns false when Windows requests reduced client-area motion. */
	bool AreSystemAnimationsEnabled() const;
	/** Returns zero when reduced motion is active, otherwise the configured duration. */
	UINT EffectiveAnimationDuration(UINT configuredDurationMs) const;
	bool ShouldShowValidationToolTip() const;
	BindingValidationChangedEvent OnValidationStateChanged;
	const BindingPropertyMetadata* FindPropertyMetadata(
		const std::wstring& propertyName);
	bool TryGetPropertyValue(
		const std::wstring& propertyName,
		BindingValue& out);
	bool TryGetPropertyValue(
		const std::wstring& propertyName,
		ControlPropertyValueSource source,
		BindingValue& out);
	bool TrySetPropertyValue(
		const std::wstring& propertyName,
		const BindingValue& value);
	bool TrySetPropertyValue(
		const std::wstring& propertyName,
		const BindingValue& value,
		ControlPropertyValueSource source);
	bool TrySetCurrentPropertyValue(
		const std::wstring& propertyName,
		const BindingValue& value);
	bool ClearPropertyValue(
		const std::wstring& propertyName,
		ControlPropertyValueSource source);
	size_t ClearPropertyValues(ControlPropertyValueSource source);
	bool HasPropertyValue(
		const std::wstring& propertyName,
		ControlPropertyValueSource source);
	ControlPropertyValueSource GetPropertyValueSource(
		const std::wstring& propertyName);
	bool ResetPropertyValue(const std::wstring& propertyName);
	/** Updates the value beneath Theme/Style/Binding/Local without creating a source. */
	bool TrySetPropertyBaseValue(
		const std::wstring& propertyName,
		const BindingValue& value);
	bool IsPropertyValueDefault(const std::wstring& propertyName);
	const std::wstring& GetStyleId() const noexcept { return _styleId; }
	void SetStyleId(std::wstring value);
	std::span<const std::wstring> GetStyleClasses() const noexcept
	{
		return std::span<const std::wstring>{
			_styleClasses.data(), _styleClasses.size() };
	}
	bool HasStyleClass(const std::wstring& value) const;
	bool AddStyleClass(std::wstring value);
	bool RemoveStyleClass(const std::wstring& value);
	void ClearStyleClasses();
	ControlStyleState GetStyleState() const noexcept { return _styleState; }
	ControlStyleState GetEffectiveStyleState() const noexcept;
	void SetStyleState(ControlStyleState state, bool enabled = true);
	std::shared_ptr<const ControlStyleSheet> GetThemeStyleSheet() const noexcept
	{
		return _themeStyleSheet;
	}
	std::shared_ptr<const ControlStyleSheet> GetStyleSheet() const noexcept
	{
		return _styleSheet;
	}
	bool SetThemeStyleSheet(
		std::shared_ptr<const ControlStyleSheet> value,
		bool recursive = true);
	bool SetStyleSheet(
		std::shared_ptr<const ControlStyleSheet> value,
		bool recursive = true);
	bool RefreshStyleValues(bool recursive = true);
	/** @brief Registers metadata owned by this runtime control type. */
	virtual void EnsureBindingPropertiesRegistered();
	// 显式设置是否由 Control 释放 Font（默认：通过属性 Font 设置时视为“拥有”）
	void SetFontEx(class Font* value, bool takeOwnership);
	READONLY_PROPERTY(int, Count);
	GET(int, Count);
	Control* operator[](int index);
	Control* GetChild(int index);
	/** @brief 在当前控件及其后代中按设计文档稳定 ID 查找。 */
	Control* FindControlByDesignId(int designId) noexcept;
	const Control* FindControlByDesignId(int designId) const noexcept;
	virtual std::span<Control* const> GetLayoutChildrenView() noexcept
	{
		return std::span<Control* const>{ Children.data(), Children.size() };
	}
	std::vector<Control*> GetChildrenInZOrder() const;
	std::vector<Control*> GetChildrenInReverseZOrder() const;

	template<typename T>
	T AddControl(T control) {
		return InsertControl(static_cast<int>(Children.size()), control);
	}

	/** @brief 在指定位置挂载控件；成功后当前容器接管所有权。 */
	template<typename T>
	T InsertControl(int index, T control) {
		if (!control)
			throw std::invalid_argument("不能添加空控件");
		if (index < 0 || static_cast<size_t>(index) > Children.size())
			throw std::out_of_range("子控件索引超出范围");
		for(auto& child : this->Children) {
			if (child == control) {
				return control;
			}
		}
		for (Control* ancestor = this; ancestor; ancestor = ancestor->Parent) {
			if (ancestor == control)
				throw std::logic_error("不能将控件添加到自身或其后代");
		}
		if (control->_isFormRoot
			|| control->Parent
			|| (control->ParentForm && control->ParentForm != this->ParentForm)) {
			throw std::logic_error("该控件已属于其他容器");
		}
		this->Children.insert(this->Children.begin() + index, control);
		return control;
	}

	/**
	 * @brief 接收一个尚未挂载的控件；挂载成功后由当前容器接管所有权。
	 * 校验或挂载抛异常时，unique_ptr 仍负责释放对象。
	 */
	template<typename T>
	T* AddOwned(std::unique_ptr<T> control)
	{
		return InsertOwned(
			static_cast<int>(Children.size()), std::move(control));
	}

	/** @brief 在指定位置接收一个未挂载控件并接管所有权。 */
	template<typename T>
	T* InsertOwned(int index, std::unique_ptr<T> control)
	{
		static_assert(std::is_base_of_v<Control, T>, "T must derive from Control");
		if (!control)
			throw std::invalid_argument("不能添加空控件");
		T* raw = control.release();
		try
		{
			this->InsertControl(index, raw);
		}
		catch (...)
		{
			// A public collection observer may throw after attachment. In that
			// case the container already owns the object and must retain it.
			if (raw->Parent != this)
				control.reset(raw);
			throw;
		}
		return raw;
	}

	/** @brief 原位构造并添加控件，避免界面构建代码直接使用裸 new。 */
	template<typename T, typename... Args>
	T* Add(Args&&... args)
	{
		static_assert(std::is_base_of_v<Control, T>, "T must derive from Control");
		return AddOwned(std::make_unique<T>(std::forward<Args>(args)...));
	}
	
	// 递归设置所有子控件的ParentForm
	static void SetChildrenParentForm(Control* control, Form* form) {
		if (!control) return;
		if (control->ParentForm != form)
			control->_hasLastInvalidatedClientRect = false;
		control->ParentForm = form;
		control->_layoutState.InvalidateMeasure();
		for (size_t i = 0; i < control->Children.size(); i++) {
			SetChildrenParentForm(control->Children[i], form);
		}
	}
	/**
	 * @brief 从当前容器分离一个直接子控件，并将所有权交还给调用方。
	 * @return 成功时返回拥有该控件的 unique_ptr；child 不是直接子控件时返回空。
	 */
	std::unique_ptr<Control> DetachControl(Control* child);
	/** @brief 按索引分离直接子控件；索引无效时返回空。 */
	std::unique_ptr<Control> DetachControlAt(int index);

	/** @brief 类型安全的 DetachControl 重载。 */
	template<typename T>
	std::unique_ptr<T> DetachControl(T* child)
	{
		static_assert(std::is_base_of_v<Control, T>, "T must derive from Control");
		auto detached = DetachControl(static_cast<Control*>(child));
		return std::unique_ptr<T>(static_cast<T*>(detached.release()));
	}

	/** @brief 从当前容器移除并销毁一个直接子控件。 */
	bool DeleteControl(Control* child);
	bool DeleteControlAt(int index);
	/** @brief 移除并销毁全部直接子控件。 */
	void ClearControls();
	int IndexOfControl(const Control* child) const noexcept;
	bool ContainsControl(const Control* child) const noexcept
	{
		return IndexOfControl(child) >= 0;
	}

	/**
	 * @brief 兼容旧代码：只解除挂载，不销毁；分离后的所有权仍由调用方负责。
	 * 新代码需要转移所有权时应使用 DetachControl()。
	 */
	void RemoveControl(Control* child);
	/** @brief 兼容整数投影；底层布局、绘制与命中请使用 GetAbsoluteLocationDip()。 */
	READONLY_PROPERTY(POINT, AbsLocation);
	GET(POINT, AbsLocation);
	/** @brief 兼容整数投影；底层布局请使用 GetActualLocationDip()。 */
	READONLY_PROPERTY(POINT, ActualLocation);
	GET(POINT, ActualLocation);
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
	/**
	 * @brief Public float-DIP sizing declarations used by Measure.
	 *
	 * Width/Height remain integer compatibility projections. LayoutWidth and
	 * LayoutHeight preserve either an exact fractional DIP value or Auto.
	 */
	__declspec(property(get = GetLayoutWidth, put = SetLayoutWidth)) cui::layout::Length LayoutWidth;
	cui::layout::Length GetLayoutWidth() const noexcept { return _layoutStyle.width; }
	void SetLayoutWidth(cui::layout::Length value);
	__declspec(property(get = GetLayoutHeight, put = SetLayoutHeight)) cui::layout::Length LayoutHeight;
	cui::layout::Length GetLayoutHeight() const noexcept { return _layoutStyle.height; }
	void SetLayoutHeight(cui::layout::Length value);
	void SetAutoSize(bool width = true, bool height = true);
	bool IsWidthAuto() const noexcept { return _layoutStyle.width.IsAuto(); }
	bool IsHeightAuto() const noexcept { return _layoutStyle.height.IsAuto(); }
	READONLY_PROPERTY(float, Right);
	GET(float, Right);
	READONLY_PROPERTY(float, Bottom);
	GET(float, Bottom);
	PROPERTY(std::wstring, Text);
	GET(std::wstring, Text);
	SET(std::wstring, Text);
	PROPERTY(D2D1_COLOR_F, BorderColor);
	GET(D2D1_COLOR_F, BorderColor);
	SET(D2D1_COLOR_F, BorderColor);
	PROPERTY(D2D1_COLOR_F, BackColor);
	GET(D2D1_COLOR_F, BackColor);
	SET(D2D1_COLOR_F, BackColor);
	PROPERTY(D2D1_COLOR_F, ForeColor);
	GET(D2D1_COLOR_F, ForeColor);
	SET(D2D1_COLOR_F, ForeColor);
	PROPERTY(std::shared_ptr<BitmapSource>, Image);
	GET(std::shared_ptr<BitmapSource>, Image);
	SET(std::shared_ptr<BitmapSource>, Image);
	// 设置图片源（设备无关），并清空设备相关缓存。
	void SetImageEx(std::shared_ptr<BitmapSource> value);
	// 获取/重建与当前渲染设备匹配的 Bitmap 缓存。
	ID2D1Bitmap* EnsureImageCache();

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
	virtual cui::core::Size MeasureCore(const cui::core::Constraints& available);
	/** @brief Legacy integer measurement extension point. */
	virtual SIZE MeasureCore(SIZE availableSize);
	/**
	 * @brief Cached measure entry point used by layout containers.
	 *
	 * Existing custom controls keep overriding MeasureCore; the wrapper owns
	 * dirty-state and constraint caching without changing that extension API.
	 */
	cui::core::Size Measure(const cui::core::Constraints& available);
	SIZE Measure(SIZE availableSize);
	/** @brief 暂停布局传播和子树重绘调度；支持嵌套调用。 */
	void SuspendLayout();
	/**
	 * @brief 恢复一层布局暂停。
	 * @param performLayout true 时在最外层恢复后立即执行本容器的待处理布局。
	 */
	void ResumeLayout(bool performLayout = true);
	/** @brief 当前控件是否仍处于布局暂停状态。 */
	bool IsLayoutSuspended() const { return _layoutDeferral.IsSuspended(); }
	const cui::layout::LayoutStyle& GetSpecifiedLayout() const { return _layoutStyle; }
	const cui::layout::LayoutState& GetComputedLayout() const { return _layoutState; }
	cui::core::Size GetDesiredSizeDip() const noexcept { return _layoutState.desiredSize; }
	cui::core::Point GetActualLocationDip() const;
	virtual cui::core::Size GetActualSizeDip();
	cui::core::Point GetAbsoluteLocationDip() const;
	cui::core::Rect GetAbsoluteRectDip();
	/**
	 * @brief 布局应用：由布局引擎/父容器设置最终浮点 DIP 矩形。
	 * POINT/SIZE 重载仅用于兼容旧容器和手工布局代码。
	 */
	void ApplyLayout(cui::core::Rect finalRect);
	void ApplyLayout(POINT location, SIZE size);
	/**
	 * @brief 仅更新运行时实际坐标，不改写 Margin/LayoutBase。
	 * 供 TabControl/Menu/ToolBar 等手动管理子控件位置的容器使用。
	 */
	void SetRuntimeLocation(cui::core::Point value);
	void SetRuntimeLocation(POINT value);

	CursorKind Cursor = CursorKind::Arrow;
	/**
	 * @brief 根据命中区域返回光标类型。
	 * @param localX 相对于控件客户区的 X。
	 * @param localY 相对于控件客户区的 Y。
	 */
	virtual CursorKind QueryCursor(int localX, int localY) { (void)localX; (void)localY; return this->Cursor; }
	virtual bool TryGetSystemCursorId(UINT32& outId) const { (void)outId; return false; }
	virtual bool ContainsPoint(int localX, int localY)
	{
		auto actualSize = this->GetActualSizeDip();
		return localX >= 0 && localY >= 0
			&& (float)localX <= actualSize.width
			&& (float)localY <= actualSize.height;
	}
	virtual bool ContainsForegroundPoint(int localX, int localY) { return ContainsPoint(localX, localY); }
	virtual bool RenderNormalWhenForeground() const { return false; }
	virtual bool HitTestChildren() const { return true; }
	virtual bool ShouldHitTestChildrenAt(int localX, int localY) const { (void)localX; (void)localY; return this->HitTestChildren(); }
	virtual POINT GetChildrenRenderOffset() const { return POINT{ 0, 0 }; }
	virtual bool ClipsChildren() { return false; }
	virtual D2D1_RECT_F GetChildrenClipRect()
	{
		auto actualSize = this->GetActualSizeDip();
		return D2D1_RECT_F{
			0.0f, 0.0f, actualSize.width, actualSize.height };
	}
	virtual bool HandlesMouseWheel() const { return false; }
	virtual bool CanHandleMouseWheel(int delta, int localX, int localY) { (void)delta; (void)localX; (void)localY; return false; }
	virtual bool HandlesNavigationKey(WPARAM key) const { (void)key; return false; }
	virtual bool AutoCloseOnOutsideClick() const { return false; }
	virtual bool AutoCloseOnFormFocusLoss() const { return false; }
	virtual void ClosePopup() {}
	void SetDCompSceneOrderOverride(int order) { _hasDCompSceneOrderOverride = true; _dcompSceneOrderOverride = order; }
	void ClearDCompSceneOrderOverride() { _hasDCompSceneOrderOverride = false; }
	bool TryGetDCompSceneOrderOverride(int& order) const { if (!_hasDCompSceneOrderOverride) return false; order = _dcompSceneOrderOverride; return true; }
	virtual void RenderImage(float cornerRadius = 0.0f);
	/** @brief 兼容整数尺寸与旧派生扩展点；底层消费端请使用 GetActualSizeDip()。 */
	virtual SIZE ActualSize();
	void SetTextInternal(std::wstring text);
	bool IsSelected() const;
	/**
	 * @brief 处理窗口消息并分发到控件。
	 * @return true 表示已处理。
	 */
	virtual bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY);
};

template<typename TOwner, typename TValue>
const BindingPropertyMetadata* BindingPropertyRegistry::Register(
	std::wstring name,
	std::function<TValue(TOwner&)> getter,
	std::function<void(TOwner&, const TValue&)> setter,
	std::function<EventConnection(TOwner&, BindingPropertyMetadata::ChangeHandler, DataSourceUpdateMode)> subscriber,
	ControlPropertyOptions<TOwner, TValue> options)
{
	static_assert(std::is_base_of_v<Control, TOwner>,
		"Bindable property owners must derive from Control.");

	constexpr BindingValueKind valueKind = []
	{
		using Value = std::remove_cv_t<TValue>;
		if constexpr (std::is_same_v<Value, bool>) return BindingValueKind::Bool;
		else if constexpr (std::is_same_v<Value, int>) return BindingValueKind::Int;
		else if constexpr (std::is_same_v<Value, long long>) return BindingValueKind::Int64;
		else if constexpr (std::is_same_v<Value, float>) return BindingValueKind::Float;
		else if constexpr (std::is_same_v<Value, double>) return BindingValueKind::Double;
		else if constexpr (std::is_same_v<Value, std::wstring>) return BindingValueKind::String;
		else return BindingValueKind::Object;
	}();

	auto matcher = [](const Control& target)
	{
		return dynamic_cast<const TOwner*>(&target) != nullptr;
	};

	BindingPropertyMetadata::ValueConverter valueConverter = [](
		const BindingValue& value,
		BindingValue& out)
	{
		using Value = std::remove_cv_t<TValue>;
		if constexpr (std::is_default_constructible_v<Value>)
		{
			Value converted{};
			if (!value.TryGet(converted)) return false;
			out = BindingValue(std::move(converted));
			return true;
		}
		else
		{
			if (value.Kind() != BindingValueKind::Object) return false;
			const auto* exact = std::any_cast<Value>(
				&std::get<std::any>(value.Raw()));
			if (!exact) return false;
			out = BindingValue(*exact);
			return true;
		}
	};

	BindingPropertyMetadata::Getter untypedGetter;
	if (getter)
	{
		untypedGetter = [getter = std::move(getter)](Control& target, BindingValue& out)
		{
			auto* owner = dynamic_cast<TOwner*>(&target);
			if (!owner) return false;
			out = BindingValue(getter(*owner));
			return true;
		};
	}

	BindingPropertyMetadata::Setter untypedSetter;
	if (setter)
	{
		untypedSetter = [setter = std::move(setter)](Control& target, const BindingValue& value)
		{
			auto* owner = dynamic_cast<TOwner*>(&target);
			if (!owner) return false;
			if constexpr (std::is_default_constructible_v<TValue>)
			{
				TValue converted{};
				if (!value.TryGet(converted)) return false;
				setter(*owner, converted);
				return true;
			}
			else
			{
				if (value.Kind() != BindingValueKind::Object) return false;
				const auto* exact = std::any_cast<TValue>(
					&std::get<std::any>(value.Raw()));
				if (!exact) return false;
				setter(*owner, *exact);
				return true;
			}
		};
	}

	BindingPropertyMetadata::Subscriber untypedSubscriber;
	if (subscriber)
	{
		untypedSubscriber = [subscriber = std::move(subscriber)](
			Control& target,
			BindingPropertyMetadata::ChangeHandler handler,
			DataSourceUpdateMode updateMode)
		{
			auto* owner = dynamic_cast<TOwner*>(&target);
			return owner
				? subscriber(*owner, std::move(handler), updateMode)
				: EventConnection{};
		};
	}

	BindingPropertyMetadata::Coercer untypedCoercer;
	if (options.Coerce)
	{
		untypedCoercer = [coerce = std::move(options.Coerce)](
			Control& target,
			const BindingValue& value,
			BindingValue& out)
		{
			auto* owner = dynamic_cast<TOwner*>(&target);
			if (!owner) return false;
			std::optional<TValue> proposed;
			if constexpr (std::is_default_constructible_v<TValue>)
			{
				TValue converted{};
				if (value.TryGet(converted)) proposed = std::move(converted);
			}
			else if (value.Kind() == BindingValueKind::Object)
			{
				if (const auto* exact = std::any_cast<TValue>(
					&std::get<std::any>(value.Raw())))
					proposed = *exact;
			}
			if (!proposed.has_value()) return false;
			auto coerced = coerce(*owner, *proposed);
			if (!coerced.has_value()) return false;
			out = BindingValue(std::move(*coerced));
			return true;
		};
	}

	BindingPropertyMetadata::Comparer typedComparer =
		[equals = std::move(options.Equals)](
		const BindingValue& left,
		const BindingValue& right) -> bool
	{
		if constexpr (std::is_default_constructible_v<TValue>)
		{
			TValue leftValue{};
			TValue rightValue{};
			if (!left.TryGet(leftValue) || !right.TryGet(rightValue)) return false;
			if (equals) return equals(leftValue, rightValue);
			if constexpr (requires(const TValue& a, const TValue& b)
				{ { a == b } -> std::convertible_to<bool>; })
			{
				return leftValue == rightValue;
			}
			else
			{
				return false;
			}
		}
		else
		{
			if (left.Kind() != BindingValueKind::Object
				|| right.Kind() != BindingValueKind::Object)
				return false;
			const auto* leftValue = std::any_cast<TValue>(
				&std::get<std::any>(left.Raw()));
			const auto* rightValue = std::any_cast<TValue>(
				&std::get<std::any>(right.Raw()));
			if (!leftValue || !rightValue) return false;
			if (equals) return equals(*leftValue, *rightValue);
			if constexpr (requires(const TValue& a, const TValue& b)
				{ { a == b } -> std::convertible_to<bool>; })
				return *leftValue == *rightValue;
			return false;
		}
	};

	BindingPropertyMetadata::Changed untypedChanged;
	if (options.Changed)
	{
		untypedChanged = [changed = std::move(options.Changed)](
			Control& target,
			const BindingValue& oldValue,
			const BindingValue& newValue)
		{
			auto* owner = dynamic_cast<TOwner*>(&target);
			if (!owner) return;
			if constexpr (std::is_default_constructible_v<TValue>)
			{
				TValue typedOld{};
				TValue typedNew{};
				if (oldValue.TryGet(typedOld) && newValue.TryGet(typedNew))
					changed(*owner, typedOld, typedNew);
			}
			else if (oldValue.Kind() == BindingValueKind::Object
				&& newValue.Kind() == BindingValueKind::Object)
			{
				const auto* typedOld = std::any_cast<TValue>(
					&std::get<std::any>(oldValue.Raw()));
				const auto* typedNew = std::any_cast<TValue>(
					&std::get<std::any>(newValue.Raw()));
				if (typedOld && typedNew) changed(*owner, *typedOld, *typedNew);
			}
		};
	}

	BindingValue defaultValue;
	const bool hasDefaultValue = options.DefaultValue.has_value();
	if (hasDefaultValue)
		defaultValue = BindingValue(std::move(*options.DefaultValue));

	return Register(BindingPropertyMetadata(
		std::move(name),
		valueKind,
		std::type_index(typeid(TValue)),
		std::type_index(typeid(TOwner)),
		std::move(matcher),
		std::move(valueConverter),
		std::move(untypedCoercer),
		std::move(typedComparer),
		std::move(untypedGetter),
		std::move(untypedSetter),
		std::move(untypedSubscriber),
		std::move(untypedChanged),
		std::move(defaultValue),
		hasDefaultValue,
		options.Flags,
		std::move(options.Design)));
}

template<typename TValue>
bool Control::SetPropertyField(
	const std::wstring& propertyName,
	TValue& storage,
	TValue value)
{
	auto* metadata = BindingPropertyRegistry::Find(*this, propertyName);
	if (!metadata)
	{
		storage = std::move(value);
		return true;
	}
	BindingValue proposed(std::move(value));
	if (_applyingPropertyMetadata != metadata
		&& (HasStoredPropertyValues(*metadata)
			|| HasControlPropertyFlag(
				metadata->Flags(), ControlPropertyFlags::TracksLocalValue)))
	{
		return TrySetPropertyValue(
			propertyName, proposed, ControlPropertyValueSource::Local);
	}

	BindingValue converted;
	if (!metadata->TryConvert(proposed, converted))
		return false;
	BindingValue effective;
	if (!metadata->TryCoerce(*this, converted, effective))
		return false;
	TValue typed = storage;
	if (!effective.TryGet(typed)) return false;

	BindingValue oldValue(storage);
	BindingValue newValue(typed);
	if (metadata->ValuesEqual(oldValue, newValue)) return false;
	storage = std::move(typed);
	ApplyPropertyMetadataChange(*metadata, oldValue, newValue);
	return true;
}

template<typename TValue>
bool Control::SetCurrentPropertyField(
	const std::wstring& propertyName,
	TValue& storage,
	TValue value)
{
	auto* metadata = BindingPropertyRegistry::Find(*this, propertyName);
	if (!metadata)
	{
		storage = std::move(value);
		return true;
	}
	return TrySetCurrentPropertyValue(
		propertyName, BindingValue(std::move(value)));
}
