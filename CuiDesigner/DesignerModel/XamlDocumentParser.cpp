#include "XamlDocumentParser.h"

#include "DesignDocumentGraph.h"
#include "DesignDocumentEventIndex.h"
#include "DesignDocumentMaterializer.h"
#include "../../XmlLite/include/Xml.h"
#include "../DesignerBindingUtils.h"
#include "../DesignerDataContextSchemaUtils.h"
#include "../DesignerEventCatalog.h"
#include "../DesignerFormPropertyCatalog.h"
#include "../DesignerPropertyCatalog.h"
#include "../DesignerStyleSheetUtils.h"

#include <Convert.h>

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace DesignerModel
{
namespace
{
	using namespace System::Xml;
	using Element = std::shared_ptr<XmlElement>;

	std::wstring FromUtf8(const std::string& value)
	{
		return Convert::Utf8ToUnicode(value);
	}

	std::string ToUtf8(const std::wstring& value)
	{
		return Convert::UnicodeToUtf8(value);
	}

	std::wstring Trim(const std::wstring& value)
	{
		size_t begin = 0;
		while (begin < value.size() && std::iswspace(value[begin])) ++begin;
		size_t end = value.size();
		while (end > begin && std::iswspace(value[end - 1])) --end;
		return value.substr(begin, end - begin);
	}

	std::wstring Lower(std::wstring value)
	{
		std::transform(value.begin(), value.end(), value.begin(),
			[](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });
		return value;
	}

	bool Equals(const std::wstring& left, const std::wstring& right)
	{
		return _wcsicmp(left.c_str(), right.c_str()) == 0;
	}

	bool Equals(const std::string& left, const char* right)
	{
		return _stricmp(left.c_str(), right) == 0;
	}

	std::vector<Element> ChildElements(const Element& parent)
	{
		std::vector<Element> result;
		if (!parent) return result;
		for (const auto& child : parent->ChildNodes())
		{
			if (child && child->NodeType() == XmlNodeType::Element)
				result.push_back(std::static_pointer_cast<XmlElement>(child));
		}
		return result;
	}

	std::optional<std::wstring> Attribute(
		const Element& element,
		const std::wstring& localName,
		const std::optional<std::wstring>& prefix = std::nullopt)
	{
		if (!element) return std::nullopt;
		for (const auto& attribute : element->Attributes())
		{
			if (!attribute || !Equals(FromUtf8(attribute->LocalName()), localName))
				continue;
			if (prefix && !Equals(FromUtf8(attribute->Prefix()), *prefix)) continue;
			return FromUtf8(attribute->Value());
		}
		return std::nullopt;
	}

	bool IsNamespaceAttribute(const XmlAttribute& attribute)
	{
		return Equals(attribute.Name(), "xmlns")
			|| Equals(attribute.Prefix(), "xmlns");
	}

	bool TryParseBool(const std::wstring& value, bool& output)
	{
		const auto normalized = Lower(Trim(value));
		if (normalized == L"true" || normalized == L"1"
			|| normalized == L"yes" || normalized == L"on")
		{
			output = true;
			return true;
		}
		if (normalized == L"false" || normalized == L"0"
			|| normalized == L"no" || normalized == L"off")
		{
			output = false;
			return true;
		}
		return false;
	}

	template<typename T>
	bool TryParseInteger(const std::wstring& value, T& output)
	{
		try
		{
			size_t consumed = 0;
			const auto parsed = std::stoll(Trim(value), &consumed, 10);
			if (consumed != Trim(value).size()
				|| parsed < static_cast<long long>((std::numeric_limits<T>::min)())
				|| parsed > static_cast<long long>((std::numeric_limits<T>::max)()))
				return false;
			output = static_cast<T>(parsed);
			return true;
		}
		catch (...)
		{
			return false;
		}
	}

	std::vector<std::wstring> Split(const std::wstring& value, wchar_t separator)
	{
		std::vector<std::wstring> result;
		size_t start = 0;
		while (start <= value.size())
		{
			const auto end = value.find(separator, start);
			result.push_back(Trim(value.substr(start,
				end == std::wstring::npos ? std::wstring::npos : end - start)));
			if (end == std::wstring::npos) break;
			start = end + 1;
		}
		return result;
	}

	std::wstring StripMarkupType(std::wstring value)
	{
		value = Trim(value);
		if (value.size() >= 2 && value.front() == L'{' && value.back() == L'}')
		{
			value = Trim(value.substr(1, value.size() - 2));
			if (Lower(value).starts_with(L"x:type"))
				value = Trim(value.substr(6));
		}
		const auto colon = value.find(L':');
		if (colon != std::wstring::npos) value = value.substr(colon + 1);
		return Trim(value);
	}

	std::wstring NormalizePropertyName(
		const std::wstring& rawName,
		const std::wstring& rawValue,
		bool formProperty = false)
	{
		const auto name = Lower(Trim(rawName));
		if (formProperty)
		{
			if (name == L"left") return L"X";
			if (name == L"top") return L"Y";
			if (name == L"isenabled" || name == L"enabled") return L"Enable";
			if (name == L"visibility" || name == L"isvisible") return L"Visible";
			return Trim(rawName);
		}

		if (name == L"x" || name == L"canvas.left") return L"Left";
		if (name == L"y" || name == L"canvas.top") return L"Top";
		if (name == L"width") return L"LayoutWidth";
		if (name == L"height") return L"LayoutHeight";
		if (name == L"isenabled" || name == L"enabled") return L"Enable";
		if (name == L"visibility" || name == L"isvisible") return L"Visible";
		if (name == L"ischecked") return L"Checked";
		if (name == L"horizontalalignment") return L"HAlign";
		if (name == L"verticalalignment") return L"VAlign";
		if (name == L"dock" || name == L"dockpanel.dock") return L"DockPosition";
		if (name == L"grid.row") return L"GridRow";
		if (name == L"grid.column") return L"GridColumn";
		if (name == L"grid.rowspan") return L"GridRowSpan";
		if (name == L"grid.columnspan") return L"GridColumnSpan";
		(void)rawValue;
		return Trim(rawName);
	}

	std::wstring NormalizeVisibility(const std::wstring& value, bool& recognized)
	{
		const auto normalized = Lower(Trim(value));
		if (normalized == L"visible")
		{
			recognized = true;
			return L"true";
		}
		if (normalized == L"hidden" || normalized == L"collapsed")
		{
			recognized = true;
			return L"false";
		}
		recognized = false;
		return value;
	}

	bool TryParseStaticResource(
		const std::wstring& value,
		std::wstring& resourceKey)
	{
		auto text = Trim(value);
		if (text.size() < 3 || text.front() != L'{' || text.back() != L'}')
			return false;
		text = Trim(text.substr(1, text.size() - 2));
		const auto lower = Lower(text);
		if (!lower.starts_with(L"staticresource")) return false;
		resourceKey = Trim(text.substr(14));
		return !resourceKey.empty();
	}

	bool TryParseBinding(
		const std::wstring& value,
		DesignerDataBinding& binding,
		std::wstring& error)
	{
		auto text = Trim(value);
		if (text.size() < 3 || text.front() != L'{' || text.back() != L'}')
			return false;
		text = Trim(text.substr(1, text.size() - 2));
		if (!Lower(text).starts_with(L"binding")) return false;
		text = Trim(text.substr(7));

		binding = {};
		bool positionalPathSeen = false;
		for (const auto& part : Split(text, L','))
		{
			if (part.empty()) continue;
			const auto equals = part.find(L'=');
			if (equals == std::wstring::npos)
			{
				if (positionalPathSeen)
				{
					error = L"Binding 只能包含一个位置路径。";
					return false;
				}
				binding.SourceProperty = part;
				positionalPathSeen = true;
				continue;
			}

			const auto key = Lower(Trim(part.substr(0, equals)));
			const auto itemValue = Trim(part.substr(equals + 1));
			if (key == L"path") binding.SourceProperty = itemValue;
			else if (key == L"mode")
			{
				if (!DesignerBindingUtils::TryParseBindingMode(itemValue, binding.Mode))
				{
					error = L"Binding Mode 无效：" + itemValue;
					return false;
				}
			}
			else if (key == L"updatesourcetrigger" || key == L"updatemode")
			{
				auto update = itemValue;
				if (Equals(update, L"PropertyChanged")) update = L"OnPropertyChanged";
				else if (Equals(update, L"LostFocus") || Equals(update, L"Validation"))
					update = L"OnValidation";
				else if (Equals(update, L"Explicit")) update = L"Never";
				if (!DesignerBindingUtils::TryParseUpdateMode(update, binding.UpdateMode))
				{
					error = L"Binding UpdateSourceTrigger 无效：" + itemValue;
					return false;
				}
			}
			else if (key == L"converter") binding.Converter = itemValue;
			else
			{
				error = L"Binding 包含不支持的参数：" + Trim(part.substr(0, equals));
				return false;
			}
		}

		binding.SourceProperty = DesignerBindingUtils::Trim(binding.SourceProperty);
		if (!DesignerBindingUtils::IsValidSourcePath(binding.SourceProperty))
		{
			error = L"Binding 源路径无效。";
			return false;
		}
		return true;
	}

	std::optional<DesignerEventDescriptor> FindEvent(
		UIClass type,
		const std::vector<DesignerCustomEventDescriptor>& customEvents,
		const std::wstring& rawName,
		const std::wstring& rawValue)
	{
		const auto trimmedValue = Lower(Trim(rawValue));
		if (trimmedValue.starts_with(L"{binding")) return std::nullopt;
		const auto events = DesignerEventCatalog::GetControlEvents(
			type, customEvents);
		for (const auto& event : events)
		{
			if (Equals(event.Name, rawName) || Equals(FromUtf8(event.EventField), rawName))
				return event;
		}

		bool booleanValue = false;
		if (Equals(rawName, L"Checked") && TryParseBool(rawValue, booleanValue))
			return std::nullopt;
		const std::map<std::wstring, std::wstring> aliases = {
			{ L"Click", L"OnMouseClick" },
			{ L"DoubleClick", L"OnMouseDoubleClick" },
			{ L"TextChanged", L"OnTextChanged" },
			{ L"Checked", L"OnChecked" },
			{ L"ValueChanged", L"OnValueChanged" },
			{ L"ExpandedChanged", L"OnExpandedChanged" },
			{ L"ItemClick", L"OnItemClick" },
			{ L"ItemDoubleClick", L"OnItemDoubleClick" },
		};
		for (const auto& [alias, canonical] : aliases)
		{
			if (!Equals(alias, rawName)) continue;
			for (const auto& event : events)
				if (Equals(event.Name, canonical)) return event;
		}
		if (Equals(rawName, L"SelectionChanged"))
		{
			for (const auto& event : events)
				if (Equals(event.Name, L"OnSelectionChanged")
					|| Equals(event.Name, L"SelectionChanged")) return event;
		}
		return std::nullopt;
	}

	std::optional<DesignerEventDescriptor> FindFormEvent(
		const std::wstring& rawName)
	{
		for (const auto& event : DesignerEventCatalog::GetFormEvents())
		{
			if (Equals(event.Name, rawName) || Equals(FromUtf8(event.EventField), rawName))
				return event;
		}
		const std::map<std::wstring, std::wstring> aliases = {
			{ L"Click", L"OnMouseClick" },
			{ L"DoubleClick", L"OnMouseDoubleClick" },
			{ L"TextChanged", L"OnTextChanged" },
			{ L"Closing", L"OnClose" },
			{ L"Closed", L"OnFormClosed" },
			{ L"Command", L"OnCommand" },
			{ L"ThemeChanged", L"OnThemeChanged" },
			{ L"Shown", L"OnShown" },
		};
		for (const auto& [alias, canonical] : aliases)
		{
			if (!Equals(alias, rawName)) continue;
			return DesignerEventCatalog::FindFormEvent(canonical);
		}
		return std::nullopt;
	}

	bool NormalizeHandler(
		const std::wstring& raw,
		std::wstring& stored,
		std::wstring& error)
	{
		stored = Trim(raw);
		if (Equals(stored, L"Auto") || DesignerEventCatalog::IsLegacyEnabledValue(stored))
		{
			stored = L"1";
			return true;
		}
		if (stored.empty())
		{
			error = L"事件处理函数名不能为空；需要默认名称时请使用 Auto。";
			return false;
		}
		return DesignerEventCatalog::ValidateHandlerName(stored, &error);
	}

	bool ParseAnchor(const std::wstring& value, int& output)
	{
		output = AnchorStyles::None;
		for (const auto& part : Split(Lower(value), L','))
		{
			if (part == L"none" || part.empty()) continue;
			if (part == L"left") output |= AnchorStyles::Left;
			else if (part == L"top") output |= AnchorStyles::Top;
			else if (part == L"right") output |= AnchorStyles::Right;
			else if (part == L"bottom") output |= AnchorStyles::Bottom;
			else return false;
		}
		return true;
	}

	DesignValue GridLengthValue(const std::wstring& raw, bool& valid)
	{
		auto value = Trim(raw);
		DesignValue result = DesignValue::object();
		if (Equals(value, L"Auto"))
		{
			result["value"] = 1.0;
			result["unit"] = "Auto";
			valid = true;
			return result;
		}
		if (!value.empty() && value.back() == L'*')
		{
			auto factor = Trim(value.substr(0, value.size() - 1));
			if (factor.empty()) factor = L"1";
			try
			{
				size_t consumed = 0;
				const double parsed = std::stod(factor, &consumed);
				valid = consumed == factor.size() && parsed >= 0.0;
				if (valid)
				{
					result["value"] = parsed;
					result["unit"] = "Star";
				}
				return result;
			}
			catch (...)
			{
				valid = false;
				return result;
			}
		}
		if (!value.empty() && value.back() == L'%')
		{
			auto factor = Trim(value.substr(0, value.size() - 1));
			try
			{
				size_t consumed = 0;
				const double parsed = std::stod(factor, &consumed);
				valid = consumed == factor.size() && parsed >= 0.0;
				if (valid)
				{
					result["value"] = parsed;
					result["unit"] = "Percent";
				}
				return result;
			}
			catch (...)
			{
				valid = false;
				return result;
			}
		}
		try
		{
			size_t consumed = 0;
			const double parsed = std::stod(value, &consumed);
			valid = consumed == value.size() && parsed >= 0.0;
			if (valid)
			{
				result["value"] = parsed;
				result["unit"] = "Pixel";
			}
		}
		catch (...)
		{
			valid = false;
		}
		return result;
	}

	bool ReadDesignValue(
		const Element& element,
		DesignValue& output,
		std::wstring& error)
	{
		if (!element)
		{
			output = nullptr;
			return true;
		}
		const auto type = Lower(FromUtf8(element->GetAttribute("type")));
		if (type.empty() || type == L"object")
		{
			DesignValue object = DesignValue::object();
			for (const auto& member : ChildElements(element))
			{
				if (!Equals(FromUtf8(member->LocalName()), L"Member"))
				{
					error = L"DesignValue Object 仅允许 Member 元素。";
					return false;
				}
				const auto name = member->GetAttribute("name");
				if (name.empty() || object.contains(name))
				{
					error = name.empty()
						? L"DesignValue Member 缺少 name。"
						: L"DesignValue Member 重复：" + FromUtf8(name);
					return false;
				}
				DesignValue value;
				if (!ReadDesignValue(member, value, error)) return false;
				object[name] = std::move(value);
			}
			output = std::move(object);
			return true;
		}
		if (type == L"array")
		{
			DesignValue array = DesignValue::array();
			for (const auto& item : ChildElements(element))
			{
				if (!Equals(FromUtf8(item->LocalName()), L"Item"))
				{
					error = L"DesignValue Array 仅允许 Item 元素。";
					return false;
				}
				DesignValue value;
				if (!ReadDesignValue(item, value, error)) return false;
				array.push_back(std::move(value));
			}
			output = std::move(array);
			return true;
		}
		if (type == L"null")
		{
			output = nullptr;
			return true;
		}
		const auto text = FromUtf8(element->InnerText());
		if (type == L"string")
		{
			output = ToUtf8(text);
			return true;
		}
		if (type == L"boolean")
		{
			bool value = false;
			if (!TryParseBool(text, value))
			{
				error = L"DesignValue Boolean 值无效。";
				return false;
			}
			output = value;
			return true;
		}
		try
		{
			const auto normalized = Trim(text);
			size_t consumed = 0;
			if (type == L"integer")
			{
				const auto value = std::stoll(normalized, &consumed, 10);
				if (consumed != normalized.size()) throw std::invalid_argument("integer");
				output = value;
				return true;
			}
			if (type == L"unsigned")
			{
				const auto value = std::stoull(normalized, &consumed, 10);
				if (consumed != normalized.size()) throw std::invalid_argument("unsigned");
				output = value;
				return true;
			}
			if (type == L"float")
			{
				const auto value = std::stod(normalized, &consumed);
				if (consumed != normalized.size()) throw std::invalid_argument("float");
				output = value;
				return true;
			}
		}
		catch (...)
		{
			error = L"DesignValue 数值无效。";
			return false;
		}
		error = L"DesignValue type 无效：" + type;
		return false;
	}

	bool MergeDesignObject(
		DesignValue& destination,
		const DesignValue& source,
		const std::wstring& bagName,
		std::wstring& error)
	{
		if (!source.is_object())
		{
			error = bagName + L" 必须是 Object。";
			return false;
		}
		if (!destination.is_object()) destination = DesignValue::object();
		for (const auto& [key, value] : source.ObjectItems())
		{
			if (key == "metadata" && destination.contains(key)
				&& destination[key].is_object() && value.is_object())
			{
				auto& targetMetadata = destination[key];
				for (const auto& [property, propertyValue] : value.ObjectItems())
				{
					if (targetMetadata.contains(property))
					{
						error = bagName + L" 的 metadata 属性重复："
							+ FromUtf8(property);
						return false;
					}
					targetMetadata[property] = propertyValue;
				}
				continue;
			}
			if (destination.contains(key))
			{
				error = bagName + L" 字段重复：" + FromUtf8(key);
				return false;
			}
			destination[key] = value;
		}
		return true;
	}

	class Parser final
	{
	public:
		Parser(DesignDocument& document, const XamlDocumentParseOptions& options)
			: _document(document), _options(options) {}

		bool Parse(const Element& root, std::wstring& error)
		{
			if (!root)
				return Fail(L"XAML 没有根元素。", error);
			const auto rootName = FromUtf8(root->LocalName());
			if (!Equals(rootName, L"Form") && !Equals(rootName, L"Window"))
				return Fail(L"XAML 根元素必须是 Form 或 Window。", error);

			// Property elements are order-independent: schema/resources first.
			for (const auto& child : ChildElements(root))
			{
				const auto name = FromUtf8(child->LocalName());
				if (IsRootPropertyElement(name, L"Resources")
					|| IsRootPropertyElement(name, L"Styles"))
				{
					if (!ParseResources(child, error)) return false;
				}
				else if (IsRootPropertyElement(name, L"DataContextSchema"))
				{
					if (!ParseDataContextSchema(child, error)) return false;
				}
			}

			if (!ParseFormAttributes(root, error)) return false;
			for (const auto& child : ChildElements(root))
			{
				const auto name = FromUtf8(child->LocalName());
				if (IsRootPropertyElement(name, L"Resources")
					|| IsRootPropertyElement(name, L"Styles")
					|| IsRootPropertyElement(name, L"DataContextSchema")) continue;
				if (name.find(L'.') != std::wstring::npos)
					return Fail(L"不支持的 Form 属性元素：" + name, error);
				if (!ParseControl(child, Parent{}, error)) return false;
			}

			MergeBindingSchema();
			DesignerDataContextSchemaUtils::Canonicalize(_document.DataContextSchema);
			DesignerStyleSheetUtils::Canonicalize(_document.StyleSheet);
			if (!DesignerDataContextSchemaUtils::Validate(
				_document.DataContextSchema, &error)) return false;
			if (!DesignerStyleSheetUtils::ValidateAgainstPropertyMetadata(
				_document.StyleSheet,
				DesignDocumentMaterializer::CreateRuntimeControl,
				&error)) return false;
			_document.RecalculateNextStableId();
			DesignDocumentGraph graph;
			if (!DesignDocumentGraph::Build(_document, graph, &error)) return false;
			DesignDocumentEventIndex eventIndex;
			if (!DesignDocumentEventIndex::Build(
				_document, eventIndex, &error)) return false;
			return true;
		}

	private:
		struct Parent
		{
			int Id = 0;
			std::wstring Ref;
		};

		DesignDocument& _document;
		const XamlDocumentParseOptions& _options;
		std::unordered_set<int> _usedIds;
		std::unordered_set<std::wstring> _usedNames;
		std::unordered_map<std::wstring, int> _nameCounters;
		std::vector<std::wstring> _bindingPaths;

		static bool Fail(std::wstring message, std::wstring& error)
		{
			error = std::move(message);
			return false;
		}

		static bool IsRootPropertyElement(
			const std::wstring& name,
			const std::wstring& property)
		{
			return Equals(name, property)
				|| Equals(name, L"Form." + property)
				|| Equals(name, L"Window." + property);
		}

		bool ValidateIdentifier(
			const std::wstring& value,
			const std::wstring& description,
			std::wstring& error)
		{
			if (value.empty()) return Fail(description + L"不能为空。", error);
			std::wstring validation;
			if (!DesignerEventCatalog::ValidateHandlerName(value, &validation))
				return Fail(description + L"无效：" + validation, error);
			return true;
		}

		bool ParseFormAttributes(const Element& root, std::wstring& error)
		{
			if (const auto name = Attribute(root, L"Name"))
				_document.Form.Name = Trim(*name);
			if (const auto xName = Attribute(root, L"Name", L"x"))
				_document.Form.Name = Trim(*xName);
			if (!ValidateIdentifier(_document.Form.Name, L"窗体名称", error)) return false;

			if (const auto className = Attribute(root, L"Class", L"x"))
				if (!DesignCodeBehindModel::TryNormalizeClassName(
					Trim(*className), _document.CodeBehind.ClassName, &error))
					return false;
			if (const auto relativePath = Attribute(root, L"CodeBehind", L"d"))
			{
				if (!DesignCodeBehindModel::TryNormalizeRelativeBasePath(
					Trim(*relativePath),
					_document.CodeBehind.RelativeBasePath, &error))
					return false;
			}
			if (!_document.CodeBehind.Validate(&error)) return false;

			for (const auto& attribute : root->Attributes())
			{
				if (!attribute || IsNamespaceAttribute(*attribute)) continue;
				const auto prefix = FromUtf8(attribute->Prefix());
				const auto name = FromUtf8(attribute->LocalName());
				const auto value = FromUtf8(attribute->Value());
				if (Equals(name, L"Name")
					|| (Equals(prefix, L"x") && Equals(name, L"Class"))
					|| (Equals(prefix, L"d") && Equals(name, L"CodeBehind")))
					continue;

				if (const auto event = FindFormEvent(name))
				{
					std::wstring handler;
					if (!NormalizeHandler(value, handler, error))
						return Fail(L"窗体事件 " + event->Name + L"：" + error, error);
					if (_document.Form.EventHandlers.contains(event->Name))
						return Fail(L"窗体事件重复：" + event->Name, error);
					_document.Form.EventHandlers[event->Name] = std::move(handler);
					continue;
				}

				auto propertyName = NormalizePropertyName(name, value, true);
				auto propertyValue = value;
				if (Equals(name, L"Visibility"))
				{
					bool recognized = false;
					propertyValue = NormalizeVisibility(value, recognized);
					if (!recognized) return Fail(L"Visibility 必须为 Visible、Hidden 或 Collapsed。", error);
				}
				const auto* descriptor = DesignerFormPropertyCatalog::Find(propertyName);
				if (!descriptor)
					return Fail(L"窗体不包含可持久化属性：" + name, error);
				DesignerStyleValue typed{ descriptor->ValueKind, propertyValue };
				std::wstring applyError;
				if (!DesignerFormPropertyCatalog::ApplyValue(
					_document.Form, descriptor->Name, typed, nullptr, &applyError))
					return Fail(L"窗体属性 " + name + L"：" + applyError, error);
			}
			return true;
		}

		bool ParseDataContextSchema(const Element& container, std::wstring& error)
		{
			for (const auto& item : ChildElements(container))
			{
				if (!Equals(FromUtf8(item->LocalName()), L"Property"))
					return Fail(L"DataContextSchema 仅支持 Property 元素。", error);
				DesignerDataContextProperty property;
				property.Path = Trim(Attribute(item, L"Path").value_or(L""));
				const auto kind = Attribute(item, L"Kind").value_or(L"Unknown");
				if (!DesignerDataContextSchemaUtils::TryParseValueKind(kind, property.ValueKind))
					return Fail(L"DataContext 属性类型无效：" + kind, error);
				for (const auto& [name, target] : {
					std::pair{ L"CanRead", &property.CanRead },
					std::pair{ L"CanWrite", &property.CanWrite },
					std::pair{ L"CanObserve", &property.CanObserve } })
				{
					if (const auto text = Attribute(item, name))
					{
						if (!TryParseBool(*text, *target))
							return Fail(L"DataContext 属性 " + property.Path
								+ L" 的 " + name + L" 必须为布尔值。", error);
					}
				}
				_document.DataContextSchema.push_back(std::move(property));
			}
			return true;
		}

		bool ParseCustomEvents(
			const Element& controlElement,
			DesignNode& node,
			std::wstring& error)
		{
			Element container;
			for (const auto& child : ChildElements(controlElement))
			{
				if (!Equals(FromUtf8(child->LocalName()), L"CustomEvents"))
					continue;
				if (!Equals(FromUtf8(child->Prefix()), L"d")
					&& !Equals(FromUtf8(child->NamespaceURI()),
						L"urn:cui:designer"))
					continue;
				if (container)
					return Fail(L"自定义事件契约容器重复。", error);
				container = child;
			}
			if (!container) return true;
			for (const auto& attribute : container->Attributes())
			{
				if (attribute && !IsNamespaceAttribute(*attribute))
					return Fail(L"d:CustomEvents 不支持属性。", error);
			}

			for (const auto& item : ChildElements(container))
			{
				if (!Equals(FromUtf8(item->LocalName()), L"Event")
					|| (!Equals(FromUtf8(item->Prefix()), L"d")
						&& !Equals(FromUtf8(item->NamespaceURI()),
							L"urn:cui:designer")))
					return Fail(L"d:CustomEvents 仅允许 d:Event。", error);
				if (!ChildElements(item).empty()
					|| !Trim(FromUtf8(item->InnerText())).empty())
					return Fail(L"d:Event 不允许包含内容。", error);
				for (const auto& attribute : item->Attributes())
				{
					if (!attribute || IsNamespaceAttribute(*attribute)) continue;
					const auto prefix = FromUtf8(attribute->Prefix());
					const auto name = FromUtf8(attribute->LocalName());
					if (!prefix.empty()
						|| (name != L"Name" && name != L"DisplayName"
							&& name != L"Field" && name != L"Category"
							&& name != L"Signature" && name != L"Order"
							&& name != L"Default"))
						return Fail(L"d:Event 包含不支持的属性：" + name, error);
				}
				DesignerCustomEventDescriptor contract;
				contract.Name = Trim(Attribute(item, L"Name").value_or(L""));
				contract.DisplayName = Trim(
					Attribute(item, L"DisplayName").value_or(contract.Name));
				const auto field = Trim(
					Attribute(item, L"Field").value_or(L""));
				const auto category = Trim(
					Attribute(item, L"Category").value_or(L"Other"));
				const auto signature = Trim(
					Attribute(item, L"Signature").value_or(L""));
				const auto order = Trim(
					Attribute(item, L"Order").value_or(L"0"));
				const auto isDefault = Trim(
					Attribute(item, L"Default").value_or(L"false"));
				if (contract.Name.empty() || field.empty()
					|| !DesignerEventCatalog::TryParseCategory(
						category, contract.Category)
					|| !DesignerEventCatalog::TryParseCustomSignature(
						signature, contract.Signature)
					|| !TryParseInteger(order, contract.Order)
					|| !TryParseBool(isDefault, contract.IsDefault))
					return Fail(L"自定义事件契约无效：" + contract.Name, error);
				if (!std::all_of(field.begin(), field.end(), [](wchar_t ch)
					{ return ch >= 0 && ch <= 0x7f; }))
					return Fail(L"自定义事件 Field 必须是 ASCII 标识符。", error);
				contract.EventField.reserve(field.size());
				for (const auto ch : field)
					contract.EventField.push_back(static_cast<char>(ch));
				node.CustomEvents.push_back(std::move(contract));
			}
			std::wstring validationError;
			if (!DesignerEventCatalog::ValidateCustomEvents(
				node.Type, node.CustomEvents, &validationError))
				return Fail(L"自定义事件契约无效：" + validationError, error);
			return true;
		}

		bool ParseResources(const Element& container, std::wstring& error)
		{
			for (const auto& item : ChildElements(container))
			{
				const auto name = FromUtf8(item->LocalName());
				if (Equals(name, L"Style"))
				{
					if (!ParseStyle(item, error)) return false;
					continue;
				}

				DesignerStyleResource resource;
				resource.Key = Trim(Attribute(item, L"Key", L"x").value_or(
					Attribute(item, L"Key").value_or(L"")));
				if (resource.Key.empty())
					return Fail(L"样式资源缺少 x:Key。", error);

				DesignerStyleValueKind kind;
				if (Equals(name, L"Resource"))
				{
					const auto kindName = Attribute(item, L"Kind").value_or(L"String");
					if (!DesignerStyleSheetUtils::TryParseValueKind(kindName, kind))
						return Fail(L"样式资源类型无效：" + kindName, error);
				}
				else if (!DesignerStyleSheetUtils::TryParseValueKind(name, kind))
				{
					return Fail(L"不支持的样式资源元素：" + name, error);
				}
				resource.Value.Kind = kind;
				resource.Value.Text = Attribute(item, L"Value").value_or(FromUtf8(item->InnerText()));
				BindingValue ignored;
				std::wstring conversionError;
				if (!DesignerStyleSheetUtils::TryConvertValue(
					resource.Value, ignored, &conversionError))
					return Fail(L"样式资源 " + resource.Key + L"：" + conversionError, error);
				_document.StyleSheet.Resources.push_back(std::move(resource));
			}
			return true;
		}

		bool ParseStyle(const Element& element, std::wstring& error)
		{
			DesignerStyleRule rule;
			if (const auto target = Attribute(element, L"TargetType"))
			{
				const auto typeName = StripMarkupType(*target);
				if (!Equals(typeName, L"Any"))
				{
					if (!TryParseType(typeName, rule.Type))
						return Fail(L"Style TargetType 无效：" + typeName, error);
					rule.HasType = true;
				}
			}
			rule.Id = Trim(Attribute(element, L"Id").value_or(
				Attribute(element, L"Key", L"x").value_or(L"")));
			rule.Classes = DesignerStyleSheetUtils::SplitClasses(
				Attribute(element, L"Classes").value_or(
					Attribute(element, L"Class").value_or(L"")));
			if (!DesignerStyleSheetUtils::TryParseStates(
				Attribute(element, L"RequiredStates").value_or(L""), rule.RequiredStates)
				|| !DesignerStyleSheetUtils::TryParseStates(
					Attribute(element, L"ExcludedStates").value_or(L""), rule.ExcludedStates))
				return Fail(L"Style 状态选择器无效。", error);

			auto probe = DesignDocumentMaterializer::CreateRuntimeControl(
				rule.HasType ? rule.Type : UIClass::UI_Base);
			if (!probe) return Fail(L"Style TargetType 尚无运行时控件工厂。", error);
			for (const auto& child : ChildElements(element))
			{
				if (!Equals(FromUtf8(child->LocalName()), L"Setter"))
					return Fail(L"Style 仅支持 Setter 子元素。", error);
				const auto rawProperty = Trim(Attribute(child, L"Property").value_or(L""));
				auto rawValue = Attribute(child, L"Value").value_or(FromUtf8(child->InnerText()));
				if (rawProperty.empty()) return Fail(L"Setter 缺少 Property。", error);
				const auto propertyName = NormalizePropertyName(rawProperty, rawValue);
				const auto properties = DesignerPropertyCatalog::GetStyleProperties(*probe);
				const auto* descriptor = DesignerPropertyCatalog::Find(properties, propertyName);
				if (!descriptor)
					return Fail(L"Style 目标类型不包含属性：" + rawProperty, error);

				DesignerStyleSetter setter;
				setter.PropertyName = descriptor->Name;
				setter.ResourceKey = Trim(Attribute(child, L"Resource").value_or(L""));
				if (setter.ResourceKey.empty())
					(void)TryParseStaticResource(rawValue, setter.ResourceKey);
				setter.UsesResource = !setter.ResourceKey.empty();
				if (!setter.UsesResource)
				{
					setter.Literal.Kind = descriptor->ValueKind;
					if (const auto kindName = Attribute(child, L"Kind"))
					{
						if (!DesignerStyleSheetUtils::TryParseValueKind(*kindName, setter.Literal.Kind))
							return Fail(L"Setter Kind 无效：" + *kindName, error);
					}
					setter.Literal.Text = NormalizePropertyText(rawProperty, rawValue, *descriptor);
					std::wstring validationError;
					if (!DesignerPropertyCatalog::ValidateStyleValue(
						*probe, setter.PropertyName, setter.Literal, &validationError))
						return Fail(L"Setter " + rawProperty + L"：" + validationError, error);
				}
				rule.Setters.push_back(std::move(setter));
			}
			_document.StyleSheet.Rules.push_back(std::move(rule));
			return true;
		}

		bool TryParseType(std::wstring typeName, UIClass& type) const
		{
			typeName = StripMarkupType(std::move(typeName));
			if (Equals(typeName, L"Grid")) typeName = L"GridPanel";
			else if (Equals(typeName, L"TextBlock")) typeName = L"Label";
			else if (Equals(typeName, L"RadioButton")) typeName = L"RadioBox";
			else if (Equals(typeName, L"Image")) typeName = L"PictureBox";
			return DesignerStyleSheetUtils::TryParseUIClass(typeName, type);
		}

		std::wstring MakeControlName(UIClass type)
		{
			auto stem = DesignerStyleSheetUtils::UIClassName(type);
			if (!stem.empty()) stem.front() = static_cast<wchar_t>(std::towlower(stem.front()));
			auto& next = _nameCounters[Lower(stem)];
			for (;;)
			{
				const auto candidate = stem + std::to_wstring(++next);
				if (!_usedNames.contains(Lower(candidate))) return candidate;
			}
		}

		bool ReadControlIdentity(
			const Element& element,
			UIClass type,
			std::wstring& name,
			int& id,
			std::wstring& error)
		{
			name = Trim(Attribute(element, L"Name", L"x").value_or(
				Attribute(element, L"Name").value_or(L"")));
			if (name.empty()) name = MakeControlName(type);
			if (!ValidateIdentifier(name, L"控件名称", error)) return false;
			if (!_usedNames.insert(Lower(name)).second)
				return Fail(L"控件名称重复：" + name, error);

			const auto idText = Attribute(element, L"DesignId").value_or(
				Attribute(element, L"Uid", L"x").value_or(L""));
			if (!idText.empty())
			{
				if (!TryParseInteger(idText, id) || id <= 0)
					return Fail(L"控件 " + name + L" 的 DesignId 必须是正整数。", error);
			}
			else
			{
				do { id = _document.AllocateNodeId(); }
				while (_usedIds.contains(id));
			}
			if (!_usedIds.insert(id).second)
				return Fail(L"控件稳定 ID 重复：" + std::to_wstring(id), error);
			if (id >= _document.NextStableId)
			{
				if (id == (std::numeric_limits<int>::max)())
					return Fail(L"控件稳定 ID 已耗尽。", error);
				_document.NextStableId = id + 1;
			}
			return true;
		}

		bool ParseControl(
			const Element& element,
			const Parent& parent,
			std::wstring& error,
			const std::string& forcedSplitRegion = {})
		{
			const auto elementName = FromUtf8(element->LocalName());
			UIClass type = UIClass::UI_Base;
			DesignerCustomControlType customType;
			const auto elementNamespace = FromUtf8(element->NamespaceURI());
			const bool builtInType = TryParseType(elementName, type)
				&& (elementNamespace.empty()
					|| Equals(elementNamespace, L"urn:cui"));
			if (!builtInType)
			{
				customType.XamlPrefix = FromUtf8(element->Prefix());
				customType.XamlName = elementName;
				customType.XamlNamespace = elementNamespace;
				customType.CppType = Trim(
					Attribute(element, L"CppType", L"d").value_or(L""));
				customType.Header = Trim(
					Attribute(element, L"Header", L"d").value_or(L""));
				const auto baseType = Attribute(
					element, L"BaseType", L"d").value_or(L"Control");
				if (customType.XamlPrefix.empty()
					|| customType.XamlNamespace.empty()
					|| customType.CppType.empty() || customType.Header.empty())
					return Fail(L"自定义控件 " + elementName
						+ L" 需要前缀命名空间、d:CppType 和 d:Header。", error);
				if (!TryParseType(baseType, type) || type == UIClass::UI_TabPage)
					return Fail(L"自定义控件 " + elementName
						+ L" 的 d:BaseType 无效：" + baseType, error);
				std::wstring normalizedCppType;
				if (!DesignCodeBehindModel::TryNormalizeClassName(
					customType.CppType, normalizedCppType, &error)
					|| normalizedCppType.empty())
					return Fail(L"自定义控件 " + elementName
						+ L" 的 d:CppType 无效：" + error, error);
				customType.CppType = std::move(normalizedCppType);
				if (customType.Header.find(L"..") != std::wstring::npos
					|| customType.Header.front() == L'/'
					|| customType.Header.front() == L'\\'
					|| customType.Header.find(L':') != std::wstring::npos
					|| customType.Header.find(L'\"') != std::wstring::npos
					|| customType.Header.find_first_of(L"\r\n") != std::wstring::npos)
					return Fail(L"自定义控件 " + elementName
						+ L" 的 d:Header 必须是安全的相对包含路径。", error);
				const auto constructor = Lower(Trim(Attribute(
					element, L"Constructor", L"d").value_or(L"Bounds")));
				if (constructor == L"default")
					customType.Constructor = DesignerCustomControlConstructor::Default;
				else if (constructor == L"bounds")
					customType.Constructor = DesignerCustomControlConstructor::Bounds;
				else if (constructor == L"textbounds")
					customType.Constructor = DesignerCustomControlConstructor::TextBounds;
				else return Fail(L"自定义控件 " + elementName
					+ L" 的 d:Constructor 必须是 Default、Bounds 或 TextBounds。", error);
			}
			else if (type == UIClass::UI_TabPage)
				return Fail(L"不支持的控件元素：" + elementName, error);
			DesignNode node;
			if (!ReadControlIdentity(element, type, node.Name, node.Id, error)) return false;
			node.Type = type;
			node.CustomType = std::move(customType);
			if (!ParseCustomEvents(element, node, error)) return false;
			node.ParentId = parent.Id;
			node.ParentRef = parent.Ref;
			node.Order = SiblingCount(parent);
			if (!forcedSplitRegion.empty()) node.Extra["splitRegion"] = forcedSplitRegion;
			std::unique_ptr<Control> probe;
			if (!node.CustomType.Empty() && _options.CustomControlFactory)
			{
				probe = _options.CustomControlFactory(node);
				if (!probe)
					return Fail(L"自定义控件尚未注册运行时工厂："
						+ node.CustomType.XamlName, error);
				if (probe->Type() != node.Type)
					return Fail(L"自定义控件工厂返回的 Type() 与 d:BaseType 不一致："
						+ node.CustomType.XamlName, error);
			}
			else probe = DesignDocumentMaterializer::CreateRuntimeControl(type);
			if (!probe)
				return Fail(L"控件类型尚无运行时工厂：" + elementName, error);
			_document.Nodes.push_back(std::move(node));
			const size_t nodeIndex = _document.Nodes.size() - 1;

			if (!ParseControlAttributes(element, nodeIndex, *probe, error)) return false;
			if (!ApplyDirectText(element, nodeIndex, *probe, error)) return false;

			const Parent childParent{
				_document.Nodes[nodeIndex].Id,
				_document.Nodes[nodeIndex].Name };
			for (const auto& child : ChildElements(element))
			{
				const auto childName = FromUtf8(child->LocalName());
				if (Equals(childName, L"DesignProps")
					|| Equals(childName, L"DesignExtra")
					|| Equals(childName, L"DesignBindings"))
				{
					DesignValue bag;
					if (!ReadDesignValue(child, bag, error)) return false;
					auto& destination = Equals(childName, L"DesignProps")
						? _document.Nodes[nodeIndex].Props
						: (Equals(childName, L"DesignBindings")
							? _document.Nodes[nodeIndex].Bindings
							: _document.Nodes[nodeIndex].Extra);
					if (!MergeDesignObject(
						destination, bag, childName, error)) return false;
					continue;
				}
				if (Equals(childName, L"CustomEvents")
					&& (Equals(FromUtf8(child->Prefix()), L"d")
						|| Equals(FromUtf8(child->NamespaceURI()),
							L"urn:cui:designer")))
					continue;
				if (type == UIClass::UI_GridPanel
					&& (Equals(childName, L"GridPanel.RowDefinitions")
						|| Equals(childName, L"Grid.RowDefinitions")
						|| Equals(childName, L"RowDefinitions")))
				{
					if (!ParseGridDefinitions(child, nodeIndex, true, error)) return false;
					continue;
				}
				if (type == UIClass::UI_GridPanel
					&& (Equals(childName, L"GridPanel.ColumnDefinitions")
						|| Equals(childName, L"Grid.ColumnDefinitions")
						|| Equals(childName, L"ColumnDefinitions")))
				{
					if (!ParseGridDefinitions(child, nodeIndex, false, error)) return false;
					continue;
				}
				if (type == UIClass::UI_TabControl && Equals(childName, L"TabPage"))
				{
					if (!ParseTabPage(child, nodeIndex, error)) return false;
					continue;
				}
				if (type == UIClass::UI_SplitContainer
					&& (Equals(childName, L"SplitContainer.FirstPanel")
						|| Equals(childName, L"SplitContainer.SecondPanel")))
				{
					const auto region = Equals(childName, L"SplitContainer.SecondPanel")
						? std::string("panel2") : std::string("panel1");
					for (const auto& grandChild : ChildElements(child))
						if (!ParseControl(grandChild, childParent, error, region)) return false;
					continue;
				}
				if (childName.find(L'.') != std::wstring::npos)
					return Fail(L"不支持的控件属性元素：" + childName, error);
				if (!ParseControl(child, childParent, error)) return false;
			}
			return true;
		}

		int SiblingCount(const Parent& parent) const
		{
			return static_cast<int>(std::count_if(
				_document.Nodes.begin(), _document.Nodes.end(), [&](const DesignNode& item)
				{
					return item.ParentId == parent.Id && item.ParentRef == parent.Ref;
				}));
		}

		bool ParseControlAttributes(
			const Element& element,
			size_t nodeIndex,
			Control& probe,
			std::wstring& error)
		{
			std::unordered_set<std::wstring> assignedProperties;
			std::unordered_set<std::wstring> projectedProperties;
			for (const auto& name : Split(
				Attribute(element, L"ProjectedProperties", L"d").value_or(L""), L','))
				if (!name.empty()) projectedProperties.insert(Lower(name));
			for (const auto& attribute : element->Attributes())
			{
				if (!attribute || IsNamespaceAttribute(*attribute)) continue;
				const auto prefix = FromUtf8(attribute->Prefix());
				const auto name = FromUtf8(attribute->LocalName());
				const auto rawName = FromUtf8(attribute->Name());
				const auto value = FromUtf8(attribute->Value());
				if (Equals(prefix, L"d") && Equals(name, L"ProjectedProperties"))
					continue;
				if (Equals(prefix, L"d")
					&& (Equals(name, L"CppType") || Equals(name, L"Header")
						|| Equals(name, L"BaseType") || Equals(name, L"Constructor")))
					continue;
				if (projectedProperties.contains(Lower(rawName))) continue;
				if (Equals(name, L"Name") || Equals(name, L"DesignId")
					|| (Equals(prefix, L"x") && Equals(name, L"Uid"))) continue;

				if (const auto event = FindEvent(
					_document.Nodes[nodeIndex].Type,
					_document.Nodes[nodeIndex].CustomEvents,
					name, value))
				{
					std::wstring handler;
					if (!NormalizeHandler(value, handler, error))
						return Fail(L"控件 " + _document.Nodes[nodeIndex].Name
							+ L" 的事件 " + event->Name + L"：" + error, error);
					auto& events = _document.Nodes[nodeIndex].Events;
					const auto key = ToUtf8(event->Name);
					if (events.contains(key)) return Fail(L"事件重复：" + event->Name, error);
					events[key] = ToUtf8(handler);
					continue;
				}

				if (Equals(name, L"StyleId"))
				{
					_document.Nodes[nodeIndex].Props["styleId"] = ToUtf8(Trim(value));
					continue;
				}
				if (Equals(name, L"Style"))
				{
					std::wstring styleKey;
					if (!TryParseStaticResource(value, styleKey))
						return Fail(L"Style 属性必须使用 {StaticResource key}。", error);
					_document.Nodes[nodeIndex].Props["styleId"] = ToUtf8(styleKey);
					continue;
				}
				if ((Equals(name, L"Class") && !Equals(prefix, L"x")) || Equals(name, L"Classes"))
				{
					DesignValue classes = DesignValue::array();
					for (const auto& item : DesignerStyleSheetUtils::SplitClasses(value))
						classes.push_back(ToUtf8(item));
					_document.Nodes[nodeIndex].Props["styleClasses"] = std::move(classes);
					continue;
				}
				if (Equals(rawName, L"SplitContainer.Region") || Equals(name, L"SplitContainer.Region"))
				{
					if (Equals(value, L"First") || Equals(value, L"FirstPanel") || Equals(value, L"Panel1"))
						_document.Nodes[nodeIndex].Extra["splitRegion"] = "panel1";
					else if (Equals(value, L"Second") || Equals(value, L"SecondPanel") || Equals(value, L"Panel2"))
						_document.Nodes[nodeIndex].Extra["splitRegion"] = "panel2";
					else return Fail(L"SplitContainer.Region 必须为 First 或 Second。", error);
					continue;
				}
				if (Equals(name, L"Anchor"))
				{
					int anchors = 0;
					if (!ParseAnchor(value, anchors)) return Fail(L"Anchor 值无效：" + value, error);
					_document.Nodes[nodeIndex].Props["anchor"] = anchors;
					continue;
				}
				if (Equals(name, L"FontName") || Equals(name, L"FontSize"))
				{
					auto& font = _document.Nodes[nodeIndex].Props["font"];
					if (!font.is_object()) font = DesignValue::object();
					if (Equals(name, L"FontName")) font["name"] = ToUtf8(value);
					else
					{
						try
						{
							size_t consumed = 0;
							const auto size = std::stod(Trim(value), &consumed);
							if (consumed != Trim(value).size() || size < 1.0 || size > 200.0)
								return Fail(L"FontSize 必须介于 1 与 200 之间。", error);
							font["size"] = size;
						}
						catch (...) { return Fail(L"FontSize 必须是数值。", error); }
					}
					continue;
				}

				auto propertyName = NormalizePropertyName(name, value);
				auto propertyValue = value;

				DesignerDataBinding binding;
				std::wstring bindingError;
				if (TryParseBinding(propertyValue, binding, bindingError))
				{
					const auto* metadata = probe.FindPropertyMetadata(propertyName);
					if (!metadata)
						return Fail(L"绑定目标属性不存在：" + name, error);
					if (!assignedProperties.insert(Lower(metadata->Name())).second)
						return Fail(L"属性重复：" + metadata->Name(), error);
					DesignValue persisted{
						{ "source", ToUtf8(binding.SourceProperty) },
						{ "mode", static_cast<int>(binding.Mode) },
						{ "updateMode", static_cast<int>(binding.UpdateMode) }
					};
					if (!binding.Converter.empty()) persisted["converter"] = ToUtf8(binding.Converter);
					_document.Nodes[nodeIndex].Bindings[ToUtf8(metadata->Name())] = std::move(persisted);
					_bindingPaths.push_back(binding.SourceProperty);
					continue;
				}
				if (!bindingError.empty())
					return Fail(L"控件 " + _document.Nodes[nodeIndex].Name
						+ L" 的属性 " + name + L"：" + bindingError, error);
				if (Equals(name, L"Visibility"))
				{
					bool recognized = false;
					propertyValue = NormalizeVisibility(value, recognized);
					if (!recognized) return Fail(L"Visibility 必须为 Visible、Hidden 或 Collapsed。", error);
				}

				const auto properties = DesignerPropertyCatalog::GetStyleProperties(probe);
				const auto* descriptor = DesignerPropertyCatalog::Find(properties, propertyName);
				if (!descriptor)
					return Fail(L"控件 " + _document.Nodes[nodeIndex].Name
						+ L" 不包含可持久化属性：" + name, error);
				if (!assignedProperties.insert(Lower(descriptor->Name)).second)
					return Fail(L"属性重复：" + descriptor->Name, error);
				DesignerStyleValue typed{
					descriptor->ValueKind,
					NormalizePropertyText(name, propertyValue, *descriptor) };
				std::wstring canonical;
				DesignerStyleValue effective;
				std::wstring applyError;
				if (!DesignerPropertyCatalog::ApplyValue(
					probe, descriptor->Name, typed, &canonical, &effective, &applyError))
					return Fail(L"控件 " + _document.Nodes[nodeIndex].Name
						+ L" 的属性 " + name + L"：" + applyError, error);
				StoreMetadata(_document.Nodes[nodeIndex], canonical, effective);
			}
			return true;
		}

		static std::wstring NormalizePropertyText(
			const std::wstring& rawName,
			const std::wstring& rawValue,
			const DesignerPropertyDescriptor& descriptor)
		{
			if (Equals(rawName, L"Visibility"))
			{
				bool recognized = false;
				return NormalizeVisibility(rawValue, recognized);
			}
			for (const auto& choice : descriptor.Choices)
			{
				if (Equals(choice.DisplayName, Trim(rawValue))
					|| Equals(choice.ValueText, Trim(rawValue)))
					return choice.ValueText;
			}
			return rawValue;
		}

		static void StoreMetadata(
			DesignNode& node,
			const std::wstring& propertyName,
			const DesignerStyleValue& value)
		{
			auto& metadata = node.Props["metadata"];
			if (!metadata.is_object()) metadata = DesignValue::object();
			metadata[ToUtf8(propertyName)] = DesignValue{
				{ "kind", ToUtf8(DesignerStyleSheetUtils::ValueKindName(value.Kind)) },
				{ "value", ToUtf8(value.Text) }
			};
		}

		bool ApplyDirectText(
			const Element& element,
			size_t nodeIndex,
			Control& probe,
			std::wstring& error)
		{
			std::wstring text;
			for (const auto& child : element->ChildNodes())
			{
				if (!child) continue;
				if (child->NodeType() == XmlNodeType::Text
					|| child->NodeType() == XmlNodeType::CDATA)
					text += FromUtf8(child->Value());
			}
			text = Trim(text);
			if (text.empty()) return true;
			if (_document.Nodes[nodeIndex].Bindings.contains("Text")) return true;
			if (_document.Nodes[nodeIndex].Props.contains("metadata")
				&& _document.Nodes[nodeIndex].Props["metadata"].contains("Text")) return true;

			const auto properties = DesignerPropertyCatalog::GetStyleProperties(probe);
			const auto* descriptor = DesignerPropertyCatalog::Find(properties, L"Text");
			if (!descriptor) return Fail(L"该控件不支持文本内容。", error);
			DesignerStyleValue effective;
			std::wstring canonical;
			std::wstring applyError;
			if (!DesignerPropertyCatalog::ApplyValue(
				probe, descriptor->Name,
				{ descriptor->ValueKind, text }, &canonical, &effective, &applyError))
				return Fail(applyError, error);
			StoreMetadata(_document.Nodes[nodeIndex], canonical, effective);
			return true;
		}

		bool ParseGridDefinitions(
			const Element& container,
			size_t gridIndex,
			bool rows,
			std::wstring& error)
		{
			DesignValue definitions = DesignValue::array();
			for (const auto& item : ChildElements(container))
			{
				const auto expected = rows ? L"RowDefinition" : L"ColumnDefinition";
				if (!Equals(FromUtf8(item->LocalName()), expected))
					return Fail(std::wstring(L"网格定义仅支持 ") + expected + L"。", error);
				const auto lengthName = rows ? L"Height" : L"Width";
				bool valid = false;
				auto length = GridLengthValue(
					Attribute(item, lengthName).value_or(L"Auto"), valid);
				if (!valid) return Fail(std::wstring(lengthName) + L" 网格长度无效。", error);
				DesignValue definition = DesignValue::object();
				definition[rows ? "height" : "width"] = std::move(length);
				for (const auto& [attributeName, key] : {
					std::pair{ rows ? L"MinHeight" : L"MinWidth", "min" },
					std::pair{ rows ? L"MaxHeight" : L"MaxWidth", "max" } })
				{
					if (const auto text = Attribute(item, attributeName))
					{
						try
						{
							size_t consumed = 0;
							const auto parsed = std::stod(Trim(*text), &consumed);
							if (consumed != Trim(*text).size() || parsed < 0.0)
								return Fail(std::wstring(attributeName) + L" 必须是非负数。", error);
							definition[key] = parsed;
						}
						catch (...) { return Fail(std::wstring(attributeName) + L" 必须是数值。", error); }
					}
				}
				definitions.push_back(std::move(definition));
			}
			_document.Nodes[gridIndex].Extra[rows ? "rows" : "columns"] = std::move(definitions);
			return true;
		}

		bool ParseTabPage(
			const Element& page,
			size_t tabIndex,
			std::wstring& error)
		{
			auto& extra = _document.Nodes[tabIndex].Extra;
			auto& pages = extra["pages"];
			if (!pages.is_array()) pages = DesignValue::array();
			const auto pageIndex = pages.size();
			const auto generatedPageId = _document.Nodes[tabIndex].Name
				+ L"#page" + std::to_wstring(pageIndex);
			const auto pageId = Trim(Attribute(page, L"DesignKey", L"d")
				.value_or(generatedPageId));
			if (!pageId.starts_with(_document.Nodes[tabIndex].Name + L"#page"))
				return Fail(L"TabPage DesignKey 必须属于当前 TabControl。", error);
			const auto text = Attribute(page, L"Header").value_or(
				Attribute(page, L"Text").value_or(L"Page"));
			pages.push_back(DesignValue{
				{ "id", ToUtf8(pageId) }, { "text", ToUtf8(text) } });

			for (const auto& attribute : page->Attributes())
			{
				if (!attribute || IsNamespaceAttribute(*attribute)) continue;
				const auto name = FromUtf8(attribute->LocalName());
				const auto prefix = FromUtf8(attribute->Prefix());
				if (!Equals(name, L"Name") && !Equals(name, L"Header")
					&& !Equals(name, L"Text")
					&& !(Equals(name, L"DesignKey") && Equals(prefix, L"d")))
					return Fail(L"TabPage 尚不支持属性：" + name, error);
			}
			for (const auto& child : ChildElements(page))
				if (!ParseControl(child, Parent{ 0, pageId }, error)) return false;
			return true;
		}

		void MergeBindingSchema()
		{
			for (const auto& path : _bindingPaths)
			{
				auto normalized = DesignerDataContextSchemaUtils::NormalizePath(path);
				size_t separator = 0;
				while (separator != std::wstring::npos)
				{
					separator = normalized.find(L'.', separator);
					const auto prefix = separator == std::wstring::npos
						? normalized : normalized.substr(0, separator);
					if (!DesignerDataContextSchemaUtils::Find(_document.DataContextSchema, prefix))
						_document.DataContextSchema.push_back({
							prefix, BindingValueKind::Empty, true, true, true });
					if (separator != std::wstring::npos) ++separator;
				}
			}
		}
	};
}

bool XamlDocumentParser::FromXaml(
	const std::string& xaml,
	DesignDocument& output,
	std::wstring* outError)

{
	return FromXaml(xaml, output, XamlDocumentParseOptions{}, outError);
}

bool XamlDocumentParser::FromXaml(
	const std::string& xaml,
	DesignDocument& output,
	const XamlDocumentParseOptions& options,
	std::wstring* outError)
{
	try
	{
		XmlDocument xml;
		xml.LoadXml(xaml);
		DesignDocument candidate;
		Parser parser(candidate, options);
		std::wstring error;
		if (!parser.Parse(xml.DocumentElement(), error))
		{
			if (outError) *outError = std::move(error);
			return false;
		}
		output = std::move(candidate);
		if (outError) outError->clear();
		return true;
	}
	catch (const std::exception& exception)
	{
		if (outError) *outError = L"XAML 解析失败：" + FromUtf8(exception.what());
		return false;
	}
	catch (...)
	{
		if (outError) *outError = L"XAML 解析失败：发生未知异常。";
		return false;
	}
}

bool XamlDocumentParser::LoadFromFile(
	const std::wstring& filePath,
	DesignDocument& output,
	std::wstring* outError)

{
	return LoadFromFile(
		filePath, output, XamlDocumentParseOptions{}, outError);
}

bool XamlDocumentParser::LoadFromFile(
	const std::wstring& filePath,
	DesignDocument& output,
	const XamlDocumentParseOptions& options,
	std::wstring* outError)
{
	try
	{
		std::ifstream stream(std::filesystem::path(filePath), std::ios::binary);
		if (!stream)
		{
			if (outError) *outError = L"无法打开 XAML 文件：" + filePath;
			return false;
		}
		std::ostringstream buffer;
		buffer << stream.rdbuf();
		if (!stream.good() && !stream.eof())
		{
			if (outError) *outError = L"读取 XAML 文件失败：" + filePath;
			return false;
		}
		return FromXaml(buffer.str(), output, options, outError);
	}
	catch (const std::exception&)
	{
		if (outError) *outError = L"读取 XAML 文件时发生异常：" + filePath;
		return false;
	}
	catch (...)
	{
		if (outError) *outError = L"读取 XAML 文件时发生未知异常。";
		return false;
	}
}
}
