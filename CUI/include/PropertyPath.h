#pragma once

#include <cwctype>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace cui::xaml
{
enum class PropertyPathSegmentKind : unsigned char
{
	Property,
	Index,
};

/** One parsed member or collection index in a WPF-style property path. */
struct PropertyPathSegment
{
	PropertyPathSegmentKind Kind = PropertyPathSegmentKind::Property;
	/** Optional owner token from `(Owner.Property)`. */
	std::wstring OwnerType;
	std::wstring Name;
	size_t Index = 0;

	bool operator==(const PropertyPathSegment&) const = default;
};

/**
 * Syntax-only representation of Storyboard.TargetProperty. Resolution stays
 * with the object adapter that owns each segment; the parser never uses C++
 * reflection or assumes that every segment is a Control property.
 */
struct PropertyPath
{
	std::vector<PropertyPathSegment> Segments;

	bool operator==(const PropertyPath&) const = default;

	bool IsDirectProperty() const noexcept
	{
		return Segments.size() == 1
			&& Segments.front().Kind == PropertyPathSegmentKind::Property
			&& Segments.front().OwnerType.empty();
	}

	std::wstring CanonicalText() const
	{
		std::wstring result;
		for (const auto& segment : Segments)
		{
			if (segment.Kind == PropertyPathSegmentKind::Index)
			{
				result += L'[' + std::to_wstring(segment.Index) + L']';
				continue;
			}
			if (!result.empty()) result += L'.';
			if (!segment.OwnerType.empty())
				result += L'(' + segment.OwnerType + L'.' + segment.Name + L')';
			else result += segment.Name;
		}
		return result;
	}
};

inline std::wstring TrimPropertyPathToken(std::wstring value)
{
	while (!value.empty() && std::iswspace(value.front())) value.erase(value.begin());
	while (!value.empty() && std::iswspace(value.back())) value.pop_back();
	return value;
}

inline bool IsPropertyPathIdentifier(const std::wstring& value) noexcept
{
	if (value.empty()) return false;
	for (const auto ch : value)
	{
		if (std::iswalnum(ch) || ch == L'_' || ch == L':') continue;
		return false;
	}
	return true;
}

/** Parses a direct property or WPF parenthesized member/index path. */
inline bool TryParsePropertyPath(
	std::wstring text,
	PropertyPath& output,
	std::wstring* outError = nullptr)
{
	auto fail = [&](const wchar_t* message)
	{
		if (outError) *outError = message;
		return false;
	};
	text = TrimPropertyPathToken(std::move(text));
	PropertyPath parsed;
	if (text.empty()) return fail(L"属性路径不能为空。");

	if (text.front() != L'(')
	{
		if (!IsPropertyPathIdentifier(text))
			return fail(L"直接属性路径必须是单一属性名。");
		parsed.Segments.push_back({
			PropertyPathSegmentKind::Property, {}, std::move(text), 0 });
		output = std::move(parsed);
		if (outError) outError->clear();
		return true;
	}

	size_t position = 0;
	bool expectMember = true;
	while (position < text.size())
	{
		while (position < text.size() && std::iswspace(text[position])) ++position;
		if (position >= text.size()) break;
		if (text[position] == L'.')
		{
			if (expectMember) return fail(L"属性路径包含多余的分隔符。");
			expectMember = true;
			++position;
			continue;
		}
		if (text[position] == L'[')
		{
			if (expectMember || parsed.Segments.empty())
				return fail(L"属性路径索引缺少前置成员。");
			const auto close = text.find(L']', position + 1);
			if (close == std::wstring::npos)
				return fail(L"属性路径索引缺少右方括号。");
			const auto token = TrimPropertyPathToken(
				text.substr(position + 1, close - position - 1));
			if (token.empty()) return fail(L"属性路径索引不能为空。");
			size_t index = 0;
			for (const auto ch : token)
			{
				if (ch < L'0' || ch > L'9')
					return fail(L"属性路径索引必须是非负整数。");
				const auto digit = static_cast<size_t>(ch - L'0');
				if (index > ((std::numeric_limits<size_t>::max)() - digit) / 10)
					return fail(L"属性路径索引超出范围。");
				index = index * 10 + digit;
			}
			parsed.Segments.push_back({
				PropertyPathSegmentKind::Index, {}, {}, index });
			position = close + 1;
			expectMember = false;
			continue;
		}
		if (text[position] != L'(' || !expectMember)
			return fail(L"复合属性路径成员必须使用 (Owner.Property)。");
		const auto close = text.find(L')', position + 1);
		if (close == std::wstring::npos)
			return fail(L"属性路径成员缺少右括号。");
		const auto token = TrimPropertyPathToken(
			text.substr(position + 1, close - position - 1));
		const auto separator = token.rfind(L'.');
		if (separator == std::wstring::npos)
			return fail(L"属性路径成员必须包含 Owner.Property。");
		auto owner = TrimPropertyPathToken(token.substr(0, separator));
		auto name = TrimPropertyPathToken(token.substr(separator + 1));
		if (!IsPropertyPathIdentifier(owner) || !IsPropertyPathIdentifier(name))
			return fail(L"属性路径所有者或属性名无效。");
		parsed.Segments.push_back({
			PropertyPathSegmentKind::Property,
			std::move(owner), std::move(name), 0 });
		position = close + 1;
		expectMember = false;
	}
	if (parsed.Segments.empty() || expectMember)
		return fail(L"属性路径不完整。");
	output = std::move(parsed);
	if (outError) outError->clear();
	return true;
}
}
