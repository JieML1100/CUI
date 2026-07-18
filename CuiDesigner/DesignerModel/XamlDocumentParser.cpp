#include "XamlDocumentParser.h"

#include "DesignDocumentGraph.h"
#include "DesignDocumentEventIndex.h"
#include "DesignDocumentMaterializer.h"
#include "XamlSourceScanner.h"
#include "../../XmlLite/include/Xml.h"
#include "../DesignerBindingUtils.h"
#include "../DesignerDataContextSchemaUtils.h"
#include "../DesignerEventCatalog.h"
#include "../DesignerFormPropertyCatalog.h"
#include "../DesignerPropertyCatalog.h"
#include "../DesignerStyleSheetUtils.h"

#include <Convert.h>
#include <Application.h>

#include <algorithm>
#include <cmath>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <initializer_list>
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

	void ResetDiagnostic(XamlDocumentDiagnostic* diagnostic)
	{
		if (diagnostic) *diagnostic = {};
	}

	void ReportFailure(
		const std::wstring& message,
		std::wstring* outError,
		XamlDocumentDiagnostic* diagnostic)
	{
		if (outError) *outError = message;
		if (diagnostic) diagnostic->Message = message;
	}

	std::string XmlExceptionMessageWithoutLocation(
		const System::Xml::XmlException& exception)
	{
		std::string message = exception.what();
		if (exception.Line() == 0 || exception.Column() == 0)
			return message;
		const std::string suffix = " Line " + std::to_string(exception.Line())
			+ ", position " + std::to_string(exception.Column()) + ".";
		if (message.size() >= suffix.size()
			&& message.compare(message.size() - suffix.size(), suffix.size(), suffix) == 0)
			message.erase(message.size() - suffix.size());
		return message;
	}

	void PopulateXmlLocation(
		const std::string& xaml,
		const System::Xml::XmlException& exception,
		XamlDocumentDiagnostic* diagnostic)
	{
		if (!diagnostic || exception.Line() == 0 || exception.Column() == 0)
			return;

		std::size_t lineStart = 0;
		for (std::size_t line = 1; line < exception.Line(); ++line)
		{
			const auto newline = xaml.find('\n', lineStart);
			if (newline == std::string::npos) return;
			lineStart = newline + 1;
		}
		const auto lineEnd = xaml.find('\n', lineStart);
		const auto available = (lineEnd == std::string::npos ? xaml.size() : lineEnd)
			- lineStart;
		const auto byteInLine = (std::min)(exception.Column() - 1, available);
		const auto byteOffset = lineStart + byteInLine;

		diagnostic->Line = exception.Line();
		try
		{
			diagnostic->Column = FromUtf8(
				xaml.substr(lineStart, byteInLine)).size() + 1;
			diagnostic->Utf16Offset = FromUtf8(
				xaml.substr(0, byteOffset)).size();
		}
		catch (...)
		{
			// Invalid UTF-8 can still report the parser's byte-based coordinates.
			diagnostic->Column = exception.Column();
		}
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

	/**
	 * Associates XmlLite DOM elements with their opening tags in the original
	 * UTF-16 editor text. XmlLite deliberately keeps source coordinates out of
	 * the DOM, so semantic validation failures need this lightweight side map.
	 */
	class XamlSourceLocationIndex final
	{
	public:
		XamlSourceLocationIndex(
			std::wstring source,
			const Element& root)
			: _source(std::move(source))
		{
			const auto tags = XamlSourceScanner::ScanTags(_source);
			std::vector<XamlSourceScanner::TagToken> openingTags;
			openingTags.reserve(tags.size());
			for (const auto& tag : tags)
				if (tag.Kind != XamlSourceScanner::TagKind::Closing)
					openingTags.push_back(tag);

			std::vector<Element> elements;
			CollectElements(root, elements);
			size_t tagIndex = 0;
			for (const auto& element : elements)
			{
				if (!element) continue;
				const auto rawName = FromUtf8(element->Name());
				while (tagIndex < openingTags.size()
					&& openingTags[tagIndex].Name != rawName)
					tagIndex++;
				if (tagIndex >= openingTags.size()) break;
				_tags.emplace(element.get(), openingTags[tagIndex++]);
			}
		}

		void Populate(
			const XmlElement* element,
			const XmlAttribute* attribute,
			XamlDocumentDiagnostic* diagnostic) const
		{
			if (!element || !diagnostic) return;
			const auto found = _tags.find(element);
			if (found == _tags.end()) return;

			size_t offset = found->second.NameStart;
			if (attribute)
			{
				const auto attributeOffset = FindAttributeOffset(
					found->second, FromUtf8(attribute->Name()));
				if (attributeOffset) offset = *attributeOffset;
			}
			PopulatePosition(offset, *diagnostic);
		}

	private:
		std::wstring _source;
		std::unordered_map<const XmlElement*, XamlSourceScanner::TagToken> _tags;

		static void CollectElements(
			const Element& element,
			std::vector<Element>& output)
		{
			if (!element) return;
			output.push_back(element);
			for (const auto& child : ChildElements(element))
				CollectElements(child, output);
		}

		std::optional<size_t> FindAttributeOffset(
			const XamlSourceScanner::TagToken& tag,
			const std::wstring& rawName) const
		{
			if (tag.End > _source.size() || tag.End == 0)
				return std::nullopt;
			size_t cursor = tag.NameStart + tag.NameLength;
			const size_t end = tag.End - 1;
			while (cursor < end)
			{
				while (cursor < end && std::iswspace(_source[cursor])) cursor++;
				if (cursor >= end || _source[cursor] == L'/') break;
				const size_t nameStart = cursor;
				while (cursor < end
					&& XamlSourceScanner::IsNameCharacter(_source[cursor])) cursor++;
				if (cursor == nameStart)
				{
					cursor++;
					continue;
				}
				const auto name = _source.substr(nameStart, cursor - nameStart);
				if (name == rawName) return nameStart;

				while (cursor < end && std::iswspace(_source[cursor])) cursor++;
				if (cursor >= end || _source[cursor] != L'=') continue;
				cursor++;
				while (cursor < end && std::iswspace(_source[cursor])) cursor++;
				if (cursor >= end
					|| (_source[cursor] != L'\'' && _source[cursor] != L'"'))
					continue;
				const wchar_t quote = _source[cursor++];
				while (cursor < end && _source[cursor] != quote) cursor++;
				if (cursor < end) cursor++;
			}
			return std::nullopt;
		}

		void PopulatePosition(
			size_t offset,
			XamlDocumentDiagnostic& diagnostic) const
		{
			offset = (std::min)(offset, _source.size());
			diagnostic.Line = 1;
			diagnostic.Column = 1;
			diagnostic.Utf16Offset = offset;
			for (size_t i = 0; i < offset;)
			{
				if (_source[i] == L'\r')
				{
					diagnostic.Line++;
					diagnostic.Column = 1;
					i++;
					if (i < offset && _source[i] == L'\n') i++;
					continue;
				}
				if (_source[i] == L'\n')
				{
					diagnostic.Line++;
					diagnostic.Column = 1;
					i++;
					continue;
				}
				if (i + 1 < offset
					&& _source[i] >= 0xD800 && _source[i] <= 0xDBFF
					&& _source[i + 1] >= 0xDC00 && _source[i + 1] <= 0xDFFF)
					i += 2;
				else
					i++;
				diagnostic.Column++;
			}
		}
	};

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

	bool TryParseUnsignedInteger(
		const std::wstring& value,
		unsigned long long& output)
	{
		try
		{
			const auto text = Trim(value);
			if (text.empty() || text.front() == L'-') return false;
			size_t consumed = 0;
			const auto parsed = std::stoull(text, &consumed, 10);
			if (consumed != text.size()) return false;
			output = parsed;
			return true;
		}
		catch (...)
		{
			return false;
		}
	}

	bool TryParseDouble(const std::wstring& value, double& output)
	{
		try
		{
			const auto text = Trim(value);
			size_t consumed = 0;
			const auto parsed = std::stod(text, &consumed);
			if (text.empty() || consumed != text.size() || !std::isfinite(parsed))
				return false;
			output = parsed;
			return true;
		}
		catch (...)
		{
			return false;
		}
	}

	bool TryParseEnum(
		const std::wstring& value,
		std::initializer_list<const wchar_t*> names,
		int& output)
	{
		const auto text = Trim(value);
		int index = 0;
		for (const auto* name : names)
		{
			if (Equals(text, name))
			{
				output = index;
				return true;
			}
			++index;
		}
		if (!TryParseInteger(text, output)) return false;
		return output >= 0 && static_cast<size_t>(output) < names.size();
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
		if (!Lower(text).starts_with(L"binding")
			|| (text.size() > 7 && std::iswspace(text[7]) == 0
				&& text[7] != L',')) return false;
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

	bool IsPathOnlyBindingExpression(const std::wstring& value)
	{
		auto text = Trim(value);
		if (text.size() < 3 || text.front() != L'{' || text.back() != L'}')
			return false;
		text = Trim(text.substr(1, text.size() - 2));
		if (!Lower(text).starts_with(L"binding")
			|| (text.size() > 7 && std::iswspace(text[7]) == 0
				&& text[7] != L',')) return false;
		text = Trim(text.substr(7));
		if (text.empty() || text.find(L',') != std::wstring::npos) return false;
		const auto equals = text.find(L'=');
		return equals == std::wstring::npos
			|| Lower(Trim(text.substr(0, equals))) == L"path";
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

	class Parser final
	{
	public:
		Parser(
			DesignDocument& document,
			const XamlDocumentParseOptions& options,
			const XamlSourceLocationIndex& sourceLocations,
			XamlDocumentDiagnostic* diagnostic)
			: _document(document),
			  _options(options),
			  _sourceLocations(sourceLocations),
			  _diagnostic(diagnostic)
		{
			const auto configured = options.ResourceBasePath.empty()
				? std::filesystem::current_path()
				: std::filesystem::path(options.ResourceBasePath);
			_rootResourceBasePath = std::filesystem::absolute(configured)
				.lexically_normal().wstring();
			_currentResourceBasePath = _rootResourceBasePath;
		}

		bool Parse(const Element& root, std::wstring& error)
		{
			DiagnosticContext context(*this, root);
			if (!root)
				return Fail(L"XAML 没有根元素。", error);
			const auto rootName = FromUtf8(root->LocalName());
			if (!Equals(rootName, L"Form") && !Equals(rootName, L"Window"))
				return Fail(L"XAML 根元素必须是 Form 或 Window。", error);

			// Property elements are order-independent: schema/resources first.
			for (const auto& child : ChildElements(root))
			{
				DiagnosticContext childContext(*this, child);
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
				DiagnosticContext childContext(*this, child);
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
				_document.DataContextSchema, &error)) return Fail(error, error);
			if (!DesignerStyleSheetUtils::ValidateAgainstPropertyMetadata(
				_document.StyleSheet,
				DesignDocumentMaterializer::CreateRuntimeControl,
				&error, _document.ResourceBasePath,
				_document.Resources)) return Fail(error, error);
			_document.RecalculateNextStableId();
			DesignDocumentGraph graph;
			if (!DesignDocumentGraph::Build(_document, graph, &error))
				return Fail(error, error);
			DesignDocumentEventIndex eventIndex;
			if (!DesignDocumentEventIndex::Build(
				_document, eventIndex, &error)) return Fail(error, error);
			return true;
		}

		void FinalizeFailure(
			const Element& root,
			const std::wstring& message)
		{
			if (!_diagnostic) return;
			_diagnostic->Message = message;
			if (!_diagnostic->HasSourceOffset())
				_sourceLocations.Populate(root.get(), nullptr, _diagnostic);
		}

	private:
		struct Parent
		{
			int Id = 0;
			std::wstring Ref;
		};

		DesignDocument& _document;
		const XamlDocumentParseOptions& _options;
		const XamlSourceLocationIndex& _sourceLocations;
		XamlDocumentDiagnostic* _diagnostic = nullptr;
		const XmlElement* _diagnosticElement = nullptr;
		const XmlAttribute* _diagnosticAttribute = nullptr;
		std::wstring _rootResourceBasePath;
		std::wstring _currentResourceBasePath;
		std::wstring _currentDictionaryOrigin;
		std::vector<std::wstring> _resourceDictionaryStack;
		std::unordered_set<int> _usedIds;
		std::unordered_set<std::wstring> _usedNames;
		std::unordered_map<std::wstring, int> _nameCounters;
		std::vector<std::wstring> _bindingPaths;

		class DiagnosticContext final
		{
		public:
			DiagnosticContext(
				Parser& parser,
				const Element& element,
				const XmlAttribute* attribute = nullptr)
				: _parser(parser),
				  _previousElement(parser._diagnosticElement),
				  _previousAttribute(parser._diagnosticAttribute)
			{
				_parser._diagnosticElement = element.get();
				_parser._diagnosticAttribute = attribute;
			}

			~DiagnosticContext()
			{
				_parser._diagnosticElement = _previousElement;
				_parser._diagnosticAttribute = _previousAttribute;
			}

		private:
			Parser& _parser;
			const XmlElement* _previousElement = nullptr;
			const XmlAttribute* _previousAttribute = nullptr;
		};

		bool Fail(std::wstring message, std::wstring& error)
		{
			error = std::move(message);
			if (_diagnostic)
			{
				_diagnostic->Message = error;
				if (!_diagnostic->HasSourceOffset())
					_sourceLocations.Populate(
						_diagnosticElement, _diagnosticAttribute, _diagnostic);
			}
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
			DiagnosticContext context(*this, root);
			if (const auto name = Attribute(root, L"Name"))
				_document.Form.Name = Trim(*name);
			if (const auto xName = Attribute(root, L"Name", L"x"))
				_document.Form.Name = Trim(*xName);
			if (!ValidateIdentifier(_document.Form.Name, L"窗体名称", error)) return false;

			if (const auto className = Attribute(root, L"Class", L"x"))
				if (!DesignCodeBehindModel::TryNormalizeClassName(
					Trim(*className), _document.CodeBehind.ClassName, &error))
					return Fail(error, error);
			if (const auto relativePath = Attribute(root, L"CodeBehind", L"d"))
			{
				if (!DesignCodeBehindModel::TryNormalizeRelativeBasePath(
					Trim(*relativePath),
					_document.CodeBehind.RelativeBasePath, &error))
					return Fail(error, error);
			}
			if (!_document.CodeBehind.Validate(&error)) return Fail(error, error);

			for (const auto& attribute : root->Attributes())
			{
				if (!attribute || IsNamespaceAttribute(*attribute)) continue;
				DiagnosticContext attributeContext(*this, root, attribute.get());
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
			DiagnosticContext context(*this, container);
			for (const auto& item : ChildElements(container))
			{
				DiagnosticContext itemContext(*this, item);
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
			DiagnosticContext context(*this, controlElement);
			Element container;
			for (const auto& child : ChildElements(controlElement))
			{
				DiagnosticContext childContext(*this, child);
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
				DiagnosticContext attributeContext(
					*this, container, attribute.get());
				if (attribute && !IsNamespaceAttribute(*attribute))
					return Fail(L"d:CustomEvents 不支持属性。", error);
			}

			for (const auto& item : ChildElements(container))
			{
				DiagnosticContext itemContext(*this, item);
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
					DiagnosticContext attributeContext(
						*this, item, attribute.get());
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
			DiagnosticContext context(*this, container);
			const auto children = ChildElements(container);
			// Merged dictionaries have lower precedence than every local entry,
			// irrespective of where the property element appears in source order.
			for (const auto& item : children)
			{
				if (!Equals(FromUtf8(item->LocalName()),
					L"ResourceDictionary.MergedDictionaries")) continue;
				if (!ValidateAttributes(item, {}, error)) return false;
				for (const auto& dictionary : ChildElements(item))
				{
					if (!Equals(FromUtf8(dictionary->LocalName()), L"ResourceDictionary"))
						return Fail(L"MergedDictionaries 只能包含 ResourceDictionary。", error);
					if (!ParseResourceDictionary(dictionary, error)) return false;
				}
			}
			for (const auto& item : children)
			{
				DiagnosticContext itemContext(*this, item);
				const auto name = FromUtf8(item->LocalName());
				if (Equals(name, L"ResourceDictionary.MergedDictionaries"))
					continue;
				if (Equals(name, L"ResourceDictionary"))
				{
					if (!ParseResourceDictionary(item, error)) return false;
					continue;
				}
				if (!ParseResourceItem(item, error)) return false;
			}
			return true;
		}

		std::wstring RebaseResourceUri(const std::wstring& uri) const
		{
			const auto trimmed = Trim(uri);
			if (trimmed.empty() || trimmed.find(L"://") != std::wstring::npos)
				return trimmed;
			auto path = std::filesystem::path(trimmed);
			if (!path.is_absolute())
				path = std::filesystem::path(_currentResourceBasePath) / path;
			path = std::filesystem::absolute(path).lexically_normal();
			try
			{
				return std::filesystem::relative(
					path, std::filesystem::path(_rootResourceBasePath))
					.generic_wstring();
			}
			catch (...)
			{
				return path.generic_wstring();
			}
		}

		void MarkImportedValue(DesignerStyleValue& value) const
		{
			if (_currentDictionaryOrigin.empty()) return;
			if (value.Kind == DesignerStyleValueKind::ImageSource)
				value.Text = RebaseResourceUri(value.Text);
			else if (value.Kind == DesignerStyleValueKind::Brush
				&& value.ObjectValue.is_object()
				&& value.ObjectValue.value("type", std::string{}) == "image"
				&& value.ObjectValue.contains("source"))
				value.ObjectValue["source"] = ToUtf8(RebaseResourceUri(
					FromUtf8(value.ObjectValue.value("source", std::string{}))));
		}

		void AddResource(DesignerStyleResource resource)
		{
			auto& resources = _document.StyleSheet.Resources;
			resources.erase(std::remove_if(resources.begin(), resources.end(),
				[&](const auto& current) { return Equals(current.Key, resource.Key); }),
				resources.end());
			resources.push_back(std::move(resource));
		}

		void AddStyleRule(DesignerStyleRule rule)
		{
			// CUI styles may intentionally share an Id while targeting different
			// states. Source order already gives local dictionary rules precedence.
			_document.StyleSheet.Rules.push_back(std::move(rule));
		}

		bool LoadMergedDictionary(
			const std::wstring& source,
			std::wstring& error)
		{
			if (Trim(source).empty())
				return Fail(L"ResourceDictionary Source 不能为空。", error);
			const auto authorUri = RebaseResourceUri(source);
			ResolvedResource resource;
			if (!_document.Resources || !_document.Resources->Resolve(
				source, _currentResourceBasePath, resource, &error))
				return Fail(error.empty()
					? L"无法加载合并资源字典：" + source : error, error);
			const auto identity = Lower(resource.Identity);
			if (std::find(_resourceDictionaryStack.begin(),
				_resourceDictionaryStack.end(), identity)
				!= _resourceDictionaryStack.end())
				return Fail(L"检测到循环合并资源字典：" + resource.Identity, error);

			try
			{
				XmlDocument xml;
				xml.LoadXml(std::string(
					reinterpret_cast<const char*>(resource.Bytes.data()),
					resource.Bytes.size()));
				const auto root = xml.DocumentElement();
				if (!root || !Equals(
					FromUtf8(root->LocalName()), L"ResourceDictionary"))
					return Fail(L"合并文件根元素必须是 ResourceDictionary："
						+ resource.Identity, error);

				const auto previousBase = _currentResourceBasePath;
				const auto previousOrigin = _currentDictionaryOrigin;
				_currentResourceBasePath = resource.BaseUri;
				if (_currentDictionaryOrigin.empty())
				{
					_currentDictionaryOrigin = authorUri;
					if (std::none_of(
						_document.StyleSheet.MergedDictionaries.begin(),
						_document.StyleSheet.MergedDictionaries.end(),
						[&](const auto& current) { return Equals(current, authorUri); }))
						_document.StyleSheet.MergedDictionaries.push_back(authorUri);
				}
				_resourceDictionaryStack.push_back(identity);
				const bool parsed = ParseResourceDictionary(root, error);
				_resourceDictionaryStack.pop_back();
				_currentResourceBasePath = previousBase;
				_currentDictionaryOrigin = previousOrigin;
				if (!parsed)
					error = L"合并资源字典 " + resource.Identity + L"：" + error;
				return parsed;
			}
			catch (const std::exception& exception)
			{
				return Fail(L"解析合并资源字典失败：" + resource.Identity
					+ L"：" + FromUtf8(exception.what()), error);
			}
		}

		bool ParseResourceDictionary(
			const Element& dictionary,
			std::wstring& error)
		{
			DiagnosticContext context(*this, dictionary);
			if (!ValidateAttributes(dictionary, { L"Source" }, error)) return false;
			if (const auto source = Attribute(dictionary, L"Source"))
			{
				if (!ChildElements(dictionary).empty())
					return Fail(L"带 Source 的 ResourceDictionary 不能包含本地项。", error);
				return LoadMergedDictionary(*source, error);
			}
			return ParseResources(dictionary, error);
		}

		bool ParseResourceItem(const Element& item, std::wstring& error)
		{
			DiagnosticContext itemContext(*this, item);
			const auto name = FromUtf8(item->LocalName());
			if (Equals(name, L"Style"))
				return ParseStyle(item, error);

			DesignerStyleResource resource;
			resource.Key = Trim(Attribute(item, L"Key", L"x").value_or(
				Attribute(item, L"Key").value_or(L"")));
			if (resource.Key.empty())
				return Fail(L"样式资源缺少 x:Key。", error);

			DesignerStyleValueKind kind = DesignerStyleValueKind::String;
			if (Equals(name, L"SolidColorBrush")
				|| Equals(name, L"LinearGradientBrush")
				|| Equals(name, L"RadialGradientBrush")
				|| Equals(name, L"ImageBrush"))
			{
				resource.Value.Kind = DesignerStyleValueKind::Brush;
				if (!ParseBrushElement(
					item, resource.Value.ObjectValue, error)) return false;
			}
			else if (Equals(name, L"BitmapImage")
				|| Equals(name, L"ImageSource"))
			{
				if (!ValidateAttributes(item,
					{ L"Key", L"UriSource", L"Source" }, error)
					|| !ChildElements(item).empty())
					return Fail(L"BitmapImage 不允许包含子元素。", error);
				resource.Value.Kind = DesignerStyleValueKind::ImageSource;
				resource.Value.Text = Attribute(item, L"UriSource").value_or(
					Attribute(item, L"Source").value_or(
						Trim(FromUtf8(item->InnerText()))));
				if (resource.Value.Text.empty())
					return Fail(L"BitmapImage 缺少 UriSource。", error);
			}
			else if (Equals(name, L"RectangleGeometry")
				|| Equals(name, L"EllipseGeometry")
				|| Equals(name, L"PathGeometry")
				|| Equals(name, L"GeometryGroup"))
			{
				resource.Value.Kind = DesignerStyleValueKind::Geometry;
				if (!ParseGeometryElement(
					item, resource.Value.ObjectValue, error, true)) return false;
			}
			else if (Equals(name, L"MatrixTransform")
				|| Equals(name, L"TranslateTransform")
				|| Equals(name, L"ScaleTransform")
				|| Equals(name, L"RotateTransform")
				|| Equals(name, L"SkewTransform")
				|| Equals(name, L"TransformGroup"))
			{
				resource.Value.Kind = DesignerStyleValueKind::Transform;
				resource.Value.ObjectValue = DesignValue::array();
				if (!ParseTransformElement(
					item, resource.Value.ObjectValue, error, true)) return false;
				if (resource.Value.ObjectValue.empty())
					return Fail(L"Transform 资源不能是空 TransformGroup。", error);
			}
			else if (Equals(name, L"Resource"))
			{
				const auto kindName = Attribute(item, L"Kind").value_or(L"String");
				if (!DesignerStyleSheetUtils::TryParseValueKind(kindName, kind))
					return Fail(L"样式资源类型无效：" + kindName, error);
			}
			else if (!DesignerStyleSheetUtils::TryParseValueKind(name, kind))
				return Fail(L"不支持的样式资源元素：" + name, error);
			if (resource.Value.Kind != DesignerStyleValueKind::Brush
				&& resource.Value.Kind != DesignerStyleValueKind::ImageSource
				&& resource.Value.Kind != DesignerStyleValueKind::Geometry
				&& resource.Value.Kind != DesignerStyleValueKind::Transform)
			{
				resource.Value.Kind = kind;
				resource.Value.Text = Attribute(item, L"Value").value_or(
					FromUtf8(item->InnerText()));
			}
			BindingValue ignored;
			std::wstring conversionError;
			if (!DesignerStyleSheetUtils::TryConvertValue(
				resource.Value, ignored, &conversionError,
				_currentResourceBasePath, _document.Resources))
				return Fail(L"样式资源 " + resource.Key + L"：" + conversionError, error);
			resource.SourceDictionary = _currentDictionaryOrigin;
			MarkImportedValue(resource.Value);
			AddResource(std::move(resource));
			return true;
		}

		bool ParseStyleSetter(
			const Element& element,
			Control& probe,
			DesignerStyleSetter& setter,
			std::wstring& error)
		{
			DiagnosticContext context(*this, element);
			if (!ValidateAttributes(element,
				{ L"Property", L"Value", L"Resource", L"Kind" }, error)) return false;
			const auto rawProperty = Trim(Attribute(element, L"Property").value_or(L""));
			auto rawValue = Attribute(element, L"Value").value_or(
				FromUtf8(element->InnerText()));
			if (rawProperty.empty()) return Fail(L"Setter 缺少 Property。", error);
			const auto propertyName = NormalizePropertyName(rawProperty, rawValue);
			const auto properties = DesignerPropertyCatalog::GetStyleProperties(probe);
			const auto* descriptor = DesignerPropertyCatalog::Find(
				properties, propertyName);
			if (!descriptor)
				return Fail(L"Style 目标类型不包含属性：" + rawProperty, error);

			setter.PropertyName = descriptor->Name;
			setter.ResourceKey = Trim(Attribute(element, L"Resource").value_or(L""));
			if (setter.ResourceKey.empty())
				(void)TryParseStaticResource(rawValue, setter.ResourceKey);
			setter.UsesResource = !setter.ResourceKey.empty();
			if (setter.UsesResource) return true;

			setter.Literal.Kind = descriptor->ValueKind;
			const auto setterChildren = ChildElements(element);
			const bool objectValue =
				descriptor->ValueKind == DesignerStyleValueKind::Brush
				|| descriptor->ValueKind == DesignerStyleValueKind::Geometry
				|| descriptor->ValueKind == DesignerStyleValueKind::Transform;
			if (objectValue && !setterChildren.empty())
			{
				if (setterChildren.size() != 1
					|| !Equals(FromUtf8(setterChildren.front()->LocalName()),
						L"Setter.Value"))
					return Fail(L"对象型 Setter 必须包含一个 Setter.Value。", error);
				const auto& valueElement = setterChildren.front();
				if (descriptor->ValueKind == DesignerStyleValueKind::Brush)
				{
					if (!ParseBrush(valueElement,
						setter.Literal.ObjectValue, error)) return false;
				}
				else
				{
					if (!ValidateAttributes(valueElement, {}, error)) return false;
					const auto nested = ChildElements(valueElement);
					if (nested.size() != 1)
						return Fail(L"Setter.Value 必须且只能包含一个对象。", error);
					if (descriptor->ValueKind == DesignerStyleValueKind::Geometry)
					{
						if (!ParseGeometryElement(nested.front(),
							setter.Literal.ObjectValue, error)) return false;
					}
					else
					{
						setter.Literal.ObjectValue = DesignValue::array();
						if (!ParseTransformElement(nested.front(),
							setter.Literal.ObjectValue, error)) return false;
						if (setter.Literal.ObjectValue.empty())
							return Fail(L"Transform Setter 不能为空。", error);
					}
				}
			}
			else if (!setterChildren.empty())
				return Fail(L"Setter.Value 仅用于对象型属性。", error);
			if (const auto kindName = Attribute(element, L"Kind"))
			{
				if (!DesignerStyleSheetUtils::TryParseValueKind(
					*kindName, setter.Literal.Kind))
					return Fail(L"Setter Kind 无效：" + *kindName, error);
			}
			if (!objectValue)
				setter.Literal.Text = NormalizePropertyText(
					rawProperty, rawValue, *descriptor);
			std::wstring validationError;
			if (!DesignerPropertyCatalog::ValidateStyleValue(
				probe, setter.PropertyName, setter.Literal, &validationError,
				_currentResourceBasePath, _document.Resources))
				return Fail(L"Setter " + rawProperty + L"：" + validationError, error);
			return true;
		}

		bool ParseStyleCondition(
			const Element& element,
			DesignerStyleCondition& condition,
			bool requireLeaf,
			std::wstring& error)
		{
			DiagnosticContext context(*this, element);
			if (!ValidateAttributes(element, { L"Property", L"Value" }, error))
				return false;
			condition.Property = DesignerStyleSheetUtils::CanonicalTriggerProperty(
				Attribute(element, L"Property").value_or(L""));
			if (condition.Property.empty())
				return Fail(L"Condition Property 仅支持 IsMouseOver、IsKeyboardFocused、"
					L"IsPressed、IsEnabled、IsChecked 或 IsSelected。", error);
			const auto value = Attribute(element, L"Value");
			if (!value || !TryParseBool(*value, condition.Value))
				return Fail(L"Condition Value 必须是布尔值。", error);
			if (requireLeaf && !ChildElements(element).empty())
				return Fail(L"Condition 不能包含子元素。", error);
			return true;
		}

		bool ParseStyleTrigger(
			const Element& element,
			Control& probe,
			DesignerStyleTrigger& trigger,
			std::wstring& error)
		{
			DesignerStyleCondition condition;
			if (!ParseStyleCondition(element, condition, false, error)) return false;
			trigger.Conditions.push_back(std::move(condition));
			for (const auto& child : ChildElements(element))
			{
				if (!Equals(FromUtf8(child->LocalName()), L"Setter"))
					return Fail(L"Trigger 仅支持 Setter 子元素。", error);
				DesignerStyleSetter setter;
				if (!ParseStyleSetter(child, probe, setter, error)) return false;
				trigger.Setters.push_back(std::move(setter));
			}
			if (trigger.Setters.empty()) return Fail(L"Trigger 至少需要一个 Setter。", error);
			return true;
		}

		bool ParseStyleMultiTrigger(
			const Element& element,
			Control& probe,
			DesignerStyleTrigger& trigger,
			std::wstring& error)
		{
			DiagnosticContext context(*this, element);
			if (!ValidateAttributes(element, {}, error)) return false;
			bool foundConditions = false;
			for (const auto& child : ChildElements(element))
			{
				const auto childName = FromUtf8(child->LocalName());
				if (Equals(childName, L"Setter"))
				{
					DesignerStyleSetter setter;
					if (!ParseStyleSetter(child, probe, setter, error)) return false;
					trigger.Setters.push_back(std::move(setter));
					continue;
				}
				if (!Equals(childName, L"MultiTrigger.Conditions"))
					return Fail(L"MultiTrigger 仅支持 MultiTrigger.Conditions 和 Setter。",
						error);
				if (foundConditions)
					return Fail(L"MultiTrigger.Conditions 不能重复。", error);
				foundConditions = true;
				if (!ValidateAttributes(child, {}, error)) return false;
				for (const auto& conditionElement : ChildElements(child))
				{
					if (!Equals(FromUtf8(conditionElement->LocalName()), L"Condition"))
						return Fail(L"MultiTrigger.Conditions 仅支持 Condition。", error);
					DesignerStyleCondition condition;
					if (!ParseStyleCondition(
						conditionElement, condition, true, error)) return false;
					trigger.Conditions.push_back(std::move(condition));
				}
			}
			if (trigger.Conditions.size() < 2)
				return Fail(L"MultiTrigger 至少需要两个 Condition。", error);
			if (trigger.Setters.empty())
				return Fail(L"MultiTrigger 至少需要一个 Setter。", error);
			return true;
		}

		bool ParseStyleDataCondition(
			const Element& element,
			DesignerStyleDataCondition& condition,
			bool requireLeaf,
			std::wstring& error)
		{
			DiagnosticContext context(*this, element);
			if (!ValidateAttributes(element, { L"Binding", L"Value" }, error))
				return false;
			const auto bindingText = Attribute(element, L"Binding");
			const auto expected = Attribute(element, L"Value");
			DesignerDataBinding binding;
			std::wstring bindingError;
			if (!bindingText || !TryParseBinding(*bindingText, binding, bindingError))
				return Fail(bindingError.empty()
					? L"DataTrigger Binding 必须使用 {Binding Path}。"
					: bindingError, error);
			if (!IsPathOnlyBindingExpression(*bindingText)
				|| binding.Mode != BindingMode::OneWay
				|| binding.UpdateMode != DataSourceUpdateMode::OnPropertyChanged
				|| !binding.Converter.empty())
				return Fail(L"DataTrigger Binding 首批只支持 Path，不支持 Mode、UpdateMode 或 Converter。",
					error);
			if (!expected)
				return Fail(L"DataTrigger 必须声明 Value。", error);
			std::wstring resourceKey;
			if (TryParseStaticResource(*expected, resourceKey))
				return Fail(L"DataTrigger Value 首批只支持字面值。", error);
			condition.SourceProperty = binding.SourceProperty;
			condition.Value.Kind = DesignerStyleValueKind::String;
			condition.Value.Text = *expected;
			_bindingPaths.push_back(binding.SourceProperty);
			if (requireLeaf && !ChildElements(element).empty())
				return Fail(L"MultiDataTrigger Condition 不能包含子元素。", error);
			return true;
		}

		bool ParseStyleDataTrigger(
			const Element& element,
			Control& probe,
			DesignerStyleTrigger& trigger,
			std::wstring& error)
		{
			DesignerStyleDataCondition condition;
			if (!ParseStyleDataCondition(
				element, condition, false, error)) return false;
			trigger.DataConditions.push_back(std::move(condition));
			for (const auto& child : ChildElements(element))
			{
				if (!Equals(FromUtf8(child->LocalName()), L"Setter"))
					return Fail(L"DataTrigger 仅支持 Setter 子元素。", error);
				DesignerStyleSetter setter;
				if (!ParseStyleSetter(child, probe, setter, error)) return false;
				trigger.Setters.push_back(std::move(setter));
			}
			if (trigger.Setters.empty())
				return Fail(L"DataTrigger 至少需要一个 Setter。", error);
			return true;
		}

		bool ParseStyleMultiDataTrigger(
			const Element& element,
			Control& probe,
			DesignerStyleTrigger& trigger,
			std::wstring& error)
		{
			DiagnosticContext context(*this, element);
			if (!ValidateAttributes(element, {}, error)) return false;
			bool foundConditions = false;
			for (const auto& child : ChildElements(element))
			{
				const auto childName = FromUtf8(child->LocalName());
				if (Equals(childName, L"Setter"))
				{
					DesignerStyleSetter setter;
					if (!ParseStyleSetter(child, probe, setter, error)) return false;
					trigger.Setters.push_back(std::move(setter));
					continue;
				}
				if (!Equals(childName, L"MultiDataTrigger.Conditions"))
					return Fail(L"MultiDataTrigger 仅支持 MultiDataTrigger.Conditions 和 Setter。",
						error);
				if (foundConditions)
					return Fail(L"MultiDataTrigger.Conditions 不能重复。", error);
				foundConditions = true;
				if (!ValidateAttributes(child, {}, error)) return false;
				for (const auto& conditionElement : ChildElements(child))
				{
					if (!Equals(FromUtf8(conditionElement->LocalName()), L"Condition"))
						return Fail(L"MultiDataTrigger.Conditions 仅支持 Condition。", error);
					DesignerStyleDataCondition condition;
					if (!ParseStyleDataCondition(
						conditionElement, condition, true, error)) return false;
					trigger.DataConditions.push_back(std::move(condition));
				}
			}
			if (trigger.DataConditions.size() < 2)
				return Fail(L"MultiDataTrigger 至少需要两个 Condition。", error);
			if (trigger.Setters.empty())
				return Fail(L"MultiDataTrigger 至少需要一个 Setter。", error);
			return true;
		}

		bool ParseStyle(const Element& element, std::wstring& error)
		{
			DiagnosticContext context(*this, element);
			if (!ValidateAttributes(element,
				{ L"TargetType", L"Id", L"BasedOn", L"Classes", L"Class",
				  L"RequiredStates", L"ExcludedStates" }, error, true)) return false;
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
			if (const auto basedOn = Attribute(element, L"BasedOn"))
			{
				if (!TryParseStaticResource(*basedOn, rule.BasedOn))
					return Fail(L"Style BasedOn 必须使用 {StaticResource key}。", error);
			}
			rule.Classes = DesignerStyleSheetUtils::SplitClasses(
				Attribute(element, L"Classes").value_or(
					Attribute(element, L"Class").value_or(L"")));
			if (!DesignerStyleSheetUtils::TryParseStates(
				Attribute(element, L"RequiredStates").value_or(L""), rule.RequiredStates)
				|| !DesignerStyleSheetUtils::TryParseStates(
					Attribute(element, L"ExcludedStates").value_or(L""), rule.ExcludedStates))
				return Fail(L"Style 状态选择器无效。", error);

			auto effectiveRule = rule;
			if (!rule.BasedOn.empty())
			{
				auto probeSheet = _document.StyleSheet;
				probeSheet.Rules.push_back(rule);
				DesignerStyleSheet resolved;
				std::wstring inheritanceError;
				if (!DesignerStyleSheetUtils::ResolveInheritance(
					probeSheet, resolved, &inheritanceError))
					return Fail(inheritanceError, error);
				effectiveRule = resolved.Rules.back();
			}
			auto probe = DesignDocumentMaterializer::CreateRuntimeControl(
				effectiveRule.HasType ? effectiveRule.Type : UIClass::UI_Base);
			if (!probe) return Fail(L"Style TargetType 尚无运行时控件工厂。", error);
			bool foundTriggers = false;
			for (const auto& child : ChildElements(element))
			{
				DiagnosticContext childContext(*this, child);
				const auto childName = FromUtf8(child->LocalName());
				if (Equals(childName, L"Setter"))
				{
					DesignerStyleSetter setter;
					if (!ParseStyleSetter(child, *probe, setter, error)) return false;
					rule.Setters.push_back(std::move(setter));
					continue;
				}
				if (!Equals(childName, L"Style.Triggers"))
					return Fail(L"Style 仅支持 Setter 和 Style.Triggers 子元素。", error);
				if (foundTriggers) return Fail(L"Style.Triggers 不能重复。", error);
				foundTriggers = true;
				if (!ValidateAttributes(child, {}, error)) return false;
				for (const auto& triggerElement : ChildElements(child))
				{
					const auto triggerName = FromUtf8(triggerElement->LocalName());
					if (!Equals(triggerName, L"Trigger")
						&& !Equals(triggerName, L"MultiTrigger")
						&& !Equals(triggerName, L"DataTrigger")
						&& !Equals(triggerName, L"MultiDataTrigger"))
						return Fail(L"Style.Triggers 仅支持 Trigger、MultiTrigger、DataTrigger 和 MultiDataTrigger。",
							error);
					DesignerStyleTrigger trigger;
					if (Equals(triggerName, L"Trigger"))
					{
						if (!ParseStyleTrigger(
							triggerElement, *probe, trigger, error)) return false;
					}
					else if (Equals(triggerName, L"MultiTrigger"))
					{
						if (!ParseStyleMultiTrigger(
							triggerElement, *probe, trigger, error)) return false;
					}
					else if (Equals(triggerName, L"DataTrigger"))
					{
						if (!ParseStyleDataTrigger(
							triggerElement, *probe, trigger, error)) return false;
					}
					else if (!ParseStyleMultiDataTrigger(
						triggerElement, *probe, trigger, error)) return false;
					rule.Triggers.push_back(std::move(trigger));
				}
			}
			if (rule.Setters.empty() && rule.BasedOn.empty()
				&& rule.Triggers.size() == 1
				&& !rule.Triggers.front().DataConditions.empty()
				&& rule.Triggers.front().Conditions.empty())
			{
				rule.DataConditions = std::move(
					rule.Triggers.front().DataConditions);
				rule.Setters = std::move(rule.Triggers.front().Setters);
				rule.Triggers.clear();
			}
			rule.SourceDictionary = _currentDictionaryOrigin;
			if (!_currentDictionaryOrigin.empty())
			{
				for (auto& setter : rule.Setters)
					if (!setter.UsesResource) MarkImportedValue(setter.Literal);
				for (auto& trigger : rule.Triggers)
					for (auto& setter : trigger.Setters)
						if (!setter.UsesResource) MarkImportedValue(setter.Literal);
			}
			AddStyleRule(std::move(rule));
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
			DiagnosticContext context(*this, element);
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
			DiagnosticContext context(*this, element);
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
				DiagnosticContext childContext(*this, child);
				const auto childName = FromUtf8(child->LocalName());
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
				bool structuredProperty = false;
				if (!TryParseContentItem(
					child, nodeIndex, type, structuredProperty, error)) return false;
				if (structuredProperty) continue;
				if (!TryParseStructuredProperty(
					child, nodeIndex, type, structuredProperty, error)) return false;
				if (structuredProperty) continue;
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
			DiagnosticContext context(*this, element);
			std::unordered_set<std::wstring> assignedProperties;
			for (const auto& attribute : element->Attributes())
			{
				if (!attribute || IsNamespaceAttribute(*attribute)) continue;
				DiagnosticContext attributeContext(
					*this, element, attribute.get());
				const auto prefix = FromUtf8(attribute->Prefix());
				const auto name = FromUtf8(attribute->LocalName());
				const auto rawName = FromUtf8(attribute->Name());
				const auto value = FromUtf8(attribute->Value());
				if (Equals(prefix, L"d")
					&& (Equals(name, L"CppType") || Equals(name, L"Header")
						|| Equals(name, L"BaseType") || Equals(name, L"Constructor")))
					continue;
				if (Equals(prefix, L"d") && Equals(name, L"Locked"))
				{
					bool locked = false;
					if (!TryParseBool(value, locked))
						return Fail(L"d:Locked 必须是布尔值。", error);
					_document.Nodes[nodeIndex].Locked = locked;
					continue;
				}
				if (Equals(name, L"Name") || Equals(name, L"DesignId")
					|| (Equals(prefix, L"x") && Equals(name, L"Uid"))) continue;
				if (_document.Nodes[nodeIndex].Type == UIClass::UI_MediaPlayer
					&& Equals(name, L"Source"))
				{
					_document.Nodes[nodeIndex].Extra["mediaFile"] = ToUtf8(value);
					continue;
				}
				if (Equals(name, L"RenderTransformOrigin"))
				{
					double x = 0.0;
					double y = 0.0;
					if (!ParsePointText(value, x, y))
						return Fail(L"RenderTransformOrigin 必须使用 x,y 格式。", error);
					_document.Nodes[nodeIndex].Extra["renderTransformOrigin"] =
						DesignValue{ { "x", x }, { "y", y } };
					continue;
				}

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
					probe, descriptor->Name, typed, &canonical, &effective, &applyError,
					_options.ResourceBasePath))
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
			auto stored = DesignValue{
				{ "kind", ToUtf8(DesignerStyleSheetUtils::ValueKindName(value.Kind)) },
				{ "value", ToUtf8(value.Text) }
			};
			if (!value.ObjectValue.is_null())
				stored["object"] = value.ObjectValue;
			metadata[ToUtf8(propertyName)] = std::move(stored);
		}

		bool ApplyDirectText(
			const Element& element,
			size_t nodeIndex,
			Control& probe,
			std::wstring& error)
		{
			DiagnosticContext context(*this, element);
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
				{ descriptor->ValueKind, text }, &canonical, &effective, &applyError,
				_options.ResourceBasePath))
				return Fail(applyError, error);
			StoreMetadata(_document.Nodes[nodeIndex], canonical, effective);
			return true;
		}

		bool ValidateAttributes(
			const Element& element,
			std::initializer_list<const wchar_t*> allowed,
			std::wstring& error,
			bool allowResourceKey = false)
		{
			for (const auto& attribute : element->Attributes())
			{
				if (!attribute || IsNamespaceAttribute(*attribute)) continue;
				DiagnosticContext attributeContext(*this, element, attribute.get());
				const auto name = FromUtf8(attribute->LocalName());
				const auto prefix = FromUtf8(attribute->Prefix());
				if (allowResourceKey && Equals(prefix, L"x") && Equals(name, L"Key"))
					continue;
				bool found = false;
				for (const auto* candidate : allowed)
					if (Equals(name, candidate)) { found = true; break; }
				if (!found)
					return Fail(FromUtf8(element->LocalName())
						+ L" 不支持属性：" + name, error);
			}
			return true;
		}

		bool ReadBoolAttribute(
			const Element& element,
			const wchar_t* name,
			bool defaultValue,
			bool& output,
			std::wstring& error)
		{
			const auto text = Attribute(element, name);
			if (!text)
			{
				output = defaultValue;
				return true;
			}
			if (TryParseBool(*text, output)) return true;
			return Fail(std::wstring(name) + L" 必须是布尔值。", error);
		}

		bool ReadDoubleAttribute(
			const Element& element,
			const wchar_t* name,
			double defaultValue,
			double& output,
			std::wstring& error)
		{
			const auto text = Attribute(element, name);
			if (!text)
			{
				output = defaultValue;
				return true;
			}
			if (TryParseDouble(*text, output)) return true;
			return Fail(std::wstring(name) + L" 必须是有限数值。", error);
		}

		bool ParseStringItems(
			const Element& property,
			DesignValue& output,
			std::wstring& error)
		{
			if (!ValidateAttributes(property, {}, error)) return false;
			output = DesignValue::array();
			for (const auto& item : ChildElements(property))
			{
				DiagnosticContext itemContext(*this, item);
				if (!Equals(FromUtf8(item->LocalName()), L"String")
					|| (!Equals(FromUtf8(item->Prefix()), L"x")
						&& !Equals(FromUtf8(item->NamespaceURI()),
							L"http://schemas.microsoft.com/winfx/2006/xaml")))
					return Fail(L"字符串集合仅允许 x:String。", error);
				if (!ValidateAttributes(item, {}, error)) return false;
				if (!ChildElements(item).empty())
					return Fail(L"x:String 不允许包含子元素。", error);
				output.push_back(item->InnerText());
			}
			return true;
		}

		bool ParseComboBoxItems(
			const Element& property,
			DesignValue& output,
			std::wstring& error)
		{
			if (!ValidateAttributes(property, {}, error)) return false;
			output = DesignValue::array();
			for (const auto& item : ChildElements(property))
			{
				DiagnosticContext itemContext(*this, item);
				const auto name = FromUtf8(item->LocalName());
				if (Equals(name, L"ComboBoxItem"))
				{
					if (!ValidateAttributes(item, { L"Content" }, error)
						|| !ChildElements(item).empty())
						return Fail(L"ComboBoxItem 仅支持 Content 属性。", error);
					output.push_back(ToUtf8(
						Attribute(item, L"Content").value_or(L"")));
					continue;
				}
				if (!Equals(name, L"String")
					|| (!Equals(FromUtf8(item->Prefix()), L"x")
						&& !Equals(FromUtf8(item->NamespaceURI()),
							L"http://schemas.microsoft.com/winfx/2006/xaml")))
					return Fail(L"ComboBox.Items 仅允许 ComboBoxItem 或 x:String。", error);
				if (!ValidateAttributes(item, {}, error) || !ChildElements(item).empty())
					return Fail(L"x:String 不允许属性或子元素。", error);
				output.push_back(item->InnerText());
			}
			return true;
		}

		bool ParseListItem(
			const Element& item,
			DesignValue& value,
			std::wstring& error)
		{
			const auto itemName = FromUtf8(item->LocalName());
			if (!Equals(itemName, L"ListViewItem")
				&& !Equals(itemName, L"ListBoxItem"))
				return Fail(L"列表项必须是 ListViewItem 或 ListBoxItem。", error);
			if (!ValidateAttributes(item,
				{ L"Content", L"Text", L"SubText", L"IsChecked", L"IsSelected", L"IsEnabled" }, error))
				return false;
			value = DesignValue::object();
			value["text"] = ToUtf8(Attribute(item, L"Content").value_or(
				Attribute(item, L"Text").value_or(L"")));
			value["subText"] = ToUtf8(Attribute(item, L"SubText").value_or(L""));
			for (const auto& [attributeName, key, defaultValue] : {
				std::tuple{ L"IsChecked", "checked", false },
				std::tuple{ L"IsSelected", "selected", false },
				std::tuple{ L"IsEnabled", "enabled", true } })
			{
				bool parsed = defaultValue;
				if (!ReadBoolAttribute(item, attributeName, defaultValue, parsed, error))
					return false;
				value[key] = parsed;
			}
			for (const auto& child : ChildElements(item))
			{
				DiagnosticContext childContext(*this, child);
				const auto childName = FromUtf8(child->LocalName());
				if ((!Equals(childName, L"ListViewItem.SubItems")
					&& !Equals(childName, L"ListBoxItem.SubItems"))
					|| value.contains("subItems"))
					return Fail(L"列表项仅允许一个 SubItems 属性元素。", error);
				DesignValue subItems;
				if (!ParseStringItems(child, subItems, error)) return false;
				if (!subItems.empty()) value["subItems"] = std::move(subItems);
			}
			return true;
		}

		bool TryParseContentItem(
			const Element& item,
			size_t nodeIndex,
			UIClass type,
			bool& handled,
			std::wstring& error)
		{
			handled = false;
			const auto name = FromUtf8(item->LocalName());
			auto& extra = _document.Nodes[nodeIndex].Extra;
			auto append = [&](DesignValue value)
			{
				if (!extra.contains("items")) extra["items"] = DesignValue::array();
				if (!extra["items"].is_array()) return false;
				extra["items"].push_back(std::move(value));
				return true;
			};
			if (type == UIClass::UI_ComboBox && Equals(name, L"ComboBoxItem"))
			{
				handled = true;
				if (!ValidateAttributes(item, { L"Content" }, error)
					|| !ChildElements(item).empty())
					return Fail(L"ComboBoxItem 仅支持 Content 属性。", error);
				return append(ToUtf8(Attribute(item, L"Content").value_or(L"")));
			}
			if ((type == UIClass::UI_ListView || type == UIClass::UI_ListBox)
				&& (Equals(name, L"ListViewItem") || Equals(name, L"ListBoxItem")))
			{
				handled = true;
				DesignValue value;
				return ParseListItem(item, value, error) && append(std::move(value));
			}
			return true;
		}

		bool ParsePointText(
			const std::wstring& text,
			double& x,
			double& y)
		{
			const auto comma = text.find(L',');
			if (comma == std::wstring::npos
				|| text.find(L',', comma + 1) != std::wstring::npos)
				return false;
			return TryParseDouble(Trim(text.substr(0, comma)), x)
				&& TryParseDouble(Trim(text.substr(comma + 1)), y);
		}

		bool ParseBrushColor(
			const std::wstring& text,
			DesignValue& output,
			std::wstring& error)
		{
			BindingValue converted;
			std::wstring conversionError;
			if (!DesignerStyleSheetUtils::TryConvertValue(
				DesignerStyleValue{ DesignerStyleValueKind::Color, text },
				converted, &conversionError))
				return Fail(L"画刷颜色无效：" + text, error);
			D2D1_COLOR_F color{};
			if (!converted.TryGet(color))
				return Fail(L"画刷颜色无法转换：" + text, error);
			output = DesignValue{
				{ "r", static_cast<double>(color.r) },
				{ "g", static_cast<double>(color.g) },
				{ "b", static_cast<double>(color.b) },
				{ "a", static_cast<double>(color.a) } };
			return true;
		}

		bool ParseGradientStops(
			const std::vector<Element>& elements,
			DesignValue& output,
			std::wstring& error)
		{
			output = DesignValue::array();
			for (const auto& element : elements)
			{
				DiagnosticContext context(*this, element);
				const auto name = FromUtf8(element->LocalName());
				if (name.find(L".GradientStops") != std::wstring::npos)
				{
					if (!ValidateAttributes(element, {}, error)) return false;
					DesignValue nested;
					if (!ParseGradientStops(ChildElements(element), nested, error))
						return false;
					for (auto& stop : nested.ArrayItems())
						output.push_back(std::move(stop));
					continue;
				}
				if (!Equals(name, L"GradientStop"))
					return Fail(L"渐变画刷仅允许 GradientStop 子元素。", error);
				if (!ValidateAttributes(element, { L"Color", L"Offset" }, error)
					|| !ChildElements(element).empty())
					return Fail(L"GradientStop 不允许包含子元素。", error);
				const auto colorText = Attribute(element, L"Color");
				if (!colorText)
					return Fail(L"GradientStop 必须指定 Color。", error);
				double offset = 0.0;
				if (!ReadDoubleAttribute(element, L"Offset", 0.0, offset, error)
					|| offset < 0.0 || offset > 1.0)
					return Fail(L"GradientStop Offset 必须位于 0 到 1。", error);
				DesignValue color;
				if (!ParseBrushColor(*colorText, color, error)) return false;
				output.push_back(DesignValue{
					{ "offset", offset }, { "color", std::move(color) } });
			}
			if (output.size() < 2)
				return Fail(L"渐变画刷至少需要两个 GradientStop。", error);
			return true;
		}

		bool ParseBrushElement(
			const Element& brush,
			DesignValue& output,
			std::wstring& error)
		{
			DiagnosticContext brushContext(*this, brush);
			const auto name = FromUtf8(brush->LocalName());
			output = DesignValue::object();
			if (Equals(name, L"SolidColorBrush"))
			{
				if (!ValidateAttributes(brush, { L"Key", L"Color", L"Opacity" }, error)
					|| !ChildElements(brush).empty())
					return Fail(L"SolidColorBrush 不允许包含子元素。", error);
				const auto colorText = Attribute(brush, L"Color");
				if (!colorText)
					return Fail(L"SolidColorBrush 必须指定 Color。", error);
				output["type"] = "solid";
				if (!ParseBrushColor(*colorText, output["color"], error)) return false;
			}
			else if (Equals(name, L"ImageBrush"))
			{
				if (!ValidateAttributes(brush,
					{ L"Key", L"ImageSource", L"Source", L"Stretch",
					  L"AlignmentX", L"AlignmentY", L"Opacity" }, error)
					|| !ChildElements(brush).empty())
					return Fail(L"ImageBrush 当前仅支持属性形式，不允许包含子元素。", error);
				const auto source = Attribute(brush, L"ImageSource").value_or(
					Attribute(brush, L"Source").value_or(L""));
				if (Trim(source).empty())
					return Fail(L"ImageBrush 必须指定 ImageSource。", error);

				const auto stretch = Attribute(brush, L"Stretch").value_or(L"Fill");
				if (Equals(stretch, L"None")) output["stretch"] = "none";
				else if (Equals(stretch, L"Fill")) output["stretch"] = "fill";
				else if (Equals(stretch, L"Uniform")) output["stretch"] = "uniform";
				else if (Equals(stretch, L"UniformToFill"))
					output["stretch"] = "uniformToFill";
				else return Fail(L"ImageBrush Stretch 必须为 None、Fill、Uniform 或 UniformToFill。", error);

				const auto alignmentX = Attribute(brush, L"AlignmentX").value_or(L"Center");
				if (Equals(alignmentX, L"Left")) output["alignmentX"] = "left";
				else if (Equals(alignmentX, L"Center")) output["alignmentX"] = "center";
				else if (Equals(alignmentX, L"Right")) output["alignmentX"] = "right";
				else return Fail(L"ImageBrush AlignmentX 必须为 Left、Center 或 Right。", error);

				const auto alignmentY = Attribute(brush, L"AlignmentY").value_or(L"Center");
				if (Equals(alignmentY, L"Top")) output["alignmentY"] = "top";
				else if (Equals(alignmentY, L"Center")) output["alignmentY"] = "center";
				else if (Equals(alignmentY, L"Bottom")) output["alignmentY"] = "bottom";
				else return Fail(L"ImageBrush AlignmentY 必须为 Top、Center 或 Bottom。", error);

				output["type"] = "image";
				output["source"] = ToUtf8(Trim(source));
			}
			else if (Equals(name, L"LinearGradientBrush")
				|| Equals(name, L"RadialGradientBrush"))
			{
				const bool radial = Equals(name, L"RadialGradientBrush");
				if (!ValidateAttributes(brush,
					{ L"StartPoint", L"EndPoint", L"Center", L"GradientOrigin",
					  L"RadiusX", L"RadiusY", L"MappingMode", L"Opacity", L"Key" }, error))
					return false;
				output["type"] = radial ? "radial" : "linear";
				const auto mapping = Attribute(brush, L"MappingMode").value_or(
					L"RelativeToBoundingBox");
				if (!Equals(mapping, L"Absolute")
					&& !Equals(mapping, L"RelativeToBoundingBox"))
					return Fail(L"MappingMode 必须为 Absolute 或 RelativeToBoundingBox。", error);
				output["mapping"] = Equals(mapping, L"Absolute")
					? "absolute" : "relative";
				auto readPoint = [&](const wchar_t* attribute,
					double defaultX, double defaultY, const char* xKey, const char* yKey)
				{
					double x = defaultX;
					double y = defaultY;
					if (const auto text = Attribute(brush, attribute);
						text && !ParsePointText(*text, x, y)) return false;
					output[xKey] = x;
					output[yKey] = y;
					return true;
				};
				if (radial)
				{
					if (!readPoint(L"Center", 0.5, 0.5, "centerX", "centerY")
						|| !readPoint(L"GradientOrigin", 0.5, 0.5,
							"originX", "originY"))
						return Fail(L"径向渐变点必须使用 x,y 格式。", error);
					double radiusX = 0.5;
					double radiusY = 0.5;
					if (!ReadDoubleAttribute(brush, L"RadiusX", 0.5, radiusX, error)
						|| !ReadDoubleAttribute(brush, L"RadiusY", 0.5, radiusY, error)
						|| radiusX < 0.0 || radiusY < 0.0)
						return Fail(L"径向渐变半径必须是非负数。", error);
					output["radiusX"] = radiusX;
					output["radiusY"] = radiusY;
				}
				else if (!readPoint(L"StartPoint", 0.0, 0.0,
					"startX", "startY")
					|| !readPoint(L"EndPoint", 1.0, 1.0, "endX", "endY"))
					return Fail(L"线性渐变点必须使用 x,y 格式。", error);
				if (!ParseGradientStops(
					ChildElements(brush), output["stops"], error)) return false;
			}
			else return Fail(L"Control.Foreground 仅支持 SolidColorBrush、"
				L"LinearGradientBrush、RadialGradientBrush 和 ImageBrush。", error);

			double opacity = 1.0;
			if (!ReadDoubleAttribute(brush, L"Opacity", 1.0, opacity, error)
				|| opacity < 0.0 || opacity > 1.0)
				return Fail(L"画刷 Opacity 必须位于 0 到 1。", error);
			output["opacity"] = opacity;
			return true;
		}

		bool ParseBrush(
			const Element& property,
			DesignValue& output,
			std::wstring& error)
		{
			if (!ValidateAttributes(property, {}, error)) return false;
			const auto brushes = ChildElements(property);
			if (brushes.size() != 1)
				return Fail(L"Control.Foreground 必须且只能包含一个画刷。", error);
			return ParseBrushElement(brushes.front(), output, error);
		}

		bool ParseMatrixText(
			std::wstring text,
			std::array<double, 6>& output)
		{
			for (auto& ch : text)
				if (ch == L',') ch = L' ';
			std::wistringstream stream(text);
			for (auto& value : output)
			{
				if (!(stream >> value) || !std::isfinite(value)) return false;
			}
			std::wstring trailing;
			return !(stream >> trailing);
		}

		bool ParseRectText(
			std::wstring text,
			std::array<double, 4>& output)
		{
			for (auto& ch : text)
				if (ch == L',') ch = L' ';
			std::wistringstream stream(text);
			for (auto& value : output)
			{
				if (!(stream >> value) || !std::isfinite(value)) return false;
			}
			std::wstring trailing;
			return !(stream >> trailing);
		}

		bool ParsePathSegment(
			const Element& element,
			DesignValue& output,
			std::wstring& error)
		{
			DiagnosticContext context(*this, element);
			const auto name = FromUtf8(element->LocalName());
			if (!ChildElements(element).empty())
				return Fail(L"PathSegment 不允许包含子元素。", error);
			auto requirePoint = [&](const wchar_t* attribute,
				const char* xKey, const char* yKey) -> bool
			{
				const auto text = Attribute(element, attribute);
				double x = 0.0;
				double y = 0.0;
				if (!text || !ParsePointText(*text, x, y))
					return Fail(std::wstring(attribute) + L" 必须使用 x,y 格式。", error);
				output[xKey] = x;
				output[yKey] = y;
				return true;
			};
			output = DesignValue::object();
			if (Equals(name, L"LineSegment"))
			{
				if (!ValidateAttributes(element, { L"Point" }, error)) return false;
				output["type"] = "line";
				return requirePoint(L"Point", "x", "y");
			}
			if (Equals(name, L"BezierSegment"))
			{
				if (!ValidateAttributes(
					element, { L"Point1", L"Point2", L"Point3" }, error)) return false;
				output["type"] = "bezier";
				return requirePoint(L"Point1", "x1", "y1")
					&& requirePoint(L"Point2", "x2", "y2")
					&& requirePoint(L"Point3", "x3", "y3");
			}
			if (Equals(name, L"QuadraticBezierSegment"))
			{
				if (!ValidateAttributes(
					element, { L"Point1", L"Point2" }, error)) return false;
				output["type"] = "quadratic";
				return requirePoint(L"Point1", "x1", "y1")
					&& requirePoint(L"Point2", "x2", "y2");
			}
			if (!Equals(name, L"ArcSegment"))
				return Fail(L"PathFigure 仅支持 LineSegment、BezierSegment、"
					L"QuadraticBezierSegment 和 ArcSegment。", error);
			if (!ValidateAttributes(element,
				{ L"Point", L"Size", L"RotationAngle", L"IsLargeArc",
				  L"SweepDirection" }, error)) return false;
			output["type"] = "arc";
			if (!requirePoint(L"Point", "x", "y")) return false;
			double width = 0.0;
			double height = 0.0;
			const auto sizeText = Attribute(element, L"Size");
			if (!sizeText || !ParsePointText(*sizeText, width, height)
				|| width < 0.0 || height < 0.0)
				return Fail(L"ArcSegment.Size 必须为非负的 width,height。", error);
			double rotation = 0.0;
			bool large = false;
			if (!ReadDoubleAttribute(
				element, L"RotationAngle", 0.0, rotation, error)
				|| !ReadBoolAttribute(element, L"IsLargeArc", false, large, error))
				return false;
			const auto sweep = Attribute(element, L"SweepDirection").value_or(
				L"Counterclockwise");
			if (!Equals(sweep, L"Clockwise")
				&& !Equals(sweep, L"Counterclockwise"))
				return Fail(L"SweepDirection 必须为 Clockwise 或 Counterclockwise。", error);
			output["width"] = width;
			output["height"] = height;
			output["rotation"] = rotation;
			output["large"] = large;
			output["sweep"] = Equals(sweep, L"Clockwise")
				? "clockwise" : "counterclockwise";
			return true;
		}

		bool ParsePathFigure(
			const Element& element,
			DesignValue& output,
			std::wstring& error)
		{
			DiagnosticContext context(*this, element);
			if (!Equals(FromUtf8(element->LocalName()), L"PathFigure"))
				return Fail(L"PathGeometry 仅允许 PathFigure。", error);
			if (!ValidateAttributes(
				element, { L"StartPoint", L"IsClosed", L"IsFilled" }, error))
				return false;
			double startX = 0.0;
			double startY = 0.0;
			if (const auto text = Attribute(element, L"StartPoint");
				text && !ParsePointText(*text, startX, startY))
				return Fail(L"PathFigure.StartPoint 必须使用 x,y 格式。", error);
			bool closed = false;
			bool filled = true;
			if (!ReadBoolAttribute(element, L"IsClosed", false, closed, error)
				|| !ReadBoolAttribute(element, L"IsFilled", true, filled, error))
				return false;
			output = DesignValue{
				{ "startX", startX }, { "startY", startY },
				{ "closed", closed }, { "filled", filled },
				{ "segments", DesignValue::array() } };
			bool usedSegmentsProperty = false;
			bool usedDirectSegments = false;
			auto appendSegment = [&](const Element& child) -> bool
			{
				DesignValue segment;
				if (!ParsePathSegment(child, segment, error)) return false;
				output["segments"].push_back(std::move(segment));
				return true;
			};
			for (const auto& child : ChildElements(element))
			{
				const auto childName = FromUtf8(child->LocalName());
				if (Equals(childName, L"PathFigure.Segments"))
				{
					if (usedSegmentsProperty || usedDirectSegments
						|| !ValidateAttributes(child, {}, error))
						return Fail(L"PathFigure 仅允许一个 Segments 属性元素。", error);
					usedSegmentsProperty = true;
					for (const auto& nested : ChildElements(child))
						if (!appendSegment(nested)) return false;
				}
				else
				{
					if (usedSegmentsProperty)
						return Fail(L"PathFigure.Segments 不能与直接 Segment 混用。", error);
					usedDirectSegments = true;
					if (!appendSegment(child)) return false;
				}
			}
			return true;
		}

		bool IsGeometryTransformProperty(
			const std::wstring& propertyName,
			const std::wstring& ownerName) const
		{
			return Equals(propertyName, L"Geometry.Transform")
				|| Equals(propertyName, ownerName + L".Transform");
		}

		bool ParseGeometryTransformProperty(
			const Element& property,
			DesignValue& output,
			std::wstring& error)
		{
			if (!ValidateAttributes(property, {}, error)) return false;
			const auto transforms = ChildElements(property);
			if (transforms.size() != 1)
				return Fail(L"Geometry.Transform 必须且只能包含一个变换对象。", error);
			output = DesignValue::array();
			if (!ParseTransformElement(transforms.front(), output, error)) return false;
			if (output.empty())
				return Fail(L"Geometry.Transform 不能是空 TransformGroup。", error);
			return true;
		}

		bool ParseGeometryElement(
			const Element& element,
			DesignValue& output,
			std::wstring& error,
			bool allowResourceKey = false)
		{
			DiagnosticContext context(*this, element);
			const auto name = FromUtf8(element->LocalName());
			output = DesignValue::object();
			auto parseOnlyTransformChild = [&]() -> bool
			{
				bool usedTransform = false;
				for (const auto& child : ChildElements(element))
				{
					const auto childName = FromUtf8(child->LocalName());
					if (usedTransform || !IsGeometryTransformProperty(childName, name))
						return Fail(name + L" 仅允许一个 Geometry.Transform 子元素。", error);
					usedTransform = true;
					if (!ParseGeometryTransformProperty(
						child, output["transform"], error)) return false;
				}
				return true;
			};
			if (Equals(name, L"RectangleGeometry"))
			{
				if (!ValidateAttributes(
					element, { L"Rect", L"RadiusX", L"RadiusY" }, error,
					allowResourceKey)) return false;
				const auto text = Attribute(element, L"Rect");
				std::array<double, 4> values{};
				if (!text || !ParseRectText(*text, values)
					|| values[2] < 0.0 || values[3] < 0.0)
					return Fail(L"RectangleGeometry.Rect 必须为 x,y,width,height，"
						L"且宽高为非负数。", error);
				double radiusX = 0.0;
				double radiusY = 0.0;
				if (!ReadDoubleAttribute(element, L"RadiusX", 0.0, radiusX, error)
					|| !ReadDoubleAttribute(element, L"RadiusY", 0.0, radiusY, error)
					|| radiusX < 0.0 || radiusY < 0.0)
					return Fail(L"RectangleGeometry 圆角半径必须是非负数。", error);
				output = DesignValue{
					{ "type", "rectangle" }, { "x", values[0] },
					{ "y", values[1] }, { "width", values[2] },
					{ "height", values[3] }, { "radiusX", radiusX },
					{ "radiusY", radiusY } };
				return parseOnlyTransformChild();
			}
			if (Equals(name, L"EllipseGeometry"))
			{
				if (!ValidateAttributes(
					element, { L"Center", L"RadiusX", L"RadiusY" }, error,
					allowResourceKey)) return false;
				double centerX = 0.0;
				double centerY = 0.0;
				if (const auto text = Attribute(element, L"Center");
					text && !ParsePointText(*text, centerX, centerY))
					return Fail(L"EllipseGeometry.Center 必须使用 x,y 格式。", error);
				double radiusX = 0.0;
				double radiusY = 0.0;
				if (!ReadDoubleAttribute(element, L"RadiusX", 0.0, radiusX, error)
					|| !ReadDoubleAttribute(element, L"RadiusY", 0.0, radiusY, error)
					|| radiusX < 0.0 || radiusY < 0.0)
					return Fail(L"EllipseGeometry 半径必须是非负数。", error);
				output = DesignValue{
					{ "type", "ellipse" }, { "centerX", centerX },
					{ "centerY", centerY }, { "radiusX", radiusX },
					{ "radiusY", radiusY } };
				return parseOnlyTransformChild();
			}
			if (Equals(name, L"PathGeometry"))
			{
				if (!ValidateAttributes(
					element, { L"FillRule" }, error, allowResourceKey)) return false;
				const auto fillRule = Attribute(element, L"FillRule").value_or(L"EvenOdd");
				if (!Equals(fillRule, L"EvenOdd") && !Equals(fillRule, L"Nonzero"))
					return Fail(L"PathGeometry.FillRule 必须为 EvenOdd 或 Nonzero。", error);
				output["type"] = "path";
				output["fillRule"] = Equals(fillRule, L"Nonzero")
					? "nonzero" : "evenodd";
				output["figures"] = DesignValue::array();
				bool usedFiguresProperty = false;
				bool usedDirectFigures = false;
				bool usedTransform = false;
				auto appendFigure = [&](const Element& child) -> bool
				{
					DesignValue figure;
					if (!ParsePathFigure(child, figure, error)) return false;
					output["figures"].push_back(std::move(figure));
					return true;
				};
				for (const auto& child : ChildElements(element))
				{
					const auto childName = FromUtf8(child->LocalName());
					if (IsGeometryTransformProperty(childName, name))
					{
						if (usedTransform) return Fail(L"Geometry.Transform 不能重复。", error);
						usedTransform = true;
						if (!ParseGeometryTransformProperty(
							child, output["transform"], error)) return false;
					}
					else if (Equals(childName, L"PathGeometry.Figures"))
					{
						if (usedFiguresProperty || usedDirectFigures
							|| !ValidateAttributes(child, {}, error))
							return Fail(L"PathGeometry 仅允许一个 Figures 属性元素。", error);
						usedFiguresProperty = true;
						for (const auto& nested : ChildElements(child))
							if (!appendFigure(nested)) return false;
					}
					else
					{
						if (usedFiguresProperty)
							return Fail(L"PathGeometry.Figures 不能与直接 PathFigure 混用。", error);
						usedDirectFigures = true;
						if (!appendFigure(child)) return false;
					}
				}
				return true;
			}
			if (!Equals(name, L"GeometryGroup"))
				return Fail(L"Clip 仅支持 RectangleGeometry、EllipseGeometry、"
					L"PathGeometry 和 GeometryGroup。", error);
			if (!ValidateAttributes(
				element, { L"FillRule" }, error, allowResourceKey)) return false;
			const auto fillRule = Attribute(element, L"FillRule").value_or(L"EvenOdd");
			if (!Equals(fillRule, L"EvenOdd") && !Equals(fillRule, L"Nonzero"))
				return Fail(L"GeometryGroup.FillRule 必须为 EvenOdd 或 Nonzero。", error);
			output["type"] = "group";
			output["fillRule"] = Equals(fillRule, L"Nonzero")
				? "nonzero" : "evenodd";
			output["children"] = DesignValue::array();
			bool usedChildrenProperty = false;
			bool usedDirectChildren = false;
			bool usedTransform = false;
			auto appendChild = [&](const Element& child) -> bool
			{
				DesignValue value;
				if (!ParseGeometryElement(child, value, error)) return false;
				output["children"].push_back(std::move(value));
				return true;
			};
			for (const auto& child : ChildElements(element))
			{
				const auto childName = FromUtf8(child->LocalName());
				if (IsGeometryTransformProperty(childName, name))
				{
					if (usedTransform) return Fail(L"Geometry.Transform 不能重复。", error);
					usedTransform = true;
					if (!ParseGeometryTransformProperty(
						child, output["transform"], error)) return false;
				}
				else if (Equals(childName, L"GeometryGroup.Children"))
				{
					if (usedChildrenProperty || usedDirectChildren
						|| !ValidateAttributes(child, {}, error))
						return Fail(L"GeometryGroup 仅允许一个 Children 属性元素。", error);
					usedChildrenProperty = true;
					for (const auto& nested : ChildElements(child))
						if (!appendChild(nested)) return false;
				}
				else
				{
					if (usedChildrenProperty)
						return Fail(L"GeometryGroup.Children 不能与直接子几何混用。", error);
					usedDirectChildren = true;
					if (!appendChild(child)) return false;
				}
			}
			return true;
		}

		bool ParseClip(
			const Element& property,
			DesignValue& output,
			std::wstring& error)
		{
			if (!ValidateAttributes(property, {}, error)) return false;
			const auto geometries = ChildElements(property);
			if (geometries.size() != 1)
				return Fail(L"Control.Clip 必须且只能包含一个几何对象。", error);
			return ParseGeometryElement(geometries.front(), output, error);
		}

		bool ParseTransformElement(
			const Element& element,
			DesignValue& output,
			std::wstring& error,
			bool allowResourceKey = false)
		{
			DiagnosticContext context(*this, element);
			const auto name = FromUtf8(element->LocalName());
			if (Equals(name, L"TransformGroup"))
			{
				if (!ValidateAttributes(element, {}, error, allowResourceKey)) return false;
				bool usedChildrenProperty = false;
				bool usedDirectChildren = false;
				for (const auto& child : ChildElements(element))
				{
					const auto childName = FromUtf8(child->LocalName());
					if (Equals(childName, L"TransformGroup.Children"))
					{
						if (usedChildrenProperty || usedDirectChildren
							|| !ValidateAttributes(child, {}, error))
							return Fail(L"TransformGroup 仅允许一个 Children 属性元素。", error);
						usedChildrenProperty = true;
						for (const auto& nested : ChildElements(child))
							if (!ParseTransformElement(nested, output, error)) return false;
					}
					else
					{
						if (usedChildrenProperty)
							return Fail(L"TransformGroup.Children 不能与直接子变换混用。", error);
						usedDirectChildren = true;
						if (!ParseTransformElement(child, output, error)) return false;
					}
				}
				return true;
			}

			DesignValue operation = DesignValue::object();
			auto read = [&](const wchar_t* attribute,
				double defaultValue, const char* key) -> bool
			{
				double value = defaultValue;
				if (!ReadDoubleAttribute(
					element, attribute, defaultValue, value, error)) return false;
				operation[key] = value;
				return true;
			};
			if (Equals(name, L"MatrixTransform"))
			{
				if (!ValidateAttributes(element, { L"Matrix" }, error, allowResourceKey)
					|| !ChildElements(element).empty())
					return Fail(L"MatrixTransform 不允许包含子元素。", error);
				const auto matrixText = Attribute(element, L"Matrix");
				std::array<double, 6> values{};
				if (!matrixText || !ParseMatrixText(*matrixText, values))
					return Fail(L"MatrixTransform.Matrix 必须包含六个有限数值。", error);
				operation["type"] = "matrix";
				for (size_t index = 0; index < values.size(); ++index)
					operation[std::array{ "m11", "m12", "m21", "m22", "dx", "dy" }[index]]
						= values[index];
			}
			else if (Equals(name, L"TranslateTransform"))
			{
				if (!ValidateAttributes(element, { L"X", L"Y" }, error, allowResourceKey)
					|| !ChildElements(element).empty())
					return Fail(L"TranslateTransform 不允许包含子元素。", error);
				operation["type"] = "translate";
				if (!read(L"X", 0.0, "x") || !read(L"Y", 0.0, "y")) return false;
			}
			else if (Equals(name, L"ScaleTransform"))
			{
				if (!ValidateAttributes(element,
					{ L"ScaleX", L"ScaleY", L"CenterX", L"CenterY" }, error,
					allowResourceKey)
					|| !ChildElements(element).empty())
					return Fail(L"ScaleTransform 不允许包含子元素。", error);
				operation["type"] = "scale";
				if (!read(L"ScaleX", 1.0, "scaleX")
					|| !read(L"ScaleY", 1.0, "scaleY")
					|| !read(L"CenterX", 0.0, "centerX")
					|| !read(L"CenterY", 0.0, "centerY")) return false;
			}
			else if (Equals(name, L"RotateTransform"))
			{
				if (!ValidateAttributes(element,
					{ L"Angle", L"CenterX", L"CenterY" }, error,
					allowResourceKey)
					|| !ChildElements(element).empty())
					return Fail(L"RotateTransform 不允许包含子元素。", error);
				operation["type"] = "rotate";
				if (!read(L"Angle", 0.0, "angle")
					|| !read(L"CenterX", 0.0, "centerX")
					|| !read(L"CenterY", 0.0, "centerY")) return false;
			}
			else if (Equals(name, L"SkewTransform"))
			{
				if (!ValidateAttributes(element,
					{ L"AngleX", L"AngleY", L"CenterX", L"CenterY" }, error,
					allowResourceKey)
					|| !ChildElements(element).empty())
					return Fail(L"SkewTransform 不允许包含子元素。", error);
				operation["type"] = "skew";
				if (!read(L"AngleX", 0.0, "angleX")
					|| !read(L"AngleY", 0.0, "angleY")
					|| !read(L"CenterX", 0.0, "centerX")
					|| !read(L"CenterY", 0.0, "centerY")) return false;
			}
			else return Fail(L"RenderTransform 仅支持 Matrix、Translate、Scale、"
				L"Rotate、Skew 和 TransformGroup。", error);
			output.push_back(std::move(operation));
			return true;
		}

		bool ParseTransform(
			const Element& property,
			DesignValue& output,
			std::wstring& error)
		{
			if (!ValidateAttributes(property, {}, error)) return false;
			const auto transforms = ChildElements(property);
			if (transforms.size() != 1)
				return Fail(L"Control.RenderTransform 必须且只能包含一个变换对象。", error);
			output = DesignValue::array();
			if (!ParseTransformElement(transforms.front(), output, error)) return false;
			if (output.empty())
				return Fail(L"RenderTransform 不能是空 TransformGroup。", error);
			return true;
		}

		bool ParseDoubleItems(
			const Element& property,
			DesignValue& output,
			std::wstring& error)
		{
			if (!ValidateAttributes(property, {}, error)) return false;
			output = DesignValue::array();
			for (const auto& item : ChildElements(property))
			{
				DiagnosticContext itemContext(*this, item);
				if (!Equals(FromUtf8(item->LocalName()), L"Double")
					|| (!Equals(FromUtf8(item->Prefix()), L"x")
						&& !Equals(FromUtf8(item->NamespaceURI()),
							L"http://schemas.microsoft.com/winfx/2006/xaml")))
					return Fail(L"数值集合仅允许 x:Double。", error);
				if (!ValidateAttributes(item, {}, error) || !ChildElements(item).empty())
					return Fail(L"x:Double 不允许属性或子元素。", error);
				double value = 0.0;
				if (!TryParseDouble(Trim(FromUtf8(item->InnerText())), value))
					return Fail(L"x:Double 必须是有限数值。", error);
				output.push_back(value);
			}
			return true;
		}

		bool ParseNavigationItems(
			const Element& property,
			DesignValue& output,
			std::wstring& error)
		{
			if (!ValidateAttributes(property, {}, error)) return false;
			output = DesignValue::array();
			for (const auto& item : ChildElements(property))
			{
				DiagnosticContext itemContext(*this, item);
				const auto itemName = FromUtf8(item->LocalName());
				const bool headerElement = Equals(itemName, L"NavigationViewHeader");
				const bool separatorElement = Equals(itemName, L"NavigationViewSeparator");
				if (!Equals(itemName, L"NavigationViewItem")
					&& !headerElement && !separatorElement)
					return Fail(L"NavigationView.Items 仅允许 NavigationViewItem、"
						L"NavigationViewHeader 或 NavigationViewSeparator。", error);
				if (!ValidateAttributes(item,
					{ L"Text", L"Header", L"Value", L"BadgeText", L"Kind",
					  L"Icon", L"IsEnabled", L"IsSelected", L"Tag" }, error)
					|| !ChildElements(item).empty())
					return Fail(L"导航项不允许包含子元素。", error);

				int kind = headerElement ? 1 : separatorElement ? 2 : 0;
				if (const auto text = Attribute(item, L"Kind"); text
					&& !TryParseEnum(*text, { L"Item", L"Header", L"Separator" }, kind))
					return Fail(L"NavigationViewItem Kind 无效。", error);
				const bool enabledDefault = kind == 0;
				bool enabled = enabledDefault;
				bool selected = false;
				if (!ReadBoolAttribute(item, L"IsEnabled", enabledDefault, enabled, error)
					|| !ReadBoolAttribute(item, L"IsSelected", false, selected, error))
					return false;
				unsigned long long tag = 0;
				if (const auto text = Attribute(item, L"Tag");
					text && !TryParseUnsignedInteger(*text, tag))
					return Fail(L"NavigationViewItem Tag 必须是非负整数。", error);
				output.push_back(DesignValue{
					{ "text", ToUtf8(Attribute(item, L"Text").value_or(
						Attribute(item, L"Header").value_or(L""))) },
					{ "value", ToUtf8(Attribute(item, L"Value").value_or(L"")) },
					{ "badgeText", ToUtf8(Attribute(item, L"BadgeText").value_or(L"")) },
					{ "icon", ToUtf8(Attribute(item, L"Icon").value_or(L"")) },
					{ "kind", kind }, { "enabled", enabled },
					{ "selected", selected }, { "tag", tag } });
			}
			return true;
		}

		bool ParseBreadcrumbItems(
			const Element& property,
			DesignValue& output,
			std::wstring& error)
		{
			if (!ValidateAttributes(property, {}, error)) return false;
			output = DesignValue::array();
			for (const auto& item : ChildElements(property))
			{
				DiagnosticContext itemContext(*this, item);
				if (!Equals(FromUtf8(item->LocalName()), L"BreadcrumbBarItem"))
					return Fail(L"BreadcrumbBar.Items 仅允许 BreadcrumbBarItem。", error);
				if (!ValidateAttributes(item,
					{ L"Text", L"Header", L"Value", L"IsEnabled", L"Tag" }, error)
					|| !ChildElements(item).empty())
					return Fail(L"BreadcrumbBarItem 不允许包含子元素。", error);
				bool enabled = true;
				if (!ReadBoolAttribute(item, L"IsEnabled", true, enabled, error))
					return false;
				unsigned long long tag = 0;
				if (const auto text = Attribute(item, L"Tag");
					text && !TryParseUnsignedInteger(*text, tag))
					return Fail(L"BreadcrumbBarItem Tag 必须是非负整数。", error);
				output.push_back(DesignValue{
					{ "text", ToUtf8(Attribute(item, L"Text").value_or(
						Attribute(item, L"Header").value_or(L""))) },
					{ "value", ToUtf8(Attribute(item, L"Value").value_or(L"")) },
					{ "enabled", enabled }, { "tag", tag } });
			}
			return true;
		}

		bool ParseFilterBarItems(
			const Element& property,
			DesignValue& output,
			std::wstring& error)
		{
			if (!ValidateAttributes(property, {}, error)) return false;
			output = DesignValue::array();
			for (const auto& item : ChildElements(property))
			{
				DiagnosticContext itemContext(*this, item);
				if (!Equals(FromUtf8(item->LocalName()), L"FilterBarItem"))
					return Fail(L"FilterBar.Items 仅允许 FilterBarItem。", error);
				if (!ValidateAttributes(item,
					{ L"Text", L"Value", L"IsSelected", L"IsEnabled", L"Tag" }, error)
					|| !ChildElements(item).empty())
					return Fail(L"FilterBarItem 不允许包含子元素。", error);
				bool selected = false;
				bool enabled = true;
				if (!ReadBoolAttribute(item, L"IsSelected", false, selected, error)
					|| !ReadBoolAttribute(item, L"IsEnabled", true, enabled, error))
					return false;
				unsigned long long tag = 0;
				if (const auto text = Attribute(item, L"Tag");
					text && !TryParseUnsignedInteger(*text, tag))
					return Fail(L"FilterBarItem Tag 必须是非负整数。", error);
				output.push_back(DesignValue{
					{ "text", ToUtf8(Attribute(item, L"Text").value_or(L"")) },
					{ "value", ToUtf8(Attribute(item, L"Value").value_or(L"")) },
					{ "selected", selected }, { "enabled", enabled }, { "tag", tag } });
			}
			return true;
		}

		bool ParseChartSeries(
			const Element& property,
			DesignValue& output,
			std::wstring& error)
		{
			if (!ValidateAttributes(property, {}, error)) return false;
			output = DesignValue::array();
			for (const auto& seriesElement : ChildElements(property))
			{
				DiagnosticContext seriesContext(*this, seriesElement);
				if (!Equals(FromUtf8(seriesElement->LocalName()), L"ChartSeries"))
					return Fail(L"ChartView.Series 仅允许 ChartSeries。", error);
				if (!ValidateAttributes(seriesElement,
					{ L"Name", L"Color", L"IsVisible" }, error)) return false;
				bool visible = true;
				if (!ReadBoolAttribute(seriesElement, L"IsVisible", true, visible, error))
					return false;
				DesignValue series = DesignValue::object();
				series["name"] = ToUtf8(Attribute(seriesElement, L"Name").value_or(L""));
				series["visible"] = visible;
				if (const auto colorText = Attribute(seriesElement, L"Color"))
					if (!ParseBrushColor(*colorText, series["color"], error)) return false;
				DesignValue points = DesignValue::array();
				bool usedPointsProperty = false;
				auto parsePoint = [&](const Element& pointElement) -> bool
				{
					DiagnosticContext pointContext(*this, pointElement);
					if (!Equals(FromUtf8(pointElement->LocalName()), L"ChartPoint"))
						return Fail(L"ChartSeries 仅允许 ChartPoint。", error);
					if (!ValidateAttributes(pointElement,
						{ L"Label", L"Value", L"Color", L"Tag" }, error)
						|| !ChildElements(pointElement).empty())
						return Fail(L"ChartPoint 不允许包含子元素。", error);
					double pointValue = 0.0;
					if (!ReadDoubleAttribute(pointElement, L"Value", 0.0, pointValue, error))
						return false;
					unsigned long long tag = 0;
					if (const auto text = Attribute(pointElement, L"Tag");
						text && !TryParseUnsignedInteger(*text, tag))
						return Fail(L"ChartPoint Tag 必须是非负整数。", error);
					DesignValue point{
						{ "label", ToUtf8(Attribute(pointElement, L"Label").value_or(L"")) },
						{ "value", pointValue }, { "tag", tag } };
					if (const auto colorText = Attribute(pointElement, L"Color"))
					{
						if (!ParseBrushColor(*colorText, point["color"], error)) return false;
						point["useCustomColor"] = true;
					}
					points.push_back(std::move(point));
					return true;
				};
				for (const auto& child : ChildElements(seriesElement))
				{
					const auto childName = FromUtf8(child->LocalName());
					if (Equals(childName, L"ChartSeries.Points"))
					{
						if (usedPointsProperty || !points.empty())
							return Fail(L"ChartSeries.Points 不能重复或与直接 ChartPoint 混用。", error);
						usedPointsProperty = true;
						if (!ValidateAttributes(child, {}, error)) return false;
						for (const auto& point : ChildElements(child))
							if (!parsePoint(point)) return false;
					}
					else
					{
						if (usedPointsProperty)
							return Fail(L"ChartSeries.Points 不能与直接 ChartPoint 混用。", error);
						if (!parsePoint(child)) return false;
					}
				}
				series["points"] = std::move(points);
				output.push_back(std::move(series));
			}
			return true;
		}

		bool ParseReportColumns(
			const Element& property,
			DesignValue& output,
			std::wstring& error)
		{
			if (!ValidateAttributes(property, {}, error)) return false;
			output = DesignValue::array();
			for (const auto& item : ChildElements(property))
			{
				DiagnosticContext itemContext(*this, item);
				if (!Equals(FromUtf8(item->LocalName()), L"ReportColumn"))
					return Fail(L"ReportView.Columns 仅允许 ReportColumn。", error);
				if (!ValidateAttributes(item,
					{ L"Header", L"Width", L"HorizontalAlignment", L"Align", L"IsSortable" }, error)
					|| !ChildElements(item).empty())
					return Fail(L"ReportColumn 不允许包含子元素。", error);
				double width = 120.0;
				if (!ReadDoubleAttribute(item, L"Width", 120.0, width, error)
					|| width < 0.0) return Fail(L"ReportColumn Width 必须是非负数。", error);
				int align = 0;
				const auto alignText = Attribute(item, L"HorizontalAlignment").value_or(
					Attribute(item, L"Align").value_or(L"Left"));
				if (!TryParseEnum(alignText, { L"Left", L"Center", L"Right" }, align))
					return Fail(L"ReportColumn Align 无效。", error);
				bool sortable = true;
				if (!ReadBoolAttribute(item, L"IsSortable", true, sortable, error)) return false;
				output.push_back(DesignValue{
					{ "header", ToUtf8(Attribute(item, L"Header").value_or(L"")) },
					{ "width", width }, { "align", align }, { "sortable", sortable } });
			}
			return true;
		}

		bool ParseReportRows(
			const Element& property,
			DesignValue& output,
			std::wstring& error)
		{
			if (!ValidateAttributes(property, {}, error)) return false;
			output = DesignValue::array();
			for (const auto& item : ChildElements(property))
			{
				DiagnosticContext itemContext(*this, item);
				const auto itemName = FromUtf8(item->LocalName());
				int kind = Equals(itemName, L"ReportGroup") ? 1
					: Equals(itemName, L"ReportSummary") ? 2 : 0;
				if (!Equals(itemName, L"ReportRow")
					&& !Equals(itemName, L"ReportGroup")
					&& !Equals(itemName, L"ReportSummary"))
					return Fail(L"ReportView.Rows 仅允许 ReportRow、ReportGroup 或 ReportSummary。", error);
				if (!ValidateAttributes(item, { L"Caption", L"IsExpanded", L"Tag" }, error))
					return false;
				bool expanded = true;
				if (!ReadBoolAttribute(item, L"IsExpanded", true, expanded, error)) return false;
				unsigned long long tag = 0;
				if (const auto text = Attribute(item, L"Tag");
					text && !TryParseUnsignedInteger(*text, tag))
					return Fail(L"ReportRow Tag 必须是非负整数。", error);
				DesignValue row{
					{ "kind", kind },
					{ "caption", ToUtf8(Attribute(item, L"Caption").value_or(L"")) },
					{ "expanded", expanded }, { "tag", tag } };
				for (const auto& child : ChildElements(item))
				{
					DiagnosticContext childContext(*this, child);
					const auto expected = itemName + L".Cells";
					if (!Equals(FromUtf8(child->LocalName()), expected)
						|| row.contains("cells"))
						return Fail(itemName + L" 仅允许一个 Cells 属性元素。", error);
					DesignValue cells;
					if (!ParseStringItems(child, cells, error)) return false;
					row["cells"] = std::move(cells);
				}
				if (!row.contains("cells")) row["cells"] = DesignValue::array();
				output.push_back(std::move(row));
			}
			return true;
		}

		bool ParseTreeItems(
			const Element& property,
			DesignValue& output,
			std::wstring& error)
		{
			if (!ValidateAttributes(property, {}, error)) return false;
			output = DesignValue::array();
			for (const auto& item : ChildElements(property))
			{
				DiagnosticContext itemContext(*this, item);
				if (!Equals(FromUtf8(item->LocalName()), L"TreeViewItem"))
					return Fail(L"TreeView.Items 仅允许 TreeViewItem。", error);
				if (!ValidateAttributes(item, { L"Header", L"IsExpanded" }, error))
					return false;
				DesignValue value = DesignValue::object();
				value["text"] = ToUtf8(Attribute(item, L"Header").value_or(L""));
				bool expanded = false;
				if (!ReadBoolAttribute(item, L"IsExpanded", false, expanded, error))
					return false;
				value["expand"] = expanded;
				for (const auto& child : ChildElements(item))
				{
					DiagnosticContext childContext(*this, child);
					if (!Equals(FromUtf8(child->LocalName()), L"TreeViewItem.Items")
						|| value.contains("children"))
						return Fail(L"TreeViewItem 仅允许一个 TreeViewItem.Items。", error);
					DesignValue children;
					if (!ParseTreeItems(child, children, error)) return false;
					if (!children.empty()) value["children"] = std::move(children);
				}
				output.push_back(std::move(value));
			}
			return true;
		}

		bool ParseMenuItems(
			const Element& property,
			bool allowSeparator,
			DesignValue& output,
			std::wstring& error)
		{
			if (!ValidateAttributes(property, {}, error)) return false;
			output = DesignValue::array();
			for (const auto& item : ChildElements(property))
			{
				DiagnosticContext itemContext(*this, item);
				const auto itemName = FromUtf8(item->LocalName());
				if (Equals(itemName, L"Separator"))
				{
					if (!allowSeparator)
						return Fail(L"Menu 顶层不支持 Separator。", error);
					if (!ValidateAttributes(item, {}, error)) return false;
					if (!ChildElements(item).empty())
						return Fail(L"Separator 不允许属性或子元素。", error);
					output.push_back(DesignValue{ { "separator", true } });
					continue;
				}
				if (!Equals(itemName, L"MenuItem"))
					return Fail(L"Menu.Items 仅允许 MenuItem。", error);
				if (!ValidateAttributes(item,
					{ L"Header", L"CommandId", L"Shortcut", L"IsEnabled" }, error))
					return false;
				DesignValue value = DesignValue::object();
				value["text"] = ToUtf8(Attribute(item, L"Header").value_or(L""));
				int commandId = 0;
				if (const auto text = Attribute(item, L"CommandId");
					text && !TryParseInteger(*text, commandId))
					return Fail(L"CommandId 必须是整数。", error);
				value["id"] = commandId;
				value["shortcut"] = ToUtf8(Attribute(item, L"Shortcut").value_or(L""));
				bool enabled = true;
				if (!ReadBoolAttribute(item, L"IsEnabled", true, enabled, error))
					return false;
				value["enable"] = enabled;
				for (const auto& child : ChildElements(item))
				{
					DiagnosticContext childContext(*this, child);
					if (!Equals(FromUtf8(child->LocalName()), L"MenuItem.Items")
						|| value.contains("subItems"))
						return Fail(L"MenuItem 仅允许一个 MenuItem.Items。", error);
					DesignValue children;
					if (!ParseMenuItems(child, true, children, error)) return false;
					if (!children.empty()) value["subItems"] = std::move(children);
				}
				output.push_back(std::move(value));
			}
			return true;
		}

		bool TryParseStructuredProperty(
			const Element& property,
			size_t nodeIndex,
			UIClass type,
			bool& handled,
			std::wstring& error)
		{
			handled = false;
			const auto name = FromUtf8(property->LocalName());
			auto& extra = _document.Nodes[nodeIndex].Extra;
			auto beginCollection = [&](const char* key) -> bool
			{
				handled = true;
				if (extra.contains(key))
					return Fail(L"属性元素重复：" + name, error);
				return true;
			};

			const auto foregroundOwner =
				DesignerStyleSheetUtils::UIClassName(type) + L".Foreground";
			if (Equals(name, L"Control.Foreground")
				|| Equals(name, foregroundOwner))
			{
				if (!beginCollection("foregroundBrush")) return false;
				DesignValue brush;
				if (!ParseBrush(property, brush, error)) return false;
				extra["foregroundBrush"] = std::move(brush);
				return true;
			}

			const auto transformOwner =
				DesignerStyleSheetUtils::UIClassName(type) + L".RenderTransform";
			if (Equals(name, L"Control.RenderTransform")
				|| Equals(name, transformOwner))
			{
				if (!beginCollection("renderTransform")) return false;
				DesignValue transform;
				if (!ParseTransform(property, transform, error)) return false;
				extra["renderTransform"] = std::move(transform);
				return true;
			}

			const auto clipOwner =
				DesignerStyleSheetUtils::UIClassName(type) + L".Clip";
			if (Equals(name, L"Control.Clip") || Equals(name, clipOwner))
			{
				if (!beginCollection("clip")) return false;
				DesignValue clip;
				if (!ParseClip(property, clip, error)) return false;
				extra["clip"] = std::move(clip);
				return true;
			}

			const bool navigation = type == UIClass::UI_NavigationView
				|| type == UIClass::UI_SideBar;
			if (navigation && (Equals(name, L"NavigationView.Items")
				|| Equals(name, L"SideBar.Items")))
			{
				if (!beginCollection("navigationItems")) return false;
				DesignValue values;
				if (!ParseNavigationItems(property, values, error)) return false;
				extra["navigationItems"] = std::move(values);
				return true;
			}
			if (type == UIClass::UI_BreadcrumbBar
				&& Equals(name, L"BreadcrumbBar.Items"))
			{
				if (!beginCollection("breadcrumbItems")) return false;
				DesignValue values;
				if (!ParseBreadcrumbItems(property, values, error)) return false;
				extra["breadcrumbItems"] = std::move(values);
				return true;
			}
			if (type == UIClass::UI_FilterBar && Equals(name, L"FilterBar.Items"))
			{
				if (!beginCollection("filterItems")) return false;
				DesignValue values;
				if (!ParseFilterBarItems(property, values, error)) return false;
				extra["filterItems"] = std::move(values);
				return true;
			}
			if (type == UIClass::UI_KpiCard && Equals(name, L"KpiCard.Sparkline"))
			{
				if (!beginCollection("sparkline")) return false;
				DesignValue values;
				if (!ParseDoubleItems(property, values, error)) return false;
				extra["sparkline"] = std::move(values);
				return true;
			}
			if (type == UIClass::UI_ChartView && Equals(name, L"ChartView.Series"))
			{
				if (!beginCollection("series")) return false;
				DesignValue values;
				if (!ParseChartSeries(property, values, error)) return false;
				extra["series"] = std::move(values);
				return true;
			}
			if (type == UIClass::UI_ReportView && Equals(name, L"ReportView.Columns"))
			{
				if (!beginCollection("reportColumns")) return false;
				DesignValue values;
				if (!ParseReportColumns(property, values, error)) return false;
				extra["reportColumns"] = std::move(values);
				return true;
			}
			if (type == UIClass::UI_ReportView && Equals(name, L"ReportView.Rows"))
			{
				if (!beginCollection("reportRows")) return false;
				DesignValue values;
				if (!ParseReportRows(property, values, error)) return false;
				extra["reportRows"] = std::move(values);
				return true;
			}

			if (type == UIClass::UI_ComboBox && Equals(name, L"ComboBox.Items"))
			{
				if (!beginCollection("items")) return false;
				DesignValue values;
				if (!ParseComboBoxItems(property, values, error)) return false;
				extra["items"] = std::move(values);
				return true;
			}
			const bool list = type == UIClass::UI_ListView || type == UIClass::UI_ListBox;
			const bool listColumns = list && (Equals(name, L"ListView.Columns")
				|| Equals(name, L"ListBox.Columns"));
			if (listColumns)
			{
				if (!beginCollection("columns") || !ValidateAttributes(property, {}, error))
					return false;
				DesignValue values = DesignValue::array();
				for (const auto& item : ChildElements(property))
				{
					DiagnosticContext itemContext(*this, item);
					if (!Equals(FromUtf8(item->LocalName()), L"ListViewColumn"))
						return Fail(L"列表列集合仅允许 ListViewColumn。", error);
					if (!ValidateAttributes(item,
						{ L"Header", L"Width", L"HorizontalAlignment" }, error)) return false;
					if (!ChildElements(item).empty())
						return Fail(L"ListViewColumn 不允许子元素。", error);
					DesignValue value = DesignValue::object();
					value["header"] = ToUtf8(Attribute(item, L"Header").value_or(L""));
					double width = 120.0;
					if (!ReadDoubleAttribute(item, L"Width", 120.0, width, error)
						|| width < 0.0) return Fail(L"Width 必须是非负数。", error);
					value["width"] = width;
					int alignment = 0;
					if (const auto text = Attribute(item, L"HorizontalAlignment");
						text && !TryParseEnum(*text, { L"Left", L"Center", L"Right" }, alignment))
						return Fail(L"HorizontalAlignment 必须为 Left、Center 或 Right。", error);
					value["align"] = alignment;
					values.push_back(std::move(value));
				}
				extra["columns"] = std::move(values);
				return true;
			}
			const bool listItems = list && (Equals(name, L"ListView.Items")
				|| Equals(name, L"ListBox.Items"));
			if (listItems)
			{
				if (!beginCollection("items") || !ValidateAttributes(property, {}, error))
					return false;
				DesignValue values = DesignValue::array();
				for (const auto& item : ChildElements(property))
				{
					DiagnosticContext itemContext(*this, item);
					DesignValue value;
					if (!ParseListItem(item, value, error)) return false;
					values.push_back(std::move(value));
				}
				extra["items"] = std::move(values);
				return true;
			}
			const bool dataGrid = type == UIClass::UI_GridView
				|| type == UIClass::UI_PagedGridView;
			const bool dataGridColumns = dataGrid
				&& (Equals(name, L"GridView.Columns")
					|| Equals(name, L"PagedGridView.Columns"));
			if (dataGridColumns)
			{
				if (!beginCollection("columns") || !ValidateAttributes(property, {}, error))
					return false;
				DesignValue values = DesignValue::array();
				for (const auto& item : ChildElements(property))
				{
					DiagnosticContext itemContext(*this, item);
					if (!Equals(FromUtf8(item->LocalName()), L"GridViewColumn"))
						return Fail(L"GridView.Columns 仅允许 GridViewColumn。", error);
					if (!ValidateAttributes(item,
						{ L"Header", L"Width", L"Type", L"CanEdit", L"ButtonText" }, error))
						return false;
					DesignValue value = DesignValue::object();
					value["name"] = ToUtf8(Attribute(item, L"Header").value_or(L""));
					double width = 120.0;
					if (!ReadDoubleAttribute(item, L"Width", 120.0, width, error)
						|| width < 0.0) return Fail(L"Width 必须是非负数。", error);
					value["width"] = width;
					int columnType = 0;
					if (const auto text = Attribute(item, L"Type"); text && !TryParseEnum(*text,
						{ L"Text", L"Image", L"Check", L"Button", L"ComboBox", L"LinkedText" },
						columnType)) return Fail(L"GridViewColumn Type 无效。", error);
					value["type"] = columnType;
					bool canEdit = true;
					if (!ReadBoolAttribute(item, L"CanEdit", true, canEdit, error)) return false;
					value["canEdit"] = canEdit;
					value["buttonText"] = ToUtf8(Attribute(item, L"ButtonText").value_or(L""));
					for (const auto& child : ChildElements(item))
					{
						DiagnosticContext childContext(*this, child);
						if (!Equals(FromUtf8(child->LocalName()), L"GridViewColumn.Items")
							|| value.contains("comboBoxItems"))
							return Fail(L"GridViewColumn 仅允许一个 GridViewColumn.Items。", error);
						DesignValue items;
						if (!ParseStringItems(child, items, error)) return false;
						if (!items.empty()) value["comboBoxItems"] = std::move(items);
					}
					values.push_back(std::move(value));
				}
				extra["columns"] = std::move(values);
				return true;
			}
			const bool dataGridRows = dataGrid
				&& (Equals(name, L"GridView.Rows")
					|| Equals(name, L"PagedGridView.Rows"));
			if (dataGridRows)
			{
				if (!beginCollection("rows") || !ValidateAttributes(property, {}, error))
					return false;
				DesignValue rows = DesignValue::array();
				for (const auto& rowElement : ChildElements(property))
				{
					DiagnosticContext rowContext(*this, rowElement);
					if (!Equals(FromUtf8(rowElement->LocalName()), L"GridViewRow"))
						return Fail(L"表格行集合仅允许 GridViewRow。", error);
					if (!ValidateAttributes(rowElement, {}, error)) return false;
					DesignValue cells = DesignValue::array();
					for (const auto& cellElement : ChildElements(rowElement))
					{
						DiagnosticContext cellContext(*this, cellElement);
						const auto cellName = FromUtf8(cellElement->LocalName());
						if (Equals(cellName, L"String")
							&& (Equals(FromUtf8(cellElement->Prefix()), L"x")
								|| Equals(FromUtf8(cellElement->NamespaceURI()),
									L"http://schemas.microsoft.com/winfx/2006/xaml")))
						{
							if (!ValidateAttributes(cellElement, {}, error)
								|| !ChildElements(cellElement).empty())
								return Fail(L"x:String 表格单元格不允许属性或子元素。", error);
							cells.push_back(DesignValue{
								{ "value", cellElement->InnerText() } });
							continue;
						}
						if (!Equals(cellName, L"GridViewCell"))
							return Fail(L"GridViewRow 仅允许 GridViewCell 或 x:String。", error);
						if (!ValidateAttributes(cellElement,
							{ L"Value", L"IsChecked", L"Tag", L"SelectedIndex" }, error)
							|| !ChildElements(cellElement).empty())
							return Fail(L"GridViewCell 不允许包含子元素。", error);
						const bool hasChecked = Attribute(cellElement, L"IsChecked").has_value();
						const bool hasTag = Attribute(cellElement, L"Tag").has_value();
						const bool hasSelected = Attribute(cellElement, L"SelectedIndex").has_value();
						if (static_cast<int>(hasChecked) + static_cast<int>(hasTag)
							+ static_cast<int>(hasSelected) > 1)
							return Fail(L"GridViewCell 的 IsChecked、Tag、SelectedIndex 互斥。", error);
						DesignValue cell = DesignValue::object();
						if (const auto value = Attribute(cellElement, L"Value"))
							cell["value"] = ToUtf8(*value);
						if (hasChecked)
						{
							bool checked = false;
							if (!ReadBoolAttribute(
								cellElement, L"IsChecked", false, checked, error)) return false;
							cell["checked"] = checked;
						}
						if (const auto text = Attribute(cellElement, L"Tag"))
						{
							long long tag = 0;
							if (!TryParseInteger(*text, tag))
								return Fail(L"GridViewCell Tag 必须是整数。", error);
							cell["tag"] = tag;
						}
						if (const auto text = Attribute(cellElement, L"SelectedIndex"))
						{
							int selectedIndex = -1;
							if (!TryParseInteger(*text, selectedIndex) || selectedIndex < -1)
								return Fail(L"SelectedIndex 必须是 -1 或非负整数。", error);
							cell["selectedIndex"] = selectedIndex;
						}
						cells.push_back(std::move(cell));
					}
					rows.push_back(DesignValue{ { "cells", std::move(cells) } });
				}
				extra["rows"] = std::move(rows);
				return true;
			}
			if (type == UIClass::UI_PropertyGrid && Equals(name, L"PropertyGrid.Items"))
			{
				if (!beginCollection("items") || !ValidateAttributes(property, {}, error))
					return false;
				DesignValue values = DesignValue::array();
				for (const auto& item : ChildElements(property))
				{
					DiagnosticContext itemContext(*this, item);
					if (!Equals(FromUtf8(item->LocalName()), L"PropertyGridItem"))
						return Fail(L"PropertyGrid.Items 仅允许 PropertyGridItem。", error);
					if (!ValidateAttributes(item, { L"Category", L"Name", L"Value", L"Description",
						L"Type", L"IsReadOnly", L"IsMixed", L"CanReset", L"Minimum",
						L"Maximum", L"Step", L"Tag" }, error)) return false;
					DesignValue value = DesignValue::object();
					for (const auto& [attributeName, key] : {
						std::pair{ L"Category", "category" }, std::pair{ L"Name", "name" },
						std::pair{ L"Value", "value" }, std::pair{ L"Description", "description" } })
						value[key] = ToUtf8(Attribute(item, attributeName).value_or(L""));
					int valueType = 0;
					if (const auto text = Attribute(item, L"Type"); text && !TryParseEnum(*text,
						{ L"Text", L"Number", L"Bool", L"Enum", L"Color", L"ReadOnly",
						  L"Action", L"Slider", L"Anchor", L"EditableEnum" }, valueType))
						return Fail(L"PropertyGridItem Type 无效。", error);
					value["type"] = valueType;
					for (const auto& [attributeName, key] : {
						std::pair{ L"IsReadOnly", "readOnly" },
						std::pair{ L"IsMixed", "isMixed" },
						std::pair{ L"CanReset", "canReset" } })
					{
						bool parsed = false;
						if (!ReadBoolAttribute(item, attributeName, false, parsed, error)) return false;
						value[key] = parsed;
					}
					for (const auto& [attributeName, key, defaultValue] : {
						std::tuple{ L"Minimum", "minimum", 0.0 },
						std::tuple{ L"Maximum", "maximum", 1.0 },
						std::tuple{ L"Step", "step", 0.01 } })
					{
						double parsed = defaultValue;
						if (!ReadDoubleAttribute(item, attributeName, defaultValue, parsed, error)) return false;
						value[key] = parsed;
					}
					unsigned long long tag = 0;
					if (const auto text = Attribute(item, L"Tag");
						text && !TryParseUnsignedInteger(*text, tag))
						return Fail(L"Tag 必须是非负整数。", error);
					value["tag"] = tag;
					for (const auto& child : ChildElements(item))
					{
						DiagnosticContext childContext(*this, child);
						if (!Equals(FromUtf8(child->LocalName()), L"PropertyGridItem.Options")
							|| value.contains("options"))
							return Fail(L"PropertyGridItem 仅允许一个 PropertyGridItem.Options。", error);
						DesignValue options;
						if (!ParseStringItems(child, options, error)) return false;
						if (!options.empty()) value["options"] = std::move(options);
					}
					values.push_back(std::move(value));
				}
				extra["items"] = std::move(values);
				return true;
			}
			if (type == UIClass::UI_TreeView && Equals(name, L"TreeView.Items"))
			{
				if (!beginCollection("nodes")) return false;
				DesignValue values;
				if (!ParseTreeItems(property, values, error)) return false;
				extra["nodes"] = std::move(values);
				return true;
			}
			if (type == UIClass::UI_StatusBar && Equals(name, L"StatusBar.Items"))
			{
				if (!beginCollection("parts") || !ValidateAttributes(property, {}, error))
					return false;
				DesignValue values = DesignValue::array();
				for (const auto& item : ChildElements(property))
				{
					DiagnosticContext itemContext(*this, item);
					if (!Equals(FromUtf8(item->LocalName()), L"StatusBarItem"))
						return Fail(L"StatusBar.Items 仅允许 StatusBarItem。", error);
					if (!ValidateAttributes(item, { L"Text", L"Width" }, error))
						return false;
					if (!ChildElements(item).empty())
						return Fail(L"StatusBarItem 不允许子元素。", error);
					int width = 0;
					if (const auto text = Attribute(item, L"Width");
						text && !TryParseInteger(*text, width))
						return Fail(L"StatusBarItem Width 必须是整数。", error);
					values.push_back(DesignValue{
						{ "text", ToUtf8(Attribute(item, L"Text").value_or(L"")) },
						{ "width", width } });
				}
				extra["parts"] = std::move(values);
				return true;
			}
			if (type == UIClass::UI_Menu && Equals(name, L"Menu.Items"))
			{
				if (!beginCollection("items")) return false;
				DesignValue values;
				if (!ParseMenuItems(property, false, values, error)) return false;
				extra["items"] = std::move(values);
				return true;
			}
			return true;
		}

		bool ParseGridDefinitions(
			const Element& container,
			size_t gridIndex,
			bool rows,
			std::wstring& error)
		{
			DiagnosticContext context(*this, container);
			DesignValue definitions = DesignValue::array();
			for (const auto& item : ChildElements(container))
			{
				DiagnosticContext itemContext(*this, item);
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
			DiagnosticContext context(*this, page);
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
				DiagnosticContext attributeContext(*this, page, attribute.get());
				const auto name = FromUtf8(attribute->LocalName());
				const auto prefix = FromUtf8(attribute->Prefix());
				if (!Equals(name, L"Name") && !Equals(name, L"Header")
					&& !Equals(name, L"Text")
					&& !(Equals(name, L"DesignKey") && Equals(prefix, L"d")))
					return Fail(L"TabPage 尚不支持属性：" + name, error);
			}
			for (const auto& child : ChildElements(page))
			{
				DiagnosticContext childContext(*this, child);
				if (!ParseControl(child, Parent{ 0, pageId }, error)) return false;
			}
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
	std::wstring* outError,
	XamlDocumentDiagnostic* outDiagnostic)

{
	return FromXaml(
		xaml, output, XamlDocumentParseOptions{}, outError, outDiagnostic);
}

bool XamlDocumentParser::FromXaml(
	const std::string& xaml,
	DesignDocument& output,
	const XamlDocumentParseOptions& options,
	std::wstring* outError,
	XamlDocumentDiagnostic* outDiagnostic)
{
	ResetDiagnostic(outDiagnostic);
	try
	{
		XmlDocument xml;
		xml.LoadXml(xaml);
		const auto root = xml.DocumentElement();
		XamlSourceLocationIndex sourceLocations(FromUtf8(xaml), root);
		DesignDocument candidate;
		auto effectiveOptions = options;
		if (!effectiveOptions.Resources)
			effectiveOptions.Resources = std::make_shared<ResourceLoadContext>(
				Application::GetResourceResolver());
		candidate.ResourceBasePath = effectiveOptions.ResourceBasePath;
		candidate.Resources = effectiveOptions.Resources;
		Parser parser(candidate, effectiveOptions, sourceLocations, outDiagnostic);
		std::wstring error;
		if (!parser.Parse(root, error))
		{
			parser.FinalizeFailure(root, error);
			ReportFailure(error, outError, outDiagnostic);
			return false;
		}
		output = std::move(candidate);
		if (outError) outError->clear();
		return true;
	}
	catch (const System::Xml::XmlException& exception)
	{
		const auto message = L"XAML 解析失败："
			+ FromUtf8(XmlExceptionMessageWithoutLocation(exception));
		ReportFailure(message, outError, outDiagnostic);
		PopulateXmlLocation(xaml, exception, outDiagnostic);
		return false;
	}
	catch (const std::exception& exception)
	{
		ReportFailure(
			L"XAML 解析失败：" + FromUtf8(exception.what()),
			outError, outDiagnostic);
		return false;
	}
	catch (...)
	{
		ReportFailure(
			L"XAML 解析失败：发生未知异常。",
			outError, outDiagnostic);
		return false;
	}
}

bool XamlDocumentParser::LoadFromFile(
	const std::wstring& filePath,
	DesignDocument& output,
	std::wstring* outError,
	XamlDocumentDiagnostic* outDiagnostic)

{
	return LoadFromFile(
		filePath, output, XamlDocumentParseOptions{}, outError, outDiagnostic);
}

bool XamlDocumentParser::LoadFromFile(
	const std::wstring& filePath,
	DesignDocument& output,
	const XamlDocumentParseOptions& options,
	std::wstring* outError,
	XamlDocumentDiagnostic* outDiagnostic)
{
	ResetDiagnostic(outDiagnostic);
	try
	{
		auto effectiveOptions = options;
		if (!effectiveOptions.Resources)
			effectiveOptions.Resources = std::make_shared<ResourceLoadContext>(
				Application::GetResourceResolver());
		ResolvedResource resource;
		std::wstring resourceError;
		if (!effectiveOptions.Resources->Resolve(
			filePath, effectiveOptions.ResourceBasePath,
			resource, &resourceError))
		{
			ReportFailure(resourceError.empty()
				? L"无法打开 XAML 文件：" + filePath : resourceError,
				outError, outDiagnostic);
			return false;
		}
		if (effectiveOptions.ResourceBasePath.empty())
			effectiveOptions.ResourceBasePath = resource.BaseUri;
		const std::string content(
			reinterpret_cast<const char*>(resource.Bytes.data()),
			resource.Bytes.size());
		return FromXaml(
			content, output, effectiveOptions, outError, outDiagnostic);
	}
	catch (const std::exception&)
	{
		ReportFailure(
			L"读取 XAML 文件时发生异常：" + filePath,
			outError, outDiagnostic);
		return false;
	}
	catch (...)
	{
		ReportFailure(
			L"读取 XAML 文件时发生未知异常。",
			outError, outDiagnostic);
		return false;
	}
}
}
