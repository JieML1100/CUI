#pragma once
// XmlReader and DTD parsing internal helpers.

#include "XmlDomInternal.h"
#include "XmlConvert.h"

namespace System::Xml {

namespace {

enum class XmlMarkupKind {
    None = 0,
    Element,
    EndTag,
    Comment,
    CData,
    DocumentType,
    XmlDeclaration,
    ProcessingInstruction,
    UnsupportedDeclaration
};

XmlMarkupKind ClassifyXmlMarkupPrefix(const char* prefix, std::size_t available) noexcept {
    if (prefix == nullptr || available == 0 || prefix[0] != '<') {
        return XmlMarkupKind::None;
    }

    if (available < 2) {
        return XmlMarkupKind::None;
    }

    const char ch1 = prefix[1];
    if (ch1 == '?') {
        if (available >= 3 && prefix[2] != 'x') {
            return XmlMarkupKind::ProcessingInstruction;
        }

        if (available >= 4 && prefix[2] == 'x' && prefix[3] != 'm') {
            return XmlMarkupKind::ProcessingInstruction;
        }

        if (available < 5) {
            return XmlMarkupKind::None;
        }

        if (prefix[2] == 'x' && prefix[3] == 'm' && prefix[4] == 'l') {
            const char next = available >= 6 ? prefix[5] : '\0';
            if (next == '\0') {
                return XmlMarkupKind::None;
            }
            if (IsWhitespace(next) || next == '?') {
                return XmlMarkupKind::XmlDeclaration;
            }
            return XmlMarkupKind::ProcessingInstruction;
        }

        return XmlMarkupKind::ProcessingInstruction;
    }

    if (ch1 == '!') {
        if (available < 3) {
            return XmlMarkupKind::None;
        }

        const char ch2 = prefix[2];
        if (ch2 == '-') {
            if (available < 4) {
                return XmlMarkupKind::None;
            }
            return prefix[3] == '-' ? XmlMarkupKind::Comment : XmlMarkupKind::UnsupportedDeclaration;
        }

        if (ch2 == '[') {
            if (available < 9) {
                return XmlMarkupKind::None;
            }
            return prefix[3] == 'C' && prefix[4] == 'D' && prefix[5] == 'A'
                && prefix[6] == 'T' && prefix[7] == 'A' && prefix[8] == '['
                ? XmlMarkupKind::CData
                : XmlMarkupKind::UnsupportedDeclaration;
        }

        if (ch2 == 'D') {
            if (available < 9) {
                return XmlMarkupKind::None;
            }
            return prefix[3] == 'O' && prefix[4] == 'C' && prefix[5] == 'T'
                && prefix[6] == 'Y' && prefix[7] == 'P' && prefix[8] == 'E'
                ? XmlMarkupKind::DocumentType
                : XmlMarkupKind::UnsupportedDeclaration;
        }

        return XmlMarkupKind::UnsupportedDeclaration;
    }

    if (ch1 == '/') {
        return XmlMarkupKind::EndTag;
    }

    if (ch1 == '\0') {
        return XmlMarkupKind::None;
    }

    return XmlMarkupKind::Element;
}

template <typename CharAtFn>
XmlMarkupKind ClassifyXmlMarkupWithCharAt(std::size_t position, CharAtFn&& charAt) noexcept {
    char prefix[9] = {};
    for (std::size_t offset = 0; offset < sizeof(prefix); ++offset) {
        prefix[offset] = charAt(position + offset);
    }
    return ClassifyXmlMarkupPrefix(prefix, sizeof(prefix));
}

XmlMarkupKind ClassifyXmlMarkupAt(const char* p, std::size_t available) noexcept
{
    return ClassifyXmlMarkupPrefix(p, available);
}

template <typename CharAtFn>
std::size_t ScanQuotedValueEndAt(std::size_t quoteStart, CharAtFn&& charAt) noexcept {
    const char quote = charAt(quoteStart);
    if (quote != '"' && quote != '\'') {
        return std::string::npos;
    }

    std::size_t position = quoteStart + 1;
    while (charAt(position) != '\0' && charAt(position) != quote) {
        ++position;
    }

    return charAt(position) == quote ? position : std::string::npos;
}

std::size_t FindQuoteOrNulInBuffer(const char* data, std::size_t length, char quote) noexcept {
    if (data == nullptr || length == 0) {
        return std::string::npos;
    }

#if defined(_M_X64) || defined(_M_IX86)
    const __m128i quoteVector = _mm_set1_epi8(quote);
    const __m128i nulVector = _mm_setzero_si128();

    std::size_t offset = 0;
    while (offset + 16 <= length) {
        const __m128i block = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + offset));
        const __m128i quoteMatches = _mm_cmpeq_epi8(block, quoteVector);
        const __m128i nulMatches = _mm_cmpeq_epi8(block, nulVector);
        const unsigned int mask = static_cast<unsigned int>(_mm_movemask_epi8(_mm_or_si128(quoteMatches, nulMatches)));
        if (mask != 0) {
            unsigned long firstMatch = 0;
            _BitScanForward(&firstMatch, mask);
            return offset + static_cast<std::size_t>(firstMatch);
        }
        offset += 16;
    }

    for (; offset < length; ++offset) {
        if (data[offset] == quote || data[offset] == '\0') {
            return offset;
        }
    }
    return std::string::npos;
#elif defined(_M_ARM64) || defined(__aarch64__)
    const uint8x16_t quoteVector = vdupq_n_u8(static_cast<std::uint8_t>(quote));
    const uint8x16_t nulVector = vdupq_n_u8(0);

    std::size_t offset = 0;
    while (offset + 16 <= length) {
        const uint8x16_t block = vld1q_u8(reinterpret_cast<const std::uint8_t*>(data + offset));
        const uint8x16_t quoteMatches = vceqq_u8(block, quoteVector);
        const uint8x16_t nulMatches = vceqq_u8(block, nulVector);
        const uint8x16_t anyMatches = vorrq_u8(quoteMatches, nulMatches);

        alignas(16) std::uint8_t maskBytes[16];
        vst1q_u8(maskBytes, anyMatches);
        for (std::size_t index = 0; index < 16; ++index) {
            if (maskBytes[index] != 0) {
                return offset + index;
            }
        }
        offset += 16;
    }

    for (; offset < length; ++offset) {
        if (data[offset] == quote || data[offset] == '\0') {
            return offset;
        }
    }
    return std::string::npos;
#else
    for (std::size_t offset = 0; offset < length; ++offset) {
        if (data[offset] == quote || data[offset] == '\0') {
            return offset;
        }
    }
    return std::string::npos;
#endif
}

std::size_t FindTextSpecialInBuffer(const char* data, std::size_t length) noexcept {
    if (data == nullptr || length == 0) {
        return std::string::npos;
    }

#if defined(_M_X64) || defined(_M_IX86)
    const __m128i lessThanVector = _mm_set1_epi8('<');
    const __m128i ampersandVector = _mm_set1_epi8('&');

    std::size_t offset = 0;
    while (offset + 16 <= length) {
        const __m128i block = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + offset));
        const __m128i lessThanMatches = _mm_cmpeq_epi8(block, lessThanVector);
        const __m128i ampersandMatches = _mm_cmpeq_epi8(block, ampersandVector);
        const unsigned int mask = static_cast<unsigned int>(_mm_movemask_epi8(_mm_or_si128(lessThanMatches, ampersandMatches)));
        if (mask != 0) {
            unsigned long firstMatch = 0;
            _BitScanForward(&firstMatch, mask);
            return offset + static_cast<std::size_t>(firstMatch);
        }
        offset += 16;
    }

    for (; offset < length; ++offset) {
        if (data[offset] == '<' || data[offset] == '&') {
            return offset;
        }
    }
    return std::string::npos;
#elif defined(_M_ARM64) || defined(__aarch64__)
    const uint8x16_t lessThanVector = vdupq_n_u8(static_cast<std::uint8_t>('<'));
    const uint8x16_t ampersandVector = vdupq_n_u8(static_cast<std::uint8_t>('&'));

    std::size_t offset = 0;
    while (offset + 16 <= length) {
        const uint8x16_t block = vld1q_u8(reinterpret_cast<const std::uint8_t*>(data + offset));
        const uint8x16_t lessThanMatches = vceqq_u8(block, lessThanVector);
        const uint8x16_t ampersandMatches = vceqq_u8(block, ampersandVector);
        const uint8x16_t anyMatches = vorrq_u8(lessThanMatches, ampersandMatches);

        alignas(16) std::uint8_t maskBytes[16];
        vst1q_u8(maskBytes, anyMatches);
        for (std::size_t index = 0; index < 16; ++index) {
            if (maskBytes[index] != 0) {
                return offset + index;
            }
        }
        offset += 16;
    }

    for (; offset < length; ++offset) {
        if (data[offset] == '<' || data[offset] == '&') {
            return offset;
        }
    }
    return std::string::npos;
#else
    for (std::size_t offset = 0; offset < length; ++offset) {
        if (data[offset] == '<' || data[offset] == '&') {
            return offset;
        }
    }
    return std::string::npos;
#endif
}

std::size_t FindTextLineSpecialInBuffer(const char* data, std::size_t length) noexcept {
    if (data == nullptr || length == 0) {
        return std::string::npos;
    }

#if defined(_M_X64) || defined(_M_IX86)
    const __m128i newlineVector = _mm_set1_epi8('\n');
    const __m128i lessThanVector = _mm_set1_epi8('<');
    const __m128i ampersandVector = _mm_set1_epi8('&');

    std::size_t offset = 0;
    while (offset + 16 <= length) {
        const __m128i block = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + offset));
        const __m128i newlineMatches = _mm_cmpeq_epi8(block, newlineVector);
        const __m128i lessThanMatches = _mm_cmpeq_epi8(block, lessThanVector);
        const __m128i ampersandMatches = _mm_cmpeq_epi8(block, ampersandVector);
        const __m128i anyMatches = _mm_or_si128(newlineMatches, _mm_or_si128(lessThanMatches, ampersandMatches));
        const unsigned int mask = static_cast<unsigned int>(_mm_movemask_epi8(anyMatches));
        if (mask != 0) {
            unsigned long firstMatch = 0;
            _BitScanForward(&firstMatch, mask);
            return offset + static_cast<std::size_t>(firstMatch);
        }
        offset += 16;
    }

    for (; offset < length; ++offset) {
        if (data[offset] == '\n' || data[offset] == '<' || data[offset] == '&') {
            return offset;
        }
    }
    return std::string::npos;
#elif defined(_M_ARM64) || defined(__aarch64__)
    const uint8x16_t newlineVector = vdupq_n_u8(static_cast<std::uint8_t>('\n'));
    const uint8x16_t lessThanVector = vdupq_n_u8(static_cast<std::uint8_t>('<'));
    const uint8x16_t ampersandVector = vdupq_n_u8(static_cast<std::uint8_t>('&'));

    std::size_t offset = 0;
    while (offset + 16 <= length) {
        const uint8x16_t block = vld1q_u8(reinterpret_cast<const std::uint8_t*>(data + offset));
        const uint8x16_t newlineMatches = vceqq_u8(block, newlineVector);
        const uint8x16_t lessThanMatches = vceqq_u8(block, lessThanVector);
        const uint8x16_t ampersandMatches = vceqq_u8(block, ampersandVector);
        const uint8x16_t anyMatches = vorrq_u8(newlineMatches, vorrq_u8(lessThanMatches, ampersandMatches));

        alignas(16) std::uint8_t maskBytes[16];
        vst1q_u8(maskBytes, anyMatches);
        for (std::size_t index = 0; index < 16; ++index) {
            if (maskBytes[index] != 0) {
                return offset + index;
            }
        }
        offset += 16;
    }

    for (; offset < length; ++offset) {
        if (data[offset] == '\n' || data[offset] == '<' || data[offset] == '&') {
            return offset;
        }
    }
    return std::string::npos;
#else
    for (std::size_t offset = 0; offset < length; ++offset) {
        if (data[offset] == '\n' || data[offset] == '<' || data[offset] == '&') {
            return offset;
        }
    }
    return std::string::npos;
#endif
}

template <typename FindFn>
std::size_t ScanDelimitedSectionEndAt(std::size_t contentStart, std::string_view terminator, FindFn&& find) noexcept {
    return find(terminator, contentStart);
}

template <typename CharAtFn, typename StartsWithFn>
bool ConsumeXmlStartTagCloseAt(
    std::size_t& position,
    bool& isEmptyElement,
    CharAtFn&& charAt,
    StartsWithFn&& startsWith) noexcept {
    if (startsWith(position, "/>")) {
        position += 2;
        isEmptyElement = true;
        return true;
    }
    if (charAt(position) == '>') {
        ++position;
        isEmptyElement = false;
        return true;
    }
    return false;
}

template <typename SkipWhitespaceFn, typename CharAtFn>
bool ConsumeXmlEndTagCloseAt(std::size_t& position, SkipWhitespaceFn&& skipWhitespace, CharAtFn&& charAt) noexcept {
    skipWhitespace(position);
    if (charAt(position) != '>') {
        return false;
    }
    ++position;
    return true;
}

struct XmlAttributeAssignmentToken {
    std::string name;
    std::size_t rawValueStart = std::string::npos;
    std::size_t rawValueEnd = std::string::npos;
    std::size_t invalidCharacterPosition = std::string::npos;
    std::size_t invalidLtPosition = std::string::npos;
    unsigned char rawValueFlags = 0;
    bool sawEquals = false;
    bool scannedRawValue = false;
    bool valid = false;
};

template <typename ParseNameFn, typename SkipWhitespaceFn, typename CharAtFn, typename ScanQuotedValueFn>
XmlAttributeAssignmentToken ParseXmlAttributeAssignmentAt(
    std::size_t& position,
    ParseNameFn&& parseName,
    SkipWhitespaceFn&& skipWhitespace,
    CharAtFn&& charAt,
    ScanQuotedValueFn&& scanQuotedValue) {
    XmlAttributeAssignmentToken token;
    token.name = parseName(position);
    if (token.name.empty()) {
        return token;
    }

    skipWhitespace(position);
    if (charAt(position) != '=') {
        return token;
    }
    token.sawEquals = true;
    ++position;

    skipWhitespace(position);
    const char quote = charAt(position);
    const auto quoteEnd = scanQuotedValue(position);
    if ((quote != '"' && quote != '\'') || quoteEnd == std::string::npos) {
        return token;
    }

    token.rawValueStart = position + 1;
    token.rawValueEnd = quoteEnd;
    position = quoteEnd + 1;
    token.valid = true;
    return token;
}

std::filesystem::path CreateTemporaryXmlReplayPath() {
    const auto tempDirectory = std::filesystem::temp_directory_path();
    static std::atomic<unsigned long long> counter{0};

    while (true) {
        const auto uniqueValue = counter.fetch_add(1, std::memory_order_relaxed) + 1;
        const auto timestamp = static_cast<unsigned long long>(
            std::chrono::steady_clock::now().time_since_epoch().count());
        const auto candidate = tempDirectory / (
            "libxml-xmlreader-replay-"
            + std::to_string(timestamp)
            + "-"
            + std::to_string(uniqueValue)
            + ".xml");
        if (!std::filesystem::exists(candidate)) {
            return candidate;
        }
    }
}

std::shared_ptr<std::istream> SpoolStreamToTemporaryReplayStream(std::istream& stream, std::string_view initialBytes = {}) {
    const auto path = CreateTemporaryXmlReplayPath();
    auto replayStream = std::shared_ptr<std::fstream>(
        new std::fstream(path, std::ios::in | std::ios::out | std::ios::binary | std::ios::trunc),
        [path](std::fstream* file) {
            if (file != nullptr) {
                file->close();
                delete file;
            }
            std::error_code error;
            std::filesystem::remove(path, error);
        });

    if (!*replayStream) {
        throw XmlException("Failed to create temporary XML replay file");
    }

    if (!initialBytes.empty()) {
        replayStream->write(initialBytes.data(), static_cast<std::streamsize>(initialBytes.size()));
        if (!*replayStream) {
            replayStream.reset();
            throw XmlException("Failed to write temporary XML replay file");
        }
    }

    char chunk[64 * 1024];
    while (stream) {
        stream.read(chunk, static_cast<std::streamsize>(sizeof(chunk)));
        const auto bytesRead = stream.gcount();
        if (bytesRead <= 0) {
            break;
        }

        replayStream->write(chunk, bytesRead);
        if (!*replayStream) {
            break;
        }
    }

    if (!*replayStream) {
        replayStream.reset();
        throw XmlException("Failed to write temporary XML replay file");
    }

    replayStream->flush();
    replayStream->clear();
    replayStream->seekg(0, std::ios::beg);
    if (!*replayStream) {
        replayStream.reset();
        throw XmlException("Failed to rewind temporary XML replay file");
    }

    return std::static_pointer_cast<std::istream>(replayStream);
}

std::string ReadStreamPrefix(std::istream& stream, std::size_t maxBytes) {
    std::string buffer;
    buffer.reserve(maxBytes);

    while (buffer.size() < maxBytes && stream) {
        const auto remaining = maxBytes - buffer.size();
        const auto chunkSize = static_cast<std::streamsize>((std::min<std::size_t>)(remaining, 64 * 1024));
        std::string chunk(static_cast<std::size_t>(chunkSize), '\0');
        stream.read(chunk.data(), chunkSize);
        const auto bytesRead = stream.gcount();
        if (bytesRead <= 0) {
            break;
        }
        chunk.resize(static_cast<std::size_t>(bytesRead));
        buffer += chunk;
    }

    return buffer;
}

std::size_t GetUtf8BomLength(std::string_view text) noexcept {
    return text.size() >= 3
            && static_cast<unsigned char>(text[0]) == 0xEF
            && static_cast<unsigned char>(text[1]) == 0xBB
            && static_cast<unsigned char>(text[2]) == 0xBF
        ? 3u
        : 0u;
}

std::size_t ScanDocumentTypeInternalSubsetEnd(std::string_view text, std::size_t contentStart) noexcept {
    std::size_t position = contentStart;
    int bracketDepth = 1;
    bool inQuote = false;
    char quote = '\0';

    const auto startsWith = [&text](std::size_t probe, std::string_view token) noexcept {
        return probe + token.size() <= text.size() && text.compare(probe, token.size(), token) == 0;
    };

    while (position < text.size() && bracketDepth > 0) {
        if (!inQuote && startsWith(position, "<!--")) {
            const auto commentEnd = text.find("-->", position + 4);
            if (commentEnd == std::string_view::npos) {
                break;
            }

            position = commentEnd + 3;
            continue;
        }

        if (!inQuote && startsWith(position, "<?")) {
            const auto processingInstructionEnd = text.find("?>", position + 2);
            if (processingInstructionEnd == std::string_view::npos) {
                break;
            }

            position = processingInstructionEnd + 2;
            continue;
        }

        const char ch = text[position++];
        if (inQuote) {
            if (ch == quote) {
                inQuote = false;
            }
            continue;
        }

        if (ch == '"' || ch == '\'') {
            inQuote = true;
            quote = ch;
        } else if (ch == '[') {
            ++bracketDepth;
        } else if (ch == ']') {
            --bracketDepth;
        }
    }

    return bracketDepth == 0 ? position - 1 : std::string::npos;
}

struct ParsedDocumentTypeDeclaration {
    std::string name;
    std::string publicId;
    std::string systemId;
    std::string internalSubset;
};

struct ParsedXmlDeclarationData {
    std::string version = "1.0";
    std::string encoding;
    std::string standalone;
};

using DtdRequiredAttributeDeclarations = std::unordered_map<std::string, std::unordered_set<std::string>>;
using DtdDefaultAttributeDeclarations = std::unordered_map<std::string, std::unordered_map<std::string, std::string>>;
using DtdFixedAttributeDeclarations = std::unordered_map<std::string, std::unordered_map<std::string, std::string>>;
using DtdEnumeratedAttributeDeclarations = std::unordered_map<std::string, std::unordered_map<std::string, std::string>>;
using DtdIdAttributeDeclarations = std::unordered_map<std::string, std::string>;
using DtdNotationAttributeDeclarations = std::unordered_map<std::string, std::string>;
using DtdNmTokenAttributeDeclarations = std::unordered_map<std::string, std::unordered_map<std::string, bool>>;
using DtdNameAttributeDeclarations = std::unordered_map<std::string, std::unordered_map<std::string, std::string>>;
using DtdDeclaredAttributeNames = std::unordered_map<std::string, std::unordered_set<std::string>>;
using DtdEmptyElementDeclarations = std::unordered_set<std::string>;
using DtdElementContentDeclarations = std::unordered_map<std::string, std::string>;


}  // namespace

namespace {

void SkipSubsetWhitespace(std::string_view text, std::size_t& position) {
    SkipXmlWhitespaceAt(text, position);
}

bool ConsumeRequiredSubsetWhitespace(std::string_view text, std::size_t& position) {
    const auto start = position;
    SkipSubsetWhitespace(text, position);
    return position > start;
}

bool StartsWithAt(std::string_view text, std::size_t position, std::string_view token) {
    return position + token.size() <= text.size() && text.substr(position, token.size()) == token;
}

bool IsValidPublicIdentifierLiteral(std::string_view value) {
    for (char ch : value) {
        const bool allowed = (ch == ' ' || ch == '\r' || ch == '\n'
            || (ch >= 'a' && ch <= 'z')
            || (ch >= 'A' && ch <= 'Z')
            || (ch >= '0' && ch <= '9')
            || ch == '-' || ch == '\'' || ch == '(' || ch == ')' || ch == '+' || ch == ','
            || ch == '.' || ch == '/' || ch == ':' || ch == '=' || ch == '?' || ch == ';'
            || ch == '!' || ch == '*' || ch == '#' || ch == '@' || ch == '$' || ch == '_' || ch == '%');
        if (!allowed) {
            return false;
        }
    }
    return true;
}

bool SystemLiteralHasUriFragment(std::string_view value) {
    return value.find('#') != std::string_view::npos;
}

std::string ReadSubsetName(std::string_view text, std::size_t& position) {
    return ParseNameAt(text, position);
}

std::string ReadSubsetQuotedValue(std::string_view text, std::size_t& position) {
    if (position >= text.size() || (text[position] != '\'' && text[position] != '"')) {
        return {};
    }

    const auto start = position + 1;
    const auto quoteEnd = ScanQuotedValueEndAt(position, [&text](std::size_t probe) noexcept {
        return probe < text.size() ? text[probe] : '\0';
    });

    if (quoteEnd == std::string::npos) {
        position = text.size();
        return std::string(text.substr(start));
    }

    position = quoteEnd + 1;

    return std::string(text.substr(start, quoteEnd - start));
}

ParsedXmlDeclarationData ParseXmlDeclarationValue(std::string_view value) {
    ParsedXmlDeclarationData declaration;
    std::size_t position = 0;
    bool sawVersion = false;
    bool sawEncoding = false;
    bool sawStandalone = false;

    while (position < value.size()) {
        SkipXmlWhitespaceAt(value, position);
        if (position >= value.size()) {
            break;
        }

        const auto token = ParseXmlAttributeAssignmentAt(
            position,
            [&value](std::size_t& probe) {
                return ParseNameAt(value, probe);
            },
            [&value](std::size_t& probe) noexcept {
                SkipXmlWhitespaceAt(value, probe);
            },
            [&value](std::size_t probe) noexcept {
                return probe < value.size() ? value[probe] : '\0';
            },
            [&value](std::size_t probe) noexcept {
                return ScanQuotedValueEndAt(probe, [&value](std::size_t innerProbe) noexcept {
                    return innerProbe < value.size() ? value[innerProbe] : '\0';
                });
            });

        if (token.name.empty() || !token.valid) {
            throw XmlException("Malformed XML declaration");
        }

        if (token.name == "version") {
            if (sawEncoding || sawStandalone) {
                throw XmlException("Malformed XML declaration");
            }
            if (sawVersion) {
                throw XmlException("Malformed XML declaration");
            }
            sawVersion = true;
            declaration.version = std::string(value.substr(token.rawValueStart, token.rawValueEnd - token.rawValueStart));
            ValidateXmlDeclarationVersion(declaration.version);
        } else if (token.name == "encoding") {
            if (!sawVersion || sawStandalone) {
                throw XmlException("Malformed XML declaration");
            }
            if (sawEncoding) {
                throw XmlException("Malformed XML declaration");
            }
            sawEncoding = true;
            declaration.encoding = std::string(value.substr(token.rawValueStart, token.rawValueEnd - token.rawValueStart));
            ValidateXmlDeclarationEncoding(declaration.encoding);
        } else if (token.name == "standalone") {
            if (!sawVersion) {
                throw XmlException("Malformed XML declaration");
            }
            if (sawStandalone) {
                throw XmlException("Malformed XML declaration");
            }
            sawStandalone = true;
            declaration.standalone = std::string(value.substr(token.rawValueStart, token.rawValueEnd - token.rawValueStart));
            if (!declaration.standalone.empty() && declaration.standalone != "yes" && declaration.standalone != "no") {
                throw XmlException("Malformed XML declaration");
            }
        } else {
            throw XmlException("Malformed XML declaration");
        }
    }

    if (!sawVersion) {
        throw XmlException("Malformed XML declaration");
    }

    return declaration;
}

ParsedDocumentTypeDeclaration ParseDocumentTypeDeclaration(std::string_view outerXml) {
    std::size_t position = 0;
    if (!StartsWithAt(outerXml, position, "<!DOCTYPE")) {
        throw XmlException("Malformed DOCTYPE declaration");
    }

    position += 9;
    SkipSubsetWhitespace(outerXml, position);

    ParsedDocumentTypeDeclaration declaration;
    declaration.name = ParseNameAt(outerXml, position);
    if (declaration.name.empty()) {
        throw XmlException("Malformed DOCTYPE declaration");
    }

    SkipSubsetWhitespace(outerXml, position);
    if (StartsWithAt(outerXml, position, "PUBLIC")) {
        position += 6;
        SkipSubsetWhitespace(outerXml, position);
        declaration.publicId = ReadSubsetQuotedValue(outerXml, position);
        if (declaration.publicId.empty() || !IsValidPublicIdentifierLiteral(declaration.publicId)) {
            throw XmlException("Malformed DOCTYPE declaration");
        }
        SkipSubsetWhitespace(outerXml, position);
        declaration.systemId = ReadSubsetQuotedValue(outerXml, position);
        if (declaration.systemId.empty()) {
            throw XmlException("Malformed DOCTYPE declaration");
        }
        if (SystemLiteralHasUriFragment(declaration.systemId)) {
            throw XmlException("Malformed DOCTYPE declaration");
        }
        SkipSubsetWhitespace(outerXml, position);
    } else if (StartsWithAt(outerXml, position, "SYSTEM")) {
        position += 6;
        SkipSubsetWhitespace(outerXml, position);
        declaration.systemId = ReadSubsetQuotedValue(outerXml, position);
        if (declaration.systemId.empty()) {
            throw XmlException("Malformed DOCTYPE declaration");
        }
        if (SystemLiteralHasUriFragment(declaration.systemId)) {
            throw XmlException("Malformed DOCTYPE declaration");
        }
        SkipSubsetWhitespace(outerXml, position);
    }

    if (position < outerXml.size() && outerXml[position] == '[') {
        ++position;
        const auto subsetEnd = ScanDocumentTypeInternalSubsetEnd(outerXml, position);
        if (subsetEnd == std::string::npos) {
            throw XmlException("Unterminated DOCTYPE internal subset");
        }
        declaration.internalSubset = Trim(std::string(outerXml.substr(position, subsetEnd - position)));
        position = subsetEnd + 1;
        SkipSubsetWhitespace(outerXml, position);
    }

    if (position >= outerXml.size() || outerXml[position] != '>') {
        throw XmlException("Malformed DOCTYPE declaration");
    }

    return declaration;
}

void AppendSchemaValidationNode(
    XmlDocument& document,
    const std::vector<std::shared_ptr<XmlElement>>& elementStack,
    const std::shared_ptr<XmlNode>& node) {
    if (node == nullptr) {
        return;
    }
    if (elementStack.empty()) {
        document.AppendChild(node);
        return;
    }
    elementStack.back()->AppendChild(node);
}




bool SkipSubsetDeclaration(std::string_view text, std::size_t& position) {
    char quote = '\0';
    while (position < text.size()) {
        const char ch = text[position++];
        if (quote != '\0') {
            if (ch == quote) {
                quote = '\0';
            }
            continue;
        }

        if (ch == '\'' || ch == '"') {
            quote = ch;
            continue;
        }

        if (ch == '>') {
            return true;
        }
    }

    return false;
}

bool ConsumeSubsetDeclarationEnd(std::string_view text, std::size_t& position) {
    SkipSubsetWhitespace(text, position);
    if (position >= text.size() || text[position] != '>') {
        return false;
    }

    ++position;
    return true;
}

std::optional<std::string> ReadSubsetAttlistToken(std::string_view text, std::size_t& position) {
    SkipSubsetWhitespace(text, position);
    if (position >= text.size()) {
        return std::nullopt;
    }

    const char ch = text[position];
    if (ch == '\'' || ch == '"') {
        return ReadSubsetQuotedValue(text, position);
    }

    if (ch == '(') {
        const std::size_t start = position;
        int depth = 0;
        do {
            if (text[position] == '(') {
                ++depth;
            } else if (text[position] == ')') {
                --depth;
            }
            ++position;
        } while (position < text.size() && depth > 0);
        return std::string(text.substr(start, position - start));
    }

    const std::size_t start = position;
    while (position < text.size() && !IsWhitespace(text[position]) && text[position] != '>') {
        ++position;
    }
    return std::string(text.substr(start, position - start));
}

void UpsertDtdValueDeclaration(
    DtdDefaultAttributeDeclarations& declarations,
    const std::string& elementName,
    std::string attributeName,
    std::string value) {
    auto& elementDeclarations = declarations[elementName];
    if (elementDeclarations.find(attributeName) == elementDeclarations.end()) {
        elementDeclarations.emplace(std::move(attributeName), std::move(value));
    }
}

void UpsertDtdFixedDeclaration(
    DtdFixedAttributeDeclarations& declarations,
    const std::string& elementName,
    std::string attributeName,
    std::string value) {
    auto& elementDeclarations = declarations[elementName];
    if (elementDeclarations.find(attributeName) == elementDeclarations.end()) {
        elementDeclarations.emplace(std::move(attributeName), std::move(value));
    }
}

void AddDtdRequiredDeclaration(
    DtdRequiredAttributeDeclarations& declarations,
    const std::string& elementName,
    std::string attributeName) {
    declarations[elementName].insert(std::move(attributeName));
}

void UpsertDtdEnumeratedDeclaration(
    DtdEnumeratedAttributeDeclarations& declarations,
    const std::string& elementName,
    std::string attributeName,
    std::string enumerationValueSet) {
    auto& elementDeclarations = declarations[elementName];
    if (elementDeclarations.find(attributeName) == elementDeclarations.end()) {
        elementDeclarations.emplace(std::move(attributeName), std::move(enumerationValueSet));
    }
}

bool IsValidDtdEnumerationValueSet(std::string_view valueSet) {
    return valueSet.size() >= 3 && valueSet.front() == '(' && valueSet.back() == ')';
}

bool IsValidDtdNmtoken(std::string_view value) {
    try {
        (void)XmlConvert::VerifyNmToken(value);
        return true;
    } catch (const XmlException&) {
        return false;
    }
}

bool IsValidDtdName(std::string_view value) {
    try {
        (void)XmlConvert::VerifyName(value);
        return true;
    } catch (const XmlException&) {
        return false;
    }
}

bool IsValidDtdNcName(std::string_view value) {
    try {
        (void)XmlConvert::VerifyNCName(value);
        return true;
    } catch (const XmlException&) {
        return false;
    }
}

template <typename Callback>
void ForEachDtdNamesValueToken(std::string_view value, Callback&& callback) {
    std::size_t position = 0;
    while (position < value.size()) {
        while (position < value.size() && IsWhitespace(value[position])) {
            ++position;
        }
        if (position >= value.size()) {
            break;
        }

        const std::size_t start = position;
        while (position < value.size() && !IsWhitespace(value[position])) {
            ++position;
        }

        callback(value.substr(start, position - start));
    }
}

bool IsValidDtdNamesValue(std::string_view value, bool allowMultipleTokens, bool requireNcName);

bool IsValidDtdNamesValue(std::string_view value, bool allowMultipleTokens) {
    return IsValidDtdNamesValue(value, allowMultipleTokens, false);
}

bool IsValidDtdNamesValue(std::string_view value, bool allowMultipleTokens, bool requireNcName) {
    if (value.empty()) {
        return false;
    }

    std::size_t tokenCount = 0;
    bool valid = true;
    ForEachDtdNamesValueToken(value, [&](std::string_view token) {
        if (!(requireNcName ? IsValidDtdNcName(token) : IsValidDtdName(token))) {
            valid = false;
            return;
        }

        ++tokenCount;
        if (!allowMultipleTokens && tokenCount > 1) {
            valid = false;
        }
    });

    return valid && tokenCount > 0;
}

bool IsValidDtdEnumeratedValueSet(std::string_view valueSet, bool requireNames) {
    if (!IsValidDtdEnumerationValueSet(valueSet)) {
        return false;
    }

    const std::string_view inner = valueSet.substr(1, valueSet.size() - 2);
    std::unordered_set<std::string> declaredValues;
    std::size_t start = 0;
    bool sawValue = false;
    while (start <= inner.size()) {
        const std::size_t separator = inner.find('|', start);
        const std::string token = Trim(std::string(inner.substr(
            start,
            separator == std::string_view::npos ? std::string_view::npos : separator - start)));
        if (token.empty()) {
            return false;
        }
        if (requireNames ? !IsValidDtdName(token) : !IsValidDtdNmtoken(token)) {
            return false;
        }
        if (!declaredValues.insert(token).second) {
            return false;
        }

        sawValue = true;
        if (separator == std::string_view::npos) {
            break;
        }
        start = separator + 1;
    }

    return sawValue;
}

bool IsValidXmlSpaceEnumerationValueSet(std::string_view valueSet) {
    if (!IsValidDtdEnumerationValueSet(valueSet)) {
        return false;
    }

    const std::string_view inner = valueSet.substr(1, valueSet.size() - 2);
    std::size_t start = 0;
    bool sawValue = false;
    while (start <= inner.size()) {
        const std::size_t separator = inner.find('|', start);
        const std::string token = Trim(std::string(inner.substr(
            start,
            separator == std::string_view::npos ? std::string_view::npos : separator - start)));
        if (token != "default" && token != "preserve") {
            return false;
        }

        sawValue = true;
        if (separator == std::string_view::npos) {
            break;
        }
        start = separator + 1;
    }

    return sawValue;
}

bool IsValidDtdNmTokensValue(std::string_view value, bool allowMultipleTokens) {
    if (value.empty()) {
        return false;
    }

    std::size_t position = 0;
    std::size_t tokenCount = 0;
    while (position < value.size()) {
        while (position < value.size() && IsWhitespace(value[position])) {
            ++position;
        }
        if (position >= value.size()) {
            break;
        }

        const std::size_t start = position;
        while (position < value.size() && !IsWhitespace(value[position])) {
            ++position;
        }

        if (!IsValidDtdNmtoken(value.substr(start, position - start))) {
            return false;
        }

        ++tokenCount;
        if (!allowMultipleTokens && tokenCount > 1) {
            return false;
        }
    }

    return tokenCount > 0;
}

void UpsertDtdNmTokenDeclaration(
    DtdNmTokenAttributeDeclarations& declarations,
    const std::string& elementName,
    std::string attributeName,
    bool allowMultipleTokens) {
    auto& elementDeclarations = declarations[elementName];
    if (elementDeclarations.find(attributeName) == elementDeclarations.end()) {
        elementDeclarations.emplace(std::move(attributeName), allowMultipleTokens);
    }
}

void UpsertDtdNameDeclaration(
    DtdNameAttributeDeclarations& declarations,
    const std::string& elementName,
    std::string attributeName,
    std::string attributeType) {
    auto& elementDeclarations = declarations[elementName];
    if (elementDeclarations.find(attributeName) == elementDeclarations.end()) {
        elementDeclarations.emplace(std::move(attributeName), std::move(attributeType));
    }
}

bool IsValidDtdAttlistAttributeType(std::string_view attributeType) {
    return attributeType == "CDATA"
        || attributeType == "ID"
        || attributeType == "IDREF"
        || attributeType == "IDREFS"
        || attributeType == "ENTITY"
        || attributeType == "ENTITIES"
        || attributeType == "NMTOKEN"
    || attributeType == "NMTOKENS";
}

bool AttributeValueMatchesDtdEnumeration(std::string_view value, std::string_view enumerationValueSet) {
    if (enumerationValueSet.size() < 2 || enumerationValueSet.front() != '(' || enumerationValueSet.back() != ')') {
        return false;
    }

    const std::string_view inner = enumerationValueSet.substr(1, enumerationValueSet.size() - 2);
    std::size_t start = 0;
    while (start <= inner.size()) {
        std::size_t separator = inner.find('|', start);
        std::string candidate = Trim(std::string(inner.substr(
            start,
            separator == std::string_view::npos ? std::string_view::npos : separator - start)));
        if (candidate == value) {
            return true;
        }

        if (separator == std::string_view::npos) {
            break;
        }
        start = separator + 1;
    }

    return false;
}

bool ReadSubsetConditionalSection(
    std::string_view text,
    std::size_t& position,
    bool& includeSection,
    std::string_view& content) {
    if (!SubsetStartsWithAt(text, position, "<![")) {
        return false;
    }

    position += 3;
    SkipSubsetWhitespace(text, position);
    if (SubsetStartsWithAt(text, position, "INCLUDE")) {
        includeSection = true;
        position += 7;
    } else if (SubsetStartsWithAt(text, position, "IGNORE")) {
        includeSection = false;
        position += 6;
    } else {
        return false;
    }

    SkipSubsetWhitespace(text, position);
    if (position >= text.size() || text[position] != '[') {
        return false;
    }
    ++position;

    const auto contentStart = position;
    int depth = 1;
    char quote = '\0';
    while (position < text.size()) {
        if (quote != '\0') {
            if (text[position++] == quote) {
                quote = '\0';
            }
            continue;
        }

        if (text[position] == '\'' || text[position] == '"') {
            quote = text[position++];
            continue;
        }

        if (StartsWithAt(text, position, "<![")) {
            position += 3;
            ++depth;
            continue;
        }

        if (StartsWithAt(text, position, "]]>") ) {
            --depth;
            if (depth == 0) {
                content = text.substr(contentStart, position - contentStart);
                position += 3;
                return true;
            }
            position += 3;
            continue;
        }

        ++position;
    }

    return false;
}

[[noreturn]] void ThrowUnsupportedDtdDeclaration(const std::string& kind) {
    throw XmlException("Unsupported DTD declaration: " + kind);
}

[[noreturn]] void ThrowMalformedDtdDeclaration(const std::string& kind) {
    throw XmlException("Malformed DTD " + kind + " declaration");
}

bool ParseDtdElementContentSpec(std::string_view text, std::size_t& position) {
    const auto invalid = []() {
        ThrowMalformedDtdDeclaration("ELEMENT");
    };
    const auto skipWhitespace = [&text](std::size_t& probe) {
        SkipSubsetWhitespace(text, probe);
    };
    const auto normalizeParticleSignature = [&text](std::size_t start, std::size_t end) {
        std::string signature;
        signature.reserve(end > start ? end - start : 0);
        for (std::size_t index = start; index < end; ++index) {
            if (!IsWhitespace(text[index])) {
                signature.push_back(text[index]);
            }
        }
        return signature;
    };

    std::function<void(std::size_t&)> parseChildrenGroup;
    std::function<void(std::size_t&)> parseContentParticle;
    std::function<void(std::size_t&)> parseMixedGroup;

    parseChildrenGroup = [&](std::size_t& probe) {
        if (probe >= text.size() || text[probe] != '(') {
            invalid();
        }
        ++probe;
        skipWhitespace(probe);

        const auto firstParticleStart = probe;
        parseContentParticle(probe);
        const std::string firstParticleSignature = normalizeParticleSignature(firstParticleStart, probe);
        skipWhitespace(probe);
        char separator = '\0';
        std::unordered_set<std::string> choiceParticleSignatures;
        while (probe < text.size() && (text[probe] == ',' || text[probe] == '|')) {
            if (separator == '\0') {
                separator = text[probe];
                if (separator == '|') {
                    choiceParticleSignatures.insert(firstParticleSignature);
                }
            } else if (text[probe] != separator) {
                invalid();
            }
            ++probe;
            skipWhitespace(probe);
            const auto particleStart = probe;
            parseContentParticle(probe);
            if (separator == '|') {
                const std::string particleSignature = normalizeParticleSignature(particleStart, probe);
                if (!choiceParticleSignatures.insert(particleSignature).second) {
                    throw XmlException("DTD validation failed: non-deterministic content model");
                }
            }
            skipWhitespace(probe);
        }

        if (probe >= text.size() || text[probe] != ')') {
            invalid();
        }
        ++probe;
    };

    parseContentParticle = [&](std::size_t& probe) {
        skipWhitespace(probe);
        if (probe >= text.size()) {
            invalid();
        }

        if (text[probe] == '(') {
            std::size_t nestedProbe = probe + 1;
            skipWhitespace(nestedProbe);
            if (StartsWithAt(text, nestedProbe, "#PCDATA")) {
                invalid();
            }
            parseChildrenGroup(probe);
        } else {
            const auto particleName = ReadSubsetName(text, probe);
            if (particleName.empty()) {
                invalid();
            }
        }

        if (probe < text.size() && (text[probe] == '?' || text[probe] == '*' || text[probe] == '+')) {
            ++probe;
        }
    };

    parseMixedGroup = [&](std::size_t& probe) {
        if (probe >= text.size() || text[probe] != '(') {
            invalid();
        }

        ++probe;
        skipWhitespace(probe);
        if (!StartsWithAt(text, probe, "#PCDATA")) {
            invalid();
        }

        std::unordered_set<std::string> mixedNames;
        probe += 7;
        skipWhitespace(probe);
        if (probe < text.size() && text[probe] == ')') {
            ++probe;
            if (probe < text.size() && text[probe] == '*') {
                ++probe;
            }
            return;
        }

        while (probe < text.size() && text[probe] == '|') {
            ++probe;
            skipWhitespace(probe);
            const auto mixedName = ReadSubsetName(text, probe);
            if (mixedName.empty()) {
                invalid();
            }
            if (!mixedNames.insert(mixedName).second) {
                invalid();
            }
            skipWhitespace(probe);
        }

        if (probe >= text.size() || text[probe] != ')') {
            invalid();
        }
        ++probe;
        if (probe >= text.size() || text[probe] != '*') {
            invalid();
        }
        ++probe;
    };

    if (StartsWithAt(text, position, "EMPTY")) {
        position += 5;
        return true;
    }

    if (StartsWithAt(text, position, "ANY")) {
        position += 3;
        return false;
    }

    if (position >= text.size() || text[position] != '(') {
        invalid();
    }

    std::size_t probe = position + 1;
    skipWhitespace(probe);
    if (StartsWithAt(text, probe, "#PCDATA")) {
        probe = position;
        parseMixedGroup(probe);
        position = probe;
        return false;
    }

    probe = position;
    parseChildrenGroup(probe);
    if (probe < text.size() && (text[probe] == '?' || text[probe] == '*' || text[probe] == '+')) {
        ++probe;
    }

    position = probe;
    return false;
}

void ValidateDtdEntityReplacementTextLiteral(
    std::string_view replacementText,
    bool allowParameterEntityReferences = false);

std::size_t ScanSubsetMarkupEndRespectingQuotes(std::string_view text, std::size_t position) {
    char quote = '\0';
    for (std::size_t probe = position; probe < text.size(); ++probe) {
        if (quote != '\0') {
            if (text[probe] == quote) {
                quote = '\0';
            }
            continue;
        }

        if (text[probe] == '\'' || text[probe] == '"') {
            quote = text[probe];
            continue;
        }

        if (text[probe] == '>') {
            return probe + 1;
        }
    }

    return std::string_view::npos;
}

bool TryParseParameterEntityReferenceAt(
    std::string_view text,
    std::size_t position,
    std::string& name,
    std::size_t& endPosition) {
    if (position >= text.size() || text[position] != '%') {
        return false;
    }

    std::size_t probe = position + 1;
    name = ReadSubsetName(text, probe);
    if (name.empty() || probe >= text.size() || text[probe] != ';') {
        return false;
    }

    endPosition = probe + 1;
    return true;
}

void CollectDtdParameterEntityDeclarations(
    std::string_view text,
    std::unordered_map<std::string, std::string>& declarations,
    bool allowConditionalSections = false,
    std::unordered_map<std::string, std::string>* externalSystemLiterals = nullptr) {
    std::size_t position = 0;
    while (position < text.size()) {
        SkipSubsetWhitespace(text, position);
        if (position >= text.size()) {
            break;
        }

        if (StartsWithAt(text, position, "<!--")) {
            const auto end = text.find("-->", position + 4);
            position = end == std::string_view::npos ? text.size() : end + 3;
            continue;
        }

        if (StartsWithAt(text, position, "<?")) {
            const auto end = text.find("?>", position + 2);
            position = end == std::string_view::npos ? text.size() : end + 2;
            continue;
        }

        if (SubsetStartsWithAt(text, position, "<![")) {
            const auto conditionalSectionStart = position;
            bool includeSection = false;
            std::string_view conditionalContent;
            if (allowConditionalSections
                && ReadSubsetConditionalSection(text, position, includeSection, conditionalContent)) {
                if (includeSection) {
                    CollectDtdParameterEntityDeclarations(conditionalContent, declarations, true, externalSystemLiterals);
                }
                continue;
            }
            position = conditionalSectionStart;
        }

        if (SubsetStartsWithAt(text, position, "<!ENTITY")) {
            std::size_t probe = position + 8;
            if (!ConsumeRequiredSubsetWhitespace(text, probe)) {
                const auto end = ScanSubsetMarkupEndRespectingQuotes(text, position);
                position = end == std::string_view::npos ? text.size() : end;
                continue;
            }

            const bool parameterEntity = probe < text.size() && text[probe] == '%';
            if (!parameterEntity) {
                const auto end = ScanSubsetMarkupEndRespectingQuotes(text, position);
                position = end == std::string_view::npos ? text.size() : end;
                continue;
            }

            ++probe;
            if (!ConsumeRequiredSubsetWhitespace(text, probe)) {
                const auto end = ScanSubsetMarkupEndRespectingQuotes(text, position);
                position = end == std::string_view::npos ? text.size() : end;
                continue;
            }

            const auto name = ReadSubsetName(text, probe);
            if (name.empty() || !ConsumeRequiredSubsetWhitespace(text, probe)) {
                const auto end = ScanSubsetMarkupEndRespectingQuotes(text, position);
                position = end == std::string_view::npos ? text.size() : end;
                continue;
            }

            if (probe < text.size() && (text[probe] == '\'' || text[probe] == '"')) {
                const std::string replacementText = ReadSubsetQuotedValue(text, probe);
                declarations.try_emplace(name, replacementText);
            } else {
                const auto externalIdKeyword = ReadSubsetName(text, probe);
                std::string systemLiteral;
                if (externalIdKeyword == "SYSTEM") {
                    if (ConsumeRequiredSubsetWhitespace(text, probe)
                        && probe < text.size()
                        && (text[probe] == '\'' || text[probe] == '"')) {
                        systemLiteral = ReadSubsetQuotedValue(text, probe);
                    }
                } else if (externalIdKeyword == "PUBLIC") {
                    if (ConsumeRequiredSubsetWhitespace(text, probe)
                        && probe < text.size()
                        && (text[probe] == '\'' || text[probe] == '"')) {
                        (void)ReadSubsetQuotedValue(text, probe);
                        if (ConsumeRequiredSubsetWhitespace(text, probe)
                            && probe < text.size()
                            && (text[probe] == '\'' || text[probe] == '"')) {
                            systemLiteral = ReadSubsetQuotedValue(text, probe);
                        }
                    }
                }

                if (!systemLiteral.empty()) {
                    if (SystemLiteralHasUriFragment(systemLiteral)) {
                        ThrowMalformedDtdDeclaration("parameter entity");
                    }
                    const auto inserted = declarations.try_emplace(name, std::string());
                    if (inserted.second && externalSystemLiterals != nullptr) {
                        externalSystemLiterals->try_emplace(name, std::move(systemLiteral));
                    }
                }
            }

            const auto end = ScanSubsetMarkupEndRespectingQuotes(text, position);
            position = end == std::string_view::npos ? text.size() : end;
            continue;
        }

        if (text[position] == '%') {
            std::string name;
            std::size_t end = position;
            if (TryParseParameterEntityReferenceAt(text, position, name, end)) {
                if (declarations.find(name) == declarations.end()) {
                    ThrowMalformedDtdDeclaration("declaration");
                }
                position = end;
                continue;
            }
        }

        if (text[position] == '<') {
            const auto end = ScanSubsetMarkupEndRespectingQuotes(text, position);
            position = end == std::string_view::npos ? text.size() : end;
            continue;
        }

        ++position;
    }
}

std::string ExpandDtdParameterEntityReferencesRecursive(
    std::string_view text,
    const std::unordered_map<std::string, std::string>& declarations,
    std::vector<std::string>& expansionStack,
    bool allowConditionalSections,
    const std::unordered_set<std::string>* externalParameterEntityNames = nullptr) {
    std::string expanded;
    std::size_t position = 0;
    while (position < text.size()) {
        if (StartsWithAt(text, position, "<!--")) {
            const auto end = text.find("-->", position + 4);
            const auto next = end == std::string_view::npos ? text.size() : end + 3;
            expanded.append(text.substr(position, next - position));
            position = next;
            continue;
        }

        if (StartsWithAt(text, position, "<?")) {
            const auto end = text.find("?>", position + 2);
            const auto next = end == std::string_view::npos ? text.size() : end + 2;
            expanded.append(text.substr(position, next - position));
            position = next;
            continue;
        }

        if (SubsetStartsWithAt(text, position, "<![")) {
            const auto conditionalSectionStart = position;
            bool includeSection = false;
            std::string_view conditionalContent;
            if (allowConditionalSections
                && ReadSubsetConditionalSection(text, position, includeSection, conditionalContent)) {
                expanded.append("<![");
                expanded.append(includeSection ? "INCLUDE[" : "IGNORE[");
                if (includeSection) {
                    expanded.append(ExpandDtdParameterEntityReferencesRecursive(
                        conditionalContent,
                        declarations,
                        expansionStack,
                        true));
                } else {
                    expanded.append(conditionalContent);
                }
                expanded.append("]]>");
                continue;
            }
            position = conditionalSectionStart;
        }

        if (SubsetStartsWithAt(text, position, "<!ENTITY")) {
            std::size_t probe = position + 8;
            bool parameterEntityDeclaration = false;
            if (ConsumeRequiredSubsetWhitespace(text, probe)
                && probe < text.size()
                && text[probe] == '%') {
                parameterEntityDeclaration = true;
            }
            if (parameterEntityDeclaration) {
                const auto end = ScanSubsetMarkupEndRespectingQuotes(text, position);
                if (end == std::string_view::npos) {
                    ThrowMalformedDtdDeclaration("entity");
                }
                expanded.append(text.substr(position, end - position));
                position = end;
                continue;
            }
        }

        if (text[position] == '\'' || text[position] == '"') {
            const char quote = text[position];
            expanded.push_back(quote);
            ++position;
            while (position < text.size()) {
                expanded.push_back(text[position]);
                if (text[position] == quote) {
                    ++position;
                    break;
                }
                ++position;
            }
            continue;
        }

        if (text[position] == '%') {
            std::string name;
            std::size_t end = position;
            if (!TryParseParameterEntityReferenceAt(text, position, name, end)) {
                ThrowMalformedDtdDeclaration("declaration");
            }

            const auto found = declarations.find(name);
            if (found == declarations.end()) {
                ThrowMalformedDtdDeclaration("declaration");
            }
            if (std::find(expansionStack.begin(), expansionStack.end(), name) != expansionStack.end()) {
                ThrowMalformedDtdDeclaration("declaration");
            }

            expansionStack.push_back(name);
            const bool referenceAllowsConditionalSections = allowConditionalSections
                || (externalParameterEntityNames != nullptr
                    && externalParameterEntityNames->find(name) != externalParameterEntityNames->end());
            expanded.append(ExpandDtdParameterEntityReferencesRecursive(
                found->second,
                declarations,
                expansionStack,
                referenceAllowsConditionalSections,
                externalParameterEntityNames));
            expansionStack.pop_back();
            position = end;
            continue;
        }

        expanded.push_back(text[position]);
        ++position;
    }

    return expanded;
}

std::string ExpandDtdParameterEntityReferences(
    std::string_view text,
    const std::unordered_map<std::string, std::string>& declarations,
    bool allowConditionalSections = false,
    const std::unordered_set<std::string>* externalParameterEntityNames = nullptr) {
    std::vector<std::string> expansionStack;
    return ExpandDtdParameterEntityReferencesRecursive(
        text,
        declarations,
        expansionStack,
        allowConditionalSections,
        externalParameterEntityNames);
}

bool IsPredefinedEntityName(std::string_view name) {
    return name == "lt" || name == "gt" || name == "amp" || name == "quot" || name == "apos";
}

std::string_view GetPredefinedEntityValue(std::string_view name) {
    if (name == "lt") {
        return "<";
    }
    if (name == "gt") {
        return ">";
    }
    if (name == "amp") {
        return "&";
    }
    if (name == "quot") {
        return "\"";
    }
    if (name == "apos") {
        return "'";
    }
    return {};
}

bool TryParseEquivalentEntityCodePoint(std::string_view entity, std::uint32_t& codePoint) {
    if (entity.empty() || entity.front() != '#') {
        return false;
    }

    const bool isHex = entity.size() > 2 && (entity[1] == 'x' || entity[1] == 'X');
    const std::string_view digits = entity.substr(isHex ? 2 : 1);
    if (digits.empty()) {
        return false;
    }

    std::uint32_t value = 0;
    for (char digit : digits) {
        std::uint32_t digitValue = 0;
        if (digit >= '0' && digit <= '9') {
            digitValue = static_cast<std::uint32_t>(digit - '0');
        } else if (isHex && digit >= 'a' && digit <= 'f') {
            digitValue = 10u + static_cast<std::uint32_t>(digit - 'a');
        } else if (isHex && digit >= 'A' && digit <= 'F') {
            digitValue = 10u + static_cast<std::uint32_t>(digit - 'A');
        } else {
            return false;
        }

        const std::uint32_t base = isHex ? 16u : 10u;
        if (value > (0x10FFFFu - digitValue) / base) {
            return false;
        }
        value = value * base + digitValue;
    }

    if (!IsValidXmlCharacterCodePoint(value)) {
        return false;
    }

    codePoint = value;
    return true;
}

void AppendUtf8CodePoint(std::string& output, std::uint32_t codePoint) {
    if (codePoint <= 0x7Fu) {
        output.push_back(static_cast<char>(codePoint));
    } else if (codePoint <= 0x7FFu) {
        output.push_back(static_cast<char>(0xC0u | (codePoint >> 6)));
        output.push_back(static_cast<char>(0x80u | (codePoint & 0x3Fu)));
    } else if (codePoint <= 0xFFFFu) {
        output.push_back(static_cast<char>(0xE0u | (codePoint >> 12)));
        output.push_back(static_cast<char>(0x80u | ((codePoint >> 6) & 0x3Fu)));
        output.push_back(static_cast<char>(0x80u | (codePoint & 0x3Fu)));
    } else {
        output.push_back(static_cast<char>(0xF0u | (codePoint >> 18)));
        output.push_back(static_cast<char>(0x80u | ((codePoint >> 12) & 0x3Fu)));
        output.push_back(static_cast<char>(0x80u | ((codePoint >> 6) & 0x3Fu)));
        output.push_back(static_cast<char>(0x80u | (codePoint & 0x3Fu)));
    }
}

std::optional<std::string> ExpandCharacterAndPredefinedEntityReferences(std::string_view value) {
    std::string expanded;
    expanded.reserve(value.size());

    std::size_t cursor = 0;
    while (cursor < value.size()) {
        const auto ampersand = value.find('&', cursor);
        if (ampersand == std::string_view::npos) {
            expanded.append(value.data() + cursor, value.size() - cursor);
            return expanded;
        }

        expanded.append(value.data() + cursor, ampersand - cursor);
        const auto semicolon = value.find(';', ampersand + 1);
        if (semicolon == std::string_view::npos) {
            expanded.append(value.data() + ampersand, value.size() - ampersand);
            return expanded;
        }

        const std::string_view entity = value.substr(ampersand + 1, semicolon - ampersand - 1);
        if (entity.empty()) {
            return std::nullopt;
        }

        if (entity.front() == '#') {
            std::uint32_t codePoint = 0;
            if (!TryParseEquivalentEntityCodePoint(entity, codePoint)) {
                return std::nullopt;
            }
            AppendUtf8CodePoint(expanded, codePoint);
        } else {
            const std::string_view predefinedValue = GetPredefinedEntityValue(entity);
            if (predefinedValue.empty()) {
                return std::nullopt;
            }
            expanded.append(predefinedValue);
        }

        cursor = semicolon + 1;
    }

    return expanded;
}

bool IsEquivalentPredefinedEntityDeclaration(std::string_view name, std::string_view replacementText) {
    const std::string_view expectedValue = GetPredefinedEntityValue(name);
    if (expectedValue.empty()) {
        return false;
    }

    std::string expanded(replacementText);
    for (int iteration = 0; iteration < 8; ++iteration) {
        auto next = ExpandCharacterAndPredefinedEntityReferences(expanded);
        if (!next.has_value()) {
            return false;
        }
        if (*next == expanded) {
            return expanded == expectedValue;
        }
        expanded = std::move(*next);
    }

    return expanded == expectedValue;
}

void ParseDocumentTypeInternalSubset(
    const std::string& internalSubset,
    std::vector<std::shared_ptr<XmlNode>>& entities,
    std::vector<std::shared_ptr<XmlNode>>& notations,
    bool allowConditionalSections = false) {
    std::string_view text(internalSubset);
    std::size_t position = 0;
    std::unordered_set<std::string> declaredEntityNames;
    std::unordered_set<std::string> declaredNotationNames;
    std::unordered_set<std::string> declaredParameterEntityNames;
    while (position < text.size()) {
        SkipSubsetWhitespace(text, position);
        if (position >= text.size()) {
            break;
        }

        if (StartsWithAt(text, position, "<!--")) {
            const auto end = text.find("-->", position + 4);
            position = end == std::string_view::npos ? text.size() : end + 3;
            continue;
        }

        if (StartsWithAt(text, position, "<?")) {
            std::size_t piPosition = position + 2;
            const auto target = ReadSubsetName(text, piPosition);
            if (target.empty()) {
                ThrowMalformedDtdDeclaration("processing instruction");
            }
            if (!IsNamespaceAwareProcessingInstructionTarget(target)) {
                throw XmlException("Invalid XML name");
            }
            if (target.size() == 3
                && (target[0] == 'x' || target[0] == 'X')
                && (target[1] == 'm' || target[1] == 'M')
                && (target[2] == 'l' || target[2] == 'L')) {
                throw XmlException("XML declaration is only allowed at the beginning of the document");
            }

            if (!SubsetStartsWithAt(text, piPosition, "?>")) {
                if (piPosition >= text.size() || !IsWhitespace(text[piPosition])) {
                    ThrowMalformedDtdDeclaration("processing instruction");
                }
            }

            const auto end = text.find("?>", piPosition);
            if (end == std::string_view::npos) {
                ThrowMalformedDtdDeclaration("processing instruction");
            }
            position = end + 2;
            continue;
        }

        if (text[position] == '%') {
            std::size_t referencePosition = position + 1;
            const auto parameterEntityName = ReadSubsetName(text, referencePosition);
            if (parameterEntityName.empty()
                || referencePosition >= text.size()
                || text[referencePosition] != ';') {
                ThrowMalformedDtdDeclaration("declaration");
            }
            if (declaredParameterEntityNames.find(parameterEntityName) == declaredParameterEntityNames.end()) {
                ThrowMalformedDtdDeclaration("declaration");
            }
            position = referencePosition + 1;
            continue;
        }

        if (!SubsetStartsWithAt(text, position, "<!")) {
            ThrowMalformedDtdDeclaration("declaration");
            continue;
        }

        if (SubsetStartsWithAt(text, position, "<!DOCTYPE")) {
            ThrowMalformedDtdDeclaration("declaration");
        }

        if (SubsetStartsWithAt(text, position, "<!ENTITY")) {
            position += 8;
            if (!ConsumeRequiredSubsetWhitespace(text, position)) {
                ThrowMalformedDtdDeclaration("entity");
            }
            const bool parameterEntity = position < text.size() && text[position] == '%';
            if (parameterEntity) {
                ++position;
                if (!ConsumeRequiredSubsetWhitespace(text, position)) {
                    ThrowMalformedDtdDeclaration("parameter entity");
                }
            }

            const auto name = ReadSubsetName(text, position);
            if (name.empty()) {
                ThrowMalformedDtdDeclaration("entity");
            }
            if (!IsNamespaceAwareDtdDeclarationName(name)) {
                throw XmlException("Invalid XML name");
            }
            const bool duplicateGeneralEntity = !parameterEntity
                && declaredEntityNames.find(name) != declaredEntityNames.end();
            if (!ConsumeRequiredSubsetWhitespace(text, position)) {
                ThrowMalformedDtdDeclaration(parameterEntity ? "parameter entity" : "entity");
            }
            std::string replacementText;
            std::string publicId;
            std::string systemId;
            std::string notationName;

            if (position < text.size() && (text[position] == '\'' || text[position] == '"')) {
                replacementText = ReadSubsetQuotedValue(text, position);
                ValidateDtdEntityReplacementTextLiteral(replacementText, allowConditionalSections);
                if (!parameterEntity
                    && IsPredefinedEntityName(name)
                    && !IsEquivalentPredefinedEntityDeclaration(name, replacementText)) {
                    throw XmlException(
                        "DTD validation failed: entity declaration '" + name + "' must not redefine a predefined entity");
                }
            } else if (SubsetStartsWithAt(text, position, "PUBLIC")) {
                position += 6;
                if (!ConsumeRequiredSubsetWhitespace(text, position)) {
                    ThrowMalformedDtdDeclaration("entity");
                }
                publicId = ReadSubsetQuotedValue(text, position);
                if (publicId.empty() || !IsValidPublicIdentifierLiteral(publicId) || !ConsumeRequiredSubsetWhitespace(text, position)) {
                    ThrowMalformedDtdDeclaration("entity");
                }
                systemId = ReadSubsetQuotedValue(text, position);
                if (publicId.empty() || systemId.empty()) {
                    ThrowMalformedDtdDeclaration("entity");
                }
                if (SystemLiteralHasUriFragment(systemId)) {
                    ThrowMalformedDtdDeclaration(parameterEntity ? "parameter entity" : "entity");
                }
            } else if (SubsetStartsWithAt(text, position, "SYSTEM")) {
                position += 6;
                if (!ConsumeRequiredSubsetWhitespace(text, position)) {
                    ThrowMalformedDtdDeclaration("entity");
                }
                systemId = ReadSubsetQuotedValue(text, position);
                if (systemId.empty()) {
                    ThrowMalformedDtdDeclaration("entity");
                }
                if (SystemLiteralHasUriFragment(systemId)) {
                    ThrowMalformedDtdDeclaration(parameterEntity ? "parameter entity" : "entity");
                }
                if (!parameterEntity && IsPredefinedEntityName(name)) {
                    throw XmlException(
                        "DTD validation failed: entity declaration '" + name + "' must not redefine a predefined entity");
                }
            } else {
                ThrowMalformedDtdDeclaration("entity");
            }

            const bool hasWhitespaceBeforeTrailingToken = ConsumeRequiredSubsetWhitespace(text, position);
            if (!hasWhitespaceBeforeTrailingToken && SubsetStartsWithAt(text, position, "NDATA")) {
                ThrowMalformedDtdDeclaration(parameterEntity ? "parameter entity" : "entity");
            }
            if (SubsetStartsWithAt(text, position, "NDATA")) {
                if (parameterEntity) {
                    ThrowMalformedDtdDeclaration("parameter entity");
                }
                if (!replacementText.empty()) {
                    ThrowMalformedDtdDeclaration("entity");
                }
                position += 5;
                if (!ConsumeRequiredSubsetWhitespace(text, position)) {
                    ThrowMalformedDtdDeclaration("entity");
                }
                notationName = ReadSubsetName(text, position);
                if (notationName.empty()) {
                    ThrowMalformedDtdDeclaration("entity");
                }
            }

            if (!ConsumeSubsetDeclarationEnd(text, position)) {
                ThrowMalformedDtdDeclaration(parameterEntity ? "parameter entity" : "entity");
            }
            if (parameterEntity && !name.empty()) {
                declaredParameterEntityNames.insert(name);
            }
            if (!parameterEntity && !name.empty() && !duplicateGeneralEntity) {
                declaredEntityNames.insert(name);
                entities.push_back(std::make_shared<XmlEntity>(name, replacementText, publicId, systemId, notationName));
            }
            continue;
        }

        if (SubsetStartsWithAt(text, position, "<!NOTATION")) {
            position += 10;
            if (!ConsumeRequiredSubsetWhitespace(text, position)) {
                ThrowMalformedDtdDeclaration("notation");
            }
            const auto name = ReadSubsetName(text, position);
            if (name.empty()) {
                ThrowMalformedDtdDeclaration("notation");
            }
            if (!IsNamespaceAwareDtdDeclarationName(name)) {
                throw XmlException("Invalid XML name");
            }
            if (!declaredNotationNames.insert(name).second) {
                throw XmlException("DTD validation failed: duplicate notation declaration '" + name + "'");
            }
            if (!ConsumeRequiredSubsetWhitespace(text, position)) {
                ThrowMalformedDtdDeclaration("notation");
            }

            std::string publicId;
            std::string systemId;
            if (SubsetStartsWithAt(text, position, "PUBLIC")) {
                position += 6;
                if (!ConsumeRequiredSubsetWhitespace(text, position)) {
                    ThrowMalformedDtdDeclaration("notation");
                }
                publicId = ReadSubsetQuotedValue(text, position);
                if (publicId.empty() || !IsValidPublicIdentifierLiteral(publicId)) {
                    ThrowMalformedDtdDeclaration("notation");
                }
                const bool hasWhitespaceBeforeSystemLiteral = ConsumeRequiredSubsetWhitespace(text, position);
                if (position < text.size() && (text[position] == '\'' || text[position] == '"')) {
                    if (!hasWhitespaceBeforeSystemLiteral) {
                        ThrowMalformedDtdDeclaration("notation");
                    }
                    systemId = ReadSubsetQuotedValue(text, position);
                    if (!systemId.empty() && SystemLiteralHasUriFragment(systemId)) {
                        ThrowMalformedDtdDeclaration("notation");
                    }
                }
            } else if (SubsetStartsWithAt(text, position, "SYSTEM")) {
                position += 6;
                if (!ConsumeRequiredSubsetWhitespace(text, position)) {
                    ThrowMalformedDtdDeclaration("notation");
                }
                systemId = ReadSubsetQuotedValue(text, position);
                if (systemId.empty()) {
                    ThrowMalformedDtdDeclaration("notation");
                }
                if (SystemLiteralHasUriFragment(systemId)) {
                    ThrowMalformedDtdDeclaration("notation");
                }
            } else {
                ThrowMalformedDtdDeclaration("notation");
            }

            if (!ConsumeSubsetDeclarationEnd(text, position)) {
                ThrowMalformedDtdDeclaration("notation");
            }
            if (!name.empty()) {
                notations.push_back(std::make_shared<XmlNotation>(name, publicId, systemId));
            }
            continue;
        }

        if (SubsetStartsWithAt(text, position, "<!ELEMENT")) {
            position += 9;
            if (!ConsumeRequiredSubsetWhitespace(text, position)) {
                ThrowMalformedDtdDeclaration("ELEMENT");
            }
            const auto elementName = ReadSubsetName(text, position);
            if (elementName.empty()) {
                ThrowMalformedDtdDeclaration("ELEMENT");
            }
            if (!ConsumeRequiredSubsetWhitespace(text, position)) {
                ThrowMalformedDtdDeclaration("ELEMENT");
            }

            std::size_t contentSpecPosition = position;
            (void)ParseDtdElementContentSpec(text, contentSpecPosition);
            SkipSubsetWhitespace(text, contentSpecPosition);
            if (contentSpecPosition >= text.size() || text[contentSpecPosition] != '>') {
                ThrowMalformedDtdDeclaration("ELEMENT");
            }
            position = contentSpecPosition + 1;
            continue;
        }

        if (SubsetStartsWithAt(text, position, "<!ATTLIST")) {
            if (!SkipSubsetDeclaration(text, position)) {
                ThrowMalformedDtdDeclaration("ATTLIST");
            }
            continue;
        }

        if (SubsetStartsWithAt(text, position, "<![")) {
            if (!allowConditionalSections) {
                ThrowMalformedDtdDeclaration("conditional section");
            }
            bool includeSection = false;
            std::string_view conditionalContent;
            if (!ReadSubsetConditionalSection(text, position, includeSection, conditionalContent)) {
                ThrowMalformedDtdDeclaration("conditional section");
            }
            if (includeSection) {
                ParseDocumentTypeInternalSubset(std::string(conditionalContent), entities, notations, true);
            }
            continue;
        }

        ThrowMalformedDtdDeclaration("declaration");
    }

    std::unordered_set<std::string> notationNames;
    notationNames.reserve(notations.size());
    for (const auto& notation : notations) {
        if (notation != nullptr && notation->NodeType() == XmlNodeType::Notation) {
            notationNames.insert(notation->Name());
        }
    }

    for (const auto& entity : entities) {
        if (entity == nullptr || entity->NodeType() != XmlNodeType::Entity) {
            continue;
        }

        const auto& notationName = static_cast<const XmlEntity&>(*entity).NotationName();
        if (!notationName.empty() && notationNames.find(notationName) == notationNames.end()) {
            throw XmlException(
                "DTD validation failed: unparsed entity '" + entity->Name()
                + "' references undeclared notation '" + notationName + "'");
        }
    }

    std::unordered_set<std::string> unparsedEntityNames;
    unparsedEntityNames.reserve(entities.size());
    for (const auto& entity : entities) {
        if (entity == nullptr || entity->NodeType() != XmlNodeType::Entity) {
            continue;
        }

        const auto& typedEntity = static_cast<const XmlEntity&>(*entity);
        if (!typedEntity.NotationName().empty()) {
            unparsedEntityNames.insert(typedEntity.Name());
        }
    }

    for (const auto& entity : entities) {
        if (entity == nullptr || entity->NodeType() != XmlNodeType::Entity) {
            continue;
        }

        const auto& typedEntity = static_cast<const XmlEntity&>(*entity);
        if (!typedEntity.NotationName().empty() || typedEntity.Value().empty()) {
            continue;
        }

        std::size_t cursor = 0;
        while (cursor < typedEntity.Value().size()) {
            const auto ampersand = typedEntity.Value().find('&', cursor);
            if (ampersand == std::string::npos) {
                break;
            }

            const auto semicolon = typedEntity.Value().find(';', ampersand + 1);
            if (semicolon == std::string::npos) {
                throw XmlException("Malformed DTD entity declaration");
            }

            const std::string_view referencedEntity = std::string_view(typedEntity.Value()).substr(
                ampersand + 1,
                semicolon - ampersand - 1);
            if (!referencedEntity.empty()
                && referencedEntity.front() != '#'
                && !IsPredefinedEntityName(referencedEntity)
                && unparsedEntityNames.find(std::string(referencedEntity)) != unparsedEntityNames.end()) {
                throw XmlException(
                    "DTD validation failed: entity value must not reference unparsed entity '"
                    + std::string(referencedEntity) + "'");
            }

            cursor = semicolon + 1;
        }
    }
}

void ParseDocumentTypeAttributeDeclarationsCore(
    std::string_view internalSubset,
    DtdDeclaredAttributeNames& declaredAttributes,
    DtdRequiredAttributeDeclarations& requiredAttributes,
    DtdDefaultAttributeDeclarations& defaultAttributes,
    DtdFixedAttributeDeclarations& fixedAttributes,
    DtdEnumeratedAttributeDeclarations& enumeratedAttributes,
    DtdIdAttributeDeclarations& idAttributes,
    DtdNotationAttributeDeclarations& notationAttributes,
    DtdNmTokenAttributeDeclarations& nmTokenAttributes,
    DtdNameAttributeDeclarations& nameAttributes,
    bool allowConditionalSections = false) {
    std::size_t position = 0;
    while (position < internalSubset.size()) {
        SkipSubsetWhitespace(internalSubset, position);
        if (position >= internalSubset.size()) {
            break;
        }

        if (StartsWithAt(internalSubset, position, "<!--")) {
            const auto end = internalSubset.find("-->", position + 4);
            position = end == std::string_view::npos ? internalSubset.size() : end + 3;
            continue;
        }

        if (StartsWithAt(internalSubset, position, "<?")) {
            const auto end = internalSubset.find("?>", position + 2);
            position = end == std::string_view::npos ? internalSubset.size() : end + 2;
            continue;
        }

        if (!SubsetStartsWithAt(internalSubset, position, "<!")) {
            ++position;
            continue;
        }

        if (SubsetStartsWithAt(internalSubset, position, "<!ATTLIST")) {
            position += 9;
            if (!ConsumeRequiredSubsetWhitespace(internalSubset, position)) {
                ThrowMalformedDtdDeclaration("ATTLIST");
            }
            const auto elementName = ReadSubsetName(internalSubset, position);
            if (elementName.empty()) {
                ThrowMalformedDtdDeclaration("ATTLIST");
            }

            while (true) {
                const bool hasSeparator = ConsumeRequiredSubsetWhitespace(internalSubset, position);
                if (position >= internalSubset.size()) {
                    ThrowMalformedDtdDeclaration("ATTLIST");
                }
                if (internalSubset[position] == '>') {
                    ++position;
                    break;
                }
                if (!hasSeparator) {
                    ThrowMalformedDtdDeclaration("ATTLIST");
                }

                const auto attributeName = ReadSubsetName(internalSubset, position);
                if (attributeName.empty()) {
                    ThrowMalformedDtdDeclaration("ATTLIST");
                }
                const bool firstDeclaration = declaredAttributes[elementName].insert(attributeName).second;

                if (!ConsumeRequiredSubsetWhitespace(internalSubset, position)) {
                    ThrowMalformedDtdDeclaration("ATTLIST");
                }
                auto attributeType = ReadSubsetAttlistToken(internalSubset, position);
                if (!attributeType.has_value()) {
                    ThrowMalformedDtdDeclaration("ATTLIST");
                }
                const bool isIdAttribute = *attributeType == "ID";
                const bool isNotationAttribute = *attributeType == "NOTATION";
                const bool isNmTokenAttribute = *attributeType == "NMTOKEN";
                const bool isNmTokensAttribute = *attributeType == "NMTOKENS";
                const bool isNameAttribute = *attributeType == "IDREF"
                    || *attributeType == "IDREFS"
                    || *attributeType == "ENTITY"
                    || *attributeType == "ENTITIES";
                const bool allowsMultipleNames = *attributeType == "IDREFS" || *attributeType == "ENTITIES";
                if (isIdAttribute && firstDeclaration) {
                    const auto existingIdAttribute = idAttributes.find(elementName);
                    if (existingIdAttribute != idAttributes.end() && existingIdAttribute->second != attributeName) {
                        ThrowMalformedDtdDeclaration("ATTLIST");
                    }
                    idAttributes[elementName] = attributeName;
                }
                if (isNotationAttribute && firstDeclaration) {
                    const auto existingNotationAttribute = notationAttributes.find(elementName);
                    if (existingNotationAttribute != notationAttributes.end() && existingNotationAttribute->second != attributeName) {
                        ThrowMalformedDtdDeclaration("ATTLIST");
                    }
                    notationAttributes[elementName] = attributeName;
                }
                if (firstDeclaration && (isNmTokenAttribute || isNmTokensAttribute)) {
                    UpsertDtdNmTokenDeclaration(
                        nmTokenAttributes,
                        elementName,
                        attributeName,
                        isNmTokensAttribute);
                }
                if (firstDeclaration && isNameAttribute) {
                    UpsertDtdNameDeclaration(nameAttributes, elementName, attributeName, *attributeType);
                }
                std::optional<std::string> enumerationValueSet;
                if (IsValidDtdEnumeratedValueSet(*attributeType, false)) {
                    enumerationValueSet = *attributeType;
                } else if (!IsValidDtdAttlistAttributeType(*attributeType) && *attributeType != "NOTATION") {
                    ThrowMalformedDtdDeclaration("ATTLIST");
                }
                if (*attributeType == "NOTATION") {
                    auto notationValues = ReadSubsetAttlistToken(internalSubset, position);
                    if (!notationValues.has_value() || !IsValidDtdEnumeratedValueSet(*notationValues, true)) {
                        ThrowMalformedDtdDeclaration("ATTLIST");
                    }
                    enumerationValueSet = *notationValues;
                }
                if (firstDeclaration && enumerationValueSet.has_value()) {
                    UpsertDtdEnumeratedDeclaration(
                        enumeratedAttributes,
                        elementName,
                        attributeName,
                        *enumerationValueSet);
                }
                if (attributeName == "xml:space") {
                    if (!enumerationValueSet.has_value() || !IsValidXmlSpaceEnumerationValueSet(*enumerationValueSet)) {
                        ThrowMalformedDtdDeclaration("ATTLIST");
                    }
                }

                const bool hasDefaultSeparator = ConsumeRequiredSubsetWhitespace(internalSubset, position);
                if (!hasDefaultSeparator || position >= internalSubset.size()) {
                    ThrowMalformedDtdDeclaration("ATTLIST");
                }

                if (internalSubset[position] == '\'' || internalSubset[position] == '"') {
                    if (isIdAttribute) {
                        ThrowMalformedDtdDeclaration("ATTLIST");
                    }
                    std::string defaultValue = ReadSubsetQuotedValue(internalSubset, position);
                    if (enumerationValueSet.has_value()
                        && !AttributeValueMatchesDtdEnumeration(defaultValue, *enumerationValueSet)) {
                        ThrowMalformedDtdDeclaration("ATTLIST");
                    }
                    if ((isNmTokenAttribute || isNmTokensAttribute)
                        && !IsValidDtdNmTokensValue(defaultValue, isNmTokensAttribute)) {
                        ThrowMalformedDtdDeclaration("ATTLIST");
                    }
                    if (isNameAttribute && !IsValidDtdNamesValue(defaultValue, allowsMultipleNames)) {
                        ThrowMalformedDtdDeclaration("ATTLIST");
                    }
                    if (firstDeclaration) {
                        UpsertDtdValueDeclaration(
                            defaultAttributes,
                            elementName,
                            attributeName,
                            std::move(defaultValue));
                    }
                    continue;
                }

                auto defaultDeclaration = ReadSubsetAttlistToken(internalSubset, position);
                if (!defaultDeclaration.has_value()) {
                    ThrowMalformedDtdDeclaration("ATTLIST");
                }

                if (*defaultDeclaration == "#REQUIRED") {
                    if (firstDeclaration) {
                        AddDtdRequiredDeclaration(requiredAttributes, elementName, attributeName);
                    }
                    continue;
                }

                if (*defaultDeclaration == "#IMPLIED") {
                    continue;
                }

                if (*defaultDeclaration == "#FIXED") {
                    if (isIdAttribute) {
                        ThrowMalformedDtdDeclaration("ATTLIST");
                    }
                    if (!ConsumeRequiredSubsetWhitespace(internalSubset, position)
                        || position >= internalSubset.size()
                        || (internalSubset[position] != '\'' && internalSubset[position] != '"')) {
                        ThrowMalformedDtdDeclaration("ATTLIST");
                    }
                    std::string fixedValue = ReadSubsetQuotedValue(internalSubset, position);
                    if (enumerationValueSet.has_value()
                        && !AttributeValueMatchesDtdEnumeration(fixedValue, *enumerationValueSet)) {
                        ThrowMalformedDtdDeclaration("ATTLIST");
                    }
                    if ((isNmTokenAttribute || isNmTokensAttribute)
                        && !IsValidDtdNmTokensValue(fixedValue, isNmTokensAttribute)) {
                        ThrowMalformedDtdDeclaration("ATTLIST");
                    }
                    if (isNameAttribute && !IsValidDtdNamesValue(fixedValue, allowsMultipleNames)) {
                        ThrowMalformedDtdDeclaration("ATTLIST");
                    }
                    if (firstDeclaration) {
                        UpsertDtdValueDeclaration(defaultAttributes, elementName, attributeName, fixedValue);
                        UpsertDtdFixedDeclaration(fixedAttributes, elementName, attributeName, std::move(fixedValue));
                    }
                    continue;
                }

                ThrowMalformedDtdDeclaration("ATTLIST");
            }
            continue;
        }

        if (SubsetStartsWithAt(internalSubset, position, "<![")) {
            if (!allowConditionalSections) {
                ThrowMalformedDtdDeclaration("conditional section");
            }
            bool includeSection = false;
            std::string_view conditionalContent;
            if (!ReadSubsetConditionalSection(internalSubset, position, includeSection, conditionalContent)) {
                ThrowMalformedDtdDeclaration("conditional section");
            }
            if (includeSection) {
                ParseDocumentTypeAttributeDeclarationsCore(
                    conditionalContent,
                    declaredAttributes,
                    requiredAttributes,
                    defaultAttributes,
                    fixedAttributes,
                    enumeratedAttributes,
                    idAttributes,
                    notationAttributes,
                    nmTokenAttributes,
                    nameAttributes,
                    true);
            }
            continue;
        }

        if (!SkipSubsetDeclaration(internalSubset, position)) {
            ThrowMalformedDtdDeclaration("declaration");
        }
    }
}

void ParseDocumentTypeAttributeDeclarations(
    std::string_view internalSubset,
    DtdDeclaredAttributeNames& declaredAttributes,
    DtdRequiredAttributeDeclarations& requiredAttributes,
    DtdDefaultAttributeDeclarations& defaultAttributes,
    DtdFixedAttributeDeclarations& fixedAttributes,
    DtdEnumeratedAttributeDeclarations& enumeratedAttributes,
    DtdIdAttributeDeclarations& idAttributes,
    DtdNotationAttributeDeclarations& notationAttributes,
    DtdNmTokenAttributeDeclarations& nmTokenAttributes,
    DtdNameAttributeDeclarations& nameAttributes,
    bool allowConditionalSections = false) {
    ParseDocumentTypeAttributeDeclarationsCore(
        internalSubset,
        declaredAttributes,
        requiredAttributes,
        defaultAttributes,
        fixedAttributes,
        enumeratedAttributes,
        idAttributes,
        notationAttributes,
        nmTokenAttributes,
        nameAttributes,
        allowConditionalSections);
}

void ParseDocumentTypeEmptyElementDeclarationsCore(
    std::string_view internalSubset,
    DtdEmptyElementDeclarations& emptyElementDeclarations,
    bool allowConditionalSections = false) {
    std::size_t position = 0;
    while (position < internalSubset.size()) {
        SkipSubsetWhitespace(internalSubset, position);
        if (position >= internalSubset.size()) {
            break;
        }

        if (StartsWithAt(internalSubset, position, "<!--")) {
            const auto end = internalSubset.find("-->", position + 4);
            position = end == std::string_view::npos ? internalSubset.size() : end + 3;
            continue;
        }

        if (StartsWithAt(internalSubset, position, "<?")) {
            const auto end = internalSubset.find("?>", position + 2);
            position = end == std::string_view::npos ? internalSubset.size() : end + 2;
            continue;
        }

        if (SubsetStartsWithAt(internalSubset, position, "<!ELEMENT")) {
            position += 9;
            if (!ConsumeRequiredSubsetWhitespace(internalSubset, position)) {
                ThrowMalformedDtdDeclaration("ELEMENT");
            }
            const auto elementName = ReadSubsetName(internalSubset, position);
            if (elementName.empty()) {
                ThrowMalformedDtdDeclaration("ELEMENT");
            }

            if (!ConsumeRequiredSubsetWhitespace(internalSubset, position)) {
                ThrowMalformedDtdDeclaration("ELEMENT");
            }

            std::size_t contentSpecPosition = position;
            const bool isEmptyElement = ParseDtdElementContentSpec(internalSubset, contentSpecPosition);
            SkipSubsetWhitespace(internalSubset, contentSpecPosition);
            if (contentSpecPosition >= internalSubset.size() || internalSubset[contentSpecPosition] != '>') {
                ThrowMalformedDtdDeclaration("ELEMENT");
            }
            position = contentSpecPosition + 1;
            if (isEmptyElement) {
                emptyElementDeclarations.insert(elementName);
            }
            continue;
        }

        if (SubsetStartsWithAt(internalSubset, position, "<![")) {
            if (!allowConditionalSections) {
                ThrowMalformedDtdDeclaration("conditional section");
            }
            bool includeSection = false;
            std::string_view conditionalContent;
            if (!ReadSubsetConditionalSection(internalSubset, position, includeSection, conditionalContent)) {
                ThrowMalformedDtdDeclaration("conditional section");
            }
            if (includeSection) {
                ParseDocumentTypeEmptyElementDeclarationsCore(conditionalContent, emptyElementDeclarations, true);
            }
            continue;
        }

        if (!SkipSubsetDeclaration(internalSubset, position)) {
            ThrowMalformedDtdDeclaration("declaration");
        }
    }
}

void ParseDocumentTypeEmptyElementDeclarations(
    std::string_view internalSubset,
    DtdEmptyElementDeclarations& emptyElementDeclarations,
    bool allowConditionalSections = false) {
    ParseDocumentTypeEmptyElementDeclarationsCore(internalSubset, emptyElementDeclarations, allowConditionalSections);
}

void ParseDocumentTypeElementDeclarationsCore(
    std::string_view internalSubset,
    DtdElementContentDeclarations& elementContentDeclarations,
    bool allowConditionalSections = false) {
    std::size_t position = 0;
    while (position < internalSubset.size()) {
        SkipSubsetWhitespace(internalSubset, position);
        if (position >= internalSubset.size()) {
            break;
        }

        if (StartsWithAt(internalSubset, position, "<!--")) {
            const auto end = internalSubset.find("-->", position + 4);
            position = end == std::string_view::npos ? internalSubset.size() : end + 3;
            continue;
        }

        if (StartsWithAt(internalSubset, position, "<?")) {
            const auto end = internalSubset.find("?>", position + 2);
            position = end == std::string_view::npos ? internalSubset.size() : end + 2;
            continue;
        }

        if (SubsetStartsWithAt(internalSubset, position, "<!ELEMENT")) {
            position += 9;
            if (!ConsumeRequiredSubsetWhitespace(internalSubset, position)) {
                ThrowMalformedDtdDeclaration("ELEMENT");
            }
            const auto elementName = ReadSubsetName(internalSubset, position);
            if (elementName.empty()) {
                ThrowMalformedDtdDeclaration("ELEMENT");
            }

            if (!ConsumeRequiredSubsetWhitespace(internalSubset, position)) {
                ThrowMalformedDtdDeclaration("ELEMENT");
            }

            const std::size_t contentSpecStart = position;
            std::size_t contentSpecPosition = position;
            ParseDtdElementContentSpec(internalSubset, contentSpecPosition);
            const std::string contentSpec = Trim(std::string(
                internalSubset.substr(contentSpecStart, contentSpecPosition - contentSpecStart)));
            SkipSubsetWhitespace(internalSubset, contentSpecPosition);
            if (contentSpecPosition >= internalSubset.size() || internalSubset[contentSpecPosition] != '>') {
                ThrowMalformedDtdDeclaration("ELEMENT");
            }
            position = contentSpecPosition + 1;
            if (!elementContentDeclarations.emplace(std::string(elementName), contentSpec).second) {
                throw XmlException(
                    "DTD validation failed: duplicate ELEMENT declaration for '" + std::string(elementName) + "'");
            }
            continue;
        }

        if (SubsetStartsWithAt(internalSubset, position, "<![")) {
            if (!allowConditionalSections) {
                ThrowMalformedDtdDeclaration("conditional section");
            }
            bool includeSection = false;
            std::string_view conditionalContent;
            if (!ReadSubsetConditionalSection(internalSubset, position, includeSection, conditionalContent)) {
                ThrowMalformedDtdDeclaration("conditional section");
            }
            if (includeSection) {
                ParseDocumentTypeElementDeclarationsCore(conditionalContent, elementContentDeclarations, true);
            }
            continue;
        }

        if (!SkipSubsetDeclaration(internalSubset, position)) {
            ThrowMalformedDtdDeclaration("declaration");
        }
    }
}

void ParseDocumentTypeElementDeclarations(
    std::string_view internalSubset,
    DtdElementContentDeclarations& elementContentDeclarations,
    bool allowConditionalSections = false) {
    ParseDocumentTypeElementDeclarationsCore(internalSubset, elementContentDeclarations, allowConditionalSections);
}

void ValidateDtdEntityReplacementTextLiteral(
    std::string_view replacementText,
    bool allowParameterEntityReferences);

void ValidateDtdAttributeValueEntityReferenceOrder(
    std::string_view value,
    const std::unordered_set<std::string>& declaredEntityNames) {
    std::size_t cursor = 0;
    while (cursor < value.size()) {
        const auto ampersand = value.find('&', cursor);
        if (ampersand == std::string_view::npos) {
            return;
        }

        const auto semicolon = value.find(';', ampersand + 1);
        if (semicolon == std::string_view::npos) {
            throw XmlException("DTD validation failed: Unterminated entity reference");
        }

        const std::string_view entity = value.substr(ampersand + 1, semicolon - ampersand - 1);
        if (entity.empty()) {
            throw XmlException("DTD validation failed: Unterminated entity reference");
        }

        if (entity.front() == '#') {
            const bool isHex = entity.size() > 2 && (entity[1] == 'x' || entity[1] == 'X');
            const std::string_view digits = entity.substr(isHex ? 2 : 1);
            if (digits.empty()) {
                throw XmlException("DTD validation failed: Invalid numeric entity reference: &" + std::string(entity) + ';');
            }

            for (char digit : digits) {
                const bool isDecimalDigit = digit >= '0' && digit <= '9';
                const bool isHexDigit = isHex
                    && ((digit >= 'a' && digit <= 'f') || (digit >= 'A' && digit <= 'F'));
                if (!isDecimalDigit && !isHexDigit) {
                    throw XmlException("DTD validation failed: Invalid numeric entity reference: &" + std::string(entity) + ';');
                }
            }
        } else if (entity != "lt"
            && entity != "gt"
            && entity != "amp"
            && entity != "quot"
            && entity != "apos") {
            const std::string entityName(entity);
            if (declaredEntityNames.find(entityName) == declaredEntityNames.end()) {
                throw XmlException("DTD validation failed: Unknown entity reference: &" + entityName + ';');
            }
        }

        cursor = semicolon + 1;
    }
}

void ValidateDtdDeclaredAttributeValueReferenceOrder(
    std::string_view internalSubset,
    bool allowConditionalSections = false) {
    std::unordered_set<std::string> declaredEntityNames;
    std::size_t position = 0;

    while (position < internalSubset.size()) {
        SkipSubsetWhitespace(internalSubset, position);
        if (position >= internalSubset.size()) {
            break;
        }

        if (StartsWithAt(internalSubset, position, "<!--")) {
            const auto end = internalSubset.find("-->", position + 4);
            position = end == std::string_view::npos ? internalSubset.size() : end + 3;
            continue;
        }

        if (StartsWithAt(internalSubset, position, "<?")) {
            const auto end = internalSubset.find("?>", position + 2);
            position = end == std::string_view::npos ? internalSubset.size() : end + 2;
            continue;
        }

        if (position < internalSubset.size() && internalSubset[position] == '%') {
            std::size_t referencePosition = position + 1;
            const auto parameterEntityName = ReadSubsetName(internalSubset, referencePosition);
            if (!parameterEntityName.empty()
                && referencePosition < internalSubset.size()
                && internalSubset[referencePosition] == ';') {
                position = referencePosition + 1;
                continue;
            }
        }

        if (!SubsetStartsWithAt(internalSubset, position, "<!")) {
            ++position;
            continue;
        }

        if (SubsetStartsWithAt(internalSubset, position, "<!ENTITY")) {
            std::size_t entityPosition = position + 8;
            if (!ConsumeRequiredSubsetWhitespace(internalSubset, entityPosition)) {
                break;
            }

            const bool parameterEntity = entityPosition < internalSubset.size() && internalSubset[entityPosition] == '%';
            if (parameterEntity) {
                ++entityPosition;
                if (!ConsumeRequiredSubsetWhitespace(internalSubset, entityPosition)) {
                    break;
                }
            }

            const auto name = ReadSubsetName(internalSubset, entityPosition);
            if (name.empty()) {
                break;
            }

            std::size_t declarationEnd = position;
            if (!SkipSubsetDeclaration(internalSubset, declarationEnd)) {
                break;
            }

            position = declarationEnd;
            if (!parameterEntity) {
                declaredEntityNames.insert(name);
            }
            continue;
        }

        if (SubsetStartsWithAt(internalSubset, position, "<!ATTLIST")) {
            std::size_t attlistPosition = position + 9;
            if (!ConsumeRequiredSubsetWhitespace(internalSubset, attlistPosition)) {
                break;
            }

            const auto elementName = ReadSubsetName(internalSubset, attlistPosition);
            if (elementName.empty()) {
                break;
            }

            while (true) {
                const bool hasSeparator = ConsumeRequiredSubsetWhitespace(internalSubset, attlistPosition);
                if (attlistPosition >= internalSubset.size()) {
                    break;
                }
                if (internalSubset[attlistPosition] == '>') {
                    ++attlistPosition;
                    break;
                }
                if (!hasSeparator) {
                    break;
                }

                const auto attributeName = ReadSubsetName(internalSubset, attlistPosition);
                if (attributeName.empty()) {
                    break;
                }
                (void)attributeName;

                if (!ConsumeRequiredSubsetWhitespace(internalSubset, attlistPosition)) {
                    break;
                }

                auto attributeType = ReadSubsetAttlistToken(internalSubset, attlistPosition);
                if (!attributeType.has_value()) {
                    break;
                }
                if (*attributeType == "NOTATION") {
                    auto notationValues = ReadSubsetAttlistToken(internalSubset, attlistPosition);
                    if (!notationValues.has_value()) {
                        break;
                    }
                }

                if (!ConsumeRequiredSubsetWhitespace(internalSubset, attlistPosition)
                    || attlistPosition >= internalSubset.size()) {
                    break;
                }

                if (internalSubset[attlistPosition] == '\'' || internalSubset[attlistPosition] == '"') {
                    const std::string defaultValue = ReadSubsetQuotedValue(internalSubset, attlistPosition);
                    ValidateDtdAttributeValueEntityReferenceOrder(defaultValue, declaredEntityNames);
                    continue;
                }

                auto defaultDeclaration = ReadSubsetAttlistToken(internalSubset, attlistPosition);
                if (!defaultDeclaration.has_value()) {
                    break;
                }

                if (*defaultDeclaration == "#FIXED") {
                    if (!ConsumeRequiredSubsetWhitespace(internalSubset, attlistPosition)
                        || attlistPosition >= internalSubset.size()
                        || (internalSubset[attlistPosition] != '\'' && internalSubset[attlistPosition] != '"')) {
                        break;
                    }
                    const std::string fixedValue = ReadSubsetQuotedValue(internalSubset, attlistPosition);
                    ValidateDtdAttributeValueEntityReferenceOrder(fixedValue, declaredEntityNames);
                }
            }

            position = attlistPosition;
            continue;
        }

        if (SubsetStartsWithAt(internalSubset, position, "<![")) {
            if (!allowConditionalSections) {
                ++position;
                continue;
            }

            bool includeSection = false;
            std::string_view conditionalContent;
            if (!ReadSubsetConditionalSection(internalSubset, position, includeSection, conditionalContent)) {
                break;
            }
            if (includeSection) {
                ValidateDtdDeclaredAttributeValueReferenceOrder(conditionalContent, true);
            }
            continue;
        }

        if (!SkipSubsetDeclaration(internalSubset, position)) {
            break;
        }
    }
}

void ValidateDtdDeclaredAttributeValues(
    const DtdDefaultAttributeDeclarations& defaultAttributes,
    const DtdFixedAttributeDeclarations& fixedAttributes,
    const std::unordered_map<std::string, std::string>& internalEntityDeclarations,
    const std::unordered_map<std::string, std::string>& externalEntitySystemIds = {}) {
    const auto validateValue = [&internalEntityDeclarations, &externalEntitySystemIds](std::string_view value) {
        const auto expandValue = [&internalEntityDeclarations, &externalEntitySystemIds](auto&& self, std::string_view rawValue, std::vector<std::string>& stack) -> std::string {
            std::string expanded;
            std::size_t cursor = 0;
            while (cursor < rawValue.size()) {
                const auto ampersand = rawValue.find('&', cursor);
                if (ampersand == std::string_view::npos) {
                    expanded.append(rawValue.data() + cursor, rawValue.size() - cursor);
                    break;
                }

                if (ampersand > cursor) {
                    expanded.append(rawValue.data() + cursor, ampersand - cursor);
                }

                const auto semicolon = rawValue.find(';', ampersand + 1);
                if (semicolon == std::string_view::npos) {
                    throw XmlException("DTD validation failed: Unterminated entity reference");
                }

                const std::string_view entity = rawValue.substr(ampersand + 1, semicolon - ampersand - 1);
                if (entity == "lt") {
                    expanded.push_back('<');
                } else if (entity == "gt") {
                    expanded.push_back('>');
                } else if (entity == "amp") {
                    expanded.push_back('&');
                } else if (entity == "quot") {
                    expanded.push_back('"');
                } else if (entity == "apos") {
                    expanded.push_back('\'');
                } else if (!entity.empty() && entity.front() == '#') {
                    const bool isHex = entity.size() > 2 && (entity[1] == 'x' || entity[1] == 'X');
                    const std::string digits(entity.substr(isHex ? 2 : 1));
                    if (digits.empty()) {
                        throw XmlException("DTD validation failed: Invalid numeric entity reference: &" + std::string(entity) + ';');
                    }
                    unsigned int codePoint = 0;
                    for (char digit : digits) {
                        unsigned int value = 0;
                        if (digit >= '0' && digit <= '9') {
                            value = static_cast<unsigned int>(digit - '0');
                        } else if (isHex && digit >= 'a' && digit <= 'f') {
                            value = 10u + static_cast<unsigned int>(digit - 'a');
                        } else if (isHex && digit >= 'A' && digit <= 'F') {
                            value = 10u + static_cast<unsigned int>(digit - 'A');
                        } else {
                            throw XmlException("DTD validation failed: Invalid numeric entity reference: &" + std::string(entity) + ';');
                        }

                        codePoint = codePoint * (isHex ? 16u : 10u) + value;
                    }

                    if (codePoint > 0x10FFFFu
                        || (codePoint >= 0xD800u && codePoint <= 0xDFFFu)
                        || codePoint == 0u
                        || (codePoint <= 0x7Fu && !XmlConvert::IsXmlChar(static_cast<char>(codePoint)))) {
                        throw XmlException("DTD validation failed: Invalid numeric entity reference: &" + std::string(entity) + ';');
                    }

                    if (codePoint <= 0x7Fu) {
                        expanded.push_back(static_cast<char>(codePoint));
                    } else if (codePoint <= 0x7FFu) {
                        expanded.push_back(static_cast<char>(0xC0u | (codePoint >> 6)));
                        expanded.push_back(static_cast<char>(0x80u | (codePoint & 0x3Fu)));
                    } else if (codePoint <= 0xFFFFu) {
                        expanded.push_back(static_cast<char>(0xE0u | (codePoint >> 12)));
                        expanded.push_back(static_cast<char>(0x80u | ((codePoint >> 6) & 0x3Fu)));
                        expanded.push_back(static_cast<char>(0x80u | (codePoint & 0x3Fu)));
                    } else {
                        expanded.push_back(static_cast<char>(0xF0u | (codePoint >> 18)));
                        expanded.push_back(static_cast<char>(0x80u | ((codePoint >> 12) & 0x3Fu)));
                        expanded.push_back(static_cast<char>(0x80u | ((codePoint >> 6) & 0x3Fu)));
                        expanded.push_back(static_cast<char>(0x80u | (codePoint & 0x3Fu)));
                    }
                } else {
                    const std::string entityName(entity);
                    if (externalEntitySystemIds.find(entityName) != externalEntitySystemIds.end()) {
                        throw XmlException("DTD validation failed: Unknown entity reference: &" + entityName + ';');
                    }

                    const auto found = internalEntityDeclarations.find(entityName);
                    if (found == internalEntityDeclarations.end()) {
                        throw XmlException("DTD validation failed: Unknown entity reference: &" + entityName + ';');
                    }
                    if (std::find(stack.begin(), stack.end(), entityName) != stack.end()) {
                        throw XmlException("DTD validation failed: Entity reference cycle detected: &" + entityName + ';');
                    }

                    stack.push_back(entityName);
                    expanded += self(self, found->second, stack);
                    stack.pop_back();
                }

                cursor = semicolon + 1;
            }

            return expanded;
        };

        std::vector<std::string> stack;
        const std::string expanded = expandValue(expandValue, value, stack);

        for (char ch : expanded) {
            if (ch == '<') {
                throw XmlException("DTD validation failed: attribute default value must not contain '<'");
            }
            if (!XmlConvert::IsXmlChar(ch)) {
                throw XmlException("DTD validation failed: attribute default value contains an invalid XML character");
            }
        }
    };

    for (const auto& [elementName, attributes] : defaultAttributes) {
        (void)elementName;
        for (const auto& [attributeName, value] : attributes) {
            (void)attributeName;
            validateValue(value);
        }
    }

    for (const auto& [elementName, attributes] : fixedAttributes) {
        (void)elementName;
        for (const auto& [attributeName, value] : attributes) {
            (void)attributeName;
            validateValue(value);
        }
    }
}

bool DeclarationContainsParameterEntityReference(
    std::string_view text,
    std::size_t start,
    std::size_t end) {
    char quote = '\0';
    for (std::size_t position = start; position < end; ++position) {
        if (quote != '\0') {
            if (text[position] == quote) {
                quote = '\0';
            }
            continue;
        }

        if (text[position] == '\'' || text[position] == '"') {
            quote = text[position];
            continue;
        }

        if (text[position] == '%') {
            std::string name;
            std::size_t referenceEnd = position;
            if (TryParseParameterEntityReferenceAt(text, position, name, referenceEnd)) {
                return true;
            }
        }
    }

    return false;
}

int ComputeDtdParenthesisBalance(std::string_view text) {
    char quote = '\0';
    int balance = 0;
    for (const char ch : text) {
        if (quote != '\0') {
            if (ch == quote) {
                quote = '\0';
            }
            continue;
        }

        if (ch == '\'' || ch == '"') {
            quote = ch;
            continue;
        }

        if (ch == '(') {
            ++balance;
        } else if (ch == ')') {
            --balance;
        }
    }

    return balance;
}

void ValidateElementDeclarationParameterEntityBoundaries(
    std::string_view text,
    const std::unordered_map<std::string, std::string>& declarations,
    const std::unordered_set<std::string>* externalParameterEntityNames = nullptr) {
    std::size_t position = 0;
    while (position < text.size()) {
        SkipSubsetWhitespace(text, position);
        if (position >= text.size()) {
            break;
        }

        if (StartsWithAt(text, position, "<!--")) {
            const auto end = text.find("-->", position + 4);
            position = end == std::string_view::npos ? text.size() : end + 3;
            continue;
        }

        if (StartsWithAt(text, position, "<?")) {
            const auto end = text.find("?>", position + 2);
            position = end == std::string_view::npos ? text.size() : end + 2;
            continue;
        }

        if (!SubsetStartsWithAt(text, position, "<!ELEMENT")) {
            const auto end = SubsetStartsWithAt(text, position, "<!")
                ? ScanSubsetMarkupEndRespectingQuotes(text, position)
                : std::string_view::npos;
            if (end != std::string_view::npos) {
                position = end;
                continue;
            }

            ++position;
            continue;
        }

        const auto declarationEnd = ScanSubsetMarkupEndRespectingQuotes(text, position);
        if (declarationEnd == std::string_view::npos) {
            ThrowMalformedDtdDeclaration("ELEMENT");
        }

        for (std::size_t probe = position; probe < declarationEnd; ++probe) {
            if (text[probe] != '%') {
                continue;
            }

            std::string name;
            std::size_t referenceEnd = probe;
            if (!TryParseParameterEntityReferenceAt(text, probe, name, referenceEnd)) {
                continue;
            }

            const auto found = declarations.find(name);
            if (found == declarations.end()) {
                ThrowMalformedDtdDeclaration("ELEMENT");
            }

            std::vector<std::string> expansionStack{ name };
            const bool referenceAllowsConditionalSections = externalParameterEntityNames != nullptr
                && externalParameterEntityNames->find(name) != externalParameterEntityNames->end();
            const std::string expanded = ExpandDtdParameterEntityReferencesRecursive(
                found->second,
                declarations,
                expansionStack,
                referenceAllowsConditionalSections,
                externalParameterEntityNames);
            if (ComputeDtdParenthesisBalance(expanded) != 0) {
                ThrowMalformedDtdDeclaration("ELEMENT");
            }

            probe = referenceEnd - 1;
        }

        position = declarationEnd;
    }
}

void ValidateInternalSubsetParameterEntityPlacement(std::string_view text) {
    std::size_t position = 0;
    while (position < text.size()) {
        SkipSubsetWhitespace(text, position);
        if (position >= text.size()) {
            break;
        }

        if (StartsWithAt(text, position, "<!--")) {
            const auto end = text.find("-->", position + 4);
            position = end == std::string_view::npos ? text.size() : end + 3;
            continue;
        }

        if (StartsWithAt(text, position, "<?")) {
            const auto end = text.find("?>", position + 2);
            position = end == std::string_view::npos ? text.size() : end + 2;
            continue;
        }

        const bool inspectDeclaration = SubsetStartsWithAt(text, position, "<!ELEMENT")
            || SubsetStartsWithAt(text, position, "<!ATTLIST")
            || SubsetStartsWithAt(text, position, "<!NOTATION");
        if (inspectDeclaration) {
            const auto end = ScanSubsetMarkupEndRespectingQuotes(text, position);
            if (end == std::string_view::npos) {
                ThrowMalformedDtdDeclaration("declaration");
            }
            if (DeclarationContainsParameterEntityReference(text, position, end)) {
                ThrowMalformedDtdDeclaration("declaration");
            }
            position = end;
            continue;
        }

        if (SubsetStartsWithAt(text, position, "<![")) {
            ThrowMalformedDtdDeclaration("conditional section");
        }

        if (SubsetStartsWithAt(text, position, "<!")) {
            const auto end = ScanSubsetMarkupEndRespectingQuotes(text, position);
            position = end == std::string_view::npos ? text.size() : end;
            continue;
        }

        if (text[position] == '%') {
            std::string name;
            std::size_t referenceEnd = position;
            if (TryParseParameterEntityReferenceAt(text, position, name, referenceEnd)) {
                position = referenceEnd;
                continue;
            }
        }

        ++position;
    }
}

void ValidateTopLevelDtdDeclarationFragment(
    std::string_view text,
    bool allowConditionalSections) {
    std::size_t position = 0;
    while (position < text.size()) {
        SkipSubsetWhitespace(text, position);
        if (position >= text.size()) {
            break;
        }

        if (StartsWithAt(text, position, "<!--")) {
            const auto end = text.find("-->", position + 4);
            if (end == std::string_view::npos) {
                ThrowMalformedDtdDeclaration("declaration");
            }
            position = end + 3;
            continue;
        }

        if (StartsWithAt(text, position, "<?")) {
            const auto end = text.find("?>", position + 2);
            if (end == std::string_view::npos) {
                ThrowMalformedDtdDeclaration("declaration");
            }
            position = end + 2;
            continue;
        }

        if (SubsetStartsWithAt(text, position, "<![")) {
            if (!allowConditionalSections) {
                ThrowMalformedDtdDeclaration("conditional section");
            }

            bool includeSection = false;
            std::string_view conditionalContent;
            if (!ReadSubsetConditionalSection(text, position, includeSection, conditionalContent)) {
                ThrowMalformedDtdDeclaration("conditional section");
            }
            if (includeSection) {
                ValidateTopLevelDtdDeclarationFragment(conditionalContent, true);
            }
            continue;
        }

        if (SubsetStartsWithAt(text, position, "<!")) {
            const auto end = ScanSubsetMarkupEndRespectingQuotes(text, position);
            if (end == std::string_view::npos) {
                ThrowMalformedDtdDeclaration("declaration");
            }
            position = end;
            continue;
        }

        ThrowMalformedDtdDeclaration("declaration");
    }
}

void ValidateTopLevelParameterEntityReferenceBoundaries(
    std::string_view text,
    const std::unordered_map<std::string, std::string>& declarations,
    bool allowConditionalSections = false,
    const std::unordered_set<std::string>* externalParameterEntityNames = nullptr) {
    std::size_t position = 0;
    while (position < text.size()) {
        SkipSubsetWhitespace(text, position);
        if (position >= text.size()) {
            break;
        }

        if (StartsWithAt(text, position, "<!--")) {
            const auto end = text.find("-->", position + 4);
            if (end == std::string_view::npos) {
                ThrowMalformedDtdDeclaration("declaration");
            }
            position = end + 3;
            continue;
        }

        if (StartsWithAt(text, position, "<?")) {
            const auto end = text.find("?>", position + 2);
            if (end == std::string_view::npos) {
                ThrowMalformedDtdDeclaration("declaration");
            }
            position = end + 2;
            continue;
        }

        if (SubsetStartsWithAt(text, position, "<![")) {
            if (!allowConditionalSections) {
                ThrowMalformedDtdDeclaration("conditional section");
            }

            bool includeSection = false;
            std::string_view conditionalContent;
            if (!ReadSubsetConditionalSection(text, position, includeSection, conditionalContent)) {
                ThrowMalformedDtdDeclaration("conditional section");
            }
            if (includeSection) {
                ValidateTopLevelParameterEntityReferenceBoundaries(
                    conditionalContent,
                    declarations,
                    true,
                    externalParameterEntityNames);
            }
            continue;
        }

        if (SubsetStartsWithAt(text, position, "<!")) {
            const auto end = ScanSubsetMarkupEndRespectingQuotes(text, position);
            if (end == std::string_view::npos) {
                ThrowMalformedDtdDeclaration("declaration");
            }
            position = end;
            continue;
        }

        if (text[position] == '%') {
            std::string name;
            std::size_t referenceEnd = position;
            if (!TryParseParameterEntityReferenceAt(text, position, name, referenceEnd)) {
                ThrowMalformedDtdDeclaration("declaration");
            }

            const auto found = declarations.find(name);
            if (found == declarations.end()) {
                ThrowMalformedDtdDeclaration("declaration");
            }

            std::vector<std::string> expansionStack{ name };
            const bool referenceAllowsConditionalSections = allowConditionalSections
                || (externalParameterEntityNames != nullptr
                    && externalParameterEntityNames->find(name) != externalParameterEntityNames->end());
            const std::string expanded = ExpandDtdParameterEntityReferencesRecursive(
                found->second,
                declarations,
                expansionStack,
                referenceAllowsConditionalSections,
                externalParameterEntityNames);
            ValidateTopLevelDtdDeclarationFragment(expanded, referenceAllowsConditionalSections);
            position = referenceEnd;
            continue;
        }

        ThrowMalformedDtdDeclaration("declaration");
    }
}

void ValidateDtdEntityReplacementTextLiteral(
    std::string_view replacementText,
    bool allowParameterEntityReferences) {
    for (std::size_t position = 0; position < replacementText.size();) {
        std::uint32_t codePoint = 0;
        std::size_t width = 0;
        if (!DecodeUtf8CodePointAt(position, [&replacementText](std::size_t index) noexcept {
                return index < replacementText.size() ? replacementText[index] : '\0';
            }, codePoint, width)
            || !IsValidXmlCharacterCodePoint(codePoint)) {
            throw XmlException("Malformed DTD entity declaration");
        }
        position += width;
    }

    std::size_t percentPosition = 0;
    while ((percentPosition = replacementText.find('%', percentPosition)) != std::string_view::npos) {
        std::size_t namePosition = percentPosition + 1;
        const auto parameterEntityName = ReadSubsetName(replacementText, namePosition);
        const bool isParameterEntityReference = !parameterEntityName.empty()
            && namePosition < replacementText.size()
            && replacementText[namePosition] == ';';
        if (!isParameterEntityReference || !allowParameterEntityReferences) {
            throw XmlException("Malformed DTD entity declaration");
        }
        percentPosition = namePosition + 1;
    }

    std::size_t cursor = 0;
    while (cursor < replacementText.size()) {
        const auto ampersand = replacementText.find('&', cursor);
        if (ampersand == std::string_view::npos) {
            break;
        }

        const auto semicolon = replacementText.find(';', ampersand + 1);
        if (semicolon == std::string_view::npos) {
            throw XmlException("Malformed DTD entity declaration");
        }

        const std::string_view entity = replacementText.substr(ampersand + 1, semicolon - ampersand - 1);
        if (entity.empty()) {
            throw XmlException("Malformed DTD entity declaration");
        }

        if (entity.front() == '#') {
            const bool isHex = entity.size() > 2 && entity[1] == 'x';
            const std::string_view digits = entity.substr(isHex ? 2 : 1);
            if (digits.empty()) {
                throw XmlException("Malformed DTD entity declaration");
            }

            for (char digit : digits) {
                const bool isDecimalDigit = digit >= '0' && digit <= '9';
                const bool isHexDigit = isHex
                    && ((digit >= 'a' && digit <= 'f') || (digit >= 'A' && digit <= 'F'));
                if (!isDecimalDigit && !isHexDigit) {
                    throw XmlException("Malformed DTD entity declaration");
                }
            }
        } else if (entity != "lt"
            && entity != "gt"
            && entity != "amp"
            && entity != "quot"
            && entity != "apos") {
            try {
                (void)XmlConvert::VerifyName(entity);
            } catch (const XmlException&) {
                throw XmlException("Malformed DTD entity declaration");
            }
        }

        cursor = semicolon + 1;
    }
}


}  // namespace

class XmlReaderInputSource {
public:
    virtual ~XmlReaderInputSource() = default;

    virtual char CharAt(std::size_t position) const noexcept = 0;
    virtual const char* PtrAt(std::size_t position, std::size_t& available) const noexcept = 0;
    virtual std::size_t Find(std::string_view token, std::size_t position) const noexcept = 0;
    virtual std::string Slice(std::size_t start, std::size_t count) const = 0;
    virtual void AppendSliceTo(std::string& target, std::size_t start, std::size_t count) const = 0;
    virtual void EnableReplay() const {}
    virtual void DiscardBefore(std::size_t) const {}
};

class StringXmlReaderInputSource final : public XmlReaderInputSource {
public:
    explicit StringXmlReaderInputSource(std::shared_ptr<const std::string> text);

    const std::shared_ptr<const std::string>& Text() const noexcept;

    char CharAt(std::size_t position) const noexcept override;
    const char* PtrAt(std::size_t position, std::size_t& available) const noexcept override;
    std::size_t Find(std::string_view token, std::size_t position) const noexcept override;
    std::size_t FindFirstOf(std::string_view tokens, std::size_t position) const noexcept;
    std::size_t ScanQuotedValueEnd(std::size_t quoteStart) const noexcept;
    std::string Slice(std::size_t start, std::size_t count) const override;
    void AppendSliceTo(std::string& target, std::size_t start, std::size_t count) const override;

private:
    std::shared_ptr<const std::string> text_;
};

}  // namespace System::Xml
