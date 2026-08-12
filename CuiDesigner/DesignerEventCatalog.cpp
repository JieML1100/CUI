#include "DesignerEventCatalog.h"

#include "../CUI/include/ChartView.h"
#include "../CUI/include/Calendar.h"
#include "../CUI/include/CalendarView.h"
#include "../CUI/include/DatePicker.h"
#include "../CUI/include/ToggleButton.h"
#include "../CUI/include/ButtonBase.h"
#include "../CUI/include/ComboBox.h"
#include "../CUI/include/Expander.h"
#include "../CUI/include/Window.h"
#include "../CUI/include/ListView.h"
#include "../CUI/include/ListBox.h"
#include "../CUI/include/DataGrid.h"
#include "../CUI/include/MediaElement.h"
#include "../CUI/include/Menu.h"
#include "../CUI/include/ContextMenu.h"
#include "../CUI/include/NumericUpDown.h"
#include "../CUI/include/PasswordBox.h"
#include "../CUI/include/TextBox.h"
#include "../CUI/include/RichTextBox.h"
#include "../CUI/include/RangeBase.h"
#include "../CUI/include/Slider.h"
#include "../CUI/include/ScrollViewer.h"
#include "../CUI/include/Popup.h"
#include "../CUI/include/TabControl.h"
#include "../CUI/include/TreeView.h"
#include "../CUI/include/WebBrowser.h"

#include <algorithm>
#include <cctype>
#include <cwctype>
#include <initializer_list>
#include <set>
#include <stdexcept>
#include <string_view>
#include <type_traits>

namespace
{
	using D = DesignerEventDescriptor;

	template<typename T>
	struct CppBaseTypeName;

#define CUI_CPP_EVENT_TYPE(type, spelling) \
	template<> struct CppBaseTypeName<type> \
	{ static constexpr std::string_view Value = spelling; }

	CUI_CPP_EVENT_TYPE(bool, "bool");
	CUI_CPP_EVENT_TYPE(int, "int");
	CUI_CPP_EVENT_TYPE(float, "float");
	CUI_CPP_EVENT_TYPE(double, "double");
	CUI_CPP_EVENT_TYPE(wchar_t, "wchar_t");
	CUI_CPP_EVENT_TYPE(HRESULT, "HRESULT");
	CUI_CPP_EVENT_TYPE(std::wstring, "std::wstring");
	template<> struct CppBaseTypeName<std::vector<std::wstring>>
	{ static constexpr std::string_view Value = "std::vector<std::wstring>"; };
	CUI_CPP_EVENT_TYPE(Control, "Control");
	CUI_CPP_EVENT_TYPE(RangeBase, "RangeBase");
	CUI_CPP_EVENT_TYPE(ToggleButton, "ToggleButton");
	CUI_CPP_EVENT_TYPE(ButtonBase, "ButtonBase");
	CUI_CPP_EVENT_TYPE(MenuItem, "MenuItem");
	CUI_CPP_EVENT_TYPE(DependencyObject, "DependencyObject");
	CUI_CPP_EVENT_TYPE(UIElement, "UIElement");
	CUI_CPP_EVENT_TYPE(Window, "Window");
	CUI_CPP_EVENT_TYPE(ComboBox, "ComboBox");
	CUI_CPP_EVENT_TYPE(ListView, "ListView");
	CUI_CPP_EVENT_TYPE(Selector, "Selector");
	CUI_CPP_EVENT_TYPE(DataGrid, "DataGrid");
	CUI_CPP_EVENT_TYPE(DataGridSortingEventArgs,
		"DataGridSortingEventArgs");
	CUI_CPP_EVENT_TYPE(DataGridBeginningEditEventArgs,
		"DataGridBeginningEditEventArgs");
	CUI_CPP_EVENT_TYPE(DataGridPreparingCellForEditEventArgs,
		"DataGridPreparingCellForEditEventArgs");
	CUI_CPP_EVENT_TYPE(DataGridCellEditEndingEventArgs,
		"DataGridCellEditEndingEventArgs");
	CUI_CPP_EVENT_TYPE(DataGridCurrentCellChangedEventArgs,
		"DataGridCurrentCellChangedEventArgs");
	CUI_CPP_EVENT_TYPE(DataGridSelectedCellsChangedEventArgs,
		"DataGridSelectedCellsChangedEventArgs");
	CUI_CPP_EVENT_TYPE(ItemContainerControl, "ItemContainerControl");
	CUI_CPP_EVENT_TYPE(Menu, "Menu");
	CUI_CPP_EVENT_TYPE(ContextMenu, "ContextMenu");
	CUI_CPP_EVENT_TYPE(ChartView, "ChartView");
	CUI_CPP_EVENT_TYPE(Calendar, "Calendar");
	CUI_CPP_EVENT_TYPE(CalendarView, "CalendarView");
	CUI_CPP_EVENT_TYPE(DatePicker, "DatePicker");
	CUI_CPP_EVENT_TYPE(DatePickerDateValidationErrorEventArgs,
		"DatePickerDateValidationErrorEventArgs");
	CUI_CPP_EVENT_TYPE(NumericUpDown, "NumericUpDown");
	CUI_CPP_EVENT_TYPE(PasswordBox, "PasswordBox");
	CUI_CPP_EVENT_TYPE(Expander, "Expander");
	CUI_CPP_EVENT_TYPE(Slider, "Slider");
	CUI_CPP_EVENT_TYPE(ScrollViewer, "ScrollViewer");
	CUI_CPP_EVENT_TYPE(Popup, "Popup");
	CUI_CPP_EVENT_TYPE(TabControl, "TabControl");
	CUI_CPP_EVENT_TYPE(TabItem, "TabItem");
	CUI_CPP_EVENT_TYPE(TreeView, "TreeView");
	CUI_CPP_EVENT_TYPE(WebBrowser, "WebBrowser");
	CUI_CPP_EVENT_TYPE(MediaElement, "MediaElement");
	CUI_CPP_EVENT_TYPE(MouseEventArgs, "MouseEventArgs");
	CUI_CPP_EVENT_TYPE(DragEventArgs, "DragEventArgs");
	CUI_CPP_EVENT_TYPE(SizeChangedEventArgs, "SizeChangedEventArgs");
	CUI_CPP_EVENT_TYPE(KeyEventArgs, "KeyEventArgs");
	CUI_CPP_EVENT_TYPE(TextCompositionEventArgs, "TextCompositionEventArgs");
	CUI_CPP_EVENT_TYPE(TextChangedEventArgs, "TextChangedEventArgs");
	CUI_CPP_EVENT_TYPE(ScrollChangedEventArgs, "ScrollChangedEventArgs");
	CUI_CPP_EVENT_TYPE(SelectionChangedEventArgs, "SelectionChangedEventArgs");
	CUI_CPP_EVENT_TYPE(RoutedPropertyChangedEventArgs<double>,
		"RoutedPropertyChangedEventArgs<double>");
	CUI_CPP_EVENT_TYPE(RoutedPropertyChangedEventArgs<BindingValue>,
		"RoutedPropertyChangedEventArgs<BindingValue>");
	CUI_CPP_EVENT_TYPE(KeyboardFocusChangedEventArgs,
		"KeyboardFocusChangedEventArgs");
	CUI_CPP_EVENT_TYPE(RoutedEventArgs, "RoutedEventArgs");
	CUI_CPP_EVENT_TYPE(CancelEventArgs, "CancelEventArgs");
	CUI_CPP_EVENT_TYPE(CanExecuteRoutedEventArgs, "CanExecuteRoutedEventArgs");
	CUI_CPP_EVENT_TYPE(ExecutedRoutedEventArgs, "ExecutedRoutedEventArgs");
	CUI_CPP_EVENT_TYPE(DependencyPropertyChangedEventArgs,
		"DependencyPropertyChangedEventArgs");
	CUI_CPP_EVENT_TYPE(BindingValidationChangedEventArgs,
		"BindingValidationChangedEventArgs");
	CUI_CPP_EVENT_TYPE(WebBrowser::NavigationStartingArgs,
		"WebBrowser::NavigationStartingArgs");
	CUI_CPP_EVENT_TYPE(WebBrowser::NavigationCompletedArgs,
		"WebBrowser::NavigationCompletedArgs");
	CUI_CPP_EVENT_TYPE(WebBrowser::ContentLoadingArgs,
		"WebBrowser::ContentLoadingArgs");
	CUI_CPP_EVENT_TYPE(WebBrowser::DomContentLoadedArgs,
		"WebBrowser::DomContentLoadedArgs");
	CUI_CPP_EVENT_TYPE(WebBrowser::SourceChangedArgs,
		"WebBrowser::SourceChangedArgs");
	CUI_CPP_EVENT_TYPE(WebBrowser::HistoryChangedArgs,
		"WebBrowser::HistoryChangedArgs");
	CUI_CPP_EVENT_TYPE(WebBrowser::DocumentTitleChangedArgs,
		"WebBrowser::DocumentTitleChangedArgs");
	CUI_CPP_EVENT_TYPE(WebBrowser::NewWindowRequestedArgs,
		"WebBrowser::NewWindowRequestedArgs");
	CUI_CPP_EVENT_TYPE(WebBrowser::ProcessFailedArgs,
		"WebBrowser::ProcessFailedArgs");
	CUI_CPP_EVENT_TYPE(WebBrowser::WebMessageReceivedArgs,
		"WebBrowser::WebMessageReceivedArgs");
	CUI_CPP_EVENT_TYPE(MediaElement::PlaybackState, "MediaElement::PlaybackState");

#undef CUI_CPP_EVENT_TYPE

	template<typename T>
	std::string CppTypeName()
	{
		if constexpr (std::is_lvalue_reference_v<T>)
		{
			using Value = std::remove_reference_t<T>;
			if constexpr (std::is_const_v<Value>)
				return "const " + CppTypeName<std::remove_const_t<Value>>() + "&";
			else
				return CppTypeName<Value>() + "&";
		}
		else if constexpr (std::is_pointer_v<T>)
		{
			using Value = std::remove_pointer_t<T>;
			if constexpr (std::is_const_v<Value>)
				return "const " + CppTypeName<std::remove_const_t<Value>>() + "*";
			else
				return CppTypeName<Value>() + "*";
		}
		else
		{
			using Value = std::remove_cv_t<T>;
			return std::string(CppBaseTypeName<Value>::Value);
		}
	}

	template<typename Function>
	struct EventParameterList;

	template<typename Return, typename... Args>
	struct EventParameterList<Return(Args...)>
	{
		static std::string Build(
			std::initializer_list<std::string_view> parameterNames)
		{
			static_assert(std::is_void_v<Return>,
				"Designer events currently require void Event functions");
			if (parameterNames.size() != sizeof...(Args))
				throw std::logic_error(
					"Designer event parameter-name count does not match Event type");
			std::vector<std::string_view> names(parameterNames);
			std::string result;
			size_t index = 0;
			auto append = [&](std::string type)
			{
				if (!result.empty()) result += ", ";
				result += std::move(type);
				result += " ";
				result += names[index++];
			};
			(append(CppTypeName<Args>()), ...);
			return result;
		}
	};

	bool Contains(std::wstring_view value, std::wstring_view token)
	{
		return value.find(token) != std::wstring_view::npos;
	}

	DesignerEventCategory ClassifyEvent(std::wstring_view name)
	{
		if (Contains(name, L"Navigation") || Contains(name, L"DOMContent")
			|| Contains(name, L"ContentLoading") || Contains(name, L"SourceChanged")
			|| Contains(name, L"HistoryChanged") || Contains(name, L"DocumentTitle")
			|| Contains(name, L"NewWindow") || Contains(name, L"ProcessFailed")
			|| Contains(name, L"WebMessage"))
			return DesignerEventCategory::Navigation;
		if (Contains(name, L"Property") || Contains(name, L"Validation")
			|| Contains(name, L"Error") || Contains(name, L"Failed"))
			return DesignerEventCategory::Diagnostics;
		if (Contains(name, L"Media") || name == L"PositionChanged"
			|| name == L"PlaybackStateChanged")
			return DesignerEventCategory::Media;
		if (Contains(name, L"Mouse") || Contains(name, L"Hover"))
			return DesignerEventCategory::Mouse;
		if (Contains(name, L"Focus"))
			return DesignerEventCategory::Focus;
		if (Contains(name, L"Key") || Contains(name, L"CharInput")
			|| Contains(name, L"TextInput"))
			return DesignerEventCategory::Keyboard;
		if (Contains(name, L"Drag") || Contains(name, L"Drop"))
			return DesignerEventCategory::DragDrop;
		if (Contains(name, L"Close") || Contains(name, L"ContentRendered")
			|| Contains(name, L"Dismissed"))
			return DesignerEventCategory::Lifecycle;
		if (Contains(name, L"Paint") || Contains(name, L"Moved")
			|| Contains(name, L"SizeChanged") || Contains(name, L"ScrollChanged")
			|| Contains(name, L"ViewportChanged"))
			return DesignerEventCategory::Layout;
		if (Contains(name, L"Query") || Contains(name, L"Filter")
			|| Contains(name, L"UserAdding") || Contains(name, L"UserAdded"))
			return DesignerEventCategory::Data;
		if (Contains(name, L"Click") || Contains(name, L"Command")
			|| Contains(name, L"Apply") || Contains(name, L"Reset"))
			return DesignerEventCategory::Action;
		if (Contains(name, L"Changed") || Contains(name, L"Checked")
			|| Contains(name, L"Unchecked") || Contains(name, L"Collapsed")
			|| Contains(name, L"Submenu")
			|| Contains(name, L"Selection") || Contains(name, L"Toggled")
			|| Contains(name, L"Expanded"))
			return DesignerEventCategory::Value;
		return DesignerEventCategory::Other;
	}

	std::wstring_view DefaultControlEventName(UIClass type)
	{
		switch (type)
		{
		case UIClass::UI_TextBox:
		case UIClass::UI_RichTextBox:
			return L"TextChanged";
		case UIClass::UI_PasswordBox:
			return L"PasswordChanged";
		case UIClass::UI_CheckBox:
		case UIClass::UI_RadioButton:
		case UIClass::UI_Switch:
			return L"Checked";
		case UIClass::UI_ComboBox:
		case UIClass::UI_ListView:
		case UIClass::UI_ListBox:
		case UIClass::UI_DataGrid:
			return L"SelectionChanged";
		case UIClass::UI_TreeView:
			return L"SelectedItemChanged";
		case UIClass::UI_Calendar:
		case UIClass::UI_CalendarView:
			return L"SelectedDatesChanged";
		case UIClass::UI_DatePicker:
			return L"SelectedDateChanged";
		case UIClass::UI_ChartView:
			return L"PointClick";
		case UIClass::UI_Slider:
		case UIClass::UI_NumericUpDown:
			return L"ValueChanged";
		case UIClass::UI_Expander:
			return L"Expanded";
		case UIClass::UI_Menu:
		case UIClass::UI_ContextMenu:
			return L"Executed";
		case UIClass::UI_Button:
		case UIClass::UI_DataGridRowHeader:
		case UIClass::UI_MenuItem:
			return L"Click";
		case UIClass::UI_WebBrowser:
			return L"NavigationCompleted";
		case UIClass::UI_MediaElement:
			return L"MediaOpened";
		default:
			return L"MouseDown";
		}
	}

	void ApplyPresentationMetadata(
		std::vector<D>& events, std::wstring_view defaultEvent)
	{
		for (auto& event : events)
		{
			event.Category = ClassifyEvent(event.Name);
			event.IsDefault = event.Name == defaultEvent;
		}
		std::stable_sort(events.begin(), events.end(), [](const D& left, const D& right)
		{
			if (left.Category != right.Category)
				return static_cast<unsigned char>(left.Category)
					< static_cast<unsigned char>(right.Category);
			if (left.IsDefault != right.IsDefault) return left.IsDefault;
			if (left.Order != right.Order) return left.Order < right.Order;
			return left.Name < right.Name;
		});
	}

	void SortPresentationMetadata(std::vector<D>& events)
	{
		std::stable_sort(events.begin(), events.end(), [](const D& left, const D& right)
		{
			if (left.Category != right.Category)
				return static_cast<unsigned char>(left.Category)
					< static_cast<unsigned char>(right.Category);
			if (left.IsDefault != right.IsDefault) return left.IsDefault;
			if (left.Order != right.Order) return left.Order < right.Order;
			return left.Name < right.Name;
		});
	}

	template<typename Owner, typename RuntimeEvent>
	D MakeEventDescriptor(
		std::wstring name,
		std::string eventField,
		RuntimeEvent Owner::* eventMember,
		std::initializer_list<std::string_view> parameterNames)
	{
		using Function = typename RuntimeEvent::function_type;
		// Native C++ event fields may use an internal `On` prefix; XAML names do not.
		// That implementation spelling is not part of the XAML contract: WPF
		// routed-event names are MouseDown, PreviewKeyDown, Executed, and so on.
		if (name.size() > 2 && name[0] == L'O' && name[1] == L'n'
			&& std::iswupper(name[2]))
			name.erase(0, 2);
		auto result = D::FromEventMember(
			std::move(name), std::move(eventField),
			EventParameterList<Function>::Build(parameterNames), eventMember);
		result.EventOwnerTypeName = CppTypeName<Owner>();
		result.Category = ClassifyEvent(result.Name);
		return result;
	}

	template<typename ExposedOwner, typename Owner, typename RuntimeEvent>
	D MakeExposedEventDescriptor(
		std::wstring name,
		std::string eventField,
		RuntimeEvent Owner::* eventMember,
		std::initializer_list<std::string_view> parameterNames)
	{
		auto result = MakeEventDescriptor(
			std::move(name), std::move(eventField),
			eventMember, parameterNames);
		result.EventOwnerTypeName = CppTypeName<ExposedOwner>();
		return result;
	}

#define CUI_WIDEN_IMPL(text) L##text
#define CUI_WIDEN(text) CUI_WIDEN_IMPL(text)
#define CUI_EVENT(owner, name, member, ...) \
	MakeEventDescriptor(CUI_WIDEN(#name), #member, \
		&owner::member, { __VA_ARGS__ })
#define CUI_EXPOSED_EVENT(exposedOwner, owner, name, member, ...) \
	MakeExposedEventDescriptor<exposedOwner>(CUI_WIDEN(#name), #member, \
		&owner::member, { __VA_ARGS__ })

	const std::vector<D>& CommonControlEvents()
	{
		static const std::vector<D> events = {
			CUI_EVENT(Control, OnPreviewMouseWheel, OnPreviewMouseWheel, "sender", "e"),
			CUI_EVENT(Control, OnMouseWheel, OnMouseWheel, "sender", "e"),
			CUI_EVENT(Control, OnPreviewMouseMove, OnPreviewMouseMove, "sender", "e"),
			CUI_EVENT(Control, OnMouseMove, OnMouseMove, "sender", "e"),
			CUI_EVENT(Control, OnPreviewMouseDown, OnPreviewMouseDown, "sender", "e"),
			CUI_EVENT(Control, OnMouseDown, OnMouseDown, "sender", "e"),
			CUI_EVENT(Control, OnPreviewMouseUp, OnPreviewMouseUp, "sender", "e"),
			CUI_EVENT(Control, OnMouseUp, OnMouseUp, "sender", "e"),
			CUI_EVENT(Control, OnPreviewMouseDoubleClick, OnPreviewMouseDoubleClick, "sender", "e"),
			CUI_EVENT(Control, OnMouseDoubleClick, OnMouseDoubleClick, "sender", "e"),
			CUI_EVENT(Control, OnMouseEnter, OnMouseEnter, "sender", "e"),
			CUI_EVENT(Control, OnMouseLeave, OnMouseLeave, "sender", "e"),
			CUI_EVENT(Control, OnGotMouseCapture, OnGotMouseCapture, "sender", "e"),
			CUI_EVENT(Control, OnLostMouseCapture, OnLostMouseCapture, "sender", "e"),
			CUI_EVENT(Control, OnPreviewKeyDown, OnPreviewKeyDown, "sender", "e"),
			CUI_EVENT(Control, OnKeyDown, OnKeyDown, "sender", "e"),
			CUI_EVENT(Control, OnPreviewKeyUp, OnPreviewKeyUp, "sender", "e"),
			CUI_EVENT(Control, OnKeyUp, OnKeyUp, "sender", "e"),
			CUI_EVENT(Control, OnPreviewTextInputStart,
				OnPreviewTextInputStart, "sender", "e"),
			CUI_EVENT(Control, OnTextInputStart,
				OnTextInputStart, "sender", "e"),
			CUI_EVENT(Control, OnPreviewTextInputUpdate,
				OnPreviewTextInputUpdate, "sender", "e"),
			CUI_EVENT(Control, OnTextInputUpdate,
				OnTextInputUpdate, "sender", "e"),
			CUI_EVENT(Control, OnPreviewTextInput, OnPreviewTextInput, "sender", "e"),
			CUI_EVENT(Control, OnTextInput, OnTextInput, "sender", "e"),
			CUI_EVENT(Control, OnPreviewGotKeyboardFocus,
				OnPreviewGotKeyboardFocus, "sender", "e"),
			CUI_EVENT(Control, OnGotKeyboardFocus,
				OnGotKeyboardFocus, "sender", "e"),
			CUI_EVENT(Control, OnPreviewLostKeyboardFocus,
				OnPreviewLostKeyboardFocus, "sender", "e"),
			CUI_EVENT(Control, OnLostKeyboardFocus,
				OnLostKeyboardFocus, "sender", "e"),
			CUI_EVENT(Control, OnGotFocus, OnGotFocus, "sender", "e"),
			CUI_EVENT(Control, OnLostFocus, OnLostFocus, "sender", "e"),
			CUI_EVENT(Control, OnPreviewCanExecute, OnPreviewCanExecute, "sender", "e"),
			CUI_EVENT(Control, OnCanExecute, OnCanExecute, "sender", "e"),
			CUI_EVENT(Control, OnPreviewExecuted, OnPreviewExecuted, "sender", "e"),
			CUI_EVENT(Control, OnExecuted, OnExecuted, "sender", "e"),
			CUI_EVENT(Control, OnPreviewDragEnter, OnPreviewDragEnter, "sender", "e"),
			CUI_EVENT(Control, OnDragEnter, OnDragEnter, "sender", "e"),
			CUI_EVENT(Control, OnPreviewDragOver, OnPreviewDragOver, "sender", "e"),
			CUI_EVENT(Control, OnDragOver, OnDragOver, "sender", "e"),
			CUI_EVENT(Control, OnPreviewDragLeave, OnPreviewDragLeave, "sender", "e"),
			CUI_EVENT(Control, OnDragLeave, OnDragLeave, "sender", "e"),
			CUI_EVENT(Control, OnPreviewDrop, OnPreviewDrop, "sender", "e"),
			CUI_EVENT(Control, OnDrop, OnDrop, "sender", "e"),
			CUI_EVENT(Control, SizeChanged, SizeChanged, "sender", "e"),
			CUI_EVENT(Control, IsVisibleChanged, IsVisibleChanged, "sender", "e"),
		};
		return events;
	}

	void Append(std::vector<D>& out, std::initializer_list<D> events)
	{
		out.insert(out.end(), events.begin(), events.end());
	}

	std::wstring Trim(const std::wstring& value)
	{
		size_t first = 0;
		while (first < value.size() && iswspace(value[first])) ++first;
		size_t last = value.size();
		while (last > first && iswspace(value[last - 1])) --last;
		return value.substr(first, last - first);
	}

	bool IsAsciiLetter(wchar_t ch)
	{
		return (ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z');
	}

	bool IsAsciiDigit(wchar_t ch)
	{
		return ch >= L'0' && ch <= L'9';
	}

	std::wstring SanitizeIdentifier(const std::wstring& value, const wchar_t* fallback)
	{
		std::wstring out;
		out.reserve(value.size() + 1);
		for (wchar_t ch : value)
			out.push_back(IsAsciiLetter(ch) || IsAsciiDigit(ch) || ch == L'_' ? ch : L'_');
		if (out.empty()) out = fallback;
		if (IsAsciiDigit(out.front())) out.insert(out.begin(), L'_');
		return out;
	}

	std::wstring LowerAscii(std::wstring value)
	{
		for (auto& ch : value)
			if (ch >= L'A' && ch <= L'Z') ch = static_cast<wchar_t>(ch - L'A' + L'a');
		return value;
	}
}

std::vector<DesignerEventDescriptor> DesignerEventCatalog::GetControlEvents(UIClass type)
{
	std::vector<D> out = CommonControlEvents();
	switch (type)
	{
	case UIClass::UI_Button:
	case UIClass::UI_DataGridRowHeader:
		Append(out, { CUI_EXPOSED_EVENT(
			ButtonBase, ButtonBase, Click, Click, "sender", "e") });
		break;
	case UIClass::UI_MenuItem:
		Append(out, {
			CUI_EXPOSED_EVENT(
				MenuItem, MenuItem, Click, Click, "sender", "e"),
			CUI_EVENT(MenuItem, Checked, Checked, "sender", "e"),
			CUI_EVENT(MenuItem, Unchecked, Unchecked, "sender", "e"),
			CUI_EVENT(MenuItem, SubmenuOpened, SubmenuOpened, "sender", "e"),
			CUI_EVENT(MenuItem, SubmenuClosed, SubmenuClosed, "sender", "e"),
		});
		break;
	case UIClass::UI_TextBox:
		Append(out, { CUI_EVENT(TextBox,
			TextChanged, OnTextChanged, "sender", "e") });
		break;
	case UIClass::UI_RichTextBox:
		Append(out, {
			CUI_EVENT(TextBox, TextChanged, OnTextChanged, "sender", "e"),
			CUI_EVENT(RichTextBox, SelectionChanged,
				SelectionChanged, "sender", "e"),
		});
		break;
	case UIClass::UI_PasswordBox:
		Append(out, { CUI_EVENT(PasswordBox,
			PasswordChanged, PasswordChanged, "sender", "e") });
		break;
	case UIClass::UI_CheckBox:
	case UIClass::UI_RadioButton:
	case UIClass::UI_Switch:
		Append(out, {
			CUI_EXPOSED_EVENT(
				ButtonBase, ButtonBase, Click, Click, "sender", "e"),
			CUI_EVENT(ToggleButton, Checked, Checked, "sender", "e"),
			CUI_EVENT(ToggleButton, Unchecked, Unchecked, "sender", "e"),
		});
		break;
	case UIClass::UI_ContextMenu:
		Append(out, {
			CUI_EVENT(ContextMenu, Opened, Opened, "sender"),
			CUI_EVENT(ContextMenu, Closed, Closed, "sender"),
		});
		break;
	case UIClass::UI_ComboBox:
		Append(out, { CUI_EVENT(ComboBox, SelectionChanged,
			SelectionChanged, "sender", "e") });
		break;
	case UIClass::UI_ListBoxItem:
	case UIClass::UI_ListViewItem:
	case UIClass::UI_ComboBoxItem:
		Append(out, {
			CUI_EVENT(ItemContainerControl, Selected,
				Selected, "sender", "e"),
			CUI_EVENT(ItemContainerControl, Unselected,
				Unselected, "sender", "e"),
		});
		break;
	case UIClass::UI_TreeView:
		Append(out, { CUI_EVENT(TreeView, SelectedItemChanged,
			SelectedItemChanged, "sender", "e") });
		break;
	case UIClass::UI_TreeViewItem:
		Append(out, {
			CUI_EVENT(TreeViewItem, Selected, Selected, "sender", "e"),
			CUI_EVENT(TreeViewItem, Unselected, Unselected, "sender", "e"),
			CUI_EVENT(TreeViewItem, Expanded, Expanded, "sender", "e"),
			CUI_EVENT(TreeViewItem, Collapsed, Collapsed, "sender", "e"),
		});
		break;
	case UIClass::UI_Calendar:
	case UIClass::UI_CalendarView:
		Append(out, { CUI_EVENT(CalendarView, SelectedDatesChanged,
			SelectedDatesChanged, "sender", "e") });
		break;
	case UIClass::UI_DatePicker:
		Append(out, {
			CUI_EVENT(DatePicker, CalendarOpened,
				CalendarOpened, "sender"),
			CUI_EVENT(DatePicker, CalendarClosed,
				CalendarClosed, "sender"),
			CUI_EVENT(DatePicker, SelectedDateChanged,
				SelectedDateChanged, "sender", "e"),
			CUI_EVENT(DatePicker, DateValidationError,
				DateValidationError, "sender", "e"),
		});
		break;
	case UIClass::UI_ListView:
	case UIClass::UI_ListBox:
		Append(out, {
			CUI_EVENT(Selector, SelectionChanged,
				SelectionChanged, "sender", "e"),
		});
		break;
	case UIClass::UI_DataGrid:
		Append(out, {
			CUI_EVENT(Selector, SelectionChanged,
				SelectionChanged, "sender", "e"),
			CUI_EVENT(DataGrid, Sorting, Sorting, "sender", "e"),
			CUI_EVENT(DataGrid, BeginningEdit,
				BeginningEdit, "sender", "e"),
			CUI_EVENT(DataGrid, PreparingCellForEdit,
				PreparingCellForEdit, "sender", "e"),
			CUI_EVENT(DataGrid, CellEditEnding,
				CellEditEnding, "sender", "e"),
			CUI_EVENT(DataGrid, CurrentCellChanged,
				CurrentCellChanged, "sender", "e"),
			CUI_EVENT(DataGrid, SelectedCellsChanged,
				SelectedCellsChanged, "sender", "e"),
		});
		break;
	case UIClass::UI_ScrollViewer:
		Append(out, { CUI_EVENT(ScrollViewer, OnScrollChanged,
			OnScrollChanged, "sender", "e") });
		break;
	case UIClass::UI_Popup:
		Append(out, {
			CUI_EVENT(Popup, Opened, Opened, "sender"),
			CUI_EVENT(Popup, Closed, Closed, "sender"),
		});
		break;
	case UIClass::UI_TabControl:
		Append(out, { CUI_EVENT(TabControl, SelectionChanged,
			SelectionChanged, "sender", "e") });
		break;
	case UIClass::UI_TabItem:
		Append(out, {
			CUI_EVENT(TabItem, Selected, Selected, "sender", "e"),
			CUI_EVENT(TabItem, Unselected, Unselected, "sender", "e"),
		});
		break;
	case UIClass::UI_ChartView:
		Append(out, {
			CUI_EVENT(ChartView, OnPointClick,
				OnPointClick, "sender", "seriesIndex", "pointIndex"),
			CUI_EVENT(ChartView, OnPointHover,
				OnPointHover, "sender", "seriesIndex", "pointIndex"),
			CUI_EVENT(ChartView, SelectionChanged,
				SelectionChanged, "sender", "e"),
			CUI_EVENT(ChartView, OnViewportChanged, OnViewportChanged, "sender"),
		});
		break;
	case UIClass::UI_Slider:
	case UIClass::UI_ProgressBar:
	case UIClass::UI_ProgressRing:
		Append(out, { CUI_EVENT(Slider, ValueChanged,
			ValueChanged, "sender", "e") });
		break;
	case UIClass::UI_NumericUpDown:
		Append(out, { CUI_EVENT(NumericUpDown, ValueChanged,
			ValueChanged, "sender", "e") });
		break;
	case UIClass::UI_Expander:
		Append(out, {
			CUI_EVENT(Expander, Expanded, Expanded, "sender", "e"),
			CUI_EVENT(Expander, Collapsed, Collapsed, "sender", "e"),
		});
		break;
	case UIClass::UI_WebBrowser:
		Append(out, {
			CUI_EVENT(WebBrowser, OnNavigationStarting,
				OnNavigationStarting, "sender", "e"),
			CUI_EVENT(WebBrowser, OnNavigationCompleted,
				OnNavigationCompleted, "sender", "e"),
			CUI_EVENT(WebBrowser, OnNavigationFailed,
				OnNavigationFailed, "sender", "e"),
			CUI_EVENT(WebBrowser, OnContentLoading,
				OnContentLoading, "sender", "e"),
			CUI_EVENT(WebBrowser, OnDOMContentLoaded,
				OnDOMContentLoaded, "sender", "e"),
			CUI_EVENT(WebBrowser, OnSourceChanged,
				OnSourceChanged, "sender", "e"),
			CUI_EVENT(WebBrowser, OnHistoryChanged,
				OnHistoryChanged, "sender", "e"),
			CUI_EVENT(WebBrowser, OnDocumentTitleChanged,
				OnDocumentTitleChanged, "sender", "e"),
			CUI_EVENT(WebBrowser, OnNewWindowRequested,
				OnNewWindowRequested, "sender", "e"),
			CUI_EVENT(WebBrowser, OnProcessFailed,
				OnProcessFailed, "sender", "e"),
			CUI_EVENT(WebBrowser, OnWebMessageReceived,
				OnWebMessageReceived, "sender", "e"),
		});
		break;
	case UIClass::UI_MediaElement:
		Append(out, {
			CUI_EVENT(MediaElement, OnMediaOpened, OnMediaOpened, "sender"),
			CUI_EVENT(MediaElement, OnMediaEnded, OnMediaEnded, "sender"),
			CUI_EVENT(MediaElement, OnMediaFailed, OnMediaFailed, "sender"),
			CUI_EVENT(MediaElement, OnPositionChanged,
				OnPositionChanged, "sender", "position"),
			CUI_EVENT(MediaElement, OnPlaybackStateChanged,
				OnPlaybackStateChanged, "sender", "oldState", "newState"),
			CUI_EVENT(MediaElement, OnMediaError,
				OnMediaError, "sender", "error"),
		});
		break;
	default:
		break;
	}
	ApplyPresentationMetadata(out, DefaultControlEventName(type));
	return out;
}

std::optional<DesignerEventDescriptor> DesignerEventCatalog::FromComponentEvent(
	const DesignerComponentEventDescriptor& event) noexcept
{
	try
	{
		std::wstring validationError;
		if (!ValidateHandlerName(event.Name, &validationError)
			|| event.Name.empty()
			|| *GetComponentRoutingStrategyName(event.RoutingStrategy) == '\0')
			return std::nullopt;
		DesignerEventDescriptor result;
		result.Name = event.Name;
		result.DisplayName = event.DisplayName.empty()
			? event.Name : event.DisplayName;
		result.EventField = "OnDeclarativeEvent";
		result.EventOwnerTypeName = "Control";
		result.ParameterList =
			"Control* sender, DeclarativeEventArgs& e";
		result.Signature = std::type_index(
			typeid(void(Control*, DeclarativeEventArgs&)));
		result.Category = event.Category;
		result.Order = event.Order;
		result.IsDefault = event.IsDefault;
		return result;
	}
	catch (...)
	{
		return std::nullopt;
	}
}

std::vector<DesignerEventDescriptor> DesignerEventCatalog::GetControlEvents(
	UIClass type,
	const std::vector<DesignerComponentEventDescriptor>& componentEvents)
{
	auto result = GetControlEvents(type);
	if (std::any_of(componentEvents.begin(), componentEvents.end(),
		[](const auto& event) { return event.IsDefault; }))
		for (auto& event : result) event.IsDefault = false;
	for (const auto& component : componentEvents)
		if (auto event = FromComponentEvent(component))
			result.push_back(std::move(*event));
	SortPresentationMetadata(result);
	return result;
}

const std::vector<DesignerEventDescriptor>& DesignerEventCatalog::GetWindowEvents()
{
	static const std::vector<D> events = []
	{
		std::vector<D> result = {
			CUI_EVENT(Control, OnPreviewMouseWheel, OnPreviewMouseWheel, "sender", "e"),
			CUI_EVENT(Control, OnMouseWheel, OnMouseWheel, "sender", "e"),
			CUI_EVENT(Control, OnPreviewMouseMove, OnPreviewMouseMove, "sender", "e"),
			CUI_EVENT(Control, OnMouseMove, OnMouseMove, "sender", "e"),
			CUI_EVENT(Control, OnPreviewMouseDown, OnPreviewMouseDown, "sender", "e"),
			CUI_EVENT(Control, OnMouseDown, OnMouseDown, "sender", "e"),
			CUI_EVENT(Control, OnPreviewMouseUp, OnPreviewMouseUp, "sender", "e"),
			CUI_EVENT(Control, OnMouseUp, OnMouseUp, "sender", "e"),
			CUI_EVENT(Control, OnPreviewMouseDoubleClick, OnPreviewMouseDoubleClick, "sender", "e"),
			CUI_EVENT(Control, OnMouseDoubleClick, OnMouseDoubleClick, "sender", "e"),
			CUI_EVENT(Control, OnMouseEnter, OnMouseEnter, "sender", "e"),
			CUI_EVENT(Control, OnMouseLeave, OnMouseLeave, "sender", "e"),
			CUI_EVENT(Control, OnGotMouseCapture, OnGotMouseCapture, "sender", "e"),
			CUI_EVENT(Control, OnLostMouseCapture, OnLostMouseCapture, "sender", "e"),
			CUI_EVENT(Control, OnPreviewKeyDown, OnPreviewKeyDown, "sender", "e"),
			CUI_EVENT(Control, OnKeyDown, OnKeyDown, "sender", "e"),
			CUI_EVENT(Control, OnPreviewKeyUp, OnPreviewKeyUp, "sender", "e"),
			CUI_EVENT(Control, OnKeyUp, OnKeyUp, "sender", "e"),
			CUI_EVENT(Control, OnPreviewTextInputStart,
				OnPreviewTextInputStart, "sender", "e"),
			CUI_EVENT(Control, OnTextInputStart,
				OnTextInputStart, "sender", "e"),
			CUI_EVENT(Control, OnPreviewTextInputUpdate,
				OnPreviewTextInputUpdate, "sender", "e"),
			CUI_EVENT(Control, OnTextInputUpdate,
				OnTextInputUpdate, "sender", "e"),
			CUI_EVENT(Control, OnPreviewTextInput, OnPreviewTextInput, "sender", "e"),
			CUI_EVENT(Control, OnTextInput, OnTextInput, "sender", "e"),
			CUI_EVENT(Control, OnPreviewGotKeyboardFocus,
				OnPreviewGotKeyboardFocus, "sender", "e"),
			CUI_EVENT(Control, OnGotKeyboardFocus,
				OnGotKeyboardFocus, "sender", "e"),
			CUI_EVENT(Control, OnPreviewLostKeyboardFocus,
				OnPreviewLostKeyboardFocus, "sender", "e"),
			CUI_EVENT(Control, OnLostKeyboardFocus,
				OnLostKeyboardFocus, "sender", "e"),
			CUI_EVENT(Control, OnGotFocus, OnGotFocus, "sender", "e"),
			CUI_EVENT(Control, OnLostFocus, OnLostFocus, "sender", "e"),
			CUI_EVENT(Control, OnPreviewCanExecute, OnPreviewCanExecute, "sender", "e"),
			CUI_EVENT(Control, OnCanExecute, OnCanExecute, "sender", "e"),
			CUI_EVENT(Control, OnPreviewExecuted, OnPreviewExecuted, "sender", "e"),
			CUI_EVENT(Control, OnExecuted, OnExecuted, "sender", "e"),
			CUI_EVENT(Control, OnPreviewDragEnter, OnPreviewDragEnter, "sender", "e"),
			CUI_EVENT(Control, OnDragEnter, OnDragEnter, "sender", "e"),
			CUI_EVENT(Control, OnPreviewDragOver, OnPreviewDragOver, "sender", "e"),
			CUI_EVENT(Control, OnDragOver, OnDragOver, "sender", "e"),
			CUI_EVENT(Control, OnPreviewDragLeave, OnPreviewDragLeave, "sender", "e"),
			CUI_EVENT(Control, OnDragLeave, OnDragLeave, "sender", "e"),
			CUI_EVENT(Control, OnPreviewDrop, OnPreviewDrop, "sender", "e"),
			CUI_EVENT(Control, OnDrop, OnDrop, "sender", "e"),
			CUI_EVENT(Window, Closing, OnClosing, "sender", "e"),
			CUI_EVENT(Window, OnLocationChanged, OnLocationChanged, "sender"),
			CUI_EVENT(Control, SizeChanged, SizeChanged, "sender", "e"),
			CUI_EVENT(Control, IsVisibleChanged, IsVisibleChanged, "sender", "e"),
			CUI_EVENT(Window, ContentRendered, ContentRendered, "sender"),
			CUI_EVENT(Window, Closed, OnWindowClosed, "sender"),
		};
		ApplyPresentationMetadata(result, L"ContentRendered");
		return result;
	}();
	return events;
}

#undef CUI_EVENT
#undef CUI_EXPOSED_EVENT
#undef CUI_WIDEN
#undef CUI_WIDEN_IMPL

std::optional<DesignerEventDescriptor> DesignerEventCatalog::FindControlEvent(
	UIClass type, const std::wstring& eventName)
{
	auto events = GetControlEvents(type);
	auto it = std::find_if(events.begin(), events.end(), [&](const D& event) {
		return event.Name == eventName;
	});
	return it == events.end() ? std::nullopt : std::optional<D>(*it);
}

std::optional<DesignerEventDescriptor> DesignerEventCatalog::FindControlEvent(
	UIClass type,
	const std::wstring& eventName,
	const std::vector<DesignerComponentEventDescriptor>& componentEvents)
{
	auto events = GetControlEvents(type, componentEvents);
	const auto found = std::find_if(events.begin(), events.end(),
		[&](const D& event) { return event.Name == eventName; });
	return found == events.end() ? std::nullopt
		: std::optional<D>(*found);
}

std::optional<DesignerEventDescriptor> DesignerEventCatalog::FindWindowEvent(
	const std::wstring& eventName)
{
	const auto& events = GetWindowEvents();
	auto it = std::find_if(events.begin(), events.end(), [&](const D& event) {
		return event.Name == eventName;
	});
	return it == events.end() ? std::nullopt : std::optional<D>(*it);
}

std::optional<DesignerEventDescriptor>
DesignerEventCatalog::GetDefaultControlEvent(UIClass type)
{
	auto events = GetControlEvents(type);
	auto it = std::find_if(events.begin(), events.end(), [](const D& event)
	{
		return event.IsDefault;
	});
	return it == events.end() ? std::nullopt : std::optional<D>(*it);
}

std::optional<DesignerEventDescriptor>
DesignerEventCatalog::GetDefaultControlEvent(
	UIClass type,
	const std::vector<DesignerComponentEventDescriptor>& componentEvents)
{
	auto events = GetControlEvents(type, componentEvents);
	const auto found = std::find_if(events.begin(), events.end(),
		[](const D& event) { return event.IsDefault; });
	return found == events.end() ? std::nullopt
		: std::optional<D>(*found);
}

std::optional<DesignerEventDescriptor> DesignerEventCatalog::GetDefaultWindowEvent()
{
	const auto& events = GetWindowEvents();
	auto it = std::find_if(events.begin(), events.end(), [](const D& event)
	{
		return event.IsDefault;
	});
	return it == events.end() ? std::nullopt : std::optional<D>(*it);
}

const wchar_t* DesignerEventCatalog::GetCategoryDisplayName(
	DesignerEventCategory category) noexcept
{
	switch (category)
	{
	case DesignerEventCategory::Action: return L"操作";
	case DesignerEventCategory::Value: return L"值变化";
	case DesignerEventCategory::Mouse: return L"鼠标";
	case DesignerEventCategory::Keyboard: return L"键盘";
	case DesignerEventCategory::Focus: return L"焦点";
	case DesignerEventCategory::DragDrop: return L"拖放";
	case DesignerEventCategory::Layout: return L"布局";
	case DesignerEventCategory::Lifecycle: return L"生命周期";
	case DesignerEventCategory::Data: return L"数据";
	case DesignerEventCategory::Navigation: return L"导航";
	case DesignerEventCategory::Media: return L"媒体";
	case DesignerEventCategory::Diagnostics: return L"诊断";
	default: return L"其他";
	}
}

const char* DesignerEventCatalog::GetCategoryName(
	DesignerEventCategory category) noexcept
{
	switch (category)
	{
	case DesignerEventCategory::Action: return "Action";
	case DesignerEventCategory::Value: return "Value";
	case DesignerEventCategory::Mouse: return "Mouse";
	case DesignerEventCategory::Keyboard: return "Keyboard";
	case DesignerEventCategory::Focus: return "Focus";
	case DesignerEventCategory::DragDrop: return "DragDrop";
	case DesignerEventCategory::Layout: return "Layout";
	case DesignerEventCategory::Lifecycle: return "Lifecycle";
	case DesignerEventCategory::Data: return "Data";
	case DesignerEventCategory::Navigation: return "Navigation";
	case DesignerEventCategory::Media: return "Media";
	case DesignerEventCategory::Diagnostics: return "Diagnostics";
	default: return "Other";
	}
}

bool DesignerEventCatalog::TryParseCategory(
	const std::wstring& value,
	DesignerEventCategory& category) noexcept
{
	const auto normalized = LowerAscii(Trim(value));
	if (normalized.empty() || normalized == L"other") category = DesignerEventCategory::Other;
	else if (normalized == L"action") category = DesignerEventCategory::Action;
	else if (normalized == L"value") category = DesignerEventCategory::Value;
	else if (normalized == L"mouse") category = DesignerEventCategory::Mouse;
	else if (normalized == L"keyboard") category = DesignerEventCategory::Keyboard;
	else if (normalized == L"focus") category = DesignerEventCategory::Focus;
	else if (normalized == L"dragdrop") category = DesignerEventCategory::DragDrop;
	else if (normalized == L"layout") category = DesignerEventCategory::Layout;
	else if (normalized == L"lifecycle") category = DesignerEventCategory::Lifecycle;
	else if (normalized == L"data") category = DesignerEventCategory::Data;
	else if (normalized == L"navigation") category = DesignerEventCategory::Navigation;
	else if (normalized == L"media") category = DesignerEventCategory::Media;
	else if (normalized == L"diagnostics") category = DesignerEventCategory::Diagnostics;
	else return false;
	return true;
}

const char* DesignerEventCatalog::GetComponentPayloadName(
	DesignerComponentEventPayload payload) noexcept
{
	switch (payload)
	{
	case DesignerComponentEventPayload::None: return "None";
	case DesignerComponentEventPayload::Bool: return "Bool";
	case DesignerComponentEventPayload::Int: return "Int";
	case DesignerComponentEventPayload::Int64: return "Int64";
	case DesignerComponentEventPayload::Float: return "Float";
	case DesignerComponentEventPayload::Double: return "Double";
	case DesignerComponentEventPayload::String: return "String";
	default: return "";
	}
}

bool DesignerEventCatalog::TryParseComponentPayload(
	const std::wstring& value,
	DesignerComponentEventPayload& payload) noexcept
{
	const auto normalized = LowerAscii(Trim(value));
	if (normalized.empty() || normalized == L"none")
		payload = DesignerComponentEventPayload::None;
	else if (normalized == L"bool")
		payload = DesignerComponentEventPayload::Bool;
	else if (normalized == L"int")
		payload = DesignerComponentEventPayload::Int;
	else if (normalized == L"int64")
		payload = DesignerComponentEventPayload::Int64;
	else if (normalized == L"float")
		payload = DesignerComponentEventPayload::Float;
	else if (normalized == L"double")
		payload = DesignerComponentEventPayload::Double;
	else if (normalized == L"string")
		payload = DesignerComponentEventPayload::String;
	else return false;
	return true;
}

const char* DesignerEventCatalog::GetComponentRoutingStrategyName(
	DeclarativeEventRoutingStrategy strategy) noexcept
{
	switch (strategy)
	{
	case DeclarativeEventRoutingStrategy::Direct: return "Direct";
	case DeclarativeEventRoutingStrategy::Bubble: return "Bubble";
	case DeclarativeEventRoutingStrategy::Tunnel: return "Tunnel";
	default: return "";
	}
}

bool DesignerEventCatalog::TryParseComponentRoutingStrategy(
	const std::wstring& value,
	DeclarativeEventRoutingStrategy& strategy) noexcept
{
	const auto normalized = LowerAscii(Trim(value));
	if (normalized.empty() || normalized == L"direct")
		strategy = DeclarativeEventRoutingStrategy::Direct;
	else if (normalized == L"bubble")
		strategy = DeclarativeEventRoutingStrategy::Bubble;
	else if (normalized == L"tunnel" || normalized == L"preview")
		strategy = DeclarativeEventRoutingStrategy::Tunnel;
	else return false;
	return true;
}

std::wstring DesignerEventCatalog::MakeAttachedComponentEventKey(
	const DesignerComponentType& ownerType,
	const std::wstring& eventName)
{
	return L"@{" + ownerType.XamlNamespace + L"}"
		+ ownerType.XamlName + L"." + eventName;
}

bool DesignerEventCatalog::TryParseAttachedComponentEventKey(
	const std::wstring& key,
	DesignerComponentType& ownerType,
	std::wstring& eventName) noexcept
{
	try
	{
		ownerType = {};
		eventName.clear();
		if (!key.starts_with(L"@{")) return false;
		const auto close = key.find(L'}', 2);
		if (close == std::wstring::npos || close == 2) return false;
		const auto separator = key.find(L'.', close + 1);
		if (separator == std::wstring::npos
			|| separator == close + 1 || separator + 1 >= key.size())
			return false;
		ownerType.XamlNamespace = key.substr(2, close - 2);
		ownerType.XamlName = key.substr(
			close + 1, separator - close - 1);
		eventName = key.substr(separator + 1);
		return true;
	}
	catch (...)
	{
		ownerType = {};
		eventName.clear();
		return false;
	}
}

bool DesignerEventCatalog::ValidateComponentEvents(
	UIClass baseType,
	const std::vector<DesignerComponentEventDescriptor>& events,
	std::wstring* outError)
{
	if (events.size() > 256)
	{
		if (outError) *outError = L"组件事件超过 256 项限制。";
		return false;
	}
	std::set<std::wstring> names;
	bool hasDefault = false;
	const auto baseEvents = GetControlEvents(baseType);
	for (const auto& event : events)
	{
		std::wstring validationError;
		if (!FromComponentEvent(event)
			|| !ValidateHandlerName(event.Name, &validationError))
		{
			if (outError) *outError = L"组件事件无效：" + event.Name
				+ (validationError.empty() ? L"。" : L"：" + validationError);
			return false;
		}
		const auto& name = event.Name;
		if (!names.insert(name).second)
		{
			if (outError) *outError = L"组件事件名称重复：" + event.Name;
			return false;
		}
		if (event.IsDefault && hasDefault)
		{
			if (outError) *outError = L"只能声明一个默认组件事件。";
			return false;
		}
		hasDefault = hasDefault || event.IsDefault;
		if (std::any_of(baseEvents.begin(), baseEvents.end(),
			[&](const auto& base) { return base.Name == name; }))
		{
			if (outError) *outError = L"组件事件与 BaseType 事件重名：" + event.Name;
			return false;
		}
	}
	if (outError) outError->clear();
	return true;
}

bool DesignerEventCatalog::IsKnownEventName(const std::wstring& eventName)
{
	if (FindWindowEvent(eventName)) return true;
	for (int value = static_cast<int>(UIClass::UI_Base);
		value <= static_cast<int>(UIClass::UI_Last); ++value)
	{
		if (FindControlEvent(static_cast<UIClass>(value), eventName)) return true;
	}
	return false;
}

std::wstring DesignerEventCatalog::MakeDefaultHandlerName(
	const std::wstring& subjectName, const std::wstring& eventName)
{
	auto subject = SanitizeIdentifier(subjectName, L"control");
	auto event = SanitizeIdentifier(eventName, L"Event");
	return subject + L"_" + event;
}

std::wstring DesignerEventCatalog::NormalizeHandlerName(
	const std::wstring& storedValue)
{
	return Trim(storedValue);
}

bool DesignerEventCatalog::ValidateHandlerName(
	const std::wstring& handlerName, std::wstring* error)
{
	const auto value = Trim(handlerName);
	if (value.empty()) return true;
	if ((value.size() >= 2 && value[0] == L'_' && value[1] == L'_') ||
		(value.size() >= 2 && value[0] == L'_'
			&& value[1] >= L'A' && value[1] <= L'Z'))
	{
		if (error) *error = L"事件处理函数名不能使用 C++ 为实现保留的标识符。";
		return false;
	}
	if ((!IsAsciiLetter(value.front()) && value.front() != L'_') ||
		!std::all_of(value.begin() + 1, value.end(), [](wchar_t ch) {
			return IsAsciiLetter(ch) || IsAsciiDigit(ch) || ch == L'_';
		}))
	{
		if (error) *error = L"事件处理函数必须是一个不含 :: 的 C++ 标识符。";
		return false;
	}

	static const std::set<std::wstring> keywords = {
		L"alignas", L"alignof", L"and", L"and_eq", L"asm", L"auto", L"bitand", L"bitor",
		L"bool", L"break", L"case", L"catch", L"char", L"char8_t", L"char16_t", L"char32_t",
		L"class", L"compl", L"concept", L"const", L"consteval", L"constexpr", L"constinit",
		L"const_cast", L"continue", L"co_await", L"co_return", L"co_yield", L"decltype", L"default",
		L"delete", L"do", L"double", L"dynamic_cast", L"else", L"enum", L"explicit", L"export",
		L"extern", L"false", L"float", L"for", L"friend", L"goto", L"if", L"inline", L"int",
		L"long", L"mutable", L"namespace", L"new", L"noexcept", L"not", L"not_eq", L"nullptr",
		L"operator", L"or", L"or_eq", L"private", L"protected", L"public", L"register",
		L"reinterpret_cast", L"requires", L"return", L"short", L"signed", L"sizeof", L"static",
		L"static_assert", L"static_cast", L"struct", L"switch", L"template", L"this", L"thread_local",
		L"throw", L"true", L"try", L"typedef", L"typeid", L"typename", L"union", L"unsigned",
		L"using", L"virtual", L"void", L"volatile", L"wchar_t", L"while", L"xor", L"xor_eq",
	};
	if (keywords.find(value) != keywords.end())
	{
		if (error) *error = L"事件处理函数名不能使用 C++ 关键字。";
		return false;
	}
	return true;
}
