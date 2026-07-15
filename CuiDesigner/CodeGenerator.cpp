#include "CodeGenerator.h"
#include "CodeGenInput.h"
#include "DesignerBindingUtils.h"
#include "DesignerPropertyCatalog.h"
#include "DesignerStyleSheetUtils.h"
#include <algorithm>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <cctype>
#include <functional>
#include <cfloat>
#include <climits>
#include <cmath>
#include <map>

// 生成时需要访问具体控件类型的公开字段/方法
#include "../CUI/include/ComboBox.h"
#include "../CUI/include/ListView.h"
#include "../CUI/include/GridView.h"
#include "../CUI/include/PropertyGrid.h"
#include "../CUI/include/ChartView.h"
#include "../CUI/include/ReportView.h"
#include "../CUI/include/KpiCard.h"
#include "../CUI/include/FilterBar.h"
#include "../CUI/include/TabControl.h"
#include "../CUI/include/ToolBar.h"
#include "../CUI/include/Button.h"
#include "../CUI/include/LoadingRing.h"
#include "../CUI/include/ProgressBar.h"
#include "../CUI/include/ProgressRing.h"
#include "../CUI/include/Slider.h"
#include "../CUI/include/NumericUpDown.h"
#include "../CUI/include/PictureBox.h"
#include "../CUI/include/DateTimePicker.h"
#include "../CUI/include/ScrollView.h"
#include "../CUI/include/TreeView.h"
#include "../CUI/include/Menu.h"
#include "../CUI/include/StatusBar.h"
#include "../CUI/include/Toast.h"
#include "../CUI/include/MediaPlayer.h"
#include "../CUI/include/GroupBox.h"
#include "../CUI/include/Expander.h"
#include "../CUI/include/SplitContainer.h"

#include "../CUI/include/Layout/GridPanel.h"
#include "../CUI/include/Layout/StackPanel.h"
#include "../CUI/include/Layout/DockPanel.h"
#include "../CUI/include/Layout/WrapPanel.h"
#include "../CUI/include/Layout/RelativePanel.h"

static bool IsLayoutContainerType(UIClass t)
{
	switch (t)
	{
	case UIClass::UI_GridPanel:
	case UIClass::UI_StackPanel:
	case UIClass::UI_DockPanel:
	case UIClass::UI_WrapPanel:
	case UIClass::UI_RelativePanel:
		return true;
	default:
		return false;
	}
}

static bool IsContainerType(UIClass t)
{
	switch (t)
	{
	case UIClass::UI_Panel:
	case UIClass::UI_GroupBox:
	case UIClass::UI_Expander:
	case UIClass::UI_SplitContainer:
	case UIClass::UI_ScrollView:
	case UIClass::UI_StackPanel:
	case UIClass::UI_GridPanel:
	case UIClass::UI_DockPanel:
	case UIClass::UI_WrapPanel:
	case UIClass::UI_RelativePanel:
	case UIClass::UI_TabControl:
	case UIClass::UI_TabPage:
	case UIClass::UI_ToolBar:
		return true;
	default:
		return false;
	}
}

static void SortSplitChildrenByRuntimeOrder(SplitContainer* split, std::vector<std::shared_ptr<DesignerControl>>& list)
{
	if (!split || list.size() <= 1) return;
	std::unordered_map<Control*, int> runtimeOrder;
	int order = 0;
	Panel* first = split->FirstPanel();
	Panel* second = split->SecondPanel();
	if (first)
	{
		for (int i = 0; i < first->Count; i++)
			runtimeOrder[first->operator[](i)] = order++;
	}
	if (second)
	{
		for (int i = 0; i < second->Count; i++)
			runtimeOrder[second->operator[](i)] = order++;
	}
	std::stable_sort(list.begin(), list.end(), [&](const auto& a, const auto& b)
		{
			int leftOrder = INT_MAX;
			int rightOrder = INT_MAX;
			auto leftOrderIt = runtimeOrder.find(a->ControlInstance);
			if (leftOrderIt != runtimeOrder.end()) leftOrder = leftOrderIt->second;
			auto rightOrderIt = runtimeOrder.find(b->ControlInstance);
			if (rightOrderIt != runtimeOrder.end()) rightOrder = rightOrderIt->second;
			return leftOrder < rightOrder;
		});
}

static std::string GetSplitChildHostExpr(SplitContainer* split, Control* runtimeParent, const std::string& parentExpr)
{
	if (split && runtimeParent == split->SecondPanel())
		return parentExpr + "->SecondPanel()";
	return parentExpr + "->FirstPanel()";
}

static std::string AnchorStylesToExpr(uint8_t a)
{
	if (a == AnchorStyles::None) return "AnchorStyles::None";
	std::string out;
	auto add = [&](const char* s) {
		if (!out.empty()) out += " | ";
		out += s;
	};
	if (a & AnchorStyles::Left) add("AnchorStyles::Left");
	if (a & AnchorStyles::Top) add("AnchorStyles::Top");
	if (a & AnchorStyles::Right) add("AnchorStyles::Right");
	if (a & AnchorStyles::Bottom) add("AnchorStyles::Bottom");
	if (out.empty()) return "AnchorStyles::None";
	return out;
}

namespace
{
	static const char* BindingModeToExpr(BindingMode mode)
	{
		switch (mode)
		{
		case BindingMode::OneWay: return "BindingMode::OneWay";
		case BindingMode::TwoWay: return "BindingMode::TwoWay";
		case BindingMode::OneWayToSource: return "BindingMode::OneWayToSource";
		case BindingMode::OneTime: return "BindingMode::OneTime";
		}
		return "BindingMode::OneWay";
	}

	static const char* DataSourceUpdateModeToExpr(DataSourceUpdateMode mode)
	{
		switch (mode)
		{
		case DataSourceUpdateMode::OnPropertyChanged: return "DataSourceUpdateMode::OnPropertyChanged";
		case DataSourceUpdateMode::OnValidation: return "DataSourceUpdateMode::OnValidation";
		case DataSourceUpdateMode::Never: return "DataSourceUpdateMode::Never";
		}
		return "DataSourceUpdateMode::OnPropertyChanged";
	}

	struct GeneratedEventBinding
	{
		std::string ControlVar;
		std::string EventField;
		std::string HandlerName;
		std::string ParamList; // "Control* sender" ...
	};

	static std::string LocalSanitizeCppIdentifier(const std::string& raw)
	{
		std::string out;
		out.reserve(raw.size() + 2);
		for (unsigned char ch : raw)
		{
			if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_')
				out.push_back((char)ch);
			else
				out.push_back('_');
		}
		if (!out.empty() && (out[0] >= '0' && out[0] <= '9'))
			out.insert(out.begin(), '_');
		if (out.empty()) out = "control";
		return out;
	}

	static bool TryGetEventSignature(UIClass controlType, const std::wstring& eventName,
		std::string& outEventField, std::string& outParamList)
	{
		std::wstring n = eventName;
		if (n == L"OnMouseWheel") { outEventField = "OnMouseWheel"; outParamList = "Control* sender, MouseEventArgs e"; return true; }
		if (n == L"OnMouseMove") { outEventField = "OnMouseMove"; outParamList = "Control* sender, MouseEventArgs e"; return true; }
		if (n == L"OnMouseUp") { outEventField = "OnMouseUp"; outParamList = "Control* sender, MouseEventArgs e"; return true; }
		if (n == L"OnMouseDown") { outEventField = "OnMouseDown"; outParamList = "Control* sender, MouseEventArgs e"; return true; }
		if (n == L"OnMouseClick") { outEventField = "OnMouseClick"; outParamList = "Control* sender, MouseEventArgs e"; return true; }
		if (n == L"OnMouseDoubleClick") { outEventField = "OnMouseDoubleClick"; outParamList = "Control* sender, MouseEventArgs e"; return true; }
		if (n == L"OnMouseEnter") { outEventField = "OnMouseEnter"; outParamList = "Control* sender, MouseEventArgs e"; return true; }
		if (n == L"OnMouseLeave") { outEventField = "OnMouseLeave"; outParamList = "Control* sender, MouseEventArgs e"; return true; }
		if (n == L"OnKeyDown") { outEventField = "OnKeyDown"; outParamList = "Control* sender, KeyEventArgs e"; return true; }
		if (n == L"OnKeyUp") { outEventField = "OnKeyUp"; outParamList = "Control* sender, KeyEventArgs e"; return true; }
		if (n == L"OnCharInput") { outEventField = "OnCharInput"; outParamList = "Control* sender, wchar_t ch"; return true; }
		if (n == L"OnGotFocus") { outEventField = "OnGotFocus"; outParamList = "Control* sender"; return true; }
		if (n == L"OnLostFocus") { outEventField = "OnLostFocus"; outParamList = "Control* sender"; return true; }
		if (n == L"OnDropFile") { outEventField = "OnDropFile"; outParamList = "Control* sender, List<std::wstring> files"; return true; }
		if (n == L"OnDropText") { outEventField = "OnDropText"; outParamList = "Control* sender, std::wstring text"; return true; }
		if (n == L"OnPaint") { outEventField = "OnPaint"; outParamList = "Control* sender"; return true; }
		if (n == L"OnClose") { outEventField = "OnClose"; outParamList = "Control* sender"; return true; }
		if (n == L"OnMoved") { outEventField = "OnMoved"; outParamList = "Control* sender"; return true; }
		if (n == L"OnSizeChanged") { outEventField = "OnSizeChanged"; outParamList = "Control* sender"; return true; }
		if (n == L"OnTextChanged") { outEventField = "OnTextChanged"; outParamList = "Control* sender, std::wstring oldText, std::wstring newText"; return true; }
		if (n == L"OnChecked") { outEventField = "OnChecked"; outParamList = "Control* sender"; return true; }
		if (n == L"OnSelectionChanged") { outEventField = "OnSelectionChanged"; outParamList = "Control* sender"; return true; }
		if (n == L"OnSelectedChanged") { outEventField = "OnSelectedChanged"; outParamList = "Control* sender"; return true; }
		if (n == L"OnScrollChanged") { outEventField = "OnScrollChanged"; outParamList = "Control* sender"; return true; }
		if (n == L"ScrollChanged") { outEventField = "ScrollChanged"; outParamList = "Control* sender"; return true; }
		if (n == L"SelectionChanged" && controlType == UIClass::UI_PropertyGrid) { outEventField = "SelectionChanged"; outParamList = "PropertyGridView* sender, int index"; return true; }
		if (n == L"SelectionChanged") { outEventField = "SelectionChanged"; outParamList = "Control* sender"; return true; }
		if (n == L"OnValueChanged")
		{
			if (controlType == UIClass::UI_PropertyGrid)
			{
				outEventField = "OnValueChanged";
				outParamList = "PropertyGridView* sender, int index, std::wstring oldValue, std::wstring newValue";
				return true;
			}
			if (controlType == UIClass::UI_NumericUpDown)
			{
				outEventField = "OnValueChanged";
				outParamList = "NumericUpDown* sender, double oldValue, double newValue";
				return true;
			}
			if (controlType != UIClass::UI_Slider) return false;
			outEventField = "OnValueChanged";
			outParamList = "Control* sender, float oldValue, float newValue";
			return true;
		}
		if (n == L"OnExpandedChanged")
		{
			if (controlType != UIClass::UI_Expander) return false;
			outEventField = "OnExpandedChanged";
			outParamList = "Expander* sender, bool expanded";
			return true;
		}
		if (n == L"OnMenuCommand")
		{
			if (controlType != UIClass::UI_Menu) return false;
			outEventField = "OnMenuCommand";
			outParamList = "Control* sender, int id";
			return true;
		}
		if (n == L"OnGridViewCheckStateChanged")
		{
			if (controlType != UIClass::UI_GridView) return false;
			outEventField = "OnGridViewCheckStateChanged";
			outParamList = "GridView* sender, int c, int r, bool v";
			return true;
		}
		if (n == L"OnGridViewLinkedTextClick")
		{
			if (controlType != UIClass::UI_GridView) return false;
			outEventField = "OnGridViewLinkedTextClick";
			outParamList = "GridView* sender, int c, int r, std::wstring text";
			return true;
		}
		if (n == L"OnItemClick")
		{
			if (controlType == UIClass::UI_PropertyGrid)
			{
				outEventField = "OnItemClick";
				outParamList = "PropertyGridView* sender, int index";
				return true;
			}
			if (controlType != UIClass::UI_ListView && controlType != UIClass::UI_ListBox) return false;
			outEventField = "OnItemClick";
			outParamList = "ListView* sender, int index";
			return true;
		}
		if (n == L"OnItemDoubleClick")
		{
			if (controlType != UIClass::UI_ListView && controlType != UIClass::UI_ListBox) return false;
			outEventField = "OnItemDoubleClick";
			outParamList = "ListView* sender, int index";
			return true;
		}
		if (n == L"OnItemCheckChanged")
		{
			if (controlType != UIClass::UI_ListView && controlType != UIClass::UI_ListBox) return false;
			outEventField = "OnItemCheckChanged";
			outParamList = "ListView* sender, int index, bool checked";
			return true;
		}
		if (n == L"OnToastClick")
		{
			if (controlType != UIClass::UI_ToastHost) return false;
			outEventField = "OnToastClick";
			outParamList = "ToastHost* sender, int index";
			return true;
		}
		if (n == L"OnToastDismissed")
		{
			if (controlType != UIClass::UI_ToastHost) return false;
			outEventField = "OnToastDismissed";
			outParamList = "ToastHost* sender, int index";
			return true;
		}
		if (n == L"OnUserAddingRow")
		{
			if (controlType != UIClass::UI_GridView) return false;
			outEventField = "OnUserAddingRow";
			outParamList = "GridView* sender, bool& cancel";
			return true;
		}
		if (n == L"OnUserAddedRow")
		{
			if (controlType != UIClass::UI_GridView) return false;
			outEventField = "OnUserAddedRow";
			outParamList = "GridView* sender, int newRowIndex";
			return true;
		}
		return false;
	}

	static bool TryGetFormEventSignature(const std::wstring& eventName,
		std::string& outEventField, std::string& outParamList)
	{
		std::wstring n = eventName;
		if (n == L"OnMouseWheel") { outEventField = "OnMouseWheel"; outParamList = "Form* sender, MouseEventArgs e"; return true; }
		if (n == L"OnMouseMove") { outEventField = "OnMouseMove"; outParamList = "Form* sender, MouseEventArgs e"; return true; }
		if (n == L"OnMouseUp") { outEventField = "OnMouseUp"; outParamList = "Form* sender, MouseEventArgs e"; return true; }
		if (n == L"OnMouseDown") { outEventField = "OnMouseDown"; outParamList = "Form* sender, MouseEventArgs e"; return true; }
		if (n == L"OnMouseClick") { outEventField = "OnMouseClick"; outParamList = "Form* sender, MouseEventArgs e"; return true; }
		if (n == L"OnKeyDown") { outEventField = "OnKeyDown"; outParamList = "Form* sender, KeyEventArgs e"; return true; }
		if (n == L"OnKeyUp") { outEventField = "OnKeyUp"; outParamList = "Form* sender, KeyEventArgs e"; return true; }
		if (n == L"OnPaint") { outEventField = "OnPaint"; outParamList = "Form* sender"; return true; }
		if (n == L"OnMoved") { outEventField = "OnMoved"; outParamList = "Form* sender"; return true; }
		if (n == L"OnSizeChanged") { outEventField = "OnSizeChanged"; outParamList = "Form* sender"; return true; }
		if (n == L"OnTextChanged") { outEventField = "OnTextChanged"; outParamList = "Form* sender, std::wstring oldText, std::wstring newText"; return true; }
		if (n == L"OnCharInput") { outEventField = "OnCharInput"; outParamList = "Form* sender, wchar_t ch"; return true; }
		if (n == L"OnGotFocus") { outEventField = "OnGotFocus"; outParamList = "Form* sender"; return true; }
		if (n == L"OnLostFocus") { outEventField = "OnLostFocus"; outParamList = "Form* sender"; return true; }
		if (n == L"OnDropFile") { outEventField = "OnDropFile"; outParamList = "Form* sender, List<std::wstring> files"; return true; }
		if (n == L"OnDropText") { outEventField = "OnDropText"; outParamList = "Form* sender, std::wstring text"; return true; }
		if (n == L"OnFormClosing") { outEventField = "OnFormClosing"; outParamList = "Form* sender"; return true; }
		if (n == L"OnFormClosed") { outEventField = "OnFormClosed"; outParamList = "Form* sender"; return true; }
		if (n == L"OnCommand") { outEventField = "OnCommand"; outParamList = "Form* sender, int Id, int info"; return true; }

		if (n == L"OnMouseDoubleClick") { outEventField = "OnMouseDoubleClick"; outParamList = "Control* sender, MouseEventArgs e"; return true; }
		if (n == L"OnMouseEnter") { outEventField = "OnMouseEnter"; outParamList = "Control* sender, MouseEventArgs e"; return true; }
		if (n == L"OnMouseLeave") { outEventField = "OnMouseLeave"; outParamList = "Control* sender, MouseEventArgs e"; return true; }
		if (n == L"OnClose") { outEventField = "OnClose"; outParamList = "Control* sender"; return true; }
		return false;
	}

	static std::string EnsureOnPrefix(std::string s)
	{
		if (s.rfind("On", 0) == 0) return s;
		return std::string("On") + s;
	}

	static std::string MakeHandlerName(const std::string& controlVar, const std::wstring& eventName)
	{
		std::string raw;
		if (!eventName.empty())
		{
			int size = WideCharToMultiByte(CP_UTF8, 0, eventName.data(), (int)eventName.size(), nullptr, 0, nullptr, nullptr);
			raw.resize(std::max(0, size));
			if (size > 0)
				WideCharToMultiByte(CP_UTF8, 0, eventName.data(), (int)eventName.size(), raw.data(), size, nullptr, nullptr);
		}
		std::string suffix = EnsureOnPrefix(LocalSanitizeCppIdentifier(raw));
		return controlVar + "_" + suffix;
	}
}

CodeGenerator::CodeGenerator(std::wstring className, const CodeGenInput& input)
	: CodeGenerator(
		std::move(className),
		input.Controls,
		input.FormText,
		input.FormSize,
		input.FormLocation,
		input.FormName,
		input.FormBackColor,
		input.FormForeColor,
		input.FormShowInTaskBar,
		input.FormTopMost,
		input.FormEnable,
		input.FormVisible,
		input.FormEventHandlers,
		input.FormVisibleHead,
		input.FormHeadHeight,
		input.FormMinBox,
		input.FormMaxBox,
		input.FormCloseBox,
		input.FormCenterTitle,
		input.FormAllowResize,
		input.FormFontName,
		input.FormFontSize)
{
	_styleSheet = input.StyleSheet;
}

CodeGenerator::CodeGenerator(std::wstring className, const std::vector<std::shared_ptr<DesignerControl>>& controls,
	std::wstring formText, SIZE formSize, POINT formLocation, std::wstring formName,
	D2D1_COLOR_F formBackColor, D2D1_COLOR_F formForeColor,
	bool formShowInTaskBar, bool formTopMost, bool formEnable, bool formVisible,
	const std::map<std::wstring, std::wstring>& formEventHandlers,
	bool formVisibleHead, int formHeadHeight,
	bool formMinBox, bool formMaxBox, bool formCloseBox,
	bool formCenterTitle, bool formAllowResize,
	std::wstring formFontName, float formFontSize)
	: _className(className), _controls(controls), _formText(formText), _formName(formName), _formSize(formSize), _formLocation(formLocation),
	_formBackColor(formBackColor), _formForeColor(formForeColor),
	_formShowInTaskBar(formShowInTaskBar), _formTopMost(formTopMost), _formEnable(formEnable), _formVisible(formVisible),
	_formEventHandlers(formEventHandlers),
	_formVisibleHead(formVisibleHead), _formHeadHeight(formHeadHeight),
	_formMinBox(formMinBox), _formMaxBox(formMaxBox), _formCloseBox(formCloseBox),
	_formCenterTitle(formCenterTitle), _formAllowResize(formAllowResize),
	_formFontName(std::move(formFontName)), _formFontSize(formFontSize)
{
	if (_formSize.cx <= 0) _formSize.cx = 800;
	if (_formSize.cy <= 0) _formSize.cy = 600;
	if (_formHeadHeight < 0) _formHeadHeight = 0;
	if (_formFontSize < 1.0f) _formFontSize = 1.0f;
	if (_formFontSize > 200.0f) _formFontSize = 200.0f;
	if (_formName.empty()) _formName = L"MainForm";
	if (_formLocation.x < -10000) _formLocation.x = -10000;
	if (_formLocation.y < -10000) _formLocation.y = -10000;
	if (_formLocation.x > 10000) _formLocation.x = 10000;
	if (_formLocation.y > 10000) _formLocation.y = 10000;
	BuildVarNameMap();
}

static bool IsCppKeyword(const std::string& s)
{
	static const std::unordered_set<std::string> k = {
		"alignas","alignof","and","and_eq","asm","atomic_cancel","atomic_commit","atomic_noexcept",
		"auto","bitand","bitor","bool","break","case","catch","char","char8_t","char16_t","char32_t",
		"class","compl","concept","const","consteval","constexpr","constinit","const_cast","continue",
		"co_await","co_return","co_yield","decltype","default","delete","do","double","dynamic_cast",
		"else","enum","explicit","export","extern","false","float","for","friend","goto","if","inline",
		"int","long","mutable","namespace","new","noexcept","not","not_eq","nullptr","operator","or",
		"or_eq","private","protected","public","register","reinterpret_cast","requires","return","short",
		"signed","sizeof","static","static_assert","static_cast","struct","switch","synchronized","template",
		"this","thread_local","throw","true","try","typedef","typeid","typename","union","unsigned","using",
		"virtual","void","volatile","wchar_t","while","xor","xor_eq"
	};
	return k.find(s) != k.end();
}

std::string CodeGenerator::SanitizeCppIdentifier(const std::string& raw)
{
	std::string out;
	out.reserve(raw.size() + 2);

	for (unsigned char ch : raw)
	{
		if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_')
			out.push_back((char)ch);
		else
			out.push_back('_');
	}

	// 不能以数字开头
	if (!out.empty() && (out[0] >= '0' && out[0] <= '9'))
		out.insert(out.begin(), '_');

	// 不能空
	if (out.empty()) out = "control";

	if (IsCppKeyword(out)) out += "_";

	return out;
}

void CodeGenerator::BuildVarNameMap()
{
	_varNameOf.clear();
	_varNameOf.reserve(_controls.size());

	std::unordered_map<std::string, int> used;
	used.reserve(_controls.size());

	for (const auto& dc : _controls)
	{
		if (!dc) continue;
		std::string base = SanitizeCppIdentifier(WStringToString(dc->Name));
		// 保守：成员变量建议以小写开头，避免与类型名混淆（仅在安全情况下调整）
		if (!base.empty() && base[0] >= 'A' && base[0] <= 'Z')
			base[0] = (char)(base[0] - 'A' + 'a');

		int& nameUseCount = used[base];
		nameUseCount++;
		std::string finalName = base;
		if (nameUseCount > 1)
			finalName = base + std::to_string(nameUseCount);

		// 二次防御：仍可能撞上关键字（例如 base="this" 调整后）
		if (IsCppKeyword(finalName)) finalName += "_";

		_varNameOf[dc.get()] = finalName;
	}
}

std::string CodeGenerator::GetVarName(const std::shared_ptr<DesignerControl>& dc) const
{
	if (!dc) return "";
	auto it = _varNameOf.find(dc.get());
	if (it != _varNameOf.end()) return it->second;
	return SanitizeCppIdentifier(WStringToString(dc->Name));
}

std::string CodeGenerator::WStringToString(const std::wstring& wstr) const
{
	if (wstr.empty()) return std::string();
	int size = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), nullptr, 0, nullptr, nullptr);
	std::string result(size, 0);
	WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &result[0], size, nullptr, nullptr);
	return result;
}

std::wstring CodeGenerator::StringToWString(const std::string& str) const
{
	if (str.empty()) return std::wstring();
	int size = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), nullptr, 0);
	std::wstring result(size, 0);
	MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &result[0], size);
	return result;
}

std::string CodeGenerator::GetControlTypeName(UIClass type)
{
	switch (type)
	{
	case UIClass::UI_Label: return "Label";
	case UIClass::UI_LinkLabel: return "LinkLabel";
	case UIClass::UI_Button: return "Button";
	case UIClass::UI_TextBox: return "TextBox";
	case UIClass::UI_RichTextBox: return "RichTextBox";
	case UIClass::UI_PasswordBox: return "PasswordBox";
	case UIClass::UI_DateTimePicker: return "DateTimePicker";
	case UIClass::UI_NumericUpDown: return "NumericUpDown";
	case UIClass::UI_Panel: return "Panel";
	case UIClass::UI_GroupBox: return "GroupBox";
	case UIClass::UI_Expander: return "Expander";
	case UIClass::UI_SplitContainer: return "SplitContainer";
	case UIClass::UI_ScrollView: return "ScrollView";
	case UIClass::UI_StackPanel: return "StackPanel";
	case UIClass::UI_GridPanel: return "GridPanel";
	case UIClass::UI_DockPanel: return "DockPanel";
	case UIClass::UI_WrapPanel: return "WrapPanel";
	case UIClass::UI_RelativePanel: return "RelativePanel";
	case UIClass::UI_CheckBox: return "CheckBox";
	case UIClass::UI_RadioBox: return "RadioBox";
	case UIClass::UI_ComboBox: return "ComboBox";
	case UIClass::UI_ListView: return "ListView";
	case UIClass::UI_ListBox: return "ListBox";
	case UIClass::UI_GridView: return "GridView";
	case UIClass::UI_PropertyGrid: return "PropertyGridView";
	case UIClass::UI_ChartView: return "ChartView";
	case UIClass::UI_ReportView: return "ReportView";
	case UIClass::UI_KpiCard: return "KpiCard";
	case UIClass::UI_FilterBar: return "FilterBar";
	case UIClass::UI_TreeView: return "TreeView";
	case UIClass::UI_ProgressBar: return "ProgressBar";
	case UIClass::UI_LoadingRing: return "LoadingRing";
	case UIClass::UI_ProgressRing: return "ProgressRing";
	case UIClass::UI_Slider: return "Slider";
	case UIClass::UI_PictureBox: return "PictureBox";
	case UIClass::UI_Switch: return "Switch";
	case UIClass::UI_TabControl: return "TabControl";
	case UIClass::UI_TabPage: return "TabPage";
	case UIClass::UI_ToolBar: return "ToolBar";
	case UIClass::UI_Menu: return "Menu";
	case UIClass::UI_StatusBar: return "StatusBar";
	case UIClass::UI_ToastHost: return "ToastHost";
	case UIClass::UI_WebBrowser: return "WebBrowser";
	case UIClass::UI_MediaPlayer: return "MediaPlayer";
	default: return "Control";
	}
}

std::string CodeGenerator::GetIncludeForType(UIClass type)
{
	switch (type)
	{
	case UIClass::UI_TabControl:
	case UIClass::UI_TabPage:
		return "TabControl.h";
	case UIClass::UI_LinkLabel:
		return "LinkLabel.h";
	case UIClass::UI_ToolBar:
		return "ToolBar.h";
	case UIClass::UI_StackPanel:
		return "Layout/StackPanel.h";
	case UIClass::UI_GridPanel:
		return "Layout/GridPanel.h";
	case UIClass::UI_DockPanel:
		return "Layout/DockPanel.h";
	case UIClass::UI_WrapPanel:
		return "Layout/WrapPanel.h";
	case UIClass::UI_RelativePanel:
		return "Layout/RelativePanel.h";
	case UIClass::UI_PropertyGrid:
		return "PropertyGrid.h";
	case UIClass::UI_ToastHost:
		return "Toast.h";
	default:
		return GetControlTypeName(type) + ".h";
	}
}

std::string CodeGenerator::EscapeWStringLiteral(const std::wstring& s)
{
	std::wstring out;
	out.reserve(s.size());
	for (wchar_t c : s)
	{
		switch (c)
		{
		case L'\\': out += L"\\\\"; break;
		case L'\"': out += L"\\\""; break;
		case L'\r': out += L"\\r"; break;
		case L'\n': out += L"\\n"; break;
		case L'\t': out += L"\\t"; break;
		default: out.push_back(c); break;
		}
	}
	return WStringToString(out);
}

std::string CodeGenerator::FloatLiteral(float v)
{
	// 生成合法 C++ float 字面量：保证有小数点，再加 f 后缀。
	// 例如：0 -> 0.f，1 -> 1.f，0.25 -> 0.25f
	const float eps = 1e-6f;
	if (v == FLT_MAX) return "FLT_MAX";
	if (v == -FLT_MAX) return "-FLT_MAX";

	if (!std::isfinite(v))
	{
		if (v > 0) return "3.402823e+38f";
		if (v < 0) return "-3.402823e+38f";
		return "0.f";
	}

	float av = std::fabs(v);
	float rounded = std::round(v);
	if (std::fabs(v - rounded) <= eps && av <= (float)INT_MAX)
	{
		std::ostringstream oss;
		oss << (int)rounded << ".f";
		return oss.str();
	}

	std::ostringstream oss;
	if ((av != 0.0f && av < 1e-4f) || av >= 1e6f)
	{
		oss.setf(std::ios::scientific);
		oss.precision(6);
		oss << v;
	}
	else
	{
		oss.setf(std::ios::fixed);
		oss.precision(6);
		oss << v;
	}

	std::string s = oss.str();
	while (!s.empty() && s.find('.') != std::string::npos && s.back() == '0')
		s.pop_back();
	if (!s.empty() && s.back() == '.')
		s.push_back('0');
	return s + "f";
}

std::string CodeGenerator::DoubleLiteral(double v)
{
	const double eps = 1e-9;
	if (!std::isfinite(v))
	{
		if (v > 0) return "1.7976931348623157e+308";
		if (v < 0) return "-1.7976931348623157e+308";
		return "0.0";
	}

	double av = std::fabs(v);
	double rounded = std::round(v);
	if (std::fabs(v - rounded) <= eps && av <= (double)INT_MAX)
	{
		std::ostringstream oss;
		oss << (int)rounded << ".0";
		return oss.str();
	}

	std::ostringstream oss;
	if ((av != 0.0 && av < 1e-6) || av >= 1e9)
	{
		oss.setf(std::ios::scientific);
		oss.precision(12);
		oss << v;
	}
	else
	{
		oss.setf(std::ios::fixed);
		oss.precision(12);
		oss << v;
	}

	std::string s = oss.str();
	while (!s.empty() && s.find('.') != std::string::npos && s.back() == '0')
		s.pop_back();
	if (!s.empty() && s.back() == '.')
		s.push_back('0');
	return s.empty() ? "0.0" : s;
}

std::string CodeGenerator::ColorToString(D2D1_COLOR_F color)
{
	std::ostringstream oss;
	oss << "D2D1::ColorF(" 
		<< FloatLiteral(color.r) << ", "
		<< FloatLiteral(color.g) << ", "
		<< FloatLiteral(color.b) << ", "
		<< FloatLiteral(color.a) << ")";
	return oss.str();
}

std::string CodeGenerator::ThicknessToString(const Thickness& t)
{
	std::ostringstream oss;
	oss << "Thickness(" << FloatLiteral(t.Left) << ", " << FloatLiteral(t.Top) << ", "
		<< FloatLiteral(t.Right) << ", " << FloatLiteral(t.Bottom) << ")";
	return oss.str();
}

std::string CodeGenerator::HorizontalAlignmentToString(::HorizontalAlignment a)
{
	switch (a)
	{
	case HorizontalAlignment::Left: return "HorizontalAlignment::Left";
	case HorizontalAlignment::Center: return "HorizontalAlignment::Center";
	case HorizontalAlignment::Right: return "HorizontalAlignment::Right";
	case HorizontalAlignment::Stretch: return "HorizontalAlignment::Stretch";
	default: return "HorizontalAlignment::Left";
	}
}

std::string CodeGenerator::VerticalAlignmentToString(::VerticalAlignment a)
{
	switch (a)
	{
	case VerticalAlignment::Top: return "VerticalAlignment::Top";
	case VerticalAlignment::Center: return "VerticalAlignment::Center";
	case VerticalAlignment::Bottom: return "VerticalAlignment::Bottom";
	case VerticalAlignment::Stretch: return "VerticalAlignment::Stretch";
	default: return "VerticalAlignment::Top";
	}
}

std::string CodeGenerator::DockToString(::Dock d)
{
	switch (d)
	{
	case Dock::Left: return "Dock::Left";
	case Dock::Top: return "Dock::Top";
	case Dock::Right: return "Dock::Right";
	case Dock::Bottom: return "Dock::Bottom";
	case Dock::Fill: return "Dock::Fill";
	default: return "Dock::Fill";
	}
}

std::string CodeGenerator::SizeUnitToString(SizeUnit u)
{
	switch (u)
	{
	case SizeUnit::Pixel: return "SizeUnit::Pixel";
	case SizeUnit::Percent: return "SizeUnit::Percent";
	case SizeUnit::Auto: return "SizeUnit::Auto";
	case SizeUnit::Star: return "SizeUnit::Star";
	default: return "SizeUnit::Pixel";
	}
}

std::string CodeGenerator::GridLengthToCtorString(const GridLength& gl)
{
	// 优先用静态工厂，生成更可读
	if (gl.Unit == SizeUnit::Auto)
		return "GridLength::Auto()";
	if (gl.Unit == SizeUnit::Star)
	{
		std::ostringstream oss;
		oss << "GridLength::Star(" << FloatLiteral(gl.Value) << ")";
		return oss.str();
	}
	if (gl.Unit == SizeUnit::Percent)
	{
		std::ostringstream oss;
		oss << "GridLength::Percent(" << FloatLiteral(gl.Value) << ")";
		return oss.str();
	}
	// Pixel / fallback
	{
		std::ostringstream oss;
		oss << "GridLength::Pixels(" << FloatLiteral(gl.Value) << ")";
		return oss.str();
	}
}

std::string CodeGenerator::GenerateControlInstantiation(const std::shared_ptr<DesignerControl>& dc, int indent)
{
	if (!dc || !dc->ControlInstance) return "";
	if (dc->Type == UIClass::UI_TabPage) return ""; // TabPage 通过 TabControl::AddPage 创建

	auto* control = dc->ControlInstance;
	std::ostringstream code;
	std::string indentStr(indent, '\t');
	std::string name = GetVarName(dc);
	std::string typeName = GetControlTypeName(dc->Type);

	code << indentStr << "// " << name << "\n";
	code << indentStr << "auto __owned_" << name << " = std::make_unique<" << typeName << ">(";

	switch (dc->Type)
	{
	case UIClass::UI_Label:
	case UIClass::UI_LinkLabel:
	case UIClass::UI_CheckBox:
	case UIClass::UI_RadioBox:
		code << "L\"" << EscapeWStringLiteral(control->Text) << "\", "
			<< control->Location.x << ", " << control->Location.y;
		break;
	case UIClass::UI_Button:
		code << "L\"" << EscapeWStringLiteral(control->Text) << "\", "
			<< control->Location.x << ", " << control->Location.y << ", "
			<< control->Size.cx << ", " << control->Size.cy;
		break;
	case UIClass::UI_TextBox:
	case UIClass::UI_RichTextBox:
	case UIClass::UI_PasswordBox:
	case UIClass::UI_DateTimePicker:
	case UIClass::UI_ComboBox:
	case UIClass::UI_GroupBox:
	case UIClass::UI_Expander:
		code << "L\"" << EscapeWStringLiteral(control->Text) << "\", "
			<< control->Location.x << ", " << control->Location.y << ", "
			<< control->Size.cx << ", " << control->Size.cy;
		break;
	case UIClass::UI_Panel:
	case UIClass::UI_SplitContainer:
	case UIClass::UI_ScrollView:
	case UIClass::UI_StackPanel:
	case UIClass::UI_GridPanel:
	case UIClass::UI_DockPanel:
	case UIClass::UI_WrapPanel:
	case UIClass::UI_RelativePanel:
	case UIClass::UI_ProgressBar:
	case UIClass::UI_LoadingRing:
	case UIClass::UI_ProgressRing:
	case UIClass::UI_Slider:
	case UIClass::UI_NumericUpDown:
	case UIClass::UI_PictureBox:
	case UIClass::UI_Switch:
	case UIClass::UI_ListView:
	case UIClass::UI_ListBox:
	case UIClass::UI_GridView:
	case UIClass::UI_PropertyGrid:
	case UIClass::UI_ChartView:
	case UIClass::UI_ReportView:
	case UIClass::UI_KpiCard:
	case UIClass::UI_FilterBar:
	case UIClass::UI_TreeView:
	case UIClass::UI_TabControl:
	case UIClass::UI_ToolBar:
	case UIClass::UI_ToastHost:
	case UIClass::UI_WebBrowser:
		code << control->Location.x << ", " << control->Location.y << ", "
			<< control->Size.cx << ", " << control->Size.cy;
		break;
	default:
		code << control->Location.x << ", " << control->Location.y << ", "
			<< control->Size.cx << ", " << control->Size.cy;
		break;
	}

	code << ");\n";
	code << indentStr << name << " = __owned_" << name << ".get();\n";

	return code.str();
}

std::string CodeGenerator::GenerateControlCommonProperties(const std::shared_ptr<DesignerControl>& dc, int indent)
{
	if (!dc || !dc->ControlInstance) return "";
	if (dc->Type == UIClass::UI_TabPage) return "";

	auto* control = dc->ControlInstance;
	std::ostringstream code;
	std::string indentStr(indent, '\t');
	std::string name = GetVarName(dc);

	// 尺寸：Label/CheckBox/RadioBox 构造函数无 size
	if (dc->Type == UIClass::UI_Label || dc->Type == UIClass::UI_LinkLabel || dc->Type == UIClass::UI_CheckBox || dc->Type == UIClass::UI_RadioBox)
	{
		code << indentStr << name << "->Size = {" << control->Size.cx << ", " << control->Size.cy << "};\n";
	}

	// 对于不在构造函数中写入 Text 的控件：补齐 Text
	if (dc->Type != UIClass::UI_Label && dc->Type != UIClass::UI_LinkLabel && dc->Type != UIClass::UI_Button &&
		dc->Type != UIClass::UI_CheckBox && dc->Type != UIClass::UI_RadioBox &&
		dc->Type != UIClass::UI_TextBox && dc->Type != UIClass::UI_RichTextBox &&
		dc->Type != UIClass::UI_PasswordBox && dc->Type != UIClass::UI_DateTimePicker &&
		dc->Type != UIClass::UI_ComboBox && dc->Type != UIClass::UI_GroupBox &&
		dc->Type != UIClass::UI_Expander)
	{
		if (!control->Text.empty())
			code << indentStr << name << "->Text = L\"" << EscapeWStringLiteral(control->Text) << "\";\n";
	}


	if (!control->Enable)
		code << indentStr << name << "->Enable = false;\n";
	if (!control->Visible)
		code << indentStr << name << "->Visible = false;\n";
	if (!control->GetStyleId().empty())
		code << indentStr << name << "->SetStyleId(L\""
			<< EscapeWStringLiteral(control->GetStyleId()) << "\");\n";
	for (const auto& styleClass : control->GetStyleClasses())
		code << indentStr << name << "->AddStyleClass(L\""
			<< EscapeWStringLiteral(styleClass) << "\");\n";

	// Font：默认字体不输出；若窗体 Font 与框架默认不同，则用共享 __formFont 绑定“默认控件”
	{
		auto* def = GetDefaultFontObject();
		std::wstring defName = def ? def->FontName : L"Arial";
		float defSize = def ? def->FontSize : 18.0f;
		std::wstring formNameW = _formFontName.empty() ? defName : _formFontName;
		float formSize = _formFontSize;
		bool formHasShared = !(_formFontName.empty() && formNameW == defName && std::fabs(formSize - defSize) < 1e-6f);

		std::wstring curNameW = control->Font ? control->Font->FontName : defName;
		float curSize = control->Font ? control->Font->FontSize : defSize;
		auto feq = [](float a, float b) { return std::fabs(a - b) < 1e-3f; };

		if (formHasShared && curNameW == formNameW && feq(curSize, formSize))
		{
			code << indentStr << name << "->SetFontEx(__formFont, false);\n";
		}
		else
		{
			if (!(curNameW == defName && feq(curSize, defSize)))
			{
				code << indentStr << name << "->Font = new ::Font(L\"" << EscapeWStringLiteral(curNameW) << "\", "
					<< FloatLiteral(curSize) << ");\n";
			}
		}
	}

	// 颜色
	code << indentStr << name << "->BackColor = " << ColorToString(control->BackColor) << ";\n";
	code << indentStr << name << "->ForeColor = " << ColorToString(control->ForeColor) << ";\n";
	code << indentStr << name << "->BorderColor = " << ColorToString(control->BorderColor) << ";\n";
	if (!control->IsPropertyValueDefault(L"ShowValidationBorder"))
		code << indentStr << name << "->ShowValidationBorder = "
			<< (control->ShowValidationBorder ? "true" : "false") << ";\n";
	if (!control->IsPropertyValueDefault(L"ShowValidationToolTip"))
		code << indentStr << name << "->ShowValidationToolTip = "
			<< (control->ShowValidationToolTip ? "true" : "false") << ";\n";
	if (!control->IsPropertyValueDefault(L"ValidationBorderThickness"))
		code << indentStr << name << "->ValidationBorderThickness = "
			<< FloatLiteral(control->ValidationBorderThickness) << ";\n";
	if (!control->IsPropertyValueDefault(L"ValidationCornerRadius"))
		code << indentStr << name << "->ValidationCornerRadius = "
			<< FloatLiteral(control->ValidationCornerRadius) << ";\n";
	if (!control->IsPropertyValueDefault(L"ValidationToolTipMaxWidth"))
		code << indentStr << name << "->ValidationToolTipMaxWidth = "
			<< FloatLiteral(control->ValidationToolTipMaxWidth) << ";\n";
	if (!control->IsPropertyValueDefault(L"AccessibleDescription"))
		code << indentStr << name << "->AccessibleDescription = L\""
			<< EscapeWStringLiteral(control->AccessibleDescription) << "\";\n";

	// 布局通用属性
	auto m = control->Margin;
	if (m.Left != 0.0f || m.Top != 0.0f || m.Right != 0.0f || m.Bottom != 0.0f)
		code << indentStr << name << "->Margin = " << ThicknessToString(m) << ";\n";
	if (control->AnchorStyles != AnchorStyles::None)
		code << indentStr << name << "->AnchorStyles = " << AnchorStylesToExpr(control->AnchorStyles) << ";\n";
		if (control->ZIndex != 0)
			code << indentStr << name << "->ZIndex = " << control->ZIndex << ";\n";
	auto p = control->Padding;
	if (p.Left != 0.0f || p.Top != 0.0f || p.Right != 0.0f || p.Bottom != 0.0f)
		code << indentStr << name << "->Padding = " << ThicknessToString(p) << ";\n";

	// Min/MaxSize（只在用户改过时输出；不精确比较，保守输出非默认）
	if (control->MinSize.cx != 0 || control->MinSize.cy != 0)
		code << indentStr << name << "->MinSize = {" << control->MinSize.cx << ", " << control->MinSize.cy << "};\n";
	if (control->MaxSize.cx != INT_MAX || control->MaxSize.cy != INT_MAX)
		code << indentStr << name << "->MaxSize = {" << control->MaxSize.cx << ", " << control->MaxSize.cy << "};\n";

	// ComboBox items
	if (dc->Type == UIClass::UI_ComboBox)
	{
		auto* comboBox = (ComboBox*)control;
		if (comboBox->Items.size() > 0)
		{
			const std::string itemsName = "__comboItems_" + name;
			code << indentStr << "std::vector<std::wstring> " << itemsName << "{\n";
			for (size_t i = 0; i < comboBox->Items.size(); ++i)
			{
				code << indentStr << "\tL\""
					<< EscapeWStringLiteral(comboBox->Items[i]) << "\"";
				if (i + 1 < comboBox->Items.size()) code << ",";
				code << "\n";
			}
			code << indentStr << "};\n";
			code << indentStr << name << "->Items = " << itemsName << ";\n";
		}
	}

	// ProgressBar
	if (dc->Type == UIClass::UI_ProgressBar)
	{
		auto* progressBar = (ProgressBar*)control;
		// 默认值 0.5f
		if (std::fabs(progressBar->PercentageValue - 0.5f) > 1e-6f)
			code << indentStr << name << "->PercentageValue = " << FloatLiteral(progressBar->PercentageValue) << ";\n";
	}

	if (dc->Type == UIClass::UI_LoadingRing)
	{
		auto* loadingRing = (LoadingRing*)control;
		if (!loadingRing->Active)
			code << indentStr << name << "->Active = false;\n";
	}

	if (dc->Type == UIClass::UI_ProgressRing)
	{
		auto* progressRing = (ProgressRing*)control;
		if (std::fabs(progressRing->PercentageValue - 0.5f) > 1e-6f)
			code << indentStr << name << "->PercentageValue = " << FloatLiteral(progressRing->PercentageValue) << ";\n";
		if (!progressRing->ShowPercentage)
			code << indentStr << name << "->ShowPercentage = false;\n";
	}

	// DateTimePicker
	if (dc->Type == UIClass::UI_DateTimePicker)
	{
		auto* dateTimePicker = (DateTimePicker*)control;
		code << indentStr << "{\n";
		code << indentStr << "\tSYSTEMTIME __dateTimePickerValue{};\n";
		code << indentStr << "\t__dateTimePickerValue.wYear = " << dateTimePicker->Value.wYear << ";\n";
		code << indentStr << "\t__dateTimePickerValue.wMonth = " << dateTimePicker->Value.wMonth << ";\n";
		code << indentStr << "\t__dateTimePickerValue.wDay = " << dateTimePicker->Value.wDay << ";\n";
		code << indentStr << "\t__dateTimePickerValue.wHour = " << dateTimePicker->Value.wHour << ";\n";
		code << indentStr << "\t__dateTimePickerValue.wMinute = " << dateTimePicker->Value.wMinute << ";\n";
		code << indentStr << "\t__dateTimePickerValue.wSecond = " << dateTimePicker->Value.wSecond << ";\n";
		code << indentStr << "\t__dateTimePickerValue.wMilliseconds = " << dateTimePicker->Value.wMilliseconds << ";\n";
		code << indentStr << "\t" << name << "->Value = __dateTimePickerValue;\n";
		code << indentStr << "}\n";
		const char* mode = "DateTimePickerMode::DateTime";
		switch (dateTimePicker->Mode)
		{
		case DateTimePickerMode::DateOnly: mode = "DateTimePickerMode::DateOnly"; break;
		case DateTimePickerMode::TimeOnly: mode = "DateTimePickerMode::TimeOnly"; break;
		case DateTimePickerMode::DateTime: default: mode = "DateTimePickerMode::DateTime"; break;
		}
		code << indentStr << name << "->Mode = " << mode << ";\n";
		if (!dateTimePicker->AllowDateSelection) code << indentStr << name << "->AllowDateSelection = false;\n";
		if (!dateTimePicker->AllowTimeSelection) code << indentStr << name << "->AllowTimeSelection = false;\n";
		if (!dateTimePicker->AllowModeSwitch) code << indentStr << name << "->AllowModeSwitch = false;\n";
		if (dateTimePicker->Expand) code << indentStr << name << "->SetExpanded(true);\n";
	}

	// PictureBox (SizeMode is on base Control)
	if (dc->Type == UIClass::UI_PictureBox)
	{
		if (control->SizeMode != ImageSizeMode::Zoom)
		{
			switch (control->SizeMode)
			{
			case ImageSizeMode::Normal: code << indentStr << name << "->SizeMode = ImageSizeMode::Normal;\n"; break;
			case ImageSizeMode::CenterImage: code << indentStr << name << "->SizeMode = ImageSizeMode::CenterImage;\n"; break;
			case ImageSizeMode::StretchImage: code << indentStr << name << "->SizeMode = ImageSizeMode::StretchImage;\n"; break;
			case ImageSizeMode::Zoom: code << indentStr << name << "->SizeMode = ImageSizeMode::Zoom;\n"; break;
			default: break;
			}
		}
	}

	// TreeView colors
	if (dc->Type == UIClass::UI_TreeView)
	{
		auto* treeView = (TreeView*)control;
		// 这些默认值在 TreeView.h 里有初始化，这里做“保守输出”：只要与默认不同就生成
		const D2D1_COLOR_F defSelBack = D2D1_COLOR_F{ 0.3882f, 0.4000f, 0.9451f, 0.14f };
		const D2D1_COLOR_F defUnderBack = D2D1_COLOR_F{ 0.3882f, 0.4000f, 0.9451f, 0.08f };
		const D2D1_COLOR_F defSelFore = Colors::Black;
		auto neq = [](const D2D1_COLOR_F& a, const D2D1_COLOR_F& b) {
			return std::fabs(a.r - b.r) > 1e-6f || std::fabs(a.g - b.g) > 1e-6f || std::fabs(a.b - b.b) > 1e-6f || std::fabs(a.a - b.a) > 1e-6f;
		};
		if (neq(treeView->SelectedBackColor, defSelBack))
			code << indentStr << name << "->SelectedBackColor = " << ColorToString(treeView->SelectedBackColor) << ";\n";
		if (neq(treeView->UnderMouseItemBackColor, defUnderBack))
			code << indentStr << name << "->UnderMouseItemBackColor = " << ColorToString(treeView->UnderMouseItemBackColor) << ";\n";
		if (neq(treeView->SelectedForeColor, defSelFore))
			code << indentStr << name << "->SelectedForeColor = " << ColorToString(treeView->SelectedForeColor) << ";\n";

		// TreeView nodes
		if (treeView->Root && treeView->Root->Children.size() > 0)
		{
			code << indentStr << "// TreeView nodes\n";
			code << indentStr << "for (auto node : " << name << "->Root->Children) delete node;\n";
			code << indentStr << name << "->Root->Children.Clear();\n";

			int nodeAutoId = 0;
			auto emitTreeNodes = [&](auto&& self, std::vector<TreeNode*>& nodes, const std::string& parentExpr) -> void
			{
				for (auto* node : nodes)
				{
					if (!node) continue;
					std::string nodeVar = name + "_node" + std::to_string(++nodeAutoId);
					code << indentStr << "auto* " << nodeVar << " = new TreeNode(L\"" << EscapeWStringLiteral(node->Text) << "\");\n";
					if (node->Expand)
						code << indentStr << nodeVar << "->Expand = true;\n";
					code << indentStr << parentExpr << "->Children.push_back(" << nodeVar << ");\n";
					if (node->Children.size() > 0)
						self(self, node->Children, nodeVar);
				}
			};
			emitTreeNodes(emitTreeNodes, treeView->Root->Children, name + "->Root");
		}
	}

	// StatusBar parts 仍是结构化集合；标量配置由通用元数据生成。
	if (dc->Type == UIClass::UI_StatusBar)
	{
		auto* statusBar = (StatusBar*)control;
		int partCount = statusBar->PartCount();
		if (partCount > 0)
		{
			code << indentStr << name << "->ClearParts();\n";
			for (int i = 0; i < partCount; i++)
			{
				code << indentStr << name << "->AddPart(L\"" << EscapeWStringLiteral(statusBar->GetPartText(i))
					<< "\", " << statusBar->GetPartWidth(i) << ");\n";
			}
		}
	}

	// Menu 基本参数 + items
	if (dc->Type == UIClass::UI_Menu)
	{
		auto* menu = (Menu*)control;
		if (menu->BarHeight != 28)
			code << indentStr << name << "->BarHeight = " << menu->BarHeight << ";\n";
		if (menu->DropItemHeight != 26)
			code << indentStr << name << "->DropItemHeight = " << menu->DropItemHeight << ";\n";
		if (std::fabs(menu->BorderThickness - 1.0f) > 1e-6f)
			code << indentStr << name << "->BorderThickness = " << FloatLiteral(menu->BorderThickness) << ";\n";

		if (menu->Count > 0)
		{
			code << indentStr << "// Menu items\n";
			code << indentStr << "while (" << name << "->Count > 0)\n";
			code << indentStr << "{\n";
			std::string innerIndentStr(indent + 1, '\t');
			code << innerIndentStr << "auto* menuItem = (MenuItem*)" << name << "->operator[](" << name << "->Count - 1);\n";
			code << innerIndentStr << name << "->DeleteControl(menuItem);\n";
			code << indentStr << "}\n";

			int menuAutoId = 0;
			auto emitItemProps = [&](MenuItem* menuItem, const std::string& itemVar)
			{
				if (!menuItem) return;
				if (menuItem->Id != 0)
					code << indentStr << itemVar << "->Id = " << menuItem->Id << ";\n";
				if (!menuItem->Enable)
					code << indentStr << itemVar << "->Enable = false;\n";
				if (!menuItem->Shortcut.empty())
					code << indentStr << itemVar << "->Shortcut = L\"" << EscapeWStringLiteral(menuItem->Shortcut) << "\";\n";
				if (menuItem->Separator)
					code << indentStr << itemVar << "->Separator = true;\n";
			};

			std::function<void(MenuItem* parentItem, const std::string& parentVar)> emitSubItems;
			emitSubItems = [&](MenuItem* parentItem, const std::string& parentVar)
			{
				if (!parentItem) return;
				for (auto* subItem : parentItem->SubItems)
				{
					if (!subItem) continue;
					if (subItem->Separator)
					{
						code << indentStr << parentVar << "->AddSeparator();\n";
						continue;
					}
					std::string itemVar = name + "_item" + std::to_string(++menuAutoId);
					code << indentStr << "auto* " << itemVar << " = " << parentVar << "->AddSubItem(L\"" << EscapeWStringLiteral(subItem->Text)
						<< "\", " << subItem->Id << ");\n";
					emitItemProps(subItem, itemVar);
					if (!subItem->SubItems.empty())
						emitSubItems(subItem, itemVar);
				}
			};

			for (int i = 0; i < menu->Count; i++)
			{
				auto* topItem = (MenuItem*)menu->operator[](i);
				if (!topItem) continue;
				std::string itemVar = name + "_item" + std::to_string(++menuAutoId);
				code << indentStr << "auto* " << itemVar << " = " << name << "->AddItem(L\"" << EscapeWStringLiteral(topItem->Text) << "\");\n";
				emitItemProps(topItem, itemVar);
				if (!topItem->SubItems.empty())
					emitSubItems(topItem, itemVar);
			}
		}
	}

	return code.str();
}

std::string CodeGenerator::GenerateMetadataProperties(
	const std::shared_ptr<DesignerControl>& dc,
	int indent)
{
	if (!dc || !dc->ControlInstance || dc->Type == UIClass::UI_TabPage
		|| dc->MetadataProperties.empty()) return "";

	std::ostringstream code;
	const std::string indentStr(indent, '\t');
	const std::string name = GetVarName(dc);
	code << indentStr << "// 属性元数据扩展\n";
	std::vector<std::pair<std::wstring, DesignerStyleValue>> orderedProperties(
		dc->MetadataProperties.begin(), dc->MetadataProperties.end());
	std::stable_sort(orderedProperties.begin(), orderedProperties.end(),
		[&](const auto& left, const auto& right)
		{
			const auto* leftMetadata = dc->ControlInstance->FindPropertyMetadata(left.first);
			const auto* rightMetadata = dc->ControlInstance->FindPropertyMetadata(right.first);
			if (leftMetadata && rightMetadata)
			{
				const auto& leftDesign = leftMetadata->Design();
				const auto& rightDesign = rightMetadata->Design();
				if (leftDesign.CategoryOrder != rightDesign.CategoryOrder)
					return leftDesign.CategoryOrder < rightDesign.CategoryOrder;
				if (leftDesign.Order != rightDesign.Order)
					return leftDesign.Order < rightDesign.Order;
			}
			else if (leftMetadata != rightMetadata)
			{
				return leftMetadata != nullptr;
			}
			return _wcsicmp(left.first.c_str(), right.first.c_str()) < 0;
		});
	for (const auto& [storedName, storedValue] : orderedProperties)
	{
		std::wstring canonicalName = storedName;
		DesignerStyleValue effectiveValue = storedValue;
		(void)DesignerPropertyCatalog::CaptureValue(
			*dc->ControlInstance,
			storedName,
			&canonicalName,
			effectiveValue);
		code << indentStr << "(void)" << name
			<< "->TrySetPropertyValue(L\"" << EscapeWStringLiteral(canonicalName)
			<< "\", " << GenerateStyleValueExpression(effectiveValue) << ");\n";
	}
	return code.str();
}

std::string CodeGenerator::GenerateStyleValueExpression(const DesignerStyleValue& value)
{
	BindingValue runtimeValue;
	if (!DesignerStyleSheetUtils::TryConvertValue(value, runtimeValue, nullptr))
		return "BindingValue()";

	switch (value.Kind)
	{
	case DesignerStyleValueKind::Bool:
	{
		bool parsed = false;
		runtimeValue.TryGet(parsed);
		return std::string("BindingValue(") + (parsed ? "true" : "false") + ")";
	}
	case DesignerStyleValueKind::Int:
	{
		int parsed = 0;
		runtimeValue.TryGet(parsed);
		return "BindingValue(" + std::to_string(parsed) + ")";
	}
	case DesignerStyleValueKind::Int64:
	{
		long long parsed = 0;
		runtimeValue.TryGet(parsed);
		return "BindingValue(" + std::to_string(parsed) + "LL)";
	}
	case DesignerStyleValueKind::Float:
	{
		float parsed = 0.0f;
		runtimeValue.TryGet(parsed);
		return "BindingValue(" + FloatLiteral(parsed) + ")";
	}
	case DesignerStyleValueKind::Double:
	{
		double parsed = 0.0;
		runtimeValue.TryGet(parsed);
		return "BindingValue(" + DoubleLiteral(parsed) + ")";
	}
	case DesignerStyleValueKind::String:
	{
		std::wstring parsed;
		runtimeValue.TryGet(parsed);
		return "BindingValue(L\"" + EscapeWStringLiteral(parsed) + "\")";
	}
	case DesignerStyleValueKind::Color:
	{
		D2D1_COLOR_F parsed{};
		runtimeValue.TryGet(parsed);
		return "BindingValue(" + ColorToString(parsed) + ")";
	}
	case DesignerStyleValueKind::Thickness:
	{
		Thickness parsed;
		runtimeValue.TryGet(parsed);
		return "BindingValue(" + ThicknessToString(parsed) + ")";
	}
	case DesignerStyleValueKind::Size:
	{
		SIZE parsed{};
		runtimeValue.TryGet(parsed);
		return "BindingValue(SIZE{ " + std::to_string(parsed.cx) + ", "
			+ std::to_string(parsed.cy) + " })";
	}
	case DesignerStyleValueKind::Length:
	{
		cui::layout::Length parsed;
		runtimeValue.TryGet(parsed);
		return parsed.IsAuto()
			? "BindingValue(cui::layout::Length::Auto())"
			: "BindingValue(cui::layout::Length::Fixed(" + FloatLiteral(parsed.value) + "))";
	}
	}
	return "BindingValue()";
}

std::string CodeGenerator::GenerateStyleSheetCode(int indent)
{
	if (_styleSheet.Empty()) return "";
	std::ostringstream code;
	const std::string indentStr(indent, '\t');
	auto styleSheet = _styleSheet;
	DesignerStyleSheetUtils::Canonicalize(styleSheet);

	code << indentStr << "// 文档级控件样式\n";
	code << indentStr << "auto __styleSheet = std::make_shared<ControlStyleSheet>();\n";
	for (const auto& resource : styleSheet.Resources)
	{
		code << indentStr << "__styleSheet->SetResource(L\""
			<< EscapeWStringLiteral(resource.Key) << "\", "
			<< GenerateStyleValueExpression(resource.Value) << ");\n";
	}
	for (size_t index = 0; index < styleSheet.Rules.size(); ++index)
	{
		const auto& rule = styleSheet.Rules[index];
		const auto selectorName = "__styleSelector" + std::to_string(index + 1);
		code << indentStr << "ControlStyleSelector " << selectorName << ";\n";
		if (rule.HasType)
		{
			code << indentStr << selectorName << ".Type = UIClass::UI_"
				<< WStringToString(DesignerStyleSheetUtils::UIClassName(rule.Type)) << ";\n";
		}
		if (!rule.Id.empty())
			code << indentStr << selectorName << ".Id = L\""
				<< EscapeWStringLiteral(rule.Id) << "\";\n";
		for (const auto& styleClass : rule.Classes)
			code << indentStr << selectorName << ".Classes.push_back(L\""
				<< EscapeWStringLiteral(styleClass) << "\");\n";
		if (rule.RequiredStates != ControlStyleState::None)
			code << indentStr << selectorName << ".RequiredStates = static_cast<ControlStyleState>("
				<< static_cast<uint32_t>(rule.RequiredStates) << "u);\n";
		if (rule.ExcludedStates != ControlStyleState::None)
			code << indentStr << selectorName << ".ExcludedStates = static_cast<ControlStyleState>("
				<< static_cast<uint32_t>(rule.ExcludedStates) << "u);\n";
		code << indentStr << "__styleSheet->AddRule(std::move(" << selectorName << "), {\n";
		for (size_t setterIndex = 0; setterIndex < rule.Setters.size(); ++setterIndex)
		{
			const auto& setter = rule.Setters[setterIndex];
			code << indentStr << "\t";
			if (setter.UsesResource)
				code << "ControlStyleSetter::Resource(L\""
					<< EscapeWStringLiteral(setter.PropertyName) << "\", L\""
					<< EscapeWStringLiteral(setter.ResourceKey) << "\")";
			else
				code << "ControlStyleSetter(L\"" << EscapeWStringLiteral(setter.PropertyName)
					<< "\", " << GenerateStyleValueExpression(setter.Literal) << ")";
			if (setterIndex + 1 < rule.Setters.size()) code << ",";
			code << "\n";
		}
		code << indentStr << "});\n";
	}
	code << indentStr << "this->SetStyleSheet(__styleSheet);\n\n";
	return code.str();
}

std::string CodeGenerator::GenerateContainerProperties(const std::shared_ptr<DesignerControl>& dc, int indent)
{
	if (!dc || !dc->ControlInstance) return "";
	if (dc->Type == UIClass::UI_TabPage) return "";

	auto* control = dc->ControlInstance;
	std::ostringstream code;
	std::string indentStr(indent, '\t');
	std::string name = GetVarName(dc);

	// 元数据已先生成；最后 Load，确保 AutoPlay/Loop/解码策略在加载前生效。
	if (dc->Type == UIClass::UI_MediaPlayer)
	{
		auto* mediaPlayer = (MediaPlayer*)control;
		auto mediaFileIt = dc->DesignStrings.find(L"mediaFile");
		const std::wstring mediaFile = mediaFileIt != dc->DesignStrings.end()
			? mediaFileIt->second : mediaPlayer->MediaFile;
		if (!mediaFile.empty())
			code << indentStr << name << "->Load(L\""
				<< EscapeWStringLiteral(mediaFile) << "\");\n";
	}

	if (dc->Type == UIClass::UI_GridPanel)
	{
		auto* gridPanel = (GridPanel*)control;
		const auto& rows = gridPanel->GetRows();
		const auto& columns = gridPanel->GetColumns();
		if (!rows.empty() || !columns.empty())
		{
			code << indentStr << name << "->ClearRows();\n";
			code << indentStr << name << "->ClearColumns();\n";
			for (auto& row : rows)
			{
				code << indentStr << name << "->AddRow(" << GridLengthToCtorString(row.Height)
					<< ", " << FloatLiteral(row.MinHeight) << ", " << FloatLiteral(row.MaxHeight) << ");\n";
			}
			for (auto& column : columns)
			{
				code << indentStr << name << "->AddColumn(" << GridLengthToCtorString(column.Width)
					<< ", " << FloatLiteral(column.MinWidth) << ", " << FloatLiteral(column.MaxWidth) << ");\n";
			}
		}
	}

	if (dc->Type == UIClass::UI_ListView || dc->Type == UIClass::UI_ListBox)
	{
		auto* listView = (ListView*)control;
		if (!listView->Columns.empty())
		{
			for (const auto& col : listView->Columns)
			{
				code << indentStr << name << "->AddColumn(ListViewColumn(L\""
					<< EscapeWStringLiteral(col.Header) << "\", "
					<< FloatLiteral(col.Width)
					<< ", static_cast<ListViewCellAlign>(" << (int)col.Align << ")));\n";
			}
		}
		if (!listView->Items.empty())
		{
			const std::string itemsName = "__listItems_" + name;
			code << indentStr << "std::vector<ListViewItem> " << itemsName << ";\n";
			code << indentStr << itemsName << ".reserve("
				<< listView->Items.size() << ");\n";
			for (int i = 0; i < (int)listView->Items.size(); ++i)
			{
				const auto& item = listView->Items[static_cast<size_t>(i)];
				const std::string itemVar = "__listItem_" + name
					+ "_" + std::to_string(i + 1);
				code << indentStr << "ListViewItem " << itemVar << "(L\""
					<< EscapeWStringLiteral(item.Text) << "\", L\""
					<< EscapeWStringLiteral(item.SubText) << "\");\n";
				for (const auto& sub : item.SubItems)
					code << indentStr << itemVar << ".SubItems.push_back(L\""
						<< EscapeWStringLiteral(sub) << "\");\n";
				if (item.Checked)
					code << indentStr << itemVar << ".Checked = true;\n";
				if (item.Selected)
					code << indentStr << itemVar << ".Selected = true;\n";
				if (!item.Enabled)
					code << indentStr << itemVar << ".Enabled = false;\n";
				code << indentStr << itemsName << ".push_back(std::move("
					<< itemVar << "));\n";
			}
			code << indentStr << name << "->SetItems(std::move("
				<< itemsName << "));\n";
		}
	}

	// GridView 列是结构化集合；在标量元数据之后批量生成，避免重复布局/重绘。
	if (dc->Type == UIClass::UI_GridView)
	{
		auto* gridView = (GridView*)control;
		if (gridView->ColumnCount() > 0)
		{
			code << indentStr << "{\n";
			code << indentStr << "\tauto __gridUpdate = " << name << "->DeferUpdates();\n";
			code << indentStr << "\t" << name << "->ClearColumns();\n";
			for (size_t i = 0; i < gridView->ColumnCount(); ++i)
			{
				const auto& column = gridView->ColumnAt(static_cast<int>(i));
				std::string columnTypeExpr = "ColumnType::Text";
				switch (column.Type)
				{
				case ColumnType::Text: columnTypeExpr = "ColumnType::Text"; break;
				case ColumnType::LinkedText: columnTypeExpr = "ColumnType::LinkedText"; break;
				case ColumnType::Image: columnTypeExpr = "ColumnType::Image"; break;
				case ColumnType::Check: columnTypeExpr = "ColumnType::Check"; break;
				case ColumnType::Button: columnTypeExpr = "ColumnType::Button"; break;
				case ColumnType::ComboBox: columnTypeExpr = "ColumnType::ComboBox"; break;
				default: break;
				}
				const std::string columnVar = "__gridColumn" + std::to_string(i + 1);
				code << indentStr << "\tGridViewColumn " << columnVar << "(L\""
					<< EscapeWStringLiteral(column.Name) << "\", "
					<< FloatLiteral(column.Width) << ", " << columnTypeExpr << ", "
					<< (column.CanEdit ? "true" : "false") << ");\n";
				if (!column.ButtonText.empty())
					code << indentStr << "\t" << columnVar << ".ButtonText = L\""
						<< EscapeWStringLiteral(column.ButtonText) << "\";\n";
				if (!column.ComboBoxItems.empty())
				{
					code << indentStr << "\t" << columnVar << ".ComboBoxItems = { ";
					for (size_t itemIndex = 0; itemIndex < column.ComboBoxItems.size(); ++itemIndex)
					{
						if (itemIndex > 0) code << ", ";
						code << "L\"" << EscapeWStringLiteral(column.ComboBoxItems[itemIndex]) << "\"";
					}
					code << " };\n";
				}
				code << indentStr << "\t" << name << "->AddColumn(" << columnVar << ");\n";
			}
			code << indentStr << "}\n";
		}
	}

	// PropertyGrid 标量由元数据生成；Items 保持结构化并在标量之后原子替换。
	if (dc->Type == UIClass::UI_PropertyGrid)
	{
		auto* propertyGrid = (PropertyGridView*)control;
		if (!propertyGrid->Items.empty())
		{
			const std::string itemsName = "__propertyItems_" + name;
			code << indentStr << "std::vector<PropertyGridItem> " << itemsName << ";\n";
			code << indentStr << itemsName << ".reserve("
				<< propertyGrid->Items.size() << ");\n";
			for (int i = 0; i < (int)propertyGrid->Items.size(); ++i)
			{
				const auto& item = propertyGrid->Items[static_cast<size_t>(i)];
				const std::string itemVar = "__propertyItem_" + name
					+ "_" + std::to_string(i + 1);
				code << indentStr << "PropertyGridItem " << itemVar << "(L\""
					<< EscapeWStringLiteral(item.Category) << "\", L\""
					<< EscapeWStringLiteral(item.Name) << "\", L\""
					<< EscapeWStringLiteral(item.Value)
					<< "\", static_cast<PropertyGridValueType>(" << (int)item.ValueType
					<< "));\n";
				if (!item.Description.empty())
					code << indentStr << itemVar << ".Description = L\""
						<< EscapeWStringLiteral(item.Description) << "\";\n";
				if (item.ReadOnly)
					code << indentStr << itemVar << ".ReadOnly = true;\n";
				if (item.Tag != 0)
					code << indentStr << itemVar << ".Tag = "
						<< static_cast<unsigned long long>(item.Tag) << "ULL;\n";
				for (const auto& option : item.Options)
					code << indentStr << itemVar << ".Options.push_back(L\""
						<< EscapeWStringLiteral(option) << "\");\n";
				code << indentStr << itemsName << ".push_back(std::move("
					<< itemVar << "));\n";
			}
			code << indentStr << name << "->SetItems(std::move("
				<< itemsName << "));\n";
		}
	}
	return code.str();
}

std::string CodeGenerator::GenerateHeader()
{
	std::ostringstream header;
	std::string className = WStringToString(_className);
	
	// 收集需要的头文件
	std::set<std::string> includes;
	includes.insert("Form.h");
	includes.insert("Control.h");
	const bool hasDataBindings = std::any_of(
		_controls.begin(), _controls.end(),
		[](const std::shared_ptr<DesignerControl>& control)
		{
			return control && !control->DataBindings.empty();
		});
	const bool hasMetadataProperties = std::any_of(
		_controls.begin(), _controls.end(),
		[](const std::shared_ptr<DesignerControl>& control)
		{
			return control && !control->MetadataProperties.empty();
		});
	if (hasDataBindings || hasMetadataProperties)
		includes.insert("Binding.h");
	
	for (const auto& dc : _controls)
	{
		includes.insert(GetIncludeForType(dc->Type));
	}
	
	// 生成头文件
	header << "#pragma once\n";
	for (const auto& inc : includes)
	{
		header << "#include \"" << inc << "\"\n";
	}
	header << "\n";
	
	header << "class " << className << " : public Form\n";
	header << "{\n";
	header << "private:\n";
	
	// 声明控件成员
	for (const auto& dc : _controls)
	{
		std::string name = GetVarName(dc);
		std::string typeName = GetControlTypeName(dc->Type);
		header << "\t" << typeName << "* " << name << ";\n";
	}

	// 声明事件处理函数（勾选启用；命名：{controlName}_OnXxx）
	{
		std::unordered_map<std::string, std::string> sigOf;
		std::vector<std::pair<std::string, std::string>> decls;
		std::string formPrefix = SanitizeCppIdentifier(WStringToString(_formName));

		// Form 事件：命名固定 Form_OnXxx
		for (const auto& kv : _formEventHandlers)
		{
			if (kv.first.empty()) continue;
			if (kv.second.empty()) continue;
			std::string evField, params;
			if (!TryGetFormEventSignature(kv.first, evField, params)) continue;
			std::string handlerName = MakeHandlerName(formPrefix, kv.first);
			auto itSig = sigOf.find(handlerName);
			if (itSig != sigOf.end() && itSig->second != params) continue;
			sigOf[handlerName] = params;
			decls.push_back({ handlerName, params });
		}

		for (const auto& dc : _controls)
		{
			if (!dc) continue;
			for (const auto& kv : dc->EventHandlers)
			{
				const auto& evNameW = kv.first;
				// value 仅代表“启用”（新格式会是 "1" 或旧格式 handlerName）
				if (kv.second.empty()) continue;
				std::string evField, params;
				if (!TryGetEventSignature(dc->Type, evNameW, evField, params)) continue;
				std::string handlerName = MakeHandlerName(GetVarName(dc), evNameW);
				auto itSig = sigOf.find(handlerName);
				if (itSig != sigOf.end() && itSig->second != params) continue;
				sigOf[handlerName] = params;
				decls.push_back({ handlerName, params });
			}
		}
		if (!decls.empty())
		{
			header << "\n";
			for (const auto& d : decls)
				header << "\tvoid " << d.first << "(" << d.second << ");\n";
		}
	}
	
	header << "\n";
	header << "public:\n";
	header << "\t" << className << "();\n";
	header << "\tvirtual ~" << className << "();\n";
	if (hasDataBindings)
		header << "\tbool BindData(IBindingSource& dataContext);\n";
	header << "};\n";
	
	return header.str();
}

std::string CodeGenerator::GenerateCpp()
{
	std::ostringstream cpp;
	std::string className = WStringToString(_className);
	
	// 包含头文件
	cpp << "#include \"" << className << ".h\"\n";
	if (!_styleSheet.Empty()) cpp << "#include \"Style.h\"\n";
	cpp << "#include <functional>\n";
	cpp << "#include <memory>\n";
	cpp << "#include <utility>\n";
	cpp << "#include <vector>\n\n";
	
	// 构造函数（注意：本框架的窗体初始化应走 Form 基类构造函数参数，
	// 在构造函数体内直接写 this->Text/Size/Location 可能不会生效。）
	cpp << className << "::" << className << "()\n";
	std::wstring text = _formText.empty() ? _className : _formText;
	cpp << "\t: Form(L\"" << EscapeWStringLiteral(text) << "\", POINT{ "
		<< _formLocation.x << ", " << _formLocation.y << " }, SIZE{ "
		<< _formSize.cx << ", " << _formSize.cy << " })\n";
	cpp << "{\n\n";
	cpp << "\t[[maybe_unused]] auto __layoutScope_form = cui::layout::DeferLayout(*this);\n\n";

	cpp << "\t// 窗体属性（标题栏/按钮/缩放）\n";
	cpp << "\tthis->VisibleHead = " << (_formVisibleHead ? "true" : "false") << ";\n";
	cpp << "\tthis->HeadHeight = " << _formHeadHeight << ";\n";
	cpp << "\tthis->MinBox = " << (_formMinBox ? "true" : "false") << ";\n";
	cpp << "\tthis->MaxBox = " << (_formMaxBox ? "true" : "false") << ";\n";
	cpp << "\tthis->CloseBox = " << (_formCloseBox ? "true" : "false") << ";\n";
	cpp << "\tthis->CenterTitle = " << (_formCenterTitle ? "true" : "false") << ";\n";
	cpp << "\tthis->AllowResize = " << (_formAllowResize ? "true" : "false") << ";\n\n";

	cpp << "\t// 窗体属性（通用）\n";
	cpp << "\tthis->BackColor = " << ColorToString(_formBackColor) << ";\n";
	cpp << "\tthis->ForeColor = " << ColorToString(_formForeColor) << ";\n";
	cpp << "\tthis->ShowInTaskBar = " << (_formShowInTaskBar ? "true" : "false") << ";\n";
	cpp << "\tthis->TopMost = " << (_formTopMost ? "true" : "false") << ";\n";
	cpp << "\tthis->Enable = " << (_formEnable ? "true" : "false") << ";\n";
	cpp << "\tthis->Visible = " << (_formVisible ? "true" : "false") << ";\n\n";

	// Font（共享给默认控件）
	{
		auto* def = GetDefaultFontObject();
		std::wstring defName = def ? def->FontName : L"Arial";
		float defSize = def ? def->FontSize : 18.0f;
		std::wstring formNameW = _formFontName.empty() ? defName : _formFontName;
		float formSize = _formFontSize;
		bool formHasShared = !(_formFontName.empty() && formNameW == defName && std::fabs(formSize - defSize) < 1e-6f);
		if (formHasShared)
		{
			cpp << "\t// Font\n";
			cpp << "\tauto* __formFont = new ::Font(L\"" << EscapeWStringLiteral(formNameW) << "\", "
				<< FloatLiteral(formSize) << ");\n";
			cpp << "\tthis->SetFontEx(__formFont, true);\n\n";
		}
	}
	
	cpp << "\t// 创建控件\n";

	std::vector<std::pair<int, std::string>> generatedLayoutScopes;

	// 1) 先实例化所有可设计控件（不做 AddControl）
	for (const auto& dc : _controls)
	{
		cpp << GenerateControlInstantiation(dc, 1);
		cpp << GenerateControlCommonProperties(dc, 1);
		cpp << GenerateMetadataProperties(dc, 1);
		cpp << GenerateContainerProperties(dc, 1);
		if (dc && dc->ControlInstance && dc->Type != UIClass::UI_TabPage)
			cpp << "\n";
	}

	// 容器各自持有布局事务。构造函数末尾按父到子的顺序显式提交；
	// 若初始化抛异常，RAII 析构仍会安全恢复而不强制同步布局。
	for (const auto& dc : _controls)
	{
		const bool supportsLayoutScope = dc &&
			(IsContainerType(dc->Type) || dc->Type == UIClass::UI_StatusBar);
		if (!dc || !dc->ControlInstance || dc->Type == UIClass::UI_TabPage || !supportsLayoutScope)
			continue;
		const std::string controlVar = GetVarName(dc);
		const std::string scopeVar = "__layoutScope_" + controlVar;
		int depth = 0;
		for (Control* parent = dc->DesignerParent; parent; parent = parent->Parent)
			++depth;
		cpp << "\t[[maybe_unused]] auto " << scopeVar
			<< " = cui::layout::DeferLayout(*" << controlVar << ");\n";
		generatedLayoutScopes.push_back({ depth, scopeVar });
	}
	if (!generatedLayoutScopes.empty())
		cpp << "\n";

	// 事件绑定（使用 std::bind_front 绑定到类成员函数；命名：{controlName}_OnXxx）
	{
		std::unordered_map<std::string, std::string> sigOf;
		std::vector<GeneratedEventBinding> binds;
		binds.reserve(_controls.size());
		std::string formPrefix = SanitizeCppIdentifier(WStringToString(_formName));

		// Form 事件
		for (const auto& kv : _formEventHandlers)
		{
			if (kv.first.empty()) continue;
			if (kv.second.empty()) continue;
			std::string evField, params;
			if (!TryGetFormEventSignature(kv.first, evField, params)) continue;
			std::string handlerName = MakeHandlerName(formPrefix, kv.first);
			auto itSig = sigOf.find(handlerName);
			if (itSig != sigOf.end() && itSig->second != params) continue;
			sigOf[handlerName] = params;
			binds.push_back(GeneratedEventBinding{ "this", evField, handlerName, params });
		}

		for (const auto& dc : _controls)
		{
			if (!dc) continue;
			std::string ctrlVar = GetVarName(dc);
			for (const auto& kv : dc->EventHandlers)
			{
				const auto& evNameW = kv.first;
				if (kv.second.empty()) continue;
				std::string evField, params;
				if (!TryGetEventSignature(dc->Type, evNameW, evField, params)) continue;
				std::string handlerName = MakeHandlerName(ctrlVar, evNameW);
				auto itSig = sigOf.find(handlerName);
				if (itSig != sigOf.end() && itSig->second != params) continue;
				sigOf[handlerName] = params;
				binds.push_back(GeneratedEventBinding{ ctrlVar, evField, handlerName, params });
			}
		}

		if (!binds.empty())
		{
			cpp << "\t// 绑定事件\n";
			for (const auto& b : binds)
			{
				cpp << "\t" << b.ControlVar << "->" << b.EventField << " += std::bind_front(&" << className << "::" << b.HandlerName << ", this);\n";
			}
			cpp << "\n";
		}
	}

	// 2) 按 DesignerParent 组装层级（容器内应为 container->AddControl）
	cpp << "\t// 组装控件层级（包含布局容器）\n";

	// 建立 Control* -> 变量名 映射
	std::unordered_map<Control*, std::string> varOf;
	varOf.reserve(_controls.size());
	for (const auto& dc : _controls)
	{
		if (!dc || !dc->ControlInstance) continue;
		varOf[dc->ControlInstance] = GetVarName(dc);
	}

	// parent -> children(designer) 映射
	std::unordered_map<Control*, std::vector<std::shared_ptr<DesignerControl>>> childrenOf;
	childrenOf.reserve(_controls.size());
	for (const auto& dc : _controls)
	{
		if (!dc || !dc->ControlInstance) continue;
		childrenOf[dc->DesignerParent].push_back(dc);
	}

	auto sortChildrenByRuntimeOrder = [&](Control* parent, std::vector<std::shared_ptr<DesignerControl>>& list)
	{
		if (!parent || list.size() <= 1) return;
		if (parent->Type() == UIClass::UI_SplitContainer)
		{
			SortSplitChildrenByRuntimeOrder((SplitContainer*)parent, list);
			return;
		}
		// 用 parent->Children 的顺序来稳定排序（用于 Stack/Warp 等需要顺序的容器）
		std::unordered_map<Control*, int> childRuntimeOrder;
		childRuntimeOrder.reserve((size_t)parent->Count);
		for (int i = 0; i < parent->Count; i++)
		{
			childRuntimeOrder[parent->operator[](i)] = i;
		}
		std::stable_sort(list.begin(), list.end(), [&](const auto& a, const auto& b)
			{
				int leftOrder = INT_MAX;
				int rightOrder = INT_MAX;
				auto leftOrderIt = childRuntimeOrder.find(a->ControlInstance);
				if (leftOrderIt != childRuntimeOrder.end()) leftOrder = leftOrderIt->second;
				auto rightOrderIt = childRuntimeOrder.find(b->ControlInstance);
				if (rightOrderIt != childRuntimeOrder.end()) rightOrder = rightOrderIt->second;
				return leftOrder < rightOrder;
			});
	};

	// TabPage 可能没有 DesignerControl：需要在生成时为其分配变量名
	std::unordered_map<Control*, std::string> tabPageVarOf;
	int tabPageAutoId = 0;

	std::function<void(Control* parentCtrl, const std::string& parentExpr, int indent)> emitChildren;
	std::function<void(const std::shared_ptr<DesignerControl>& dc, const std::string& parentExpr, int indent)> emitControl;

	emitChildren = [&](Control* parentCtrl, const std::string& parentExpr, int indent)
	{
		auto it = childrenOf.find(parentCtrl);
		if (it == childrenOf.end()) return;
		auto list = it->second;
		sortChildrenByRuntimeOrder(parentCtrl, list);
		for (auto& childDc : list)
			emitControl(childDc, parentExpr, indent);
	};

	auto emitLayoutPropsForParent = [&](Control* parentCtrl, const std::string& childVar, Control* childCtrl, int indent)
	{
		if (!parentCtrl || !childCtrl) return;
		std::string indentStr(indent, '\t');
		UIClass pt = parentCtrl->Type();
		if (pt == UIClass::UI_GridPanel)
		{
			cpp << indentStr << childVar << "->GridRow = " << childCtrl->GridRow << ";\n";
			cpp << indentStr << childVar << "->GridColumn = " << childCtrl->GridColumn << ";\n";
			cpp << indentStr << childVar << "->GridRowSpan = " << childCtrl->GridRowSpan << ";\n";
			cpp << indentStr << childVar << "->GridColumnSpan = " << childCtrl->GridColumnSpan << ";\n";
			cpp << indentStr << childVar << "->HAlign = " << HorizontalAlignmentToString(childCtrl->HAlign) << ";\n";
			cpp << indentStr << childVar << "->VAlign = " << VerticalAlignmentToString(childCtrl->VAlign) << ";\n";
		}
		else if (pt == UIClass::UI_DockPanel)
		{
			cpp << indentStr << childVar << "->DockPosition = " << DockToString(childCtrl->DockPosition) << ";\n";
		}
		else if (pt == UIClass::UI_RelativePanel)
		{
			// 设计器中 RelativePanel 主要用 Margin.Left/Top 表示定位
			auto m = childCtrl->Margin;
			if (m.Left != 0.0f || m.Top != 0.0f || m.Right != 0.0f || m.Bottom != 0.0f)
				cpp << indentStr << childVar << "->Margin = " << ThicknessToString(m) << ";\n";
		}
		else if (pt == UIClass::UI_StackPanel || pt == UIClass::UI_WrapPanel)
		{
			// 可选：输出对齐（设计器中经常用 Stretch）
			cpp << indentStr << childVar << "->HAlign = " << HorizontalAlignmentToString(childCtrl->HAlign) << ";\n";
			cpp << indentStr << childVar << "->VAlign = " << VerticalAlignmentToString(childCtrl->VAlign) << ";\n";
		}
	};

	emitControl = [&](const std::shared_ptr<DesignerControl>& dc, const std::string& parentExpr, int indent)
	{
		if (!dc || !dc->ControlInstance) return;
		// TabPage 由 TabControl::AddPage 创建，不走通用 AddControl 流程
		if (dc->Type == UIClass::UI_TabPage) return;
		auto* c = dc->ControlInstance;
		std::string childVar = GetVarName(dc);
		std::string indentStr(indent, '\t');

		// 添加到父容器
		UIClass parentType = UIClass::UI_CUSTOM;
		if (dc->DesignerParent) parentType = dc->DesignerParent->Type();
		if (parentType == UIClass::UI_ToolBar)
		{
			cpp << indentStr << parentExpr << "->AddOwned(std::move(__owned_" << childVar << "));\n";
		}
		else if (parentType == UIClass::UI_SplitContainer)
		{
			auto* split = (SplitContainer*)dc->DesignerParent;
			std::string hostExpr = GetSplitChildHostExpr(split, c->Parent, parentExpr);
			cpp << indentStr << hostExpr << "->AddOwned(std::move(__owned_" << childVar << "));\n";
		}
		else
		{
			cpp << indentStr << parentExpr << "->AddOwned(std::move(__owned_" << childVar << "));\n";
		}

		emitLayoutPropsForParent(dc->DesignerParent, childVar, c, indent);

		// 如果是容器，递归添加子控件
		if (dc->Type == UIClass::UI_TabControl)
		{
			// TabControl：先创建页，再向页内添加子控件
			auto* tabControl = (TabControl*)c;
			for (int i = 0; i < tabControl->Count; i++)
			{
				auto* page = tabControl->operator[](i);
				if (!page) continue;
				std::string pageVar;
				// 如果页本身有 DesignerControl 包装，则用成员变量接收
				auto itVar = varOf.find(page);
				if (itVar != varOf.end())
				{
					pageVar = itVar->second;
					cpp << indentStr << pageVar << " = " << childVar << "->AddPage(L\"" << EscapeWStringLiteral(page->Text) << "\");\n";
				}
				else
				{
					pageVar = childVar + "_page" + std::to_string(i + 1);
					// 避免命名冲突
					pageVar += "_" + std::to_string(++tabPageAutoId);
					cpp << indentStr << "auto* " << pageVar << " = " << childVar << "->AddPage(L\"" << EscapeWStringLiteral(page->Text) << "\");\n";
				}
				tabPageVarOf[page] = pageVar;
				// 页是容器：继续往里加
				emitChildren(page, pageVar, indent);
			}
		}
		else if (dc->Type == UIClass::UI_ToolBar)
		{
			// ToolBar：子控件统一走 AddToolItem，由工具栏负责横向排布
			emitChildren(c, childVar, indent);
		}
		else if (IsContainerType(dc->Type))
		{
			emitChildren(c, childVar, indent);
		}

		cpp << "\n";
	};

	// 根级控件：DesignerParent == nullptr
	auto rootsIt = childrenOf.find(nullptr);
	if (rootsIt != childrenOf.end())
	{
		auto roots = rootsIt->second;
		sortChildrenByRuntimeOrder(nullptr, roots);
		for (auto& rootDc : roots)
			emitControl(rootDc, "this", 1);
	}

	cpp << GenerateStyleSheetCode(1);

	cpp << "\t// 父容器先确定子控件最终矩形，再由子容器完成内部布局。\n";
	cpp << "\t__layoutScope_form.Commit();\n";
	std::stable_sort(generatedLayoutScopes.begin(), generatedLayoutScopes.end(),
		[](const auto& left, const auto& right) { return left.first < right.first; });
	for (const auto& entry : generatedLayoutScopes)
		cpp << "\t" << entry.second << ".Commit();\n";
	
	cpp << "}\n\n";
	
	// 析构函数
	cpp << className << "::~" << className << "()\n";
	cpp << "{\n";
	cpp << "}\n";

	const bool hasDataBindings = std::any_of(
		_controls.begin(), _controls.end(),
		[](const std::shared_ptr<DesignerControl>& control)
		{
			return control && !control->DataBindings.empty();
		});
	if (hasDataBindings)
	{
		cpp << "\n";
		cpp << "bool " << className << "::BindData(IBindingSource& dataContext)\n";
		cpp << "{\n";
		cpp << "\tbool success = true;\n";
		for (const auto& dc : _controls)
		{
			if (!dc || dc->DataBindings.empty()) continue;
			const std::string controlVar = GetVarName(dc);
			cpp << "\t" << controlVar << "->DataBindings.Clear();\n";
			for (const auto& [targetProperty, binding] : dc->DataBindings)
			{
				const auto converterName = DesignerBindingUtils::Trim(binding.Converter);
				if (converterName.empty())
				{
					cpp << "\tif (!" << controlVar << "->DataBindings.Add(L\""
						<< EscapeWStringLiteral(targetProperty) << "\", dataContext, L\""
						<< EscapeWStringLiteral(binding.SourceProperty) << "\", "
						<< BindingModeToExpr(binding.Mode) << ", "
						<< DataSourceUpdateModeToExpr(binding.UpdateMode)
						<< ")) success = false;\n";
				}
				else
				{
					cpp << "\t{\n";
					cpp << "\t\tauto converter = BindingValueConverterRegistry::Create(L\""
						<< EscapeWStringLiteral(converterName) << "\");\n";
					cpp << "\t\tif (!converter || !" << controlVar << "->DataBindings.Add(L\""
						<< EscapeWStringLiteral(targetProperty) << "\", dataContext, L\""
						<< EscapeWStringLiteral(binding.SourceProperty) << "\", "
						<< BindingModeToExpr(binding.Mode) << ", "
						<< DataSourceUpdateModeToExpr(binding.UpdateMode)
						<< ", converter)) success = false;\n";
					cpp << "\t}\n";
				}
			}
		}
		cpp << "\treturn success;\n";
		cpp << "}\n";
	}

	// 事件处理函数定义
	{
		std::unordered_map<std::string, std::string> sigOf;
		std::vector<std::pair<std::string, std::string>> defs;
		std::string formPrefix = SanitizeCppIdentifier(WStringToString(_formName));

		// Form 事件
		for (const auto& kv : _formEventHandlers)
		{
			if (kv.first.empty()) continue;
			if (kv.second.empty()) continue;
			std::string evField, params;
			if (!TryGetFormEventSignature(kv.first, evField, params)) continue;
			std::string handlerName = MakeHandlerName(formPrefix, kv.first);
			auto itSig = sigOf.find(handlerName);
			if (itSig != sigOf.end() && itSig->second != params) continue;
			sigOf[handlerName] = params;
			defs.push_back({ handlerName, params });
		}

		for (const auto& dc : _controls)
		{
			if (!dc) continue;
			for (const auto& kv : dc->EventHandlers)
			{
				const auto& evNameW = kv.first;
				if (kv.second.empty()) continue;
				std::string evField, params;
				if (!TryGetEventSignature(dc->Type, evNameW, evField, params)) continue;
				std::string handlerName = MakeHandlerName(GetVarName(dc), evNameW);
				auto itSig = sigOf.find(handlerName);
				if (itSig != sigOf.end() && itSig->second != params) continue;
				sigOf[handlerName] = params;
				defs.push_back({ handlerName, params });
			}
		}

		if (!defs.empty())
		{
			cpp << "\n";
			for (const auto& d : defs)
			{
				cpp << "void " << className << "::" << d.first << "(" << d.second << ")\n";
				cpp << "{\n";
				// 避免未使用参数警告
				if (d.second.find("MouseEventArgs") != std::string::npos)
					cpp << "\t(void)sender; (void)e;\n";
				else if (d.second.find("KeyEventArgs") != std::string::npos)
					cpp << "\t(void)sender; (void)e;\n";
				else if (d.second.find("List<std::wstring>") != std::string::npos)
					cpp << "\t(void)sender; (void)files;\n";
				else if (d.second.find("std::wstring") != std::string::npos)
				{
					if (d.second.find("oldText") != std::string::npos)
						cpp << "\t(void)sender; (void)oldText; (void)newText;\n";
					else
						cpp << "\t(void)sender; (void)text;\n";
				}
				else if (d.second.find("wchar_t") != std::string::npos)
					cpp << "\t(void)sender; (void)ch;\n";
				else if (d.second.find("int Id") != std::string::npos)
					cpp << "\t(void)sender; (void)Id; (void)info;\n";
				else if (d.second.find("GridView*") != std::string::npos)
					cpp << "\t(void)sender; (void)c; (void)r; (void)v;\n";
				else
					cpp << "\t(void)sender;\n";
				cpp << "}\n\n";
			}
		}
	}
	
	return cpp.str();
}

bool CodeGenerator::GenerateFiles(std::wstring headerPath, std::wstring cppPath)
{
	try
	{
		// 生成头文件
		std::ofstream headerFile(headerPath);
		if (!headerFile.is_open()) return false;
		headerFile << GenerateHeader();
		headerFile.close();
		
		// 生成cpp文件
		std::ofstream cppFile(cppPath);
		if (!cppFile.is_open()) return false;
		cppFile << GenerateCpp();
		cppFile.close();
		
		return true;
	}
	catch (...)
	{
		return false;
	}
}
