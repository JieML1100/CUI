#include "BindingConverterCatalog.h"

#include "../XmlLite/include/Xml.h"

#include <Convert.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

using namespace System::Xml;

namespace DesignerModel
{
namespace
{
	constexpr std::size_t MaximumManifestBytes = 1024 * 1024;

	void SetError(std::wstring* output, std::wstring message)
	{
		if (output) *output = std::move(message);
	}

	std::wstring Widen(std::string_view value)
	{
		return Convert::Utf8ToUnicode(std::string(value));
	}

	std::wstring Trim(std::wstring value)
	{
		auto isSpace = [](wchar_t value)
		{
			return std::iswspace(static_cast<wint_t>(value)) != 0;
		};
		while (!value.empty() && isSpace(value.front())) value.erase(value.begin());
		while (!value.empty() && isSpace(value.back())) value.pop_back();
		return value;
	}

	std::string Trim(std::string value)
	{
		auto isSpace = [](unsigned char value)
		{
			return std::isspace(value) != 0;
		};
		while (!value.empty()
			&& isSpace(static_cast<unsigned char>(value.front())))
			value.erase(value.begin());
		while (!value.empty()
			&& isSpace(static_cast<unsigned char>(value.back())))
			value.pop_back();
		return value;
	}

	bool IsAsciiIdentifierStart(char value) noexcept
	{
		const auto character = static_cast<unsigned char>(value);
		return value == '_' || std::isalpha(character) != 0;
	}

	bool IsAsciiIdentifierPart(char value) noexcept
	{
		const auto character = static_cast<unsigned char>(value);
		return value == '_' || std::isalnum(character) != 0;
	}

	bool IsPortableConverterId(std::wstring_view value) noexcept
	{
		if (value.empty()) return false;
		bool atSegmentStart = true;
		for (const auto character : value)
		{
			if (character > 0x7f) return false;
			if (character == L'.')
			{
				if (atSegmentStart) return false;
				atSegmentStart = true;
				continue;
			}
			const auto ascii = static_cast<char>(character);
			if (atSegmentStart)
			{
				if (!IsAsciiIdentifierStart(ascii)) return false;
				atSegmentStart = false;
			}
			else if (!IsAsciiIdentifierPart(ascii)) return false;
		}
		return !atSegmentStart;
	}

	bool IsQualifiedFactorySymbol(std::string_view value) noexcept
	{
		if (value.empty()) return false;
		if (value.starts_with("::")) value.remove_prefix(2);
		bool sawQualifier = false;
		while (!value.empty())
		{
			const auto separator = value.find("::");
			const auto segment = separator == std::string_view::npos
				? value : value.substr(0, separator);
			if (segment.empty() || !IsAsciiIdentifierStart(segment.front()))
				return false;
			if (!std::all_of(segment.begin() + 1, segment.end(),
				IsAsciiIdentifierPart)) return false;
			if (separator == std::string_view::npos) break;
			sawQualifier = true;
			value.remove_prefix(separator + 2);
		}
		return sawQualifier;
	}

	bool IsSafeQuotedInclude(const std::string& value)
	{
		if (value.empty() || value.front() == '/' || value.front() == '\\')
			return false;
		if (value.find('\0') != std::string::npos
			|| value.find_first_of("\"<>\r\n") != std::string::npos)
			return false;
		const std::filesystem::path path(value);
		if (path.empty() || path.is_absolute()
			|| path.has_root_name() || path.has_root_directory()) return false;
		for (const auto& part : path)
			if (part == "." || part == "..") return false;
		return true;
	}

	wchar_t FoldAscii(wchar_t value) noexcept
	{
		return value >= L'A' && value <= L'Z' ? value - L'A' + L'a' : value;
	}

	bool SameId(std::wstring_view left, std::wstring_view right) noexcept
	{
		if (left.size() != right.size()) return false;
		for (std::size_t index = 0; index < left.size(); ++index)
			if (FoldAscii(left[index]) != FoldAscii(right[index])) return false;
		return true;
	}

	bool IsBuiltInId(std::wstring_view id) noexcept
	{
		for (const auto* builtIn : {
			L"BooleanNegation", L"StringIsNotEmpty", L"StringTrim" })
			if (SameId(id, builtIn)) return true;
		return false;
	}

	bool ParseValueKind(const std::string& text, BindingValueKind& output) noexcept
	{
		auto equals = [&](std::string_view candidate)
		{
			return text.size() == candidate.size()
				&& std::equal(text.begin(), text.end(), candidate.begin(),
					[](char left, char right)
					{
						return std::tolower(static_cast<unsigned char>(left))
							== std::tolower(static_cast<unsigned char>(right));
					});
		};
		if (equals("Any") || equals("Empty")) output = BindingValueKind::Empty;
		else if (equals("Bool")) output = BindingValueKind::Bool;
		else if (equals("NullableBool")) output = BindingValueKind::NullableBool;
		else if (equals("Int")) output = BindingValueKind::Int;
		else if (equals("Int64")) output = BindingValueKind::Int64;
		else if (equals("Float")) output = BindingValueKind::Float;
		else if (equals("Double")) output = BindingValueKind::Double;
		else if (equals("String")) output = BindingValueKind::String;
		else if (equals("Object")) output = BindingValueKind::Object;
		else return false;
		return true;
	}

	bool ParseBool(const std::string& text, bool& output) noexcept
	{
		if (text == "true") output = true;
		else if (text == "false") output = false;
		else return false;
		return true;
	}

	bool ParseCount(const std::string& text, std::size_t& output) noexcept
	{
		if (text.empty()) return false;
		unsigned long long parsed = 0;
		const auto [end, error] = std::from_chars(
			text.data(), text.data() + text.size(), parsed);
		if (error != std::errc{} || end != text.data() + text.size()
			|| parsed < 2
			|| parsed > static_cast<unsigned long long>(
				(std::numeric_limits<std::size_t>::max)())) return false;
		output = static_cast<std::size_t>(parsed);
		return true;
	}

	bool ValidateAttributes(
		const std::shared_ptr<XmlElement>& element,
		std::initializer_list<const char*> required,
		std::wstring* outError)
	{
		std::set<std::string> expected;
		for (const auto* name : required) expected.emplace(name);
		for (const auto& attribute : element->Attributes())
		{
			if (!attribute || !expected.erase(attribute->Name()))
			{
				SetError(outError, L"Converter manifest "
					+ Widen(element->Name()) + L" contains an unknown attribute: "
					+ (attribute ? Widen(attribute->Name()) : L"<null>"));
				return false;
			}
		}
		if (!expected.empty())
		{
			SetError(outError, L"Converter manifest " + Widen(element->Name())
				+ L" is missing required attribute: " + Widen(*expected.begin()));
			return false;
		}
		for (const auto& child : element->ChildNodes())
		{
			if (!child) continue;
			switch (child->NodeType())
			{
			case XmlNodeType::Whitespace:
			case XmlNodeType::SignificantWhitespace:
			case XmlNodeType::Comment:
				break;
			case XmlNodeType::Text:
				if (Trim(child->Value()).empty()) break;
				[[fallthrough]];
			default:
				SetError(outError, L"Converter manifest " + Widen(element->Name())
					+ L" must not contain nested content.");
				return false;
			}
		}
		return true;
	}

	bool ParseEntry(
		const std::shared_ptr<XmlElement>& element,
		BindingConverterCatalogKind kind,
		BindingConverterCatalogEntry& output,
		std::wstring* outError)
	{
		const bool single = kind == BindingConverterCatalogKind::Single;
		if (!ValidateAttributes(element,
			single
				? std::initializer_list<const char*>{
					"Id", "Include", "Factory", "SourceKind", "TargetKind",
					"CanConvertBack" }
				: std::initializer_list<const char*>{
					"Id", "Include", "Factory", "MinimumInputCount",
					"TargetKind", "CanConvertBack" }, outError)) return false;

		BindingConverterCatalogEntry entry;
		entry.Kind = kind;
		const auto rawId = element->GetAttribute("Id");
		entry.Id = Widen(rawId);
		if (entry.Id != Trim(entry.Id) || !IsPortableConverterId(entry.Id))
		{
			SetError(outError,
				L"Converter manifest Id must be a dot-qualified portable identifier: "
				+ entry.Id);
			return false;
		}
		if (IsBuiltInId(entry.Id))
		{
			SetError(outError, L"Converter manifest must not shadow framework built-in: "
				+ entry.Id);
			return false;
		}

		entry.Include = element->GetAttribute("Include");
		if (entry.Include != Trim(entry.Include)
			|| !IsSafeQuotedInclude(entry.Include))
		{
			SetError(outError, L"Converter manifest Include must be a safe relative "
				L"quoted-include path for " + entry.Id + L".");
			return false;
		}
		entry.FactorySymbol = element->GetAttribute("Factory");
		if (entry.FactorySymbol != Trim(entry.FactorySymbol)
			|| !IsQualifiedFactorySymbol(entry.FactorySymbol))
		{
			SetError(outError, L"Converter manifest Factory must be a qualified "
				L"free-function symbol without parentheses for " + entry.Id + L".");
			return false;
		}

		if (single)
		{
			if (!ParseValueKind(element->GetAttribute("SourceKind"), entry.SourceKind))
			{
				SetError(outError, L"Converter manifest SourceKind is invalid for "
					+ entry.Id + L".");
				return false;
			}
		}
		else if (!ParseCount(
			element->GetAttribute("MinimumInputCount"), entry.MinimumInputCount))
		{
			SetError(outError, L"Converter manifest MinimumInputCount must be an "
				L"integer of at least 2 for " + entry.Id + L".");
			return false;
		}
		if (!ParseValueKind(element->GetAttribute("TargetKind"), entry.TargetKind))
		{
			SetError(outError, L"Converter manifest TargetKind is invalid for "
				+ entry.Id + L".");
			return false;
		}
		if (!ParseBool(
			element->GetAttribute("CanConvertBack"), entry.CanConvertBack))
		{
			SetError(outError, L"Converter manifest CanConvertBack must be exactly "
				L"true or false for " + entry.Id + L".");
			return false;
		}
		output = std::move(entry);
		return true;
	}

	bool ParseDocument(
		XmlDocument& document,
		std::vector<BindingConverterCatalogEntry>& output,
		std::wstring* outError)
	{
		const auto root = document.DocumentElement();
		if (!root || root->Name() != "CuiBindingConverters")
		{
			SetError(outError,
				L"Converter manifest root must be CuiBindingConverters.");
			return false;
		}
		if (root->Attributes().size() != 1
			|| root->GetAttribute("Version")
				!= std::to_string(BindingConverterCatalog::CurrentManifestVersion))
		{
			SetError(outError, L"Converter manifest requires Version=\""
				+ std::to_wstring(BindingConverterCatalog::CurrentManifestVersion)
				+ L"\" and no other root attributes.");
			return false;
		}

		std::vector<BindingConverterCatalogEntry> parsed;
		for (const auto& child : root->ChildNodes())
		{
			if (!child) continue;
			if (child->NodeType() == XmlNodeType::Whitespace
				|| child->NodeType() == XmlNodeType::SignificantWhitespace
				|| child->NodeType() == XmlNodeType::Comment) continue;
			if (child->NodeType() == XmlNodeType::Text
				&& Trim(child->Value()).empty()) continue;
			if (child->NodeType() != XmlNodeType::Element)
			{
				SetError(outError,
					L"Converter manifest root contains unsupported content.");
				return false;
			}
			const auto element = std::dynamic_pointer_cast<XmlElement>(child);
			const auto kind = element && element->Name() == "Single"
				? BindingConverterCatalogKind::Single
				: element && element->Name() == "Multi"
					? BindingConverterCatalogKind::Multi
					: static_cast<BindingConverterCatalogKind>(0xff);
			if (!element || (kind != BindingConverterCatalogKind::Single
				&& kind != BindingConverterCatalogKind::Multi))
			{
				SetError(outError, L"Converter manifest accepts only Single and Multi "
					L"entries.");
				return false;
			}
			BindingConverterCatalogEntry entry;
			if (!ParseEntry(element, kind, entry, outError)) return false;
			if (std::any_of(parsed.begin(), parsed.end(),
				[&](const BindingConverterCatalogEntry& existing)
				{
					return SameId(existing.Id, entry.Id);
				}))
			{
				SetError(outError, L"Converter manifest contains a duplicate "
					L"case-insensitive Id: " + entry.Id);
				return false;
			}
			parsed.push_back(std::move(entry));
		}
		output = std::move(parsed);
		if (outError) outError->clear();
		return true;
	}
}

std::string BindingConverterCatalogEntry::FactoryCallExpression() const
{
	return (FactorySymbol.starts_with("::") ? std::string{} : std::string("::"))
		+ FactorySymbol + "()";
}

bool BindingConverterCatalog::FromXml(
	std::string_view xml,
	BindingConverterCatalog& output,
	std::wstring* outError)
{
	if (outError) outError->clear();
	if (xml.size() > MaximumManifestBytes)
	{
		SetError(outError, L"Converter manifest exceeds the 1 MiB limit.");
		return false;
	}
	try
	{
		XmlReaderSettings settings;
		settings.DtdProcessing = DtdProcessing::Prohibit;
		settings.MaxCharactersInDocument = MaximumManifestBytes;
		XmlDocument document;
		document.LoadXml(xml, settings);
		std::vector<BindingConverterCatalogEntry> entries;
		if (!ParseDocument(document, entries, outError)) return false;
		BindingConverterCatalog parsed;
		parsed._entries = std::move(entries);
		output = std::move(parsed);
		return true;
	}
	catch (const std::exception& error)
	{
		SetError(outError, L"Converter manifest XML is invalid: "
			+ Widen(error.what()));
		return false;
	}
	catch (...)
	{
		SetError(outError,
			L"Converter manifest XML is invalid: unknown parser failure.");
		return false;
	}
}

bool BindingConverterCatalog::LoadFile(
	const std::wstring& path,
	BindingConverterCatalog& output,
	std::wstring* outError)
{
	if (outError) outError->clear();
	if (path.empty())
	{
		SetError(outError, L"Converter manifest path is empty.");
		return false;
	}
	try
	{
		std::ifstream stream(
			std::filesystem::path(path), std::ios::binary | std::ios::ate);
		if (!stream)
		{
			SetError(outError, L"Unable to open converter manifest: " + path);
			return false;
		}
		const auto size = stream.tellg();
		if (size < 0 || static_cast<unsigned long long>(size)
			> MaximumManifestBytes)
		{
			SetError(outError, L"Converter manifest exceeds the 1 MiB limit: "
				+ path);
			return false;
		}
		std::string xml(static_cast<std::size_t>(size), '\0');
		stream.seekg(0, std::ios::beg);
		if (!xml.empty()
			&& !stream.read(xml.data(), static_cast<std::streamsize>(xml.size())))
		{
			SetError(outError, L"Unable to read converter manifest: " + path);
			return false;
		}
		std::wstring parseError;
		if (!FromXml(xml, output, &parseError))
		{
			SetError(outError, L"Converter manifest " + path + L": " + parseError);
			return false;
		}
		return true;
	}
	catch (const std::exception& error)
	{
		SetError(outError, L"Unable to load converter manifest " + path + L": "
			+ Widen(error.what()));
		return false;
	}
	catch (...)
	{
		SetError(outError, L"Unable to load converter manifest " + path
			+ L": unknown failure.");
		return false;
	}
}

const BindingConverterCatalogEntry* BindingConverterCatalog::Find(
	std::wstring_view id) const noexcept
{
	const auto found = std::find_if(_entries.begin(), _entries.end(),
		[&](const BindingConverterCatalogEntry& entry)
		{
			return SameId(entry.Id, id);
		});
	return found == _entries.end() ? nullptr : &*found;
}

const BindingConverterCatalogEntry* BindingConverterCatalog::Find(
	std::wstring_view id,
	BindingConverterCatalogKind kind) const noexcept
{
	const auto* entry = Find(id);
	return entry && entry->Kind == kind ? entry : nullptr;
}

std::vector<std::string> BindingConverterCatalog::Includes() const
{
	std::vector<std::string> result;
	for (const auto& entry : _entries)
		if (std::find(result.begin(), result.end(), entry.Include) == result.end())
			result.push_back(entry.Include);
	return result;
}
}
