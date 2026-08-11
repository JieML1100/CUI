#pragma once

#include "TextBoundary.h"

#include <algorithm>
#include <cstdint>
#include <cwctype>
#include <string>

namespace CuiTextEdit
{
	struct EditOptions
	{
		bool allowMultiLine = false;
		bool acceptsTab = false;
		size_t maxTextLength = 0;
	};

	struct SelectionSpan
	{
		int start = 0;
		int end = 0;

		int Length() const
		{
			return end - start;
		}

		bool HasSelection() const
		{
			return end > start;
		}
	};

	struct EditResult
	{
		bool applied = false;
		bool textChanged = false;
		int replaceStart = 0;
		int replaceEnd = 0;
		int caret = 0;
		std::wstring removedText;
		std::wstring insertedText;
	};

	struct FindOptions
	{
		bool matchCase = false;
		bool wrap = true;
	};

	struct ReplaceAllResult
	{
		std::wstring text;
		size_t replacements = 0;
	};

	struct TextPosition
	{
		size_t line = 1;
		size_t column = 1;
	};

	inline int ClampIndex(int index, size_t textLength)
	{
		return (std::clamp)(index, 0, static_cast<int>(textLength));
	}

	inline SelectionSpan NormalizeSelection(int selectionStart, int selectionEnd, size_t textLength)
	{
		selectionStart = ClampIndex(selectionStart, textLength);
		selectionEnd = ClampIndex(selectionEnd, textLength);
		if (selectionStart > selectionEnd)
			std::swap(selectionStart, selectionEnd);
		return SelectionSpan{ selectionStart, selectionEnd };
	}

	inline bool IsHighSurrogate(wchar_t ch)
	{
		return CuiTextBoundary::IsHighSurrogate(ch);
	}

	inline bool IsLowSurrogate(wchar_t ch)
	{
		return CuiTextBoundary::IsLowSurrogate(ch);
	}

	inline bool IsTextElementBoundary(
		const std::wstring& text, int index, bool preserveCrLf = true)
	{
		return CuiTextBoundary::IsTextElementBoundary(
			text, static_cast<size_t>(ClampIndex(index, text.size())),
			preserveCrLf);
	}

	inline bool HasCrLfAt(const std::wstring& text, int index)
	{
		return index >= 0
			&& index + 1 < static_cast<int>(text.size())
			&& text[static_cast<size_t>(index)] == L'\r'
			&& text[static_cast<size_t>(index) + 1] == L'\n';
	}

	inline bool IsBetweenCrLf(const std::wstring& text, int index)
	{
		return index > 0
			&& index < static_cast<int>(text.size())
			&& text[static_cast<size_t>(index) - 1] == L'\r'
			&& text[static_cast<size_t>(index)] == L'\n';
	}

	inline bool HasSurrogatePairAt(const std::wstring& text, int index)
	{
		return index >= 0
			&& index + 1 < static_cast<int>(text.size())
			&& IsHighSurrogate(text[static_cast<size_t>(index)])
			&& IsLowSurrogate(text[static_cast<size_t>(index) + 1]);
	}

	inline bool IsBetweenSurrogatePair(const std::wstring& text, int index)
	{
		return index > 0
			&& index < static_cast<int>(text.size())
			&& IsHighSurrogate(text[static_cast<size_t>(index) - 1])
			&& IsLowSurrogate(text[static_cast<size_t>(index)]);
	}

	/** Expands a rendering chunk so its end never bisects a text element. */
	inline size_t ExpandChunkToTextElementBoundary(
		const std::wstring& text, size_t start, size_t requestedLength)
	{
		start = (std::min)(start, text.size());
		size_t end = start + (std::min)(
			requestedLength, text.size() - start);
		if (!CuiTextBoundary::IsTextElementBoundary(text, end, true))
			end = CuiTextBoundary::GetNextTextElementBoundary(
				text, end, true);
		return end - start;
	}

	/**
	 * A virtual text-layout block. Length covers its complete source range.
	 * LayoutLength omits a terminating CRLF when another block follows; the
	 * layout layer represents that separator with a styled zero-width sentinel
	 * so independent layouts do not both account for the following visual line.
	 */
	struct TextLayoutChunk
	{
		size_t Length = 0;
		size_t LayoutLength = 0;
	};

	/**
	 * Chooses a chunk boundary only after a canonical CRLF. If the requested
	 * size falls inside a long paragraph, the chunk expands through that whole
	 * paragraph instead of cutting shaping or wrapping context mid-line.
	 */
	inline TextLayoutChunk FindSafeTextLayoutChunk(
		const std::wstring& text, size_t start, size_t requestedLength)
	{
		start = (std::min)(start, text.size());
		const size_t remaining = text.size() - start;
		if (remaining == 0) return {};
		requestedLength = (std::max)(requestedLength, size_t{ 1 });
		const size_t requestedEnd = start + (std::min)(
			requestedLength, remaining);
		if (requestedEnd == text.size()) return { remaining, remaining };

		auto isBreakEnd = [&](size_t end)
		{
			return end >= start + 2 && end <= text.size()
				&& text[end - 2] == L'\r' && text[end - 1] == L'\n';
		};

		size_t boundary = requestedEnd;
		while (boundary > start + 1 && !isBreakEnd(boundary))
			--boundary;
		if (!isBreakEnd(boundary))
		{
			boundary = (std::max)(requestedEnd, start + 2);
			while (boundary <= text.size() && !isBreakEnd(boundary))
				++boundary;
		}
		if (boundary > text.size()) return { remaining, remaining };

		const size_t length = boundary - start;
		// A final trailing CRLF intentionally retains DirectWrite's terminal
		// empty line. Only seams with following text elide the delimiter.
		return boundary < text.size()
			? TextLayoutChunk{ length, length - 2 }
			: TextLayoutChunk{ length, length };
	}

	inline bool IsTextInputChar(wchar_t ch)
	{
		return ch >= L' ' && ch != 0x7F;
	}

	inline std::wstring NormalizeInput(const std::wstring& input, const EditOptions& options)
	{
		if (input.empty()) return input;

		std::wstring normalized;
		normalized.reserve(input.size());
		for (size_t i = 0; i < input.size(); i++)
		{
			const wchar_t ch = input[i];
			if (ch == L'\0')
				continue;

			if (options.allowMultiLine)
			{
				if (ch == L'\r')
				{
					normalized.append(L"\r\n");
					if (i + 1 < input.size() && input[i + 1] == L'\n')
						i++;
				}
				else if (ch == L'\n')
				{
					normalized.append(L"\r\n");
				}
				else
				{
					normalized.push_back(
						ch == L'\t' && !options.acceptsTab ? L' ' : ch);
				}
			}
			else
			{
				// WPF's plain-text editor filters interactive input to the
				// first line when AcceptsReturn is false.
				if (ch == L'\r' || ch == L'\n')
					break;
				normalized.push_back(
					ch == L'\t' && !options.acceptsTab ? L' ' : ch);
			}
		}

		return normalized;
	}

	inline void NormalizeSelectionForTextElements(const std::wstring& text, int& start, int& end, bool preserveCrLf)
	{
		start = ClampIndex(start, text.size());
		end = ClampIndex(end, text.size());
		if (start > end)
			std::swap(start, end);

		start = static_cast<int>(
			CuiTextBoundary::SnapToTextElementBoundary(
				text, static_cast<size_t>(start), false, preserveCrLf));
		end = static_cast<int>(
			CuiTextBoundary::SnapToTextElementBoundary(
				text, static_cast<size_t>(end), true, preserveCrLf));

		start = ClampIndex(start, text.size());
		end = ClampIndex(end, text.size());
	}

	inline int GetNextCaretIndex(const std::wstring& text, int index, bool preserveCrLf)
	{
		index = ClampIndex(index, text.size());
		return static_cast<int>(
			CuiTextBoundary::GetNextTextElementBoundary(
				text, static_cast<size_t>(index), preserveCrLf));
	}

	inline int GetPreviousCaretIndex(const std::wstring& text, int index, bool preserveCrLf)
	{
		index = ClampIndex(index, text.size());
		return static_cast<int>(
			CuiTextBoundary::GetPreviousTextElementBoundary(
				text, static_cast<size_t>(index), preserveCrLf));
	}

	inline int GetLineStartIndex(const std::wstring& text, int index)
	{
		index = ClampIndex(index, text.size());
		for (int i = index; i > 0; i--)
		{
			if (text[static_cast<size_t>(i) - 1] == L'\n')
				return i;
		}
		return 0;
	}

	inline int GetLineEndIndex(const std::wstring& text, int index)
	{
		index = ClampIndex(index, text.size());
		for (int i = index; i < static_cast<int>(text.size()); i++)
		{
			const wchar_t ch = text[static_cast<size_t>(i)];
			if (ch == L'\r' || ch == L'\n')
				return i;
		}
		return static_cast<int>(text.size());
	}

	enum class WordCharacterClass : uint8_t
	{
		WhiteSpace,
		Word,
		Punctuation,
		LineBreak
	};

	inline WordCharacterClass ClassifyWordCharacter(
		const std::wstring& text, int index)
	{
		index = ClampIndex(index, text.size());
		if (index >= static_cast<int>(text.size()))
			return WordCharacterClass::WhiteSpace;
		const auto value = CuiTextBoundary::GetCodePointAt(
			text, static_cast<size_t>(index));
		if (value == L'\r' || value == L'\n')
			return WordCharacterClass::LineBreak;
		if (CuiTextBoundary::IsWhiteSpaceCodePoint(value))
			return WordCharacterClass::WhiteSpace;
		if (CuiTextBoundary::IsWordCodePoint(value)
			|| value > 0xFFFFu)
		{
			return WordCharacterClass::Word;
		}
		return WordCharacterClass::Punctuation;
	}

	/**
	 * Returns the Unicode text-element-safe run selected by WPF-style double click.
	 * This intentionally distinguishes words, whitespace, punctuation and line
	 * breaks instead of expanding every double click to the whole document.
	 */
	inline SelectionSpan GetWordSelectionSpan(
		const std::wstring& text, int index, bool preserveCrLf)
	{
		if (text.empty()) return {};
		index = ClampIndex(index, text.size());
		if (index == static_cast<int>(text.size()))
			index = GetPreviousCaretIndex(text, index, preserveCrLf);
		if (IsBetweenSurrogatePair(text, index))
			index--;
		if (preserveCrLf && IsBetweenCrLf(text, index))
			index--;

		const auto category = ClassifyWordCharacter(text, index);
		int start = index;
		int end = GetNextCaretIndex(text, index, preserveCrLf);
		while (start > 0)
		{
			const int previous =
				GetPreviousCaretIndex(text, start, preserveCrLf);
			if (ClassifyWordCharacter(text, previous) != category)
				break;
			start = previous;
		}
		while (end < static_cast<int>(text.size())
			&& ClassifyWordCharacter(text, end) == category)
		{
			end = GetNextCaretIndex(text, end, preserveCrLf);
		}
		NormalizeSelectionForTextElements(
			text, start, end, preserveCrLf);
		return SelectionSpan{ start, end };
	}

	inline int GetNextWordCaretIndex(
		const std::wstring& text, int index, bool preserveCrLf)
	{
		index = ClampIndex(index, text.size());
		if (index >= static_cast<int>(text.size()))
			return static_cast<int>(text.size());

		const auto firstCategory =
			ClassifyWordCharacter(text, index);
		while (index < static_cast<int>(text.size())
			&& ClassifyWordCharacter(text, index) == firstCategory)
		{
			index = GetNextCaretIndex(
				text, index, preserveCrLf);
		}
		while (index < static_cast<int>(text.size()))
		{
			const auto category =
				ClassifyWordCharacter(text, index);
			if (category != WordCharacterClass::WhiteSpace
				&& category != WordCharacterClass::LineBreak)
			{
				break;
			}
			index = GetNextCaretIndex(
				text, index, preserveCrLf);
		}
		return index;
	}

	inline int GetPreviousWordCaretIndex(
		const std::wstring& text, int index, bool preserveCrLf)
	{
		index = ClampIndex(index, text.size());
		while (index > 0)
		{
			const int previous = GetPreviousCaretIndex(
				text, index, preserveCrLf);
			const auto category =
				ClassifyWordCharacter(text, previous);
			if (category != WordCharacterClass::WhiteSpace
				&& category != WordCharacterClass::LineBreak)
			{
				index = previous;
				break;
			}
			index = previous;
		}
		if (index <= 0) return 0;

		const auto category =
			ClassifyWordCharacter(text, index);
		while (index > 0)
		{
			const int previous = GetPreviousCaretIndex(
				text, index, preserveCrLf);
			if (ClassifyWordCharacter(text, previous)
				!= category)
			{
				break;
			}
			index = previous;
		}
		return index;
	}

	inline wchar_t FoldSearchCharacter(wchar_t ch)
	{
		return static_cast<wchar_t>(std::towlower(static_cast<wint_t>(ch)));
	}

	inline bool MatchesAt(
		const std::wstring& text,
		const std::wstring& query,
		size_t position,
		bool matchCase = false)
	{
		if (query.empty() || position > text.size()
			|| query.size() > text.size() - position)
		{
			return false;
		}

		for (size_t i = 0; i < query.size(); i++)
		{
			wchar_t actual = text[position + i];
			wchar_t expected = query[i];
			if (!matchCase)
			{
				actual = FoldSearchCharacter(actual);
				expected = FoldSearchCharacter(expected);
			}
			if (actual != expected)
				return false;
		}
		return true;
	}

	/** Finds a UTF-16 substring from startIndex, optionally backwards and wrapping. */
	inline int FindText(
		const std::wstring& text,
		const std::wstring& query,
		int startIndex,
		bool backwards = false,
		const FindOptions& options = {})
	{
		if (query.empty() || query.size() > text.size())
			return -1;

		const int lastStart = static_cast<int>(text.size() - query.size());
		if (!backwards)
		{
			const int first = (std::max)(0, startIndex);
			for (int i = first; i <= lastStart; i++)
			{
				if (MatchesAt(text, query, static_cast<size_t>(i), options.matchCase))
					return i;
			}
			if (!options.wrap) return -1;
			const int wrappedEnd = (std::min)(lastStart, first - 1);
			for (int i = 0; i <= wrappedEnd; i++)
			{
				if (MatchesAt(text, query, static_cast<size_t>(i), options.matchCase))
					return i;
			}
			return -1;
		}

		const int first = (std::min)(startIndex, lastStart);
		for (int i = first; i >= 0; i--)
		{
			if (MatchesAt(text, query, static_cast<size_t>(i), options.matchCase))
				return i;
		}
		if (!options.wrap) return -1;
		const int wrappedEnd = (std::max)(0, first + 1);
		for (int i = lastStart; i >= wrappedEnd; i--)
		{
			if (MatchesAt(text, query, static_cast<size_t>(i), options.matchCase))
				return i;
		}
		return -1;
	}

	inline ReplaceAllResult ReplaceAllText(
		const std::wstring& text,
		const std::wstring& query,
		const std::wstring& replacement,
		bool matchCase = false)
	{
		ReplaceAllResult result;
		if (query.empty() || query.size() > text.size())
		{
			result.text = text;
			return result;
		}

		result.text.reserve(text.size());
		size_t copiedUntil = 0;
		for (size_t i = 0; i + query.size() <= text.size();)
		{
			if (!MatchesAt(text, query, i, matchCase))
			{
				i++;
				continue;
			}
			result.text.append(text, copiedUntil, i - copiedUntil);
			result.text.append(replacement);
			result.replacements++;
			i += query.size();
			copiedUntil = i;
		}
		result.text.append(text, copiedUntil, std::wstring::npos);
		return result;
	}

	/** Returns a 1-based user-facing line/column for a UTF-16 caret offset. */
	inline TextPosition GetTextPosition(const std::wstring& text, size_t index)
	{
		index = (std::min)(index, text.size());
		index = CuiTextBoundary::SnapToTextElementBoundary(
			text, index, false, true);
		TextPosition result;
		for (size_t i = 0; i < index;)
		{
			if (text[i] == L'\r')
			{
				result.line++;
				result.column = 1;
				i++;
				if (i < index && text[i] == L'\n') i++;
				continue;
			}
			if (text[i] == L'\n')
			{
				result.line++;
				result.column = 1;
				i++;
				continue;
			}
			i = CuiTextBoundary::GetNextTextElementBoundary(
				text, i, true);
			result.column++;
		}
		return result;
	}

	inline std::wstring LimitReplacementForMaxLength(
		const std::wstring& text,
		const SelectionSpan& span,
		const std::wstring& replacement,
		size_t maxTextLength)
	{
		if (maxTextLength == 0)
			return replacement;

		const size_t retainedLength = text.size() - static_cast<size_t>(span.Length());
		if (retainedLength >= maxTextLength)
			return L"";

		size_t allowedLength = maxTextLength - retainedLength;
		if (replacement.size() <= allowedLength)
			return replacement;

		allowedLength = CuiTextBoundary::SnapToTextElementBoundary(
			replacement, allowedLength, false, true);

		return replacement.substr(0, allowedLength);
	}

	inline EditResult ReplaceSelection(
		std::wstring& text,
		int& selectionStart,
		int& selectionEnd,
		const std::wstring& input,
		const EditOptions& options)
	{
		SelectionSpan span = NormalizeSelection(selectionStart, selectionEnd, text.size());
		std::wstring replacement = NormalizeInput(input, options);
		replacement = LimitReplacementForMaxLength(text, span, replacement, options.maxTextLength);

		EditResult result;
		result.replaceStart = span.start;
		result.replaceEnd = span.end;
		result.caret = span.start;
		if (!span.HasSelection() && replacement.empty())
			return result;

		result.applied = true;
		result.removedText = text.substr(static_cast<size_t>(span.start), static_cast<size_t>(span.Length()));
		result.insertedText = replacement;
		result.textChanged = result.removedText != result.insertedText;
		if (result.textChanged)
			text.replace(static_cast<size_t>(span.start), static_cast<size_t>(span.Length()), replacement);

		result.caret = span.start + static_cast<int>(replacement.size());
		selectionStart = selectionEnd = result.caret;
		return result;
	}

	inline bool GetBackspaceEraseRange(
		const std::wstring& text,
		int caretIndex,
		bool preserveCrLf,
		int& eraseStart,
		int& eraseLength)
	{
		caretIndex = ClampIndex(caretIndex, text.size());
		if (caretIndex <= 0) return false;
		const auto caret = static_cast<size_t>(caretIndex);
		const auto start = CuiTextBoundary::GetPreviousTextElementBoundary(
			text, caret, preserveCrLf);
		const auto end = CuiTextBoundary::IsTextElementBoundary(
			text, caret, preserveCrLf)
			? caret
			: CuiTextBoundary::GetNextTextElementBoundary(
				text, caret, preserveCrLf);
		eraseStart = static_cast<int>(start);
		eraseLength = static_cast<int>(end - start);
		return eraseLength > 0;
	}

	inline bool GetDeleteEraseRange(
		const std::wstring& text,
		int caretIndex,
		bool preserveCrLf,
		int& eraseStart,
		int& eraseLength)
	{
		caretIndex = ClampIndex(caretIndex, text.size());
		if (caretIndex >= static_cast<int>(text.size())) return false;
		const auto caret = static_cast<size_t>(caretIndex);
		const auto start = CuiTextBoundary::IsTextElementBoundary(
			text, caret, preserveCrLf)
			? caret
			: CuiTextBoundary::GetPreviousTextElementBoundary(
				text, caret, preserveCrLf);
		const auto end = CuiTextBoundary::GetNextTextElementBoundary(
			text, caret, preserveCrLf);
		eraseStart = static_cast<int>(start);
		eraseLength = static_cast<int>(end - start);
		return eraseLength > 0;
	}

	inline EditResult EraseRange(std::wstring& text, int& selectionStart, int& selectionEnd, int start, int length)
	{
		EditResult result;
		start = ClampIndex(start, text.size());
		const int end = ClampIndex(start + length, text.size());
		if (end <= start)
			return result;

		result.applied = true;
		result.textChanged = true;
		result.replaceStart = start;
		result.replaceEnd = end;
		result.caret = start;
		result.removedText = text.substr(static_cast<size_t>(start), static_cast<size_t>(end - start));
		text.erase(static_cast<size_t>(start), static_cast<size_t>(end - start));
		selectionStart = selectionEnd = start;
		return result;
	}

	inline EditResult Backspace(std::wstring& text, int& selectionStart, int& selectionEnd, const EditOptions& options)
	{
		SelectionSpan span = NormalizeSelection(selectionStart, selectionEnd, text.size());
		NormalizeSelectionForTextElements(text, span.start, span.end, options.allowMultiLine);
		if (span.HasSelection())
			return EraseRange(text, selectionStart, selectionEnd, span.start, span.Length());

		int eraseStart = 0;
		int eraseLength = 0;
		if (!GetBackspaceEraseRange(text, selectionEnd, options.allowMultiLine, eraseStart, eraseLength))
			return EditResult{};

		return EraseRange(text, selectionStart, selectionEnd, eraseStart, eraseLength);
	}

	inline EditResult DeleteForward(std::wstring& text, int& selectionStart, int& selectionEnd, const EditOptions& options)
	{
		SelectionSpan span = NormalizeSelection(selectionStart, selectionEnd, text.size());
		NormalizeSelectionForTextElements(text, span.start, span.end, options.allowMultiLine);
		if (span.HasSelection())
			return EraseRange(text, selectionStart, selectionEnd, span.start, span.Length());

		int eraseStart = 0;
		int eraseLength = 0;
		if (!GetDeleteEraseRange(text, selectionEnd, options.allowMultiLine, eraseStart, eraseLength))
			return EditResult{};

		return EraseRange(text, selectionStart, selectionEnd, eraseStart, eraseLength);
	}
}
