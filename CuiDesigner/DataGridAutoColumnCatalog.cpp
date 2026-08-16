#include "DataGridAutoColumnCatalog.h"

#include "../XmlLite/include/Xml.h"

#include <Convert.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <set>
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
		auto space = [](wchar_t value)
		{
			return std::iswspace(static_cast<wint_t>(value)) != 0;
		};
		while (!value.empty() && space(value.front())) value.erase(value.begin());
		while (!value.empty() && space(value.back())) value.pop_back();
		return value;
	}

	std::string Trim(std::string value)
	{
		auto space = [](unsigned char value)
		{
			return std::isspace(value) != 0;
		};
		while (!value.empty()
			&& space(static_cast<unsigned char>(value.front())))
			value.erase(value.begin());
		while (!value.empty()
			&& space(static_cast<unsigned char>(value.back())))
			value.pop_back();
		return value;
	}

	std::wstring Lower(std::wstring value)
	{
		std::transform(value.begin(), value.end(), value.begin(),
			[](wchar_t character)
			{
				return static_cast<wchar_t>(std::towlower(character));
			});
		return value;
	}

	bool IsPortableName(std::wstring_view value, bool allowDots) noexcept
	{
		if (value.empty()) return false;
		bool segmentStart = true;
		for (const auto character : value)
		{
			if (character > 0x7f) return false;
			if (allowDots && character == L'.')
			{
				if (segmentStart) return false;
				segmentStart = true;
				continue;
			}
			const bool alpha = (character >= L'a' && character <= L'z')
				|| (character >= L'A' && character <= L'Z');
			const bool digit = character >= L'0' && character <= L'9';
			if (segmentStart)
			{
				if (!alpha && character != L'_') return false;
				segmentStart = false;
			}
			else if (!alpha && !digit && character != L'_') return false;
		}
		return !segmentStart;
	}

	bool ParseBool(const std::string& value, bool& output) noexcept
	{
		if (value == "true") output = true;
		else if (value == "false") output = false;
		else return false;
		return true;
	}

	bool ParseDouble(const std::wstring& value, double& output) noexcept
	{
		try
		{
			size_t consumed = 0;
			const auto parsed = std::stod(value, &consumed);
			if (consumed != value.size() || !std::isfinite(parsed)) return false;
			output = parsed;
			return true;
		}
		catch (...)
		{
			return false;
		}
	}

	bool ParseWidth(
		const std::string& raw,
		DesignDataGridLength& output) noexcept
	{
		const auto value = Trim(Widen(raw));
		const auto lower = Lower(value);
		DesignDataGridLength parsed;
		if (lower == L"auto") parsed.Unit = DesignDataGridLengthUnit::Auto;
		else if (lower == L"sizetoheader")
			parsed.Unit = DesignDataGridLengthUnit::SizeToHeader;
		else if (lower == L"sizetocells")
			parsed.Unit = DesignDataGridLengthUnit::SizeToCells;
		else
		{
			std::wstring number = value;
			if (!number.empty() && number.back() == L'*')
			{
				parsed.Unit = DesignDataGridLengthUnit::Star;
				number = Trim(number.substr(0, number.size() - 1));
				if (number.empty()) number = L"1";
			}
			else parsed.Unit = DesignDataGridLengthUnit::Pixel;
			if (!ParseDouble(number, parsed.Value) || parsed.Value < 0.0)
				return false;
		}
		output = parsed;
		return true;
	}

	bool HasOnlyWhitespaceContent(
		const std::shared_ptr<XmlElement>& element,
		std::wstring* outError)
	{
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
				SetError(outError,
					L"DataGrid auto-column Rule must not contain nested content.");
				return false;
			}
		}
		return true;
	}

	bool ParseRule(
		const std::shared_ptr<XmlElement>& element,
		DataGridAutoColumnRule& output,
		std::wstring* outError)
	{
		static const std::set<std::string> allowed{
			"DataType", "Property", "GridName", "Action", "Kind",
			"Header", "Width", "IsReadOnly", "IsThreeState",
			"CanUserSort", "CanUserResize", "CanUserReorder",
			"Visibility", "SortMemberPath" };
		std::map<std::string, std::string> attributes;
		for (const auto& attribute : element->Attributes())
		{
			if (!attribute || !allowed.contains(attribute->Name()))
			{
				SetError(outError,
					L"DataGrid auto-column Rule contains an unknown attribute: "
					+ (attribute ? Widen(attribute->Name()) : L"<null>"));
				return false;
			}
			attributes.emplace(attribute->Name(), attribute->Value());
		}
		if (!HasOnlyWhitespaceContent(element, outError)) return false;
		for (const auto* required : { "DataType", "Property" })
			if (!attributes.contains(required))
			{
				SetError(outError, L"DataGrid auto-column Rule is missing required "
					L"attribute: " + Widen(required));
				return false;
			}

		DataGridAutoColumnRule rule;
		rule.DataType = Widen(attributes["DataType"]);
		rule.Property = Widen(attributes["Property"]);
		if (rule.DataType != Trim(rule.DataType)
			|| !IsPortableName(rule.DataType, true))
		{
			SetError(outError, L"DataGrid auto-column DataType must be a portable "
				L"identifier: " + rule.DataType);
			return false;
		}
		if (rule.Property != Trim(rule.Property)
			|| !IsPortableName(rule.Property, true))
		{
			SetError(outError, L"DataGrid auto-column Property must be a portable "
				L"property path: " + rule.Property);
			return false;
		}
		if (const auto found = attributes.find("GridName");
			found != attributes.end())
		{
			rule.GridName = Widen(found->second);
			if (rule.GridName != Trim(rule.GridName)
				|| !IsPortableName(rule.GridName, false))
			{
				SetError(outError, L"DataGrid auto-column GridName must be a portable "
					L"x:Name: " + rule.GridName);
				return false;
			}
		}

		if (const auto found = attributes.find("Action");
			found != attributes.end())
		{
			if (found->second == "Transform")
				rule.Action = DataGridAutoColumnRuleAction::Transform;
			else if (found->second == "Suppress")
				rule.Action = DataGridAutoColumnRuleAction::Suppress;
			else
			{
				SetError(outError, L"DataGrid auto-column Action must be Transform or "
					L"Suppress.");
				return false;
			}
		}

		auto optional = [&](const char* name) -> const std::string*
		{
			const auto found = attributes.find(name);
			return found == attributes.end() ? nullptr : &found->second;
		};
		if (const auto* value = optional("Kind"))
		{
			if (*value == "Text") rule.Kind = DesignDataGridColumnKind::Text;
			else if (*value == "CheckBox")
				rule.Kind = DesignDataGridColumnKind::CheckBox;
			else if (*value == "Hyperlink")
				rule.Kind = DesignDataGridColumnKind::Hyperlink;
			else
			{
				SetError(outError, L"DataGrid auto-column Kind must be Text, CheckBox, "
					L"or Hyperlink.");
				return false;
			}
		}
		if (const auto* value = optional("Header"))
			rule.Header = Widen(*value);
		if (const auto* value = optional("Width"))
		{
			DesignDataGridLength width;
			if (!ParseWidth(*value, width))
			{
				SetError(outError, L"DataGrid auto-column Width must be Auto, "
					L"SizeToHeader, SizeToCells, a non-negative pixel value, or a "
					L"non-negative Star value.");
				return false;
			}
			rule.Width = width;
		}
		auto parseOptionalBool = [&](const char* name, std::optional<bool>& target)
		{
			const auto* value = optional(name);
			if (!value) return true;
			bool parsed = false;
			if (!ParseBool(*value, parsed))
			{
				SetError(outError, L"DataGrid auto-column " + Widen(name)
					+ L" must be exactly true or false.");
				return false;
			}
			target = parsed;
			return true;
		};
		if (!parseOptionalBool("IsReadOnly", rule.IsReadOnly)
			|| !parseOptionalBool("IsThreeState", rule.IsThreeState)
			|| !parseOptionalBool("CanUserSort", rule.CanUserSort)
			|| !parseOptionalBool("CanUserResize", rule.CanUserResize)
			|| !parseOptionalBool("CanUserReorder", rule.CanUserReorder))
			return false;

		if (const auto* value = optional("Visibility"))
		{
			if (*value == "Visible")
				rule.Visibility = DesignDataGridColumnVisibility::Visible;
			else if (*value == "Hidden")
				rule.Visibility = DesignDataGridColumnVisibility::Hidden;
			else if (*value == "Collapsed")
				rule.Visibility = DesignDataGridColumnVisibility::Collapsed;
			else
			{
				SetError(outError, L"DataGrid auto-column Visibility must be Visible, "
					L"Hidden, or Collapsed.");
				return false;
			}
		}
		if (const auto* value = optional("SortMemberPath"))
		{
			auto path = Widen(*value);
			if (!path.empty()
				&& (path != Trim(path) || !IsPortableName(path, true)))
			{
				SetError(outError, L"DataGrid auto-column SortMemberPath must be empty "
					L"or a portable property path.");
				return false;
			}
			rule.SortMemberPath = std::move(path);
		}

		if (rule.Action == DataGridAutoColumnRuleAction::Suppress
			&& attributes.size() > static_cast<size_t>(
				2 + (!rule.GridName.empty() ? 1 : 0) + 1))
		{
			SetError(outError, L"A suppressing DataGrid auto-column Rule cannot also "
				L"contain transformation attributes.");
			return false;
		}
		output = std::move(rule);
		return true;
	}

	bool ParseDocument(
		XmlDocument& document,
		std::vector<DataGridAutoColumnRule>& output,
		std::wstring* outError)
	{
		const auto root = document.DocumentElement();
		if (!root || root->Name() != "CuiDataGridAutoColumns")
		{
			SetError(outError,
				L"DataGrid auto-column manifest root must be CuiDataGridAutoColumns.");
			return false;
		}
		if (root->Attributes().size() != 1
			|| root->GetAttribute("Version")
				!= std::to_string(DataGridAutoColumnCatalog::CurrentManifestVersion))
		{
			SetError(outError, L"DataGrid auto-column manifest requires Version=\""
				+ std::to_wstring(
					DataGridAutoColumnCatalog::CurrentManifestVersion)
				+ L"\" and no other root attributes.");
			return false;
		}

		std::vector<DataGridAutoColumnRule> rules;
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
				SetError(outError, L"DataGrid auto-column manifest root contains "
					L"unsupported content.");
				return false;
			}
			const auto element = std::dynamic_pointer_cast<XmlElement>(child);
			if (!element || element->Name() != "Rule")
			{
				SetError(outError, L"DataGrid auto-column manifest accepts only Rule "
					L"entries.");
				return false;
			}
			DataGridAutoColumnRule rule;
			if (!ParseRule(element, rule, outError)) return false;
			if (std::any_of(rules.begin(), rules.end(),
				[&](const DataGridAutoColumnRule& existing)
				{
					return existing.DataType == rule.DataType
						&& existing.Property == rule.Property
						&& existing.GridName == rule.GridName;
				}))
			{
				SetError(outError, L"DataGrid auto-column manifest contains a duplicate "
					L"DataType/Property/GridName rule: " + rule.DataType + L"/"
					+ rule.Property + L"/" + rule.GridName);
				return false;
			}
			rules.push_back(std::move(rule));
		}
		output = std::move(rules);
		return true;
	}
}

bool DataGridAutoColumnCatalog::FromXml(
	std::string_view xml,
	DataGridAutoColumnCatalog& output,
	std::wstring* outError)
{
	if (outError) outError->clear();
	if (xml.size() > MaximumManifestBytes)
	{
		SetError(outError,
			L"DataGrid auto-column manifest exceeds the 1 MiB limit.");
		return false;
	}
	try
	{
		XmlReaderSettings settings;
		settings.DtdProcessing = DtdProcessing::Prohibit;
		settings.MaxCharactersInDocument = MaximumManifestBytes;
		XmlDocument document;
		document.LoadXml(xml, settings);
		std::vector<DataGridAutoColumnRule> rules;
		if (!ParseDocument(document, rules, outError)) return false;
		DataGridAutoColumnCatalog parsed;
		parsed._rules = std::move(rules);
		output = std::move(parsed);
		return true;
	}
	catch (const std::exception& error)
	{
		SetError(outError, L"DataGrid auto-column manifest XML is invalid: "
			+ Widen(error.what()));
		return false;
	}
	catch (...)
	{
		SetError(outError, L"DataGrid auto-column manifest XML is invalid: "
			L"unknown parser failure.");
		return false;
	}
}

bool DataGridAutoColumnCatalog::LoadFile(
	const std::wstring& path,
	DataGridAutoColumnCatalog& output,
	std::wstring* outError)
{
	if (outError) outError->clear();
	if (path.empty())
	{
		SetError(outError, L"DataGrid auto-column manifest path is empty.");
		return false;
	}
	try
	{
		std::ifstream stream(
			std::filesystem::path(path), std::ios::binary | std::ios::ate);
		if (!stream)
		{
			SetError(outError,
				L"Unable to open DataGrid auto-column manifest: " + path);
			return false;
		}
		const auto size = stream.tellg();
		if (size < 0 || static_cast<unsigned long long>(size)
			> MaximumManifestBytes)
		{
			SetError(outError, L"DataGrid auto-column manifest exceeds the 1 MiB "
				L"limit: " + path);
			return false;
		}
		std::string xml(static_cast<std::size_t>(size), '\0');
		stream.seekg(0, std::ios::beg);
		if (!xml.empty()
			&& !stream.read(xml.data(), static_cast<std::streamsize>(xml.size())))
		{
			SetError(outError,
				L"Unable to read DataGrid auto-column manifest: " + path);
			return false;
		}
		std::wstring error;
		if (!FromXml(xml, output, &error))
		{
			SetError(outError, L"DataGrid auto-column manifest " + path + L": "
				+ error);
			return false;
		}
		return true;
	}
	catch (const std::exception& error)
	{
		SetError(outError, L"Unable to load DataGrid auto-column manifest " + path
			+ L": " + Widen(error.what()));
		return false;
	}
	catch (...)
	{
		SetError(outError, L"Unable to load DataGrid auto-column manifest " + path
			+ L": unknown failure.");
		return false;
	}
}

const DataGridAutoColumnRule* DataGridAutoColumnCatalog::Find(
	std::wstring_view dataType,
	std::wstring_view property,
	std::wstring_view gridName) const noexcept
{
	const DataGridAutoColumnRule* fallback = nullptr;
	for (const auto& rule : _rules)
	{
		if (rule.DataType != dataType || rule.Property != property) continue;
		if (!rule.GridName.empty())
		{
			if (rule.GridName == gridName) return &rule;
		}
		else fallback = &rule;
	}
	return fallback;
}
}
