#pragma once

/**
 * @file DesignerTypes.h
 * @brief 设计器数据模型与辅助类型定义（控件树、属性、布局等）。
 */
#include "../CUI/include/Control.h"
#include "DesignerPropertyValue.h"
#include <functional>
#include <memory>
#include <string>
#include <map>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

struct DesignerStyleSheet;
namespace DesignerModel
{
	struct DesignObjectResourceDictionary;
}

// 设计器中控件的元数据
struct ControlMetadata
{
	UIClass Type;
	std::wstring Name;
	std::wstring DisplayName;
	SIZE DefaultSize;
	bool IsContainer;
};

enum class DesignerBindingRelativeSource : unsigned char
{
	None,
	Self,
	TemplatedParent,
	FindAncestor
};

struct DesignerDataBinding
{
	std::wstring SourceProperty;
	BindingMode Mode = BindingMode::Default;
	DataSourceUpdateMode UpdateMode = DataSourceUpdateMode::Default;
	/** Named runtime converter resolved through BindingValueConverterRegistry. */
	std::wstring Converter;
	/** Empty selects DataContext; otherwise resolves an x:Name in the local namescope. */
	std::wstring ElementName;
	DesignerBindingRelativeSource RelativeSource =
		DesignerBindingRelativeSource::None;
	/** Canonical XAML type token used by RelativeSource FindAncestor. */
	std::wstring AncestorType;
	/** Expanded namespace URI; empty identifies a built-in CUI control type. */
	std::wstring AncestorTypeNamespace;
	int AncestorLevel = 1;
	/** Target-typed value used when the source path/value cannot be produced. */
	std::optional<DesignerStyleValue> FallbackValue;
	/** Target-typed replacement used when the source explicitly returns Empty. */
	std::optional<DesignerStyleValue> TargetNullValue;
	/** Scalar value supplied to parameter-aware converters. */
	std::optional<DesignerStyleValue> ConverterParameter;
	/** Single-value composite format applied after Convert and before target coercion. */
	std::optional<std::wstring> StringFormat;
	/** Non-empty turns this expression into a WPF-style MultiBinding. */
	std::vector<DesignerDataBinding> ChildBindings;
	bool IsMultiBinding() const noexcept { return !ChildBindings.empty(); }

	bool operator==(const DesignerDataBinding&) const = default;
};

enum class DesignerBindingPreviewStatus : unsigned char
{
	Detached,
	Active,
	Error
};

struct DesignerBindingPreviewState
{
	DesignerBindingPreviewStatus Status = DesignerBindingPreviewStatus::Detached;
	std::wstring Message;

	bool operator==(const DesignerBindingPreviewState&) const = default;
};

/** Exact contract carried by an Object-valued DataContext property. */
enum class DesignerDataObjectKind : unsigned char
{
	/** Opaque object payload; it is neither traversable nor a collection. */
	Opaque,
	/** Nested IBindingSource record that may own dotted child paths. */
	BindingSource,
	/** Observable IBindingList consumed by ItemsSource. */
	BindingList
};

/** One discoverable property path on the form's design-time data context. */
struct DesignerDataContextProperty
{
	std::wstring Path;
	BindingValueKind ValueKind = BindingValueKind::Empty;
	bool CanRead = true;
	bool CanWrite = true;
	bool CanObserve = true;
	DesignerDataObjectKind ObjectKind = DesignerDataObjectKind::Opaque;
	/** Required for BindingList; identifies the DataTemplate item contract. */
	std::wstring ItemType;
	/** Optional for BindingSource; identifies the single-object DataTemplate contract. */
	std::wstring DataType;

	bool operator==(const DesignerDataContextProperty&) const = default;
};

/**
 * Flat dotted paths keep the persisted format simple while still describing a
 * nested property tree (for example Profile and Profile.DisplayName).
 */
using DesignerDataContextSchema = std::vector<DesignerDataContextProperty>;

/** Portable XAML identity of a component declared entirely by a document. */
struct DesignerComponentType
{
	std::wstring XamlPrefix;
	std::wstring XamlName;
	std::wstring XamlNamespace;

	bool Empty() const noexcept
	{
		return XamlName.empty() && XamlNamespace.empty();
	}
	std::wstring RegistryKey() const
	{
		return XamlNamespace + L"|" + XamlName;
	}
	bool operator==(const DesignerComponentType&) const = default;
};

/** One stable string value exposed by a declarative enum property. */
struct DesignerComponentPropertyChoice
{
	std::wstring Value;
	std::wstring DisplayName;

	bool operator==(const DesignerComponentPropertyChoice&) const = default;
};

enum class DesignerComponentContentCardinality : unsigned char
{
	Single,
	Multiple
};

/** One visual child slot exposed by a declarative component. */
struct DesignerComponentContentPropertyDescriptor
{
	std::wstring Name;
	std::wstring DisplayName;
	DesignerComponentContentCardinality Cardinality =
		DesignerComponentContentCardinality::Single;
	bool IsDefault = false;

	bool operator==(const DesignerComponentContentPropertyDescriptor&) const = default;
};

/** Serializable property schema owned by a document component definition. */
struct DesignerComponentPropertyDescriptor
{
	std::wstring Name;
	std::wstring DisplayName;
	std::wstring Category = L"Component";
	int CategoryOrder = 500;
	int Order = 0;
	DesignerStyleValue DefaultValue;
	/** Optional resource-backed default, resolved before the contract is installed. */
	std::wstring DefaultResourceKey;
	ControlPropertyEditorKind Editor = ControlPropertyEditorKind::Auto;
	std::vector<DesignerComponentPropertyChoice> Choices;
	std::optional<double> Minimum;
	std::optional<double> Maximum;
	std::optional<double> Step;
	ControlPropertyFlags Flags = ControlPropertyFlags::None;
	DataSourceUpdateMode DefaultUpdateMode =
		DataSourceUpdateMode::OnPropertyChanged;
	bool IsReadOnly = false;

	bool operator==(const DesignerComponentPropertyDescriptor&) const = default;
};

/** Stable design-time grouping shared by built-in and component events. */
enum class DesignerEventCategory : unsigned char
{
	Action,
	Value,
	Mouse,
	Keyboard,
	Focus,
	DragDrop,
	Layout,
	Lifecycle,
	Data,
	Navigation,
	Media,
	Diagnostics,
	Other
};

/** Serializable payload contract for one XAML-declared component event. */
enum class DesignerComponentEventPayload : unsigned char
{
	None,
	Bool,
	Int,
	Int64,
	Float,
	Double,
	String,
};

struct DesignerComponentEventDescriptor
{
	std::wstring Name;
	std::wstring DisplayName;
	DesignerEventCategory Category = DesignerEventCategory::Other;
	DesignerComponentEventPayload Payload = DesignerComponentEventPayload::None;
	DeclarativeEventRoutingStrategy RoutingStrategy =
		DeclarativeEventRoutingStrategy::Direct;
	int Order = 0;
	bool IsDefault = false;

	bool operator==(const DesignerComponentEventDescriptor&) const = default;
};

/** One component-host property predicate used by a VisualState. */
struct DesignerVisualStateCondition
{
	std::wstring PropertyName;
	DesignerStyleValue Value;

	bool operator==(const DesignerVisualStateCondition&) const = default;
};

/** One VisualState value targeting the host or a template-local named part. */
struct DesignerVisualStateSetter
{
	std::wstring TargetName;
	std::wstring PropertyName;
	bool UsesResource = false;
	std::wstring ResourceKey;
	DesignerStyleValue Literal;

	bool operator==(const DesignerVisualStateSetter&) const = default;
};

enum class DesignerAnimationKind : unsigned char
{
	Double,
	Color,
	Thickness,
	Point,
	Vector,
	Rect,
	Size,
	Matrix,
	Object,
};

enum class DesignerEasingKind : unsigned char
{
	Linear,
	Quadratic,
	Cubic,
	Sine,
};

enum class DesignerEasingMode : unsigned char
{
	EaseIn,
	EaseOut,
	EaseInOut,
};

enum class DesignerKeyFrameKind : unsigned char
{
	Discrete,
	Linear,
	Easing,
	Spline,
};

enum class DesignerRepeatBehaviorKind : unsigned char
{
	Count,
	Duration,
	Forever,
};

enum class DesignerTimelineFillBehavior : unsigned char
{
	HoldEnd,
	Stop,
};

struct DesignerAnimationKeyFrame
{
	DesignerKeyFrameKind Kind = DesignerKeyFrameKind::Linear;
	unsigned long long KeyTimeMilliseconds = 0;
	bool UsesResource = false;
	std::wstring ResourceKey;
	DesignerStyleValue Value;
	DesignerEasingKind Easing = DesignerEasingKind::Linear;
	DesignerEasingMode EasingMode = DesignerEasingMode::EaseOut;
	float KeySplineX1 = 0.0f;
	float KeySplineY1 = 0.0f;
	float KeySplineX2 = 1.0f;
	float KeySplineY2 = 1.0f;

	bool operator==(const DesignerAnimationKeyFrame&) const = default;
};

/** One finite WPF-style animation in a VisualState Storyboard. */
struct DesignerVisualStateAnimation
{
	DesignerAnimationKind Kind = DesignerAnimationKind::Double;
	std::wstring TargetName;
	std::wstring PropertyName;
	bool HasFrom = false;
	bool FromUsesResource = false;
	std::wstring FromResourceKey;
	DesignerStyleValue From;
	bool HasTo = false;
	bool ToUsesResource = false;
	std::wstring ToResourceKey;
	DesignerStyleValue To;
	bool HasBy = false;
	bool ByUsesResource = false;
	std::wstring ByResourceKey;
	DesignerStyleValue By;
	bool IsAdditive = false;
	bool IsCumulative = false;
	unsigned long long BeginTimeMilliseconds = 0;
	unsigned long long DurationMilliseconds = 0;
	DesignerRepeatBehaviorKind RepeatBehavior =
		DesignerRepeatBehaviorKind::Count;
	double RepeatCount = 1.0;
	unsigned long long RepeatDurationMilliseconds = 0;
	bool AutoReverse = false;
	DesignerTimelineFillBehavior FillBehavior =
		DesignerTimelineFillBehavior::HoldEnd;
	double SpeedRatio = 1.0;
	double AccelerationRatio = 0.0;
	double DecelerationRatio = 0.0;
	DesignerEasingKind Easing = DesignerEasingKind::Linear;
	DesignerEasingMode EasingMode = DesignerEasingMode::EaseOut;
	std::vector<DesignerAnimationKeyFrame> KeyFrames;

	bool operator==(const DesignerVisualStateAnimation&) const = default;
};

enum class DesignerStoryboardActionKind : unsigned char
{
	Begin,
	Pause,
	Resume,
	Stop,
};

struct DesignerEventTriggerAction
{
	DesignerStoryboardActionKind Kind = DesignerStoryboardActionKind::Begin;
	std::wstring StoryboardName;
	std::vector<DesignerVisualStateAnimation> Animations;

	bool operator==(const DesignerEventTriggerAction&) const = default;
};

struct DesignerEventTrigger
{
	std::wstring EventName;
	std::vector<DesignerEventTriggerAction> Actions;

	bool operator==(const DesignerEventTrigger&) const = default;
};

struct DesignerVisualState
{
	std::wstring Name;
	std::vector<DesignerVisualStateCondition> Conditions;
	std::vector<std::wstring> EventNames;
	std::vector<DesignerVisualStateSetter> Setters;
	std::vector<DesignerVisualStateAnimation> Animations;

	bool operator==(const DesignerVisualState&) const = default;
};

struct DesignerVisualTransition
{
	std::wstring FromState;
	std::wstring ToState;
	unsigned long long GeneratedDurationMilliseconds = 0;
	DesignerEasingKind GeneratedEasing = DesignerEasingKind::Linear;
	DesignerEasingMode GeneratedEasingMode = DesignerEasingMode::EaseOut;
	std::vector<DesignerVisualStateAnimation> Animations;

	bool operator==(const DesignerVisualTransition&) const = default;
};

struct DesignerVisualStateGroup
{
	std::wstring Name;
	std::vector<DesignerVisualState> States;
	std::vector<DesignerVisualTransition> Transitions;

	bool operator==(const DesignerVisualStateGroup&) const = default;
};

/** One built-in Designer creation/toolbox entry. */
struct DesignerControlDescriptor
{
	UIClass Type = UIClass::UI_Base;
	std::wstring Name;
	std::wstring DisplayName;
	SIZE DefaultSize{ 100, 30 };
	bool IsContainer = false;
	std::wstring Category;
	bool IsValid() const noexcept
	{
		if (Type == UIClass::UI_Base || Type == UIClass::UI_TabPage
			|| Name.empty() || DisplayName.empty()
			|| DefaultSize.cx <= 0 || DefaultSize.cy <= 0) return false;
		return true;
	}

	static DesignerControlDescriptor BuiltIn(
		const ControlMetadata& metadata,
		std::wstring category = {})
	{
		DesignerControlDescriptor result;
		result.Type = metadata.Type;
		result.Name = metadata.Name;
		result.DisplayName = metadata.DisplayName;
		result.DefaultSize = metadata.DefaultSize;
		result.IsContainer = metadata.IsContainer;
		result.Category = std::move(category);
		return result;
	}
};

// 设计器中的控件包装类
class DesignerControl
{
public:
	Control* ControlInstance;
	// 文档内稳定身份。重命名、重排和代码重新生成均不得改变该值。
	int StableId = 0;
	// 设计器层面的父容器：nullptr 表示直接属于窗体（画布根级）。
	// 注意：不要与 ControlInstance->Parent 混淆；后者在设计器运行时可能指向 DesignerCanvas。
	Control* DesignerParent = nullptr;
	std::wstring Name;
	UIClass Type;
	DesignerComponentType ComponentType;
	// Visual content property used when this public control is parented by a
	// declarative component. The runtime parent may instead be its presenter.
	std::wstring ComponentContentProperty;
	std::vector<DesignerComponentContentPropertyDescriptor>
		ComponentContentProperties;
	std::map<std::wstring, Control*> ComponentContentPresenters;
	std::vector<DesignerComponentEventDescriptor> ComponentEvents;
	bool IsSelected;
	// Design-time only. Prevents accidental placement/tree changes while
	// keeping the control selectable, editable, copyable and removable.
	bool IsLocked = false;

	// 事件绑定：key 为事件成员名（如 OnMouseClick/OnTextChanged），value 为类成员函数名
	// 仅用于设计期保存/加载与导出代码生成；运行时不自动绑定。
	std::map<std::wstring, std::wstring> EventHandlers;
	// 数据绑定：key 为目标属性名，value 描述统一数据上下文上的源路径与模式。
	std::map<std::wstring, DesignerDataBinding> DataBindings;
	// Transient preview state. It is deliberately excluded from persistence.
	std::map<std::wstring, DesignerBindingPreviewState> BindingPreviewStates;
	std::map<std::wstring, std::optional<BindingValue>> BindingPreviewLocalValues;
	// Metadata-backed properties not represented by legacy Props/Extra fields.
	std::map<std::wstring, DesignerStyleValue> MetadataProperties;
	// Canonical property name -> authored StaticResource key. The tracked value
	// above remains the current effective value used by the preview, while this
	// map preserves the XAML expression across design-time edits and saves.
	std::map<std::wstring, std::wstring> MetadataPropertyResourceKeys;
	// Canonical property name -> authored DynamicResource key. Kept separate
	// from StaticResource so persistence and runtime reevaluation remain exact.
	std::map<std::wstring, std::wstring> MetadataPropertyDynamicResourceKeys;
	// Authored <Control.Resources> dictionary. It belongs to this logical node
	// and is intentionally not flattened into the document resource sheet.
	std::shared_ptr<DesignerStyleSheet> LocalResources;
	// XAML-owned DataTemplate/ComponentDefinition declarations in this scope.
	// Kept as model data because they have no standalone native control object.
	std::shared_ptr<DesignerModel::DesignObjectResourceDictionary>
		LocalObjectResources;

	// 设计期附加数据（不一定映射到运行时属性）。
	// 例如：MediaPlayer 的媒体源路径等。
	std::unordered_map<std::wstring, std::wstring> DesignStrings;
	
	// 用于调整大小的句柄位置
	enum class ResizeHandle
	{
		None,
		TopLeft,
		Top,
		TopRight,
		Right,
		BottomRight,
		Bottom,
		BottomLeft,
		Left
	};
	
	DesignerControl(Control* control, std::wstring name, UIClass type,
		Control* designerParent = nullptr, int stableId = 0)
		: ControlInstance(control), StableId(stableId),
		  DesignerParent(designerParent), Name(name), Type(type), IsSelected(false)
	{
	}
	
	ResizeHandle HitTestHandle(POINT pt, int handleSize = 6);
	std::vector<RECT> GetHandleRects(int handleSize = 6);
};

// 可用控件类型列表
class ControlRegistry
{
public:
	static std::vector<ControlMetadata> GetAvailableControls()
	{
		return {
			{ UIClass::UI_Label, L"Label", L"标签", {100, 20}, false },
			{ UIClass::UI_LinkLabel, L"LinkLabel", L"链接标签", {120, 20}, false },
			{ UIClass::UI_Button, L"Button", L"按钮", {120, 30}, true },
			{ UIClass::UI_TextBox, L"TextBox", L"文本框", {200, 25}, false },
			{ UIClass::UI_PasswordBox, L"PasswordBox", L"密码框", {200, 25}, false },
			{ UIClass::UI_RichTextBox, L"RichTextBox", L"富文本框", {300, 160}, false },
			{ UIClass::UI_DateTimePicker, L"DateTimePicker", L"日期时间选择器", {200, 28}, false },
			{ UIClass::UI_NumericUpDown, L"NumericUpDown", L"数值步进框", {140, 30}, false },
			{ UIClass::UI_Panel, L"Panel", L"面板", {200, 200}, true },
			{ UIClass::UI_GroupBox, L"GroupBox", L"分组框", {240, 180}, true },
			{ UIClass::UI_Expander, L"Expander", L"折叠面板", {260, 160}, true },
			{ UIClass::UI_ScrollView, L"ScrollView", L"滚动视图", {240, 200}, true },
			{ UIClass::UI_StackPanel, L"StackPanel", L"堆叠面板", {200, 200}, true },
			{ UIClass::UI_GridPanel, L"GridPanel", L"网格面板", {200, 200}, true },
			{ UIClass::UI_DockPanel, L"DockPanel", L"停靠面板", {200, 200}, true },
			{ UIClass::UI_WrapPanel, L"WrapPanel", L"换行面板", {200, 200}, true },
			{ UIClass::UI_RelativePanel, L"RelativePanel", L"相对面板", {200, 200}, true },
			{ UIClass::UI_SplitContainer, L"SplitContainer", L"分割容器", {360, 220}, true },
			{ UIClass::UI_CheckBox, L"CheckBox", L"复选框", {100, 20}, false },
			{ UIClass::UI_RadioBox, L"RadioBox", L"单选框", {100, 20}, false },
			{ UIClass::UI_ComboBox, L"ComboBox", L"下拉框", {150, 25}, false },
			{ UIClass::UI_ListView, L"ListView", L"列表视图", {320, 220}, false },
			{ UIClass::UI_ListBox, L"ListBox", L"列表框", {220, 180}, false },
			{ UIClass::UI_ItemsControl, L"ItemsControl", L"模板化列表", {260, 220}, false },
			{ UIClass::UI_ContentPresenter, L"ContentPresenter", L"内容呈现器", {260, 120}, false },
			{ UIClass::UI_ContentControl, L"ContentControl", L"内容控件", {260, 140}, true },
			{ UIClass::UI_GridView, L"GridView", L"表格", {360, 200}, false },
			{ UIClass::UI_PropertyGrid, L"PropertyGrid", L"属性表", {300, 320}, false },
			{ UIClass::UI_ChartView, L"ChartView", L"交互图表", {420, 260}, false },
			{ UIClass::UI_ReportView, L"ReportView", L"报表视图", {480, 300}, false },
			{ UIClass::UI_KpiCard, L"KpiCard", L"指标卡片", {220, 132}, false },
			{ UIClass::UI_FilterBar, L"FilterBar", L"筛选条", {640, 48}, false },
			{ UIClass::UI_TreeView, L"TreeView", L"树", {220, 220}, false },
			{ UIClass::UI_ProgressBar, L"ProgressBar", L"进度条", {200, 20}, false },
			{ UIClass::UI_LoadingRing, L"LoadingRing", L"环形加载器", {48, 48}, false },
			{ UIClass::UI_ProgressRing, L"ProgressRing", L"环形进度环", {72, 72}, false },
			{ UIClass::UI_Slider, L"Slider", L"滑块", {200, 30}, false },
			{ UIClass::UI_PictureBox, L"PictureBox", L"图片框", {150, 150}, false },
			{ UIClass::UI_Switch, L"Switch", L"开关", {60, 30}, false },
			{ UIClass::UI_TabControl, L"TabControl", L"选项卡", {360, 240}, true },
			{ UIClass::UI_ToolBar, L"ToolBar", L"工具栏", {360, 34}, false },
			{ UIClass::UI_Menu, L"Menu", L"菜单", {600, 28}, false },
			{ UIClass::UI_StatusBar, L"StatusBar", L"状态栏", {600, 26}, false },
			{ UIClass::UI_ToastHost, L"ToastHost", L"通知宿主", {340, 260}, false },
			{ UIClass::UI_WebBrowser, L"WebBrowser", L"浏览器", {500, 360}, false },
			{ UIClass::UI_MediaPlayer, L"MediaPlayer", L"媒体播放器", {640, 360}, false },
			{ UIClass::UI_NativeSurface, L"NativeSurface", L"原生表面", {320, 180}, false },
			{ UIClass::UI_NavigationView, L"NavigationView", L"导航视图", {220, 360}, false },
			{ UIClass::UI_SideBar, L"SideBar", L"侧边栏", {200, 360}, false },
			{ UIClass::UI_BreadcrumbBar, L"BreadcrumbBar", L"面包屑", {320, 32}, false },
			{ UIClass::UI_CalendarView, L"CalendarView", L"日历", {280, 300}, false },
			{ UIClass::UI_DateRangePicker, L"DateRangePicker", L"日期范围", {240, 30}, false },
			{ UIClass::UI_ColorPicker, L"ColorPicker", L"颜色选择器", {180, 30}, false },
			{ UIClass::UI_PagedGridView, L"PagedGridView", L"分页表格", {520, 320}, false },
		};
	}
};
