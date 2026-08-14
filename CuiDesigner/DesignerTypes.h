#pragma once

/**
 * @file DesignerTypes.h
 * @brief 设计器数据模型与辅助类型定义（控件树、属性、布局等）。
 */
#include "../CUI/include/Control.h"
#include "DesignerPropertyValue.h"
#include <array>
#include <functional>
#include <memory>
#include <string>
#include <map>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>
#include <string_view>

struct DesignerStyleSheet;
namespace DesignerModel
{
	struct DesignObjectResourceDictionary;
	struct DesignFlowDocument;

	/** One XAML CommandBinding; handler names are resolved by native code. */
	struct DesignCommandBinding
	{
		using HandlerRoute =
			std::pair<std::wstring, const std::wstring*>;

		std::wstring Command;
		std::wstring PreviewCanExecute;
		std::wstring CanExecute;
		std::wstring PreviewExecuted;
		std::wstring Executed;

		std::array<HandlerRoute, 4> HandlerRoutes() const noexcept
		{
			return {{
				{ L"PreviewCanExecute", &PreviewCanExecute },
				{ L"CanExecute", &CanExecute },
				{ L"PreviewExecuted", &PreviewExecuted },
				{ L"Executed", &Executed }
			}};
		}
		bool operator==(const DesignCommandBinding&) const = default;
	};

	enum class DesignInputBindingKind : unsigned char
	{
		Key,
		Mouse
	};

	/** One XAML KeyBinding or MouseBinding with a canonical gesture. */
	struct DesignInputBinding
	{
		DesignInputBindingKind Kind = DesignInputBindingKind::Key;
		std::wstring Command;
		std::wstring Gesture;
		std::wstring CommandParameter;
		/** Canonical x:Name resolved to Control* only during materialization. */
		std::wstring CommandTarget;
		bool operator==(const DesignInputBinding&) const = default;
	};

	/** Stable, exact ordering for case-sensitive canonical XAML member names. */
	struct DesignPropertyNameLess
	{
		bool operator()(
			const std::wstring& left,
			const std::wstring& right) const noexcept;
	};
	/** Authored routed-event connections keyed by canonical Schema event name. */
	using DesignEventHandlerMap = std::map<
		std::wstring, std::wstring, DesignPropertyNameLess>;
}

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
	DependencyPropertyEditorKind Editor = DependencyPropertyEditorKind::Auto;
	std::vector<DesignerComponentPropertyChoice> Choices;
	std::optional<double> Minimum;
	std::optional<double> Maximum;
	std::optional<double> Step;
	DependencyPropertyFlags Flags = DependencyPropertyFlags::None;
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
	cui::core::Size DefaultSize{ 100.0f, 30.0f };
	bool IsContainer = false;
	std::wstring Category;
	bool IsValid() const noexcept
	{
		if (Type == UIClass::UI_Base || Name.empty() || DisplayName.empty()
			|| DefaultSize.width <= 0.0f || DefaultSize.height <= 0.0f)
			return false;
		return true;
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
	// 注意：不要与 ControlInstance->GetVisualParent() 混淆；后者在设计器运行时可能指向 DesignerCanvas。
	Control* DesignerParent = nullptr;
	std::wstring Name;
	// True when Name exists only as a transient graph/namescope key. Such a
	// control is deliberately absent from the public runtime lookup surface.
	bool NameIsGenerated = false;
	UIClass Type;
	// Authoritative built-in XAML type identity (for example Canvas -> Panel).
	RuntimeTypeId XamlType;
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

	// XAML 事件的瞬态物化投影。DesignDocument 是唯一作者态；Designer
	// 预览和 RuntimeDocument 可据此挂接处理函数，静态 CodeGen 不读取本缓存。
	DesignerModel::DesignEventHandlerMap EventHandlers;
	std::vector<DesignerModel::DesignCommandBinding> CommandBindings;
	std::vector<DesignerModel::DesignInputBinding> InputBindings;
	// ICommandSource object references stay as authored x:Name identities. The
	// native weak pointer is only a preview/runtime projection.
	std::wstring AuthoredCommandTarget;
	// 数据绑定：key 为目标属性名，value 描述统一数据上下文上的源路径与模式。
	std::map<std::wstring, DesignerDataBinding> DataBindings;
	// Transient preview state. It is deliberately excluded from persistence.
	std::map<std::wstring, DesignerBindingPreviewState> BindingPreviewStates;
	// Metadata-backed authored properties projected from the shared Schema.
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
	// Authoritative authored rich-text structure. The runtime FlowDocument is
	// only a preview projection and cannot preserve empty/nested wrappers when
	// the Designer rebuilds its DesignDocument.
	std::shared_ptr<const DesignerModel::DesignFlowDocument>
		AuthoredRichTextDocument;

	// 设计期附加数据（不一定映射到运行时属性）。
	// 例如：MediaElement 的媒体源路径等。
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
