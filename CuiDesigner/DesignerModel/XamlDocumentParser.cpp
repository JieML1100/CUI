#include "XamlDocumentParser.h"

#include "DesignDocumentGraph.h"
#include "DesignDocumentEventIndex.h"
#include "DesignDocumentMaterializer.h"
#include "DesignDataResourceUtils.h"
#include "StoryboardPropertyPath.h"
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
#include <array>
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

	std::wstring DirectText(const Element& element)
	{
		std::wstring result;
		if (!element) return result;
		for (const auto& child : element->ChildNodes())
			if (child && (child->NodeType() == XmlNodeType::Text
				|| child->NodeType() == XmlNodeType::CDATA))
				result += FromUtf8(child->Value());
		return Trim(result);
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

	bool IsContentHostType(UIClass type) noexcept
	{
		return type == UIClass::UI_ContentPresenter
			|| type == UIClass::UI_ContentControl
			|| type == UIClass::UI_SelectorItem
			|| type == UIClass::UI_ComboBoxItem
			|| type == UIClass::UI_TreeViewItem
			|| type == UIClass::UI_Button
			|| type == UIClass::UI_GroupBox
			|| type == UIClass::UI_Expander;
	}

	bool IsVisualContentControlType(UIClass type) noexcept
	{
		return type == UIClass::UI_ContentControl
			|| type == UIClass::UI_SelectorItem
			|| type == UIClass::UI_ComboBoxItem
			|| type == UIClass::UI_TreeViewItem
			|| type == UIClass::UI_Button
			|| type == UIClass::UI_GroupBox
			|| type == UIClass::UI_Expander;
	}

	bool IsControlTemplateHostType(UIClass type) noexcept
	{
		return IsVisualContentControlType(type)
			|| type == UIClass::UI_ItemsControl
			|| type == UIClass::UI_ListBox;
	}

	bool IsHeaderedContentControlType(UIClass type) noexcept
	{
		return type == UIClass::UI_GroupBox
			|| type == UIClass::UI_Expander
			|| type == UIClass::UI_TreeViewItem;
	}

	bool IsControlTemplateTargetCompatible(
		UIClass actual, UIClass target) noexcept
	{
		return actual == target
			|| (target == UIClass::UI_ContentControl
				&& (actual == UIClass::UI_SelectorItem
					|| actual == UIClass::UI_ComboBoxItem
					|| actual == UIClass::UI_TreeViewItem
					|| actual == UIClass::UI_Button
					|| actual == UIClass::UI_GroupBox
					|| actual == UIClass::UI_Expander))
			|| (target == UIClass::UI_ItemsControl
				&& actual == UIClass::UI_ListBox);
	}

	bool IsControlTemplateTargetCompatible(
		const DesignNode& actual,
		const DesignControlTemplate& target) noexcept
	{
		if (!target.TargetComponentType.Empty())
			return !actual.ComponentType.Empty()
				&& actual.ComponentType.RegistryKey()
					== target.TargetComponentType.RegistryKey();
		return IsControlTemplateTargetCompatible(actual.Type, target.TargetType);
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

	std::wstring LookupNamespaceUri(
		const Element& element,
		const std::wstring& prefix)
	{
		for (const XmlNode* node = element.get(); node; node = node->ParentNode())
		{
			if (node->NodeType() != XmlNodeType::Element) continue;
			const auto* current = static_cast<const XmlElement*>(node);
			const auto uri = current->FindNamespaceDeclarationValue(ToUtf8(prefix));
			if (!uri.empty()) return FromUtf8(uri);
		}
		return {};
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

	bool TryParseTimeSpanMilliseconds(
		const std::wstring& value,
		unsigned long long& output)
	{
		const auto text = Trim(value);
		const auto parts = Split(text, L':');
		if (parts.size() != 3) return false;
		unsigned long long days = 0;
		unsigned long long hours = 0;
		auto hourText = parts[0];
		if (const auto dot = hourText.find(L'.'); dot != std::wstring::npos)
		{
			if (!TryParseUnsignedInteger(hourText.substr(0, dot), days)
				|| !TryParseUnsignedInteger(hourText.substr(dot + 1), hours)
				|| hours >= 24) return false;
		}
		else if (!TryParseUnsignedInteger(hourText, hours)) return false;
		unsigned long long minutes = 0;
		if (!TryParseUnsignedInteger(parts[1], minutes) || minutes >= 60)
			return false;
		double seconds = 0.0;
		if (!TryParseDouble(parts[2], seconds)
			|| seconds < 0.0 || seconds >= 60.0) return false;
		const long double totalMilliseconds =
			(static_cast<long double>(days) * 24.0L * 60.0L * 60.0L
				+ static_cast<long double>(hours) * 60.0L * 60.0L
				+ static_cast<long double>(minutes) * 60.0L
				+ static_cast<long double>(seconds)) * 1000.0L;
		// llround returns a signed 64-bit value. Keep the accepted range inside
		// that domain instead of allowing an otherwise finite but unrepresentable
		// duration to overflow during conversion.
		if (totalMilliseconds < 0.0L
			|| totalMilliseconds > static_cast<long double>(
				(std::numeric_limits<long long>::max)())) return false;
		output = static_cast<unsigned long long>(std::llround(totalMilliseconds));
		return true;
	}

	bool TryParseKeySpline(
		const std::wstring& value,
		float& x1,
		float& y1,
		float& x2,
		float& y2)
	{
		auto normalized = Trim(value);
		std::replace(normalized.begin(), normalized.end(), L',', L' ');
		std::wistringstream stream(normalized);
		std::vector<std::wstring> tokens;
		for (std::wstring token; stream >> token;)
			tokens.push_back(std::move(token));
		if (tokens.size() != 4) return false;
		double values[4]{};
		for (size_t index = 0; index < 4; ++index)
			if (!TryParseDouble(tokens[index], values[index])
				|| values[index] < 0.0 || values[index] > 1.0)
				return false;
		x1 = static_cast<float>(values[0]);
		y1 = static_cast<float>(values[1]);
		x2 = static_cast<float>(values[2]);
		y2 = static_cast<float>(values[3]);
		return true;
	}

	std::vector<std::wstring> SplitMarkupArguments(const std::wstring& value)
	{
		std::vector<std::wstring> result;
		size_t start = 0;
		int depth = 0;
		wchar_t quote = 0;
		for (size_t index = 0; index < value.size(); ++index)
		{
			if (quote != 0)
			{
				if (value[index] == quote)
				{
					if (index + 1 < value.size() && value[index + 1] == quote)
						++index;
					else quote = 0;
				}
				continue;
			}
			if (value[index] == L'\'' || value[index] == L'"')
			{
				quote = value[index];
				continue;
			}
			if (value[index] == L'{') ++depth;
			else if (value[index] == L'}') --depth;
			else if (value[index] == L',' && depth == 0)
			{
				result.push_back(Trim(value.substr(start, index - start)));
				start = index + 1;
			}
		}
		result.push_back(Trim(value.substr(start)));
		return result;
	}

	bool TryUnquoteMarkupArgument(std::wstring value, std::wstring& result)
	{
		value = Trim(std::move(value));
		result.clear();
		const bool startsQuoted = !value.empty()
			&& (value.front() == L'\'' || value.front() == L'"');
		const bool endsQuoted = !value.empty()
			&& (value.back() == L'\'' || value.back() == L'"');
		if (!startsQuoted && !endsQuoted)
		{
			result = std::move(value);
			return true;
		}
		if (value.size() < 2 || !startsQuoted || value.back() != value.front())
			return false;
		const auto quote = value.front();
		value = value.substr(1, value.size() - 2);
		result.reserve(value.size());
		for (size_t index = 0; index < value.size(); ++index)
		{
			result.push_back(value[index]);
			if (value[index] == quote && index + 1 < value.size()
				&& value[index + 1] == quote) ++index;
		}
		return true;
	}

	std::wstring MarkupTypeToken(std::wstring value)
	{
		value = Trim(value);
		if (value.size() >= 2 && value.front() == L'{' && value.back() == L'}')
		{
			value = Trim(value.substr(1, value.size() - 2));
			if (Lower(value).starts_with(L"x:type"))
				value = Trim(value.substr(6));
		}
		return Trim(value);
	}

	std::wstring StripMarkupType(std::wstring value)
	{
		value = MarkupTypeToken(std::move(value));
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

	bool TryParseDynamicResource(
		const std::wstring& value,
		std::wstring& resourceKey)
	{
		auto text = Trim(value);
		if (text.size() < 3 || text.front() != L'{' || text.back() != L'}')
			return false;
		text = Trim(text.substr(1, text.size() - 2));
		const auto lower = Lower(text);
		if (!lower.starts_with(L"dynamicresource")) return false;
		resourceKey = Trim(text.substr(15));
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
		bool updateSourceTriggerSeen = false;
		for (const auto& part : SplitMarkupArguments(text))
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
				if (updateSourceTriggerSeen)
				{
					error = L"Binding 不能重复声明 UpdateSourceTrigger/UpdateMode。";
					return false;
				}
				updateSourceTriggerSeen = true;
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
			else if (key == L"converterparameter")
			{
				std::wstring literal;
				if (!TryUnquoteMarkupArgument(itemValue, literal))
				{
					error = L"Binding ConverterParameter 引号未闭合。";
					return false;
				}
				binding.ConverterParameter = DesignerStyleValue{
					DesignerStyleValueKind::String, std::move(literal) };
			}
			else if (key == L"stringformat")
			{
				std::wstring format;
				if (!TryUnquoteMarkupArgument(itemValue, format))
				{
					error = L"Binding StringFormat 引号未闭合。";
					return false;
				}
				if (!IsValidBindingStringFormat(format))
				{
					error = L"Binding StringFormat 复合格式语法无效。";
					return false;
				}
				binding.StringFormat = std::move(format);
			}
			else if (key == L"elementname") binding.ElementName = itemValue;
			else if (key == L"fallbackvalue")
			{
				std::wstring literal;
				if (!TryUnquoteMarkupArgument(itemValue, literal))
				{
					error = L"Binding FallbackValue 引号未闭合。";
					return false;
				}
				binding.FallbackValue = DesignerStyleValue{
					DesignerStyleValueKind::String, std::move(literal) };
			}
			else if (key == L"targetnullvalue")
			{
				std::wstring literal;
				if (!TryUnquoteMarkupArgument(itemValue, literal))
				{
					error = L"Binding TargetNullValue 引号未闭合。";
					return false;
				}
				binding.TargetNullValue = DesignerStyleValue{
					DesignerStyleValueKind::String, std::move(literal) };
			}
			else if (key == L"relativesource")
			{
				auto source = Trim(itemValue);
				if (source.size() >= 2 && source.front() == L'{'
					&& source.back() == L'}')
				{
					source = Trim(source.substr(1, source.size() - 2));
					if (!Lower(source).starts_with(L"relativesource"))
					{
						error = L"Binding RelativeSource 标记扩展无效。";
						return false;
					}
					source = Trim(source.substr(14));
				}

				std::wstring mode;
				for (const auto& sourcePart : SplitMarkupArguments(source))
				{
					if (sourcePart.empty()) continue;
					const auto sourceEquals = sourcePart.find(L'=');
					if (sourceEquals == std::wstring::npos)
					{
						if (!mode.empty())
						{
							error = L"RelativeSource 包含多个模式。";
							return false;
						}
						mode = Trim(sourcePart);
						continue;
					}
					const auto sourceKey = Lower(Trim(
						sourcePart.substr(0, sourceEquals)));
					auto sourceValue = Trim(sourcePart.substr(sourceEquals + 1));
					if (sourceKey == L"mode") mode = sourceValue;
					else if (sourceKey == L"ancestortype")
					{
						if (sourceValue.size() >= 2 && sourceValue.front() == L'{'
							&& sourceValue.back() == L'}')
						{
							sourceValue = Trim(sourceValue.substr(
								1, sourceValue.size() - 2));
							const auto lowerType = Lower(sourceValue);
							if (!lowerType.starts_with(L"x:type")
								|| (sourceValue.size() > 6
									&& std::iswspace(sourceValue[6]) == 0))
							{
								error = L"RelativeSource AncestorType 标记扩展无效。";
								return false;
							}
							sourceValue = Trim(sourceValue.substr(6));
						}
						binding.AncestorType = sourceValue;
					}
					else if (sourceKey == L"ancestorlevel")
					{
						try
						{
							size_t consumed = 0;
							const auto level = std::stoi(sourceValue, &consumed);
							if (consumed != sourceValue.size() || level < 1)
								throw std::invalid_argument("level");
							binding.AncestorLevel = level;
						}
						catch (...)
						{
							error = L"RelativeSource AncestorLevel 必须是大于等于 1 的整数。";
							return false;
						}
					}
					else
					{
						error = L"RelativeSource 包含不支持的参数："
							+ Trim(sourcePart.substr(0, sourceEquals));
						return false;
					}
				}

				if (Equals(mode, L"Self"))
					binding.RelativeSource = DesignerBindingRelativeSource::Self;
				else if (Equals(mode, L"TemplatedParent"))
					binding.RelativeSource = DesignerBindingRelativeSource::TemplatedParent;
				else if (Equals(mode, L"FindAncestor"))
				{
					binding.RelativeSource = DesignerBindingRelativeSource::FindAncestor;
					binding.AncestorType = Trim(binding.AncestorType);
					if (binding.AncestorType.empty())
					{
						error = L"RelativeSource FindAncestor 必须声明 AncestorType。";
						return false;
					}
				}
				else
				{
					error = L"Binding RelativeSource 仅支持 Self、TemplatedParent 或 FindAncestor。";
					return false;
				}
			}
			else
			{
				error = L"Binding 包含不支持的参数：" + Trim(part.substr(0, equals));
				return false;
			}
		}

		binding.SourceProperty = DesignerBindingUtils::Trim(binding.SourceProperty);
		binding.ElementName = Trim(binding.ElementName);
		if (binding.RelativeSource != DesignerBindingRelativeSource::FindAncestor
			&& (!binding.AncestorType.empty() || binding.AncestorLevel != 1))
		{
			error = L"AncestorType/AncestorLevel 只能用于 RelativeSource FindAncestor。";
			return false;
		}
		if (!binding.ElementName.empty()
			&& binding.RelativeSource != DesignerBindingRelativeSource::None)
		{
			error = L"Binding 不能同时声明 ElementName 与 RelativeSource。";
			return false;
		}
		if (!DesignerBindingUtils::IsValidSourcePath(binding.SourceProperty))
		{
			error = L"Binding 源路径无效。";
			return false;
		}
		if (!binding.ElementName.empty())
		{
			if (binding.ElementName.find_first_of(L".,={} \t\r\n")
				!= std::wstring::npos)
			{
				error = L"Binding ElementName 必须是直接 x:Name。";
				return false;
			}
		}
		return true;
	}

	bool TryParseTemplateBinding(
		const std::wstring& value,
		std::wstring& sourceProperty,
		std::wstring& error)
	{
		auto text = Trim(value);
		if (text.size() < 3 || text.front() != L'{' || text.back() != L'}')
			return false;
		text = Trim(text.substr(1, text.size() - 2));
		if (!Lower(text).starts_with(L"templatebinding")) return false;
		if (text.size() > 15 && std::iswspace(text[15]) == 0)
		{
			error = L"TemplateBinding 后必须是组件属性名。";
			return false;
		}
		sourceProperty = Trim(text.substr(15));
		if (sourceProperty.empty()
			|| sourceProperty.find_first_of(L".,={}") != std::wstring::npos)
		{
			error = L"TemplateBinding 只接受一个直接组件属性名。";
			return false;
		}
		return true;
	}

	bool TryParseRaiseEvent(
		const std::wstring& value,
		std::wstring& eventName,
		std::wstring& error)
	{
		eventName.clear();
		error.clear();
		auto text = Trim(value);
		if (text.size() < 3 || text.front() != L'{' || text.back() != L'}')
			return false;
		text = Trim(text.substr(1, text.size() - 2));
		if (!Lower(text).starts_with(L"raiseevent")) return false;
		if (text.size() > 10 && std::iswspace(text[10]) == 0)
		{
			error = L"RaiseEvent 标记扩展格式无效。";
			return false;
		}
		eventName = Trim(text.substr(10));
		if (eventName.empty()
			|| eventName.find_first_of(L" ,=") != std::wstring::npos)
		{
			error = L"RaiseEvent 只接受一个直接组件事件名。";
			return false;
		}
		return true;
	}

	bool CanForwardTemplateEvent(
		const std::wstring& sourceEvent,
		DesignerComponentEventPayload payload)
	{
		if (payload == DesignerComponentEventPayload::String)
			return Equals(sourceEvent, L"OnTextChanged")
				|| Equals(sourceEvent, L"OnDropText");
		if (payload == DesignerComponentEventPayload::Bool)
			return Equals(sourceEvent, L"OnChecked");
		if (payload != DesignerComponentEventPayload::None) return false;
		for (const auto* supported : {
			L"OnMouseClick", L"OnMouseDoubleClick", L"OnMouseEnter",
			L"OnMouseLeave", L"OnGotFocus", L"OnLostFocus", L"OnPaint",
			L"OnClose", L"OnMoved", L"OnSizeChanged", L"OnSelectedChanged",
			L"OnScrollChanged" })
			if (Equals(sourceEvent, supported)) return true;
		return false;
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
		const std::vector<DesignerComponentEventDescriptor>& componentEvents,
		const std::wstring& rawName,
		const std::wstring& rawValue)
	{
		const auto trimmedValue = Lower(Trim(rawValue));
		if (trimmedValue.starts_with(L"{binding")
			|| trimmedValue.starts_with(L"{templatebinding"))
			return std::nullopt;
		const auto events = DesignerEventCatalog::GetControlEvents(
			type, componentEvents);
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
			_resourceTarget = &_document.StyleSheet;
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
			if (!ValidateBindingSources(
				_document.Nodes, L"文档", false, error)) return false;
			for (const auto& component : _document.Components)
				if (!ValidateBindingSources(
					component.Template,
					L"组件 " + component.Type.XamlName, true, error)) return false;
			for (const auto& dataTemplate : _document.DataTemplates)
				if (!ValidateBindingSources(
					dataTemplate.Template,
					L"DataTemplate " + dataTemplate.DisplayName(), false, error))
					return false;
			for (const auto& controlTemplate : _document.ControlTemplates)
				if (!ValidateBindingSources(
					controlTemplate.Template,
					L"ControlTemplate " + controlTemplate.DisplayName(), true, error))
					return false;
			DesignerDataContextSchemaUtils::Canonicalize(_document.DataContextSchema);
			DesignerStyleSheetUtils::Canonicalize(_document.StyleSheet);
			if (!DesignDataResourceUtils::ValidateAndCanonicalize(
				_document, &error)) return Fail(error, error);
			if (!DesignerStyleSheetUtils::ValidateAgainstRulePropertyMetadata(
				_document.StyleSheet,
				[&](const DesignerStyleRule& rule) -> std::unique_ptr<Control>
				{
					auto probe = DesignDocumentMaterializer::CreateRuntimeControl(
						rule.HasType ? rule.Type : UIClass::UI_Base);
					if (!probe || rule.ComponentType.Empty()) return probe;
					const auto* component = FindVisibleComponent(rule.ComponentType);
					std::wstring ignored;
					if (!component || !DesignDocumentMaterializer::InstallComponentContract(
						*probe, *component, _document, &ignored)) return nullptr;
					return probe;
				},
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
		DesignerStyleSheet* _resourceTarget = nullptr;
		DesignObjectResourceDictionary* _objectResourceTarget = nullptr;
		// Copies are deliberate: _document.Nodes may reallocate while descendants
		// are parsed, whereas lexical resource scopes must remain stable.
		std::vector<DesignerStyleSheet> _resourceScopes;
		std::vector<DesignObjectResourceDictionary> _objectResourceScopes;
		bool _parsingLocalResources = false;
		std::unordered_set<int> _usedIds;
		std::unordered_set<std::wstring> _usedNames;
		std::unordered_map<std::wstring, int> _nameCounters;
		std::vector<std::wstring> _bindingPaths;
		DesignComponentDefinition* _activeTemplateComponent = nullptr;
		Control* _activeControlTemplateProbe = nullptr;
		// True only while parsing the visual tree of a ControlTemplate itself.
		// Nested DataTemplate/ComponentDefinition resources must not inherit the
		// outer template's TemplateBinding/ItemsPresenter privileges.
		bool _parsingControlTemplateVisual = false;
		bool _parsingComponentTemplateVisual = false;
		Element _pendingVisualStateGroups;
		Element _pendingEventTriggers;

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

		class LexicalResourceScope final
		{
		public:
			LexicalResourceScope(Parser& parser, const DesignerStyleSheet& resources)
				: _parser(parser), _active(!resources.Empty())
			{
				if (_active) _parser._resourceScopes.push_back(resources);
			}
			~LexicalResourceScope()
			{
				if (_active) _parser._resourceScopes.pop_back();
			}
		private:
			Parser& _parser;
			bool _active = false;
		};

		class LexicalObjectResourceScope final
		{
		public:
			LexicalObjectResourceScope(
				Parser& parser,
				const DesignObjectResourceDictionary& resources)
				: _parser(parser), _active(!resources.Empty())
			{
				if (_active) _parser._objectResourceScopes.push_back(resources);
			}
			~LexicalObjectResourceScope()
			{
				if (_active) _parser._objectResourceScopes.pop_back();
			}
		private:
			Parser& _parser;
			bool _active = false;
		};

		const DesignComponentDefinition* FindVisibleComponent(
			const std::wstring& xamlNamespace,
			const std::wstring& xamlName) const
		{
			auto find = [&](const DesignObjectResourceDictionary& resources)
				-> const DesignComponentDefinition*
			{
				const auto item = std::find_if(resources.Components.rbegin(),
					resources.Components.rend(), [&](const auto& component)
					{
						return Equals(component.Type.XamlNamespace, xamlNamespace)
							&& Equals(component.Type.XamlName, xamlName);
					});
				return item == resources.Components.rend() ? nullptr : &*item;
			};
			if (_objectResourceTarget)
				if (const auto* value = find(*_objectResourceTarget)) return value;
			for (auto scope = _objectResourceScopes.rbegin();
				scope != _objectResourceScopes.rend(); ++scope)
				if (const auto* value = find(*scope)) return value;
			return _document.FindComponent(xamlNamespace, xamlName);
		}

		const DesignComponentDefinition* FindVisibleComponent(
			const DesignerComponentType& type) const
		{
			return FindVisibleComponent(type.XamlNamespace, type.XamlName);
		}

		const DesignDataTemplate* FindVisibleDataTemplate(
			const std::wstring& key) const
		{
			auto find = [&](const DesignObjectResourceDictionary& resources)
				-> const DesignDataTemplate*
			{
				const auto item = std::find_if(resources.DataTemplates.rbegin(),
					resources.DataTemplates.rend(), [&](const auto& dataTemplate)
					{ return Equals(dataTemplate.Key, key); });
				return item == resources.DataTemplates.rend() ? nullptr : &*item;
			};
			if (_objectResourceTarget)
				if (const auto* value = find(*_objectResourceTarget)) return value;
			for (auto scope = _objectResourceScopes.rbegin();
				scope != _objectResourceScopes.rend(); ++scope)
				if (const auto* value = find(*scope)) return value;
			return _document.FindDataTemplate(key);
		}

		const DesignControlTemplate* FindVisibleControlTemplate(
			const std::wstring& key) const
		{
			auto find = [&](const DesignObjectResourceDictionary& resources)
				-> const DesignControlTemplate*
			{
				const auto item = std::find_if(
					resources.ControlTemplates.rbegin(),
					resources.ControlTemplates.rend(), [&](const auto& value)
					{ return !value.IsImplicit() && Equals(value.Key, key); });
				return item == resources.ControlTemplates.rend()
					? nullptr : &*item;
			};
			if (_objectResourceTarget)
				if (const auto* value = find(*_objectResourceTarget)) return value;
			for (auto scope = _objectResourceScopes.rbegin();
				scope != _objectResourceScopes.rend(); ++scope)
				if (const auto* value = find(*scope)) return value;
			return _document.FindControlTemplate(key);
		}

		const DesignItemsPanelTemplate* FindVisibleItemsPanelTemplate(
			const std::wstring& key) const
		{
			auto find = [&](const DesignObjectResourceDictionary& resources)
				-> const DesignItemsPanelTemplate*
			{
				const auto item = std::find_if(
					resources.ItemsPanelTemplates.rbegin(),
					resources.ItemsPanelTemplates.rend(), [&](const auto& value)
					{ return Equals(value.Key, key); });
				return item == resources.ItemsPanelTemplates.rend()
					? nullptr : &*item;
			};
			if (_objectResourceTarget)
				if (const auto* value = find(*_objectResourceTarget)) return value;
			for (auto scope = _objectResourceScopes.rbegin();
				scope != _objectResourceScopes.rend(); ++scope)
				if (const auto* value = find(*scope)) return value;
			return _document.FindItemsPanelTemplate(key);
		}

		const DesignGroupStyle* FindVisibleGroupStyle(
			const std::wstring& key) const
		{
			auto find = [&](const DesignObjectResourceDictionary& resources)
				-> const DesignGroupStyle*
			{
				const auto item = std::find_if(resources.GroupStyles.rbegin(),
					resources.GroupStyles.rend(), [&](const auto& value)
					{ return Equals(value.Key, key); });
				return item == resources.GroupStyles.rend() ? nullptr : &*item;
			};
			if (_objectResourceTarget)
				if (const auto* value = find(*_objectResourceTarget)) return value;
			for (auto scope = _objectResourceScopes.rbegin();
				scope != _objectResourceScopes.rend(); ++scope)
				if (const auto* value = find(*scope)) return value;
			return _document.FindGroupStyle(key);
		}

		const DesignerStyleResource* FindVisibleResource(
			const std::wstring& key) const
		{
			auto find = [&](const DesignerStyleSheet& sheet)
				-> const DesignerStyleResource*
			{
				const auto item = std::find_if(sheet.Resources.rbegin(),
					sheet.Resources.rend(), [&](const auto& candidate)
					{ return Equals(candidate.Key, key); });
				return item == sheet.Resources.rend() ? nullptr : &*item;
			};
			if (_resourceTarget && _resourceTarget != &_document.StyleSheet)
				if (const auto* value = find(*_resourceTarget)) return value;
			for (auto scope = _resourceScopes.rbegin();
				scope != _resourceScopes.rend(); ++scope)
				if (const auto* value = find(*scope)) return value;
			return find(_document.StyleSheet);
		}

		DesignerStyleSheet VisibleStyleSheet(
			const DesignerStyleSheet* extra = nullptr) const
		{
			DesignerStyleSheet result = _document.StyleSheet;
			auto append = [&](const DesignerStyleSheet& source)
			{
				for (const auto& dictionary : source.MergedDictionaries)
					if (std::none_of(result.MergedDictionaries.begin(),
						result.MergedDictionaries.end(), [&](const auto& current)
						{ return Equals(current, dictionary); }))
						result.MergedDictionaries.push_back(dictionary);
				for (const auto& resource : source.Resources)
				{
					result.Resources.erase(std::remove_if(
						result.Resources.begin(), result.Resources.end(),
						[&](const auto& current)
						{ return Equals(current.Key, resource.Key); }),
						result.Resources.end());
					result.Resources.push_back(resource);
				}
				result.Rules.insert(
					result.Rules.end(), source.Rules.begin(), source.Rules.end());
			};
			for (const auto& scope : _resourceScopes) append(scope);
			if (_resourceTarget && _resourceTarget != &_document.StyleSheet)
				append(*_resourceTarget);
			if (extra && extra != _resourceTarget) append(*extra);
			return result;
		}

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
				if (const auto objectType = Attribute(item, L"ObjectType"))
				{
					if (!DesignerDataContextSchemaUtils::TryParseObjectKind(
						*objectType, property.ObjectKind))
						return Fail(L"DataContext 对象契约无效：" + *objectType, error);
				}
				property.ItemType = Trim(Attribute(item, L"ItemType").value_or(L""));
				property.DataType = Trim(Attribute(item, L"DataType").value_or(L""));
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
			// Nested dictionaries and local value resources are data dependencies of
			// component defaults. Resolve them before component schema and Style,
			// independent of authoring order. Direct local values keep precedence.
			for (const auto& item : children)
			{
				if (Equals(FromUtf8(item->LocalName()), L"ResourceDictionary"))
					if (!ParseResourceDictionary(item, error)) return false;
			}
			for (const auto& item : children)
			{
				const auto name = FromUtf8(item->LocalName());
				if (_parsingLocalResources
					&& (Equals(name, L"DataType")
						|| Equals(name, L"DataList")
						|| Equals(name, L"CollectionViewSource")))
					return Fail(L"控件级 ResourceDictionary 当前只接受值、画刷、"
						L"图形、变换、图像、Style、ControlTemplate、DataTemplate、HierarchicalDataTemplate、ItemsPanelTemplate、"
						L"GroupStyle 和 ComponentDefinition；"
						L"其他结构型资源仍应放在 Form.Resources。",
						error);
				if (Equals(name, L"ResourceDictionary.MergedDictionaries")
					|| Equals(name, L"ResourceDictionary")
					|| Equals(name, L"DataType")
					|| Equals(name, L"DataList")
					|| Equals(name, L"CollectionViewSource")
					|| Equals(name, L"ControlTemplate")
					|| Equals(name, L"DataTemplate")
					|| Equals(name, L"HierarchicalDataTemplate")
					|| Equals(name, L"ItemsPanelTemplate")
					|| Equals(name, L"GroupStyle")
					|| Equals(name, L"ComponentDefinition")
					|| Equals(name, L"Style")) continue;
				DiagnosticContext itemContext(*this, item);
				if (!ParseResourceItem(item, error)) return false;
			}
			// Data and component types are schema declarations, so resource source
			// order must not decide whether templates/styles can resolve them.
			for (const auto& item : children)
			{
				if (!Equals(FromUtf8(item->LocalName()), L"DataType")) continue;
				if (!ParseDataType(item, error)) return false;
			}
			for (const auto& item : children)
			{
				if (!Equals(FromUtf8(item->LocalName()), L"DataList")) continue;
				if (!ParseDataList(item, error)) return false;
			}
			for (const auto& item : children)
			{
				if (!Equals(FromUtf8(item->LocalName()), L"CollectionViewSource"))
					continue;
				if (!ParseCollectionViewSource(item, error)) return false;
			}
			for (const auto& item : children)
			{
				if (!Equals(FromUtf8(item->LocalName()), L"ComponentDefinition"))
					continue;
				if (!ParseComponentDefinition(item, error)) return false;
			}
			// Styles are lexical dependencies of controls nested inside templates.
			// Parse them after component/type declarations but before template visual
			// trees, regardless of the implementation's structural-resource passes.
			for (const auto& item : children)
			{
				if (!Equals(FromUtf8(item->LocalName()), L"Style")) continue;
				DiagnosticContext itemContext(*this, item);
				if (!ParseResourceItem(item, error)) return false;
			}
			for (const auto& item : children)
			{
				if (!Equals(FromUtf8(item->LocalName()), L"ControlTemplate"))
					continue;
				if (!ParseControlTemplate(item, error)) return false;
			}
			for (const auto& item : children)
			{
				if (!Equals(FromUtf8(item->LocalName()), L"ItemsPanelTemplate"))
					continue;
				if (!ParseItemsPanelTemplate(item, error)) return false;
			}
			for (const auto& item : children)
			{
				const auto name = FromUtf8(item->LocalName());
				if (!Equals(name, L"DataTemplate")
					&& !Equals(name, L"HierarchicalDataTemplate")) continue;
				if (!ParseDataTemplate(item, error)) return false;
			}
			for (const auto& item : children)
			{
				if (!Equals(FromUtf8(item->LocalName()), L"GroupStyle")) continue;
				if (!ParseGroupStyle(item, error)) return false;
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
			auto& resources = (_resourceTarget
				? *_resourceTarget : _document.StyleSheet).Resources;
			resources.erase(std::remove_if(resources.begin(), resources.end(),
				[&](const auto& current) { return Equals(current.Key, resource.Key); }),
				resources.end());
			resources.push_back(std::move(resource));
		}

		void AddStyleRule(DesignerStyleRule rule)
		{
			// CUI styles may intentionally share an Id while targeting different
			// states. Source order already gives local dictionary rules precedence.
			(_resourceTarget ? *_resourceTarget : _document.StyleSheet)
				.Rules.push_back(std::move(rule));
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
						(_resourceTarget ? *_resourceTarget : _document.StyleSheet)
							.MergedDictionaries.begin(),
						(_resourceTarget ? *_resourceTarget : _document.StyleSheet)
							.MergedDictionaries.end(),
						[&](const auto& current) { return Equals(current, authorUri); }))
						(_resourceTarget ? *_resourceTarget : _document.StyleSheet)
							.MergedDictionaries.push_back(authorUri);
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
			if (Equals(name, L"ComponentDefinition"))
				return ParseComponentDefinition(item, error);
			if (Equals(name, L"DataType"))
				return ParseDataType(item, error);
			if (Equals(name, L"DataList"))
				return ParseDataList(item, error);
			if (Equals(name, L"CollectionViewSource"))
				return ParseCollectionViewSource(item, error);
			if (Equals(name, L"ControlTemplate"))
				return ParseControlTemplate(item, error);
			if (Equals(name, L"DataTemplate"))
				return ParseDataTemplate(item, error);
			if (Equals(name, L"HierarchicalDataTemplate"))
				return ParseDataTemplate(item, error);
			if (Equals(name, L"ItemsPanelTemplate"))
				return ParseItemsPanelTemplate(item, error);
			if (Equals(name, L"GroupStyle"))
				return ParseGroupStyle(item, error);
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

		static std::wstring DefaultComponentValue(
			DesignerStyleValueKind kind)
		{
			switch (kind)
			{
			case DesignerStyleValueKind::Bool: return L"false";
			case DesignerStyleValueKind::Int:
			case DesignerStyleValueKind::Int64:
			case DesignerStyleValueKind::Float:
			case DesignerStyleValueKind::Double: return L"0";
			case DesignerStyleValueKind::String: return {};
			case DesignerStyleValueKind::Color: return L"#00000000";
			case DesignerStyleValueKind::Thickness: return L"0";
			case DesignerStyleValueKind::Point: return L"0, 0";
			case DesignerStyleValueKind::Vector: return L"0, 0";
			case DesignerStyleValueKind::Rect: return L"0, 0, 0, 0";
			case DesignerStyleValueKind::Size: return L"0, 0";
			case DesignerStyleValueKind::Matrix: return L"1, 0, 0, 1, 0, 0";
			case DesignerStyleValueKind::Length: return L"Auto";
			default: return {};
			}
		}

		static bool IsSupportedComponentPropertyKind(
			DesignerStyleValueKind kind)
		{
			switch (kind)
			{
			case DesignerStyleValueKind::Bool:
			case DesignerStyleValueKind::Int:
			case DesignerStyleValueKind::Int64:
			case DesignerStyleValueKind::Float:
			case DesignerStyleValueKind::Double:
			case DesignerStyleValueKind::String:
			case DesignerStyleValueKind::Color:
			case DesignerStyleValueKind::Thickness:
			case DesignerStyleValueKind::Point:
			case DesignerStyleValueKind::Vector:
			case DesignerStyleValueKind::Rect:
			case DesignerStyleValueKind::Size:
			case DesignerStyleValueKind::Matrix:
			case DesignerStyleValueKind::Length:
			case DesignerStyleValueKind::Brush:
			case DesignerStyleValueKind::Geometry:
			case DesignerStyleValueKind::Transform:
				return true;
			default:
				return false;
			}
		}

		static bool IsNumericComponentPropertyKind(
			DesignerStyleValueKind kind)
		{
			return kind == DesignerStyleValueKind::Int
				|| kind == DesignerStyleValueKind::Int64
				|| kind == DesignerStyleValueKind::Float
				|| kind == DesignerStyleValueKind::Double;
		}

		bool ParseComponentPropertyEditor(
			const std::wstring& value,
			ControlPropertyEditorKind& editor,
			std::wstring& error)
		{
			for (const auto& [name, kind] : {
				std::pair{ L"Auto", ControlPropertyEditorKind::Auto },
				std::pair{ L"Text", ControlPropertyEditorKind::Text },
				std::pair{ L"Boolean", ControlPropertyEditorKind::Boolean },
				std::pair{ L"Number", ControlPropertyEditorKind::Number },
				std::pair{ L"Choice", ControlPropertyEditorKind::Choice },
				std::pair{ L"Color", ControlPropertyEditorKind::Color },
				std::pair{ L"Thickness", ControlPropertyEditorKind::Thickness },
				std::pair{ L"Size", ControlPropertyEditorKind::Size },
				std::pair{ L"Length", ControlPropertyEditorKind::Length } })
			{
				if (Equals(value, name))
				{
					editor = kind;
					return true;
				}
			}
			return Fail(L"组件属性 Editor 无效：" + value, error);
		}

		bool ParseDataType(const Element& element, std::wstring& error)
		{
			DiagnosticContext context(*this, element);
			if (!ValidateAttributes(element, { L"Key" }, error)) return false;
			DesignDataTypeDefinition definition;
			definition.Name = Trim(Attribute(element, L"Key", L"x").value_or(
				Attribute(element, L"Key").value_or(L"")));
			if (!ValidateIdentifier(definition.Name, L"DataType x:Key", error))
				return false;
			definition.SourceDictionary = _currentDictionaryOrigin;
			const auto children = ChildElements(element);
			if (children.size() != 1
				|| !Equals(FromUtf8(children.front()->LocalName()),
					L"DataType.Properties"))
				return Fail(L"DataType 必须且只能包含 DataType.Properties。", error);
			if (!ValidateAttributes(children.front(), {}, error)) return false;
			for (const auto& item : ChildElements(children.front()))
			{
				DiagnosticContext itemContext(*this, item);
				if (!Equals(FromUtf8(item->LocalName()), L"Property"))
					return Fail(L"DataType.Properties 仅支持 Property。", error);
				if (!ValidateAttributes(item, { L"Path", L"Kind", L"ObjectType",
					L"ItemType", L"DataType", L"CanRead", L"CanWrite", L"CanObserve" }, error))
					return false;
				DesignerDataContextProperty property;
				property.Path = Trim(Attribute(item, L"Path").value_or(L""));
				const auto kind = Attribute(item, L"Kind").value_or(L"Unknown");
				if (!DesignerDataContextSchemaUtils::TryParseValueKind(
					kind, property.ValueKind))
					return Fail(L"DataType 属性类型无效：" + kind, error);
				if (const auto objectType = Attribute(item, L"ObjectType"))
					if (!DesignerDataContextSchemaUtils::TryParseObjectKind(
						*objectType, property.ObjectKind))
						return Fail(L"DataType 对象契约无效：" + *objectType, error);
				property.ItemType = Trim(Attribute(item, L"ItemType").value_or(L""));
				property.DataType = Trim(Attribute(item, L"DataType").value_or(L""));
				for (const auto& [name, target] : {
					std::pair{ L"CanRead", &property.CanRead },
					std::pair{ L"CanWrite", &property.CanWrite },
					std::pair{ L"CanObserve", &property.CanObserve } })
					if (const auto value = Attribute(item, name))
						if (!TryParseBool(*value, *target))
							return Fail(L"DataType 属性能力必须为布尔值："
								+ std::wstring(name), error);
				definition.Properties.push_back(std::move(property));
			}
			DesignerDataContextSchemaUtils::Canonicalize(definition.Properties);
			std::wstring schemaError;
			if (definition.Properties.empty()
				|| !DesignerDataContextSchemaUtils::Validate(
					definition.Properties, &schemaError))
				return Fail(definition.Properties.empty()
					? L"DataType 至少需要一个 Property。"
					: L"DataType " + definition.Name + L"：" + schemaError, error);
			_document.DataTypes.erase(std::remove_if(
				_document.DataTypes.begin(), _document.DataTypes.end(),
				[&](const DesignDataTypeDefinition& existing)
				{ return Equals(existing.Name, definition.Name); }),
				_document.DataTypes.end());
			_document.DataTypes.push_back(std::move(definition));
			return true;
		}

		bool ParseDataList(const Element& element, std::wstring& error)
		{
			DiagnosticContext context(*this, element);
			if (!ValidateAttributes(element, { L"Key", L"ItemType" }, error))
				return false;
			DesignDataList definition;
			definition.Key = Trim(Attribute(element, L"Key", L"x").value_or(
				Attribute(element, L"Key").value_or(L"")));
			definition.ItemType = Trim(
				Attribute(element, L"ItemType").value_or(L""));
			if (!ValidateIdentifier(definition.Key, L"DataList x:Key", error)
				|| !ValidateIdentifier(
					definition.ItemType, L"DataList ItemType", error)) return false;
			definition.SourceDictionary = _currentDictionaryOrigin;
			for (const auto& recordElement : ChildElements(element))
			{
				DiagnosticContext recordContext(*this, recordElement);
				if (!Equals(FromUtf8(recordElement->LocalName()), L"DataRecord"))
					return Fail(L"DataList 仅支持 DataRecord 子项。", error);
				DesignDataRecord record;
				auto addField = [&](std::wstring path, std::wstring value)
				{
					path = DesignerDataContextSchemaUtils::NormalizePath(path);
					if (!DesignerDataContextSchemaUtils::IsValidPath(path)
						|| std::any_of(record.Fields.begin(), record.Fields.end(),
							[&](const auto& field) { return Equals(field.first, path); }))
						return false;
					record.Fields.emplace(std::move(path), std::move(value));
					return true;
				};
				for (const auto& attribute : recordElement->Attributes())
				{
					if (!attribute || IsNamespaceAttribute(*attribute)) continue;
					const auto prefix = FromUtf8(attribute->Prefix());
					if (!prefix.empty())
						return Fail(L"DataRecord 字段不支持命名空间前缀："
							+ FromUtf8(attribute->Name()), error);
					if (!addField(FromUtf8(attribute->LocalName()),
						FromUtf8(attribute->Value())))
						return Fail(L"DataRecord 字段路径无效或重复："
							+ FromUtf8(attribute->LocalName()), error);
				}
				for (const auto& fieldElement : ChildElements(recordElement))
				{
					DiagnosticContext fieldContext(*this, fieldElement);
					if (!Equals(FromUtf8(fieldElement->LocalName()), L"Field")
						|| !ValidateAttributes(
							fieldElement, { L"Path", L"Value" }, error))
						return Fail(L"DataRecord 仅支持 Field(Path, Value)。", error);
					const auto path = Attribute(fieldElement, L"Path").value_or(L"");
					const auto value = Attribute(fieldElement, L"Value").value_or(
						FromUtf8(fieldElement->InnerText()));
					if (!addField(path, value))
						return Fail(L"DataRecord Field 路径无效或重复：" + path,
							error);
				}
				definition.Records.push_back(std::move(record));
			}
			_document.DataLists.erase(std::remove_if(
				_document.DataLists.begin(), _document.DataLists.end(),
				[&](const DesignDataList& existing)
				{ return Equals(existing.Key, definition.Key); }),
				_document.DataLists.end());
			_document.DataLists.push_back(std::move(definition));
			return true;
		}

		bool ParseCollectionViewSource(
			const Element& element,
			std::wstring& error)
		{
			DiagnosticContext context(*this, element);
			if (!ValidateAttributes(element, { L"Key", L"Source" }, error))
				return false;
			DesignCollectionViewSource definition;
			definition.Key = Trim(Attribute(element, L"Key", L"x").value_or(
				Attribute(element, L"Key").value_or(L"")));
			if (!ValidateIdentifier(
				definition.Key, L"CollectionViewSource x:Key", error)) return false;
			const auto source = Attribute(element, L"Source").value_or(L"");
			if (!TryParseStaticResource(source, definition.SourceResource))
			{
				DesignerDataBinding binding;
				std::wstring bindingError;
				if (!TryParseBinding(source, binding, bindingError)
					|| binding.SourceProperty.empty() || !binding.Converter.empty()
					|| !binding.ElementName.empty()
					|| binding.RelativeSource != DesignerBindingRelativeSource::None
					|| binding.FallbackValue || binding.TargetNullValue
					|| binding.ConverterParameter || binding.StringFormat)
					return Fail(bindingError.empty()
						? L"CollectionViewSource Source 必须使用 {StaticResource key} 或不带转换器的 {Binding Path}。"
						: L"CollectionViewSource Source：" + bindingError, error);
				definition.SourceBindingPath = binding.SourceProperty;
			}
			for (const auto& propertyElement : ChildElements(element))
			{
				DiagnosticContext propertyContext(*this, propertyElement);
				const auto propertyName = FromUtf8(propertyElement->LocalName());
				if (!ValidateAttributes(propertyElement, {}, error)) return false;
				if (Equals(propertyName,
					L"CollectionViewSource.GroupDescriptions"))
				{
					for (const auto& item : ChildElements(propertyElement))
					{
						DiagnosticContext itemContext(*this, item);
						if (!Equals(FromUtf8(item->LocalName()), L"GroupDescription")
							|| !ValidateAttributes(item,
								{ L"PropertyName", L"Direction", L"IgnoreCase" }, error))
							return Fail(L"GroupDescriptions 仅支持 GroupDescription。", error);
						DesignCollectionGroupDescription group;
						group.PropertyName = DesignerDataContextSchemaUtils::NormalizePath(
							Attribute(item, L"PropertyName").value_or(L""));
						if (!DesignerDataContextSchemaUtils::IsValidPath(group.PropertyName))
							return Fail(L"GroupDescription PropertyName 无效。", error);
						const auto direction = Attribute(
							item, L"Direction").value_or(L"Ascending");
						if (Equals(direction, L"Ascending"))
							group.Direction = CollectionSortDirection::Ascending;
						else if (Equals(direction, L"Descending"))
							group.Direction = CollectionSortDirection::Descending;
						else return Fail(L"GroupDescription Direction 必须为 Ascending 或 Descending。", error);
						if (const auto ignoreCase = Attribute(item, L"IgnoreCase"))
							if (!TryParseBool(*ignoreCase, group.IgnoreCase))
								return Fail(L"GroupDescription IgnoreCase 必须为布尔值。", error);
						definition.GroupDescriptions.push_back(std::move(group));
					}
					continue;
				}
				if (Equals(propertyName,
					L"CollectionViewSource.SortDescriptions"))
				{
					for (const auto& item : ChildElements(propertyElement))
					{
						DiagnosticContext itemContext(*this, item);
						if (!Equals(FromUtf8(item->LocalName()), L"SortDescription")
							|| !ValidateAttributes(item,
								{ L"PropertyName", L"Direction", L"IgnoreCase" }, error))
							return Fail(L"SortDescriptions 仅支持 SortDescription。", error);
						DesignCollectionSortDescription sort;
						sort.PropertyName = DesignerDataContextSchemaUtils::NormalizePath(
							Attribute(item, L"PropertyName").value_or(L""));
						if (!DesignerDataContextSchemaUtils::IsValidPath(sort.PropertyName))
							return Fail(L"SortDescription PropertyName 无效。", error);
						const auto direction = Attribute(
							item, L"Direction").value_or(L"Ascending");
						if (Equals(direction, L"Ascending"))
							sort.Direction = CollectionSortDirection::Ascending;
						else if (Equals(direction, L"Descending"))
							sort.Direction = CollectionSortDirection::Descending;
						else return Fail(L"SortDescription Direction 必须为 Ascending 或 Descending。", error);
						if (const auto ignoreCase = Attribute(item, L"IgnoreCase"))
							if (!TryParseBool(*ignoreCase, sort.IgnoreCase))
								return Fail(L"SortDescription IgnoreCase 必须为布尔值。", error);
						definition.SortDescriptions.push_back(std::move(sort));
					}
					continue;
				}
				if (Equals(propertyName,
					L"CollectionViewSource.AggregateDescriptions"))
				{
					for (const auto& item : ChildElements(propertyElement))
					{
						DiagnosticContext itemContext(*this, item);
						if (!Equals(FromUtf8(item->LocalName()), L"AggregateDescription")
							|| !ValidateAttributes(item,
								{ L"Name", L"PropertyName", L"Function" }, error))
							return Fail(L"AggregateDescriptions 仅支持 AggregateDescription。", error);
						DesignCollectionAggregateDescription aggregate;
						aggregate.Name = Trim(Attribute(item, L"Name").value_or(L""));
						aggregate.PropertyName = DesignerDataContextSchemaUtils::NormalizePath(
							Attribute(item, L"PropertyName").value_or(L""));
						const auto function = Attribute(item, L"Function").value_or(L"Count");
						if (Equals(function, L"Count"))
							aggregate.Function = CollectionAggregateFunction::Count;
						else if (Equals(function, L"Sum"))
							aggregate.Function = CollectionAggregateFunction::Sum;
						else if (Equals(function, L"Average"))
							aggregate.Function = CollectionAggregateFunction::Average;
						else if (Equals(function, L"Min"))
							aggregate.Function = CollectionAggregateFunction::Min;
						else if (Equals(function, L"Max"))
							aggregate.Function = CollectionAggregateFunction::Max;
						else return Fail(L"AggregateDescription Function 必须为 Count、Sum、Average、Min 或 Max。", error);
						definition.AggregateDescriptions.push_back(std::move(aggregate));
					}
					continue;
				}
				if (Equals(propertyName,
					L"CollectionViewSource.FilterDescriptions"))
				{
					for (const auto& item : ChildElements(propertyElement))
					{
						DiagnosticContext itemContext(*this, item);
						if (!Equals(FromUtf8(item->LocalName()), L"FilterDescription")
							|| !ValidateAttributes(item,
								{ L"PropertyName", L"Operator", L"Value", L"IgnoreCase" }, error))
							return Fail(L"FilterDescriptions 仅支持 FilterDescription。", error);
						DesignCollectionFilterDescription filter;
						filter.PropertyName = DesignerDataContextSchemaUtils::NormalizePath(
							Attribute(item, L"PropertyName").value_or(L""));
						if (!DesignerDataContextSchemaUtils::IsValidPath(filter.PropertyName))
							return Fail(L"FilterDescription PropertyName 无效。", error);
						const auto op = Attribute(item, L"Operator").value_or(L"Equals");
						const std::pair<const wchar_t*, CollectionFilterOperator> operators[] = {
							{ L"Equals", CollectionFilterOperator::Equals },
							{ L"NotEquals", CollectionFilterOperator::NotEquals },
							{ L"LessThan", CollectionFilterOperator::LessThan },
							{ L"LessThanOrEqual", CollectionFilterOperator::LessThanOrEqual },
							{ L"GreaterThan", CollectionFilterOperator::GreaterThan },
							{ L"GreaterThanOrEqual", CollectionFilterOperator::GreaterThanOrEqual },
							{ L"Contains", CollectionFilterOperator::Contains },
							{ L"StartsWith", CollectionFilterOperator::StartsWith },
							{ L"EndsWith", CollectionFilterOperator::EndsWith },
							{ L"IsEmpty", CollectionFilterOperator::IsEmpty },
							{ L"IsNotEmpty", CollectionFilterOperator::IsNotEmpty }
						};
						const auto found = std::find_if(std::begin(operators), std::end(operators),
							[&](const auto& candidate) { return Equals(op, candidate.first); });
						if (found == std::end(operators))
							return Fail(L"FilterDescription Operator 无效：" + op, error);
						filter.Operator = found->second;
						filter.Value = Attribute(item, L"Value").value_or(L"");
						if (const auto ignoreCase = Attribute(item, L"IgnoreCase"))
							if (!TryParseBool(*ignoreCase, filter.IgnoreCase))
								return Fail(L"FilterDescription IgnoreCase 必须为布尔值。", error);
						definition.FilterDescriptions.push_back(std::move(filter));
					}
					continue;
				}
				return Fail(L"CollectionViewSource 仅支持 GroupDescriptions、AggregateDescriptions、SortDescriptions 和 FilterDescriptions。", error);
			}
			definition.SourceDictionary = _currentDictionaryOrigin;
			_document.CollectionViews.erase(std::remove_if(
				_document.CollectionViews.begin(), _document.CollectionViews.end(),
				[&](const auto& existing)
				{ return Equals(existing.Key, definition.Key); }),
				_document.CollectionViews.end());
			_document.CollectionViews.push_back(std::move(definition));
			return true;
		}

		bool ParseControlTemplate(const Element& element, std::wstring& error)
		{
			DiagnosticContext context(*this, element);
			if (!ValidateAttributes(element, { L"Key", L"TargetType" }, error))
				return false;
			DesignControlTemplate definition;
			definition.Key = Trim(Attribute(element, L"Key", L"x").value_or(
				Attribute(element, L"Key").value_or(L"")));
			if (!definition.Key.empty()
				&& !ValidateIdentifier(definition.Key, L"ControlTemplate x:Key", error))
				return false;
			const auto targetType = Trim(
				Attribute(element, L"TargetType").value_or(L""));
			const auto typeToken = MarkupTypeToken(targetType);
			const auto separator = typeToken.find(L':');
			const DesignComponentDefinition* targetComponent = nullptr;
			if (separator != std::wstring::npos && separator > 0
				&& separator + 1 < typeToken.size())
			{
				const auto prefix = Trim(typeToken.substr(0, separator));
				const auto localName = Trim(typeToken.substr(separator + 1));
				targetComponent = FindVisibleComponent(
					LookupNamespaceUri(element, prefix), localName);
			}
			if (targetComponent)
			{
				definition.TargetType = targetComponent->BaseType;
				definition.TargetComponentType = targetComponent->Type;
			}
			else if (!TryParseType(StripMarkupType(typeToken), definition.TargetType)
				|| !IsControlTemplateHostType(definition.TargetType))
				return Fail(L"ControlTemplate TargetType 必须是 ContentControl、"
					L"Button、GroupBox、Expander、ItemsControl、ListBox "
					L"或已声明的组件 QName："
					+ targetType, error);
			const auto roots = ChildElements(element);
			if (roots.size() != 1)
				return Fail(L"ControlTemplate 必须且只能包含一个视觉根。", error);

			auto targetProbe = DesignDocumentMaterializer::CreateRuntimeControl(
				definition.TargetType);
			if (!targetProbe)
				return Fail(L"ControlTemplate TargetType 尚无运行时工厂："
					+ targetType, error);
			if (targetComponent
				&& !DefineComponentProbeProperties(
					*targetProbe, *targetComponent, error)) return false;
			targetProbe->EnsureBindingPropertiesRegistered();

			DesignComponentDefinition templateContext;
			if (targetComponent)
			{
				templateContext = *targetComponent;
				templateContext.Template.clear();
				templateContext.VisualStateGroups.clear();
				templateContext.EventTriggers.clear();
			}
			else
			{
				templateContext.BaseType = definition.TargetType;
				templateContext.Type.XamlNamespace = L"urn:cui";
				templateContext.Type.XamlName =
					DesignerStyleSheetUtils::UIClassName(definition.TargetType);
			}

			auto pageNodes = std::move(_document.Nodes);
			const int pageNextStableId = _document.NextStableId;
			auto pageUsedIds = std::move(_usedIds);
			auto pageUsedNames = std::move(_usedNames);
			auto pageNameCounters = std::move(_nameCounters);
			_document.Nodes.clear();
			_document.NextStableId = 1;
			_usedIds.clear();
			_usedNames.clear();
			_nameCounters.clear();
			auto* previousTemplate = _activeTemplateComponent;
			auto* previousControlTemplateProbe = _activeControlTemplateProbe;
			const bool previousControlTemplateVisual =
				_parsingControlTemplateVisual;
			const bool previousComponentTemplateVisual =
				_parsingComponentTemplateVisual;
			const auto previousVisualStateGroups = _pendingVisualStateGroups;
			const auto previousEventTriggers = _pendingEventTriggers;
			_pendingVisualStateGroups.reset();
			_pendingEventTriggers.reset();
			_activeTemplateComponent = &templateContext;
			_activeControlTemplateProbe = targetProbe.get();
			_parsingControlTemplateVisual = true;
			_parsingComponentTemplateVisual = false;
			bool parsed = ParseControl(roots.front(), Parent{}, error);
			if (parsed && (_pendingVisualStateGroups || _pendingEventTriggers))
				templateContext.Template = _document.Nodes;
			if (parsed && _pendingVisualStateGroups)
				parsed = ParseVisualStateGroups(
					_pendingVisualStateGroups, templateContext, error);
			if (parsed && _pendingEventTriggers)
				parsed = ParseEventTriggers(
					_pendingEventTriggers, templateContext, error);
			_activeTemplateComponent = previousTemplate;
			_activeControlTemplateProbe = previousControlTemplateProbe;
			_parsingControlTemplateVisual = previousControlTemplateVisual;
			_parsingComponentTemplateVisual = previousComponentTemplateVisual;
			_pendingVisualStateGroups = previousVisualStateGroups;
			_pendingEventTriggers = previousEventTriggers;
			if (parsed)
			{
				definition.Template = std::move(_document.Nodes);
				definition.VisualStateGroups =
					std::move(templateContext.VisualStateGroups);
				definition.EventTriggers =
					std::move(templateContext.EventTriggers);
			}
			_document.Nodes = std::move(pageNodes);
			_document.NextStableId = pageNextStableId;
			_usedIds = std::move(pageUsedIds);
			_usedNames = std::move(pageUsedNames);
			_nameCounters = std::move(pageNameCounters);
			if (!parsed) return false;

			definition.SourceDictionary = _currentDictionaryOrigin;
			auto& templates = _objectResourceTarget
				? _objectResourceTarget->ControlTemplates
				: _document.ControlTemplates;
			templates.erase(std::remove_if(
				templates.begin(), templates.end(), [&](const auto& existing)
				{ return existing.HasSameResourceIdentity(definition); }),
				templates.end());
			templates.push_back(std::move(definition));
			return true;
		}

		bool ParseDataTemplate(const Element& element, std::wstring& error)
		{
			DiagnosticContext context(*this, element);
			const bool hierarchical = Equals(
				FromUtf8(element->LocalName()), L"HierarchicalDataTemplate");
			if (hierarchical)
			{
				if (!ValidateAttributes(
					element, { L"Key", L"DataType", L"ItemsSource" }, error))
					return false;
			}
			else if (!ValidateAttributes(element, { L"Key", L"DataType" }, error))
				return false;
			DesignDataTemplate definition;
			definition.Hierarchical = hierarchical;
			definition.Key = Trim(Attribute(element, L"Key", L"x").value_or(
				Attribute(element, L"Key").value_or(L"")));
			if (!definition.Key.empty()
				&& !ValidateIdentifier(definition.Key, L"DataTemplate x:Key", error))
				return false;
			definition.DataType = Trim(Attribute(element, L"DataType").value_or(L""));
			if (!ValidateIdentifier(definition.DataType, L"DataTemplate DataType", error))
				return false;
			if (const auto source = Attribute(element, L"ItemsSource"))
			{
				DesignerDataBinding binding;
				std::wstring bindingError;
				if (!TryParseBinding(*source, binding, bindingError))
					return Fail(bindingError.empty()
						? L"HierarchicalDataTemplate.ItemsSource 必须使用 Binding。"
						: L"HierarchicalDataTemplate.ItemsSource：" + bindingError,
						error);
				definition.ItemsSourceBinding = std::move(binding);
			}
			const auto roots = ChildElements(element);
			if (roots.size() != 1)
				return Fail(L"DataTemplate 必须且只能包含一个视觉根。", error);

			auto pageNodes = std::move(_document.Nodes);
			const int pageNextStableId = _document.NextStableId;
			auto pageUsedIds = std::move(_usedIds);
			auto pageUsedNames = std::move(_usedNames);
			auto pageNameCounters = std::move(_nameCounters);
			auto pageBindingPaths = std::move(_bindingPaths);
			const bool previousControlTemplateVisual =
				_parsingControlTemplateVisual;
			const bool previousComponentTemplateVisual =
				_parsingComponentTemplateVisual;
			_document.Nodes.clear();
			_document.NextStableId = 1;
			_usedIds.clear();
			_usedNames.clear();
			_nameCounters.clear();
			_bindingPaths.clear();
			_parsingControlTemplateVisual = false;
			_parsingComponentTemplateVisual = false;
			const bool parsed = ParseControl(roots.front(), Parent{}, error);
			const auto templateBindingPaths = std::move(_bindingPaths);
			if (parsed) definition.Template = std::move(_document.Nodes);
			_document.Nodes = std::move(pageNodes);
			_document.NextStableId = pageNextStableId;
			_usedIds = std::move(pageUsedIds);
			_usedNames = std::move(pageUsedNames);
			_nameCounters = std::move(pageNameCounters);
			_bindingPaths = std::move(pageBindingPaths);
			_parsingControlTemplateVisual = previousControlTemplateVisual;
			_parsingComponentTemplateVisual = previousComponentTemplateVisual;
			if (!parsed) return false;
			(void)templateBindingPaths;
			definition.SourceDictionary = _currentDictionaryOrigin;
			auto& dataTemplates = _objectResourceTarget
				? _objectResourceTarget->DataTemplates : _document.DataTemplates;
			dataTemplates.erase(std::remove_if(
				dataTemplates.begin(), dataTemplates.end(),
				[&](const DesignDataTemplate& existing)
				{ return existing.HasSameResourceIdentity(definition); }),
				dataTemplates.end());
			dataTemplates.push_back(std::move(definition));
			return true;
		}

		bool ParseItemsPanelTemplate(
			const Element& element,
			std::wstring& error)
		{
			DiagnosticContext context(*this, element);
			if (!ValidateAttributes(element, { L"Key" }, error)) return false;
			DesignItemsPanelTemplate definition;
			definition.Key = Trim(Attribute(element, L"Key", L"x").value_or(
				Attribute(element, L"Key").value_or(L"")));
			if (!ValidateIdentifier(
				definition.Key, L"ItemsPanelTemplate x:Key", error)) return false;
			const auto children = ChildElements(element);
			if (children.size() != 1)
				return Fail(L"ItemsPanelTemplate 必须且只能包含一个面板声明。", error);
			const auto& panel = children.front();
			DiagnosticContext panelContext(*this, panel);
			const auto panelName = FromUtf8(panel->LocalName());
			if (Equals(panelName, L"StackPanel"))
			{
				definition.Value.Kind = ItemsPanelKind::Stack;
				if (!ValidateAttributes(panel,
					{ L"Orientation", L"Spacing" }, error)) return false;
			}
			else if (Equals(panelName, L"WrapPanel"))
			{
				definition.Value.Kind = ItemsPanelKind::Wrap;
				if (!ValidateAttributes(panel,
					{ L"Orientation", L"ItemWidth", L"ItemHeight" }, error))
					return false;
			}
			else if (Equals(panelName, L"VirtualizingStackPanel"))
			{
				definition.Value.Kind = ItemsPanelKind::VirtualizingStack;
				if (!ValidateAttributes(panel,
					{ L"Orientation", L"Spacing", L"ItemHeight", L"CacheLength" },
					error)) return false;
			}
			else
				return Fail(L"ItemsPanelTemplate 仅支持 StackPanel、WrapPanel 或 VirtualizingStackPanel。", error);

			const auto orientation = Attribute(panel, L"Orientation").value_or(
				definition.Value.Kind == ItemsPanelKind::Wrap
					? L"Horizontal" : L"Vertical");
			if (Equals(orientation, L"Vertical"))
				definition.Value.Orientation = Orientation::Vertical;
			else if (Equals(orientation, L"Horizontal"))
				definition.Value.Orientation = Orientation::Horizontal;
			else
				return Fail(L"ItemsPanelTemplate Orientation 必须为 Horizontal 或 Vertical。", error);

			auto parseNonNegative = [&](const wchar_t* name, float& output)
			{
				const auto text = Attribute(panel, name);
				if (!text) return true;
				double parsed = 0.0;
				if (!TryParseDouble(*text, parsed) || parsed < 0.0
					|| parsed > static_cast<double>((std::numeric_limits<float>::max)()))
					return Fail(L"ItemsPanelTemplate " + std::wstring(name)
						+ L" 必须为有限非负数。", error);
				output = static_cast<float>(parsed);
				return true;
			};
			if (!parseNonNegative(L"Spacing", definition.Value.Spacing)
				|| !parseNonNegative(L"ItemWidth", definition.Value.ItemWidth)
				|| !parseNonNegative(L"ItemHeight", definition.Value.ItemHeight)
				|| !parseNonNegative(L"CacheLength", definition.Value.CacheLength))
				return false;
			if (!Attribute(panel, L"CacheLength")) definition.Value.CacheLength = 1.0f;
			if (definition.Value.Kind == ItemsPanelKind::VirtualizingStack
				&& (definition.Value.Orientation != Orientation::Vertical
					|| definition.Value.ItemHeight <= 0.0f))
				return Fail(L"VirtualizingStackPanel 只支持 Vertical，且必须声明正数 ItemHeight。", error);
			definition.SourceDictionary = _currentDictionaryOrigin;
			auto& definitions = _objectResourceTarget
				? _objectResourceTarget->ItemsPanelTemplates
				: _document.ItemsPanelTemplates;
			definitions.erase(std::remove_if(
				definitions.begin(), definitions.end(),
				[&](const auto& existing)
				{ return Equals(existing.Key, definition.Key); }),
				definitions.end());
			definitions.push_back(std::move(definition));
			return true;
		}

		bool ParseGroupStyle(const Element& element, std::wstring& error)
		{
			DiagnosticContext context(*this, element);
			if (!ValidateAttributes(element,
				{ L"Key", L"HeaderTemplate", L"HeaderIndent", L"HeaderSpacing",
					L"HeaderHeight" },
				error)) return false;
			DesignGroupStyle definition;
			definition.Key = Trim(Attribute(element, L"Key", L"x").value_or(
				Attribute(element, L"Key").value_or(L"")));
			if (!ValidateIdentifier(definition.Key, L"GroupStyle x:Key", error))
				return false;
			if (const auto header = Attribute(element, L"HeaderTemplate"))
				if (!TryParseStaticResource(*header, definition.HeaderTemplate))
					return Fail(L"GroupStyle.HeaderTemplate 必须引用 DataTemplate。", error);
				else if (const auto* dataTemplate = FindVisibleDataTemplate(
					definition.HeaderTemplate))
					definition.HeaderTemplate = dataTemplate->Key;
				else return Fail(L"GroupStyle.HeaderTemplate 引用了未声明的 DataTemplate："
					+ definition.HeaderTemplate, error);
			auto parseNonNegative = [&](const wchar_t* name, float& output)
			{
				const auto text = Attribute(element, name);
				if (!text) return true;
				double parsed = 0.0;
				if (!TryParseDouble(*text, parsed) || parsed < 0.0
					|| parsed > static_cast<double>((std::numeric_limits<float>::max)()))
					return Fail(L"GroupStyle " + std::wstring(name)
						+ L" 必须为有限非负数。", error);
				output = static_cast<float>(parsed);
				return true;
			};
			if (!parseNonNegative(L"HeaderIndent", definition.HeaderIndent)
				|| !parseNonNegative(L"HeaderSpacing", definition.HeaderSpacing)
				|| !parseNonNegative(L"HeaderHeight", definition.HeaderHeight))
				return false;
			if (definition.HeaderHeight <= 0.0f)
				return Fail(L"GroupStyle HeaderHeight 必须为有限正数。", error);
			if (!ChildElements(element).empty())
				return Fail(L"GroupStyle 当前只支持属性声明。", error);
			definition.SourceDictionary = _currentDictionaryOrigin;
			auto& definitions = _objectResourceTarget
				? _objectResourceTarget->GroupStyles : _document.GroupStyles;
			definitions.erase(std::remove_if(
				definitions.begin(), definitions.end(),
				[&](const auto& existing)
				{ return Equals(existing.Key, definition.Key); }),
				definitions.end());
			definitions.push_back(std::move(definition));
			return true;
		}

		bool ParseComponentDefinition(
			const Element& item,
			std::wstring& error)
		{
			DiagnosticContext context(*this, item);
			if (!ValidateAttributes(item,
				{ L"BaseType", L"DisplayName", L"Category" }, error, true))
				return false;

			const auto qualifiedName = Trim(Attribute(
				item, L"Key", L"x").value_or(L""));
			const auto separator = qualifiedName.find(L':');
			if (separator == std::wstring::npos || separator == 0
				|| separator + 1 >= qualifiedName.size()
				|| qualifiedName.find(L':', separator + 1) != std::wstring::npos)
				return Fail(L"ComponentDefinition 的 x:Key 必须是 prefix:TypeName。", error);

			DesignComponentDefinition definition;
			definition.Type.XamlPrefix = qualifiedName.substr(0, separator);
			definition.Type.XamlName = qualifiedName.substr(separator + 1);
			definition.Type.XamlNamespace = LookupNamespaceUri(
				item, definition.Type.XamlPrefix);
			if (Equals(definition.Type.XamlPrefix, L"x")
				|| Equals(definition.Type.XamlPrefix, L"d")
				|| definition.Type.XamlNamespace.empty()
				|| Equals(definition.Type.XamlNamespace, L"urn:cui"))
				return Fail(L"ComponentDefinition 必须使用独立的应用命名空间。", error);
			if (!ValidateIdentifier(
				definition.Type.XamlName, L"组件类型名称", error)) return false;

			const auto baseType = Trim(Attribute(
				item, L"BaseType").value_or(L"Panel"));
			if (!TryParseType(baseType, definition.BaseType)
				|| definition.BaseType == UIClass::UI_TabPage
				|| definition.BaseType == UIClass::UI_SelectorItem
				|| definition.BaseType == UIClass::UI_ComboBoxItem
				|| definition.BaseType == UIClass::UI_TreeViewItem)
				return Fail(L"组件 " + definition.Type.XamlName
					+ L" 的 BaseType 无效：" + baseType, error);
			definition.DisplayName = Trim(Attribute(
				item, L"DisplayName").value_or(definition.Type.XamlName));
			definition.Category = Trim(Attribute(
				item, L"Category").value_or(L"Components"));
			definition.SourceDictionary = _currentDictionaryOrigin;

			Element propertiesElement;
			Element contentPropertiesElement;
			Element eventsElement;
			Element templateElement;
			for (const auto& child : ChildElements(item))
			{
				const auto childName = FromUtf8(child->LocalName());
				if (Equals(childName, L"ComponentDefinition.Properties"))
				{
					if (propertiesElement)
						return Fail(L"ComponentDefinition.Properties 重复。", error);
					propertiesElement = child;
				}
				else if (Equals(childName, L"ComponentDefinition.ContentProperties"))
				{
					if (contentPropertiesElement)
						return Fail(L"ComponentDefinition.ContentProperties 重复。", error);
					contentPropertiesElement = child;
				}
				else if (Equals(childName, L"ComponentDefinition.Template"))
				{
					if (templateElement)
						return Fail(L"ComponentDefinition.Template 重复。", error);
					templateElement = child;
				}
				else if (Equals(childName, L"ComponentDefinition.Events"))
				{
					if (eventsElement)
						return Fail(L"ComponentDefinition.Events 重复。", error);
					eventsElement = child;
				}
				else return Fail(L"ComponentDefinition 仅支持 Properties、ContentProperties、Events 和 Template。", error);
			}

			if (eventsElement)
			{
				if (!ValidateAttributes(eventsElement, {}, error)) return false;
				for (const auto& eventElement : ChildElements(eventsElement))
				{
					DiagnosticContext eventContext(*this, eventElement);
					if (!Equals(FromUtf8(eventElement->LocalName()), L"ComponentEvent"))
						return Fail(L"ComponentDefinition.Events 仅允许 ComponentEvent。", error);
					if (!ChildElements(eventElement).empty()
						|| !Trim(FromUtf8(eventElement->InnerText())).empty())
						return Fail(L"ComponentEvent 不允许包含内容。", error);
					if (!ValidateAttributes(eventElement,
						{ L"Name", L"DisplayName", L"Category", L"Payload",
						  L"RoutingStrategy", L"Order", L"Default" }, error)) return false;

					DesignerComponentEventDescriptor event;
					event.Name = Trim(Attribute(eventElement, L"Name").value_or(L""));
					if (!ValidateIdentifier(event.Name, L"组件事件名称", error)) return false;
					event.DisplayName = Trim(Attribute(
						eventElement, L"DisplayName").value_or(event.Name));
					if (!DesignerEventCatalog::TryParseCategory(
						Attribute(eventElement, L"Category").value_or(L"Other"),
						event.Category))
						return Fail(L"组件事件 Category 无效：" + event.Name, error);
					if (!DesignerEventCatalog::TryParseComponentPayload(
						Attribute(eventElement, L"Payload").value_or(L"None"),
						event.Payload))
						return Fail(L"组件事件 Payload 无效：" + event.Name, error);
					if (!DesignerEventCatalog::TryParseComponentRoutingStrategy(
						Attribute(eventElement, L"RoutingStrategy").value_or(L"Direct"),
						event.RoutingStrategy))
						return Fail(L"组件事件 RoutingStrategy 无效："
							+ event.Name, error);
					if (const auto value = Attribute(eventElement, L"Order"))
						if (!TryParseInteger(*value, event.Order))
							return Fail(L"组件事件 Order 必须是整数。", error);
					if (const auto value = Attribute(eventElement, L"Default"))
						if (!TryParseBool(*value, event.IsDefault))
							return Fail(L"组件事件 Default 必须是布尔值。", error);
					definition.Events.push_back(std::move(event));
				}
				std::wstring validationError;
				if (!DesignerEventCatalog::ValidateComponentEvents(
					definition.BaseType, definition.Events, &validationError))
					return Fail(validationError, error);
			}

			auto baseProbe = DesignDocumentMaterializer::CreateRuntimeControl(
				definition.BaseType);
			if (!baseProbe)
				return Fail(L"组件 BaseType 尚无运行时工厂：" + baseType, error);
			if (contentPropertiesElement)
			{
				if (!ValidateAttributes(contentPropertiesElement, {}, error)) return false;
				for (const auto& contentElement : ChildElements(contentPropertiesElement))
				{
					DiagnosticContext contentContext(*this, contentElement);
					if (!Equals(FromUtf8(contentElement->LocalName()),
						L"ComponentContentProperty")
						|| !ChildElements(contentElement).empty()
						|| !Trim(FromUtf8(contentElement->InnerText())).empty())
						return Fail(L"ComponentDefinition.ContentProperties 仅允许空 ComponentContentProperty。", error);
					if (!ValidateAttributes(contentElement,
						{ L"Name", L"DisplayName", L"Cardinality", L"Default" }, error))
						return false;
					DesignerComponentContentPropertyDescriptor content;
					content.Name = Trim(Attribute(contentElement, L"Name").value_or(L""));
					if (!ValidateIdentifier(content.Name, L"组件内容属性名称", error)) return false;
					if (baseProbe->FindPropertyMetadata(content.Name))
						return Fail(L"组件内容属性不能覆盖 BaseType 属性：" + content.Name, error);
					if (std::any_of(definition.Events.begin(), definition.Events.end(),
						[&](const auto& event) { return Equals(event.Name, content.Name); }))
						return Fail(L"组件内容属性与事件名称冲突：" + content.Name, error);
					if (std::any_of(definition.ContentProperties.begin(),
						definition.ContentProperties.end(), [&](const auto& existing)
						{
							return Equals(existing.Name, content.Name);
						}))
						return Fail(L"组件内容属性名称重复：" + content.Name, error);
					content.DisplayName = Trim(Attribute(
						contentElement, L"DisplayName").value_or(content.Name));
					const auto cardinality = Trim(Attribute(
						contentElement, L"Cardinality").value_or(L"Single"));
					if (Equals(cardinality, L"Single"))
						content.Cardinality = DesignerComponentContentCardinality::Single;
					else if (Equals(cardinality, L"Multiple"))
						content.Cardinality = DesignerComponentContentCardinality::Multiple;
					else return Fail(L"组件内容属性 Cardinality 只能是 Single 或 Multiple。", error);
					if (const auto value = Attribute(contentElement, L"Default"))
						if (!TryParseBool(*value, content.IsDefault))
							return Fail(L"组件内容属性 Default 必须是布尔值。", error);
					if (content.IsDefault && std::any_of(
						definition.ContentProperties.begin(), definition.ContentProperties.end(),
						[](const auto& existing) { return existing.IsDefault; }))
						return Fail(L"组件只能声明一个默认内容属性。", error);
					definition.ContentProperties.push_back(std::move(content));
				}
			}
			if (propertiesElement)
			{
				if (!ValidateAttributes(propertiesElement, {}, error)) return false;
				for (const auto& propertyElement : ChildElements(propertiesElement))
				{
					DiagnosticContext propertyContext(*this, propertyElement);
					if (!Equals(FromUtf8(propertyElement->LocalName()), L"ComponentProperty")
						&& !Equals(FromUtf8(propertyElement->LocalName()), L"Property"))
						return Fail(L"ComponentDefinition.Properties 仅允许 ComponentProperty。", error);
					if (!Trim(FromUtf8(propertyElement->InnerText())).empty())
						return Fail(L"ComponentProperty 不允许包含文本内容。", error);
					if (!ValidateAttributes(propertyElement,
						{ L"Name", L"Type", L"Default", L"DisplayName", L"Category",
						  L"CategoryOrder", L"Order", L"Editor", L"Minimum", L"Maximum",
						  L"Step", L"AffectsMeasure", L"AffectsArrange", L"AffectsRender",
						  L"AffectsParentMeasure", L"AffectsParentArrange", L"Inherits",
						  L"BindsTwoWayByDefault", L"DefaultUpdateSourceTrigger", L"ReadOnly" },
						error)) return false;

					DesignerComponentPropertyDescriptor property;
					property.Name = Trim(Attribute(
						propertyElement, L"Name").value_or(L""));
					if (!ValidateIdentifier(
						property.Name, L"组件属性名称", error)) return false;
					if (baseProbe->FindPropertyMetadata(property.Name))
						return Fail(L"组件属性不能覆盖 BaseType 属性：" + property.Name, error);
					if (std::any_of(definition.ContentProperties.begin(),
						definition.ContentProperties.end(), [&](const auto& content)
						{
							return Equals(content.Name, property.Name);
						}))
						return Fail(L"组件标量属性与内容属性名称冲突：" + property.Name, error);
					if (std::any_of(definition.Properties.begin(), definition.Properties.end(),
						[&](const auto& existing) { return Equals(existing.Name, property.Name); }))
						return Fail(L"组件属性名称重复：" + property.Name, error);

					const auto typeName = Trim(Attribute(
						propertyElement, L"Type").value_or(L"String"));
					const bool isEnum = Equals(typeName, L"Enum");
					if (isEnum)
						property.DefaultValue.Kind = DesignerStyleValueKind::String;
					else if (!DesignerStyleSheetUtils::TryParseValueKind(
						typeName, property.DefaultValue.Kind)
						|| !IsSupportedComponentPropertyKind(property.DefaultValue.Kind))
						return Fail(L"组件属性暂不支持该类型：" + typeName, error);
					const auto defaultAttribute = Attribute(propertyElement, L"Default");
					if (defaultAttribute)
						(void)TryParseStaticResource(
							*defaultAttribute, property.DefaultResourceKey);

					Element choicesElement;
					Element defaultElement;
					for (const auto& child : ChildElements(propertyElement))
					{
						const auto childName = FromUtf8(child->LocalName());
						if (Equals(childName, L"ComponentProperty.Choices") && !choicesElement)
							choicesElement = child;
						else if (Equals(childName, L"ComponentProperty.Default") && !defaultElement)
							defaultElement = child;
						else
							return Fail(L"ComponentProperty 仅允许各一个 Choices 和 Default 子节点。", error);
					}
					if (isEnum && !choicesElement)
						return Fail(L"Enum 组件属性必须声明 ComponentProperty.Choices。", error);
					if (!isEnum && choicesElement)
						return Fail(L"ComponentProperty.Choices 仅适用于 Enum 类型。", error);
					const bool isObject = property.DefaultValue.Kind == DesignerStyleValueKind::Brush
						|| property.DefaultValue.Kind == DesignerStyleValueKind::Geometry
						|| property.DefaultValue.Kind == DesignerStyleValueKind::Transform;
					if (isObject && !defaultElement && property.DefaultResourceKey.empty())
						return Fail(L"对象型组件属性必须声明 ComponentProperty.Default。", error);
					if (!isObject && defaultElement)
						return Fail(L"ComponentProperty.Default 仅适用于对象型组件属性。", error);
					if (defaultElement && !property.DefaultResourceKey.empty())
						return Fail(L"组件属性不能同时使用资源默认值和 Default 子节点。", error);
					if (isObject && defaultAttribute && property.DefaultResourceKey.empty())
						return Fail(L"对象型组件属性不能同时使用 Default 属性和 Default 子节点。", error);
					if (choicesElement)
					{
						if (!ValidateAttributes(choicesElement, {}, error)
							|| !Trim(FromUtf8(choicesElement->InnerText())).empty())
							return Fail(L"ComponentProperty.Choices 不允许属性或文本内容。", error);
						for (const auto& choiceElement : ChildElements(choicesElement))
						{
							DiagnosticContext choiceContext(*this, choiceElement);
							if (!Equals(FromUtf8(choiceElement->LocalName()), L"ComponentChoice")
								|| !ChildElements(choiceElement).empty()
								|| !Trim(FromUtf8(choiceElement->InnerText())).empty())
								return Fail(L"ComponentProperty.Choices 仅允许空 ComponentChoice。", error);
							if (!ValidateAttributes(choiceElement,
								{ L"Value", L"DisplayName" }, error)) return false;
							DesignerComponentPropertyChoice choice;
							choice.Value = Trim(Attribute(choiceElement, L"Value").value_or(L""));
							if (!ValidateIdentifier(choice.Value, L"组件枚举值", error)) return false;
							if (std::any_of(property.Choices.begin(), property.Choices.end(),
								[&](const auto& existing) { return Equals(existing.Value, choice.Value); }))
								return Fail(L"组件枚举值重复：" + choice.Value, error);
							choice.DisplayName = Trim(Attribute(
								choiceElement, L"DisplayName").value_or(choice.Value));
							property.Choices.push_back(std::move(choice));
						}
						if (property.Choices.empty())
							return Fail(L"Enum 组件属性至少需要一个 ComponentChoice。", error);
					}
					if (!property.DefaultResourceKey.empty())
					{
						const auto* resource = FindVisibleResource(
							property.DefaultResourceKey);
						if (!resource)
							return Fail(L"组件属性引用了不存在的默认资源："
								+ property.DefaultResourceKey, error);
						if (resource->Value.Kind != property.DefaultValue.Kind)
							return Fail(L"组件属性默认资源类型与声明 Type 不一致："
								+ property.Name, error);
						if (_objectResourceTarget)
							property.DefaultValue = resource->Value;
					}
					else if (isObject)
					{
						if (property.DefaultValue.Kind == DesignerStyleValueKind::Brush)
						{
							if (!ParseBrush(defaultElement,
								property.DefaultValue.ObjectValue, error)) return false;
						}
						else if (property.DefaultValue.Kind == DesignerStyleValueKind::Geometry)
						{
							if (!ParseClip(defaultElement,
								property.DefaultValue.ObjectValue, error)) return false;
						}
						else if (!ParseTransform(defaultElement,
							property.DefaultValue.ObjectValue, error)) return false;
					}
					else property.DefaultValue.Text = defaultAttribute.value_or(
							isEnum ? property.Choices.front().Value
								: DefaultComponentValue(property.DefaultValue.Kind));
					if (isEnum && property.DefaultResourceKey.empty())
					{
						const auto found = std::find_if(property.Choices.begin(), property.Choices.end(),
							[&](const auto& choice) { return Equals(choice.Value, property.DefaultValue.Text); });
						if (found == property.Choices.end())
							return Fail(L"组件枚举默认值不在候选集合中：" + property.DefaultValue.Text, error);
						property.DefaultValue.Text = found->Value;
					}
					const DesignerStyleValue* effectiveDefault = &property.DefaultValue;
					if (!property.DefaultResourceKey.empty())
					{
						const auto* resource = FindVisibleResource(
							property.DefaultResourceKey);
						if (!resource)
							return Fail(L"组件属性引用了不存在的默认资源："
								+ property.DefaultResourceKey, error);
						effectiveDefault = &resource->Value;
					}
					BindingValue converted;
					std::wstring conversionError;
					if (!DesignerStyleSheetUtils::TryConvertValue(
						*effectiveDefault, converted, &conversionError,
						_currentResourceBasePath, _document.Resources))
						return Fail(L"组件属性 " + property.Name
							+ L" 的默认值无效：" + conversionError, error);

					property.DisplayName = Trim(Attribute(
						propertyElement, L"DisplayName").value_or(property.Name));
					property.Category = Trim(Attribute(
						propertyElement, L"Category").value_or(L"Component"));
					if (const auto value = Attribute(propertyElement, L"CategoryOrder"))
						if (!TryParseInteger(*value, property.CategoryOrder))
							return Fail(L"组件属性 CategoryOrder 必须是整数。", error);
					if (const auto value = Attribute(propertyElement, L"Order"))
						if (!TryParseInteger(*value, property.Order))
							return Fail(L"组件属性 Order 必须是整数。", error);
					if (const auto value = Attribute(propertyElement, L"Editor"))
						if (!ParseComponentPropertyEditor(*value, property.Editor, error)) return false;
					if (isEnum)
					{
						if (property.Editor != ControlPropertyEditorKind::Auto
							&& property.Editor != ControlPropertyEditorKind::Choice)
							return Fail(L"Enum 组件属性只能使用 Auto 或 Choice 编辑器。", error);
						property.Editor = ControlPropertyEditorKind::Choice;
					}
					for (const auto& [name, target] : {
						std::pair{ L"Minimum", &property.Minimum },
						std::pair{ L"Maximum", &property.Maximum },
						std::pair{ L"Step", &property.Step } })
					{
						if (const auto value = Attribute(propertyElement, name))
						{
							if (!IsNumericComponentPropertyKind(property.DefaultValue.Kind))
								return Fail(L"组件属性 " + property.Name + L" 的 "
									+ name + L" 仅适用于数值类型。", error);
							double parsed = 0.0;
							if (!TryParseDouble(*value, parsed))
								return Fail(std::wstring(L"组件属性 ")
									+ name + L" 必须是有限数字。", error);
							*target = parsed;
						}
					}
					for (const auto& [name, flag] : {
						std::pair{ L"AffectsMeasure", ControlPropertyFlags::AffectsMeasure },
						std::pair{ L"AffectsArrange", ControlPropertyFlags::AffectsArrange },
						std::pair{ L"AffectsRender", ControlPropertyFlags::AffectsRender },
						std::pair{ L"AffectsParentMeasure", ControlPropertyFlags::AffectsParentMeasure },
						std::pair{ L"AffectsParentArrange", ControlPropertyFlags::AffectsParentArrange },
						std::pair{ L"Inherits", ControlPropertyFlags::Inherits },
						std::pair{ L"BindsTwoWayByDefault", ControlPropertyFlags::BindsTwoWayByDefault } })
					{
						if (const auto value = Attribute(propertyElement, name))
						{
							bool enabled = false;
							if (!TryParseBool(*value, enabled))
								return Fail(std::wstring(L"组件属性 ")
									+ name + L" 必须是布尔值。", error);
							if (enabled) property.Flags |= flag;
						}
					}
					if (const auto value = Attribute(propertyElement, L"ReadOnly"))
					{
						if (!TryParseBool(*value, property.IsReadOnly))
							return Fail(L"组件属性 ReadOnly 必须是布尔值。", error);
					}
					if (const auto value = Attribute(
						propertyElement, L"DefaultUpdateSourceTrigger"))
					{
						if (!DesignerBindingUtils::TryParseUpdateMode(
							*value, property.DefaultUpdateMode)
							|| property.DefaultUpdateMode
								== DataSourceUpdateMode::Default)
							return Fail(L"组件属性 DefaultUpdateSourceTrigger 必须为 PropertyChanged、LostFocus 或 Explicit。", error);
					}
					if (property.IsReadOnly && HasControlPropertyFlag(
						property.Flags, ControlPropertyFlags::BindsTwoWayByDefault))
						return Fail(L"只读组件属性不能声明 BindsTwoWayByDefault。", error);
					if (property.IsReadOnly && property.DefaultUpdateMode
						!= DataSourceUpdateMode::OnPropertyChanged)
						return Fail(L"只读组件属性不能声明 DefaultUpdateSourceTrigger。", error);
					definition.Properties.push_back(std::move(property));
				}
			}

			if (templateElement)
			{
				if (!ValidateAttributes(templateElement, {}, error)) return false;
				const auto roots = ChildElements(templateElement);
				if (roots.size() != 1)
					return Fail(L"ComponentDefinition.Template 必须且只能包含一个视觉根。", error);

				auto pageNodes = std::move(_document.Nodes);
				const int pageNextStableId = _document.NextStableId;
				auto pageUsedIds = std::move(_usedIds);
				auto pageUsedNames = std::move(_usedNames);
				auto pageNameCounters = std::move(_nameCounters);
				_document.Nodes.clear();
				_document.NextStableId = 1;
				_usedIds.clear();
				_usedNames.clear();
				_nameCounters.clear();
				auto* previousTemplate = _activeTemplateComponent;
				const bool previousControlTemplateVisual =
					_parsingControlTemplateVisual;
				const bool previousComponentTemplateVisual =
					_parsingComponentTemplateVisual;
				const auto previousVisualStateGroups = _pendingVisualStateGroups;
				const auto previousEventTriggers = _pendingEventTriggers;
				_pendingVisualStateGroups.reset();
				_pendingEventTriggers.reset();
				_activeTemplateComponent = &definition;
				_parsingControlTemplateVisual = false;
				_parsingComponentTemplateVisual = true;
				bool parsed = ParseControl(roots.front(), Parent{}, error);
				if (parsed && (_pendingVisualStateGroups || _pendingEventTriggers))
					definition.Template = _document.Nodes;
				if (parsed && _pendingVisualStateGroups)
				{
					parsed = ParseVisualStateGroups(
						_pendingVisualStateGroups, definition, error);
				}
				if (parsed && _pendingEventTriggers)
					parsed = ParseEventTriggers(
						_pendingEventTriggers, definition, error);
				_activeTemplateComponent = previousTemplate;
				_parsingControlTemplateVisual = previousControlTemplateVisual;
				_parsingComponentTemplateVisual = previousComponentTemplateVisual;
				_pendingVisualStateGroups = previousVisualStateGroups;
				_pendingEventTriggers = previousEventTriggers;
				if (parsed) definition.Template = std::move(_document.Nodes);
				_document.Nodes = std::move(pageNodes);
				_document.NextStableId = pageNextStableId;
				_usedIds = std::move(pageUsedIds);
				_usedNames = std::move(pageUsedNames);
				_nameCounters = std::move(pageNameCounters);
				if (!parsed) return false;
			}
			if (!definition.ContentProperties.empty() && definition.Template.empty())
				return Fail(L"声明内容属性的组件必须提供 Template。", error);
			for (const auto& content : definition.ContentProperties)
			{
				const auto presenterCount = std::count_if(
					definition.Template.begin(), definition.Template.end(),
					[&](const auto& node)
					{
						return Equals(node.PresentedComponentContent, content.Name);
					});
				if (presenterCount != 1)
					return Fail(L"组件内容属性必须在模板中恰好拥有一个 Presenter："
						+ content.Name, error);
			}

			auto& components = _objectResourceTarget
				? _objectResourceTarget->Components : _document.Components;
			components.erase(std::remove_if(
				components.begin(), components.end(),
				[&](const DesignComponentDefinition& existing)
				{
					return Equals(existing.Type.XamlNamespace,
						definition.Type.XamlNamespace)
						&& Equals(existing.Type.XamlName,
							definition.Type.XamlName);
				}), components.end());
			components.push_back(std::move(definition));
			return true;
		}

		bool DefineComponentProbeProperties(
			Control& probe,
			const DesignComponentDefinition& component,
			std::wstring& error)
		{
			for (const auto& property : component.Properties)
			{
				const DesignerStyleValue* source = &property.DefaultValue;
				if (!property.DefaultResourceKey.empty())
				{
					const auto* resource = FindVisibleResource(
						property.DefaultResourceKey);
					if (!resource)
						return Fail(L"组件属性引用了不存在的默认资源："
							+ property.DefaultResourceKey, error);
					source = &resource->Value;
				}
				BindingValue defaultValue;
				std::wstring conversionError;
				if (!DesignerStyleSheetUtils::TryConvertValue(
					*source, defaultValue, &conversionError,
					_currentResourceBasePath, _document.Resources))
					return Fail(L"组件属性 " + property.Name
						+ L" 的默认值无效：" + conversionError, error);
				DynamicControlPropertyDefinition definition;
				definition.Name = property.Name;
				switch (property.DefaultValue.Kind)
				{
				case DesignerStyleValueKind::Bool:
					definition.ValueKind = BindingValueKind::Bool; break;
				case DesignerStyleValueKind::Int:
					definition.ValueKind = BindingValueKind::Int; break;
				case DesignerStyleValueKind::Int64:
					definition.ValueKind = BindingValueKind::Int64; break;
				case DesignerStyleValueKind::Float:
					definition.ValueKind = BindingValueKind::Float; break;
				case DesignerStyleValueKind::Double:
					definition.ValueKind = BindingValueKind::Double; break;
				case DesignerStyleValueKind::String:
					definition.ValueKind = BindingValueKind::String; break;
				case DesignerStyleValueKind::Color:
				case DesignerStyleValueKind::Thickness:
				case DesignerStyleValueKind::Point:
				case DesignerStyleValueKind::Vector:
				case DesignerStyleValueKind::Rect:
				case DesignerStyleValueKind::Size:
				case DesignerStyleValueKind::Matrix:
				case DesignerStyleValueKind::Length:
				case DesignerStyleValueKind::Brush:
				case DesignerStyleValueKind::Geometry:
				case DesignerStyleValueKind::Transform:
					definition.ValueKind = BindingValueKind::Object; break;
				default:
					return Fail(L"组件属性类型尚未进入动态属性契约：" + property.Name, error);
				}
				definition.DefaultValue = std::move(defaultValue);
				definition.Flags = property.Flags;
				definition.DefaultUpdateMode = property.DefaultUpdateMode;
				definition.IsReadOnly = property.IsReadOnly;
				if (HasControlPropertyFlag(
					property.Flags, ControlPropertyFlags::Inherits))
					definition.InheritanceKey = component.Type.RegistryKey()
						+ L"|" + property.Name;
				definition.Design.DisplayName = property.DisplayName;
				definition.Design.Category = property.Category;
				definition.Design.CategoryOrder = property.CategoryOrder;
				definition.Design.Order = property.Order;
				definition.Design.Editor = property.Editor;
				for (const auto& choice : property.Choices)
				{
					BindingValue value(std::wstring(choice.Value));
					definition.AllowedValues.push_back(value);
					definition.Design.Choices.push_back({
						choice.DisplayName.empty() ? choice.Value : choice.DisplayName,
						std::move(value)
					});
				}
				definition.Design.Minimum = property.Minimum;
				definition.Design.Maximum = property.Maximum;
				definition.Design.Step = property.Step;
				definition.Design.Persistence = ControlPropertyPersistence::Metadata;
				std::wstring definitionError;
				if (!probe.DefineDynamicProperty(
					std::move(definition), &definitionError))
					return Fail(L"组件属性 " + property.Name
						+ L" 无法安装：" + definitionError, error);
			}
			return true;
		}

		bool ParseStyleSetter(
			const Element& element,
			Control& probe,
			DesignerStyleSetter& setter,
			std::wstring& error,
			bool allowTargetName = false,
			bool allowControlTemplate = false)
		{
			DiagnosticContext context(*this, element);
			if (allowTargetName)
			{
				if (!ValidateAttributes(element,
					{ L"TargetName", L"Property", L"Value", L"Resource", L"Kind" },
					error)) return false;
			}
			else if (!ValidateAttributes(element,
				{ L"Property", L"Value", L"Resource", L"Kind" }, error)) return false;
			const auto rawProperty = Trim(Attribute(element, L"Property").value_or(L""));
			auto rawValue = Attribute(element, L"Value").value_or(
				FromUtf8(element->InnerText()));
			if (rawProperty.empty()) return Fail(L"Setter 缺少 Property。", error);
			const auto propertyName = NormalizePropertyName(rawProperty, rawValue);
			if (Equals(propertyName, L"Template"))
			{
				if (!allowControlTemplate)
					return Fail(L"Template 目前只允许作为 Style 的普通 Setter；"
						L"Trigger/VisualState 动态换模板尚未开放。", error);
				if (!ChildElements(element).empty())
					return Fail(L"Template Setter 必须通过 StaticResource 引用 ControlTemplate。",
						error);
				setter.PropertyName = L"Template";
				setter.ResourceKey = Trim(
					Attribute(element, L"Resource").value_or(L""));
				if (setter.ResourceKey.empty()
					&& !TryParseStaticResource(rawValue, setter.ResourceKey))
					return Fail(L"Template Setter 必须通过 {StaticResource key} "
						L"引用 ControlTemplate。", error);
				setter.UsesResource = true;
				setter.UsesDynamicResource = false;
				return true;
			}
			const auto properties = DesignerPropertyCatalog::GetStyleProperties(probe);
			const auto* descriptor = DesignerPropertyCatalog::Find(
				properties, propertyName);
			if (!descriptor)
			{
				if (const auto* metadata = probe.FindPropertyMetadata(propertyName);
					metadata && metadata->IsReadOnly())
					return Fail(L"Style Setter 不能写入只读属性：" + rawProperty, error);
				return Fail(L"Style 目标类型不包含属性：" + rawProperty, error);
			}

			setter.PropertyName = descriptor->Name;
			setter.ResourceKey = Trim(Attribute(element, L"Resource").value_or(L""));
			if (setter.ResourceKey.empty())
			{
				setter.UsesDynamicResource =
					TryParseDynamicResource(rawValue, setter.ResourceKey);
				if (!setter.UsesDynamicResource)
					(void)TryParseStaticResource(rawValue, setter.ResourceKey);
			}
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

		std::unique_ptr<Control> CreateVisualStateTargetProbe(
			const DesignComponentDefinition& component,
			const std::wstring& targetName)
		{
			if (targetName.empty())
			{
				auto probe = DesignDocumentMaterializer::CreateRuntimeControl(
					component.BaseType);
				std::wstring ignored;
				if (!probe || !DefineComponentProbeProperties(
					*probe, component, ignored)) return nullptr;
				return probe;
			}
			const auto node = std::find_if(
				component.Template.begin(), component.Template.end(),
				[&](const auto& candidate)
				{ return Equals(candidate.Name, targetName); });
			if (node == component.Template.end()) return nullptr;
			auto probe = DesignDocumentMaterializer::CreateRuntimeControl(node->Type);
			if (!probe) return nullptr;
			if (!node->ComponentType.Empty())
			{
				const auto* nested = FindVisibleComponent(node->ComponentType);
				std::wstring ignored;
				if (!nested || !DefineComponentProbeProperties(
					*probe, *nested, ignored)) return nullptr;
			}
			return probe;
		}

		bool ParseTimelineBehavior(
			const Element& element,
			DesignerVisualStateAnimation& animation,
			std::wstring& error)
		{
			if (const auto repeat = Attribute(element, L"RepeatBehavior"))
			{
				auto value = Trim(*repeat);
				if (Equals(value, L"Forever"))
					animation.RepeatBehavior = DesignerRepeatBehaviorKind::Forever;
				else if (value.find(L':') != std::wstring::npos)
				{
					animation.RepeatBehavior = DesignerRepeatBehaviorKind::Duration;
					if (!TryParseTimeSpanMilliseconds(
						value, animation.RepeatDurationMilliseconds)
						|| animation.RepeatDurationMilliseconds == 0)
						return Fail(L"RepeatBehavior Duration 必须是有限正 TimeSpan。",
							error);
				}
				else
				{
					if (!value.empty()
						&& (value.back() == L'x' || value.back() == L'X'))
						value = Trim(value.substr(0, value.size() - 1));
					double count = 0.0;
					if (!TryParseDouble(value, count)
						|| !std::isfinite(count) || count <= 0.0)
						return Fail(L"RepeatBehavior Count 必须是有限正数或 Forever。",
							error);
					animation.RepeatBehavior = DesignerRepeatBehaviorKind::Count;
					animation.RepeatCount = count;
				}
			}
			if (const auto reverse = Attribute(element, L"AutoReverse");
				reverse && !TryParseBool(*reverse, animation.AutoReverse))
				return Fail(L"AutoReverse 必须是布尔值。", error);
			if (const auto additive = Attribute(element, L"IsAdditive");
				additive && !TryParseBool(*additive, animation.IsAdditive))
				return Fail(L"IsAdditive 必须是布尔值。", error);
			if (const auto cumulative = Attribute(element, L"IsCumulative");
				cumulative && !TryParseBool(*cumulative, animation.IsCumulative))
				return Fail(L"IsCumulative 必须是布尔值。", error);
			if (const auto fill = Attribute(element, L"FillBehavior"))
			{
				const auto value = Trim(*fill);
				if (Equals(value, L"HoldEnd"))
					animation.FillBehavior =
						DesignerTimelineFillBehavior::HoldEnd;
				else if (Equals(value, L"Stop"))
					animation.FillBehavior = DesignerTimelineFillBehavior::Stop;
				else return Fail(L"FillBehavior 只能是 HoldEnd 或 Stop。", error);
			}
			if (const auto speed = Attribute(element, L"SpeedRatio"); speed
				&& (!TryParseDouble(*speed, animation.SpeedRatio)
					|| animation.SpeedRatio <= 0.0))
				return Fail(L"SpeedRatio 必须是有限正数。", error);
			auto parseUnitRatio = [&](const wchar_t* name, double& output)
			{
				const auto text = Attribute(element, name);
				if (!text) return true;
				if (!TryParseDouble(*text, output)
					|| output < 0.0 || output > 1.0)
					return Fail(std::wstring(name) + L" 必须位于 0..1。", error);
				return true;
			};
			if (!parseUnitRatio(L"AccelerationRatio", animation.AccelerationRatio)
				|| !parseUnitRatio(L"DecelerationRatio", animation.DecelerationRatio))
				return false;
			if (animation.AccelerationRatio + animation.DecelerationRatio > 1.0)
				return Fail(L"AccelerationRatio 与 DecelerationRatio 之和不能超过 1。",
					error);
			return true;
		}

		bool ParseStoryboardAnimation(
			const Element& animationElement,
			const DesignComponentDefinition& component,
			DesignerVisualStateAnimation& animation,
			StoryboardObjectPathKind& objectPathKind,
			std::wstring& error)
		{
			DiagnosticContext animationContext(*this, animationElement);
			const auto animationName = FromUtf8(animationElement->LocalName());
			bool keyFrameAnimation = false;
			if (Equals(animationName, L"DoubleAnimation")
				|| Equals(animationName, L"DoubleAnimationUsingKeyFrames"))
			{
				animation.Kind = DesignerAnimationKind::Double;
				keyFrameAnimation = Equals(
					animationName, L"DoubleAnimationUsingKeyFrames");
			}
			else if (Equals(animationName, L"ColorAnimation")
				|| Equals(animationName, L"ColorAnimationUsingKeyFrames"))
			{
				animation.Kind = DesignerAnimationKind::Color;
				keyFrameAnimation = Equals(
					animationName, L"ColorAnimationUsingKeyFrames");
			}
			else if (Equals(animationName, L"ThicknessAnimation")
				|| Equals(animationName, L"ThicknessAnimationUsingKeyFrames"))
			{
				animation.Kind = DesignerAnimationKind::Thickness;
				keyFrameAnimation = Equals(
					animationName, L"ThicknessAnimationUsingKeyFrames");
			}
			else if (Equals(animationName, L"PointAnimation")
				|| Equals(animationName, L"PointAnimationUsingKeyFrames"))
			{
				animation.Kind = DesignerAnimationKind::Point;
				keyFrameAnimation = Equals(
					animationName, L"PointAnimationUsingKeyFrames");
			}
			else if (Equals(animationName, L"VectorAnimation")
				|| Equals(animationName, L"VectorAnimationUsingKeyFrames"))
			{
				animation.Kind = DesignerAnimationKind::Vector;
				keyFrameAnimation = Equals(
					animationName, L"VectorAnimationUsingKeyFrames");
			}
			else if (Equals(animationName, L"RectAnimation")
				|| Equals(animationName, L"RectAnimationUsingKeyFrames"))
			{
				animation.Kind = DesignerAnimationKind::Rect;
				keyFrameAnimation = Equals(
					animationName, L"RectAnimationUsingKeyFrames");
			}
			else if (Equals(animationName, L"SizeAnimation")
				|| Equals(animationName, L"SizeAnimationUsingKeyFrames"))
			{
				animation.Kind = DesignerAnimationKind::Size;
				keyFrameAnimation = Equals(
					animationName, L"SizeAnimationUsingKeyFrames");
			}
			else if (Equals(animationName, L"MatrixAnimation")
				|| Equals(animationName, L"MatrixAnimationUsingKeyFrames"))
			{
				animation.Kind = DesignerAnimationKind::Matrix;
				keyFrameAnimation = Equals(
					animationName, L"MatrixAnimationUsingKeyFrames");
			}
			else if (Equals(animationName, L"ObjectAnimationUsingKeyFrames"))
			{
				animation.Kind = DesignerAnimationKind::Object;
				keyFrameAnimation = true;
			}
			else return Fail(L"Storyboard 仅支持 Double/Color/Thickness/Point/Vector/Rect/Size/Matrix Animation 和 "
				L"ObjectAnimationUsingKeyFrames。",
				error);
			const bool objectAnimation =
				animation.Kind == DesignerAnimationKind::Object;
			const bool validAttributes = objectAnimation
				? ValidateAttributes(animationElement,
					{ L"Storyboard.TargetName", L"Storyboard.TargetProperty",
						L"Duration", L"BeginTime", L"RepeatBehavior",
						L"AutoReverse", L"FillBehavior", L"SpeedRatio",
						L"AccelerationRatio", L"DecelerationRatio" }, error)
				: keyFrameAnimation
				? ValidateAttributes(animationElement,
					{ L"Storyboard.TargetName", L"Storyboard.TargetProperty",
						L"Duration", L"BeginTime", L"RepeatBehavior",
						L"AutoReverse", L"IsAdditive", L"IsCumulative",
						L"FillBehavior", L"SpeedRatio",
						L"AccelerationRatio", L"DecelerationRatio" }, error)
				: ValidateAttributes(animationElement,
					{ L"Storyboard.TargetName", L"Storyboard.TargetProperty",
						L"From", L"To", L"By", L"Duration", L"BeginTime",
						L"RepeatBehavior", L"AutoReverse", L"IsAdditive",
						L"IsCumulative", L"FillBehavior",
						L"SpeedRatio", L"AccelerationRatio",
						L"DecelerationRatio" }, error);
			if (!validAttributes) return false;
			animation.TargetName = Trim(Attribute(
				animationElement, L"Storyboard.TargetName").value_or(L""));
			if (!animation.TargetName.empty()
				&& !ValidateIdentifier(animation.TargetName,
					L"Storyboard TargetName", error)) return false;
			const auto rawProperty = Trim(Attribute(animationElement,
				L"Storyboard.TargetProperty").value_or(L""));
			if (rawProperty.empty())
				return Fail(L"动画缺少 Storyboard.TargetProperty。", error);
			auto targetProbe = CreateVisualStateTargetProbe(
				component, animation.TargetName);
			if (!targetProbe)
				return Fail(L"Storyboard 找不到模板部件："
					+ animation.TargetName, error);
			const DesignerPropertyDescriptor* descriptor = nullptr;
			const BindingPropertyMetadata* targetMetadata = nullptr;
			std::vector<DesignerPropertyDescriptor> targetProperties;
			DesignerStyleValueKind endpointKind = DesignerStyleValueKind::Float;
			objectPathKind = StoryboardObjectPathKind::None;
			if (rawProperty.front() != L'(')
			{
				const auto propertyName = NormalizePropertyName(rawProperty, L"");
				targetProperties = DesignerPropertyCatalog::GetStyleProperties(
					*targetProbe);
				descriptor = DesignerPropertyCatalog::Find(
					targetProperties, propertyName);
				if (!descriptor)
					return Fail(L"Storyboard 目标属性不存在或不可写："
						+ rawProperty, error);
				const bool compatible = animation.Kind == DesignerAnimationKind::Object
					? true
					: animation.Kind == DesignerAnimationKind::Thickness
						? descriptor->ValueKind == DesignerStyleValueKind::Thickness
					: animation.Kind == DesignerAnimationKind::Point
						? descriptor->ValueKind == DesignerStyleValueKind::Point
					: animation.Kind == DesignerAnimationKind::Vector
						? descriptor->ValueKind == DesignerStyleValueKind::Vector
					: animation.Kind == DesignerAnimationKind::Rect
						? descriptor->ValueKind == DesignerStyleValueKind::Rect
					: animation.Kind == DesignerAnimationKind::Size
						? descriptor->ValueKind == DesignerStyleValueKind::Size
					: animation.Kind == DesignerAnimationKind::Matrix
						? descriptor->ValueKind == DesignerStyleValueKind::Matrix
					: animation.Kind == DesignerAnimationKind::Color
						? descriptor->ValueKind == DesignerStyleValueKind::Color
						: descriptor->ValueKind == DesignerStyleValueKind::Int
						|| descriptor->ValueKind == DesignerStyleValueKind::Int64
						|| descriptor->ValueKind == DesignerStyleValueKind::Float
						|| descriptor->ValueKind == DesignerStyleValueKind::Double;
				if (!compatible)
					return Fail(animationName + L" 与目标属性类型不兼容："
						+ descriptor->Name, error);
				animation.PropertyName = descriptor->Name;
				endpointKind = descriptor->ValueKind;
				targetMetadata = targetProbe->FindPropertyMetadata(descriptor->Name);
			}
			else
			{
				std::wstring pathError;
				ResolvedStoryboardObjectPath resolvedPath;
				if (!TryResolveStoryboardObjectPath(component,
					animation.TargetName, rawProperty, animation.Kind,
					resolvedPath, &pathError)) return Fail(pathError, error);
				animation.PropertyName = resolvedPath.CanonicalPath;
				objectPathKind = resolvedPath.Kind;
				endpointKind = StoryboardObjectPathValueKind(objectPathKind);
			}
			auto parseEndpoint = [&](const std::wstring& raw,
				DesignerStyleValue& literal, bool& usesResource,
				std::wstring& resourceKey, const std::wstring& label,
				bool isDelta = false)
			{
				usesResource = TryParseStaticResource(raw, resourceKey);
				const DesignerStyleValue* value = &literal;
				if (usesResource)
				{
					const auto* resource = FindVisibleResource(resourceKey);
					if (!resource)
						return Fail(L"动画 " + label + L" 引用了不存在的资源："
							+ resourceKey, error);
					if (_objectResourceTarget)
					{
						literal = resource->Value;
						value = &literal;
					}
					else value = &resource->Value;
				}
				else
				{
					literal.Kind = endpointKind;
					literal.Text = descriptor
						? NormalizePropertyText(rawProperty, raw, *descriptor) : raw;
				}
				BindingValue parsed;
				BindingValue converted;
				BindingValue coerced;
				std::wstring validationError;
				if (!DesignerStyleSheetUtils::TryConvertValue(
					*value, parsed, &validationError,
					_currentResourceBasePath, _document.Resources))
					return Fail(L"动画 " + label + L" 无效："
						+ validationError, error);
				if (objectPathKind != StoryboardObjectPathKind::None)
				{
					if (!ValidateStoryboardObjectPathValue(
						objectPathKind, parsed, isDelta))
						return Fail(L"动画 " + label
							+ L" 与对象路径末端类型或取值范围不兼容。", error);
				}
				else if (!targetMetadata || !targetMetadata->CanWrite()
					|| !targetMetadata->TryConvert(parsed, converted)
					|| (!isDelta && !targetMetadata->TryCoerce(
						*targetProbe, converted, coerced)))
					return Fail(L"动画 " + label
						+ L" 无法通过目标属性元数据转换或 Coerce。", error);
				return true;
			};
			const auto animationChildren = ChildElements(animationElement);
			if (!Trim(FromUtf8(animationElement->InnerText())).empty())
				return Fail(L"动画不允许文本内容。", error);
			auto parseEasing = [&](const Element& easingProperty,
				const std::wstring& expectedPropertyName,
				DesignerEasingKind& kind, DesignerEasingMode& mode)
			{
				if (!Equals(FromUtf8(easingProperty->LocalName()), expectedPropertyName))
					return Fail(L"EasingFunction 属性元素名称无效。", error);
				if (!ValidateAttributes(easingProperty, {}, error)
					|| !Trim(FromUtf8(easingProperty->InnerText())).empty()) return false;
				const auto children = ChildElements(easingProperty);
				if (children.size() != 1)
					return Fail(L"EasingFunction 必须包含一个缓动对象。", error);
				const auto& easing = children.front();
				const auto easingName = FromUtf8(easing->LocalName());
				if (Equals(easingName, L"QuadraticEase"))
					kind = DesignerEasingKind::Quadratic;
				else if (Equals(easingName, L"CubicEase"))
					kind = DesignerEasingKind::Cubic;
				else if (Equals(easingName, L"SineEase"))
					kind = DesignerEasingKind::Sine;
				else return Fail(L"EasingFunction 第一批仅支持 QuadraticEase、"
					L"CubicEase 和 SineEase。", error);
				if (!ValidateAttributes(easing, { L"EasingMode" }, error)
					|| !ChildElements(easing).empty()
					|| !Trim(FromUtf8(easing->InnerText())).empty()) return false;
				if (const auto modeText = Attribute(easing, L"EasingMode"))
				{
					int parsedMode = 0;
					if (!TryParseEnum(*modeText,
						{ L"EaseIn", L"EaseOut", L"EaseInOut" }, parsedMode))
						return Fail(L"EasingMode 无效。", error);
					mode = static_cast<DesignerEasingMode>(parsedMode);
				}
				return true;
			};
			if (!keyFrameAnimation)
			{
				if (const auto to = Attribute(animationElement, L"To"))
				{
					animation.HasTo = true;
					if (!parseEndpoint(*to, animation.To,
						animation.ToUsesResource, animation.ToResourceKey, L"To"))
						return false;
				}
				if (const auto from = Attribute(animationElement, L"From"))
				{
					animation.HasFrom = true;
					if (!parseEndpoint(*from, animation.From,
						animation.FromUsesResource,
						animation.FromResourceKey, L"From")) return false;
				}
				if (const auto by = Attribute(animationElement, L"By"))
				{
					animation.HasBy = true;
					if (!parseEndpoint(*by, animation.By,
						animation.ByUsesResource,
						animation.ByResourceKey, L"By", true)) return false;
				}
				const auto duration = Attribute(animationElement, L"Duration");
				if (!duration || !TryParseTimeSpanMilliseconds(
					*duration, animation.DurationMilliseconds))
					return Fail(L"动画 Duration 必须是有限 TimeSpan。", error);
				if (!animationChildren.empty())
				{
					if (animationChildren.size() != 1)
						return Fail(L"动画只允许一个 EasingFunction 属性元素。",
							error);
					if (!parseEasing(animationChildren.front(),
						animationName + L".EasingFunction",
						animation.Easing, animation.EasingMode)) return false;
				}
			}
			else
			{
				const auto valueType = animation.Kind == DesignerAnimationKind::Color
					? L"Color" : animation.Kind == DesignerAnimationKind::Object
						? L"Object" : animation.Kind == DesignerAnimationKind::Thickness
							? L"Thickness" : animation.Kind == DesignerAnimationKind::Point
								? L"Point" : animation.Kind == DesignerAnimationKind::Vector
								? L"Vector" : animation.Kind == DesignerAnimationKind::Rect
								? L"Rect" : animation.Kind == DesignerAnimationKind::Size
								? L"Size" : animation.Kind == DesignerAnimationKind::Matrix
								? L"Matrix" : L"Double";
				for (const auto& keyFrameElement : animationChildren)
				{
					DiagnosticContext keyFrameContext(*this, keyFrameElement);
					const auto keyFrameName = FromUtf8(keyFrameElement->LocalName());
					DesignerAnimationKeyFrame keyFrame;
					if (Equals(keyFrameName,
						L"Discrete" + std::wstring(valueType) + L"KeyFrame"))
						keyFrame.Kind = DesignerKeyFrameKind::Discrete;
					else if (!objectAnimation && Equals(keyFrameName,
						L"Linear" + std::wstring(valueType) + L"KeyFrame"))
						keyFrame.Kind = DesignerKeyFrameKind::Linear;
					else if (!objectAnimation && Equals(keyFrameName,
						L"Easing" + std::wstring(valueType) + L"KeyFrame"))
						keyFrame.Kind = DesignerKeyFrameKind::Easing;
					else if (!objectAnimation && Equals(keyFrameName,
						L"Spline" + std::wstring(valueType) + L"KeyFrame"))
						keyFrame.Kind = DesignerKeyFrameKind::Spline;
					else return Fail(L"UsingKeyFrames 包含不兼容的关键帧类型："
						+ keyFrameName, error);
					const bool spline = keyFrame.Kind == DesignerKeyFrameKind::Spline;
					if (!ValidateAttributes(keyFrameElement,
						spline
							? std::initializer_list<const wchar_t*>{
								L"KeyTime", L"Value", L"KeySpline" }
							: std::initializer_list<const wchar_t*>{
								L"KeyTime", L"Value" }, error)) return false;
					const auto keyTime = Attribute(keyFrameElement, L"KeyTime");
					if (!keyTime || !TryParseTimeSpanMilliseconds(
						*keyTime, keyFrame.KeyTimeMilliseconds))
						return Fail(L"关键帧 KeyTime 必须是显式有限 TimeSpan。", error);
					const auto children = ChildElements(keyFrameElement);
					if (!Trim(FromUtf8(keyFrameElement->InnerText())).empty())
						return Fail(L"关键帧不允许文本内容。", error);
					const auto value = Attribute(keyFrameElement, L"Value");
					if (value)
					{
						if (!children.empty()
							&& keyFrame.Kind != DesignerKeyFrameKind::Easing)
							return Fail(L"关键帧 Value 特性不能与 Value 属性元素混用。",
								error);
						if (!parseEndpoint(*value, keyFrame.Value,
							keyFrame.UsesResource, keyFrame.ResourceKey, L"KeyFrame"))
							return false;
					}
					else if (objectAnimation)
					{
						if (!descriptor || children.size() != 1
							|| !Equals(FromUtf8(children.front()->LocalName()),
								L"DiscreteObjectKeyFrame.Value"))
							return Fail(L"DiscreteObjectKeyFrame 必须声明 Value，"
								L"或包含一个 DiscreteObjectKeyFrame.Value。", error);
						keyFrame.Value.Kind = endpointKind;
						const auto& valueProperty = children.front();
						if (endpointKind == DesignerStyleValueKind::Brush)
						{
							if (!ParseBrush(valueProperty,
								keyFrame.Value.ObjectValue, error)) return false;
						}
						else
						{
							if (!ValidateAttributes(valueProperty, {}, error)) return false;
							const auto nested = ChildElements(valueProperty);
							if (nested.size() != 1)
								return Fail(L"DiscreteObjectKeyFrame.Value 必须且只能包含一个对象。",
									error);
							if (endpointKind == DesignerStyleValueKind::Geometry)
							{
								if (!ParseGeometryElement(nested.front(),
									keyFrame.Value.ObjectValue, error)) return false;
							}
							else if (endpointKind == DesignerStyleValueKind::Transform)
							{
								keyFrame.Value.ObjectValue = DesignValue::array();
								if (!ParseTransformElement(nested.front(),
									keyFrame.Value.ObjectValue, error)) return false;
								if (keyFrame.Value.ObjectValue.empty())
									return Fail(L"Object 关键帧的 Transform 不能为空。",
										error);
							}
							else return Fail(L"该目标属性仅支持标量 Object 关键帧 Value。",
								error);
						}
						BindingValue parsed;
						BindingValue converted;
						BindingValue coerced;
						std::wstring validationError;
						if (!DesignerStyleSheetUtils::TryConvertValue(
							keyFrame.Value, parsed, &validationError,
							_currentResourceBasePath, _document.Resources)
							|| !targetMetadata || !targetMetadata->CanWrite()
							|| !targetMetadata->TryConvert(parsed, converted)
							|| !targetMetadata->TryCoerce(
								*targetProbe, converted, coerced))
							return Fail(L"动画 KeyFrame 无法通过目标属性元数据转换或 Coerce。",
								error);
					}
					else return Fail(L"关键帧必须声明 Value。", error);
					if (keyFrame.Kind == DesignerKeyFrameKind::Easing)
					{
						if (children.size() > 1)
							return Fail(L"EasingKeyFrame 最多包含一个缓动函数。",
								error);
						if (!children.empty() && !parseEasing(children.front(),
							keyFrameName + L".EasingFunction",
							keyFrame.Easing, keyFrame.EasingMode)) return false;
					}
					else if (!objectAnimation && !children.empty())
						return Fail(L"仅 EasingKeyFrame 允许 EasingFunction 子元素。",
							error);
					if (spline)
					{
						const auto splineText = Attribute(keyFrameElement, L"KeySpline");
						if (!splineText || !TryParseKeySpline(*splineText,
							keyFrame.KeySplineX1, keyFrame.KeySplineY1,
							keyFrame.KeySplineX2, keyFrame.KeySplineY2))
							return Fail(L"KeySpline 必须是 0..1 内的两组控制点。",
								error);
					}
					animation.KeyFrames.push_back(std::move(keyFrame));
				}
				if (animation.KeyFrames.empty())
					return Fail(L"UsingKeyFrames 至少需要一个关键帧。", error);
				std::stable_sort(animation.KeyFrames.begin(),
					animation.KeyFrames.end(), [](const auto& left, const auto& right)
					{
						return left.KeyTimeMilliseconds < right.KeyTimeMilliseconds;
					});
				if (const auto duration = Attribute(animationElement, L"Duration"))
				{
					if (!TryParseTimeSpanMilliseconds(
						*duration, animation.DurationMilliseconds))
						return Fail(L"动画 Duration 必须是有限 TimeSpan。", error);
				}
				else animation.DurationMilliseconds =
					animation.KeyFrames.back().KeyTimeMilliseconds;
			}
			if (const auto begin = Attribute(animationElement, L"BeginTime");
				begin && !TryParseTimeSpanMilliseconds(
					*begin, animation.BeginTimeMilliseconds))
				return Fail(L"动画 BeginTime 必须是有限 TimeSpan。", error);
			return ParseTimelineBehavior(animationElement, animation, error);
		}

		bool ParseVisualStateGroups(
			const Element& element,
			DesignComponentDefinition& component,
			std::wstring& error)
		{
			DiagnosticContext context(*this, element);
			if (!ValidateAttributes(element, {}, error)
				|| !Trim(FromUtf8(element->InnerText())).empty())
				return Fail(L"VisualStateManager.VisualStateGroups 不允许属性或文本内容。",
					error);
			auto hostProbe = DesignDocumentMaterializer::CreateRuntimeControl(
				component.BaseType);
			if (!hostProbe || !DefineComponentProbeProperties(
				*hostProbe, component, error)) return false;

			auto createTargetProbe = [&](const std::wstring& targetName)
				-> std::unique_ptr<Control>
			{
				if (targetName.empty())
				{
					auto probe = DesignDocumentMaterializer::CreateRuntimeControl(
						component.BaseType);
					std::wstring ignored;
					if (!probe || !DefineComponentProbeProperties(
						*probe, component, ignored)) return nullptr;
					return probe;
				}
				const auto node = std::find_if(
					component.Template.begin(), component.Template.end(),
					[&](const auto& candidate)
					{ return Equals(candidate.Name, targetName); });
				if (node == component.Template.end()) return nullptr;
				auto probe = DesignDocumentMaterializer::CreateRuntimeControl(node->Type);
				if (!probe) return nullptr;
				if (!node->ComponentType.Empty())
				{
					const auto* nested = FindVisibleComponent(node->ComponentType);
					std::wstring ignored;
					if (!nested || !DefineComponentProbeProperties(
						*probe, *nested, ignored)) return nullptr;
				}
				return probe;
			};

			std::vector<std::pair<std::wstring, std::wstring>> controlledProperties;
			for (const auto& groupElement : ChildElements(element))
			{
				DiagnosticContext groupContext(*this, groupElement);
				if (!Equals(FromUtf8(groupElement->LocalName()), L"VisualStateGroup"))
					return Fail(L"VisualStateGroups 仅允许 VisualStateGroup。", error);
				if (!ValidateAttributes(groupElement, { L"Name" }, error, true)
					|| !Trim(FromUtf8(groupElement->InnerText())).empty()) return false;
				DesignerVisualStateGroup group;
				group.Name = Trim(Attribute(groupElement, L"Name", L"x").value_or(
					Attribute(groupElement, L"Name").value_or(L"")));
				if (!ValidateIdentifier(group.Name, L"视觉状态组名称", error)) return false;
				if (std::any_of(component.VisualStateGroups.begin(),
					component.VisualStateGroups.end(), [&](const auto& existing)
					{ return Equals(existing.Name, group.Name); }))
					return Fail(L"视觉状态组名称重复：" + group.Name, error);
				std::optional<size_t> fallbackState;
				std::vector<std::wstring> groupEvents;
				Element transitionsElement;
				for (const auto& stateElement : ChildElements(groupElement))
				{
					DiagnosticContext stateContext(*this, stateElement);
					if (Equals(FromUtf8(stateElement->LocalName()),
						L"VisualStateGroup.Transitions"))
					{
						if (transitionsElement)
							return Fail(L"VisualStateGroup.Transitions 不能重复。", error);
						if (!ValidateAttributes(stateElement, {}, error)
							|| !Trim(FromUtf8(stateElement->InnerText())).empty())
							return false;
						transitionsElement = stateElement;
						continue;
					}
					if (!Equals(FromUtf8(stateElement->LocalName()), L"VisualState"))
						return Fail(L"VisualStateGroup 仅允许 Transitions 和 VisualState。",
							error);
					if (!ValidateAttributes(stateElement, { L"Name" }, error, true))
						return false;
					DesignerVisualState state;
					state.Name = Trim(Attribute(stateElement, L"Name", L"x").value_or(
						Attribute(stateElement, L"Name").value_or(L"")));
					if (!ValidateIdentifier(state.Name, L"视觉状态名称", error)) return false;
					if (std::any_of(group.States.begin(), group.States.end(),
						[&](const auto& existing)
						{ return Equals(existing.Name, state.Name); }))
						return Fail(L"视觉状态名称重复：" + state.Name, error);
					bool foundTriggers = false;
					bool foundSetters = false;
					bool foundStoryboard = false;
					for (const auto& child : ChildElements(stateElement))
					{
						const auto childName = FromUtf8(child->LocalName());
						if (Equals(childName, L"VisualState.StateTriggers"))
						{
							if (foundTriggers)
								return Fail(L"VisualState.StateTriggers 不能重复。", error);
							foundTriggers = true;
							if (!ValidateAttributes(child, {}, error)
								|| !Trim(FromUtf8(child->InnerText())).empty()) return false;
							for (const auto& trigger : ChildElements(child))
							{
								DiagnosticContext triggerContext(*this, trigger);
								const auto triggerName = FromUtf8(trigger->LocalName());
								if (!ChildElements(trigger).empty()
									|| !Trim(FromUtf8(trigger->InnerText())).empty())
									return Fail(L"视觉状态触发器不能包含内容。", error);
								if (Equals(triggerName, L"StateTrigger"))
								{
									if (!state.EventNames.empty())
										return Fail(L"同一 VisualState 不能混用 StateTrigger 与 EventTrigger。",
											error);
									if (!ValidateAttributes(trigger,
										{ L"Property", L"Value" }, error)) return false;
									const auto rawProperty = Trim(Attribute(
										trigger, L"Property").value_or(L""));
									const auto rawValue = Attribute(trigger, L"Value");
									std::wstring resourceKey;
									if (rawProperty.empty() || !rawValue
										|| TryParseStaticResource(*rawValue, resourceKey))
										return Fail(L"StateTrigger 需要 Property 和字面 Value。", error);
									const auto propertyName = NormalizePropertyName(
										rawProperty, *rawValue);
									const auto properties = DesignerPropertyCatalog::GetConditionProperties(
										*hostProbe);
									const auto* descriptor = DesignerPropertyCatalog::Find(
										properties, propertyName);
									if (!descriptor)
										return Fail(L"StateTrigger 属性不存在：" + rawProperty, error);
									if (std::any_of(state.Conditions.begin(), state.Conditions.end(),
										[&](const auto& existing)
										{ return Equals(existing.PropertyName, descriptor->Name); }))
										return Fail(L"StateTrigger 属性重复：" + descriptor->Name, error);
									DesignerVisualStateCondition condition;
									condition.PropertyName = descriptor->Name;
									condition.Value = { descriptor->ValueKind,
										NormalizePropertyText(rawProperty, *rawValue, *descriptor) };
									std::wstring validationError;
									if (!DesignerPropertyCatalog::ValidateConditionValue(
										*hostProbe, descriptor->Name, condition.Value,
										&validationError, _currentResourceBasePath,
										_document.Resources))
										return Fail(L"StateTrigger " + rawProperty + L"："
											+ validationError, error);
									state.Conditions.push_back(std::move(condition));
								}
								else if (Equals(triggerName, L"EventTrigger"))
								{
									if (!state.Conditions.empty())
										return Fail(L"同一 VisualState 不能混用 StateTrigger 与 EventTrigger。",
											error);
									if (!ValidateAttributes(trigger, { L"Event" }, error))
										return false;
									const auto eventName = Trim(Attribute(
										trigger, L"Event").value_or(L""));
									const auto event = std::find_if(
										component.Events.begin(), component.Events.end(),
										[&](const auto& candidate)
										{ return Equals(candidate.Name, eventName); });
									if (event == component.Events.end()
										|| std::any_of(groupEvents.begin(), groupEvents.end(),
											[&](const auto& existing)
											{ return Equals(existing, eventName); }))
										return Fail(L"EventTrigger 事件不存在或在组内重复："
											+ eventName, error);
									groupEvents.push_back(event->Name);
									state.EventNames.push_back(event->Name);
								}
								else return Fail(L"VisualState.StateTriggers 仅支持 StateTrigger 和 EventTrigger。",
									error);
							}
							continue;
						}
						if (Equals(childName, L"VisualState.Storyboard"))
						{
							if (foundStoryboard)
								return Fail(L"VisualState.Storyboard 不能重复。", error);
							foundStoryboard = true;
							if (!ValidateAttributes(child, {}, error)
								|| !Trim(FromUtf8(child->InnerText())).empty()) return false;
							const auto storyboards = ChildElements(child);
							if (storyboards.size() != 1
								|| !Equals(FromUtf8(storyboards.front()->LocalName()),
									L"Storyboard"))
								return Fail(L"VisualState.Storyboard 必须包含一个 Storyboard。",
									error);
							const auto& storyboard = storyboards.front();
							if (!ValidateAttributes(storyboard, {}, error)
								|| !Trim(FromUtf8(storyboard->InnerText())).empty()) return false;
							for (const auto& animationElement : ChildElements(storyboard))
							{
								const auto sharedAnimationName = FromUtf8(
									animationElement->LocalName());
								if (Equals(sharedAnimationName, L"DoubleAnimation")
									|| Equals(sharedAnimationName,
										L"DoubleAnimationUsingKeyFrames")
									|| Equals(sharedAnimationName, L"ColorAnimation")
									|| Equals(sharedAnimationName,
										L"ColorAnimationUsingKeyFrames")
									|| Equals(sharedAnimationName,
									L"ObjectAnimationUsingKeyFrames")
									|| Equals(sharedAnimationName, L"ThicknessAnimation")
									|| Equals(sharedAnimationName,
										L"ThicknessAnimationUsingKeyFrames")
									|| Equals(sharedAnimationName, L"PointAnimation")
									|| Equals(sharedAnimationName,
										L"PointAnimationUsingKeyFrames")
									|| Equals(sharedAnimationName, L"VectorAnimation")
									|| Equals(sharedAnimationName,
										L"VectorAnimationUsingKeyFrames")
									|| Equals(sharedAnimationName, L"RectAnimation")
									|| Equals(sharedAnimationName,
										L"RectAnimationUsingKeyFrames")
									|| Equals(sharedAnimationName, L"SizeAnimation")
									|| Equals(sharedAnimationName,
										L"SizeAnimationUsingKeyFrames")
									|| Equals(sharedAnimationName, L"MatrixAnimation")
									|| Equals(sharedAnimationName,
										L"MatrixAnimationUsingKeyFrames"))
								{
									DesignerVisualStateAnimation animation;
									StoryboardObjectPathKind objectPathKind =
										StoryboardObjectPathKind::None;
									if (!ParseStoryboardAnimation(animationElement, component,
										animation, objectPathKind, error)) return false;
									const auto rootProperty = StoryboardAnimationRootProperty(
										animation.PropertyName);
									const bool indirectPath = objectPathKind
										!= StoryboardObjectPathKind::None;
									const bool duplicate = std::any_of(
										state.Setters.begin(), state.Setters.end(), [&](const auto& existing)
										{ return Equals(existing.TargetName, animation.TargetName)
											&& Equals(existing.PropertyName, rootProperty); })
										|| std::any_of(state.Animations.begin(), state.Animations.end(),
											[&](const auto& existing)
											{
												if (!Equals(existing.TargetName, animation.TargetName)) return false;
												const auto existingKind = ClassifyStoryboardObjectPath(
													existing.PropertyName);
												const auto existingRoot = StoryboardAnimationRootProperty(
													existing.PropertyName);
												return Equals(existing.PropertyName, animation.PropertyName)
													|| (Equals(existingRoot, rootProperty)
														&& (existingKind == StoryboardObjectPathKind::None
															|| !indirectPath));
											});
									if (duplicate)
										return Fail(L"同一 VisualState 的 Setter/Storyboard 目标重复："
											+ animation.TargetName + L"." + animation.PropertyName, error);
									const auto controlledKey = Lower(animation.TargetName) + L"|"
										+ Lower(rootProperty);
									const auto controlled = std::find_if(
										controlledProperties.begin(), controlledProperties.end(),
										[&](const auto& existing)
										{ return existing.first == controlledKey; });
									if (controlled != controlledProperties.end()
										&& !Equals(controlled->second, group.Name))
										return Fail(L"不同 VisualStateGroup 不能控制同一动画属性："
											+ animation.PropertyName, error);
									if (controlled == controlledProperties.end())
										controlledProperties.emplace_back(controlledKey, group.Name);
									state.Animations.push_back(std::move(animation));
									continue;
								}
								DiagnosticContext animationContext(*this, animationElement);
								const auto animationName = FromUtf8(
									animationElement->LocalName());
								DesignerVisualStateAnimation animation;
								bool keyFrameAnimation = false;
								if (Equals(animationName, L"DoubleAnimation")
									|| Equals(animationName,
										L"DoubleAnimationUsingKeyFrames"))
								{
									animation.Kind = DesignerAnimationKind::Double;
									keyFrameAnimation = Equals(animationName,
										L"DoubleAnimationUsingKeyFrames");
								}
								else if (Equals(animationName, L"ColorAnimation")
									|| Equals(animationName,
										L"ColorAnimationUsingKeyFrames"))
								{
									animation.Kind = DesignerAnimationKind::Color;
									keyFrameAnimation = Equals(animationName,
										L"ColorAnimationUsingKeyFrames");
								}
								else return Fail(L"Storyboard 仅支持 Double/Color Animation 与 UsingKeyFrames。",
									error);
								const bool validAnimationAttributes = keyFrameAnimation
									? ValidateAttributes(animationElement,
										{ L"Storyboard.TargetName", L"Storyboard.TargetProperty",
											L"Duration", L"BeginTime", L"RepeatBehavior",
											L"AutoReverse", L"IsAdditive", L"IsCumulative",
											L"FillBehavior", L"SpeedRatio",
											L"AccelerationRatio", L"DecelerationRatio" }, error)
									: ValidateAttributes(animationElement,
										{ L"Storyboard.TargetName", L"Storyboard.TargetProperty",
											L"From", L"To", L"By", L"Duration", L"BeginTime",
											L"RepeatBehavior", L"AutoReverse", L"IsAdditive",
											L"IsCumulative", L"FillBehavior",
											L"SpeedRatio", L"AccelerationRatio",
											L"DecelerationRatio" }, error);
								if (!validAnimationAttributes)
									return false;
								animation.TargetName = Trim(Attribute(animationElement,
									L"Storyboard.TargetName").value_or(L""));
								if (!animation.TargetName.empty()
									&& !ValidateIdentifier(animation.TargetName,
										L"Storyboard TargetName", error)) return false;
								const auto rawProperty = Trim(Attribute(animationElement,
									L"Storyboard.TargetProperty").value_or(L""));
								if (rawProperty.empty())
									return Fail(L"动画缺少 Storyboard.TargetProperty。", error);
								auto targetProbe = createTargetProbe(animation.TargetName);
								if (!targetProbe)
									return Fail(L"Storyboard 找不到模板部件："
										+ animation.TargetName, error);
								const DesignerPropertyDescriptor* descriptor = nullptr;
								const BindingPropertyMetadata* targetMetadata = nullptr;
								std::vector<DesignerPropertyDescriptor> targetProperties;
								DesignerStyleValueKind endpointKind =
									DesignerStyleValueKind::Float;
								bool transformPath = false;
								if (rawProperty.front() != L'(')
								{
									const auto propertyName = NormalizePropertyName(rawProperty, L"");
									targetProperties = DesignerPropertyCatalog::GetStyleProperties(
										*targetProbe);
									descriptor = DesignerPropertyCatalog::Find(
										targetProperties, propertyName);
									if (!descriptor)
										return Fail(L"Storyboard 目标属性不存在或不可写："
											+ rawProperty, error);
									const bool compatible = animation.Kind
										== DesignerAnimationKind::Color
										? descriptor->ValueKind == DesignerStyleValueKind::Color
										: descriptor->ValueKind == DesignerStyleValueKind::Int
											|| descriptor->ValueKind == DesignerStyleValueKind::Int64
											|| descriptor->ValueKind == DesignerStyleValueKind::Float
											|| descriptor->ValueKind == DesignerStyleValueKind::Double;
									if (!compatible)
										return Fail(animationName + L" 与目标属性类型不兼容："
											+ descriptor->Name, error);
									animation.PropertyName = descriptor->Name;
									endpointKind = descriptor->ValueKind;
									targetMetadata = targetProbe->FindPropertyMetadata(
										descriptor->Name);
								}
								else
								{
									ResolvedStoryboardObjectPath resolvedPath;
									std::wstring pathError;
									if (!TryResolveStoryboardObjectPath(component,
										animation.TargetName, rawProperty, animation.Kind,
										resolvedPath, &pathError))
										return Fail(pathError, error);
									animation.PropertyName = resolvedPath.CanonicalPath;
									transformPath = resolvedPath.Kind
										== StoryboardObjectPathKind::RenderTransform;
								}
								auto parseEndpoint = [&](const std::wstring& raw,
									DesignerStyleValue& literal, bool& usesResource,
									std::wstring& resourceKey, const std::wstring& label,
									bool isDelta = false)
								{
									usesResource = TryParseStaticResource(raw, resourceKey);
									const DesignerStyleValue* value = &literal;
									if (usesResource)
									{
										const auto* resource = FindVisibleResource(resourceKey);
										if (!resource)
											return Fail(L"动画 " + label + L" 引用了不存在的资源："
												+ resourceKey, error);
										value = &resource->Value;
									}
									else
									{
										literal.Kind = endpointKind;
										literal.Text = descriptor
											? NormalizePropertyText(rawProperty, raw, *descriptor)
											: raw;
									}
									// Animation values intentionally follow metadata conversion
									// instead of the Style Setter's exact-kind rule: a typed
									// Double resource is a valid endpoint for a Float property.
									BindingValue parsed;
									BindingValue converted;
									BindingValue coerced;
									std::wstring validationError;
									if (!DesignerStyleSheetUtils::TryConvertValue(
											*value, parsed, &validationError,
											_currentResourceBasePath, _document.Resources))
										return Fail(L"动画 " + label + L" 无效："
											+ validationError, error);
									if (transformPath)
									{
										double number = 0.0;
										if (!parsed.TryGetDouble(number) || !std::isfinite(number)
											|| number < -(std::numeric_limits<float>::max)()
											|| number > (std::numeric_limits<float>::max)())
											return Fail(L"动画 " + label + L" 必须是有限 Float 值。",
												error);
									}
									else if (!targetMetadata || !targetMetadata->CanWrite()
										|| !targetMetadata->TryConvert(parsed, converted)
										|| (!isDelta && !targetMetadata->TryCoerce(
											*targetProbe, converted, coerced)))
										return Fail(L"动画 " + label + L" 无效："
											+ (validationError.empty()
												? L"无法通过目标属性元数据转换或 Coerce。"
												: validationError), error);
									return true;
								};
								const auto animationChildren = ChildElements(animationElement);
								if (!Trim(FromUtf8(animationElement->InnerText())).empty())
									return Fail(L"动画不允许文本内容。", error);
								auto parseEasing = [&](const Element& easingProperty,
									const std::wstring& expectedPropertyName,
									DesignerEasingKind& kind,
									DesignerEasingMode& mode)
								{
									if (!Equals(FromUtf8(easingProperty->LocalName()),
										expectedPropertyName))
										return Fail(L"EasingFunction 属性元素名称无效。", error);
									if (!ValidateAttributes(easingProperty, {}, error)
										|| !Trim(FromUtf8(easingProperty->InnerText())).empty())
										return false;
									const auto easingChildren = ChildElements(easingProperty);
									if (easingChildren.size() != 1)
										return Fail(L"EasingFunction 必须包含一个缓动对象。", error);
									const auto& easing = easingChildren.front();
									const auto easingName = FromUtf8(easing->LocalName());
									if (Equals(easingName, L"QuadraticEase"))
										kind = DesignerEasingKind::Quadratic;
									else if (Equals(easingName, L"CubicEase"))
										kind = DesignerEasingKind::Cubic;
									else if (Equals(easingName, L"SineEase"))
										kind = DesignerEasingKind::Sine;
									else return Fail(L"EasingFunction 第一批仅支持 QuadraticEase、CubicEase 和 SineEase。",
										error);
									if (!ValidateAttributes(easing, { L"EasingMode" }, error)
										|| !ChildElements(easing).empty()
										|| !Trim(FromUtf8(easing->InnerText())).empty()) return false;
									if (const auto modeText = Attribute(easing, L"EasingMode"))
									{
										int parsedMode = 0;
										if (!TryParseEnum(*modeText,
											{ L"EaseIn", L"EaseOut", L"EaseInOut" }, parsedMode))
											return Fail(L"EasingMode 无效。", error);
										mode = static_cast<DesignerEasingMode>(parsedMode);
									}
									return true;
								};
								if (!keyFrameAnimation)
								{
									if (const auto to = Attribute(animationElement, L"To"))
									{
										animation.HasTo = true;
										if (!parseEndpoint(*to, animation.To,
											animation.ToUsesResource,
											animation.ToResourceKey, L"To")) return false;
									}
									if (const auto from = Attribute(animationElement, L"From"))
									{
										animation.HasFrom = true;
										if (!parseEndpoint(*from, animation.From,
											animation.FromUsesResource,
											animation.FromResourceKey, L"From")) return false;
									}
									if (const auto by = Attribute(animationElement, L"By"))
									{
										animation.HasBy = true;
										if (!parseEndpoint(*by, animation.By,
											animation.ByUsesResource,
											animation.ByResourceKey, L"By", true)) return false;
									}
									const auto duration = Attribute(animationElement, L"Duration");
									if (!duration || !TryParseTimeSpanMilliseconds(
										*duration, animation.DurationMilliseconds))
										return Fail(L"动画 Duration 必须是有限 TimeSpan。", error);
									if (!animationChildren.empty())
									{
										if (animationChildren.size() != 1)
											return Fail(L"动画只允许一个 EasingFunction 属性元素。",
												error);
										if (!parseEasing(animationChildren.front(),
											animationName + L".EasingFunction",
											animation.Easing, animation.EasingMode)) return false;
									}
								}
								else
								{
									const auto valueType = animation.Kind
										== DesignerAnimationKind::Color ? L"Color" : L"Double";
									for (const auto& keyFrameElement : animationChildren)
									{
										DiagnosticContext keyFrameContext(*this, keyFrameElement);
										const auto keyFrameName = FromUtf8(
											keyFrameElement->LocalName());
										DesignerAnimationKeyFrame keyFrame;
										if (Equals(keyFrameName,
											L"Discrete" + std::wstring(valueType) + L"KeyFrame"))
											keyFrame.Kind = DesignerKeyFrameKind::Discrete;
										else if (Equals(keyFrameName,
											L"Linear" + std::wstring(valueType) + L"KeyFrame"))
											keyFrame.Kind = DesignerKeyFrameKind::Linear;
										else if (Equals(keyFrameName,
											L"Easing" + std::wstring(valueType) + L"KeyFrame"))
											keyFrame.Kind = DesignerKeyFrameKind::Easing;
										else if (Equals(keyFrameName,
											L"Spline" + std::wstring(valueType) + L"KeyFrame"))
											keyFrame.Kind = DesignerKeyFrameKind::Spline;
										else return Fail(L"UsingKeyFrames 包含不兼容的关键帧类型："
											+ keyFrameName, error);
										const bool spline = keyFrame.Kind
											== DesignerKeyFrameKind::Spline;
										if (!ValidateAttributes(keyFrameElement,
											spline
												? std::initializer_list<const wchar_t*>{
													L"KeyTime", L"Value", L"KeySpline" }
												: std::initializer_list<const wchar_t*>{
													L"KeyTime", L"Value" }, error)) return false;
										const auto keyTime = Attribute(keyFrameElement, L"KeyTime");
										if (!keyTime || !TryParseTimeSpanMilliseconds(
											*keyTime, keyFrame.KeyTimeMilliseconds))
											return Fail(L"关键帧 KeyTime 必须是显式有限 TimeSpan。", error);
										const auto value = Attribute(keyFrameElement, L"Value");
										if (!value || !parseEndpoint(*value, keyFrame.Value,
											keyFrame.UsesResource, keyFrame.ResourceKey,
											L"KeyFrame"))
											return value.has_value() ? false
												: Fail(L"关键帧必须声明 Value。", error);
										const auto children = ChildElements(keyFrameElement);
										if (!Trim(FromUtf8(keyFrameElement->InnerText())).empty())
											return Fail(L"关键帧不允许文本内容。", error);
										if (keyFrame.Kind == DesignerKeyFrameKind::Easing)
										{
											if (children.size() > 1)
												return Fail(L"EasingKeyFrame 最多包含一个缓动函数。",
													error);
											if (!children.empty() && !parseEasing(children.front(),
													keyFrameName + L".EasingFunction",
													keyFrame.Easing, keyFrame.EasingMode))
												return false;
										}
										else if (!children.empty())
											return Fail(L"仅 EasingKeyFrame 允许 EasingFunction 子元素。",
												error);
										if (spline)
										{
											const auto splineText = Attribute(
												keyFrameElement, L"KeySpline");
											if (!splineText || !TryParseKeySpline(*splineText,
												keyFrame.KeySplineX1, keyFrame.KeySplineY1,
												keyFrame.KeySplineX2, keyFrame.KeySplineY2))
												return Fail(L"KeySpline 必须是 0..1 内的两组控制点。",
													error);
										}
										animation.KeyFrames.push_back(std::move(keyFrame));
									}
									if (animation.KeyFrames.empty())
										return Fail(L"UsingKeyFrames 至少需要一个关键帧。", error);
									std::stable_sort(animation.KeyFrames.begin(),
										animation.KeyFrames.end(), [](const auto& left,
											const auto& right)
										{
											return left.KeyTimeMilliseconds
												< right.KeyTimeMilliseconds;
										});
									if (const auto duration = Attribute(animationElement, L"Duration"))
									{
										if (!TryParseTimeSpanMilliseconds(
											*duration, animation.DurationMilliseconds))
											return Fail(L"动画 Duration 必须是有限 TimeSpan。", error);
									}
									else animation.DurationMilliseconds =
										animation.KeyFrames.back().KeyTimeMilliseconds;
								}
								if (const auto begin = Attribute(animationElement, L"BeginTime");
									begin && !TryParseTimeSpanMilliseconds(
										*begin, animation.BeginTimeMilliseconds))
									return Fail(L"动画 BeginTime 必须是有限 TimeSpan。", error);
								if (!ParseTimelineBehavior(
									animationElement, animation, error)) return false;
								const bool duplicate = std::any_of(
									state.Setters.begin(), state.Setters.end(), [&](const auto& existing)
									{ return Equals(existing.TargetName, animation.TargetName)
										&& (Equals(existing.PropertyName, animation.PropertyName)
											|| (transformPath
												&& Equals(existing.PropertyName, L"RenderTransform"))); })
									|| std::any_of(state.Animations.begin(), state.Animations.end(),
										[&](const auto& existing)
										{ return Equals(existing.TargetName, animation.TargetName)
											&& Equals(existing.PropertyName, animation.PropertyName); });
								if (duplicate)
									return Fail(L"同一 VisualState 的 Setter/Storyboard 目标重复："
										+ animation.TargetName + L"." + animation.PropertyName, error);
								const auto controlledKey = Lower(animation.TargetName) + L"|"
									+ Lower(transformPath ? L"RenderTransform"
										: animation.PropertyName);
								const auto controlled = std::find_if(
									controlledProperties.begin(), controlledProperties.end(),
									[&](const auto& existing)
									{ return existing.first == controlledKey; });
								if (controlled != controlledProperties.end()
									&& !Equals(controlled->second, group.Name))
									return Fail(L"不同 VisualStateGroup 不能控制同一动画属性："
										+ animation.PropertyName, error);
								if (controlled == controlledProperties.end())
									controlledProperties.emplace_back(controlledKey, group.Name);
								state.Animations.push_back(std::move(animation));
							}
							continue;
						}
						if (!Equals(childName, L"VisualState.Setters"))
							return Fail(L"VisualState 仅支持 StateTriggers、Setters 和 Storyboard。", error);
						if (foundSetters)
							return Fail(L"VisualState.Setters 不能重复。", error);
						foundSetters = true;
						if (!ValidateAttributes(child, {}, error)
							|| !Trim(FromUtf8(child->InnerText())).empty()) return false;
						for (const auto& setterElement : ChildElements(child))
						{
							if (!Equals(FromUtf8(setterElement->LocalName()), L"Setter"))
								return Fail(L"VisualState.Setters 仅允许 Setter。", error);
							DesignerVisualStateSetter setter;
							setter.TargetName = Trim(Attribute(
								setterElement, L"TargetName").value_or(L""));
							if (!setter.TargetName.empty()
								&& !ValidateIdentifier(
									setter.TargetName, L"视觉状态 TargetName", error))
								return false;
							auto targetProbe = createTargetProbe(setter.TargetName);
							if (!targetProbe)
								return Fail(L"VisualState Setter 找不到模板部件："
									+ setter.TargetName, error);
							DesignerStyleSetter value;
							if (!ParseStyleSetter(
								setterElement, *targetProbe, value, error, true)) return false;
							setter.PropertyName = value.PropertyName;
							setter.UsesResource = value.UsesResource;
							setter.ResourceKey = std::move(value.ResourceKey);
							setter.Literal = std::move(value.Literal);
							if (setter.UsesResource)
							{
								const auto* resource = FindVisibleResource(
									setter.ResourceKey);
								std::wstring validationError;
								if (!resource
									|| !DesignerPropertyCatalog::ValidateStyleValue(
										*targetProbe, setter.PropertyName, resource->Value,
										&validationError, _currentResourceBasePath,
										_document.Resources))
									return Fail(L"VisualState Setter 资源不存在或类型不兼容："
										+ setter.ResourceKey, error);
								if (_objectResourceTarget)
									setter.Literal = resource->Value;
							}
							if (std::any_of(state.Setters.begin(), state.Setters.end(),
								[&](const auto& existing)
								{
									return Equals(existing.TargetName, setter.TargetName)
										&& Equals(existing.PropertyName, setter.PropertyName);
								}) || std::any_of(state.Animations.begin(),
									state.Animations.end(), [&](const auto& existing)
								{
									return Equals(existing.TargetName, setter.TargetName)
										&& Equals(StoryboardAnimationRootProperty(
											existing.PropertyName), setter.PropertyName);
									}))
								return Fail(L"VisualState Setter 重复：" + setter.TargetName
									+ L"." + setter.PropertyName, error);
							state.Setters.push_back(std::move(setter));
						}
					}
					if (state.Conditions.empty() && state.EventNames.empty())
					{
						if (fallbackState)
							return Fail(L"每个 VisualStateGroup 只能有一个无触发器状态。",
								error);
						fallbackState = group.States.size();
					}
					for (const auto& setter : state.Setters)
					{
						const auto key = Lower(setter.TargetName) + L"|"
							+ Lower(setter.PropertyName);
						const auto controlled = std::find_if(
							controlledProperties.begin(), controlledProperties.end(),
							[&](const auto& existing) { return existing.first == key; });
						if (controlled != controlledProperties.end()
							&& !Equals(controlled->second, group.Name))
							return Fail(L"不同 VisualStateGroup 不能控制同一属性："
								+ setter.PropertyName, error);
						if (controlled == controlledProperties.end())
							controlledProperties.emplace_back(key, group.Name);
					}
					group.States.push_back(std::move(state));
				}
				if (transitionsElement)
				{
					auto stateExists = [&](const std::wstring& name)
					{
						return name.empty() || std::any_of(
							group.States.begin(), group.States.end(),
							[&](const auto& state) { return Equals(state.Name, name); });
					};
					for (const auto& transitionElement : ChildElements(transitionsElement))
					{
						DiagnosticContext transitionContext(*this, transitionElement);
						if (!Equals(FromUtf8(transitionElement->LocalName()),
							L"VisualTransition"))
							return Fail(L"VisualStateGroup.Transitions 仅允许 VisualTransition。",
								error);
						if (!ValidateAttributes(transitionElement,
							{ L"From", L"To", L"GeneratedDuration" }, error)
							|| !Trim(FromUtf8(transitionElement->InnerText())).empty())
							return false;
						DesignerVisualTransition transition;
						transition.FromState = Trim(Attribute(
							transitionElement, L"From").value_or(L""));
						transition.ToState = Trim(Attribute(
							transitionElement, L"To").value_or(L""));
						if ((!transition.FromState.empty()
							&& !ValidateIdentifier(transition.FromState,
								L"VisualTransition.From", error))
							|| (!transition.ToState.empty()
								&& !ValidateIdentifier(transition.ToState,
									L"VisualTransition.To", error))) return false;
						if (!stateExists(transition.FromState))
							return Fail(L"VisualTransition.From 状态不存在："
								+ transition.FromState, error);
						if (!stateExists(transition.ToState))
							return Fail(L"VisualTransition.To 状态不存在："
								+ transition.ToState, error);
						if (std::any_of(group.Transitions.begin(), group.Transitions.end(),
							[&](const auto& existing)
							{
								return Equals(existing.FromState, transition.FromState)
									&& Equals(existing.ToState, transition.ToState);
							}))
							return Fail(L"VisualTransition From/To 选择器重复。", error);
						if (const auto duration = Attribute(
							transitionElement, L"GeneratedDuration"))
							if (!TryParseTimeSpanMilliseconds(
								*duration, transition.GeneratedDurationMilliseconds))
								return Fail(L"VisualTransition.GeneratedDuration 必须是有限 TimeSpan。",
									error);
						bool foundEasing = false;
						bool foundStoryboard = false;
						for (const auto& child : ChildElements(transitionElement))
						{
							const auto childName = FromUtf8(child->LocalName());
							if (Equals(childName,
								L"VisualTransition.GeneratedEasingFunction"))
							{
								if (foundEasing)
									return Fail(L"VisualTransition.GeneratedEasingFunction 不能重复。",
										error);
								foundEasing = true;
								if (!ValidateAttributes(child, {}, error)
									|| !Trim(FromUtf8(child->InnerText())).empty()) return false;
								const auto easingChildren = ChildElements(child);
								if (easingChildren.size() != 1)
									return Fail(L"GeneratedEasingFunction 必须包含一个缓动对象。",
										error);
								const auto& easing = easingChildren.front();
								const auto easingName = FromUtf8(easing->LocalName());
								if (Equals(easingName, L"QuadraticEase"))
									transition.GeneratedEasing = DesignerEasingKind::Quadratic;
								else if (Equals(easingName, L"CubicEase"))
									transition.GeneratedEasing = DesignerEasingKind::Cubic;
								else if (Equals(easingName, L"SineEase"))
									transition.GeneratedEasing = DesignerEasingKind::Sine;
								else return Fail(L"GeneratedEasingFunction 第一批仅支持 "
									L"QuadraticEase、CubicEase 和 SineEase。", error);
								if (!ValidateAttributes(easing, { L"EasingMode" }, error)
									|| !ChildElements(easing).empty()
									|| !Trim(FromUtf8(easing->InnerText())).empty()) return false;
								if (const auto mode = Attribute(easing, L"EasingMode"))
								{
									int parsedMode = 0;
									if (!TryParseEnum(*mode,
										{ L"EaseIn", L"EaseOut", L"EaseInOut" }, parsedMode))
										return Fail(L"GeneratedEasingFunction.EasingMode 无效。",
											error);
									transition.GeneratedEasingMode =
										static_cast<DesignerEasingMode>(parsedMode);
								}
								continue;
							}
							Element storyboard;
							if (Equals(childName, L"VisualTransition.Storyboard"))
							{
								if (foundStoryboard)
									return Fail(L"VisualTransition.Storyboard 不能重复。", error);
								foundStoryboard = true;
								if (!ValidateAttributes(child, {}, error)
									|| !Trim(FromUtf8(child->InnerText())).empty()) return false;
								const auto storyboards = ChildElements(child);
								if (storyboards.size() != 1
									|| !Equals(FromUtf8(storyboards.front()->LocalName()),
										L"Storyboard"))
									return Fail(L"VisualTransition.Storyboard 必须包含一个 Storyboard。",
										error);
								storyboard = storyboards.front();
							}
							else if (Equals(childName, L"Storyboard"))
							{
								if (foundStoryboard)
									return Fail(L"VisualTransition Storyboard 不能重复。", error);
								foundStoryboard = true;
								storyboard = child;
							}
							else return Fail(L"VisualTransition 仅支持 GeneratedEasingFunction "
								L"和 Storyboard。", error);
							if (!ValidateAttributes(storyboard, {}, error)
								|| !Trim(FromUtf8(storyboard->InnerText())).empty()) return false;
							for (const auto& animationElement : ChildElements(storyboard))
							{
								DesignerVisualStateAnimation animation;
								StoryboardObjectPathKind objectPathKind =
									StoryboardObjectPathKind::None;
								if (!ParseStoryboardAnimation(animationElement, component,
									animation, objectPathKind, error)) return false;
								const auto rootProperty = StoryboardAnimationRootProperty(
									animation.PropertyName);
								for (const auto& existing : transition.Animations)
								{
									if (!Equals(existing.TargetName, animation.TargetName)) continue;
									const bool existingPath = ClassifyStoryboardObjectPath(
										existing.PropertyName)
										!= StoryboardObjectPathKind::None;
									const auto existingRoot = StoryboardAnimationRootProperty(
										existing.PropertyName);
									if (Equals(existing.PropertyName, animation.PropertyName)
										|| (Equals(existingRoot, rootProperty)
											&& (!existingPath || objectPathKind
												== StoryboardObjectPathKind::None)))
										return Fail(L"VisualTransition Storyboard 目标重复："
											+ animation.PropertyName, error);
								}
								const auto controlledKey = Lower(animation.TargetName) + L"|"
									+ Lower(rootProperty);
								const auto controlled = std::find_if(
									controlledProperties.begin(), controlledProperties.end(),
									[&](const auto& existing)
									{ return existing.first == controlledKey; });
								if (controlled != controlledProperties.end()
									&& !Equals(controlled->second, group.Name))
									return Fail(L"不同 VisualStateGroup 不能控制同一 Transition 属性："
										+ rootProperty, error);
								if (controlled == controlledProperties.end())
									controlledProperties.emplace_back(controlledKey, group.Name);
								transition.Animations.push_back(std::move(animation));
							}
						}
						group.Transitions.push_back(std::move(transition));
					}
				}
				if (group.States.empty() || !fallbackState)
					return Fail(L"VisualStateGroup 必须包含一个无触发器的回退状态："
						+ group.Name, error);
				component.VisualStateGroups.push_back(std::move(group));
			}
			if (component.VisualStateGroups.empty())
				return Fail(L"VisualStateGroups 至少需要一个 VisualStateGroup。", error);
			return true;
		}

		bool ParseStyleCondition(
			const Element& element,
			Control& probe,
			DesignerStyleTrigger& trigger,
			bool requireLeaf,
			std::wstring& error)
		{
			DiagnosticContext context(*this, element);
			if (!ValidateAttributes(element, { L"Property", L"Value" }, error))
				return false;
			const auto rawProperty = Trim(
				Attribute(element, L"Property").value_or(L""));
			const auto value = Attribute(element, L"Value");
			if (rawProperty.empty() || !value)
				return Fail(L"Condition 必须声明 Property 和 Value。", error);
			const auto stateProperty =
				DesignerStyleSheetUtils::CanonicalTriggerProperty(rawProperty);
			if (!stateProperty.empty())
			{
				DesignerStyleCondition condition;
				condition.Property = stateProperty;
				if (!TryParseBool(*value, condition.Value))
					return Fail(L"状态 Condition Value 必须是布尔值。", error);
				trigger.Conditions.push_back(std::move(condition));
			}
			else
			{
				const auto propertyName = NormalizePropertyName(rawProperty, *value);
				const auto properties =
					DesignerPropertyCatalog::GetPropertyGridProperties(probe);
				const auto* descriptor = DesignerPropertyCatalog::Find(
					properties, propertyName);
				if (!descriptor || !descriptor->Metadata
					|| !descriptor->Metadata->CanRead()
					|| !descriptor->Metadata->CanObserve())
					return Fail(L"Condition Property 必须是目标类型中可读、可观察的元数据属性："
						+ rawProperty, error);
				DesignerStylePropertyCondition condition;
				condition.Property = descriptor->Name;
				condition.Value = DesignerStyleValue{
					descriptor->ValueKind,
					NormalizePropertyText(rawProperty, *value, *descriptor) };
				std::wstring validationError;
				if (!DesignerPropertyCatalog::ValidateConditionValue(
					probe, condition.Property, condition.Value, &validationError,
					_currentResourceBasePath, _document.Resources))
					return Fail(L"Condition " + rawProperty + L"："
						+ validationError, error);
				trigger.PropertyConditions.push_back(std::move(condition));
			}
			if (requireLeaf && !ChildElements(element).empty())
				return Fail(L"Condition 不能包含子元素。", error);
			return true;
		}

		bool ParseStoryboardActionCollection(
			const Element& element,
			const DesignComponentDefinition& target,
			std::vector<DesignerEventTriggerAction>& actions,
			std::vector<std::wstring>& beginNames,
			std::vector<std::wstring>& referencedNames,
			bool styleScope,
			std::wstring& error)
		{
			DiagnosticContext context(*this, element);
			if (!ValidateAttributes(element, {}, error)
				|| !Trim(FromUtf8(element->InnerText())).empty()) return false;
			for (const auto& actionElement : ChildElements(element))
			{
				DiagnosticContext actionContext(*this, actionElement);
				const auto actionName = FromUtf8(actionElement->LocalName());
				DesignerEventTriggerAction action;
				if (Equals(actionName, L"BeginStoryboard"))
				{
					action.Kind = DesignerStoryboardActionKind::Begin;
					if (!ValidateAttributes(actionElement, { L"Name" }, error, true)
						|| !Trim(FromUtf8(actionElement->InnerText())).empty())
						return false;
					action.StoryboardName = Trim(Attribute(
						actionElement, L"Name", L"x").value_or(
							Attribute(actionElement, L"Name").value_or(L"")));
					if (!action.StoryboardName.empty())
					{
						if (!ValidateIdentifier(action.StoryboardName,
							L"BeginStoryboard x:Name", error)) return false;
						if (std::any_of(beginNames.begin(), beginNames.end(),
							[&](const auto& existing)
							{ return Equals(existing, action.StoryboardName); }))
							return Fail(L"BeginStoryboard x:Name 重复："
								+ action.StoryboardName, error);
						beginNames.push_back(action.StoryboardName);
					}
					const auto storyboards = ChildElements(actionElement);
					if (storyboards.size() != 1
						|| !Equals(FromUtf8(storyboards.front()->LocalName()),
							L"Storyboard"))
						return Fail(L"BeginStoryboard 必须包含一个 Storyboard。",
							error);
					const auto& storyboard = storyboards.front();
					if (!ValidateAttributes(storyboard, {}, error)
						|| !Trim(FromUtf8(storyboard->InnerText())).empty())
						return false;
					struct PropertyOwnership
					{
						std::wstring RootProperty;
						bool Exclusive = false;
						std::vector<std::wstring> Paths;
					};
					std::vector<PropertyOwnership> properties;
					for (const auto& animationElement : ChildElements(storyboard))
					{
						DesignerVisualStateAnimation animation;
						StoryboardObjectPathKind objectPathKind =
							StoryboardObjectPathKind::None;
						if (!ParseStoryboardAnimation(animationElement, target,
							animation, objectPathKind, error)) return false;
						if (styleScope && !animation.TargetName.empty())
							return Fail(L"Style Storyboard 不支持 Storyboard.TargetName。",
								error);
						const auto rootProperty =
							StoryboardAnimationRootProperty(animation.PropertyName);
						auto owner = std::find_if(properties.begin(), properties.end(),
							[&](const auto& existing)
							{ return Equals(existing.RootProperty, rootProperty); });
						const bool exclusive = objectPathKind
							== StoryboardObjectPathKind::None;
						if (owner != properties.end())
						{
							if (exclusive || owner->Exclusive
								|| std::any_of(owner->Paths.begin(), owner->Paths.end(),
									[&](const auto& path)
									{ return Equals(path, animation.PropertyName); }))
								return Fail(L"BeginStoryboard 目标重复："
									+ animation.PropertyName, error);
							owner->Paths.push_back(animation.PropertyName);
						}
						else
						{
							PropertyOwnership ownership;
							ownership.RootProperty = rootProperty;
							ownership.Exclusive = exclusive;
							if (!exclusive)
								ownership.Paths.push_back(animation.PropertyName);
							properties.push_back(std::move(ownership));
						}
						action.Animations.push_back(std::move(animation));
					}
					if (action.Animations.empty())
						return Fail(L"BeginStoryboard 的 Storyboard 不能为空。",
							error);
				}
				else
				{
					if (Equals(actionName, L"PauseStoryboard"))
						action.Kind = DesignerStoryboardActionKind::Pause;
					else if (Equals(actionName, L"ResumeStoryboard"))
						action.Kind = DesignerStoryboardActionKind::Resume;
					else if (Equals(actionName, L"StopStoryboard"))
						action.Kind = DesignerStoryboardActionKind::Stop;
					else return Fail(L"TriggerAction 仅支持 Begin/Pause/Resume/StopStoryboard。",
							error);
					if (!ValidateAttributes(actionElement,
						{ L"BeginStoryboardName" }, error)
						|| !ChildElements(actionElement).empty()
						|| !Trim(FromUtf8(actionElement->InnerText())).empty())
						return false;
					action.StoryboardName = Trim(Attribute(actionElement,
						L"BeginStoryboardName").value_or(L""));
					if (!ValidateIdentifier(action.StoryboardName,
						L"BeginStoryboardName", error)) return false;
					referencedNames.push_back(action.StoryboardName);
				}
				actions.push_back(std::move(action));
			}
			if (actions.empty())
				return Fail(L"EnterActions/ExitActions 至少需要一个 TriggerAction。",
					error);
			return true;
		}

		bool ParseStyleTrigger(
			const Element& element,
			Control& probe,
			const DesignComponentDefinition& target,
			DesignerStyleTrigger& trigger,
			std::wstring& error)
		{
			if (!ParseStyleCondition(
				element, probe, trigger, false, error)) return false;
			bool foundEnterActions = false;
			bool foundExitActions = false;
			std::vector<std::wstring> beginNames;
			std::vector<std::wstring> referencedNames;
			for (const auto& child : ChildElements(element))
			{
				const auto childName = FromUtf8(child->LocalName());
				if (Equals(childName, L"Setter"))
				{
					DesignerStyleSetter setter;
					if (!ParseStyleSetter(child, probe, setter, error)) return false;
					trigger.Setters.push_back(std::move(setter));
				}
				else if (Equals(childName, L"Trigger.EnterActions"))
				{
					if (foundEnterActions)
						return Fail(L"Trigger.EnterActions 不能重复。", error);
					foundEnterActions = true;
					if (!ParseStoryboardActionCollection(child, target,
						trigger.EnterActions, beginNames, referencedNames,
						true, error)) return false;
				}
				else if (Equals(childName, L"Trigger.ExitActions"))
				{
					if (foundExitActions)
						return Fail(L"Trigger.ExitActions 不能重复。", error);
					foundExitActions = true;
					if (!ParseStoryboardActionCollection(child, target,
						trigger.ExitActions, beginNames, referencedNames,
						true, error)) return false;
				}
				else return Fail(L"Trigger 仅支持 Setter、EnterActions 和 ExitActions。",
					error);
			}
			if (trigger.Setters.empty() && trigger.EnterActions.empty()
				&& trigger.ExitActions.empty())
				return Fail(L"Trigger 至少需要一个 Setter 或 TriggerAction。", error);
			for (const auto& name : referencedNames)
				if (std::none_of(beginNames.begin(), beginNames.end(),
					[&](const auto& candidate) { return Equals(candidate, name); }))
					return Fail(L"Storyboard 控制动作找不到 BeginStoryboard："
						+ name, error);
			return true;
		}

		bool ParseStyleMultiTrigger(
			const Element& element,
			Control& probe,
			const DesignComponentDefinition& target,
			DesignerStyleTrigger& trigger,
			std::wstring& error)
		{
			DiagnosticContext context(*this, element);
			if (!ValidateAttributes(element, {}, error)) return false;
			bool foundConditions = false;
			bool foundEnterActions = false;
			bool foundExitActions = false;
			std::vector<std::wstring> beginNames;
			std::vector<std::wstring> referencedNames;
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
				if (Equals(childName, L"MultiTrigger.EnterActions")
					|| Equals(childName, L"MultiTrigger.ExitActions"))
				{
					const bool enter = Equals(childName, L"MultiTrigger.EnterActions");
					if ((enter && foundEnterActions) || (!enter && foundExitActions))
						return Fail(childName + L" 不能重复。", error);
					if (enter) foundEnterActions = true;
					else foundExitActions = true;
					if (!ParseStoryboardActionCollection(child, target,
						enter ? trigger.EnterActions : trigger.ExitActions,
						beginNames, referencedNames, true, error)) return false;
					continue;
				}
				if (!Equals(childName, L"MultiTrigger.Conditions"))
					return Fail(L"MultiTrigger 仅支持 Conditions、Setter、EnterActions 和 ExitActions。",
						error);
				if (foundConditions)
					return Fail(L"MultiTrigger.Conditions 不能重复。", error);
				foundConditions = true;
				if (!ValidateAttributes(child, {}, error)) return false;
				for (const auto& conditionElement : ChildElements(child))
				{
					if (!Equals(FromUtf8(conditionElement->LocalName()), L"Condition"))
						return Fail(L"MultiTrigger.Conditions 仅支持 Condition。", error);
					if (!ParseStyleCondition(
						conditionElement, probe, trigger, true, error)) return false;
				}
			}
			if (trigger.Conditions.size() + trigger.PropertyConditions.size() < 2)
				return Fail(L"MultiTrigger 至少需要两个 Condition。", error);
			if (trigger.Setters.empty() && trigger.EnterActions.empty()
				&& trigger.ExitActions.empty())
				return Fail(L"MultiTrigger 至少需要一个 Setter 或 TriggerAction。", error);
			for (const auto& name : referencedNames)
				if (std::none_of(beginNames.begin(), beginNames.end(),
					[&](const auto& candidate) { return Equals(candidate, name); }))
					return Fail(L"Storyboard 控制动作找不到 BeginStoryboard："
						+ name, error);
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
				|| (binding.Mode != BindingMode::Default
					&& binding.Mode != BindingMode::OneWay)
				|| (binding.UpdateMode != DataSourceUpdateMode::Default
					&& binding.UpdateMode
						!= DataSourceUpdateMode::OnPropertyChanged)
				|| !binding.Converter.empty()
				|| !binding.ElementName.empty()
				|| binding.RelativeSource != DesignerBindingRelativeSource::None)
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
			const DesignComponentDefinition& target,
			DesignerStyleTrigger& trigger,
			std::wstring& error)
		{
			DesignerStyleDataCondition condition;
			if (!ParseStyleDataCondition(
				element, condition, false, error)) return false;
			trigger.DataConditions.push_back(std::move(condition));
			bool foundEnterActions = false;
			bool foundExitActions = false;
			std::vector<std::wstring> beginNames;
			std::vector<std::wstring> referencedNames;
			for (const auto& child : ChildElements(element))
			{
				const auto childName = FromUtf8(child->LocalName());
				if (Equals(childName, L"Setter"))
				{
					DesignerStyleSetter setter;
					if (!ParseStyleSetter(child, probe, setter, error)) return false;
					trigger.Setters.push_back(std::move(setter));
				}
				else if (Equals(childName, L"DataTrigger.EnterActions"))
				{
					if (foundEnterActions)
						return Fail(L"DataTrigger.EnterActions 不能重复。", error);
					foundEnterActions = true;
					if (!ParseStoryboardActionCollection(child, target,
						trigger.EnterActions, beginNames, referencedNames,
						true, error)) return false;
				}
				else if (Equals(childName, L"DataTrigger.ExitActions"))
				{
					if (foundExitActions)
						return Fail(L"DataTrigger.ExitActions 不能重复。", error);
					foundExitActions = true;
					if (!ParseStoryboardActionCollection(child, target,
						trigger.ExitActions, beginNames, referencedNames,
						true, error)) return false;
				}
				else return Fail(L"DataTrigger 仅支持 Setter、EnterActions 和 ExitActions。",
					error);
			}
			if (trigger.Setters.empty() && trigger.EnterActions.empty()
				&& trigger.ExitActions.empty())
				return Fail(L"DataTrigger 至少需要一个 Setter 或 TriggerAction。",
					error);
			for (const auto& name : referencedNames)
				if (std::none_of(beginNames.begin(), beginNames.end(),
					[&](const auto& candidate) { return Equals(candidate, name); }))
					return Fail(L"Storyboard 控制动作找不到 BeginStoryboard："
						+ name, error);
			return true;
		}

		bool ParseStyleMultiDataTrigger(
			const Element& element,
			Control& probe,
			const DesignComponentDefinition& target,
			DesignerStyleTrigger& trigger,
			std::wstring& error)
		{
			DiagnosticContext context(*this, element);
			if (!ValidateAttributes(element, {}, error)) return false;
			bool foundConditions = false;
			bool foundEnterActions = false;
			bool foundExitActions = false;
			std::vector<std::wstring> beginNames;
			std::vector<std::wstring> referencedNames;
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
				if (Equals(childName, L"MultiDataTrigger.EnterActions"))
				{
					if (foundEnterActions)
						return Fail(L"MultiDataTrigger.EnterActions 不能重复。", error);
					foundEnterActions = true;
					if (!ParseStoryboardActionCollection(child, target,
						trigger.EnterActions, beginNames, referencedNames,
						true, error)) return false;
					continue;
				}
				if (Equals(childName, L"MultiDataTrigger.ExitActions"))
				{
					if (foundExitActions)
						return Fail(L"MultiDataTrigger.ExitActions 不能重复。", error);
					foundExitActions = true;
					if (!ParseStoryboardActionCollection(child, target,
						trigger.ExitActions, beginNames, referencedNames,
						true, error)) return false;
					continue;
				}
				if (!Equals(childName, L"MultiDataTrigger.Conditions"))
					return Fail(L"MultiDataTrigger 仅支持 Conditions、Setter、EnterActions 和 ExitActions。",
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
			if (trigger.Setters.empty() && trigger.EnterActions.empty()
				&& trigger.ExitActions.empty())
				return Fail(L"MultiDataTrigger 至少需要一个 Setter 或 TriggerAction。",
					error);
			for (const auto& name : referencedNames)
				if (std::none_of(beginNames.begin(), beginNames.end(),
					[&](const auto& candidate) { return Equals(candidate, name); }))
					return Fail(L"Storyboard 控制动作找不到 BeginStoryboard："
						+ name, error);
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
				const auto typeToken = MarkupTypeToken(*target);
				const auto typeName = StripMarkupType(typeToken);
				if (!Equals(typeName, L"Any"))
				{
					const auto separator = typeToken.find(L':');
					const DesignComponentDefinition* component = nullptr;
					if (separator != std::wstring::npos && separator > 0
						&& separator + 1 < typeToken.size())
					{
						const auto prefix = Trim(typeToken.substr(0, separator));
						const auto localName = Trim(typeToken.substr(separator + 1));
						component = FindVisibleComponent(
							LookupNamespaceUri(element, prefix), localName);
					}
					if (component)
					{
						rule.Type = component->BaseType;
						rule.ComponentType = component->Type;
					}
					else if (!TryParseType(typeName, rule.Type))
						return Fail(L"Style TargetType 无效：" + typeToken, error);
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
				auto probeSheet = VisibleStyleSheet();
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
			DesignComponentDefinition styleTarget;
			styleTarget.BaseType = effectiveRule.HasType
				? effectiveRule.Type : UIClass::UI_Base;
			if (!effectiveRule.ComponentType.Empty())
			{
				const auto* component = FindVisibleComponent(
					effectiveRule.ComponentType);
				if (!component || !DefineComponentProbeProperties(
					*probe, *component, error)) return false;
				styleTarget = *component;
			}
			bool foundTriggers = false;
			for (const auto& child : ChildElements(element))
			{
				DiagnosticContext childContext(*this, child);
				const auto childName = FromUtf8(child->LocalName());
				if (Equals(childName, L"Setter"))
				{
					DesignerStyleSetter setter;
					if (!ParseStyleSetter(
						child, *probe, setter, error, false, true)) return false;
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
							triggerElement, *probe, styleTarget,
							trigger, error)) return false;
					}
					else if (Equals(triggerName, L"MultiTrigger"))
					{
						if (!ParseStyleMultiTrigger(
							triggerElement, *probe, styleTarget,
							trigger, error)) return false;
					}
					else if (Equals(triggerName, L"DataTrigger"))
					{
						if (!ParseStyleDataTrigger(
							triggerElement, *probe, styleTarget,
							trigger, error)) return false;
					}
					else if (!ParseStyleMultiDataTrigger(
						triggerElement, *probe, styleTarget,
						trigger, error)) return false;
					rule.Triggers.push_back(std::move(trigger));
				}
			}
			if (rule.Setters.empty() && rule.BasedOn.empty()
				&& rule.Triggers.size() == 1
				&& rule.Triggers.front().Conditions.empty()
				&& (!rule.Triggers.front().DataConditions.empty()
					|| !rule.Triggers.front().PropertyConditions.empty()))
			{
				rule.DataConditions = std::move(
					rule.Triggers.front().DataConditions);
				rule.PropertyConditions = std::move(
					rule.Triggers.front().PropertyConditions);
				rule.Setters = std::move(rule.Triggers.front().Setters);
				rule.EnterActions = std::move(
					rule.Triggers.front().EnterActions);
				rule.ExitActions = std::move(
					rule.Triggers.front().ExitActions);
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
			else if (Equals(typeName, L"ListBoxItem")) typeName = L"SelectorItem";
			return DesignerStyleSheetUtils::TryParseUIClass(typeName, type);
		}

		bool ResolveBindingAncestorType(
			const Element& element,
			DesignerDataBinding& binding,
			std::wstring& error)
		{
			if (binding.RelativeSource != DesignerBindingRelativeSource::FindAncestor)
				return true;
			auto token = Trim(binding.AncestorType);
			const auto separator = token.find(L':');
			const auto prefix = separator == std::wstring::npos
				? std::wstring{} : token.substr(0, separator);
			const auto localName = separator == std::wstring::npos
				? token : token.substr(separator + 1);
			if (localName.empty() || token.find(L':', separator == std::wstring::npos
				? 0 : separator + 1) != std::wstring::npos)
				return Fail(L"RelativeSource AncestorType 不是有效的 XAML 类型名："
					+ token, error);

			UIClass builtIn = UIClass::UI_Base;
			const auto namespaceUri = prefix.empty()
				? std::wstring{} : LookupNamespaceUri(element, prefix);
			if ((prefix.empty() || Equals(namespaceUri, L"urn:cui"))
				&& TryParseType(localName, builtIn))
			{
				binding.AncestorType = DesignerStyleSheetUtils::UIClassName(builtIn);
				binding.AncestorTypeNamespace.clear();
				return true;
			}
			if (prefix.empty() || namespaceUri.empty())
				return Fail(L"RelativeSource AncestorType 无法解析：" + token, error);
			const DesignComponentDefinition* component = nullptr;
			if (_activeTemplateComponent
				&& Equals(_activeTemplateComponent->Type.XamlNamespace, namespaceUri)
				&& Equals(_activeTemplateComponent->Type.XamlName, localName))
				component = _activeTemplateComponent;
			else component = FindVisibleComponent(namespaceUri, localName);
			if (!component)
				return Fail(L"RelativeSource AncestorType 引用了未声明的组件："
					+ token, error);
			binding.AncestorType = component->Type.XamlPrefix + L":"
				+ component->Type.XamlName;
			binding.AncestorTypeNamespace = component->Type.XamlNamespace;
			return true;
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

		bool ParseBindingObjectElement(
			const Element& element,
			DesignerDataBinding& binding,
			std::wstring& error)
		{
			DiagnosticContext context(*this, element);
			if (!ValidateAttributes(element,
				{ L"Path", L"Mode", L"UpdateMode", L"UpdateSourceTrigger",
				  L"Converter", L"ConverterParameter", L"StringFormat",
				  L"ElementName", L"FallbackValue", L"TargetNullValue",
				  L"RelativeSource" }, error)) return false;
			if (!ChildElements(element).empty())
				return Fail(L"Binding 子项不能包含子元素。", error);
			if (Attribute(element, L"UpdateMode")
				&& Attribute(element, L"UpdateSourceTrigger"))
				return Fail(L"Binding 子项不能同时声明 UpdateMode 与 UpdateSourceTrigger。", error);
			std::wstring markup = L"{Binding Path="
				+ Attribute(element, L"Path").value_or(L"");
			auto appendRaw = [&](const wchar_t* name)
			{
				if (const auto value = Attribute(element, name))
					markup += L", " + std::wstring(name) + L"=" + *value;
			};
			auto appendLiteral = [&](const wchar_t* name)
			{
				if (const auto value = Attribute(element, name))
				{
					auto escaped = *value;
					size_t position = 0;
					while ((position = escaped.find(L'\'', position))
						!= std::wstring::npos)
					{
						escaped.insert(position, 1, L'\'');
						position += 2;
					}
					markup += L", " + std::wstring(name) + L"='"
						+ escaped + L"'";
				}
			};
			appendRaw(L"Mode");
			appendRaw(Attribute(element, L"UpdateSourceTrigger")
				? L"UpdateSourceTrigger" : L"UpdateMode");
			appendRaw(L"Converter");
			appendRaw(L"ElementName");
			appendRaw(L"RelativeSource");
			appendLiteral(L"ConverterParameter");
			appendLiteral(L"StringFormat");
			appendLiteral(L"FallbackValue");
			appendLiteral(L"TargetNullValue");
			markup += L"}";
			std::wstring bindingError;
			if (!TryParseBinding(markup, binding, bindingError))
				return Fail(bindingError.empty()
					? L"Binding 子项无效。" : bindingError, error);
			return true;
		}

		bool TryParseMultiBindingProperty(
			const Element& propertyElement,
			size_t nodeIndex,
			Control& probe,
			bool& handled,
			std::wstring& error)
		{
			handled = false;
			const auto propertyElementName = FromUtf8(
				propertyElement->LocalName());
			const auto separator = propertyElementName.find(L'.');
			if (separator == std::wstring::npos) return true;
			const auto children = ChildElements(propertyElement);
			if (children.size() != 1
				|| !Equals(FromUtf8(children.front()->LocalName()), L"MultiBinding"))
				return true;
			handled = true;
			DiagnosticContext propertyContext(*this, propertyElement);
			if (!ValidateAttributes(propertyElement, {}, error)) return false;
			const auto propertyName = NormalizePropertyName(
				propertyElementName.substr(separator + 1), L"");
			const auto* metadata = probe.FindPropertyMetadata(propertyName);
			if (!metadata)
				return Fail(L"MultiBinding 目标属性不存在：" + propertyName, error);
			auto& node = _document.Nodes[nodeIndex];
			const auto propertyKey = ToUtf8(metadata->Name());
			if (node.Bindings.contains(propertyKey)
				|| (node.Props.contains("metadata")
					&& node.Props["metadata"].contains(propertyKey)))
				return Fail(L"属性重复：" + metadata->Name(), error);

			const auto& multiElement = children.front();
			DiagnosticContext multiContext(*this, multiElement);
			if (!ValidateAttributes(multiElement,
				{ L"Mode", L"UpdateMode", L"UpdateSourceTrigger", L"Converter",
				  L"ConverterParameter", L"StringFormat", L"FallbackValue",
				  L"TargetNullValue" }, error)) return false;
			if (Attribute(multiElement, L"UpdateMode")
				&& Attribute(multiElement, L"UpdateSourceTrigger"))
				return Fail(L"MultiBinding 不能同时声明 UpdateMode 与 UpdateSourceTrigger。", error);
			DesignerDataBinding binding;
			if (const auto mode = Attribute(multiElement, L"Mode"); mode
				&& !DesignerBindingUtils::TryParseBindingMode(*mode, binding.Mode))
				return Fail(L"MultiBinding Mode 无效：" + *mode, error);
			if (const auto update = Attribute(multiElement,
				Attribute(multiElement, L"UpdateSourceTrigger")
					? L"UpdateSourceTrigger" : L"UpdateMode"))
			{
				auto normalized = *update;
				if (Equals(normalized, L"PropertyChanged")) normalized = L"OnPropertyChanged";
				else if (Equals(normalized, L"LostFocus")
					|| Equals(normalized, L"Validation")) normalized = L"OnValidation";
				else if (Equals(normalized, L"Explicit")) normalized = L"Never";
				if (!DesignerBindingUtils::TryParseUpdateMode(
					normalized, binding.UpdateMode))
					return Fail(L"MultiBinding UpdateSourceTrigger 无效："
						+ *update, error);
			}
			binding.Converter = Attribute(
				multiElement, L"Converter").value_or(L"");
			if (const auto value = Attribute(multiElement, L"ConverterParameter"))
				binding.ConverterParameter = DesignerStyleValue{
					DesignerStyleValueKind::String, *value };
			if (const auto value = Attribute(multiElement, L"StringFormat"))
				binding.StringFormat = *value;
			if (const auto value = Attribute(multiElement, L"FallbackValue"))
				binding.FallbackValue = DesignerStyleValue{
					DesignerStyleValueKind::String, *value };
			if (const auto value = Attribute(multiElement, L"TargetNullValue"))
				binding.TargetNullValue = DesignerStyleValue{
					DesignerStyleValueKind::String, *value };

			for (const auto& child : ChildElements(multiElement))
			{
				if (!Equals(FromUtf8(child->LocalName()), L"Binding"))
					return Fail(L"MultiBinding 只能包含 Binding 子项。", error);
				const bool hasMode = Attribute(child, L"Mode").has_value();
				const bool hasUpdateMode = Attribute(child, L"UpdateMode").has_value()
					|| Attribute(child, L"UpdateSourceTrigger").has_value();
				DesignerDataBinding childBinding;
				if (!ParseBindingObjectElement(child, childBinding, error)) return false;
				if (!hasMode) childBinding.Mode = binding.Mode;
				if (!hasUpdateMode) childBinding.UpdateMode = binding.UpdateMode;
				if (!DesignerBindingUtils::IsValidSourcePath(
					childBinding.SourceProperty))
					return Fail(L"MultiBinding 子项 Path 无效："
						+ childBinding.SourceProperty, error);
				if (!ResolveBindingAncestorType(child, childBinding, error))
					return false;
				binding.ChildBindings.push_back(std::move(childBinding));
			}
			if (binding.ChildBindings.size() < 2)
				return Fail(L"MultiBinding 至少需要两个 Binding 子项。", error);
			if (!binding.Converter.empty())
			{
				if (binding.StringFormat
					&& !IsValidBindingStringFormat(*binding.StringFormat))
					return Fail(L"MultiBinding StringFormat 语法无效。", error);
			}
			else if (!binding.StringFormat
				|| !IsValidMultiBindingStringFormat(
					*binding.StringFormat, binding.ChildBindings.size()))
				return Fail(L"MultiBinding 需要 Converter 或有效的 StringFormat。", error);
			if (binding.StringFormat
				&& metadata->ValueKind() != BindingValueKind::String)
				return Fail(L"MultiBinding StringFormat 只能用于字符串目标属性："
					+ metadata->Name(), error);
			const auto effectiveMode = ::ResolveBindingMode(*metadata, binding.Mode);
			if ((effectiveMode == BindingMode::TwoWay
				|| effectiveMode == BindingMode::OneWayToSource)
				&& binding.Converter.empty())
				return Fail(L"可回写的 MultiBinding 必须声明 Converter。", error);

			DesignerStyleValueKind targetValueKind{};
			if ((binding.FallbackValue || binding.TargetNullValue)
				&& !DesignerPropertyCatalog::TryGetStyleValueKind(
					*metadata, targetValueKind))
				return Fail(L"属性 " + metadata->Name()
					+ L" 的类型暂不支持 MultiBinding 缺省值。", error);
			auto normalizeDefault = [&](auto& literal, const wchar_t* option)
			{
				if (!literal) return true;
				literal->Kind = targetValueKind;
				std::wstring literalError;
				if (DesignerPropertyCatalog::ValidateStyleValue(
					probe, metadata->Name(), *literal, &literalError,
					_currentResourceBasePath, _document.Resources)) return true;
				return Fail(L"MultiBinding " + std::wstring(option)
					+ L" 无法转换到属性 " + metadata->Name()
					+ L"：" + literalError, error);
			};
			if (!normalizeDefault(binding.FallbackValue, L"FallbackValue")
				|| !normalizeDefault(binding.TargetNullValue, L"TargetNullValue"))
				return false;
			std::wstring targetError;
			if (!DesignerBindingUtils::ValidateTarget(
				DesignerBindingUtils::ProjectTargetMetadata(*metadata),
				binding, &targetError))
				return Fail(L"属性 " + metadata->Name()
					+ L" 的 MultiBinding 无效：" + targetError, error);
			node.Bindings[propertyKey] =
				DesignerBindingUtils::WriteBindingDefinition(binding);
			return true;
		}

		bool ParseEventTriggers(
			const Element& element,
			DesignComponentDefinition& component,
			std::wstring& error)
		{
			DiagnosticContext context(*this, element);
			if (!ValidateAttributes(element, {}, error)
				|| !Trim(FromUtf8(element->InnerText())).empty())
				return Fail(L"模板根 Triggers 不允许属性或文本内容。", error);
			std::vector<std::wstring> beginNames;
			std::vector<std::wstring> referencedNames;
			for (const auto& triggerElement : ChildElements(element))
			{
				DiagnosticContext triggerContext(*this, triggerElement);
				if (!Equals(FromUtf8(triggerElement->LocalName()), L"EventTrigger"))
					return Fail(L"模板根 Triggers 仅支持 EventTrigger。", error);
				if (!ValidateAttributes(triggerElement, { L"RoutedEvent" }, error)
					|| !Trim(FromUtf8(triggerElement->InnerText())).empty()) return false;
				DesignerEventTrigger trigger;
				trigger.EventName = Trim(Attribute(
					triggerElement, L"RoutedEvent").value_or(L""));
				const auto event = std::find_if(component.Events.begin(),
					component.Events.end(), [&](const auto& candidate)
					{ return Equals(candidate.Name, trigger.EventName); });
				if (event == component.Events.end())
					return Fail(L"EventTrigger.RoutedEvent 不是组件声明事件："
						+ trigger.EventName, error);
				trigger.EventName = event->Name;
				for (const auto& actionElement : ChildElements(triggerElement))
				{
					DiagnosticContext actionContext(*this, actionElement);
					const auto actionName = FromUtf8(actionElement->LocalName());
					DesignerEventTriggerAction action;
					if (Equals(actionName, L"BeginStoryboard"))
					{
						action.Kind = DesignerStoryboardActionKind::Begin;
						if (!ValidateAttributes(actionElement, { L"Name" }, error, true)
							|| !Trim(FromUtf8(actionElement->InnerText())).empty())
							return false;
						action.StoryboardName = Trim(Attribute(
							actionElement, L"Name", L"x").value_or(
								Attribute(actionElement, L"Name").value_or(L"")));
						if (!action.StoryboardName.empty())
						{
							if (!ValidateIdentifier(action.StoryboardName,
								L"BeginStoryboard x:Name", error)) return false;
							if (std::any_of(beginNames.begin(), beginNames.end(),
								[&](const auto& existing)
								{ return Equals(existing, action.StoryboardName); }))
								return Fail(L"BeginStoryboard x:Name 重复："
									+ action.StoryboardName, error);
							beginNames.push_back(action.StoryboardName);
						}
						const auto storyboards = ChildElements(actionElement);
						if (storyboards.size() != 1
							|| !Equals(FromUtf8(storyboards.front()->LocalName()),
								L"Storyboard"))
							return Fail(L"BeginStoryboard 必须包含一个 Storyboard。",
								error);
						const auto& storyboard = storyboards.front();
						if (!ValidateAttributes(storyboard, {}, error)
							|| !Trim(FromUtf8(storyboard->InnerText())).empty())
							return false;
						struct StoryboardPropertyOwnership
						{
							std::wstring TargetName;
							std::wstring RootProperty;
							bool Exclusive = false;
							std::vector<std::wstring> Paths;
						};
						std::vector<StoryboardPropertyOwnership> properties;
						for (const auto& animationElement : ChildElements(storyboard))
						{
							DesignerVisualStateAnimation animation;
							StoryboardObjectPathKind objectPathKind =
								StoryboardObjectPathKind::None;
							if (!ParseStoryboardAnimation(animationElement, component,
								animation, objectPathKind, error)) return false;
							const auto rootProperty =
								StoryboardAnimationRootProperty(animation.PropertyName);
							auto owner = std::find_if(properties.begin(),
								properties.end(), [&](const auto& existing)
								{
									return Equals(existing.TargetName,
										animation.TargetName)
										&& Equals(existing.RootProperty, rootProperty);
								});
							const bool exclusive = objectPathKind
								== StoryboardObjectPathKind::None;
							if (owner != properties.end())
							{
								if (exclusive || owner->Exclusive
									|| std::any_of(owner->Paths.begin(), owner->Paths.end(),
										[&](const auto& path)
										{ return Equals(path, animation.PropertyName); }))
									return Fail(L"BeginStoryboard 目标重复："
										+ animation.PropertyName, error);
								owner->Paths.push_back(animation.PropertyName);
							}
							else
							{
								StoryboardPropertyOwnership ownership;
								ownership.TargetName = animation.TargetName;
								ownership.RootProperty = rootProperty;
								ownership.Exclusive = exclusive;
								if (!exclusive)
									ownership.Paths.push_back(animation.PropertyName);
								properties.push_back(std::move(ownership));
							}
							action.Animations.push_back(std::move(animation));
						}
						if (action.Animations.empty())
							return Fail(L"BeginStoryboard 的 Storyboard 不能为空。",
								error);
					}
					else
					{
						if (Equals(actionName, L"PauseStoryboard"))
							action.Kind = DesignerStoryboardActionKind::Pause;
						else if (Equals(actionName, L"ResumeStoryboard"))
							action.Kind = DesignerStoryboardActionKind::Resume;
						else if (Equals(actionName, L"StopStoryboard"))
							action.Kind = DesignerStoryboardActionKind::Stop;
						else return Fail(L"EventTrigger 仅支持 Begin/Pause/Resume/StopStoryboard。",
								error);
						if (!ValidateAttributes(actionElement,
							{ L"BeginStoryboardName" }, error)
							|| !ChildElements(actionElement).empty()
							|| !Trim(FromUtf8(actionElement->InnerText())).empty())
							return false;
						action.StoryboardName = Trim(Attribute(actionElement,
							L"BeginStoryboardName").value_or(L""));
						if (!ValidateIdentifier(action.StoryboardName,
							L"BeginStoryboardName", error)) return false;
						referencedNames.push_back(action.StoryboardName);
					}
					trigger.Actions.push_back(std::move(action));
				}
				if (trigger.Actions.empty())
					return Fail(L"EventTrigger 至少需要一个 TriggerAction。", error);
				component.EventTriggers.push_back(std::move(trigger));
			}
			if (component.EventTriggers.empty())
				return Fail(L"模板根 Triggers 至少需要一个 EventTrigger。", error);
			for (const auto& name : referencedNames)
				if (std::none_of(beginNames.begin(), beginNames.end(),
					[&](const auto& candidate) { return Equals(candidate, name); }))
					return Fail(L"Storyboard 控制动作找不到 BeginStoryboard："
						+ name, error);
			return true;
		}

		bool ParseControl(
			const Element& element,
			const Parent& parent,
			std::wstring& error,
			const std::string& forcedSplitRegion = {},
			bool forcedHeader = false)
		{
			DiagnosticContext context(*this, element);
			const auto elementName = FromUtf8(element->LocalName());
			UIClass type = UIClass::UI_Base;
			DesignerComponentType componentType;
			const auto elementNamespace = FromUtf8(element->NamespaceURI());
			const bool builtInType = TryParseType(elementName, type)
				&& type != UIClass::UI_SelectorItem
				&& type != UIClass::UI_ComboBoxItem
				&& type != UIClass::UI_TreeViewItem
				&& (elementNamespace.empty()
					|| Equals(elementNamespace, L"urn:cui"));
			if (!builtInType)
			{
				if (const auto* component = FindVisibleComponent(
					elementNamespace, elementName))
				{
					componentType = component->Type;
					type = component->BaseType;
				}
				else
				{
					return Fail(L"未声明的 XAML 组件类型：" + elementName
						+ L"。请先在资源中定义 ComponentDefinition。", error);
				}
			}
			else if (type == UIClass::UI_TabPage)
				return Fail(L"不支持的控件元素：" + elementName, error);
			if (type == UIClass::UI_ItemsPresenter)
			{
				const auto targetType = _activeControlTemplateProbe
					? _activeControlTemplateProbe->Type() : UIClass::UI_Base;
				if (!_parsingControlTemplateVisual
					|| (targetType != UIClass::UI_ItemsControl
						&& targetType != UIClass::UI_ListBox))
					return Fail(L"ItemsPresenter 只能出现在 ItemsControl 或 ListBox "
						L"的 ControlTemplate 中。", error);
				if (std::any_of(_document.Nodes.begin(), _document.Nodes.end(),
					[](const auto& candidate)
					{ return candidate.Type == UIClass::UI_ItemsPresenter; }))
					return Fail(L"一个 ControlTemplate 最多只能包含一个 ItemsPresenter。",
						error);
			}
			DesignNode node;
			if (!ReadControlIdentity(element, type, node.Name, node.Id, error)) return false;
			node.Type = type;
			node.ComponentType = std::move(componentType);
			node.ParentId = parent.Id;
			node.ParentRef = parent.Ref;
			node.Order = SiblingCount(parent);
			if (!forcedSplitRegion.empty()) node.Extra["splitRegion"] = forcedSplitRegion;
			if (forcedHeader) node.Extra["headeredRegion"] = "header";
			std::unique_ptr<Control> probe;
			probe = DesignDocumentMaterializer::CreateRuntimeControl(type);
			if (!probe)
				return Fail(L"控件类型尚无运行时工厂：" + elementName, error);
			if (!node.ComponentType.Empty())
			{
				const auto* component = FindVisibleComponent(node.ComponentType);
				if (!component || !DefineComponentProbeProperties(
					*probe, *component, error)) return false;
			}
			_document.Nodes.push_back(std::move(node));
			const size_t nodeIndex = _document.Nodes.size() - 1;
			const auto elementChildren = ChildElements(element);
			auto isResourcesProperty = [&](const std::wstring& name)
			{
				if (Equals(name, L"Resources")) return true;
				const auto separator = name.rfind(L'.');
				if (separator == std::wstring::npos
					|| !Equals(name.substr(separator + 1), L"Resources"))
					return false;
				const auto owner = name.substr(0, separator);
				return Equals(owner, elementName)
					|| Equals(owner, L"Control")
					|| Equals(owner, L"FrameworkElement")
					|| Equals(owner, L"UIElement");
			};
			Element resourcesElement;
			for (const auto& child : elementChildren)
			{
				const auto childName = FromUtf8(child->LocalName());
				if (!isResourcesProperty(childName)) continue;
				if (resourcesElement)
					return Fail(L"控件 Resources 属性元素不能重复。", error);
				resourcesElement = child;
			}
			if (resourcesElement)
			{
				if (!ValidateAttributes(resourcesElement, {}, error)) return false;
				DesignerStyleSheet localResources;
				DesignObjectResourceDictionary localObjectResources;
				auto* previousTarget = _resourceTarget;
				auto* previousObjectTarget = _objectResourceTarget;
				const bool previousLocal = _parsingLocalResources;
				_resourceTarget = &localResources;
				_objectResourceTarget = &localObjectResources;
				_parsingLocalResources = true;
				const bool parsed = ParseResources(resourcesElement, error);
				auto restoreTargets = [&]()
				{
					_resourceTarget = previousTarget;
					_objectResourceTarget = previousObjectTarget;
					_parsingLocalResources = previousLocal;
				};
				if (!parsed)
				{
					restoreTargets();
					return false;
				}
				DesignerStyleSheetUtils::Canonicalize(localResources);
				auto visibleLocalStyles = VisibleStyleSheet(&localResources);
				if (!DesignerStyleSheetUtils::Validate(
					visibleLocalStyles, &error, _document.ResourceBasePath,
					_document.Resources))
				{
					restoreTargets();
					return Fail(error, error);
				}
				if (!DesignerStyleSheetUtils::ValidateAgainstRulePropertyMetadata(
					visibleLocalStyles,
					[&](const DesignerStyleRule& rule) -> std::unique_ptr<Control>
					{
						auto control = DesignDocumentMaterializer::CreateRuntimeControl(
							rule.HasType ? rule.Type : UIClass::UI_Base);
						if (!control || rule.ComponentType.Empty()) return control;
						const auto* component = FindVisibleComponent(
							rule.ComponentType);
						std::wstring ignored;
						if (!component || !DesignDocumentMaterializer::
							InstallComponentContract(
								*control, *component, _document, &ignored)) return nullptr;
						return control;
					}, &error, _document.ResourceBasePath,
					_document.Resources))
				{
					restoreTargets();
					return Fail(error, error);
				}
				restoreTargets();
				_document.Nodes[nodeIndex].LocalResources =
					std::move(localResources);
				_document.Nodes[nodeIndex].LocalObjectResources =
					std::move(localObjectResources);
			}
			LexicalResourceScope resourceScope(
				*this, _document.Nodes[nodeIndex].LocalResources);
			LexicalObjectResourceScope objectResourceScope(
				*this, _document.Nodes[nodeIndex].LocalObjectResources);

			if (!ParseControlAttributes(element, nodeIndex, *probe, error)) return false;
			if (!ApplyDirectText(element, nodeIndex, *probe, error)) return false;

			const Parent childParent{
				_document.Nodes[nodeIndex].Id,
				_document.Nodes[nodeIndex].Name };
			const auto* instanceComponent = _document.Nodes[nodeIndex].ComponentType.Empty()
				? nullptr : FindVisibleComponent(_document.Nodes[nodeIndex].ComponentType);
			auto contentCount = [&](const std::wstring& contentName)
			{
				return std::count_if(_document.Nodes.begin(), _document.Nodes.end(),
					[&](const auto& candidate)
					{
						return candidate.ParentId == childParent.Id
							&& Equals(candidate.ComponentContentProperty, contentName);
					});
			};
			auto appendContent = [&](const Element& content,
				const DesignerComponentContentPropertyDescriptor& contract) -> bool
			{
				if (contract.Cardinality == DesignerComponentContentCardinality::Single
					&& contentCount(contract.Name) != 0)
					return Fail(L"组件单值内容属性重复：" + contract.Name, error);
				const auto rootIndex = _document.Nodes.size();
				if (!ParseControl(content, childParent, error)) return false;
				if (rootIndex >= _document.Nodes.size()) return false;
				_document.Nodes[rootIndex].ComponentContentProperty = contract.Name;
				return true;
			};
			for (const auto& child : elementChildren)
			{
				DiagnosticContext childContext(*this, child);
				const auto childName = FromUtf8(child->LocalName());
				if (isResourcesProperty(childName)) continue;
				if (_activeTemplateComponent
					&& parent.Id == 0 && parent.Ref.empty()
					&& Equals(childName,
						L"VisualStateManager.VisualStateGroups"))
				{
					if (_pendingVisualStateGroups)
						return Fail(L"VisualStateManager.VisualStateGroups 不能重复。",
							error);
					_pendingVisualStateGroups = child;
					continue;
				}
				const auto triggerSeparator = childName.rfind(L'.');
				if (_activeTemplateComponent
					&& parent.Id == 0 && parent.Ref.empty()
					&& triggerSeparator != std::wstring::npos
					&& Equals(childName.substr(triggerSeparator + 1), L"Triggers")
					&& Equals(childName.substr(0, triggerSeparator),
						FromUtf8(element->LocalName())))
				{
					if (_pendingEventTriggers)
						return Fail(L"模板根 Triggers 不能重复。", error);
					_pendingEventTriggers = child;
					continue;
				}
				if (instanceComponent && !instanceComponent->ContentProperties.empty()
					&& Equals(FromUtf8(child->NamespaceURI()),
						instanceComponent->Type.XamlNamespace))
				{
					const auto separator = childName.find(L'.');
					if (separator != std::wstring::npos
						&& Equals(childName.substr(0, separator),
							instanceComponent->Type.XamlName))
					{
						const auto contentName = childName.substr(separator + 1);
						const auto contract = std::find_if(
							instanceComponent->ContentProperties.begin(),
							instanceComponent->ContentProperties.end(),
							[&](const auto& content)
							{
								return Equals(content.Name, contentName);
							});
						if (contract == instanceComponent->ContentProperties.end())
							return Fail(L"组件实例引用了未声明的内容属性：" + contentName, error);
						if (!ValidateAttributes(child, {}, error))
							return false;
						const auto roots = ChildElements(child);
						if (contract->Cardinality == DesignerComponentContentCardinality::Single
							&& roots.size() != 1)
							return Fail(L"Single 组件内容属性必须包含一个视觉根："
								+ contract->Name, error);
						for (const auto& root : roots)
							if (!appendContent(root, *contract)) return false;
						continue;
					}
				}
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
				if (IsHeaderedContentControlType(type)
					&& (Equals(childName,
						DesignerStyleSheetUtils::UIClassName(type) + L".Header")
						|| Equals(childName, L"HeaderedContentControl.Header")))
				{
					if (!ValidateAttributes(child, {}, error)) return false;
					auto& current = _document.Nodes[nodeIndex];
					const bool alreadyAssigned = current.Bindings.contains("Header")
						|| (current.Extra.is_object()
							&& (current.Extra.contains("headerText")
								|| current.Extra.contains("headerTemplate")))
						|| std::any_of(_document.Nodes.begin(), _document.Nodes.end(),
							[&](const auto& candidate)
							{
								return candidate.ParentId == childParent.Id
									&& candidate.ParentRef == childParent.Ref
									&& candidate.Extra.is_object()
									&& candidate.Extra.value(
										"headeredRegion", std::string{}) == "header";
							});
					if (alreadyAssigned)
						return Fail(L"属性重复：Header", error);
					const auto roots = ChildElements(child);
					const auto text = DirectText(child);
					if (roots.size() > 1 || (!roots.empty() && !text.empty()))
						return Fail(L"Header 属性元素只能包含一个视觉根或一个文本值。", error);
					if (!roots.empty())
					{
						if (!ParseControl(roots.front(), childParent, error, {}, true))
							return false;
					}
					else current.Extra["headerText"] = ToUtf8(text);
					continue;
				}
				if (IsContentHostType(type))
				{
					const auto owner = DesignerStyleSheetUtils::UIClassName(type);
					if (Equals(childName, owner + L".Content")
						|| Equals(childName, L"ContentControl.Content"))
					{
						if (!ValidateAttributes(child, {}, error)) return false;
						const auto roots = ChildElements(child);
						const auto text = DirectText(child);
						if (roots.size() > 1 || (!roots.empty() && !text.empty()))
							return Fail(L"Content 属性元素只能包含一个视觉根或一个文本值。", error);
						if (!roots.empty())
						{
							if (type == UIClass::UI_ContentPresenter)
								return Fail(L"ContentPresenter 不接受视觉 Content。", error);
							if (!ParseControl(roots.front(), childParent, error)) return false;
						}
						else
						{
							auto& current = _document.Nodes[nodeIndex];
							if (current.Bindings.contains("Content")
								|| current.Extra.contains("contentText"))
								return Fail(L"属性重复：Content", error);
							current.Extra["contentText"] = ToUtf8(text);
						}
						continue;
					}
				}
				bool bindingProperty = false;
				if (!TryParseMultiBindingProperty(
					child, nodeIndex, *probe, bindingProperty, error)) return false;
				if (bindingProperty) continue;
				bool structuredProperty = false;
				if (!TryParseContentItem(
					child, nodeIndex, type, structuredProperty, error)) return false;
				if (structuredProperty) continue;
				if (!TryParseStructuredProperty(
					child, nodeIndex, type, structuredProperty, error)) return false;
				if (structuredProperty) continue;
				if (childName.find(L'.') != std::wstring::npos)
					return Fail(L"不支持的控件属性元素：" + childName, error);
				if (type == UIClass::UI_ContentPresenter)
					return Fail(L"ContentPresenter 不接受直接视觉子节点；"
						L"请使用 Content 和 ContentTemplate。", error);
				if (type == UIClass::UI_ItemsPresenter)
					return Fail(L"ItemsPresenter 的视觉子节点由 ItemsPanelTemplate 生成，"
						L"不能手工声明。", error);
				if (instanceComponent && !instanceComponent->Template.empty())
				{
					const auto contract = std::find_if(
						instanceComponent->ContentProperties.begin(),
						instanceComponent->ContentProperties.end(),
						[](const auto& content) { return content.IsDefault; });
					if (contract == instanceComponent->ContentProperties.end())
						return Fail(L"带 Template 的组件没有可接收直接视觉内容的默认属性。", error);
					if (!appendContent(child, *contract)) return false;
					continue;
				}
				if (!ParseControl(child, childParent, error)) return false;
			}
			if (IsVisualContentControlType(type))
			{
				const auto childCount = std::count_if(
					_document.Nodes.begin(), _document.Nodes.end(),
					[&](const auto& candidate)
					{
						return candidate.ParentId == childParent.Id
							&& candidate.ParentRef == childParent.Ref
							&& (!candidate.Extra.is_object()
								|| candidate.Extra.value(
									"headeredRegion", std::string{}) != "header");
					});
				if (childCount > 1)
					return Fail(L"内容控件最多接受一个直接视觉子节点。", error);
				const auto& current = _document.Nodes[nodeIndex];
				const bool hasDataContent = current.Bindings.is_object()
					&& current.Bindings.contains("Content");
				const bool hasTextContent = current.Extra.is_object()
					&& current.Extra.contains("contentText");
				const bool hasTemplate = current.Extra.is_object()
					&& current.Extra.contains("contentTemplate");
				if (childCount != 0
					&& (hasDataContent || hasTextContent || hasTemplate))
					return Fail(L"内容控件的直接视觉内容不能与 Content 或 ContentTemplate 同时使用。",
						error);
				if (hasTextContent && hasTemplate)
					return Fail(L"文本 Content 当前不能使用 DataTemplate。", error);
			}
			if (IsHeaderedContentControlType(type))
			{
				const auto headerCount = std::count_if(
					_document.Nodes.begin(), _document.Nodes.end(),
					[&](const auto& candidate)
					{
						return candidate.ParentId == childParent.Id
							&& candidate.ParentRef == childParent.Ref
							&& candidate.Extra.is_object()
							&& candidate.Extra.value(
								"headeredRegion", std::string{}) == "header";
					});
				const auto& current = _document.Nodes[nodeIndex];
				const bool hasHeaderBinding = current.Bindings.is_object()
					&& current.Bindings.contains("Header");
				const bool hasHeaderText = current.Extra.is_object()
					&& current.Extra.contains("headerText");
				const bool hasHeaderTemplate = current.Extra.is_object()
					&& current.Extra.contains("headerTemplate");
				if (headerCount > 1 || (headerCount != 0
					&& (hasHeaderBinding || hasHeaderText || hasHeaderTemplate)))
					return Fail(L"HeaderedContentControl 最多接受一个视觉 Header，"
						L"且不能与 Header 或 HeaderTemplate 同时使用。", error);
				if (hasHeaderText && hasHeaderTemplate)
					return Fail(L"文本 Header 当前不能使用 DataTemplate。", error);
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
				const bool supportsItemsSource =
					_document.Nodes[nodeIndex].Type == UIClass::UI_ItemsControl
					|| _document.Nodes[nodeIndex].Type == UIClass::UI_ComboBox
					|| _document.Nodes[nodeIndex].Type == UIClass::UI_ListView
					|| _document.Nodes[nodeIndex].Type == UIClass::UI_ListBox
					|| _document.Nodes[nodeIndex].Type == UIClass::UI_TreeView;
				if (supportsItemsSource && Equals(name, L"ItemsSource"))
				{
					std::wstring resourceKey;
					if (TryParseStaticResource(value, resourceKey))
					{
						if (!assignedProperties.insert(L"itemssource").second)
							return Fail(L"属性重复：ItemsSource", error);
						_document.Nodes[nodeIndex].Extra["itemsSourceResource"] =
							ToUtf8(resourceKey);
						continue;
					}
				}
				if ((_document.Nodes[nodeIndex].Type == UIClass::UI_ItemsControl
					|| _document.Nodes[nodeIndex].Type == UIClass::UI_ListBox
					|| _document.Nodes[nodeIndex].Type == UIClass::UI_ComboBox
					|| _document.Nodes[nodeIndex].Type == UIClass::UI_TreeView)
					&& Equals(name, L"ItemTemplate"))
				{
					std::wstring resourceKey;
					if (!TryParseStaticResource(value, resourceKey))
						return Fail(L"ItemTemplate 必须引用已声明的 DataTemplate："
							+ value, error);
					const auto* definition = FindVisibleDataTemplate(resourceKey);
					if (!definition)
						return Fail(L"ItemTemplate 引用了未声明的 DataTemplate："
							+ resourceKey, error);
					if (!assignedProperties.insert(L"itemtemplate").second)
						return Fail(L"属性重复：ItemTemplate", error);
					_document.Nodes[nodeIndex].Extra["itemTemplate"] =
						ToUtf8(definition->Key);
					continue;
				}
				if ((IsControlTemplateHostType(_document.Nodes[nodeIndex].Type)
					|| !_document.Nodes[nodeIndex].ComponentType.Empty())
					&& Equals(name, L"Template"))
				{
					std::wstring resourceKey;
					if (!TryParseStaticResource(value, resourceKey))
						return Fail(L"Control.Template 必须引用已声明的 ControlTemplate："
							+ value, error);
					const auto* definition = FindVisibleControlTemplate(resourceKey);
					if (!definition)
						return Fail(L"Control.Template 引用了未声明的 ControlTemplate："
							+ resourceKey, error);
					if (!IsControlTemplateTargetCompatible(
						_document.Nodes[nodeIndex], *definition))
						return Fail(L"Control.Template TargetType 与控件类型不兼容："
							+ definition->DisplayName(), error);
					if (!assignedProperties.insert(L"template").second)
						return Fail(L"属性重复：Template", error);
					_document.Nodes[nodeIndex].Extra["controlTemplate"] =
						ToUtf8(definition->Key);
					continue;
				}
				if (IsContentHostType(_document.Nodes[nodeIndex].Type)
					&& Equals(name, L"ContentTemplate"))
				{
					std::wstring resourceKey;
					if (!TryParseStaticResource(value, resourceKey))
						return Fail(L"ContentPresenter.ContentTemplate 必须引用已声明的 DataTemplate："
							+ value, error);
					const auto* definition = FindVisibleDataTemplate(resourceKey);
					if (!definition)
						return Fail(L"ContentPresenter.ContentTemplate 引用了未声明的 DataTemplate："
							+ resourceKey, error);
					if (!assignedProperties.insert(L"contenttemplate").second)
						return Fail(L"属性重复：ContentTemplate", error);
					_document.Nodes[nodeIndex].Extra["contentTemplate"] =
						ToUtf8(definition->Key);
					continue;
				}
				if (IsHeaderedContentControlType(_document.Nodes[nodeIndex].Type)
					&& Equals(name, L"HeaderTemplate"))
				{
					std::wstring resourceKey;
					if (!TryParseStaticResource(value, resourceKey))
						return Fail(L"HeaderTemplate 必须引用已声明的 DataTemplate："
							+ value, error);
					const auto* definition = FindVisibleDataTemplate(resourceKey);
					if (!definition)
						return Fail(L"HeaderTemplate 引用了未声明的 DataTemplate："
							+ resourceKey, error);
					if (!assignedProperties.insert(L"headertemplate").second)
						return Fail(L"属性重复：HeaderTemplate", error);
					_document.Nodes[nodeIndex].Extra["headerTemplate"] =
						ToUtf8(definition->Key);
					continue;
				}
				if ((_document.Nodes[nodeIndex].Type == UIClass::UI_ItemsControl
					|| _document.Nodes[nodeIndex].Type == UIClass::UI_ListBox)
					&& Equals(name, L"GroupStyle"))
				{
					std::wstring resourceKey;
					if (!TryParseStaticResource(value, resourceKey))
						return Fail(L"ItemsControl.GroupStyle 必须引用已声明的 GroupStyle："
							+ value, error);
					const auto* definition = FindVisibleGroupStyle(resourceKey);
					if (!definition)
						return Fail(L"ItemsControl.GroupStyle 引用了未声明的 GroupStyle："
							+ resourceKey, error);
					if (!assignedProperties.insert(L"groupstyle").second)
						return Fail(L"属性重复：GroupStyle", error);
					_document.Nodes[nodeIndex].Extra["groupStyle"] =
						ToUtf8(definition->Key);
					continue;
				}
				if ((_document.Nodes[nodeIndex].Type == UIClass::UI_ItemsControl
					|| _document.Nodes[nodeIndex].Type == UIClass::UI_ListBox)
					&& Equals(name, L"ItemsPanel"))
				{
					std::wstring resourceKey;
					if (!TryParseStaticResource(value, resourceKey))
						return Fail(L"ItemsControl.ItemsPanel 必须引用已声明的 ItemsPanelTemplate："
							+ value, error);
					const auto* definition = FindVisibleItemsPanelTemplate(resourceKey);
					if (!definition)
						return Fail(L"ItemsControl.ItemsPanel 引用了未声明的 ItemsPanelTemplate："
							+ resourceKey, error);
					if (!assignedProperties.insert(L"itemspanel").second)
						return Fail(L"属性重复：ItemsPanel", error);
					_document.Nodes[nodeIndex].Extra["itemsPanel"] =
						ToUtf8(definition->Key);
					continue;
				}
				if ((_document.Nodes[nodeIndex].Type == UIClass::UI_ListBox
					|| _document.Nodes[nodeIndex].Type == UIClass::UI_ComboBox
					|| _document.Nodes[nodeIndex].Type == UIClass::UI_TreeView)
					&& Equals(name, L"ItemContainerStyle"))
				{
					std::wstring resourceKey;
					if (!TryParseStaticResource(value, resourceKey))
						return Fail(L"ItemContainerStyle 必须引用已声明的 Style："
							+ value, error);
					if (!assignedProperties.insert(L"itemcontainerstyle").second)
						return Fail(L"属性重复：ItemContainerStyle", error);
					_document.Nodes[nodeIndex].Extra["itemContainerStyle"] =
						ToUtf8(resourceKey);
					continue;
				}
				if (_document.Nodes[nodeIndex].Type == UIClass::UI_ContentPresenter
					&& Equals(name, L"ContentSource"))
				{
					if (!_parsingControlTemplateVisual
						|| !_activeControlTemplateProbe)
						return Fail(L"ContentPresenter.ContentSource 只能出现在 ControlTemplate 中。",
							error);
					const auto requested = Trim(value);
					std::wstring canonical;
					std::array<std::pair<const wchar_t*, const wchar_t*>, 3> aliases{};
					if (Equals(requested, L"Content"))
					{
						canonical = L"Content";
						aliases = { {
							{ L"Content", L"Content" },
							{ L"ContentTemplate", L"ContentTemplate" },
							{ L"DisplayMemberPath", L"DisplayMemberPath" }
						} };
					}
					else if (Equals(requested, L"Header")
						&& IsHeaderedContentControlType(
							_activeControlTemplateProbe->Type()))
					{
						canonical = L"Header";
						aliases = { {
							{ L"Content", L"Header" },
							{ L"ContentTemplate", L"HeaderTemplate" },
							{ L"DisplayMemberPath", L"HeaderDisplayMemberPath" }
						} };
					}
					else return Fail(L"ContentSource 必须是 Content；"
						L"Header 仅适用于 GroupBox 或 Expander 模板。", error);

					for (const auto& [targetName, sourceName] : aliases)
					{
						const auto* target = probe.FindPropertyMetadata(targetName);
						const auto* source = _activeControlTemplateProbe
							->FindPropertyMetadata(sourceName);
						BindingValue sourceDefault;
						BindingValue compatible;
						if (!target || !target->CanWrite() || !source || !source->CanRead()
							|| (!source->TryGetDefaultValue(sourceDefault)
								&& !source->TryGet(*_activeControlTemplateProbe,
									sourceDefault))
							|| !target->TryConvert(sourceDefault, compatible))
							return Fail(L"ContentSource 无法建立模板别名："
								+ std::wstring(sourceName) + L" -> " + targetName,
								error);
						if (!assignedProperties.insert(Lower(target->Name())).second)
							return Fail(L"ContentSource 与显式属性重复："
								+ target->Name(), error);
					}
					if (std::any_of(_document.Nodes.begin(), _document.Nodes.end(),
						[&](const auto& node)
						{
							return !node.TemplateContentSource.empty()
								&& Equals(node.TemplateContentSource, canonical);
						}))
						return Fail(L"ControlTemplate 的 " + canonical
							+ L" 只能拥有一个 ContentPresenter。", error);
					_document.Nodes[nodeIndex].TemplateContentSource = canonical;
					continue;
				}
				if (Equals(rawName, L"ComponentSlot.Presents"))
				{
					if (!_activeTemplateComponent)
						return Fail(L"ComponentSlot.Presents 只能出现在组件模板中。", error);
					const auto slotName = Trim(value);
					const auto slot = std::find_if(
						_activeTemplateComponent->ContentProperties.begin(),
						_activeTemplateComponent->ContentProperties.end(),
						[&](const auto& content) { return Equals(content.Name, slotName); });
					if (slot == _activeTemplateComponent->ContentProperties.end())
						return Fail(L"ComponentSlot.Presents 引用了未声明的内容属性："
							+ slotName, error);
					const auto& hostNode = _document.Nodes[nodeIndex];
					const auto hostType = hostNode.Type;
					if (!hostNode.ComponentType.Empty())
						return Fail(L"ComponentSlot.Presents 不能标记另一个声明组件实例。", error);
					if (hostType != UIClass::UI_Panel
						&& hostType != UIClass::UI_StackPanel
						&& hostType != UIClass::UI_WrapPanel
						&& hostType != UIClass::UI_DockPanel
						&& hostType != UIClass::UI_GridPanel
						&& hostType != UIClass::UI_RelativePanel)
						return Fail(L"ComponentSlot.Presents 只能标记布局容器。", error);
					if (std::any_of(_document.Nodes.begin(), _document.Nodes.end(),
						[&](const auto& node)
						{
							return !node.PresentedComponentContent.empty()
								&& Equals(node.PresentedComponentContent, slotName);
						}))
						return Fail(L"组件内容属性只能拥有一个 Presenter：" + slotName, error);
					_document.Nodes[nodeIndex].PresentedComponentContent = slot->Name;
					continue;
				}

				std::wstring templateSource;
				std::wstring templateError;
				if (TryParseTemplateBinding(value, templateSource, templateError))
				{
					if (!_parsingComponentTemplateVisual
						&& !_parsingControlTemplateVisual)
						return Fail(L"TemplateBinding 只能出现在组件或控件模板中。", error);
					const auto propertyName = NormalizePropertyName(name, value);
					const auto* target = probe.FindPropertyMetadata(propertyName);
					if (!target || !target->CanWrite())
						return Fail(L"TemplateBinding 目标属性不可写或不存在：" + name, error);

					std::wstring canonicalSource = templateSource;
					BindingValue sourceDefault;
					if (_parsingControlTemplateVisual
						&& _activeControlTemplateProbe)
					{
						const auto* source = _activeControlTemplateProbe
							->FindPropertyMetadata(templateSource);
						if (!source || !source->CanRead())
							return Fail(L"TemplateBinding 引用了 TargetType 不存在或不可读的属性："
								+ templateSource, error);
						canonicalSource = source->Name();
						if (!source->TryGetDefaultValue(sourceDefault)
							&& !source->TryGet(*_activeControlTemplateProbe, sourceDefault))
							return Fail(L"TemplateBinding 无法读取 TargetType 属性默认值："
								+ canonicalSource, error);
					}
					else
					{
						const auto source = std::find_if(
							_activeTemplateComponent->Properties.begin(),
							_activeTemplateComponent->Properties.end(),
							[&](const auto& property)
							{ return Equals(property.Name, templateSource); });
						if (source == _activeTemplateComponent->Properties.end())
							return Fail(L"TemplateBinding 引用了未声明的组件属性："
								+ templateSource, error);
						canonicalSource = source->Name;
						const DesignerStyleValue* sourceValue = &source->DefaultValue;
						if (!source->DefaultResourceKey.empty())
						{
							const auto* resource = FindVisibleResource(
								source->DefaultResourceKey);
							if (!resource)
								return Fail(L"TemplateBinding 默认资源不存在："
									+ source->DefaultResourceKey, error);
							sourceValue = &resource->Value;
						}
						std::wstring conversionError;
						if (!DesignerStyleSheetUtils::TryConvertValue(
							*sourceValue, sourceDefault, &conversionError,
							_currentResourceBasePath, _document.Resources))
							return Fail(L"TemplateBinding 源属性默认值无效："
								+ canonicalSource, error);
					}
					BindingValue compatible;
					if (!target->TryConvert(sourceDefault, compatible))
						return Fail(L"TemplateBinding 类型不兼容：" + canonicalSource
							+ L" -> " + target->Name(), error);
					if (!assignedProperties.insert(Lower(target->Name())).second)
						return Fail(L"属性重复：" + target->Name(), error);
					_document.Nodes[nodeIndex].TemplateBindings[
						target->Name()] = canonicalSource;
					continue;
				}
				if (!templateError.empty())
					return Fail(L"控件 " + _document.Nodes[nodeIndex].Name
						+ L" 的属性 " + name + L"：" + templateError, error);
				if (_document.Nodes[nodeIndex].Type == UIClass::UI_MediaPlayer
					&& Equals(name, L"Source"))
				{
					_document.Nodes[nodeIndex].Extra["mediaFile"] = ToUtf8(value);
					continue;
				}
				const auto* component = FindVisibleComponent(
					_document.Nodes[nodeIndex].ComponentType);
				if (!prefix.empty() && !Equals(prefix, L"x")
					&& !Equals(prefix, L"d"))
				{
					const auto separator = name.find(L'.');
					if (separator != std::wstring::npos
						&& separator != 0 && separator + 1 < name.size())
					{
						const auto ownerName = name.substr(0, separator);
						const auto routedEventName = name.substr(separator + 1);
						const auto ownerNamespace = FromUtf8(attribute->NamespaceURI());
						if (const auto* owner = FindVisibleComponent(
							ownerNamespace, ownerName))
						{
							const auto contract = std::find_if(
								owner->Events.begin(), owner->Events.end(),
								[&](const auto& candidate)
								{ return Equals(candidate.Name, routedEventName); });
							if (contract == owner->Events.end())
								return Fail(L"附加组件事件不存在：" + rawName, error);
							std::wstring handler;
							if (!NormalizeHandler(value, handler, error))
								return Fail(L"控件 " + _document.Nodes[nodeIndex].Name
									+ L" 的附加事件 " + rawName + L"：" + error, error);
							auto& events = _document.Nodes[nodeIndex].Events;
							const auto stableName =
								DesignerEventCatalog::MakeAttachedComponentEventKey(
									owner->Type, contract->Name);
							const auto key = ToUtf8(stableName);
							if (events.contains(key)
								|| (owner->Type == _document.Nodes[nodeIndex].ComponentType
									&& events.contains(ToUtf8(contract->Name))))
								return Fail(L"事件重复：" + rawName, error);
							events[key] = ToUtf8(handler);
							continue;
						}
					}
				}
				if (const auto event = FindEvent(
					_document.Nodes[nodeIndex].Type,
					component ? component->Events
						: std::vector<DesignerComponentEventDescriptor>{},
					name, value))
				{
					std::wstring raisedEvent;
					std::wstring raiseError;
					if (TryParseRaiseEvent(value, raisedEvent, raiseError))
					{
						if (!_activeTemplateComponent)
							return Fail(L"RaiseEvent 只能出现在 ComponentDefinition.Template 中。", error);
						const auto contract = std::find_if(
							_activeTemplateComponent->Events.begin(),
							_activeTemplateComponent->Events.end(),
							[&](const auto& candidate)
							{ return Equals(candidate.Name, raisedEvent); });
						if (contract == _activeTemplateComponent->Events.end())
							return Fail(L"RaiseEvent 引用了未声明的组件事件："
								+ raisedEvent, error);
						if (!CanForwardTemplateEvent(event->Name, contract->Payload))
							return Fail(L"当前模板事件转发不支持：" + event->Name
								+ L" -> " + contract->Name, error);
						auto& forwards =
							_document.Nodes[nodeIndex].TemplateEventBindings;
						if (!forwards.emplace(event->Name, contract->Name).second)
							return Fail(L"模板事件转发重复：" + event->Name, error);
						continue;
					}
					if (!raiseError.empty())
						return Fail(L"控件 " + _document.Nodes[nodeIndex].Name
							+ L" 的事件 " + event->Name + L"：" + raiseError, error);
					std::wstring handler;
					if (!NormalizeHandler(value, handler, error))
						return Fail(L"控件 " + _document.Nodes[nodeIndex].Name
							+ L" 的事件 " + event->Name + L"：" + error, error);
					auto& events = _document.Nodes[nodeIndex].Events;
					const auto key = ToUtf8(event->Name);
					const auto attachedKey = component
						? ToUtf8(DesignerEventCatalog::MakeAttachedComponentEventKey(
							component->Type, event->Name)) : std::string{};
					if (events.contains(key)
						|| (!attachedKey.empty() && events.contains(attachedKey)))
						return Fail(L"事件重复：" + event->Name, error);
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
					if (!ResolveBindingAncestorType(element, binding, error))
						return false;
					const auto* metadata = probe.FindPropertyMetadata(propertyName);
					if (!metadata)
						return Fail(L"绑定目标属性不存在：" + name, error);
					if (binding.StringFormat
						&& metadata->ValueKind() != BindingValueKind::String)
						return Fail(L"Binding StringFormat 只能用于字符串目标属性："
							+ metadata->Name(), error);
					DesignerStyleValueKind targetValueKind{};
					if ((binding.FallbackValue || binding.TargetNullValue)
						&& !DesignerPropertyCatalog::TryGetStyleValueKind(
							*metadata, targetValueKind))
						return Fail(L"属性 " + metadata->Name()
							+ L" 的类型暂不支持 Binding 缺省值。", error);
					auto normalizeBindingLiteral = [&](auto& literal,
						const wchar_t* optionName)
					{
						if (!literal) return true;
						literal->Kind = targetValueKind;
						std::wstring literalError;
						if (DesignerPropertyCatalog::ValidateStyleValue(
							probe, metadata->Name(), *literal, &literalError,
							_currentResourceBasePath, _document.Resources))
							return true;
						return Fail(L"Binding " + std::wstring(optionName)
							+ L" 无法转换到属性 " + metadata->Name()
							+ L"：" + literalError, error);
					};
					if (!normalizeBindingLiteral(binding.FallbackValue, L"FallbackValue")
						|| !normalizeBindingLiteral(binding.TargetNullValue,
							L"TargetNullValue")) return false;
					if (!DesignerBindingUtils::ValidateTarget(
						DesignerBindingUtils::ProjectTargetMetadata(*metadata),
						binding, &bindingError))
						return Fail(L"控件 " + _document.Nodes[nodeIndex].Name
							+ L" 的属性 " + metadata->Name()
							+ L" 绑定无效：" + bindingError, error);
					if (!assignedProperties.insert(Lower(metadata->Name())).second)
						return Fail(L"属性重复：" + metadata->Name(), error);
					_document.Nodes[nodeIndex].Bindings[ToUtf8(metadata->Name())] =
						DesignerBindingUtils::WriteBindingDefinition(binding);
					continue;
				}
				if (!bindingError.empty())
					return Fail(L"控件 " + _document.Nodes[nodeIndex].Name
						+ L" 的属性 " + name + L"：" + bindingError, error);
				if (IsContentHostType(_document.Nodes[nodeIndex].Type)
					&& Equals(name, L"Content"))
				{
					if (!value.empty() && value.front() == L'{')
						return Fail(L"Content 当前只支持文本字面量或 Binding。", error);
					if (!assignedProperties.insert(L"content").second)
						return Fail(L"属性重复：Content", error);
					_document.Nodes[nodeIndex].Extra["contentText"] = ToUtf8(value);
					continue;
				}
				if (IsHeaderedContentControlType(_document.Nodes[nodeIndex].Type)
					&& Equals(name, L"Header"))
				{
					if (!value.empty() && value.front() == L'{')
						return Fail(L"Header 当前只支持文本字面量或 Binding。", error);
					if (!assignedProperties.insert(L"header").second)
						return Fail(L"属性重复：Header", error);
					_document.Nodes[nodeIndex].Extra["headerText"] = ToUtf8(value);
					continue;
				}
				if (Equals(name, L"Visibility"))
				{
					bool recognized = false;
					propertyValue = NormalizeVisibility(value, recognized);
					if (!recognized) return Fail(L"Visibility 必须为 Visible、Hidden 或 Collapsed。", error);
				}

				const auto properties = DesignerPropertyCatalog::GetStyleProperties(probe);
				const auto* descriptor = DesignerPropertyCatalog::Find(properties, propertyName);
				if (!descriptor)
				{
					if (const auto* metadata = probe.FindPropertyMetadata(propertyName);
						metadata && metadata->IsReadOnly())
						return Fail(L"控件 " + _document.Nodes[nodeIndex].Name
							+ L" 的属性只读：" + name, error);
					return Fail(L"控件 " + _document.Nodes[nodeIndex].Name
						+ L" 不包含可持久化属性：" + name, error);
				}
				if (!assignedProperties.insert(Lower(descriptor->Name)).second)
					return Fail(L"属性重复：" + descriptor->Name, error);
				std::wstring resourceKey;
				std::wstring dynamicResourceKey;
				const DesignerStyleValue* resourceValue = nullptr;
				if (TryParseStaticResource(propertyValue, resourceKey))
				{
					const auto* resource = FindVisibleResource(resourceKey);
					if (!resource)
						return Fail(L"控件 " + _document.Nodes[nodeIndex].Name
							+ L" 的属性 " + name + L" 引用了不存在的资源："
							+ resourceKey, error);
					resourceValue = &resource->Value;
				}
				else if (TryParseDynamicResource(
					propertyValue, dynamicResourceKey))
				{
					if (const auto* resource = FindVisibleResource(dynamicResourceKey))
						resourceValue = &resource->Value;
				}
				if (!dynamicResourceKey.empty() && !resourceValue)
				{
					std::wstring canonical;
					DesignerStyleValue effective;
					std::wstring captureError;
					if (!DesignerPropertyCatalog::CaptureValue(
						probe, descriptor->Name, &canonical, effective,
						&captureError))
						return Fail(L"控件 " + _document.Nodes[nodeIndex].Name
							+ L" 的属性 " + name + L"：" + captureError, error);
					StoreMetadata(_document.Nodes[nodeIndex], canonical,
						effective, {}, dynamicResourceKey);
					continue;
				}
				DesignerStyleValue typed = resourceValue
					? *resourceValue
					: DesignerStyleValue{
						descriptor->ValueKind,
						NormalizePropertyText(name, propertyValue, *descriptor) };
				std::wstring canonical;
				DesignerStyleValue effective;
				std::wstring applyError;
				if (!DesignerPropertyCatalog::ApplyValue(
					probe, descriptor->Name, typed, &canonical, &effective, &applyError,
					_options.ResourceBasePath, _document.Resources))
					return Fail(L"控件 " + _document.Nodes[nodeIndex].Name
						+ L" 的属性 " + name + L"：" + applyError, error);
				StoreMetadata(
					_document.Nodes[nodeIndex], canonical, effective,
					resourceKey, dynamicResourceKey);
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
			auto enumValue = [&](const std::wstring& property,
				std::initializer_list<const wchar_t*> names)
				-> std::optional<std::wstring>
			{
				if (!Equals(descriptor.Name, property)) return std::nullopt;
				int value = 0;
				for (const auto* name : names)
				{
					if (Equals(name, Trim(rawValue))) return std::to_wstring(value);
					++value;
				}
				return std::nullopt;
			};
			if (const auto value = enumValue(
				L"HAlign", { L"Left", L"Center", L"Right", L"Stretch" }))
				return *value;
			if (const auto value = enumValue(
				L"VAlign", { L"Top", L"Center", L"Bottom", L"Stretch" }))
				return *value;
			if (const auto value = enumValue(
				L"DockPosition", { L"Left", L"Top", L"Right", L"Bottom", L"Fill" }))
				return *value;
			return rawValue;
		}

		static void StoreMetadata(
			DesignNode& node,
			const std::wstring& propertyName,
			const DesignerStyleValue& value,
			const std::wstring& resourceKey = {},
			const std::wstring& dynamicResourceKey = {})
		{
			auto& metadata = node.Props["metadata"];
			if (!metadata.is_object()) metadata = DesignValue::object();
			auto stored = DesignValue{
				{ "kind", ToUtf8(DesignerStyleSheetUtils::ValueKindName(value.Kind)) },
				{ "value", ToUtf8(value.Text) }
			};
			if (!value.ObjectValue.is_null())
				stored["object"] = value.ObjectValue;
			if (!resourceKey.empty())
				stored["resourceKey"] = ToUtf8(resourceKey);
			if (!dynamicResourceKey.empty())
				stored["dynamicResourceKey"] = ToUtf8(dynamicResourceKey);
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
			if (IsContentHostType(_document.Nodes[nodeIndex].Type))
			{
				if (_document.Nodes[nodeIndex].Bindings.contains("Content")
					|| (_document.Nodes[nodeIndex].Extra.is_object()
						&& _document.Nodes[nodeIndex].Extra.contains("contentText")))
					return Fail(L"属性重复：Content", error);
				_document.Nodes[nodeIndex].Extra["contentText"] = ToUtf8(text);
				return true;
			}
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
			if (!Equals(itemName, L"ListViewItem"))
				return Fail(L"列表项必须是 ListViewItem。", error);
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
				if (!Equals(childName, L"ListViewItem.SubItems")
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
			if (type == UIClass::UI_ListView && Equals(name, L"ListViewItem"))
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
			std::vector<Element> contentChildren;
			bool foundTransform = false;
			bool foundRelativeTransform = false;
			for (const auto& child : ChildElements(brush))
			{
				const auto childName = FromUtf8(child->LocalName());
				const auto separator = childName.rfind(L'.');
				const auto owner = separator == std::wstring::npos
					? std::wstring{} : childName.substr(0, separator);
				const auto property = separator == std::wstring::npos
					? std::wstring{} : childName.substr(separator + 1);
				const bool transformProperty = Equals(property, L"Transform");
				const bool relativeProperty = Equals(property, L"RelativeTransform");
				if (!transformProperty && !relativeProperty)
				{
					contentChildren.push_back(child);
					continue;
				}
				const bool gradient = Equals(name, L"LinearGradientBrush")
					|| Equals(name, L"RadialGradientBrush");
				if (!Equals(owner, L"Brush") && !Equals(owner, name)
					&& !(gradient && Equals(owner, L"GradientBrush")))
					return Fail(L"Brush 变换属性元素所有者与画刷类型不匹配："
						+ childName, error);
				auto& found = transformProperty ? foundTransform : foundRelativeTransform;
				if (found)
					return Fail(L"Brush.Transform/RelativeTransform 不能重复。", error);
				found = true;
				DesignValue transform;
				if (!ParseTransform(child, transform, error, childName)) return false;
				output[transformProperty ? "transform" : "relativeTransform"] =
					std::move(transform);
			}
			if (Equals(name, L"SolidColorBrush"))
			{
				if (!ValidateAttributes(brush, { L"Key", L"Color", L"Opacity" }, error))
					return false;
				if (!contentChildren.empty())
					return Fail(L"SolidColorBrush 仅允许 Transform 属性子元素。", error);
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
					  L"AlignmentX", L"AlignmentY", L"Opacity" }, error))
					return false;
				if (!contentChildren.empty())
					return Fail(L"ImageBrush 仅允许 Transform 属性子元素。", error);
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
					contentChildren, output["stops"], error)) return false;
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
			std::wstring& error,
			const std::wstring& contextName = L"Control.RenderTransform")
		{
			if (!ValidateAttributes(property, {}, error)) return false;
			const auto transforms = ChildElements(property);
			if (transforms.size() != 1)
				return Fail(contextName + L" 必须且只能包含一个变换对象。", error);
			output = DesignValue::array();
			if (!ParseTransformElement(transforms.front(), output, error)) return false;
			if (output.empty())
				return Fail(contextName + L" 不能是空 TransformGroup。", error);
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

		bool ParseTreeItem(
			const Element& item,
			DesignValue& value,
			std::wstring& error)
		{
			if (!Equals(FromUtf8(item->LocalName()), L"TreeViewItem"))
				return Fail(L"TreeView.Items 仅允许 TreeViewItem。", error);
			if (!ValidateAttributes(item, { L"Header", L"IsExpanded" }, error))
				return false;
			value = DesignValue::object();
			value["text"] = ToUtf8(Attribute(item, L"Header").value_or(L""));
			bool expanded = false;
			if (!ReadBoolAttribute(item, L"IsExpanded", false, expanded, error))
				return false;
			value["expand"] = expanded;
			bool hasExplicitItems = false;
			bool hasImplicitItems = false;
			for (const auto& child : ChildElements(item))
			{
				DiagnosticContext childContext(*this, child);
				const auto childName = FromUtf8(child->LocalName());
				if (Equals(childName, L"TreeViewItem.Items"))
				{
					if (hasExplicitItems || hasImplicitItems)
						return Fail(L"TreeViewItem 仅允许一个 TreeViewItem.Items。", error);
					hasExplicitItems = true;
					DesignValue children;
					if (!ParseTreeItems(child, children, error)) return false;
					if (!children.empty()) value["children"] = std::move(children);
					continue;
				}
				if (!Equals(childName, L"TreeViewItem"))
					return Fail(
						L"TreeViewItem 仅允许 TreeViewItem 或 TreeViewItem.Items。",
						error);
				if (hasExplicitItems)
					return Fail(
						L"TreeViewItem 不能混用隐式子项和 TreeViewItem.Items。",
						error);
				hasImplicitItems = true;
				if (!value.contains("children"))
					value["children"] = DesignValue::array();
				DesignValue nested;
				if (!ParseTreeItem(child, nested, error)) return false;
				value["children"].push_back(std::move(nested));
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
				DesignValue value;
				if (!ParseTreeItem(item, value, error)) return false;
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
			const bool list = type == UIClass::UI_ListView;
			const bool listColumns = list && Equals(name, L"ListView.Columns");
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
			const bool listItems = list && Equals(name, L"ListView.Items");
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
			std::vector<std::wstring> paths = _bindingPaths;
			std::unordered_map<int, size_t> byId;
			std::unordered_map<std::wstring, size_t> byName;
			for (size_t index = 0; index < _document.Nodes.size(); ++index)
			{
				byId.emplace(_document.Nodes[index].Id, index);
				byName.emplace(Lower(_document.Nodes[index].Name), index);
			}
			std::vector<std::optional<std::wstring>> prefixes(_document.Nodes.size());
			std::vector<unsigned char> state(_document.Nodes.size());
			auto join = [](const std::wstring& prefix, const std::wstring& path)
			{
				return prefix.empty() ? path : prefix + L"." + path;
			};
			std::function<std::optional<std::wstring>(size_t)> resolve;
			resolve = [&](size_t index) -> std::optional<std::wstring>
			{
				if (state[index] == 2) return prefixes[index];
				if (state[index] == 1) return std::nullopt;
				state[index] = 1;
				const auto& node = _document.Nodes[index];
				std::optional<std::wstring> inherited = std::wstring{};
				std::optional<size_t> parentIndex;
				if (node.ParentId > 0)
				{
					const auto found = byId.find(node.ParentId);
					if (found != byId.end()) parentIndex = found->second;
				}
				else if (!node.ParentRef.empty())
				{
					const auto found = byName.find(Lower(node.ParentRef));
					if (found != byName.end()) parentIndex = found->second;
				}
				if (parentIndex) inherited = resolve(*parentIndex);

				auto effective = inherited;
				if (node.Bindings.is_object())
				{
					for (const auto& [target, binding] : node.Bindings.ObjectItems())
					{
						if (!binding.is_object()) continue;
						if (Equals(FromUtf8(target), L"DataContext")
							&& binding.contains("bindings"))
						{
							effective.reset();
							continue;
						}
						const auto source = DesignerBindingUtils::Trim(FromUtf8(
							binding.value("source", std::string{})));
						const bool explicitSource = !binding.value(
							"elementName", std::string{}).empty()
							|| !binding.value("relativeSource", std::string{}).empty();
						if (Equals(FromUtf8(target), L"DataContext"))
						{
							if (!explicitSource && inherited)
							{
								paths.push_back(join(*inherited, source));
								effective = join(*inherited, source);
							}
							else effective.reset();
						}
					}
					for (const auto& [target, binding] : node.Bindings.ObjectItems())
					{
						if (Equals(FromUtf8(target), L"DataContext")
							|| !binding.is_object() || !effective) continue;
						(void)DesignerBindingUtils::VisitLeafBindingDefinitions(
							binding, [&](const DesignerModel::DesignValue& child)
							{
								if (!child.value("elementName", std::string{}).empty()
									|| !child.value("relativeSource", std::string{}).empty())
									return true;
								paths.push_back(join(*effective,
									DesignerBindingUtils::Trim(FromUtf8(
										child.value("source", std::string{})))));
								return true;
							});
					}
				}
				prefixes[index] = effective;
				state[index] = 2;
				return effective;
			};
			for (size_t index = 0; index < _document.Nodes.size(); ++index)
				(void)resolve(index);

			for (const auto& path : paths)
			{
				std::vector<BindingPathStep> pathSteps;
				if (!TryParseBindingPropertyPath(path, pathSteps)) continue;
				std::wstring schemaPrefix;
				for (const auto& step : pathSteps)
				{
					if (step.Kind == BindingPathStepKind::Indexer) break;
					if (!schemaPrefix.empty()) schemaPrefix += L'.';
					schemaPrefix += step.Value;
				}
				auto normalized = DesignerDataContextSchemaUtils::NormalizePath(
					schemaPrefix);
				if (normalized.empty()) continue;
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

		bool ValidateBindingSources(
			const std::vector<DesignNode>& nodes,
			const std::wstring& owner,
			bool allowTemplatedParent,
			std::wstring& error)
		{
			std::unordered_set<std::wstring> names;
			for (const auto& node : nodes) names.insert(node.Name);
			for (const auto& node : nodes)
			{
				if (!node.Bindings.is_object()) continue;
				for (const auto& [targetProperty, binding]
					: node.Bindings.ObjectItems())
				{
					std::wstring leafError;
					if (!DesignerBindingUtils::VisitLeafBindingDefinitions(
						binding, [&](const DesignerModel::DesignValue& child)
						{
							if (child.value("relativeSource", std::string{})
								== "TemplatedParent" && !allowTemplatedParent)
							{
								leafError = owner + L" 中控件 " + node.Name
									+ L" 的绑定 " + FromUtf8(targetProperty)
									+ L" 只能在组件模板内使用 TemplatedParent。";
								return false;
							}
							if (!child.contains("elementName")
								|| !child["elementName"].is_string()) return true;
							const auto sourceName = FromUtf8(
								child["elementName"].get<std::string>());
							if (sourceName.empty() || names.contains(sourceName)) return true;
							leafError = owner + L" 中控件 " + node.Name
								+ L" 的绑定 " + FromUtf8(targetProperty)
								+ L" 引用了当前 namescope 中不存在的 ElementName："
								+ sourceName;
							return false;
						}, &leafError))
						return Fail(leafError, error);
				}
			}
			return true;
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
