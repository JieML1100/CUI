#pragma once

#include <algorithm>
#include <cwctype>
#include <string>
#include <vector>

namespace DesignerModel::XamlSourceScanner
{
	enum class TagKind : unsigned char
	{
		Opening,
		Closing,
		SelfClosing,
	};

	struct TagToken
	{
		TagKind Kind = TagKind::Opening;
		std::wstring Name;
		size_t Start = 0;
		size_t End = 0;
		size_t NameStart = 0;
		size_t NameLength = 0;
	};

	inline bool IsNameCharacter(wchar_t ch)
	{
		return std::iswalnum(static_cast<wint_t>(ch))
			|| ch == L'_' || ch == L'-' || ch == L'.' || ch == L':';
	}

	/**
	 * Scans opening-tag locations without interpreting XAML semantics.
	 * XmlLite does not retain source coordinates, so the parser uses these
	 * tokens solely to attach semantic diagnostics to the original UTF-16 text.
	 */
	inline std::vector<TagToken> ScanTags(
		const std::wstring& text,
		size_t limit = std::wstring::npos)
	{
		limit = (std::min)(limit, text.size());
		std::vector<TagToken> result;
		for (size_t i = 0; i < limit;)
		{
			if (text[i] != L'<')
			{
				++i;
				continue;
			}
			if (text.compare(i, 4, L"<!--") == 0)
			{
				const auto end = text.find(L"-->", i + 4);
				i = end == std::wstring::npos || end + 3 > limit
					? limit : end + 3;
				continue;
			}
			if (text.compare(i, 9, L"<![CDATA[") == 0)
			{
				const auto end = text.find(L"]]>", i + 9);
				i = end == std::wstring::npos || end + 3 > limit
					? limit : end + 3;
				continue;
			}

			wchar_t quote = 0;
			size_t end = i + 1;
			for (; end < limit; ++end)
			{
				const wchar_t ch = text[end];
				if (quote != 0)
				{
					if (ch == quote) quote = 0;
					continue;
				}
				if (ch == L'\'' || ch == L'"') quote = ch;
				else if (ch == L'>') break;
			}
			if (end >= limit || text[end] != L'>') break;

			size_t cursor = i + 1;
			while (cursor < end && std::iswspace(text[cursor])) ++cursor;
			if (cursor >= end || text[cursor] == L'!' || text[cursor] == L'?')
			{
				i = end + 1;
				continue;
			}
			bool closing = false;
			if (text[cursor] == L'/')
			{
				closing = true;
				++cursor;
				while (cursor < end && std::iswspace(text[cursor])) ++cursor;
			}
			const size_t nameStart = cursor;
			while (cursor < end && IsNameCharacter(text[cursor])) ++cursor;
			if (cursor == nameStart)
			{
				i = end + 1;
				continue;
			}
			size_t tail = end;
			while (tail > cursor && std::iswspace(text[tail - 1])) --tail;
			const bool selfClosing = !closing && tail > cursor
				&& text[tail - 1] == L'/';
			result.push_back(TagToken{
				closing ? TagKind::Closing
					: (selfClosing ? TagKind::SelfClosing : TagKind::Opening),
				text.substr(nameStart, cursor - nameStart),
				i, end + 1, nameStart, cursor - nameStart });
			i = end + 1;
		}
		return result;
	}
}
