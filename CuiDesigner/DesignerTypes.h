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

// 设计器中控件的元数据
struct ControlMetadata
{
	UIClass Type;
	std::wstring Name;
	std::wstring DisplayName;
	SIZE DefaultSize;
	bool IsContainer;
};

struct DesignerDataBinding
{
	std::wstring SourceProperty;
	BindingMode Mode = BindingMode::OneWay;
	DataSourceUpdateMode UpdateMode = DataSourceUpdateMode::OnPropertyChanged;
	/** Named runtime converter resolved through BindingValueConverterRegistry. */
	std::wstring Converter;

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

/** One discoverable property path on the form's design-time data context. */
struct DesignerDataContextProperty
{
	std::wstring Path;
	BindingValueKind ValueKind = BindingValueKind::Empty;
	bool CanRead = true;
	bool CanWrite = true;
	bool CanObserve = true;

	bool operator==(const DesignerDataContextProperty&) const = default;
};

/**
 * Flat dotted paths keep the persisted format simple while still describing a
 * nested property tree (for example Profile and Profile.DisplayName).
 */
using DesignerDataContextSchema = std::vector<DesignerDataContextProperty>;

enum class DesignerCustomControlConstructor : unsigned char
{
	Default,
	Bounds,
	TextBounds,
};

/**
 * Portable identity of a control implemented outside CUI. Type remains the
 * built-in metadata/layout base used by the Designer and headless generator;
 * this descriptor selects the real runtime/C++ type.
 */
struct DesignerCustomControlType
{
	std::wstring XamlPrefix;
	std::wstring XamlName;
	std::wstring XamlNamespace;
	std::wstring CppType;
	std::wstring Header;
	DesignerCustomControlConstructor Constructor =
		DesignerCustomControlConstructor::Bounds;

	bool Empty() const noexcept
	{
		return XamlName.empty() && XamlNamespace.empty()
			&& CppType.empty() && Header.empty();
	}
	std::wstring RegistryKey() const
	{
		return XamlNamespace + L"|" + XamlName;
	}
	bool operator==(const DesignerCustomControlType&) const = default;
};

/** Design-time schema for a property declared by an external C++ control. */
struct DesignerCustomPropertyDescriptor
{
	struct Choice
	{
		std::wstring DisplayName;
		std::wstring ValueText;

		bool operator==(const Choice&) const = default;
	};

	std::wstring Name;
	std::wstring DisplayName;
	std::wstring Category = L"Custom";
	int CategoryOrder = 500;
	int Order = 0;
	DesignerStyleValue DefaultValue;
	ControlPropertyEditorKind Editor = ControlPropertyEditorKind::Auto;
	std::vector<Choice> Choices;
	std::optional<double> Minimum;
	std::optional<double> Maximum;
	std::optional<double> Step;
	bool Bindable = true;
	bool SupportsTwoWayBinding = false;

	bool operator==(const DesignerCustomPropertyDescriptor&) const = default;
};

/** Stable design-time grouping shared by built-in and manifest events. */
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

/**
 * Safe portable event signatures. The sender variants deliberately use
 * Control* rather than a manifest-provided C++ type string; the runtime
 * registry verifies the real Event::function_type before subscribing.
 */
enum class DesignerCustomEventSignature : unsigned char
{
	None,
	Sender,
	SenderBool,
	SenderInt,
	SenderFloat,
	SenderDouble,
	SenderString,
	SenderIntInt,
	SenderIntBool,
	SenderDoubleDouble,
	SenderStringString,
};

/** Portable event contract declared by an external-control manifest. */
struct DesignerCustomEventDescriptor
{
	std::wstring Name;
	std::wstring DisplayName;
	std::string EventField;
	DesignerEventCategory Category = DesignerEventCategory::Other;
	DesignerCustomEventSignature Signature =
		DesignerCustomEventSignature::Sender;
	int Order = 0;
	bool IsDefault = false;

	bool operator==(const DesignerCustomEventDescriptor&) const = default;
};

/**
 * One Designer creation/toolbox entry. Built-ins leave CustomType and
 * PreviewFactory empty; external catalogs retain their portable type identity
 * while optionally supplying a process-local real preview factory.
 */
struct DesignerControlDescriptor
{
	UIClass Type = UIClass::UI_Base;
	std::wstring Name;
	std::wstring DisplayName;
	SIZE DefaultSize{ 100, 30 };
	bool IsContainer = false;
	std::wstring Category;
	DesignerCustomControlType CustomType;
	std::vector<DesignerCustomPropertyDescriptor> CustomProperties;
	std::vector<DesignerCustomEventDescriptor> CustomEvents;
	std::function<std::unique_ptr<Control>(int x, int y)> PreviewFactory;

	bool IsCustom() const noexcept { return !CustomType.Empty(); }
	bool IsValid() const noexcept
	{
		if (Type == UIClass::UI_Base || Type == UIClass::UI_TabPage
			|| Name.empty() || DisplayName.empty()
			|| DefaultSize.cx <= 0 || DefaultSize.cy <= 0) return false;
		if (!IsCustom()) return true;
		return !CustomType.XamlPrefix.empty()
			&& !CustomType.XamlName.empty()
			&& !CustomType.XamlNamespace.empty()
			&& !CustomType.CppType.empty()
			&& !CustomType.Header.empty();
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
	DesignerCustomControlType CustomType;
	// Process-local schema restored from the matching control catalog.
	std::vector<DesignerCustomPropertyDescriptor> CustomProperties;
	// Portable event contracts are persisted so headless codegen stays deterministic.
	std::vector<DesignerCustomEventDescriptor> CustomEvents;
	// Optional process-local sink (for example a plugin preview session).
	std::function<void(
		const std::wstring&, const DesignerStyleValue&)> PreviewPropertyChanged;
	bool IsSelected;

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
			{ UIClass::UI_Button, L"Button", L"按钮", {120, 30}, false },
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
		};
	}
};
