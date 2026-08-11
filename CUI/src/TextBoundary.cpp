#define NOMINMAX
#include "TextBoundary.h"

#include <Windows.h>

#include <algorithm>

namespace
{
	struct CodePoint final
	{
		std::uint32_t Value = 0;
		std::size_t Start = 0;
		std::size_t End = 0;
	};

	CodePoint DecodeAt(
		std::wstring_view text, std::size_t position) noexcept
	{
		position = (std::min)(position, text.size());
		if (position >= text.size())
			return { 0, text.size(), text.size() };
		const auto first = static_cast<std::uint32_t>(text[position]);
		if (CuiTextBoundary::IsHighSurrogate(text[position])
			&& position + 1 < text.size()
			&& CuiTextBoundary::IsLowSurrogate(text[position + 1]))
		{
			const auto second =
				static_cast<std::uint32_t>(text[position + 1]);
			return { 0x10000u + ((first - 0xD800u) << 10)
				+ (second - 0xDC00u), position, position + 2 };
		}
		return { first, position, position + 1 };
	}

	CodePoint DecodeBefore(
		std::wstring_view text, std::size_t position) noexcept
	{
		position = (std::min)(position, text.size());
		if (position == 0) return {};
		std::size_t start = position - 1;
		if (CuiTextBoundary::IsLowSurrogate(text[start])
			&& start > 0
			&& CuiTextBoundary::IsHighSurrogate(text[start - 1]))
		{
			--start;
		}
		return DecodeAt(text, start);
	}

	bool InRange(
		std::uint32_t value,
		std::uint32_t first,
		std::uint32_t last) noexcept
	{
		return value >= first && value <= last;
	}

	bool QueryCharacterType(
		std::uint32_t value, DWORD type, WORD mask) noexcept
	{
		wchar_t text[2]{};
		int length = 1;
		if (value > 0xFFFFu && value <= 0x10FFFFu)
		{
			value -= 0x10000u;
			text[0] = static_cast<wchar_t>(0xD800u + (value >> 10));
			text[1] = static_cast<wchar_t>(0xDC00u + (value & 0x3FFu));
			length = 2;
		}
		else
		{
			text[0] = static_cast<wchar_t>(value);
		}
		WORD result[2]{};
		return GetStringTypeW(type, text, length, result) != FALSE
			&& (result[0] & mask) != 0;
	}

	bool IsControl(std::uint32_t value) noexcept
	{
		if (value == 0x200Cu || value == 0x200Du) return false;
		if (value >= 0xE0020u && value <= 0xE007Fu) return false;
		if (QueryCharacterType(value, CT_CTYPE1, C1_CNTRL)) return true;
		return value == 0x00ADu || value == 0x061Cu
			|| value == 0x180Eu
			|| InRange(value, 0x200Bu, 0x200Fu)
			|| InRange(value, 0x202Au, 0x202Eu)
			|| InRange(value, 0x2060u, 0x206Fu)
			|| value == 0xFEFFu
			|| InRange(value, 0xFFF9u, 0xFFFBu)
			|| InRange(value, 0x1BCA0u, 0x1BCA3u)
			|| InRange(value, 0x1D173u, 0x1D17Au)
			|| value == 0xE0001u;
	}

	bool IsExtend(std::uint32_t value) noexcept
	{
		if (value == 0x200Cu) return true; // ZWNJ
		if (InRange(value, 0xFE00u, 0xFE0Fu)
			|| InRange(value, 0xE0100u, 0xE01EFu)
			|| InRange(value, 0x1F3FBu, 0x1F3FFu)
			|| InRange(value, 0xE0020u, 0xE007Fu))
		{
			return true;
		}
		return QueryCharacterType(value, CT_CTYPE3,
			C3_NONSPACING | C3_DIACRITIC | C3_VOWELMARK);
	}

	bool IsPrepend(std::uint32_t value) noexcept
	{
		return InRange(value, 0x0600u, 0x0605u)
			|| value == 0x06DDu || value == 0x070Fu
			|| InRange(value, 0x0890u, 0x0891u)
			|| value == 0x08E2u || value == 0x0D4Eu
			|| value == 0x110BDu || value == 0x110CDu
			|| InRange(value, 0x111C2u, 0x111C3u)
			|| value == 0x1193Fu || value == 0x11941u
			|| value == 0x11A3Au
			|| InRange(value, 0x11A84u, 0x11A89u)
			|| value == 0x11D46u;
	}

	bool IsRegionalIndicator(std::uint32_t value) noexcept
	{
		return InRange(value, 0x1F1E6u, 0x1F1FFu);
	}

	bool IsExtendedPictographic(std::uint32_t value) noexcept
	{
		if (InRange(value, 0x1F000u, 0x1FAFFu)) return true;
		if (value == 0x00A9u || value == 0x00AEu
			|| value == 0x203Cu || value == 0x2049u
			|| value == 0x2122u || value == 0x2139u
			|| InRange(value, 0x2194u, 0x2199u)
			|| InRange(value, 0x21A9u, 0x21AAu)
			|| InRange(value, 0x231Au, 0x231Bu)
			|| value == 0x2328u || value == 0x2388u
			|| value == 0x23CFu
			|| InRange(value, 0x23E9u, 0x23F3u)
			|| InRange(value, 0x23F8u, 0x23FAu)
			|| value == 0x24C2u
			|| InRange(value, 0x25AAu, 0x25ABu)
			|| value == 0x25B6u || value == 0x25C0u
			|| InRange(value, 0x25FBu, 0x25FEu)
			|| InRange(value, 0x2600u, 0x27BFu)
			|| InRange(value, 0x2934u, 0x2935u)
			|| InRange(value, 0x2B05u, 0x2B07u)
			|| InRange(value, 0x2B1Bu, 0x2B1Cu)
			|| value == 0x2B50u || value == 0x2B55u
			|| value == 0x3030u || value == 0x303Du
			|| value == 0x3297u || value == 0x3299u)
		{
			return true;
		}
		return false;
	}

	enum class HangulClass : unsigned char
	{
		None,
		L,
		V,
		T,
		LV,
		LVT
	};

	HangulClass GetHangulClass(std::uint32_t value) noexcept
	{
		if (InRange(value, 0x1100u, 0x115Fu)
			|| InRange(value, 0xA960u, 0xA97Cu))
			return HangulClass::L;
		if (InRange(value, 0x1160u, 0x11A7u)
			|| InRange(value, 0xD7B0u, 0xD7C6u))
			return HangulClass::V;
		if (InRange(value, 0x11A8u, 0x11FFu)
			|| InRange(value, 0xD7CBu, 0xD7FBu))
			return HangulClass::T;
		if (InRange(value, 0xAC00u, 0xD7A3u))
			return ((value - 0xAC00u) % 28u) == 0
				? HangulClass::LV : HangulClass::LVT;
		return HangulClass::None;
	}

	bool IsIndicLinker(std::uint32_t value) noexcept
	{
		switch (value)
		{
		case 0x094D: case 0x09CD: case 0x0A4D: case 0x0ACD:
		case 0x0B4D: case 0x0BCD: case 0x0C4D: case 0x0CCD:
		case 0x0D3B: case 0x0D3C: case 0x0D4D: case 0x0DCA:
		case 0x0E3A: case 0x0F84: case 0x1039: case 0x103A:
		case 0x1714: case 0x1734: case 0x17D2: case 0x1A60:
		case 0x1B44: case 0x1BAA: case 0x1BAB: case 0xA806:
		case 0xA8C4: case 0xA953: case 0xA9C0: case 0xAAF6:
		case 0xABED: case 0x10A3F: case 0x11046: case 0x1107F:
		case 0x110B9: case 0x11133: case 0x11134: case 0x111C0:
		case 0x11235: case 0x112EA: case 0x1134D: case 0x11442:
		case 0x114C2: case 0x115BF: case 0x1163F: case 0x116B6:
		case 0x1172B: case 0x11839: case 0x1193D: case 0x1193E:
		case 0x119E0: case 0x11A34: case 0x11A47: case 0x11A99:
		case 0x11C3F: case 0x11D44: case 0x11D45: case 0x11D97:
		case 0x11F41: case 0x11F42:
			return true;
		default:
			return false;
		}
	}

	bool IsIndicCharacter(std::uint32_t value) noexcept
	{
		return InRange(value, 0x0900u, 0x0DFFu)
			|| InRange(value, 0x0F00u, 0x109Fu)
			|| InRange(value, 0x1700u, 0x1CFFu)
			|| InRange(value, 0xA800u, 0xABFFu)
			|| InRange(value, 0x11000u, 0x11FFFu);
	}

	bool HasIndicConjunctBefore(
		std::wstring_view text,
		std::size_t position,
		std::uint32_t following) noexcept
	{
		if (!IsIndicCharacter(following)) return false;
		bool hasLinker = false;
		std::size_t cursor = position;
		while (cursor > 0)
		{
			const auto previous = DecodeBefore(text, cursor);
			if (IsIndicLinker(previous.Value))
			{
				hasLinker = true;
				cursor = previous.Start;
				continue;
			}
			if (IsExtend(previous.Value) || previous.Value == 0x200Du)
			{
				cursor = previous.Start;
				continue;
			}
			return hasLinker && IsIndicCharacter(previous.Value)
				&& !IsControl(previous.Value);
		}
		return false;
	}
}

namespace CuiTextBoundary
{
	bool IsHighSurrogate(wchar_t value) noexcept
	{
		const auto code = static_cast<std::uint32_t>(value);
		return code >= 0xD800u && code <= 0xDBFFu;
	}

	bool IsLowSurrogate(wchar_t value) noexcept
	{
		const auto code = static_cast<std::uint32_t>(value);
		return code >= 0xDC00u && code <= 0xDFFFu;
	}

	bool IsTextElementBoundary(
		std::wstring_view text,
		std::size_t position,
		bool preserveCrLf) noexcept
	{
		position = (std::min)(position, text.size());
		if (position == 0 || position == text.size()) return true;
		if (IsHighSurrogate(text[position - 1])
			&& IsLowSurrogate(text[position])) return false;

		const auto previous = DecodeBefore(text, position);
		const auto following = DecodeAt(text, position);
		if (preserveCrLf
			&& previous.Value == 0x000Du
			&& following.Value == 0x000Au) return false;
		if (previous.Value == 0x000Du || previous.Value == 0x000Au
			|| following.Value == 0x000Du || following.Value == 0x000Au
			|| IsControl(previous.Value) || IsControl(following.Value))
		{
			return true;
		}

		const auto leftHangul = GetHangulClass(previous.Value);
		const auto rightHangul = GetHangulClass(following.Value);
		if (leftHangul == HangulClass::L
			&& (rightHangul == HangulClass::L
				|| rightHangul == HangulClass::V
				|| rightHangul == HangulClass::LV
				|| rightHangul == HangulClass::LVT)) return false;
		if ((leftHangul == HangulClass::LV
				|| leftHangul == HangulClass::V)
			&& (rightHangul == HangulClass::V
				|| rightHangul == HangulClass::T)) return false;
		if ((leftHangul == HangulClass::LVT
				|| leftHangul == HangulClass::T)
			&& rightHangul == HangulClass::T) return false;

		if (IsExtend(following.Value) || following.Value == 0x200Du)
			return false;
		if (IsPrepend(previous.Value)) return false;
		if (HasIndicConjunctBefore(text, position, following.Value))
			return false;

		if (previous.Value == 0x200Du
			&& IsExtendedPictographic(following.Value))
		{
			std::size_t cursor = previous.Start;
			while (cursor > 0)
			{
				const auto candidate = DecodeBefore(text, cursor);
				if (IsExtend(candidate.Value))
				{
					cursor = candidate.Start;
					continue;
				}
				if (IsExtendedPictographic(candidate.Value)) return false;
				break;
			}
		}

		if (IsRegionalIndicator(previous.Value)
			&& IsRegionalIndicator(following.Value))
		{
			std::size_t count = 0;
			std::size_t cursor = position;
			while (cursor > 0)
			{
				const auto candidate = DecodeBefore(text, cursor);
				if (!IsRegionalIndicator(candidate.Value)) break;
				++count;
				cursor = candidate.Start;
			}
			if ((count & 1u) != 0) return false;
		}
		return true;
	}

	std::size_t GetNextTextElementBoundary(
		std::wstring_view text,
		std::size_t position,
		bool preserveCrLf) noexcept
	{
		position = (std::min)(position, text.size());
		if (position >= text.size()) return text.size();
		std::size_t cursor = DecodeAt(text, position).End;
		while (cursor < text.size()
			&& !IsTextElementBoundary(text, cursor, preserveCrLf))
		{
			const auto next = DecodeAt(text, cursor).End;
			if (next <= cursor) return text.size();
			cursor = next;
		}
		return cursor;
	}

	std::size_t GetPreviousTextElementBoundary(
		std::wstring_view text,
		std::size_t position,
		bool preserveCrLf) noexcept
	{
		position = (std::min)(position, text.size());
		if (position == 0) return 0;
		std::size_t cursor = DecodeBefore(text, position).Start;
		while (cursor > 0
			&& !IsTextElementBoundary(text, cursor, preserveCrLf))
		{
			const auto previous = DecodeBefore(text, cursor).Start;
			if (previous >= cursor) return 0;
			cursor = previous;
		}
		return cursor;
	}

	std::size_t SnapToTextElementBoundary(
		std::wstring_view text,
		std::size_t position,
		bool forward,
		bool preserveCrLf) noexcept
	{
		position = (std::min)(position, text.size());
		if (IsTextElementBoundary(text, position, preserveCrLf))
			return position;
		return forward
			? GetNextTextElementBoundary(text, position, preserveCrLf)
			: GetPreviousTextElementBoundary(text, position, preserveCrLf);
	}

	bool IsWhiteSpaceCodePoint(std::uint32_t value) noexcept
	{
		return QueryCharacterType(value, CT_CTYPE1, C1_SPACE);
	}

	bool IsWordCodePoint(std::uint32_t value) noexcept
	{
		if (value == L'_') return true;
		if (IsExtend(value) || value == 0x200Du) return true;
		return QueryCharacterType(value, CT_CTYPE1,
			C1_ALPHA | C1_DIGIT);
	}

	std::uint32_t GetCodePointAt(
		std::wstring_view text, std::size_t position) noexcept
	{
		return DecodeAt(text, position).Value;
	}
}
