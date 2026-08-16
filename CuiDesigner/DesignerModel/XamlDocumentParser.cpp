#include "XamlDocumentParser.h"

#include "DesignDocumentGraph.h"
#include "DesignDocumentEventIndex.h"
#include "../../CuiRuntime/include/XamlRuntimeSchema.h"
#include "../../CUI/include/RichTextDocument.h"
#include "DesignDataResourceUtils.h"
#include "StoryboardPropertyPath.h"
#include "XamlSourceScanner.h"
#include "../../XmlLite/include/Xml.h"
#include "../DesignerBindingUtils.h"
#include "../DesignerDataContextSchemaUtils.h"
#include "../DesignerEventCatalog.h"
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
#include <tuple>
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
		if (diagnostic)
		{
			*diagnostic = {};
			diagnostic->Stage = XamlDiagnosticStage::Parse;
		}
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

	bool PreservesXmlWhitespace(const Element& element)
	{
		for (const XmlNode* node = element.get(); node != nullptr;
			node = node->ParentNode())
		{
			const auto* current = dynamic_cast<const XmlElement*>(node);
			if (!current) continue;
			if (!current->HasAttribute("xml:space")) continue;
			return current->GetAttribute("xml:space") == "preserve";
		}
		return false;
	}

	bool IsVisibleInlineContentNode(const std::shared_ptr<XmlNode>& node)
	{
		if (!node) return false;
		if (node->NodeType() == XmlNodeType::Element) return true;
		return (node->NodeType() == XmlNodeType::Text
			|| node->NodeType() == XmlNodeType::CDATA)
			&& !node->Value().empty();
	}

	bool IsCuiLineBreakNode(const std::shared_ptr<XmlNode>& node)
	{
		if (!node || node->NodeType() != XmlNodeType::Element) return false;
		const auto element = std::static_pointer_cast<XmlElement>(node);
		return element->NamespaceURI() == "urn:cui"
			&& element->LocalName() == "LineBreak";
	}

	bool IsNormalizedInlineSeparator(
		const Element& container, std::size_t index)
	{
		if (!container || index >= container->ChildNodes().size()) return false;
		const auto& node = container->ChildNodes()[index];
		if (!node || (node->NodeType() != XmlNodeType::Whitespace
			&& node->NodeType() != XmlNodeType::SignificantWhitespace))
			return false;
		const auto& value = node->Value();
		if (value.empty()
			|| value.find_first_of("\r\n") != std::string::npos)
			return false;

		bool before = false;
		for (std::size_t candidate = index; candidate > 0; --candidate)
		{
			const auto& sibling = container->ChildNodes()[candidate - 1];
			if (IsVisibleInlineContentNode(sibling))
			{
				if (IsCuiLineBreakNode(sibling)) return false;
				before = true;
				break;
			}
			if (sibling && (sibling->NodeType() == XmlNodeType::Whitespace
				|| sibling->NodeType()
					== XmlNodeType::SignificantWhitespace))
				return false;
			if (sibling && sibling->NodeType() != XmlNodeType::Comment
				&& sibling->NodeType() != XmlNodeType::Whitespace
				&& sibling->NodeType()
					!= XmlNodeType::SignificantWhitespace)
				break;
		}
		if (!before) return false;
		for (std::size_t candidate = index + 1;
			candidate < container->ChildNodes().size(); ++candidate)
		{
			const auto& sibling = container->ChildNodes()[candidate];
			if (IsVisibleInlineContentNode(sibling))
				return !IsCuiLineBreakNode(sibling);
			if (sibling && sibling->NodeType() != XmlNodeType::Comment
				&& sibling->NodeType() != XmlNodeType::Whitespace
				&& sibling->NodeType()
					!= XmlNodeType::SignificantWhitespace)
				return false;
		}
		return false;
	}

	std::wstring RawDirectText(const Element& element)
	{
		std::wstring result;
		if (!element) return result;
		for (const auto& child : element->ChildNodes())
			if (child && (child->NodeType() == XmlNodeType::Text
				|| child->NodeType() == XmlNodeType::CDATA
				|| (PreservesXmlWhitespace(element)
					&& (child->NodeType() == XmlNodeType::Whitespace
						|| child->NodeType()
							== XmlNodeType::SignificantWhitespace))))
				result += FromUtf8(child->Value());
		return result;
	}

	std::wstring DirectText(const Element& element)
	{
		return Trim(RawDirectText(element));
	}

	std::wstring Lower(std::wstring value)
	{
		std::transform(value.begin(), value.end(), value.begin(),
			[](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });
		return value;
	}

	bool Equals(const std::wstring& left, const std::wstring& right)
	{
		return left == right;
	}

	bool Equals(const std::string& left, const char* right)
	{
		return right && left == right;
	}

	bool IsContentHostType(UIClass type) noexcept
	{
		return type == UIClass::UI_ContentPresenter
			|| IsUIClassAssignableFrom(UIClass::UI_ContentControl, type);
	}

	bool IsVisualContentControlType(UIClass type) noexcept
	{
		return IsUIClassAssignableFrom(UIClass::UI_ContentControl, type);
	}

	bool IsSingleVisualChildHostType(UIClass type) noexcept
	{
		return type == UIClass::UI_Popup
			|| IsUIClassAssignableFrom(UIClass::UI_Decorator, type)
			|| IsVisualContentControlType(type);
	}

	bool IsControlTemplateHostType(UIClass type) noexcept
	{
		return IsControlTemplateHostClass(type);
	}

	bool IsHeaderedContentControlType(UIClass type) noexcept
	{
		return IsUIClassAssignableFrom(
			UIClass::UI_HeaderedContentControl, type)
			|| IsUIClassAssignableFrom(
				UIClass::UI_HeaderedItemsControl, type);
	}

	bool IsControlTemplateTargetCompatible(
		UIClass actual, UIClass target) noexcept
	{
		return IsUIClassAssignableFrom(target, actual);
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
			// Semantic diagnostics request a span for nearly every parsed node
			// and attribute. Index line starts once so those requests do not
			// repeatedly scan the complete prefix of an ever larger document.
			_lineStarts.reserve(
				static_cast<size_t>(std::count(
					_source.begin(), _source.end(), L'\n')) + 1);
			_lineStarts.push_back(0);
			for (size_t index = 0; index < _source.size(); ++index)
			{
				if (_source[index] == L'\r')
				{
					if (index + 1 < _source.size()
						&& _source[index + 1] == L'\n')
						++index;
					_lineStarts.push_back(index + 1);
				}
				else if (_source[index] == L'\n')
					_lineStarts.push_back(index + 1);
			}

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
			if (!diagnostic) return;
			diagnostic->Apply(Span(element, attribute));
		}

		XamlSourceSpan Span(
			const XmlElement* element,
			const XmlAttribute* attribute = nullptr) const
		{
			XamlSourceSpan result;
			if (!element) return result;
			const auto found = _tags.find(element);
			if (found == _tags.end()) return result;

			size_t offset = found->second.NameStart;
			size_t length = found->second.NameLength;
			if (attribute)
			{
				const auto attributeRange = FindAttributeRange(
					found->second, FromUtf8(attribute->Name()));
				if (attributeRange)
				{
					offset = attributeRange->first;
					length = attributeRange->second;
				}
			}
			XamlDocumentDiagnostic start;
			XamlDocumentDiagnostic end;
			PopulatePosition(offset, start);
			PopulatePosition(offset + length, end);
			result.Utf16Offset = offset;
			result.Utf16Length = length;
			result.Line = start.Line;
			result.Column = start.Column;
			result.EndLine = end.Line;
			result.EndColumn = end.Column;
			return result;
		}

	private:
		std::wstring _source;
		std::vector<size_t> _lineStarts;
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

		std::optional<std::pair<size_t, size_t>> FindAttributeRange(
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
				const bool matches = name == rawName;

				while (cursor < end && std::iswspace(_source[cursor])) cursor++;
				if (cursor >= end || _source[cursor] != L'=')
				{
					if (matches) return std::pair{ nameStart, cursor - nameStart };
					continue;
				}
				cursor++;
				while (cursor < end && std::iswspace(_source[cursor])) cursor++;
				if (cursor >= end
					|| (_source[cursor] != L'\'' && _source[cursor] != L'"'))
				{
					if (matches) return std::pair{ nameStart, cursor - nameStart };
					continue;
				}
				const wchar_t quote = _source[cursor++];
				while (cursor < end && _source[cursor] != quote) cursor++;
				if (cursor < end) cursor++;
				if (matches) return std::pair{ nameStart, cursor - nameStart };
			}
			return std::nullopt;
		}

		void PopulatePosition(
			size_t offset,
			XamlDocumentDiagnostic& diagnostic) const
		{
			offset = (std::min)(offset, _source.size());
			const auto nextLine = std::upper_bound(
				_lineStarts.begin(), _lineStarts.end(), offset);
			const auto lineIndex = nextLine == _lineStarts.begin()
				? size_t{ 0 }
				: static_cast<size_t>(
					std::distance(_lineStarts.begin(), nextLine) - 1);
			const size_t lineStart = _lineStarts.empty()
				? 0 : _lineStarts[lineIndex];
			diagnostic.Line = lineIndex + 1;
			diagnostic.Column = 1;
			diagnostic.Utf16Offset = offset;
			for (size_t i = lineStart; i < offset;)
			{
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
		if (normalized == L"true")
		{
			output = true;
			return true;
		}
		if (normalized == L"false")
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
			if (value.starts_with(L"x:Type"))
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

	bool TryParseCommandTargetReference(
		std::wstring value,
		std::wstring& targetName,
		std::wstring& error)
	{
		value = Trim(std::move(value));
		targetName.clear();
		if (value.empty()) return true;
		if (value.front() == L'{' || value.back() == L'}')
		{
			if (value.size() < 3 || value.front() != L'{' || value.back() != L'}')
			{
				error = L"CommandTarget 标记扩展缺少配对的大括号。";
				return false;
			}
			auto inner = Trim(value.substr(1, value.size() - 2));
			constexpr std::wstring_view prefix = L"x:Reference";
			if (!inner.starts_with(prefix)
				|| (inner.size() > prefix.size()
					&& !std::iswspace(inner[prefix.size()])))
			{
				error = L"CommandTarget 仅支持直接 x:Name 或 {x:Reference ...}。";
				return false;
			}
			inner = Trim(inner.substr(prefix.size()));
			if (const auto assignment = inner.find(L'=');
				assignment != std::wstring::npos)
			{
				if (Trim(inner.substr(0, assignment)) != L"Name")
				{
					error = L"CommandTarget x:Reference 只接受 Name 参数。";
					return false;
				}
				inner = Trim(inner.substr(assignment + 1));
			}
			if (!TryUnquoteMarkupArgument(std::move(inner), targetName))
			{
				error = L"CommandTarget x:Reference 引号无效。";
				return false;
			}
		}
		else targetName = std::move(value);
		targetName = Trim(std::move(targetName));
		if (targetName.empty()
			|| targetName.find_first_of(L".,={} \t\r\n") != std::wstring::npos)
		{
			error = L"CommandTarget 必须引用当前 namescope 中的直接 x:Name。";
			targetName.clear();
			return false;
		}
		return true;
	}

	std::wstring NormalizePropertyName(
		const std::wstring& rawName,
		const std::wstring& rawValue,
		bool formProperty = false)
	{
		(void)rawValue;
		(void)formProperty;
		return Trim(rawName);
	}

	bool TryNormalizeDirectStoryboardProperty(
		const std::wstring& rawProperty,
		const CuiRuntime::XamlTypePropertySchema& targetSchema,
		std::wstring& propertyName)
	{
		if (rawProperty.empty()) return false;
		if (rawProperty.front() != L'(')
		{
			propertyName = NormalizePropertyName(rawProperty, L"");
			return true;
		}

		cui::xaml::PropertyPath path;
		if (!cui::xaml::TryParsePropertyPath(rawProperty, path, nullptr)
			|| path.Segments.size() != 1
			|| path.Segments.front().Kind
				!= cui::xaml::PropertyPathSegmentKind::Property
			|| path.Segments.front().OwnerType.empty()) return false;

		const auto& segment = path.Segments.front();
		const auto candidate = StoryboardPathLocalType(segment.OwnerType)
			+ L"." + segment.Name;
		const auto normalized = NormalizePropertyName(candidate, L"");
		if (!targetSchema.FindProperty(normalized)) return false;
		propertyName = normalized;
		return true;
	}

	std::wstring NormalizeVisibility(const std::wstring& value, bool& recognized)
	{
		const auto normalized = Lower(Trim(value));
		if (normalized == L"visible")
		{
			recognized = true;
			return L"Visible";
		}
		if (normalized == L"hidden")
		{
			recognized = true;
			return L"Hidden";
		}
		if (normalized == L"collapsed")
		{
			recognized = true;
			return L"Collapsed";
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
		if (!text.starts_with(L"StaticResource")
			|| (text.size() > 14 && !std::iswspace(text[14]))) return false;
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
		if (!text.starts_with(L"DynamicResource")
			|| (text.size() > 15 && !std::iswspace(text[15]))) return false;
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
		if (!text.starts_with(L"Binding")
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

			const auto key = Trim(part.substr(0, equals));
			const auto itemValue = Trim(part.substr(equals + 1));
			if (key == L"Path") binding.SourceProperty = itemValue;
			else if (key == L"Mode")
			{
				if (!DesignerBindingUtils::TryParseBindingMode(itemValue, binding.Mode))
				{
					error = L"Binding Mode 无效：" + itemValue;
					return false;
				}
			}
			else if (key == L"UpdateSourceTrigger")
			{
				if (updateSourceTriggerSeen)
				{
					error = L"Binding 不能重复声明 UpdateSourceTrigger。";
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
			else if (key == L"Converter") binding.Converter = itemValue;
			else if (key == L"ConverterParameter")
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
			else if (key == L"StringFormat")
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
			else if (key == L"ElementName") binding.ElementName = itemValue;
			else if (key == L"FallbackValue")
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
			else if (key == L"TargetNullValue")
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
			else if (key == L"RelativeSource")
			{
				auto source = Trim(itemValue);
				if (source.size() >= 2 && source.front() == L'{'
					&& source.back() == L'}')
				{
					source = Trim(source.substr(1, source.size() - 2));
					if (!source.starts_with(L"RelativeSource"))
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
					const auto sourceKey = Trim(
						sourcePart.substr(0, sourceEquals));
					auto sourceValue = Trim(sourcePart.substr(sourceEquals + 1));
					if (sourceKey == L"Mode") mode = sourceValue;
					else if (sourceKey == L"AncestorType")
					{
						if (sourceValue.size() >= 2 && sourceValue.front() == L'{'
							&& sourceValue.back() == L'}')
						{
							sourceValue = Trim(sourceValue.substr(
								1, sourceValue.size() - 2));
							if (!sourceValue.starts_with(L"x:Type")
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
					else if (sourceKey == L"AncestorLevel")
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

	bool TryParseDataGridColumnBinding(
		const std::wstring& value,
		DesignerDataBinding& binding,
		std::wstring& error)
	{
		auto text = Trim(value);
		if (text.size() < 3 || text.front() != L'{' || text.back() != L'}')
		{
			error = L"DataGrid 列 Binding 必须使用 {Binding ...}。";
			return false;
		}
		text = Trim(text.substr(1, text.size() - 2));
		if (!text.starts_with(L"Binding")
			|| (text.size() > 7 && std::iswspace(text[7]) == 0
				&& text[7] != L','))
		{
			error = L"DataGrid 列 Binding 必须使用 {Binding ...}。";
			return false;
		}
		text = Trim(text.substr(7));
		bool positionalPath = false;
		std::unordered_set<std::wstring> namedArguments;
		for (const auto& rawPart : SplitMarkupArguments(text))
		{
			const auto part = Trim(rawPart);
			if (part.empty())
			{
				error = L"DataGrid 列 Binding 包含空参数。";
				return false;
			}
			const auto equals = part.find(L'=');
			if (equals == std::wstring::npos)
			{
				if (positionalPath || namedArguments.contains(L"Path"))
				{
					error = L"DataGrid 列 Binding 不能重复声明 Path。";
					return false;
				}
				positionalPath = true;
				continue;
			}
			const auto key = Trim(part.substr(0, equals));
			const auto argument = Trim(part.substr(equals + 1));
			if (key != L"Path" && key != L"Mode"
				&& key != L"UpdateSourceTrigger" && key != L"Converter"
				&& key != L"ConverterParameter" && key != L"StringFormat"
				&& key != L"FallbackValue" && key != L"TargetNullValue"
				&& key != L"ElementName" && key != L"RelativeSource")
			{
				error = L"DataGrid 列 Binding 不支持参数：" + key;
				return false;
			}
			if (argument.empty())
			{
				error = L"DataGrid 列 Binding 参数不能为空：" + key;
				return false;
			}
			if (!namedArguments.insert(key).second
				|| (key == L"Path" && positionalPath))
			{
				error = L"DataGrid 列 Binding 不能重复声明：" + key;
				return false;
			}
		}
		if (!TryParseBinding(value, binding, error))
		{
			if (error.empty()) error = L"DataGrid 列 Binding 语法无效。";
			return false;
		}
		return DesignerBindingUtils::ValidateDataGridColumnBindingSource(
			binding, nullptr, &error);
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
		if (!text.starts_with(L"TemplateBinding")) return false;
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
		if (!text.starts_with(L"RaiseEvent")) return false;
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
			return Equals(sourceEvent, L"TextChanged")
				|| Equals(sourceEvent, L"PreviewTextInputStart")
				|| Equals(sourceEvent, L"TextInputStart")
				|| Equals(sourceEvent, L"PreviewTextInputUpdate")
				|| Equals(sourceEvent, L"TextInputUpdate")
				|| Equals(sourceEvent, L"PreviewTextInput")
				|| Equals(sourceEvent, L"TextInput");
		if (payload == DesignerComponentEventPayload::Bool)
			return Equals(sourceEvent, L"Checked")
				|| Equals(sourceEvent, L"Unchecked");
		if (payload != DesignerComponentEventPayload::None) return false;
		for (const auto* supported : {
			L"Click", L"PreviewMouseDown", L"MouseDown",
			L"PreviewMouseUp", L"MouseUp", L"MouseDoubleClick",
			L"MouseEnter", L"MouseLeave", L"PreviewGotKeyboardFocus",
			L"GotKeyboardFocus", L"PreviewLostKeyboardFocus",
			L"LostKeyboardFocus", L"GotFocus", L"LostFocus",
			L"SizeChanged", L"IsVisibleChanged", L"SelectionChanged",
			L"Selected", L"Unselected",
			L"Checked", L"Unchecked", L"Expanded", L"Collapsed",
			L"SubmenuOpened", L"SubmenuClosed", L"Opened", L"Closed",
			L"ScrollChanged", L"PreviewTextInputStart", L"TextInputStart",
			L"PreviewTextInputUpdate", L"TextInputUpdate",
			L"PreviewTextInput", L"TextInput",
			L"PreviewDragEnter", L"DragEnter",
			L"PreviewDragOver", L"DragOver",
			L"PreviewDragLeave", L"DragLeave",
			L"PreviewDrop", L"Drop" })
			if (Equals(sourceEvent, supported)) return true;
		return false;
	}

	bool IsPathOnlyBindingExpression(const std::wstring& value)
	{
		auto text = Trim(value);
		if (text.size() < 3 || text.front() != L'{' || text.back() != L'}')
			return false;
		text = Trim(text.substr(1, text.size() - 2));
		if (!text.starts_with(L"Binding")
			|| (text.size() > 7 && std::iswspace(text[7]) == 0
				&& text[7] != L',')) return false;
		text = Trim(text.substr(7));
		if (text.empty() || text.find(L',') != std::wstring::npos) return false;
		const auto equals = text.find(L'=');
		return equals == std::wstring::npos
			|| Trim(text.substr(0, equals)) == L"Path";
	}

	std::optional<DesignerEventDescriptor> FindEvent(
		UIClass type,
		const std::vector<DesignerComponentEventDescriptor>& componentEvents,
		const std::wstring& rawName,
		const std::wstring& rawValue)
	{
		const auto trimmedValue = Trim(rawValue);
		if (trimmedValue.starts_with(L"{Binding")
			|| trimmedValue.starts_with(L"{TemplateBinding"))
			return std::nullopt;
		const auto events = DesignerEventCatalog::GetControlEvents(
			type, componentEvents);
		for (const auto& event : events)
		{
			if (Equals(event.Name, rawName))
				return event;
		}

		bool booleanValue = false;
		if (Equals(rawName, L"IsChecked") && TryParseBool(rawValue, booleanValue))
			return std::nullopt;
		return std::nullopt;
	}

	std::optional<DesignerEventDescriptor> FindWindowEvent(
		const std::wstring& rawName)
	{
		for (const auto& event : DesignerEventCatalog::GetWindowEvents())
		{
			if (Equals(event.Name, rawName))
				return event;
		}
		return std::nullopt;
	}

	bool NormalizeHandler(
		const std::wstring& raw,
		std::wstring& stored,
		std::wstring& error)
	{
		stored = Trim(raw);
		if (stored.empty())
		{
			error = L"事件处理函数名不能为空，必须显式填写 C++ 成员函数名。";
			return false;
		}
		return DesignerEventCatalog::ValidateHandlerName(stored, &error);
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

	DesignValue DataGridLengthValue(const std::wstring& raw, bool& valid)
	{
		const auto value = Trim(raw);
		const auto lower = Lower(value);
		DesignValue result = DesignValue::object();
		for (const auto& [name, unit] : {
			std::pair{ L"auto", "Auto" },
			std::pair{ L"sizetoheader", "SizeToHeader" },
			std::pair{ L"sizetocells", "SizeToCells" } })
		{
			if (lower != name) continue;
			result["value"] = 1.0;
			result["unit"] = unit;
			valid = true;
			return result;
		}
		if (!value.empty() && value.back() == L'*')
		{
			auto factor = Trim(value.substr(0, value.size() - 1));
			if (factor.empty()) factor = L"1";
			double parsed = 0.0;
			valid = TryParseDouble(factor, parsed) && parsed >= 0.0;
			if (valid)
			{
				result["value"] = parsed;
				result["unit"] = "Star";
			}
			return result;
		}
		double parsed = 0.0;
		valid = TryParseDouble(value, parsed) && parsed >= 0.0;
		if (valid)
		{
			result["value"] = parsed;
			result["unit"] = "Pixel";
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
			IndexSourceSymbols(root);
			const auto rootName = FromUtf8(root->LocalName());
			if (!Equals(rootName, L"Window"))
				return Fail(L"XAML 根元素必须是 Window。", error);

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

			if (!ParseWindowAttributes(root, error)) return false;
			bool hasContent = false;
			bool hasCommandBindings = false;
			bool hasInputBindings = false;
			for (const auto& child : ChildElements(root))
			{
				DiagnosticContext childContext(*this, child);
				const auto name = FromUtf8(child->LocalName());
				if (IsRootPropertyElement(name, L"Resources")
					|| IsRootPropertyElement(name, L"Styles")
					|| IsRootPropertyElement(name, L"DataContextSchema")) continue;
				if (IsRootPropertyElement(name, L"CommandBindings"))
				{
					if (hasCommandBindings)
						return Fail(L"Window.CommandBindings 不能重复。", error);
					hasCommandBindings = true;
					if (!ParseCommandBindings(
						child, _document.Window.CommandBindings, error)) return false;
					continue;
				}
				if (IsRootPropertyElement(name, L"InputBindings"))
				{
					if (hasInputBindings)
						return Fail(L"Window.InputBindings 不能重复。", error);
					hasInputBindings = true;
					if (!ParseInputBindings(
						child, _document.Window.InputBindings, error)) return false;
					continue;
				}
				if (IsRootPropertyElement(name, L"Content"))
				{
					if (hasContent)
						return Fail(L"Window 只能声明一个 Content。", error);
					const auto content = ChildElements(child);
					if (content.size() != 1)
						return Fail(L"Window.Content 必须包含且只包含一个元素。", error);
					DiagnosticContext contentContext(*this, content.front());
					if (!ParseControl(content.front(), Parent{}, error)) return false;
					hasContent = true;
					continue;
				}
				if (name.find(L'.') != std::wstring::npos)
					return Fail(L"不支持的 Window 属性元素：" + name, error);
				if (hasContent)
					return Fail(L"Window 只能声明一个 Content；请使用 Panel、Grid 或其他布局容器组织子元素。", error);
				if (!ParseControl(child, Parent{}, error)) return false;
				hasContent = true;
			}

			return FinalizeDocument(error);
		}

		bool ParseResourceDictionaryRoot(
			const Element& root,
			std::wstring& error)
		{
			DiagnosticContext context(*this, root);
			if (!root)
				return Fail(L"XAML 没有根元素。", error);
			IndexSourceSymbols(root);
			if (!Equals(FromUtf8(root->LocalName()), L"ResourceDictionary"))
				return Fail(L"资源文件根元素必须是 ResourceDictionary。", error);
			if (!ParseResourceDictionary(root, error)) return false;
			return FinalizeDocument(error);
		}

	private:
		bool FinalizeDocument(std::wstring& error)
		{
			if (!ValidateRelativePanelConstraints(
				_document.Nodes, L"文档", error)) return false;
			MergeBindingSchema();
			if (!ValidateBindingSources(
				_document.Nodes, L"文档", false, error)) return false;
			for (const auto& component : _document.Components)
			{
				const auto owner = L"组件 " + component.Type.XamlName;
				if (!ValidateBindingSources(
					component.Template, owner, true, error)) return false;
			}
			for (const auto& dataTemplate : _document.DataTemplates)
			{
				const auto owner = L"DataTemplate " + dataTemplate.DisplayName();
				if (!ValidateBindingSources(
					dataTemplate.Template, owner, false, error)) return false;
			}
			for (const auto& controlTemplate : _document.ControlTemplates)
			{
				const auto owner = L"ControlTemplate "
					+ controlTemplate.DisplayName();
				if (!ValidateBindingSources(
					controlTemplate.Template, owner, true, error)) return false;
			}
			if (!_document.ValidateRichTextStructure(&error))
				return Fail(error, error);
			if (!_document.ValidateCommandTargetReferences(&error))
				return Fail(error, error);
			if (!_document.ValidateDataGridColumnBindingSources(&error))
				return Fail(error, error);
			DesignerDataContextSchemaUtils::Canonicalize(_document.DataContextSchema);
			DesignerStyleSheetUtils::Canonicalize(_document.StyleSheet);
			if (!DesignDataResourceUtils::ValidateAndCanonicalize(
				_document, &error)) return Fail(error, error);
			if (!DesignerStyleSheetUtils::ValidateAgainstRulePropertyMetadata(
				_document.StyleSheet,
				[&](const DesignerStyleRule& rule,
					CuiRuntime::XamlTypePropertySchema& schema,
					std::wstring* schemaError) -> bool
				{
					const auto* component = rule.ComponentType.Empty()
						? nullptr : FindVisibleComponent(rule.ComponentType);
					if (!rule.ComponentType.Empty() && !component)
					{
						if (schemaError) *schemaError =
							L"样式 TargetType 组件不存在。";
						return false;
					}
					return CuiRuntime::XamlRuntimeSchema::BuildPropertySchema(
						rule.HasType ? rule.Type : UIClass::UI_Base,
						component, _document, schema, schemaError);
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

	public:

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
		// Generated graph keys must never consume an authored x:Name that appears
		// later in source order. This set may span nested namescopes: over-reserving
		// only changes an invisible key and cannot reject otherwise valid XAML.
		std::unordered_set<std::wstring> _reservedAuthoredNames;
		std::unordered_map<std::wstring, int> _nameCounters;
		std::vector<std::wstring> _bindingPaths;
		DesignComponentDefinition* _activeTemplateComponent = nullptr;
		const CuiRuntime::XamlTypePropertySchema* _activeControlTemplateSchema = nullptr;
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

		bool ValidateVisibleDataGridColumnStyle(
			const std::wstring& key,
			UIClass generatedType,
			const std::wstring& member,
			std::wstring& error)
		{
			auto visible = VisibleStyleSheet();
			DesignerStyleSheet resolved;
			std::wstring inheritanceError;
			if (!DesignerStyleSheetUtils::ResolveInheritance(
				visible, resolved, &inheritanceError))
				return Fail(member + L" 无法解析 Style："
					+ inheritanceError, error);
			const auto found = std::find_if(
				resolved.Rules.rbegin(), resolved.Rules.rend(),
				[&](const auto& candidate)
				{ return Equals(candidate.Id, key); });
			if (found == resolved.Rules.rend())
				return Fail(member + L" 引用了未声明的 Style：" + key, error);
			if (!found->ComponentType.Empty()
				|| (found->HasType && found->Type != generatedType))
				return Fail(member + L" 的 Style TargetType 与生成元素不兼容："
					+ key, error);
			return true;
		}

		DesignerStyleSheet VisibleStyleSheet(
			const DesignerStyleSheet* extra = nullptr) const
		{
			DesignerStyleSheet result = _document.StyleSheet;
			auto append = [&](const DesignerStyleSheet& source)
			{
				DesignerStyleSheetUtils::AppendLexicalScope(result, source);
			};
			for (const auto& scope : _resourceScopes) append(scope);
			if (_resourceTarget && _resourceTarget != &_document.StyleSheet)
				append(*_resourceTarget);
			if (extra && extra != _resourceTarget) append(*extra);
			return result;
		}

		bool BuildVisiblePropertySchema(
			UIClass nativeType,
			const DesignComponentDefinition* component,
			CuiRuntime::XamlTypePropertySchema& schema,
			std::wstring& error)
		{
			auto schemaDocument = _document;
			schemaDocument.StyleSheet = VisibleStyleSheet();
			std::wstring schemaError;
			if (!CuiRuntime::XamlRuntimeSchema::BuildPropertySchema(
				nativeType, component, schemaDocument, schema, &schemaError))
				return Fail(L"XAML 类型 Schema 无法构造：" + schemaError, error);
			return true;
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

		void IndexSourceSymbols(const Element& root)
		{
			if (!root) return;
			if (!_document.Sources.Root.Valid())
				_document.Sources.Root = _sourceLocations.Span(root.get());
			for (const auto& attribute : root->Attributes())
			{
				if (!attribute || IsNamespaceAttribute(*attribute)) continue;
				const auto name = FromUtf8(attribute->LocalName());
				if (Equals(name, L"Name"))
				{
					auto authoredName = Trim(FromUtf8(attribute->Value()));
					if (!authoredName.empty())
						_reservedAuthoredNames.insert(std::move(authoredName));
				}
				if (!Equals(name, L"Key") && !Equals(name, L"Name")
					&& !Equals(name, L"TargetType")
					&& !Equals(name, L"DataType")
					&& !Equals(name, L"Property")
					&& !Equals(name, L"Event")
					&& !Equals(name, L"Source")) continue;
				_document.Sources.RecordSymbol(
					Trim(FromUtf8(attribute->Value())),
					_sourceLocations.Span(root.get(), attribute.get()));
			}
			for (const auto& child : ChildElements(root))
				IndexSourceSymbols(child);
		}

		static bool IsRootPropertyElement(
			const std::wstring& name,
			const std::wstring& property)
		{
			return Equals(name, property)
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

		bool ParseWindowAttributes(const Element& root, std::wstring& error)
		{
			DiagnosticContext context(*this, root);
			_document.Window.Source.Element = _sourceLocations.Span(root.get());
			const auto name = Attribute(root, L"Name");
			const auto xName = Attribute(root, L"Name", L"x");
			_document.Window.NameIsGenerated = !name && !xName;
			if (name)
				_document.Window.Name = Trim(*name);
			if (xName)
				_document.Window.Name = Trim(*xName);
			if (!ValidateIdentifier(_document.Window.Name, L"窗体名称", error)) return false;

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
			CuiRuntime::XamlTypePropertySchema schema;
			if (!BuildVisiblePropertySchema(
				UIClass::UI_Window, nullptr, schema, error)) return false;

			std::unordered_set<std::wstring> assignedProperties;
			for (const auto& attribute : root->Attributes())
			{
				if (!attribute || IsNamespaceAttribute(*attribute)) continue;
				DiagnosticContext attributeContext(*this, root, attribute.get());
				const auto prefix = FromUtf8(attribute->Prefix());
				auto name = FromUtf8(attribute->LocalName());
				const auto value = FromUtf8(attribute->Value());
				const auto attributeNamespace =
					FromUtf8(attribute->NamespaceURI());
				if (Equals(attributeNamespace,
					L"http://www.w3.org/XML/1998/namespace")
					&& Equals(name, L"lang"))
					name = L"Language";
				const auto sourceSpan = _sourceLocations.Span(
					root.get(), attribute.get());
				_document.Window.Source.RecordMember(name, sourceSpan);
				if (Equals(name, L"Name")
					|| (Equals(prefix, L"x") && Equals(name, L"Class"))
					|| (Equals(prefix, L"d") && Equals(name, L"CodeBehind")))
					continue;

				if (const auto event = FindWindowEvent(name))
				{
					std::wstring handler;
					if (!NormalizeHandler(value, handler, error))
						return Fail(L"窗体事件 " + event->Name + L"：" + error, error);
					if (_document.Window.Events.contains(event->Name))
						return Fail(L"窗体事件重复：" + event->Name, error);
					_document.Window.Events[event->Name] = std::move(handler);
					continue;
				}

				if (Equals(name, L"Style"))
				{
					std::wstring styleKey;
					if (!TryParseStaticResource(value, styleKey))
						return Fail(L"Window.Style 必须使用 {StaticResource key}。", error);
					_document.Window.Properties.StyleResourceKey = std::move(styleKey);
					continue;
				}
				auto propertyName = NormalizePropertyName(name, value, true);
				auto propertyValue = value;
				DesignerDataBinding binding;
				std::wstring bindingError;
				if (TryParseBinding(propertyValue, binding, bindingError))
				{
					if (!ResolveBindingAncestorType(root, binding, error)) return false;
					const auto* metadata = schema.FindProperty(propertyName);
					if (!metadata || !metadata->CanWrite())
						return Fail(L"Window 绑定目标属性不存在或不可写：" + name,
							error);
					if (binding.StringFormat
						&& metadata->ValueKind() != BindingValueKind::String)
						return Fail(L"Binding StringFormat 只能用于字符串目标属性："
							+ metadata->Name(), error);
					if (!DesignerBindingUtils::ValidateTarget(
						DesignerBindingUtils::ProjectTargetMetadata(*metadata),
						binding, &bindingError))
						return Fail(L"Window 属性 " + metadata->Name()
							+ L" 绑定无效：" + bindingError, error);
					if (!assignedProperties.insert(metadata->Name()).second)
						return Fail(L"Window 属性重复：" + metadata->Name(), error);
					_document.Window.Bindings[metadata->Name()] = std::move(binding);
					continue;
				}
				if (!bindingError.empty())
					return Fail(L"Window 属性 " + name + L"：" + bindingError, error);
				if (Equals(propertyName, L"Language"))
				{
					const auto normalized = NormalizeRichTextLanguageTag(
						Trim(propertyValue));
					if (!normalized)
						return Fail(L"Window 的 Language/xml:lang 不是有效的 RFC 3066 标签。",
							error);
					propertyValue = *normalized;
				}
				if (Equals(name, L"Visibility"))
				{
					bool recognized = false;
					propertyValue = NormalizeVisibility(value, recognized);
					if (!recognized) return Fail(L"Visibility 必须为 Visible、Hidden 或 Collapsed。", error);
				}
				DesignerPropertyDescriptor descriptor;
				if (!DesignerPropertyCatalog::TryGetStyleProperty(
					schema.Properties, propertyName, descriptor)
					|| !descriptor.Metadata)
					return Fail(L"Window 不包含可持久化属性：" + name, error);
				if (!assignedProperties.insert(descriptor.Name).second)
					return Fail(L"Window 属性重复：" + descriptor.Name, error);
				std::wstring resourceKey;
				std::wstring dynamicResourceKey;
				const DesignerStyleValue* resourceValue = nullptr;
				if (TryParseStaticResource(propertyValue, resourceKey))
				{
					const auto* resource = FindVisibleResource(resourceKey);
					if (!resource) return Fail(L"Window 属性 " + name
						+ L" 引用了不存在的资源：" + resourceKey, error);
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
					DesignerStyleValue effective;
					if (!DesignerPropertyCatalog::CaptureDefaultValue(
						*descriptor.Metadata, effective, &error)) return false;
					StoreMetadata(_document.Window, descriptor.Name,
						effective, {}, dynamicResourceKey);
					continue;
				}
				DesignerStyleValue typed = resourceValue ? *resourceValue
					: DesignerStyleValue{ descriptor.ValueKind,
						NormalizePropertyText(name, propertyValue, descriptor) };
				DesignerStyleValue effective;
				std::wstring applyError;
				if (!DesignerPropertyCatalog::NormalizeStyleValue(
					*descriptor.Metadata, typed, effective, &applyError,
					_options.ResourceBasePath, _document.Resources))
					return Fail(L"窗体属性 " + name + L"：" + applyError, error);
				StoreMetadata(_document.Window, descriptor.Name, effective,
					resourceKey, dynamicResourceKey);
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
						L"其他结构型资源仍应放在 Window.Resources。",
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
			auto& rules = (_resourceTarget
				? *_resourceTarget : _document.StyleSheet).Rules;
			rules.erase(std::remove_if(rules.begin(), rules.end(),
				[&](const auto& current)
				{
					return DesignerStyleSheetUtils::HasSameStyleResourceIdentity(
						current, rule);
				}), rules.end());
			rules.push_back(std::move(rule));
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
			const auto identity = resource.Identity;
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
			case DesignerStyleValueKind::NullableBool: return L"{x:Null}";
			case DesignerStyleValueKind::Int:
			case DesignerStyleValueKind::Int64:
			case DesignerStyleValueKind::Float:
			case DesignerStyleValueKind::Double: return L"0";
			case DesignerStyleValueKind::String: return {};
			case DesignerStyleValueKind::Color: return L"#00000000";
			case DesignerStyleValueKind::Thickness: return L"0";
			case DesignerStyleValueKind::CornerRadius: return L"0";
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
			case DesignerStyleValueKind::NullableBool:
			case DesignerStyleValueKind::Int:
			case DesignerStyleValueKind::Int64:
			case DesignerStyleValueKind::Float:
			case DesignerStyleValueKind::Double:
			case DesignerStyleValueKind::String:
			case DesignerStyleValueKind::Color:
			case DesignerStyleValueKind::Thickness:
			case DesignerStyleValueKind::CornerRadius:
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
			DependencyPropertyEditorKind& editor,
			std::wstring& error)
		{
			for (const auto& [name, kind] : {
				std::pair{ L"Auto", DependencyPropertyEditorKind::Auto },
				std::pair{ L"Text", DependencyPropertyEditorKind::Text },
				std::pair{ L"Boolean", DependencyPropertyEditorKind::Boolean },
				std::pair{ L"Number", DependencyPropertyEditorKind::Number },
				std::pair{ L"Choice", DependencyPropertyEditorKind::Choice },
				std::pair{ L"Color", DependencyPropertyEditorKind::Color },
				std::pair{ L"Thickness", DependencyPropertyEditorKind::Thickness },
				std::pair{ L"Size", DependencyPropertyEditorKind::Size },
				std::pair{ L"Length", DependencyPropertyEditorKind::Length } })
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
				return Fail(L"ControlTemplate TargetType 必须是可模板化 Control "
					L"或已声明的组件 QName："
					+ targetType, error);
			const auto roots = ChildElements(element);
			if (roots.size() != 1)
				return Fail(L"ControlTemplate 必须且只能包含一个视觉根。", error);

			CuiRuntime::XamlTypePropertySchema targetSchema;
			if (!BuildVisiblePropertySchema(
				definition.TargetType, targetComponent, targetSchema, error)) return false;

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
			auto* previousControlTemplateSchema = _activeControlTemplateSchema;
			const bool previousControlTemplateVisual =
				_parsingControlTemplateVisual;
			const bool previousComponentTemplateVisual =
				_parsingComponentTemplateVisual;
			const auto previousVisualStateGroups = _pendingVisualStateGroups;
			const auto previousEventTriggers = _pendingEventTriggers;
			_pendingVisualStateGroups.reset();
			_pendingEventTriggers.reset();
			_activeTemplateComponent = &templateContext;
			_activeControlTemplateSchema = &targetSchema;
			_parsingControlTemplateVisual = true;
			_parsingComponentTemplateVisual = false;
			bool parsed = ParseControl(roots.front(), Parent{}, error);
			if (parsed) parsed = ValidateRelativePanelConstraints(
				_document.Nodes, L"ControlTemplate " + definition.DisplayName(), error);
			if (parsed && (_pendingVisualStateGroups || _pendingEventTriggers))
				templateContext.Template = _document.Nodes;
			if (parsed && _pendingVisualStateGroups)
				parsed = ParseVisualStateGroups(
					_pendingVisualStateGroups, templateContext, error);
			if (parsed && _pendingEventTriggers)
				parsed = ParseEventTriggers(
					_pendingEventTriggers, templateContext, error);
			_activeTemplateComponent = previousTemplate;
			_activeControlTemplateSchema = previousControlTemplateSchema;
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
			bool parsed = ParseControl(roots.front(), Parent{}, error);
			if (parsed) parsed = ValidateRelativePanelConstraints(
				_document.Nodes, L"DataTemplate " + definition.DisplayName(), error);
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
					{ L"Orientation" }, error)) return false;
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
					{ L"Orientation", L"ItemHeight", L"CacheLength" },
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
			if (!parseNonNegative(L"ItemWidth", definition.Value.ItemWidth)
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
				{ L"Key", L"HeaderTemplate" },
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
				|| definition.BaseType == UIClass::UI_TabItem
				|| definition.BaseType == UIClass::UI_ListBoxItem
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
					if (CuiRuntime::XamlRuntimeSchema::FindNativeProperty(
						definition.BaseType, content.Name))
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
					if (CuiRuntime::XamlRuntimeSchema::FindNativeProperty(
						definition.BaseType, property.Name))
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
						if (property.Editor != DependencyPropertyEditorKind::Auto
							&& property.Editor != DependencyPropertyEditorKind::Choice)
							return Fail(L"Enum 组件属性只能使用 Auto 或 Choice 编辑器。", error);
						property.Editor = DependencyPropertyEditorKind::Choice;
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
						std::pair{ L"AffectsMeasure", DependencyPropertyFlags::AffectsMeasure },
						std::pair{ L"AffectsArrange", DependencyPropertyFlags::AffectsArrange },
						std::pair{ L"AffectsRender", DependencyPropertyFlags::AffectsRender },
						std::pair{ L"AffectsParentMeasure", DependencyPropertyFlags::AffectsParentMeasure },
						std::pair{ L"AffectsParentArrange", DependencyPropertyFlags::AffectsParentArrange },
						std::pair{ L"Inherits", DependencyPropertyFlags::Inherits },
						std::pair{ L"BindsTwoWayByDefault", DependencyPropertyFlags::BindsTwoWayByDefault } })
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
					if (property.IsReadOnly && HasDependencyPropertyFlag(
						property.Flags, DependencyPropertyFlags::BindsTwoWayByDefault))
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
				if (parsed) parsed = ValidateRelativePanelConstraints(
					_document.Nodes, L"组件 " + definition.Type.XamlName, error);
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

		bool ParseStyleSetter(
			const Element& element,
			const CuiRuntime::XamlTypePropertySchema& schema,
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
			const bool controlTemplateSetter =
				Equals(propertyName, L"Template");
			const bool itemsPanelSetter =
				Equals(propertyName, L"ItemsPanel");
			if (controlTemplateSetter || itemsPanelSetter)
			{
				if (!allowControlTemplate)
					return Fail(controlTemplateSetter
						? L"Template 目前只允许作为 Style 的普通 Setter；"
							L"Trigger/VisualState 动态换模板尚未开放。"
						: L"ItemsPanel 目前只允许作为 Style 的普通 Setter；"
							L"Trigger/VisualState 动态切换布局模板尚未开放。",
						error);
				if (!ChildElements(element).empty())
					return Fail(propertyName
						+ L" Setter 必须通过 StaticResource 引用结构资源。",
						error);
				if (itemsPanelSetter)
				{
					const auto* metadata = schema.FindProperty(propertyName);
					if (!metadata || !metadata->CanWrite())
						return Fail(L"Style 目标类型不包含可写属性："
							+ rawProperty, error);
				}
				setter.PropertyName = propertyName;
				setter.ResourceKey = Trim(
					Attribute(element, L"Resource").value_or(L""));
				if (setter.ResourceKey.empty()
					&& !TryParseStaticResource(rawValue, setter.ResourceKey))
					return Fail(propertyName
						+ L" Setter 必须通过 {StaticResource key} 引用"
						+ (controlTemplateSetter
							? L" ControlTemplate。" : L" ItemsPanelTemplate。"),
						error);
				setter.UsesResource = true;
				setter.UsesDynamicResource = false;
				return true;
			}
			const auto properties = DesignerPropertyCatalog::GetStyleProperties(
				schema.Properties);
			const auto* descriptor = DesignerPropertyCatalog::Find(
				properties, propertyName);
			std::vector<DesignerPropertyDescriptor>
				qualifiedTargetProperties;
			// WPF permits a dependency property to be owner-qualified in a
			// Setter (for example Border.CornerRadius).  Attached properties
			// already have that exact schema name, so only fall back to the
			// member name when the qualifier is a built-in base of the style
			// target.
			if (!descriptor)
			{
				const auto separator = propertyName.find(L'.');
				if (separator != std::wstring::npos
					&& separator > 0
					&& separator + 1 < propertyName.size()
					&& propertyName.find(L'.', separator + 1)
						== std::wstring::npos)
				{
					const auto ownerName =
						propertyName.substr(0, separator);
					const auto memberName =
						propertyName.substr(separator + 1);
					const auto* owner =
						CuiRuntime::XamlRuntimeSchema::FindBuiltInType(
							CuiRuntime::XamlRuntimeSchema::CuiNamespace,
							ownerName);
					if (owner)
					{
						if (IsUIClassAssignableFrom(
							owner->NativeType, schema.NativeType))
							descriptor = DesignerPropertyCatalog::Find(
								properties, memberName);
						else if (allowTargetName)
						{
							const auto ownerMetadata =
								CuiRuntime::XamlRuntimeSchema::
									NativeProperties(owner->NativeType);
							qualifiedTargetProperties =
								DesignerPropertyCatalog::GetStyleProperties(
									ownerMetadata);
							descriptor = DesignerPropertyCatalog::Find(
								qualifiedTargetProperties, memberName);
						}
					}
				}
			}
			if (!descriptor)
			{
				if (const auto* metadata = schema.FindProperty(propertyName);
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
			if (!objectValue || setterChildren.empty())
				setter.Literal.Text = NormalizePropertyText(
					rawProperty, rawValue, *descriptor);
			std::wstring validationError;
			DesignerStyleValue canonical;
			if (!descriptor->Metadata
				|| !DesignerPropertyCatalog::NormalizeStyleValue(
					*descriptor->Metadata, setter.Literal, canonical, &validationError,
				_currentResourceBasePath, _document.Resources))
				return Fail(L"Setter " + rawProperty + L"：" + validationError, error);
			setter.Literal = std::move(canonical);
			return true;
		}

		bool BuildVisualStateTargetSchema(
			const DesignComponentDefinition& component,
			const std::wstring& targetName,
			CuiRuntime::XamlTypePropertySchema& schema,
			std::wstring& error)
		{
			if (targetName.empty())
				return BuildVisiblePropertySchema(
					component.BaseType,
					component.Type.Empty() ? nullptr : &component,
					schema, error);
			const auto node = std::find_if(
				component.Template.begin(), component.Template.end(),
				[&](const auto& candidate)
				{ return Equals(candidate.Name, targetName); });
			if (node == component.Template.end())
				return Fail(L"Storyboard 找不到模板部件：" + targetName, error);
			const DesignComponentDefinition* nested = nullptr;
			if (!node->ComponentType.Empty())
			{
				nested = FindVisibleComponent(node->ComponentType);
				if (!nested)
					return Fail(L"Storyboard 目标组件 Schema 不存在："
						+ node->ComponentType.XamlName, error);
			}
			return BuildVisiblePropertySchema(node->Type, nested, schema, error);
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
			CuiRuntime::XamlTypePropertySchema targetSchema;
			if (!BuildVisualStateTargetSchema(
				component, animation.TargetName, targetSchema, error)) return false;
			const DesignerPropertyDescriptor* descriptor = nullptr;
			const DependencyPropertyMetadata* targetMetadata = nullptr;
			std::vector<DesignerPropertyDescriptor> targetProperties;
			DesignerStyleValueKind endpointKind = DesignerStyleValueKind::Float;
			objectPathKind = StoryboardObjectPathKind::None;
			std::wstring directPropertyName;
			if (TryNormalizeDirectStoryboardProperty(
				rawProperty, targetSchema, directPropertyName))
			{
				targetProperties = DesignerPropertyCatalog::GetStyleProperties(
					targetSchema.Properties);
				descriptor = DesignerPropertyCatalog::Find(
					targetProperties, directPropertyName);
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
				targetMetadata = targetSchema.FindProperty(descriptor->Name);
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
					|| !targetMetadata->TryConvert(parsed, converted))
					return Fail(L"动画 " + label
						+ L" 无法通过目标属性 Schema 转换。", error);
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
						std::wstring validationError;
						if (!DesignerStyleSheetUtils::TryConvertValue(
							keyFrame.Value, parsed, &validationError,
							_currentResourceBasePath, _document.Resources)
							|| !targetMetadata || !targetMetadata->CanWrite()
							|| !targetMetadata->TryConvert(parsed, converted))
							return Fail(L"动画 KeyFrame 无法通过目标属性 Schema 转换。",
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
			CuiRuntime::XamlTypePropertySchema hostSchema;
			if (!BuildVisiblePropertySchema(
				component.BaseType, &component, hostSchema, error)) return false;

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
										hostSchema.Properties);
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
									if (!descriptor->Metadata
										|| !DesignerPropertyCatalog::ValidateConditionValue(
											*descriptor->Metadata, condition.Value,
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
									const auto controlledKey = animation.TargetName + L"|"
										+ rootProperty;
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
								CuiRuntime::XamlTypePropertySchema targetSchema;
								if (!BuildVisualStateTargetSchema(
									component, animation.TargetName,
									targetSchema, error)) return false;
								const DesignerPropertyDescriptor* descriptor = nullptr;
								const DependencyPropertyMetadata* targetMetadata = nullptr;
								std::vector<DesignerPropertyDescriptor> targetProperties;
								DesignerStyleValueKind endpointKind =
									DesignerStyleValueKind::Float;
								bool transformPath = false;
								std::wstring directPropertyName;
								if (TryNormalizeDirectStoryboardProperty(
									rawProperty, targetSchema, directPropertyName))
								{
									targetProperties = DesignerPropertyCatalog::GetStyleProperties(
										targetSchema.Properties);
									descriptor = DesignerPropertyCatalog::Find(
										targetProperties, directPropertyName);
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
									targetMetadata = targetSchema.FindProperty(
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
										|| !targetMetadata->TryConvert(parsed, converted))
										return Fail(L"动画 " + label + L" 无效："
											+ (validationError.empty()
												? L"无法通过目标属性 Schema 转换。"
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
								const auto controlledKey = animation.TargetName + L"|"
									+ (transformPath ? L"RenderTransform"
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
							CuiRuntime::XamlTypePropertySchema targetSchema;
							if (!BuildVisualStateTargetSchema(
								component, setter.TargetName,
								targetSchema, error)) return false;
							DesignerStyleSetter value;
							if (!ParseStyleSetter(
								setterElement, targetSchema, value, error, true)) return false;
							setter.PropertyName = value.PropertyName;
							setter.UsesResource = value.UsesResource;
							setter.ResourceKey = std::move(value.ResourceKey);
							setter.Literal = std::move(value.Literal);
							if (setter.UsesResource)
							{
								const auto* resource = FindVisibleResource(
									setter.ResourceKey);
								std::wstring validationError;
								const auto* metadata = targetSchema.FindProperty(
									setter.PropertyName);
								DesignerStyleValue canonical;
								if (!resource || !metadata
									|| !DesignerPropertyCatalog::NormalizeStyleValue(
										*metadata, resource->Value, canonical,
										&validationError, _currentResourceBasePath,
										_document.Resources))
									return Fail(L"VisualState Setter 资源不存在或类型不兼容："
										+ setter.ResourceKey, error);
								if (_objectResourceTarget)
									setter.Literal = std::move(canonical);
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
						const auto key = setter.TargetName + L"|"
							+ setter.PropertyName;
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
								const auto controlledKey = animation.TargetName + L"|"
									+ rootProperty;
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
			const CuiRuntime::XamlTypePropertySchema& schema,
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
			const auto propertyName = NormalizePropertyName(rawProperty, *value);
			const auto properties =
				DesignerPropertyCatalog::GetConditionProperties(schema.Properties);
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
				*descriptor->Metadata, condition.Value, &validationError,
				_currentResourceBasePath, _document.Resources))
				return Fail(L"Condition " + rawProperty + L"："
					+ validationError, error);
			trigger.PropertyConditions.push_back(std::move(condition));
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
			const CuiRuntime::XamlTypePropertySchema& schema,
			const DesignComponentDefinition& target,
			DesignerStyleTrigger& trigger,
			std::wstring& error)
		{
			if (!ParseStyleCondition(
				element, schema, trigger, false, error)) return false;
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
					if (!ParseStyleSetter(child, schema, setter, error)) return false;
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
			const CuiRuntime::XamlTypePropertySchema& schema,
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
					if (!ParseStyleSetter(child, schema, setter, error)) return false;
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
						conditionElement, schema, trigger, true, error)) return false;
				}
			}
			if (trigger.PropertyConditions.size() < 2)
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
				return Fail(L"DataTrigger Binding 首批只支持 Path，不支持 Mode、UpdateSourceTrigger 或 Converter。",
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
			const CuiRuntime::XamlTypePropertySchema& schema,
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
					if (!ParseStyleSetter(child, schema, setter, error)) return false;
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
			const CuiRuntime::XamlTypePropertySchema& schema,
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
					if (!ParseStyleSetter(child, schema, setter, error)) return false;
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
				{ L"TargetType", L"BasedOn" }, error, true)) return false;
			DesignerStyleRule rule;
			if (const auto target = Attribute(element, L"TargetType"))
			{
				const auto typeToken = MarkupTypeToken(*target);
				const auto typeName = StripMarkupType(typeToken);
				if (!Equals(typeName, L"Any"))
				{
					const auto separator = typeToken.find(L':');
					const auto prefix = separator == std::wstring::npos
						? std::wstring{} : Trim(typeToken.substr(0, separator));
					const auto localName = separator == std::wstring::npos
						? typeName : Trim(typeToken.substr(separator + 1));
					const auto namespaceUri = prefix.empty()
						? std::wstring(CuiRuntime::XamlRuntimeSchema::CuiNamespace)
						: LookupNamespaceUri(element, prefix);
					const DesignComponentDefinition* component = nullptr;
					if (separator != std::wstring::npos && separator > 0
						&& separator + 1 < typeToken.size())
					{
						component = FindVisibleComponent(
							namespaceUri, localName);
					}
					if (component)
					{
						rule.Type = component->BaseType;
						rule.ComponentType = component->Type;
					}
					else
					{
						const auto* descriptor =
							CuiRuntime::XamlRuntimeSchema::FindBuiltInType(
								namespaceUri, localName);
						if (!descriptor)
							return Fail(L"Style TargetType 无效：" + typeToken, error);
						rule.Type = descriptor->NativeType;
						rule.XamlType = descriptor->TypeId;
					}
					rule.HasType = true;
				}
			}
			rule.Id = Trim(Attribute(element, L"Key", L"x").value_or(L""));
			if (const auto basedOn = Attribute(element, L"BasedOn"))
			{
				if (!TryParseStaticResource(*basedOn, rule.BasedOn))
					return Fail(L"Style BasedOn 必须使用 {StaticResource key}。", error);
			}
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
			DesignComponentDefinition styleTarget;
			styleTarget.BaseType = effectiveRule.HasType
				? effectiveRule.Type : UIClass::UI_Base;
			const DesignComponentDefinition* component = nullptr;
			if (!effectiveRule.ComponentType.Empty())
			{
				component = FindVisibleComponent(
					effectiveRule.ComponentType);
				if (!component)
					return Fail(L"Style TargetType 组件不存在。", error);
				styleTarget = *component;
			}
			CuiRuntime::XamlTypePropertySchema targetSchema;
			if (!BuildVisiblePropertySchema(
				styleTarget.BaseType, component, targetSchema, error)) return false;
			bool foundTriggers = false;
			for (const auto& child : ChildElements(element))
			{
				DiagnosticContext childContext(*this, child);
				const auto childName = FromUtf8(child->LocalName());
				if (Equals(childName, L"Setter"))
				{
					DesignerStyleSetter setter;
					if (!ParseStyleSetter(
						child, targetSchema, setter, error, false, true)) return false;
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
							triggerElement, targetSchema, styleTarget,
							trigger, error)) return false;
					}
					else if (Equals(triggerName, L"MultiTrigger"))
					{
						if (!ParseStyleMultiTrigger(
							triggerElement, targetSchema, styleTarget,
							trigger, error)) return false;
					}
					else if (Equals(triggerName, L"DataTrigger"))
					{
						if (!ParseStyleDataTrigger(
							triggerElement, targetSchema, styleTarget,
							trigger, error)) return false;
					}
					else if (!ParseStyleMultiDataTrigger(
						triggerElement, targetSchema, styleTarget,
						trigger, error)) return false;
					rule.Triggers.push_back(std::move(trigger));
				}
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
			const auto* descriptor =
				CuiRuntime::XamlRuntimeSchema::FindBuiltInType({}, typeName);
			if (!descriptor) return false;
			type = descriptor->NativeType;
			return true;
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

			const auto namespaceUri = prefix.empty()
				? std::wstring{} : LookupNamespaceUri(element, prefix);
			if (const auto* builtIn =
				CuiRuntime::XamlRuntimeSchema::FindBuiltInType(
					namespaceUri, localName))
			{
				binding.AncestorType = builtIn->IsDefaultForNativeType
					? DesignerStyleSheetUtils::UIClassName(builtIn->NativeType)
					: builtIn->TypeId.LocalName;
				binding.AncestorTypeNamespace = builtIn->IsDefaultForNativeType
					? std::wstring{} : builtIn->TypeId.NamespaceUri;
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
			auto& next = _nameCounters[stem];
			for (;;)
			{
				const auto candidate = stem + std::to_wstring(++next);
				if (!_usedNames.contains(candidate)
					&& !_reservedAuthoredNames.contains(candidate)) return candidate;
			}
		}

		bool ReadControlIdentity(
			const Element& element,
			UIClass type,
			std::wstring& name,
			bool& nameIsGenerated,
			int& id,
			std::wstring& error)
		{
			DiagnosticContext context(*this, element);
			name = Trim(Attribute(element, L"Name", L"x").value_or(
				Attribute(element, L"Name").value_or(L"")));
			nameIsGenerated = name.empty();
			if (nameIsGenerated) name = MakeControlName(type);
			if (!ValidateIdentifier(name, L"控件名称", error)) return false;
			if (!_usedNames.insert(name).second)
				return Fail(L"控件名称重复：" + name, error);

			do { id = _document.AllocateNodeId(); }
			while (_usedIds.contains(id));
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
				{ L"Path", L"Mode", L"UpdateSourceTrigger",
				  L"Converter", L"ConverterParameter", L"StringFormat",
				  L"ElementName", L"FallbackValue", L"TargetNullValue",
				  L"RelativeSource" }, error)) return false;
			if (!ChildElements(element).empty())
				return Fail(L"Binding 子项不能包含子元素。", error);
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
			appendRaw(L"UpdateSourceTrigger");
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
			const CuiRuntime::XamlTypePropertySchema& schema,
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
			const auto* metadata = schema.FindProperty(propertyName);
			if (!metadata)
				return Fail(L"MultiBinding 目标属性不存在：" + propertyName, error);
			auto& node = _document.Nodes[nodeIndex];
			const auto propertyKey = metadata->Name();
			if (node.Bindings.contains(propertyKey)
				|| node.Properties.Find(metadata->Name()))
				return Fail(L"属性重复：" + metadata->Name(), error);

			const auto& multiElement = children.front();
			DiagnosticContext multiContext(*this, multiElement);
			if (!ValidateAttributes(multiElement,
				{ L"Mode", L"UpdateSourceTrigger", L"Converter",
				  L"ConverterParameter", L"StringFormat", L"FallbackValue",
				  L"TargetNullValue" }, error)) return false;
			DesignerDataBinding binding;
			if (const auto mode = Attribute(multiElement, L"Mode"); mode
				&& !DesignerBindingUtils::TryParseBindingMode(*mode, binding.Mode))
				return Fail(L"MultiBinding Mode 无效：" + *mode, error);
			if (const auto update = Attribute(
				multiElement, L"UpdateSourceTrigger"))
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
				const bool hasUpdateSourceTrigger =
					Attribute(child, L"UpdateSourceTrigger").has_value();
				DesignerDataBinding childBinding;
				if (!ParseBindingObjectElement(child, childBinding, error)) return false;
				if (!hasMode) childBinding.Mode = binding.Mode;
				if (!hasUpdateSourceTrigger)
					childBinding.UpdateMode = binding.UpdateMode;
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
				DesignerStyleValue canonical;
				if (DesignerPropertyCatalog::NormalizeStyleValue(
					*metadata, *literal, canonical, &literalError,
					_currentResourceBasePath, _document.Resources))
				{
					*literal = std::move(canonical);
					return true;
				}
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
			node.Bindings[propertyKey] = std::move(binding);
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
				const auto event = DesignerEventCatalog::FindControlEvent(
					component.BaseType, trigger.EventName, component.Events);
				if (!event)
					return Fail(L"EventTrigger.RoutedEvent 不是模板宿主公开的路由事件："
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
			bool forcedHeader = false)
		{
			DiagnosticContext context(*this, element);
			const auto elementName = FromUtf8(element->LocalName());
			UIClass type = UIClass::UI_Base;
			DesignerComponentType componentType;
			const auto elementNamespace = FromUtf8(element->NamespaceURI());
			const auto* builtInDescriptor =
				CuiRuntime::XamlRuntimeSchema::FindBuiltInType(
					elementNamespace, elementName);
			const bool builtInType = builtInDescriptor != nullptr;
			if (builtInType && !builtInDescriptor->IsConstructible)
				return Fail(L"XAML 类型不可直接实例化：" + elementName, error);
			if (builtInType) type = builtInDescriptor->NativeType;
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
			if (type == UIClass::UI_ItemsPresenter)
			{
				const auto targetType = _activeControlTemplateSchema
					? _activeControlTemplateSchema->NativeType : UIClass::UI_Base;
				if (!_parsingControlTemplateVisual
					|| !IsUIClassAssignableFrom(
						UIClass::UI_ItemsControl, targetType))
					return Fail(L"ItemsPresenter 只能出现在 ItemsControl 或 ListBox "
						L"的 ControlTemplate 中。", error);
				if (std::any_of(_document.Nodes.begin(), _document.Nodes.end(),
					[](const auto& candidate)
					{ return candidate.Type == UIClass::UI_ItemsPresenter; }))
					return Fail(L"一个 ControlTemplate 最多只能包含一个 ItemsPresenter。",
						error);
			}
			DesignNode node;
			if (!ReadControlIdentity(
				element, type, node.Name, node.NameIsGenerated, node.Id, error))
				return false;
			node.Type = type;
			if (builtInType) node.XamlType = builtInDescriptor->TypeId;
			node.ComponentType = std::move(componentType);
			node.ParentId = parent.Id;
			node.ParentRef = parent.Ref;
			node.Order = SiblingCount(parent);
			if (forcedHeader) node.Structure.ChildRole = DesignNodeChildRole::Header;
			node.Source.Element = _sourceLocations.Span(element.get());
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
			auto isOwnedCollectionProperty = [&](const std::wstring& name,
				const std::wstring& property)
			{
				if (Equals(name, property)) return true;
				const auto separator = name.rfind(L'.');
				if (separator == std::wstring::npos
					|| !Equals(name.substr(separator + 1), property)) return false;
				const auto owner = name.substr(0, separator);
				return Equals(owner, elementName) || Equals(owner, L"Control")
					|| Equals(owner, L"FrameworkElement")
					|| Equals(owner, L"UIElement");
			};
			Element resourcesElement;
			Element commandBindingsElement;
			Element inputBindingsElement;
			for (const auto& child : elementChildren)
			{
				const auto childName = FromUtf8(child->LocalName());
				if (isResourcesProperty(childName))
				{
					if (resourcesElement)
						return Fail(L"控件 Resources 属性元素不能重复。", error);
					resourcesElement = child;
				}
				else if (isOwnedCollectionProperty(childName, L"CommandBindings"))
				{
					if (commandBindingsElement)
						return Fail(L"控件 CommandBindings 属性元素不能重复。", error);
					commandBindingsElement = child;
				}
				else if (isOwnedCollectionProperty(childName, L"InputBindings"))
				{
					if (inputBindingsElement)
						return Fail(L"控件 InputBindings 属性元素不能重复。", error);
					inputBindingsElement = child;
				}
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
				auto localSchemaDocument = _document;
				localSchemaDocument.StyleSheet = visibleLocalStyles;
				if (!DesignerStyleSheetUtils::Validate(
					visibleLocalStyles, &error, _document.ResourceBasePath,
					_document.Resources))
				{
					restoreTargets();
					return Fail(error, error);
				}
				if (!DesignerStyleSheetUtils::ValidateAgainstRulePropertyMetadata(
					visibleLocalStyles,
					[&](const DesignerStyleRule& rule,
						CuiRuntime::XamlTypePropertySchema& schema,
						std::wstring* schemaError) -> bool
					{
						const auto* component = rule.ComponentType.Empty()
							? nullptr : FindVisibleComponent(rule.ComponentType);
						if (!rule.ComponentType.Empty() && !component)
						{
							if (schemaError) *schemaError =
								L"样式 TargetType 组件不存在。";
							return false;
						}
						return CuiRuntime::XamlRuntimeSchema::BuildPropertySchema(
							rule.HasType ? rule.Type : UIClass::UI_Base,
							component, localSchemaDocument, schema, schemaError);
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

			const auto* instanceComponent = _document.Nodes[nodeIndex].ComponentType.Empty()
				? nullptr : FindVisibleComponent(_document.Nodes[nodeIndex].ComponentType);
			if (!_document.Nodes[nodeIndex].ComponentType.Empty() && !instanceComponent)
				return Fail(L"组件实例的 XAML Schema 不存在："
					+ _document.Nodes[nodeIndex].ComponentType.XamlName, error);
			CuiRuntime::XamlTypePropertySchema nodeSchema;
			if (!BuildVisiblePropertySchema(
				type, instanceComponent, nodeSchema, error)) return false;
			if (!ParseControlAttributes(
				element, nodeIndex, nodeSchema, error)) return false;
			if (commandBindingsElement && !ParseCommandBindings(
				commandBindingsElement,
				_document.Nodes[nodeIndex].CommandBindings, error)) return false;
			if (inputBindingsElement && !ParseInputBindings(
				inputBindingsElement,
				_document.Nodes[nodeIndex].InputBindings, error)) return false;
			if (!ApplyDirectText(element, nodeIndex, nodeSchema, error)) return false;

			const Parent childParent{
				_document.Nodes[nodeIndex].Id,
				_document.Nodes[nodeIndex].Name };
			bool usedItemsProperty = false;
			bool usedDirectItems = false;
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
			bool usedRichTextDocument =
				_document.Nodes[nodeIndex].Structure.Document.has_value();
			auto storeRichTextDocument = [&](const Element& documentElement,
				const Element& sourceElement) -> bool
			{
				if (usedRichTextDocument)
					return Fail(L"RichTextBox.Document 不能重复。", error);
				const auto& current = _document.Nodes[nodeIndex];
				if (current.Properties.Find(L"Text")
					|| current.Bindings.contains(L"Text")
					|| current.TemplateBindings.contains(L"Text"))
					return Fail(L"RichTextBox.Text 不能与 Document 同时使用。", error);
				DesignValue document;
				if (!ParseFlowDocument(documentElement, document, error)) return false;
				if (!StoreStructureValue(
					nodeIndex, "document", std::move(document), error)) return false;
				usedRichTextDocument = true;
				_document.Nodes[nodeIndex].Source.RecordMember(
					L"Document", _sourceLocations.Span(sourceElement.get()));
				return true;
			};
			for (const auto& child : elementChildren)
			{
				DiagnosticContext childContext(*this, child);
				const auto childName = FromUtf8(child->LocalName());
				if (isResourcesProperty(childName)
					|| isOwnedCollectionProperty(childName, L"CommandBindings")
					|| isOwnedCollectionProperty(childName, L"InputBindings")) continue;
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
				if (type == UIClass::UI_RichTextBox
					&& IsCuiRichTextElement(
						child, L"RichTextBox.Document"))
				{
					if (!ValidateAttributes(child, {}, error)
						|| !DirectText(child).empty())
						return Fail(L"RichTextBox.Document 只能包含一个 FlowDocument。",
							error);
					const auto roots = ChildElements(child);
					if (roots.size() != 1)
						return Fail(L"RichTextBox.Document 必须且只能包含一个 FlowDocument。",
							error);
					if (!storeRichTextDocument(roots.front(), child)) return false;
					continue;
				}
				if (type == UIClass::UI_RichTextBox
					&& Equals(childName, L"RichTextBox.Document"))
				{
					return Fail(
						L"RichTextBox.Document 必须使用 CUI 命名空间。", error);
				}
				if (type == UIClass::UI_RichTextBox
					&& IsCuiRichTextElement(child, L"FlowDocument"))
				{
					if (!storeRichTextDocument(child, child)) return false;
					continue;
				}
				if (Equals(childName, L"FlowDocument")
					|| Equals(childName, L"FlowDocument.Blocks")
					|| Equals(childName, L"Paragraph")
					|| Equals(childName, L"Paragraph.Inlines")
					|| Equals(childName, L"Run")
					|| Equals(childName, L"Run.Text")
					|| Equals(childName, L"Span")
					|| Equals(childName, L"Span.Inlines")
					|| Equals(childName, L"Bold")
					|| Equals(childName, L"Bold.Inlines")
					|| Equals(childName, L"Italic")
					|| Equals(childName, L"Italic.Inlines")
					|| Equals(childName, L"Underline")
					|| Equals(childName, L"Underline.Inlines")
					|| Equals(childName, L"LineBreak"))
					return Fail(L"富文本对象嵌套位置无效：" + childName, error);
				if (type == UIClass::UI_Grid
					&& Equals(childName, L"Grid.RowDefinitions"))
				{
					if (!ParseGridDefinitions(child, nodeIndex, true, error)) return false;
					continue;
				}
				if (type == UIClass::UI_Grid
					&& Equals(childName, L"Grid.ColumnDefinitions"))
				{
					if (!ParseGridDefinitions(child, nodeIndex, false, error)) return false;
					continue;
				}
				if (IsUIClassAssignableFrom(UIClass::UI_ItemsControl, type)
					&& (Equals(childName,
						DesignerStyleSheetUtils::UIClassName(type) + L".Items")
						|| Equals(childName, L"ItemsControl.Items")))
				{
					if (usedItemsProperty || usedDirectItems)
						return Fail(L"ItemsControl.Items 属性元素不能重复，"
							L"也不能与隐式 Items 混用。", error);
					if (!ValidateAttributes(child, {}, error)) return false;
					usedItemsProperty = true;
					for (const auto& item : ChildElements(child))
						if (!ParseControl(item, childParent, error)) return false;
					continue;
				}
				if (IsHeaderedContentControlType(type)
					&& (Equals(childName,
						DesignerStyleSheetUtils::UIClassName(type) + L".Header")
						|| Equals(childName, L"HeaderedContentControl.Header")
						|| Equals(childName, L"HeaderedItemsControl.Header")))
				{
					if (!ValidateAttributes(child, {}, error)) return false;
					auto& current = _document.Nodes[nodeIndex];
					const bool alreadyAssigned = current.Bindings.contains(L"Header")
						|| current.Properties.Find(L"Header")
						|| !current.Structure.HeaderTemplate.empty()
						|| std::any_of(_document.Nodes.begin(), _document.Nodes.end(),
							[&](const auto& candidate)
							{
								return candidate.ParentId == childParent.Id
									&& candidate.ParentRef == childParent.Ref
									&& candidate.Structure.ChildRole
										== DesignNodeChildRole::Header;
							});
					if (alreadyAssigned)
						return Fail(L"属性重复：Header", error);
					const auto roots = ChildElements(child);
					const auto text = DirectText(child);
					if (roots.size() > 1 || (!roots.empty() && !text.empty()))
						return Fail(L"Header 属性元素只能包含一个视觉根或一个文本值。", error);
					if (!roots.empty())
					{
						if (!ParseControl(roots.front(), childParent, error, true))
							return false;
					}
					else if (!StoreLiteralProperty(
						nodeIndex, nodeSchema, L"Header", text, error)) return false;
					continue;
				}
				if (type == UIClass::UI_Popup
					&& childName == L"Popup.Child")
				{
					if (!ValidateAttributes(child, {}, error)) return false;
					const bool alreadyAssigned = std::any_of(
						_document.Nodes.begin(), _document.Nodes.end(),
						[&](const auto& candidate)
						{
							return candidate.ParentId == childParent.Id
								&& candidate.ParentRef == childParent.Ref;
						});
					if (alreadyAssigned)
						return Fail(L"属性重复：Child", error);
					const auto roots = ChildElements(child);
					if (roots.size() > 1 || !DirectText(child).empty())
						return Fail(
							L"Popup.Child 只能包含一个视觉根，不能包含文本值。",
							error);
					if (!roots.empty()
						&& !ParseControl(roots.front(), childParent, error))
						return false;
					continue;
				}
				if (IsUIClassAssignableFrom(
						UIClass::UI_Decorator, type)
					&& (childName == L"Decorator.Child"
						|| childName == L"Border.Child"))
				{
					if (!ValidateAttributes(child, {}, error)) return false;
					const bool alreadyAssigned = std::any_of(
						_document.Nodes.begin(), _document.Nodes.end(),
						[&](const auto& candidate)
						{
							return candidate.ParentId == childParent.Id
								&& candidate.ParentRef == childParent.Ref;
						});
					if (alreadyAssigned)
						return Fail(L"属性重复：Child", error);
					const auto roots = ChildElements(child);
					if (roots.size() > 1 || !DirectText(child).empty())
						return Fail(
							L"Decorator.Child 只能包含一个视觉根，不能包含文本值。",
							error);
					if (!roots.empty()
						&& !ParseControl(roots.front(), childParent, error))
						return false;
					continue;
				}
				if (IsContentHostType(type))
				{
					const auto owner = DesignerStyleSheetUtils::UIClassName(type);
					if (Equals(childName, owner + L".Content")
						|| Equals(childName, L"ContentControl.Content"))
					{
						if (!ValidateAttributes(child, {}, error)) return false;
						auto& current = _document.Nodes[nodeIndex];
						const bool alreadyAssigned = current.Bindings.contains(L"Content")
							|| current.Properties.Find(L"Content")
							|| !current.Structure.ContentTemplate.empty()
							|| std::any_of(_document.Nodes.begin(), _document.Nodes.end(),
								[&](const auto& candidate)
								{
									return candidate.ParentId == childParent.Id
										&& candidate.ParentRef == childParent.Ref
										&& candidate.Structure.ChildRole
											!= DesignNodeChildRole::Header;
								});
						if (alreadyAssigned)
							return Fail(L"属性重复：Content", error);
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
						else if (!StoreLiteralProperty(
							nodeIndex, nodeSchema, L"Content", text, error)) return false;
						continue;
					}
				}
				bool bindingProperty = false;
				if (!TryParseMultiBindingProperty(
					child, nodeIndex, nodeSchema, bindingProperty, error)) return false;
				if (bindingProperty) continue;
				bool structuredProperty = false;
				if (!TryParseStructuredProperty(
					child, nodeIndex, type, nodeSchema,
					structuredProperty, error)) return false;
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
				if (IsUIClassAssignableFrom(UIClass::UI_ItemsControl, type))
				{
					if (usedItemsProperty)
						return Fail(L"ItemsControl.Items 属性元素不能与隐式 Items 混用。",
							error);
					usedDirectItems = true;
				}
				if (!ParseControl(child, childParent, error)) return false;
			}
			if (IsSingleVisualChildHostType(type))
			{
				const auto childCount = std::count_if(
					_document.Nodes.begin(), _document.Nodes.end(),
					[&](const auto& candidate)
					{
						return candidate.ParentId == childParent.Id
							&& candidate.ParentRef == childParent.Ref
							&& candidate.Structure.ChildRole
								!= DesignNodeChildRole::Header;
					});
				if (childCount > 1)
					return Fail(type == UIClass::UI_Popup
						? L"Popup 最多接受一个 Child。"
						: IsUIClassAssignableFrom(
							UIClass::UI_Decorator, type)
							? L"Decorator 最多接受一个 Child。"
							: L"内容控件最多接受一个直接视觉子节点。",
						error);
				if (type != UIClass::UI_Popup
					&& !IsUIClassAssignableFrom(
						UIClass::UI_Decorator, type))
				{
					const auto& current = _document.Nodes[nodeIndex];
					const bool hasContentBinding =
						current.Bindings.contains(L"Content");
					const bool hasContentValue =
						current.Properties.Find(L"Content");
					const bool hasTemplate =
						!current.Structure.ContentTemplate.empty();
					if (childCount != 0
						&& (hasContentBinding || hasContentValue || hasTemplate))
						return Fail(
							L"内容控件的直接视觉内容不能与 Content 或 "
							L"ContentTemplate 同时使用。",
							error);
					if (hasContentValue && hasTemplate)
						return Fail(
							L"标量 Content 当前不能使用 DataTemplate。",
							error);
				}
			}
			if (IsHeaderedContentControlType(type))
			{
				const auto headerCount = std::count_if(
					_document.Nodes.begin(), _document.Nodes.end(),
					[&](const auto& candidate)
					{
						return candidate.ParentId == childParent.Id
							&& candidate.ParentRef == childParent.Ref
							&& candidate.Structure.ChildRole
								== DesignNodeChildRole::Header;
					});
				const auto& current = _document.Nodes[nodeIndex];
				const bool hasHeaderBinding = current.Bindings.contains(L"Header");
				const bool hasHeaderValue = current.Properties.Find(L"Header");
				const bool hasHeaderTemplate = !current.Structure.HeaderTemplate.empty();
				if (headerCount > 1 || (headerCount != 0
					&& (hasHeaderBinding || hasHeaderValue || hasHeaderTemplate)))
					return Fail(L"HeaderedContentControl 最多接受一个视觉 Header，"
						L"且不能与 Header 或 HeaderTemplate 同时使用。", error);
				if (hasHeaderValue && hasHeaderTemplate)
					return Fail(L"标量 Header 当前不能使用 DataTemplate。", error);
			}
			if (IsUIClassAssignableFrom(UIClass::UI_ItemsControl, type))
			{
				const auto& current = _document.Nodes[nodeIndex];
				const bool hasItemsSource =
					!current.Structure.ItemsSourceResource.empty()
					|| current.Bindings.contains(L"ItemsSource");
				const bool hasAuthoredItems = std::any_of(
					_document.Nodes.begin(), _document.Nodes.end(),
					[&](const auto& candidate)
					{
						return candidate.ParentId == childParent.Id
							&& candidate.ParentRef == childParent.Ref;
					});
				if (hasItemsSource && hasAuthoredItems)
					return Fail(L"ItemsControl.Items 与 ItemsSource 不能同时赋值。",
						error);
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
			const CuiRuntime::XamlTypePropertySchema& schema,
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
				auto name = FromUtf8(attribute->LocalName());
				const auto rawName = FromUtf8(attribute->Name());
				const auto value = FromUtf8(attribute->Value());
				const auto sourceSpan = _sourceLocations.Span(
					element.get(), attribute.get());
				_document.Nodes[nodeIndex].Source.RecordMember(rawName, sourceSpan);
				_document.Nodes[nodeIndex].Source.RecordMember(name, sourceSpan);
				if (Equals(prefix, L"d") && Equals(name, L"Locked"))
				{
					bool locked = false;
					if (!TryParseBool(value, locked))
						return Fail(L"d:Locked 必须是布尔值。", error);
					_document.Nodes[nodeIndex].Locked = locked;
					continue;
				}
				if (Equals(name, L"Name")) continue;
				const auto commandSourceType = _document.Nodes[nodeIndex].Type;
				if (prefix.empty() && Equals(name, L"CommandTarget")
					&& (commandSourceType == UIClass::UI_Button
						|| commandSourceType == UIClass::UI_MenuItem))
				{
					if (!assignedProperties.insert(L"commandtarget").second)
						return Fail(L"属性重复：CommandTarget", error);
					std::wstring targetName;
					std::wstring targetError;
					if (!TryParseCommandTargetReference(
						value, targetName, targetError) || targetName.empty())
						return Fail(L"控件 " + _document.Nodes[nodeIndex].Name
							+ L" 的 CommandTarget："
							+ (targetError.empty()
								? L"CommandTarget 必须引用直接 x:Name。"
								: targetError), error);
					_document.Nodes[nodeIndex].Structure.CommandTarget =
						std::move(targetName);
					continue;
				}

				const auto attributeNamespace = FromUtf8(attribute->NamespaceURI());
				if (Equals(attributeNamespace,
					L"http://www.w3.org/XML/1998/namespace")
					&& Equals(name, L"lang"))
					name = L"Language";
				const bool builtInAttachedNamespace = attributeNamespace.empty()
					|| Equals(attributeNamespace, L"urn:cui");
				const std::pair<const wchar_t*, const char*> relativeBooleans[] = {
					{ L"CenterHorizontal", "centerHorizontal" },
					{ L"CenterVertical", "centerVertical" },
					{ L"AlignLeftWithPanel", "alignLeftWithPanel" },
					{ L"AlignTopWithPanel", "alignTopWithPanel" },
					{ L"AlignRightWithPanel", "alignRightWithPanel" },
					{ L"AlignBottomWithPanel", "alignBottomWithPanel" }
				};
				const std::pair<const wchar_t*, const char*> relativeReferences[] = {
					{ L"Above", "above" }, { L"Below", "below" },
					{ L"LeftOf", "leftOf" }, { L"RightOf", "rightOf" },
					{ L"AlignLeftWith", "alignLeftWith" },
					{ L"AlignRightWith", "alignRightWith" },
					{ L"AlignTopWith", "alignTopWith" },
					{ L"AlignBottomWith", "alignBottomWith" }
				};
				auto relativeMember = [&](const wchar_t* member)
				{
					return Equals(name, L"RelativePanel." + std::wstring(member));
				};
				auto requireRelativeParent = [&]()
				{
					const auto& node = _document.Nodes[nodeIndex];
					const auto parent = std::find_if(
						_document.Nodes.begin(), _document.Nodes.end(),
						[&](const DesignNode& candidate)
						{
							return (node.ParentId > 0 && candidate.Id == node.ParentId)
								|| (node.ParentId == 0 && !node.ParentRef.empty()
									&& candidate.Name == node.ParentRef);
						});
					return parent != _document.Nodes.end()
						&& parent->Type == UIClass::UI_RelativePanel;
				};
				bool handledRelativeConstraint = false;
				if (builtInAttachedNamespace)
				{
					for (const auto& [member, key] : relativeBooleans)
					{
						if (!relativeMember(member)) continue;
						if (!requireRelativeParent())
							return Fail(L"RelativePanel." + std::wstring(member)
								+ L" 只能设置在 RelativePanel 的直接子控件上。",
								error);
						bool typed = false;
						if (!TryParseBool(value, typed))
							return Fail(L"RelativePanel." + std::wstring(member)
								+ L" 必须是布尔值。", error);
						if (!assignedProperties.insert(
							L"RelativePanel." + std::wstring(member)).second)
							return Fail(L"属性重复：RelativePanel."
								+ std::wstring(member), error);
						auto& relativePanel = _document.Nodes[nodeIndex]
							.Structure.RelativePanel;
						if (!relativePanel) relativePanel.emplace();
						auto& constraints = *relativePanel;
						if (std::string_view(key) == "centerHorizontal")
							constraints.CenterHorizontal = typed;
						else if (std::string_view(key) == "centerVertical")
							constraints.CenterVertical = typed;
						else if (std::string_view(key) == "alignLeftWithPanel")
							constraints.AlignLeftWithPanel = typed;
						else if (std::string_view(key) == "alignTopWithPanel")
							constraints.AlignTopWithPanel = typed;
						else if (std::string_view(key) == "alignRightWithPanel")
							constraints.AlignRightWithPanel = typed;
						else constraints.AlignBottomWithPanel = typed;
						handledRelativeConstraint = true;
						break;
					}
					if (!handledRelativeConstraint)
					{
						for (const auto& [member, key] : relativeReferences)
						{
							if (!relativeMember(member)) continue;
							if (!requireRelativeParent())
								return Fail(L"RelativePanel." + std::wstring(member)
									+ L" 只能设置在 RelativePanel 的直接子控件上。",
									error);
							auto targetName = Trim(value);
							if (!ValidateIdentifier(targetName,
								L"RelativePanel." + std::wstring(member)
									+ L" 目标 x:Name", error)) return false;
							if (!assignedProperties.insert(
								L"RelativePanel." + std::wstring(member)).second)
								return Fail(L"属性重复：RelativePanel."
									+ std::wstring(member), error);
							auto& relativePanel = _document.Nodes[nodeIndex]
								.Structure.RelativePanel;
							if (!relativePanel) relativePanel.emplace();
							auto& constraints = *relativePanel;
							if (std::string_view(key) == "above") constraints.Above = targetName;
							else if (std::string_view(key) == "below") constraints.Below = targetName;
							else if (std::string_view(key) == "leftOf") constraints.LeftOf = targetName;
							else if (std::string_view(key) == "rightOf") constraints.RightOf = targetName;
							else if (std::string_view(key) == "alignLeftWith") constraints.AlignLeftWith = targetName;
							else if (std::string_view(key) == "alignRightWith") constraints.AlignRightWith = targetName;
							else if (std::string_view(key) == "alignTopWith") constraints.AlignTopWith = targetName;
							else constraints.AlignBottomWith = targetName;
							handledRelativeConstraint = true;
							break;
						}
					}
				}
				if (handledRelativeConstraint) continue;
				const bool supportsItemsSource =
					IsUIClassAssignableFrom(UIClass::UI_ItemsControl,
						_document.Nodes[nodeIndex].Type);
				if (supportsItemsSource && Equals(name, L"ItemsSource"))
				{
					std::wstring resourceKey;
					if (TryParseStaticResource(value, resourceKey))
					{
						if (!assignedProperties.insert(L"itemssource").second)
							return Fail(L"属性重复：ItemsSource", error);
						_document.Nodes[nodeIndex].Structure.ItemsSourceResource =
							resourceKey;
						continue;
					}
				}
				if (IsUIClassAssignableFrom(UIClass::UI_ItemsControl,
					_document.Nodes[nodeIndex].Type)
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
					_document.Nodes[nodeIndex].Structure.ItemTemplate = definition->Key;
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
					_document.Nodes[nodeIndex].Structure.ControlTemplate = definition->Key;
					continue;
				}
				if (_document.Nodes[nodeIndex].Type == UIClass::UI_DataGrid
					&& Equals(name, L"RowValidationErrorTemplate"))
				{
					std::wstring resourceKey;
					if (!TryParseStaticResource(value, resourceKey))
						return Fail(L"DataGrid.RowValidationErrorTemplate 必须引用已声明的 ControlTemplate："
							+ value, error);
					const auto* definition =
						FindVisibleControlTemplate(resourceKey);
					if (!definition)
						return Fail(L"DataGrid.RowValidationErrorTemplate 引用了未声明的 ControlTemplate："
							+ resourceKey, error);
					if (!definition->TargetComponentType.Empty()
						|| !IsControlTemplateTargetCompatible(
							UIClass::UI_Control, definition->TargetType))
						return Fail(L"DataGrid.RowValidationErrorTemplate TargetType 必须兼容 Control："
							+ definition->DisplayName(), error);
					if (!assignedProperties.insert(
						L"rowvalidationerrortemplate").second)
						return Fail(L"属性重复：RowValidationErrorTemplate", error);
					_document.Nodes[nodeIndex].Structure.
						RowValidationErrorTemplate = definition->Key;
					continue;
				}
				if (_document.Nodes[nodeIndex].Type == UIClass::UI_DataGrid)
				{
					struct DataGridStyleSlot final
					{
						const wchar_t* Name;
						const wchar_t* AssignmentKey;
						UIClass TargetType;
						std::wstring DesignNodeStructure::* Field;
					};
					static constexpr DataGridStyleSlot styleSlots[] = {
						{ L"CellStyle", L"cellstyle", UIClass::UI_DataGridCell,
							&DesignNodeStructure::DataGridCellStyle },
						{ L"ColumnHeaderStyle", L"columnheaderstyle",
							UIClass::UI_DataGridColumnHeader,
							&DesignNodeStructure::DataGridColumnHeaderStyle },
						{ L"RowStyle", L"rowstyle", UIClass::UI_DataGridRow,
							&DesignNodeStructure::DataGridRowStyle },
						{ L"RowHeaderStyle", L"rowheaderstyle",
							UIClass::UI_DataGridRowHeader,
							&DesignNodeStructure::DataGridRowHeaderStyle }
					};
					bool handledStyle = false;
					for (const auto& slot : styleSlots)
					{
						if (!Equals(name, slot.Name)) continue;
						std::wstring resourceKey;
						if (!TryParseStaticResource(value, resourceKey))
							return Fail(L"DataGrid." + std::wstring(slot.Name)
								+ L" 必须引用已声明的 Style。", error);
						if (!ValidateVisibleDataGridColumnStyle(
							resourceKey, slot.TargetType,
							L"DataGrid." + std::wstring(slot.Name), error))
							return false;
						if (!assignedProperties.insert(slot.AssignmentKey).second)
							return Fail(L"属性重复：" + std::wstring(slot.Name), error);
						_document.Nodes[nodeIndex].Structure.*(slot.Field) = resourceKey;
						handledStyle = true;
						break;
					}
					if (handledStyle) continue;
					if (Equals(name, L"RowHeaderTemplate")
						|| Equals(name, L"RowDetailsTemplate"))
					{
						const bool rowDetails =
							Equals(name, L"RowDetailsTemplate");
						const std::wstring propertyName = rowDetails
							? L"RowDetailsTemplate" : L"RowHeaderTemplate";
						std::wstring resourceKey;
						if (!TryParseStaticResource(value, resourceKey))
							return Fail(L"DataGrid." + propertyName
								+ L" 必须引用已声明的 "
								L"DataTemplate。", error);
						const auto* definition =
							FindVisibleDataTemplate(resourceKey);
						if (!definition)
							return Fail(L"DataGrid." + propertyName
								+ L" 引用了未声明的 "
								L"DataTemplate：" + resourceKey, error);
						if (!assignedProperties.insert(
							rowDetails ? L"rowdetailstemplate"
								: L"rowheadertemplate").second)
							return Fail(L"属性重复：" + propertyName, error);
						if (rowDetails)
							_document.Nodes[nodeIndex].Structure.
								DataGridRowDetailsTemplate = definition->Key;
						else _document.Nodes[nodeIndex].Structure.
							DataGridRowHeaderTemplate = definition->Key;
						continue;
					}
				}
				if (IsContentHostType(_document.Nodes[nodeIndex].Type)
					&& Equals(name, L"ContentTemplate"))
				{
					std::wstring templateSource;
					std::wstring templateError;
					if (!TryParseTemplateBinding(
						value, templateSource, templateError))
					{
						if (!templateError.empty())
							return Fail(templateError, error);
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
						_document.Nodes[nodeIndex].Structure.ContentTemplate = definition->Key;
						continue;
					}
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
					_document.Nodes[nodeIndex].Structure.HeaderTemplate = definition->Key;
					continue;
				}
				if (IsUIClassAssignableFrom(UIClass::UI_ItemsControl,
					_document.Nodes[nodeIndex].Type)
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
					_document.Nodes[nodeIndex].Structure.GroupStyle = definition->Key;
					continue;
				}
				if (IsUIClassAssignableFrom(UIClass::UI_ItemsControl,
					_document.Nodes[nodeIndex].Type)
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
					_document.Nodes[nodeIndex].Structure.ItemsPanel = definition->Key;
					continue;
				}
				if (schema.FindProperty(L"ItemContainerStyle")
					&& Equals(name, L"ItemContainerStyle"))
				{
					std::wstring resourceKey;
					if (!TryParseStaticResource(value, resourceKey))
						return Fail(L"ItemContainerStyle 必须引用已声明的 Style："
							+ value, error);
					if (!assignedProperties.insert(L"itemcontainerstyle").second)
						return Fail(L"属性重复：ItemContainerStyle", error);
					_document.Nodes[nodeIndex].Structure.ItemContainerStyle = resourceKey;
					continue;
				}
				if (_document.Nodes[nodeIndex].Type == UIClass::UI_ContentPresenter
					&& Equals(name, L"ContentSource"))
				{
					if (!_parsingControlTemplateVisual
						|| !_activeControlTemplateSchema)
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
							_activeControlTemplateSchema->NativeType))
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
						const auto* target = schema.FindProperty(targetName);
						const auto* source = _activeControlTemplateSchema
							->FindProperty(sourceName);
						BindingValue sourceDefault;
						BindingValue compatible;
						if (!target || !target->CanWrite() || !source || !source->CanRead()
							|| !source->TryGetDefaultValue(sourceDefault)
							|| !target->TryConvert(sourceDefault, compatible))
							return Fail(L"ContentSource 无法建立模板别名："
								+ std::wstring(sourceName) + L" -> " + targetName,
								error);
						if (!assignedProperties.insert(target->Name()).second)
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
					if (hostType != UIClass::UI_Canvas
						&& hostType != UIClass::UI_StackPanel
						&& hostType != UIClass::UI_WrapPanel
						&& hostType != UIClass::UI_DockPanel
						&& hostType != UIClass::UI_Grid
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
					const auto* target = schema.FindProperty(propertyName);
					if (!target || !target->CanWrite())
						return Fail(L"TemplateBinding 目标属性不可写或不存在：" + name, error);

					std::wstring canonicalSource = templateSource;
					BindingValue sourceDefault;
					if (_parsingControlTemplateVisual
						&& _activeControlTemplateSchema)
					{
						const auto* source = _activeControlTemplateSchema
							->FindProperty(templateSource);
						if (!source || !source->CanRead())
							return Fail(L"TemplateBinding 引用了 TargetType 不存在或不可读的属性："
								+ templateSource, error);
						canonicalSource = source->Name();
						if (!source->TryGetDefaultValue(sourceDefault))
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
					if (!assignedProperties.insert(target->Name()).second)
						return Fail(L"属性重复：" + target->Name(), error);
					_document.Nodes[nodeIndex].TemplateBindings[
						target->Name()] = canonicalSource;
					continue;
				}
				if (!templateError.empty())
					return Fail(L"控件 " + _document.Nodes[nodeIndex].Name
						+ L" 的属性 " + name + L"：" + templateError, error);
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
							const auto& key = stableName;
							if (events.contains(key)
								|| (owner->Type == _document.Nodes[nodeIndex].ComponentType
									&& events.contains(contract->Name)))
								return Fail(L"事件重复：" + rawName, error);
							events[key] = handler;
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
					const auto& key = event->Name;
					const auto attachedKey = component
						? DesignerEventCatalog::MakeAttachedComponentEventKey(
							component->Type, event->Name) : std::wstring{};
					if (events.contains(key)
						|| (!attachedKey.empty() && events.contains(attachedKey)))
						return Fail(L"事件重复：" + event->Name, error);
					events[key] = handler;
					continue;
				}

				if (Equals(name, L"Style"))
				{
					std::wstring styleKey;
					if (!TryParseStaticResource(value, styleKey))
						return Fail(L"Style 属性必须使用 {StaticResource key}。", error);
					_document.Nodes[nodeIndex].Properties.StyleResourceKey =
						std::move(styleKey);
					continue;
				}
				auto propertyName = NormalizePropertyName(name, value);
				_document.Nodes[nodeIndex].Source.RecordMember(
					propertyName, sourceSpan);
				auto propertyValue = value;

				DesignerDataBinding binding;
				std::wstring bindingError;
				if (TryParseBinding(propertyValue, binding, bindingError))
				{
					if (!ResolveBindingAncestorType(element, binding, error))
						return false;
					const auto* metadata = schema.FindProperty(propertyName);
					if (!metadata)
						return Fail(L"绑定目标属性不存在："
							+ _document.Nodes[nodeIndex].XamlType.LocalName
							+ L"." + name, error);
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
						DesignerStyleValue canonical;
						if (DesignerPropertyCatalog::NormalizeStyleValue(
							*metadata, *literal, canonical, &literalError,
							_currentResourceBasePath, _document.Resources))
						{
							*literal = std::move(canonical);
							return true;
						}
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
					if (!assignedProperties.insert(metadata->Name()).second)
						return Fail(L"属性重复：" + metadata->Name(), error);
					_document.Nodes[nodeIndex].Bindings[metadata->Name()] =
						std::move(binding);
					continue;
				}
				if (!bindingError.empty())
					return Fail(L"控件 " + _document.Nodes[nodeIndex].Name
						+ L" 的属性 " + name + L"：" + bindingError, error);
				if (Equals(propertyName, L"Language"))
				{
					const auto normalized = NormalizeRichTextLanguageTag(
						Trim(propertyValue));
					if (!normalized)
						return Fail(L"控件 " + _document.Nodes[nodeIndex].Name
							+ L" 的 Language/xml:lang 不是有效的 RFC 3066 标签。",
							error);
					propertyValue = *normalized;
				}
				if (Equals(name, L"Visibility"))
				{
					bool recognized = false;
					propertyValue = NormalizeVisibility(value, recognized);
					if (!recognized) return Fail(L"Visibility 必须为 Visible、Hidden 或 Collapsed。", error);
				}

				DesignerPropertyDescriptor descriptor;
				if (!DesignerPropertyCatalog::TryGetStyleProperty(
					schema.Properties, propertyName, descriptor))
				{
					if (const auto* metadata = schema.FindProperty(propertyName);
						metadata && metadata->IsReadOnly())
						return Fail(L"控件 " + _document.Nodes[nodeIndex].Name
							+ L" 的属性只读：" + name, error);
					return Fail(L"控件 " + _document.Nodes[nodeIndex].Name
						+ L" 不包含可持久化属性：" + name, error);
				}
				if (!assignedProperties.insert(descriptor.Name).second)
					return Fail(L"属性重复：" + descriptor.Name, error);
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
					DesignerStyleValue effective;
					std::wstring captureError;
					if (!descriptor.Metadata
						|| !DesignerPropertyCatalog::CaptureDefaultValue(
							*descriptor.Metadata, effective, &captureError))
						return Fail(L"控件 " + _document.Nodes[nodeIndex].Name
							+ L" 的属性 " + name + L"：" + captureError, error);
					StoreMetadata(_document.Nodes[nodeIndex], descriptor.Name,
						effective, {}, dynamicResourceKey);
					continue;
				}
				DesignerStyleValue typed = resourceValue
					? *resourceValue
					: DesignerStyleValue{
						descriptor.ValueKind,
						NormalizePropertyText(name, propertyValue, descriptor) };
				DesignerStyleValue effective;
				std::wstring applyError;
				if (!descriptor.Metadata
					|| !DesignerPropertyCatalog::NormalizeStyleValue(
						*descriptor.Metadata, typed, effective, &applyError,
					_options.ResourceBasePath, _document.Resources))
					return Fail(L"控件 " + _document.Nodes[nodeIndex].Name
						+ L" 的属性 " + name + L"：" + applyError, error);
				if (_document.Nodes[nodeIndex].Type == UIClass::UI_DataGrid
					&& Equals(descriptor.Name, L"FrozenColumnCount"))
				{
					int count = 0;
					if (!TryParseInteger(effective.Text, count) || count < 0)
						return Fail(L"控件 " + _document.Nodes[nodeIndex].Name
							+ L" 的 FrozenColumnCount 不能为负数。", error);
				}
				StoreMetadata(
					_document.Nodes[nodeIndex], descriptor.Name, effective,
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
				L"HorizontalAlignment", { L"Left", L"Center", L"Right", L"Stretch" }))
				return *value;
			if (const auto value = enumValue(
				L"VerticalAlignment", { L"Top", L"Center", L"Bottom", L"Stretch" }))
				return *value;
			if (const auto value = enumValue(
				L"DockPanel.Dock", { L"Left", L"Top", L"Right", L"Bottom" }))
				return *value;
			if (const auto value = enumValue(
				L"TabStripPlacement", { L"Left", L"Top", L"Right", L"Bottom" }))
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
			node.Properties.Set(propertyName,
				{ value, resourceKey, dynamicResourceKey });
		}

		bool StoreLiteralProperty(
			size_t nodeIndex,
			const CuiRuntime::XamlTypePropertySchema& schema,
			const std::wstring& propertyName,
			const std::wstring& text,
			std::wstring& error)
		{
			DesignerPropertyDescriptor descriptor;
			if (!DesignerPropertyCatalog::TryGetStyleProperty(
				schema.Properties, propertyName, descriptor)
				|| !descriptor.Metadata)
				return Fail(L"控件 " + _document.Nodes[nodeIndex].Name
					+ L" 不包含可持久化属性：" + propertyName, error);
			DesignerStyleValue canonical;
			std::wstring conversionError;
			if (!DesignerPropertyCatalog::NormalizeStyleValue(
				*descriptor.Metadata,
				{ descriptor.ValueKind, text }, canonical, &conversionError,
				_currentResourceBasePath, _document.Resources))
				return Fail(L"控件 " + _document.Nodes[nodeIndex].Name
					+ L" 的属性 " + propertyName + L"：" + conversionError, error);
			if (_document.Nodes[nodeIndex].Type == UIClass::UI_DataGrid
				&& Equals(descriptor.Name, L"FrozenColumnCount"))
			{
				int count = 0;
				if (!TryParseInteger(canonical.Text, count) || count < 0)
					return Fail(L"控件 " + _document.Nodes[nodeIndex].Name
						+ L" 的 FrozenColumnCount 不能为负数。", error);
			}
			StoreMetadata(
				_document.Nodes[nodeIndex], descriptor.Name, canonical);
			return true;
		}

		bool ApplyDirectText(
			const Element& element,
			size_t nodeIndex,
			const CuiRuntime::XamlTypePropertySchema& schema,
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
				if (_document.Nodes[nodeIndex].Bindings.contains(L"Content")
					|| _document.Nodes[nodeIndex].Properties.Find(L"Content"))
					return Fail(L"属性重复：Content", error);
				return StoreLiteralProperty(
					nodeIndex, schema, L"Content", text, error);
			}
			if (_document.Nodes[nodeIndex].Bindings.contains(L"Text")) return true;
			if (_document.Nodes[nodeIndex].Properties.Find(L"Text")) return true;

			const auto properties = DesignerPropertyCatalog::GetStyleProperties(
				schema.Properties);
			const auto* descriptor = DesignerPropertyCatalog::Find(properties, L"Text");
			if (!descriptor) return Fail(L"该控件不支持文本内容。", error);
			DesignerStyleValue effective;
			std::wstring applyError;
			if (!descriptor->Metadata
				|| !DesignerPropertyCatalog::NormalizeStyleValue(
					*descriptor->Metadata,
					{ descriptor->ValueKind, text }, effective, &applyError,
				_options.ResourceBasePath))
				return Fail(applyError, error);
			StoreMetadata(_document.Nodes[nodeIndex], descriptor->Name, effective);
			return true;
		}

		bool ValidateAttributes(
			const Element& element,
			std::initializer_list<const wchar_t*> allowed,
			std::wstring& error,
			bool allowResourceKey = false,
			bool allowXmlLanguage = false)
		{
			for (const auto& attribute : element->Attributes())
			{
				if (!attribute || IsNamespaceAttribute(*attribute)) continue;
				DiagnosticContext attributeContext(*this, element, attribute.get());
				const auto name = FromUtf8(attribute->LocalName());
				const auto prefix = FromUtf8(attribute->Prefix());
				if (Equals(prefix, L"xml") && Equals(name, L"space"))
				{
					const auto value = FromUtf8(attribute->Value());
					if (value != L"default" && value != L"preserve")
						return Fail(L"xml:space 只允许 default 或 preserve。",
							error);
					continue;
				}
				if (Equals(prefix, L"xml") && Equals(name, L"lang"))
				{
					if (allowXmlLanguage) continue;
					return Fail(FromUtf8(element->LocalName())
						+ L" 不支持属性：xml:lang", error);
				}
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

		bool StoreStructureValue(
			size_t nodeIndex,
			const char* key,
			DesignValue value,
			std::wstring& error)
		{
			if (nodeIndex >= _document.Nodes.size())
				return Fail(L"结构属性目标不存在。", error);
			auto& node = _document.Nodes[nodeIndex];
			auto encoded = EncodeDesignNodeStructure(node.Type, node.Structure);
			encoded[key] = std::move(value);
			DesignNodeStructure decoded;
			std::wstring structuralError;
			if (!DecodeDesignNodeStructure(
				node.Type, encoded, decoded, &structuralError))
				return Fail(L"结构属性 " + FromUtf8(key) + L" 无效："
					+ structuralError, error);
			node.Structure = std::move(decoded);
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
			else return Fail(L"Brush 属性仅支持 SolidColorBrush、"
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
				return Fail(L"Brush 属性必须且只能包含一个画刷。", error);
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

		bool TryParseDataGridColumnMultiBinding(
			const Element& propertyElement,
			DesignerDataBinding& binding,
			std::wstring& error)
		{
			if (!ValidateAttributes(propertyElement, {}, error)) return false;
			if (!DirectText(propertyElement).empty())
				return Fail(L"DataGrid 列 MultiBinding 属性不允许文本内容。", error);
			const auto children = ChildElements(propertyElement);
			if (children.size() != 1
				|| !Equals(FromUtf8(children.front()->LocalName()), L"MultiBinding"))
				return Fail(L"DataGrid 列 Binding 属性必须包含一个 MultiBinding。",
					error);
			const auto& multiElement = children.front();
			DiagnosticContext multiContext(*this, multiElement);
			if (!ValidateAttributes(multiElement,
				{ L"Mode", L"UpdateSourceTrigger", L"Converter",
				  L"ConverterParameter", L"StringFormat", L"FallbackValue",
				  L"TargetNullValue" }, error)) return false;
			binding = {};
			if (const auto mode = Attribute(multiElement, L"Mode"); mode
				&& !DesignerBindingUtils::TryParseBindingMode(*mode, binding.Mode))
				return Fail(L"DataGrid 列 MultiBinding Mode 无效：" + *mode, error);
			if (const auto update = Attribute(
				multiElement, L"UpdateSourceTrigger"))
			{
				auto normalized = *update;
				if (Equals(normalized, L"PropertyChanged"))
					normalized = L"OnPropertyChanged";
				else if (Equals(normalized, L"LostFocus")
					|| Equals(normalized, L"Validation"))
					normalized = L"OnValidation";
				else if (Equals(normalized, L"Explicit")) normalized = L"Never";
				if (!DesignerBindingUtils::TryParseUpdateMode(
					normalized, binding.UpdateMode))
					return Fail(L"DataGrid 列 MultiBinding UpdateSourceTrigger 无效："
						+ *update, error);
			}
			binding.Converter = Attribute(multiElement, L"Converter").value_or(L"");
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
					return Fail(L"DataGrid 列 MultiBinding 只能包含 Binding 子项。",
						error);
				DesignerDataBinding childBinding;
				if (!ParseBindingObjectElement(child, childBinding, error)
					|| !ResolveBindingAncestorType(child, childBinding, error))
					return false;
				binding.ChildBindings.push_back(std::move(childBinding));
			}
			std::wstring bindingError;
			if (!DesignerBindingUtils::ValidateDataGridColumnBindingSource(
				binding, nullptr, &bindingError))
				return Fail(bindingError, error);
			return true;
		}

		bool ParseDataGridColumns(
			const Element& property,
			DesignValue& output,
			std::wstring& error)
		{
			if (!ValidateAttributes(property, {}, error)) return false;
			if (!DirectText(property).empty())
				return Fail(L"DataGrid.Columns 不允许包含文本内容。", error);
			output = DesignValue::array();
			for (const auto& columnElement : ChildElements(property))
			{
				DiagnosticContext columnContext(*this, columnElement);
				if (!Equals(FromUtf8(columnElement->NamespaceURI()), L"urn:cui"))
					return Fail(L"DataGrid.Columns 仅允许 CUI DataGridColumn。",
						error);
				const auto name = FromUtf8(columnElement->LocalName());
				const bool textColumn = Equals(name, L"DataGridTextColumn");
				const bool checkBoxColumn = Equals(
					name, L"DataGridCheckBoxColumn");
				const bool comboBoxColumn = Equals(
					name, L"DataGridComboBoxColumn");
				const bool hyperlinkColumn = Equals(
					name, L"DataGridHyperlinkColumn");
				const bool templateColumn = Equals(
					name, L"DataGridTemplateColumn");
				if (!textColumn && !checkBoxColumn && !comboBoxColumn
					&& !hyperlinkColumn
					&& !templateColumn)
					return Fail(L"DataGrid.Columns 仅允许 DataGridTextColumn、"
						L"DataGridCheckBoxColumn、DataGridComboBoxColumn、"
						L"DataGridHyperlinkColumn 或 "
						L"DataGridTemplateColumn。", error);
				for (const auto& attribute : columnElement->Attributes())
				{
					if (!attribute || IsNamespaceAttribute(*attribute)) continue;
					if (!FromUtf8(attribute->Prefix()).empty())
						return Fail(name + L" 不支持带命名空间的属性："
							+ FromUtf8(attribute->Name()), error);
				}
				if (templateColumn)
				{
					if (!ValidateAttributes(columnElement,
						{ L"Header", L"HeaderStyle", L"HeaderTemplate",
						  L"CellStyle", L"Width", L"MinWidth", L"MaxWidth",
						  L"IsReadOnly", L"CanUserSort", L"CanUserResize",
						  L"CanUserReorder",
						  L"Visibility",
						  L"SortMemberPath",
						  L"CellTemplate", L"CellEditingTemplate" }, error))
						return false;
				}
				else if (checkBoxColumn)
				{
					if (!ValidateAttributes(columnElement,
						{ L"Header", L"HeaderStyle", L"HeaderTemplate",
						  L"CellStyle", L"Binding", L"Width", L"MinWidth",
						  L"MaxWidth", L"IsReadOnly", L"IsThreeState",
						  L"ElementStyle", L"EditingElementStyle",
						  L"CanUserSort", L"CanUserResize", L"CanUserReorder",
						  L"Visibility",
						  L"SortMemberPath" }, error))
						return false;
				}
				else if (comboBoxColumn)
				{
					if (!ValidateAttributes(columnElement,
						{ L"Header", L"HeaderStyle", L"HeaderTemplate",
						  L"CellStyle", L"ItemsSource", L"DisplayMemberPath",
						  L"SelectedValuePath", L"SelectedItemBinding",
						  L"SelectedValueBinding", L"Width", L"MinWidth",
						  L"ElementStyle", L"EditingElementStyle",
						  L"MaxWidth", L"IsReadOnly", L"CanUserSort",
						  L"CanUserResize", L"CanUserReorder", L"Visibility",
						  L"SortMemberPath" }, error))
						return false;
				}
				else if (hyperlinkColumn)
				{
					if (!ValidateAttributes(columnElement,
						{ L"Header", L"HeaderStyle", L"HeaderTemplate",
						  L"CellStyle", L"Binding", L"ContentBinding",
						  L"TargetName", L"Width", L"MinWidth", L"MaxWidth",
						  L"ElementStyle", L"EditingElementStyle",
						  L"IsReadOnly", L"CanUserSort", L"CanUserResize",
						  L"CanUserReorder", L"Visibility",
						  L"SortMemberPath" }, error))
						return false;
				}
				else if (!ValidateAttributes(columnElement,
					{ L"Header", L"HeaderStyle", L"HeaderTemplate",
					  L"CellStyle", L"Binding", L"Width", L"MinWidth", L"MaxWidth",
					  L"ElementStyle", L"EditingElementStyle",
					  L"IsReadOnly", L"CanUserSort", L"CanUserResize",
					  L"CanUserReorder",
					  L"Visibility",
					  L"SortMemberPath" }, error))
					return false;
				if (!DirectText(columnElement).empty())
					return Fail(name + L" 不允许包含文本内容。", error);
				std::unordered_map<std::wstring, DesignerDataBinding>
					propertyBindings;
				for (const auto& child : ChildElements(columnElement))
				{
					const auto propertyName = FromUtf8(child->LocalName());
					const auto prefix = name + L".";
					if (!propertyName.starts_with(prefix))
						return Fail(name + L" 不支持子元素：" + propertyName,
							error);
					const auto member = propertyName.substr(prefix.size());
					const bool supported = templateColumn ? false
						: comboBoxColumn
							? member == L"SelectedItemBinding"
								|| member == L"SelectedValueBinding"
							: hyperlinkColumn
								? member == L"Binding" || member == L"ContentBinding"
								: member == L"Binding";
					if (!supported)
						return Fail(name + L" 不支持 Binding 属性元素："
							+ member, error);
					if (propertyBindings.contains(member))
						return Fail(name + L" 重复声明 Binding 属性：" + member,
							error);
					DesignerDataBinding propertyBinding;
					if (!TryParseDataGridColumnMultiBinding(
						child, propertyBinding, error)) return false;
					propertyBindings.emplace(member, std::move(propertyBinding));
				}

				DesignValue column = DesignValue::object();
				column["kind"] = textColumn ? "Text"
					: checkBoxColumn ? "CheckBox"
						: comboBoxColumn ? "ComboBox"
							: hyperlinkColumn ? "Hyperlink" : "Template";
				if (const auto header = Attribute(columnElement, L"Header"))
					column["header"] = ToUtf8(*header);
				for (const auto& [attributeName, key, targetType] : {
					std::tuple{ L"HeaderStyle", "headerStyle",
						UIClass::UI_DataGridColumnHeader },
					std::tuple{ L"CellStyle", "cellStyle",
						UIClass::UI_DataGridCell } })
				{
					const auto styleText = Attribute(columnElement, attributeName);
					if (!styleText) continue;
					std::wstring resourceKey;
					if (!TryParseStaticResource(*styleText, resourceKey))
						return Fail(name + L"." + attributeName
							+ L" 必须引用已声明的 Style。", error);
					if (!ValidateVisibleDataGridColumnStyle(
						resourceKey, targetType,
						name + L"." + attributeName, error)) return false;
					column[key] = ToUtf8(resourceKey);
				}
				if (const auto templateText = Attribute(
					columnElement, L"HeaderTemplate"))
				{
					std::wstring resourceKey;
					if (!TryParseStaticResource(*templateText, resourceKey))
						return Fail(name + L".HeaderTemplate 必须引用已声明的 "
							L"DataTemplate。", error);
					const auto* definition = FindVisibleDataTemplate(resourceKey);
					if (!definition)
						return Fail(name + L".HeaderTemplate 引用了未声明的 "
							L"DataTemplate：" + resourceKey, error);
					column["headerTemplate"] = ToUtf8(definition->Key);
				}

				bool validWidth = false;
				column["width"] = DataGridLengthValue(
					Attribute(columnElement, L"Width").value_or(L"SizeToHeader"),
					validWidth);
				if (!validWidth)
					return Fail(name + L".Width 必须是 Auto、SizeToHeader、"
						L"SizeToCells、非负像素值或非负 Star 长度。", error);

				double minimum = 20.0;
				double maximum = (std::numeric_limits<double>::infinity)();
				if (const auto text = Attribute(columnElement, L"MinWidth"))
				{
					if (!TryParseDouble(*text, minimum) || minimum < 0.0)
						return Fail(name + L".MinWidth 必须是非负有限数值。",
							error);
					column["minWidth"] = minimum;
				}
				if (const auto text = Attribute(columnElement, L"MaxWidth"))
				{
					if (!TryParseDouble(*text, maximum) || maximum < minimum)
						return Fail(name + L".MaxWidth 必须是不小于 MinWidth 的"
							L"非负有限数值。", error);
					column["maxWidth"] = maximum;
				}
				if (maximum < minimum)
					return Fail(name + L".MaxWidth 不能小于 MinWidth。", error);

				bool isReadOnly = false;
				bool canUserSort = true;
				bool canUserResize = true;
				bool canUserReorder = true;
				if (!ReadBoolAttribute(columnElement,
					L"IsReadOnly", false, isReadOnly, error)
					|| !ReadBoolAttribute(columnElement,
						L"CanUserSort", true, canUserSort, error)
					|| !ReadBoolAttribute(columnElement,
						L"CanUserResize", true, canUserResize, error)
					|| !ReadBoolAttribute(columnElement,
						L"CanUserReorder", true, canUserReorder, error)) return false;
				if (isReadOnly) column["isReadOnly"] = true;
				if (checkBoxColumn)
				{
					bool isThreeState = false;
					if (!ReadBoolAttribute(columnElement,
						L"IsThreeState", false, isThreeState, error)) return false;
					if (isThreeState) column["isThreeState"] = true;
				}
				if (!canUserSort) column["canUserSort"] = false;
				if (!canUserResize) column["canUserResize"] = false;
				if (!canUserReorder) column["canUserReorder"] = false;
				if (const auto visibility = Attribute(
					columnElement, L"Visibility"))
				{
					if (!Equals(*visibility, L"Visible")
						&& !Equals(*visibility, L"Hidden")
						&& !Equals(*visibility, L"Collapsed"))
						return Fail(name + L".Visibility 必须是 Visible、"
							L"Hidden 或 Collapsed。", error);
					if (!Equals(*visibility, L"Visible"))
						column["visibility"] = ToUtf8(*visibility);
				}
				if (const auto sortPath = Attribute(
					columnElement, L"SortMemberPath"))
				{
					const auto normalized = DesignerBindingUtils::Trim(*sortPath);
					if (!normalized.empty()
						&& !DesignerBindingUtils::IsValidSourcePath(normalized))
						return Fail(name + L".SortMemberPath 无效。", error);
					if (!normalized.empty())
						column["sortMemberPath"] = ToUtf8(normalized);
				}
				if (!templateColumn)
				{
					const UIClass elementType = textColumn
						? UIClass::UI_Label
						: checkBoxColumn ? UIClass::UI_CheckBox
							: comboBoxColumn ? UIClass::UI_ComboBox
								: UIClass::UI_Button;
					const UIClass editingType = textColumn || hyperlinkColumn
						? UIClass::UI_TextBox : elementType;
					for (const auto& [attributeName, key, generatedType] : {
						std::tuple{ L"ElementStyle", "elementStyle", elementType },
						std::tuple{ L"EditingElementStyle", "editingElementStyle",
							editingType } })
					{
						const auto styleText = Attribute(
							columnElement, attributeName);
						if (!styleText) continue;
						std::wstring resourceKey;
						if (!TryParseStaticResource(*styleText, resourceKey))
							return Fail(name + L"." + attributeName
								+ L" 必须引用已声明的 Style。", error);
						if (!ValidateVisibleDataGridColumnStyle(
							resourceKey, generatedType,
							name + L"." + attributeName, error)) return false;
						column[key] = ToUtf8(resourceKey);
					}
				}

				if (comboBoxColumn)
				{
					const auto itemsSourceText = Attribute(
						columnElement, L"ItemsSource");
					std::wstring resourceKey;
					if (!itemsSourceText
						|| !TryParseStaticResource(*itemsSourceText, resourceKey)
						|| (!_document.FindDataList(resourceKey)
							&& !_document.FindCollectionView(resourceKey)))
						return Fail(name + L".ItemsSource 必须引用已声明的 "
							L"DataList 或 CollectionViewSource。", error);
					column["itemsSourceResource"] = ToUtf8(resourceKey);

					const auto selectedItemBinding = Attribute(
						columnElement, L"SelectedItemBinding");
					const auto selectedValueBinding = Attribute(
						columnElement, L"SelectedValueBinding");
					const bool hasSelectedItem = selectedItemBinding.has_value()
						|| propertyBindings.contains(L"SelectedItemBinding");
					const bool hasSelectedValue = selectedValueBinding.has_value()
						|| propertyBindings.contains(L"SelectedValueBinding");
					if (hasSelectedItem == hasSelectedValue
						|| (selectedItemBinding
							&& propertyBindings.contains(L"SelectedItemBinding"))
						|| (selectedValueBinding
							&& propertyBindings.contains(L"SelectedValueBinding")))
						return Fail(name + L" 必须且只能声明 SelectedItemBinding "
							L"或 SelectedValueBinding 之一。", error);
					DesignerDataBinding binding;
					std::wstring bindingError;
					if (hasSelectedValue
						&& propertyBindings.contains(L"SelectedValueBinding"))
						binding = propertyBindings.at(L"SelectedValueBinding");
					else if (hasSelectedItem
						&& propertyBindings.contains(L"SelectedItemBinding"))
						binding = propertyBindings.at(L"SelectedItemBinding");
					else
					{
						const auto& bindingText = selectedValueBinding
							? *selectedValueBinding : *selectedItemBinding;
						if (!TryParseDataGridColumnBinding(
							bindingText, binding, bindingError))
						return Fail(name + L"."
							+ (hasSelectedValue
								? L"SelectedValueBinding："
								: L"SelectedItemBinding：")
							+ bindingError, error);
					}
					column["binding"] =
						DesignerBindingUtils::WriteBindingDefinition(binding);
					if (hasSelectedValue)
						column["selectionBinding"] = "SelectedValue";

					for (const auto& [attributeName, key] : {
						std::pair{ L"DisplayMemberPath", "displayMemberPath" },
						std::pair{ L"SelectedValuePath", "selectedValuePath" } })
					{
						const auto authored = Attribute(columnElement, attributeName);
						if (!authored) continue;
						const auto path = DesignerBindingUtils::Trim(*authored);
						if (!path.empty()
							&& !DesignerBindingUtils::IsValidSourcePath(path))
							return Fail(name + L"." + attributeName + L" 无效。", error);
						if (!path.empty()) column[key] = ToUtf8(path);
					}
				}
				else if (!templateColumn)
				{
					const auto bindingText = Attribute(columnElement, L"Binding");
					const auto propertyBinding = propertyBindings.find(L"Binding");
					if (!bindingText && propertyBinding == propertyBindings.end())
						return Fail(name + L" 必须声明 Binding。", error);
					if (bindingText && propertyBinding != propertyBindings.end())
						return Fail(name + L".Binding 不能同时使用属性和属性元素。",
							error);
					DesignerDataBinding binding;
					std::wstring bindingError;
					if (propertyBinding != propertyBindings.end())
						binding = propertyBinding->second;
					else if (!TryParseDataGridColumnBinding(
						*bindingText, binding, bindingError))
						return Fail(name + L".Binding：" + bindingError, error);
					column["binding"] =
						DesignerBindingUtils::WriteBindingDefinition(binding);
					if (hyperlinkColumn)
					{
						const auto contentBindingText = Attribute(
							columnElement, L"ContentBinding");
						const auto contentProperty =
							propertyBindings.find(L"ContentBinding");
						if (contentBindingText
							&& contentProperty != propertyBindings.end())
							return Fail(name + L".ContentBinding 不能同时使用属性和"
								L"属性元素。", error);
						if (contentBindingText
							|| contentProperty != propertyBindings.end())
						{
							DesignerDataBinding contentBinding;
							if (contentProperty != propertyBindings.end())
								contentBinding = contentProperty->second;
							else if (!TryParseDataGridColumnBinding(
								*contentBindingText, contentBinding, bindingError))
								return Fail(name + L".ContentBinding："
									+ bindingError, error);
							column["contentBinding"] =
								DesignerBindingUtils::WriteBindingDefinition(
									contentBinding);
						}
						if (const auto targetName = Attribute(
							columnElement, L"TargetName"))
						{
							const auto normalized = Trim(*targetName);
							if (!normalized.empty())
								column["targetName"] = ToUtf8(normalized);
						}
					}
				}
				else
				{
					for (const auto& [attributeName, key] : {
						std::pair{ L"CellTemplate", "cellTemplate" },
						std::pair{ L"CellEditingTemplate", "cellEditingTemplate" } })
					{
						const auto templateText = Attribute(
							columnElement, attributeName);
						if (!templateText) continue;
						std::wstring resourceKey;
						if (!TryParseStaticResource(*templateText, resourceKey))
							return Fail(name + L"." + attributeName
								+ L" 必须引用已声明的 DataTemplate。", error);
						const auto* definition = FindVisibleDataTemplate(resourceKey);
						if (!definition)
							return Fail(name + L"." + attributeName
								+ L" 引用了未声明的 DataTemplate：" + resourceKey,
								error);
						column[key] = ToUtf8(definition->Key);
					}
				}
				output.push_back(std::move(column));
			}
			return true;
		}

		bool ParseRichTextFormatting(
			const Element& element,
			bool allowText,
			bool allowTextAlignment,
			DesignValue& output,
			std::wstring& error)
		{
			if (allowText)
			{
				if (!ValidateAttributes(element,
					{ L"Text", L"Foreground", L"Background", L"FontFamily",
					  L"Language", L"FontSize", L"FontWeight", L"FontStretch",
					  L"FontStyle", L"Underline", L"Strikethrough" },
					error, false, true)) return false;
			}
			else if (allowTextAlignment)
			{
				if (!ValidateAttributes(element,
					{ L"Foreground", L"Background", L"FontFamily", L"Language", L"FontSize",
					  L"FontWeight", L"FontStretch", L"FontStyle", L"Underline", L"Strikethrough",
					  L"TextAlignment", L"FlowDirection" }, error, false, true)) return false;
			}
			else if (!ValidateAttributes(element,
				{ L"Foreground", L"Background", L"FontFamily", L"Language", L"FontSize",
				  L"FontWeight", L"FontStretch", L"FontStyle", L"Underline", L"Strikethrough" },
				error, false, true)) return false;

			for (const auto& [attributeName, key] : {
				std::pair{ L"Foreground", "foreground" },
				std::pair{ L"Background", "background" } })
			{
				if (const auto text = Attribute(element, attributeName))
					if (!ParseBrushColor(*text, output[key], error)) return false;
			}
			if (const auto family = Attribute(element, L"FontFamily"))
			{
				const auto normalized = Trim(*family);
				if (normalized.empty())
					return Fail(L"FontFamily 不能为空。", error);
				output["fontFamily"] = ToUtf8(normalized);
			}
			const auto languageProperty = Attribute(element, L"Language");
			const auto xmlLanguage = Attribute(element, L"lang", L"xml");
			if (languageProperty && xmlLanguage)
				return Fail(L"Language 与 xml:lang 不能同时设置。", error);
			if (languageProperty || xmlLanguage)
			{
				const auto normalized = NormalizeRichTextLanguageTag(
					Trim(languageProperty ? *languageProperty : *xmlLanguage));
				if (!normalized)
					return Fail(L"Language/xml:lang 不是有效的 RFC 3066 标签。",
						error);
				output["language"] = ToUtf8(*normalized);
			}
			if (const auto text = Attribute(element, L"FontSize"))
			{
				double value = 0.0;
				if (!TryParseDouble(*text, value)
					|| value < (1.0 / 300.0) || value > 160000.0)
					return Fail(
						L"FontSize 必须位于 1/300 到 160000 之间。", error);
				output["fontSize"] = value;
			}
			auto canonicalName = [&](const std::wstring& value,
				std::initializer_list<const wchar_t*> names)
				-> std::optional<std::wstring>
			{
				const auto normalized = Lower(Trim(value));
				for (const auto* name : names)
					if (Lower(name) == normalized) return std::wstring(name);
				return std::nullopt;
			};
			if (const auto text = Attribute(element, L"FontWeight"))
			{
				const auto value = canonicalName(*text,
					{ L"Thin", L"ExtraLight", L"UltraLight", L"Light",
					  L"SemiLight", L"Normal", L"Regular", L"Medium",
					  L"DemiBold", L"SemiBold", L"Bold", L"ExtraBold",
					  L"UltraBold", L"Black", L"Heavy", L"ExtraBlack",
					  L"UltraBlack" });
				if (!value) return Fail(L"FontWeight 值无效：" + *text, error);
				output["fontWeight"] = ToUtf8(*value);
			}
			if (const auto text = Attribute(element, L"FontStretch"))
			{
				const auto value = canonicalName(*text,
					{ L"UltraCondensed", L"ExtraCondensed", L"Condensed",
					  L"SemiCondensed", L"Normal", L"Medium",
					  L"SemiExpanded", L"Expanded", L"ExtraExpanded",
					  L"UltraExpanded" });
				if (!value) return Fail(L"FontStretch 值无效：" + *text, error);
				output["fontStretch"] = ToUtf8(*value);
			}
			if (const auto text = Attribute(element, L"FontStyle"))
			{
				const auto value = canonicalName(
					*text, { L"Normal", L"Oblique", L"Italic" });
				if (!value) return Fail(L"FontStyle 值无效：" + *text, error);
				output["fontStyle"] = ToUtf8(*value);
			}
			if (allowTextAlignment)
			{
				if (const auto text = Attribute(element, L"TextAlignment"))
				{
					const auto value = canonicalName(*text,
						{ L"Left", L"Right", L"Center", L"Justify" });
					if (!value)
						return Fail(L"TextAlignment 值无效：" + *text, error);
					output["textAlignment"] = ToUtf8(*value);
				}
				if (const auto text = Attribute(element, L"FlowDirection"))
				{
					const auto value = canonicalName(*text,
						{ L"LeftToRight", L"RightToLeft" });
					if (!value)
						return Fail(L"FlowDirection 值无效：" + *text, error);
					output["flowDirection"] = ToUtf8(*value);
				}
			}
			for (const auto& [attributeName, key] : {
				std::pair{ L"Underline", "underline" },
				std::pair{ L"Strikethrough", "strikethrough" } })
			{
				if (const auto text = Attribute(element, attributeName))
				{
					bool value = false;
					if (!TryParseBool(*text, value))
						return Fail(std::wstring(attributeName)
							+ L" 必须是布尔值。", error);
					output[key] = value;
				}
			}
			return true;
		}

		bool IsCuiRichTextElement(
			const Element& element,
			const std::wstring& name) const
		{
			if (!element || !Equals(FromUtf8(element->LocalName()), name))
				return false;
			const auto xamlNamespace = FromUtf8(element->NamespaceURI());
			const auto separator = name.find(L'.');
			if (separator == std::wstring::npos)
				return CuiRuntime::XamlRuntimeSchema::FindNonVisualType(
					xamlNamespace, name) != nullptr;
			return CuiRuntime::XamlRuntimeSchema::FindObjectMember(
				xamlNamespace, name.substr(0, separator),
				name.substr(separator + 1)) != nullptr;
		}

		bool ParseRichTextRun(
			const Element& element,
			DesignValue& output,
			std::wstring& error)
		{
			DiagnosticContext context(*this, element);
			if (!IsCuiRichTextElement(element, L"Run"))
				return Fail(L"Inline 内容必须使用 CUI Run。", error);
			output = DesignValue::object();
			output["kind"] = "Run";
			if (!ParseRichTextFormatting(
				element, true, false, output, error)) return false;
			const auto textAttribute = Attribute(element, L"Text");
			const auto children = ChildElements(element);
			std::wstring text;
			if (children.empty())
			{
				const auto directText = RawDirectText(element);
				if (textAttribute && !directText.empty())
					return Fail(L"Run.Text 属性不能与直接文本内容同时使用。", error);
				text = textAttribute.value_or(directText);
			}
			else
			{
				if (children.size() != 1
					|| !IsCuiRichTextElement(children.front(), L"Run.Text"))
					return Fail(L"Run 仅允许 Text 属性元素，不能嵌套其他对象。", error);
				if (textAttribute || !RawDirectText(element).empty())
					return Fail(L"Run.Text 不能重复或与直接文本内容混用。", error);
				const auto& property = children.front();
				if (!ValidateAttributes(property, {}, error)
					|| !ChildElements(property).empty())
					return Fail(L"Run.Text 只能包含文本。", error);
				text = RawDirectText(property);
			}
			output["text"] = ToUtf8(text);
			return true;
		}

		bool ParseRichTextInline(
			const Element& element,
			DesignValue& output,
			std::wstring& error)
		{
			const auto localName = FromUtf8(element->LocalName());
			if (Equals(localName, L"Run"))
				return ParseRichTextRun(element, output, error);
			if (Equals(localName, L"LineBreak"))
			{
				DiagnosticContext context(*this, element);
				if (!IsCuiRichTextElement(element, L"LineBreak"))
					return Fail(L"Inline 内容必须使用 CUI LineBreak。", error);
				output = DesignValue::object();
				output["kind"] = "LineBreak";
				if (!ParseRichTextFormatting(
					element, false, false, output, error)) return false;
				if (!ChildElements(element).empty()
					|| !RawDirectText(element).empty())
				{
					return Fail(
						L"LineBreak 不能包含文本或子元素。", error);
				}
				return true;
			}
			const bool supported = Equals(localName, L"Span")
				|| Equals(localName, L"Bold")
				|| Equals(localName, L"Italic")
				|| Equals(localName, L"Underline");
			if (!supported || !IsCuiRichTextElement(element, localName))
				return Fail(
					L"Inline 内容仅允许 CUI Run/LineBreak/Span/Bold/Italic/Underline。",
					error);

			DiagnosticContext context(*this, element);
			output = DesignValue::object();
			output["kind"] = ToUtf8(localName);
			if (!ParseRichTextFormatting(
				element, false, false, output, error)) return false;

			auto appendImplicitRun = [&](DesignValue& inlines,
				const std::wstring& text)
			{
				if (text.empty()) return;
				inlines.push_back(DesignValue{
					{ "kind", "Run" }, { "text", ToUtf8(text) } });
			};
			auto parseContent = [&](const auto& self,
				const Element& container,
				DesignValue& inlines) -> bool
			{
				for (std::size_t childIndex = 0;
					childIndex < container->ChildNodes().size(); ++childIndex)
				{
					const auto& child = container->ChildNodes()[childIndex];
					if (!child) continue;
					switch (child->NodeType())
					{
					case XmlNodeType::Comment:
						continue;
					case XmlNodeType::Whitespace:
					case XmlNodeType::SignificantWhitespace:
						if (PreservesXmlWhitespace(container))
							appendImplicitRun(
								inlines, FromUtf8(child->Value()));
						else if (IsNormalizedInlineSeparator(
							container, childIndex))
							appendImplicitRun(inlines, L" ");
						continue;
					case XmlNodeType::Text:
					case XmlNodeType::CDATA:
						appendImplicitRun(inlines, FromUtf8(child->Value()));
						continue;
					case XmlNodeType::Element:
					{
						auto childElement =
							std::static_pointer_cast<XmlElement>(child);
						DesignValue inlineValue;
						if (!ParseRichTextInline(
							childElement, inlineValue, error)) return false;
						inlines.push_back(std::move(inlineValue));
						continue;
					}
					default:
						return Fail(L"Inline 内容包含不支持的 XML 节点。", error);
					}
				}
				return true;
			};

			Element propertyElement;
			bool hasDirectContent = false;
			const auto propertyName = localName + L".Inlines";
			for (const auto& child : element->ChildNodes())
			{
				if (!child) continue;
				if (child->NodeType() == XmlNodeType::Element)
				{
					auto childElement =
						std::static_pointer_cast<XmlElement>(child);
					if (IsCuiRichTextElement(childElement, propertyName))
					{
						if (propertyElement)
							return Fail(propertyName + L" 不能重复。", error);
						propertyElement = std::move(childElement);
					}
					else hasDirectContent = true;
				}
				else if (child->NodeType() == XmlNodeType::Text
					|| child->NodeType() == XmlNodeType::CDATA
					|| (PreservesXmlWhitespace(element)
						&& (child->NodeType() == XmlNodeType::Whitespace
							|| child->NodeType()
								== XmlNodeType::SignificantWhitespace)))
					hasDirectContent = hasDirectContent
						|| !FromUtf8(child->Value()).empty();
			}
			if (propertyElement && hasDirectContent)
				return Fail(propertyName
					+ L" 不能与直接 Inline/文本内容混用。", error);

			DesignValue inlines = DesignValue::array();
			if (propertyElement)
			{
				if (!ValidateAttributes(propertyElement, {}, error)) return false;
				if (!parseContent(parseContent, propertyElement, inlines))
					return false;
			}
			else if (!parseContent(parseContent, element, inlines))
				return false;
			output["inlines"] = std::move(inlines);
			return true;
		}

		bool ParseRichTextParagraph(
			const Element& element,
			DesignValue& output,
			std::wstring& error)
		{
			DiagnosticContext context(*this, element);
			if (!IsCuiRichTextElement(element, L"Paragraph"))
				return Fail(L"FlowDocument.Blocks 仅允许 CUI Paragraph。", error);
			output = DesignValue::object();
			if (!ParseRichTextFormatting(
				element, false, true, output, error)) return false;
			DesignValue inlines = DesignValue::array();
			auto appendImplicitRun = [&](DesignValue& target,
				const std::wstring& text)
			{
				if (!text.empty()) target.push_back(DesignValue{
					{ "kind", "Run" }, { "text", ToUtf8(text) } });
			};
			auto parseContent = [&](const Element& container) -> bool
			{
				for (std::size_t childIndex = 0;
					childIndex < container->ChildNodes().size(); ++childIndex)
				{
					const auto& child = container->ChildNodes()[childIndex];
					if (!child) continue;
					if (child->NodeType() == XmlNodeType::Comment) continue;
					if (child->NodeType() == XmlNodeType::Whitespace
						|| child->NodeType()
							== XmlNodeType::SignificantWhitespace)
					{
						if (PreservesXmlWhitespace(container))
							appendImplicitRun(
								inlines, FromUtf8(child->Value()));
						else if (IsNormalizedInlineSeparator(
							container, childIndex))
							appendImplicitRun(inlines, L" ");
						continue;
					}
					if (child->NodeType() == XmlNodeType::Text
						|| child->NodeType() == XmlNodeType::CDATA)
					{
						appendImplicitRun(
							inlines, FromUtf8(child->Value()));
						continue;
					}
					if (child->NodeType() != XmlNodeType::Element)
						return Fail(
							L"Paragraph.Inlines 包含不支持的 XML 节点。", error);
					DesignValue inlineValue;
					if (!ParseRichTextInline(
						std::static_pointer_cast<XmlElement>(child),
						inlineValue, error)) return false;
					inlines.push_back(std::move(inlineValue));
				}
				return true;
			};
			Element inlinesProperty;
			bool directContent = false;
			for (const auto& child : element->ChildNodes())
			{
				if (!child) continue;
				if (child->NodeType() == XmlNodeType::Element)
				{
					auto childElement =
						std::static_pointer_cast<XmlElement>(child);
					if (IsCuiRichTextElement(
						childElement, L"Paragraph.Inlines"))
					{
						if (inlinesProperty)
							return Fail(L"Paragraph.Inlines 不能重复。", error);
						inlinesProperty = std::move(childElement);
					}
					else directContent = true;
				}
				else if (child->NodeType() == XmlNodeType::Text
					|| child->NodeType() == XmlNodeType::CDATA
					|| (PreservesXmlWhitespace(element)
						&& (child->NodeType() == XmlNodeType::Whitespace
							|| child->NodeType()
								== XmlNodeType::SignificantWhitespace)))
					directContent = directContent
						|| !FromUtf8(child->Value()).empty();
			}
			if (inlinesProperty && directContent)
				return Fail(
					L"Paragraph.Inlines 不能与直接内容混用。", error);
			if (inlinesProperty
				&& !ValidateAttributes(inlinesProperty, {}, error)) return false;
			if (!parseContent(inlinesProperty ? inlinesProperty : element))
				return false;
			output["inlines"] = std::move(inlines);
			return true;
		}

		bool ParseFlowDocument(
			const Element& element,
			DesignValue& output,
			std::wstring& error)
		{
			DiagnosticContext context(*this, element);
			if (!IsCuiRichTextElement(element, L"FlowDocument"))
				return Fail(L"RichTextBox.Document 必须是 CUI FlowDocument。", error);
			output = DesignValue::object();
			if (!ParseRichTextFormatting(
				element, false, true, output, error)) return false;
			if (!DirectText(element).empty())
				return Fail(L"FlowDocument 不接受直接文本；请使用 Paragraph/Run。",
					error);

			DesignValue paragraphs = DesignValue::array();
			bool usedBlocksProperty = false;
			bool usedDirectParagraphs = false;
			for (const auto& child : ChildElements(element))
			{
				DiagnosticContext childContext(*this, child);
				if (IsCuiRichTextElement(child, L"FlowDocument.Blocks"))
				{
					if (usedBlocksProperty || usedDirectParagraphs)
						return Fail(L"FlowDocument.Blocks 不能重复或与直接 Paragraph 混用。",
							error);
					usedBlocksProperty = true;
					if (!ValidateAttributes(child, {}, error)
						|| !DirectText(child).empty())
						return Fail(L"FlowDocument.Blocks 只能包含 Paragraph。", error);
					for (const auto& paragraphElement : ChildElements(child))
					{
						DesignValue paragraph;
						if (!ParseRichTextParagraph(
							paragraphElement, paragraph, error)) return false;
						paragraphs.push_back(std::move(paragraph));
					}
					continue;
				}
				if (usedBlocksProperty)
					return Fail(L"FlowDocument.Blocks 不能与直接 Paragraph 混用。",
						error);
				usedDirectParagraphs = true;
				DesignValue paragraph;
				if (!ParseRichTextParagraph(child, paragraph, error)) return false;
				paragraphs.push_back(std::move(paragraph));
			}
			output["paragraphs"] = std::move(paragraphs);
			return true;
		}

		bool ParseCommandBindings(
			const Element& property,
			std::vector<DesignCommandBinding>& output,
			std::wstring& error)
		{
			if (!ValidateAttributes(property, {}, error)) return false;
			for (const auto& item : ChildElements(property))
			{
				DiagnosticContext itemContext(*this, item);
				if (!Equals(FromUtf8(item->LocalName()), L"CommandBinding"))
					return Fail(L"CommandBindings 仅允许 CommandBinding。", error);
				if (!ValidateAttributes(item,
					{ L"Command", L"PreviewCanExecute", L"CanExecute",
					  L"PreviewExecuted", L"Executed" }, error)) return false;
				if (!ChildElements(item).empty()
					|| !Trim(FromUtf8(item->InnerText())).empty())
					return Fail(L"CommandBinding 不允许子元素或文本。", error);
				DesignCommandBinding binding;
				binding.Command = Trim(Attribute(item, L"Command").value_or(L""));
				if (binding.Command.empty())
					return Fail(L"CommandBinding.Command 不能为空。", error);
				auto readHandler = [&](const wchar_t* name,
					std::wstring& target) -> bool
				{
					const auto value = Attribute(item, name);
					if (!value) return true;
					std::wstring handlerError;
					if (!NormalizeHandler(*value, target, handlerError))
						return Fail(L"CommandBinding." + std::wstring(name)
							+ L"：" + handlerError, error);
					return true;
				};
				if (!readHandler(L"PreviewCanExecute", binding.PreviewCanExecute)
					|| !readHandler(L"CanExecute", binding.CanExecute)
					|| !readHandler(L"PreviewExecuted", binding.PreviewExecuted)
					|| !readHandler(L"Executed", binding.Executed)) return false;
				if (binding.PreviewCanExecute.empty() && binding.CanExecute.empty()
					&& binding.PreviewExecuted.empty() && binding.Executed.empty())
					return Fail(L"CommandBinding 至少需要一个处理器。", error);
				output.push_back(std::move(binding));
			}
			return true;
		}

		bool ParseInputBindings(
			const Element& property,
			std::vector<DesignInputBinding>& output,
			std::wstring& error)
		{
			if (!ValidateAttributes(property, {}, error)) return false;
			std::unordered_set<std::wstring> gestures;
			for (const auto& existing : output)
				gestures.insert((existing.Kind == DesignInputBindingKind::Mouse
					? L"mouse:" : L"key:") + Lower(existing.Gesture));
			for (const auto& item : ChildElements(property))
			{
				DiagnosticContext itemContext(*this, item);
				const auto itemName = FromUtf8(item->LocalName());
				const bool isKey = Equals(itemName, L"KeyBinding");
				const bool isMouse = Equals(itemName, L"MouseBinding");
				if (!isKey && !isMouse)
					return Fail(
						L"InputBindings 仅允许 KeyBinding 或 MouseBinding。", error);
				if (isKey)
				{
					if (!ValidateAttributes(item,
						{ L"Command", L"Gesture", L"Key", L"Modifiers",
						  L"CommandParameter", L"CommandTarget" }, error)) return false;
				}
				else if (!ValidateAttributes(item,
					{ L"Command", L"Gesture", L"MouseAction", L"Modifiers",
					  L"CommandParameter", L"CommandTarget" }, error)) return false;
				if (!ChildElements(item).empty()
					|| !Trim(FromUtf8(item->InnerText())).empty())
					return Fail(itemName + L" 不允许子元素或文本。", error);
				DesignInputBinding binding;
				binding.Kind = isMouse
					? DesignInputBindingKind::Mouse : DesignInputBindingKind::Key;
				binding.Command = Trim(Attribute(item, L"Command").value_or(L""));
				binding.CommandParameter =
					Attribute(item, L"CommandParameter").value_or(L"");
				if (!TryParseCommandTargetReference(
					Attribute(item, L"CommandTarget").value_or(L""),
					binding.CommandTarget, error)) return false;
				if (binding.Command.empty())
					return Fail(itemName + L".Command 不能为空。", error);
				const auto explicitGesture = Attribute(item, L"Gesture");
				const auto action = Attribute(item,
					isKey ? L"Key" : L"MouseAction");
				const auto modifiers = Attribute(item, L"Modifiers");
				if (explicitGesture && (action || modifiers))
					return Fail(itemName
						+ L".Gesture 不能与动作/Modifiers 同时使用。", error);
				if (explicitGesture) binding.Gesture = Trim(*explicitGesture);
				else
				{
					if (!action || Trim(*action).empty())
						return Fail(itemName
							+ L" 必须声明 Gesture 或动作。", error);
					auto modifierText = Trim(modifiers.value_or(L""));
					std::replace(modifierText.begin(), modifierText.end(), L',', L'+');
					binding.Gesture = modifierText.empty()
						? Trim(*action) : modifierText + L"+" + Trim(*action);
				}
				std::wstring gestureError;
				if (isKey)
				{
					KeyGesture gesture;
					if (!TryParseKeyGesture(binding.Gesture, gesture, &gestureError))
						return Fail(L"KeyBinding.Gesture：" + gestureError, error);
					binding.Gesture = FormatKeyGesture(gesture);
				}
				else
				{
					MouseGesture gesture;
					if (!TryParseMouseGesture(binding.Gesture, gesture, &gestureError))
						return Fail(L"MouseBinding.Gesture：" + gestureError, error);
					binding.Gesture = FormatMouseGesture(gesture);
				}
				const auto gestureIdentity = (isMouse ? L"mouse:" : L"key:")
					+ Lower(binding.Gesture);
				if (!gestures.insert(gestureIdentity).second)
					return Fail(L"InputBindings 包含重复手势："
						+ binding.Gesture, error);
				output.push_back(std::move(binding));
			}
			return true;
		}

		bool TryParseStructuredProperty(
			const Element& property,
			size_t nodeIndex,
			UIClass type,
			const CuiRuntime::XamlTypePropertySchema& schema,
			bool& handled,
			std::wstring& error)
		{
			handled = false;
			const auto name = FromUtf8(property->LocalName());
			auto storeCanonical = [&](const std::wstring& propertyName,
				DesignerStyleValue value) -> bool
			{
				const auto* metadata = schema.FindProperty(propertyName);
				DesignerStyleValue canonical;
				std::wstring conversionError;
				if (!metadata || !DesignerPropertyCatalog::NormalizeStyleValue(
					*metadata, value, canonical, &conversionError,
					_currentResourceBasePath, _document.Resources))
					return Fail(L"属性元素 " + propertyName
						+ L" 无法通过 Schema 规范化：" + conversionError, error);
				StoreMetadata(
					_document.Nodes[nodeIndex], propertyName, canonical);
				return true;
			};
			const auto encodedStructure = EncodeDesignNodeStructure(
				_document.Nodes[nodeIndex].Type,
				_document.Nodes[nodeIndex].Structure);
			auto beginCollection = [&](const char* key) -> bool
			{
				handled = true;
				if (encodedStructure.contains(key))
					return Fail(L"属性元素重复：" + name, error);
				return true;
			};

			const auto ownerName = DesignerStyleSheetUtils::UIClassName(type);
			std::wstring brushProperty;
			for (const auto* candidate : { L"Background", L"Foreground", L"BorderBrush" })
			{
				if (Equals(name, L"Control." + std::wstring(candidate))
					|| Equals(name, ownerName + L"." + candidate))
				{
					brushProperty = candidate;
					break;
				}
			}
			if (!brushProperty.empty())
			{
				handled = true;
				auto& node = _document.Nodes[nodeIndex];
				if (node.Properties.Find(brushProperty))
					return Fail(L"属性元素重复：" + name, error);
				DesignValue brush;
				if (!ParseBrush(property, brush, error)) return false;
				DesignerStyleValue value;
				value.Kind = DesignerStyleValueKind::Brush;
				value.ObjectValue = std::move(brush);
				return storeCanonical(brushProperty, std::move(value));
			}

			const auto transformOwner =
				DesignerStyleSheetUtils::UIClassName(type) + L".RenderTransform";
			if (Equals(name, L"Control.RenderTransform")
				|| Equals(name, transformOwner))
			{
				handled = true;
				auto& node = _document.Nodes[nodeIndex];
				if (node.Properties.Find(L"RenderTransform"))
					return Fail(L"属性元素重复：" + name, error);
				DesignValue transform;
				if (!ParseTransform(property, transform, error)) return false;
				DesignerStyleValue value;
				value.Kind = DesignerStyleValueKind::Transform;
				value.ObjectValue = std::move(transform);
				return storeCanonical(L"RenderTransform", std::move(value));
			}

			const auto clipOwner =
				DesignerStyleSheetUtils::UIClassName(type) + L".Clip";
			if (Equals(name, L"Control.Clip") || Equals(name, clipOwner))
			{
				handled = true;
				auto& node = _document.Nodes[nodeIndex];
				if (node.Properties.Find(L"Clip"))
					return Fail(L"属性元素重复：" + name, error);
				DesignValue clip;
				if (!ParseClip(property, clip, error)) return false;
				DesignerStyleValue value;
				value.Kind = DesignerStyleValueKind::Geometry;
				value.ObjectValue = std::move(clip);
				return storeCanonical(L"Clip", std::move(value));
			}

			if (type == UIClass::UI_ChartView && Equals(name, L"ChartView.Series"))
			{
				if (!beginCollection("series")) return false;
				DesignValue values;
				if (!ParseChartSeries(property, values, error)) return false;
				return StoreStructureValue(nodeIndex,
					"series", std::move(values), error);
			}
			if (type == UIClass::UI_DataGrid && Equals(name, L"DataGrid.Columns"))
			{
				if (!beginCollection("dataGridColumns")) return false;
				if (!Equals(FromUtf8(property->NamespaceURI()), L"urn:cui"))
					return Fail(L"DataGrid.Columns 必须使用 CUI 命名空间。", error);
				DesignValue values;
				if (!ParseDataGridColumns(property, values, error)) return false;
				return StoreStructureValue(nodeIndex,
					"dataGridColumns", std::move(values), error);
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
			return StoreStructureValue(gridIndex,
				rows ? "rows" : "columns", std::move(definitions), error);
		}

		bool ValidateRelativePanelConstraints(
			const std::vector<DesignNode>& nodes,
			const std::wstring& owner,
			std::wstring& error)
		{
			std::unordered_map<int, const DesignNode*> byId;
			std::unordered_map<std::wstring, const DesignNode*> byName;
			byId.reserve(nodes.size());
			byName.reserve(nodes.size());
			for (const auto& node : nodes)
			{
				byId.emplace(node.Id, &node);
				byName.emplace(node.Name, &node);
			}
			auto parentOf = [&](const DesignNode& node) -> const DesignNode*
			{
				if (node.ParentId > 0)
				{
					const auto found = byId.find(node.ParentId);
					if (found != byId.end()) return found->second;
				}
				if (!node.ParentRef.empty())
				{
					const auto found = byName.find(node.ParentRef);
					if (found != byName.end()) return found->second;
				}
				return nullptr;
			};
			for (const auto& node : nodes)
			{
				if (!node.Structure.RelativePanel
					|| node.Structure.RelativePanel->Empty()) continue;
				const auto* parent = parentOf(node);
				if (!parent || parent->Type != UIClass::UI_RelativePanel)
					return Fail(owner + L" 中控件 " + node.Name
						+ L" 的 RelativePanel 约束只能应用于 "
							L"RelativePanel 的直接子控件。", error);
				const auto& constraints = *node.Structure.RelativePanel;
				const std::pair<const wchar_t*, const std::optional<std::wstring>*> references[] = {
					{ L"Above", &constraints.Above },
					{ L"Below", &constraints.Below },
					{ L"LeftOf", &constraints.LeftOf },
					{ L"RightOf", &constraints.RightOf },
					{ L"AlignLeftWith", &constraints.AlignLeftWith },
					{ L"AlignRightWith", &constraints.AlignRightWith },
					{ L"AlignTopWith", &constraints.AlignTopWith },
					{ L"AlignBottomWith", &constraints.AlignBottomWith }
				};
				for (const auto& [member, reference] : references)
				{
					if (!*reference) continue;
					const auto& targetName = **reference;
					const auto target = byName.find(targetName);
					if (target == byName.end())
						return Fail(owner + L" 中控件 " + node.Name
							+ L" 的 RelativePanel 约束引用了不存在的 "
								L"x:Name：" + targetName + L"（" + member + L"）", error);
					if (target->second == &node)
						return Fail(owner + L" 中控件 " + node.Name
							+ L" 的 RelativePanel 约束不能引用自身。",
							error);
					if (parentOf(*target->second) != parent)
						return Fail(owner + L" 中控件 " + node.Name
							+ L" 的 RelativePanel 约束目标必须是同一面板的"
								L"直接兄弟：" + targetName, error);
				}
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
				byName.emplace(_document.Nodes[index].Name, index);
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
					const auto found = byName.find(node.ParentRef);
					if (found != byName.end()) parentIndex = found->second;
				}
				if (parentIndex) inherited = resolve(*parentIndex);

				auto effective = inherited;
				for (const auto& [target, binding] : node.Bindings)
				{
					if (Equals(target, L"DataContext")
						&& binding.IsMultiBinding())
					{
						effective.reset();
						continue;
					}
					const auto source = DesignerBindingUtils::Trim(
						binding.SourceProperty);
					const bool explicitSource = !binding.ElementName.empty()
						|| binding.RelativeSource
							!= DesignerBindingRelativeSource::None;
					if (Equals(target, L"DataContext"))
					{
						if (!explicitSource && inherited)
						{
							paths.push_back(join(*inherited, source));
							effective = join(*inherited, source);
						}
						else effective.reset();
					}
				}
				for (const auto& [target, binding] : node.Bindings)
				{
					if (Equals(target, L"DataContext") || !effective) continue;
					(void)DesignerBindingUtils::VisitLeafBindingDefinitions(
						binding, [&](const DesignerDataBinding& child)
						{
							if (!child.ElementName.empty()
								|| child.RelativeSource
									!= DesignerBindingRelativeSource::None)
								return true;
							paths.push_back(join(*effective,
								DesignerBindingUtils::Trim(child.SourceProperty)));
							return true;
						});
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
				for (const auto& [targetProperty, binding]
					: node.Bindings)
				{
					std::wstring leafError;
					if (!DesignerBindingUtils::VisitLeafBindingDefinitions(
						binding, [&](const DesignerDataBinding& child)
						{
							if (child.RelativeSource
								== DesignerBindingRelativeSource::TemplatedParent
								&& !allowTemplatedParent)
							{
								leafError = owner + L" 中控件 " + node.Name
									+ L" 的绑定 " + targetProperty
									+ L" 只能在组件模板内使用 TemplatedParent。";
								return false;
							}
							const auto& sourceName = child.ElementName;
							if (sourceName.empty()) return true;
							if (sourceName.empty() || names.contains(sourceName)) return true;
							leafError = owner + L" 中控件 " + node.Name
								+ L" 的绑定 " + targetProperty
								+ L" 引用了当前 namescope 中不存在的 ElementName："
								+ sourceName;
							return false;
						}))
						return Fail(leafError, error);
				}
			}
			return true;
		}
	};
}

static bool ParseXamlDocumentCore(
	const std::string& xaml,
	DesignDocument& output,
	const XamlDocumentParseOptions& options,
	bool resourceDictionary,
	std::wstring* outError,
	XamlDocumentDiagnostic* outDiagnostic)
{
	ResetDiagnostic(outDiagnostic);
	try
	{
		XmlDocument xml;
		xml.SetPreserveWhitespace(true);
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
		const bool parsed = resourceDictionary
			? parser.ParseResourceDictionaryRoot(root, error)
			: parser.Parse(root, error);
		if (!parsed)
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

static bool LoadXamlDocumentCore(
	const std::wstring& filePath,
	DesignDocument& output,
	const XamlDocumentParseOptions& options,
	bool resourceDictionary,
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
		return ParseXamlDocumentCore(
			content, output, effectiveOptions, resourceDictionary,
			outError, outDiagnostic);
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
	return ParseXamlDocumentCore(
		xaml, output, options, false, outError, outDiagnostic);
}

bool XamlDocumentParser::FromResourceDictionary(
	const std::string& xaml,
	DesignDocument& output,
	std::wstring* outError,
	XamlDocumentDiagnostic* outDiagnostic)
{
	return FromResourceDictionary(
		xaml, output, XamlDocumentParseOptions{}, outError, outDiagnostic);
}

bool XamlDocumentParser::FromResourceDictionary(
	const std::string& xaml,
	DesignDocument& output,
	const XamlDocumentParseOptions& options,
	std::wstring* outError,
	XamlDocumentDiagnostic* outDiagnostic)
{
	return ParseXamlDocumentCore(
		xaml, output, options, true, outError, outDiagnostic);
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
	return LoadXamlDocumentCore(
		filePath, output, options, false, outError, outDiagnostic);
}

bool XamlDocumentParser::LoadResourceDictionary(
	const std::wstring& filePath,
	DesignDocument& output,
	std::wstring* outError,
	XamlDocumentDiagnostic* outDiagnostic)
{
	return LoadResourceDictionary(
		filePath, output, XamlDocumentParseOptions{}, outError, outDiagnostic);
}

bool XamlDocumentParser::LoadResourceDictionary(
	const std::wstring& filePath,
	DesignDocument& output,
	const XamlDocumentParseOptions& options,
	std::wstring* outError,
	XamlDocumentDiagnostic* outDiagnostic)
{
	return LoadXamlDocumentCore(
		filePath, output, options, true, outError, outDiagnostic);
}
}
