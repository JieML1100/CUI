#pragma once

#include <algorithm>
#include <cwctype>
#include <limits>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace DesignerModel::XamlEditorAssist
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

	struct TextRange
	{
		size_t Start = 0;
		size_t Length = 0;
	};

	enum class XamlSyntaxKind : unsigned char
	{
		MarkupDelimiter,
		ElementName,
		AttributeName,
		AttributeValue,
		Comment,
		CData,
		ProcessingInstruction,
		Declaration,
		EntityReference,
	};

	/** One non-overlapping, UTF-16 source span used only for editor coloring. */
	struct XamlSyntaxSpan
	{
		XamlSyntaxKind Kind = XamlSyntaxKind::MarkupDelimiter;
		size_t Start = 0;
		size_t Length = 0;
	};

	struct TagMatch
	{
		std::wstring Name;
		std::vector<TextRange> Ranges;

		bool HasMatch() const noexcept { return !Ranges.empty(); }
	};

	enum class CompletionKind : unsigned char
	{
		None,
		Element,
		ClosingElement,
		Attribute,
		AttributeValue,
	};

	struct CompletionContext
	{
		CompletionKind Kind = CompletionKind::None;
		std::wstring ElementName;
		std::wstring AttributeName;
		std::wstring Prefix;
		std::vector<std::wstring> UsedAttributes;
		size_t ReplaceStart = 0;
		size_t ReplaceEnd = 0;

		bool IsValid() const noexcept { return Kind != CompletionKind::None; }
	};

	struct NewLineEdit
	{
		std::wstring Text;
		size_t CaretOffset = 0;
	};

	/** One whole-line indentation edit with the post-edit selection. */
	struct LineIndentEdit
	{
		size_t ReplaceStart = 0;
		size_t ReplaceLength = 0;
		std::wstring Text;
		size_t SelectionStart = 0;
		size_t SelectionEnd = 0;
		bool Changed = false;
	};

	/** Design identity and source span for one complete XAML element. */
	struct ElementIdentitySpan
	{
		std::wstring ElementName;
		std::wstring Name;
		int StableId = 0;
		TextRange OpeningTag;
		TextRange ElementNameRange;
		TextRange Element;

		bool IsForm() const noexcept
		{
			const auto separator = ElementName.find_last_of(L':');
			const auto local = separator == std::wstring::npos
				? ElementName : ElementName.substr(separator + 1);
			return local.size() == 4
				&& std::towlower(local[0]) == L'f'
				&& std::towlower(local[1]) == L'o'
				&& std::towlower(local[2]) == L'r'
				&& std::towlower(local[3]) == L'm';
		}

		bool HasDesignIdentity() const noexcept
		{
			return StableId > 0 || !Name.empty() || IsForm();
		}
	};

	inline bool IsNameCharacter(wchar_t ch)
	{
		return std::iswalnum(static_cast<wint_t>(ch))
			|| ch == L'_' || ch == L'-' || ch == L'.' || ch == L':';
	}

	inline std::vector<XamlSyntaxSpan> ScanXamlSyntax(
		const std::wstring& text)
	{
		std::vector<XamlSyntaxSpan> spans;
		auto append = [&](XamlSyntaxKind kind, size_t start, size_t end)
		{
			end = (std::min)(end, text.size());
			if (start >= end) return;
			spans.push_back({ kind, start, end - start });
		};
		auto specialSpan = [&](size_t start, size_t markerLength,
			const wchar_t* terminator, size_t terminatorLength,
			XamlSyntaxKind kind)
		{
			const auto found = text.find(terminator, start + markerLength);
			const size_t end = found == std::wstring::npos
				? text.size() : found + terminatorLength;
			append(kind, start, end);
			return end;
		};

		for (size_t i = 0; i < text.size();)
		{
			if (text.compare(i, 4, L"<!--") == 0)
			{
				i = specialSpan(i, 4, L"-->", 3, XamlSyntaxKind::Comment);
				continue;
			}
			if (text.compare(i, 9, L"<![CDATA[") == 0)
			{
				i = specialSpan(i, 9, L"]]>", 3, XamlSyntaxKind::CData);
				continue;
			}
			if (text.compare(i, 2, L"<?") == 0)
			{
				i = specialSpan(i, 2, L"?>", 2,
					XamlSyntaxKind::ProcessingInstruction);
				continue;
			}
			if (text.compare(i, 2, L"<!") == 0)
			{
				const size_t start = i;
				wchar_t quote = 0;
				i += 2;
				for (; i < text.size(); ++i)
				{
					const wchar_t ch = text[i];
					if (quote)
					{
						if (ch == quote) quote = 0;
						continue;
					}
					if (ch == L'\'' || ch == L'"') quote = ch;
					else if (ch == L'>')
					{
						++i;
						break;
					}
				}
				append(XamlSyntaxKind::Declaration, start, i);
				continue;
			}
			if (text[i] == L'&')
			{
				size_t end = i + 1;
				while (end < text.size() && end - i <= 32
					&& (IsNameCharacter(text[end]) || text[end] == L'#'))
					++end;
				if (end < text.size() && text[end] == L';' && end > i + 1)
				{
					append(XamlSyntaxKind::EntityReference, i, end + 1);
					i = end + 1;
					continue;
				}
			}
			if (text[i] != L'<')
			{
				++i;
				continue;
			}

			const size_t tagStart = i;
			bool closing = i + 1 < text.size() && text[i + 1] == L'/';
			append(XamlSyntaxKind::MarkupDelimiter,
				i, (std::min)(text.size(), i + (closing ? 2 : 1)));
			i += closing ? 2 : 1;
			while (i < text.size() && std::iswspace(text[i])) ++i;
			const size_t elementStart = i;
			while (i < text.size() && IsNameCharacter(text[i])) ++i;
			if (i == elementStart)
			{
				i = tagStart + 1;
				continue;
			}
			append(XamlSyntaxKind::ElementName, elementStart, i);

			while (i < text.size())
			{
				while (i < text.size() && std::iswspace(text[i])) ++i;
				if (i >= text.size()) break;
				if (text[i] == L'>')
				{
					append(XamlSyntaxKind::MarkupDelimiter, i, i + 1);
					++i;
					break;
				}
				if (text[i] == L'/' && i + 1 < text.size()
					&& text[i + 1] == L'>')
				{
					append(XamlSyntaxKind::MarkupDelimiter, i, i + 2);
					i += 2;
					break;
				}
				if (text[i] == L'<') break;

				const size_t attributeStart = i;
				while (i < text.size() && IsNameCharacter(text[i])) ++i;
				if (i == attributeStart)
				{
					++i;
					continue;
				}
				append(XamlSyntaxKind::AttributeName, attributeStart, i);
				while (i < text.size() && std::iswspace(text[i])) ++i;
				if (i >= text.size() || text[i] != L'=') continue;
				append(XamlSyntaxKind::MarkupDelimiter, i, i + 1);
				++i;
				while (i < text.size() && std::iswspace(text[i])) ++i;
				if (i >= text.size()) break;
				const size_t valueStart = i;
				if (text[i] == L'\'' || text[i] == L'"')
				{
					const wchar_t quote = text[i++];
					while (i < text.size() && text[i] != quote) ++i;
					if (i < text.size()) ++i;
				}
				else
				{
					while (i < text.size() && !std::iswspace(text[i])
						&& text[i] != L'>')
					{
						if (text[i] == L'/' && i + 1 < text.size()
							&& text[i + 1] == L'>') break;
						++i;
					}
				}
				append(XamlSyntaxKind::AttributeValue, valueStart, i);
			}
		}
		return spans;
	}

	inline bool NamesEqual(const std::wstring& left, const std::wstring& right)
	{
		return left == right;
	}

	inline std::wstring Lower(std::wstring value)
	{
		std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch)
		{
			return static_cast<wchar_t>(std::towlower(static_cast<wint_t>(ch)));
		});
		return value;
	}

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
				i++;
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
			for (; end < limit; end++)
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
			while (cursor < end && std::iswspace(text[cursor])) cursor++;
			if (cursor >= end || text[cursor] == L'!' || text[cursor] == L'?')
			{
				i = end + 1;
				continue;
			}
			bool closing = false;
			if (text[cursor] == L'/')
			{
				closing = true;
				cursor++;
				while (cursor < end && std::iswspace(text[cursor])) cursor++;
			}
			const size_t nameStart = cursor;
			while (cursor < end && IsNameCharacter(text[cursor])) cursor++;
			if (cursor == nameStart)
			{
				i = end + 1;
				continue;
			}
			size_t tail = end;
			while (tail > cursor && std::iswspace(text[tail - 1])) tail--;
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

	inline bool TryParseStableId(const std::wstring& value, int& stableId)
	{
		stableId = 0;
		if (value.empty()) return false;
		unsigned long long parsed = 0;
		for (const wchar_t ch : value)
		{
			if (ch < L'0' || ch > L'9') return false;
			const auto digit = static_cast<unsigned>(ch - L'0');
			if (parsed > (static_cast<unsigned long long>(
				(std::numeric_limits<int>::max)()) - digit) / 10)
				return false;
			parsed = parsed * 10 + digit;
		}
		if (parsed == 0) return false;
		stableId = static_cast<int>(parsed);
		return true;
	}

	inline ElementIdentitySpan ReadElementIdentity(
		const std::wstring& text,
		const TagToken& tag)
	{
		ElementIdentitySpan result;
		result.ElementName = tag.Name;
		result.OpeningTag = { tag.Start, tag.End - tag.Start };
		result.ElementNameRange = { tag.NameStart, tag.NameLength };
		if (tag.Kind == TagKind::Closing || tag.End > text.size()) return result;

		size_t cursor = tag.NameStart + tag.NameLength;
		const size_t end = tag.End - 1;
		while (cursor < end)
		{
			while (cursor < end && std::iswspace(text[cursor])) cursor++;
			if (cursor >= end || text[cursor] == L'/') break;
			const size_t attributeStart = cursor;
			while (cursor < end && IsNameCharacter(text[cursor])) cursor++;
			if (cursor == attributeStart)
			{
				cursor++;
				continue;
			}
			const auto attributeName =
				text.substr(attributeStart, cursor - attributeStart);
			while (cursor < end && std::iswspace(text[cursor])) cursor++;
			if (cursor >= end || text[cursor] != L'=') continue;
			cursor++;
			while (cursor < end && std::iswspace(text[cursor])) cursor++;
			if (cursor >= end || (text[cursor] != L'\'' && text[cursor] != L'"'))
				continue;
			const wchar_t quote = text[cursor++];
			const size_t valueStart = cursor;
			while (cursor < end && text[cursor] != quote) cursor++;
			const auto value = text.substr(valueStart, cursor - valueStart);
			if (cursor < end) cursor++;

			if (result.Name.empty()
				&& (attributeName == L"Name" || attributeName == L"x:Name"))
			{
				result.Name = value;
			}
			else if (result.StableId <= 0
				&& (attributeName == L"DesignId" || attributeName == L"x:Uid"))
			{
				(void)TryParseStableId(value, result.StableId);
			}
		}
		return result;
	}

	inline std::vector<ElementIdentitySpan> ScanElementIdentitySpans(
		const std::wstring& text)
	{
		const auto tags = ScanTags(text);
		std::vector<int> pairs(tags.size(), -1);
		std::vector<size_t> stack;
		for (size_t i = 0; i < tags.size(); i++)
		{
			if (tags[i].Kind == TagKind::Opening)
			{
				stack.push_back(i);
				continue;
			}
			if (tags[i].Kind != TagKind::Closing) continue;
			for (size_t j = stack.size(); j > 0; j--)
			{
				const size_t opening = stack[j - 1];
				if (!NamesEqual(tags[opening].Name, tags[i].Name)) continue;
				pairs[opening] = static_cast<int>(i);
				pairs[i] = static_cast<int>(opening);
				stack.resize(j - 1);
				break;
			}
		}

		std::vector<ElementIdentitySpan> result;
		for (size_t i = 0; i < tags.size(); i++)
		{
			if (tags[i].Kind == TagKind::Closing) continue;
			auto identity = ReadElementIdentity(text, tags[i]);
			const size_t elementEnd = pairs[i] >= 0
				? tags[static_cast<size_t>(pairs[i])].End : tags[i].End;
			identity.Element = { tags[i].Start, elementEnd - tags[i].Start };
			result.push_back(std::move(identity));
		}
		return result;
	}

	/** Finds the innermost designed element containing a source caret. */
	inline std::optional<ElementIdentitySpan> FindElementAtPosition(
		const std::wstring& text,
		size_t position)
	{
		position = (std::min)(position, text.size());
		std::optional<ElementIdentitySpan> best;
		for (auto& candidate : ScanElementIdentitySpans(text))
		{
			if (!candidate.HasDesignIdentity()) continue;
			const size_t end = candidate.Element.Start + candidate.Element.Length;
			if (position < candidate.Element.Start || position > end) continue;
			if (!best || candidate.Element.Length <= best->Element.Length)
				best = std::move(candidate);
		}
		return best;
	}

	/** Resolves a current design selection back to its canonical XAML element. */
	inline std::optional<ElementIdentitySpan> FindElementByDesignIdentity(
		const std::wstring& text,
		int stableId,
		const std::wstring& name)
	{
		auto elements = ScanElementIdentitySpans(text);
		if (stableId > 0)
		{
			for (auto& candidate : elements)
				if (candidate.StableId == stableId) return candidate;
		}
		if (!name.empty())
		{
			const auto normalizedName = Lower(name);
			for (auto& candidate : elements)
				if (Lower(candidate.Name) == normalizedName) return candidate;
		}
		return std::nullopt;
	}

	inline std::vector<std::wstring> OpenElementNames(
		const std::wstring& text,
		size_t before)
	{
		std::vector<std::wstring> stack;
		for (const auto& tag : ScanTags(text, before))
		{
			if (tag.Kind == TagKind::Opening)
			{
				stack.push_back(tag.Name);
				continue;
			}
			if (tag.Kind != TagKind::Closing) continue;
			for (size_t i = stack.size(); i > 0; i--)
			{
				if (!NamesEqual(stack[i - 1], tag.Name)) continue;
				stack.resize(i - 1);
				break;
			}
		}
		std::reverse(stack.begin(), stack.end());
		std::vector<std::wstring> unique;
		for (const auto& name : stack)
		{
			if (std::find(unique.begin(), unique.end(), name) == unique.end())
				unique.push_back(name);
		}
		return unique;
	}

	inline TagMatch FindTagMatch(const std::wstring& text, size_t caret)
	{
		caret = (std::min)(caret, text.size());
		const auto tags = ScanTags(text);
		if (tags.empty()) return {};
		std::vector<int> pairs(tags.size(), -1);
		std::vector<size_t> stack;
		for (size_t i = 0; i < tags.size(); i++)
		{
			if (tags[i].Kind == TagKind::Opening)
			{
				stack.push_back(i);
				continue;
			}
			if (tags[i].Kind != TagKind::Closing) continue;
			for (size_t j = stack.size(); j > 0; j--)
			{
				const size_t opening = stack[j - 1];
				if (!NamesEqual(tags[opening].Name, tags[i].Name)) continue;
				pairs[opening] = static_cast<int>(i);
				pairs[i] = static_cast<int>(opening);
				stack.resize(j - 1);
				break;
			}
		}

		int target = -1;
		for (size_t i = 0; i < tags.size(); i++)
		{
			const auto& tag = tags[i];
			if (caret >= tag.Start && caret <= tag.End)
			{
				target = static_cast<int>(i);
				if (caret >= tag.NameStart
					&& caret <= tag.NameStart + tag.NameLength) break;
			}
		}
		if (target < 0) return {};
		const auto& tag = tags[static_cast<size_t>(target)];
		TagMatch result;
		result.Name = tag.Name;
		result.Ranges.push_back({ tag.NameStart, tag.NameLength });
		const int pair = pairs[static_cast<size_t>(target)];
		if (pair >= 0)
		{
			const auto& other = tags[static_cast<size_t>(pair)];
			result.Ranges.push_back({ other.NameStart, other.NameLength });
			std::sort(result.Ranges.begin(), result.Ranges.end(),
				[](const auto& left, const auto& right)
				{ return left.Start < right.Start; });
		}
		return result;
	}

	inline CompletionContext GetCompletionContext(
		const std::wstring& text,
		size_t caret)
	{
		CompletionContext result;
		caret = (std::min)(caret, text.size());
		if (caret == 0) return result;
		const size_t tagStart = text.rfind(L'<', caret - 1);
		if (tagStart == std::wstring::npos) return result;
		const size_t lastClose = text.rfind(L'>', caret - 1);
		if (lastClose != std::wstring::npos && lastClose > tagStart) return result;
		if (text.compare(tagStart, 4, L"<!--") == 0
			|| text.compare(tagStart, 2, L"<?") == 0
			|| text.compare(tagStart, 2, L"<!") == 0)
			return result;

		size_t cursor = tagStart + 1;
		while (cursor < caret && std::iswspace(text[cursor])) cursor++;
		bool closing = false;
		if (cursor < caret && text[cursor] == L'/')
		{
			closing = true;
			cursor++;
		}
		const size_t nameStart = cursor;
		while (cursor < caret && IsNameCharacter(text[cursor])) cursor++;
		const size_t nameEnd = cursor;
		if (nameEnd == nameStart && cursor < caret) return result;
		if (cursor == caret)
		{
			result.Kind = closing
				? CompletionKind::ClosingElement : CompletionKind::Element;
			result.Prefix = text.substr(nameStart, caret - nameStart);
			result.ReplaceStart = nameStart;
			result.ReplaceEnd = caret;
			while (result.ReplaceEnd < text.size()
				&& IsNameCharacter(text[result.ReplaceEnd])) result.ReplaceEnd++;
			return result;
		}
		if (closing) return result;
		result.ElementName = text.substr(nameStart, nameEnd - nameStart);

		wchar_t quote = 0;
		size_t quoteStart = std::wstring::npos;
		size_t equalsBeforeQuote = std::wstring::npos;
		for (size_t i = nameEnd; i < caret; i++)
		{
			const wchar_t ch = text[i];
			if (quote != 0)
			{
				if (ch == quote) quote = 0;
				continue;
			}
			if (ch == L'=') equalsBeforeQuote = i;
			else if (ch == L'\'' || ch == L'"')
			{
				quote = ch;
				quoteStart = i;
			}
		}

		auto collectUsedAttributes = [&]()
		{
			std::set<std::wstring> seen;
			for (size_t i = nameEnd; i < caret;)
			{
				while (i < caret && std::iswspace(text[i])) i++;
				const size_t start = i;
				while (i < caret && IsNameCharacter(text[i])) i++;
				if (i > start)
				{
					auto name = text.substr(start, i - start);
					if (seen.insert(Lower(name)).second)
						result.UsedAttributes.push_back(std::move(name));
				}
				while (i < caret && !std::iswspace(text[i]))
				{
					if (text[i] == L'\'' || text[i] == L'"')
					{
						const wchar_t valueQuote = text[i++];
						while (i < caret && text[i] != valueQuote) i++;
						if (i < caret) i++;
					}
					else i++;
				}
			}
		};
		collectUsedAttributes();

		if (quote != 0 && quoteStart != std::wstring::npos
			&& equalsBeforeQuote != std::wstring::npos
			&& equalsBeforeQuote < quoteStart)
		{
			size_t attrEnd = equalsBeforeQuote;
			while (attrEnd > nameEnd && std::iswspace(text[attrEnd - 1])) attrEnd--;
			size_t attrStart = attrEnd;
			while (attrStart > nameEnd && IsNameCharacter(text[attrStart - 1])) attrStart--;
			if (attrStart == attrEnd) return CompletionContext{};
			result.Kind = CompletionKind::AttributeValue;
			result.AttributeName = text.substr(attrStart, attrEnd - attrStart);
			result.ReplaceStart = quoteStart + 1;
			result.ReplaceEnd = caret;
			while (result.ReplaceEnd < text.size()
				&& text[result.ReplaceEnd] != quote) result.ReplaceEnd++;
			result.Prefix = text.substr(result.ReplaceStart,
				caret - result.ReplaceStart);
			return result;
		}

		size_t attributeStart = caret;
		while (attributeStart > nameEnd
			&& !std::iswspace(text[attributeStart - 1])) attributeStart--;
		if (text.substr(attributeStart, caret - attributeStart).find(L'=')
			!= std::wstring::npos)
			return CompletionContext{};
		result.Kind = CompletionKind::Attribute;
		result.ReplaceStart = attributeStart;
		result.ReplaceEnd = caret;
		while (result.ReplaceEnd < text.size()
			&& IsNameCharacter(text[result.ReplaceEnd])) result.ReplaceEnd++;
		result.Prefix = text.substr(attributeStart, caret - attributeStart);
		return result;
	}

	inline bool StartsWithIgnoreCase(
		const std::wstring& value,
		const std::wstring& prefix)
	{
		if (prefix.size() > value.size()) return false;
		for (size_t i = 0; i < prefix.size(); i++)
		{
			if (std::towlower(value[i]) != std::towlower(prefix[i])) return false;
		}
		return true;
	}

	inline std::vector<std::wstring> FilterSuggestions(
		std::vector<std::wstring> candidates,
		const CompletionContext& context,
		size_t maximum = 80)
	{
		std::set<std::wstring> used;
		if (context.Kind == CompletionKind::Attribute)
		{
			for (const auto& item : context.UsedAttributes)
				used.insert(Lower(item));
		}
		std::vector<std::wstring> result;
		std::set<std::wstring> unique;
		for (auto& candidate : candidates)
		{
			const auto lower = Lower(candidate);
			if (!unique.insert(lower).second || used.contains(lower)) continue;
			if (!StartsWithIgnoreCase(candidate, context.Prefix)) continue;
			result.push_back(std::move(candidate));
		}
		std::sort(result.begin(), result.end(), [](const auto& left, const auto& right)
		{
			return Lower(left) < Lower(right);
		});
		if (result.size() > maximum) result.resize(maximum);
		return result;
	}

	inline std::wstring RemoveOneIndent(
		std::wstring indent,
		const std::wstring& indentUnit)
	{
		if (!indentUnit.empty() && indent.size() >= indentUnit.size()
			&& indent.compare(indent.size() - indentUnit.size(),
				indentUnit.size(), indentUnit) == 0)
		{
			indent.resize(indent.size() - indentUnit.size());
		}
		else if (!indent.empty())
		{
			indent.pop_back();
		}
		return indent;
	}

	/**
	 * Builds a single replacement for indenting or outdenting every touched
	 * line. A selection ending immediately after a newline does not pull the
	 * following untouched line into the edit.
	 */
	inline LineIndentEdit BuildLineIndentEdit(
		const std::wstring& text,
		size_t selectionStart,
		size_t selectionEnd,
		bool outdent,
		const std::wstring& indentUnit = L"\t")
	{
		LineIndentEdit result;
		selectionStart = (std::min)(selectionStart, text.size());
		selectionEnd = (std::min)(selectionEnd, text.size());
		if (selectionStart > selectionEnd)
			std::swap(selectionStart, selectionEnd);
		result.SelectionStart = selectionStart;
		result.SelectionEnd = selectionEnd;
		if (indentUnit.empty()) return result;

		const auto lineBreak = selectionStart == 0
			? std::wstring::npos : text.rfind(L'\n', selectionStart - 1);
		result.ReplaceStart = lineBreak == std::wstring::npos
			? 0 : lineBreak + 1;
		size_t effectiveEnd = selectionEnd;
		if (selectionEnd > selectionStart && selectionEnd > 0
			&& text[selectionEnd - 1] == L'\n')
			effectiveEnd--;
		const auto lastLineBreak = text.find(L'\n', effectiveEnd);
		const size_t replaceEnd = lastLineBreak == std::wstring::npos
			? text.size() : lastLineBreak + 1;
		result.ReplaceLength = replaceEnd - result.ReplaceStart;
		if (result.ReplaceLength == 0) return result;

		struct PrefixChange
		{
			size_t Position = 0;
			size_t Removed = 0;
			size_t Inserted = 0;
		};
		std::vector<PrefixChange> changes;
		result.Text.reserve(result.ReplaceLength
			+ (outdent ? 0 : indentUnit.size() * 4));
		size_t lineStart = result.ReplaceStart;
		while (lineStart < replaceEnd)
		{
			const auto newline = text.find(L'\n', lineStart);
			const size_t lineEnd = newline == std::wstring::npos
				? replaceEnd : (std::min)(replaceEnd, newline + 1);
			size_t removeCount = 0;
			if (outdent)
			{
				if (lineStart + indentUnit.size() <= lineEnd
					&& text.compare(lineStart, indentUnit.size(), indentUnit) == 0)
				{
					removeCount = indentUnit.size();
				}
				else if (lineStart < lineEnd && text[lineStart] == L'\t')
				{
					removeCount = 1;
				}
				else
				{
					const bool spacesOnly = std::all_of(
						indentUnit.begin(), indentUnit.end(),
						[](wchar_t ch) { return ch == L' '; });
					const size_t maximumSpaces = spacesOnly
						? indentUnit.size() : 4;
					while (removeCount < maximumSpaces
						&& lineStart + removeCount < lineEnd
						&& text[lineStart + removeCount] == L' ')
						removeCount++;
				}
				if (removeCount > 0)
				{
					changes.push_back({ lineStart, removeCount, 0 });
					result.Changed = true;
				}
			}
			else
			{
				result.Text += indentUnit;
				changes.push_back({ lineStart, 0, indentUnit.size() });
				result.Changed = true;
			}
			result.Text.append(text, lineStart + removeCount,
				lineEnd - lineStart - removeCount);
			lineStart = lineEnd;
		}
		if (!result.Changed)
		{
			result.Text = text.substr(result.ReplaceStart, result.ReplaceLength);
			return result;
		}

		auto mapPosition = [&](size_t position)
			{
				ptrdiff_t delta = 0;
				for (const auto& change : changes)
				{
					if (position < change.Position) break;
					if (change.Removed > 0
						&& position <= change.Position + change.Removed)
					{
						return static_cast<size_t>(
							static_cast<ptrdiff_t>(change.Position)
							+ delta + static_cast<ptrdiff_t>(change.Inserted));
					}
					if (change.Removed == 0 && position == change.Position)
					{
						return static_cast<size_t>(
							static_cast<ptrdiff_t>(change.Position)
							+ delta + static_cast<ptrdiff_t>(change.Inserted));
					}
					delta += static_cast<ptrdiff_t>(change.Inserted)
						- static_cast<ptrdiff_t>(change.Removed);
				}
				return static_cast<size_t>(
					static_cast<ptrdiff_t>(position) + delta);
			};
		result.SelectionStart = mapPosition(selectionStart);
		result.SelectionEnd = mapPosition(selectionEnd);
		return result;
	}

	inline NewLineEdit BuildNewLineEdit(
		const std::wstring& text,
		size_t selectionStart,
		size_t selectionEnd,
		const std::wstring& indentUnit = L"\t")
	{
		selectionStart = (std::min)(selectionStart, text.size());
		selectionEnd = (std::min)(selectionEnd, text.size());
		if (selectionStart > selectionEnd) std::swap(selectionStart, selectionEnd);
		const auto lineBreak = selectionStart == 0
			? std::wstring::npos : text.rfind(L'\n', selectionStart - 1);
		const size_t lineStart = lineBreak == std::wstring::npos ? 0 : lineBreak + 1;
		size_t indentEnd = lineStart;
		while (indentEnd < selectionStart
			&& (text[indentEnd] == L' ' || text[indentEnd] == L'\t')) indentEnd++;
		std::wstring indent = text.substr(lineStart, indentEnd - lineStart);
		size_t contentEnd = selectionStart;
		while (contentEnd > lineStart && std::iswspace(text[contentEnd - 1])) contentEnd--;

		std::wstring openingName;
		const auto tags = ScanTags(text, contentEnd);
		if (!tags.empty() && tags.back().End == contentEnd
			&& tags.back().Kind == TagKind::Opening)
		{
			openingName = tags.back().Name;
		}

		size_t suffix = selectionEnd;
		while (suffix < text.size()
			&& (text[suffix] == L' ' || text[suffix] == L'\t')) suffix++;
		std::wstring closingName;
		if (suffix + 2 <= text.size() && text.compare(suffix, 2, L"</") == 0)
		{
			size_t nameStart = suffix + 2;
			size_t nameEnd = nameStart;
			while (nameEnd < text.size() && IsNameCharacter(text[nameEnd])) nameEnd++;
			closingName = text.substr(nameStart, nameEnd - nameStart);
		}

		NewLineEdit result;
		if (!openingName.empty() && NamesEqual(openingName, closingName))
		{
			const std::wstring innerIndent = indent + indentUnit;
			result.Text = L"\r\n" + innerIndent + L"\r\n" + indent;
			result.CaretOffset = 2 + innerIndent.size();
			return result;
		}

		if (!openingName.empty()) indent += indentUnit;
		else if (contentEnd == lineStart && !closingName.empty())
			indent = RemoveOneIndent(std::move(indent), indentUnit);
		result.Text = L"\r\n" + indent;
		result.CaretOffset = result.Text.size();
		return result;
	}
}
