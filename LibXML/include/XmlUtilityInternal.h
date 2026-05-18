#pragma once
// Shared internal utility helpers.

#include "Xml.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <fstream>
#include <iomanip>
#include <limits>
#include <mutex>
#include <optional>
#include <regex>
#include <sstream>
#include <string_view>
#include <tuple>
#include <utility>

#if defined(_M_X64) || defined(_M_IX86)
#include <emmintrin.h>
#include <intrin.h>
#elif defined(_M_ARM64) || defined(__aarch64__)
#include <arm_neon.h>
#endif

namespace System::Xml {

std::string BuildExceptionMessage(const std::string& message, std::size_t line, std::size_t column);
std::string ComposeQualifiedName(std::string_view prefix, std::string_view localName);
std::string ResolveNodeNamespaceUri(const XmlNode& node);
const std::string& EmptyString();
void ValidateXmlDeclarationVersion(std::string_view version);
void ValidateXmlDeclarationEncoding(std::string_view encoding);
void ValidateXmlDeclarationStandalone(std::string_view standalone);
std::string BuildDeclarationValue(
    const std::string& version,
    const std::string& encoding,
    const std::string& standalone);
std::string BuildDeclarationValue(const XmlDeclaration& declaration);

namespace {

constexpr unsigned char kAttributeValueDecoded = 0x01;
constexpr unsigned char kAttributeValueNeedsDecoding = 0x02;

bool IsWhitespace(char ch) {
    static constexpr bool kIsWhitespaceTable[256] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    };
    return kIsWhitespaceTable[static_cast<unsigned char>(ch)];
}

std::string NormalizeXmlAttributeWhitespace(std::string_view value) {
    std::string normalized;
    normalized.reserve(value.size());

    bool pendingSpace = false;
    for (const char ch : value) {
        if (IsWhitespace(ch)) {
            pendingSpace = !normalized.empty();
            continue;
        }

        if (pendingSpace) {
            normalized.push_back(' ');
            pendingSpace = false;
        }
        normalized.push_back(ch);
    }

    return normalized;
}

bool IsNameStartChar(char ch) {
    if (static_cast<unsigned char>(ch) >= 0x80) {
        return true;
    }

    static constexpr bool kIsNameStartCharTable[256] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0,
        0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1,
        0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    };
    return kIsNameStartCharTable[static_cast<unsigned char>(ch)];
}

bool IsNameChar(char ch) {
    if (static_cast<unsigned char>(ch) >= 0x80) {
        return true;
    }

    static constexpr bool kIsNameCharTable[256] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
        0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1,
        0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    };
    return kIsNameCharTable[static_cast<unsigned char>(ch)];
}

bool IsXmlNameStartCodePoint(std::uint32_t codePoint) noexcept {
    return codePoint == ':'
        || (codePoint >= 'A' && codePoint <= 'Z')
        || codePoint == '_'
        || (codePoint >= 'a' && codePoint <= 'z')
        || (codePoint >= 0xC0 && codePoint <= 0xD6)
        || (codePoint >= 0xD8 && codePoint <= 0xF6)
        || (codePoint >= 0xF8 && codePoint <= 0x2FF)
        || (codePoint >= 0x370 && codePoint <= 0x37D)
        || (codePoint >= 0x37F && codePoint <= 0x1FFF)
        || (codePoint >= 0x200C && codePoint <= 0x200D)
        || (codePoint >= 0x2070 && codePoint <= 0x218F)
        || (codePoint >= 0x2C00 && codePoint <= 0x2FEF)
        || (codePoint >= 0x3001 && codePoint <= 0xD7FF)
        || (codePoint >= 0xF900 && codePoint <= 0xFDCF)
        || (codePoint >= 0xFDF0 && codePoint <= 0xFFFD)
        || (codePoint >= 0x10000 && codePoint <= 0xEFFFF);
}

bool IsLegacyXmlPiTargetStartCodePoint(std::uint32_t codePoint) noexcept {
    if (!IsXmlNameStartCodePoint(codePoint)) {
        return false;
    }

    static constexpr std::uint32_t kRejectedCodePoints[] = {
        0x00D7, 0x00F7, 0x0132, 0x0133, 0x013F, 0x0140, 0x0149, 0x017F,
        0x01C4, 0x01CC, 0x01F1, 0x01F3, 0x01F6, 0x01F9, 0x0230, 0x02AF,
        0x02CF, 0x0387, 0x038B, 0x03A2, 0x03CF, 0x03D7, 0x03DD, 0x03E1,
        0x03F4, 0x040D, 0x0450, 0x045D, 0x0482, 0x04C5, 0x04C6, 0x04C9,
        0x04EC, 0x04ED, 0x04F6, 0x04FA, 0x0557, 0x0558, 0x0587, 0x05EB,
        0x05F3, 0x0620, 0x063B, 0x064B, 0x06B8, 0x06BF, 0x06CF, 0x06D4,
        0x06D6, 0x06E7, 0x093A, 0x093E, 0x0962, 0x098D, 0x0991, 0x0992,
        0x09A9, 0x09B1, 0x09B5, 0x09BA, 0x09DE, 0x09E2, 0x09F2, 0x0A0B,
        0x0A11, 0x0A29, 0x0A31, 0x0A34, 0x0A37, 0x0A3A, 0x0A5D, 0x0A70,
        0x0A75, 0x0A84, 0x0A92, 0x0AA9, 0x0AB1, 0x0AB4, 0x0ABA, 0x0ABC,
        0x0B04, 0x0B0D, 0x0B11, 0x0B29, 0x0B31, 0x0B34, 0x0B3A, 0x0B3E,
        0x0B5E, 0x0B62, 0x0B8B, 0x0B91, 0x0B98, 0x0B9B, 0x0B9D, 0x0BA0,
        0x0BA7, 0x0BAB, 0x0BB6, 0x0BBA, 0x0C0D, 0x0C11, 0x0C29, 0x0C34,
        0x0C5F, 0x0C62, 0x0C8D, 0x0C91, 0x0CA9, 0x0CB4, 0x0CBA, 0x0CDF,
        0x0CE2, 0x0D0D, 0x0D11, 0x0D29, 0x0D3A, 0x0D62, 0x0E2F, 0x0E31,
        0x0E34, 0x0E46, 0x0E83, 0x0E85, 0x0E89, 0x0E8B, 0x0E8E, 0x0E98,
        0x0EA0, 0x0EA4, 0x0EA6, 0x0EA8, 0x0EAC, 0x0EAF, 0x0EB1, 0x0EB4,
        0x0EBE, 0x0EC5, 0x0F48, 0x0F6A, 0x10C6, 0x10F7, 0x1101, 0x1104,
        0x1108, 0x110A, 0x110D, 0x113B, 0x113F, 0x1141, 0x114D, 0x114F,
        0x1151, 0x1156, 0x115A, 0x1162, 0x1164, 0x1166, 0x116B, 0x116F,
        0x1174, 0x119F, 0x11AC, 0x11B6, 0x11B9, 0x11BB, 0x11C3, 0x11F1,
        0x11FA, 0x1E9C, 0x1EFA, 0x1F16, 0x1F1E, 0x1F46, 0x1F4F, 0x1F58,
        0x1F5A, 0x1F5C, 0x1F5E, 0x1F7E, 0x1FB5, 0x1FBD, 0x1FBF, 0x1FC5,
        0x1FCD, 0x1FD5, 0x1FDC, 0x1FED, 0x1FF5, 0x1FFD, 0x2127, 0x212F,
        0x2183, 0x3008, 0x302A, 0x3095, 0x30FB, 0x312D, 0x4CFF, 0x9FA6,
        0xD7A4,
    };

    return std::find(
        kRejectedCodePoints,
        kRejectedCodePoints + (sizeof(kRejectedCodePoints) / sizeof(kRejectedCodePoints[0])),
        codePoint) == (kRejectedCodePoints + (sizeof(kRejectedCodePoints) / sizeof(kRejectedCodePoints[0])));
}

bool IsLegacyXmlPiTargetNameCodePoint(std::uint32_t codePoint) noexcept {
    if (!(IsXmlNameStartCodePoint(codePoint)
        || codePoint == '-'
        || codePoint == '.'
        || (codePoint >= '0' && codePoint <= '9')
        || codePoint == 0xB7
        || (codePoint >= 0x0300 && codePoint <= 0x036F)
        || (codePoint >= 0x203F && codePoint <= 0x2040))) {
        return false;
    }

    static constexpr std::uint32_t kRejectedCodePoints[] = {
        0x02FF, 0x0346, 0x0362, 0x0487, 0x05A2, 0x05BA, 0x05BE, 0x05C0,
        0x05C3, 0x0653, 0x06B8, 0x06B9, 0x06E9, 0x06EE, 0x0904, 0x093B,
        0x094E, 0x0955, 0x0964, 0x0984, 0x09C5, 0x09C9, 0x09CE, 0x09D8,
        0x09E4, 0x0A03, 0x0A3D, 0x0A46, 0x0A49, 0x0A4E, 0x0A80, 0x0A84,
        0x0ABB, 0x0AC6, 0x0ACA, 0x0ACE, 0x0B04, 0x0B3B, 0x0B44, 0x0B4A,
        0x0B4E, 0x0B58, 0x0B84, 0x0BC3, 0x0BC9, 0x0BD6, 0x0C0D, 0x0C45,
        0x0C49, 0x0C54, 0x0C81, 0x0C84, 0x0CC5, 0x0CC9, 0x0CD4, 0x0CD7,
        0x0D04, 0x0D45, 0x0D49, 0x0D4E, 0x0D58, 0x0E3B, 0x0E3F, 0x0E4F,
        0x0EBA, 0x0EBE, 0x0ECE, 0x0F1A, 0x0F36, 0x0F38, 0x0F3A, 0x0F3B,
        0x0F70, 0x0F85, 0x0F8C, 0x0F96, 0x0F98, 0x0FB0, 0x0FB8, 0x0FBA,
        0x066A, 0x06FA, 0x0970, 0x09F2, 0x0AF0, 0x0B70, 0x0C65, 0x0CE5,
        0x0CF0, 0x0D70, 0x0E5A, 0x0EDA, 0x0F2A, 0x02D2, 0x03FE, 0x065F,
        0x0EC7, 0x20DD, 0x20E2, 0x3006, 0x3030, 0x3036, 0x309B, 0x309C,
        0x309F, 0x30FF, 0x3008, 0x4CFF, 0x9FA6,
    };

    return std::find(
        kRejectedCodePoints,
        kRejectedCodePoints + (sizeof(kRejectedCodePoints) / sizeof(kRejectedCodePoints[0])),
        codePoint) == (kRejectedCodePoints + (sizeof(kRejectedCodePoints) / sizeof(kRejectedCodePoints[0])));
}

bool IsXmlNameCodePoint(std::uint32_t codePoint) noexcept {
    return IsXmlNameStartCodePoint(codePoint)
        || codePoint == '-'
        || codePoint == '.'
        || (codePoint >= '0' && codePoint <= '9')
        || codePoint == 0xB7
        || (codePoint >= 0x0300 && codePoint <= 0x036F)
        || (codePoint >= 0x203F && codePoint <= 0x2040);
}

bool IsValidXmlCharacterCodePoint(std::uint32_t codePoint) noexcept {
    return codePoint == 0x09
        || codePoint == 0x0A
        || codePoint == 0x0D
        || (codePoint >= 0x20 && codePoint <= 0xD7FF)
        || (codePoint >= 0xE000 && codePoint <= 0xFFFD)
        || (codePoint >= 0x10000 && codePoint <= 0x10FFFF);
}

template <typename CharAtFn>
bool DecodeUtf8CodePointAt(std::size_t position, CharAtFn&& charAt, std::uint32_t& codePoint, std::size_t& width) noexcept {
    const auto first = static_cast<unsigned char>(charAt(position));
    if (first == 0) {
        return false;
    }

    if (first < 0x80) {
        codePoint = first;
        width = 1;
        return true;
    }

    std::uint32_t value = 0;
    std::uint32_t minValue = 0;
    if ((first & 0xE0) == 0xC0) {
        value = first & 0x1Fu;
        width = 2;
        minValue = 0x80;
    } else if ((first & 0xF0) == 0xE0) {
        value = first & 0x0Fu;
        width = 3;
        minValue = 0x800;
    } else if ((first & 0xF8) == 0xF0) {
        value = first & 0x07u;
        width = 4;
        minValue = 0x10000;
    } else {
        return false;
    }

    for (std::size_t index = 1; index < width; ++index) {
        const auto continuation = static_cast<unsigned char>(charAt(position + index));
        if (continuation == 0 || (continuation & 0xC0) != 0x80) {
            return false;
        }
        value = (value << 6) | static_cast<std::uint32_t>(continuation & 0x3Fu);
    }

    if (value < minValue || value > 0x10FFFF || (value >= 0xD800 && value <= 0xDFFF)) {
        return false;
    }

    codePoint = value;
    return true;
}

template <typename CharAtFn>
std::size_t ConsumeXmlNameAt(std::size_t position, CharAtFn&& charAt, bool ncName = false) noexcept {
    std::uint32_t codePoint = 0;
    std::size_t width = 0;
    if (!DecodeUtf8CodePointAt(position, charAt, codePoint, width)) {
        return position;
    }

    if (!IsXmlNameStartCodePoint(codePoint) || (ncName && codePoint == ':')) {
        return position;
    }

    position += width;
    while (DecodeUtf8CodePointAt(position, charAt, codePoint, width)) {
        if (!IsXmlNameCodePoint(codePoint) || (ncName && codePoint == ':')) {
            break;
        }
        position += width;
    }

    return position;
}

inline bool IsLegacyXmlProcessingInstructionTarget(std::string_view target) noexcept {
    std::uint32_t codePoint = 0;
    std::size_t width = 0;
    std::size_t position = 0;
    if (!DecodeUtf8CodePointAt(0, [&target](std::size_t index) noexcept {
        return index < target.size() ? target[index] : '\0';
    }, codePoint, width)
        || !IsLegacyXmlPiTargetStartCodePoint(codePoint)) {
        return false;
    }

    position += width;
    while (DecodeUtf8CodePointAt(position, [&target](std::size_t index) noexcept {
        return index < target.size() ? target[index] : '\0';
    }, codePoint, width)) {
        if (!IsLegacyXmlPiTargetNameCodePoint(codePoint)) {
            return false;
        }
        position += width;
    }

    return position == target.size();
}

template <typename CharAtFn>
std::size_t ConsumeXmlNmTokenAt(std::size_t position, CharAtFn&& charAt, bool ncName = false) noexcept {
    std::uint32_t codePoint = 0;
    std::size_t width = 0;
    std::size_t current = position;
    while (DecodeUtf8CodePointAt(current, charAt, codePoint, width)) {
        if (!IsXmlNameCodePoint(codePoint) || (ncName && codePoint == ':')) {
            break;
        }
        current += width;
    }
    return current;
}

std::size_t ConsumeNameCharsInBuffer(const char* data, std::size_t length) noexcept {
    if (data == nullptr || length == 0) {
        return 0;
    }

#if defined(_M_X64) || defined(_M_IX86)
    const __m128i colonVector = _mm_set1_epi8(':');
    const __m128i underscoreVector = _mm_set1_epi8('_');
    const __m128i minusVector = _mm_set1_epi8('-');
    const __m128i dotVector = _mm_set1_epi8('.');
    const __m128i biasVector = _mm_set1_epi8(static_cast<char>(0x80));
    const __m128i digitLow = _mm_set1_epi8(static_cast<char>(('0' - 1) ^ 0x80));
    const __m128i digitHigh = _mm_set1_epi8(static_cast<char>(('9' + 1) ^ 0x80));
    const __m128i upperLow = _mm_set1_epi8(static_cast<char>(('A' - 1) ^ 0x80));
    const __m128i upperHigh = _mm_set1_epi8(static_cast<char>(('Z' + 1) ^ 0x80));
    const __m128i lowerLow = _mm_set1_epi8(static_cast<char>(('a' - 1) ^ 0x80));
    const __m128i lowerHigh = _mm_set1_epi8(static_cast<char>(('z' + 1) ^ 0x80));

    std::size_t offset = 0;
    while (offset + 16 <= length) {
        const __m128i block = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + offset));
        const __m128i biasedBlock = _mm_xor_si128(block, biasVector);

        const __m128i isColon = _mm_cmpeq_epi8(block, colonVector);
        const __m128i isUnderscore = _mm_cmpeq_epi8(block, underscoreVector);
        const __m128i isMinus = _mm_cmpeq_epi8(block, minusVector);
        const __m128i isDot = _mm_cmpeq_epi8(block, dotVector);
        const __m128i isDigit = _mm_and_si128(_mm_cmpgt_epi8(biasedBlock, digitLow), _mm_cmpgt_epi8(digitHigh, biasedBlock));
        const __m128i isUpper = _mm_and_si128(_mm_cmpgt_epi8(biasedBlock, upperLow), _mm_cmpgt_epi8(upperHigh, biasedBlock));
        const __m128i isLower = _mm_and_si128(_mm_cmpgt_epi8(biasedBlock, lowerLow), _mm_cmpgt_epi8(lowerHigh, biasedBlock));

        __m128i valid = _mm_or_si128(isColon, isUnderscore);
        valid = _mm_or_si128(valid, isMinus);
        valid = _mm_or_si128(valid, isDot);
        valid = _mm_or_si128(valid, isDigit);
        valid = _mm_or_si128(valid, isUpper);
        valid = _mm_or_si128(valid, isLower);

        const unsigned int mask = static_cast<unsigned int>(_mm_movemask_epi8(valid));
        if (mask != 0xFFFFu) {
            const unsigned int invalidMask = (~mask) & 0xFFFFu;
            unsigned long firstInvalid = 0;
            _BitScanForward(&firstInvalid, invalidMask);
            return offset + static_cast<std::size_t>(firstInvalid);
        }

        offset += 16;
    }

    for (; offset < length; ++offset) {
        if (!IsNameChar(data[offset])) {
            break;
        }
    }
    return offset;
#elif defined(_M_ARM64) || defined(__aarch64__)
    const uint8x16_t colonVector = vdupq_n_u8(static_cast<std::uint8_t>(':'));
    const uint8x16_t underscoreVector = vdupq_n_u8(static_cast<std::uint8_t>('_'));
    const uint8x16_t minusVector = vdupq_n_u8(static_cast<std::uint8_t>('-'));
    const uint8x16_t dotVector = vdupq_n_u8(static_cast<std::uint8_t>('.'));
    const uint8x16_t digitLow = vdupq_n_u8(static_cast<std::uint8_t>('0'));
    const uint8x16_t digitHigh = vdupq_n_u8(static_cast<std::uint8_t>('9'));
    const uint8x16_t upperLow = vdupq_n_u8(static_cast<std::uint8_t>('A'));
    const uint8x16_t upperHigh = vdupq_n_u8(static_cast<std::uint8_t>('Z'));
    const uint8x16_t lowerLow = vdupq_n_u8(static_cast<std::uint8_t>('a'));
    const uint8x16_t lowerHigh = vdupq_n_u8(static_cast<std::uint8_t>('z'));

    std::size_t offset = 0;
    while (offset + 16 <= length) {
        const uint8x16_t block = vld1q_u8(reinterpret_cast<const std::uint8_t*>(data + offset));

        uint8x16_t valid = vorrq_u8(vceqq_u8(block, colonVector), vceqq_u8(block, underscoreVector));
        valid = vorrq_u8(valid, vceqq_u8(block, minusVector));
        valid = vorrq_u8(valid, vceqq_u8(block, dotVector));
        valid = vorrq_u8(valid, vandq_u8(vcgeq_u8(block, digitLow), vcleq_u8(block, digitHigh)));
        valid = vorrq_u8(valid, vandq_u8(vcgeq_u8(block, upperLow), vcleq_u8(block, upperHigh)));
        valid = vorrq_u8(valid, vandq_u8(vcgeq_u8(block, lowerLow), vcleq_u8(block, lowerHigh)));

        alignas(16) std::uint8_t maskBytes[16];
        vst1q_u8(maskBytes, valid);
        for (std::size_t index = 0; index < 16; ++index) {
            if (maskBytes[index] == 0) {
                return offset + index;
            }
        }

        offset += 16;
    }

    for (; offset < length; ++offset) {
        if (!IsNameChar(data[offset])) {
            break;
        }
    }
    return offset;
#else
    std::size_t offset = 0;
    while (offset < length && IsNameChar(data[offset])) {
        ++offset;
    }
    return offset;
#endif
}

bool IsTextLike(XmlNodeType nodeType) {
    return nodeType == XmlNodeType::Text
    || nodeType == XmlNodeType::EntityReference
        || nodeType == XmlNodeType::CDATA
        || nodeType == XmlNodeType::Whitespace
        || nodeType == XmlNodeType::SignificantWhitespace;
}

const char* NodeTypeDisplayName(XmlNodeType nodeType) {
    switch (nodeType) {
    case XmlNodeType::None: return "None";
    case XmlNodeType::Element: return "Element";
    case XmlNodeType::Attribute: return "Attribute";
    case XmlNodeType::Text: return "Text";
    case XmlNodeType::CDATA: return "CDATA";
    case XmlNodeType::EntityReference: return "EntityReference";
    case XmlNodeType::Entity: return "Entity";
    case XmlNodeType::ProcessingInstruction: return "ProcessingInstruction";
    case XmlNodeType::Comment: return "Comment";
    case XmlNodeType::Document: return "Document";
    case XmlNodeType::DocumentType: return "DocumentType";
    case XmlNodeType::DocumentFragment: return "DocumentFragment";
    case XmlNodeType::Notation: return "Notation";
    case XmlNodeType::Whitespace: return "Whitespace";
    case XmlNodeType::SignificantWhitespace: return "SignificantWhitespace";
    case XmlNodeType::EndElement: return "EndElement";
    case XmlNodeType::EndEntity: return "EndEntity";
    case XmlNodeType::XmlDeclaration: return "XmlDeclaration";
    }

    return "Unknown";
}

std::string NormalizeNewLines(std::string_view value, const std::string& replacement) {
    std::string normalized;
    normalized.reserve(value.size());

    for (std::size_t index = 0; index < value.size(); ++index) {
        if (value[index] == '\r') {
            if (index + 1 < value.size() && value[index + 1] == '\n') {
                ++index;
            }
            normalized += replacement;
        } else if (value[index] == '\n') {
            normalized += replacement;
        } else {
            normalized.push_back(value[index]);
        }
    }

    return normalized;
}

std::string Trim(const std::string& value) {
    const auto start = std::find_if_not(value.begin(), value.end(), IsWhitespace);
    if (start == value.end()) {
        return {};
    }

    const auto end = std::find_if_not(value.rbegin(), value.rend(), IsWhitespace).base();
    return std::string(start, end);
}

std::pair<std::string, std::string> SplitQualifiedName(const std::string& name) {
    const auto separator = name.find(':');
    if (separator == std::string::npos) {
        return {{}, name};
    }

    return {name.substr(0, separator), name.substr(separator + 1)};
}

std::pair<std::string_view, std::string_view> SplitQualifiedNameView(std::string_view name) noexcept {
    const auto separator = name.find(':');
    if (separator == std::string_view::npos) {
        return {{}, name};
    }

    return {name.substr(0, separator), name.substr(separator + 1)};
}

bool IsValidXmlNameToken(std::string_view name) {
    if (name.empty()) {
        return false;
    }

    const auto end = ConsumeXmlNameAt(0, [name](std::size_t index) noexcept {
        return index < name.size() ? name[index] : '\0';
    });
    return end == name.size();
}

bool IsValidXmlQualifiedName(std::string_view name) {
    if (name.empty()) {
        return false;
    }

    const auto separator = name.find(':');
    if (separator == std::string_view::npos) {
        return IsValidXmlNameToken(name);
    }

    if (name.find(':', separator + 1) != std::string_view::npos) {
        return false;
    }

    return IsValidXmlNameToken(name.substr(0, separator))
        && IsValidXmlNameToken(name.substr(separator + 1));
}

inline bool IsNamespaceAwareProcessingInstructionTarget(std::string_view target) noexcept {
    return IsValidXmlNameToken(target)
        && target.find(':') == std::string_view::npos;
}

inline bool IsNamespaceAwareDtdDeclarationName(std::string_view name) {
    return IsValidXmlNameToken(name)
        && name.find(':') == std::string_view::npos;
}

bool IsNamespaceDeclarationName(const std::string& name) {
    return name == "xmlns" || name.rfind("xmlns:", 0) == 0;
}

bool IsNamespaceDeclarationName(std::string_view name) noexcept {
    return name == "xmlns" || name.rfind("xmlns:", 0) == 0;
}

std::string NamespaceDeclarationPrefix(const std::string& name) {
    if (name == "xmlns") {
        return {};
    }
    if (name.rfind("xmlns:", 0) == 0) {
        return name.substr(6);
    }
    return {};
}

std::string_view NamespaceDeclarationPrefixView(std::string_view name) noexcept {
    if (name == "xmlns") {
        return {};
    }
    if (name.rfind("xmlns:", 0) == 0) {
        return name.substr(6);
    }
    return {};
}

bool IsAbsoluteNamespaceUri(std::string_view namespaceUri) {
    if (namespaceUri.empty()) {
        return false;
    }

    const std::size_t colon = namespaceUri.find(':');
    if (colon == std::string_view::npos) {
        return false;
    }

    const std::size_t firstDelimiter = namespaceUri.find_first_of("/?#");
    return firstDelimiter == std::string_view::npos || colon < firstDelimiter;
}

void ValidateNamespaceDeclarationBinding(std::string_view prefix, std::string_view namespaceUri) {
    constexpr std::string_view kXmlNamespaceUri = "http://www.w3.org/XML/1998/namespace";
    constexpr std::string_view kXmlnsNamespaceUri = "http://www.w3.org/2000/xmlns/";

    if (prefix == "xmlns") {
        throw XmlException("Invalid namespace declaration for reserved prefix 'xmlns'");
    }
    if (prefix == "xml") {
        if (namespaceUri != kXmlNamespaceUri) {
            throw XmlException("Invalid namespace declaration for reserved prefix 'xml'");
        }
        return;
    }
    if (!prefix.empty() && namespaceUri.empty()) {
        throw XmlException("Namespace prefixes cannot be undeclared in XML 1.0");
    }
    if (!namespaceUri.empty() && !IsAbsoluteNamespaceUri(namespaceUri)) {
        throw XmlException("Relative namespace URI declarations are not supported");
    }
    if (namespaceUri == kXmlnsNamespaceUri) {
        throw XmlException("The namespace URI 'http://www.w3.org/2000/xmlns/' cannot be declared");
    }
    if (namespaceUri == kXmlNamespaceUri) {
        throw XmlException("The namespace URI 'http://www.w3.org/XML/1998/namespace' must be bound to the prefix 'xml'");
    }
}

std::string LookupPrefixOnElement(const XmlElement* element, std::string_view namespaceUri) {
    for (auto current = element; current != nullptr; ) {
        const std::string prefix = current->FindNamespaceDeclarationPrefix(namespaceUri);
        if (!prefix.empty()) {
            return prefix;
        }
        const XmlNode* parent = current->ParentNode();
        current = parent != nullptr && parent->NodeType() == XmlNodeType::Element
            ? static_cast<const XmlElement*>(parent)
            : nullptr;
    }
    if (namespaceUri == "http://www.w3.org/XML/1998/namespace") return "xml";
    if (namespaceUri == "http://www.w3.org/2000/xmlns/") return "xmlns";
    return {};
}

const char kBase64Chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string EncodeBase64(const unsigned char* data, std::size_t length) {
    std::string result;
    result.reserve(((length + 2) / 3) * 4);
    for (std::size_t i = 0; i < length; i += 3) {
        unsigned int triplet = static_cast<unsigned int>(data[i]) << 16;
        if (i + 1 < length) triplet |= static_cast<unsigned int>(data[i + 1]) << 8;
        if (i + 2 < length) triplet |= static_cast<unsigned int>(data[i + 2]);
        result.push_back(kBase64Chars[(triplet >> 18) & 0x3F]);
        result.push_back(kBase64Chars[(triplet >> 12) & 0x3F]);
        result.push_back(i + 1 < length ? kBase64Chars[(triplet >> 6) & 0x3F] : '=');
        result.push_back(i + 2 < length ? kBase64Chars[triplet & 0x3F] : '=');
    }
    return result;
}

int DecodeBase64Char(char ch) {
    if (ch >= 'A' && ch <= 'Z') return ch - 'A';
    if (ch >= 'a' && ch <= 'z') return ch - 'a' + 26;
    if (ch >= '0' && ch <= '9') return ch - '0' + 52;
    if (ch == '+') return 62;
    if (ch == '/') return 63;
    return -1;
}

std::vector<unsigned char> DecodeBase64(const std::string& input) {
    std::vector<unsigned char> result;
    result.reserve((input.size() / 4) * 3);
    unsigned int accumulator = 0;
    int bits = 0;
    for (char ch : input) {
        if (IsWhitespace(ch) || ch == '=') continue;
        int value = DecodeBase64Char(ch);
        if (value < 0) continue;
        accumulator = (accumulator << 6) | static_cast<unsigned int>(value);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            result.push_back(static_cast<unsigned char>((accumulator >> bits) & 0xFF));
        }
    }
    return result;
}

void AppendDecodedBase64Chunk(std::string_view input, std::vector<unsigned char>& buffer, unsigned int& accumulator, int& bits) {
    for (char ch : input) {
        if (IsWhitespace(ch) || ch == '=') {
            continue;
        }

        const int value = DecodeBase64Char(ch);
        if (value < 0) {
            continue;
        }

        accumulator = (accumulator << 6) | static_cast<unsigned int>(value);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            buffer.push_back(static_cast<unsigned char>((accumulator >> bits) & 0xFF));
        }
    }
}

}  // namespace

}  // namespace System::Xml
