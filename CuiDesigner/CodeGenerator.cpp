#include "CodeGenerator.h"
#include "CodeGenInput.h"
#include "DesignerEventCatalog.h"
#include "DesignerModel/AtomicFile.h"
#include "DesignerModel/CppUserCodeIndex.h"
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
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string_view>

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

static bool IsCppKeyword(const std::string& s);

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

	struct GeneratedRuntimeEventRoute
	{
		std::string HandlerName;
		std::string ParameterList;
		std::wstring EventName;
		std::string EventField;
		std::string EventOwnerType;
		bool IsForm = false;
		UIClass ControlType = UIClass::UI_Base;
		bool IsCustom = false;
		std::wstring CustomXamlNamespace;
		std::wstring CustomXamlName;
		std::string CustomCppType;
		std::string CustomSignatureName;
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
		auto descriptor = DesignerEventCatalog::FindControlEvent(controlType, eventName);
		if (!descriptor) return false;
		outEventField = descriptor->EventField;
		outParamList = descriptor->ParameterList;
		return true;
	}

	static bool TryGetFormEventSignature(const std::wstring& eventName,
		std::string& outEventField, std::string& outParamList)
	{
		auto descriptor = DesignerEventCatalog::FindFormEvent(eventName);
		if (!descriptor) return false;
		outEventField = descriptor->EventField;
		outParamList = descriptor->ParameterList;
		return true;
	}


	static std::string ResolveHandlerName(
		const std::wstring& storedValue,
		const std::string& subjectName,
		const std::wstring& eventName)
	{
		const std::wstring subject(subjectName.begin(), subjectName.end());
		const auto resolved = DesignerEventCatalog::ResolveHandlerName(
			storedValue, subject, eventName);
		if (resolved.empty()) return {};
		const int size = WideCharToMultiByte(
			CP_UTF8, 0, resolved.data(), static_cast<int>(resolved.size()),
			nullptr, 0, nullptr, nullptr);
		std::string result(static_cast<size_t>(std::max(0, size)), '\0');
		if (size > 0)
			WideCharToMultiByte(CP_UTF8, 0, resolved.data(),
				static_cast<int>(resolved.size()), result.data(), size, nullptr, nullptr);
		return result;
	}

	static std::string GenerateUnusedParameterLines(
		const std::string& params, const char* indent = "\t")
	{
		std::ostringstream output;
		size_t begin = 0;
		while (begin < params.size())
		{
			auto comma = params.find(',', begin);
			if (comma == std::string::npos) comma = params.size();
			auto end = comma;
			while (end > begin && std::isspace(
				static_cast<unsigned char>(params[end - 1]))) --end;
			auto nameBegin = end;
			while (nameBegin > begin)
			{
				const auto ch = static_cast<unsigned char>(params[nameBegin - 1]);
				if (!std::isalnum(ch) && ch != '_') break;
				--nameBegin;
			}
			if (nameBegin < end)
				output << indent << "(void)"
					<< params.substr(nameBegin, end - nameBegin) << ";\n";
			begin = comma + 1;
		}
		return output.str();
	}

	/** Removes generated parameter identifiers while preserving their C++ types. */
	static std::string CanonicalGeneratedParameterTypes(
		std::string_view parameters)
	{
		std::string result;
		size_t begin = 0;
		int angleDepth = 0;
		for (size_t position = 0; position <= parameters.size(); ++position)
		{
			const char ch = position < parameters.size()
				? parameters[position] : ',';
			if (ch == '<') ++angleDepth;
			else if (ch == '>' && angleDepth > 0) --angleDepth;
			if (ch != ',' || angleDepth != 0) continue;

			auto first = begin;
			while (first < position && std::isspace(
				static_cast<unsigned char>(parameters[first]))) ++first;
			auto end = position;
			while (end > first && std::isspace(
				static_cast<unsigned char>(parameters[end - 1]))) --end;
			auto nameBegin = end;
			while (nameBegin > first)
			{
				const auto value = static_cast<unsigned char>(
					parameters[nameBegin - 1]);
				if (!std::isalnum(value) && value != '_') break;
				--nameBegin;
			}
			auto typeEnd = nameBegin;
			while (typeEnd > first && std::isspace(
				static_cast<unsigned char>(parameters[typeEnd - 1]))) --typeEnd;
			if (!result.empty()) result += ',';
			result.append(parameters.substr(first, typeEnd - first));
			begin = position + 1;
		}
		return result;
	}

	static bool IsCppIdentifierStart(unsigned char value) noexcept
	{
		return std::isalpha(value) || value == '_';
	}

	static bool IsCppIdentifierPart(unsigned char value) noexcept
	{
		return std::isalnum(value) || value == '_';
	}

	struct QualifiedCppClassName
	{
		std::vector<std::string> Segments;
		std::string NamespaceName;
		std::string UserLeaf;
		std::string GeneratedLeaf;
		std::string QualifiedUser;
		std::string QualifiedGenerated;
	};

	static QualifiedCppClassName ParseQualifiedCppClassName(
		const std::string& value)
	{
		QualifiedCppClassName result;
		size_t begin = 0;
		while (begin <= value.size())
		{
			const auto end = value.find("::", begin);
			const auto segment = value.substr(begin,
				end == std::string::npos ? std::string::npos : end - begin);
			if (segment.empty())
				throw std::invalid_argument("C++ code-behind class identity is invalid");
			if (!IsCppIdentifierStart(static_cast<unsigned char>(segment.front()))
				|| !std::all_of(segment.begin() + 1, segment.end(),
					[](unsigned char ch) { return IsCppIdentifierPart(ch); })
				|| IsCppKeyword(segment))
				throw std::invalid_argument("C++ code-behind class segment is invalid");
			result.Segments.push_back(segment);
			if (end == std::string::npos) break;
			begin = end + 2;
		}
		result.UserLeaf = result.Segments.back();
		result.GeneratedLeaf = result.UserLeaf + "Generated";
		for (size_t index = 0; index < result.Segments.size(); ++index)
		{
			if (index > 0) result.QualifiedUser += "::";
			result.QualifiedUser += result.Segments[index];
			if (index + 1 < result.Segments.size())
			{
				if (!result.NamespaceName.empty()) result.NamespaceName += "::";
				result.NamespaceName += result.Segments[index];
			}
		}
		result.QualifiedGenerated = result.NamespaceName.empty()
			? result.GeneratedLeaf
			: result.NamespaceName + "::" + result.GeneratedLeaf;
		return result;
	}

	static std::optional<std::string> ReadUserClassIdentityMarker(
		std::string_view source)
	{
		constexpr std::string_view beginMarker = "<cui-designer-class>";
		constexpr std::string_view endMarker = "</cui-designer-class>";
		const auto begin = source.find(beginMarker);
		if (begin == std::string_view::npos) return std::nullopt;
		const auto valueBegin = begin + beginMarker.size();
		const auto end = source.find(endMarker, valueBegin);
		if (end == std::string_view::npos) return std::string{};
		return std::string(source.substr(valueBegin, end - valueBegin));
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

bool CodeGenerator::InspectUserHandlerDefinitions(
	std::string_view userSource,
	std::vector<CodeGeneratorHandlerDefinitionInspection>& inspections)

{
	return InspectUserHandlerDefinitions({}, userSource, inspections);
}

bool CodeGenerator::InspectUserHandlerDefinitions(
	std::string_view userHeader,
	std::string_view userSource,
	std::vector<CodeGeneratorHandlerDefinitionInspection>& inspections)
{
	inspections.clear();
	_lastError.clear();
	try
	{
		std::vector<std::pair<std::string, std::string>> handlers;
		std::wstring error;
		if (!CollectEventHandlers(handlers, &error))
		{
			_lastError = error.empty()
				? L"无法建立事件处理函数索引。" : std::move(error);
			return false;
		}
		const auto identity = ParseQualifiedCppClassName(
			WStringToString(_className));
		DesignerModel::CppUserCodeIndex headerIndex;
		DesignerModel::CppUserCodeIndex sourceIndex;
		if (!DesignerModel::CppUserCodeIndex::Build(
			userHeader, identity.QualifiedUser, headerIndex, &error))
		{
			_lastError = error.empty()
				? L"无法建立用户头文件事件代码索引。" : std::move(error);
			return false;
		}
		if (!DesignerModel::CppUserCodeIndex::Build(
			userSource, identity.QualifiedUser, sourceIndex, &error))
		{
			_lastError = error.empty()
				? L"无法建立用户源文件事件代码索引。" : std::move(error);
			return false;
		}
		inspections.reserve(handlers.size());
		for (const auto& [name, parameterList] : handlers)
		{
			CodeGeneratorHandlerDefinitionInspection inspection;
			inspection.Name = name;
			inspection.ParameterList = parameterList;
			const auto headerDefinitions = headerIndex.InspectHandler(
				name, parameterList);
			const auto sourceDefinitions = sourceIndex.InspectHandler(
				name, parameterList);
			inspection.HeaderDefinitionCount =
				headerDefinitions.DefinitionCount;
			inspection.HeaderCompatibleDefinitionCount =
				headerDefinitions.CompatibleDefinitionCount;
			inspection.HeaderIncompatibleShapeDefinitionCount =
				headerDefinitions.IncompatibleShapeDefinitionCount;
			inspection.HeaderDeletedCompatibleDefinitionCount =
				headerDefinitions.DeletedCompatibleDefinitionCount;
			inspection.SourceDefinitionCount =
				sourceDefinitions.DefinitionCount;
			inspection.SourceCompatibleDefinitionCount =
				sourceDefinitions.CompatibleDefinitionCount;
			inspection.SourceIncompatibleShapeDefinitionCount =
				sourceDefinitions.IncompatibleShapeDefinitionCount;
			inspection.SourceDeletedCompatibleDefinitionCount =
				sourceDefinitions.DeletedCompatibleDefinitionCount;
			inspection.FirstHeaderDefinitionLine =
				headerDefinitions.FirstDefinitionLine;
			inspection.FirstHeaderCompatibleDefinitionLine =
				headerDefinitions.FirstCompatibleDefinitionLine;
			inspection.FirstSourceDefinitionLine =
				sourceDefinitions.FirstDefinitionLine;
			inspection.FirstSourceCompatibleDefinitionLine =
				sourceDefinitions.FirstCompatibleDefinitionLine;
			inspection.DefinitionCount =
				inspection.HeaderDefinitionCount
				+ inspection.SourceDefinitionCount;
			inspection.CompatibleDefinitionCount =
				inspection.HeaderCompatibleDefinitionCount
				+ inspection.SourceCompatibleDefinitionCount;
			inspection.IncompatibleShapeDefinitionCount =
				inspection.HeaderIncompatibleShapeDefinitionCount
				+ inspection.SourceIncompatibleShapeDefinitionCount;
			inspection.DeletedCompatibleDefinitionCount =
				inspection.HeaderDeletedCompatibleDefinitionCount
				+ inspection.SourceDeletedCompatibleDefinitionCount;
			if (inspection.DefinitionCount == 0)
				inspection.State =
					CodeGeneratorHandlerDefinitionState::Missing;
			else if (inspection.CompatibleDefinitionCount > 1)
				inspection.State =
					CodeGeneratorHandlerDefinitionState::DuplicateCompatible;
			else if (inspection.IncompatibleShapeDefinitionCount != 0
				|| inspection.DeletedCompatibleDefinitionCount != 0)
				inspection.State =
					CodeGeneratorHandlerDefinitionState::Incompatible;
			else if (inspection.CompatibleDefinitionCount == 1)
				inspection.State =
					CodeGeneratorHandlerDefinitionState::Compatible;
			else
				inspection.State =
					CodeGeneratorHandlerDefinitionState::Incompatible;
			inspections.push_back(std::move(inspection));
		}
		return true;
	}
	catch (const std::exception& error)
	{
		_lastError = L"无法检查用户事件处理函数："
			+ StringToWString(error.what());
	}
	catch (...)
	{
		_lastError = L"无法检查用户事件处理函数：发生未知异常。";
	}
	inspections.clear();
	return false;
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

	std::unordered_set<std::string> used;
	used.reserve(_controls.size());

	for (const auto& dc : _controls)
	{
		if (!dc) continue;
		std::string base = SanitizeCppIdentifier(WStringToString(dc->Name));
		// 保守：成员变量建议以小写开头，避免与类型名混淆（仅在安全情况下调整）
		if (!base.empty() && base[0] >= 'A' && base[0] <= 'Z')
			base[0] = (char)(base[0] - 'A' + 'a');

		std::string finalName = base;
		for (int suffix = 2; used.contains(finalName); ++suffix)
			finalName = base + std::to_string(suffix);

		// 二次防御：仍可能撞上关键字（例如 base="this" 调整后）
		if (IsCppKeyword(finalName)) finalName += "_";
		while (used.contains(finalName)) finalName += "_";

		used.insert(finalName);
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

std::string CodeGenerator::GetControlTypeName(
	const DesignerControl& control)
{
	return control.CustomType.Empty()
		? GetControlTypeName(control.Type)
		: WStringToString(control.CustomType.CppType);
}

std::string CodeGenerator::GetIncludeForType(
	const DesignerControl& control)
{
	return control.CustomType.Empty()
		? GetIncludeForType(control.Type)
		: WStringToString(control.CustomType.Header);
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
	std::string typeName = GetControlTypeName(*dc);

	code << indentStr << "// " << name << "\n";
	code << indentStr << "auto __owned_" << name << " = std::make_unique<" << typeName << ">(";

	if (!dc->CustomType.Empty())
	{
		switch (dc->CustomType.Constructor)
		{
		case DesignerCustomControlConstructor::Default:
			break;
		case DesignerCustomControlConstructor::TextBounds:
			code << "L\"" << EscapeWStringLiteral(control->Text) << "\", "
				<< control->Location.x << ", " << control->Location.y << ", "
				<< control->Size.cx << ", " << control->Size.cy;
			break;
		case DesignerCustomControlConstructor::Bounds:
		default:
			code << control->Location.x << ", " << control->Location.y << ", "
				<< control->Size.cx << ", " << control->Size.cy;
			break;
		}
	}
	else switch (dc->Type)
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
	if (!dc->CustomType.Empty())
	{
		if (dc->CustomType.Constructor
			== DesignerCustomControlConstructor::Default)
		{
			code << indentStr << name << "->Location = {"
				<< control->Location.x << ", " << control->Location.y << "};\n";
			code << indentStr << name << "->Size = {"
				<< control->Size.cx << ", " << control->Size.cy << "};\n";
		}
		if (dc->CustomType.Constructor
			!= DesignerCustomControlConstructor::TextBounds
			&& !control->Text.empty())
			code << indentStr << name << "->Text = L\""
				<< EscapeWStringLiteral(control->Text) << "\";\n";
	}
	if (dc->StableId > 0)
		code << indentStr << name << "->DesignId = " << dc->StableId << ";\n";

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
	if (dc->CustomType.Empty() && (dc->Type == UIClass::UI_Label || dc->Type == UIClass::UI_LinkLabel || dc->Type == UIClass::UI_CheckBox || dc->Type == UIClass::UI_RadioBox))
	{
		code << indentStr << name << "->Size = {" << control->Size.cx << ", " << control->Size.cy << "};\n";
	}

	// 对于不在构造函数中写入 Text 的控件：补齐 Text
	if (dc->CustomType.Empty() && dc->Type != UIClass::UI_Label && dc->Type != UIClass::UI_LinkLabel && dc->Type != UIClass::UI_Button &&
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
	bool appliedToRoot = false;
	for (const auto& control : _controls)
	{
		if (!control || !control->ControlInstance
			|| control->DesignerParent != nullptr
			|| control->Type == UIClass::UI_TabPage) continue;
		code << indentStr << GetVarName(control)
			<< "->SetStyleSheet(__styleSheet, true);\n";
		appliedToRoot = true;
	}
	if (!appliedToRoot)
		code << indentStr << "(void)__styleSheet;\n";
	code << "\n";
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
				if (item.IsMixed)
					code << indentStr << itemVar << ".IsMixed = true;\n";
				if (item.CanReset)
					code << indentStr << itemVar << ".CanReset = true;\n";
				if (item.ValueType == PropertyGridValueType::Slider)
				{
					code << indentStr << itemVar << ".Minimum = "
						<< item.Minimum << ";\n";
					code << indentStr << itemVar << ".Maximum = "
						<< item.Maximum << ";\n";
					code << indentStr << itemVar << ".Step = "
						<< item.Step << ";\n";
				}
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

bool CodeGenerator::CollectEventHandlers(
	std::vector<std::pair<std::string, std::string>>& handlers,
	std::wstring* outError) const
{
	handlers.clear();
	if (outError) outError->clear();
	std::unordered_map<std::string, std::type_index> signatures;
	auto add = [&](const std::wstring& eventName,
		const std::wstring& storedHandler,
		const std::string& subjectName,
		const std::optional<DesignerEventDescriptor>& descriptor) -> bool
	{
		if (storedHandler.empty()) return true;
		if (!descriptor)
		{
			if (outError) *outError = L"无法生成未知事件 “" + eventName + L"”。";
			return false;
		}
		const std::wstring subject(subjectName.begin(), subjectName.end());
		const auto resolved = DesignerEventCatalog::ResolveHandlerName(
			storedHandler, subject, eventName);
		std::wstring validationError;
		if (!DesignerEventCatalog::ValidateHandlerName(resolved, &validationError))
		{
			if (outError) *outError = L"事件 “" + eventName + L"”：" + validationError;
			return false;
		}
		const auto handler = ResolveHandlerName(storedHandler, subjectName, eventName);
		if (handler.empty()) return true;
		auto existing = signatures.find(handler);
		if (existing != signatures.end())
		{
			if (existing->second == descriptor->Signature) return true;
			if (outError) *outError = L"处理函数 “" + resolved
				+ L"” 被参数签名不同的事件复用。";
			return false;
		}
		signatures.emplace(handler, descriptor->Signature);
		handlers.emplace_back(handler, descriptor->ParameterList);
		return true;
	};

	const auto formSubject = SanitizeCppIdentifier(WStringToString(_formName));
	for (const auto& [eventName, storedHandler] : _formEventHandlers)
		if (!add(eventName, storedHandler, formSubject,
			DesignerEventCatalog::FindFormEvent(eventName))) return false;
	for (const auto& control : _controls)
	{
		if (!control) continue;
		const auto subject = GetVarName(control);
		for (const auto& [eventName, storedHandler] : control->EventHandlers)
			if (!add(eventName, storedHandler, subject,
				DesignerEventCatalog::FindControlEvent(
					control->Type, eventName, control->CustomEvents))) return false;
	}
	return true;
}

std::string CodeGenerator::GenerateHeader()
{
	std::ostringstream header;
	const auto identity = ParseQualifiedCppClassName(
		WStringToString(_className));
	const auto& className = identity.GeneratedLeaf;
	std::vector<std::pair<std::string, std::string>> eventHandlers;
	std::wstring eventError;
	if (!CollectEventHandlers(eventHandlers, &eventError))
		throw std::invalid_argument(WStringToString(eventError));

	std::vector<GeneratedRuntimeEventRoute> runtimeRoutes;
	std::set<std::string> runtimeRouteKeys;
	auto appendBuiltInRoute = [&](bool isForm,
		UIClass controlType,
		const std::wstring& eventName,
		const std::wstring& storedHandler,
		const std::string& subject,
		const DesignerEventDescriptor& descriptor)
	{
		const auto handler = ResolveHandlerName(
			storedHandler, subject, eventName);
		if (handler.empty()) return;
		const bool wildcard = !isForm
			&& descriptor.EventOwnerTypeName == "Control";
		const auto effectiveType = wildcard ? UIClass::UI_Base : controlType;
		const auto key = std::string(isForm ? "F|" : "B|") + handler
			+ "|" + WStringToString(eventName)
			+ "|" + descriptor.EventOwnerTypeName
			+ "|" + descriptor.EventField
			+ "|" + std::to_string(static_cast<int>(effectiveType));
		if (!runtimeRouteKeys.insert(key).second) return;
		GeneratedRuntimeEventRoute route;
		route.HandlerName = handler;
		route.ParameterList = descriptor.ParameterList;
		route.EventName = eventName;
		route.EventField = descriptor.EventField;
		route.EventOwnerType = descriptor.EventOwnerTypeName;
		route.IsForm = isForm;
		route.ControlType = effectiveType;
		runtimeRoutes.push_back(std::move(route));
	};

	const auto formSubject = SanitizeCppIdentifier(WStringToString(_formName));
	for (const auto& [eventName, storedHandler] : _formEventHandlers)
		if (const auto descriptor = DesignerEventCatalog::FindFormEvent(eventName))
			appendBuiltInRoute(true, UIClass::UI_Base, eventName,
				storedHandler, formSubject, *descriptor);
	for (const auto& control : _controls)
	{
		if (!control) continue;
		const auto subject = GetVarName(control);
		for (const auto& [eventName, storedHandler] : control->EventHandlers)
		{
			const auto descriptor = DesignerEventCatalog::FindControlEvent(
				control->Type, eventName, control->CustomEvents);
			if (!descriptor) continue;
			const auto custom = std::find_if(
				control->CustomEvents.begin(), control->CustomEvents.end(),
				[&](const auto& value) { return value.Name == eventName; });
			if (custom == control->CustomEvents.end())
			{
				appendBuiltInRoute(false, control->Type, eventName,
					storedHandler, subject, *descriptor);
				continue;
			}

			const auto handler = ResolveHandlerName(
				storedHandler, subject, eventName);
			if (handler.empty()) continue;
			const auto key = std::string("C|") + handler
				+ "|" + WStringToString(control->CustomType.RegistryKey())
				+ "|" + WStringToString(eventName)
				+ "|" + custom->EventField;
			if (!runtimeRouteKeys.insert(key).second) continue;
			GeneratedRuntimeEventRoute route;
			route.HandlerName = handler;
			route.ParameterList = descriptor->ParameterList;
			route.EventName = eventName;
			route.EventField = custom->EventField;
			route.IsCustom = true;
			route.CustomXamlNamespace = control->CustomType.XamlNamespace;
			route.CustomXamlName = control->CustomType.XamlName;
			route.CustomCppType = GetControlTypeName(*control);
			route.CustomSignatureName =
				DesignerEventCatalog::GetCustomSignatureName(custom->Signature);
			runtimeRoutes.push_back(std::move(route));
		}
	}
	
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
		if (dc) includes.insert(GetIncludeForType(*dc));
	}
	
	// 生成头文件
	header << "#pragma once\n";
	for (const auto& inc : includes)
	{
		header << "#include \"" << inc << "\"\n";
	}
	header << "#include <functional>\n";
	header << "#include <memory>\n";
	header << "#include <string>\n";
	header << "#include <utility>\n";
	header << "#include <vector>\n";
	header << "\n";
	if (!identity.NamespaceName.empty())
		header << "namespace " << identity.NamespaceName << "\n{\n\n";

	if (!eventHandlers.empty())
	{
		const auto eventSinkName = identity.UserLeaf + "EventSink";
		header << "class " << eventSinkName << "\n{\n";
		header << "public:\n";
		header << "\t" << eventSinkName << "() = default;\n";
		header << "\tvirtual ~" << eventSinkName
			<< "() { UnregisterDynamicEventHandlers(); }\n";
		header << "\t" << eventSinkName << "(const " << eventSinkName
			<< "&) = delete;\n";
		header << "\t" << eventSinkName << "& operator=(const "
			<< eventSinkName << "&) = delete;\n";
		header << "\t" << eventSinkName << "(" << eventSinkName
			<< "&&) = delete;\n";
		header << "\t" << eventSinkName << "& operator=("
			<< eventSinkName << "&&) = delete;\n\n";
		header << "\ttemplate<typename TRegistry>\n";
		header << "\tbool RegisterDynamicEventHandlers(\n";
		header << "\t\tTRegistry& registry, std::wstring* outError = nullptr)\n";
		header << "\t{\n";
		header << "\t\ttry\n";
		header << "\t\t{\n";
		header << "\t\t\tauto lifetime = std::make_shared<int>(0);\n";
		header << "\t\t\tauto registration = registry.RegisterScopedBatch(\n";
		header << "\t\t\t[this, lifetime = std::weak_ptr<void>(lifetime)](\n";
		header << "\t\t\t\tauto& routes, std::wstring& error)\n";
		header << "\t\t\t{\n";
		for (const auto& route : runtimeRoutes)
		{
			const auto parameterTypes = CanonicalGeneratedParameterTypes(
				route.ParameterList);
			header << "\t\t\t\tif (!routes.";
			if (route.IsCustom)
			{
				header << "RegisterGeneratedCustomControl(\n";
				header << "\t\t\t\t\tL\""
					<< EscapeWStringLiteral(StringToWString(route.HandlerName))
					<< "\", L\""
					<< EscapeWStringLiteral(route.CustomXamlNamespace)
					<< "\", L\""
					<< EscapeWStringLiteral(route.CustomXamlName)
					<< "\", L\"" << EscapeWStringLiteral(route.EventName)
					<< "\", \"" << route.EventField << "\", L\""
					<< route.CustomSignatureName << "\",\n";
				header << "\t\t\t\t\t&" << route.CustomCppType
					<< "::" << route.EventField << ",\n";
			}
			else if (route.IsForm)
			{
				header << "RegisterForm(\n";
				header << "\t\t\t\t\tL\""
					<< EscapeWStringLiteral(StringToWString(route.HandlerName))
					<< "\", L\"" << EscapeWStringLiteral(route.EventName)
					<< "\", &" << route.EventOwnerType << "::"
					<< route.EventField << ",\n";
			}
			else
			{
				header << "RegisterControl(\n";
				header << "\t\t\t\t\tL\""
					<< EscapeWStringLiteral(StringToWString(route.HandlerName))
					<< "\", UIClass::UI_"
					<< WStringToString(DesignerStyleSheetUtils::UIClassName(
						route.ControlType))
					<< ", L\"" << EscapeWStringLiteral(route.EventName)
					<< "\", &" << route.EventOwnerType << "::"
					<< route.EventField << ",\n";
			}
			header << "\t\t\t\t\tGuardDynamicEventHandler(\n";
			header << "\t\t\t\t\t\tlifetime, std::bind_front(\n";
			header << "\t\t\t\t\t\t\tstatic_cast<void (" << eventSinkName
				<< "::*)(" << parameterTypes << ")>(\n";
			header << "\t\t\t\t\t\t\t\t&" << eventSinkName << "::"
				<< route.HandlerName << "), this)), &error))\n";
			header << "\t\t\t\t\treturn false;\n";
		}
		header << "\t\t\t\treturn true;\n";
		header << "\t\t\t}, outError);\n";
		header << "\t\t\tif (!registration) return false;\n";
		header << "\t\t\tstruct DynamicEventRegistration final\n";
		header << "\t\t\t{\n";
		header << "\t\t\t\tdecltype(registration) Lease;\n";
		header << "\t\t\t\tstd::shared_ptr<void> Lifetime;\n";
		header << "\t\t\t\tDynamicEventRegistration(\n";
		header << "\t\t\t\t\tdecltype(registration)&& lease,\n";
		header << "\t\t\t\t\tstd::shared_ptr<void> lifetime) noexcept\n";
		header << "\t\t\t\t\t: Lease(std::move(lease)),\n";
		header << "\t\t\t\t\tLifetime(std::move(lifetime)) {}\n";
		header << "\t\t\t};\n";
		header << "\t\t\tauto owned = std::make_shared<DynamicEventRegistration>(\n";
		header << "\t\t\t\tstd::move(registration), std::move(lifetime));\n";
		header << "\t\t\t_dynamicEventRegistration = std::move(owned);\n";
		header << "\t\t\tif (outError) outError->clear();\n";
		header << "\t\t\treturn true;\n";
		header << "\t\t}\n";
		header << "\t\tcatch (...)\n";
		header << "\t\t{\n";
		header << "\t\t\tif (outError) *outError =\n";
		header << "\t\t\t\tL\"无法保存动态事件注册租约。\";\n";
		header << "\t\t\treturn false;\n";
		header << "\t\t}\n";
		header << "\t}\n\n";
		header << "\tvoid UnregisterDynamicEventHandlers() noexcept\n";
		header << "\t{\n";
		header << "\t\t_dynamicEventRegistration.reset();\n";
		header << "\t}\n\n";
		header << "private:\n";
		header << "\ttemplate<typename TCallback>\n";
		header << "\tstatic auto GuardDynamicEventHandler(\n";
		header << "\t\tstd::weak_ptr<void> lifetime, TCallback callback)\n";
		header << "\t{\n";
		header << "\t\treturn [lifetime = std::move(lifetime),\n";
		header << "\t\t\tcallback = std::move(callback)](auto&&... args) mutable\n";
		header << "\t\t{\n";
		header << "\t\t\tauto alive = lifetime.lock();\n";
		header << "\t\t\tif (!alive) return;\n";
		header << "\t\t\tstd::invoke(callback,\n";
		header << "\t\t\t\tstd::forward<decltype(args)>(args)...);\n";
		header << "\t\t};\n";
		header << "\t}\n\n";
		header << "\tstd::shared_ptr<void> _dynamicEventRegistration;\n\n";
		header << "protected:\n";
		for (const auto& handler : eventHandlers)
			header << "\tvirtual void " << handler.first << "("
				<< handler.second << ") = 0;\n";
		header << "};\n\n";
	}
	
	header << "class " << className << " : public Form";
	if (!eventHandlers.empty())
		header << ", public " << identity.UserLeaf << "EventSink";
	header << "\n";
	header << "{\n";
	header << "protected:\n";
	
	// 声明控件成员
	for (const auto& dc : _controls)
	{
		std::string name = GetVarName(dc);
		std::string typeName = GetControlTypeName(*dc);
		header << "\t" << typeName << "* " << name << " = nullptr;\n";
	}
	header << "\tstd::vector<EventConnection> _generatedEventConnections;\n";

	// Generated virtual hooks are overridden by declarations in the user class.
	if (!eventHandlers.empty())
	{
		header << "\n";
		for (const auto& handler : eventHandlers)
			header << "\tvoid " << handler.first << "("
				<< handler.second << ") override;\n";
	}
	
	header << "\n";
	header << "public:\n";
	const bool hasStableControlIds = std::any_of(
		_controls.begin(), _controls.end(),
		[](const std::shared_ptr<DesignerControl>& control)
		{
			return control && control->StableId > 0;
		});
	if (hasStableControlIds)
	{
		header << "\t// Stable identities shared by static and dynamic document paths.\n";
		header << "\tstruct ControlIds final\n\t{\n";
		for (const auto& dc : _controls)
		{
			if (!dc || dc->StableId <= 0) continue;
			header << "\t\tstatic constexpr int " << GetVarName(dc)
				<< " = " << dc->StableId << ";\n";
		}
		header << "\t};\n\n";
	}
	if (!_controls.empty())
	{
		header << "\t// Type-safe x:Name accessors; ownership remains with the generated Form.\n";
		for (const auto& dc : _controls)
		{
			if (!dc) continue;
			auto accessorName = GetVarName(dc);
			if (!accessorName.empty() && accessorName.front() >= 'a'
				&& accessorName.front() <= 'z')
				accessorName.front() = static_cast<char>(
					accessorName.front() - 'a' + 'A');
			const auto typeName = GetControlTypeName(*dc);
			header << "\t[[nodiscard]] " << typeName << "* Get"
				<< accessorName << "() noexcept { return "
				<< GetVarName(dc) << "; }\n";
			header << "\t[[nodiscard]] const " << typeName << "* Get"
				<< accessorName << "() const noexcept { return "
				<< GetVarName(dc) << "; }\n";
		}
		header << "\n";
	}
	header << "\t" << className << "();\n";
	header << "\tvirtual ~" << className << "();\n";
	if (hasDataBindings)
		header << "\tbool BindData(IBindingSource& dataContext);\n";
	header << "};\n";

	// A zero-owning typed view over the dynamic RuntimeDocument contract. Keep
	// this template independent of CuiRuntime headers so static-only consumers
	// do not acquire a runtime link dependency merely by including the .g.h.
	header << "\n";
	header << "// Non-owning typed access for a dynamically loaded document.\n";
	header << "// GetXxx resolves the current instance; ReferenceXxx follows reloads.\n";
	header << "template<typename TDocument>\n";
	header << "class " << identity.UserLeaf << "References final\n";
	header << "{\n";
	header << "public:\n";
	header << "\tusing DocumentReference = decltype(\n";
	header << "\t\tstd::declval<TDocument&>().Reference());\n\n";
	header << "\texplicit " << identity.UserLeaf
		<< "References(TDocument& document) noexcept\n";
	header << "\t\t: _document(document.Reference()) {}\n\n";
	header << "\t[[nodiscard]] explicit operator bool() const noexcept\n";
	header << "\t{\n\t\treturn static_cast<bool>(_document);\n\t}\n";
	header << "\t[[nodiscard]] TDocument* TryDocument() const noexcept\n";
	header << "\t{\n\t\treturn _document.Get();\n\t}\n";
	header << "\t// Precondition: the view is still alive; prefer TryDocument() when uncertain.\n";
	header << "\t[[nodiscard]] TDocument& Document() const noexcept"
		" { return *_document.Get(); }\n";
	for (const auto& dc : _controls)
	{
		if (!dc || dc->StableId <= 0) continue;
		auto accessorName = GetVarName(dc);
		if (!accessorName.empty() && accessorName.front() >= 'a'
			&& accessorName.front() <= 'z')
			accessorName.front() = static_cast<char>(
				accessorName.front() - 'a' + 'A');
		const auto typeName = GetControlTypeName(*dc);
		header << "\t[[nodiscard]] " << typeName << "* Get"
			<< accessorName << "() const noexcept\n\t{\n";
		header << "\t\treturn _document.template FindControlByDesignId<"
			<< typeName << ">(\n";
		header << "\t\t\t" << className << "::ControlIds::"
			<< GetVarName(dc) << ");\n\t}\n";
		header << "\t[[nodiscard]] auto Reference" << accessorName
			<< "() const noexcept\n\t{\n";
		header << "\t\treturn _document.template ReferenceByDesignId<"
			<< typeName << ">(\n";
		header << "\t\t\t" << className << "::ControlIds::"
			<< GetVarName(dc) << ");\n\t}\n";
	}
	header << "\nprivate:\n";
	header << "\tDocumentReference _document;\n";
	header << "};\n";
	if (!identity.NamespaceName.empty())
		header << "\n}\n";
	
	return header.str();
}

std::string CodeGenerator::GenerateCpp()
{
	const auto identity = ParseQualifiedCppClassName(
		WStringToString(_className));
	return GenerateCppForBaseName(identity.UserLeaf);
}

std::string CodeGenerator::GenerateCppForBaseName(
	const std::string& generatedHeaderBaseName)
{
	std::ostringstream cpp;
	const auto identity = ParseQualifiedCppClassName(
		WStringToString(_className));
	const auto& className = identity.QualifiedGenerated;
	const auto& classLeaf = identity.GeneratedLeaf;
	
	// 包含头文件
	cpp << "#include \"" << generatedHeaderBaseName << ".g.h\"\n";
	if (!_styleSheet.Empty()) cpp << "#include \"Style.h\"\n";
	cpp << "#include <functional>\n";
	cpp << "#include <memory>\n";
	cpp << "#include <utility>\n";
	cpp << "#include <vector>\n\n";
	
	// 构造函数（注意：本框架的窗体初始化应走 Form 基类构造函数参数，
	// 在构造函数体内直接写 this->Text/Size/Location 可能不会生效。）
	cpp << className << "::" << classLeaf << "()\n";
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

	// Event subscriptions are owned by RAII connections and disconnect before Form teardown.
	{
		std::unordered_map<std::string, std::type_index> sigOf;
		std::vector<GeneratedEventBinding> binds;
		binds.reserve(_controls.size());
		std::string formPrefix = SanitizeCppIdentifier(WStringToString(_formName));

		// Form 事件
		for (const auto& kv : _formEventHandlers)
		{
			if (kv.first.empty()) continue;
			if (kv.second.empty()) continue;
			const auto descriptor = DesignerEventCatalog::FindFormEvent(kv.first);
			if (!descriptor) continue;
			std::string handlerName = ResolveHandlerName(kv.second, formPrefix, kv.first);
			auto itSig = sigOf.find(handlerName);
			if (itSig != sigOf.end()
				&& itSig->second != descriptor->Signature) continue;
			if (itSig == sigOf.end())
				sigOf.emplace(handlerName, descriptor->Signature);
			binds.push_back(GeneratedEventBinding{ "this",
				descriptor->EventField, handlerName, descriptor->ParameterList });
		}

		for (const auto& dc : _controls)
		{
			if (!dc) continue;
			std::string ctrlVar = GetVarName(dc);
			for (const auto& kv : dc->EventHandlers)
			{
				const auto& evNameW = kv.first;
				if (kv.second.empty()) continue;
				const auto descriptor = DesignerEventCatalog::FindControlEvent(
					dc->Type, evNameW, dc->CustomEvents);
				if (!descriptor) continue;
				std::string handlerName = ResolveHandlerName(kv.second, ctrlVar, evNameW);
				auto itSig = sigOf.find(handlerName);
				if (itSig != sigOf.end()
					&& itSig->second != descriptor->Signature) continue;
				if (itSig == sigOf.end())
					sigOf.emplace(handlerName, descriptor->Signature);
				binds.push_back(GeneratedEventBinding{ ctrlVar,
					descriptor->EventField, handlerName, descriptor->ParameterList });
			}
		}

		if (!binds.empty())
		{
			cpp << "\t// 绑定事件\n";
			for (const auto& b : binds)
			{
				cpp << "\t_generatedEventConnections.emplace_back(\n";
				cpp << "\t\t" << b.ControlVar << "->" << b.EventField
					<< ".Subscribe(std::bind_front(&" << className << "::"
					<< b.HandlerName << ", this)));\n";
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
	cpp << className << "::~" << classLeaf << "()\n";
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
				const bool writesTarget = binding.Mode != BindingMode::OneWayToSource;
				const auto converterName = DesignerBindingUtils::Trim(binding.Converter);
				cpp << "\t{\n";
				if (writesTarget)
				{
					cpp << "\t\tBindingValue __previousLocal;\n";
					cpp << "\t\tconst bool __hadLocal = " << controlVar
						<< "->TryGetPropertyValue(L\""
						<< EscapeWStringLiteral(targetProperty)
						<< "\", ControlPropertyValueSource::Local, __previousLocal);\n";
					cpp << "\t\tconst bool __localReady = !__hadLocal || "
						<< controlVar << "->ClearPropertyValue(L\""
						<< EscapeWStringLiteral(targetProperty)
						<< "\", ControlPropertyValueSource::Local);\n";
					cpp << "\t\tbool __bound = false;\n";
					cpp << "\t\tif (__localReady)\n\t\t{\n";
				}
				else
				{
					cpp << "\t\tbool __bound = false;\n";
				}
				const char* operationIndent = writesTarget ? "\t\t\t" : "\t\t";
				if (converterName.empty())
				{
					cpp << operationIndent << "__bound = " << controlVar
						<< "->DataBindings.Add(L\""
						<< EscapeWStringLiteral(targetProperty) << "\", dataContext, L\""
						<< EscapeWStringLiteral(binding.SourceProperty) << "\", "
						<< BindingModeToExpr(binding.Mode) << ", "
						<< DataSourceUpdateModeToExpr(binding.UpdateMode)
						<< ") != nullptr;\n";
				}
				else
				{
					cpp << operationIndent
						<< "auto __converter = BindingValueConverterRegistry::Create(L\""
						<< EscapeWStringLiteral(converterName) << "\");\n";
					cpp << operationIndent << "__bound = __converter && " << controlVar
						<< "->DataBindings.Add(L\""
						<< EscapeWStringLiteral(targetProperty) << "\", dataContext, L\""
						<< EscapeWStringLiteral(binding.SourceProperty) << "\", "
						<< BindingModeToExpr(binding.Mode) << ", "
						<< DataSourceUpdateModeToExpr(binding.UpdateMode)
						<< ", __converter) != nullptr;\n";
				}
				if (writesTarget)
					cpp << "\t\t}\n";
				cpp << "\t\tif (!__bound)\n\t\t{\n";
				cpp << "\t\t\tsuccess = false;\n";
				if (writesTarget)
				{
					cpp << "\t\t\tif (__localReady && __hadLocal)\n";
					cpp << "\t\t\t\t(void)" << controlVar
						<< "->TrySetPropertyValue(L\""
						<< EscapeWStringLiteral(targetProperty)
						<< "\", __previousLocal, ControlPropertyValueSource::Local);\n";
				}
				cpp << "\t\t}\n";
				cpp << "\t}\n";
			}
		}
		cpp << "\treturn success;\n";
		cpp << "}\n";
	}

	// 事件处理函数定义
	{
		std::vector<std::pair<std::string, std::string>> defs;
		std::wstring eventError;
		if (!CollectEventHandlers(defs, &eventError))
			throw std::invalid_argument(WStringToString(eventError));

		if (!defs.empty())
		{
			cpp << "\n";
			for (const auto& d : defs)
			{
				cpp << "void " << className << "::" << d.first << "(" << d.second << ")\n";
				cpp << "{\n";
				cpp << GenerateUnusedParameterLines(d.second);
				cpp << "}\n\n";
			}
		}
	}
	
	return cpp.str();
}

bool CodeGenerator::BuildFilePlan(
	std::wstring headerPath,
	std::wstring cppPath,
	std::vector<CodeGeneratorFileContent>& files)
{
	files.clear();
	_lastError.clear();
	try
	{
		namespace fs = std::filesystem;
		const fs::path userHeaderPath(headerPath);
		const fs::path userCppPath(cppPath);
		if (headerPath.empty() || cppPath.empty())
		{
			_lastError = L"导出路径不能为空。";
			return false;
		}

		std::vector<std::pair<std::string, std::string>> currentHandlers;
		if (!CollectEventHandlers(currentHandlers, &_lastError)) return false;

		const auto baseName = userHeaderPath.stem().wstring();
		const auto baseNameUtf8 = WStringToString(baseName);
		const auto identity = ParseQualifiedCppClassName(
			WStringToString(_className));
		const auto generatedHeaderPath = userHeaderPath.parent_path()
			/ fs::path(baseName + L".g.h");
		const auto generatedCppPath = userCppPath.parent_path()
			/ fs::path(baseName + L".g.cpp");
		const auto handlerIncludePath = userHeaderPath.parent_path()
			/ fs::path(baseName + L".handlers.g.inc");

		DesignerModel::AtomicFileBatchSnapshot inputSnapshot;
		std::wstring snapshotError;
		if (!DesignerModel::AtomicFileBatchSnapshot::Capture({
			userHeaderPath.wstring(),
			userCppPath.wstring(),
			generatedHeaderPath.wstring(),
			generatedCppPath.wstring(),
			handlerIncludePath.wstring(),
		}, inputSnapshot, &snapshotError))
		{
			_lastError = snapshotError.empty()
				? L"无法捕获代码生成输入文件快照。"
				: std::move(snapshotError);
			return false;
		}
		const auto& existingFiles = inputSnapshot.Entries();
		if (existingFiles.size() != 5)
		{
			_lastError = L"代码生成输入文件快照不完整。";
			return false;
		}
		auto requireRecognizedUserFile = [&](
			const DesignerModel::AtomicFileSnapshotEntry& snapshot,
			const char* marker, std::string& content) -> bool
		{
			if (!snapshot.Existed) return true;
			content = snapshot.Content;
			if (content.find(marker) == std::string::npos)
			{
				_lastError = L"为避免覆盖已有代码，未修改文件："
					+ snapshot.FilePath
					+ L"。请选择新的导出文件名，或手动迁移到生成基类结构。";
				return false;
			}
			return true;
		};

		std::string existingUserHeader;
		std::string existingUserCpp;
		if (!requireRecognizedUserFile(existingFiles[0],
			"<cui-designer-user-header>", existingUserHeader) ||
			!requireRecognizedUserFile(existingFiles[1],
			"<cui-designer-user-source>", existingUserCpp))
			return false;
		auto matchesIdentityMarker = [&](const std::string& content)
		{
			const auto marker = ReadUserClassIdentityMarker(content);
			if (marker) return *marker == identity.QualifiedUser;
			return identity.Segments.size() == 1;
		};
		if ((!existingUserHeader.empty()
				&& !matchesIdentityMarker(existingUserHeader))
			|| (!existingUserCpp.empty()
				&& !matchesIdentityMarker(existingUserCpp)))
		{
			_lastError = L"现有 Designer 用户文件属于不同的 C++ 类；"
				L"为避免生成基类与用户类身份混用，请选择新的导出基路径，"
				L"或先手动迁移用户代码。";
			return false;
		}

		DesignerModel::CppUserCodeIndex userHeaderIndex;
		DesignerModel::CppUserCodeIndex userSourceIndex;
		DesignerModel::CppUserHandlerDefinitionInspection headerConstructor;
		DesignerModel::CppUserHandlerDefinitionInspection sourceConstructor;
		std::wstring constructorIndexError;
		if (!existingUserHeader.empty())
		{
			if (!DesignerModel::CppUserCodeIndex::Build(
				existingUserHeader, identity.QualifiedUser,
				userHeaderIndex, &constructorIndexError))
			{
				_lastError = constructorIndexError.empty()
					? L"无法建立用户头文件代码索引。"
					: std::move(constructorIndexError);
				return false;
			}
			const auto classDefinition =
				userHeaderIndex.InspectGeneratedClassDefinition();
			if (classDefinition.DefinitionCount != 1
				|| classDefinition.CompatibleGeneratedBaseCount != 1)
			{
				_lastError = L"用户头文件必须在当前 x:Class namespace 中"
					L"恰好定义一个用户类，并直接继承对应的 Generated 基类。";
				return false;
			}
			headerConstructor = userHeaderIndex.InspectConstructor();
		}
		if (!existingUserCpp.empty())
		{
			if (!DesignerModel::CppUserCodeIndex::Build(
				existingUserCpp, identity.QualifiedUser,
				userSourceIndex, &constructorIndexError))
			{
				_lastError = constructorIndexError.empty()
					? L"无法建立用户源文件代码索引。"
					: std::move(constructorIndexError);
				return false;
			}
			sourceConstructor = userSourceIndex.InspectConstructor();
		}
		const auto compatibleConstructorCount =
			headerConstructor.CompatibleDefinitionCount
			+ sourceConstructor.CompatibleDefinitionCount;
		const auto deletedConstructorCount =
			headerConstructor.DeletedCompatibleDefinitionCount
			+ sourceConstructor.DeletedCompatibleDefinitionCount;
		if (deletedConstructorCount != 0
			|| compatibleConstructorCount > 1
			|| (!existingUserCpp.empty()
				&& compatibleConstructorCount != 1))
		{
			_lastError = deletedConstructorCount != 0
				? L"用户类的默认构造函数已被删除，无法实例化生成窗体。"
				: compatibleConstructorCount > 1
					? L"用户类的默认构造函数在头文件或源文件中存在多个定义。"
					: L"用户类缺少默认构造函数定义；可在头文件中内联，"
						L"或在用户源文件中定义。";
			return false;
		}

		std::map<std::string, std::string> retainedHandlers;
		const auto& oldHandlerInclude = existingFiles[4].Content;
		std::istringstream oldLines(oldHandlerInclude);
		for (std::string line; std::getline(oldLines, line);)
		{
			const auto first = line.find_first_not_of(" \t");
			if (first == std::string::npos || line.compare(first, 5, "void ") != 0) continue;
			const auto open = line.find('(', first + 5);
			const auto semicolon = line.rfind(';');
			const auto close = semicolon == std::string::npos
				? std::string::npos : line.rfind(')', semicolon);
			if (open == std::string::npos || close == std::string::npos
				|| semicolon == std::string::npos || close < open) continue;
			const auto name = line.substr(first + 5, open - (first + 5));
			const auto params = line.substr(open + 1, close - open - 1);
			if (!name.empty()) retainedHandlers[name] = params;
		}
		for (const auto& [name, params] : currentHandlers)
		{
			auto previous = retainedHandlers.find(name);
			if (previous != retainedHandlers.end()
				&& CanonicalGeneratedParameterTypes(previous->second)
					!= CanonicalGeneratedParameterTypes(params))
			{
				_lastError = L"已有用户处理函数 “"
					+ StringToWString(name)
					+ L"” 的参数签名与新事件不兼容。请改用新的函数名。";
				return false;
			}
			retainedHandlers[name] = params;
		}

		std::ostringstream handlerInclude;
		handlerInclude << "// Generated by CUI Designer. Do not edit.\n";
		handlerInclude << "// Declarations are retained after unbinding so existing user definitions keep compiling.\n";
		for (const auto& [name, params] : retainedHandlers)
		{
			const auto inlineDefinitions =
				userHeaderIndex.InspectHandler(name, params);
			if (inlineDefinitions.CompatibleDefinitionCount > 1)
			{
				_lastError = L"用户头文件中的处理函数 “"
					+ StringToWString(name)
					+ L"” 存在多个相同签名的内联定义。";
				return false;
			}
			// A second declaration in the generated include would conflict with
			// an in-class definition of the same member.
			if (inlineDefinitions.CompatibleDefinitionCount == 1) continue;
			const auto active = std::any_of(currentHandlers.begin(), currentHandlers.end(),
				[&](const auto& handler)
				{
					return handler.first == name
						&& CanonicalGeneratedParameterTypes(handler.second)
							== CanonicalGeneratedParameterTypes(params);
				});
			handlerInclude << "\tvoid " << name << "(" << params << ")"
				<< (active ? " override" : "") << ";\n";
		}

		std::ostringstream newUserHeader;
		newUserHeader
			<< "#pragma once\n"
			<< "// <cui-designer-user-header> Created once; safe for user edits.\n"
			<< "// <cui-designer-class>" << identity.QualifiedUser
			<< "</cui-designer-class>\n"
			<< "#include \"" << baseNameUtf8 << ".g.h\"\n\n";
		if (!identity.NamespaceName.empty())
			newUserHeader << "namespace " << identity.NamespaceName << "\n{\n\n";
		newUserHeader
			<< "class " << identity.UserLeaf << " : public "
			<< identity.GeneratedLeaf << "\n"
			<< "{\n"
			<< "public:\n"
			<< "\t" << identity.UserLeaf << "();\n"
			<< "\t~" << identity.UserLeaf << "() override = default;\n\n"
			<< "private:\n"
			<< "#include \"" << baseNameUtf8 << ".handlers.g.inc\"\n"
			<< "};\n";
		if (!identity.NamespaceName.empty()) newUserHeader << "\n}\n";

		std::ostringstream newUserCpp;
		if (existingUserCpp.empty())
		{
			newUserCpp
				<< "// <cui-designer-user-source> Created once; safe for user edits.\n"
				<< "// <cui-designer-class>" << identity.QualifiedUser
				<< "</cui-designer-class>\n"
				<< "#include \"" << baseNameUtf8 << ".h\"\n\n";
			if (headerConstructor.CompatibleDefinitionCount == 0)
				newUserCpp
					<< identity.QualifiedUser << "::" << identity.UserLeaf << "()\n"
					<< "\t: " << identity.QualifiedGenerated << "()\n"
					<< "{\n"
					<< "\t// User initialization belongs here.\n"
					<< "}\n";
		}
		else
			newUserCpp << existingUserCpp;

		auto appendUnusedParameters = [&](std::ostringstream& output,
			const std::string& params)
		{
			size_t begin = 0;
			while (begin < params.size())
			{
				auto comma = params.find(',', begin);
				if (comma == std::string::npos) comma = params.size();
				auto end = comma;
				while (end > begin && std::isspace(
					static_cast<unsigned char>(params[end - 1]))) --end;
				auto nameBegin = end;
				while (nameBegin > begin)
				{
					const auto ch = static_cast<unsigned char>(params[nameBegin - 1]);
					if (!std::isalnum(ch) && ch != '_') break;
					--nameBegin;
				}
				if (nameBegin < end)
					output << "\t(void)" << params.substr(nameBegin, end - nameBegin) << ";\n";
				begin = comma + 1;
			}
		};
		for (const auto& [name, params] : currentHandlers)
		{
			const auto headerDefinitions =
				userHeaderIndex.InspectHandler(name, params);
			const auto sourceDefinitions =
				userSourceIndex.InspectHandler(name, params);
			const auto definitionCount =
				headerDefinitions.DefinitionCount
				+ sourceDefinitions.DefinitionCount;
			const auto compatibleDefinitions =
				headerDefinitions.CompatibleDefinitionCount
				+ sourceDefinitions.CompatibleDefinitionCount;
			const auto incompatibleShapes =
				headerDefinitions.IncompatibleShapeDefinitionCount
				+ sourceDefinitions.IncompatibleShapeDefinitionCount;
			const auto deletedDefinitions =
				headerDefinitions.DeletedCompatibleDefinitionCount
				+ sourceDefinitions.DeletedCompatibleDefinitionCount;
			if (definitionCount != 0)
			{
				if (compatibleDefinitions > 1)
				{
					_lastError = L"用户头文件或源文件中的处理函数 “"
						+ StringToWString(name)
						+ L"” 存在多个相同签名的定义。"
							L"请仅保留一个定义后重新生成。";
					return false;
				}
				if (incompatibleShapes == 0 && deletedDefinitions == 0
					&& compatibleDefinitions == 1) continue;
				_lastError = L"用户头文件或源文件中的处理函数 “"
					+ StringToWString(name)
					+ L"” 的返回类型、static/cv/ref 限定或参数签名"
						L"与设计事件不兼容。"
						L"请修正该定义，或在设计器中改用新的函数名。";
				return false;
			}
			newUserCpp << "\nvoid " << identity.QualifiedUser << "::" << name
				<< "(" << params << ")\n{\n";
			appendUnusedParameters(newUserCpp, params);
			newUserCpp << "}\n";
		}

		files.reserve(5);
		auto appendPlannedFile = [&](size_t snapshotIndex,
			const fs::path& path, std::string content)
		{
			const auto& expected = existingFiles[snapshotIndex];
			files.push_back({ path.wstring(), std::move(content),
				expected.Existed, expected.Content });
		};
		appendPlannedFile(0, userHeaderPath,
			existingUserHeader.empty()
				? newUserHeader.str() : existingUserHeader);
		appendPlannedFile(1, userCppPath, newUserCpp.str());
		appendPlannedFile(2, generatedHeaderPath, GenerateHeader());
		appendPlannedFile(3, generatedCppPath,
			GenerateCppForBaseName(baseNameUtf8));
		appendPlannedFile(4, handlerIncludePath, handlerInclude.str());
		return true;
	}
	catch (const std::exception& error)
	{
		files.clear();
		_lastError = L"准备代码生成计划失败：" + StringToWString(error.what());
		return false;
	}
	catch (...)
	{
		files.clear();
		_lastError = L"准备代码生成计划时发生未知错误。";
		return false;
	}
}

bool CodeGenerator::GenerateFiles(std::wstring headerPath, std::wstring cppPath)
{
	std::vector<CodeGeneratorFileContent> files;
	if (!BuildFilePlan(std::move(headerPath), std::move(cppPath), files))
		return false;
	try
	{
		std::vector<DesignerModel::AtomicFileWriteEntry> writes;
		writes.reserve(files.size());
		for (auto& file : files)
		{
			DesignerModel::AtomicFileWriteEntry write;
			write.FilePath = std::move(file.Path);
			write.Content = std::move(file.Content);
			write.RequireExpectedState = true;
			write.ExpectedExisted = file.ExpectedExisted;
			write.ExpectedContent = std::move(file.ExpectedContent);
			writes.push_back(std::move(write));
		}
		std::wstring writeError;
		if (!DesignerModel::AtomicFile::WriteBatch(writes, &writeError))
		{
			_lastError = L"代码文件批次提交失败；已尝试恢复导出前版本。";
			if (!writeError.empty()) _lastError += L"\n" + writeError;
			return false;
		}
		return true;
	}
	catch (const std::exception& error)
	{
		_lastError = L"提交代码生成计划失败：" + StringToWString(error.what());
		return false;
	}
	catch (...)
	{
		_lastError = L"提交代码生成计划时发生未知错误。";
		return false;
	}
}
