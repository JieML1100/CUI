#include "include/XamlRuntimeSchema.h"

#include "../CUI/include/Panel.h"
#include "../CUI/include/Canvas.h"
#include "../CUI/include/Decorator.h"
#include "../CUI/include/Border.h"
#include "../CUI/include/XamlInfrastructure.h"
#include "../CUI/include/DependencyPropertyInfrastructure.h"
#include "../CUI/include/ButtonBase.h"
#include "../CUI/include/ToggleButton.h"
#include "../CUI/include/Button.h"
#include "../CUI/include/CheckBox.h"
#include "../CUI/include/ComboBox.h"
#include "../CUI/include/Expander.h"
#include "../CUI/include/GroupBox.h"
#include "../CUI/include/Label.h"
#include "../CUI/include/LoadingRing.h"
#include "../CUI/include/NumericUpDown.h"
#include "../CUI/include/PasswordBox.h"
#include "../CUI/include/Image.h"
#include "../CUI/include/ProgressBar.h"
#include "../CUI/include/ProgressRing.h"
#include "../CUI/include/RadioButton.h"
#include "../CUI/include/RichTextBox.h"
#include "../CUI/include/ScrollViewer.h"
#include "../CUI/include/Popup.h"
#include "../CUI/include/Slider.h"
#include "../CUI/include/Switch.h"
#include "../CUI/include/TextBox.h"
#include "../CUI/include/WebBrowser.h"
#include "../CUI/include/ListView.h"
#include "../CUI/include/ListBox.h"
#include "../CUI/include/Selector.h"
#include "../CUI/include/ChartView.h"
#include "../CUI/include/TreeView.h"
#include "../CUI/include/TabControl.h"
#include "../CUI/include/ToolBar.h"
#include "../CUI/include/Menu.h"
#include "../CUI/include/ContextMenu.h"
#include "../CUI/include/StatusBar.h"
#include "../CUI/include/MediaPlayer.h"
#include "../CUI/include/NativeSurface.h"
#include "../CUI/include/ItemsControl.h"
#include "../CUI/include/ItemsPresenter.h"
#include "../CUI/include/ContentPresenter.h"
#include "../CUI/include/ContentControl.h"
#include "../CUI/include/Window.h"
#include "../CUI/include/CalendarView.h"
#include "../CUI/include/Layout/StackPanel.h"
#include "../CUI/include/Layout/Grid.h"
#include "../CUI/include/Layout/DockPanel.h"
#include "../CUI/include/Layout/WrapPanel.h"
#include "../CUI/include/Layout/RelativePanel.h"

#include <algorithm>
#include <array>
#include <cwctype>
#include <iterator>
#include <type_traits>
#include <typeindex>

namespace
{
	bool EqualName(std::wstring_view left, std::wstring_view right) noexcept
	{
		// XAML type and member names are XML/CLR identities, not
		// case-insensitive UI labels. Accept only the canonical spelling so an
		// obsolete spelling cannot survive as an implicit compatibility alias.
		return left == right;
	}

	RuntimeTypeId CuiType(std::wstring localName)
	{
		return { std::wstring(CuiRuntime::XamlRuntimeSchema::CuiNamespace),
			std::move(localName) };
	}

	CuiRuntime::BuiltInXamlTypeDescriptor BuiltInType(
		std::wstring localName,
		UIClass nativeType,
		bool focusable = false)
	{
		return {
			CuiType(std::move(localName)), nativeType, true, true, focusable
		};
	}

	CuiRuntime::BuiltInXamlTypeDescriptor AbstractType(
		std::wstring localName,
		UIClass nativeType)
	{
		auto result = BuiltInType(std::move(localName), nativeType);
		result.IsConstructible = false;
		return result;
	}

	CuiRuntime::BuiltInXamlTypeDescriptor ToolboxType(
		std::wstring localName,
		UIClass nativeType,
		bool focusable,
		std::wstring_view displayName,
		float width,
		float height,
		bool isContainer,
		std::wstring_view category)
	{
		auto result = BuiltInType(std::move(localName), nativeType, focusable);
		result.DesignerDisplayName = displayName;
		result.DesignerDefaultWidth = width;
		result.DesignerDefaultHeight = height;
		result.DesignerIsContainer = isContainer;
		result.DesignerCategory = category;
		return result;
	}

	const auto BuiltInTypeTable =
		std::to_array<CuiRuntime::BuiltInXamlTypeDescriptor>({
		AbstractType(L"FrameworkElement", UIClass::UI_FrameworkElement),
		AbstractType(L"Control", UIClass::UI_Control),
		ToolboxType(L"TextBlock", UIClass::UI_Label, false,
			L"文本块", 100, 20, false, L"基础控件"),
		ToolboxType(L"Button", UIClass::UI_Button, true,
			L"按钮", 120, 30, true, L"基础控件"),
		ToolboxType(L"Image", UIClass::UI_Image, false,
			L"图像", 150, 150, false, L"基础控件"),
		ToolboxType(L"TextBox", UIClass::UI_TextBox, true,
			L"文本框", 200, 25, false, L"输入"),
		ToolboxType(L"RichTextBox", UIClass::UI_RichTextBox, true,
			L"富文本框", 300, 160, false, L"输入"),
		ToolboxType(L"PasswordBox", UIClass::UI_PasswordBox, true,
			L"密码框", 200, 25, false, L"输入"),
		ToolboxType(L"ComboBox", UIClass::UI_ComboBox, true,
			L"下拉框", 150, 25, false, L"输入"),
		ToolboxType(L"ListView", UIClass::UI_ListView, true,
			L"列表视图", 320, 220, false, L"数据与列表"),
		ToolboxType(L"ListBox", UIClass::UI_ListBox, true,
			L"列表框", 220, 180, false, L"数据与列表"),
		ToolboxType(L"CheckBox", UIClass::UI_CheckBox, true,
			L"复选框", 100, 20, false, L"基础控件"),
		ToolboxType(L"RadioButton", UIClass::UI_RadioButton, true,
			L"单选按钮", 100, 20, false, L"基础控件"),
		ToolboxType(L"ProgressBar", UIClass::UI_ProgressBar, false,
			L"进度条", 200, 20, false, L"状态与反馈"),
		ToolboxType(L"LoadingRing", UIClass::UI_LoadingRing, false,
			L"环形加载器", 48, 48, false, L"状态与反馈"),
		ToolboxType(L"ProgressRing", UIClass::UI_ProgressRing, false,
			L"环形进度环", 72, 72, false, L"状态与反馈"),
		ToolboxType(L"TreeView", UIClass::UI_TreeView, true,
			L"树", 220, 220, false, L"数据与列表"),
		AbstractType(L"Panel", UIClass::UI_Panel),
		AbstractType(L"Decorator", UIClass::UI_Decorator),
		ToolboxType(L"Border", UIClass::UI_Border, false,
			L"边框", 240, 160, true, L"布局"),
		ToolboxType(L"Canvas", UIClass::UI_Canvas, false,
			L"画布", 200, 200, true, L"布局"),
		ToolboxType(L"GroupBox", UIClass::UI_GroupBox, false,
			L"分组框", 240, 180, true, L"布局"),
		ToolboxType(L"ScrollViewer", UIClass::UI_ScrollViewer, true,
			L"滚动查看器", 240, 200, true, L"布局"),
		ToolboxType(L"Popup", UIClass::UI_Popup, true,
			L"弹出层", 240, 200, true, L"布局"),
		BuiltInType(L"TabItem", UIClass::UI_TabItem),
		ToolboxType(L"TabControl", UIClass::UI_TabControl, true,
			L"选项卡", 360, 240, true, L"导航与外壳"),
		ToolboxType(L"Switch", UIClass::UI_Switch, true,
			L"开关", 60, 30, false, L"基础控件"),
		ToolboxType(L"Menu", UIClass::UI_Menu, false,
			L"菜单", 600, 28, false, L"导航与外壳"),
		BuiltInType(L"MenuItem", UIClass::UI_MenuItem),
		BuiltInType(L"Separator", UIClass::UI_Separator),
		ToolboxType(L"ToolBar", UIClass::UI_ToolBar, false,
			L"工具栏", 360, 34, false, L"导航与外壳"),
		ToolboxType(L"StatusBar", UIClass::UI_StatusBar, false,
			L"状态栏", 600, 26, false, L"导航与外壳"),
		BuiltInType(L"StatusBarItem", UIClass::UI_StatusBarItem),
		ToolboxType(L"Slider", UIClass::UI_Slider, true,
			L"滑块", 200, 30, false, L"输入"),
		ToolboxType(L"WebBrowser", UIClass::UI_WebBrowser, true,
			L"浏览器", 500, 360, false, L"媒体与 Web"),
		ToolboxType(L"MediaPlayer", UIClass::UI_MediaPlayer, true,
			L"媒体播放器", 640, 360, false, L"媒体与 Web"),
		ToolboxType(L"StackPanel", UIClass::UI_StackPanel, false,
			L"堆叠面板", 200, 200, true, L"布局"),
		ToolboxType(L"Grid", UIClass::UI_Grid, false,
			L"网格", 200, 200, true, L"布局"),
		ToolboxType(L"DockPanel", UIClass::UI_DockPanel, false,
			L"停靠面板", 200, 200, true, L"布局"),
		ToolboxType(L"WrapPanel", UIClass::UI_WrapPanel, false,
			L"换行面板", 200, 200, true, L"布局"),
		ToolboxType(L"RelativePanel", UIClass::UI_RelativePanel, false,
			L"相对面板", 200, 200, true, L"布局"),
		BuiltInType(L"ContextMenu", UIClass::UI_ContextMenu),
		ToolboxType(L"ChartView", UIClass::UI_ChartView, true,
			L"交互图表", 420, 260, false, L"数据与列表"),
		ToolboxType(L"CalendarView", UIClass::UI_CalendarView, true,
			L"日历", 280, 300, false, L"基础控件"),
		ToolboxType(L"NumericUpDown", UIClass::UI_NumericUpDown, true,
			L"数值步进框", 140, 30, false, L"输入"),
		ToolboxType(L"Expander", UIClass::UI_Expander, true,
			L"折叠面板", 260, 160, true, L"布局"),
		ToolboxType(L"NativeSurface", UIClass::UI_NativeSurface, true,
			L"原生表面", 320, 180, false, L"基础控件"),
		ToolboxType(L"ItemsControl", UIClass::UI_ItemsControl, false,
			L"模板化列表", 260, 220, false, L"基础控件"),
		BuiltInType(L"ListBoxItem", UIClass::UI_ListBoxItem),
		BuiltInType(L"ListViewItem", UIClass::UI_ListViewItem),
		BuiltInType(L"ComboBoxItem", UIClass::UI_ComboBoxItem),
		BuiltInType(L"TreeViewItem", UIClass::UI_TreeViewItem),
		ToolboxType(L"ContentPresenter", UIClass::UI_ContentPresenter, false,
			L"内容呈现器", 260, 120, false, L"基础控件"),
		BuiltInType(L"ItemsPresenter", UIClass::UI_ItemsPresenter),
		ToolboxType(L"ContentControl", UIClass::UI_ContentControl, false,
			L"内容控件", 260, 140, true, L"基础控件"),
		BuiltInType(L"Window", UIClass::UI_Window),
	});

	const auto AttachedProperties =
		std::to_array<CuiRuntime::XamlAttachedPropertyDescriptor>({
			{ CuiType(L"Canvas"), L"Left", L"Canvas.Left",
				BindingValueKind::Float },
			{ CuiType(L"Canvas"), L"Top", L"Canvas.Top",
				BindingValueKind::Float },
			{ CuiType(L"Canvas"), L"Right", L"Canvas.Right",
				BindingValueKind::Float },
			{ CuiType(L"Canvas"), L"Bottom", L"Canvas.Bottom",
				BindingValueKind::Float },
			{ CuiType(L"DockPanel"), L"Dock", L"DockPanel.Dock",
				BindingValueKind::Int },
			{ CuiType(L"Grid"), L"Row", L"Grid.Row",
				BindingValueKind::Int },
			{ CuiType(L"Grid"), L"Column", L"Grid.Column",
				BindingValueKind::Int },
			{ CuiType(L"Grid"), L"RowSpan", L"Grid.RowSpan",
				BindingValueKind::Int },
			{ CuiType(L"Grid"), L"ColumnSpan", L"Grid.ColumnSpan",
				BindingValueKind::Int },
			{ CuiType(L"FocusManager"), L"IsFocusScope",
				L"FocusManager.IsFocusScope", BindingValueKind::Bool },
			{ CuiType(L"KeyboardNavigation"), L"TabNavigation",
				L"KeyboardNavigation.TabNavigation", BindingValueKind::Int },
			{ CuiType(L"KeyboardNavigation"), L"DirectionalNavigation",
				L"KeyboardNavigation.DirectionalNavigation", BindingValueKind::Int },
			{ CuiType(L"AutomationProperties"), L"Name",
				L"AutomationProperties.Name", BindingValueKind::String },
			{ CuiType(L"AutomationProperties"), L"FullDescription",
				L"AutomationProperties.FullDescription", BindingValueKind::String },
			{ CuiType(L"AutomationProperties"), L"HelpText",
				L"AutomationProperties.HelpText", BindingValueKind::String },
			{ CuiType(L"AutomationProperties"), L"AutomationId",
				L"AutomationProperties.AutomationId", BindingValueKind::String },
			{ CuiType(L"Validation"), L"HasError",
				L"Validation.HasError", BindingValueKind::Bool },
			{ CuiType(L"Validation"), L"Errors",
				L"Validation.Errors", BindingValueKind::Object },
		});

	template<typename TConcrete>
	const std::vector<std::type_index>& NativeOwnerTypes()
	{
		TConcrete::RegisterDependencyProperties();
		static const std::vector<std::type_index> owners = []
		{
			std::vector<std::type_index> result;
#define CUI_SCHEMA_OWNER(type) \
			if constexpr (std::is_base_of_v<type, TConcrete>) \
				result.emplace_back(typeid(type))
			// Most-derived metadata wins when Window projects inherited
			// FrameworkElement properties onto native platform state.
			CUI_SCHEMA_OWNER(Window);
			CUI_SCHEMA_OWNER(Control);
			CUI_SCHEMA_OWNER(Panel);
			CUI_SCHEMA_OWNER(Decorator);
			CUI_SCHEMA_OWNER(Border);
			CUI_SCHEMA_OWNER(Canvas);
			CUI_SCHEMA_OWNER(ButtonBase);
			CUI_SCHEMA_OWNER(ToggleButton);
			CUI_SCHEMA_OWNER(RangeBase);
			CUI_SCHEMA_OWNER(Label);
			CUI_SCHEMA_OWNER(Button);
			CUI_SCHEMA_OWNER(CheckBox);
			CUI_SCHEMA_OWNER(RadioButton);
			CUI_SCHEMA_OWNER(Switch);
			CUI_SCHEMA_OWNER(CalendarView);
			CUI_SCHEMA_OWNER(ChartView);
			CUI_SCHEMA_OWNER(ComboBoxItem);
			CUI_SCHEMA_OWNER(ComboBox);
			CUI_SCHEMA_OWNER(ContentControl);
			CUI_SCHEMA_OWNER(ContentPresenter);
			CUI_SCHEMA_OWNER(Expander);
			CUI_SCHEMA_OWNER(GroupBox);
			CUI_SCHEMA_OWNER(HeaderedContentControl);
			CUI_SCHEMA_OWNER(HeaderedItemsControl);
			CUI_SCHEMA_OWNER(ItemContainerControl);
			CUI_SCHEMA_OWNER(ItemsControl);
			CUI_SCHEMA_OWNER(ItemsPresenter);
			CUI_SCHEMA_OWNER(DockPanel);
			CUI_SCHEMA_OWNER(StackPanel);
			CUI_SCHEMA_OWNER(Grid);
			CUI_SCHEMA_OWNER(WrapPanel);
			CUI_SCHEMA_OWNER(RelativePanel);
			CUI_SCHEMA_OWNER(ListBox);
			CUI_SCHEMA_OWNER(ListView);
			CUI_SCHEMA_OWNER(ListViewItem);
			CUI_SCHEMA_OWNER(LoadingRing);
			CUI_SCHEMA_OWNER(MediaPlayer);
			CUI_SCHEMA_OWNER(Menu);
			CUI_SCHEMA_OWNER(MenuItem);
			CUI_SCHEMA_OWNER(ContextMenu);
			CUI_SCHEMA_OWNER(NativeSurface);
			CUI_SCHEMA_OWNER(NumericUpDown);
			CUI_SCHEMA_OWNER(PasswordBox);
			CUI_SCHEMA_OWNER(Image);
			CUI_SCHEMA_OWNER(ProgressBar);
			CUI_SCHEMA_OWNER(ProgressRing);
			CUI_SCHEMA_OWNER(Popup);
			CUI_SCHEMA_OWNER(ScrollViewer);
			CUI_SCHEMA_OWNER(ListBoxItem);
			CUI_SCHEMA_OWNER(Selector);
			CUI_SCHEMA_OWNER(Separator);
			CUI_SCHEMA_OWNER(Slider);
			CUI_SCHEMA_OWNER(StatusBar);
			CUI_SCHEMA_OWNER(StatusBarItem);
			CUI_SCHEMA_OWNER(TabControl);
			CUI_SCHEMA_OWNER(TabItem);
			CUI_SCHEMA_OWNER(TextBox);
			CUI_SCHEMA_OWNER(RichTextBox);
			CUI_SCHEMA_OWNER(ToolBar);
			CUI_SCHEMA_OWNER(TreeViewItem);
			CUI_SCHEMA_OWNER(TreeView);
			CUI_SCHEMA_OWNER(WebBrowser);
#undef CUI_SCHEMA_OWNER
			return result;
		}();
		return owners;
	}

	template<typename TConcrete>
	const std::vector<const DependencyPropertyMetadata*>& NativePropertiesFor()
	{
		static const auto properties =
			DependencyPropertyRegistry::GetRegisteredProperties(
				NativeOwnerTypes<TConcrete>());
		return properties;
	}

	template<typename TConcrete>
	const std::vector<const DependencyPropertyMetadata*>* NativePropertiesPointer()
	{
		return &NativePropertiesFor<TConcrete>();
	}

	const std::vector<const DependencyPropertyMetadata*>* NativePropertySchema(
		UIClass type)
	{
		switch (type)
		{
		case UIClass::UI_FrameworkElement:
		case UIClass::UI_Control:
			return NativePropertiesPointer<Control>();
		case UIClass::UI_RangeBase: return NativePropertiesPointer<RangeBase>();
		case UIClass::UI_Label: return NativePropertiesPointer<Label>();
		case UIClass::UI_Button: return NativePropertiesPointer<Button>();
		case UIClass::UI_Image: return NativePropertiesPointer<Image>();
		case UIClass::UI_TextBox: return NativePropertiesPointer<TextBox>();
		case UIClass::UI_RichTextBox: return NativePropertiesPointer<RichTextBox>();
		case UIClass::UI_PasswordBox: return NativePropertiesPointer<PasswordBox>();
		case UIClass::UI_NumericUpDown: return NativePropertiesPointer<NumericUpDown>();
		case UIClass::UI_Panel: return NativePropertiesPointer<Panel>();
		case UIClass::UI_Decorator:
			return NativePropertiesPointer<Decorator>();
		case UIClass::UI_Border: return NativePropertiesPointer<Border>();
		case UIClass::UI_Canvas: return NativePropertiesPointer<Canvas>();
		case UIClass::UI_GroupBox: return NativePropertiesPointer<GroupBox>();
		case UIClass::UI_Expander: return NativePropertiesPointer<Expander>();
		case UIClass::UI_ScrollViewer: return NativePropertiesPointer<ScrollViewer>();
		case UIClass::UI_Popup: return NativePropertiesPointer<Popup>();
		case UIClass::UI_StackPanel: return NativePropertiesPointer<StackPanel>();
		case UIClass::UI_Grid: return NativePropertiesPointer<Grid>();
		case UIClass::UI_DockPanel: return NativePropertiesPointer<DockPanel>();
		case UIClass::UI_WrapPanel: return NativePropertiesPointer<WrapPanel>();
		case UIClass::UI_RelativePanel: return NativePropertiesPointer<RelativePanel>();
		case UIClass::UI_CheckBox: return NativePropertiesPointer<CheckBox>();
		case UIClass::UI_RadioButton: return NativePropertiesPointer<RadioButton>();
		case UIClass::UI_ComboBox: return NativePropertiesPointer<ComboBox>();
		case UIClass::UI_ListView: return NativePropertiesPointer<ListView>();
		case UIClass::UI_ListBox: return NativePropertiesPointer<ListBox>();
		case UIClass::UI_ChartView: return NativePropertiesPointer<ChartView>();
		case UIClass::UI_TreeView: return NativePropertiesPointer<TreeView>();
		case UIClass::UI_ProgressBar: return NativePropertiesPointer<ProgressBar>();
		case UIClass::UI_LoadingRing: return NativePropertiesPointer<LoadingRing>();
		case UIClass::UI_ProgressRing: return NativePropertiesPointer<ProgressRing>();
		case UIClass::UI_Slider: return NativePropertiesPointer<Slider>();
		case UIClass::UI_Switch: return NativePropertiesPointer<Switch>();
		case UIClass::UI_TabItem: return NativePropertiesPointer<TabItem>();
		case UIClass::UI_TabControl: return NativePropertiesPointer<TabControl>();
		case UIClass::UI_ToolBar: return NativePropertiesPointer<ToolBar>();
		case UIClass::UI_Menu: return NativePropertiesPointer<Menu>();
		case UIClass::UI_MenuItem: return NativePropertiesPointer<MenuItem>();
		case UIClass::UI_Separator: return NativePropertiesPointer<Separator>();
		case UIClass::UI_ContextMenu: return NativePropertiesPointer<ContextMenu>();
		case UIClass::UI_StatusBar: return NativePropertiesPointer<StatusBar>();
		case UIClass::UI_StatusBarItem: return NativePropertiesPointer<StatusBarItem>();
		case UIClass::UI_WebBrowser: return NativePropertiesPointer<WebBrowser>();
		case UIClass::UI_MediaPlayer: return NativePropertiesPointer<MediaPlayer>();
		case UIClass::UI_NativeSurface: return NativePropertiesPointer<NativeSurface>();
		case UIClass::UI_ItemsControl: return NativePropertiesPointer<ItemsControl>();
		case UIClass::UI_ContentPresenter: return NativePropertiesPointer<ContentPresenter>();
		case UIClass::UI_ItemsPresenter: return NativePropertiesPointer<ItemsPresenter>();
		case UIClass::UI_ContentControl: return NativePropertiesPointer<ContentControl>();
		case UIClass::UI_Window: return NativePropertiesPointer<Window>();
		case UIClass::UI_ListBoxItem: return NativePropertiesPointer<ListBoxItem>();
		case UIClass::UI_ListViewItem: return NativePropertiesPointer<ListViewItem>();
		case UIClass::UI_ComboBoxItem: return NativePropertiesPointer<ComboBoxItem>();
		case UIClass::UI_TreeViewItem: return NativePropertiesPointer<TreeViewItem>();
		case UIClass::UI_CalendarView: return NativePropertiesPointer<CalendarView>();
		default: return nullptr;
		}
	}
}

const CuiRuntime::BuiltInXamlTypeDescriptor*
CuiRuntime::XamlRuntimeSchema::FindBuiltInType(
	std::wstring_view namespaceUri,
	std::wstring_view localName) noexcept
{
	if (namespaceUri.empty()) namespaceUri = CuiNamespace;
	const auto found = std::find_if(
		BuiltInTypeTable.begin(), BuiltInTypeTable.end(),
		[&](const auto& candidate)
		{
			return EqualName(candidate.TypeId.NamespaceUri, namespaceUri)
				&& EqualName(candidate.TypeId.LocalName, localName);
		});
	return found == BuiltInTypeTable.end() ? nullptr : &*found;
}

std::span<const CuiRuntime::BuiltInXamlTypeDescriptor>
CuiRuntime::XamlRuntimeSchema::EnumerateBuiltInTypes() noexcept
{
	return BuiltInTypeTable;
}

const DependencyPropertyMetadata*
CuiRuntime::XamlTypePropertySchema::FindProperty(
	std::wstring_view propertyName) const noexcept
{
	const auto found = std::find_if(Properties.begin(), Properties.end(),
		[&](const DependencyPropertyMetadata* property)
		{
			return property && EqualName(property->Name(), propertyName);
		});
	return found == Properties.end() ? nullptr : *found;
}

const CuiRuntime::BuiltInXamlTypeDescriptor*
CuiRuntime::XamlRuntimeSchema::DefaultTypeFor(UIClass nativeType) noexcept
{
	const auto found = std::find_if(
		BuiltInTypeTable.begin(), BuiltInTypeTable.end(),
		[&](const auto& candidate)
		{
			return candidate.NativeType == nativeType
				&& candidate.IsDefaultForNativeType;
		});
	return found == BuiltInTypeTable.end() ? nullptr : &*found;
}

const CuiRuntime::XamlAttachedPropertyDescriptor*
CuiRuntime::XamlRuntimeSchema::FindAttachedProperty(
	std::wstring_view ownerNamespaceUri,
	std::wstring_view ownerLocalName,
	std::wstring_view memberName) noexcept
{
	if (ownerNamespaceUri.empty()) ownerNamespaceUri = CuiNamespace;
	const auto found = std::find_if(
		AttachedProperties.begin(), AttachedProperties.end(),
		[&](const auto& candidate)
		{
			return EqualName(candidate.OwnerType.NamespaceUri, ownerNamespaceUri)
				&& EqualName(candidate.OwnerType.LocalName, ownerLocalName)
				&& EqualName(candidate.Name, memberName);
		});
	return found == AttachedProperties.end() ? nullptr : &*found;
}

std::vector<const DependencyPropertyMetadata*>
CuiRuntime::XamlRuntimeSchema::NativeProperties(UIClass nativeType)
{
	const auto* properties = NativePropertySchema(nativeType);
	if (!properties) return {};
	std::vector<const DependencyPropertyMetadata*> result;
	result.reserve(properties->size());
	std::copy_if(
		properties->begin(), properties->end(),
		std::back_inserter(result),
		[nativeType](const DependencyPropertyMetadata* property)
		{
			return property
				&& IsNativePropertySupportedByUIClass(
					nativeType, *property);
		});
	return result;
}

const DependencyPropertyMetadata*
CuiRuntime::XamlRuntimeSchema::FindNativeProperty(
	UIClass nativeType,
	std::wstring_view propertyName)
{
	const auto* properties = NativePropertySchema(nativeType);
	if (!properties) return nullptr;
	const auto found = std::find_if(properties->begin(), properties->end(),
		[&](const DependencyPropertyMetadata* property)
		{
			return property
				&& IsNativePropertySupportedByUIClass(
					nativeType, *property)
				&& EqualName(property->Name(), propertyName);
		});
	return found == properties->end() ? nullptr : *found;
}

bool CuiRuntime::XamlRuntimeSchema::BuildPropertySchema(
	UIClass nativeType,
	const DesignerModel::DesignComponentDefinition* component,
	const DesignerModel::DesignDocument& document,
	XamlTypePropertySchema& output,
	std::wstring* outError)
{
	XamlTypePropertySchema candidate;
	candidate.NativeType = nativeType;
	candidate.Properties = NativeProperties(nativeType);
	if (component)
	{
		candidate.DeclarativeType = CreateComponentTypeDescriptor(
			*component, document, outError);
		if (!candidate.DeclarativeType) return false;
		const auto declarative = candidate.DeclarativeType->Properties();
		candidate.Properties.insert(candidate.Properties.begin(),
			declarative.begin(), declarative.end());
	}
	if (candidate.Properties.empty() && nativeType != UIClass::UI_Base)
	{
		if (outError) *outError = L"控件类型没有依赖属性 Schema。";
		return false;
	}
	output = std::move(candidate);
	if (outError) outError->clear();
	return true;
}

bool CuiRuntime::XamlRuntimeSchema::AttachBuiltInType(
	Control& control,
	const BuiltInXamlTypeDescriptor& type,
	XamlSchemaContext& context,
	std::wstring* outError)
{
	if (!cui::framework::DependencyPropertyAccess::SetValue(
		control, L"Focusable", BindingValue(type.FocusableByDefault),
		DependencyPropertyValueSource::Theme))
	{
		if (outError)
			*outError = L"无法应用内建 XAML 类型的 Focusable 默认值。";
		return false;
	}
	if (auto descriptor = context.Find(type.TypeId))
		return cui::framework::XamlAccess::SetTypeDescriptor(
			control, std::move(descriptor), outError);
	auto descriptor = DeclarativeTypeDescriptor::Create(type.TypeId, {}, {}, {},
		outError);
	if (!descriptor) return false;
	descriptor = context.GetOrAdd(std::move(descriptor), outError);
	return descriptor
		&& cui::framework::XamlAccess::SetTypeDescriptor(
			control, std::move(descriptor), outError);
}

std::unique_ptr<Control>
CuiRuntime::XamlRuntimeSchema::CreateNativeControl(UIClass type)
{
	auto control = [&]() -> std::unique_ptr<Control>
	{
		switch (type)
		{
	case UIClass::UI_Control: return std::make_unique<Control>();
	case UIClass::UI_Decorator: return std::make_unique<Decorator>();
	case UIClass::UI_Border: return std::make_unique<Border>();
	case UIClass::UI_Label: return std::make_unique<Label>();
	case UIClass::UI_Button: return std::make_unique<Button>();
	case UIClass::UI_TextBox: return std::make_unique<TextBox>();
	case UIClass::UI_RichTextBox: return std::make_unique<RichTextBox>();
	case UIClass::UI_PasswordBox: return std::make_unique<PasswordBox>();
	case UIClass::UI_NumericUpDown: return std::make_unique<NumericUpDown>();
	case UIClass::UI_Panel: return std::make_unique<Panel>();
	case UIClass::UI_Canvas: return std::make_unique<Canvas>();
	case UIClass::UI_GroupBox: return std::make_unique<GroupBox>();
	case UIClass::UI_Expander: return std::make_unique<Expander>();
	case UIClass::UI_ScrollViewer: return std::make_unique<ScrollViewer>();
	case UIClass::UI_Popup: return std::make_unique<Popup>();
	case UIClass::UI_StackPanel: return std::make_unique<StackPanel>();
	case UIClass::UI_Grid: return std::make_unique<Grid>();
	case UIClass::UI_DockPanel: return std::make_unique<DockPanel>();
	case UIClass::UI_WrapPanel: return std::make_unique<WrapPanel>();
	case UIClass::UI_RelativePanel: return std::make_unique<RelativePanel>();
	case UIClass::UI_CheckBox: return std::make_unique<CheckBox>();
	case UIClass::UI_RadioButton: return std::make_unique<RadioButton>();
	case UIClass::UI_ComboBox: return std::make_unique<ComboBox>();
	case UIClass::UI_ListView: return std::make_unique<ListView>();
	case UIClass::UI_ListBox: return std::make_unique<ListBox>();
	case UIClass::UI_ChartView: return std::make_unique<ChartView>();
	case UIClass::UI_TreeView: return std::make_unique<TreeView>();
	case UIClass::UI_ProgressBar: return std::make_unique<ProgressBar>();
	case UIClass::UI_LoadingRing: return std::make_unique<LoadingRing>();
	case UIClass::UI_ProgressRing: return std::make_unique<ProgressRing>();
	case UIClass::UI_Slider: return std::make_unique<Slider>();
	case UIClass::UI_Image: return std::make_unique<Image>();
	case UIClass::UI_Switch: return std::make_unique<Switch>();
	case UIClass::UI_TabItem: return std::make_unique<TabItem>();
	case UIClass::UI_TabControl: return std::make_unique<TabControl>();
	case UIClass::UI_ToolBar: return std::make_unique<ToolBar>();
	case UIClass::UI_Menu: return std::make_unique<Menu>();
	case UIClass::UI_MenuItem: return std::make_unique<MenuItem>();
	case UIClass::UI_Separator: return std::make_unique<Separator>();
	case UIClass::UI_ContextMenu: return std::make_unique<ContextMenu>();
	case UIClass::UI_StatusBar: return std::make_unique<StatusBar>();
	case UIClass::UI_StatusBarItem: return std::make_unique<StatusBarItem>();
	case UIClass::UI_WebBrowser: return std::make_unique<WebBrowser>();
	case UIClass::UI_MediaPlayer: return std::make_unique<MediaPlayer>();
	case UIClass::UI_NativeSurface: return std::make_unique<NativeSurface>();
	case UIClass::UI_ItemsControl: return std::make_unique<ItemsControl>();
	case UIClass::UI_ContentPresenter: return std::make_unique<ContentPresenter>();
	case UIClass::UI_ItemsPresenter: return std::make_unique<ItemsPresenter>();
	case UIClass::UI_ContentControl: return std::make_unique<ContentControl>();
	case UIClass::UI_ListBoxItem: return std::make_unique<ListBoxItem>();
	case UIClass::UI_ListViewItem: return std::make_unique<ListViewItem>();
	case UIClass::UI_ComboBoxItem: return std::make_unique<ComboBoxItem>();
	case UIClass::UI_TreeViewItem: return std::make_unique<TreeViewItem>();
	case UIClass::UI_CalendarView: return std::make_unique<CalendarView>();
		default: return nullptr;
		}
	}();
	if (control)
		if (const auto* descriptor = DefaultTypeFor(type))
			(void)cui::framework::DependencyPropertyAccess::SetValue(
				*control, L"Focusable",
				BindingValue(descriptor->FocusableByDefault),
				DependencyPropertyValueSource::Theme);
	return control;
}
