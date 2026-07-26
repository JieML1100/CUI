#include "CodeGenerator.h"
#include "DesignerEventCatalog.h"
#include "DesignerModel/AtomicFile.h"
#include "DesignerModel/CppUserCodeIndex.h"
#include "DesignerModel/DesignDocumentGraph.h"
#include "DesignerBindingUtils.h"
#include "DesignerPropertyCatalog.h"
#include "DesignerStyleSheetUtils.h"
#include "../CuiRuntime/include/XamlDocumentCompiler.h"
#include "../CuiRuntime/include/XamlObjectMaterializer.h"
#include "../CuiRuntime/include/XamlRuntimeSchema.h"
#include <algorithm>
#include <array>
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

static bool IsCppKeyword(const std::string& s);

namespace
{
	static const char* BindingModeToExpr(BindingMode mode)
	{
		switch (mode)
		{
		case BindingMode::Default: return "BindingMode::Default";
		case BindingMode::OneWay: return "BindingMode::OneWay";
		case BindingMode::TwoWay: return "BindingMode::TwoWay";
		case BindingMode::OneWayToSource: return "BindingMode::OneWayToSource";
		case BindingMode::OneTime: return "BindingMode::OneTime";
		}
		return "BindingMode::Default";
	}

	static const char* DataSourceUpdateModeToExpr(DataSourceUpdateMode mode)
	{
		switch (mode)
		{
		case DataSourceUpdateMode::Default: return "DataSourceUpdateMode::Default";
		case DataSourceUpdateMode::OnPropertyChanged: return "DataSourceUpdateMode::OnPropertyChanged";
		case DataSourceUpdateMode::OnValidation: return "DataSourceUpdateMode::OnValidation";
		case DataSourceUpdateMode::Never: return "DataSourceUpdateMode::Never";
		}
		return "DataSourceUpdateMode::OnPropertyChanged";
	}

	static std::string KeyToExpr(Key key)
	{
		const auto value = static_cast<int>(key);
		if (key >= Key::A && key <= Key::Z)
			return "Key::" + std::string(1, static_cast<char>(
				'A' + value - static_cast<int>(Key::A)));
		if (key >= Key::D0 && key <= Key::D9)
			return "Key::D" + std::to_string(
				value - static_cast<int>(Key::D0));
		if (key >= Key::F1 && key <= Key::F24)
			return "Key::F" + std::to_string(
				value - static_cast<int>(Key::F1) + 1);
		switch (key)
		{
		case Key::Back: return "Key::Back";
		case Key::Tab: return "Key::Tab";
		case Key::Return: return "Key::Return";
		case Key::Escape: return "Key::Escape";
		case Key::Space: return "Key::Space";
		case Key::PageUp: return "Key::PageUp";
		case Key::PageDown: return "Key::PageDown";
		case Key::Home: return "Key::Home";
		case Key::End: return "Key::End";
		case Key::Left: return "Key::Left";
		case Key::Up: return "Key::Up";
		case Key::Right: return "Key::Right";
		case Key::Down: return "Key::Down";
		case Key::Insert: return "Key::Insert";
		case Key::Delete: return "Key::Delete";
		case Key::OemPlus: return "Key::OemPlus";
		case Key::OemMinus: return "Key::OemMinus";
		case Key::OemComma: return "Key::OemComma";
		case Key::OemPeriod: return "Key::OemPeriod";
		default: return {};
		}
	}

	static std::string ModifierKeysToExpr(ModifierKeys modifiers)
	{
		if (modifiers == ModifierKeys::None) return "ModifierKeys::None";
		std::string result;
		auto append = [&](ModifierKeys flag, const char* expression)
		{
			if (!HasModifier(modifiers, flag)) return;
			if (!result.empty()) result += " | ";
			result += expression;
		};
		append(ModifierKeys::Control, "ModifierKeys::Control");
		append(ModifierKeys::Alt, "ModifierKeys::Alt");
		append(ModifierKeys::Shift, "ModifierKeys::Shift");
		append(ModifierKeys::Windows, "ModifierKeys::Windows");
		return result;
	}

	static const char* MouseActionToExpr(MouseAction action)
	{
		switch (action)
		{
		case MouseAction::LeftClick: return "MouseAction::LeftClick";
		case MouseAction::RightClick: return "MouseAction::RightClick";
		case MouseAction::MiddleClick: return "MouseAction::MiddleClick";
		case MouseAction::WheelClick: return "MouseAction::WheelClick";
		case MouseAction::LeftDoubleClick: return "MouseAction::LeftDoubleClick";
		case MouseAction::RightDoubleClick: return "MouseAction::RightDoubleClick";
		case MouseAction::MiddleDoubleClick: return "MouseAction::MiddleDoubleClick";
		default: return "MouseAction::None";
		}
	}

	struct GeneratedEventBinding
	{
		std::string ControlVar;
		std::string EventField;
		std::string HandlerName;
		std::string ParamList; // "Control* sender" ...
		std::wstring CommandName;
	};

	struct GeneratedRuntimeEventRoute
	{
		std::string HandlerName;
		std::string ParameterList;
		std::wstring EventName;
		std::string EventField;
		std::string EventOwnerType;
		bool IsWindow = false;
		UIClass ControlType = UIClass::UI_Base;
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

	static bool TryGetWindowEventSignature(const std::wstring& eventName,
		std::string& outEventField, std::string& outParamList)
	{
		auto descriptor = DesignerEventCatalog::FindWindowEvent(eventName);
		if (!descriptor) return false;
		outEventField = descriptor->EventField;
		outParamList = descriptor->ParameterList;
		return true;
	}


	static std::string Utf8HandlerName(const std::wstring& storedValue)
	{
		const auto resolved = DesignerEventCatalog::NormalizeHandlerName(
			storedValue);
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

CodeGenerator::CodeGenerator(
	std::wstring className,
	const DesignerModel::DesignDocument& document)
	: _className(std::move(className)),
	_sourceDocument(document),
	_styleSheet(_sourceDocument.StyleSheet),
	_resourceBasePath(_sourceDocument.ResourceBasePath)
{
	if (_sourceDocument.Window.Type != UIClass::UI_Window
		|| !_sourceDocument.Window.XamlType.Valid())
		throw std::invalid_argument(
			"CodeGenerator requires an authored XAML Window document");
	for (const auto& node : _sourceDocument.Nodes)
		if (!node.XamlType.Valid())
			throw std::invalid_argument(
				"Every generated control must have an authored XAML type");
	BuildVarNameMap();
}

bool CodeGenerator::ValidateDocument(
	const DesignerModel::DesignDocument& document,
	std::wstring* outError,
	DesignerModel::XamlDocumentDiagnostic* outDiagnostic)
{
	using namespace DesignerModel;
	if (outDiagnostic)
	{
		*outDiagnostic = {};
		outDiagnostic->Stage = XamlDiagnosticStage::CodeGeneration;
	}
	auto fail = [&](std::wstring message,
		const DesignNode* node = nullptr,
		const std::wstring& member = std::wstring{})
	{
		if (outError) *outError = message;
		if (outDiagnostic)
		{
			outDiagnostic->Message = message;
			outDiagnostic->Member = member;
			if (node)
			{
				outDiagnostic->QName = node->XamlType.Valid()
					? node->XamlType.LocalName
					: DesignerStyleSheetUtils::UIClassName(node->Type);
				const auto* span = member.empty()
					? nullptr : node->Source.FindMember(member);
				outDiagnostic->Apply(span ? *span : node->Source.Element);
			}
			else
			{
				std::wstring symbol;
				if (const auto* span = document.Sources.FindMentionedSymbol(
					message, &symbol))
				{
					if (outDiagnostic->Member.empty())
						outDiagnostic->Member = std::move(symbol);
					outDiagnostic->Apply(*span);
				}
				else outDiagnostic->Apply(document.Sources.Root);
			}
		}
		return false;
	};

	const bool hasLocalObjectResources = std::any_of(
		document.Nodes.begin(), document.Nodes.end(), [](const auto& node)
		{ return !node.LocalObjectResources.Empty(); });
	if (!document.Components.empty()
		|| !document.DataTypes.empty()
		|| !document.DataTemplates.empty()
		|| !document.ItemsPanelTemplates.empty()
		|| !document.GroupStyles.empty()
		|| !document.DataLists.empty()
		|| !document.CollectionViews.empty()
		|| hasLocalObjectResources)
		return fail(
			L"声明组件、局部对象资源、DataType、DataList、CollectionViewSource、DataTemplate、ItemsPanelTemplate 和 GroupStyle 属于动态 XAML 类型系统，"
			L"完整 C++ UI 生成器不再尝试展开它们。");

	DesignDocumentGraph graph;
	std::wstring validationError;
	if (!DesignDocumentGraph::Build(document, graph, &validationError))
		return fail(std::move(validationError));
	if (!document.ValidateCommandTargetReferences(&validationError))
		return fail(std::move(validationError));
	if (document.Window.Type != UIClass::UI_Window
		|| !document.Window.XamlType.Valid())
		return fail(
			L"静态代码生成要求一个具有 XAML 类型标识的 Window 根节点。",
			&document.Window);

	auto validateNode = [&](const DesignNode& node)
	{
		if (!node.XamlType.Valid())
			return fail(L"静态代码生成节点缺少 XAML 类型标识: "
				+ node.Name, &node);
		CuiRuntime::XamlTypePropertySchema schema;
		std::wstring schemaError;
		if (!CuiRuntime::XamlRuntimeSchema::BuildPropertySchema(
			node.Type, nullptr, document, schema, &schemaError))
			return fail(L"无法解析静态代码生成节点的属性 Schema: "
				+ schemaError, &node);
		for (const auto& [propertyName, assignment] : node.Properties.Values)
		{
			const auto* metadata = schema.FindProperty(propertyName);
			if (!metadata || !metadata->CanWrite())
				return fail(L"静态代码生成节点没有可写属性: "
					+ propertyName, &node, propertyName);
			if (!assignment.DynamicResourceKey.empty()) continue;
			DesignerStyleValue canonical;
			std::wstring propertyError;
			if (!DesignerPropertyCatalog::NormalizeStyleValue(
				*metadata, assignment.Value, canonical, &propertyError,
				document.ResourceBasePath))
				return fail(propertyError.empty()
					? L"静态代码生成属性值无效: " + propertyName
					: std::move(propertyError), &node, propertyName);
			const auto& design = metadata->Design();
			if (design.Minimum || design.Maximum)
			{
				BindingValue parsed;
				BindingValue converted;
				double number = 0.0;
				if (!DesignerStyleSheetUtils::TryConvertValue(
					canonical, parsed, &propertyError,
					document.ResourceBasePath)
					|| !metadata->TryConvert(parsed, converted)
					|| !converted.TryGetDouble(number)
					|| !std::isfinite(number)
					|| (design.Minimum && number < *design.Minimum)
					|| (design.Maximum && number > *design.Maximum))
					return fail(L"属性值超出 Schema 允许范围: "
						+ propertyName, &node, propertyName);
			}
		}
		if (!DesignerStyleSheetUtils::ValidateAgainstPropertyMetadata(
			node.LocalResources, &schemaError, document.ResourceBasePath))
			return fail(L"局部 Resources 无法静态生成: " + schemaError,
				&node, L"Resources");
		return true;
	};
	if (!validateNode(document.Window)) return false;
	for (const auto& node : document.Nodes)
		if (!validateNode(node)) return false;
	if (!DesignerStyleSheetUtils::ValidateAgainstPropertyMetadata(
		document.StyleSheet, &validationError, document.ResourceBasePath))
		return fail(L"Window.Resources 无法静态生成: " + validationError,
			&document.Window, L"Resources");
	if (outError) outError->clear();
	return true;
}

const std::vector<DesignerComponentEventDescriptor>&
CodeGenerator::ComponentEvents(
	const DesignerModel::DesignNode& node) const noexcept
{
	static const std::vector<DesignerComponentEventDescriptor> empty;
	if (node.ComponentType.Empty()) return empty;
	const auto found = std::find_if(
		_sourceDocument.Components.begin(), _sourceDocument.Components.end(),
		[&](const auto& component)
		{ return component.Type == node.ComponentType; });
	return found == _sourceDocument.Components.end() ? empty : found->Events;
}

std::string CodeGenerator::CommandTargetExpression(
	const std::wstring& name) const
{
	if (name.empty()) return "nullptr";
	if (name == _sourceDocument.Window.Name) return "this";
	const auto found = std::find_if(
		_sourceDocument.Nodes.begin(), _sourceDocument.Nodes.end(),
		[&](const auto& node) { return node.Name == name; });
	if (found == _sourceDocument.Nodes.end())
		throw std::invalid_argument(
			"Code generation encountered an unresolved CommandTarget");
	return GetVarName(*found);
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
	_varNameOf.reserve(_sourceDocument.Nodes.size());

	std::unordered_set<std::string> used;
	used.reserve(_sourceDocument.Nodes.size());

	for (const auto& node : _sourceDocument.Nodes)
	{
		std::string base = SanitizeCppIdentifier(
			WStringToString(node.Name));
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
		_varNameOf[&node] = finalName;
	}
}

std::string CodeGenerator::GetVarName(
	const DesignerModel::DesignNode& node) const
{
	auto it = _varNameOf.find(&node);
	if (it != _varNameOf.end()) return it->second;
	return SanitizeCppIdentifier(WStringToString(node.Name));
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
	case UIClass::UI_Button: return "Button";
	case UIClass::UI_TextBox: return "TextBox";
	case UIClass::UI_RichTextBox: return "RichTextBox";
	case UIClass::UI_PasswordBox: return "PasswordBox";
	case UIClass::UI_NumericUpDown: return "NumericUpDown";
	case UIClass::UI_Panel: return "Panel";
	case UIClass::UI_Decorator: return "Decorator";
	case UIClass::UI_Border: return "Border";
	case UIClass::UI_Canvas: return "Canvas";
	case UIClass::UI_GroupBox: return "GroupBox";
	case UIClass::UI_Expander: return "Expander";
	case UIClass::UI_ScrollViewer: return "ScrollViewer";
	case UIClass::UI_Popup: return "Popup";
	case UIClass::UI_StackPanel: return "StackPanel";
	case UIClass::UI_Grid: return "Grid";
	case UIClass::UI_DockPanel: return "DockPanel";
	case UIClass::UI_WrapPanel: return "WrapPanel";
	case UIClass::UI_RelativePanel: return "RelativePanel";
	case UIClass::UI_CheckBox: return "CheckBox";
	case UIClass::UI_RadioButton: return "RadioButton";
	case UIClass::UI_ComboBox: return "ComboBox";
	case UIClass::UI_ComboBoxItem: return "ComboBoxItem";
	case UIClass::UI_ListView: return "ListView";
	case UIClass::UI_ListBox: return "ListBox";
	case UIClass::UI_ListBoxItem: return "ListBoxItem";
	case UIClass::UI_ChartView: return "ChartView";
	case UIClass::UI_TreeView: return "TreeView";
	case UIClass::UI_TreeViewItem: return "TreeViewItem";
	case UIClass::UI_ProgressBar: return "ProgressBar";
	case UIClass::UI_LoadingRing: return "LoadingRing";
	case UIClass::UI_ProgressRing: return "ProgressRing";
	case UIClass::UI_Slider: return "Slider";
	case UIClass::UI_Image: return "Image";
	case UIClass::UI_Switch: return "Switch";
	case UIClass::UI_TabControl: return "TabControl";
	case UIClass::UI_TabItem: return "TabItem";
	case UIClass::UI_ToolBar: return "ToolBar";
	case UIClass::UI_Menu: return "Menu";
	case UIClass::UI_MenuItem: return "MenuItem";
	case UIClass::UI_Separator: return "Separator";
	case UIClass::UI_StatusBar: return "StatusBar";
	case UIClass::UI_WebBrowser: return "WebBrowser";
	case UIClass::UI_MediaPlayer: return "MediaPlayer";
	case UIClass::UI_NativeSurface: return "NativeSurface";
	case UIClass::UI_ItemsControl: return "ItemsControl";
	case UIClass::UI_ContentPresenter: return "ContentPresenter";
	case UIClass::UI_ItemsPresenter: return "ItemsPresenter";
	case UIClass::UI_ContentControl: return "ContentControl";
	default: return "Control";
	}
}

std::string CodeGenerator::GetIncludeForType(UIClass type)
{
	switch (type)
	{
	case UIClass::UI_TabControl:
	case UIClass::UI_TabItem:
		return "TabControl.h";
	case UIClass::UI_ComboBox:
	case UIClass::UI_ComboBoxItem:
		return "ComboBox.h";
	case UIClass::UI_ListBox:
	case UIClass::UI_ListBoxItem:
		return "ListBox.h";
	case UIClass::UI_TreeView:
	case UIClass::UI_TreeViewItem:
		return "TreeView.h";
	case UIClass::UI_Menu:
	case UIClass::UI_MenuItem:
		return "Menu.h";
	case UIClass::UI_Separator:
		return "Separator.h";
	case UIClass::UI_ToolBar:
		return "ToolBar.h";
	case UIClass::UI_StackPanel:
		return "Layout/StackPanel.h";
	case UIClass::UI_Grid:
		return "Layout/Grid.h";
	case UIClass::UI_DockPanel:
		return "Layout/DockPanel.h";
	case UIClass::UI_WrapPanel:
		return "Layout/WrapPanel.h";
	case UIClass::UI_RelativePanel:
		return "Layout/RelativePanel.h";
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

std::string CodeGenerator::GridLengthToCtorString(
	const DesignerModel::DesignGridLength& length)
{
	if (length.Unit == DesignerModel::DesignGridLengthUnit::Auto)
		return "GridLength::Auto()";
	if (length.Unit == DesignerModel::DesignGridLengthUnit::Star)
	{
		std::ostringstream oss;
		oss << "GridLength::Star("
			<< FloatLiteral(static_cast<float>(length.Value)) << ")";
		return oss.str();
	}
	std::ostringstream oss;
	oss << "GridLength::Pixels("
		<< FloatLiteral(static_cast<float>(length.Value)) << ")";
	return oss.str();
}

std::string CodeGenerator::GenerateControlInstantiation(
	const DesignerModel::DesignNode& node, int indent)
{
	std::ostringstream code;
	std::string indentStr(indent, '\t');
	std::string name = GetVarName(node);
	std::string typeName = GetControlTypeName(node.Type);
	code << indentStr << "// " << name << "\n";
	code << indentStr << "auto __owned_" << name
		<< " = std::make_unique<" << typeName << ">();\n";
	if (node.TemplateState.Generated)
		code << indentStr << "auto* " << name
			<< " = __owned_" << name << ".get();\n";
	else
		code << indentStr << name << " = __owned_" << name << ".get();\n";
	code << indentStr << "(void)" << name
		<< "->ClearPropertyValues();\n";
	if (node.XamlType.Valid())
	{
		const auto descriptorName = "__xamlType_" + name;
		code << indentStr << "static const auto " << descriptorName
			<< " = DeclarativeTypeDescriptor::Create(\n";
		code << indentStr << "\tRuntimeTypeId{ L\""
			<< EscapeWStringLiteral(node.XamlType.NamespaceUri) << "\", L\""
			<< EscapeWStringLiteral(node.XamlType.LocalName)
			<< "\" }, {});\n";
		code << indentStr << "if (!" << descriptorName << " || !" << name
			<< " || !cui::framework::XamlAccess::SetTypeDescriptor(*" << name
			<< ", " << descriptorName << "))\n";
		code << indentStr << "\tthrow std::runtime_error("
			<< "\"Generated XAML type attachment failed\");\n";
	}
	const auto* xamlType =
		CuiRuntime::XamlRuntimeSchema::DefaultTypeFor(node.Type);
	if (!xamlType)
		throw std::invalid_argument(
			"Code generation encountered an unregistered native XAML type");
	code << indentStr
		<< "(void)cui::framework::DependencyPropertyAccess::SetValue(*"
		<< name << ", L\"Focusable\", BindingValue("
		<< (xamlType->FocusableByDefault ? "true" : "false")
		<< "), DependencyPropertyValueSource::Theme);\n";
	if (node.Id > 0 && !node.TemplateState.Generated)
		code << indentStr
			<< "cui::framework::DesignIdentityAccess::Set(*" << name << ", "
			<< node.Id << ");\n";

	return code.str();
}

std::string CodeGenerator::GenerateControlCommonProperties(
	const DesignerModel::DesignNode& node,
	int indent)
{
	std::ostringstream code;
	const std::string indentStr(indent, '\t');
	const std::string name = GetVarName(node);
	if ((node.Type == UIClass::UI_Button
		|| node.Type == UIClass::UI_MenuItem)
		&& !node.Structure.CommandTarget.empty())
	{
		code << indentStr << name << "->CommandTarget = "
			<< CommandTargetExpression(
				node.Structure.CommandTarget) << ";\n";
	}
	if (!node.Properties.StyleResourceKey.empty())
		code << indentStr
			<< "cui::framework::StyleAccess::SetResourceKey(*"
			<< name << ", L\""
			<< EscapeWStringLiteral(node.Properties.StyleResourceKey)
			<< "\");\n";
	return code.str();
}

std::string CodeGenerator::GenerateAuthoredProperties(
	const DesignerModel::DesignNode& node,
	int indent)
{
	if (node.Properties.Values.empty()) return "";

	std::ostringstream code;
	const std::string indentStr(indent, '\t');
	const std::string name = GetVarName(node);
	code << indentStr << (node.TemplateState.Generated
		? "// ControlTemplate-authored properties/resources\n"
		: "// XAML authored Local properties/resources\n");
	std::vector<std::pair<std::wstring,
		const DesignerModel::DesignPropertyAssignment*>> orderedProperties;
	orderedProperties.reserve(node.Properties.Values.size());
	for (const auto& [propertyName, assignment]
		: node.Properties.Values)
		orderedProperties.emplace_back(propertyName, &assignment);
	std::stable_sort(orderedProperties.begin(), orderedProperties.end(),
		[&](const auto& left, const auto& right)
		{
			const auto* leftMetadata = CuiRuntime::XamlRuntimeSchema::FindNativeProperty(
				node.Type, left.first);
			const auto* rightMetadata = CuiRuntime::XamlRuntimeSchema::FindNativeProperty(
				node.Type, right.first);
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
				return leftMetadata != nullptr;
			return left.first < right.first;
		});
	for (const auto& [propertyName, assignment] : orderedProperties)
	{
		if (!assignment->DynamicResourceKey.empty())
		{
			if (node.TemplateState.Generated)
				code << indentStr
					<< "(void)cui::framework::DependencyPropertyAccess::"
					<< "SetDynamicResource(*" << name << ", L\""
					<< EscapeWStringLiteral(propertyName) << "\", L\""
					<< EscapeWStringLiteral(assignment->DynamicResourceKey)
					<< "\", DependencyPropertyValueSource::Template);\n";
			else
				code << indentStr << "(void)" << name
					<< "->SetDynamicResource(L\""
					<< EscapeWStringLiteral(propertyName) << "\", L\""
					<< EscapeWStringLiteral(assignment->DynamicResourceKey)
					<< "\");\n";
			continue;
		}
		if (node.TemplateState.Generated)
			code << indentStr
				<< "(void)cui::framework::DependencyPropertyAccess::SetValue(*"
				<< name << ", L\"" << EscapeWStringLiteral(propertyName)
				<< "\", " << GenerateStyleValueExpression(assignment->Value)
				<< ", DependencyPropertyValueSource::Template);\n";
		else
			code << indentStr << "(void)" << name
				<< "->TrySetPropertyValue(L\""
				<< EscapeWStringLiteral(propertyName) << "\", "
				<< GenerateStyleValueExpression(assignment->Value) << ");\n";
	}
	return code.str();
}

std::string CodeGenerator::GenerateTransformExpression(
	const cui::drawing::Transform& value)
{
	std::ostringstream expression;
	expression << "[] { cui::drawing::Transform value; ";
	for (const auto& operation : value.Operations)
	{
		expression << "{ cui::drawing::TransformOperation operation; operation.Kind = ";
		switch (operation.Kind)
		{
		case cui::drawing::TransformKind::Matrix:
			expression << "cui::drawing::TransformKind::Matrix; operation.Matrix = "
				"D2D1::Matrix3x2F(" << FloatLiteral(operation.Matrix._11) << ", "
				<< FloatLiteral(operation.Matrix._12) << ", "
				<< FloatLiteral(operation.Matrix._21) << ", "
				<< FloatLiteral(operation.Matrix._22) << ", "
				<< FloatLiteral(operation.Matrix._31) << ", "
				<< FloatLiteral(operation.Matrix._32) << "); ";
			break;
		case cui::drawing::TransformKind::Translate:
			expression << "cui::drawing::TransformKind::Translate; operation.X = "
				<< FloatLiteral(operation.X) << "; operation.Y = "
				<< FloatLiteral(operation.Y) << "; ";
			break;
		case cui::drawing::TransformKind::Scale:
			expression << "cui::drawing::TransformKind::Scale; operation.ScaleX = "
				<< FloatLiteral(operation.ScaleX) << "; operation.ScaleY = "
				<< FloatLiteral(operation.ScaleY) << "; operation.CenterX = "
				<< FloatLiteral(operation.CenterX) << "; operation.CenterY = "
				<< FloatLiteral(operation.CenterY) << "; ";
			break;
		case cui::drawing::TransformKind::Rotate:
			expression << "cui::drawing::TransformKind::Rotate; operation.Angle = "
				<< FloatLiteral(operation.Angle) << "; operation.CenterX = "
				<< FloatLiteral(operation.CenterX) << "; operation.CenterY = "
				<< FloatLiteral(operation.CenterY) << "; ";
			break;
		case cui::drawing::TransformKind::Skew:
			expression << "cui::drawing::TransformKind::Skew; operation.AngleX = "
				<< FloatLiteral(operation.AngleX) << "; operation.AngleY = "
				<< FloatLiteral(operation.AngleY) << "; operation.CenterX = "
				<< FloatLiteral(operation.CenterX) << "; operation.CenterY = "
				<< FloatLiteral(operation.CenterY) << "; ";
			break;
		}
		expression << "value.Operations.push_back(operation); } ";
	}
	expression << "return value; }()";
	return expression.str();
}

std::string CodeGenerator::GenerateGeometryExpression(
	const cui::drawing::Geometry& geometry)
{
	std::ostringstream expression;
	expression << "[] { cui::drawing::Geometry value; value.Kind = ";
	switch (geometry.Kind)
	{
	case cui::drawing::GeometryKind::Rectangle:
		expression << "cui::drawing::GeometryKind::Rectangle; value.Rect = D2D1::RectF("
			<< FloatLiteral(geometry.Rect.left) << ", "
			<< FloatLiteral(geometry.Rect.top) << ", "
			<< FloatLiteral(geometry.Rect.right) << ", "
			<< FloatLiteral(geometry.Rect.bottom) << "); value.RadiusX = "
			<< FloatLiteral(geometry.RadiusX) << "; value.RadiusY = "
			<< FloatLiteral(geometry.RadiusY) << "; ";
		break;
	case cui::drawing::GeometryKind::Ellipse:
		expression << "cui::drawing::GeometryKind::Ellipse; value.Center = D2D1::Point2F("
			<< FloatLiteral(geometry.Center.x) << ", "
			<< FloatLiteral(geometry.Center.y) << "); value.RadiusX = "
			<< FloatLiteral(geometry.RadiusX) << "; value.RadiusY = "
			<< FloatLiteral(geometry.RadiusY) << "; ";
		break;
	case cui::drawing::GeometryKind::Path:
		expression << "cui::drawing::GeometryKind::Path; ";
		if (geometry.FillRule == cui::drawing::GeometryFillRule::Nonzero)
			expression << "value.FillRule = cui::drawing::GeometryFillRule::Nonzero; ";
		for (const auto& figure : geometry.Figures)
		{
			expression << "value.Figures.push_back([] { cui::drawing::PathFigure figure; "
				"figure.StartPoint = D2D1::Point2F("
				<< FloatLiteral(figure.StartPoint.x) << ", "
				<< FloatLiteral(figure.StartPoint.y) << "); figure.IsClosed = "
				<< (figure.IsClosed ? "true" : "false") << "; figure.IsFilled = "
				<< (figure.IsFilled ? "true" : "false") << "; ";
			for (const auto& segment : figure.Segments)
			{
				expression << "{ cui::drawing::PathSegment segment; segment.Kind = ";
				const char* kind = segment.Kind == cui::drawing::PathSegmentKind::Line
					? "cui::drawing::PathSegmentKind::Line"
					: segment.Kind == cui::drawing::PathSegmentKind::Bezier
						? "cui::drawing::PathSegmentKind::Bezier"
						: segment.Kind == cui::drawing::PathSegmentKind::QuadraticBezier
							? "cui::drawing::PathSegmentKind::QuadraticBezier"
							: "cui::drawing::PathSegmentKind::Arc";
				expression << kind << "; segment.Point = D2D1::Point2F("
					<< FloatLiteral(segment.Point.x) << ", "
					<< FloatLiteral(segment.Point.y) << "); segment.Point1 = D2D1::Point2F("
					<< FloatLiteral(segment.Point1.x) << ", "
					<< FloatLiteral(segment.Point1.y) << "); segment.Point2 = D2D1::Point2F("
					<< FloatLiteral(segment.Point2.x) << ", "
					<< FloatLiteral(segment.Point2.y) << "); segment.Point3 = D2D1::Point2F("
					<< FloatLiteral(segment.Point3.x) << ", "
					<< FloatLiteral(segment.Point3.y) << "); segment.Size = D2D1::SizeF("
					<< FloatLiteral(segment.Size.width) << ", "
					<< FloatLiteral(segment.Size.height) << "); segment.RotationAngle = "
					<< FloatLiteral(segment.RotationAngle) << "; segment.IsLargeArc = "
					<< (segment.IsLargeArc ? "true" : "false") << "; segment.Sweep = "
					<< (segment.Sweep == cui::drawing::SweepDirection::Clockwise
						? "cui::drawing::SweepDirection::Clockwise"
						: "cui::drawing::SweepDirection::Counterclockwise")
					<< "; figure.Segments.push_back(segment); } ";
			}
			expression << "return figure; }()); ";
		}
		break;
	case cui::drawing::GeometryKind::Group:
		expression << "cui::drawing::GeometryKind::Group; ";
		if (geometry.FillRule == cui::drawing::GeometryFillRule::Nonzero)
			expression << "value.FillRule = cui::drawing::GeometryFillRule::Nonzero; ";
		for (const auto& child : geometry.Children)
			expression << "value.Children.push_back("
				<< GenerateGeometryExpression(child) << "); ";
		break;
	}
	if (geometry.LocalTransform)
		expression << "value.LocalTransform = "
			<< GenerateTransformExpression(*geometry.LocalTransform) << "; ";
	expression << "return value; }()";
	return expression.str();
}

std::string CodeGenerator::GenerateStyleValueExpression(const DesignerStyleValue& value)
{
	if (value.Kind == DesignerStyleValueKind::ImageSource)
	{
		return "BindingValue(cui::resources::LoadBitmapResource(L\""
			+ EscapeWStringLiteral(value.Text) + "\"))";
	}
	BindingValue runtimeValue;
	if (!DesignerStyleSheetUtils::TryConvertValue(
		value, runtimeValue, nullptr, _resourceBasePath))
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
	case DesignerStyleValueKind::Point:
	{
		cui::core::Point parsed{};
		runtimeValue.TryGet(parsed);
		return "BindingValue(cui::core::Point{ " + FloatLiteral(parsed.x)
			+ ", " + FloatLiteral(parsed.y) + " })";
	}
	case DesignerStyleValueKind::Vector:
	{
		cui::core::Vector parsed{};
		runtimeValue.TryGet(parsed);
		return "BindingValue(cui::core::Vector{ " + FloatLiteral(parsed.x)
			+ ", " + FloatLiteral(parsed.y) + " })";
	}
	case DesignerStyleValueKind::Rect:
	{
		cui::core::Rect parsed{};
		runtimeValue.TryGet(parsed);
		return "BindingValue(cui::core::Rect{ " + FloatLiteral(parsed.x)
			+ ", " + FloatLiteral(parsed.y)
			+ ", " + FloatLiteral(parsed.width)
			+ ", " + FloatLiteral(parsed.height) + " })";
	}
	case DesignerStyleValueKind::Size:
	{
		cui::core::Size parsed{};
		runtimeValue.TryGet(parsed);
		return "BindingValue(cui::core::Size{ " + FloatLiteral(parsed.width)
			+ ", " + FloatLiteral(parsed.height) + " })";
	}
	case DesignerStyleValueKind::Matrix:
	{
		D2D1_MATRIX_3X2_F parsed{};
		runtimeValue.TryGet(parsed);
		return "BindingValue(D2D1::Matrix3x2F(" + FloatLiteral(parsed._11)
			+ ", " + FloatLiteral(parsed._12)
			+ ", " + FloatLiteral(parsed._21)
			+ ", " + FloatLiteral(parsed._22)
			+ ", " + FloatLiteral(parsed._31)
			+ ", " + FloatLiteral(parsed._32) + "))";
	}
	case DesignerStyleValueKind::Length:
	{
		cui::layout::Length parsed;
		runtimeValue.TryGet(parsed);
		return parsed.IsAuto()
			? "BindingValue(cui::layout::Length::Auto())"
			: "BindingValue(cui::layout::Length::Fixed(" + FloatLiteral(parsed.value) + "))";
	}
	case DesignerStyleValueKind::Brush:
	{
		cui::drawing::Brush parsed;
		if (!runtimeValue.TryGet(parsed)) return "BindingValue()";
		if (parsed.Kind == cui::drawing::BrushKind::None)
			return "BindingValue(cui::drawing::NoBrush())";
		std::ostringstream expression;
		expression << "BindingValue([] { cui::drawing::Brush value; value.Kind = "
			<< (parsed.Kind == cui::drawing::BrushKind::Solid
				? "cui::drawing::BrushKind::Solid"
				: parsed.Kind == cui::drawing::BrushKind::LinearGradient
					? "cui::drawing::BrushKind::LinearGradient"
					: parsed.Kind == cui::drawing::BrushKind::RadialGradient
						? "cui::drawing::BrushKind::RadialGradient"
						: "cui::drawing::BrushKind::Image")
			<< "; value.MappingMode = "
			<< (parsed.MappingMode == cui::drawing::BrushMappingMode::Absolute
				? "cui::drawing::BrushMappingMode::Absolute"
				: "cui::drawing::BrushMappingMode::RelativeToBoundingBox")
			<< "; value.Opacity = " << FloatLiteral(parsed.Opacity) << "; ";
		if (parsed.Kind == cui::drawing::BrushKind::Solid)
			expression << "value.Color = " << ColorToString(parsed.Color) << "; ";
		else if (parsed.Kind == cui::drawing::BrushKind::LinearGradient)
			expression << "value.StartPoint = D2D1::Point2F("
				<< FloatLiteral(parsed.StartPoint.x) << ", "
				<< FloatLiteral(parsed.StartPoint.y) << "); value.EndPoint = D2D1::Point2F("
				<< FloatLiteral(parsed.EndPoint.x) << ", "
				<< FloatLiteral(parsed.EndPoint.y) << "); ";
		else if (parsed.Kind == cui::drawing::BrushKind::RadialGradient)
			expression << "value.Center = D2D1::Point2F("
				<< FloatLiteral(parsed.Center.x) << ", "
				<< FloatLiteral(parsed.Center.y) << "); value.GradientOrigin = D2D1::Point2F("
				<< FloatLiteral(parsed.GradientOrigin.x) << ", "
				<< FloatLiteral(parsed.GradientOrigin.y) << "); value.RadiusX = "
				<< FloatLiteral(parsed.RadiusX) << "; value.RadiusY = "
				<< FloatLiteral(parsed.RadiusY) << "; ";
		else
		{
			expression << "value.ImageSource = cui::resources::LoadBitmapResource(L\""
				<< EscapeWStringLiteral(parsed.ImageSource
					? parsed.ImageSource->GetSourceUri() : L"") << "\"); ";
			expression << "value.Stretch = "
				<< (parsed.Stretch == cui::drawing::ImageBrushStretch::None
					? "cui::drawing::ImageBrushStretch::None"
					: parsed.Stretch == cui::drawing::ImageBrushStretch::Uniform
						? "cui::drawing::ImageBrushStretch::Uniform"
						: parsed.Stretch == cui::drawing::ImageBrushStretch::UniformToFill
							? "cui::drawing::ImageBrushStretch::UniformToFill"
							: "cui::drawing::ImageBrushStretch::Fill") << "; ";
			expression << "value.AlignmentX = "
				<< (parsed.AlignmentX == cui::drawing::ImageBrushAlignmentX::Left
					? "cui::drawing::ImageBrushAlignmentX::Left"
					: parsed.AlignmentX == cui::drawing::ImageBrushAlignmentX::Right
						? "cui::drawing::ImageBrushAlignmentX::Right"
						: "cui::drawing::ImageBrushAlignmentX::Center") << "; ";
			expression << "value.AlignmentY = "
				<< (parsed.AlignmentY == cui::drawing::ImageBrushAlignmentY::Top
					? "cui::drawing::ImageBrushAlignmentY::Top"
					: parsed.AlignmentY == cui::drawing::ImageBrushAlignmentY::Bottom
						? "cui::drawing::ImageBrushAlignmentY::Bottom"
						: "cui::drawing::ImageBrushAlignmentY::Center") << "; ";
		}
		if (parsed.Kind == cui::drawing::BrushKind::LinearGradient
			|| parsed.Kind == cui::drawing::BrushKind::RadialGradient)
			for (const auto& stop : parsed.GradientStops)
				expression << "value.GradientStops.push_back({ "
					<< FloatLiteral(stop.Offset) << ", "
					<< ColorToString(stop.Color) << " }); ";
		if (parsed.Transform)
			expression << "value.Transform = "
				<< GenerateTransformExpression(*parsed.Transform) << "; ";
		if (parsed.RelativeTransform)
			expression << "value.RelativeTransform = "
				<< GenerateTransformExpression(*parsed.RelativeTransform) << "; ";
		expression << "return value; }())";
		return expression.str();
	}
	case DesignerStyleValueKind::Geometry:
	{
		cui::drawing::Geometry parsed;
		if (!runtimeValue.TryGet(parsed)) return "BindingValue()";
		return "BindingValue(" + GenerateGeometryExpression(parsed) + ")";
	}
	case DesignerStyleValueKind::Transform:
	{
		cui::drawing::Transform parsed;
		if (!runtimeValue.TryGet(parsed)) return "BindingValue()";
		return "BindingValue(" + GenerateTransformExpression(parsed) + ")";
	}
	case DesignerStyleValueKind::ImageSource:
		break;
	}
	return "BindingValue()";
}

std::string CodeGenerator::GenerateBindingValueExpression(
	const BindingValue& value)
{
	switch (value.Kind())
	{
	case BindingValueKind::Empty:
		return "BindingValue()";
	case BindingValueKind::Bool:
	{
		bool parsed = false;
		if (!value.TryGet(parsed)) break;
		return std::string("BindingValue(")
			+ (parsed ? "true)" : "false)");
	}
	case BindingValueKind::Int:
	{
		int parsed = 0;
		if (!value.TryGet(parsed)) break;
		return "BindingValue(" + std::to_string(parsed) + ")";
	}
	case BindingValueKind::Int64:
	{
		long long parsed = 0;
		if (!value.TryGet(parsed)) break;
		return "BindingValue(" + std::to_string(parsed) + "LL)";
	}
	case BindingValueKind::Float:
	{
		float parsed = 0.0f;
		if (!value.TryGet(parsed)) break;
		return "BindingValue(" + FloatLiteral(parsed) + ")";
	}
	case BindingValueKind::Double:
	{
		double parsed = 0.0;
		if (!value.TryGet(parsed)) break;
		return "BindingValue(" + DoubleLiteral(parsed) + ")";
	}
	case BindingValueKind::String:
	{
		std::wstring parsed;
		if (!value.TryGet(parsed)) break;
		return "BindingValue(L\"" + EscapeWStringLiteral(parsed) + "\")";
	}
	case BindingValueKind::Object:
		break;
	}

	D2D1_COLOR_F color{};
	if (value.TryGet(color))
		return "BindingValue(" + ColorToString(color) + ")";
	Thickness thickness;
	if (value.TryGet(thickness))
		return "BindingValue(" + ThicknessToString(thickness) + ")";
	cui::core::Point point{};
	if (value.TryGet(point))
		return "BindingValue(cui::core::Point{ " + FloatLiteral(point.x)
			+ ", " + FloatLiteral(point.y) + " })";
	cui::core::Vector vector{};
	if (value.TryGet(vector))
		return "BindingValue(cui::core::Vector{ " + FloatLiteral(vector.x)
			+ ", " + FloatLiteral(vector.y) + " })";
	cui::core::Rect rect{};
	if (value.TryGet(rect))
		return "BindingValue(cui::core::Rect{ " + FloatLiteral(rect.x)
			+ ", " + FloatLiteral(rect.y)
			+ ", " + FloatLiteral(rect.width)
			+ ", " + FloatLiteral(rect.height) + " })";
	cui::core::Size size{};
	if (value.TryGet(size))
		return "BindingValue(cui::core::Size{ " + FloatLiteral(size.width)
			+ ", " + FloatLiteral(size.height) + " })";
	D2D1_MATRIX_3X2_F matrix{};
	if (value.TryGet(matrix))
		return "BindingValue(D2D1::Matrix3x2F("
			+ FloatLiteral(matrix._11) + ", " + FloatLiteral(matrix._12)
			+ ", " + FloatLiteral(matrix._21) + ", "
			+ FloatLiteral(matrix._22) + ", " + FloatLiteral(matrix._31)
			+ ", " + FloatLiteral(matrix._32) + "))";
	cui::layout::Length length;
	if (value.TryGet(length))
		return length.IsAuto()
			? "BindingValue(cui::layout::Length::Auto())"
			: "BindingValue(cui::layout::Length::Fixed("
				+ FloatLiteral(length.value) + "))";
	std::shared_ptr<BitmapSource> bitmap;
	if (value.TryGet(bitmap))
		return bitmap
			? "BindingValue(cui::resources::LoadBitmapResource(L\""
				+ EscapeWStringLiteral(bitmap->GetSourceUri()) + "\"))"
			: "BindingValue(std::shared_ptr<BitmapSource>{})";

	cui::drawing::Brush brush;
	if (value.TryGet(brush))
	{
		if (brush.Kind == cui::drawing::BrushKind::None)
			return "BindingValue(cui::drawing::NoBrush())";
		std::ostringstream expression;
		expression << "BindingValue([] { cui::drawing::Brush value; value.Kind = "
			<< (brush.Kind == cui::drawing::BrushKind::Solid
				? "cui::drawing::BrushKind::Solid"
				: brush.Kind == cui::drawing::BrushKind::LinearGradient
					? "cui::drawing::BrushKind::LinearGradient"
					: brush.Kind == cui::drawing::BrushKind::RadialGradient
						? "cui::drawing::BrushKind::RadialGradient"
						: "cui::drawing::BrushKind::Image")
			<< "; value.MappingMode = "
			<< (brush.MappingMode
					== cui::drawing::BrushMappingMode::Absolute
				? "cui::drawing::BrushMappingMode::Absolute"
				: "cui::drawing::BrushMappingMode::RelativeToBoundingBox")
			<< "; value.Opacity = " << FloatLiteral(brush.Opacity) << "; ";
		if (brush.Kind == cui::drawing::BrushKind::Solid)
			expression << "value.Color = "
				<< ColorToString(brush.Color) << "; ";
		else if (brush.Kind == cui::drawing::BrushKind::LinearGradient)
			expression << "value.StartPoint = D2D1::Point2F("
				<< FloatLiteral(brush.StartPoint.x) << ", "
				<< FloatLiteral(brush.StartPoint.y)
				<< "); value.EndPoint = D2D1::Point2F("
				<< FloatLiteral(brush.EndPoint.x) << ", "
				<< FloatLiteral(brush.EndPoint.y) << "); ";
		else if (brush.Kind == cui::drawing::BrushKind::RadialGradient)
			expression << "value.Center = D2D1::Point2F("
				<< FloatLiteral(brush.Center.x) << ", "
				<< FloatLiteral(brush.Center.y)
				<< "); value.GradientOrigin = D2D1::Point2F("
				<< FloatLiteral(brush.GradientOrigin.x) << ", "
				<< FloatLiteral(brush.GradientOrigin.y)
				<< "); value.RadiusX = "
				<< FloatLiteral(brush.RadiusX)
				<< "; value.RadiusY = "
				<< FloatLiteral(brush.RadiusY) << "; ";
		else
		{
			expression
				<< "value.ImageSource = cui::resources::LoadBitmapResource(L\""
				<< EscapeWStringLiteral(brush.ImageSource
					? brush.ImageSource->GetSourceUri() : L"")
				<< "\"); value.Stretch = "
				<< (brush.Stretch
						== cui::drawing::ImageBrushStretch::None
					? "cui::drawing::ImageBrushStretch::None"
					: brush.Stretch
							== cui::drawing::ImageBrushStretch::Uniform
						? "cui::drawing::ImageBrushStretch::Uniform"
						: brush.Stretch
								== cui::drawing::ImageBrushStretch::UniformToFill
							? "cui::drawing::ImageBrushStretch::UniformToFill"
							: "cui::drawing::ImageBrushStretch::Fill")
				<< "; value.AlignmentX = "
				<< (brush.AlignmentX
						== cui::drawing::ImageBrushAlignmentX::Left
					? "cui::drawing::ImageBrushAlignmentX::Left"
					: brush.AlignmentX
							== cui::drawing::ImageBrushAlignmentX::Right
						? "cui::drawing::ImageBrushAlignmentX::Right"
						: "cui::drawing::ImageBrushAlignmentX::Center")
				<< "; value.AlignmentY = "
				<< (brush.AlignmentY
						== cui::drawing::ImageBrushAlignmentY::Top
					? "cui::drawing::ImageBrushAlignmentY::Top"
					: brush.AlignmentY
							== cui::drawing::ImageBrushAlignmentY::Bottom
						? "cui::drawing::ImageBrushAlignmentY::Bottom"
						: "cui::drawing::ImageBrushAlignmentY::Center")
				<< "; ";
		}
		if (brush.Kind == cui::drawing::BrushKind::LinearGradient
			|| brush.Kind == cui::drawing::BrushKind::RadialGradient)
			for (const auto& stop : brush.GradientStops)
				expression << "value.GradientStops.push_back({ "
					<< FloatLiteral(stop.Offset) << ", "
					<< ColorToString(stop.Color) << " }); ";
		if (brush.Transform)
			expression << "value.Transform = "
				<< GenerateTransformExpression(*brush.Transform) << "; ";
		if (brush.RelativeTransform)
			expression << "value.RelativeTransform = "
				<< GenerateTransformExpression(*brush.RelativeTransform)
				<< "; ";
		expression << "return value; }())";
		return expression.str();
	}
	cui::drawing::Geometry geometry;
	if (value.TryGet(geometry))
		return "BindingValue(" + GenerateGeometryExpression(geometry) + ")";
	cui::drawing::Transform transform;
	if (value.TryGet(transform))
		return "BindingValue(" + GenerateTransformExpression(transform) + ")";

	throw std::invalid_argument(
		"Static declarative interaction contains an unsupported BindingValue type");
}

std::string CodeGenerator::GenerateDeclarativeAnimationCode(
	const DeclarativeVisualStateAnimation& animation,
	const std::string& collectionExpression,
	int indent)
{
	const std::string base(indent, '\t');
	const std::string body(indent + 1, '\t');
	std::ostringstream code;
	auto animationKind = [](DeclarativeAnimationKind value)
	{
		switch (value)
		{
		case DeclarativeAnimationKind::Color:
			return "DeclarativeAnimationKind::Color";
		case DeclarativeAnimationKind::Thickness:
			return "DeclarativeAnimationKind::Thickness";
		case DeclarativeAnimationKind::Point:
			return "DeclarativeAnimationKind::Point";
		case DeclarativeAnimationKind::Vector:
			return "DeclarativeAnimationKind::Vector";
		case DeclarativeAnimationKind::Rect:
			return "DeclarativeAnimationKind::Rect";
		case DeclarativeAnimationKind::Size:
			return "DeclarativeAnimationKind::Size";
		case DeclarativeAnimationKind::Matrix:
			return "DeclarativeAnimationKind::Matrix";
		case DeclarativeAnimationKind::Object:
			return "DeclarativeAnimationKind::Object";
		case DeclarativeAnimationKind::Double:
		default:
			return "DeclarativeAnimationKind::Double";
		}
	};
	auto easing = [](DeclarativeEasingKind value)
	{
		switch (value)
		{
		case DeclarativeEasingKind::Quadratic:
			return "DeclarativeEasingKind::Quadratic";
		case DeclarativeEasingKind::Cubic:
			return "DeclarativeEasingKind::Cubic";
		case DeclarativeEasingKind::Sine:
			return "DeclarativeEasingKind::Sine";
		case DeclarativeEasingKind::Linear:
		default:
			return "DeclarativeEasingKind::Linear";
		}
	};
	auto easingMode = [](DeclarativeEasingMode value)
	{
		switch (value)
		{
		case DeclarativeEasingMode::EaseIn:
			return "DeclarativeEasingMode::EaseIn";
		case DeclarativeEasingMode::EaseInOut:
			return "DeclarativeEasingMode::EaseInOut";
		case DeclarativeEasingMode::EaseOut:
		default:
			return "DeclarativeEasingMode::EaseOut";
		}
	};

	code << base << "{\n";
	code << body << "DeclarativeVisualStateAnimation animation;\n";
	code << body << "animation.Kind = "
		<< animationKind(animation.Kind) << ";\n";
	code << body << "animation.TargetName = L\""
		<< EscapeWStringLiteral(animation.TargetName) << "\";\n";
	code << body << "animation.PropertyName = L\""
		<< EscapeWStringLiteral(animation.PropertyName) << "\";\n";
	if (animation.From)
		code << body << "animation.From = "
			<< GenerateBindingValueExpression(*animation.From) << ";\n";
	if (animation.To)
		code << body << "animation.To = "
			<< GenerateBindingValueExpression(*animation.To) << ";\n";
	if (animation.By)
		code << body << "animation.By = "
			<< GenerateBindingValueExpression(*animation.By) << ";\n";
	code << body << "animation.IsAdditive = "
		<< (animation.IsAdditive ? "true" : "false") << ";\n";
	code << body << "animation.IsCumulative = "
		<< (animation.IsCumulative ? "true" : "false") << ";\n";
	code << body << "animation.BeginTimeMilliseconds = "
		<< animation.BeginTimeMilliseconds << "ULL;\n";
	code << body << "animation.DurationMilliseconds = "
		<< animation.DurationMilliseconds << "ULL;\n";
	code << body << "animation.RepeatBehavior = "
		<< (animation.RepeatBehavior == DeclarativeRepeatBehaviorKind::Duration
			? "DeclarativeRepeatBehaviorKind::Duration"
			: animation.RepeatBehavior == DeclarativeRepeatBehaviorKind::Forever
				? "DeclarativeRepeatBehaviorKind::Forever"
				: "DeclarativeRepeatBehaviorKind::Count")
		<< ";\n";
	code << body << "animation.RepeatCount = "
		<< DoubleLiteral(animation.RepeatCount) << ";\n";
	code << body << "animation.RepeatDurationMilliseconds = "
		<< animation.RepeatDurationMilliseconds << "ULL;\n";
	code << body << "animation.AutoReverse = "
		<< (animation.AutoReverse ? "true" : "false") << ";\n";
	code << body << "animation.FillBehavior = "
		<< (animation.FillBehavior == DeclarativeTimelineFillBehavior::Stop
			? "DeclarativeTimelineFillBehavior::Stop"
			: "DeclarativeTimelineFillBehavior::HoldEnd")
		<< ";\n";
	code << body << "animation.SpeedRatio = "
		<< DoubleLiteral(animation.SpeedRatio) << ";\n";
	code << body << "animation.AccelerationRatio = "
		<< DoubleLiteral(animation.AccelerationRatio) << ";\n";
	code << body << "animation.DecelerationRatio = "
		<< DoubleLiteral(animation.DecelerationRatio) << ";\n";
	code << body << "animation.Easing = "
		<< easing(animation.Easing) << ";\n";
	code << body << "animation.EasingMode = "
		<< easingMode(animation.EasingMode) << ";\n";
	for (const auto& keyFrame : animation.KeyFrames)
	{
		code << body << "{\n";
		code << body << "\tDeclarativeAnimationKeyFrame keyFrame;\n";
		code << body << "\tkeyFrame.Kind = "
			<< (keyFrame.Kind == DeclarativeKeyFrameKind::Discrete
				? "DeclarativeKeyFrameKind::Discrete"
				: keyFrame.Kind == DeclarativeKeyFrameKind::Easing
					? "DeclarativeKeyFrameKind::Easing"
					: keyFrame.Kind == DeclarativeKeyFrameKind::Spline
						? "DeclarativeKeyFrameKind::Spline"
						: "DeclarativeKeyFrameKind::Linear")
			<< ";\n";
		code << body << "\tkeyFrame.KeyTimeMilliseconds = "
			<< keyFrame.KeyTimeMilliseconds << "ULL;\n";
		code << body << "\tkeyFrame.Value = "
			<< GenerateBindingValueExpression(keyFrame.Value) << ";\n";
		code << body << "\tkeyFrame.Easing = "
			<< easing(keyFrame.Easing) << ";\n";
		code << body << "\tkeyFrame.EasingMode = "
			<< easingMode(keyFrame.EasingMode) << ";\n";
		code << body << "\tkeyFrame.KeySplineX1 = "
			<< FloatLiteral(keyFrame.KeySplineX1) << ";\n";
		code << body << "\tkeyFrame.KeySplineY1 = "
			<< FloatLiteral(keyFrame.KeySplineY1) << ";\n";
		code << body << "\tkeyFrame.KeySplineX2 = "
			<< FloatLiteral(keyFrame.KeySplineX2) << ";\n";
		code << body << "\tkeyFrame.KeySplineY2 = "
			<< FloatLiteral(keyFrame.KeySplineY2) << ";\n";
		code << body
			<< "\tanimation.KeyFrames.push_back(std::move(keyFrame));\n";
		code << body << "}\n";
	}
	code << body << collectionExpression
		<< ".push_back(std::move(animation));\n";
	code << base << "}\n";
	return code.str();
}

std::string CodeGenerator::GenerateDeclarativeStoryboardActionsCode(
	const std::vector<DeclarativeEventTriggerActionDefinition>& actions,
	const std::string& collectionExpression,
	int indent)
{
	const std::string base(indent, '\t');
	const std::string body(indent + 1, '\t');
	std::ostringstream code;
	for (const auto& action : actions)
	{
		code << base << "{\n";
		code << body
			<< "DeclarativeEventTriggerActionDefinition action;\n";
		code << body << "action.Kind = "
			<< (action.Kind == DeclarativeStoryboardActionKind::Begin
				? "DeclarativeStoryboardActionKind::Begin"
				: action.Kind == DeclarativeStoryboardActionKind::Pause
					? "DeclarativeStoryboardActionKind::Pause"
					: action.Kind == DeclarativeStoryboardActionKind::Resume
						? "DeclarativeStoryboardActionKind::Resume"
						: "DeclarativeStoryboardActionKind::Stop")
			<< ";\n";
		code << body << "action.StoryboardName = L\""
			<< EscapeWStringLiteral(action.StoryboardName) << "\";\n";
		for (const auto& animation : action.Animations)
			code << GenerateDeclarativeAnimationCode(
				animation, "action.Animations", indent + 1);
		code << body << collectionExpression
			<< ".push_back(std::move(action));\n";
		code << base << "}\n";
	}
	return code.str();
}

std::string CodeGenerator::GenerateDeclarativeInteractionsCode(
	const std::vector<DeclarativeVisualStateGroupDefinition>& visualStateGroups,
	const std::vector<DeclarativeEventTriggerDefinition>& eventTriggers,
	const std::string& targetExpression,
	int indent)
{
	if (visualStateGroups.empty() && eventTriggers.empty()) return {};
	const std::string tabs(indent, '\t');
	const std::string inner(indent + 1, '\t');
	const std::string deep(indent + 2, '\t');
	std::ostringstream code;
	auto easing = [](DeclarativeEasingKind value)
	{
		switch (value)
		{
		case DeclarativeEasingKind::Quadratic:
			return "DeclarativeEasingKind::Quadratic";
		case DeclarativeEasingKind::Cubic:
			return "DeclarativeEasingKind::Cubic";
		case DeclarativeEasingKind::Sine:
			return "DeclarativeEasingKind::Sine";
		case DeclarativeEasingKind::Linear:
		default:
			return "DeclarativeEasingKind::Linear";
		}
	};
	auto easingMode = [](DeclarativeEasingMode value)
	{
		switch (value)
		{
		case DeclarativeEasingMode::EaseIn:
			return "DeclarativeEasingMode::EaseIn";
		case DeclarativeEasingMode::EaseInOut:
			return "DeclarativeEasingMode::EaseInOut";
		case DeclarativeEasingMode::EaseOut:
		default:
			return "DeclarativeEasingMode::EaseOut";
		}
	};
	code << tabs << "{\n";
	code << inner
		<< "std::vector<DeclarativeVisualStateGroupDefinition> "
			"visualStateGroups;\n";
	code << inner
		<< "std::vector<DeclarativeEventTriggerDefinition> eventTriggers;\n";
	for (const auto& sourceGroup : visualStateGroups)
	{
		code << inner << "{\n";
		code << deep << "DeclarativeVisualStateGroupDefinition group;\n";
		code << deep << "group.Name = L\""
			<< EscapeWStringLiteral(sourceGroup.Name) << "\";\n";
		for (const auto& sourceState : sourceGroup.States)
		{
			code << deep << "{\n";
			code << deep << "\tDeclarativeVisualStateDefinition state;\n";
			code << deep << "\tstate.Name = L\""
				<< EscapeWStringLiteral(sourceState.Name) << "\";\n";
			for (const auto& eventName : sourceState.EventNames)
				code << deep << "\tstate.EventNames.push_back(L\""
					<< EscapeWStringLiteral(eventName) << "\");\n";
			for (const auto& condition : sourceState.Conditions)
				code << deep << "\tstate.Conditions.push_back({ L\""
					<< EscapeWStringLiteral(condition.PropertyName) << "\", "
					<< GenerateBindingValueExpression(condition.Value)
					<< " });\n";
			for (const auto& setter : sourceState.Setters)
				code << deep << "\tstate.Setters.push_back({ L\""
					<< EscapeWStringLiteral(setter.TargetName) << "\", L\""
					<< EscapeWStringLiteral(setter.PropertyName) << "\", "
					<< GenerateBindingValueExpression(setter.Value)
					<< " });\n";
			for (const auto& animation : sourceState.Animations)
				code << GenerateDeclarativeAnimationCode(
					animation, "state.Animations", indent + 3);
			code << deep
				<< "\tgroup.States.push_back(std::move(state));\n";
			code << deep << "}\n";
		}
		for (const auto& sourceTransition : sourceGroup.Transitions)
		{
			code << deep << "{\n";
			code << deep
				<< "\tDeclarativeVisualTransitionDefinition transition;\n";
			code << deep << "\ttransition.FromState = L\""
				<< EscapeWStringLiteral(sourceTransition.FromState) << "\";\n";
			code << deep << "\ttransition.ToState = L\""
				<< EscapeWStringLiteral(sourceTransition.ToState) << "\";\n";
			code << deep << "\ttransition.GeneratedDurationMilliseconds = "
				<< sourceTransition.GeneratedDurationMilliseconds << "ULL;\n";
			code << deep << "\ttransition.GeneratedEasing = "
				<< easing(sourceTransition.GeneratedEasing) << ";\n";
			code << deep << "\ttransition.GeneratedEasingMode = "
				<< easingMode(sourceTransition.GeneratedEasingMode) << ";\n";
			for (const auto& animation : sourceTransition.Animations)
				code << GenerateDeclarativeAnimationCode(
					animation, "transition.Animations", indent + 3);
			code << deep
				<< "\tgroup.Transitions.push_back(std::move(transition));\n";
			code << deep << "}\n";
		}
		code << deep
			<< "visualStateGroups.push_back(std::move(group));\n";
		code << inner << "}\n";
	}
	for (const auto& sourceTrigger : eventTriggers)
	{
		code << inner << "{\n";
		code << deep << "DeclarativeEventTriggerDefinition trigger;\n";
		code << deep << "trigger.EventName = L\""
			<< EscapeWStringLiteral(sourceTrigger.EventName) << "\";\n";
		code << GenerateDeclarativeStoryboardActionsCode(
			sourceTrigger.Actions, "trigger.Actions", indent + 2);
		code << deep
			<< "eventTriggers.push_back(std::move(trigger));\n";
		code << inner << "}\n";
	}
	code << inner << "std::wstring interactionError;\n";
	code << inner << "if (!cui::framework::XamlAccess::DefineInteractions("
		<< targetExpression
		<< ", std::move(visualStateGroups), std::move(eventTriggers), "
			"&interactionError))\n";
	code << deep << "return fail(L\"ControlTemplate 声明交互安装失败：\" "
		"+ interactionError);\n";
	code << tabs << "}\n";
	return code.str();
}

std::string CodeGenerator::GenerateStyleSheetCode(
	int indent,
	const std::vector<std::pair<std::wstring, std::string>>& objectResources)
{
	if (_styleSheet.Empty() && objectResources.empty()) return "";
	std::ostringstream code;
	const std::string indentStr(indent, '\t');
	auto styleSheet = _styleSheet;
	DesignerStyleSheetUtils::Canonicalize(styleSheet);
	DesignerStyleSheet resolvedStyleSheet;
	std::wstring inheritanceError;
	if (!DesignerStyleSheetUtils::ExpandRuntimeRules(
		styleSheet, resolvedStyleSheet, &inheritanceError))
		throw std::invalid_argument(WStringToString(inheritanceError));
	styleSheet = std::move(resolvedStyleSheet);

	code << indentStr << "// 文档级控件样式\n";
	code << indentStr << "auto __styleSheet = std::make_shared<ControlStyleSheet>();\n";
	for (const auto& resource : styleSheet.Resources)
	{
		code << indentStr << "__styleSheet->SetResource(L\""
			<< EscapeWStringLiteral(resource.Key) << "\", "
			<< GenerateStyleValueExpression(resource.Value) << ");\n";
	}
	for (const auto& [key, expression] : objectResources)
		code << indentStr << "__styleSheet->SetResource(L\""
			<< EscapeWStringLiteral(key) << "\", "
			<< expression << ");\n";
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
		if (!rule.ComponentType.Empty())
		{
			code << indentStr << selectorName
				<< ".DeclarativeTypeNamespace = L\""
				<< EscapeWStringLiteral(rule.ComponentType.XamlNamespace)
				<< "\";\n";
			code << indentStr << selectorName
				<< ".DeclarativeTypeName = L\""
				<< EscapeWStringLiteral(rule.ComponentType.XamlName) << "\";\n";
		}
		else if (rule.XamlType.Valid())
		{
			code << indentStr << selectorName
				<< ".DeclarativeTypeNamespace = L\""
				<< EscapeWStringLiteral(rule.XamlType.NamespaceUri) << "\";\n";
			code << indentStr << selectorName
				<< ".DeclarativeTypeName = L\""
				<< EscapeWStringLiteral(rule.XamlType.LocalName) << "\";\n";
		}
		if (!rule.Id.empty())
			code << indentStr << selectorName << ".StyleResourceKey = L\""
				<< EscapeWStringLiteral(rule.Id) << "\";\n";
		for (const auto& condition : rule.PropertyConditions)
			code << indentStr << selectorName
				<< ".PropertyConditions.push_back({ L\""
				<< EscapeWStringLiteral(condition.Property) << "\", "
				<< GenerateStyleValueExpression(condition.Value) << " });\n";
		for (const auto& condition : rule.DataConditions)
			code << indentStr << selectorName
				<< ".DataConditions.push_back({ L\""
				<< EscapeWStringLiteral(condition.SourceProperty) << "\", "
				<< GenerateStyleValueExpression(condition.Value) << " });\n";
		std::vector<DeclarativeEventTriggerActionDefinition> enterActions;
		std::vector<DeclarativeEventTriggerActionDefinition> exitActions;
		std::wstring actionError;
		if (!DesignerStyleSheetUtils::MaterializeStoryboardActions(
			rule.EnterActions, styleSheet, enterActions, &actionError,
			_resourceBasePath, _sourceDocument.Resources,
			L"Style Trigger.EnterActions")
			|| !DesignerStyleSheetUtils::MaterializeStoryboardActions(
				rule.ExitActions, styleSheet, exitActions, &actionError,
				_resourceBasePath, _sourceDocument.Resources,
				L"Style Trigger.ExitActions"))
			throw std::invalid_argument(WStringToString(actionError));
		const auto enterActionsName =
			"__styleEnterActions" + std::to_string(index + 1);
		const auto exitActionsName =
			"__styleExitActions" + std::to_string(index + 1);
		if (!enterActions.empty() || !exitActions.empty())
		{
			code << indentStr
				<< "std::vector<DeclarativeEventTriggerActionDefinition> "
				<< enterActionsName << ";\n";
			code << GenerateDeclarativeStoryboardActionsCode(
				enterActions, enterActionsName, indent);
			code << indentStr
				<< "std::vector<DeclarativeEventTriggerActionDefinition> "
				<< exitActionsName << ";\n";
			code << GenerateDeclarativeStoryboardActionsCode(
				exitActions, exitActionsName, indent);
		}
		code << indentStr << "__styleSheet->AddRule(std::move(" << selectorName << "), {\n";
		std::vector<const DesignerStyleSetter*> emittedSetters;
		for (const auto& setter : rule.Setters)
			emittedSetters.push_back(&setter);
		for (size_t setterIndex = 0;
			setterIndex < emittedSetters.size(); ++setterIndex)
		{
			const auto& setter = *emittedSetters[setterIndex];
			code << indentStr << "\t";
			if (setter.UsesResource)
				code << (setter.UsesDynamicResource
					? "ControlStyleSetter::DynamicResource(L\""
					: "ControlStyleSetter::Resource(L\"")
					<< EscapeWStringLiteral(setter.PropertyName) << "\", L\""
					<< EscapeWStringLiteral(setter.ResourceKey) << "\")";
			else
				code << "ControlStyleSetter(L\"" << EscapeWStringLiteral(setter.PropertyName)
					<< "\", " << GenerateStyleValueExpression(setter.Literal) << ")";
			if (setterIndex + 1 < emittedSetters.size()) code << ",";
			code << "\n";
		}
		code << indentStr << "}";
		if (!enterActions.empty() || !exitActions.empty())
			code << ", std::move(" << enterActionsName
				<< "), std::move(" << exitActionsName << ")";
		code << ");\n";
	}
	// Window is the document root. One stylesheet attachment covers the root
	// itself and the complete logical Content subtree.
	code << indentStr
		<< "cui::framework::StyleAccess::SetDocumentStyles("
		"*this, __styleSheet, true);\n";
	code << "\n";
	return code.str();
}

std::string CodeGenerator::GenerateLocalResources(
	const DesignerModel::DesignNode& node,
	int indent,
	const DesignerModel::DesignDocument* sourceDocument,
	const std::vector<std::pair<std::wstring, std::string>>* objectResources)
{
	if (node.LocalResources.Empty()) return {};
	const auto& document =
		sourceDocument ? *sourceDocument : _sourceDocument;
	const std::string indentStr(indent, '\t');
	const std::string controlName = GetVarName(node);
	const std::string dictionaryName = "__resources_" + controlName;
	DesignerStyleSheet visible = document.StyleSheet;
	std::vector<const DesignerModel::DesignNode*> route;
	for (auto* scope = &node; scope;)
	{
		route.push_back(scope);
		auto found = document.Nodes.end();
		if (scope->ParentId > 0)
			found = std::find_if(
				document.Nodes.begin(), document.Nodes.end(),
				[&](const auto& candidate)
				{ return candidate.Id == scope->ParentId; });
		else if (!scope->ParentRef.empty())
			found = std::find_if(
				document.Nodes.begin(), document.Nodes.end(),
				[&](const auto& candidate)
				{ return candidate.Name == scope->ParentRef; });
		scope = found == document.Nodes.end() ? nullptr : &*found;
	}
	for (auto scope = route.rbegin(); scope != route.rend(); ++scope)
		DesignerStyleSheetUtils::AppendLexicalScope(
			visible, (*scope)->LocalResources);
	std::wstring styleError;
	DesignerStyleSheet local;
	if (!DesignerStyleSheetUtils::PrepareLocalRuntimeStyleSheet(
		node.LocalResources, visible, local, &styleError))
		throw std::invalid_argument(WStringToString(styleError));
	DesignerStyleSheet expanded;
	if (!DesignerStyleSheetUtils::ExpandRuntimeRules(
		local, expanded, &styleError))
		throw std::invalid_argument(WStringToString(styleError));
	local = std::move(expanded);
	std::ostringstream code;
	code << indentStr << "// 控件级词法资源作用域\n";
	code << indentStr << "auto " << dictionaryName
		<< " = std::make_shared<ControlStyleSheet>();\n";
	for (const auto& resource : local.Resources)
		code << indentStr << dictionaryName << "->SetResource(L\""
			<< EscapeWStringLiteral(resource.Key) << "\", "
			<< GenerateStyleValueExpression(resource.Value) << ");\n";
	for (size_t index = 0; index < local.Rules.size(); ++index)
	{
		const auto& rule = local.Rules[index];
		const auto selectorName = dictionaryName + "_selector_"
			+ std::to_string(index + 1);
		code << indentStr << "ControlStyleSelector " << selectorName << ";\n";
		if (rule.HasType)
			code << indentStr << selectorName << ".Type = UIClass::UI_"
				<< WStringToString(DesignerStyleSheetUtils::UIClassName(rule.Type))
				<< ";\n";
		if (!rule.ComponentType.Empty())
		{
			code << indentStr << selectorName
				<< ".DeclarativeTypeNamespace = L\""
				<< EscapeWStringLiteral(rule.ComponentType.XamlNamespace)
				<< "\";\n";
			code << indentStr << selectorName
				<< ".DeclarativeTypeName = L\""
				<< EscapeWStringLiteral(rule.ComponentType.XamlName) << "\";\n";
		}
		else if (rule.XamlType.Valid())
		{
			code << indentStr << selectorName
				<< ".DeclarativeTypeNamespace = L\""
				<< EscapeWStringLiteral(rule.XamlType.NamespaceUri) << "\";\n";
			code << indentStr << selectorName
				<< ".DeclarativeTypeName = L\""
				<< EscapeWStringLiteral(rule.XamlType.LocalName) << "\";\n";
		}
		if (!rule.Id.empty())
			code << indentStr << selectorName << ".StyleResourceKey = L\""
				<< EscapeWStringLiteral(rule.Id) << "\";\n";
		for (const auto& condition : rule.PropertyConditions)
			code << indentStr << selectorName
				<< ".PropertyConditions.push_back({ L\""
				<< EscapeWStringLiteral(condition.Property) << "\", "
				<< GenerateStyleValueExpression(condition.Value) << " });\n";
		for (const auto& condition : rule.DataConditions)
			code << indentStr << selectorName
				<< ".DataConditions.push_back({ L\""
				<< EscapeWStringLiteral(condition.SourceProperty) << "\", "
				<< GenerateStyleValueExpression(condition.Value) << " });\n";
		std::vector<DeclarativeEventTriggerActionDefinition> enterActions;
		std::vector<DeclarativeEventTriggerActionDefinition> exitActions;
		std::wstring actionError;
		if (!DesignerStyleSheetUtils::MaterializeStoryboardActions(
			rule.EnterActions, local, enterActions, &actionError,
			document.ResourceBasePath, document.Resources,
			L"Local Style Trigger.EnterActions")
			|| !DesignerStyleSheetUtils::MaterializeStoryboardActions(
				rule.ExitActions, local, exitActions, &actionError,
				document.ResourceBasePath, document.Resources,
				L"Local Style Trigger.ExitActions"))
			throw std::invalid_argument(WStringToString(actionError));
		const auto enterActionsName = dictionaryName + "_enterActions_"
			+ std::to_string(index + 1);
		const auto exitActionsName = dictionaryName + "_exitActions_"
			+ std::to_string(index + 1);
		if (!enterActions.empty() || !exitActions.empty())
		{
			code << indentStr
				<< "std::vector<DeclarativeEventTriggerActionDefinition> "
				<< enterActionsName << ";\n";
			code << GenerateDeclarativeStoryboardActionsCode(
				enterActions, enterActionsName, indent);
			code << indentStr
				<< "std::vector<DeclarativeEventTriggerActionDefinition> "
				<< exitActionsName << ";\n";
			code << GenerateDeclarativeStoryboardActionsCode(
				exitActions, exitActionsName, indent);
		}
		code << indentStr << dictionaryName << "->AddRule(std::move("
			<< selectorName << "), {\n";
		std::vector<const DesignerStyleSetter*> emittedSetters;
		for (const auto& setter : rule.Setters)
			emittedSetters.push_back(&setter);
		for (size_t setterIndex = 0;
			setterIndex < emittedSetters.size(); ++setterIndex)
		{
			const auto& setter = *emittedSetters[setterIndex];
			code << indentStr << "\t";
			const std::string* objectExpression = nullptr;
			if (objectResources
				&& setter.PropertyName == L"Template"
				&& setter.UsesResource
				&& !setter.UsesDynamicResource)
			{
				const auto objectResource = std::find_if(
					objectResources->begin(), objectResources->end(),
					[&](const auto& resource)
					{ return resource.first == setter.ResourceKey; });
				if (objectResource != objectResources->end())
					objectExpression = &objectResource->second;
			}
			if (objectExpression)
				code << "ControlStyleSetter(L\"Template\", "
					<< *objectExpression << ")";
			else if (setter.UsesResource)
				code << (setter.UsesDynamicResource
					? "ControlStyleSetter::DynamicResource(L\""
					: "ControlStyleSetter::Resource(L\"")
					<< EscapeWStringLiteral(setter.PropertyName) << "\", L\""
					<< EscapeWStringLiteral(setter.ResourceKey) << "\")";
			else
				code << "ControlStyleSetter(L\""
					<< EscapeWStringLiteral(setter.PropertyName) << "\", "
					<< GenerateStyleValueExpression(setter.Literal) << ")";
			if (setterIndex + 1 < emittedSetters.size()) code << ",";
			code << "\n";
		}
		code << indentStr << "}";
		if (!enterActions.empty() || !exitActions.empty())
			code << ", std::move(" << enterActionsName
				<< "), std::move(" << exitActionsName << ")";
		code << ");\n";
	}
	code << indentStr
		<< "cui::framework::StyleAccess::SetResources(*"
		<< controlName << ", " << dictionaryName << ");\n";
	return code.str();
}

std::string CodeGenerator::GenerateContainerProperties(
	const DesignerModel::DesignNode& node, int indent)
{
	std::ostringstream code;
	std::string indentStr(indent, '\t');
	std::string name = GetVarName(node);

	// 元数据已先生成；最后 Load，确保 AutoPlay/Loop/解码策略在加载前生效。
	if (node.Type == UIClass::UI_MediaPlayer
		&& !node.Structure.MediaFile.empty())
	{
		code << indentStr << name << "->Load(L\""
			<< EscapeWStringLiteral(node.Structure.MediaFile)
			<< "\");\n";
	}

	if (node.Type == UIClass::UI_Grid)
	{
		const auto& rows = node.Structure.GridRows;
		const auto& columns = node.Structure.GridColumns;
		if (rows || columns)
		{
			code << indentStr << name << "->ClearRows();\n";
			code << indentStr << name << "->ClearColumns();\n";
			if (rows)
				for (const auto& row : *rows)
					code << indentStr << name << "->AddRow("
						<< GridLengthToCtorString(row.Length) << ", "
						<< FloatLiteral(static_cast<float>(row.Minimum)) << ", "
						<< FloatLiteral(static_cast<float>(row.Maximum))
						<< ");\n";
			if (columns)
				for (const auto& column : *columns)
					code << indentStr << name << "->AddColumn("
						<< GridLengthToCtorString(column.Length) << ", "
						<< FloatLiteral(static_cast<float>(column.Minimum)) << ", "
						<< FloatLiteral(static_cast<float>(column.Maximum))
						<< ");\n";
		}
	}

	if (node.Type == UIClass::UI_ChartView
		&& node.Structure.ChartSeries)
	{
		auto colorExpression = [&](const DesignerModel::DesignColor& color)
		{
			return ColorToString(D2D1_COLOR_F{
				static_cast<float>(color.R), static_cast<float>(color.G),
				static_cast<float>(color.B), static_cast<float>(color.A) });
		};
		const auto& chartSeries = *node.Structure.ChartSeries;
		code << indentStr << name << "->Clear();\n";
		for (size_t seriesIndex = 0; seriesIndex < chartSeries.size(); ++seriesIndex)
		{
			const auto& series = chartSeries[seriesIndex];
			const auto seriesVar = "__chartSeries_" + name + "_"
				+ std::to_string(seriesIndex + 1);
			code << indentStr << "ChartSeries " << seriesVar << ";\n";
			code << indentStr << seriesVar << ".Name = L\""
				<< EscapeWStringLiteral(series.Name) << "\";\n";
			if (series.Color)
				code << indentStr << seriesVar << ".Color = "
					<< colorExpression(*series.Color) << ";\n";
			if (!series.Visible)
				code << indentStr << seriesVar << ".Visible = false;\n";
			for (size_t pointIndex = 0;
				pointIndex < series.Points.size(); ++pointIndex)
			{
				const auto& point = series.Points[pointIndex];
				code << indentStr << seriesVar << ".Points.emplace_back(L\""
					<< EscapeWStringLiteral(point.Label) << "\", "
					<< DoubleLiteral(point.Value);
				if (point.Color) code << ", " << colorExpression(*point.Color);
				code << ");\n";
				if (point.Tag != 0) code << indentStr << seriesVar
					<< ".Points.back().Tag = " << point.Tag << "ULL;\n";
			}
			code << indentStr << name << "->AddSeries(" << seriesVar << ");\n";
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
		const std::optional<DesignerEventDescriptor>& descriptor) -> bool
	{
		if (storedHandler.empty()) return true;
		if (!descriptor)
		{
			if (outError) *outError = L"无法生成未知事件 “" + eventName + L"”。";
			return false;
		}
		const auto resolved = DesignerEventCatalog::NormalizeHandlerName(
			storedHandler);
		std::wstring validationError;
		if (!DesignerEventCatalog::ValidateHandlerName(resolved, &validationError))
		{
			if (outError) *outError = L"事件 “" + eventName + L"”：" + validationError;
			return false;
		}
		const auto handler = Utf8HandlerName(storedHandler);
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

	for (const auto& [eventName, storedHandler]
		: _sourceDocument.Window.Events)
		if (!add(eventName, storedHandler,
			DesignerEventCatalog::FindWindowEvent(eventName))) return false;
	for (const auto& binding : _sourceDocument.Window.CommandBindings)
	{
		for (const auto& [eventName, handler] : binding.HandlerRoutes())
			if (handler && !add(eventName, *handler,
				DesignerEventCatalog::FindWindowEvent(eventName))) return false;
	}
	for (const auto& node : _sourceDocument.Nodes)
	{
		for (const auto& [eventName, storedHandler] : node.Events)
			if (!add(eventName, storedHandler,
				DesignerEventCatalog::FindControlEvent(
					node.Type, eventName,
					ComponentEvents(node)))) return false;
		for (const auto& binding : node.CommandBindings)
		{
			for (const auto& [eventName, handler] : binding.HandlerRoutes())
				if (handler && !add(eventName, *handler,
					DesignerEventCatalog::FindControlEvent(
					node.Type, eventName,
					ComponentEvents(node)))) return false;
		}
	}
	for (const auto& definition : _sourceDocument.ControlTemplates)
	{
		for (const auto& node : definition.Template)
		{
			for (const auto& [eventName, storedHandler] : node.Events)
				if (!add(eventName, storedHandler,
					DesignerEventCatalog::FindControlEvent(
						node.Type, eventName,
						ComponentEvents(node)))) return false;
			for (const auto& binding : node.CommandBindings)
			{
				for (const auto& [eventName, handler]
					: binding.HandlerRoutes())
					if (handler && !add(eventName, *handler,
						DesignerEventCatalog::FindControlEvent(
							node.Type, eventName,
							ComponentEvents(node)))) return false;
			}
		}
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
	auto appendBuiltInRoute = [&](bool isWindow,
		UIClass controlType,
		const std::wstring& eventName,
		const std::wstring& storedHandler,
		const DesignerEventDescriptor& descriptor)
	{
		const auto handler = Utf8HandlerName(storedHandler);
		if (handler.empty()) return;
		const auto baseDescriptor = isWindow
			? std::optional<DesignerEventDescriptor>{}
			: DesignerEventCatalog::FindControlEvent(
				UIClass::UI_Base, eventName);
		const bool wildcard = baseDescriptor
			&& baseDescriptor->EventField == descriptor.EventField
			&& baseDescriptor->EventOwnerTypeName
				== descriptor.EventOwnerTypeName
			&& baseDescriptor->SameSignature(descriptor);
		const auto effectiveType = wildcard ? UIClass::UI_Base : controlType;
		const auto key = std::string(isWindow ? "F|" : "B|") + handler
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
		route.IsWindow = isWindow;
		route.ControlType = effectiveType;
		runtimeRoutes.push_back(std::move(route));
	};

	for (const auto& [eventName, storedHandler]
		: _sourceDocument.Window.Events)
		if (const auto descriptor = DesignerEventCatalog::FindWindowEvent(eventName))
			appendBuiltInRoute(true, UIClass::UI_Base, eventName,
				storedHandler, *descriptor);
	for (const auto& binding : _sourceDocument.Window.CommandBindings)
	{
		for (const auto& [eventName, handler] : binding.HandlerRoutes())
			if (handler && !handler->empty())
				if (const auto descriptor =
					DesignerEventCatalog::FindWindowEvent(eventName))
					appendBuiltInRoute(true, UIClass::UI_Base,
						eventName, *handler, *descriptor);
	}
	for (const auto& node : _sourceDocument.Nodes)
	{
		for (const auto& [eventName, storedHandler] : node.Events)
		{
			const auto descriptor = DesignerEventCatalog::FindControlEvent(
				node.Type, eventName, ComponentEvents(node));
			if (!descriptor) continue;
			appendBuiltInRoute(false, node.Type, eventName,
				storedHandler, *descriptor);
		}
		for (const auto& binding : node.CommandBindings)
		{
			for (const auto& [eventName, handler] : binding.HandlerRoutes())
			{
				if (!handler || handler->empty()) continue;
				const auto descriptor = DesignerEventCatalog::FindControlEvent(
					node.Type, eventName, ComponentEvents(node));
				if (descriptor) appendBuiltInRoute(false, node.Type,
					eventName, *handler, *descriptor);
			}
		}
	}
	for (const auto& definition : _sourceDocument.ControlTemplates)
	{
		for (const auto& node : definition.Template)
		{
			for (const auto& [eventName, storedHandler] : node.Events)
			{
				const auto descriptor =
					DesignerEventCatalog::FindControlEvent(
						node.Type, eventName,
						ComponentEvents(node));
				if (descriptor)
					appendBuiltInRoute(
						false, node.Type, eventName,
						storedHandler, *descriptor);
			}
			for (const auto& binding : node.CommandBindings)
			{
				for (const auto& [eventName, handler]
					: binding.HandlerRoutes())
				{
					if (!handler || handler->empty()) continue;
					const auto descriptor =
						DesignerEventCatalog::FindControlEvent(
							node.Type, eventName,
							ComponentEvents(node));
					if (descriptor)
						appendBuiltInRoute(
							false, node.Type, eventName,
							*handler, *descriptor);
				}
			}
		}
	}
	
	// 收集需要的头文件
	std::set<std::string> includes;
	includes.insert("Window.h");
	includes.insert("Control.h");
	const bool hasCommands = !_sourceDocument.Window.CommandBindings.empty()
		|| !_sourceDocument.Window.InputBindings.empty()
		|| std::any_of(
			_sourceDocument.Nodes.begin(), _sourceDocument.Nodes.end(),
			[](const auto& node)
			{
				return !node.CommandBindings.empty()
					|| !node.InputBindings.empty();
			})
		|| std::any_of(
			_sourceDocument.ControlTemplates.begin(),
			_sourceDocument.ControlTemplates.end(),
			[](const auto& definition)
			{
				return std::any_of(
					definition.Template.begin(),
					definition.Template.end(),
					[](const auto& node)
					{
						return !node.CommandBindings.empty()
							|| !node.InputBindings.empty();
					});
			});
	if (hasCommands) includes.insert("RoutedCommand.h");
	const bool hasStyleDataTriggers = std::any_of(
		_styleSheet.Rules.begin(), _styleSheet.Rules.end(),
		[](const DesignerStyleRule& rule)
		{
			return !rule.DataConditions.empty()
				|| std::any_of(rule.Triggers.begin(), rule.Triggers.end(),
					[](const DesignerStyleTrigger& trigger)
					{
						return !trigger.DataConditions.empty();
					});
		});
	const bool hasDataBindings = !_sourceDocument.Window.Bindings.empty()
		|| hasStyleDataTriggers || std::any_of(
			_sourceDocument.Nodes.begin(), _sourceDocument.Nodes.end(),
			[](const auto& node) { return !node.Bindings.empty(); })
		|| std::any_of(
			_sourceDocument.ControlTemplates.begin(),
			_sourceDocument.ControlTemplates.end(),
			[](const auto& definition)
			{
				return std::any_of(
					definition.Template.begin(),
					definition.Template.end(),
					[](const auto& node)
					{ return !node.Bindings.empty(); });
			});
	const bool hasAuthoredProperties =
		!_sourceDocument.Window.Properties.Values.empty()
		|| std::any_of(
			_sourceDocument.Nodes.begin(), _sourceDocument.Nodes.end(),
			[](const auto& node)
			{ return !node.Properties.Values.empty(); })
		|| std::any_of(
			_sourceDocument.ControlTemplates.begin(),
			_sourceDocument.ControlTemplates.end(),
			[](const auto& definition)
			{
				return std::any_of(
					definition.Template.begin(),
					definition.Template.end(),
					[](const auto& node)
					{ return !node.Properties.Values.empty(); });
			});
	if (hasDataBindings || hasAuthoredProperties)
		includes.insert("Binding.h");
	
	for (const auto& node : _sourceDocument.Nodes)
		includes.insert(GetIncludeForType(node.Type));
	
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
			<< "() { UnregisterDeclarativeEventHandlers(); }\n";
		header << "\t" << eventSinkName << "(const " << eventSinkName
			<< "&) = delete;\n";
		header << "\t" << eventSinkName << "& operator=(const "
			<< eventSinkName << "&) = delete;\n";
		header << "\t" << eventSinkName << "(" << eventSinkName
			<< "&&) = delete;\n";
		header << "\t" << eventSinkName << "& operator=("
			<< eventSinkName << "&&) = delete;\n\n";
		header << "\ttemplate<typename TRegistry>\n";
		header << "\tbool RegisterDeclarativeEventHandlers(\n";
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
			if (route.IsWindow)
			{
				header << "RegisterWindow(\n";
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
					<< "\", static_cast<UIClass>("
					<< static_cast<int>(route.ControlType) << ")"
					<< ", L\"" << EscapeWStringLiteral(route.EventName)
					<< "\", &" << route.EventOwnerType << "::"
					<< route.EventField << ",\n";
			}
			header << "\t\t\t\t\tGuardDeclarativeEventHandler(\n";
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
		header << "\t\t\tstruct DeclarativeEventRegistration final\n";
		header << "\t\t\t{\n";
		header << "\t\t\t\tdecltype(registration) Lease;\n";
		header << "\t\t\t\tstd::shared_ptr<void> Lifetime;\n";
		header << "\t\t\t\tDeclarativeEventRegistration(\n";
		header << "\t\t\t\t\tdecltype(registration)&& lease,\n";
		header << "\t\t\t\t\tstd::shared_ptr<void> lifetime) noexcept\n";
		header << "\t\t\t\t\t: Lease(std::move(lease)),\n";
		header << "\t\t\t\t\tLifetime(std::move(lifetime)) {}\n";
		header << "\t\t\t};\n";
		header << "\t\t\tauto owned = std::make_shared<DeclarativeEventRegistration>(\n";
		header << "\t\t\t\tstd::move(registration), std::move(lifetime));\n";
		header << "\t\t\t_declarativeEventRegistration = std::move(owned);\n";
		header << "\t\t\tif (outError) outError->clear();\n";
		header << "\t\t\treturn true;\n";
		header << "\t\t}\n";
		header << "\t\tcatch (...)\n";
		header << "\t\t{\n";
		header << "\t\t\tif (outError) *outError =\n";
		header << "\t\t\t\tL\"无法保存声明事件注册租约。\";\n";
		header << "\t\t\treturn false;\n";
		header << "\t\t}\n";
		header << "\t}\n\n";
		header << "\tvoid UnregisterDeclarativeEventHandlers() noexcept\n";
		header << "\t{\n";
		header << "\t\t_declarativeEventRegistration.reset();\n";
		header << "\t}\n\n";
		header << "private:\n";
		header << "\ttemplate<typename TCallback>\n";
		header << "\tstatic auto GuardDeclarativeEventHandler(\n";
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
		header << "\tstd::shared_ptr<void> _declarativeEventRegistration;\n\n";
		header << "protected:\n";
		for (const auto& handler : eventHandlers)
			header << "\tvirtual void " << handler.first << "("
				<< handler.second << ") = 0;\n";
		header << "};\n\n";
	}
	
	header << "class " << className << " : public Window";
	if (!eventHandlers.empty())
		header << ", public " << identity.UserLeaf << "EventSink";
	header << "\n";
	header << "{\n";
	header << "protected:\n";
	
	// 声明控件成员
	for (const auto& node : _sourceDocument.Nodes)
	{
		if (node.TemplateState.Generated) continue;
		std::string name = GetVarName(node);
		std::string typeName = GetControlTypeName(node.Type);
		header << "\t" << typeName << "* " << name << " = nullptr;\n";
	}
	header << "\tstd::vector<EventConnection> _generatedEventConnections;\n";
	header << "\tbool _componentInitialized = false;\n";
	header << "\tvoid InitializeComponent();\n";

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
		_sourceDocument.Nodes.begin(), _sourceDocument.Nodes.end(),
		[](const auto& node)
		{ return node.Id > 0 && !node.TemplateState.Generated; });
	if (hasStableControlIds)
	{
		header << "\t// Stable identities shared by static and dynamic document paths.\n";
		header << "\tstruct ControlIds final\n\t{\n";
		for (const auto& node : _sourceDocument.Nodes)
		{
			if (node.Id <= 0 || node.TemplateState.Generated) continue;
			header << "\t\tstatic constexpr int " << GetVarName(node)
				<< " = " << node.Id << ";\n";
		}
		header << "\t};\n\n";
	}
	const bool hasAuthoredNamedControls = std::any_of(
		_sourceDocument.Nodes.begin(), _sourceDocument.Nodes.end(),
		[](const auto& node) { return !node.TemplateState.Generated; });
	if (hasAuthoredNamedControls)
	{
		header << "\t// Type-safe x:Name accessors; ownership remains with the generated Window.\n";
		for (const auto& node : _sourceDocument.Nodes)
		{
			if (node.TemplateState.Generated) continue;
			auto accessorName = GetVarName(node);
			if (!accessorName.empty() && accessorName.front() >= 'a'
				&& accessorName.front() <= 'z')
				accessorName.front() = static_cast<char>(
					accessorName.front() - 'a' + 'A');
			const auto typeName = GetControlTypeName(node.Type);
			header << "\t[[nodiscard]] " << typeName << "* Get"
				<< accessorName << "() noexcept { return "
				<< GetVarName(node) << "; }\n";
			header << "\t[[nodiscard]] const " << typeName << "* Get"
				<< accessorName << "() const noexcept { return "
				<< GetVarName(node) << "; }\n";
		}
		header << "\n";
	}
	header << "\t" << className << "();\n";
	header << "\tvirtual ~" << className << "();\n";
	if (hasDataBindings)
		header << "\tbool BindData(BindingSourceReference dataContext);\n";
	header << "};\n";

	// A zero-owning typed view over the dynamic RuntimeDocument contract. Keep
	// this template independent of CuiRuntime headers; Generic.xaml support is
	// consumed by the generated implementation, not leaked through the API.
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
	for (const auto& node : _sourceDocument.Nodes)
	{
		if (node.Id <= 0 || node.TemplateState.Generated) continue;
		auto accessorName = GetVarName(node);
		if (!accessorName.empty() && accessorName.front() >= 'a'
			&& accessorName.front() <= 'z')
			accessorName.front() = static_cast<char>(
				accessorName.front() - 'a' + 'A');
		const auto typeName = GetControlTypeName(node.Type);
		header << "\t[[nodiscard]] " << typeName << "* Get"
			<< accessorName << "() const noexcept\n\t{\n";
		header << "\t\treturn _document.template FindControlByDesignId<"
			<< typeName << ">(\n";
		header << "\t\t\t" << className << "::ControlIds::"
			<< GetVarName(node) << ");\n\t}\n";
		header << "\t[[nodiscard]] auto Reference" << accessorName
			<< "() const noexcept\n\t{\n";
		header << "\t\treturn _document.template ReferenceByDesignId<"
			<< typeName << ">(\n";
		header << "\t\t\t" << className << "::ControlIds::"
			<< GetVarName(node) << ");\n\t}\n";
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
	struct StaticControlTemplateBlueprint final
	{
		size_t SourceIndex = 0;
		DesignerModel::DesignDocument Document;
		std::wstring OwnerName;
		std::string VariableName;
		std::vector<DeclarativeVisualStateGroupDefinition> VisualStateGroups;
		std::vector<DeclarativeEventTriggerDefinition> EventTriggers;
	};

	std::vector<StaticControlTemplateBlueprint> templateBlueprints;
	templateBlueprints.reserve(_sourceDocument.ControlTemplates.size());
	for (size_t templateIndex = 0;
		templateIndex < _sourceDocument.ControlTemplates.size();
		++templateIndex)
	{
		const auto& definition =
			_sourceDocument.ControlTemplates[templateIndex];
		if (!definition.TargetComponentType.Empty())
			throw std::invalid_argument(
				"Static ControlTemplate builders require a built-in TargetType");
		const auto* targetDescriptor =
			CuiRuntime::XamlRuntimeSchema::DefaultTypeFor(
				definition.TargetType);
		if (!targetDescriptor || !targetDescriptor->IsConstructible)
			throw std::invalid_argument(
				"Static ControlTemplate builder TargetType is not constructible");

		DesignerModel::DesignDocument synthetic;
		synthetic.Window = _sourceDocument.Window;
		synthetic.Window.Name =
			L"__cuiStaticTemplateWindow"
			+ std::to_wstring(templateIndex + 1);
		synthetic.Window.Properties = {};
		synthetic.Window.Structure = {};
		synthetic.Window.TemplateState = {};
		synthetic.Window.Events.clear();
		synthetic.Window.Bindings.clear();
		synthetic.Window.CommandBindings.clear();
		synthetic.Window.InputBindings.clear();
		synthetic.Window.LocalResources = {};
		synthetic.Window.LocalObjectResources = {};
		synthetic.Window.TemplateBindings.clear();
		synthetic.Window.TemplateEventBindings.clear();
		synthetic.CodeBehind = {};
		synthetic.DataContextSchema = {};
		synthetic.StyleSheet = _sourceDocument.StyleSheet;
		synthetic.Components.clear();
		synthetic.ControlTemplates =
			_sourceDocument.ControlTemplates;
		synthetic.DataTypes.clear();
		synthetic.DataTemplates.clear();
		synthetic.ItemsPanelTemplates.clear();
		synthetic.GroupStyles.clear();
		synthetic.DataLists.clear();
		synthetic.CollectionViews.clear();
		synthetic.Nodes.clear();
		synthetic.ResourceBasePath = _sourceDocument.ResourceBasePath;
		synthetic.Resources = _sourceDocument.Resources;
		synthetic.NextStableId = 2;

		std::wstring selectedKey = definition.Key;
		if (selectedKey.empty())
		{
			selectedKey = L"__cuiStaticImplicitControlTemplate"
				+ std::to_wstring(templateIndex + 1);
			synthetic.ControlTemplates[templateIndex].Key = selectedKey;
		}

		DesignerModel::DesignNode owner;
		owner.Id = 1;
		owner.Name = L"__cuiStaticTemplateOwner"
			+ std::to_wstring(templateIndex + 1);
		owner.Type = definition.TargetType;
		owner.XamlType = targetDescriptor->TypeId;
		owner.Order = 0;
		owner.Structure.ControlTemplate = selectedKey;
		synthetic.Nodes.push_back(owner);

		CuiRuntime::XamlCompiledDocument compiledTemplate;
		std::wstring compileError;
		if (!CuiRuntime::XamlDocumentCompiler::Compile(
			synthetic, compiledTemplate, {}, &compileError))
			throw std::invalid_argument(
				"Static ControlTemplate blueprint compilation failed: "
				+ WStringToString(compileError));
		const auto compiledOwner = std::find_if(
			compiledTemplate.Document.Nodes.begin(),
			compiledTemplate.Document.Nodes.end(),
			[&](const auto& node)
			{
				return !node.TemplateState.Generated
					&& node.Name == owner.Name;
			});
		if (compiledOwner == compiledTemplate.Document.Nodes.end()
			|| compiledOwner->TemplateState.AppliedControlTemplate.empty()
			|| !std::any_of(
				compiledTemplate.Document.Nodes.begin(),
				compiledTemplate.Document.Nodes.end(),
				[&](const auto& node)
				{
					return node.TemplateState.Generated
						&& node.TemplateState.Owner == owner.Name
						&& node.TemplateState.ControlTemplateRoot;
				}))
			throw std::invalid_argument(
				"Static ControlTemplate blueprint has no generated root");

		StaticControlTemplateBlueprint blueprint;
		blueprint.SourceIndex = templateIndex;
		blueprint.Document = std::move(compiledTemplate.Document);
		blueprint.OwnerName = std::move(owner.Name);
		const auto identityName = definition.Key.empty()
			? L"Implicit_" + DesignerStyleSheetUtils::UIClassName(
				definition.TargetType)
			: definition.Key;
		blueprint.VariableName =
			"__controlTemplate_"
			+ SanitizeCppIdentifier(WStringToString(identityName))
			+ "_" + std::to_string(templateIndex + 1);
		std::wstring interactionError;
		if (!CuiRuntime::XamlObjectMaterializer::
			MaterializeDeclarativeInteractions(
				definition.VisualStateGroups, definition.EventTriggers,
				_sourceDocument, blueprint.VisualStateGroups,
				blueprint.EventTriggers, &interactionError))
			throw std::invalid_argument(
				"Static ControlTemplate interaction lowering failed: "
				+ WStringToString(interactionError));
		templateBlueprints.push_back(std::move(blueprint));
	}
	std::vector<std::pair<std::wstring, std::string>>
		staticObjectResources;
	for (const auto& blueprint : templateBlueprints)
	{
		const auto& definition =
			_sourceDocument.ControlTemplates[blueprint.SourceIndex];
		if (definition.Key.empty()) continue;
		staticObjectResources.emplace_back(
			definition.Key,
			"BindingValue(ControlTemplateReference("
				+ blueprint.VariableName + "))");
	}
	std::vector<std::pair<std::wstring, std::string>>
		weakStaticObjectResources;
	for (const auto& blueprint : templateBlueprints)
	{
		const auto& definition =
			_sourceDocument.ControlTemplates[blueprint.SourceIndex];
		if (definition.Key.empty()) continue;
		weakStaticObjectResources.emplace_back(
			definition.Key,
			"BindingValue(ControlTemplateReference(__weak_"
				+ blueprint.VariableName.substr(2) + ".lock()))");
	}

	std::ostringstream cpp;
	const auto identity = ParseQualifiedCppClassName(
		WStringToString(_className));
	const auto& className = identity.QualifiedGenerated;
	const auto& classLeaf = identity.GeneratedLeaf;
	
	// 包含头文件
	cpp << "#include \"" << generatedHeaderBaseName << ".g.h\"\n";
	std::set<std::string> templateImplementationIncludes;
	for (const auto& blueprint : templateBlueprints)
		for (const auto& node : blueprint.Document.Nodes)
			if (node.TemplateState.Generated)
				templateImplementationIncludes.insert(
					GetIncludeForType(node.Type));
	for (const auto& include : templateImplementationIncludes)
		cpp << "#include \"" << include << "\"\n";
	cpp << "#include \"ControlTemplate.h\"\n";
	cpp << "#include \"XamlInfrastructure.h\"\n";
	cpp << "#include \"DependencyPropertyInfrastructure.h\"\n";
	cpp << "#include \"StyleInfrastructure.h\"\n";
	cpp << "#include \"TemplateInfrastructure.h\"\n";
	cpp << "#include \"XamlFrameworkTheme.h\"\n";
	cpp << "#include \"HeaderedContentControl.h\"\n";
	cpp << "#include \"HeaderedItemsControl.h\"\n";
	const bool hasLocalStyleSheets = std::any_of(
		_sourceDocument.Nodes.begin(), _sourceDocument.Nodes.end(),
		[](const auto& node) { return !node.LocalResources.Empty(); });
	if (!_styleSheet.Empty() || hasLocalStyleSheets
		|| !templateBlueprints.empty())
		cpp << "#include \"Style.h\"\n";
	auto usesImageValue = [](const DesignerStyleValue& value)
	{
		return value.Kind == DesignerStyleValueKind::ImageSource
			|| (value.Kind == DesignerStyleValueKind::Brush
				&& value.ObjectValue.is_object()
				&& value.ObjectValue.value("type", std::string{}) == "image");
	};
	auto animationUsesImage = [&](const DesignerVisualStateAnimation& animation)
	{
		if ((animation.HasFrom && usesImageValue(animation.From))
			|| (animation.HasTo && usesImageValue(animation.To))
			|| (animation.HasBy && usesImageValue(animation.By)))
			return true;
		return std::any_of(
			animation.KeyFrames.begin(), animation.KeyFrames.end(),
			[&](const auto& frame) { return usesImageValue(frame.Value); });
	};
	auto actionsUseImage = [&](const auto& actions)
	{
		for (const auto& action : actions)
			if (std::any_of(
				action.Animations.begin(), action.Animations.end(),
				animationUsesImage)) return true;
		return false;
	};
	auto styleSheetUsesImage = [&](const DesignerStyleSheet& sheet)
	{
		if (std::any_of(sheet.Resources.begin(), sheet.Resources.end(),
			[&](const auto& resource)
			{ return usesImageValue(resource.Value); })) return true;
		for (const auto& rule : sheet.Rules)
		{
			if (std::any_of(rule.Setters.begin(), rule.Setters.end(),
				[&](const auto& setter)
				{
						return !setter.UsesResource
							&& usesImageValue(setter.Literal);
					})) return true;
			if (actionsUseImage(rule.EnterActions)
				|| actionsUseImage(rule.ExitActions)) return true;
			for (const auto& trigger : rule.Triggers)
			{
				if (std::any_of(
					trigger.Setters.begin(), trigger.Setters.end(),
					[&](const auto& setter)
					{
						return !setter.UsesResource
							&& usesImageValue(setter.Literal);
					})) return true;
				if (actionsUseImage(trigger.EnterActions)
					|| actionsUseImage(trigger.ExitActions)) return true;
			}
		}
		return false;
	};
	auto propertiesUseImage = [&](const auto& properties)
	{
		return std::any_of(
			properties.Values.begin(), properties.Values.end(),
			[&](const auto& property)
			{ return usesImageValue(property.second.Value); });
	};
	bool usesResources = styleSheetUsesImage(_styleSheet)
		|| propertiesUseImage(_sourceDocument.Window.Properties)
		|| std::any_of(
			templateBlueprints.begin(), templateBlueprints.end(),
			[](const auto& blueprint)
			{
				return !blueprint.VisualStateGroups.empty()
					|| !blueprint.EventTriggers.empty();
			})
		|| std::any_of(
			_sourceDocument.Nodes.begin(), _sourceDocument.Nodes.end(),
			[&](const auto& node)
			{
				return propertiesUseImage(node.Properties)
					|| styleSheetUsesImage(node.LocalResources);
			})
		|| std::any_of(
			templateBlueprints.begin(), templateBlueprints.end(),
			[&](const auto& blueprint)
			{
				return std::any_of(
					blueprint.Document.Nodes.begin(),
					blueprint.Document.Nodes.end(),
					[&](const auto& node)
					{
						return node.TemplateState.Generated
							&& (propertiesUseImage(node.Properties)
								|| styleSheetUsesImage(
									node.LocalResources));
					});
			});
	if (usesResources) cpp << "#include \"Resource.h\"\n";
	cpp << "#include <functional>\n";
	cpp << "#include <memory>\n";
	cpp << "#include <stdexcept>\n";
	cpp << "#include <utility>\n";
	cpp << "#include <vector>\n\n";

	if (!templateBlueprints.empty())
	{
		cpp << "namespace\n{\n";
		cpp << "\tclass CuiGeneratedControlTemplate final\n";
		cpp << "\t\t: public IControlTemplate,\n";
		cpp << "\t\t  public std::enable_shared_from_this<"
			"CuiGeneratedControlTemplate>\n";
		cpp << "\t{\n";
		cpp << "\tpublic:\n";
		cpp << "\t\tusing ApplyCallback = std::function<bool(\n";
		cpp << "\t\t\tControl&, std::wstring*)>;\n";
		cpp << "\t\tusing HostFactory = std::function<"
			"std::unique_ptr<Control>()>;\n\n";
		cpp << "\t\tCuiGeneratedControlTemplate(\n";
		cpp << "\t\t\tUIClass targetType,\n";
		cpp << "\t\t\tstd::wstring identity,\n";
		cpp << "\t\t\tHostFactory hostFactory)\n";
		cpp << "\t\t\t: _targetType(targetType),\n";
		cpp << "\t\t\t  _identity(std::move(identity)),\n";
		cpp << "\t\t\t  _hostFactory(std::move(hostFactory)) {}\n\n";
		cpp << "\t\tvoid SetApplyCallback(ApplyCallback value)\n";
		cpp << "\t\t{\n\t\t\t_apply = std::move(value);\n\t\t}\n\n";
		cpp << "\t\tUIClass TargetType() const noexcept override\n";
		cpp << "\t\t{\n\t\t\treturn _targetType;\n\t\t}\n\n";
		cpp << "\t\tbool Apply(Control& owner,\n";
		cpp << "\t\t\tstd::wstring* outError = nullptr) const override\n";
		cpp << "\t\t{\n";
		cpp << "\t\t\tif (!IsUIClassAssignableFrom("
			"_targetType, owner.Type()))\n";
		cpp << "\t\t\t{\n";
		cpp << "\t\t\t\tif (outError) *outError =\n";
		cpp << "\t\t\t\t\tL\"生成的 ControlTemplate TargetType "
			"与宿主不兼容：\" + _identity;\n";
		cpp << "\t\t\t\treturn false;\n";
		cpp << "\t\t\t}\n";
		cpp << "\t\t\tif (!_apply)\n";
		cpp << "\t\t\t{\n";
		cpp << "\t\t\t\tif (outError) *outError =\n";
		cpp << "\t\t\t\t\tL\"生成的 ControlTemplate 尚未完成初始化：\""
			" + _identity;\n";
		cpp << "\t\t\t\treturn false;\n";
		cpp << "\t\t\t}\n";
		cpp << "\t\t\treturn _apply(owner, outError);\n";
		cpp << "\t\t}\n\n";
		cpp << "\t\tstd::unique_ptr<Control> Build(\n";
		cpp << "\t\t\tstd::wstring* outError = nullptr) const override\n";
		cpp << "\t\t{\n";
		cpp << "\t\t\tauto owner = _hostFactory ? _hostFactory() : nullptr;\n";
		cpp << "\t\t\tif (!owner)\n";
		cpp << "\t\t\t{\n";
		cpp << "\t\t\t\tif (outError) *outError =\n";
		cpp << "\t\t\t\t\tL\"生成的 ControlTemplate 无法构造宿主：\""
			" + _identity;\n";
		cpp << "\t\t\t\treturn {};\n";
		cpp << "\t\t\t}\n";
		cpp << "\t\t\tauto self = std::static_pointer_cast<"
			"const IControlTemplate>(shared_from_this());\n";
		cpp << "\t\t\tif (!cui::framework::XamlAccess::SetTemplate(\n";
		cpp << "\t\t\t\t*owner, ControlTemplateReference(std::move(self)),\n";
		cpp << "\t\t\t\tDependencyPropertyValueSource::Local))\n";
		cpp << "\t\t\t{\n";
		cpp << "\t\t\t\tif (outError) *outError =\n";
		cpp << "\t\t\t\t\tL\"生成的 ControlTemplate 无法写入宿主：\""
			" + _identity;\n";
		cpp << "\t\t\t\treturn {};\n";
		cpp << "\t\t\t}\n";
		cpp << "\t\t\t(void)owner->ApplyTemplate();\n";
		cpp << "\t\t\tif (!cui::framework::TemplateAccess::"
			"GetTemplateRoot(*owner)\n";
		cpp << "\t\t\t\t|| !owner->LastTemplateError().empty())\n";
		cpp << "\t\t\t{\n";
		cpp << "\t\t\t\tif (outError) *outError = "
			"owner->LastTemplateError().empty()\n";
		cpp << "\t\t\t\t\t? L\"生成的 ControlTemplate 未生成视觉根：\""
			" + _identity\n";
		cpp << "\t\t\t\t\t: owner->LastTemplateError();\n";
		cpp << "\t\t\t\treturn {};\n";
		cpp << "\t\t\t}\n";
		cpp << "\t\t\tif (outError) outError->clear();\n";
		cpp << "\t\t\treturn owner;\n";
		cpp << "\t\t}\n\n";
		cpp << "\tprivate:\n";
		cpp << "\t\tUIClass _targetType = UIClass::UI_Base;\n";
		cpp << "\t\tstd::wstring _identity;\n";
		cpp << "\t\tHostFactory _hostFactory;\n";
		cpp << "\t\tApplyCallback _apply;\n";
		cpp << "\t};\n";
		cpp << "}\n\n";
	}
	
	// Do not lower XAML from the generated base constructor. InitializeComponent
	// is called from the user class constructor body, after base construction,
	// so virtual C++ event/command hooks dispatch to the authored overrides just
	// as WPF initializes a completed code-behind instance.
	cpp << className << "::" << classLeaf << "()\n";
	cpp << "\t: Window()\n";
	cpp << "{\n";
	cpp << "}\n\n";
	cpp << "void " << className << "::InitializeComponent()\n";
	cpp << "{\n\n";
	cpp << "\tif (_componentInitialized) return;\n";
	cpp << "\t_componentInitialized = true;\n\n";
	cpp << "\t// Native constructors are behavior-host implementation details.\n";
	cpp << "\t// Begin from the same empty Local-value surface as dynamic XAML.\n";
	cpp << "\t(void)this->ClearPropertyValues();\n\n";
	cpp << "\tstatic const auto __xamlType_this = "
		"DeclarativeTypeDescriptor::Create(\n";
	cpp << "\t\tRuntimeTypeId{ L\""
		<< EscapeWStringLiteral(_sourceDocument.Window.XamlType.NamespaceUri)
		<< "\", L\""
		<< EscapeWStringLiteral(_sourceDocument.Window.XamlType.LocalName)
		<< "\" }, {});\n";
	cpp << "\tif (!__xamlType_this || "
		"!cui::framework::XamlAccess::SetTypeDescriptor("
		"*this, __xamlType_this))\n";
	cpp << "\t\tthrow std::runtime_error("
		"\"Generated XAML type attachment failed\");\n\n";
	cpp << "\t// 创建控件\n";

	// 1) 先实例化所有可设计控件（不做 AdoptVisualChild）。x:Reference
	// wiring is emitted only after every member pointer has been assigned.
	for (const auto& node : _sourceDocument.Nodes)
		if (!node.TemplateState.Generated)
			cpp << GenerateControlInstantiation(node, 1);
	if (std::any_of(
		_sourceDocument.Nodes.begin(), _sourceDocument.Nodes.end(),
		[](const auto& node) { return !node.TemplateState.Generated; }))
		cpp << "\n";
	cpp << "\tstd::wstring __frameworkThemeError;\n";

	auto findSourceNodeByName = [&](const std::wstring& name)
		-> const DesignerModel::DesignNode*
	{
		const auto found = std::find_if(
			_sourceDocument.Nodes.begin(), _sourceDocument.Nodes.end(),
			[&](const auto& candidate) { return candidate.Name == name; });
		return found == _sourceDocument.Nodes.end() ? nullptr : &*found;
	};

	if (!templateBlueprints.empty())
	{
		cpp << "\n\t// Repeatable pure-C++ factories for authored "
			"ControlTemplate resources.\n";
		for (const auto& blueprint : templateBlueprints)
		{
			const auto& definition =
				_sourceDocument.ControlTemplates[blueprint.SourceIndex];
			const auto* descriptor =
				CuiRuntime::XamlRuntimeSchema::DefaultTypeFor(
					definition.TargetType);
			if (!descriptor)
				throw std::invalid_argument(
					"Static ControlTemplate TargetType descriptor is missing");
			const auto typeName = GetControlTypeName(definition.TargetType);
			const auto identityText = definition.Key.empty()
				? L"{x:Type "
					+ DesignerStyleSheetUtils::UIClassName(
						definition.TargetType) + L"}"
				: definition.Key;
			cpp << "\tauto " << blueprint.VariableName
				<< " = std::make_shared<CuiGeneratedControlTemplate>(\n";
			cpp << "\t\tUIClass::UI_"
				<< WStringToString(
					DesignerStyleSheetUtils::UIClassName(
						definition.TargetType))
				<< ", L\"" << EscapeWStringLiteral(identityText)
				<< "\", []() -> std::unique_ptr<Control>\n";
			cpp << "\t\t{\n";
			cpp << "\t\t\tauto result = std::make_unique<"
				<< typeName << ">();\n";
			cpp << "\t\t\t(void)result->ClearPropertyValues();\n";
			cpp << "\t\t\tstatic const auto descriptor = "
				"DeclarativeTypeDescriptor::Create(\n";
			cpp << "\t\t\t\tRuntimeTypeId{ L\""
				<< EscapeWStringLiteral(descriptor->TypeId.NamespaceUri)
				<< "\", L\""
				<< EscapeWStringLiteral(descriptor->TypeId.LocalName)
				<< "\" }, {});\n";
			cpp << "\t\t\tif (!descriptor || "
				"!cui::framework::XamlAccess::SetTypeDescriptor("
				"*result, descriptor))\n";
			cpp << "\t\t\t\treturn {};\n";
			cpp << "\t\t\t(void)cui::framework::DependencyPropertyAccess::"
				"SetValue(*result, L\"Focusable\", BindingValue("
				<< (descriptor->FocusableByDefault ? "true" : "false")
				<< "), DependencyPropertyValueSource::Theme);\n";
			cpp << "\t\t\treturn result;\n";
			cpp << "\t\t});\n";
			cpp << "\tstd::weak_ptr<const IControlTemplate> __weak_"
				<< blueprint.VariableName.substr(2) << " = "
				<< blueprint.VariableName << ";\n";
		}
		cpp << "\n";

		for (const auto& blueprint : templateBlueprints)
		{
			const auto& blueprintDocument = blueprint.Document;
			const auto blueprintOwner = std::find_if(
				blueprintDocument.Nodes.begin(),
				blueprintDocument.Nodes.end(),
				[&](const auto& node)
				{
					return !node.TemplateState.Generated
						&& node.Name == blueprint.OwnerName;
				});
			if (blueprintOwner == blueprintDocument.Nodes.end())
				throw std::invalid_argument(
					"Static ControlTemplate blueprint owner is missing");
			std::vector<const DesignerModel::DesignNode*> generatedNodes;
			for (const auto& node : blueprintDocument.Nodes)
				if (node.TemplateState.Generated)
					generatedNodes.push_back(&node);

			auto findBlueprintNodeByName =
				[&](const std::wstring& name)
				-> const DesignerModel::DesignNode*
			{
				const auto found = std::find_if(
					blueprintDocument.Nodes.begin(),
					blueprintDocument.Nodes.end(),
					[&](const auto& node)
					{
						return node.Name == name
							|| (node.TemplateState.Generated
								&& node.TemplateState.PartName == name);
					});
				return found == blueprintDocument.Nodes.end()
					? nullptr : &*found;
			};
			auto templateOwnerPointerExpression =
				[&](const std::wstring& name) -> std::string
			{
				if (name == blueprint.OwnerName)
					return "&__templateOwner";
				const auto* node = findBlueprintNodeByName(name);
				if (!node || !node->TemplateState.Generated)
					throw std::invalid_argument(
						"Static ControlTemplate owner cannot be resolved");
				return GetVarName(*node);
			};
			auto templateOwnerReferenceExpression =
				[&](const std::wstring& name) -> std::string
			{
				if (name == blueprint.OwnerName)
					return "__templateOwner";
				const auto* node = findBlueprintNodeByName(name);
				if (!node || !node->TemplateState.Generated)
					throw std::invalid_argument(
						"Static ControlTemplate owner cannot be resolved");
				return "*" + GetVarName(*node);
			};
			auto commandTargetExpression =
				[&](const std::wstring& name) -> std::string
			{
				if (name.empty()) return "nullptr";
				if (name == blueprint.OwnerName)
					return "&__templateOwner";
				if (name == _sourceDocument.Window.Name)
					return "this";
				if (const auto* node = findBlueprintNodeByName(name);
					node && node->TemplateState.Generated)
					return GetVarName(*node);
				if (const auto* node = findSourceNodeByName(name);
					node && !node->TemplateState.Generated)
					return GetVarName(*node);
				throw std::invalid_argument(
					"Static ControlTemplate CommandTarget cannot be resolved");
			};
			auto bindingElementExpression =
				[&](const std::wstring& name) -> std::string
			{
				if (name == blueprint.OwnerName)
					return "__templateOwner";
				if (name == _sourceDocument.Window.Name)
					return "*this";
				if (const auto* node = findBlueprintNodeByName(name);
					node && node->TemplateState.Generated)
					return "*" + GetVarName(*node);
				if (const auto* node = findSourceNodeByName(name);
					node && !node->TemplateState.Generated)
					return "*" + GetVarName(*node);
				throw std::invalid_argument(
					"Static ControlTemplate ElementName cannot be resolved");
			};
			auto sourceTemplateIndex =
				[&](const DesignerModel::DesignNode& owner)
				-> std::optional<size_t>
			{
				if (owner.TemplateState.AppliedControlTemplateFromTheme
					|| owner.TemplateState.AppliedControlTemplate.empty())
					return std::nullopt;
				const auto& key =
					owner.TemplateState.AppliedControlTemplateResource;
				const auto* definition = !key.empty()
					? blueprintDocument.FindControlTemplate(
						blueprintDocument.Nodes, owner, key)
					: owner.ComponentType.Empty()
						? blueprintDocument.FindImplicitControlTemplate(
							blueprintDocument.Nodes, owner, owner.Type)
						: blueprintDocument.FindImplicitControlTemplate(
							blueprintDocument.Nodes, owner,
							owner.ComponentType);
				if (!definition) return std::nullopt;
				const auto* begin =
					blueprintDocument.ControlTemplates.data();
				const auto index =
					static_cast<size_t>(definition - begin);
				return index < templateBlueprints.size()
					? std::optional<size_t>{ index } : std::nullopt;
			};

			cpp << "\t" << blueprint.VariableName
				<< "->SetApplyCallback([this";
			for (const auto& captured : templateBlueprints)
				cpp << ", __weak_"
					<< captured.VariableName.substr(2);
			cpp << "](Control& __templateOwner, "
				"std::wstring* outError) -> bool\n";
			cpp << "\t{\n";
			cpp << "\t\tauto fail = [&](std::wstring message)\n";
			cpp << "\t\t{\n";
			cpp << "\t\t\tif (outError) *outError = std::move(message);\n";
			cpp << "\t\t\treturn false;\n";
			cpp << "\t\t};\n";
			cpp << "\t\ttry\n\t\t{\n";
			cpp << "\t\t\tstd::wstring __templateThemeError;\n";

			for (const auto* node : generatedNodes)
				cpp << GenerateControlInstantiation(*node, 3);
			if (!generatedNodes.empty()) cpp << "\n";

			for (const auto* node : generatedNodes)
			{
				if (node->TemplateState.
					AppliedControlTemplateFromTheme)
				{
					const auto& resourceKey = node->TemplateState.
						AppliedControlTemplateResource;
					if (resourceKey.empty())
						throw std::invalid_argument(
							"Nested Theme ControlTemplate key is missing");
					cpp << "\t\t\tif (!CuiRuntime::XamlFrameworkTheme::"
						"InstallTemplateValue(*" << GetVarName(*node)
						<< ", L\"" << EscapeWStringLiteral(resourceKey)
						<< "\", &__templateThemeError))\n";
					cpp << "\t\t\t\treturn fail("
						"L\"嵌套 Generic.xaml Template 安装失败：\" "
						"+ __templateThemeError);\n";
				}
				else if (const auto nestedIndex =
					sourceTemplateIndex(*node))
				{
					const auto& nested =
						templateBlueprints[*nestedIndex];
					const auto lockName = "__nestedTemplate_"
						+ GetVarName(*node);
					cpp << "\t\t\t{\n";
					cpp << "\t\t\t\tauto " << lockName
						<< " = __weak_"
						<< nested.VariableName.substr(2)
						<< ".lock();\n";
					cpp << "\t\t\t\tif (!" << lockName
						<< " || !cui::framework::XamlAccess::"
						"SetTemplate(*" << GetVarName(*node)
						<< ", ControlTemplateReference(std::move("
						<< lockName << ")), "
						"DependencyPropertyValueSource::Template))\n";
					cpp << "\t\t\t\t\treturn fail("
						"L\"嵌套作者 ControlTemplate 安装失败。\");\n";
					cpp << "\t\t\t}\n";
				}
			}
			if (std::any_of(
				generatedNodes.begin(), generatedNodes.end(),
				[](const auto* node)
				{
					return !node->TemplateState.
						AppliedControlTemplate.empty();
				}))
				cpp << "\n";

			cpp << "\t\t\t// Establish a fresh template namescope "
				"for this application.\n";
			for (const auto* node : generatedNodes)
			{
				const auto nodeVar = GetVarName(*node);
				const auto ownerPointer =
					templateOwnerPointerExpression(
						node->TemplateState.Owner);
				const auto ownerReference =
					templateOwnerReferenceExpression(
						node->TemplateState.Owner);
				cpp << "\t\t\tcui::framework::XamlAccess::"
					"SetTemplatedParent(*" << nodeVar << ", "
					<< ownerPointer << ");\n";
				if (!node->TemplateState.PartName.empty())
				{
					cpp << "\t\t\tif (!cui::framework::XamlAccess::"
						"RegisterTemplatePart(" << ownerReference
						<< ", L\""
						<< EscapeWStringLiteral(
							node->TemplateState.PartName)
						<< "\", " << nodeVar << "))\n";
					cpp << "\t\t\t\treturn fail("
						"L\"ControlTemplate 部件注册失败。\");\n";
				}
				if (node->TemplateContentSource == L"Content")
				{
					cpp << "\t\t\t{\n";
					cpp << "\t\t\t\tauto* contentOwner = "
						"dynamic_cast<ContentControl*>("
						<< ownerPointer << ");\n";
					cpp << "\t\t\t\tauto* presenter = "
						"dynamic_cast<ContentPresenter*>("
						<< nodeVar << ");\n";
					cpp << "\t\t\t\tif (!contentOwner || !presenter "
						"|| !cui::framework::TemplateAccess::"
						"RegisterContentPresenter(*contentOwner, presenter))\n";
					cpp << "\t\t\t\t\treturn fail("
						"L\"ControlTemplate ContentPresenter 注册失败。\");\n";
					cpp << "\t\t\t}\n";
				}
				else if (node->TemplateContentSource == L"Header")
				{
					cpp << "\t\t\t{\n";
					cpp << "\t\t\t\tauto* presenter = "
						"dynamic_cast<ContentPresenter*>("
						<< nodeVar << ");\n";
					cpp << "\t\t\t\tbool registered = false;\n";
					cpp << "\t\t\t\tif (auto* contentOwner = "
						"dynamic_cast<HeaderedContentControl*>("
						<< ownerPointer << "))\n";
					cpp << "\t\t\t\t\tregistered = contentOwner->"
						"RegisterTemplateHeaderPresenter(presenter);\n";
					cpp << "\t\t\t\telse if (auto* itemsOwner = "
						"dynamic_cast<HeaderedItemsControl*>("
						<< ownerPointer << "))\n";
					cpp << "\t\t\t\t\tregistered = itemsOwner->"
						"RegisterTemplateHeaderPresenter(presenter);\n";
					cpp << "\t\t\t\tif (!registered)\n";
					cpp << "\t\t\t\t\treturn fail("
						"L\"ControlTemplate HeaderPresenter 注册失败。\");\n";
					cpp << "\t\t\t}\n";
				}
				if (node->Type == UIClass::UI_ItemsPresenter)
				{
					cpp << "\t\t\t{\n";
					cpp << "\t\t\t\tauto* itemsOwner = "
						"dynamic_cast<ItemsControl*>("
						<< ownerPointer << ");\n";
					cpp << "\t\t\t\tauto* presenter = "
						"dynamic_cast<ItemsPresenter*>("
						<< nodeVar << ");\n";
					cpp << "\t\t\t\tif (!itemsOwner || !presenter "
						"|| !cui::framework::TemplateAccess::"
						"RegisterItemsPresenter(*itemsOwner, presenter))\n";
					cpp << "\t\t\t\t\treturn fail("
						"L\"ControlTemplate ItemsPresenter 注册失败。\");\n";
					cpp << "\t\t\t}\n";
				}
			}

			for (const auto* node : generatedNodes)
			{
				const auto nodeVar = GetVarName(*node);
				if ((node->Type == UIClass::UI_Button
					|| node->Type == UIClass::UI_MenuItem)
					&& !node->Structure.CommandTarget.empty())
					cpp << "\t\t\t" << nodeVar
						<< "->CommandTarget = "
						<< commandTargetExpression(
							node->Structure.CommandTarget)
						<< ";\n";
				if (!node->Properties.StyleResourceKey.empty())
					cpp << "\t\t\tcui::framework::StyleAccess::"
						"SetResourceKey(*" << nodeVar << ", L\""
						<< EscapeWStringLiteral(
							node->Properties.StyleResourceKey)
						<< "\");\n";
				cpp << GenerateLocalResources(
					*node, 3, &blueprintDocument,
					&weakStaticObjectResources);
				cpp << GenerateAuthoredProperties(*node, 3);
				cpp << GenerateContainerProperties(*node, 3);
			}

			for (const auto* node : generatedNodes)
			{
				if (node->TemplateBindings.empty()) continue;
				const auto nodeVar = GetVarName(*node);
				const auto ownerReference =
					templateOwnerReferenceExpression(
						node->TemplateState.Owner);
				for (const auto& [targetProperty, sourceProperty]
					: node->TemplateBindings)
				{
					cpp << "\t\t\tif (!" << nodeVar
						<< "->DataBindings.AddTemplateBinding(L\""
						<< EscapeWStringLiteral(targetProperty)
						<< "\", " << ownerReference << ", L\""
						<< EscapeWStringLiteral(sourceProperty)
						<< "\"))\n";
					cpp << "\t\t\t\treturn fail("
						"L\"ControlTemplate TemplateBinding 安装失败。\");\n";
				}
			}

			for (const auto* node : generatedNodes)
			{
				const auto nodeVar = GetVarName(*node);
				for (const auto& binding : node->InputBindings)
				{
					std::wstring gestureError;
					if (binding.Kind
						== DesignerModel::DesignInputBindingKind::Key)
					{
						KeyGesture gesture;
						if (!TryParseKeyGesture(
							binding.Gesture, gesture, &gestureError))
							throw std::invalid_argument(
								"Static template KeyBinding is invalid");
						const auto keyExpression =
							KeyToExpr(gesture.Key);
						if (keyExpression.empty())
							throw std::invalid_argument(
								"Static template KeyBinding key is unsupported");
						cpp << "\t\t\t(void)" << nodeVar
							<< "->AddInputBinding(KeyBinding{ "
							"RoutedCommand(L\""
							<< EscapeWStringLiteral(binding.Command)
							<< "\"), KeyGesture{ " << keyExpression
							<< ", "
							<< ModifierKeysToExpr(gesture.Modifiers)
							<< " }, ";
					}
					else
					{
						MouseGesture gesture;
						if (!TryParseMouseGesture(
							binding.Gesture, gesture, &gestureError))
							throw std::invalid_argument(
								"Static template MouseBinding is invalid");
						cpp << "\t\t\t(void)" << nodeVar
							<< "->AddInputBinding(MouseBinding{ "
							"RoutedCommand(L\""
							<< EscapeWStringLiteral(binding.Command)
							<< "\"), MouseGesture{ "
							<< MouseActionToExpr(gesture.Action)
							<< ", "
							<< ModifierKeysToExpr(gesture.Modifiers)
							<< " }, ";
					}
					if (binding.CommandParameter.empty())
						cpp << "{}";
					else cpp << "std::wstring(L\""
						<< EscapeWStringLiteral(
							binding.CommandParameter)
						<< "\")";
					cpp << ", "
						<< commandTargetExpression(
							binding.CommandTarget)
						<< " });\n";
				}

				for (const auto& [eventName, storedHandler]
					: node->Events)
				{
					if (storedHandler.empty()) continue;
					const auto descriptor =
						DesignerEventCatalog::FindControlEvent(
							node->Type, eventName,
							ComponentEvents(*node));
					if (!descriptor)
						throw std::invalid_argument(
							"Static template event is unsupported");
					cpp << "\t\t\tcui::framework::XamlAccess::"
						"RetainTemplateEventConnection(__templateOwner,\n";
					cpp << "\t\t\t\t" << nodeVar << "->"
						<< descriptor->EventField
						<< ".Subscribe(std::bind_front(&"
						<< className << "::"
						<< Utf8HandlerName(storedHandler)
						<< ", this)));\n";
				}

				for (const auto& binding : node->CommandBindings)
				{
					cpp << "\t\t\t{\n";
					cpp << "\t\t\t\tCommandBinding commandBinding;\n";
					cpp << "\t\t\t\tcommandBinding.Command = "
						"RoutedCommand(L\""
						<< EscapeWStringLiteral(binding.Command)
						<< "\");\n";
					auto emitCanExecute =
						[&](const char* field,
							const std::wstring& storedHandler)
					{
						if (storedHandler.empty()) return;
						cpp << "\t\t\t\tcommandBinding."
							<< field
							<< " = [this](Control* sender, "
							"CanExecuteRoutedEventArgs& e) { "
							<< Utf8HandlerName(storedHandler)
							<< "(sender, e); };\n";
					};
					auto emitExecuted =
						[&](const char* field,
							const std::wstring& storedHandler)
					{
						if (storedHandler.empty()) return;
						cpp << "\t\t\t\tcommandBinding."
							<< field
							<< " = [this](Control* sender, "
							"ExecutedRoutedEventArgs& e) { "
							<< Utf8HandlerName(storedHandler)
							<< "(sender, e); };\n";
					};
					emitCanExecute(
						"PreviewCanExecute",
						binding.PreviewCanExecute);
					emitCanExecute(
						"CanExecute", binding.CanExecute);
					emitExecuted(
						"PreviewExecuted",
						binding.PreviewExecuted);
					emitExecuted(
						"Executed", binding.Executed);
					cpp << "\t\t\t\tcui::framework::XamlAccess::"
						"RetainTemplateEventConnection(__templateOwner,\n";
					cpp << "\t\t\t\t\t" << nodeVar
						<< "->AddCommandBinding(std::move("
						"commandBinding)));\n";
					cpp << "\t\t\t}\n";
				}
			}

			std::unordered_map<const DesignerModel::DesignNode*,
				std::vector<const DesignerModel::DesignNode*>>
				templateChildren;
			auto findBlueprintNodeById =
				[&](int id) -> const DesignerModel::DesignNode*
			{
				const auto found = std::find_if(
					blueprintDocument.Nodes.begin(),
					blueprintDocument.Nodes.end(),
					[&](const auto& node) { return node.Id == id; });
				return found == blueprintDocument.Nodes.end()
					? nullptr : &*found;
			};
			for (const auto* node : generatedNodes)
			{
				const auto* parent = node->ParentId > 0
					? findBlueprintNodeById(node->ParentId)
					: !node->ParentRef.empty()
						? findBlueprintNodeByName(node->ParentRef)
						: nullptr;
				templateChildren[parent].push_back(node);
			}
			auto sortTemplateChildren = [](auto& children)
			{
				std::stable_sort(
					children.begin(), children.end(),
					[](const auto* left, const auto* right)
					{
						const auto leftOrder = left->Order < 0
							? (std::numeric_limits<int>::max)()
							: left->Order;
						const auto rightOrder = right->Order < 0
							? (std::numeric_limits<int>::max)()
							: right->Order;
						return leftOrder < rightOrder;
					});
			};
			std::function<void(
				const DesignerModel::DesignNode*, int)>
				emitTemplateChildren;
			std::function<void(
				const DesignerModel::DesignNode&, int)>
				emitTemplateControl;
			emitTemplateChildren =
				[&](const DesignerModel::DesignNode* parent, int indent)
			{
				auto found = templateChildren.find(parent);
				if (found == templateChildren.end()) return;
				auto children = found->second;
				sortTemplateChildren(children);
				for (const auto* child : children)
					emitTemplateControl(*child, indent);
			};
			emitTemplateControl =
				[&](const DesignerModel::DesignNode& node, int indent)
			{
				const std::string indentText(indent, '\t');
				const auto nodeVar = GetVarName(node);
				const auto* parent = node.ParentId > 0
					? findBlueprintNodeById(node.ParentId)
					: !node.ParentRef.empty()
						? findBlueprintNodeByName(node.ParentRef)
						: nullptr;
				const bool parentIsTop = parent == &*blueprintOwner;
				const auto parentPointer = parentIsTop
					? std::string("&__templateOwner")
					: parent ? GetVarName(*parent) : std::string{};
				const auto parentReference = parentIsTop
					? std::string("__templateOwner")
					: parent ? "*" + GetVarName(*parent)
						: std::string{};
				const auto parentType = parentIsTop
					? blueprintOwner->Type
					: parent ? parent->Type : UIClass::UI_CUSTOM;
				if (!parent)
					throw std::invalid_argument(
						"Static ControlTemplate node has no parent");
				const bool isVisualHeader =
					node.Structure.ChildRole
					== DesignerModel::DesignNodeChildRole::Header;
				if (node.TemplateState.ControlTemplateRoot)
					cpp << indentText
						<< "cui::framework::TemplateAccess::"
						"SetTemplateRoot(" << parentReference
						<< ", std::move(__owned_" << nodeVar
						<< "));\n";
				else if (isVisualHeader
					&& (IsUIClassAssignableFrom(
							UIClass::UI_HeaderedContentControl,
							parentType)
						|| IsUIClassAssignableFrom(
							UIClass::UI_HeaderedItemsControl,
							parentType)))
					cpp << indentText << parentPointer
						<< "->SetVisualHeader(std::move(__owned_"
						<< nodeVar << "));\n";
				else if (IsUIClassAssignableFrom(
					UIClass::UI_ItemsControl, parentType))
					cpp << indentText << parentPointer
						<< "->AddItemControl(std::move(__owned_"
						<< nodeVar << "));\n";
				else if (IsUIClassAssignableFrom(
					UIClass::UI_ContentControl, parentType))
					cpp << indentText << parentPointer
						<< "->SetVisualContent(std::move(__owned_"
						<< nodeVar << "));\n";
				else if (parentType == UIClass::UI_Popup)
					cpp << indentText << parentPointer
						<< "->SetChild(std::move(__owned_"
						<< nodeVar << "));\n";
				else if (IsUIClassAssignableFrom(
					UIClass::UI_Decorator, parentType))
					cpp << indentText << parentPointer
						<< "->SetChild(std::move(__owned_"
						<< nodeVar << "));\n";
				else
					cpp << indentText << parentPointer
						<< "->AddOwned(std::move(__owned_"
						<< nodeVar << "));\n";

				if (node.TemplateState.ControlTemplateRoot)
					cpp << indentText
						<< "cui::framework::XamlAccess::"
						"SetLogicalParent(*" << nodeVar
						<< ", nullptr);\n";
				if (!node.TemplateState.ContentOwner.empty())
				{
					const auto logicalOwner =
						node.TemplateState.ContentOwner
							== blueprint.OwnerName
						? std::string("&__templateOwner")
						: templateOwnerPointerExpression(
							node.TemplateState.ContentOwner);
					cpp << indentText
						<< "cui::framework::XamlAccess::"
						"SetLogicalParent(*" << nodeVar << ", "
						<< logicalOwner << ");\n";
				}
				emitTemplateChildren(&node, indent);
			};
			emitTemplateChildren(&*blueprintOwner, 3);

			cpp << "\t\t\tif (!CuiRuntime::XamlFrameworkTheme::"
				"Apply(__templateOwner, true, &__templateThemeError))\n";
			cpp << "\t\t\t\treturn fail("
				"L\"ControlTemplate 子树主题应用失败：\" "
				"+ __templateThemeError);\n";
			cpp << "\t\t\tif (auto documentStyles = "
				"cui::framework::StyleAccess::DocumentStyles("
				"__templateOwner);\n";
			cpp << "\t\t\t\tdocumentStyles && "
				"!cui::framework::StyleAccess::SetDocumentStyles("
				"__templateOwner, std::move(documentStyles), true))\n";
			cpp << "\t\t\t\treturn fail("
				"L\"ControlTemplate 子树文档样式应用失败。\");\n";

			for (const auto* node : generatedNodes)
			{
				if (node->Bindings.empty()) continue;
				const auto nodeVar = GetVarName(*node);
				for (const auto& [targetProperty, binding]
					: node->Bindings)
				{
					if (binding.IsMultiBinding())
						throw std::invalid_argument(
							"Static ControlTemplate MultiBinding "
							"is not supported");
					if (binding.RelativeSource
						== DesignerBindingRelativeSource::FindAncestor)
						throw std::invalid_argument(
							"Static ControlTemplate FindAncestor "
							"requires dynamic materialization");

					std::string sourceExpression =
						nodeVar + "->DataContextSource()";
					std::string sourceGuard;
					if (!binding.ElementName.empty())
						sourceExpression =
							bindingElementExpression(
								binding.ElementName);
					else if (binding.RelativeSource
						== DesignerBindingRelativeSource::Self)
						sourceExpression = "*" + nodeVar;
					else if (binding.RelativeSource
						== DesignerBindingRelativeSource::
							TemplatedParent)
						sourceExpression =
							templateOwnerReferenceExpression(
								node->TemplateState.Owner);
					else if (targetProperty == L"DataContext")
					{
						sourceGuard = nodeVar
							+ "->GetInheritanceParent() && ";
						sourceExpression = nodeVar
							+ "->GetInheritanceParent()->"
								"DataContextSource()";
					}

					const auto converterName =
						DesignerBindingUtils::Trim(
							binding.Converter);
					const auto fallbackExpression =
						binding.FallbackValue
						? GenerateStyleValueExpression(
							*binding.FallbackValue)
						: "{}";
					const auto targetNullExpression =
						binding.TargetNullValue
						? GenerateStyleValueExpression(
							*binding.TargetNullValue)
						: "{}";
					const auto converterParameterExpression =
						binding.ConverterParameter
						? GenerateStyleValueExpression(
							*binding.ConverterParameter)
						: "{}";
					const auto stringFormatExpression =
						binding.StringFormat
						? "std::optional<std::wstring>(L\""
							+ EscapeWStringLiteral(
								*binding.StringFormat)
							+ "\")"
						: "{}";
					const bool hasExtendedOptions =
						binding.FallbackValue.has_value()
						|| binding.TargetNullValue.has_value()
						|| binding.ConverterParameter.has_value()
						|| binding.StringFormat.has_value();

					cpp << "\t\t\t{\n";
					if (!converterName.empty())
					{
						cpp << "\t\t\t\tauto converter = "
							"BindingValueConverterRegistry::Create(L\""
							<< EscapeWStringLiteral(converterName)
							<< "\");\n";
						cpp << "\t\t\t\tif (!converter)\n";
						cpp << "\t\t\t\t\treturn fail("
							"L\"ControlTemplate Binding Converter "
							"不存在。\");\n";
					}
					cpp << "\t\t\t\tconst bool attached = "
						<< sourceGuard << nodeVar
						<< "->DataBindings.Add(L\""
						<< EscapeWStringLiteral(targetProperty)
						<< "\", " << sourceExpression << ", L\""
						<< EscapeWStringLiteral(
							binding.SourceProperty)
						<< "\", " << BindingModeToExpr(binding.Mode)
						<< ", "
						<< DataSourceUpdateModeToExpr(
							binding.UpdateMode);
					if (!converterName.empty())
						cpp << ", std::move(converter)";
					else if (hasExtendedOptions)
						cpp << ", {}";
					if (hasExtendedOptions)
						cpp << ", " << fallbackExpression
							<< ", " << targetNullExpression
							<< ", "
							<< converterParameterExpression
							<< ", " << stringFormatExpression;
					cpp << ") != nullptr;\n";
					cpp << "\t\t\t\tif (!attached)\n";
					cpp << "\t\t\t\t\treturn fail("
						"L\"ControlTemplate Binding 安装失败。\");\n";
					cpp << "\t\t\t}\n";
				}
			}

			cpp << GenerateDeclarativeInteractionsCode(
				blueprint.VisualStateGroups,
				blueprint.EventTriggers,
				"__templateOwner", 3);

			for (const auto* node : generatedNodes)
			{
				if (!node->TemplateState.
					AppliedControlTemplateFromTheme) continue;
				const auto& resourceKey = node->TemplateState.
					AppliedControlTemplateResource;
				cpp << "\t\t\tif (!CuiRuntime::XamlFrameworkTheme::"
					"ApplyTemplateVisualStates(*"
					<< GetVarName(*node) << ", L\""
					<< EscapeWStringLiteral(resourceKey)
					<< "\", &__templateThemeError))\n";
				cpp << "\t\t\t\treturn fail("
					"L\"嵌套 Generic.xaml VisualState 安装失败：\" "
					"+ __templateThemeError);\n";
			}
			for (const auto* node : generatedNodes)
				if (!node->TemplateState.
					AppliedControlTemplate.empty())
					cpp << "\t\t\tcui::framework::TemplateAccess::"
						"CompleteTemplateApplication(*"
						<< GetVarName(*node) << ");\n";

			cpp << "\t\t\tif (!cui::framework::TemplateAccess::"
				"GetTemplateRoot(__templateOwner))\n";
			cpp << "\t\t\t\treturn fail("
				"L\"ControlTemplate 未生成唯一视觉根。\");\n";
			cpp << "\t\t\tif (outError) outError->clear();\n";
			cpp << "\t\t\treturn true;\n";
			cpp << "\t\t}\n";
			cpp << "\t\tcatch (const std::exception&)\n";
			cpp << "\t\t{\n";
			cpp << "\t\t\treturn fail("
				"L\"ControlTemplate 静态构造发生运行时异常。\");\n";
			cpp << "\t\t}\n";
			cpp << "\t\tcatch (...)\n";
			cpp << "\t\t{\n";
			cpp << "\t\t\treturn fail("
				"L\"ControlTemplate 静态构造发生未知异常。\");\n";
			cpp << "\t\t}\n";
			cpp << "\t});\n\n";
		}
	}

	// 2) Apply scalar/structured state after the complete namescope exists.
	for (const auto& node : _sourceDocument.Nodes)
	{
		if (node.TemplateState.Generated) continue;
		cpp << GenerateControlCommonProperties(node, 1);
		cpp << GenerateLocalResources(
			node, 1, nullptr, &staticObjectResources);
		cpp << GenerateAuthoredProperties(node, 1);
		cpp << GenerateContainerProperties(node, 1);
		cpp << "\n";
	}

	auto emitInputBindings = [&](const auto& bindings,
		const std::string& target)
	{
		for (const auto& binding : bindings)
		{
			std::wstring gestureError;
			if (binding.Kind == DesignerModel::DesignInputBindingKind::Key)
			{
				KeyGesture gesture;
				if (!TryParseKeyGesture(binding.Gesture, gesture, &gestureError))
					throw std::invalid_argument(
						"Code generation encountered an invalid KeyBinding");
				const auto keyExpression = KeyToExpr(gesture.Key);
				if (keyExpression.empty())
					throw std::invalid_argument(
						"Code generation encountered an unsupported Key identity");
				cpp << "\t(void)" << target << "->AddInputBinding(KeyBinding{ "
					<< "RoutedCommand(L\"" << EscapeWStringLiteral(binding.Command)
					<< "\"), KeyGesture{ " << keyExpression << ", "
					<< ModifierKeysToExpr(gesture.Modifiers) << " }, ";
			}
			else
			{
				MouseGesture gesture;
				if (!TryParseMouseGesture(binding.Gesture, gesture, &gestureError))
					throw std::invalid_argument(
						"Code generation encountered an invalid MouseBinding");
				cpp << "\t(void)" << target << "->AddInputBinding(MouseBinding{ "
					<< "RoutedCommand(L\"" << EscapeWStringLiteral(binding.Command)
					<< "\"), MouseGesture{ " << MouseActionToExpr(gesture.Action)
					<< ", " << ModifierKeysToExpr(gesture.Modifiers) << " }, ";
			}
			if (binding.CommandParameter.empty()) cpp << "{}";
			else cpp << "std::wstring(L\""
				<< EscapeWStringLiteral(binding.CommandParameter) << "\")";
			cpp << ", " << CommandTargetExpression(binding.CommandTarget)
				<< " });\n";
		}
	};
	if (!_sourceDocument.Window.InputBindings.empty())
	{
		cpp << "\t// XAML InputBindings\n";
		emitInputBindings(_sourceDocument.Window.InputBindings, "this");
	}
	for (const auto& node : _sourceDocument.Nodes)
	{
		if (node.TemplateState.Generated) continue;
		if (node.InputBindings.empty()) continue;
		emitInputBindings(node.InputBindings, GetVarName(node));
	}
	if (!_sourceDocument.Window.InputBindings.empty()
		|| std::any_of(
			_sourceDocument.Nodes.begin(), _sourceDocument.Nodes.end(),
			[](const auto& node)
			{
				return !node.TemplateState.Generated
					&& !node.InputBindings.empty();
			})) cpp << "\n";

	// Event subscriptions are owned by RAII connections and disconnect before Window teardown.
	{
		std::unordered_map<std::string, std::type_index> sigOf;
		std::vector<GeneratedEventBinding> binds;
		binds.reserve(_sourceDocument.Nodes.size());
		// Window 事件
		for (const auto& kv : _sourceDocument.Window.Events)
		{
			if (kv.first.empty()) continue;
			if (kv.second.empty()) continue;
			const auto descriptor = DesignerEventCatalog::FindWindowEvent(kv.first);
			if (!descriptor) continue;
			std::string handlerName = Utf8HandlerName(kv.second);
			auto itSig = sigOf.find(handlerName);
			if (itSig != sigOf.end()
				&& itSig->second != descriptor->Signature) continue;
			if (itSig == sigOf.end())
				sigOf.emplace(handlerName, descriptor->Signature);
			binds.push_back(GeneratedEventBinding{ "this",
				descriptor->EventField, handlerName, descriptor->ParameterList });
		}

		for (const auto& node : _sourceDocument.Nodes)
		{
			if (node.TemplateState.Generated) continue;
			std::string ctrlVar = GetVarName(node);
			for (const auto& kv : node.Events)
			{
				const auto& evNameW = kv.first;
				if (kv.second.empty()) continue;
				const auto descriptor = DesignerEventCatalog::FindControlEvent(
					node.Type, evNameW, ComponentEvents(node));
				if (!descriptor) continue;
				std::string handlerName = Utf8HandlerName(kv.second);
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

	// CommandBinding is a first-class command collection, not four unrelated
	// routed-event subscriptions. Keeping this grouping is required for class
	// bindings, command-source requery, and atomic replacement to share the same
	// command identity and lifetime.
	auto emitCommandBindings = [&](const auto& bindings,
		const std::string& target)
	{
		for (const auto& binding : bindings)
		{
			cpp << "\t{\n";
			cpp << "\t\tCommandBinding __commandBinding;\n";
			cpp << "\t\t__commandBinding.Command = RoutedCommand(L\""
				<< EscapeWStringLiteral(binding.Command) << "\");\n";
			auto emitCanExecute = [&](const char* field,
				const std::wstring& storedHandler)
			{
				if (storedHandler.empty()) return;
				cpp << "\t\t__commandBinding." << field
					<< " = [this](Control* sender, CanExecuteRoutedEventArgs& e) { "
					<< Utf8HandlerName(storedHandler) << "(sender, e); };\n";
			};
			auto emitExecuted = [&](const char* field,
				const std::wstring& storedHandler)
			{
				if (storedHandler.empty()) return;
				cpp << "\t\t__commandBinding." << field
					<< " = [this](Control* sender, ExecutedRoutedEventArgs& e) { "
					<< Utf8HandlerName(storedHandler) << "(sender, e); };\n";
			};
			emitCanExecute("PreviewCanExecute", binding.PreviewCanExecute);
			emitCanExecute("CanExecute", binding.CanExecute);
			emitExecuted("PreviewExecuted", binding.PreviewExecuted);
			emitExecuted("Executed", binding.Executed);
			cpp << "\t\t_generatedEventConnections.emplace_back("
				<< target
				<< "->AddCommandBinding(std::move(__commandBinding)));\n";
			cpp << "\t}\n";
		}
	};
	if (!_sourceDocument.Window.CommandBindings.empty())
	{
		cpp << "\t// XAML CommandBindings\n";
		emitCommandBindings(_sourceDocument.Window.CommandBindings, "this");
	}
	for (const auto& node : _sourceDocument.Nodes)
	{
		if (node.TemplateState.Generated) continue;
		if (node.CommandBindings.empty()) continue;
		emitCommandBindings(node.CommandBindings, GetVarName(node));
	}
	if (!_sourceDocument.Window.CommandBindings.empty()
		|| std::any_of(
			_sourceDocument.Nodes.begin(), _sourceDocument.Nodes.end(),
			[](const auto& node)
			{
				return !node.TemplateState.Generated
					&& !node.CommandBindings.empty();
			})) cpp << "\n";

	// 2) Assemble the logical authored hierarchy. ItemsControl children enter
	//    Items; ordinary container children enter the visual collection.
	cpp << "\t// 组装控件层级（包含布局容器）\n";

	std::unordered_map<const DesignerModel::DesignNode*,
		std::vector<const DesignerModel::DesignNode*>> childrenOf;
	childrenOf.reserve(_sourceDocument.Nodes.size());
	auto findNodeById = [&](int id) -> const DesignerModel::DesignNode*
	{
		const auto found = std::find_if(
			_sourceDocument.Nodes.begin(), _sourceDocument.Nodes.end(),
			[&](const auto& node) { return node.Id == id; });
		return found == _sourceDocument.Nodes.end() ? nullptr : &*found;
	};
	auto findNodeByName = [&](const std::wstring& name)
		-> const DesignerModel::DesignNode*
	{
		const auto found = std::find_if(
			_sourceDocument.Nodes.begin(), _sourceDocument.Nodes.end(),
			[&](const auto& node) { return node.Name == name; });
		return found == _sourceDocument.Nodes.end() ? nullptr : &*found;
	};
	for (const auto& node : _sourceDocument.Nodes)
	{
		if (node.TemplateState.Generated) continue;
		const auto* parent = node.ParentId > 0
			? findNodeById(node.ParentId)
			: !node.ParentRef.empty()
				? findNodeByName(node.ParentRef) : nullptr;
		childrenOf[parent].push_back(&node);
	}

	auto sortAuthoredChildren = [&](auto& list)
	{
		std::stable_sort(list.begin(), list.end(), [](const auto& left,
			const auto& right)
			{
				const auto leftOrder = left->Order < 0
					? (std::numeric_limits<int>::max)() : left->Order;
				const auto rightOrder = right->Order < 0
					? (std::numeric_limits<int>::max)() : right->Order;
				return leftOrder < rightOrder;
			});
	};

	std::function<void(const DesignerModel::DesignNode*,
		const std::string&, int)> emitChildren;
	std::function<void(const DesignerModel::DesignNode&,
		const std::string&, int)> emitControl;

	emitChildren = [&](const DesignerModel::DesignNode* parent,
		const std::string& parentExpr, int indent)
	{
		auto it = childrenOf.find(parent);
		if (it == childrenOf.end()) return;
		auto list = it->second;
		sortAuthoredChildren(list);
		for (const auto* child : list)
			emitControl(*child, parentExpr, indent);
	};

	emitControl = [&](const DesignerModel::DesignNode& node,
		const std::string& parentExpr, int indent)
	{
		std::string childVar = GetVarName(node);
		std::string indentStr(indent, '\t');

		// 添加到父容器
		UIClass parentType = UIClass::UI_CUSTOM;
		const auto* parent = node.ParentId > 0
			? findNodeById(node.ParentId)
			: !node.ParentRef.empty()
				? findNodeByName(node.ParentRef) : nullptr;
		if (parent) parentType = parent->Type;
		const bool isVisualHeader = node.Structure.ChildRole
			== DesignerModel::DesignNodeChildRole::Header;
		if (!parent)
		{
			cpp << indentStr << parentExpr
				<< "->SetVisualContent(std::move(__owned_" << childVar << "));\n";
		}
		else if (node.TemplateState.ControlTemplateRoot)
		{
			cpp << indentStr
				<< "cui::framework::TemplateAccess::SetTemplateRoot(*"
				<< parentExpr << ", std::move(__owned_" << childVar << "));\n";
		}
		else if (isVisualHeader
			&& (IsUIClassAssignableFrom(
					UIClass::UI_HeaderedContentControl, parentType)
				|| IsUIClassAssignableFrom(
					UIClass::UI_HeaderedItemsControl, parentType)))
		{
			cpp << indentStr << parentExpr
				<< "->SetVisualHeader(std::move(__owned_" << childVar << "));\n";
		}
		else if (IsUIClassAssignableFrom(
			UIClass::UI_ItemsControl, parentType))
		{
			cpp << indentStr << parentExpr
				<< "->AddItemControl(std::move(__owned_" << childVar << "));\n";
		}
		else if (IsUIClassAssignableFrom(
			UIClass::UI_ContentControl, parentType))
		{
			cpp << indentStr << parentExpr
				<< "->SetVisualContent(std::move(__owned_" << childVar << "));\n";
		}
		else if (parentType == UIClass::UI_Popup)
		{
			cpp << indentStr << parentExpr
				<< "->SetChild(std::move(__owned_" << childVar << "));\n";
		}
		else if (IsUIClassAssignableFrom(
			UIClass::UI_Decorator, parentType))
		{
			cpp << indentStr << parentExpr
				<< "->SetChild(std::move(__owned_" << childVar << "));\n";
		}
		else
		{
			cpp << indentStr << parentExpr << "->AddOwned(std::move(__owned_" << childVar << "));\n";
		}

		if (node.TemplateState.Generated
			&& node.TemplateState.ControlTemplateRoot)
			cpp << indentStr
				<< "cui::framework::XamlAccess::SetLogicalParent(*"
				<< childVar << ", nullptr);\n";
		if (!node.TemplateState.ContentOwner.empty())
		{
			const auto* logicalOwner = findSourceNodeByName(
				node.TemplateState.ContentOwner);
			if (!logicalOwner)
				throw std::invalid_argument(
					"Projected template content has no logical owner");
			cpp << indentStr
				<< "cui::framework::XamlAccess::SetLogicalParent(*"
				<< childVar << ", " << GetVarName(*logicalOwner) << ");\n";
		}

		emitChildren(&node, childVar, indent);

		cpp << "\n";
	};

	// 根级控件由文档 ParentId/ParentRef 唯一决定。
	auto rootsIt = childrenOf.find(nullptr);
	if (rootsIt != childrenOf.end())
	{
		auto roots = rootsIt->second;
		sortAuthoredChildren(roots);
		for (const auto* root : roots)
			emitControl(*root, "this", 1);
	}

	cpp << "\tif (!CuiRuntime::XamlFrameworkTheme::Apply("
		"*this, true, &__frameworkThemeError))\n";
	cpp << "\t\tthrow std::runtime_error("
		"\"Generated Generic.xaml theme installation failed\");\n";

	if (!_sourceDocument.Window.Properties.StyleResourceKey.empty())
		cpp << "\tcui::framework::StyleAccess::SetResourceKey(*this, L\""
			<< EscapeWStringLiteral(
				_sourceDocument.Window.Properties.StyleResourceKey)
			<< "\");\n";
	cpp << GenerateStyleSheetCode(1, staticObjectResources);

	auto findAuthoredTemplateIndex =
		[&](const DesignerModel::DesignNode& owner)
		-> std::optional<size_t>
	{
		if (owner.TemplateState.AppliedControlTemplateFromTheme)
			return std::nullopt;
		const auto key = !owner.Structure.ControlTemplate.empty()
			? owner.Structure.ControlTemplate
			: owner.TemplateState.AppliedControlTemplateResource;
		const DesignerModel::DesignControlTemplate* definition = nullptr;
		if (!key.empty())
			definition = _sourceDocument.FindControlTemplate(
				_sourceDocument.Nodes, owner, key);
		else if (!owner.TemplateState.AppliedControlTemplate.empty())
			definition = owner.ComponentType.Empty()
				? _sourceDocument.FindImplicitControlTemplate(
					_sourceDocument.Nodes, owner, owner.Type)
				: _sourceDocument.FindImplicitControlTemplate(
					_sourceDocument.Nodes, owner, owner.ComponentType);
		else if (_sourceDocument.Nodes.size()
			== std::count_if(
				_sourceDocument.Nodes.begin(),
				_sourceDocument.Nodes.end(),
				[](const auto& node)
				{ return !node.TemplateState.Generated; }))
			definition = owner.ComponentType.Empty()
				? _sourceDocument.FindImplicitControlTemplate(
					_sourceDocument.Nodes, owner, owner.Type)
				: _sourceDocument.FindImplicitControlTemplate(
					_sourceDocument.Nodes, owner, owner.ComponentType);
		if (!definition) return std::nullopt;
		const auto index = static_cast<size_t>(
			definition - _sourceDocument.ControlTemplates.data());
		return index < templateBlueprints.size()
			? std::optional<size_t>{ index } : std::nullopt;
	};
	for (const auto& owner : _sourceDocument.Nodes)
	{
		if (owner.TemplateState.Generated) continue;
		const auto templateIndex =
			findAuthoredTemplateIndex(owner);
		if (!templateIndex) continue;
		const auto& blueprint = templateBlueprints[*templateIndex];
		const auto source = !owner.Structure.ControlTemplate.empty()
			? "DependencyPropertyValueSource::Local"
			: "DependencyPropertyValueSource::Style";
		cpp << "\tif (!cui::framework::XamlAccess::SetTemplate(*"
			<< GetVarName(owner)
			<< ", ControlTemplateReference("
			<< blueprint.VariableName << "), " << source << "))\n";
		cpp << "\t\tthrow std::runtime_error("
			"\"Generated authored Control.Template installation failed\");\n";
	}
	if (!templateBlueprints.empty()) cpp << "\n";

	if (!_sourceDocument.Window.Properties.Values.empty())
		cpp << "\t// XAML Window Local 属性/资源表达式\n";
	for (const auto& [propertyName, assignment]
		: _sourceDocument.Window.Properties.Values)
	{
		if (!assignment.DynamicResourceKey.empty())
			cpp << "\t(void)this->SetDynamicResource(L\""
				<< EscapeWStringLiteral(propertyName) << "\", L\""
				<< EscapeWStringLiteral(assignment.DynamicResourceKey) << "\");\n";
		else
			cpp << "\t(void)this->TrySetPropertyValue(L\""
				<< EscapeWStringLiteral(propertyName) << "\", "
				<< GenerateStyleValueExpression(assignment.Value) << ");\n";
	}
	if (!_sourceDocument.Window.Properties.Values.empty()) cpp << "\n";

	for (const auto& owner : _sourceDocument.Nodes)
	{
		if (owner.TemplateState.Generated) continue;
		cpp << "\tif (" << GetVarName(owner)
			<< "->GetTemplate())\n";
		cpp << "\t{\n";
		cpp << "\t\t(void)" << GetVarName(owner)
			<< "->ApplyTemplate();\n";
		cpp << "\t\tif (!cui::framework::TemplateAccess::"
			"GetTemplateRoot(*" << GetVarName(owner)
			<< ") || !" << GetVarName(owner)
			<< "->LastTemplateError().empty())\n";
		cpp << "\t\t\tthrow std::runtime_error("
			"\"Generated ControlTemplate application failed\");\n";
		cpp << "\t}\n";
	}
	if (std::any_of(
		_sourceDocument.Nodes.begin(), _sourceDocument.Nodes.end(),
		[](const auto& node)
		{ return !node.TemplateState.Generated; }))
		cpp << "\n";

	cpp << "}\n\n";
	
	// 析构函数
	cpp << className << "::~" << classLeaf << "()\n";
	cpp << "{\n";
	cpp << "}\n";

	const bool hasStyleDataTriggers = std::any_of(
		_styleSheet.Rules.begin(), _styleSheet.Rules.end(),
		[](const DesignerStyleRule& rule)
		{
			return !rule.DataConditions.empty()
				|| std::any_of(rule.Triggers.begin(), rule.Triggers.end(),
					[](const DesignerStyleTrigger& trigger)
					{
						return !trigger.DataConditions.empty();
					});
		});
	const bool hasDataBindings = !_sourceDocument.Window.Bindings.empty()
		|| hasStyleDataTriggers || std::any_of(
			_sourceDocument.Nodes.begin(), _sourceDocument.Nodes.end(),
			[](const auto& node) { return !node.Bindings.empty(); })
		|| std::any_of(
			_sourceDocument.ControlTemplates.begin(),
			_sourceDocument.ControlTemplates.end(),
			[](const auto& definition)
			{
				return std::any_of(
					definition.Template.begin(),
					definition.Template.end(),
					[](const auto& node)
					{ return !node.Bindings.empty(); });
			});
	if (hasDataBindings)
	{
		cpp << "\n";
		cpp << "bool " << className
			<< "::BindData(BindingSourceReference dataContext)\n";
		cpp << "{\n";
		cpp << "\tif (!dataContext) return false;\n";
		cpp << "\tauto __windowDataContext = dataContext;\n";
		cpp << "\tif (!SetDataContext(std::move(dataContext))) return false;\n";
		cpp << "\tbool success = true;\n";
		auto emitBindings = [&](const auto& bindings,
			const std::string& controlVar, bool isWindow)
		{
			if (bindings.empty()) return;
			cpp << "\t" << controlVar << "->DataBindings.Clear();\n";
			for (const auto& [targetProperty, binding] : bindings)
			{
				if (binding.IsMultiBinding())
					throw std::invalid_argument(
						"MultiBinding requires dynamic XAML materialization");
				std::string sourceExpression = controlVar
					+ "->DataContextSource()";
				std::string sourceGuard;
				if (!binding.ElementName.empty())
				{
					if (binding.ElementName == _sourceDocument.Window.Name)
						sourceExpression = "*this";
					else
					{
						const auto sourceControl = std::find_if(
							_sourceDocument.Nodes.begin(),
							_sourceDocument.Nodes.end(), [&](const auto& candidate)
							{
								return candidate.Name == binding.ElementName;
							});
						if (sourceControl == _sourceDocument.Nodes.end())
							throw std::invalid_argument(
								"ElementName binding source is missing");
						sourceExpression = "*" + GetVarName(*sourceControl);
					}
				}
				else if (binding.RelativeSource
					== DesignerBindingRelativeSource::Self)
					sourceExpression = "*" + controlVar;
				else if (binding.RelativeSource
					== DesignerBindingRelativeSource::TemplatedParent)
					throw std::invalid_argument(
						"TemplatedParent requires dynamic XAML component materialization");
				else if (binding.RelativeSource
					== DesignerBindingRelativeSource::FindAncestor)
					throw std::invalid_argument(
						"FindAncestor requires dynamic XAML materialization");
				else if (targetProperty == L"DataContext")
				{
					if (isWindow)
						sourceExpression = "__windowDataContext";
					else
					{
						sourceGuard = controlVar + "->GetInheritanceParent() && ";
						sourceExpression = controlVar
							+ "->GetInheritanceParent()->DataContextSource()";
					}
				}
				const auto converterName = DesignerBindingUtils::Trim(binding.Converter);
				const auto fallbackExpression = binding.FallbackValue
					? GenerateStyleValueExpression(*binding.FallbackValue) : "{}";
				const auto targetNullExpression = binding.TargetNullValue
					? GenerateStyleValueExpression(*binding.TargetNullValue) : "{}";
				const auto converterParameterExpression = binding.ConverterParameter
					? GenerateStyleValueExpression(*binding.ConverterParameter) : "{}";
				const auto stringFormatExpression = binding.StringFormat
					? "std::optional<std::wstring>(L\""
						+ EscapeWStringLiteral(*binding.StringFormat) + "\")"
					: "{}";
				const bool hasExtendedOptions = binding.FallbackValue.has_value()
					|| binding.TargetNullValue.has_value()
					|| binding.ConverterParameter.has_value()
					|| binding.StringFormat.has_value();
				cpp << "\t{\n";
				cpp << "\t\tbool cuiBindingAttached = false;\n";
				const char* operationIndent = "\t\t";
				if (converterName.empty())
				{
					cpp << operationIndent << "cuiBindingAttached = " << sourceGuard
						<< controlVar
						<< "->DataBindings.Add(L\""
						<< EscapeWStringLiteral(targetProperty) << "\", "
						<< sourceExpression << ", L\""
						<< EscapeWStringLiteral(binding.SourceProperty) << "\", "
						<< BindingModeToExpr(binding.Mode) << ", "
						<< DataSourceUpdateModeToExpr(binding.UpdateMode);
					if (hasExtendedOptions)
						cpp << ", {}, " << fallbackExpression << ", "
							<< targetNullExpression << ", "
							<< converterParameterExpression << ", "
							<< stringFormatExpression;
					cpp << ") != nullptr;\n";
				}
				else
				{
					cpp << operationIndent
						<< "auto cuiConverter = BindingValueConverterRegistry::Create(L\""
						<< EscapeWStringLiteral(converterName) << "\");\n";
					cpp << operationIndent
						<< "cuiBindingAttached = cuiConverter && "
						<< sourceGuard << controlVar
						<< "->DataBindings.Add(L\""
						<< EscapeWStringLiteral(targetProperty) << "\", "
						<< sourceExpression << ", L\""
						<< EscapeWStringLiteral(binding.SourceProperty) << "\", "
						<< BindingModeToExpr(binding.Mode) << ", "
						<< DataSourceUpdateModeToExpr(binding.UpdateMode)
						<< ", cuiConverter";
					if (hasExtendedOptions)
						cpp << ", " << fallbackExpression << ", "
							<< targetNullExpression << ", "
							<< converterParameterExpression << ", "
							<< stringFormatExpression;
					cpp << ") != nullptr;\n";
				}
				cpp << "\t\tif (!cuiBindingAttached)\n\t\t{\n";
				cpp << "\t\t\tsuccess = false;\n";
				cpp << "\t\t}\n";
				cpp << "\t}\n";
			}
		};
		emitBindings(_sourceDocument.Window.Bindings, "this", true);
		for (const auto& node : _sourceDocument.Nodes)
		{
			if (node.TemplateState.Generated) continue;
			if (node.Bindings.empty()) continue;
			emitBindings(node.Bindings, GetVarName(node), false);
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
					<< "\tInitializeComponent();\n"
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
