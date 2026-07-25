#pragma once
#define NOMINMAX
#include "Event.h"
#include "Binding.h"
#include "XamlSchema.h"
#include "ComponentBehavior.h"
#include "ObservableCollection.h"
#include <Colors.h>
#include "ThemePalette.h"
#include "Brush.h"
#include "Geometry.h"
#include "Transform.h"
#include "FrameworkElement.h"
#include "InputReport.h"
#include "AutomationPeer.h"
#include <Font.h>
#include <Factory.h>
#include <Graphics.h>


#include <string>
#include <vector>
#include <array>
#include <algorithm>
#include <cstdint>
#include <exception>
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
 * - 资源所有权需显式：字体和派生控件资源由各自的语义所有者管理。
 * - 布局相关属性（Canvas offsets/Margin/Padding/Grid/Dock/Min/Max）由布局引擎与容器协同使用。
 */

/**
 * @brief 运行时 UI 类型标识，用于 RTTI/序列化/设计器等场景。
 */
enum class UIClass : int
{
	/** Internal wildcard/root used by routed infrastructure; never a XAML type. */
	UI_Base,
	/** Abstract WPF FrameworkElement type identity. */
	UI_FrameworkElement,
	/** Abstract WPF Control type identity and native component behavior host. */
	UI_Control,
	UI_Label,
	/** Non-authored semantic base for all push-button controls. */
	UI_ButtonBase,
	/** Non-authored semantic base that owns IsChecked and routed state events. */
	UI_ToggleButton,
	/** Non-authored semantic base that owns Minimum/Maximum/Value. */
	UI_RangeBase,
	UI_Button,
	UI_Image,
	UI_TextBox,
	UI_RichTextBox,
	UI_PasswordBox,
	UI_ComboBox,
	UI_ListView,
	UI_ListBox,
	UI_CheckBox,
	UI_RadioButton,
	UI_ProgressBar,
	UI_LoadingRing,
	UI_ProgressRing,
	UI_TreeView,
	/** Non-instantiable WPF Panel type shared by panel behavior hosts. */
	UI_Panel,
	/** Non-instantiable single-child FrameworkElement base. */
	UI_Decorator,
	/** WPF decorator that owns Background/BorderBrush/BorderThickness/Padding. */
	UI_Border,
	UI_Canvas,
	UI_GroupBox,
	UI_ScrollViewer,
	/** WPF FrameworkElement-level transient presentation with one Child slot. */
	UI_Popup,
	UI_TabItem,
	UI_TabControl,
	UI_Switch,
	UI_Menu,
	UI_MenuItem,
	UI_Separator,
	UI_ToolBar,
	UI_StatusBar,
	/** Generated content container for StatusBar records. */
	UI_StatusBarItem,
	UI_Slider,
	UI_WebBrowser,
	UI_MediaPlayer,
	UI_StackPanel,
	UI_Grid,
	UI_DockPanel,
	UI_WrapPanel,
	UI_RelativePanel,
	UI_ContextMenu,
	UI_ChartView,
	UI_CalendarView,
	UI_NumericUpDown,
	UI_Expander,
	UI_NativeSurface,
	UI_ItemsControl,
	/** Non-authored semantic base with distinct Header and Items slots. */
	UI_HeaderedItemsControl,
	/** Non-authored semantic base for selectable ItemsControl derivatives. */
	UI_Selector,
	/** Generated item container for Selector controls; not an authored tree node. */
	UI_ListBoxItem,
	/** Generated item container for ListView controls. */
	UI_ListViewItem,
	/** Generated item container for ComboBox controls; not an authored tree node. */
	UI_ComboBoxItem,
	/** Authored or generated hierarchical item container for TreeView controls. */
	UI_TreeViewItem,
	UI_ContentPresenter,
	UI_ContentControl,
	/** Non-authored semantic base with distinct Header and Content slots. */
	UI_HeaderedContentControl,
	/** ControlTemplate slot that receives an ItemsControl's generated ItemsHost. */
	UI_ItemsPresenter,
	/** Top-level FrameworkElement projected through PlatformWindowHost. */
	UI_Window,
	UI_CUSTOM
};

/** Returns the nearest framework base represented by UIClass. */
UIClass GetUIClassBase(UIClass type) noexcept;

/** True when baseType is type itself or one of its represented bases. */
bool IsUIClassAssignableFrom(UIClass baseType, UIClass type) noexcept;

/** True for runtime types that correspond to WPF Control rather than a
 *  structural FrameworkElement such as Panel, Popup or a presenter. */
bool IsControlTemplateHostClass(UIClass type) noexcept;

/** Canonical generated container type for an ItemsControl family. */
UIClass GetDefaultItemContainerType(UIClass itemsControlType) noexcept;

/** Zero for an exact match, positive for a base, and -1 when unrelated. */
int GetUIClassInheritanceDistance(UIClass baseType, UIClass type) noexcept;

/**
 * Returns whether native metadata is part of the public XAML surface for the
 * represented WPF type. This deliberately follows UIClass ancestry rather
 * than the private C++ behavior-host implementation graph.
 */
bool IsNativePropertySupportedByUIClass(
	UIClass type,
	const DependencyPropertyMetadata& metadata) noexcept;

/** Stable, value-only accessibility view used by native bridges and tests. */
struct AccessibilitySnapshot
{
	AutomationControlType ControlType = AutomationControlType::Custom;
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
	AutomationControlType ControlType = AutomationControlType::ListItem;
	AutomationPattern Patterns = AutomationPattern::None;
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
	AutomationPattern Patterns = AutomationPattern::None;
	bool CanSelectMultiple = false;
	bool IsSelectionRequired = false;
	int RowCount = 0;
	int ColumnCount = 0;
};

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
	No,
	/** Nullable/default WPF Cursor projection; native behavior chooses the shape. */
	Auto
};

/**
 * @brief 控件通用事件回调类型别名。
 *
 * sender 一般为触发事件的控件指针。
 */
typedef Event<void(class Control*, EventArgs)> EventHandler;

/**
 * Stable payload used by events declared in XAML component contracts.
 * The framework intentionally exposes a value object instead of a C++ member
 * type so dynamic documents never need reflection into application classes.
 */
struct DeclarativeEventArgs : RoutedEventArgs
{
	std::wstring Name;
	BindingValue Value;
	RuntimeTypeId OwnerType;
};

typedef Event<void(class Control*, DeclarativeEventArgs&)>
	DeclarativeEvent;

/** One host-property predicate that can activate a declarative visual state. */
struct DeclarativeVisualStateCondition
{
	std::wstring PropertyName;
	BindingValue Value;
};

/** One property value applied to the component host or a named template part. */
struct DeclarativeVisualStateSetter
{
	/** Empty targets the component host; otherwise this is a template-local name. */
	std::wstring TargetName;
	std::wstring PropertyName;
	BindingValue Value;
};

enum class DeclarativeAnimationKind : unsigned char
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

enum class DeclarativeEasingKind : unsigned char
{
	Linear,
	Quadratic,
	Cubic,
	Sine,
};

enum class DeclarativeEasingMode : unsigned char
{
	EaseIn,
	EaseOut,
	EaseInOut,
};

enum class DeclarativeKeyFrameKind : unsigned char
{
	Discrete,
	Linear,
	Easing,
	Spline,
};

/** WPF RepeatBehavior discriminator for one animation Timeline. */
enum class DeclarativeRepeatBehaviorKind : unsigned char
{
	Count,
	Duration,
	Forever,
};

enum class DeclarativeTimelineFillBehavior : unsigned char
{
	HoldEnd,
	Stop,
};

/** One explicitly timed value in a Double/Color/Thickness/Point/Rect/Size/Object key-frame animation. */
struct DeclarativeAnimationKeyFrame
{
	DeclarativeKeyFrameKind Kind = DeclarativeKeyFrameKind::Linear;
	unsigned long long KeyTimeMilliseconds = 0;
	BindingValue Value;
	DeclarativeEasingKind Easing = DeclarativeEasingKind::Linear;
	DeclarativeEasingMode EasingMode = DeclarativeEasingMode::EaseOut;
	/** Cubic Bezier control points used only by Spline key frames. */
	float KeySplineX1 = 0.0f;
	float KeySplineY1 = 0.0f;
	float KeySplineX2 = 1.0f;
	float KeySplineY2 = 1.0f;
};

/** One finite Timeline in a VisualState Storyboard. */
struct DeclarativeVisualStateAnimation
{
	DeclarativeAnimationKind Kind = DeclarativeAnimationKind::Double;
	/** Empty targets the component host; otherwise this is a template-local name. */
	std::wstring TargetName;
	std::wstring PropertyName;
	/** WPF From/To/By endpoints; missing values use the current/base animation inputs. */
	std::optional<BindingValue> From;
	std::optional<BindingValue> To;
	std::optional<BindingValue> By;
	/** Adds the timeline's local output to its default origin when WPF does so. */
	bool IsAdditive = false;
	/** Accumulates the timeline's iteration delta across repeat cycles. */
	bool IsCumulative = false;
	unsigned long long BeginTimeMilliseconds = 0;
	unsigned long long DurationMilliseconds = 0;
	DeclarativeRepeatBehaviorKind RepeatBehavior =
		DeclarativeRepeatBehaviorKind::Count;
	/** May be fractional; the WPF default is one repetition. */
	double RepeatCount = 1.0;
	unsigned long long RepeatDurationMilliseconds = 0;
	bool AutoReverse = false;
	DeclarativeTimelineFillBehavior FillBehavior =
		DeclarativeTimelineFillBehavior::HoldEnd;
	/** Timeline-local speed; BeginTime and duration-form RepeatBehavior stay in parent time. */
	double SpeedRatio = 1.0;
	/** Fractions of one simple duration; their sum may not exceed one. */
	double AccelerationRatio = 0.0;
	double DecelerationRatio = 0.0;
	DeclarativeEasingKind Easing = DeclarativeEasingKind::Linear;
	DeclarativeEasingMode EasingMode = DeclarativeEasingMode::EaseOut;
	/** Non-empty selects the corresponding UsingKeyFrames timeline. */
	std::vector<DeclarativeAnimationKeyFrame> KeyFrames;
};

/** WPF-style action executed by a component EventTrigger. */
enum class DeclarativeStoryboardActionKind : unsigned char
{
	Begin,
	Pause,
	Resume,
	Stop,
};

struct DeclarativeEventTriggerActionDefinition
{
	DeclarativeStoryboardActionKind Kind =
		DeclarativeStoryboardActionKind::Begin;
	/** BeginStoryboard x:Name, or the referenced BeginStoryboardName. */
	std::wstring StoryboardName;
	/** Populated only for BeginStoryboard. */
	std::vector<DeclarativeVisualStateAnimation> Animations;
};

/** One template-root EventTrigger driven by a component-owned routed event. */
struct DeclarativeEventTriggerDefinition
{
	std::wstring EventName;
	std::vector<DeclarativeEventTriggerActionDefinition> Actions;
};

/** One mutually exclusive state inside a declarative visual-state group. */
struct DeclarativeVisualStateDefinition
{
	std::wstring Name;
	/** All conditions must match. Empty conditions and events identify the fallback state. */
	std::vector<DeclarativeVisualStateCondition> Conditions;
	/** A matching component-owned routed event enters this state. */
	std::vector<std::wstring> EventNames;
	std::vector<DeclarativeVisualStateSetter> Setters;
	std::vector<DeclarativeVisualStateAnimation> Animations;
};

/** One WPF-style route between states in the same visual-state group. */
struct DeclarativeVisualTransitionDefinition
{
	/** Empty means any source/target state. */
	std::wstring FromState;
	std::wstring ToState;
	unsigned long long GeneratedDurationMilliseconds = 0;
	DeclarativeEasingKind GeneratedEasing = DeclarativeEasingKind::Linear;
	DeclarativeEasingMode GeneratedEasingMode = DeclarativeEasingMode::EaseOut;
	/** Explicit transition timelines override generated timelines per target. */
	std::vector<DeclarativeVisualStateAnimation> Animations;
};

struct DeclarativeVisualStateGroupDefinition
{
	std::wstring Name;
	std::vector<DeclarativeVisualStateDefinition> States;
	std::vector<DeclarativeVisualTransitionDefinition> Transitions;
};

struct DeclarativeVisualStateChangedEventArgs
{
	std::wstring GroupName;
	std::wstring OldState;
	std::wstring NewState;
};

typedef Event<void(class Control*, const DeclarativeVisualStateChangedEventArgs&)>
	DeclarativeVisualStateChangedEvent;

class ControlStyleSheet;
struct ControlStyleResolution;
class Control;
class Canvas;
class Grid;
class DockPanel;

namespace cui::framework
{
	struct TemplateAccess;
	struct PresentationAccess;
	struct InputAccess;
	struct NativeVisualStateAccess;
	struct XamlAccess;
	struct DependencyPropertyAccess;
	struct StyleAccess;
}

/**
 * @brief 所有可视控件的基类。
 *
 * 控件是轻量对象，主要职责：
 * - 保存通用几何、外观、输入和依赖属性状态
 * - 消费平台无关输入报告（ProcessInput）并触发相应事件
 * - 参与布局：提供 MeasureCore/Arrange，且通过 RequestLayout 通知父容器重排
 *
 */
class Control : public FrameworkElement
{
protected:
	friend class Visual;
	friend class Expander;
	friend class Canvas;
	friend class Grid;
	friend class DockPanel;
	friend class PresentationScene;
	friend class TextCompositionManager;
	friend class Window;
	friend struct cui::framework::InputAccess;
	friend struct cui::framework::PresentationAccess;
	friend struct cui::framework::NativeVisualStateAccess;
	friend struct cui::framework::XamlAccess;
	friend struct cui::framework::DependencyPropertyAccess;
	friend struct cui::framework::StyleAccess;
	/** Creates the semantic peer used by every native accessibility bridge. */
	virtual std::unique_ptr<AutomationPeer> OnCreateAutomationPeer();
	void SetPresentationSuppressed(bool value);
	void SetParticipatesInPresentationScene(bool value);
	/** Current frame-only DrawingContext for native control behavior. */
	D2DGraphics* GetDrawingContext() const noexcept;
	/** Draws this element only; PresentationScene owns all tree traversal. */
	virtual void OnRender();
	void BeginRender();
	void BeginRender(float clipW, float clipH);
	void EndRender();
	/**
	 * Invalidates cached geometry for this visual and every retained descendant.
	 * Containers call this when their render-only child offset/transform changes
	 * without a new layout pass.
	 */
	void InvalidateDescendantRenderGeometry() noexcept;
	D2D1_COLOR_F _backcolor = cui::theme::palette::Surface;
	D2D1_COLOR_F _forecolor = cui::theme::palette::TextPrimary;
	D2D1_COLOR_F _bordercolor = cui::theme::palette::Border;
	bool _participatesInPresentationScene = true;
	/** Native-presenter fallback only; never a XAML/application property. */
	PROPERTY(D2D1_COLOR_F, RendererBackgroundColor);
	GET(D2D1_COLOR_F, RendererBackgroundColor);
	SET(D2D1_COLOR_F, RendererBackgroundColor);
	PROPERTY(D2D1_COLOR_F, RendererForegroundColor);
	GET(D2D1_COLOR_F, RendererForegroundColor);
	SET(D2D1_COLOR_F, RendererForegroundColor);
	PROPERTY(D2D1_COLOR_F, RendererBorderColor);
	GET(D2D1_COLOR_F, RendererBorderColor);
	SET(D2D1_COLOR_F, RendererBorderColor);
	/** Effective-value projection hooks; public mutation must use the DP surface. */
	void ApplyBackgroundBrush(const cui::drawing::Brush& brush);
	void ClearBackgroundBrush();
	void ApplyForegroundBrush(const cui::drawing::Brush& brush);
	void ClearForegroundBrush();
	void ApplyBorderBrush(const cui::drawing::Brush& brush);
	void ClearBorderBrush();
	Thickness _borderThickness{};
	void InitializeControlBorderThicknessDefault(float value) noexcept
	{
		_borderThickness = Thickness(std::isfinite(value)
			? (std::max)(0.0f, value) : 0.0f);
	}
	template<typename TOwner>
	static void RegisterControlBorderThicknessMetadata(
		float defaultValue, int designOrder = 40)
	{
		DependencyPropertyOptions<TOwner, Thickness> options;
		options.DefaultValue = Thickness(defaultValue);
		options.Flags = DependencyPropertyFlags::AffectsMeasure
			| DependencyPropertyFlags::AffectsArrange
			| DependencyPropertyFlags::AffectsRender;
		options.Coerce = [](
			TOwner&, const Thickness& proposed) -> std::optional<Thickness>
		{
			const bool valid = std::isfinite(proposed.Left)
				&& proposed.Left >= 0.0f
				&& std::isfinite(proposed.Top)
				&& proposed.Top >= 0.0f
				&& std::isfinite(proposed.Right)
				&& proposed.Right >= 0.0f
				&& std::isfinite(proposed.Bottom)
				&& proposed.Bottom >= 0.0f;
			return valid ? std::optional<Thickness>{ proposed } : std::nullopt;
		};
		options.Design.Category = L"Appearance";
		options.Design.CategoryOrder = 200;
		options.Design.Order = designOrder;
		options.Design.Editor = DependencyPropertyEditorKind::Thickness;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;

		DependencyPropertyRegistry::Register<TOwner, Thickness>(
			L"BorderThickness",
			[](TOwner& target)
			{
				return static_cast<Control&>(target).GetBorderThickness();
			},
			[](TOwner& target, const Thickness& value)
			{
				static_cast<Control&>(target).SetBorderThickness(value);
			}, {}, std::move(options));
	}
	std::wstring _text = std::wstring(L"");
	/** Internal storage projection; Control itself does not own a public Text API. */
	PROPERTY(std::wstring, Text);
	GET(std::wstring, Text);
	SET(std::wstring, Text);
	// Effective WPF-style typography values. The DirectWrite object is a
	// private render projection and never participates in the public value
	// system or external ownership.
	std::wstring _fontName = L"Arial";
	double _fontSize = 14.0;
	std::unique_ptr<Font> _renderFont;
	std::unique_ptr<Font> _systemScaledFont;
	float _systemScaledFontSourceSize = 0.0f;
	float _systemScaledFontFactor = 1.0f;
	D2D1_RECT_F _caretBlinkRect{ 0,0,0,0 };
	bool _caretBlinkRectValid = false;
	int _caretBlinkSelectionStart = 0;
	int _caretBlinkSelectionEnd = 0;
	bool _caretBlinkFocused = false;
	ULONGLONG _caretBlinkResetTick = 0;
	bool _validationHasError = false;
	std::vector<BindingValidationResult> _validationErrors;
	std::wstring _automationName;
	std::wstring _automationFullDescription;
	std::wstring _automationHelpText;
	std::wstring _automationId;
	uint32_t _accessibilityRuntimeId = 0;
	mutable std::unique_ptr<AutomationPeer> _automationPeer;
	std::unique_ptr<IDeclarativeComponentBehavior>
		_declarativeComponentBehavior;
	std::unordered_map<std::wstring, Control*> _templateNameScope;
	Control* _controlTemplateRoot = nullptr;
	std::vector<std::pair<std::wstring, Control*>>
		_declarativeContentPresenters;
	struct DeclarativeVisualStateRuntime;
	std::unique_ptr<DeclarativeVisualStateRuntime> _declarativeVisualStates;
	bool _dispatchingComponentBehaviorInput = false;
	std::optional<cui::drawing::Brush> _backgroundBrush;
	std::optional<cui::drawing::Brush> _foregroundBrush;
	std::optional<cui::drawing::Brush> _borderBrush;
	BindingValue _tag;
	CursorKind _cursor = CursorKind::Auto;
	// Authored/local IsEnabled value.  It is deliberately not exposed as a
	// writable field: every mutation must flow through SetLocalEnabled so the
	// effective routed subtree is republished atomically.
	bool _localEnabled = true;
	// WPF-style command sources contribute a coercion predicate to IsEnabled.
	// The authored/local value remains intact while command availability
	// changes, so a later requery never overwrites XAML, style or binding state.
	bool _commandCanExecute = true;
	bool _hasCommandCanExecute = false;

	friend class BindingCollection;
	friend class Binding;
	friend class FocusManager;
	friend class DependencyPropertyMetadata;
	friend class DependencyPropertyRegistry;
	friend class DeclarativeTypeDescriptor;
	friend class IDeclarativeComponentBehavior;
	const DependencyPropertyMetadata* FindDeclarativePropertyMetadata(
		const std::wstring& propertyName) const;
	std::vector<const DependencyPropertyMetadata*>
		GetDeclarativePropertyMetadata() const;
	bool SupportsNativeProperty(
		const DependencyPropertyMetadata& metadata) const override;
	bool TryGetDeclarativePropertyBacking(
		const DeclarativeTypeDescriptor& owner,
		std::size_t slot,
		BindingValue& out) const;
	bool TrySetDeclarativePropertyBacking(
		const DeclarativeTypeDescriptor& owner,
		std::size_t slot,
		const BindingValue& value);
	void OnBindingValidationChanged(const std::wstring& targetProperty);
	void SetIsFocusedCore(bool value);
	void SetIsKeyboardFocusedCore(bool value);
	void SetIsKeyboardFocusWithinCore(bool value);
	void SetMouseOverCore(bool isMouseOver, bool isMouseDirectlyOver);
	virtual void OnIsMouseOverChanged(bool, bool) {}
	void ApplyTypographyFont();
	/** Native text realization used only by C++ measure/render implementations. */
	class Font* GetRenderFont();
	void NotifyAccessibilityStructureChanged();
	void NotifyAccessibilityStateChanged();
	void NotifyAccessibilityValueChanged();
	void NotifyAccessibilityScrollChanged();
	void NotifyAccessibilityVirtualChanged(
		uint32_t virtualId, AccessibilityChange change);
	void PublishEffectiveIsEnabledChanges(
		std::vector<std::pair<ControlWeakReference, bool>> snapshot);
	void PublishEffectiveIsVisibleChanges(
		std::vector<std::pair<ControlWeakReference, bool>> snapshot);
	void RequestArrange();
	void RefreshInheritedPropertiesRecursive();
	void RefreshInheritedPropertyValues();
	void ApplyPropertyMetadataChange(
		const DependencyPropertyMetadata& metadata,
		const BindingValue& oldValue,
		const BindingValue& newValue);
	bool ApplyEffectivePropertyValue(
		const DependencyPropertyMetadata& metadata,
		const BindingValue& value,
		DependencyPropertyValueSource source,
		bool allowReadOnly = false);
	bool TryResolveEffectivePropertyValue(
		const DependencyPropertyMetadata& metadata,
		const EffectiveValueEntry& entry,
		BindingValue& value,
		DependencyPropertyValueSource& source) const;
	bool TryEvaluateEffectivePropertyValue(
		const DependencyPropertyMetadata& metadata,
		const EffectiveValueEntry& entry,
		BindingValue& value,
		DependencyPropertyValueSource& source) const;
	bool CanAcquireBindingPropertyValue(
		const std::wstring& propertyName,
		const Binding* owner,
		DependencyPropertyValueSource source,
		DependencyPropertyExpressionKind expressionKind);
	bool TryAttachBindingPropertyExpression(
		const std::wstring& propertyName,
		const Binding* owner,
		DependencyPropertyValueSource source,
		DependencyPropertyExpressionKind expressionKind);
	bool TrySetBindingPropertyValue(
		const std::wstring& propertyName,
		const BindingValue& value,
		const Binding* owner,
		DependencyPropertyValueSource source,
		DependencyPropertyExpressionKind expressionKind);
	bool TrySetEffectiveValueEntry(
		const DependencyPropertyMetadata& metadata,
		std::optional<BindingValue> proposedValue,
		DependencyPropertyValueSource source,
		DependencyPropertyExpressionKind expressionKind,
		const Binding* owner,
		std::wstring resourceKey,
		bool allowReadOnly);
	bool TrySetPropertyValueOwned(
		const std::wstring& propertyName,
		const BindingValue& value,
		DependencyPropertyValueSource source,
		const Binding* owner,
		bool allowReadOnly = false);
	bool ClearPropertyValueOwned(
		const std::wstring& propertyName,
		DependencyPropertyValueSource source,
		const Binding* owner,
		bool allowReadOnly = false);
	bool ClearBindingPropertyValue(
		const std::wstring& propertyName,
		const Binding* owner,
		DependencyPropertyValueSource source,
		DependencyPropertyExpressionKind expressionKind);
	bool IsBindingExpressionOwner(
		const std::wstring& propertyName,
		const Binding* owner,
		DependencyPropertyValueSource source,
		DependencyPropertyExpressionKind expressionKind) const;
	void RetireBindingExpression(
		const std::wstring& propertyName,
		const Binding* owner);
	bool TrySetDynamicResourceExpressionOwned(
		const std::wstring& propertyName,
		std::wstring resourceKey,
		DependencyPropertyValueSource source);
	std::vector<std::shared_ptr<const ControlStyleSheet>>
		VisibleAuthorStyleSheets() const;
	bool RefreshStyleValuesForSource(
		DependencyPropertyValueSource source,
		const std::vector<std::shared_ptr<const ControlStyleSheet>>& sheets,
		std::vector<std::wstring>& appliedProperties);
	bool SynchronizeStyleTriggerActions(
		DependencyPropertyValueSource source,
		const std::shared_ptr<const ControlStyleSheet>& sheet,
		const ControlStyleResolution& resolution);
	void PruneStyleTriggerActions(
		DependencyPropertyValueSource source,
		const std::vector<std::shared_ptr<const ControlStyleSheet>>& sheets);
	bool TryResolveDynamicResource(
		const std::wstring& resourceKey, BindingValue& value) const;
	bool RefreshDynamicResourceValues(bool recursive);
	void RebuildStylePropertyConditionSubscription();
	void RebuildStyleDataContextSubscriptions();
	void RebuildStyleSubscriptions(bool recursive);
	void UpdateEffectiveDataContext(BindingSourceReference value);
	/** Framework/attached-behavior equivalent of SetValue(DependencyPropertyKey). */
	bool TrySetReadOnlyPropertyValue(
		const std::wstring& propertyName,
		const BindingValue& value);
	bool ClearReadOnlyPropertyValue(const std::wstring& propertyName);

	template<typename TValue>
	bool SetPropertyField(
		const std::wstring& propertyName,
		TValue& storage,
		TValue value);

	template<typename TValue>
	bool SetReadOnlyPropertyField(
		const std::wstring& propertyName,
		TValue& storage,
		TValue value);

	template<typename TValue>
	bool SetCurrentPropertyField(
		const std::wstring& propertyName,
		TValue& storage,
		TValue value);

	/** Routes Visual::ZIndex's CLR wrapper through this object's DP store. */
	bool RouteVisualZIndexSet(int value);

	/** Re-runs conversion/coercion for the property's current value source. */
	bool ReevaluatePropertyValue(const std::wstring& propertyName);

	cui::core::Size ResolveDesiredSize(
		cui::core::Size intrinsicSize,
		const cui::core::Constraints& available) const;
	virtual cui::core::Size GetRenderSizeDip();

	void UpdateCaretBlinkState(bool focused, int selectionStart, int selectionEnd, bool caretRectValid, const D2D1_RECT_F* caretRect = nullptr);
	bool IsCaretBlinkVisible() const;
	bool IsCaretBlinkAnimating() const;
	bool GetCaretBlinkInvalidRect(D2D1_RECT_F& outRect) const;
	virtual bool DefaultSelectOnLeftButtonDown() const { return _focusable; }
	virtual bool DefaultRaiseClickOnLeftButtonUp() const { return false; }
	virtual bool DefaultInvalidateVisualOnMouseDown(MouseButton button) const { (void)button; return true; }
	virtual bool DefaultInvalidateVisualOnMouseUp(MouseButton button) const { (void)button; return true; }
	virtual void BeforeDefaultMouseMove(MouseEventArgs& e) { (void)e; }
	virtual void BeforeDefaultMouseDown(MouseButton button, MouseEventArgs& e) { (void)button; (void)e; }
	virtual void BeforeDefaultMouseUp(MouseButton button, MouseEventArgs& e, bool hasMatchingPress) { (void)button; (void)e; (void)hasMatchingPress; }
	virtual void BeforeDefaultClick(MouseButton button, MouseEventArgs& e) { (void)button; (void)e; }
	virtual void AfterDefaultClick(MouseButton button, MouseEventArgs& e) { (void)button; (void)e; }
	virtual void OnComputedLayoutSizeChanged() {}
	virtual void PerformPendingLayout() {}
	/** Framework-owned visual root generated from this Control's template. */
	virtual Control* GetControlTemplateRoot() const noexcept
	{
		return _controlTemplateRoot;
	}
	virtual Control* SetControlTemplateRoot(std::unique_ptr<Control> value);
	virtual std::unique_ptr<Control> DetachVisualChildTemplateRoot();
	virtual void ConfigureControlTemplateVisual(Control& child);
	virtual void OnControlTemplatePresentationChanged() {}
	/** Allows specialized containers to reject structurally invalid child types. */
	virtual bool ValidateVisualChildCollection(
		std::span<Control* const> children,
		std::string& error) const
	{
		(void)children;
		(void)error;
		return true;
	}
	/** Called after visual-parent/presentation-host synchronization and before public notification. */
	virtual void OnVisualChildCollectionChanged(
		const CollectionChangedEventArgs& change,
		std::span<Control* const> previousChildren)
	{
		(void)change;
		(void)previousChildren;
	}
	/** Framework-only propagation of the current Window presentation source. */
	static void PropagatePresentationWindow(Control* control, Window* window);
	/** Called after this element and its visual descendants enter/leave a Window. */
	virtual void OnPresentationWindowChanged(
		Window* previousWindow, Window* currentWindow)
	{
		(void)previousWindow;
		(void)currentWindow;
	}
	/** Effective IsEnabled transition hook; callers must revalidate lifetime after it. */
	virtual void OnEffectiveIsEnabledChanged(
		bool previousValue, bool currentValue)
	{
		(void)previousValue;
		(void)currentValue;
	}
	/** Effective IsVisible transition hook; Window projects it to HWND state. */
	virtual void OnEffectiveIsVisibleChanged(
		bool previousValue, bool currentValue)
	{
		(void)previousValue;
		(void)currentValue;
	}
	void SynchronizeVisualChildCollection(const CollectionChangedEventArgs& change);
	void SetVisualParentCore(Control* value);
	void SetLogicalParentCore(Control* value);
	void SetTemplatedParentCore(Control* value);
	void RefreshInheritanceContext(bool recursive);
	void RegisterInheritanceChild(Control* child);
	void UnregisterInheritanceChild(Control* child);

	// 通知父容器（Panel 或 Window）需要重新布局
	virtual void RequestLayout();
	void BeginLayoutUpdateDeferral() noexcept;
	void EndLayoutUpdateDeferral(bool performLayout);
	class ScopedLayoutUpdate final
	{
	public:
		explicit ScopedLayoutUpdate(Control& owner, bool performLayout = true)
			: _owner(&owner), _performLayout(performLayout),
			_uncaughtOnEntry(std::uncaught_exceptions())
		{
			_owner->BeginLayoutUpdateDeferral();
		}
		ScopedLayoutUpdate(const ScopedLayoutUpdate&) = delete;
		ScopedLayoutUpdate& operator=(const ScopedLayoutUpdate&) = delete;
		~ScopedLayoutUpdate() noexcept
		{
			if (!_owner) return;
			const bool unwinding =
				std::uncaught_exceptions() > _uncaughtOnEntry;
			try
			{
				_owner->EndLayoutUpdateDeferral(
					unwinding ? false : _performLayout);
			}
			catch (...) {}
		}

	private:
		Control* _owner = nullptr;
		bool _performLayout = true;
		int _uncaughtOnEntry = 0;
	};
	void InvalidateMeasureSubtree();
	void InvalidateVisualSubtree();
	void InvalidateVisualBoundsSubtree();
	void InvalidateVisualRectCore(
		const D2D1_RECT_F& contentRect,
		bool contentChanged);
	void MarkPresentationInvalidation(
		PresentationInvalidationKind kind) noexcept;
	void InvalidatePresentationGeometrySubtree() noexcept;
	D2D1_MATRIX_3X2_F GetEffectiveDescendantRenderTransform() const;
	// 将内容区 DIP 矩形统一转换为窗口客户区物理像素，并与上次区域取并集。
	void InvalidateVisualRect(const D2D1_RECT_F& contentRect);
	void DispatchInvalidatedClientRect(const D2D1_RECT_F& clientRect);

	friend class Panel;
	friend class Window;
	friend struct cui::framework::TemplateAccess;
public:
	/** Sets a local DataContext; descendants inherit its effective source. */
	bool SetDataContext(BindingSourceReference value);
	/** Removes the local value and resumes inheritance from the logical/template context. */
	bool ClearDataContext();
	const BindingSourceReference& GetDataContext() const noexcept
	{
		return _effectiveDataContext;
	}
	/** Stable source identity used by bindings across inherited-source changes. */
	IBindingSource& DataContextSource();
	PropertyChangedEvent& DataContextChanged() noexcept { return _dataContextChanged; }
	/** Every node on a declarative routed-event route publishes the event here. */
	DeclarativeEvent OnDeclarativeEvent;
	const DeclarativeEventDefinition* FindDeclarativeEvent(
		const std::wstring& eventName) const noexcept;
	bool RaiseDeclarativeEvent(
		std::wstring eventName,
		BindingValue value = {});
	/** WPF-style overload that returns routed state, including Handled, to the raiser. */
	bool RaiseDeclarativeEvent(DeclarativeEventArgs& args);
	/** Explicit state entry used by component behavior and declarative event triggers. */
	bool GoToVisualState(
		const std::wstring& groupName,
		const std::wstring& stateName,
		std::wstring* outError = nullptr);
	/** WPF-style overload that can explicitly bypass VisualTransition objects. */
	bool GoToVisualState(
		const std::wstring& groupName,
		const std::wstring& stateName,
		bool useTransitions,
		std::wstring* outError = nullptr);
	/** Searches all groups; fails when the state name is absent or ambiguous. */
	bool GoToVisualState(
		const std::wstring& stateName,
		std::wstring* outError = nullptr);
	bool GoToVisualState(
		const std::wstring& stateName,
		bool useTransitions,
		std::wstring* outError = nullptr);
	std::wstring GetCurrentVisualState(
		const std::wstring& groupName) const;
	/** True while at least one VisualState Storyboard timeline is active. */
	bool HasActiveVisualStateAnimations() const noexcept;
	DeclarativeVisualStateChangedEvent OnVisualStateChanged;
	/** Effective WPF-style IsEnabled value; assignments update the local value. */
	PROPERTY(bool, IsEnabled);
	virtual GET(bool, IsEnabled);
	virtual SET(bool, IsEnabled);
	/** WPF AllowDrop DP; FrameworkElement metadata inherits through the tree. */
	PROPERTY(bool, AllowDrop);
	virtual GET(bool, AllowDrop);
	virtual SET(bool, AllowDrop);
	PROPERTY(::Visibility, Visibility);
	virtual GET(::Visibility, Visibility);
	virtual SET(::Visibility, Visibility);
	READONLY_PROPERTY(bool, IsVisible);
	bool GetIsVisible() const;
	bool IsCollapsed() const noexcept
	{
		return _presentationSuppressed
			|| _visibility == ::Visibility::Collapsed;
	}
	IsVisibleChangedEvent IsVisibleChanged;
	/**
	 * @brief 可观察的拥有型可视子节点集合。
	 *
	 * 直接 insert/erase/move/swap 会同步 VisualParent；普通插入同时建立
	 * LogicalParent。模板/内容基础设施必须使用显式树关系 API；
	 * erase/clear 只分离对象，不销毁，销毁请使用 DeleteVisualChild。
	 */
	/** @brief 创建基础控件。 */
	Control();
	/** @brief 虚析构：释放控件私有的渲染、输入与表达式资源。 */
	virtual ~Control();
	/** @brief 返回运行时类型标识。 */
	virtual UIClass Type();

protected:
	/** Root/runtime hook; normal descendants inherit through the tree. */
	void SetInheritedDataContext(BindingSourceReference value);
	/** Installs one immutable set of XAML-owned visual-state groups. */
	bool DefineVisualStateGroups(
		std::vector<DeclarativeVisualStateGroupDefinition> groups,
		std::wstring* outError = nullptr);
	/** Installs XAML visual states and template EventTrigger actions atomically. */
	bool DefineDeclarativeInteractions(
		std::vector<DeclarativeVisualStateGroupDefinition> groups,
		std::vector<DeclarativeEventTriggerDefinition> eventTriggers,
		std::wstring* outError = nullptr);
	/** Advances XAML-owned timelines from the Window presentation clock. */
	bool AdvanceVisualStateAnimations(unsigned long long nowMilliseconds);
	/** Retains one framework/template subscription for this object's lifetime. */
	void RetainEventConnection(EventConnection connection)
	{
		if (connection.Connected())
			_retainedEventConnections.push_back(std::move(connection));
	}
	void ClearRetainedEventConnections() noexcept
	{
		_retainedEventConnections.clear();
	}
	void SetLogicalParent(Control* value)
	{
		VerifyAccess();
		SetLogicalParentCore(value);
	}
	void SetTemplatedParent(Control* value)
	{
		VerifyAccess();
		SetTemplatedParentCore(value);
	}
	/** Derived state/layout preparation performed before retained scene rendering. */
	virtual void PreparePresentation() { PerformPendingLayout(); }
	/** Selects the drawing or native-composition path without runtime type switches. */
	virtual PresentationSurfaceKind GetPresentationSurfaceKind() const noexcept
	{
		return PresentationSurfaceKind::Drawing;
	}
	/** Host DPI changed; device-independent controls normally need no work. */
	virtual void NotifyDpiChanged(float dpiScale);
	/** Host render device is about to be discarded after device loss. */
	virtual void NotifyDeviceResourcesInvalidated() noexcept;
	/** Native fallback visual-state cache; public Style conditions observe DPs. */
	ControlStyleState GetStyleState() const noexcept { return _styleState; }
	ControlStyleState GetEffectiveStyleState() const noexcept;
	void SetStyleState(ControlStyleState state, bool enabled = true);
	/** Projects the internal press gesture into ButtonBase.IsPressed. */
	virtual void OnPressedVisualStateChanged(bool) {}
	bool SetDeclarativeComponentBehavior(
		std::unique_ptr<IDeclarativeComponentBehavior> behavior,
		const DeclarativeComponentBehaviorContext& context,
		std::wstring* outError = nullptr);
	void ClearDeclarativeComponentBehavior() noexcept;
	bool SetDeclarativeTypeDescriptor(
		std::shared_ptr<const DeclarativeTypeDescriptor> descriptor,
		std::wstring* outError = nullptr);
	bool RegisterDeclarativeTemplatePart(
		std::wstring localName,
		Control* instance);
	bool RegisterDeclarativeContentPresenter(
		std::wstring propertyName,
		Control* instance);
	void ClearDeclarativeTemplateScope();
	bool TrySetPropertyValue(
		const std::wstring& propertyName,
		const BindingValue& value,
		DependencyPropertyValueSource source);
	bool ClearPropertyValue(
		const std::wstring& propertyName,
		DependencyPropertyValueSource source);
	size_t ClearPropertyValues(DependencyPropertyValueSource source);
	/** Updates the metadata base value without creating a precedence contribution. */
	bool TrySetPropertyBaseValue(
		const std::wstring& propertyName,
		const BindingValue& value);
	bool SetDynamicResource(
		const std::wstring& propertyName,
		std::wstring resourceKey,
		DependencyPropertyValueSource source);
	bool ClearDynamicResource(
		const std::wstring& propertyName,
		DependencyPropertyValueSource source);

public:
	/**
	 * Installs one application behavior on this XAML component instance.
	 * The Control owns it and guarantees Detach before template children die.
	 */
	IDeclarativeComponentBehavior* GetDeclarativeComponentBehavior() noexcept
	{
		return _declarativeComponentBehavior.get();
	}
	const IDeclarativeComponentBehavior*
		GetDeclarativeComponentBehavior() const noexcept
	{
		return _declarativeComponentBehavior.get();
	}
	bool HasDeclarativeComponentBehavior() const noexcept
	{
		return static_cast<bool>(_declarativeComponentBehavior);
	}
	/** Device-independent WPF brush surfaces; raw colors are renderer fallbacks only. */
	PROPERTY(cui::drawing::Brush, Background);
	GET(cui::drawing::Brush, Background);
	SET(cui::drawing::Brush, Background);
	const std::optional<cui::drawing::Brush>& GetBackgroundBrush() const noexcept
	{
		return _backgroundBrush;
	}
	/** Effective Brush value used by diagnostics and non-rendering consumers. */
	cui::drawing::Brush GetComputedBackgroundBrush() const;
	ID2D1Brush* CreateBackgroundBrush(
		D2DGraphics& graphics,
		D2D1_SIZE_F bounds) const;
	PROPERTY(cui::drawing::Brush, Foreground);
	GET(cui::drawing::Brush, Foreground);
	SET(cui::drawing::Brush, Foreground);
	const std::optional<cui::drawing::Brush>& GetForegroundBrush() const noexcept
	{
		return _foregroundBrush;
	}
	cui::drawing::Brush GetComputedForegroundBrush() const;
	/** Returns an owned COM brush reference, or nullptr when no brush is set. */
	ID2D1Brush* CreateForegroundBrush(
		D2DGraphics& graphics,
		D2D1_SIZE_F bounds) const;
	PROPERTY(cui::drawing::Brush, BorderBrush);
	GET(cui::drawing::Brush, BorderBrush);
	SET(cui::drawing::Brush, BorderBrush);
	const std::optional<cui::drawing::Brush>& GetLocalBorderBrush() const noexcept
	{
		return _borderBrush;
	}
	cui::drawing::Brush GetComputedBorderBrush() const;
	ID2D1Brush* CreateBorderBrush(
		D2DGraphics& graphics,
		D2D1_SIZE_F bounds) const;
	/** Sets an additional local geometry clip that also applies to descendants. */
	void SetClip(const cui::drawing::Geometry& geometry);
	void ClearClip();
	const std::optional<cui::drawing::Geometry>& GetClip() const noexcept
	{
		return _clip;
	}
	/** Sets a device-independent transform that affects this control and its descendants. */
	void SetRenderTransform(const cui::drawing::Transform& transform);
	void ClearRenderTransform();
	const std::optional<cui::drawing::Transform>& GetRenderTransform() const noexcept
	{
		return _renderTransform;
	}
	/** Relative point in the control bounds used as the transform origin. */
	void SetRenderTransformOrigin(D2D1_POINT_2F origin);
	D2D1_POINT_2F GetRenderTransformOrigin() const noexcept
	{
		return _renderTransformOrigin;
	}
	/** Floating-DIP metadata projection used by XAML, binding and animation. */
	void SetRenderTransformOriginDip(cui::core::Point origin);
	cui::core::Point GetRenderTransformOriginDip() const noexcept
	{
		return { _renderTransformOrigin.x, _renderTransformOrigin.y };
	}
	/** @brief 使控件可视区域失效，并请求窗口在下一次绘制中刷新它。 */
	virtual void InvalidateVisual();
	/**
	 * Requests a compositor-only refresh without changing drawing content.
	 * Native surfaces and behaviors use this when their retained resource is
	 * unchanged but its presentation state must be recommitted.
	 */
	void InvalidateComposition();
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
	READONLY_PROPERTY(const std::wstring&, FontFamily);
	GET(const std::wstring&, FontFamily);
	READONLY_PROPERTY(double, FontSize);
	GET(double, FontSize);
	READONLY_PROPERTY(BindingCollection&, DataBindings);
	GET(BindingCollection&, DataBindings);
	/** WPF FrameworkElement.Tag equivalent; retains arbitrary scalar/object values. */
	PROPERTY(BindingValue, Tag);
	GET(BindingValue, Tag);
	SET(BindingValue, Tag);
	/** Whether this element can receive logical and keyboard focus. */
	PROPERTY(bool, Focusable);
	GET(bool, Focusable);
	SET(bool, Focusable);
	/** Whether this focusable element participates in Tab/Shift+Tab navigation. */
	PROPERTY(bool, IsTabStop);
	GET(bool, IsTabStop);
	SET(bool, IsTabStop);
	/** Stable order within a Window; ties preserve tree/insertion order. */
	PROPERTY(int, TabIndex);
	GET(int, TabIndex);
	SET(int, TabIndex);
	/** True while this element owns logical focus in at least one focus scope. */
	READONLY_PROPERTY(bool, IsFocused);
	GET(bool, IsFocused);
	/** True only for the Window's current keyboard-focus element. */
	READONLY_PROPERTY(bool, IsKeyboardFocused);
	GET(bool, IsKeyboardFocused);
	/** True for the keyboard-focus element and every routed ancestor. */
	READONLY_PROPERTY(bool, IsKeyboardFocusWithin);
	GET(bool, IsKeyboardFocusWithin);
	/** True for the directly hit element and every routed ancestor. */
	READONLY_PROPERTY(bool, IsMouseOver);
	GET(bool, IsMouseOver);
	/** True only for the Window's current pointer hit element. */
	READONLY_PROPERTY(bool, IsMouseDirectlyOver);
	GET(bool, IsMouseDirectlyOver);
	/** WPF FocusManager.IsFocusScope attached-property projection. */
	PROPERTY(bool, IsFocusScope);
	GET(bool, IsFocusScope);
	SET(bool, IsFocusScope);
	/** WPF KeyboardNavigation.TabNavigation attached-property projection. */
	PROPERTY(KeyboardNavigationMode, TabNavigation);
	GET(KeyboardNavigationMode, TabNavigation);
	SET(KeyboardNavigationMode, TabNavigation);
	/** WPF KeyboardNavigation.DirectionalNavigation attached-property projection. */
	PROPERTY(KeyboardNavigationMode, DirectionalNavigation);
	GET(KeyboardNavigationMode, DirectionalNavigation);
	SET(KeyboardNavigationMode, DirectionalNavigation);
	PROPERTY(std::wstring, AutomationName);
	GET(std::wstring, AutomationName);
	SET(std::wstring, AutomationName);
	PROPERTY(std::wstring, AutomationFullDescription);
	GET(std::wstring, AutomationFullDescription);
	SET(std::wstring, AutomationFullDescription);
	PROPERTY(std::wstring, AutomationHelpText);
	GET(std::wstring, AutomationHelpText);
	SET(std::wstring, AutomationHelpText);
	PROPERTY(std::wstring, AutomationId);
	GET(std::wstring, AutomationId);
	SET(std::wstring, AutomationId);
	std::vector<BindingValidationResult> GetValidationResults() const;
	bool HasValidationIssues() const;
	bool HasValidationErrors() const;
	bool TryGetValidationSeverity(BindingValidationSeverity& severity) const;
	std::wstring GetValidationSummary(size_t maxIssues = 0) const;
	std::wstring GetEffectiveAutomationName() const;
	/** Native text projection supplied by the semantic property owner. */
	virtual std::wstring GetSemanticText() const;
	/** Text rendered by WPF AccessText-aware controls ('_' markers removed). */
	std::wstring GetDisplayText() const;
	std::wstring GetEffectiveAutomationFullDescription() const;
	std::wstring GetEffectiveKeyboardShortcut() const;
	wchar_t GetEffectiveAccessKey() const;
	/** Returns the lazily-created WPF-style semantic peer for this instance. */
	AutomationPeer& GetAutomationPeer() const;
	/** Stable per-process id used by native accessibility fragment providers. */
	uint32_t GetAccessibilityRuntimeId() const noexcept
	{
		return _accessibilityRuntimeId;
	}
	AccessibilitySnapshot GetAccessibilitySnapshot() const;
	/** ToggleButton overrides this; ordinary controls never carry toggle state. */
	virtual bool IsCheckedForAccessibility() const noexcept { return false; }
	virtual bool IsAccessibilityReadOnly() const { return false; }
	/**
	 * Effective WPF-style IsEnabled value.  The local value, command-source
	 * predicate and every routed ancestor all participate without destroying
	 * the local property value.
	 */
	bool IsEffectivelyEnabled() const noexcept;
	/** Returns the authored/local IsEnabled value before ancestor/command coercion. */
	bool IsLocallyEnabled() const noexcept { return _localEnabled; }
	/** Updates the authored/local IsEnabled value and republishes subtree effects. */
	void SetLocalEnabled(bool value);
	/** Installs/updates the command-source CanExecute coercion predicate. */
	void SetCommandCanExecuteState(bool value);
	/** Removes command participation while preserving the local IsEnabled value. */
	void ClearCommandCanExecuteState();
	/** Applies Focusable, visibility, enabled and ancestor state. */
	bool CanReceiveKeyboardFocus() const;
	/** Adds IsTabStop to the effective keyboard-focus eligibility check. */
	bool CanParticipateInTabNavigation() const;
	/** Moves its owning Window's logical keyboard focus to this control. */
	bool Focus();
	/** Captures pointer delivery through the owning Window InputManager. */
	bool CaptureMouse();
	/** Releases pointer delivery when this control owns capture. */
	bool ReleaseMouseCapture();
	bool IsMouseCaptured() const;
	/** Performs the control's primary action; overridden by actionable controls. */
	virtual bool Invoke();
	/** Returns false when Windows requests reduced client-area motion. */
	bool AreSystemAnimationsEnabled() const;
	/** Returns zero when reduced motion is active, otherwise the configured duration. */
	UINT EffectiveAnimationDuration(UINT configuredDurationMs) const;
	BindingValidationChangedEvent OnValidationStateChanged;
	const DependencyPropertyMetadata* FindPropertyMetadata(
		const std::wstring& propertyName);
	bool TryGetPropertyValue(
		const std::wstring& propertyName,
		BindingValue& out);
	bool TryGetPropertyValue(
		const std::wstring& propertyName,
		DependencyPropertyValueSource source,
		BindingValue& out);
	bool TrySetPropertyValue(
		const std::wstring& propertyName,
		const BindingValue& value) override;
	bool TrySetCurrentPropertyValue(
		const std::wstring& propertyName,
		const BindingValue& value) override;
	/** WPF ClearValue semantics: removes only the Local contribution. */
	bool ClearPropertyValue(const std::wstring& propertyName);
	/** Clears all Local values and expressions from this object. */
	size_t ClearPropertyValues();
	bool HasPropertyValue(
		const std::wstring& propertyName,
		DependencyPropertyValueSource source);
	DependencyPropertyValueSource GetPropertyValueSource(
		const std::wstring& propertyName);
	DependencyPropertyExpressionKind GetPropertyExpressionKind(
		const std::wstring& propertyName,
		DependencyPropertyValueSource source =
			DependencyPropertyValueSource::Local);
	bool ResetPropertyValue(const std::wstring& propertyName);
	bool IsPropertyValueDefault(const std::wstring& propertyName);
	const std::shared_ptr<const DeclarativeTypeDescriptor>&
		GetDeclarativeTypeDescriptor() const noexcept
	{
		return _declarativeTypeDescriptor;
	}
	const RuntimeTypeId& GetDeclarativeTypeId() const noexcept
	{
		static const RuntimeTypeId empty;
		return _declarativeTypeDescriptor
			? _declarativeTypeDescriptor->TypeId() : empty;
	}
	const std::wstring& GetDeclarativeTypeNamespace() const noexcept
	{
		return GetDeclarativeTypeId().NamespaceUri;
	}
	const std::wstring& GetDeclarativeTypeName() const noexcept
	{
		return GetDeclarativeTypeId().LocalName;
	}
	/** Runtime-only equivalent of WPF GetTemplateChild for the owning component. */
	Control* FindDeclarativeTemplatePart(const std::wstring& localName) noexcept;
	const Control* FindDeclarativeTemplatePart(
		const std::wstring& localName) const noexcept;
	/** Returns the generated presenter for one declared visual content property. */
	Control* FindDeclarativeContentPresenter(
		const std::wstring& propertyName) noexcept;
	const Control* FindDeclarativeContentPresenter(
		const std::wstring& propertyName) const noexcept;
private:
	/** XAML Style/Resources lowering surface; only StyleAccess may project it. */
	const std::wstring& GetStyleResourceKey() const noexcept
	{
		return _styleResourceKey;
	}
	void SetStyleResourceKey(std::wstring value);
	std::shared_ptr<const ControlStyleSheet> GetThemeStyleSheet() const noexcept
	{
		return _themeStyleSheet;
	}
	std::shared_ptr<const ControlStyleSheet> GetStyleSheet() const noexcept
	{
		return _styleSheet;
	}
	/** Returns this control's local ResourceDictionary, not an inherited one. */
	std::shared_ptr<const ControlStyleSheet> GetResourceDictionary() const noexcept
	{
		return _resourceDictionary;
	}
	bool SetThemeStyleSheet(
		std::shared_ptr<const ControlStyleSheet> value,
		bool recursive = true);
	bool SetStyleSheet(
		std::shared_ptr<const ControlStyleSheet> value,
		bool recursive = true);
	bool SetResourceDictionary(
		std::shared_ptr<const ControlStyleSheet> value);
	bool RefreshStyleValues(bool recursive = true);

public:
	/** WPF-like lookup: self, logical ancestors, document, then theme/application. */
	bool TryFindResource(
		const std::wstring& resourceKey,
		BindingValue& value) const;
	/** Installs a WPF-like DynamicResource expression as a Local value. */
	bool SetDynamicResource(
		const std::wstring& propertyName,
		std::wstring resourceKey);
	/** Removes a Local DynamicResource expression and its cached value. */
	bool ClearDynamicResource(const std::wstring& propertyName);
	bool TryGetDynamicResourceKey(
		const std::wstring& propertyName,
		std::wstring& resourceKey,
		DependencyPropertyValueSource source =
			DependencyPropertyValueSource::Local);
	/** @brief Registers metadata owned by this runtime control type. */
	static void RegisterDependencyProperties();
	void EnsureBindingPropertiesRegistered() override { RegisterDependencyProperties(); }
	/** Number of implementation-owned visual children. This is not an authored
	 *  Items/Content collection; semantic containers expose their own API. */
	int VisualChildCount() const noexcept
	{
		return static_cast<int>(_visualChildren.size());
	}
	Control* GetVisualChild(int index) const noexcept;
	/** @brief 在当前控件及其后代中按设计文档稳定 ID 查找。 */
	Control* FindControlByDesignId(int designId) noexcept;
	const Control* FindControlByDesignId(int designId) const noexcept;
	virtual std::span<Control* const> GetLayoutChildrenView() noexcept
	{
		return std::span<Control* const>{
			_visualChildren.data(), _visualChildren.size() };
	}
	std::vector<Control*> GetVisualChildrenInZOrder() const;
	std::vector<Control*> GetVisualChildrenInReverseZOrder() const;

	template<typename T>
	T AdoptVisualChild(T control) {
		return InsertVisualChild(
			static_cast<int>(_visualChildren.size()), control);
	}

	/** @brief 在指定位置挂载控件；成功后当前容器接管所有权。 */
	template<typename T>
	T InsertVisualChild(int index, T control) {
		if (!control)
			throw std::invalid_argument("不能添加空控件");
		if (index < 0 || static_cast<size_t>(index) > _visualChildren.size())
			throw std::out_of_range("子控件索引超出范围");
		for(auto& child : this->_visualChildren) {
			if (child == control)
				throw std::logic_error("不能重复添加同一控件");
		}
		for (Control* ancestor = this; ancestor;
			ancestor = ancestor->GetVisualParent()) {
			if (ancestor == control)
				throw std::logic_error("不能将控件添加到自身或其后代");
		}
		if (control->_isWindowRoot
			|| control->_visualParent || control->_logicalParent
			|| (control->GetPresentationWindow() && control->GetPresentationWindow() != this->GetPresentationWindow())) {
			throw std::logic_error("该控件已属于其他容器");
		}
		this->_visualChildren.insert(
			this->_visualChildren.begin() + index, control);
		return control;
	}

	/** Reorders one owned visual child without exposing the backing collection. */
	bool MoveVisualChild(int oldIndex, int newIndex);

	/**
	 * @brief 接收一个尚未挂载的控件；挂载成功后由当前容器接管所有权。
	 * 校验或挂载抛异常时，unique_ptr 仍负责释放对象。
	 */
	template<typename T>
	T* AddOwned(std::unique_ptr<T> control)
	{
		return InsertOwned(
			static_cast<int>(_visualChildren.size()), std::move(control));
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
			this->InsertVisualChild(index, raw);
		}
		catch (...)
		{
			// A public collection observer may throw after attachment. In that
			// case the container already owns the object and must retain it.
			if (raw->GetVisualParent() != this)
				control.reset(raw);
			throw;
		}
		return raw;
	}

	/** Programmatic tree construction always starts from a property-neutral host. */
	template<typename T>
	T* Add()
	{
		static_assert(std::is_base_of_v<Control, T>, "T must derive from Control");
		return AddOwned(std::make_unique<T>());
	}
	
	/**
	 * @brief 从当前容器分离一个直接子控件，并将所有权交还给调用方。
	 * @return 成功时返回拥有该控件的 unique_ptr；child 不是直接子控件时返回空。
	 */
	std::unique_ptr<Control> DetachVisualChild(Control* child);
	/** @brief 按索引分离直接子控件；索引无效时返回空。 */
	std::unique_ptr<Control> DetachVisualChildAt(int index);

	/** @brief 类型安全的 DetachVisualChild 重载。 */
	template<typename T>
	std::unique_ptr<T> DetachVisualChild(T* child)
	{
		static_assert(std::is_base_of_v<Control, T>, "T must derive from Control");
		auto detached = DetachVisualChild(static_cast<Control*>(child));
		return std::unique_ptr<T>(static_cast<T*>(detached.release()));
	}

	/** @brief 从当前容器移除并销毁一个直接子控件。 */
	bool DeleteVisualChild(Control* child);
	bool DeleteVisualChildAt(int index);
	/** @brief 移除并销毁全部直接子控件。 */
	void ClearVisualChildren();
	int IndexOfVisualChild(const Control* child) const noexcept;
	bool ContainsControl(const Control* child) const noexcept
	{
		return IndexOfVisualChild(child) >= 0;
	}

private:
	/** Backing wrappers for WPF attached properties owned by Canvas. */
	PROPERTY(float, CanvasLeft);
	GET(float, CanvasLeft);
	SET(float, CanvasLeft);
	PROPERTY(float, CanvasTop);
	GET(float, CanvasTop);
	SET(float, CanvasTop);
	PROPERTY(float, CanvasRight);
	GET(float, CanvasRight);
	SET(float, CanvasRight);
	PROPERTY(float, CanvasBottom);
	GET(float, CanvasBottom);
	SET(float, CanvasBottom);

public:
	/** WPF-style specified dimensions. Auto is represented by Length::Auto(). */
	PROPERTY(cui::layout::Length, Width);
	GET(cui::layout::Length, Width);
	SET(cui::layout::Length, Width);
	PROPERTY(cui::layout::Length, Height);
	GET(cui::layout::Length, Height);
	SET(cui::layout::Length, Height);
	bool IsWidthAuto() const noexcept { return _layoutStyle.width.IsAuto(); }
	bool IsHeightAuto() const noexcept { return _layoutStyle.height.IsAuto(); }
	PROPERTY(float, MinWidth);
	GET(float, MinWidth);
	SET(float, MinWidth);
	PROPERTY(float, MinHeight);
	GET(float, MinHeight);
	SET(float, MinHeight);
	PROPERTY(float, MaxWidth);
	GET(float, MaxWidth);
	SET(float, MaxWidth);
	PROPERTY(float, MaxHeight);
	GET(float, MaxHeight);
	SET(float, MaxHeight);
	READONLY_PROPERTY(float, ActualWidth);
	GET(float, ActualWidth);
	READONLY_PROPERTY(float, ActualHeight);
	GET(float, ActualHeight);
	PROPERTY(Thickness, BorderThickness);
	GET(Thickness, BorderThickness);
	SET(Thickness, BorderThickness);
	// 布局属性访问器
	PROPERTY(Thickness, Margin);
	GET(Thickness, Margin);
	SET(Thickness, Margin);
	PROPERTY(Thickness, Padding);
	GET(Thickness, Padding);
	SET(Thickness, Padding);
	PROPERTY(::HorizontalAlignment, HorizontalAlignment);
	GET(::HorizontalAlignment, HorizontalAlignment);
	SET(::HorizontalAlignment, HorizontalAlignment);
	PROPERTY(::VerticalAlignment, VerticalAlignment);
	GET(::VerticalAlignment, VerticalAlignment);
	SET(::VerticalAlignment, VerticalAlignment);
private:
	/** Backing wrappers for attached properties owned by Grid and DockPanel. */
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

public:
	cui::core::Size GetMinSizeDip() const noexcept;
	void SetMinSizeDip(cui::core::Size value);
	cui::core::Size GetMaxSizeDip() const noexcept;
	void SetMaxSizeDip(cui::core::Size value);
	
	/**
	 * @brief 测量阶段：返回控件期望尺寸。
	 * @param availableSize 可用空间（由父布局提供）。
	 */
	virtual cui::core::Size MeasureCore(const cui::core::Constraints& available);
	/**
	 * @brief Cached measure entry point used by layout containers.
	 *
	 * Derived controls override the float-DIP MeasureCore entry point; the
	 * wrapper owns dirty-state and constraint caching.
	 */
	cui::core::Size Measure(const cui::core::Constraints& available);
	/**
	 * @brief 立即执行当前控件类型的待处理布局。
	 *
	 * 与直接调用 Panel::PerformLayout 不同，此入口会保留 GroupBox、
	 * Expander、ScrollViewer 等派生容器的专用内容区语义。
	 */
	void UpdateLayout() { PerformPendingLayout(); }
	const cui::layout::LayoutStyle& GetSpecifiedLayout() const { return _layoutStyle; }
	const cui::layout::LayoutState& GetComputedLayout() const { return _layoutState; }
	cui::core::Size GetDesiredSizeDip() const noexcept { return _layoutState.desiredSize; }
	cui::core::Point GetActualLocationDip() const;
	cui::core::Size GetActualSizeDip() const override;
	cui::core::Point GetAbsoluteLocationDip() const;
	cui::core::Rect GetAbsoluteRectDip() const;
	D2D1_RECT_F GetAbsoluteBoundsDip() const;
	/**
	 * @brief 可选的后代绘制变换，输入和输出都位于窗体内容区 DIP 坐标。
	 *
	 * 容器可借此实现只影响视图、不改写布局数据的缩放和平移。控件绘制、
	 * 脏矩形和 DComp 裁剪会统一继承该变换。
	 */
	virtual bool TryGetDescendantRenderTransform(
		D2D1_MATRIX_3X2_F& transform) const
	{
		transform = D2D1::Matrix3x2F::Identity();
		return false;
	}
	/** @brief 返回所有祖先为当前控件提供的内容区绘制变换。 */
	D2D1_MATRIX_3X2_F GetInheritedRenderTransform() const;
	/** @brief 返回当前控件局部坐标到实际内容区绘制坐标的完整变换。 */
	D2D1_MATRIX_3X2_F GetLocalToRenderTransform() const;
	/** @brief 把实际绘制坐标反变换为当前控件局部坐标；奇异矩阵返回 false。 */
	bool TryTransformRenderPointToLocal(
		D2D1_POINT_2F renderPoint,
		D2D1_POINT_2F& localPoint) const;
	/** Returns false when the point is outside this control or any ancestor Clip. */
	bool IsRenderPointInsideClip(D2D1_POINT_2F renderPoint) const;
	/** @brief 将绝对内容区矩形映射到当前控件实际的绘制空间。 */
	D2D1_RECT_F TransformAbsoluteRectToRenderSpace(
		const D2D1_RECT_F& rect) const;
	/** @brief 返回应用祖先绘制变换后的绝对内容区边界。 */
	D2D1_RECT_F GetRenderedAbsoluteRectDip();
	/** Commits the sole computed layout rectangle. */
	void Arrange(cui::core::Rect finalRect) override;

	/** WPF-style inherited pointer cursor; authored values participate in the DP pipeline. */
	PROPERTY(CursorKind, Cursor);
	GET(CursorKind, Cursor);
	SET(CursorKind, Cursor);
	/**
	 * Resolves the effective pointer cursor. Any concrete authored/style/theme/
	 * inherited value wins; only Auto delegates to the native behavior hook.
	 */
	CursorKind ResolvePointerCursor(int localX, int localY);
	/**
	 * @brief 根据命中区域返回原生行为建议的光标类型。
	 * @param localX 相对于控件客户区的 X。
	 * @param localY 相对于控件客户区的 Y。
	 */
	virtual CursorKind QueryCursor(int localX, int localY)
	{
		(void)localX;
		(void)localY;
		return CursorKind::Arrow;
	}
	virtual bool TryGetSystemCursorId(UINT32& outId) const { (void)outId; return false; }
	virtual bool ContainsPoint(int localX, int localY)
	{
		auto actualSize = this->GetActualSizeDip();
		return localX >= 0 && localY >= 0
			&& (float)localX <= actualSize.width
			&& (float)localY <= actualSize.height;
	}
	/** False for visuals presented exclusively through a separate overlay/native
	 *  surface. Their subtree is omitted from the retained main scene. */
	virtual bool ParticipatesInPresentationScene() const
	{
		return _participatesInPresentationScene;
	}
	virtual bool HitTestChildren() const { return true; }
	virtual bool ShouldHitTestChildrenAt(int localX, int localY) const { (void)localX; (void)localY; return this->HitTestChildren(); }
	/**
	 * @brief 子控件布局内容区相对当前控件的原点（DIP）。
	 *
	 * 设计器和容器布局都可用它在“容器局部坐标”与“子控件布局坐标”
	 * 之间换算。滚动等仅影响绘制的位置仍由 GetVisualChildrenRenderOffset 表达。
	 */
	virtual cui::core::Point GetVisualChildrenLayoutOriginDip() { return {}; }
	virtual cui::core::Point GetVisualChildrenRenderOffset() const { return {}; }
	virtual bool ClipsChildren() { return false; }
	virtual D2D1_RECT_F GetVisualChildrenClipRect()
	{
		auto actualSize = this->GetActualSizeDip();
		return D2D1_RECT_F{
			0.0f, 0.0f, actualSize.width, actualSize.height };
	}
	virtual bool HandlesMouseWheel() const { return false; }
	virtual bool CanHandleMouseWheel(int delta, int localX, int localY) { (void)delta; (void)localX; (void)localY; return false; }
	virtual bool HandlesNavigationKey(Key key) const { (void)key; return false; }

protected:
	void SetPresentationOrderOverride(int order) { _hasPresentationOrderOverride = true; _presentationOrderOverride = order; }
	void ClearPresentationOrderOverride() { _hasPresentationOrderOverride = false; }
	bool TryGetPresentationOrderOverride(int& order) const { if (!_hasPresentationOrderOverride) return false; order = _presentationOrderOverride; return true; }
	/** Applies normalized committed text through behavior, then control logic. */
	bool DispatchTextInput(TextCompositionEventArgs& input);
	/** Resolves behavior-provided or built-in caret geometry. */
	bool ResolveTextInputCaretRect(D2D1_RECT_F& outRect);
	/** Built-in text-client hook; native messages never reach this method. */
	virtual bool ApplyTextInput(const TextCompositionEventArgs& input)
	{
		(void)input;
		return false;
	}
	/** Returns the active caret in top-level client DIPs for IME placement. */
	virtual bool TryGetTextInputCaretRect(D2D1_RECT_F& outRect)
	{
		(void)outRect;
		return false;
	}
	virtual bool ProcessInput(const InputReport& input);
	/** Framework-only behavior dispatch after Window InputManager staging. */
	bool DispatchInput(const InputReport& input);
};

namespace cui::property_system_detail
{
	template<typename TValue>
	constexpr BindingValueKind ValueKind() noexcept
	{
		using Value = std::remove_cvref_t<TValue>;
		if constexpr (std::is_same_v<Value, BindingValue>)
			return BindingValueKind::Object;
		else if constexpr (std::is_same_v<Value, bool>)
			return BindingValueKind::Bool;
		else if constexpr (std::is_same_v<Value, int>)
			return BindingValueKind::Int;
		else if constexpr (std::is_same_v<Value, long long>)
			return BindingValueKind::Int64;
		else if constexpr (std::is_same_v<Value, float>)
			return BindingValueKind::Float;
		else if constexpr (std::is_same_v<Value, double>)
			return BindingValueKind::Double;
		else if constexpr (std::is_same_v<Value, std::wstring>)
			return BindingValueKind::String;
		else if constexpr (std::is_enum_v<Value>)
		{
			using Underlying = std::underlying_type_t<Value>;
			constexpr bool fitsInt = sizeof(Underlying) < sizeof(int)
				|| (sizeof(Underlying) == sizeof(int)
					&& std::is_signed_v<Underlying>);
			return fitsInt ? BindingValueKind::Int : BindingValueKind::Int64;
		}
		else
			return BindingValueKind::Object;
	}

	/**
	 * Native enum storage is an implementation detail. The dependency-property
	 * interchange contract exposes every enum as a canonical signed number plus
	 * its metadata Choices, so XAML tooling never needs a C++ enum whitelist.
	 */
	template<typename TValue>
	BindingValue Pack(TValue&& value)
	{
		using Value = std::remove_cvref_t<TValue>;
		if constexpr (std::is_enum_v<Value>)
		{
			if constexpr (ValueKind<Value>() == BindingValueKind::Int)
				return BindingValue(static_cast<int>(value));
			else
				return BindingValue(static_cast<long long>(value));
		}
		else
		{
			return BindingValue(std::forward<TValue>(value));
		}
	}
}

template<typename TOwner, typename TValue>
const DependencyPropertyMetadata* DependencyPropertyRegistry::Register(
	std::wstring name,
	std::function<TValue(TOwner&)> getter,
	std::function<void(TOwner&, const TValue&)> setter,
	std::function<EventConnection(TOwner&, DependencyPropertyMetadata::ChangeHandler, DataSourceUpdateMode)> subscriber,
	DependencyPropertyOptions<TOwner, TValue> options)
{
	static_assert(std::is_base_of_v<DependencyObject, TOwner>,
		"Bindable property owners must derive from DependencyObject.");

	constexpr BindingValueKind valueKind =
		cui::property_system_detail::ValueKind<TValue>();

	auto matcher = [](const DependencyObject& target)
	{
		return dynamic_cast<const TOwner*>(&target) != nullptr;
	};

	auto customValueConverter = std::move(options.Convert);
	DependencyPropertyMetadata::ValueConverter valueConverter = [
		customValueConverter = std::move(customValueConverter)](
		const BindingValue& value,
		BindingValue& out)
	{
		using Value = std::remove_cv_t<TValue>;
		if (customValueConverter)
		{
			auto converted = customValueConverter(value);
			if (!converted.has_value()) return false;
			out = cui::property_system_detail::Pack(
				std::move(*converted));
			return true;
		}
		if constexpr (std::is_same_v<Value, BindingValue>)
		{
			out = value;
			return true;
		}
		else if constexpr (std::is_default_constructible_v<Value>)
		{
			Value converted{};
			if (!value.TryGet(converted)) return false;
			out = cui::property_system_detail::Pack(std::move(converted));
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

	DependencyPropertyMetadata::Getter untypedGetter;
	if (getter)
	{
		untypedGetter = [getter = std::move(getter)](DependencyObject& target, BindingValue& out)
		{
			auto* owner = dynamic_cast<TOwner*>(&target);
			if (!owner) return false;
			if constexpr (std::is_same_v<std::remove_cv_t<TValue>, BindingValue>)
				out = getter(*owner);
			else
				out = cui::property_system_detail::Pack(getter(*owner));
			return true;
		};
	}

	DependencyPropertyMetadata::Setter untypedSetter;
	if (setter)
	{
		untypedSetter = [setter = std::move(setter)](DependencyObject& target, const BindingValue& value)
		{
			auto* owner = dynamic_cast<TOwner*>(&target);
			if (!owner) return false;
			if constexpr (std::is_same_v<std::remove_cv_t<TValue>, BindingValue>)
			{
				setter(*owner, value);
				return true;
			}
			else if constexpr (std::is_default_constructible_v<TValue>)
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

	DependencyPropertyMetadata::Subscriber untypedSubscriber;
	if (subscriber)
	{
		untypedSubscriber = [subscriber = std::move(subscriber)](
			DependencyObject& target,
			DependencyPropertyMetadata::ChangeHandler handler,
			DataSourceUpdateMode updateMode)
		{
			auto* owner = dynamic_cast<TOwner*>(&target);
			return owner
				? subscriber(*owner, std::move(handler), updateMode)
				: EventConnection{};
		};
	}
	else
	{
		// Dependency properties are observable by definition.  Native owners no
		// longer need to repeat a per-property forwarding subscriber merely to
		// participate in Binding, Trigger, or designer observation.
		auto observedName = name;
		untypedSubscriber = [observedName = std::move(observedName)](
			DependencyObject& target,
			DependencyPropertyMetadata::ChangeHandler handler,
			DataSourceUpdateMode updateMode)
		{
			if (updateMode == DataSourceUpdateMode::OnValidation)
			{
				if (auto* control = dynamic_cast<Control*>(&target))
					return control->OnLostFocus.Subscribe(
						[handler = std::move(handler)](Control*)
						{
							handler();
						});
			}
			return target.OnPropertyValueChanged.Subscribe(
				[observedName, handler = std::move(handler)](
					DependencyObject*,
					const DependencyPropertyChangedEventArgs& args)
				{
					if (args.PropertyName == observedName)
						handler();
				});
		};
	}

	DependencyPropertyMetadata::Coercer untypedCoercer;
	if (options.Coerce)
	{
		untypedCoercer = [coerce = std::move(options.Coerce)](
			DependencyObject& target,
			const BindingValue& value,
			BindingValue& out)
		{
			auto* owner = dynamic_cast<TOwner*>(&target);
			if (!owner) return false;
			std::optional<TValue> proposed;
			if constexpr (std::is_same_v<std::remove_cv_t<TValue>, BindingValue>)
				proposed = value;
			else if constexpr (std::is_default_constructible_v<TValue>)
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
			out = cui::property_system_detail::Pack(std::move(*coerced));
			return true;
		};
	}

	DependencyPropertyMetadata::Comparer typedComparer =
		[equals = std::move(options.Equals)](
		const BindingValue& left,
		const BindingValue& right) -> bool
	{
		if constexpr (std::is_same_v<std::remove_cv_t<TValue>, BindingValue>)
		{
			if (equals) return equals(left, right);
			return BindingValuesEqual(left, right);
		}
		else if constexpr (std::is_default_constructible_v<TValue>)
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

	DependencyPropertyMetadata::Changed untypedChanged;
	if (options.Changed)
	{
		untypedChanged = [changed = std::move(options.Changed)](
			DependencyObject& target,
			const BindingValue& oldValue,
			const BindingValue& newValue)
		{
			auto* owner = dynamic_cast<TOwner*>(&target);
			if (!owner) return;
			if constexpr (std::is_same_v<std::remove_cv_t<TValue>, BindingValue>)
				changed(*owner, oldValue, newValue);
			else if constexpr (std::is_default_constructible_v<TValue>)
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
	{
		defaultValue = cui::property_system_detail::Pack(
			std::move(*options.DefaultValue));
	}

	return Register(DependencyPropertyMetadata(
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
		options.IsReadOnly,
		options.DefaultUpdateMode,
		{},
		std::move(options.Design)));
}

template<typename TValue>
bool Control::SetPropertyField(
	const std::wstring& propertyName,
	TValue& storage,
	TValue value)
{
	VerifyAccess();
	auto* metadata = DependencyPropertyRegistry::Find(*this, propertyName);
	// A missing effective metadata entry means that the projected XAML/WPF
	// type does not own this property.  The private C++ behavior-host
	// inheritance graph must never turn that into an untracked side channel.
	// Native implementation state which is intentionally not a dependency
	// property is written explicitly and must not use this helper.
	if (!metadata) return false;
	BindingValue proposed =
		cui::property_system_detail::Pack(std::move(value));
	if (_applyingPropertyMetadata == metadata)
	{
		TValue typed = storage;
		if constexpr (std::is_same_v<std::remove_cv_t<TValue>, BindingValue>)
			typed = proposed;
		else if (!proposed.TryGet(typed)) return false;

		BindingValue oldValue = cui::property_system_detail::Pack(storage);
		BindingValue newValue = cui::property_system_detail::Pack(typed);
		if (metadata->ValuesEqual(oldValue, newValue)) return true;
		storage = std::move(typed);
		ApplyPropertyMetadataChange(*metadata, oldValue, newValue);
		return true;
	}
	// A public CLR-style wrapper is exactly SetValue in WPF terms.  Internal
	// behavior that must preserve an expression uses SetCurrentPropertyField;
	// metadata application is handled by the guarded branch above.
	return TrySetPropertyValue(
		propertyName, proposed, DependencyPropertyValueSource::Local);
}

template<typename TValue>
bool Control::SetCurrentPropertyField(
	const std::wstring& propertyName,
	TValue& storage,
	TValue value)
{
	VerifyAccess();
	(void)storage;
	auto* metadata = DependencyPropertyRegistry::Find(*this, propertyName);
	// SetCurrentValue has the same ownership boundary as SetValue: it may
	// preserve an existing expression, but it cannot manufacture a property
	// which the projected type does not expose.
	if (!metadata) return false;
	return TrySetCurrentPropertyValue(
		propertyName, BindingValue(std::move(value)));
}

template<typename TValue>
bool Control::SetReadOnlyPropertyField(
	const std::wstring& propertyName,
	TValue& storage,
	TValue value)
{
	VerifyAccess();
	auto* metadata = DependencyPropertyRegistry::Find(*this, propertyName);
	if (!metadata || !metadata->IsReadOnly()) return false;
	BindingValue proposed =
		cui::property_system_detail::Pack(std::move(value));
	if (_applyingPropertyMetadata == metadata)
	{
		TValue typed = storage;
		if constexpr (std::is_same_v<std::remove_cv_t<TValue>, BindingValue>)
			typed = proposed;
		else if (!proposed.TryGet(typed)) return false;

		BindingValue oldValue = cui::property_system_detail::Pack(storage);
		BindingValue newValue = cui::property_system_detail::Pack(typed);
		if (metadata->ValuesEqual(oldValue, newValue)) return true;
		storage = std::move(typed);
		ApplyPropertyMetadataChange(*metadata, oldValue, newValue);
		return true;
	}
	return TrySetReadOnlyPropertyValue(propertyName, proposed);
}
