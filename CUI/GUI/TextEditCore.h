#pragma once

#include <algorithm>
#include <string>

namespace CuiTextEdit
{
	struct EditOptions
	{
		bool allowMultiLine = false;
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
		const unsigned int value = static_cast<unsigned int>(ch);
		return value >= 0xD800 && value <= 0xDBFF;
	}

	inline bool IsLowSurrogate(wchar_t ch)
	{
		const unsigned int value = static_cast<unsigned int>(ch);
		return value >= 0xDC00 && value <= 0xDFFF;
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
					normalized.push_back(ch);
				}
			}
			else
			{
				if (ch == L'\r')
				{
					normalized.push_back(L' ');
					if (i + 1 < input.size() && input[i + 1] == L'\n')
						i++;
				}
				else if (ch == L'\n')
				{
					normalized.push_back(L' ');
				}
				else
				{
					normalized.push_back(ch);
				}
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

		if (preserveCrLf)
		{
			if (IsBetweenCrLf(text, start)) start--;
			if (IsBetweenCrLf(text, end)) end++;
		}

		if (IsBetweenSurrogatePair(text, start)) start--;
		if (IsBetweenSurrogatePair(text, end)) end++;

		start = ClampIndex(start, text.size());
		end = ClampIndex(end, text.size());
	}

	inline int GetNextCaretIndex(const std::wstring& text, int index, bool preserveCrLf)
	{
		index = ClampIndex(index, text.size());
		if (index >= static_cast<int>(text.size()))
			return static_cast<int>(text.size());
		if (preserveCrLf && HasCrLfAt(text, index))
			return (std::min)(index + 2, static_cast<int>(text.size()));
		if (HasSurrogatePairAt(text, index))
			return (std::min)(index + 2, static_cast<int>(text.size()));
		if ((preserveCrLf && IsBetweenCrLf(text, index)) || IsBetweenSurrogatePair(text, index))
			return (std::min)(index + 1, static_cast<int>(text.size()));
		return index + 1;
	}

	inline int GetPreviousCaretIndex(const std::wstring& text, int index, bool preserveCrLf)
	{
		index = ClampIndex(index, text.size());
		if (index <= 0)
			return 0;
		if (preserveCrLf && index >= 2 && text[static_cast<size_t>(index) - 2] == L'\r' && text[static_cast<size_t>(index) - 1] == L'\n')
			return index - 2;
		if (index >= 2 && IsHighSurrogate(text[static_cast<size_t>(index) - 2]) && IsLowSurrogate(text[static_cast<size_t>(index) - 1]))
			return index - 2;
		if ((preserveCrLf && IsBetweenCrLf(text, index)) || IsBetweenSurrogatePair(text, index))
			return index - 1;
		return index - 1;
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

		if (allowedLength > 0 && allowedLength < replacement.size())
		{
			if (replacement[allowedLength - 1] == L'\r' && replacement[allowedLength] == L'\n')
				allowedLength--;
			if (allowedLength > 0
				&& allowedLength < replacement.size()
				&& IsHighSurrogate(replacement[allowedLength - 1])
				&& IsLowSurrogate(replacement[allowedLength]))
			{
				allowedLength--;
			}
		}

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
		if (caretIndex <= 0)
			return false;
		if (preserveCrLf && IsBetweenCrLf(text, caretIndex))
		{
			eraseStart = caretIndex - 1;
			eraseLength = 2;
			return true;
		}
		if (IsBetweenSurrogatePair(text, caretIndex))
		{
			eraseStart = caretIndex - 1;
			eraseLength = 2;
			return true;
		}
		if (preserveCrLf && caretIndex >= 2 && text[static_cast<size_t>(caretIndex) - 2] == L'\r' && text[static_cast<size_t>(caretIndex) - 1] == L'\n')
		{
			eraseStart = caretIndex - 2;
			eraseLength = 2;
			return true;
		}
		if (caretIndex >= 2 && IsHighSurrogate(text[static_cast<size_t>(caretIndex) - 2]) && IsLowSurrogate(text[static_cast<size_t>(caretIndex) - 1]))
		{
			eraseStart = caretIndex - 2;
			eraseLength = 2;
			return true;
		}

		eraseStart = caretIndex - 1;
		eraseLength = 1;
		return true;
	}

	inline bool GetDeleteEraseRange(
		const std::wstring& text,
		int caretIndex,
		bool preserveCrLf,
		int& eraseStart,
		int& eraseLength)
	{
		caretIndex = ClampIndex(caretIndex, text.size());
		if (caretIndex >= static_cast<int>(text.size()))
			return false;
		if (preserveCrLf && IsBetweenCrLf(text, caretIndex))
		{
			eraseStart = caretIndex - 1;
			eraseLength = 2;
			return true;
		}
		if (IsBetweenSurrogatePair(text, caretIndex))
		{
			eraseStart = caretIndex - 1;
			eraseLength = 2;
			return true;
		}
		if (preserveCrLf && HasCrLfAt(text, caretIndex))
		{
			eraseStart = caretIndex;
			eraseLength = 2;
			return true;
		}
		if (HasSurrogatePairAt(text, caretIndex))
		{
			eraseStart = caretIndex;
			eraseLength = 2;
			return true;
		}

		eraseStart = caretIndex;
		eraseLength = 1;
		return true;
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
