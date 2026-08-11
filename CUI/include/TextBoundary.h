#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

/**
 * Unicode text-element boundary helpers shared by every CUI text editor.
 *
 * Offsets are UTF-16 code-unit offsets. The implementation uses Windows
 * character properties plus explicit extended-text-element rules for CRLF,
 * surrogate pairs, combining/spacing marks, Hangul syllables, Indic linkers,
 * emoji modifiers, variation/tag selectors, regional-indicator pairs, and
 * extended-pictographic ZWJ sequences. Structural RichText spans may still
 * cross these boundaries; these functions define caret/edit stops, not
 * document-tree validity or a claim of exhaustive Unicode conformance.
 */
namespace CuiTextBoundary
{
	bool IsHighSurrogate(wchar_t value) noexcept;
	bool IsLowSurrogate(wchar_t value) noexcept;

	/** Returns whether position is a legal caret/edit boundary. */
	bool IsTextElementBoundary(
		std::wstring_view text,
		std::size_t position,
		bool preserveCrLf = true) noexcept;

	/** Returns the first legal boundary strictly after position. */
	std::size_t GetNextTextElementBoundary(
		std::wstring_view text,
		std::size_t position,
		bool preserveCrLf = true) noexcept;

	/** Returns the first legal boundary strictly before position. */
	std::size_t GetPreviousTextElementBoundary(
		std::wstring_view text,
		std::size_t position,
		bool preserveCrLf = true) noexcept;

	/** Snaps an invalid offset to the requested enclosing boundary. */
	std::size_t SnapToTextElementBoundary(
		std::wstring_view text,
		std::size_t position,
		bool forward,
		bool preserveCrLf = true) noexcept;

	/** Unicode-aware classifications used by word navigation. */
	bool IsWhiteSpaceCodePoint(std::uint32_t value) noexcept;
	bool IsWordCodePoint(std::uint32_t value) noexcept;
	std::uint32_t GetCodePointAt(
		std::wstring_view text, std::size_t position) noexcept;
}
