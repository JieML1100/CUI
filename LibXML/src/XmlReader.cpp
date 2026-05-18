#include "../include/XmlInternal.h"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace System::Xml {

namespace {

bool IsReservedXmlProcessingTarget(std::string_view target) noexcept {
    return target.size() == 3
        && (target[0] == 'x' || target[0] == 'X')
        && (target[1] == 'm' || target[1] == 'M')
        && (target[2] == 'l' || target[2] == 'L');
}

bool HasUtf16BigEndianBom(std::string_view text) noexcept {
    return text.size() >= 2
        && static_cast<unsigned char>(text[0]) == 0xFE
        && static_cast<unsigned char>(text[1]) == 0xFF;
}

bool HasUtf16LittleEndianBom(std::string_view text) noexcept {
    return text.size() >= 2
        && static_cast<unsigned char>(text[0]) == 0xFF
        && static_cast<unsigned char>(text[1]) == 0xFE;
}

bool HasUtf16Bom(std::string_view text) noexcept {
    return HasUtf16BigEndianBom(text) || HasUtf16LittleEndianBom(text);
}

bool LooksLikeMalformedUtf16XmlPrefix(std::string_view text) noexcept {
    if (text.size() < 8 || HasUtf16Bom(text)) {
        return false;
    }

    const auto first = static_cast<unsigned char>(text[0]);
    if (first == 0xFF) {
        return static_cast<unsigned char>(text[1]) == 0x00
            && static_cast<unsigned char>(text[2]) == '<'
            && static_cast<unsigned char>(text[3]) == 0x00
            && static_cast<unsigned char>(text[4]) == '?'
            && static_cast<unsigned char>(text[5]) == 0x00
            && static_cast<unsigned char>(text[6]) == 'x'
            && static_cast<unsigned char>(text[7]) == 0x00;
    }

    if (first == 0xFE) {
        return static_cast<unsigned char>(text[1]) == '<'
            && static_cast<unsigned char>(text[2]) == 0x00
            && static_cast<unsigned char>(text[3]) == '?'
            && static_cast<unsigned char>(text[4]) == 0x00
            && static_cast<unsigned char>(text[5]) == 'x'
            && static_cast<unsigned char>(text[6]) == 0x00
            && static_cast<unsigned char>(text[7]) == 'm';
    }

    return false;
}

bool IsXmlWhitespaceCodePoint(unsigned int codePoint) noexcept {
    return codePoint == 0x20 || codePoint == 0x09 || codePoint == 0x0A || codePoint == 0x0D;
}

bool TryClassifyElementContentTextValue(
    std::string_view value,
    bool& isWhitespaceOnly,
    bool& matchesElementContentWhitespace) {
    isWhitespaceOnly = true;
    matchesElementContentWhitespace = true;

    for (std::size_t cursor = 0; cursor < value.size();) {
        if (value[cursor] != '&') {
            if (!IsWhitespace(value[cursor])) {
                isWhitespaceOnly = false;
                matchesElementContentWhitespace = false;
                return true;
            }

            ++cursor;
            continue;
        }

        const auto semicolon = value.find(';', cursor + 1);
        if (semicolon == std::string_view::npos) {
            return false;
        }

        const std::string_view entity = value.substr(cursor + 1, semicolon - cursor - 1);
        const auto predefined = ResolvePredefinedEntityReferenceValueView(entity);
        if (!predefined.empty()) {
            if (!IsWhitespaceOnly(predefined)) {
                isWhitespaceOnly = false;
                matchesElementContentWhitespace = false;
                return true;
            }
        } else if (!entity.empty() && entity.front() == '#') {
            unsigned int codePoint = 0;
            if (!TryParseNumericEntityReferenceCodePoint(entity, codePoint)) {
                return false;
            }

            if (!IsXmlWhitespaceCodePoint(codePoint)) {
                isWhitespaceOnly = false;
                matchesElementContentWhitespace = false;
                return true;
            }

            matchesElementContentWhitespace = false;
        } else {
            return false;
        }

        cursor = semicolon + 1;
    }

    return true;
}

char ToLowerAscii(char value) noexcept {
    static const char kToLowerAsciiTable[] =
        "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F"
        "\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F"
        "\x20\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2A\x2B\x2C\x2D\x2E\x2F"
        "\x30\x31\x32\x33\x34\x35\x36\x37\x38\x39\x3A\x3B\x3C\x3D\x3E\x3F"
        "\x40\x61\x62\x63\x64\x65\x66\x67\x68\x69\x6A\x6B\x6C\x6D\x6E\x6F"
        "\x70\x71\x72\x73\x74\x75\x76\x77\x78\x79\x7A\x5B\x5C\x5D\x5E\x5F"
        "\x60\x61\x62\x63\x64\x65\x66\x67\x68\x69\x6A\x6B\x6C\x6D\x6E\x6F"
        "\x70\x71\x72\x73\x74\x75\x76\x77\x78\x79\x7A\x7B\x7C\x7D\x7E\x7F"
        "\x80\x81\x82\x83\x84\x85\x86\x87\x88\x89\x8A\x8B\x8C\x8D\x8E\x8F"
        "\x90\x91\x92\x93\x94\x95\x96\x97\x98\x99\x9A\x9B\x9C\x9D\x9E\x9F"
        "\xA0\xA1\xA2\xA3\xA4\xA5\xA6\xA7\xA8\xA9\xAA\xAB\xAC\xAD\xAE\xAF"
        "\xB0\xB1\xB2\xB3\xB4\xB5\xB6\xB7\xB8\xB9\xBA\xBB\xBC\xBD\xBE\xBF"
        "\xC0\xC1\xC2\xC3\xC4\xC5\xC6\xC7\xC8\xC9\xCA\xCB\xCC\xCD\xCE\xCF"
        "\xD0\xD1\xD2\xD3\xD4\xD5\xD6\xD7\xD8\xD9\xDA\xDB\xDC\xDD\xDE\xDF"
        "\xE0\xE1\xE2\xE3\xE4\xE5\xE6\xE7\xE8\xE9\xEA\xEB\xEC\xED\xEE\xEF"
        "\xF0\xF1\xF2\xF3\xF4\xF5\xF6\xF7\xF8\xF9\xFA\xFB\xFC\xFD\xFE\xFF";
    return kToLowerAsciiTable[static_cast<unsigned char>(value)];
}

bool EqualsAsciiIgnoreCase(std::string_view left, std::string_view right) noexcept {
    if (left.size() != right.size()) {
        return false;
    }

    for (std::size_t index = 0; index < left.size(); ++index) {
        if (ToLowerAscii(left[index]) != ToLowerAscii(right[index])) {
            return false;
        }
    }

    return true;
}

bool IsUtf16EncodingDeclaration(std::string_view encoding) noexcept {
    return EqualsAsciiIgnoreCase(encoding, "utf-16")
        || EqualsAsciiIgnoreCase(encoding, "utf-16le")
        || EqualsAsciiIgnoreCase(encoding, "utf-16be");
}

bool IsUtf8EncodingDeclaration(std::string_view encoding) noexcept {
    return EqualsAsciiIgnoreCase(encoding, "utf-8");
}

enum class DtdContentModelNodeKind {
    Name,
    Sequence,
    Choice,
};

struct DtdContentModelNode {
    DtdContentModelNodeKind kind = DtdContentModelNodeKind::Name;
    std::string name;
    std::vector<DtdContentModelNode> children;
    char occurrence = '\0';
};

void CollectUniquePositions(
    std::vector<std::size_t>& target,
    const std::vector<std::size_t>& source) {
    for (const std::size_t position : source) {
        if (std::find(target.begin(), target.end(), position) == target.end()) {
            target.push_back(position);
        }
    }
}

void SkipDtdContentModelWhitespace(std::string_view text, std::size_t& position) {
    SkipSubsetWhitespace(text, position);
}

char ConsumeDtdContentModelOccurrence(std::string_view text, std::size_t& position) {
    if (position < text.size() && (text[position] == '?' || text[position] == '*' || text[position] == '+')) {
        return text[position++];
    }
    return '\0';
}

DtdContentModelNode ParseDtdContentModelParticle(std::string_view text, std::size_t& position);

DtdContentModelNode ParseDtdContentModelGroup(std::string_view text, std::size_t& position) {
    if (position >= text.size() || text[position] != '(') {
        throw XmlException("Malformed DTD content model");
    }

    ++position;
    SkipDtdContentModelWhitespace(text, position);
    DtdContentModelNode firstChild = ParseDtdContentModelParticle(text, position);
    SkipDtdContentModelWhitespace(text, position);

    if (position < text.size() && (text[position] == ',' || text[position] == '|')) {
        const char separator = text[position++];
        DtdContentModelNode node;
        node.kind = separator == ','
            ? DtdContentModelNodeKind::Sequence
            : DtdContentModelNodeKind::Choice;
        node.children.push_back(std::move(firstChild));
        while (true) {
            SkipDtdContentModelWhitespace(text, position);
            node.children.push_back(ParseDtdContentModelParticle(text, position));
            SkipDtdContentModelWhitespace(text, position);
            if (position >= text.size() || text[position] != separator) {
                break;
            }
            ++position;
        }
        if (position >= text.size() || text[position] != ')') {
            throw XmlException("Malformed DTD content model");
        }
        ++position;
        return node;
    }

    if (position >= text.size() || text[position] != ')') {
        throw XmlException("Malformed DTD content model");
    }
    ++position;

    DtdContentModelNode node;
    node.kind = DtdContentModelNodeKind::Sequence;
    node.children.push_back(std::move(firstChild));
    return node;
}

DtdContentModelNode ParseDtdContentModelParticle(std::string_view text, std::size_t& position) {
    SkipDtdContentModelWhitespace(text, position);

    DtdContentModelNode node;
    if (position < text.size() && text[position] == '(') {
        node = ParseDtdContentModelGroup(text, position);
    } else {
        const auto name = ReadSubsetName(text, position);
        if (name.empty()) {
            throw XmlException("Malformed DTD content model");
        }
        node.kind = DtdContentModelNodeKind::Name;
        node.name = std::string(name);
    }

    SkipDtdContentModelWhitespace(text, position);
    const char occurrence = ConsumeDtdContentModelOccurrence(text, position);
    if (occurrence != '\0') {
        node.occurrence = occurrence;
    }
    return node;
}

std::vector<std::size_t> MatchDtdContentModelNode(
    const DtdContentModelNode& node,
    const std::vector<std::string>& observedChildren,
    std::size_t position);

std::vector<std::size_t> MatchDtdContentModelNodeBase(
    const DtdContentModelNode& node,
    const std::vector<std::string>& observedChildren,
    std::size_t position) {
    switch (node.kind) {
    case DtdContentModelNodeKind::Name:
        if (position < observedChildren.size() && observedChildren[position] == node.name) {
            return {position + 1};
        }
        return {};
    case DtdContentModelNodeKind::Sequence: {
        std::vector<std::size_t> positions = {position};
        for (const auto& child : node.children) {
            std::vector<std::size_t> nextPositions;
            for (const std::size_t candidate : positions) {
                CollectUniquePositions(nextPositions, MatchDtdContentModelNode(child, observedChildren, candidate));
            }
            positions = std::move(nextPositions);
            if (positions.empty()) {
                break;
            }
        }
        return positions;
    }
    case DtdContentModelNodeKind::Choice: {
        std::vector<std::size_t> positions;
        for (const auto& child : node.children) {
            CollectUniquePositions(positions, MatchDtdContentModelNode(child, observedChildren, position));
        }
        return positions;
    }
    }

    return {};
}

std::vector<std::size_t> MatchDtdContentModelNode(
    const DtdContentModelNode& node,
    const std::vector<std::string>& observedChildren,
    std::size_t position) {
    const auto matchRepeated = [&](std::vector<std::size_t> seedPositions) {
        std::vector<std::size_t> results = seedPositions;
        std::vector<std::size_t> frontier = std::move(seedPositions);
        while (!frontier.empty()) {
            std::vector<std::size_t> nextFrontier;
            for (const std::size_t candidate : frontier) {
                const auto matches = MatchDtdContentModelNodeBase(node, observedChildren, candidate);
                for (const std::size_t matchPosition : matches) {
                    if (std::find(results.begin(), results.end(), matchPosition) == results.end()) {
                        results.push_back(matchPosition);
                        nextFrontier.push_back(matchPosition);
                    }
                }
            }
            frontier = std::move(nextFrontier);
        }
        return results;
    };

    switch (node.occurrence) {
    case '\0':
        return MatchDtdContentModelNodeBase(node, observedChildren, position);
    case '?': {
        std::vector<std::size_t> positions = {position};
        CollectUniquePositions(positions, MatchDtdContentModelNodeBase(node, observedChildren, position));
        return positions;
    }
    case '*':
        return matchRepeated({position});
    case '+':
        return matchRepeated(MatchDtdContentModelNodeBase(node, observedChildren, position));
    default:
        throw XmlException("Malformed DTD content model");
    }
}

bool TryParseMixedContentSpecification(
    std::string_view specification,
    std::unordered_set<std::string>& allowedChildren) {
    std::size_t position = 0;
    SkipDtdContentModelWhitespace(specification, position);
    if (position >= specification.size() || specification[position] != '(') {
        return false;
    }

    ++position;
    SkipDtdContentModelWhitespace(specification, position);
    if (!StartsWithAt(specification, position, "#PCDATA")) {
        return false;
    }

    position += 7;
    SkipDtdContentModelWhitespace(specification, position);
    if (position < specification.size() && specification[position] == ')') {
        ++position;
        SkipDtdContentModelWhitespace(specification, position);
        return position == specification.size();
    }

    while (position < specification.size() && specification[position] == '|') {
        ++position;
        SkipDtdContentModelWhitespace(specification, position);
        const auto name = ReadSubsetName(specification, position);
        if (name.empty() || !allowedChildren.insert(std::string(name)).second) {
            throw XmlException("Malformed DTD content model");
        }
        SkipDtdContentModelWhitespace(specification, position);
    }

    if (position >= specification.size() || specification[position] != ')') {
        throw XmlException("Malformed DTD content model");
    }
    ++position;
    if (position >= specification.size() || specification[position] != '*') {
        throw XmlException("Malformed DTD content model");
    }
    ++position;
    SkipDtdContentModelWhitespace(specification, position);
    return position == specification.size();
}

bool DoesObservedChildrenMatchContentModel(
    std::string_view specification,
    const std::vector<std::string>& observedChildren) {
    std::size_t position = 0;
    DtdContentModelNode root = ParseDtdContentModelParticle(specification, position);
    SkipDtdContentModelWhitespace(specification, position);
    if (position != specification.size()) {
        throw XmlException("Malformed DTD content model");
    }

    const auto positions = MatchDtdContentModelNode(root, observedChildren, 0);
    return std::find(positions.begin(), positions.end(), observedChildren.size()) != positions.end();
}

std::string DecodeUtf16BomEncodedXml(std::string_view text);

std::string NormalizeBomEncodedXmlText(std::string_view text) {
    return HasUtf16Bom(text) ? DecodeUtf16BomEncodedXml(text) : std::string(text);
}

std::string DecodeUtf16BomEncodedXml(std::string_view text) {
    const bool littleEndian = HasUtf16LittleEndianBom(text);
    if (!littleEndian && !HasUtf16BigEndianBom(text)) {
        return std::string(text);
    }

    if ((text.size() - 2) % 2 != 0) {
        throw XmlException("Malformed UTF-16 XML input");
    }

    std::string decoded;
    decoded.reserve(text.size() / 2);

    std::size_t position = 2;
    while (position < text.size()) {
        const auto readCodeUnit = [&](std::size_t offset) -> std::uint16_t {
            const unsigned char first = static_cast<unsigned char>(text[offset]);
            const unsigned char second = static_cast<unsigned char>(text[offset + 1]);
            return littleEndian
                ? static_cast<std::uint16_t>(first | (static_cast<std::uint16_t>(second) << 8))
                : static_cast<std::uint16_t>((static_cast<std::uint16_t>(first) << 8) | second);
        };

        const std::uint16_t firstUnit = readCodeUnit(position);
        position += 2;

        std::uint32_t codePoint = firstUnit;
        if (firstUnit >= 0xD800 && firstUnit <= 0xDBFF) {
            if (position >= text.size()) {
                throw XmlException("Malformed UTF-16 XML input");
            }

            const std::uint16_t secondUnit = readCodeUnit(position);
            position += 2;
            if (secondUnit < 0xDC00 || secondUnit > 0xDFFF) {
                throw XmlException("Malformed UTF-16 XML input");
            }

            codePoint = 0x10000u
                + ((static_cast<std::uint32_t>(firstUnit - 0xD800u) << 10)
                    | static_cast<std::uint32_t>(secondUnit - 0xDC00u));
        } else if (firstUnit >= 0xDC00 && firstUnit <= 0xDFFF) {
            throw XmlException("Malformed UTF-16 XML input");
        }

        AppendCodePointUtf8(decoded, codePoint);
    }

    return decoded;
}

std::size_t ConsumeLeadingTextDeclarationIfPresent(std::string_view text) {
    if (!StartsWithAt(text, 0, "<?xml")) {
        return 0;
    }

    if (text.size() <= 5 || !IsWhitespace(text[5])) {
        throw XmlException("Malformed XML declaration");
    }

    const auto end = text.find("?>", 5);
    if (end == std::string_view::npos) {
        throw XmlException("Malformed XML declaration");
    }

    const std::string_view value = text.substr(5, end - 5);
    std::size_t position = 0;
    bool sawVersion = false;
    bool sawEncoding = false;

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

        const std::string tokenValue(value.substr(token.rawValueStart, token.rawValueEnd - token.rawValueStart));
        if (token.name == "version") {
            if (sawVersion || sawEncoding) {
                throw XmlException("Malformed XML declaration");
            }
            sawVersion = true;
            if (tokenValue != "1.0") {
                throw XmlException("Malformed XML declaration");
            }
            continue;
        }

        if (token.name == "encoding") {
            if (sawEncoding) {
                throw XmlException("Malformed XML declaration");
            }
            sawEncoding = true;
            ValidateXmlDeclarationEncoding(tokenValue);
            continue;
        }

        throw XmlException("Malformed XML declaration");
    }

    if (!sawEncoding) {
        throw XmlException("Malformed XML declaration");
    }

    return end + 2;
}

}  // namespace

class StreamXmlReaderInputSource final : public XmlReaderInputSource {
public:
    ~StreamXmlReaderInputSource() override {
        CloseReplayFile();
        RemoveReplayFile();
    }

    explicit StreamXmlReaderInputSource(std::istream& stream)
        : stream_(&stream) {
    }

    StreamXmlReaderInputSource(std::istream& stream, std::string initialBuffer)
        : stream_(&stream),
          buffer_(std::move(initialBuffer)),
          eof_(stream_ == nullptr || !stream_->good()) {
    }

    explicit StreamXmlReaderInputSource(std::shared_ptr<std::istream> stream)
        : ownedStream_(std::move(stream)),
          stream_(ownedStream_.get()) {
    }

    StreamXmlReaderInputSource(std::shared_ptr<std::istream> stream, std::string initialBuffer)
        : ownedStream_(std::move(stream)),
          stream_(ownedStream_.get()),
          buffer_(std::move(initialBuffer)),
          eof_(stream_ == nullptr || !stream_->good()) {
    }

    char CharAt(std::size_t position) const noexcept override {
        std::size_t bufferIndex = 0;
        if (TryGetBufferedIndex(position, bufferIndex)) {
            return buffer_[bufferIndex];
        }

        EnsureBufferedThrough(position);
        if (TryGetBufferedIndex(position, bufferIndex)) {
            return buffer_[bufferIndex];
        }
        return ReadReplayCharAt(position);
    }

    const char* PtrAt(std::size_t position, std::size_t& available) const noexcept override {
        available = 0;
        std::size_t bufferIndex = 0;
        if (TryGetBufferedIndex(position, bufferIndex)) {
            available = buffer_.size() - bufferIndex;
            return buffer_.data() + static_cast<std::ptrdiff_t>(bufferIndex);
        }

        EnsureBufferedThrough(position);
        if (TryGetBufferedIndex(position, bufferIndex)) {
            available = buffer_.size() - bufferIndex;
            return buffer_.data() + static_cast<std::ptrdiff_t>(bufferIndex);
        }
        return ReadReplayPtrAt(position, available);
    }

    std::size_t Find(std::string_view token, std::size_t position) const noexcept override {
        if (token.empty()) {
            return position;
        }

        const bool singleToken = token.size() == 1;
        const char singleTokenChar = singleToken ? token.front() : '\0';

        if (position < bufferStartOffset_) {
            const std::size_t replaySearchEnd = (std::min)(bufferStartOffset_, replayLength_);
            const std::size_t replayFound = singleToken
                ? FindFirstOfInReplay(std::string_view(&singleTokenChar, 1), position, replaySearchEnd)
                : FindInReplay(token, position, replaySearchEnd);
            if (replayFound != std::string::npos) {
                return replayFound;
            }

            if (token.size() > 1 && bufferStartOffset_ != 0) {
                const std::size_t boundaryStart = bufferStartOffset_ >= token.size() - 1
                    ? (std::max)(position, bufferStartOffset_ - (token.size() - 1))
                    : position;
                const std::size_t boundaryEnd = BufferedEndOffset();
                for (std::size_t cursor = boundaryStart; cursor < bufferStartOffset_; ++cursor) {
                    if (cursor > boundaryEnd || token.size() > boundaryEnd - cursor) {
                        break;
                    }

                    bool matched = true;
                    for (std::size_t index = 0; index < token.size(); ++index) {
                        if (CharAt(cursor + index) != token[index]) {
                            matched = false;
                            break;
                        }
                    }
                    if (matched) {
                        return cursor;
                    }
                }
            }

            position = bufferStartOffset_;
        }

        EnsureBufferedThrough(position);
        std::size_t searchPosition = position >= bufferStartOffset_
            ? bufferLogicalStart_ + (position - bufferStartOffset_)
            : bufferLogicalStart_;
        while (true) {
            const auto found = singleToken
                ? buffer_.find(singleTokenChar, searchPosition)
                : buffer_.find(token, searchPosition);
            if (found != std::string::npos) {
                return bufferStartOffset_ + (found - bufferLogicalStart_);
            }
            if (eof_) {
                return std::string::npos;
            }

            const auto sizeBefore = buffer_.size();
            ReadMore();
            if (buffer_.size() == sizeBefore) {
                return std::string::npos;
            }

            if (token.size() > 1 && sizeBefore >= token.size() - 1) {
                searchPosition = (std::max)(searchPosition, sizeBefore - (token.size() - 1));
            } else {
                searchPosition = (std::max)(searchPosition, sizeBefore);
            }
        }
    }

    std::size_t FindFirstOf(std::string_view tokens, std::size_t position) const noexcept {
        if (tokens.empty()) {
            return position;
        }

        const bool textSpecialTokens = tokens == "<&";
        const bool textLineSpecialTokens = tokens == "\n<&";

        if (position < bufferStartOffset_) {
            const std::size_t replaySearchEnd = (std::min)(bufferStartOffset_, replayLength_);
            const std::size_t replayFound = FindFirstOfInReplay(tokens, position, replaySearchEnd);
            if (replayFound != std::string::npos) {
                return replayFound;
            }
            position = bufferStartOffset_;
        }

        EnsureBufferedThrough(position);
        std::size_t searchPosition = position >= bufferStartOffset_
            ? bufferLogicalStart_ + (position - bufferStartOffset_)
            : bufferLogicalStart_;
        while (true) {
            std::size_t found = std::string::npos;
            if (searchPosition < buffer_.size()) {
                const char* searchData = buffer_.data() + static_cast<std::ptrdiff_t>(searchPosition);
                const std::size_t searchLength = buffer_.size() - searchPosition;
                if (textSpecialTokens) {
                    const std::size_t offset = FindTextSpecialInBuffer(searchData, searchLength);
                    found = offset == std::string::npos ? std::string::npos : searchPosition + offset;
                } else if (textLineSpecialTokens) {
                    const std::size_t offset = FindTextLineSpecialInBuffer(searchData, searchLength);
                    found = offset == std::string::npos ? std::string::npos : searchPosition + offset;
                } else {
                    found = buffer_.find_first_of(tokens, searchPosition);
                }
            }
            if (found != std::string::npos) {
                return bufferStartOffset_ + (found - bufferLogicalStart_);
            }
            if (eof_) {
                return std::string::npos;
            }

            const auto sizeBefore = buffer_.size();
            ReadMore();
            if (buffer_.size() == sizeBefore) {
                return std::string::npos;
            }

            searchPosition = (std::max)(searchPosition, sizeBefore);
        }
    }

    std::size_t FindNextTextSpecial(std::size_t position) const noexcept {
        if (position < bufferStartOffset_) {
            const std::size_t replaySearchEnd = (std::min)(bufferStartOffset_, replayLength_);
            const std::size_t replayFound = FindFirstOfInReplay("<&", position, replaySearchEnd);
            if (replayFound != std::string::npos) {
                return replayFound;
            }
            position = bufferStartOffset_;
        }

        EnsureBufferedThrough(position);
        std::size_t searchPosition = position >= bufferStartOffset_
            ? bufferLogicalStart_ + (position - bufferStartOffset_)
            : bufferLogicalStart_;
        while (true) {
            std::size_t found = std::string::npos;
            if (searchPosition < buffer_.size()) {
                const std::size_t offset = FindTextSpecialInBuffer(
                    buffer_.data() + static_cast<std::ptrdiff_t>(searchPosition),
                    buffer_.size() - searchPosition);
                found = offset == std::string::npos ? std::string::npos : searchPosition + offset;
            }
            if (found != std::string::npos) {
                return bufferStartOffset_ + (found - bufferLogicalStart_);
            }
            if (eof_) {
                return std::string::npos;
            }

            const auto sizeBefore = buffer_.size();
            ReadMore();
            if (buffer_.size() == sizeBefore) {
                return std::string::npos;
            }

            searchPosition = (std::max)(searchPosition, sizeBefore);
        }
    }

    std::size_t ScanQuotedValueEnd(std::size_t quoteStart) const noexcept {
        const char quote = CharAt(quoteStart);
        if (quote != '"' && quote != '\'') {
            return std::string::npos;
        }

        std::size_t scanPosition = quoteStart + 1;
        while (true) {
            EnsureBufferedThrough(scanPosition);
            if (scanPosition < bufferStartOffset_) {
                break;
            }

            const std::size_t bufferIndex = bufferLogicalStart_ + (scanPosition - bufferStartOffset_);
            if (bufferIndex < buffer_.size()) {
                const std::size_t offset = FindQuoteOrNulInBuffer(
                    buffer_.data() + bufferIndex,
                    buffer_.size() - bufferIndex,
                    quote);
                if (offset != std::string::npos) {
                    const std::size_t match = scanPosition + offset;
                    return CharAt(match) == quote ? match : std::string::npos;
                }
            }

            if (eof_) {
                return std::string::npos;
            }

            const std::size_t previousEnd = BufferedEndOffset();
            ReadMore();
            if (BufferedEndOffset() == previousEnd) {
                return std::string::npos;
            }
            scanPosition = previousEnd;
        }

        return ScanQuotedValueEndAt(quoteStart, [this](std::size_t probe) noexcept {
            return CharAt(probe);
        });
    }


    std::string Slice(std::size_t start, std::size_t count) const override {
        std::string result;
        AppendSliceTo(result, start, count);
        return result;
    }

    void AppendSliceTo(std::string& target, std::size_t start, std::size_t count) const override {
        if (count == std::string::npos) {
            EnsureAllBuffered();
        } else if (count != 0 && start <= (std::numeric_limits<std::size_t>::max)() - (count - 1)) {
            EnsureBufferedThrough(start + count - 1);
        } else {
            EnsureBufferedThrough(start);
        }

        const std::size_t bufferedEnd = BufferedEndOffset();
        if (start >= bufferedEnd && !(HasReplayFile() && start < replayLength_)) {
            return;
        }

        const std::size_t requestedEnd = count == std::string::npos
            ? bufferedEnd
            : start + (std::min)(count, bufferedEnd > start ? bufferedEnd - start : 0);
        const std::size_t actualEnd = count == std::string::npos
            ? bufferedEnd
            : (std::min)(requestedEnd, bufferedEnd);

        if (start < bufferStartOffset_) {
            const std::size_t replayEnd = (std::min)(actualEnd, bufferStartOffset_);
            if (replayEnd > start) {
                AppendReplaySliceTo(target, start, replayEnd - start);
            }
        }

        if (actualEnd > bufferStartOffset_) {
            const std::size_t memoryStart = start < bufferStartOffset_ ? bufferStartOffset_ : start;
            const std::size_t memoryCount = actualEnd - memoryStart;
            if (memoryCount != 0) {
                target.append(buffer_, bufferLogicalStart_ + (memoryStart - bufferStartOffset_), memoryCount);
            }
        }
    }

private:
    static constexpr std::size_t ChunkSize = 64 * 1024;
    static constexpr std::size_t BufferCompactionThreshold = 256 * 1024;
    static constexpr std::size_t ReplayIoBufferSize = 256 * 1024;

    std::size_t ActiveBufferSize() const noexcept {
        return buffer_.size() >= bufferLogicalStart_ ? buffer_.size() - bufferLogicalStart_ : 0;
    }

    bool TryGetBufferedIndex(std::size_t position, std::size_t& bufferIndex) const noexcept {
        if (position < bufferStartOffset_) {
            return false;
        }

        const std::size_t relative = position - bufferStartOffset_;
        const std::size_t activeSize = ActiveBufferSize();
        if (relative >= activeSize) {
            return false;
        }

        bufferIndex = bufferLogicalStart_ + relative;
        return bufferIndex < buffer_.size();
    }

    std::size_t BufferedEndOffset() const noexcept {
        return bufferStartOffset_ + ActiveBufferSize();
    }

    void MaybeCompactBuffer() const {
        if (bufferLogicalStart_ == 0) {
            return;
        }

        const std::size_t activeSize = ActiveBufferSize();
        if (bufferLogicalStart_ < BufferCompactionThreshold && bufferLogicalStart_ < activeSize) {
            return;
        }

        buffer_.erase(0, bufferLogicalStart_);
        bufferLogicalStart_ = 0;
    }

    void CloseOwnedStreamIfPossible() const noexcept {
        if (ownedStream_ == nullptr) {
            return;
        }

        if (auto* fileStream = dynamic_cast<std::ifstream*>(ownedStream_.get()); fileStream != nullptr) {
            fileStream->close();
        }
        stream_ = nullptr;
    }

    void CloseReplayFile() const noexcept {
        if (replayFile_ != nullptr && replayFile_->is_open()) {
            replayFile_->close();
        }
    }

    void RemoveReplayFile() const noexcept {
        if (replayPath_.empty()) {
            return;
        }
        std::error_code error;
        std::filesystem::remove(replayPath_, error);
        replayPath_.clear();
    }

    bool HasReplayFile() const noexcept {
        return replayFile_ != nullptr && replayFile_->is_open();
    }

    void EnableReplay() const override {
        replayEnabled_ = true;
        if (!HasReplayFile()) {
            EnsureReplayFile();
        }
    }

    void DiscardBefore(std::size_t position) const override {
        if (position <= bufferStartOffset_) {
            return;
        }

        const std::size_t bufferEnd = BufferedEndOffset();
        const std::size_t discardEnd = (std::min)(position, bufferEnd);
        if (discardEnd <= bufferStartOffset_) {
            return;
        }

        if (replayEnabled_ && !HasReplayFile()) {
            EnsureReplayFile();
        }

        const std::size_t discardCount = discardEnd - bufferStartOffset_;
        bufferStartOffset_ = discardEnd;
        bufferLogicalStart_ += discardCount;
        MaybeCompactBuffer();
    }

    void EnsureReplayFile() const {
        if (HasReplayFile()) {
            return;
        }

        replayPath_ = CreateTemporaryXmlReplayPath();
        replayFile_ = std::make_unique<std::fstream>(
            replayPath_,
            std::ios::in | std::ios::out | std::ios::binary | std::ios::trunc);
        if (replayFile_ == nullptr || !replayFile_->is_open()) {
            RemoveReplayFile();
            throw XmlException("Failed to create stream replay file");
        }

        replayIoBuffer_.assign(ReplayIoBufferSize, '\0');
        replayFile_->rdbuf()->pubsetbuf(replayIoBuffer_.data(), static_cast<std::streamsize>(replayIoBuffer_.size()));

        const std::size_t activeSize = ActiveBufferSize();
        if (activeSize != 0) {
            replayFile_->seekp(static_cast<std::streamoff>(bufferStartOffset_), std::ios::beg);
            replayFile_->write(
                buffer_.data() + static_cast<std::ptrdiff_t>(bufferLogicalStart_),
                static_cast<std::streamsize>(activeSize));
            if (!*replayFile_) {
                CloseReplayFile();
                RemoveReplayFile();
                replayFile_.reset();
                throw XmlException("Failed to seed stream replay file");
            }
        }
        replayLength_ = bufferStartOffset_ + activeSize;
        replayFile_->flush();
    }

    char ReadReplayCharAt(std::size_t position) const noexcept {
        std::size_t available = 0;
        if (const char* ptr = ReadReplayPtrAt(position, available); ptr != nullptr) {
            return *ptr;
        }
        return '\0';
    }

    const char* ReadReplayPtrAt(std::size_t position, std::size_t& available) const noexcept {
        available = 0;
        if (!HasReplayFile() || position >= replayLength_) {
            return nullptr;
        }

        if (position < replayWindowStart_ || position >= replayWindowStart_ + replayWindow_.size()) {
            replayWindowStart_ = position;
            const std::size_t remaining = replayLength_ - position;
            const std::size_t windowSize = (std::min)(ChunkSize, remaining);
            replayWindow_.assign(windowSize, '\0');

            replayFile_->clear();
            replayFile_->seekg(static_cast<std::streamoff>(position), std::ios::beg);
            replayFile_->read(replayWindow_.data(), static_cast<std::streamsize>(windowSize));
            const auto bytesRead = replayFile_->gcount();
            if (bytesRead <= 0) {
                replayFile_->clear();
                replayWindow_.clear();
                return nullptr;
            }

            replayWindow_.resize(static_cast<std::size_t>(bytesRead));
            replayFile_->clear();
        }

        const std::size_t windowIndex = position - replayWindowStart_;
        if (windowIndex >= replayWindow_.size()) {
            return nullptr;
        }

        available = replayWindow_.size() - windowIndex;
        return replayWindow_.data() + static_cast<std::ptrdiff_t>(windowIndex);
    }

    void AppendReplaySliceTo(std::string& target, std::size_t start, std::size_t count) const {
        if (!HasReplayFile() || count == 0 || start >= replayLength_) {
            return;
        }

        const std::size_t actualCount = (std::min)(count, replayLength_ - start);
        const std::size_t originalSize = target.size();
        target.resize(originalSize + actualCount);
        replayFile_->clear();
        std::streambuf* replayBuffer = replayFile_->rdbuf();
        replayBuffer->pubseekpos(static_cast<std::streampos>(start), std::ios::in);
        const auto bytesRead = replayBuffer->sgetn(
            target.data() + static_cast<std::ptrdiff_t>(originalSize),
            static_cast<std::streamsize>(actualCount));
        replayFile_->clear();
        if (bytesRead > 0) {
            target.resize(originalSize + static_cast<std::size_t>(bytesRead));
        } else {
            target.resize(originalSize);
        }
    }

    std::size_t FindInReplay(std::string_view token, std::size_t start, std::size_t end) const noexcept {
        if (!HasReplayFile() || start >= end) {
            return std::string::npos;
        }

        const std::size_t overlap = token.size() > 1 ? token.size() - 1 : 0;
        std::size_t cursor = start;
        std::string carry;

        while (cursor < end) {
            const std::size_t remaining = end - cursor;
            const std::size_t chunkSize = (std::min)(ChunkSize, remaining);
            std::string block = carry;
            const std::size_t blockPrefixSize = carry.size();

            replayFile_->clear();
            replayFile_->seekg(static_cast<std::streamoff>(cursor), std::ios::beg);
            const std::size_t blockOriginalSize = block.size();
            block.resize(blockOriginalSize + chunkSize);
            replayFile_->read(block.data() + static_cast<std::ptrdiff_t>(blockOriginalSize), static_cast<std::streamsize>(chunkSize));
            const auto bytesRead = replayFile_->gcount();
            replayFile_->clear();
            if (bytesRead <= 0) {
                return std::string::npos;
            }
            block.resize(blockOriginalSize + static_cast<std::size_t>(bytesRead));

            const auto found = block.find(token);
            if (found != std::string::npos) {
                const std::size_t matchStart = cursor - blockPrefixSize + found;
                if (matchStart + token.size() <= end) {
                    return matchStart;
                }
            }

            if (overlap != 0) {
                const std::size_t carrySize = (std::min)(overlap, block.size());
                carry.assign(block, block.size() - carrySize, carrySize);
            }
            cursor += static_cast<std::size_t>(bytesRead);
        }

        return std::string::npos;
    }

    std::size_t FindFirstOfInReplay(std::string_view tokens, std::size_t start, std::size_t end) const noexcept {
        if (!HasReplayFile() || start >= end || tokens.empty()) {
            return std::string::npos;
        }

        std::size_t cursor = start;
        const bool textSpecialTokens = tokens == "<&";
        const bool textLineSpecialTokens = tokens == "\n<&";
        const bool singleToken = tokens.size() == 1;
        const char singleTokenChar = singleToken ? tokens.front() : '\0';
        while (cursor < end) {
            const std::size_t remaining = end - cursor;
            const std::size_t chunkSize = (std::min)(ChunkSize, remaining);
            std::string block(chunkSize, '\0');

            replayFile_->clear();
            replayFile_->seekg(static_cast<std::streamoff>(cursor), std::ios::beg);
            replayFile_->read(block.data(), static_cast<std::streamsize>(chunkSize));
            const auto bytesRead = replayFile_->gcount();
            replayFile_->clear();
            if (bytesRead <= 0) {
                return std::string::npos;
            }

            if (textSpecialTokens) {
                for (std::size_t index = 0; index < static_cast<std::size_t>(bytesRead); ++index) {
                    const char ch = block[index];
                    if (ch == '<' || ch == '&') {
                        return cursor + index;
                    }
                }
            } else if (textLineSpecialTokens) {
                for (std::size_t index = 0; index < static_cast<std::size_t>(bytesRead); ++index) {
                    const char ch = block[index];
                    if (ch == '\n' || ch == '<' || ch == '&') {
                        return cursor + index;
                    }
                }
            } else if (singleToken) {
                for (std::size_t index = 0; index < static_cast<std::size_t>(bytesRead); ++index) {
                    if (block[index] == singleTokenChar) {
                        return cursor + index;
                    }
                }
            } else {
                for (std::size_t index = 0; index < static_cast<std::size_t>(bytesRead); ++index) {
                    if (tokens.find(block[index]) != std::string_view::npos) {
                        return cursor + index;
                    }
                }
            }

            cursor += static_cast<std::size_t>(bytesRead);
        }

        return std::string::npos;
    }


    void EnsureBufferedThrough(std::size_t position) const noexcept {
        while (!eof_ && BufferedEndOffset() <= position) {
            ReadMore();
        }
    }

    void EnsureAllBuffered() const noexcept {
        while (!eof_) {
            ReadMore();
        }
    }

    void ReadMore() const noexcept {
        if (stream_ == nullptr || eof_) {
            eof_ = true;
            return;
        }

        char chunk[ChunkSize];
        stream_->read(chunk, static_cast<std::streamsize>(ChunkSize));
        const auto bytesRead = stream_->gcount();
        if (bytesRead > 0) {
            const std::size_t byteCount = static_cast<std::size_t>(bytesRead);
            buffer_.append(chunk, byteCount);
            if (replayEnabled_ && !HasReplayFile()) {
                try {
                    EnsureReplayFile();
                } catch (...) {
                    eof_ = true;
                    CloseOwnedStreamIfPossible();
                    return;
                }
            }
            if (replayEnabled_ && HasReplayFile()) {
                replayFile_->clear();
                replayFile_->seekp(static_cast<std::streamoff>(replayLength_), std::ios::beg);
                replayFile_->write(chunk, bytesRead);
                replayFile_->flush();
                if (!*replayFile_) {
                    eof_ = true;
                    CloseOwnedStreamIfPossible();
                    return;
                }
                replayLength_ += byteCount;
            }
        }
        if (bytesRead < static_cast<std::streamsize>(ChunkSize) || !stream_->good()) {
            eof_ = true;
            CloseOwnedStreamIfPossible();
        }
    }

    mutable std::shared_ptr<std::istream> ownedStream_;
    mutable std::istream* stream_ = nullptr;
    mutable std::string buffer_;
    mutable std::size_t bufferStartOffset_ = 0;
    mutable std::size_t bufferLogicalStart_ = 0;
    mutable std::unique_ptr<std::fstream> replayFile_;
    mutable std::filesystem::path replayPath_;
    mutable std::vector<char> replayIoBuffer_;
    mutable std::size_t replayLength_ = 0;
    mutable std::size_t replayWindowStart_ = 0;
    mutable std::string replayWindow_;
    mutable bool replayEnabled_ = false;
    mutable bool eof_ = false;
};

#if defined(_WIN32)
class MemoryMappedXmlReaderInputSource final : public XmlReaderInputSource {
public:
    explicit MemoryMappedXmlReaderInputSource(const std::filesystem::path& path) {
        const std::wstring widePath = path.wstring();
        fileHandle_ = ::CreateFileW(
            widePath.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS,
            nullptr);
        if (fileHandle_ == INVALID_HANDLE_VALUE) {
            throw XmlException("Failed to open XML file mapping");
        }

        LARGE_INTEGER fileSize;
        if (!::GetFileSizeEx(fileHandle_, &fileSize) || fileSize.QuadPart < 0) {
            throw XmlException("Failed to query XML file size for mapping");
        }
        if (static_cast<unsigned long long>(fileSize.QuadPart)
            > static_cast<unsigned long long>((std::numeric_limits<std::size_t>::max)())) {
            throw XmlException("XML file is too large to map into the current process");
        }

        size_ = static_cast<std::size_t>(fileSize.QuadPart);
        if (size_ == 0) {
            return;
        }

        mappingHandle_ = ::CreateFileMappingW(fileHandle_, nullptr, PAGE_READONLY, 0, 0, nullptr);
        if (mappingHandle_ == nullptr) {
            throw XmlException("Failed to create XML file mapping");
        }

        mappedData_ = static_cast<const char*>(::MapViewOfFile(mappingHandle_, FILE_MAP_READ, 0, 0, 0));
        if (mappedData_ == nullptr) {
            throw XmlException("Failed to map XML file view");
        }
    }

    ~MemoryMappedXmlReaderInputSource() override {
        if (mappedData_ != nullptr) {
            ::UnmapViewOfFile(mappedData_);
        }
        if (mappingHandle_ != nullptr) {
            ::CloseHandle(mappingHandle_);
        }
        if (fileHandle_ != INVALID_HANDLE_VALUE) {
            ::CloseHandle(fileHandle_);
        }
    }

    char CharAt(std::size_t position) const noexcept override {
        return position < size_ ? mappedData_[position] : '\0';
    }

    const char* PtrAt(std::size_t position, std::size_t& available) const noexcept override {
        available = 0;
        if (position >= size_ || mappedData_ == nullptr) {
            return nullptr;
        }

        available = size_ - position;
        return mappedData_ + static_cast<std::ptrdiff_t>(position);
    }

    std::size_t Find(std::string_view token, std::size_t position) const noexcept override {
        if (token.empty()) {
            return position <= size_ ? position : std::string::npos;
        }
        if (position >= size_ || token.size() > size_ - position || mappedData_ == nullptr) {
            return std::string::npos;
        }

        if (token.size() == 1) {
            const void* found = std::memchr(
                mappedData_ + static_cast<std::ptrdiff_t>(position),
                token.front(),
                size_ - position);
            return found == nullptr
                ? std::string::npos
                : static_cast<std::size_t>(static_cast<const char*>(found) - mappedData_);
        }

        const std::string_view haystack(mappedData_, size_);
        return haystack.find(token, position);
    }

    std::size_t FindFirstOf(std::string_view tokens, std::size_t position) const noexcept {
        if (tokens.empty()) {
            return position <= size_ ? position : std::string::npos;
        }
        if (position >= size_ || mappedData_ == nullptr) {
            return std::string::npos;
        }

        if (tokens.size() == 1) {
            return Find(tokens, position);
        }

        const std::string_view haystack(mappedData_, size_);
        return haystack.find_first_of(tokens, position);
    }

    std::size_t ScanQuotedValueEnd(std::size_t quoteStart) const noexcept {
        const char quote = CharAt(quoteStart);
        if ((quote != '\'' && quote != '"') || quoteStart + 1 > size_ || mappedData_ == nullptr) {
            return std::string::npos;
        }

        const void* found = std::memchr(
            mappedData_ + static_cast<std::ptrdiff_t>(quoteStart + 1),
            quote,
            size_ - (quoteStart + 1));
        return found == nullptr
            ? std::string::npos
            : static_cast<std::size_t>(static_cast<const char*>(found) - mappedData_);
    }

    std::string Slice(std::size_t start, std::size_t count) const override {
        if (start >= size_ || mappedData_ == nullptr) {
            return {};
        }

        const std::size_t available = size_ - start;
        const std::size_t clampedCount = count == std::string::npos ? available : (std::min)(count, available);
        return std::string(mappedData_ + static_cast<std::ptrdiff_t>(start), clampedCount);
    }

    void AppendSliceTo(std::string& target, std::size_t start, std::size_t count) const override {
        if (start >= size_ || mappedData_ == nullptr) {
            return;
        }

        const std::size_t available = size_ - start;
        const std::size_t clampedCount = count == std::string::npos ? available : (std::min)(count, available);
        target.append(mappedData_ + static_cast<std::ptrdiff_t>(start), clampedCount);
    }

private:
    HANDLE fileHandle_ = INVALID_HANDLE_VALUE;
    HANDLE mappingHandle_ = nullptr;
    const char* mappedData_ = nullptr;
    std::size_t size_ = 0;
};
#endif

class SubrangeXmlReaderInputSource final : public XmlReaderInputSource {
public:
    SubrangeXmlReaderInputSource(std::shared_ptr<const XmlReaderInputSource> inputSource, std::size_t start, std::size_t length)
        : inputSource_(std::move(inputSource)),
          start_(start),
          length_(length) {
    }

    char CharAt(std::size_t position) const noexcept override {
        if (inputSource_ == nullptr || position >= length_) {
            return '\0';
        }
        return inputSource_->CharAt(start_ + position);
    }

    const char* PtrAt(std::size_t position, std::size_t& available) const noexcept override {
        available = 0;
        if (inputSource_ == nullptr || position >= length_) {
            return nullptr;
        }

        std::size_t sourceAvailable = 0;
        const char* ptr = inputSource_->PtrAt(start_ + position, sourceAvailable);
        if (ptr == nullptr) {
            return nullptr;
        }

        available = (std::min)(sourceAvailable, length_ - position);
        return available == 0 ? nullptr : ptr;
    }

    std::size_t Find(std::string_view token, std::size_t position) const noexcept override {
        if (inputSource_ == nullptr) {
            return std::string::npos;
        }
        if (token.empty()) {
            return position <= length_ ? position : std::string::npos;
        }
        if (position >= length_) {
            return std::string::npos;
        }

        const std::size_t found = inputSource_->Find(token, start_ + position);
        if (found == std::string::npos || found < start_) {
            return std::string::npos;
        }

        const std::size_t end = start_ + length_;
        if (found >= end || token.size() > end - found) {
            return std::string::npos;
        }

        return found - start_;
    }

    std::size_t FindFirstOf(std::string_view tokens, std::size_t position) const noexcept {
        if (tokens.empty()) {
            return position <= length_ ? position : std::string::npos;
        }
        if (inputSource_ == nullptr || position >= length_) {
            return std::string::npos;
        }

        const std::size_t end = start_ + length_;
        if (const auto* streamSource = dynamic_cast<const StreamXmlReaderInputSource*>(inputSource_.get()); streamSource != nullptr) {
            const std::size_t found = tokens == "<&"
                ? streamSource->FindNextTextSpecial(start_ + position)
                : streamSource->FindFirstOf(tokens, start_ + position);
            if (found == std::string::npos || found < start_ || found >= end) {
                return std::string::npos;
            }
            return found - start_;
        }
        if (const auto* stringSource = dynamic_cast<const StringXmlReaderInputSource*>(inputSource_.get()); stringSource != nullptr) {
            const std::size_t found = stringSource->FindFirstOf(tokens, start_ + position);
            if (found == std::string::npos || found < start_ || found >= end) {
                return std::string::npos;
            }
            return found - start_;
        }
#if defined(_WIN32)
        if (const auto* mappedSource = dynamic_cast<const MemoryMappedXmlReaderInputSource*>(inputSource_.get()); mappedSource != nullptr) {
            const std::size_t found = mappedSource->FindFirstOf(tokens, start_ + position);
            if (found == std::string::npos || found < start_ || found >= end) {
                return std::string::npos;
            }
            return found - start_;
        }
#endif

        for (std::size_t probe = position; probe < length_; ++probe) {
            const char ch = inputSource_->CharAt(start_ + probe);
            if (ch == '\0') {
                return std::string::npos;
            }
            if (tokens.find(ch) != std::string_view::npos) {
                return probe;
            }
        }
        return std::string::npos;
    }

    std::size_t ScanQuotedValueEnd(std::size_t quoteStart) const noexcept {
        if (inputSource_ == nullptr || quoteStart >= length_) {
            return std::string::npos;
        }

        std::size_t found = std::string::npos;
        if (const auto* stringSource = dynamic_cast<const StringXmlReaderInputSource*>(inputSource_.get()); stringSource != nullptr) {
            found = stringSource->ScanQuotedValueEnd(start_ + quoteStart);
        } else if (const auto* streamSource = dynamic_cast<const StreamXmlReaderInputSource*>(inputSource_.get()); streamSource != nullptr) {
            found = streamSource->ScanQuotedValueEnd(start_ + quoteStart);
#if defined(_WIN32)
        } else if (const auto* mappedSource = dynamic_cast<const MemoryMappedXmlReaderInputSource*>(inputSource_.get()); mappedSource != nullptr) {
            found = mappedSource->ScanQuotedValueEnd(start_ + quoteStart);
#endif
        } else {
            found = ScanQuotedValueEndAt(start_ + quoteStart, [this](std::size_t probe) noexcept {
                return inputSource_->CharAt(probe);
            });
        }

        if (found == std::string::npos || found < start_ || found >= start_ + length_) {
            return std::string::npos;
        }
        return found - start_;
    }

    std::string Slice(std::size_t start, std::size_t count) const override {
        if (inputSource_ == nullptr || start >= length_) {
            return {};
        }

        const std::size_t available = length_ - start;
        const std::size_t clampedCount = count == std::string::npos ? available : (std::min)(count, available);
        return inputSource_->Slice(start_ + start, clampedCount);
    }

    void AppendSliceTo(std::string& target, std::size_t start, std::size_t count) const override {
        if (inputSource_ == nullptr || start >= length_) {
            return;
        }

        const std::size_t available = length_ - start;
        const std::size_t clampedCount = count == std::string::npos ? available : (std::min)(count, available);
        inputSource_->AppendSliceTo(target, start_ + start, clampedCount);
    }

    void EnableReplay() const override {
        if (inputSource_ != nullptr) {
            inputSource_->EnableReplay();
        }
    }

private:
    std::shared_ptr<const XmlReaderInputSource> inputSource_;
    std::size_t start_ = 0;
    std::size_t length_ = 0;
};

std::size_t FindFirstOfFromInputSource(const XmlReaderInputSource* inputSource, std::string_view tokens, std::size_t position) noexcept {
    if (inputSource == nullptr) {
        return std::string::npos;
    }
    if (const auto* streamSource = dynamic_cast<const StreamXmlReaderInputSource*>(inputSource); streamSource != nullptr) {
        if (tokens == "<&") {
            return streamSource->FindNextTextSpecial(position);
        }
        return streamSource->FindFirstOf(tokens, position);
    }
#if defined(_WIN32)
    if (const auto* mappedSource = dynamic_cast<const MemoryMappedXmlReaderInputSource*>(inputSource); mappedSource != nullptr) {
        return mappedSource->FindFirstOf(tokens, position);
    }
#endif
    if (const auto* subrangeSource = dynamic_cast<const SubrangeXmlReaderInputSource*>(inputSource); subrangeSource != nullptr) {
        return subrangeSource->FindFirstOf(tokens, position);
    }

    if (tokens == "<&") {
        const std::size_t nextEntity = inputSource->Find("&", position);
        const std::size_t nextMarkup = inputSource->Find("<", position);
        if (nextEntity == std::string::npos) {
            return nextMarkup;
        }
        if (nextMarkup == std::string::npos) {
            return nextEntity;
        }
        return (std::min)(nextEntity, nextMarkup);
    }

    if (tokens.empty()) {
        return position;
    }
    for (std::size_t probe = position; ; ++probe) {
        const char ch = inputSource->CharAt(probe);
        if (ch == '\0') {
            return std::string::npos;
        }
        if (tokens.find(ch) != std::string_view::npos) {
            return probe;
        }
    }
}

void AdvanceLineInfoFromInputSource(
    const XmlReaderInputSource* inputSource,
    std::size_t start,
    std::size_t end,
    std::size_t& line,
    std::size_t& column) noexcept {
    if (end <= start) {
        return;
    }
    if (inputSource == nullptr) {
        column += end - start;
        return;
    }

    while (start < end) {
        std::size_t available = 0;
        const char* chunk = inputSource->PtrAt(start, available);
        if (chunk == nullptr || available == 0) {
            for (std::size_t index = start; index < end; ++index) {
                if (inputSource->CharAt(index) == '\n') {
                    ++line;
                    column = 1;
                } else {
                    ++column;
                }
            }
            return;
        }

        const std::size_t chunkLength = (std::min)(available, end - start);
        const char* cursor = chunk;
        std::size_t remaining = chunkLength;
        while (remaining != 0) {
            const void* newlinePtr = std::memchr(cursor, '\n', remaining);
            if (newlinePtr == nullptr) {
                column += remaining;
                break;
            }

            const char* newline = static_cast<const char*>(newlinePtr);
            column += static_cast<std::size_t>(newline - cursor);
            ++line;
            column = 1;
            const std::size_t consumed = static_cast<std::size_t>(newline - cursor) + 1;
            cursor += consumed;
            remaining -= consumed;
        }

        start += chunkLength;
    }
}

void SkipXmlWhitespaceAt(const XmlReaderInputSource* inputSource, std::size_t& position) noexcept {
    while (inputSource != nullptr) {
        const char ch = inputSource->CharAt(position);
        if (ch == '\0' || !IsWhitespace(ch)) {
            break;
        }
        ++position;
    }
}

void SkipXmlWhitespaceAt(std::string_view text, std::size_t& position) noexcept {
    const std::size_t size = text.size();
    if (position >= size) return;

#if defined(_M_X64) || defined(_M_IX86) || defined(__x86_64__)
    const __m128i v_space = _mm_set1_epi8(' ');
    const __m128i v_tab = _mm_set1_epi8('\t');
    const __m128i v_lf = _mm_set1_epi8('\n');
    const __m128i v_cr = _mm_set1_epi8('\r');

    while (position + 16 <= size) {
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(text.data() + position));

        __m128i m_space = _mm_cmpeq_epi8(chunk, v_space);
        __m128i m_tab = _mm_cmpeq_epi8(chunk, v_tab);
        __m128i m_lf = _mm_cmpeq_epi8(chunk, v_lf);
        __m128i m_cr = _mm_cmpeq_epi8(chunk, v_cr);

        __m128i is_ws = _mm_or_si128(_mm_or_si128(m_space, m_tab),
            _mm_or_si128(m_lf, m_cr));

        unsigned int mask = static_cast<unsigned int>(_mm_movemask_epi8(is_ws));

        if (mask != 0xFFFF) {
            unsigned long first_non_ws_index = 0;
#ifdef _MSC_VER
            _BitScanForward(&first_non_ws_index, ~mask & 0xFFFF);
#else
            first_non_ws_index = __builtin_ctz(~mask | 0x10000);
#endif
            position += first_non_ws_index;
            return;
        }
        position += 16;
    }
#elif defined(_M_ARM64) || defined(__aarch64__)
    const uint8x16_t v_space = vdupq_n_u8(' ');
    const uint8x16_t v_tab = vdupq_n_u8('\t');
    const uint8x16_t v_lf = vdupq_n_u8('\n');
    const uint8x16_t v_cr = vdupq_n_u8('\r');

    while (position + 16 <= size) {
        const uint8x16_t chunk = vld1q_u8(reinterpret_cast<const std::uint8_t*>(text.data() + position));
        const uint8x16_t m_space = vceqq_u8(chunk, v_space);
        const uint8x16_t m_tab = vceqq_u8(chunk, v_tab);
        const uint8x16_t m_lf = vceqq_u8(chunk, v_lf);
        const uint8x16_t m_cr = vceqq_u8(chunk, v_cr);
        const uint8x16_t isWs = vorrq_u8(vorrq_u8(m_space, m_tab), vorrq_u8(m_lf, m_cr));

        alignas(16) std::uint8_t maskBytes[16];
        vst1q_u8(maskBytes, isWs);

        std::size_t firstNonWhitespace = 0;
        while (firstNonWhitespace < 16 && maskBytes[firstNonWhitespace] != 0) {
            ++firstNonWhitespace;
        }

        if (firstNonWhitespace != 16) {
            position += firstNonWhitespace;
            return;
        }

        position += 16;
    }
#endif
    while (position < size && IsWhitespace(text[position])) {
        ++position;
    }
}

bool TryConsumeNameFromInputSourceAt(const XmlReaderInputSource* inputSource, std::size_t& position) noexcept {
    if (inputSource == nullptr) {
        return false;
    }

    const auto start = position;
    std::size_t available = 0;
    if (const char* chunk = inputSource->PtrAt(start, available); chunk != nullptr && available != 0) {
        const unsigned char first = static_cast<unsigned char>(chunk[0]);
        if (first < 0x80 && IsNameStartChar(static_cast<char>(first))) {
            const std::size_t nameLength = 1 + ConsumeNameCharsInBuffer(chunk + 1, available - 1);
            if (nameLength < available && static_cast<unsigned char>(chunk[nameLength]) < 0x80) {
                position = start + nameLength;
                return true;
            }
        }
    }

    position = ConsumeXmlNameAt(position, [inputSource](std::size_t index) noexcept {
        return inputSource->CharAt(index);
    });
    if (position == start) {
        return false;
    }

    return true;
}

std::string ParseNameFromInputSourceAt(const XmlReaderInputSource* inputSource, std::size_t& position) {
    const std::size_t start = position;
    bool usedAsciiFastPath = false;
    std::size_t available = 0;
    if (inputSource != nullptr) {
        if (const char* chunk = inputSource->PtrAt(start, available); chunk != nullptr && available != 0) {
            const unsigned char first = static_cast<unsigned char>(chunk[0]);
            if (first < 0x80 && IsNameStartChar(static_cast<char>(first))) {
                const std::size_t nameLength = 1 + ConsumeNameCharsInBuffer(chunk + 1, available - 1);
                if (nameLength < available && static_cast<unsigned char>(chunk[nameLength]) < 0x80) {
                    position = start + nameLength;
                    usedAsciiFastPath = true;
                }
            }
        }
    }
    if (!usedAsciiFastPath && !TryConsumeNameFromInputSourceAt(inputSource, position)) {
        return {};
    }

    std::string name = inputSource->Slice(start, position - start);
    if (!usedAsciiFastPath) {
        try {
            (void)XmlConvert::VerifyName(name);
        } catch (const XmlException&) {
            position = start;
            return {};
        }
    }

    return name;
}

bool SkipTagFromInputSourceAt(const XmlReaderInputSource* inputSource, std::size_t& position, bool& isEmptyElement) noexcept {
    if (inputSource == nullptr || inputSource->CharAt(position) != '<') {
        return false;
    }

    ++position;
    if (!TryConsumeNameFromInputSourceAt(inputSource, position)) {
        return false;
    }

    while (true) {
        const char ch = inputSource->CharAt(position);
        if (ch == '\0') {
            return false;
        }

        if (ch == '"' || ch == '\'') {
            const auto quoteEnd = ScanQuotedValueEndAt(position, [inputSource](std::size_t probe) noexcept {
                return inputSource->CharAt(probe);
            });
            if (quoteEnd == std::string::npos) {
                return false;
            }
            position = quoteEnd + 1;
            continue;
        }

        if (inputSource->CharAt(position) == '/' && inputSource->CharAt(position + 1) == '>') {
            position += 2;
            isEmptyElement = true;
            return true;
        }

        if (ch == '>') {
            ++position;
            isEmptyElement = false;
            return true;
        }

        ++position;
    }
}

bool StartsWithAt(const XmlReaderInputSource* inputSource, std::size_t position, std::string_view token) noexcept {
    if (inputSource == nullptr) {
        return false;
    }

    std::size_t compared = 0;
    while (compared < token.size()) {
        std::size_t available = 0;
        const char* chunk = inputSource->PtrAt(position + compared, available);
        if (chunk == nullptr || available == 0) {
            return false;
        }

        const std::size_t chunkLength = (std::min)(available, token.size() - compared);
        if (std::memcmp(chunk, token.data() + static_cast<std::ptrdiff_t>(compared), chunkLength) != 0) {
            return false;
        }

        compared += chunkLength;
    }

    return true;
}

std::size_t ScanDocumentTypeInternalSubsetEndAt(const XmlReaderInputSource* inputSource, std::size_t contentStart) noexcept {
    if (inputSource == nullptr) {
        return std::string::npos;
    }

    std::size_t position = contentStart;
    int bracketDepth = 1;
    bool inQuote = false;
    char quote = '\0';

    const auto scanTerminator = [inputSource](std::size_t start, std::string_view terminator) noexcept {
        for (std::size_t probe = start; inputSource->CharAt(probe) != '\0'; ++probe) {
            if (StartsWithAt(inputSource, probe, terminator)) {
                return probe;
            }
        }

        return std::string::npos;
    };

    while (bracketDepth > 0) {
        if (!inQuote && StartsWithAt(inputSource, position, "<!--")) {
            const auto commentEnd = scanTerminator(position + 4, "-->");
            if (commentEnd == std::string::npos) {
                break;
            }

            position = commentEnd + 3;
            continue;
        }

        if (!inQuote && StartsWithAt(inputSource, position, "<?")) {
            const auto processingInstructionEnd = scanTerminator(position + 2, "?>");
            if (processingInstructionEnd == std::string::npos) {
                break;
            }

            position = processingInstructionEnd + 2;
            continue;
        }

        const char ch = inputSource->CharAt(position);
        if (ch == '\0') {
            break;
        }

        ++position;
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

std::size_t ScanQuotedValueEndAt(const XmlReaderInputSource* inputSource, std::size_t quoteStart) noexcept {
    if (inputSource == nullptr) {
        return std::string::npos;
    }

    if (const auto* stringSource = dynamic_cast<const StringXmlReaderInputSource*>(inputSource); stringSource != nullptr) {
        return stringSource->ScanQuotedValueEnd(quoteStart);
    }
    if (const auto* streamSource = dynamic_cast<const StreamXmlReaderInputSource*>(inputSource); streamSource != nullptr) {
        return streamSource->ScanQuotedValueEnd(quoteStart);
    }
#if defined(_WIN32)
    if (const auto* mappedSource = dynamic_cast<const MemoryMappedXmlReaderInputSource*>(inputSource); mappedSource != nullptr) {
        return mappedSource->ScanQuotedValueEnd(quoteStart);
    }
#endif
    if (const auto* subrangeSource = dynamic_cast<const SubrangeXmlReaderInputSource*>(inputSource); subrangeSource != nullptr) {
        return subrangeSource->ScanQuotedValueEnd(quoteStart);
    }

    return ScanQuotedValueEndAt(quoteStart, [inputSource](std::size_t probe) noexcept {
        return inputSource->CharAt(probe);
    });
}

std::size_t ScanDelimitedSectionEndAt(
    const XmlReaderInputSource* inputSource,
    std::size_t contentStart,
    std::string_view terminator) noexcept {
    if (inputSource == nullptr) {
        return std::string::npos;
    }

    return ScanDelimitedSectionEndAt(contentStart, terminator, [inputSource](std::string_view token, std::size_t position) noexcept {
        return inputSource->Find(std::string(token), position);
    });
}

struct XmlQuotedValueToken {
    std::string rawValue;
    bool valid = false;
};

XmlQuotedValueToken ParseQuotedLiteralFromInputSourceAt(const XmlReaderInputSource* inputSource, std::size_t& position) {
    XmlQuotedValueToken token;
    if (inputSource == nullptr) {
        return token;
    }

    const char quote = inputSource->CharAt(position);
    const auto quoteEnd = ScanQuotedValueEndAt(position, [inputSource](std::size_t probe) noexcept {
        return inputSource->CharAt(probe);
    });
    if ((quote != '"' && quote != '\'') || quoteEnd == std::string::npos) {
        return token;
    }

    token.rawValue = inputSource->Slice(position + 1, quoteEnd - position - 1);
    position = quoteEnd + 1;
    token.valid = true;
    return token;
}

struct XmlEntityReferenceToken {
    std::string raw;
    std::string name;
    std::size_t end = std::string::npos;
};

XmlEntityReferenceToken ScanEntityReferenceFromInputSourceAt(const XmlReaderInputSource* inputSource, std::size_t ampersandPosition) {
    if (inputSource == nullptr) {
        return {};
    }

    const auto semicolon = inputSource->Find(";", ampersandPosition);
    if (semicolon == std::string::npos) {
        return {};
    }

    return {
        inputSource->Slice(ampersandPosition, semicolon - ampersandPosition + 1),
        inputSource->Slice(ampersandPosition + 1, semicolon - ampersandPosition - 1),
        semicolon + 1};
}

class XmlReaderTokenizer final {
public:
    using AttributeAssignmentToken = XmlAttributeAssignmentToken;
    using QuotedValueToken = XmlQuotedValueToken;
    using EntityReferenceToken = XmlEntityReferenceToken;

    explicit XmlReaderTokenizer(const XmlReaderInputSource* inputSource) noexcept
                : inputSource_(inputSource),
                    stringInputSource_(dynamic_cast<const StringXmlReaderInputSource*>(inputSource)),
                    streamInputSource_(dynamic_cast<const StreamXmlReaderInputSource*>(inputSource)),
                    subrangeInputSource_(dynamic_cast<const SubrangeXmlReaderInputSource*>(inputSource)) {
    }

    bool HasChar(std::size_t position) const noexcept {
        return CharAt(position) != '\0';
    }

    char CharAt(std::size_t position) const noexcept {
        return inputSource_ == nullptr ? '\0' : inputSource_->CharAt(position);
    }

    const char* PtrAt(std::size_t position, std::size_t& available) const noexcept {
        available = 0;
        return inputSource_ == nullptr ? nullptr : inputSource_->PtrAt(position, available);
    }

    bool StartsWith(std::size_t position, std::string_view token) const noexcept {
        return StartsWithAt(inputSource_, position, token);
    }

    std::size_t Find(std::string_view token, std::size_t position) const noexcept {
        return inputSource_ == nullptr ? std::string::npos : inputSource_->Find(token, position);
    }

    std::size_t ScanQuotedValueEnd(std::size_t quoteStart) const noexcept {
        return ScanQuotedValueEndAt(inputSource_, quoteStart);
    }

    std::size_t ScanDelimitedSectionEnd(std::size_t contentStart, std::string_view terminator) const noexcept {
        return ScanDelimitedSectionEndAt(inputSource_, contentStart, terminator);
    }

    std::size_t ScanDocumentTypeInternalSubsetEnd(std::size_t contentStart) const noexcept {
        return ScanDocumentTypeInternalSubsetEndAt(inputSource_, contentStart);
    }

    XmlMarkupKind ClassifyMarkup(std::size_t position) const noexcept {
        return ClassifyXmlMarkupWithCharAt(position, [this](std::size_t probe) noexcept {
            return CharAt(probe);
        });
    }

    std::string ParseNameAt(std::size_t& position) const {
        return ParseNameFromInputSourceAt(inputSource_, position);
    }

    bool SkipTag(std::size_t& position, bool& isEmptyElement) const {
        return SkipTagFromInputSourceAt(inputSource_, position, isEmptyElement);
    }

    void SkipWhitespace(std::size_t& position) const noexcept {
        SkipXmlWhitespaceAt(inputSource_, position);
    }

    bool ConsumeStartTagClose(std::size_t& position, bool& isEmptyElement) const noexcept {
        return ConsumeXmlStartTagCloseAt(
            position,
            isEmptyElement,
            [this](std::size_t probe) noexcept {
                return CharAt(probe);
            },
            [this](std::size_t probe, std::string_view token) noexcept {
                return StartsWith(probe, token);
            });
    }

    bool ConsumeEndTagClose(std::size_t& position) const noexcept {
        return ConsumeXmlEndTagCloseAt(
            position,
            [this](std::size_t& probe) noexcept {
                SkipWhitespace(probe);
            },
            [this](std::size_t probe) noexcept {
                return CharAt(probe);
            });
    }

    AttributeAssignmentToken ParseAttributeAssignment(std::size_t& position) const {
        AttributeAssignmentToken token;
        token.name = ParseNameAt(position);
        if (token.name.empty()) {
            return token;
        }

        SkipWhitespace(position);
        if (CharAt(position) != '=') {
            return token;
        }
        token.sawEquals = true;
        ++position;

        SkipWhitespace(position);
        const char quote = CharAt(position);
        if (quote != '"' && quote != '\'') {
            return token;
        }

        token.rawValueStart = position + 1;
        token.scannedRawValue = true;

        for (std::size_t probe = token.rawValueStart;;) {
            std::size_t available = 0;
            const char* chunk = PtrAt(probe, available);
            if (chunk == nullptr || available == 0) {
                const unsigned char ch = static_cast<unsigned char>(CharAt(probe));
                if (ch == 0) {
                    return token;
                }
                if (ch == static_cast<unsigned char>(quote)) {
                    token.rawValueEnd = probe;
                    position = probe + 1;
                    token.valid = true;
                    return token;
                }
                if (ch < 0x80) {
                    if (ch == '<' && token.invalidLtPosition == std::string::npos) {
                        token.invalidLtPosition = probe;
                    }
                    if (ch == '&') {
                        token.rawValueFlags = static_cast<unsigned char>(token.rawValueFlags | kAttributeValueNeedsDecoding);
                    }
                    if (!(ch == 0x09 || ch == 0x0A || ch == 0x0D || ch >= 0x20)
                        && token.invalidCharacterPosition == std::string::npos) {
                        token.invalidCharacterPosition = probe;
                    }
                    ++probe;
                    continue;
                }

                std::uint32_t codePoint = 0;
                std::size_t width = 0;
                if (!DecodeUtf8CodePointAt(probe, [this](std::size_t index) noexcept {
                        return CharAt(index);
                    }, codePoint, width)
                    || !IsValidXmlCharacterCodePoint(codePoint)) {
                    if (token.invalidCharacterPosition == std::string::npos) {
                        token.invalidCharacterPosition = probe;
                    }
                    ++probe;
                    continue;
                }

                probe += width;
                continue;
            }

            std::size_t offset = 0;
            while (offset < available) {
                const unsigned char ch = static_cast<unsigned char>(chunk[offset]);
                if (ch == 0) {
                    return token;
                }
                if (ch == static_cast<unsigned char>(quote)) {
                    token.rawValueEnd = probe + offset;
                    position = token.rawValueEnd + 1;
                    token.valid = true;
                    return token;
                }
                if (ch >= 0x80) {
                    break;
                }
                if (ch == '<' && token.invalidLtPosition == std::string::npos) {
                    token.invalidLtPosition = probe + offset;
                }
                if (ch == '&') {
                    token.rawValueFlags = static_cast<unsigned char>(token.rawValueFlags | kAttributeValueNeedsDecoding);
                }
                if (!(ch == 0x09 || ch == 0x0A || ch == 0x0D || ch >= 0x20)
                    && token.invalidCharacterPosition == std::string::npos) {
                    token.invalidCharacterPosition = probe + offset;
                }
                ++offset;
            }

            probe += offset;
            if (offset == available) {
                continue;
            }

            std::uint32_t codePoint = 0;
            std::size_t width = 0;
            if (!DecodeUtf8CodePointAt(probe, [this](std::size_t index) noexcept {
                    return CharAt(index);
                }, codePoint, width)
                || !IsValidXmlCharacterCodePoint(codePoint)) {
                if (token.invalidCharacterPosition == std::string::npos) {
                    token.invalidCharacterPosition = probe;
                }
                ++probe;
                continue;
            }

            probe += width;
        }
    }

    QuotedValueToken ParseQuotedLiteral(std::size_t& position) const {
        return ParseQuotedLiteralFromInputSourceAt(inputSource_, position);
    }

    EntityReferenceToken ScanEntityReference(std::size_t ampersandPosition) const {
        return ScanEntityReferenceFromInputSourceAt(inputSource_, ampersandPosition);
    }

    std::string Slice(std::size_t start, std::size_t count = std::string::npos) const {
        return inputSource_ == nullptr ? std::string{} : inputSource_->Slice(start, count);
    }

private:
    const XmlReaderInputSource* inputSource_;
    const StringXmlReaderInputSource* stringInputSource_ = nullptr;
    const StreamXmlReaderInputSource* streamInputSource_ = nullptr;
    const SubrangeXmlReaderInputSource* subrangeInputSource_ = nullptr;
};

XmlReader::XmlReader(XmlReaderSettings settings) : settings_(std::move(settings)) {
}

char XmlReader::SourceCharAt(std::size_t position) const noexcept {
    return inputSource_ == nullptr ? '\0' : inputSource_->CharAt(position);
}

const char* XmlReader::SourcePtrAt(std::size_t position, std::size_t& available) const noexcept {
    available = 0;
    return inputSource_ == nullptr ? nullptr : inputSource_->PtrAt(position, available);
}

bool XmlReader::HasSourceChar(std::size_t position) const noexcept {
    return SourceCharAt(position) != '\0';
}

std::size_t XmlReader::FindInSource(std::string_view token, std::size_t position) const noexcept {
    return inputSource_ == nullptr ? std::string::npos : inputSource_->Find(token, position);
}

std::string XmlReader::SourceSubstr(std::size_t start, std::size_t count) const {
    return inputSource_ == nullptr ? std::string{} : inputSource_->Slice(start, count);
}

bool XmlReader::SourceRangeContains(std::size_t start, std::size_t end, char value) const noexcept {
    if (inputSource_ == nullptr || end <= start) {
        return false;
    }

    while (start < end) {
        std::size_t available = 0;
        const char* chunk = SourcePtrAt(start, available);
        if (chunk == nullptr || available == 0) {
            break;
        }

        const std::size_t chunkLength = (std::min)(available, end - start);
        if (std::char_traits<char>::find(chunk, chunkLength, value) != nullptr) {
            return true;
        }

        start += chunkLength;
    }

    return false;
}

std::size_t XmlReader::ValidateXmlCharacterAt(std::size_t position) const {
    const unsigned char first = static_cast<unsigned char>(SourceCharAt(position));
    if (first < 0x80) {
        if (first == 0x09 || first == 0x0A || first == 0x0D || first >= 0x20) {
            return 1;
        }

        const auto [line, column] = ComputeLineColumn(position);
        throw XmlException("Invalid XML character", line, column);
    }

    std::uint32_t codePoint = 0;
    std::size_t width = 0;
    if (!DecodeUtf8CodePointAt(position, [this](std::size_t index) noexcept {
            return SourceCharAt(index);
        }, codePoint, width)
        || !IsValidXmlCharacterCodePoint(codePoint)) {
        const auto [line, column] = ComputeLineColumn(position);
        throw XmlException("Invalid XML character", line, column);
    }

    return width;
}

void XmlReader::AppendSourceSubstrTo(std::string& target, std::size_t start, std::size_t count) const {
    if (inputSource_ == nullptr) {
        return;
    }
    inputSource_->AppendSliceTo(target, start, count);
}

std::size_t XmlReader::AppendDecodedSourceRangeTo(std::string& target, std::size_t start, std::size_t end) const {
    if (inputSource_ == nullptr || end <= start) {
        return 0;
    }

    const std::size_t originalSize = target.size();
    target.reserve(originalSize + (end - start));
    XmlReaderTokenizer tokenizer(inputSource_.get());

    if (const auto* stringSource = dynamic_cast<const StringXmlReaderInputSource*>(inputSource_.get()); stringSource != nullptr) {
        const std::string& source = *stringSource->Text();
        
        if (!dtdState_ || dtdState_->entityDeclarations.empty()) {
            target.resize(originalSize + (end - start));
            char* out = target.data() + originalSize;
            const char* in = source.data() + start;
            const char* const inEnd = source.data() + end;

            while (in < inEnd) {
                const char* ampersand = static_cast<const char*>(std::memchr(in, '&', inEnd - in));
                if (!ampersand) {
                    const std::size_t len = inEnd - in;
                    std::memcpy(out, in, len);
                    out += len;
                    break;
                }

                const std::size_t len = ampersand - in;
                if (len > 0) {
                    std::memcpy(out, in, len);
                    out += len;
                }
                in = ampersand;

                const char* semicolon = static_cast<const char*>(std::memchr(in + 1, ';', inEnd - (in + 1)));
                if (!semicolon) {
                    Throw("Unterminated entity reference");
                }

                const std::string_view entity(in + 1, semicolon - (in + 1));
                const auto predefined = ResolvePredefinedEntityReferenceValueView(entity);
                if (!predefined.empty()) {
                    std::memcpy(out, predefined.data(), predefined.size());
                    out += predefined.size();
                } else if (!entity.empty() && entity.front() == '#') {
                    unsigned int codePoint = 0;
                    if (!TryParseNumericEntityReferenceCodePoint(entity, codePoint)) {
                        Throw("Invalid numeric entity reference: &" + std::string(entity) + ';');
                    }
                    if (codePoint <= 0x7F) {
                        *out++ = static_cast<char>(codePoint);
                    } else if (codePoint <= 0x7FF) {
                        *out++ = static_cast<char>(0xC0 | (codePoint >> 6));
                        *out++ = static_cast<char>(0x80 | (codePoint & 0x3F));
                    } else if (codePoint <= 0xFFFF) {
                        *out++ = static_cast<char>(0xE0 | (codePoint >> 12));
                        *out++ = static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
                        *out++ = static_cast<char>(0x80 | (codePoint & 0x3F));
                    } else {
                        *out++ = static_cast<char>(0xF0 | (codePoint >> 18));
                        *out++ = static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F));
                        *out++ = static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
                        *out++ = static_cast<char>(0x80 | (codePoint & 0x3F));
                    }
                } else {
                    ThrowUnknownEntityReference(entity);
                }
                in = semicolon + 1;
            }
            
            const std::size_t finalLen = out - target.data();
            target.resize(finalLen);
            const std::size_t appendedBytes = finalLen - originalSize;
            if (settings_.MaxCharactersFromEntities != 0) {
                const_cast<XmlReader*>(this)->entityCharactersRead_ += appendedBytes;
                if (const_cast<XmlReader*>(this)->entityCharactersRead_ > settings_.MaxCharactersFromEntities) {
                    Throw("The XML document exceeds the configured MaxCharactersFromEntities limit");
                }
            }
            return appendedBytes;
        }

        std::size_t cursor = start;
        while (cursor < end) {
            const std::size_t ampersand = tokenizer.Find("&", cursor);
            if (ampersand == std::string::npos || ampersand >= end) {
                target.append(source, cursor, end - cursor);
                break;
            }

            if (ampersand > cursor) {
                target.append(source, cursor, ampersand - cursor);
            }

            const auto entityToken = tokenizer.ScanEntityReference(ampersand);
            if (entityToken.end == std::string::npos || entityToken.end > end) {
                Throw("Unterminated entity reference");
            }

            const std::string_view entity(source.data() + ampersand + 1, entityToken.end - ampersand - 2);
            const auto predefined = ResolvePredefinedEntityReferenceValueView(entity);
            if (!predefined.empty()) {
                target.append(predefined.data(), predefined.size());
            } else if (!entity.empty() && entity.front() == '#') {
                unsigned int codePoint = 0;
                if (!TryParseNumericEntityReferenceCodePoint(entity, codePoint)) {
                    Throw("Invalid numeric entity reference: &" + std::string(entity) + ';');
                }
                AppendCodePointUtf8(target, codePoint);
            } else {
                const std::string entityName(entity);
                if (dtdState_) {
                    if (const auto found = dtdState_->entityDeclarations.find(entityName); found != dtdState_->entityDeclarations.end()) {
                        DecodeEntityTextTo(
                            target,
                            found->second,
                            [this](const std::string& nestedEntity) -> std::optional<std::string> {
                                const auto resolved = dtdState_->entityDeclarations.find(nestedEntity);
                                return resolved == dtdState_->entityDeclarations.end() ? std::nullopt : std::optional<std::string>(resolved->second);
                            },
                            [this](const std::string& message) {
                                Throw(message);
                            });
                    } else {
                        ThrowUnknownEntityReference(entityName);
                    }
                } else {
                    ThrowUnknownEntityReference(entityName);
                }
            }

            cursor = entityToken.end;
        }

        const std::size_t appendedBytes = target.size() - originalSize;
        if (settings_.MaxCharactersFromEntities != 0) {
            const_cast<XmlReader*>(this)->entityCharactersRead_ += appendedBytes;
            if (const_cast<XmlReader*>(this)->entityCharactersRead_ > settings_.MaxCharactersFromEntities) {
                Throw("The XML document exceeds the configured MaxCharactersFromEntities limit");
            }
        }

        return appendedBytes;
    }

    std::size_t cursor = start;
    while (cursor < end) {
        std::size_t ampersand = cursor;
        while (ampersand < end && SourceCharAt(ampersand) != '&') {
            ++ampersand;
        }
        if (ampersand > cursor) {
            AppendSourceSubstrTo(target, cursor, ampersand - cursor);
            cursor = ampersand;
            continue;
        }

        std::size_t semicolon = ampersand + 1;
        while (semicolon < end && SourceCharAt(semicolon) != ';') {
            ++semicolon;
        }
        if (semicolon >= end || SourceCharAt(semicolon) != ';') {
            Throw("Unterminated entity reference");
        }

        std::string entity;
        entity.reserve(semicolon - ampersand - 1);
        for (std::size_t probe = ampersand + 1; probe < semicolon; ++probe) {
            entity.push_back(SourceCharAt(probe));
        }

        const auto predefined = ResolvePredefinedEntityReferenceValueView(entity);
        if (!predefined.empty()) {
            target.append(predefined.data(), predefined.size());
        } else if (!entity.empty() && entity.front() == '#') {
            unsigned int codePoint = 0;
            if (!TryParseNumericEntityReferenceCodePoint(entity, codePoint)) {
                Throw("Invalid numeric entity reference: &" + entity + ';');
            }
            AppendCodePointUtf8(target, codePoint);
        } else if (dtdState_) {
            if (const auto found = dtdState_->entityDeclarations.find(entity); found != dtdState_->entityDeclarations.end()) {
                DecodeEntityTextTo(
                    target,
                    found->second,
                    [this](const std::string& nestedEntity) -> std::optional<std::string> {
                        const auto resolved = dtdState_->entityDeclarations.find(nestedEntity);
                        return resolved == dtdState_->entityDeclarations.end() ? std::nullopt : std::optional<std::string>(resolved->second);
                    },
                    [this](const std::string& message) {
                        Throw(message);
                    });
            } else {
                ThrowUnknownEntityReference(entity);
            }
        } else {
            ThrowUnknownEntityReference(entity);
        }

        cursor = semicolon + 1;
    }

    const std::size_t appendedBytes = target.size() - originalSize;
    if (settings_.MaxCharactersFromEntities != 0) {
        const_cast<XmlReader*>(this)->entityCharactersRead_ += appendedBytes;
        if (const_cast<XmlReader*>(this)->entityCharactersRead_ > settings_.MaxCharactersFromEntities) {
            Throw("The XML document exceeds the configured MaxCharactersFromEntities limit");
        }
    }

    return appendedBytes;
}

void XmlReader::AppendCurrentValueTo(std::string& target) const {
    if (!currentValue_.empty()) {
        target += currentValue_;
        return;
    }

    if (currentValueStart_ != std::string::npos
        && currentValueEnd_ != std::string::npos
        && currentValueEnd_ >= currentValueStart_) {
        AppendSourceSubstrTo(target, currentValueStart_, currentValueEnd_ - currentValueStart_);
        return;
    }

    target += Value();
}

void XmlReader::DecodeAndAppendCurrentBase64(std::vector<unsigned char>& buffer, unsigned int& accumulator, int& bits) const {
    if (!currentValue_.empty()) {
        AppendDecodedBase64Chunk(currentValue_, buffer, accumulator, bits);
        return;
    }

    if (currentValueStart_ != std::string::npos
        && currentValueEnd_ != std::string::npos
        && currentValueEnd_ >= currentValueStart_) {
        for (std::size_t index = currentValueStart_; index < currentValueEnd_; ++index) {
            const char ch = SourceCharAt(index);
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
        return;
    }

    AppendDecodedBase64Chunk(Value(), buffer, accumulator, bits);
}

std::size_t XmlReader::EarliestRetainedSourceOffset() const noexcept {
    std::size_t earliest = position_;

    auto includeOffset = [&earliest](std::size_t offset) {
        if (offset != std::string::npos && offset < earliest) {
            earliest = offset;
        }
    };

    auto includeValueRange = [&includeOffset](const std::string& value, std::size_t valueStart, std::size_t valueEnd) {
        if (valueStart != std::string::npos && valueEnd != std::string::npos && valueEnd >= valueStart && value.empty()) {
            includeOffset(valueStart);
        }
    };

    includeValueRange(currentValue_, currentValueStart_, currentValueEnd_);

    if (currentNodeType_ == XmlNodeType::Element) {
        if ((currentOuterXml_.empty() || (!currentIsEmptyElement_ && currentInnerXml_.empty()))
            && currentElementStart_ != std::string::npos) {
            includeOffset(currentElementStart_);
        }
        includeOffset(currentEarliestRetainedAttributeValueStart_);
    } else if (currentOuterXml_.empty() && currentNodeStart_ != std::string::npos && currentNodeEnd_ != std::string::npos) {
        includeOffset(currentNodeStart_);
    }

    for (const auto& node : bufferedNodes_) {
        includeValueRange(node.value, node.valueStart, node.valueEnd);
        if (node.nodeType == XmlNodeType::Element) {
            if ((node.outerXml.empty() || (!node.isEmptyElement && node.innerXml.empty()))
                && node.elementStart != std::string::npos) {
                includeOffset(node.elementStart);
            }
        } else if (node.outerXml.empty() && node.nodeStart != std::string::npos && node.nodeEnd != std::string::npos) {
            includeOffset(node.nodeStart);
        }
    }

    return earliest;
}

void XmlReader::MaybeDiscardSourcePrefix() const {
    if (inputSource_ == nullptr) {
        return;
    }

    if (dynamic_cast<const StreamXmlReaderInputSource*>(inputSource_.get()) == nullptr) {
        return;
    }

    const std::size_t discardBefore = EarliestRetainedSourceOffset();
    if (discardBefore <= discardedSourceOffset_) {
        return;
    }

    // Avoid per-node buffer compaction on stream-backed readers; reclaim in chunks.
    static constexpr std::size_t DiscardChunkThreshold = 512 * 1024;
    if (discardBefore - discardedSourceOffset_ < DiscardChunkThreshold) {
        return;
    }

    std::size_t line = 0;
    std::size_t column = 0;
    if (discardBefore == position_) {
        line = lineNumber_;
        column = linePosition_;
    } else {
        const auto computed = ComputeLineColumn(discardBefore);
        line = computed.first;
        column = computed.second;
    }
    inputSource_->DiscardBefore(discardBefore);
    discardedSourceOffset_ = discardBefore;
    discardedLineNumber_ = line;
    discardedLinePosition_ = column;
}

XmlReader::DtdState& XmlReader::EnsureDtdState() {
    if (!dtdState_) {
        dtdState_ = std::make_unique<DtdState>();
    }
    return *dtdState_;
}

void XmlReader::RegisterDtdDeclarations(
    const std::vector<std::shared_ptr<XmlNode>>& entities,
    const std::vector<std::shared_ptr<XmlNode>>& notations) {
    if (entities.empty() && notations.empty()) {
        return;
    }

    PopulateInternalEntityDeclarations(entities, EnsureDtdState().entityDeclarations);
    dtdState_->parsedEntities.insert(dtdState_->parsedEntities.end(), entities.begin(), entities.end());
    dtdState_->parsedNotations.insert(dtdState_->parsedNotations.end(), notations.begin(), notations.end());
    for (const auto& entity : entities) {
        if (entity == nullptr) {
            continue;
        }

        dtdState_->declaredEntityNames.insert(entity->Name());
        const auto typedEntity = std::dynamic_pointer_cast<XmlEntity>(entity);
        if (typedEntity == nullptr) {
            continue;
        }

        if (!typedEntity->SystemId().empty()) {
            dtdState_->externalEntitySystemIds[typedEntity->Name()] = typedEntity->SystemId();
        }
        if (!typedEntity->NotationName().empty()) {
            dtdState_->unparsedEntityDeclarationNames.insert(typedEntity->Name());
        }
    }

    for (const auto& notation : notations) {
        if (notation != nullptr) {
            dtdState_->notationDeclarationNames.insert(notation->Name());
        }
    }
}

void XmlReader::RegisterDtdElementDeclarations(
    const std::unordered_map<std::string, std::string>& elementContentDeclarations,
    bool fromExternalSubset) {
    if (elementContentDeclarations.empty()) {
        return;
    }

    DtdState& dtdState = EnsureDtdState();
    for (const auto& [elementName, contentSpecification] : elementContentDeclarations) {
        if (!dtdState.elementContentDeclarations.emplace(elementName, contentSpecification).second) {
            throw XmlException(
                "DTD validation failed: duplicate ELEMENT declaration for '" + elementName + "'");
        }
        if (contentSpecification == "EMPTY") {
            dtdState.emptyElementDeclarations.insert(elementName);
        }
        if (fromExternalSubset) {
            dtdState.externalElementDeclarationNames.insert(elementName);
        }
    }
}

void XmlReader::RegisterDtdAttributeDeclarations(
    const std::unordered_map<std::string, std::unordered_set<std::string>>& declaredAttributes,
    const std::unordered_map<std::string, std::string>& idAttributes,
    const std::unordered_map<std::string, std::string>& notationAttributes,
    const std::unordered_map<std::string, std::unordered_map<std::string, bool>>& nmTokenAttributes,
    const std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& nameAttributes,
    const std::unordered_map<std::string, std::unordered_set<std::string>>& requiredAttributes,
    const std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& defaultAttributes,
    const std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& fixedAttributes,
    const std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& enumeratedAttributes,
    bool fromExternalSubset) {
    if (declaredAttributes.empty() && idAttributes.empty() && notationAttributes.empty() && nmTokenAttributes.empty() && nameAttributes.empty() && requiredAttributes.empty() && defaultAttributes.empty() && fixedAttributes.empty() && enumeratedAttributes.empty()) {
        return;
    }

    DtdState& dtdState = EnsureDtdState();
    for (const auto& [elementName, attributeNames] : declaredAttributes) {
        auto& destination = dtdState.declaredAttributes[elementName];
        destination.insert(attributeNames.begin(), attributeNames.end());
    }

    for (const auto& [elementName, attributeName] : idAttributes) {
        const auto existingIdAttribute = dtdState.idAttributes.find(elementName);
        if (existingIdAttribute != dtdState.idAttributes.end() && existingIdAttribute->second != attributeName) {
            throw XmlException(
                "DTD validation failed: element '" + elementName + "' must not declare more than one ID attribute");
        }
        dtdState.idAttributes[elementName] = attributeName;
    }

    for (const auto& [elementName, attributeName] : notationAttributes) {
        const auto existingNotationAttribute = dtdState.notationAttributes.find(elementName);
        if (existingNotationAttribute != dtdState.notationAttributes.end() && existingNotationAttribute->second != attributeName) {
            throw XmlException(
                "DTD validation failed: element '" + elementName + "' must not declare more than one NOTATION attribute");
        }
        dtdState.notationAttributes[elementName] = attributeName;
    }

    for (const auto& [elementName, attributes] : nmTokenAttributes) {
        auto& destination = dtdState.nmTokenAttributes[elementName];
        for (const auto& [attributeName, allowMultipleTokens] : attributes) {
            destination.emplace(attributeName, allowMultipleTokens);
        }
    }

    for (const auto& [elementName, attributes] : nameAttributes) {
        auto& destination = dtdState.nameAttributes[elementName];
        for (const auto& [attributeName, attributeType] : attributes) {
            destination.emplace(attributeName, attributeType);
        }
    }

    for (const auto& [elementName, attributeNames] : requiredAttributes) {
        auto& destination = dtdState.requiredAttributes[elementName];
        destination.insert(attributeNames.begin(), attributeNames.end());
        if (fromExternalSubset) {
            auto& externalDestination = dtdState.externalRequiredAttributes[elementName];
            externalDestination.insert(attributeNames.begin(), attributeNames.end());
        }
    }

    for (const auto& [elementName, attributes] : defaultAttributes) {
        auto& destination = dtdState.defaultAttributes[elementName];
        for (const auto& [attributeName, value] : attributes) {
            if (fromExternalSubset) {
                if (destination.find(attributeName) == destination.end()) {
                    destination.emplace(attributeName, value);
                }
                dtdState.externalDefaultAttributes[elementName].emplace(attributeName, value);
            } else {
                destination[attributeName] = value;
            }
        }
    }

    for (const auto& [elementName, attributes] : fixedAttributes) {
        auto& destination = dtdState.fixedAttributes[elementName];
        for (const auto& [attributeName, value] : attributes) {
            if (fromExternalSubset) {
                if (destination.find(attributeName) == destination.end()) {
                    destination.emplace(attributeName, value);
                }
                dtdState.externalFixedAttributes[elementName].emplace(attributeName, value);
            } else {
                destination[attributeName] = value;
            }
        }
    }

    for (const auto& [elementName, attributes] : enumeratedAttributes) {
        auto& destination = dtdState.enumeratedAttributes[elementName];
        for (const auto& [attributeName, valueSet] : attributes) {
            if (fromExternalSubset) {
                if (destination.find(attributeName) == destination.end()) {
                    destination.emplace(attributeName, valueSet);
                }
                dtdState.externalEnumeratedAttributes[elementName].emplace(attributeName, valueSet);
            } else {
                destination[attributeName] = valueSet;
            }
        }
    }

    for (const auto& [elementName, attributeName] : notationAttributes) {
        const auto elementEnumerations = dtdState.enumeratedAttributes.find(elementName);
        if (elementEnumerations == dtdState.enumeratedAttributes.end()) {
            continue;
        }

        const auto enumeration = elementEnumerations->second.find(attributeName);
        if (enumeration == elementEnumerations->second.end()) {
            continue;
        }

        const std::string_view values = enumeration->second;
        const std::string_view inner = values.substr(1, values.size() - 2);
        std::size_t start = 0;
        while (start <= inner.size()) {
            const std::size_t separator = inner.find('|', start);
            const std::string notationName = Trim(std::string(inner.substr(
                start,
                separator == std::string_view::npos ? std::string_view::npos : separator - start)));
            if (!notationName.empty()
                && dtdState.notationDeclarationNames.find(notationName) == dtdState.notationDeclarationNames.end()) {
                throw XmlException(
                    "DTD validation failed: NOTATION attribute '" + attributeName
                    + "' on element '" + elementName
                    + "' references undeclared notation '" + notationName + "'");
            }

            if (separator == std::string_view::npos) {
                break;
            }
            start = separator + 1;
        }
    }

    ValidateDtdDeclaredAttributeValues(
        defaultAttributes,
        fixedAttributes,
        dtdState.entityDeclarations,
        dtdState.externalEntitySystemIds);
}

void XmlReader::LoadExternalSubsetDeclarations(
    std::string_view systemLiteral,
    const std::unordered_map<std::string, std::string>* inheritedParameterEntityDeclarations) {
    DtdState& dtdState = EnsureDtdState();
    if (!systemLiteral.empty()) {
        dtdState.externalSubsetSystemId = std::string(systemLiteral);
    }
    if (dtdState.externalSubsetDeclarationsLoaded
        || settings_.DtdProcessing != DtdProcessing::Parse
        || settings_.Resolver == nullptr
        || systemLiteral.empty()) {
        return;
    }

    const std::string absoluteUri = settings_.Resolver->ResolveUri(baseUri_, systemLiteral);
    const std::string resolvedExternalSubset = NormalizeBomEncodedXmlText(settings_.Resolver->GetEntity(absoluteUri));
    std::string_view externalSubsetView(resolvedExternalSubset);
    const std::size_t textDeclarationEnd = ConsumeLeadingTextDeclarationIfPresent(externalSubsetView);
    if (textDeclarationEnd != 0) {
        externalSubsetView.remove_prefix(textDeclarationEnd);
    }
    const std::string externalSubset = Trim(std::string(externalSubsetView));
    dtdState.externalSubsetDeclarationsLoaded = true;
    if (externalSubset.empty()) {
        return;
    }

    std::unordered_map<std::string, std::string> parameterEntityDeclarations;
    std::unordered_map<std::string, std::string> externalParameterEntitySystemLiterals;
    std::unordered_set<std::string> externalParameterEntityNames;
    if (inheritedParameterEntityDeclarations != nullptr) {
        parameterEntityDeclarations = *inheritedParameterEntityDeclarations;
    }
    CollectDtdParameterEntityDeclarations(
        externalSubset,
        parameterEntityDeclarations,
        true,
        &externalParameterEntitySystemLiterals);
    LoadExternalParameterEntityDeclarations(
        externalParameterEntitySystemLiterals,
        absoluteUri,
        parameterEntityDeclarations,
        externalParameterEntityNames);
    ValidateTopLevelParameterEntityReferenceBoundaries(
        externalSubset,
        parameterEntityDeclarations,
        true,
        &externalParameterEntityNames);
    ValidateElementDeclarationParameterEntityBoundaries(
        externalSubset,
        parameterEntityDeclarations,
        &externalParameterEntityNames);
    const std::string expandedExternalSubset = ExpandDtdParameterEntityReferences(
        externalSubset,
        parameterEntityDeclarations,
        true,
        &externalParameterEntityNames);

    std::vector<std::shared_ptr<XmlNode>> entities;
    std::vector<std::shared_ptr<XmlNode>> notations;
    ParseDocumentTypeInternalSubset(expandedExternalSubset, entities, notations, true);
    for (const auto& entity : entities) {
        if (entity != nullptr && entity->NodeType() == XmlNodeType::Entity) {
            dtdState.externalSubsetEntityNames.insert(entity->Name());
        }
    }
    RegisterDtdDeclarations(entities, notations);

    DtdRequiredAttributeDeclarations requiredAttributes;
    DtdDefaultAttributeDeclarations defaultAttributes;
    DtdFixedAttributeDeclarations fixedAttributes;
    DtdEnumeratedAttributeDeclarations enumeratedAttributes;
    DtdElementContentDeclarations elementContentDeclarations;
    DtdDeclaredAttributeNames declaredAttributes;
    DtdIdAttributeDeclarations idAttributes;
    DtdNotationAttributeDeclarations notationAttributes;
    DtdNmTokenAttributeDeclarations nmTokenAttributes;
    DtdNameAttributeDeclarations nameAttributes;
    ParseDocumentTypeElementDeclarations(expandedExternalSubset, elementContentDeclarations, true);
    ParseDocumentTypeAttributeDeclarations(expandedExternalSubset, declaredAttributes, requiredAttributes, defaultAttributes, fixedAttributes, enumeratedAttributes, idAttributes, notationAttributes, nmTokenAttributes, nameAttributes, true);
    ValidateDtdDeclaredAttributeValueReferenceOrder(expandedExternalSubset, true);
    RegisterDtdElementDeclarations(elementContentDeclarations, true);
    RegisterDtdAttributeDeclarations(declaredAttributes, idAttributes, notationAttributes, nmTokenAttributes, nameAttributes, requiredAttributes, defaultAttributes, fixedAttributes, enumeratedAttributes, true);
}

void XmlReader::LoadExternalParameterEntityDeclarations(
    const std::unordered_map<std::string, std::string>& externalParameterEntitySystemLiterals,
    std::string_view baseUri,
    std::unordered_map<std::string, std::string>& declarations,
    std::unordered_set<std::string>& externalParameterEntityNames) {
    if (externalParameterEntitySystemLiterals.empty()
        || settings_.DtdProcessing != DtdProcessing::Parse
        || settings_.Resolver == nullptr) {
        return;
    }

    std::vector<std::string> loadingStack;
    const auto loadRecursive = [&](const auto& self,
                                   const std::unordered_map<std::string, std::string>& systemLiterals,
                                   std::string_view currentBaseUri) -> void {
        for (const auto& [name, systemLiteral] : systemLiterals) {
            if (externalParameterEntityNames.find(name) != externalParameterEntityNames.end()) {
                continue;
            }

            auto declaration = declarations.find(name);
            if (declaration == declarations.end()) {
                continue;
            }

            if (std::find(loadingStack.begin(), loadingStack.end(), name) != loadingStack.end()) {
                ThrowMalformedDtdDeclaration("declaration");
            }

            const std::string absoluteUri = settings_.Resolver->ResolveUri(currentBaseUri, systemLiteral);
            const std::string rawResolvedValue = NormalizeBomEncodedXmlText(settings_.Resolver->GetEntity(absoluteUri));
            if (settings_.MaxCharactersFromEntities != 0) {
                entityCharactersRead_ += rawResolvedValue.size();
                if (entityCharactersRead_ > settings_.MaxCharactersFromEntities) {
                    Throw("The XML document exceeds the configured MaxCharactersFromEntities limit");
                }
            }

            loadingStack.push_back(name);
            try {
                std::string_view resolvedValue(rawResolvedValue);
                const std::size_t textDeclarationEnd = ConsumeLeadingTextDeclarationIfPresent(resolvedValue);
                if (textDeclarationEnd != 0) {
                    resolvedValue.remove_prefix(textDeclarationEnd);
                }

                declaration->second.assign(resolvedValue.data(), resolvedValue.size());

                std::unordered_map<std::string, std::string> nestedExternalParameterEntitySystemLiterals;
                CollectDtdParameterEntityDeclarations(
                    resolvedValue,
                    declarations,
                    true,
                    &nestedExternalParameterEntitySystemLiterals);
                self(self, nestedExternalParameterEntitySystemLiterals, absoluteUri);
                externalParameterEntityNames.insert(name);
            } catch (...) {
                loadingStack.pop_back();
                throw;
            }
            loadingStack.pop_back();
        }
    };

    loadRecursive(loadRecursive, externalParameterEntitySystemLiterals, baseUri);
}

void XmlReader::EnsureExternalSubsetDeclarationsLoaded() {
    if (!dtdState_ || dtdState_->externalSubsetSystemId.empty()) {
        return;
    }

    if (dtdState_->externalSubsetDeclarationsLoaded) {
        return;
    }

    LoadExternalSubsetDeclarations(dtdState_->externalSubsetSystemId);
}

[[noreturn]] void XmlReader::ThrowUnknownEntityReference(std::string_view entity) const {
    Throw("Unknown entity reference: &" + std::string(entity) + ';');
}

bool XmlReader::ApplyDtdAttributeDeclarations(
    std::string_view elementName,
    std::vector<std::pair<std::string, std::string>>& attributes,
    std::vector<AttributeValueMetadata>& attributeValueMetadata,
    bool& retainLocalNamespaceDeclarationsForAttributes) const {
    if (dtdState_ == nullptr) {
        return false;
    }

    bool requiresAttributePostProcessing = false;

    const auto hasAttribute = [&attributes](std::string_view attributeName) {
        for (const auto& attribute : attributes) {
            if (attribute.first == attributeName) {
                return true;
            }
        }
        return false;
    };

    const std::string elementNameString(elementName);
    const auto attributeUsesTokenizedType = [this, &elementNameString](std::string_view attributeName) {
        if (dtdState_ == nullptr) {
            return false;
        }

        if (const auto idIt = dtdState_->idAttributes.find(elementNameString);
            idIt != dtdState_->idAttributes.end() && idIt->second == attributeName) {
            return true;
        }
        if (const auto notationIt = dtdState_->notationAttributes.find(elementNameString);
            notationIt != dtdState_->notationAttributes.end() && notationIt->second == attributeName) {
            return true;
        }
        if (const auto nmTokenIt = dtdState_->nmTokenAttributes.find(elementNameString);
            nmTokenIt != dtdState_->nmTokenAttributes.end()
                && nmTokenIt->second.find(std::string(attributeName)) != nmTokenIt->second.end()) {
            return true;
        }
        if (const auto nameIt = dtdState_->nameAttributes.find(elementNameString);
            nameIt != dtdState_->nameAttributes.end()
                && nameIt->second.find(std::string(attributeName)) != nameIt->second.end()) {
            return true;
        }
        if (const auto enumeratedIt = dtdState_->enumeratedAttributes.find(elementNameString);
            enumeratedIt != dtdState_->enumeratedAttributes.end()
                && enumeratedIt->second.find(std::string(attributeName)) != enumeratedIt->second.end()) {
            return true;
        }

        return false;
    };

    const auto normalizeAttributeValue = [&attributeUsesTokenizedType](std::string_view attributeName, std::string value) {
        return attributeUsesTokenizedType(attributeName)
            ? NormalizeXmlAttributeWhitespace(value)
            : value;
    };

    const auto attributeValueAt = [this, &attributes, &attributeValueMetadata, &normalizeAttributeValue](std::size_t index) {
        if (index >= attributes.size()) {
            return std::string{};
        }
        if (index >= attributeValueMetadata.size()) {
            return normalizeAttributeValue(attributes[index].first, attributes[index].second);
        }

        const auto& metadata = attributeValueMetadata[index];
        if ((metadata.flags & kAttributeValueDecoded) != 0
            || metadata.valueStart == std::string::npos
            || metadata.valueEnd == std::string::npos
            || metadata.valueEnd < metadata.valueStart) {
            return normalizeAttributeValue(attributes[index].first, attributes[index].second);
        }

        std::string value = SourceSubstr(metadata.valueStart, metadata.valueEnd - metadata.valueStart);
        if ((metadata.flags & kAttributeValueNeedsDecoding) != 0) {
            value = DecodeEntities(value);
        }
        return normalizeAttributeValue(attributes[index].first, std::move(value));
    };

    if (documentDeclarationStandalone_ == "yes") {
        const bool hasExternalRequired = dtdState_->externalRequiredAttributes.find(elementNameString)
            != dtdState_->externalRequiredAttributes.end();
        const bool hasExternalDefault = dtdState_->externalDefaultAttributes.find(elementNameString)
            != dtdState_->externalDefaultAttributes.end();
        const bool hasExternalFixed = dtdState_->externalFixedAttributes.find(elementNameString)
            != dtdState_->externalFixedAttributes.end();
        const bool hasExternalEnumerated = dtdState_->externalEnumeratedAttributes.find(elementNameString)
            != dtdState_->externalEnumeratedAttributes.end();
        if (hasExternalRequired || hasExternalDefault || hasExternalFixed || hasExternalEnumerated) {
            throw XmlException(
                "Standalone document declaration must not use external DTD attribute declarations");
        }
    }

    const auto fixedIt = dtdState_->fixedAttributes.find(elementNameString);
    if (fixedIt != dtdState_->fixedAttributes.end()) {
        for (const auto& [attributeName, fixedValue] : fixedIt->second) {
            for (std::size_t index = 0; index < attributes.size(); ++index) {
                if (attributes[index].first != attributeName) {
                    continue;
                }

                if (attributeValueAt(index) != fixedValue) {
                    throw XmlException(
                        "DTD validation failed: attribute '" + attributeName
                        + "' on element '" + elementNameString
                        + "' must match the fixed value '" + fixedValue + "'");
                }
                break;
            }
        }
    }

    const auto requiredIt = dtdState_->requiredAttributes.find(elementNameString);
    if (requiredIt != dtdState_->requiredAttributes.end()) {
        for (const auto& attributeName : requiredIt->second) {
            if (!hasAttribute(attributeName)) {
                throw XmlException(
                    "DTD validation failed: required attribute '" + attributeName
                    + "' is missing on element '" + elementNameString + "'");
            }
        }
    }

    const auto defaultIt = dtdState_->defaultAttributes.find(elementNameString);
    if (defaultIt != dtdState_->defaultAttributes.end()) {
        for (const auto& [attributeName, defaultValue] : defaultIt->second) {
            if (hasAttribute(attributeName)) {
                continue;
            }

            if (attributeName.find(':') != std::string::npos && attributeName.rfind("xmlns", 0) != 0) {
                retainLocalNamespaceDeclarationsForAttributes = true;
            }

            attributes.emplace_back(attributeName, defaultValue);
            attributeValueMetadata.push_back(AttributeValueMetadata{
                std::string::npos,
                std::string::npos,
                static_cast<unsigned char>(kAttributeValueDecoded)});
            if (attributeName == "xml:space" || IsNamespaceDeclarationName(attributeName)) {
                requiresAttributePostProcessing = true;
            }
        }
    }

    const auto enumeratedIt = dtdState_->enumeratedAttributes.find(elementNameString);
    const auto nmTokenIt = dtdState_->nmTokenAttributes.find(elementNameString);
    const auto nameIt = dtdState_->nameAttributes.find(elementNameString);
    const auto idIt = dtdState_->idAttributes.find(elementNameString);

    for (std::size_t index = 0; index < attributes.size(); ++index) {
        if (idIt != dtdState_->idAttributes.end() && attributes[index].first == idIt->second) {
            const std::string value = attributeValueAt(index);
            if (!IsValidDtdNcName(value)) {
                throw XmlException(
                    "DTD validation failed: attribute '" + attributes[index].first
                    + "' on element '" + elementNameString
                    + "' must be a valid ID value");
            }
            if (!dtdState_->seenIdAttributeValues.insert(value).second) {
                throw XmlException(
                    "DTD validation failed: attribute '" + attributes[index].first
                    + "' on element '" + elementNameString
                    + "' duplicates ID value '" + value + "'");
            }
        }

        if (nmTokenIt != dtdState_->nmTokenAttributes.end()) {
            const auto nmTokenDeclaration = nmTokenIt->second.find(attributes[index].first);
            if (nmTokenDeclaration != nmTokenIt->second.end()) {
                const std::string value = attributeValueAt(index);
                if (!IsValidDtdNmTokensValue(value, nmTokenDeclaration->second)) {
                    throw XmlException(
                        "DTD validation failed: attribute '" + attributes[index].first
                        + "' on element '" + elementNameString
                        + "' must be a valid "
                        + std::string(nmTokenDeclaration->second ? "NMTOKENS" : "NMTOKEN")
                        + " value");
                }
            }
        }

        if (nameIt != dtdState_->nameAttributes.end()) {
            const auto nameDeclaration = nameIt->second.find(attributes[index].first);
            if (nameDeclaration != nameIt->second.end()) {
                const std::string value = attributeValueAt(index);
                const bool allowsMultipleNames = nameDeclaration->second == "IDREFS" || nameDeclaration->second == "ENTITIES";
                const bool requiresNcName = nameDeclaration->second == "IDREF" || nameDeclaration->second == "IDREFS";
                if (!IsValidDtdNamesValue(value, allowsMultipleNames, requiresNcName)) {
                    throw XmlException(
                        "DTD validation failed: attribute '" + attributes[index].first
                        + "' on element '" + elementNameString
                        + "' must be a valid " + nameDeclaration->second + " value");
                }

                if (nameDeclaration->second == "ENTITY" || nameDeclaration->second == "ENTITIES") {
                    ForEachDtdNamesValueToken(value, [&](std::string_view entityName) {
                        if (dtdState_->unparsedEntityDeclarationNames.find(std::string(entityName))
                            == dtdState_->unparsedEntityDeclarationNames.end()) {
                            throw XmlException(
                                "DTD validation failed: attribute '" + attributes[index].first
                                + "' on element '" + elementNameString
                                + "' must reference a declared unparsed " + nameDeclaration->second + " value");
                        }
                    });
                } else if (nameDeclaration->second == "IDREF" || nameDeclaration->second == "IDREFS") {
                    ForEachDtdNamesValueToken(value, [&](std::string_view idValue) {
                        dtdState_->pendingIdAttributeReferences.push_back(PendingDtdIdReference{
                            std::string(idValue),
                            attributes[index].first,
                            elementNameString});
                    });
                }
            }
        }

    }

    if (enumeratedIt == dtdState_->enumeratedAttributes.end()) {
        return requiresAttributePostProcessing;
    }

    for (std::size_t index = 0; index < attributes.size(); ++index) {
        const auto enumerationDeclaration = enumeratedIt->second.find(attributes[index].first);
        if (enumerationDeclaration == enumeratedIt->second.end()) {
            continue;
        }

        const std::string value = attributeValueAt(index);
        if (!AttributeValueMatchesDtdEnumeration(value, enumerationDeclaration->second)) {
            throw XmlException(
                "DTD validation failed: attribute '" + attributes[index].first
                + "' on element '" + elementNameString
                + "' must match one of " + enumerationDeclaration->second);
        }
    }

    return requiresAttributePostProcessing;
}

void XmlReader::ValidatePendingDtdIdReferences() {
    if (dtdState_ == nullptr || dtdState_->pendingIdAttributeReferences.empty()) {
        return;
    }

    if (settings_.Validation == ValidationType::None) {
        dtdState_->pendingIdAttributeReferences.clear();
        return;
    }

    for (const auto& reference : dtdState_->pendingIdAttributeReferences) {
        if (dtdState_->seenIdAttributeValues.find(reference.value) == dtdState_->seenIdAttributeValues.end()) {
            throw XmlException(
                "DTD validation failed: attribute '" + reference.attributeName
                + "' on element '" + reference.elementName
                + "' references unknown ID value '" + reference.value + "'");
        }
    }

    dtdState_->pendingIdAttributeReferences.clear();
}

void XmlReader::FinalizeSuccessfulRead() {
    eof_ = false;
    MaybeDiscardSourcePrefix();
}

char XmlReader::Peek() const noexcept {
    return SourceCharAt(position_);
}

char XmlReader::ReadChar() {
    if (!HasSourceChar(position_)) {
        Throw("Unexpected end of XML document");
    }

    const char ch = SourceCharAt(position_++);
    ++totalCharactersRead_;
    if (settings_.MaxCharactersInDocument != 0 && totalCharactersRead_ > settings_.MaxCharactersInDocument) {
        Throw("The XML document exceeds the configured MaxCharactersInDocument limit");
    }
    if (ch == '\n') {
        ++lineNumber_;
        linePosition_ = 1;
    } else {
        ++linePosition_;
    }

    return ch;
}

bool XmlReader::StartsWith(std::string_view token) const noexcept {
    for (std::size_t index = 0; index < token.size(); ++index) {
        if (SourceCharAt(position_ + index) != token[index]) {
            return false;
        }
    }
    return true;
}

void XmlReader::SkipWhitespace() {
    while (HasSourceChar(position_) && IsWhitespace(SourceCharAt(position_))) {
        if (SourceCharAt(position_) == '\n') {
            ++lineNumber_;
            linePosition_ = 1;
        } else {
            ++linePosition_;
        }
        ++position_;
    }
}

std::string XmlReader::ParseName() {
    const auto start = position_;
    bool usedAsciiFastPath = false;
    std::size_t available = 0;
    if (const char* chunk = SourcePtrAt(start, available); chunk != nullptr && available != 0) {
        const unsigned char first = static_cast<unsigned char>(chunk[0]);
        if (first < 0x80 && IsNameStartChar(static_cast<char>(first))) {
            const std::size_t nameLength = 1 + ConsumeNameCharsInBuffer(chunk + 1, available - 1);
            if (nameLength < available && static_cast<unsigned char>(chunk[nameLength]) < 0x80) {
                position_ = start + nameLength;
                usedAsciiFastPath = true;
            }
        }
    }

    if (!usedAsciiFastPath) {
        position_ = ConsumeXmlNameAt(position_, [this](std::size_t index) noexcept {
            return SourceCharAt(index);
        });
        if (position_ == start) {
            Throw("Invalid XML name");
        }
    }

    const std::size_t nameLength = position_ - start;
    linePosition_ += nameLength;

    std::size_t internAvailable = 0;
    if (const char* ptr = SourcePtrAt(start, internAvailable); ptr != nullptr && internAvailable >= nameLength) {
        const std::string_view nameView(ptr, nameLength);
        if (nameTable_.Get(nameView) != nullptr) {
            return std::string(nameView);
        }
    }

    std::string name = SourceSubstr(start, nameLength);
    if (nameTable_.Get(name) != nullptr) {
        return name;
    }

    if (!usedAsciiFastPath) {
        try {
            (void)XmlConvert::VerifyName(name);
        } catch (const XmlException&) {
            position_ = start;
            Throw("Invalid XML name");
        }
    }

    return name;
}

std::string XmlReader::DecodeEntities(std::string_view value) const {
    if (dtdState_ != nullptr) {
        const_cast<XmlReader*>(this)->EnsureExternalSubsetDeclarationsLoaded();
    }
    const std::string decoded = DecodeEntityText(
        value,
        [this](const std::string& entity) -> std::optional<std::string> {
            if (!dtdState_) {
                return std::nullopt;
            }
            if (documentDeclarationStandalone_ == "yes"
                && dtdState_->externalSubsetEntityNames.find(entity) != dtdState_->externalSubsetEntityNames.end()) {
                Throw("Standalone document declaration must not use external general entity references");
            }
            const auto found = dtdState_->entityDeclarations.find(entity);
            return found == dtdState_->entityDeclarations.end() ? std::nullopt : std::optional<std::string>(found->second);
        },
        [this](const std::string& message) {
            Throw(message);
        });
    if (settings_.MaxCharactersFromEntities != 0) {
        const_cast<XmlReader*>(this)->entityCharactersRead_ += decoded.size();
        if (const_cast<XmlReader*>(this)->entityCharactersRead_ > settings_.MaxCharactersFromEntities) {
            Throw("The XML document exceeds the configured MaxCharactersFromEntities limit");
        }
    }
    return decoded;
}

void XmlReader::ValidateParsedEntityReplacementText(std::string_view rawReplacementText) const {
    if (rawReplacementText.find('<') == std::string_view::npos
        && rawReplacementText.find("&#") == std::string_view::npos) {
        return;
    }

    if (rawReplacementText.find('<') != std::string_view::npos
        && rawReplacementText.find('&') != std::string_view::npos) {
        XmlReaderSettings rawFragmentSettings = settings_;
        rawFragmentSettings.Conformance = ConformanceLevel::Fragment;
        rawFragmentSettings.Validation = ValidationType::None;

        auto rawFragmentSource = std::make_shared<const std::string>(std::string(rawReplacementText));
        XmlReader rawFragmentReader = CreateFromValidatedString(rawFragmentSource, rawFragmentSettings);
        if (dtdState_ != nullptr) {
            rawFragmentReader.dtdState_ = std::make_unique<DtdState>(*dtdState_);
        }
        rawFragmentReader.documentDeclarationStandalone_ = documentDeclarationStandalone_;
        rawFragmentReader.baseUri_ = baseUri_;
        rawFragmentReader.baseUriNeedsResolution_ = baseUriNeedsResolution_;
        rawFragmentReader.activeExternalEntityNames_ = activeExternalEntityNames_;

        while (rawFragmentReader.Read()) {
        }
    }

    const auto normalizeNumericReferences = [this](std::string_view value) {
        std::string normalized;
        normalized.reserve(value.size());

        std::size_t cursor = 0;
        while (cursor < value.size()) {
            const auto ampersand = value.find('&', cursor);
            if (ampersand == std::string_view::npos) {
                normalized.append(value.data() + cursor, value.size() - cursor);
                break;
            }

            normalized.append(value.data() + cursor, ampersand - cursor);
            if (ampersand + 1 < value.size() && value[ampersand + 1] == '#') {
                const auto semicolon = value.find(';', ampersand + 2);
                if (semicolon == std::string_view::npos) {
                    Throw("Unterminated entity reference");
                }

                const std::string_view entity = value.substr(ampersand + 1, semicolon - ampersand - 1);
                unsigned int codePoint = 0;
                if (!TryParseNumericEntityReferenceCodePoint(entity, codePoint)) {
                    Throw("Invalid numeric entity reference: &" + std::string(entity) + ';');
                }
                AppendCodePointUtf8(normalized, codePoint);
                cursor = semicolon + 1;
                continue;
            }

            normalized.push_back('&');
            cursor = ampersand + 1;
        }

        return normalized;
    };

    const std::string normalizedReplacementText = normalizeNumericReferences(rawReplacementText);
    if (normalizedReplacementText.find('<') == std::string::npos
        && normalizedReplacementText.find('&') == std::string::npos) {
        return;
    }

    XmlReaderSettings fragmentSettings = settings_;
    fragmentSettings.Conformance = ConformanceLevel::Fragment;
    fragmentSettings.Validation = ValidationType::None;

    auto fragmentSource = std::make_shared<const std::string>(normalizedReplacementText);
    XmlReader fragmentReader = CreateFromValidatedString(fragmentSource, fragmentSettings);
    if (dtdState_ != nullptr) {
        fragmentReader.dtdState_ = std::make_unique<DtdState>(*dtdState_);
    }
    fragmentReader.documentDeclarationStandalone_ = documentDeclarationStandalone_;
    fragmentReader.baseUri_ = baseUri_;
    fragmentReader.baseUriNeedsResolution_ = baseUriNeedsResolution_;
    fragmentReader.activeExternalEntityNames_ = activeExternalEntityNames_;

    while (fragmentReader.Read()) {
    }
}

void XmlReader::ValidateAttributeEntityReplacementText(std::string_view rawReplacementText) const {
    if (rawReplacementText.find('<') == std::string_view::npos
        && rawReplacementText.find('&') == std::string_view::npos
        && rawReplacementText.find("&#") == std::string_view::npos) {
        return;
    }

    std::string normalized;
    normalized.reserve(rawReplacementText.size());

    std::size_t cursor = 0;
    while (cursor < rawReplacementText.size()) {
        const auto ampersand = rawReplacementText.find('&', cursor);
        if (ampersand == std::string_view::npos) {
            normalized.append(rawReplacementText.data() + cursor, rawReplacementText.size() - cursor);
            break;
        }

        normalized.append(rawReplacementText.data() + cursor, ampersand - cursor);
        if (ampersand + 1 < rawReplacementText.size() && rawReplacementText[ampersand + 1] == '#') {
            const auto semicolon = rawReplacementText.find(';', ampersand + 2);
            if (semicolon == std::string_view::npos) {
                Throw("Unterminated entity reference");
            }

            const std::string_view entity = rawReplacementText.substr(ampersand + 1, semicolon - ampersand - 1);
            unsigned int codePoint = 0;
            if (!TryParseNumericEntityReferenceCodePoint(entity, codePoint)) {
                Throw("Invalid numeric entity reference: &" + std::string(entity) + ';');
            }
            AppendCodePointUtf8(normalized, codePoint);
            cursor = semicolon + 1;
            continue;
        }

        normalized.push_back('&');
        cursor = ampersand + 1;
    }

    if (normalized.find('<') != std::string::npos) {
        Throw("'<' is not allowed in attribute values");
    }

    const std::string expanded = DecodeEntityText(
        normalized,
        [this](const std::string& entity) -> std::optional<std::string> {
            if (!dtdState_) {
                return std::nullopt;
            }
            const auto found = dtdState_->entityDeclarations.find(entity);
            return found == dtdState_->entityDeclarations.end() ? std::nullopt : std::optional<std::string>(found->second);
        },
        [this](const std::string& message) {
            Throw(message);
        });

    if (expanded.find('<') != std::string::npos) {
        Throw("'<' is not allowed in attribute values");
    }
}

void XmlReader::QueueNode(
    XmlNodeType nodeType,
    std::string name,
    std::string namespaceUri,
    std::string value,
    int depth,
    bool isEmptyElement,
    std::string innerXml,
    std::string outerXml,
    std::size_t valueStart,
    std::size_t valueEnd,
    std::size_t nodeStart,
    std::size_t nodeEnd,
    std::vector<std::pair<std::string, std::string>> attributes,
    std::vector<std::string> attributeNamespaceUris,
    std::size_t elementStart,
    std::size_t contentStart,
    std::size_t closeStart,
    std::size_t closeEnd) {
    bufferedNodes_.push_back(BufferedNode{
        nodeType,
        std::move(name),
        std::move(namespaceUri),
        std::move(value),
        valueStart,
        valueEnd,
        depth,
        isEmptyElement,
        std::move(innerXml),
        std::move(outerXml),
        nodeStart,
        nodeEnd,
        std::move(attributes),
        std::move(attributeNamespaceUris),
        elementStart,
        contentStart,
        closeStart,
        closeEnd});
}

bool XmlReader::TryConsumeBufferedNode() {
    if (bufferedNodes_.empty()) {
        return false;
    }

    auto node = std::move(bufferedNodes_.front());
    bufferedNodes_.pop_front();
    SetCurrentNode(
        node.nodeType,
        std::move(node.name),
        std::move(node.namespaceUri),
        std::move(node.value),
        node.depth,
        node.isEmptyElement,
        std::move(node.innerXml),
        std::move(node.outerXml),
        node.valueStart,
        node.valueEnd,
        node.nodeStart,
        node.nodeEnd,
        std::move(node.attributes),
        std::move(node.attributeNamespaceUris),
        node.elementStart,
        node.contentStart,
        node.closeStart,
        node.closeEnd,
        true);
    return true;
}

std::string XmlReader::ParseQuotedValue(bool decodeEntities) {
    XmlReaderTokenizer tokenizer(inputSource_.get());
    const char quote = ReadChar();
    if (quote != '\"' && quote != '\'') {
        Throw("Expected quoted value");
    }

    const auto start = position_;
    const auto quoteEnd = tokenizer.ScanQuotedValueEnd(start - 1);
    if (quoteEnd == std::string::npos) {
        Throw("Unterminated quoted value");
    }

    const auto raw = tokenizer.Slice(start, quoteEnd - start);
    position_ = quoteEnd + 1;
    return decodeEntities ? DecodeEntities(raw) : raw;
}

void XmlReader::ResetCurrentNode() {
    currentNodeType_ = XmlNodeType::None;
    currentName_.clear();
    currentLocalName_.clear();
    currentPrefix_.clear();
    currentNamePartsResolved_ = false;
    currentNamespaceUri_.clear();
    currentValue_.clear();
    currentInnerXml_.clear();
    currentOuterXml_.clear();
    currentAttributes_.clear();
    currentAttributeLocalNames_.clear();
    currentAttributePrefixes_.clear();
    currentAttributeNamePartsResolved_.clear();
    currentAttributeNamespaceUrisResolved_.clear();
    currentAttributeValueMetadata_.clear();
    currentAttributeNamespaceUris_.clear();
    currentLocalNamespaceDeclarations_.clear();
    currentEarliestRetainedAttributeValueStart_ = std::string::npos;
    currentDeclarationVersion_.clear();
    currentDeclarationEncoding_.clear();
    currentDeclarationStandalone_.clear();
    currentValueStart_ = std::string::npos;
    currentValueEnd_ = std::string::npos;
    currentDepth_ = 0;
    currentIsEmptyElement_ = false;
    currentNodeStart_ = std::string::npos;
    currentNodeEnd_ = std::string::npos;
    currentElementStart_ = std::string::npos;
    currentContentStart_ = std::string::npos;
    currentCloseStart_ = std::string::npos;
    currentCloseEnd_ = std::string::npos;
}

void XmlReader::SetCurrentNode(
    XmlNodeType nodeType,
    std::string name,
    std::string namespaceUri,
    std::string value,
    int depth,
    bool isEmptyElement,
    std::string innerXml,
    std::string outerXml,
    std::size_t valueStart,
    std::size_t valueEnd,
    std::size_t nodeStart,
    std::size_t nodeEnd,
    std::vector<std::pair<std::string, std::string>> attributes,
    std::vector<std::string> attributeNamespaceUris,
    std::size_t elementStart,
    std::size_t contentStart,
    std::size_t closeStart,
    std::size_t closeEnd,
    bool namesAlreadyValidated) {
    if (nodeType == XmlNodeType::Element && !name.empty() && static_cast<unsigned char>(name[0]) == 0xCC) {
        name = "INVALID_NAME_MARKER";
    }

    if (!namesAlreadyValidated) {
        auto ensureValidXmlName = [this](std::string_view candidate) {
            if (candidate.empty()) {
                return;
            }

            if (nameTable_.Get(candidate) != nullptr) {
                return;
            }

            try {
                (void)XmlConvert::VerifyName(candidate);
            } catch (const XmlException&) {
                Throw("Invalid XML name");
            }
        };

        switch (nodeType) {
        case XmlNodeType::Element:
        case XmlNodeType::Attribute:
        case XmlNodeType::EndElement:
        case XmlNodeType::ProcessingInstruction:
        case XmlNodeType::DocumentType:
            ensureValidXmlName(name);
            break;
        default:
            break;
        }

        if (nodeType == XmlNodeType::Element) {
            for (const auto& attribute : attributes) {
                ensureValidXmlName(attribute.first);
            }
        }
    }

    if (!name.empty()) {
        nameTable_.Add(name);
    }
    if (!namespaceUri.empty()) {
        nameTable_.Add(namespaceUri);
    }
    for (const auto& attribute : attributes) {
        if (!attribute.first.empty()) {
            nameTable_.Add(attribute.first);
        }
    }
    for (const auto& attributeNamespaceUri : attributeNamespaceUris) {
        if (!attributeNamespaceUri.empty()) {
            nameTable_.Add(attributeNamespaceUri);
        }
    }
    currentNodeType_ = nodeType;
    currentName_ = std::move(name);
    currentLocalName_.clear();
    currentPrefix_.clear();
    currentNamePartsResolved_ = false;
    currentNamespaceUri_ = std::move(namespaceUri);
    currentValue_ = std::move(value);
    currentDepth_ = depth;
    currentIsEmptyElement_ = isEmptyElement;
    currentInnerXml_ = std::move(innerXml);
    currentOuterXml_ = std::move(outerXml);
    currentAttributes_ = std::move(attributes);
    currentAttributeLocalNames_.clear();
    currentAttributePrefixes_.clear();
    currentAttributeNamePartsResolved_.assign(currentAttributes_.size(), static_cast<unsigned char>(0));
    currentAttributeNamespaceUrisResolved_.assign(attributeNamespaceUris.size(), static_cast<unsigned char>(1));
    currentAttributeValueMetadata_.clear();
    currentAttributeNamespaceUris_ = std::move(attributeNamespaceUris);
    currentLocalNamespaceDeclarations_.clear();
    currentDeclarationVersion_.clear();
    currentDeclarationEncoding_.clear();
    currentDeclarationStandalone_.clear();
    currentValueStart_ = valueStart;
    currentValueEnd_ = valueEnd;
    currentNodeStart_ = nodeStart;
    currentNodeEnd_ = nodeEnd;
    currentElementStart_ = elementStart;
    currentContentStart_ = contentStart;
    currentCloseStart_ = closeStart;
    currentCloseEnd_ = closeEnd;
}

void XmlReader::SetCurrentNameParts(std::string_view prefix, std::string_view localName) {
    currentPrefix_.assign(prefix);
    currentLocalName_.assign(localName);
    currentNamePartsResolved_ = true;
}

void XmlReader::SetCurrentAttributeNameParts(std::vector<std::string> prefixes, std::vector<std::string> localNames) {
    currentAttributePrefixes_ = std::move(prefixes);
    currentAttributeLocalNames_ = std::move(localNames);
    currentAttributeNamePartsResolved_.assign(currentAttributes_.size(), static_cast<unsigned char>(1));
}

const XmlNameTable& XmlReader::NameTable() const noexcept {
    return nameTable_;
}

std::pair<std::size_t, std::size_t> XmlReader::ComputeLineColumn(std::size_t position) const noexcept {
    std::size_t scanStart = 0;
    std::size_t line = 1;
    std::size_t column = 1;
    if (position >= discardedSourceOffset_) {
        scanStart = discardedSourceOffset_;
        line = discardedLineNumber_;
        column = discardedLinePosition_;
    }

    if (position > scanStart && inputSource_ != nullptr) {
        const std::size_t firstNewline = FindFirstOfFromInputSource(inputSource_.get(), "\n", scanStart);
        if (firstNewline == std::string::npos || firstNewline >= position) {
            return {line, column + (position - scanStart)};
        }
    }

    for (std::size_t index = scanStart; index < position && HasSourceChar(index); ++index) {
        if (SourceCharAt(index) == '\n') {
            ++line;
            column = 1;
        } else {
            ++column;
        }
    }
    return {line, column};
}

[[noreturn]] void XmlReader::Throw(std::string_view message) const {
    const auto [line, column] = ComputeLineColumn(position_);
    throw XmlException(std::string(message), line, column);
}

void XmlReader::ParseDeclaration() {
    XmlReaderTokenizer tokenizer(inputSource_.get());
    const auto start = position_;
    if (!xmlDeclarationAllowed_) {
        position_ += 5;
        linePosition_ += 5;
        Throw("XML declaration is only allowed at the beginning of the document");
    }
    position_ += 5;
    linePosition_ += 5;

    std::string version = "1.0";
    std::string encoding;
    std::string standalone;
    bool sawVersion = false;
    bool sawEncoding = false;
    bool sawStandalone = false;

    while (true) {
        const auto beforeWhitespace = position_;
        SkipWhitespace();
        if (tokenizer.StartsWith(position_, "?>")) {
            position_ += 2;
            linePosition_ += 2;
            break;
        }
        if (position_ == beforeWhitespace) {
            Throw("Malformed XML declaration");
        }

        const auto attributeStart = position_;
        std::size_t cursor = position_;
        const auto attributeToken = tokenizer.ParseAttributeAssignment(cursor);
        position_ = cursor;
        AdvanceLineInfoFromInputSource(inputSource_.get(), attributeStart, position_, lineNumber_, linePosition_);
        if (attributeToken.name.empty()) {
            Throw("Invalid XML name");
        }

        if (!attributeToken.valid) {
            if (!attributeToken.sawEquals) {
                if (tokenizer.HasChar(position_)) {
                    ++linePosition_;
                    ++position_;
                }
                Throw("Expected '=' in XML declaration");
            }
            if (tokenizer.HasChar(position_)) {
                ++linePosition_;
                ++position_;
            }
            Throw("Expected quoted value");
        }

        const std::string name = attributeToken.name;
        const std::string value = DecodeEntities(SourceSubstr(attributeToken.rawValueStart, attributeToken.rawValueEnd - attributeToken.rawValueStart));

        if (name == "version") {
            if (sawEncoding || sawStandalone) {
                Throw("Malformed XML declaration");
            }
            if (sawVersion) {
                Throw("Malformed XML declaration");
            }
            sawVersion = true;
            ValidateXmlDeclarationVersion(value);
            version = value;
        } else if (name == "encoding") {
            if (!sawVersion || sawStandalone) {
                Throw("Malformed XML declaration");
            }
            if (sawEncoding) {
                Throw("Malformed XML declaration");
            }
            sawEncoding = true;
            ValidateXmlDeclarationEncoding(value);
            if (IsUtf16EncodingDeclaration(value) && !sourceWasUtf16_) {
                Throw("Malformed XML declaration");
            }
            if (sourceWasUtf16_ && !IsUtf16EncodingDeclaration(value)) {
                Throw("Malformed XML declaration");
            }
            if (sourceWasUtf8Bom_ && !IsUtf8EncodingDeclaration(value)) {
                Throw("Malformed XML declaration");
            }
            encoding = value;
        } else if (name == "standalone") {
            if (!sawVersion) {
                Throw("Malformed XML declaration");
            }
            if (sawStandalone) {
                Throw("Malformed XML declaration");
            }
            sawStandalone = true;
            if (value != "yes" && value != "no") {
                Throw("Malformed XML declaration");
            }
            standalone = value;
        } else {
            Throw("Unsupported XML declaration attribute: " + name);
        }
    }

    if (!sawVersion) {
        Throw("Malformed XML declaration");
    }

    XmlDeclaration declaration(version, encoding, standalone);
    documentDeclarationStandalone_ = declaration.Standalone();
    SetCurrentNode(
        XmlNodeType::XmlDeclaration,
        "xml",
        {},
        {},
        0,
        false,
        {},
        {},
        std::string::npos,
        std::string::npos,
        start,
        position_,
        {},
        {},
        std::string::npos,
        std::string::npos,
        std::string::npos,
        std::string::npos,
        true);
    SetCurrentNameParts({}, "xml");
    currentDeclarationVersion_ = declaration.Version();
    currentDeclarationEncoding_ = declaration.Encoding();
    currentDeclarationStandalone_ = declaration.Standalone();
    xmlDeclarationAllowed_ = false;
}

void XmlReader::ParseDocumentType() {
    XmlReaderTokenizer tokenizer(inputSource_.get());
    if (settings_.DtdProcessing == DtdProcessing::Prohibit) {
        Throw("DTD is prohibited in this XML document");
    }

    const auto start = position_;
    position_ += 9;
    linePosition_ += 9;
    SkipWhitespace();

    const std::string name = ParseName();
    SkipWhitespace();

    std::string publicId;
    std::string systemId;
    std::string internalSubset;

    dtdState_.reset();

    if (tokenizer.StartsWith(position_, "PUBLIC")) {
        position_ += 6;
        linePosition_ += 6;
        const auto publicWhitespaceStart = position_;
        SkipWhitespace();
        if (position_ == publicWhitespaceStart
            && HasSourceChar(position_)
            && (SourceCharAt(position_) == '\'' || SourceCharAt(position_) == '"')) {
            Throw("Malformed DOCTYPE declaration");
        }
        const auto publicLiteralStart = position_;
        std::size_t cursor = position_;
        auto publicIdToken = tokenizer.ParseQuotedLiteral(cursor);
        position_ = cursor;
        AdvanceLineInfoFromInputSource(inputSource_.get(), publicLiteralStart, position_, lineNumber_, linePosition_);
        if (!publicIdToken.valid) {
            if (tokenizer.HasChar(position_)) {
                ++linePosition_;
                ++position_;
            }
            Throw("Expected quoted value");
        }
        publicId = publicIdToken.rawValue;
        if (!IsValidPublicIdentifierLiteral(publicId)) {
            Throw("Malformed DOCTYPE declaration");
        }

        const auto betweenIdsStart = position_;
        tokenizer.SkipWhitespace(position_);
        AdvanceLineInfoFromInputSource(inputSource_.get(), betweenIdsStart, position_, lineNumber_, linePosition_);
        if (position_ == betweenIdsStart
            && HasSourceChar(position_)
            && (SourceCharAt(position_) == '\'' || SourceCharAt(position_) == '"')) {
            Throw("Malformed DOCTYPE declaration");
        }
        const auto systemLiteralStart = position_;
        cursor = position_;
        auto systemIdToken = tokenizer.ParseQuotedLiteral(cursor);
        position_ = cursor;
        AdvanceLineInfoFromInputSource(inputSource_.get(), systemLiteralStart, position_, lineNumber_, linePosition_);
        if (!systemIdToken.valid) {
            if (tokenizer.HasChar(position_)) {
                ++linePosition_;
                ++position_;
            }
            Throw("Expected quoted value");
        }
        systemId = systemIdToken.rawValue;
        SkipWhitespace();
    } else if (tokenizer.StartsWith(position_, "SYSTEM")) {
        position_ += 6;
        linePosition_ += 6;
        SkipWhitespace();
        const auto systemLiteralStart = position_;
        std::size_t cursor = position_;
        auto systemIdToken = tokenizer.ParseQuotedLiteral(cursor);
        position_ = cursor;
        AdvanceLineInfoFromInputSource(inputSource_.get(), systemLiteralStart, position_, lineNumber_, linePosition_);
        if (!systemIdToken.valid) {
            if (tokenizer.HasChar(position_)) {
                ++linePosition_;
                ++position_;
            }
            Throw("Expected quoted value");
        }
        systemId = systemIdToken.rawValue;
        SkipWhitespace();
    }

    if (!systemId.empty()) {
        EnsureDtdState().externalSubsetSystemId = systemId;
        dtdState_->externalSubsetDeclarationsLoaded = false;
    }

    std::unordered_map<std::string, std::string> parameterEntityDeclarations;
    std::unordered_map<std::string, std::string> externalParameterEntitySystemLiterals;
    std::unordered_set<std::string> externalParameterEntityNames;

    if (Peek() == '[') {
        ++position_;
        ++linePosition_;
        const auto subsetStart = position_;
        const auto subsetEnd = tokenizer.ScanDocumentTypeInternalSubsetEnd(subsetStart);
        if (subsetEnd == std::string::npos) {
            Throw("Unterminated DOCTYPE internal subset");
        }
        position_ = subsetEnd + 1;
        AdvanceLineInfoFromInputSource(inputSource_.get(), subsetStart, position_, lineNumber_, linePosition_);
        internalSubset = Trim(SourceSubstr(subsetStart, subsetEnd - subsetStart));
        if (settings_.DtdProcessing == DtdProcessing::Parse) {
            CollectDtdParameterEntityDeclarations(
                internalSubset,
                parameterEntityDeclarations,
                false,
                &externalParameterEntitySystemLiterals);
            LoadExternalParameterEntityDeclarations(
                externalParameterEntitySystemLiterals,
                baseUri_,
                parameterEntityDeclarations,
                externalParameterEntityNames);
            ValidateInternalSubsetParameterEntityPlacement(internalSubset);
            ValidateTopLevelParameterEntityReferenceBoundaries(
                internalSubset,
                parameterEntityDeclarations,
                false,
                &externalParameterEntityNames);
            const std::string expandedInternalSubset = ExpandDtdParameterEntityReferences(
                internalSubset,
                parameterEntityDeclarations,
                false,
                &externalParameterEntityNames);
            const bool allowConditionalSections = !externalParameterEntityNames.empty();
            std::vector<std::shared_ptr<XmlNode>> entities;
            std::vector<std::shared_ptr<XmlNode>> notations;
            ParseDocumentTypeInternalSubset(expandedInternalSubset, entities, notations, allowConditionalSections);
            RegisterDtdDeclarations(entities, notations);

            DtdRequiredAttributeDeclarations requiredAttributes;
            DtdDefaultAttributeDeclarations defaultAttributes;
            DtdFixedAttributeDeclarations fixedAttributes;
            DtdEnumeratedAttributeDeclarations enumeratedAttributes;
            DtdElementContentDeclarations elementContentDeclarations;
            DtdDeclaredAttributeNames declaredAttributes;
            DtdIdAttributeDeclarations idAttributes;
            DtdNotationAttributeDeclarations notationAttributes;
            DtdNmTokenAttributeDeclarations nmTokenAttributes;
            DtdNameAttributeDeclarations nameAttributes;
            ParseDocumentTypeElementDeclarations(expandedInternalSubset, elementContentDeclarations, allowConditionalSections);
            ParseDocumentTypeAttributeDeclarations(expandedInternalSubset, declaredAttributes, requiredAttributes, defaultAttributes, fixedAttributes, enumeratedAttributes, idAttributes, notationAttributes, nmTokenAttributes, nameAttributes, allowConditionalSections);
            ValidateDtdDeclaredAttributeValueReferenceOrder(expandedInternalSubset, allowConditionalSections);
            RegisterDtdElementDeclarations(elementContentDeclarations);
            RegisterDtdAttributeDeclarations(declaredAttributes, idAttributes, notationAttributes, nmTokenAttributes, nameAttributes, requiredAttributes, defaultAttributes, fixedAttributes, enumeratedAttributes);
        }
        SkipWhitespace();
    }

    if (ReadChar() != '>') {
        Throw("Expected '>' after DOCTYPE");
    }

    const auto declaration = ParseDocumentTypeDeclaration(SourceSubstr(start, position_ - start));
    if (settings_.DtdProcessing == DtdProcessing::Parse && settings_.Resolver != nullptr && !declaration.systemId.empty()) {
        EnsureDtdState().externalSubsetSystemId = declaration.systemId;
        if (!dtdState_->externalSubsetDeclarationsLoaded
            || (dtdState_->declaredEntityNames.empty() && dtdState_->parsedEntities.empty() && dtdState_->parsedNotations.empty())) {
            LoadExternalSubsetDeclarations(declaration.systemId, &parameterEntityDeclarations);
        }
    }

    if ((dtdState_ == nullptr || (dtdState_->declaredEntityNames.empty()
            && dtdState_->parsedEntities.empty() && dtdState_->parsedNotations.empty()))
        && settings_.DtdProcessing == DtdProcessing::Parse && settings_.Resolver != nullptr) {
        LoadExternalSubsetDeclarations(declaration.systemId, &parameterEntityDeclarations);
    }

    SetCurrentNode(
        XmlNodeType::DocumentType,
        name,
        {},
        {},
        static_cast<int>(elementStack_.size()),
        false,
        {},
        {},
        std::string::npos,
        std::string::npos,
        start,
        position_,
        {},
        {},
        std::string::npos,
        std::string::npos,
        std::string::npos,
        std::string::npos,
        true);
    SetCurrentNameParts({}, currentName_);
    documentTypeName_ = currentName_;
    sawDocumentType_ = true;
}

void XmlReader::ParseProcessingInstruction() {
    XmlReaderTokenizer tokenizer(inputSource_.get());
    if (!elementStack_.empty() && dtdState_ != nullptr
        && dtdState_->emptyElementDeclarations.find(elementStack_.back()) != dtdState_->emptyElementDeclarations.end()) {
        throw XmlException(
            "DTD validation failed: element '" + elementStack_.back() + "' declared EMPTY must not contain processing instructions");
    }
    const auto start = position_;
    position_ += 2;
    linePosition_ += 2;
    const std::string target = ParseName();
    if (!IsNamespaceAwareProcessingInstructionTarget(target)) {
        Throw("Invalid XML name");
    }
    if (IsReservedXmlProcessingTarget(target)) {
        Throw("XML declaration is only allowed at the beginning of the document");
    }
    if (HasSourceChar(position_) && !StartsWith("?>") && !IsWhitespace(Peek())) {
        Throw("Malformed processing instruction");
    }
    std::size_t dataStart = std::string::npos;
    std::size_t dataEnd = std::string::npos;
    if (!StartsWith("?>")) {
        const auto end = tokenizer.ScanDelimitedSectionEnd(position_, "?>");
        if (end == std::string::npos) {
            Throw("Unterminated processing instruction");
        }
        for (std::size_t probe = position_; probe < end;) {
            probe += ValidateXmlCharacterAt(probe);
        }
        dataStart = position_;
        dataEnd = end;
        while (dataStart < dataEnd && IsWhitespace(SourceCharAt(dataStart))) {
            ++dataStart;
        }
        while (dataEnd > dataStart && IsWhitespace(SourceCharAt(dataEnd - 1))) {
            --dataEnd;
        }
        const auto lineInfoStart = position_;
        position_ = end;
        AdvanceLineInfoFromInputSource(inputSource_.get(), lineInfoStart, position_, lineNumber_, linePosition_);
    }
    position_ += 2;
    linePosition_ += 2;

    SetCurrentNode(
        XmlNodeType::ProcessingInstruction,
        target,
        {},
        {},
        static_cast<int>(elementStack_.size()),
        false,
        {},
        {},
        dataStart,
        dataEnd,
        start,
        position_,
        {},
        {},
        std::string::npos,
        std::string::npos,
        std::string::npos,
        std::string::npos,
        true);
    {
        const auto [prefixView, localNameView] = SplitQualifiedNameView(currentName_);
        SetCurrentNameParts(prefixView, localNameView);
    }
}

void XmlReader::ParseComment() {
    XmlReaderTokenizer tokenizer(inputSource_.get());
    if (!elementStack_.empty() && dtdState_ != nullptr
        && dtdState_->emptyElementDeclarations.find(elementStack_.back()) != dtdState_->emptyElementDeclarations.end()) {
        throw XmlException(
            "DTD validation failed: element '" + elementStack_.back() + "' declared EMPTY must not contain comments");
    }
    const auto start = position_;
    const auto end = tokenizer.ScanDelimitedSectionEnd(position_ + 4, "-->");
    if (end == std::string::npos) {
        const auto [line, column] = ComputeLineColumn(position_ + 4);
        throw XmlException("Unterminated comment", line, column);
    }
    const auto valueStart = position_ + 4;
    for (std::size_t probe = valueStart; probe < end;) {
        probe += ValidateXmlCharacterAt(probe);
    }
    const std::string commentValue = SourceSubstr(valueStart, end - valueStart);
    const auto invalidDoubleHyphen = commentValue.find("--");
    if (invalidDoubleHyphen != std::string::npos) {
            const auto [line, column] = ComputeLineColumn(valueStart + invalidDoubleHyphen);
            throw XmlException("Comment may not contain '--'", line, column);
    }
    if (end > valueStart && SourceCharAt(end - 1) == '-') {
        const auto [line, column] = ComputeLineColumn(end - 1);
        throw XmlException("Comment may not contain '--'", line, column);
    }
    const auto valueEnd = end;
    position_ = end + 3;
    AdvanceLineInfoFromInputSource(inputSource_.get(), start, position_, lineNumber_, linePosition_);

    SetCurrentNode(XmlNodeType::Comment, {}, {}, {}, static_cast<int>(elementStack_.size()), false, {}, {}, valueStart, valueEnd, start, position_, {}, {}, std::string::npos, std::string::npos, std::string::npos, std::string::npos, true);
}

void XmlReader::ParseCData() {
    XmlReaderTokenizer tokenizer(inputSource_.get());
    const auto start = position_;
    const auto end = tokenizer.ScanDelimitedSectionEnd(position_ + 9, "]]>");
    if (end == std::string::npos) {
        const auto [line, column] = ComputeLineColumn(position_ + 9);
        throw XmlException("Unterminated CDATA section", line, column);
    }
    const auto valueStart = position_ + 9;
    const auto valueEnd = end;
    for (std::size_t probe = valueStart; probe < valueEnd;) {
        probe += ValidateXmlCharacterAt(probe);
    }
    if (!elementStack_.empty() && dtdState_ != nullptr
        && dtdState_->emptyElementDeclarations.find(elementStack_.back()) != dtdState_->emptyElementDeclarations.end()) {
        throw XmlException(
            "DTD validation failed: element '" + elementStack_.back() + "' declared EMPTY must not contain character data");
    }
    position_ = end + 3;
    AdvanceLineInfoFromInputSource(inputSource_.get(), start, position_, lineNumber_, linePosition_);

    bool hasNonWhitespaceContent = false;
    for (std::size_t probe = valueStart; probe < valueEnd; ++probe) {
        if (!IsWhitespace(SourceCharAt(probe))) {
            hasNonWhitespaceContent = true;
            break;
        }
    }
    if (valueEnd > valueStart) {
        RecordTextForDtdValidation(!hasNonWhitespaceContent, false);
    }

    SetCurrentNode(XmlNodeType::CDATA, {}, {}, {}, static_cast<int>(elementStack_.size()), false, {}, {}, valueStart, valueEnd, start, position_, {}, {}, std::string::npos, std::string::npos, std::string::npos, std::string::npos, true);
}

void XmlReader::ParseText() {
    XmlReaderTokenizer tokenizer(inputSource_.get());
    if (dtdState_ != nullptr) {
        EnsureExternalSubsetDeclarationsLoaded();
    }
    if (!elementStack_.empty() && dtdState_ != nullptr
        && dtdState_->emptyElementDeclarations.find(elementStack_.back()) != dtdState_->emptyElementDeclarations.end()) {
        throw XmlException(
            "DTD validation failed: element '" + elementStack_.back() + "' declared EMPTY must not contain character data");
    }
    const int textDepth = static_cast<int>(elementStack_.size());
    const bool preserveSpace = !xmlSpacePreserveStack_.empty() && xmlSpacePreserveStack_.back();
    std::string textBuffer;
    std::size_t rawSegmentStart = std::string::npos;
    bool textBufferMaterialized = false;
    bool rawSegmentKnownNonWhitespace = false;
    bool textBufferIncludesNonSWhitespace = false;

    auto scanAsciiTextChunk = [this](std::size_t start, bool& sawNonWhitespace) {
        std::size_t available = 0;
        const char* chunk = SourcePtrAt(start, available);
        if (chunk == nullptr || available == 0) {
            return start;
        }

        std::size_t offset = 0;
        while (offset < available) {
            const unsigned char ch = static_cast<unsigned char>(chunk[offset]);
            if (ch == '<' || ch == '&' || ch == '\n' || ch == ']') {
                break;
            }
            if (ch >= 0x80) {
                break;
            }
            if (!(ch == 0x09 || ch == 0x0D || ch >= 0x20)) {
                const auto [line, column] = ComputeLineColumn(start + offset);
                throw XmlException("Invalid XML character", line, column);
            }
            if (!sawNonWhitespace && !IsWhitespace(static_cast<char>(ch))) {
                sawNonWhitespace = true;
            }
            ++offset;
        }

        return start + offset;
    };

    auto flushText = [&](std::size_t segmentEnd) {
        if (rawSegmentStart == std::string::npos) {
            return;
        }

        bool isWhitespaceOnly = false;
        if (textBufferMaterialized) {
            isWhitespaceOnly = IsWhitespaceOnly(textBuffer);
        } else {
            isWhitespaceOnly = !rawSegmentKnownNonWhitespace;
        }

        XmlNodeType nodeType = XmlNodeType::Text;
        if (isWhitespaceOnly) {
            nodeType = preserveSpace ? XmlNodeType::SignificantWhitespace : XmlNodeType::Whitespace;
        }

        if (textBufferMaterialized) {
            QueueNode(nodeType, {}, {}, textBuffer, textDepth, false, {}, {}, std::string::npos, std::string::npos, rawSegmentStart, segmentEnd);
        } else {
            QueueNode(nodeType, {}, {}, {}, textDepth, false, {}, {}, rawSegmentStart, segmentEnd, rawSegmentStart, segmentEnd);
        }
        if (segmentEnd > rawSegmentStart) {
            RecordTextForDtdValidation(isWhitespaceOnly, isWhitespaceOnly && !textBufferIncludesNonSWhitespace);
        }
        textBuffer.clear();
        rawSegmentStart = std::string::npos;
        textBufferMaterialized = false;
        rawSegmentKnownNonWhitespace = false;
        textBufferIncludesNonSWhitespace = false;
    };

    while (HasSourceChar(position_) && SourceCharAt(position_) != '<') {
        if (tokenizer.StartsWith(position_, "]]>") ) {
            const auto [line, column] = ComputeLineColumn(position_);
            throw XmlException("']]>' is not allowed in text content", line, column);
        }

        if (SourceCharAt(position_) != '&') {
            std::size_t segmentEnd = position_;
            bool segmentHasNonWhitespace = false;
            while (HasSourceChar(segmentEnd)) {
                const std::size_t asciiSegmentEnd = scanAsciiTextChunk(segmentEnd, segmentHasNonWhitespace);
                if (asciiSegmentEnd != segmentEnd) {
                    segmentEnd = asciiSegmentEnd;
                    continue;
                }

                const char segmentChar = SourceCharAt(segmentEnd);
                if (segmentChar == '\0'
                    || segmentChar == '\n'
                    || segmentChar == '<'
                    || segmentChar == '&') {
                    break;
                }
                if (segmentChar == ']'
                    && tokenizer.StartsWith(segmentEnd, "]]>") ) {
                    const auto [line, column] = ComputeLineColumn(segmentEnd);
                    throw XmlException("']]>' is not allowed in text content", line, column);
                }
                if (!segmentHasNonWhitespace && !IsWhitespace(segmentChar)) {
                    segmentHasNonWhitespace = true;
                }
                segmentEnd += ValidateXmlCharacterAt(segmentEnd);
            }

            if (segmentEnd > position_) {
                if (rawSegmentStart == std::string::npos) {
                    rawSegmentStart = position_;
                }
                if (!rawSegmentKnownNonWhitespace && segmentHasNonWhitespace) {
                    rawSegmentKnownNonWhitespace = true;
                }
                if (textBufferMaterialized) {
                    AppendSourceSubstrTo(textBuffer, position_, segmentEnd - position_);
                }
                linePosition_ += segmentEnd - position_;
                position_ = segmentEnd;
                continue;
            }

            if (rawSegmentStart == std::string::npos) {
                rawSegmentStart = position_;
            }
            if (!rawSegmentKnownNonWhitespace && !IsWhitespace(SourceCharAt(position_))) {
                rawSegmentKnownNonWhitespace = true;
            }
            if (textBufferMaterialized) {
                textBuffer.push_back(SourceCharAt(position_));
            }
            if (SourceCharAt(position_) == '\n') {
                ++lineNumber_;
                linePosition_ = 1;
            } else {
                ++linePosition_;
            }
            ++position_;
            continue;
        }

        const auto entityStart = position_;
        const auto entityToken = tokenizer.ScanEntityReference(position_);
        if (entityToken.end == std::string::npos) {
            Throw("Unterminated entity reference");
        }

        const std::string rawEntity = entityToken.raw;
        const std::string entity = entityToken.name;
        position_ = entityToken.end;
        linePosition_ += entityToken.end - entityStart;

        const auto predefined = ResolvePredefinedEntityReferenceValueView(entity);
        if (!predefined.empty()) {
            if (rawSegmentStart == std::string::npos) {
                rawSegmentStart = entityStart;
            }
            if (!textBufferMaterialized) {
                textBuffer = SourceSubstr(rawSegmentStart, entityStart - rawSegmentStart);
                textBufferMaterialized = true;
            }
            textBuffer.append(predefined.data(), predefined.size());
            continue;
        }

        if (!entity.empty() && entity.front() == '#') {
            if (rawSegmentStart == std::string::npos) {
                rawSegmentStart = entityStart;
            }
            if (!textBufferMaterialized) {
                textBuffer = SourceSubstr(rawSegmentStart, entityStart - rawSegmentStart);
                textBufferMaterialized = true;
            }
            unsigned int codePoint = 0;
            if (!TryParseNumericEntityReferenceCodePoint(entity, codePoint)) {
                Throw("Invalid numeric entity reference: &" + entity + ';');
            }
            AppendCodePointUtf8(textBuffer, codePoint);
            if (IsXmlWhitespaceCodePoint(codePoint)) {
                textBufferIncludesNonSWhitespace = true;
            }
            continue;
        }

        if (!dtdState_ || dtdState_->declaredEntityNames.find(entity) == dtdState_->declaredEntityNames.end()) {
            ThrowUnknownEntityReference(entity);
        }

        flushText(entityStart);

        std::string resolvedValue;
        const auto declared = dtdState_->entityDeclarations.find(entity);
        if (declared != dtdState_->entityDeclarations.end()) {
            if (documentDeclarationStandalone_ == "yes"
                && dtdState_->externalSubsetEntityNames.find(entity) != dtdState_->externalSubsetEntityNames.end()) {
                Throw("Standalone document declaration must not use external general entity references");
            }
            ValidateParsedEntityReplacementText(declared->second);
            resolvedValue = DecodeEntityText(
                declared->second,
                [this](const std::string& nestedEntity) -> std::optional<std::string> {
                    const auto found = dtdState_->entityDeclarations.find(nestedEntity);
                    return found == dtdState_->entityDeclarations.end() ? std::nullopt : std::optional<std::string>(found->second);
                },
                [this](const std::string& message) {
                    Throw(message);
                });
            if (settings_.MaxCharactersFromEntities != 0) {
                entityCharactersRead_ += resolvedValue.size();
                if (entityCharactersRead_ > settings_.MaxCharactersFromEntities) {
                    Throw("The XML document exceeds the configured MaxCharactersFromEntities limit");
                }
            }
        } else if (settings_.Resolver != nullptr) {
            const auto external = dtdState_->externalEntitySystemIds.find(entity);
            if (external != dtdState_->externalEntitySystemIds.end()) {
                if (documentDeclarationStandalone_ == "yes") {
                    throw XmlException(
                        "Standalone document declaration must not use external general entity references");
                }
                if (std::find(activeExternalEntityNames_.begin(), activeExternalEntityNames_.end(), entity)
                    != activeExternalEntityNames_.end()) {
                    Throw("Entity reference cycle detected: &" + entity + ';');
                }
                const std::string absoluteUri = settings_.Resolver->ResolveUri(baseUri_, external->second);
                const std::string rawResolvedValue = NormalizeBomEncodedXmlText(settings_.Resolver->GetEntity(absoluteUri));
                if (settings_.MaxCharactersFromEntities != 0) {
                    entityCharactersRead_ += rawResolvedValue.size();
                    if (entityCharactersRead_ > settings_.MaxCharactersFromEntities) {
                        Throw("The XML document exceeds the configured MaxCharactersFromEntities limit");
                    }
                }

                activeExternalEntityNames_.push_back(entity);
                try {
                    std::string_view validatedValue(rawResolvedValue);
                    const std::size_t textDeclarationEnd = ConsumeLeadingTextDeclarationIfPresent(validatedValue);
                    if (textDeclarationEnd != 0) {
                        validatedValue.remove_prefix(textDeclarationEnd);
                    }

                    if (!validatedValue.empty()) {
                        XmlReaderSettings fragmentSettings = settings_;
                        fragmentSettings.Conformance = ConformanceLevel::Fragment;
                        fragmentSettings.Validation = ValidationType::None;

                        auto fragmentSource = std::make_shared<const std::string>(std::string(validatedValue));
                        XmlReader fragmentReader = CreateFromValidatedString(fragmentSource, fragmentSettings);
                        if (dtdState_ != nullptr) {
                            fragmentReader.dtdState_ = std::make_unique<DtdState>(*dtdState_);
                        }
                        fragmentReader.documentDeclarationStandalone_ = documentDeclarationStandalone_;
                        fragmentReader.baseUri_ = baseUri_;
                        fragmentReader.baseUriNeedsResolution_ = baseUriNeedsResolution_;
                        fragmentReader.activeExternalEntityNames_ = activeExternalEntityNames_;

                        while (fragmentReader.Read()) {
                        }
                    }

                    resolvedValue.assign(validatedValue.data(), validatedValue.size());
                } catch (...) {
                    activeExternalEntityNames_.pop_back();
                    throw;
                }
                activeExternalEntityNames_.pop_back();
            }
        }

        QueueNode(XmlNodeType::EntityReference, entity, {}, resolvedValue, textDepth, false, {}, {}, std::string::npos, std::string::npos, entityStart, entityToken.end);
        if (!resolvedValue.empty()) {
            const XmlNodeType resolvedNodeType = IsWhitespaceOnly(resolvedValue)
                ? (preserveSpace ? XmlNodeType::SignificantWhitespace : XmlNodeType::Whitespace)
                : XmlNodeType::Text;
            QueueNode(resolvedNodeType, {}, {}, resolvedValue, textDepth + 1, false, {}, resolvedValue);
            if (resolvedValue.find('<') == std::string::npos) {
                bool isWhitespaceOnly = false;
                bool matchesElementContentWhitespace = false;
                if (TryClassifyElementContentTextValue(resolvedValue, isWhitespaceOnly, matchesElementContentWhitespace)) {
                    RecordTextForDtdValidation(isWhitespaceOnly, matchesElementContentWhitespace);
                }
            }
        }
        QueueNode(XmlNodeType::EndEntity, entity, {}, {}, textDepth, false, {}, {}, std::string::npos, std::string::npos, entityStart, entityToken.end);
    }

    flushText(position_);
    if (!TryConsumeBufferedNode()) {
        Throw("Unexpected empty text segment");
    }
}

bool XmlReader::TryReadSimpleElementContentAsString(std::string& result, std::size_t& closeStart, std::size_t& closeEnd) {
    if (inputSource_ == nullptr
        || currentElementStart_ == std::string::npos
        || currentContentStart_ == std::string::npos
        || currentIsEmptyElement_) {
        return false;
    }

    const auto bounds = EnsureCurrentElementXmlBounds();
    closeStart = bounds.first;
    closeEnd = bounds.second;
    if (closeStart == std::string::npos || closeEnd == std::string::npos || closeStart < currentContentStart_) {
        return false;
    }

    XmlReaderTokenizer tokenizer(inputSource_.get());
    result.clear();
    result.reserve(closeStart - currentContentStart_);

    std::size_t position = currentContentStart_;
    while (position < closeStart) {
        const std::size_t nextSpecial = FindFirstOfFromInputSource(inputSource_.get(), "<&", position);
        const std::size_t segmentEnd = nextSpecial == std::string::npos || nextSpecial > closeStart
            ? closeStart
            : nextSpecial;

        if (segmentEnd > position) {
            AppendSourceSubstrTo(result, position, segmentEnd - position);
            position = segmentEnd;
            continue;
        }

        if (position >= closeStart) {
            break;
        }

        if (SourceCharAt(position) == '&') {
            const auto entityToken = tokenizer.ScanEntityReference(position);
            if (entityToken.end == std::string::npos) {
                return false;
            }

            const std::string entity = entityToken.name;
            const auto predefined = ResolvePredefinedEntityReferenceValueView(entity);
            if (!predefined.empty()) {
                result.append(predefined.data(), predefined.size());
                position = entityToken.end;
                continue;
            }

            if (!entity.empty() && entity.front() == '#') {
                unsigned int codePoint = 0;
                if (!TryParseNumericEntityReferenceCodePoint(entity, codePoint)) {
                    return false;
                }
                AppendCodePointUtf8(result, codePoint);
                position = entityToken.end;
                continue;
            }

            return false;
        }

        if (tokenizer.StartsWith(position, "<![CDATA[")) {
            const auto end = tokenizer.ScanDelimitedSectionEnd(position + 9, "]]>");
            if (end == std::string::npos || end > closeStart) {
                return false;
            }
            AppendSourceSubstrTo(result, position + 9, end - (position + 9));
            position = end + 3;
            continue;
        }

        if (tokenizer.StartsWith(position, "<!--")) {
            const auto end = tokenizer.ScanDelimitedSectionEnd(position + 4, "-->");
            if (end == std::string::npos || end > closeStart) {
                return false;
            }
            position = end + 3;
            continue;
        }

        if (tokenizer.StartsWith(position, "<?")) {
            const auto end = tokenizer.ScanDelimitedSectionEnd(position + 2, "?>");
            if (end == std::string::npos || end > closeStart) {
                return false;
            }
            position = end + 2;
            continue;
        }

        return false;
    }

    return position == closeStart;
}

std::pair<std::size_t, std::size_t> XmlReader::EnsureCurrentElementXmlBounds() const {
    if (currentCloseStart_ == std::string::npos || currentCloseEnd_ == std::string::npos) {
        const auto bounds = FindElementXmlBounds(currentElementStart_, currentContentStart_, currentName_);
        currentCloseStart_ = bounds.first;
        currentCloseEnd_ = bounds.second;
    }
    return {currentCloseStart_, currentCloseEnd_};
}

std::pair<std::size_t, std::size_t> XmlReader::FindElementXmlBounds(
    std::size_t,
    std::size_t contentStart,
    std::string_view elementName) const {
    XmlReaderTokenizer tokenizer(inputSource_.get());
    std::vector<std::string> elementNames;
    elementNames.push_back(std::string(elementName));

    auto throwAt = [this](const std::string& message, std::size_t errorPosition) -> void {
        const auto [line, column] = ComputeLineColumn(errorPosition);
        throw XmlException(message, line, column);
    };

    std::size_t position = contentStart;
    std::size_t closeStart = std::string::npos;
    std::size_t closeEnd = std::string::npos;

    // Fast path for the common text-only case: the first markup after content start
    // is the matching closing tag for the current element.
    const std::size_t firstMarkup = FindInSource("<", contentStart);
    if (firstMarkup != std::string::npos && tokenizer.StartsWith(firstMarkup, "</")) {
        std::size_t closeCursor = firstMarkup + 2;
        const std::string closeName = tokenizer.ParseNameAt(closeCursor);
        if (closeName == elementName && tokenizer.ConsumeEndTagClose(closeCursor)) {
            return {firstMarkup, closeCursor};
        }
    }

    while (HasSourceChar(position)) {
        if (SourceCharAt(position) != '<') {
            ++position;
            continue;
        }

        if (tokenizer.StartsWith(position, "<!--")) {
            const auto end = tokenizer.ScanDelimitedSectionEnd(position + 4, "-->");
            if (end == std::string::npos) {
                throwAt("Unterminated comment", position + 4);
            }
            position = end + 3;
            continue;
        }

        if (tokenizer.StartsWith(position, "<![CDATA[")) {
            const auto end = tokenizer.ScanDelimitedSectionEnd(position + 9, "]]>");
            if (end == std::string::npos) {
                throwAt("Unterminated CDATA section", position + 9);
            }
            position = end + 3;
            continue;
        }

        if (tokenizer.StartsWith(position, "<?")) {
            const auto end = tokenizer.ScanDelimitedSectionEnd(position + 2, "?>");
            if (end == std::string::npos) {
                std::size_t errorPosition = position + 2;
                const std::size_t nameStart = errorPosition;
                std::string target = tokenizer.ParseNameAt(errorPosition);
                if (target.empty()) {
                    errorPosition = nameStart;
                }
                throwAt("Unterminated processing instruction", errorPosition);
            }
            position = end + 2;
            continue;
        }

        if (tokenizer.StartsWith(position, "</")) {
            closeStart = position;
            position += 2;
            const std::string closeName = tokenizer.ParseNameAt(position);
            if (closeName.empty()) {
                break;
            }

            const std::size_t errorPosition = position;
            while (HasSourceChar(position) && SourceCharAt(position) != '>') {
                ++position;
            }
            if (!HasSourceChar(position)) {
                break;
            }
            ++position;

            if (elementNames.empty()) {
                throwAt("Unexpected closing tag: </" + closeName + ">", errorPosition);
            }
            if (closeName != elementNames.back()) {
                throwAt(
                    "Mismatched closing tag. Expected </" + elementNames.back() + "> but found </" + closeName + ">",
                    errorPosition);
            }

            elementNames.pop_back();
            if (elementNames.empty()) {
                closeEnd = position;
                break;
            }
            continue;
        }

        if (tokenizer.StartsWith(position, "<!")) {
            const auto end = tokenizer.ScanDelimitedSectionEnd(position + 2, ">");
            if (end == std::string::npos) {
                break;
            }
            position = end + 1;
            continue;
        }

        std::size_t namePosition = position + 1;
        const std::string openName = tokenizer.ParseNameAt(namePosition);
        bool isEmptyElement = false;
        if (!tokenizer.SkipTag(position, isEmptyElement)) {
            break;
        }
        if (!isEmptyElement) {
            elementNames.push_back(openName);
        }
    }

    if (closeStart == std::string::npos || closeEnd == std::string::npos) {
        if (!elementNames.empty()) {
            throwAt("Unexpected end of input inside element <" + elementNames.back() + ">", position);
        }
        Throw("Unterminated element");
    }

    return {closeStart, closeEnd};
}

std::pair<std::string, std::string> XmlReader::CaptureElementXml(
    std::size_t elementStart,
    std::size_t contentStart) const {
    const auto [closeStart, closeEnd] = EnsureCurrentElementXmlBounds();
    return {
        SourceSubstr(contentStart, closeStart - contentStart),
        SourceSubstr(elementStart, closeEnd - elementStart)};
}

void XmlReader::ParseElement() {
    XmlReaderTokenizer tokenizer(inputSource_.get());
    const auto start = position_;
    if (dtdState_ != nullptr) {
        EnsureExternalSubsetDeclarationsLoaded();
    }
    ReadChar();
    std::string name = ParseName();
    if (!elementStack_.empty() && dtdState_ != nullptr
        && dtdState_->emptyElementDeclarations.find(elementStack_.back()) != dtdState_->emptyElementDeclarations.end()) {
        throw XmlException(
            "DTD validation failed: element '" + elementStack_.back() + "' declared EMPTY must not contain child elements");
    }
    if (dtdState_ != nullptr
        && !dtdState_->elementContentDeclarations.empty()
        && dtdState_->elementContentDeclarations.find(name) == dtdState_->elementContentDeclarations.end()) {
        throw XmlException("DTD validation failed: element '" + name + "' was not declared");
    }
    const auto [elementPrefixView, elementLocalNameView] = SplitQualifiedNameView(name);
    if (!IsValidXmlQualifiedName(name) || elementPrefixView == "xmlns") {
        Throw("Invalid XML name");
    }
    const bool topLevelElement = elementStack_.empty();
    std::vector<std::pair<std::string, std::string>> attributes;
    std::vector<AttributeValueMetadata> attributeValueMetadata;
    std::vector<std::pair<std::string, std::string>> localNamespaceDeclarations;
    bool retainLocalNamespaceDeclarationsForAttributes = false;
    const bool inheritedPreserveSpace = !xmlSpacePreserveStack_.empty() && xmlSpacePreserveStack_.back();
    bool preserveSpace = inheritedPreserveSpace;

    auto lookupNamespaceInScopes = [this, &localNamespaceDeclarations](std::string_view prefix) {
        for (auto it = localNamespaceDeclarations.rbegin(); it != localNamespaceDeclarations.rend(); ++it) {
            if (it->first == prefix) {
                return it->second;
            }
        }
        return LookupNamespaceUri(prefix);
    };

    auto setLocalNamespaceDeclaration = [&localNamespaceDeclarations](std::string prefix, std::string namespaceUri) {
        for (auto it = localNamespaceDeclarations.rbegin(); it != localNamespaceDeclarations.rend(); ++it) {
            if (it->first == prefix) {
                it->second = std::move(namespaceUri);
                return;
            }
        }

        localNamespaceDeclarations.emplace_back(std::move(prefix), std::move(namespaceUri));
    };

    auto sourceRangeEqualsLiteral = [this](std::size_t rangeStart, std::size_t rangeEnd, std::string_view literal) {
        if (rangeStart == std::string::npos || rangeEnd == std::string::npos || rangeEnd < rangeStart) {
            return false;
        }

        const std::size_t length = rangeEnd - rangeStart;
        if (length != literal.size()) {
            return false;
        }

        for (std::size_t index = 0; index < length; ++index) {
            if (SourceCharAt(rangeStart + index) != literal[index]) {
                return false;
            }
        }

        return true;
    };

    auto applyXmlSpaceValue = [this, &preserveSpace](std::string_view value) {
        if (value == "preserve") {
            preserveSpace = true;
            return;
        }
        if (value == "default") {
            preserveSpace = false;
            return;
        }

        Throw("xml:space must be 'default' or 'preserve'");
    };

    auto applyXmlSpaceFromAttributes = [this, &attributes, &attributeValueMetadata, &sourceRangeEqualsLiteral, &applyXmlSpaceValue]() {
        for (std::size_t index = 0; index < attributes.size(); ++index) {
            if (attributes[index].first != "xml:space") {
                continue;
            }

            const auto& metadata = attributeValueMetadata[index];
            if (metadata.valueStart == std::string::npos || metadata.valueEnd == std::string::npos) {
                applyXmlSpaceValue(attributes[index].second);
                continue;
            }

            if ((metadata.flags & kAttributeValueNeedsDecoding) == 0) {
                if (sourceRangeEqualsLiteral(metadata.valueStart, metadata.valueEnd, "preserve")) {
                    applyXmlSpaceValue("preserve");
                } else if (sourceRangeEqualsLiteral(metadata.valueStart, metadata.valueEnd, "default")) {
                    applyXmlSpaceValue("default");
                } else {
                    Throw("xml:space must be 'default' or 'preserve'");
                }
                continue;
            }

            std::string attributeValue = SourceSubstr(metadata.valueStart, metadata.valueEnd - metadata.valueStart);
            attributeValue = DecodeEntities(attributeValue);
            applyXmlSpaceValue(attributeValue);
        }
    };

    auto validateAttributeEntityReplacementTexts = [this, &tokenizer](std::size_t valueStart, std::size_t valueEnd) {
        if (dtdState_ == nullptr || valueStart == std::string::npos || valueEnd == std::string::npos || valueEnd <= valueStart) {
            return;
        }

        std::size_t scan = valueStart;
        while (scan < valueEnd) {
            const auto ampersand = FindInSource("&", scan);
            if (ampersand == std::string::npos || ampersand >= valueEnd) {
                break;
            }

            const auto entityToken = tokenizer.ScanEntityReference(ampersand);
            if (entityToken.end == std::string::npos || entityToken.end > valueEnd) {
                Throw("Unterminated entity reference");
            }

            const std::string entity = entityToken.name;
            if (ResolvePredefinedEntityReferenceValueView(entity).empty()
                && (entity.empty() || entity.front() != '#')) {
                const auto declared = dtdState_->entityDeclarations.find(entity);
                if (declared != dtdState_->entityDeclarations.end()) {
                    ValidateAttributeEntityReplacementText(declared->second);
                }
            }

            scan = entityToken.end;
        }
    };

    auto namespaceDeclarationUsesTokenizedType = [this, &name](std::string_view attributeName) {
        if (dtdState_ == nullptr) {
            return false;
        }

        const std::string elementName(name);
        if (const auto idIt = dtdState_->idAttributes.find(elementName);
            idIt != dtdState_->idAttributes.end() && idIt->second == attributeName) {
            return true;
        }
        if (const auto notationIt = dtdState_->notationAttributes.find(elementName);
            notationIt != dtdState_->notationAttributes.end() && notationIt->second == attributeName) {
            return true;
        }
        if (const auto nmTokenIt = dtdState_->nmTokenAttributes.find(elementName);
            nmTokenIt != dtdState_->nmTokenAttributes.end()
                && nmTokenIt->second.find(std::string(attributeName)) != nmTokenIt->second.end()) {
            return true;
        }
        if (const auto nameIt = dtdState_->nameAttributes.find(elementName);
            nameIt != dtdState_->nameAttributes.end()
                && nameIt->second.find(std::string(attributeName)) != nameIt->second.end()) {
            return true;
        }
        if (const auto enumeratedIt = dtdState_->enumeratedAttributes.find(elementName);
            enumeratedIt != dtdState_->enumeratedAttributes.end()
                && enumeratedIt->second.find(std::string(attributeName)) != enumeratedIt->second.end()) {
            return true;
        }

        return false;
    };

    auto normalizeNamespaceDeclarationValue = [&namespaceDeclarationUsesTokenizedType](std::string_view attributeName, std::string value) {
        if (namespaceDeclarationUsesTokenizedType(attributeName)) {
            return NormalizeXmlAttributeWhitespace(value);
        }
        return value;
    };

    auto materializeNamespaceDeclarationValue = [this, &attributes, &attributeValueMetadata, &normalizeNamespaceDeclarationValue](std::size_t index) {
        if (index >= attributes.size()) {
            return std::string{};
        }
        if (index >= attributeValueMetadata.size()) {
            return normalizeNamespaceDeclarationValue(attributes[index].first, attributes[index].second);
        }

        const auto& metadata = attributeValueMetadata[index];
        if (metadata.valueStart == std::string::npos || metadata.valueEnd == std::string::npos) {
            return normalizeNamespaceDeclarationValue(attributes[index].first, attributes[index].second);
        }

        std::string value = SourceSubstr(metadata.valueStart, metadata.valueEnd - metadata.valueStart);
        if ((metadata.flags & kAttributeValueNeedsDecoding) != 0) {
            value = DecodeEntities(value);
        }
        return normalizeNamespaceDeclarationValue(attributes[index].first, std::move(value));
    };

    auto applyNamespaceDeclarationAttributes = [this, &attributes, &attributeValueMetadata, &localNamespaceDeclarations, &setLocalNamespaceDeclaration, &materializeNamespaceDeclarationValue]() {
        for (std::size_t index = 0; index < attributes.size(); ++index) {
            const std::string& attributeName = attributes[index].first;
            if (!IsNamespaceDeclarationName(attributeName)) {
                continue;
            }

            const std::string_view prefix = NamespaceDeclarationPrefixView(attributeName);
            bool alreadyBound = false;
            for (const auto& entry : localNamespaceDeclarations) {
                if (std::string_view(entry.first) == prefix) {
                    alreadyBound = true;
                    break;
                }
            }
            if (alreadyBound) {
                continue;
            }

            std::string value = materializeNamespaceDeclarationValue(index);
            ValidateNamespaceDeclarationBinding(prefix, value);
            setLocalNamespaceDeclaration(std::string(prefix), std::move(value));
        }
    };

    auto validateNamespaceAwareNames = [this, &attributes, &lookupNamespaceInScopes](std::string_view currentElementPrefix) {
        if (!currentElementPrefix.empty() && lookupNamespaceInScopes(currentElementPrefix).empty()) {
            Throw("Undeclared namespace prefix '" + std::string(currentElementPrefix) + "'");
        }

        std::vector<std::pair<std::string, std::string>> expandedAttributeNames;
        expandedAttributeNames.reserve(attributes.size());
        for (const auto& attribute : attributes) {
            const std::string& attributeName = attribute.first;
            const auto [attributePrefix, attributeLocalName] = SplitQualifiedNameView(attributeName);
            if (!IsValidXmlQualifiedName(attributeName)
                || (attributePrefix == "xmlns" && !IsNamespaceDeclarationName(attributeName))) {
                Throw("Invalid XML name");
            }

            std::string namespaceUri;
            if (attributeName == "xmlns" || attributePrefix == "xmlns") {
                namespaceUri = "http://www.w3.org/2000/xmlns/";
            } else if (!attributePrefix.empty()) {
                namespaceUri = lookupNamespaceInScopes(attributePrefix);
                if (namespaceUri.empty()) {
                    Throw("Undeclared namespace prefix '" + std::string(attributePrefix) + "'");
                }
            }

            for (const auto& expanded : expandedAttributeNames) {
                if (expanded.first == namespaceUri && expanded.second == attributeLocalName) {
                    Throw("Duplicate attribute '" + attributeName + "'");
                }
            }
            expandedAttributeNames.emplace_back(std::move(namespaceUri), std::string(attributeLocalName));
        }
    };

    auto validateDtdDeclaredAttributes = [this, &attributes, &name]() {
        if (dtdState_ == nullptr || dtdState_->elementContentDeclarations.empty()) {
            return;
        }

        const auto elementDeclarations = dtdState_->declaredAttributes.find(name);
        for (const auto& attribute : attributes) {
            const bool declared = elementDeclarations != dtdState_->declaredAttributes.end()
                && elementDeclarations->second.find(attribute.first) != elementDeclarations->second.end();
            if (!declared) {
                throw XmlException(
                    "DTD validation failed: attribute '" + attribute.first
                    + "' on element '" + name + "' was not declared");
            }
        }
    };

    while (true) {
        const auto whitespaceStart = position_;
        tokenizer.SkipWhitespace(position_);
        AdvanceLineInfoFromInputSource(inputSource_.get(), whitespaceStart, position_, lineNumber_, linePosition_);
        bool isEmptyElement = false;
        const auto closeStart = position_;
        if (tokenizer.ConsumeStartTagClose(position_, isEmptyElement)) {
            AdvanceLineInfoFromInputSource(inputSource_.get(), closeStart, position_, lineNumber_, linePosition_);
            if (isEmptyElement) {
                const bool requiresAttributePostProcessing = ApplyDtdAttributeDeclarations(
                    name,
                    attributes,
                    attributeValueMetadata,
                    retainLocalNamespaceDeclarationsForAttributes);
                if (requiresAttributePostProcessing) {
                    applyNamespaceDeclarationAttributes();
                    applyXmlSpaceFromAttributes();
                }
                if (topLevelElement && !documentTypeName_.empty() && name != documentTypeName_) {
                    Throw("Document type name '" + documentTypeName_ + "' does not match root element name '" + name + "'");
                }
                validateNamespaceAwareNames(elementPrefixView);
                validateDtdDeclaredAttributes();
                if (dtdState_ != nullptr
                    && !dtdState_->elementContentDeclarations.empty()
                    && !topLevelElement) {
                    RecordChildElementForDtdValidation(name);
                }
                if (dtdState_ != nullptr && !dtdState_->elementContentDeclarations.empty()) {
                    ValidateElementDtdContent(name, ElementContentValidationFrame{});
                }
                std::string nsUri = lookupNamespaceInScopes(elementPrefixView);
                SetCurrentNode(
                    XmlNodeType::Element,
                    std::move(name),
                    std::move(nsUri),
                    {},
                    static_cast<int>(elementStack_.size()),
                    true,
                    {},
                    {},
                    std::string::npos,
                    std::string::npos,
                    start,
                    position_,
                    std::move(attributes),
                    {},
                    start,
                    position_,
                    std::string::npos,
                    std::string::npos,
                    true);
                currentLocalNamespaceDeclarations_ = localNamespaceDeclarations;
                currentAttributeValueMetadata_ = std::move(attributeValueMetadata);
                RefreshCurrentEarliestRetainedAttributeValueStart();
                if (topLevelElement) {
                    sawRootElement_ = true;
                    completedRootElement_ = true;
                }
                return;
            }
            break;
        }

        if (!tokenizer.HasChar(position_)) {
            Throw("Unexpected end of XML document");
        }
        if (position_ == whitespaceStart) {
            Throw("Whitespace is required between attributes");
        }

        const auto attributeStart = position_;
        auto attributeToken = tokenizer.ParseAttributeAssignment(position_);
        AdvanceLineInfoFromInputSource(inputSource_.get(), attributeStart, position_, lineNumber_, linePosition_);
        if (attributeToken.name.empty()) {
            if (tokenizer.CharAt(position_) == '/' || tokenizer.CharAt(position_) == '>') {
                Throw("Expected '>' after element attributes");
            }
            Throw("Invalid XML name");
        }
        if (!attributeToken.valid) {
            if (!attributeToken.sawEquals) {
                if (tokenizer.HasChar(position_)) {
                    ++position_;
                }
                Throw("Expected '=' after attribute name");
            }
            if (tokenizer.HasChar(position_)) {
                ++position_;
            }
            Throw("Expected quoted value");
        }
        const auto rawValueStart = attributeToken.rawValueStart;
        const auto rawValueEnd = attributeToken.rawValueEnd;
        const bool needsDecoding = (attributeToken.rawValueFlags & kAttributeValueNeedsDecoding) != 0;
        const std::size_t invalidCharacterPosition = attributeToken.invalidCharacterPosition;
        const std::size_t invalidLt = attributeToken.invalidLtPosition;
        if (invalidCharacterPosition != std::string::npos
            && (invalidLt == std::string::npos || invalidCharacterPosition < invalidLt)) {
            const auto [line, column] = ComputeLineColumn(invalidCharacterPosition);
            throw XmlException("Invalid XML character", line, column);
        }
        if (invalidLt != std::string::npos) {
            const auto [line, column] = ComputeLineColumn(invalidLt);
            throw XmlException("'<' is not allowed in attribute values", line, column);
        }
        if (std::any_of(attributes.begin(), attributes.end(), [&](const auto& existingAttribute) {
                return existingAttribute.first == attributeToken.name;
            })) {
            Throw("Duplicate attribute '" + attributeToken.name + "'");
        }
        if (attributes.empty()) {
            attributes.reserve(4);
            attributeValueMetadata.reserve(4);
        }
        attributes.emplace_back(std::move(attributeToken.name), std::string{});
        const std::string& attributeName = attributes.back().first;
        const auto attributeColon = attributeName.find(':');
        if (attributeColon != std::string::npos
            && std::string_view(attributeName).substr(0, attributeColon) != "xmlns") {
            retainLocalNamespaceDeclarationsForAttributes = true;
        }
        attributeValueMetadata.push_back(AttributeValueMetadata{
            rawValueStart,
            rawValueEnd,
            static_cast<unsigned char>(needsDecoding ? kAttributeValueNeedsDecoding : 0)});
        if (needsDecoding
            && attributeName != "xmlns"
            && attributeName.rfind("xmlns:", 0) != 0
            && attributeName != "xml:space") {
            validateAttributeEntityReplacementTexts(rawValueStart, rawValueEnd);
            (void)DecodeEntities(SourceSubstr(rawValueStart, rawValueEnd - rawValueStart));
        }
        if (attributeName == "xmlns") {
            std::string attributeValue = SourceSubstr(rawValueStart, rawValueEnd - rawValueStart);
            if (needsDecoding) {
                attributeValue = DecodeEntities(attributeValue);
            }
            attributeValue = normalizeNamespaceDeclarationValue(attributeName, std::move(attributeValue));
            ValidateNamespaceDeclarationBinding({}, attributeValue);
            setLocalNamespaceDeclaration({}, std::move(attributeValue));
        } else if (attributeName.rfind("xmlns:", 0) == 0) {
            std::string attributeValue = SourceSubstr(rawValueStart, rawValueEnd - rawValueStart);
            if (needsDecoding) {
                attributeValue = DecodeEntities(attributeValue);
            }
            attributeValue = normalizeNamespaceDeclarationValue(attributeName, std::move(attributeValue));
            ValidateNamespaceDeclarationBinding(attributeName.substr(6), attributeValue);
            setLocalNamespaceDeclaration(attributeName.substr(6), std::move(attributeValue));
        } else if (attributeName == "xml:space") {
            if (!needsDecoding) {
                if (sourceRangeEqualsLiteral(rawValueStart, rawValueEnd, "preserve")) {
                    applyXmlSpaceValue("preserve");
                } else if (sourceRangeEqualsLiteral(rawValueStart, rawValueEnd, "default")) {
                    applyXmlSpaceValue("default");
                } else {
                    Throw("xml:space must be 'default' or 'preserve'");
                }
            } else {
                std::string attributeValue = SourceSubstr(rawValueStart, rawValueEnd - rawValueStart);
                attributeValue = DecodeEntities(attributeValue);
                applyXmlSpaceValue(attributeValue);
            }
        }
    }

    const bool requiresAttributePostProcessing = ApplyDtdAttributeDeclarations(
        name,
        attributes,
        attributeValueMetadata,
        retainLocalNamespaceDeclarationsForAttributes);
    if (requiresAttributePostProcessing) {
        applyNamespaceDeclarationAttributes();
        applyXmlSpaceFromAttributes();
    }
    if (topLevelElement && !documentTypeName_.empty() && name != documentTypeName_) {
        Throw("Document type name '" + documentTypeName_ + "' does not match root element name '" + name + "'");
    }
    validateNamespaceAwareNames(elementPrefixView);
    validateDtdDeclaredAttributes();

    const bool pushedNamespaceScope = !localNamespaceDeclarations.empty();
    const std::string elementNamespaceUri = lookupNamespaceInScopes(elementPrefixView);
    currentLocalNamespaceDeclarations_ = localNamespaceDeclarations;
    if (pushedNamespaceScope) {
        namespaceScopes_.push_back(std::move(localNamespaceDeclarations));
    }
    if (dtdState_ != nullptr
        && !dtdState_->elementContentDeclarations.empty()
        && !topLevelElement) {
        RecordChildElementForDtdValidation(name);
    }
    elementStack_.push_back(name);
    if (dtdState_ != nullptr && !dtdState_->elementContentDeclarations.empty()) {
        elementContentValidationStack_.push_back(ElementContentValidationFrame{});
    }
    SetCurrentNode(
        XmlNodeType::Element,
        std::move(name),
        std::move(elementNamespaceUri),
        {},
        static_cast<int>(elementStack_.size()) - 1,
        false,
        {},
        {},
        std::string::npos,
        std::string::npos,
        start,
        position_,
        std::move(attributes),
        {},
        start,
        position_,
        std::string::npos,
        std::string::npos,
        true);
    currentAttributeValueMetadata_ = std::move(attributeValueMetadata);
    RefreshCurrentEarliestRetainedAttributeValueStart();
    const bool pushedXmlSpacePreserve = preserveSpace != inheritedPreserveSpace;
    if (pushedXmlSpacePreserve) {
        xmlSpacePreserveStack_.push_back(preserveSpace);
    }
    namespaceScopeFramePushedStack_.push_back(pushedNamespaceScope);
    xmlSpacePreserveFramePushedStack_.push_back(pushedXmlSpacePreserve);
    if (topLevelElement) {
        sawRootElement_ = true;
    }
}

void XmlReader::ParseEndElement() {
    XmlReaderTokenizer tokenizer(inputSource_.get());
    const auto start = position_;
    std::size_t cursor = position_ + 2;
    const std::size_t nameStart = cursor;
    if (!TryConsumeNameFromInputSourceAt(inputSource_.get(), cursor)) {
        Throw("Expected element name after </");
    }
    const std::size_t nameEnd = cursor;
    const std::size_t nameLen = nameEnd - nameStart;
    position_ = cursor;

    if (elementStack_.empty()) {
        const std::string name = SourceSubstr(nameStart, nameLen);
        Throw("Unexpected closing tag: </" + name + ">");
    }

    {
        const std::string name = SourceSubstr(nameStart, nameLen);
        const auto [prefixView, localNameView] = SplitQualifiedNameView(name);
        if (!IsValidXmlQualifiedName(name) || prefixView == "xmlns") {
            Throw("Invalid XML name");
        }
    }

    const std::string& expected = elementStack_.back();
    bool nameMatch = (expected.size() == nameLen);
    if (nameMatch) {
        std::size_t available = 0;
        const char* ptr = inputSource_->PtrAt(nameStart, available);
        if (ptr != nullptr && available >= nameLen) {
            nameMatch = (std::memcmp(ptr, expected.data(), nameLen) == 0);
        } else {
            for (std::size_t i = 0; i < nameLen; ++i) {
                if (inputSource_->CharAt(nameStart + i) != expected[i]) {
                    nameMatch = false;
                    break;
                }
            }
        }
    }
    if (!nameMatch) {
        const std::string name = SourceSubstr(nameStart, nameLen);
        Throw("Mismatched closing tag. Expected </" + expected + "> but found </" + name + ">");
    }

    if (!tokenizer.ConsumeEndTagClose(cursor)) {
        const std::size_t errorPosition = tokenizer.HasChar(cursor) ? cursor + 1 : cursor;
        const auto [line, column] = ComputeLineColumn(errorPosition);
        throw XmlException("Expected '>' after closing tag", line, column);
    }
    position_ = cursor;
    AdvanceLineInfoFromInputSource(inputSource_.get(), start, position_, lineNumber_, linePosition_);

    const int depth = static_cast<int>(elementStack_.size()) - 1;
    if (!elementContentValidationStack_.empty()) {
        ValidateElementDtdContent(expected, elementContentValidationStack_.back());
    }
    std::string name = std::move(elementStack_.back());
    elementStack_.pop_back();
    if (!elementContentValidationStack_.empty()) {
        elementContentValidationStack_.pop_back();
    }

    const auto [prefixView, localNameView] = SplitQualifiedNameView(name);
    const std::string namespaceUri = LookupNamespaceUri(prefixView);

    bool popNamespaceScope = false;
    if (!namespaceScopeFramePushedStack_.empty()) {
        popNamespaceScope = namespaceScopeFramePushedStack_.back();
        namespaceScopeFramePushedStack_.pop_back();
    }
    std::vector<std::pair<std::string, std::string>> endElementLocalNamespaceDeclarations;
    if (popNamespaceScope && namespaceScopes_.size() > 1) {
        endElementLocalNamespaceDeclarations = namespaceScopes_.back();
    }
    bool popXmlSpacePreserve = false;
    if (!xmlSpacePreserveFramePushedStack_.empty()) {
        popXmlSpacePreserve = xmlSpacePreserveFramePushedStack_.back();
        xmlSpacePreserveFramePushedStack_.pop_back();
    }
    if (popNamespaceScope && namespaceScopes_.size() > 1) {
        namespaceScopes_.pop_back();
    }
    if (popXmlSpacePreserve && xmlSpacePreserveStack_.size() > 1) {
        xmlSpacePreserveStack_.pop_back();
    }
    if (elementStack_.empty() && sawRootElement_) {
        completedRootElement_ = true;
    }

    SetCurrentNode(
        XmlNodeType::EndElement,
        std::move(name),
        namespaceUri,
        {},
        depth,
        false,
        {},
        {},
        std::string::npos,
        std::string::npos,
        start,
        position_,
        {},
        {},
        std::string::npos,
        std::string::npos,
        std::string::npos,
        std::string::npos,
        true);
    currentLocalNamespaceDeclarations_ = std::move(endElementLocalNamespaceDeclarations);
}

std::string XmlReader::LookupNamespaceUri(std::string_view prefix) const {
    for (auto it = namespaceScopes_.rbegin(); it != namespaceScopes_.rend(); ++it) {
        for (auto jt = it->rbegin(); jt != it->rend(); ++jt) {
            if (jt->first == prefix) {
                return jt->second;
            }
        }
    }

    if (prefix == "xml") {
        return "http://www.w3.org/XML/1998/namespace";
    }
    if (prefix == "xmlns") {
        return "http://www.w3.org/2000/xmlns/";
    }

    return {};
}

const std::vector<std::pair<std::string, std::string>>& XmlReader::CurrentAttributes() const {
    return currentAttributes_;
}

void XmlReader::RecordChildElementForDtdValidation(std::string_view childName) {
    if (dtdState_ != nullptr
        && !dtdState_->elementContentDeclarations.empty()
        && !elementContentValidationStack_.empty()) {
        elementContentValidationStack_.back().childElements.emplace_back(childName);
    }
}

void XmlReader::RecordTextForDtdValidation(bool isWhitespaceOnly, bool matchesElementContentWhitespace) {
    if (dtdState_ != nullptr
        && !dtdState_->elementContentDeclarations.empty()
        && !elementContentValidationStack_.empty()) {
        auto& frame = elementContentValidationStack_.back();
        if (isWhitespaceOnly) {
            frame.hasWhitespaceText = true;
            if (!matchesElementContentWhitespace) {
                frame.hasNonSWhitespaceText = true;
            }
            return;
        }

        frame.hasNonWhitespaceText = true;
    }
}

void XmlReader::ValidateElementDtdContent(
    std::string_view elementName,
    const ElementContentValidationFrame& frame) const {
    if (dtdState_ == nullptr) {
        return;
    }

    if (dtdState_->elementContentDeclarations.empty()) {
        return;
    }

    const auto declaration = dtdState_->elementContentDeclarations.find(std::string(elementName));
    if (declaration == dtdState_->elementContentDeclarations.end()) {
        throw XmlException("DTD validation failed: element '" + std::string(elementName) + "' was not declared");
    }

    const std::string& contentSpecification = declaration->second;
    if (contentSpecification == "ANY") {
        return;
    }

    if (contentSpecification == "EMPTY") {
        if (frame.hasWhitespaceText || frame.hasNonWhitespaceText || !frame.childElements.empty()) {
            throw XmlException(
                "DTD validation failed: element '" + std::string(elementName) + "' content does not match declaration");
        }
        return;
    }

    std::unordered_set<std::string> mixedAllowedChildren;
    if (TryParseMixedContentSpecification(contentSpecification, mixedAllowedChildren)) {
        for (const auto& childName : frame.childElements) {
            if (mixedAllowedChildren.find(childName) == mixedAllowedChildren.end()) {
                throw XmlException(
                    "DTD validation failed: element '" + std::string(elementName) + "' content does not match declaration");
            }
        }
        return;
    }

    const bool declarationIsExternal = documentDeclarationStandalone_ == "yes"
        && dtdState_->externalElementDeclarationNames.find(std::string(elementName)) != dtdState_->externalElementDeclarationNames.end();
    if (frame.hasNonWhitespaceText
        || frame.hasNonSWhitespaceText
        || (declarationIsExternal && frame.hasWhitespaceText)
        || !DoesObservedChildrenMatchContentModel(contentSpecification, frame.childElements)) {
        throw XmlException(
            "DTD validation failed: element '" + std::string(elementName) + "' content does not match declaration");
    }
}

std::string XmlReader::CurrentLocalName() const {
    if (!currentNamePartsResolved_) {
        const auto [prefixView, localNameView] = SplitQualifiedNameView(currentName_);
        currentPrefix_ = std::string(prefixView);
        currentLocalName_ = std::string(localNameView);
        currentNamePartsResolved_ = true;
    }
    return currentLocalName_;
}

std::string XmlReader::CurrentPrefix() const {
    if (!currentNamePartsResolved_) {
        const auto [prefixView, localNameView] = SplitQualifiedNameView(currentName_);
        currentPrefix_ = std::string(prefixView);
        currentLocalName_ = std::string(localNameView);
        currentNamePartsResolved_ = true;
    }
    return currentPrefix_;
}

std::string XmlReader::CurrentAttributeLocalName(std::size_t index) const {
    if (index >= currentAttributes_.size()) {
        return {};
    }

    if (currentAttributeLocalNames_.size() < currentAttributes_.size()) {
        currentAttributeLocalNames_.resize(currentAttributes_.size());
    }
    if (currentAttributePrefixes_.size() < currentAttributes_.size()) {
        currentAttributePrefixes_.resize(currentAttributes_.size());
    }
    if (currentAttributeNamePartsResolved_.size() < currentAttributes_.size()) {
        currentAttributeNamePartsResolved_.resize(currentAttributes_.size(), static_cast<unsigned char>(0));
    }
    if (currentAttributeNamePartsResolved_[index] == 0) {
        const auto [prefixView, localNameView] = SplitQualifiedNameView(currentAttributes_[index].first);
        currentAttributePrefixes_[index] = std::string(prefixView);
        currentAttributeLocalNames_[index] = std::string(localNameView);
        currentAttributeNamePartsResolved_[index] = static_cast<unsigned char>(1);
    }
    return currentAttributeLocalNames_[index];
}

std::string XmlReader::CurrentAttributePrefix(std::size_t index) const {
    if (index >= currentAttributes_.size()) {
        return {};
    }

    if (currentAttributeLocalNames_.size() < currentAttributes_.size()) {
        currentAttributeLocalNames_.resize(currentAttributes_.size());
    }
    if (currentAttributePrefixes_.size() < currentAttributes_.size()) {
        currentAttributePrefixes_.resize(currentAttributes_.size());
    }
    if (currentAttributeNamePartsResolved_.size() < currentAttributes_.size()) {
        currentAttributeNamePartsResolved_.resize(currentAttributes_.size(), static_cast<unsigned char>(0));
    }
    if (currentAttributeNamePartsResolved_[index] == 0) {
        const auto [prefixView, localNameView] = SplitQualifiedNameView(currentAttributes_[index].first);
        currentAttributePrefixes_[index] = std::string(prefixView);
        currentAttributeLocalNames_[index] = std::string(localNameView);
        currentAttributeNamePartsResolved_[index] = static_cast<unsigned char>(1);
    }
    return currentAttributePrefixes_[index];
}

const std::string& XmlReader::CurrentAttributeNamespaceUri(std::size_t index) const {
    if (index >= currentAttributes_.size()) {
        return EmptyString();
    }

    if (index >= currentAttributeNamespaceUris_.size()) {
        currentAttributeNamespaceUris_.resize(currentAttributes_.size());
    }
    if (index >= currentAttributeNamespaceUrisResolved_.size()) {
        currentAttributeNamespaceUrisResolved_.resize(currentAttributes_.size(), static_cast<unsigned char>(0));
    }
    if (currentAttributeNamespaceUrisResolved_[index] != 0) {
        return currentAttributeNamespaceUris_[index];
    }

    const std::string& attributeName = currentAttributes_[index].first;
    const auto colon = attributeName.find(':');
    const bool hasPrefix = colon != std::string::npos;
    const std::string_view attributePrefix = hasPrefix
        ? std::string_view(attributeName).substr(0, colon)
        : std::string_view{};

    std::string namespaceUri;
    if (attributeName == "xmlns" || attributePrefix == "xmlns") {
        namespaceUri = "http://www.w3.org/2000/xmlns/";
    } else if (hasPrefix) {
        for (const auto& entry : currentLocalNamespaceDeclarations_) {
            if (std::string_view(entry.first) == attributePrefix) {
                namespaceUri = entry.second;
                break;
            }
        }
        if (namespaceUri.empty()) {
            const std::string prefix(attributePrefix);
            namespaceUri = LookupNamespaceUri(prefix);
        }
    }

    currentAttributeNamespaceUris_[index] = std::move(namespaceUri);
    currentAttributeNamespaceUrisResolved_[index] = static_cast<unsigned char>(1);
    return currentAttributeNamespaceUris_[index];
}

void XmlReader::RefreshCurrentEarliestRetainedAttributeValueStart() const noexcept {
    currentEarliestRetainedAttributeValueStart_ = std::string::npos;
    for (const auto& metadata : currentAttributeValueMetadata_) {
        if ((metadata.flags & kAttributeValueDecoded) != 0) {
            continue;
        }
        if (metadata.valueStart == std::string::npos
            || metadata.valueEnd == std::string::npos
            || metadata.valueEnd < metadata.valueStart) {
            continue;
        }
        if (currentEarliestRetainedAttributeValueStart_ == std::string::npos
            || metadata.valueStart < currentEarliestRetainedAttributeValueStart_) {
            currentEarliestRetainedAttributeValueStart_ = metadata.valueStart;
        }
    }
}

void XmlReader::EnsureCurrentAttributeValueDecoded(std::size_t index) const {
    if (index >= currentAttributes_.size()) {
        return;
    }
    if (index >= currentAttributeValueMetadata_.size()) {
        return;
    }

    auto& metadata = currentAttributeValueMetadata_[index];
    if ((metadata.flags & kAttributeValueDecoded) != 0) {
        return;
    }

    currentAttributes_[index].second.clear();
    if (metadata.valueStart != std::string::npos
        && metadata.valueEnd != std::string::npos
        && metadata.valueEnd >= metadata.valueStart) {
        if ((metadata.flags & kAttributeValueNeedsDecoding) != 0) {
            AppendDecodedSourceRangeTo(currentAttributes_[index].second, metadata.valueStart, metadata.valueEnd);
        } else {
            currentAttributes_[index].second = SourceSubstr(metadata.valueStart, metadata.valueEnd - metadata.valueStart);
        }
    }
    metadata.flags = static_cast<unsigned char>(metadata.flags | kAttributeValueDecoded);

    if (metadata.valueStart != std::string::npos
        && metadata.valueEnd != std::string::npos
        && metadata.valueEnd >= metadata.valueStart
        && metadata.valueStart == currentEarliestRetainedAttributeValueStart_) {
        RefreshCurrentEarliestRetainedAttributeValueStart();
    }
}

const std::string& XmlReader::CurrentAttributeValue(std::size_t index) const {
    EnsureCurrentAttributeValueDecoded(index);
    return currentAttributes_[index].second;
}

void XmlReader::AppendCurrentAttributesForLoad(XmlElement& element) {
    if (currentAttributes_.empty()) {
        return;
    }

    const bool hasMetadataForAllAttributes = currentAttributeValueMetadata_.size() == currentAttributes_.size();
    std::size_t totalStorageBytes = 0;
    if (hasMetadataForAllAttributes) {
        for (std::size_t index = 0; index < currentAttributes_.size(); ++index) {
            const auto& attribute = currentAttributes_[index];
            const auto& metadata = currentAttributeValueMetadata_[index];
            totalStorageBytes += attribute.first.size();
            if ((metadata.flags & kAttributeValueDecoded) == 0
                && metadata.valueStart != std::string::npos
                && metadata.valueEnd != std::string::npos
                && metadata.valueEnd >= metadata.valueStart) {
                totalStorageBytes += metadata.valueEnd - metadata.valueStart;
                continue;
            }
            totalStorageBytes += attribute.second.size();
        }
    } else {
        for (std::size_t index = 0; index < currentAttributes_.size(); ++index) {
            const auto& attribute = currentAttributes_[index];
            totalStorageBytes += attribute.first.size();
            if (index < currentAttributeValueMetadata_.size()) {
                const auto& metadata = currentAttributeValueMetadata_[index];
                if ((metadata.flags & kAttributeValueDecoded) == 0
                    && metadata.valueStart != std::string::npos
                    && metadata.valueEnd != std::string::npos
                    && metadata.valueEnd >= metadata.valueStart) {
                    totalStorageBytes += metadata.valueEnd - metadata.valueStart;
                    continue;
                }
            } else {
                EnsureCurrentAttributeValueDecoded(index);
            }
            totalStorageBytes += attribute.second.size();
        }
    }

    currentEarliestRetainedAttributeValueStart_ = std::string::npos;

    element.ReserveAttributesForLoad(currentAttributes_.size(), totalStorageBytes);
    auto& pendingStorage = element.pendingLoadAttributeStorage_;
    auto& pendingAttributes = element.pendingLoadAttributes_;
    const auto appendPendingAttribute = [&pendingAttributes](
        std::size_t nameOffset,
        std::size_t nameLength,
        std::size_t valueOffset,
        std::size_t valueLength) {
        pendingAttributes.push_back(XmlElement::PendingLoadAttribute{
            nameOffset,
            nameLength,
            valueOffset,
            valueLength});
    };

    if (hasMetadataForAllAttributes) {
        for (std::size_t index = 0; index < currentAttributes_.size(); ++index) {
            const auto& attribute = currentAttributes_[index];
            const auto& metadata = currentAttributeValueMetadata_[index];
            const std::size_t nameOffset = pendingStorage.size();
            pendingStorage += attribute.first;

            const std::size_t valueOffset = pendingStorage.size();
            std::size_t valueLength = attribute.second.size();
            if ((metadata.flags & kAttributeValueDecoded) == 0
                && metadata.valueStart != std::string::npos
                && metadata.valueEnd != std::string::npos
                && metadata.valueEnd >= metadata.valueStart) {
                const std::size_t rawLength = metadata.valueEnd - metadata.valueStart;
                if ((metadata.flags & kAttributeValueNeedsDecoding) == 0) {
                    valueLength = rawLength;
                    AppendSourceSubstrTo(pendingStorage, metadata.valueStart, valueLength);
                } else {
                    valueLength = AppendDecodedSourceRangeTo(pendingStorage, metadata.valueStart, metadata.valueEnd);
                }

                appendPendingAttribute(nameOffset, attribute.first.size(), valueOffset, valueLength);
                continue;
            }

            pendingStorage += attribute.second;
            appendPendingAttribute(nameOffset, attribute.first.size(), valueOffset, valueLength);
        }
        return;
    }

    for (std::size_t index = 0; index < currentAttributes_.size(); ++index) {
        const auto& attribute = currentAttributes_[index];
        const std::size_t nameOffset = pendingStorage.size();
        pendingStorage += attribute.first;

        const std::size_t valueOffset = pendingStorage.size();
        std::size_t valueLength = attribute.second.size();

        if (index < currentAttributeValueMetadata_.size()) {
            const auto& metadata = currentAttributeValueMetadata_[index];
            if ((metadata.flags & kAttributeValueDecoded) == 0
                && metadata.valueStart != std::string::npos
                && metadata.valueEnd != std::string::npos
                && metadata.valueEnd >= metadata.valueStart) {
                const std::size_t rawLength = metadata.valueEnd - metadata.valueStart;
                if ((metadata.flags & kAttributeValueNeedsDecoding) == 0) {
                    valueLength = rawLength;
                    AppendSourceSubstrTo(pendingStorage, metadata.valueStart, valueLength);
                } else {
                    valueLength = AppendDecodedSourceRangeTo(pendingStorage, metadata.valueStart, metadata.valueEnd);
                }

                appendPendingAttribute(nameOffset, attribute.first.size(), valueOffset, valueLength);
                continue;
            }
        }

        pendingStorage += attribute.second;
        appendPendingAttribute(nameOffset, attribute.first.size(), valueOffset, valueLength);
    }
}

void XmlReader::InitializeInputState() {
    elementContentValidationStack_.clear();
    namespaceScopes_.clear();
    namespaceScopeFramePushedStack_.clear();
    xmlSpacePreserveStack_.clear();
    xmlSpacePreserveFramePushedStack_.clear();
    documentDeclarationStandalone_.clear();
    documentTypeName_.clear();
    namespaceScopes_.push_back({});
    namespaceScopes_.back().emplace_back("xml", "http://www.w3.org/XML/1998/namespace");
    namespaceScopes_.back().emplace_back("xmlns", "http://www.w3.org/2000/xmlns/");
    xmlSpacePreserveStack_.push_back(false);
    sourceWasUtf8Bom_ = HasSourceChar(2)
        && static_cast<unsigned char>(SourceCharAt(0)) == 0xEF
        && static_cast<unsigned char>(SourceCharAt(1)) == 0xBB
        && static_cast<unsigned char>(SourceCharAt(2)) == 0xBF;
    if (HasSourceChar(2)
        && static_cast<unsigned char>(SourceCharAt(0)) == 0xEF
        && static_cast<unsigned char>(SourceCharAt(1)) == 0xBB
        && static_cast<unsigned char>(SourceCharAt(2)) == 0xBF) {
        position_ = 3;
    }
    eof_ = !HasSourceChar(position_);
    lineNumber_ = 1;
    linePosition_ = 1;
    discardedSourceOffset_ = 0;
    discardedLineNumber_ = 1;
    discardedLinePosition_ = 1;
}

XmlReader XmlReader::CreateFromValidatedString(
    std::shared_ptr<const std::string> xml,
    const XmlReaderSettings& settings) {
    const std::size_t sourceSize = xml == nullptr ? 0 : xml->size();
    XmlReader reader(settings);
    reader.sourceWasUtf16_ = false;
    reader.inputSource_ = std::make_shared<StringXmlReaderInputSource>(std::move(xml));
    reader.InitializeInputState();
    if (reader.settings_.MaxCharactersInDocument != 0
        && sourceSize > reader.position_
        && sourceSize - reader.position_ > reader.settings_.MaxCharactersInDocument) {
        throw XmlException("The XML document exceeds the configured MaxCharactersInDocument limit");
    }
    return reader;
}

XmlReader XmlReader::Create(const std::string& xml, const XmlReaderSettings& settings) {
    if (LooksLikeMalformedUtf16XmlPrefix(xml)) {
        throw XmlException("Malformed UTF-16 XML input");
    }

    const bool sourceWasUtf16 = HasUtf16Bom(xml);
    auto sourceText = std::make_shared<std::string>(NormalizeBomEncodedXmlText(xml));
    ValidateXmlReaderInputAgainstSchemas(sourceText, settings);
    XmlReader reader = CreateFromValidatedString(sourceText, settings);
    reader.sourceWasUtf16_ = sourceWasUtf16;
    return reader;
}

XmlReader XmlReader::Create(std::istream& stream, const XmlReaderSettings& settings) {
    if (settings.Validation == ValidationType::Schema) {
        const auto startPosition = stream.tellg();
        if (startPosition != std::streampos(-1)) {
            XmlReaderSettings validationSettings = settings;
            validationSettings.Validation = ValidationType::None;

            auto validatingReader = XmlReader::Create(stream, validationSettings);
            ValidateXmlReaderInputAgainstSchemas(validatingReader, settings);

            stream.clear();
            stream.seekg(startPosition);
            if (stream.good()) {
                XmlReader reader(settings);
                reader.inputSource_ = std::make_shared<StreamXmlReaderInputSource>(stream);
                reader.InitializeInputState();
                return reader;
            }

            stream.clear();
        }

        std::string initialBuffer;
        if (settings.MaxCharactersInDocument != 0) {
            initialBuffer = ReadStreamPrefix(stream, settings.MaxCharactersInDocument + 4);
            const std::size_t bomLength = GetUtf8BomLength(initialBuffer);
            if (initialBuffer.size() > bomLength
                && initialBuffer.size() - bomLength > settings.MaxCharactersInDocument) {
                throw XmlException("The XML document exceeds the configured MaxCharactersInDocument limit");
            }
        }

        auto replayStream = SpoolStreamToTemporaryReplayStream(stream, initialBuffer);

        XmlReaderSettings validationSettings = settings;
        validationSettings.Validation = ValidationType::None;
        auto validatingReader = XmlReader::Create(*replayStream, validationSettings);
        ValidateXmlReaderInputAgainstSchemas(validatingReader, settings);

        replayStream->clear();
        replayStream->seekg(0, std::ios::beg);
        if (!*replayStream) {
            throw XmlException("Failed to rewind temporary XML replay file");
        }

        XmlReader reader(settings);
        reader.sourceWasUtf16_ = false;
        reader.inputSource_ = std::make_shared<StreamXmlReaderInputSource>(replayStream);
        reader.InitializeInputState();
        return reader;
    }

    XmlReader reader(settings);
    reader.sourceWasUtf16_ = false;
    std::string initialBuffer;
    if (settings.MaxCharactersInDocument != 0) {
        initialBuffer = ReadStreamPrefix(stream, settings.MaxCharactersInDocument + 4);
        const std::size_t bomLength = GetUtf8BomLength(initialBuffer);
        if (initialBuffer.size() > bomLength
            && initialBuffer.size() - bomLength > settings.MaxCharactersInDocument) {
            throw XmlException("The XML document exceeds the configured MaxCharactersInDocument limit");
        }
    }
    reader.inputSource_ = std::make_shared<StreamXmlReaderInputSource>(stream, std::move(initialBuffer));
    reader.InitializeInputState();
    return reader;
}

XmlReader XmlReader::CreateFromFile(const std::string& path, const XmlReaderSettings& settings) {
    const auto absolutePath = std::filesystem::absolute(std::filesystem::path(path)).string();

    auto stream = std::make_shared<std::ifstream>(std::filesystem::path(path), std::ios::binary);
    if (!*stream) {
        throw XmlException("Failed to open XML file: " + path);
    }

    char bomProbeBuffer[8] = {};
    stream->read(bomProbeBuffer, static_cast<std::streamsize>(sizeof(bomProbeBuffer)));
    const auto bytesRead = stream->gcount();
    const std::string bomProbe(bomProbeBuffer, static_cast<std::size_t>(bytesRead > 0 ? bytesRead : 0));
    stream->clear();
    stream->seekg(0, std::ios::beg);
    if (!*stream) {
        throw XmlException("Failed to rewind XML file after BOM detection: " + path);
    }

    if (LooksLikeMalformedUtf16XmlPrefix(bomProbe)) {
        throw XmlException("Malformed UTF-16 XML input");
    }

    if (HasUtf16Bom(bomProbe)) {
        const std::string rawXml((std::istreambuf_iterator<char>(*stream)), std::istreambuf_iterator<char>());
        XmlReader reader = XmlReader::Create(NormalizeBomEncodedXmlText(rawXml), settings);
        reader.sourceWasUtf16_ = true;
        reader.baseUri_ = absolutePath;
        return reader;
    }

    bool maxCharactersChecked = false;
    if (settings.MaxCharactersInDocument != 0) {
        std::error_code fileSizeError;
        const auto fileSize = std::filesystem::file_size(std::filesystem::path(path), fileSizeError);
        if (!fileSizeError) {
            const std::size_t bomLength = GetUtf8BomLength(bomProbe);
            if (fileSize > bomLength && fileSize - bomLength > settings.MaxCharactersInDocument) {
                throw XmlException("The XML document exceeds the configured MaxCharactersInDocument limit");
            }
            maxCharactersChecked = true;
        }
    }

    auto createReaderFromInputSource = [&absolutePath](
                                          std::shared_ptr<const XmlReaderInputSource> inputSource,
                                          const XmlReaderSettings& readerSettings) {
        XmlReader reader(readerSettings);
        reader.sourceWasUtf16_ = false;
        reader.inputSource_ = std::move(inputSource);
        reader.InitializeInputState();
        reader.baseUri_ = absolutePath;
        return reader;
    };

    std::shared_ptr<const XmlReaderInputSource> mappedInputSource;
#if defined(_WIN32)
    try {
        mappedInputSource = std::make_shared<MemoryMappedXmlReaderInputSource>(std::filesystem::path(path));
    } catch (const XmlException&) {
        mappedInputSource.reset();
    }
#endif

    if (settings.Validation == ValidationType::Schema) {
        if (mappedInputSource != nullptr) {
            XmlReaderSettings validationSettings = settings;
            validationSettings.Validation = ValidationType::None;
            auto validatingReader = createReaderFromInputSource(mappedInputSource, validationSettings);
            ValidateXmlReaderInputAgainstSchemas(validatingReader, settings);
            return createReaderFromInputSource(std::move(mappedInputSource), settings);
        }

        XmlReaderSettings validationSettings = settings;
        validationSettings.Validation = ValidationType::None;
        auto validatingReader = XmlReader::Create(*stream, validationSettings);
        ValidateXmlReaderInputAgainstSchemas(validatingReader, settings);

        stream->clear();
        stream->seekg(0, std::ios::beg);
        if (!*stream) {
            throw XmlException("Failed to rewind XML file after schema validation: " + path);
        }

        XmlReader reader(settings);
        reader.sourceWasUtf16_ = false;
        reader.inputSource_ = std::make_shared<StreamXmlReaderInputSource>(
            std::static_pointer_cast<std::istream>(stream));
        reader.InitializeInputState();
        reader.baseUri_ = absolutePath;
        return reader;
    }

    if (mappedInputSource != nullptr) {
        return createReaderFromInputSource(std::move(mappedInputSource), settings);
    }

    std::string initialBuffer;
    if (!maxCharactersChecked && settings.MaxCharactersInDocument != 0) {
        initialBuffer = ReadStreamPrefix(*stream, settings.MaxCharactersInDocument + 4);
        const std::size_t bomLength = GetUtf8BomLength(initialBuffer);
        if (initialBuffer.size() > bomLength
            && initialBuffer.size() - bomLength > settings.MaxCharactersInDocument) {
            throw XmlException("The XML document exceeds the configured MaxCharactersInDocument limit");
        }
    }

    XmlReader reader(settings);
    reader.sourceWasUtf16_ = false;
    reader.inputSource_ = std::make_shared<StreamXmlReaderInputSource>(
        std::static_pointer_cast<std::istream>(stream),
        std::move(initialBuffer));
    reader.InitializeInputState();
    reader.baseUri_ = absolutePath;
    return reader;
}

bool XmlReader::Read() {
    XmlReaderTokenizer tokenizer(inputSource_.get());
    if (closed_) {
        return false;
    }
    attributeIndex_ = -1;
    started_ = true;

    while (true) {
        ResetCurrentNode();
        MaybeDiscardSourcePrefix();
        if (TryConsumeBufferedNode()) {
            if (settings_.IgnoreWhitespace
                && (currentNodeType_ == XmlNodeType::Whitespace || currentNodeType_ == XmlNodeType::SignificantWhitespace)) {
                continue;
            }
            FinalizeSuccessfulRead();
            return true;
        }

        if (!HasSourceChar(position_)) {
            if (settings_.Conformance == ConformanceLevel::Document && !sawRootElement_) {
                Throw("Unexpected end of XML document");
            }
            if (!elementStack_.empty()) {
                Throw("Unexpected end of XML document");
            }
            ValidatePendingDtdIdReferences();
            MaybeDiscardSourcePrefix();
            eof_ = true;
            return false;
        }

        if (elementStack_.empty()) {
            const auto markupKind = tokenizer.ClassifyMarkup(position_);
            if (settings_.Conformance == ConformanceLevel::Document && !sawRootElement_) {
                if (Peek() != '<') {
                    if (!IsWhitespace(Peek())) {
                        Throw("Unexpected content before the root element");
                    }

                    std::size_t scan = position_;
                    while (HasSourceChar(scan) && SourceCharAt(scan) != '<') {
                        if (!IsWhitespace(SourceCharAt(scan))) {
                            Throw("Unexpected content before the root element");
                        }
                        ++scan;
                    }
                } else if (markupKind == XmlMarkupKind::CData) {
                    Throw("Unexpected content before the root element");
                }
            }

            if (settings_.Conformance == ConformanceLevel::Document && completedRootElement_) {
                if (IsWhitespace(Peek())) {
                    std::size_t scan = position_;
                    while (HasSourceChar(scan) && SourceCharAt(scan) != '<') {
                        if (!IsWhitespace(SourceCharAt(scan))) {
                            Throw("Unexpected content after the root element");
                        }
                        ++scan;
                    }

                    ParseText();
                    if (settings_.IgnoreWhitespace
                        && (currentNodeType_ == XmlNodeType::Whitespace || currentNodeType_ == XmlNodeType::SignificantWhitespace)) {
                        continue;
                    }
                    FinalizeSuccessfulRead();
                    return true;
                }

                if (markupKind == XmlMarkupKind::Comment) {
                    ParseComment();
                    if (settings_.IgnoreComments) {
                        continue;
                    }
                    FinalizeSuccessfulRead();
                    return true;
                }

                if (markupKind == XmlMarkupKind::ProcessingInstruction) {
                    ParseProcessingInstruction();
                    if (settings_.IgnoreProcessingInstructions) {
                        continue;
                    }
                    FinalizeSuccessfulRead();
                    return true;
                }

                if (markupKind == XmlMarkupKind::DocumentType) {
                    Throw("DOCTYPE must appear before the root element");
                }

                Throw("Unexpected content after the root element");
            }

            if (settings_.Conformance == ConformanceLevel::Document
                && markupKind == XmlMarkupKind::DocumentType
                && sawDocumentType_) {
                Throw("XML document can only contain a single DOCTYPE declaration");
            }
        }

        if (Peek() != '<') {
            ParseText();
            if (elementStack_.empty()) {
                xmlDeclarationAllowed_ = false;
            }
            if (settings_.IgnoreWhitespace
                && (currentNodeType_ == XmlNodeType::Whitespace || currentNodeType_ == XmlNodeType::SignificantWhitespace)) {
                continue;
            }
            FinalizeSuccessfulRead();
            return true;
        }

        switch (tokenizer.ClassifyMarkup(position_)) {
        case XmlMarkupKind::XmlDeclaration:
            if (settings_.Conformance == ConformanceLevel::Fragment) {
                Throw("XML declaration is only allowed in document conformance mode");
            }
            ParseDeclaration();
            break;
        case XmlMarkupKind::Comment:
            ParseComment();
            if (elementStack_.empty()) {
                xmlDeclarationAllowed_ = false;
            }
            if (settings_.IgnoreComments) {
                continue;
            }
            break;
        case XmlMarkupKind::CData:
            ParseCData();
            break;
        case XmlMarkupKind::ProcessingInstruction:
            ParseProcessingInstruction();
            if (elementStack_.empty()) {
                xmlDeclarationAllowed_ = false;
            }
            if (settings_.IgnoreProcessingInstructions) {
                continue;
            }
            break;
        case XmlMarkupKind::EndTag:
            ParseEndElement();
            break;
        case XmlMarkupKind::DocumentType:
            if (settings_.Conformance == ConformanceLevel::Fragment) {
                Throw("DOCTYPE is only allowed in document conformance mode");
            }
            ParseDocumentType();
            if (elementStack_.empty()) {
                xmlDeclarationAllowed_ = false;
            }
            if (settings_.DtdProcessing == DtdProcessing::Ignore) {
                continue;
            }
            break;
        case XmlMarkupKind::UnsupportedDeclaration:
            Throw("Unsupported markup declaration");
            break;
        case XmlMarkupKind::Element:
            ParseElement();
            xmlDeclarationAllowed_ = false;
            if (settings_.Conformance == ConformanceLevel::Document && elementStack_.empty() && sawRootElement_ && completedRootElement_) {
                // Existing document-mode validation remains enforced by top-level checks.
            }
            break;
        case XmlMarkupKind::None:
            Throw("Unexpected content after the root element");
            break;
        }

        FinalizeSuccessfulRead();
        return true;
    }
}

bool XmlReader::IsEOF() const noexcept {
    return eof_;
}

void XmlReader::Close() {
    closed_ = true;
    eof_ = true;
    started_ = true;
    attributeIndex_ = -1;
    ResetCurrentNode();
    bufferedNodes_.clear();
    elementStack_.clear();
    elementContentValidationStack_.clear();
    namespaceScopes_.clear();
    namespaceScopeFramePushedStack_.clear();
    xmlSpacePreserveStack_.clear();
    xmlSpacePreserveFramePushedStack_.clear();
}

XmlNodeType XmlReader::NodeType() const {
    if (attributeIndex_ >= 0) {
        return XmlNodeType::Attribute;
    }
    if (!started_ || eof_) {
        return XmlNodeType::None;
    }
    return currentNodeType_;
}

const std::string& XmlReader::Name() const {
    if (attributeIndex_ >= 0) {
        return CurrentAttributes()[static_cast<std::size_t>(attributeIndex_)].first;
    }
    if (!started_ || eof_) {
        return EmptyString();
    }
    return currentName_;
}

std::string XmlReader::LocalName() const {
    if (attributeIndex_ >= 0) {
        return CurrentAttributeLocalName(static_cast<std::size_t>(attributeIndex_));
    }
    if (!started_ || eof_) {
        return {};
    }
    return CurrentLocalName();
}

std::string XmlReader::Prefix() const {
    if (attributeIndex_ >= 0) {
        return CurrentAttributePrefix(static_cast<std::size_t>(attributeIndex_));
    }
    if (!started_ || eof_) {
        return {};
    }
    return CurrentPrefix();
}

const std::string& XmlReader::NamespaceURI() const {
    if (attributeIndex_ >= 0) {
        return CurrentAttributeNamespaceUri(static_cast<std::size_t>(attributeIndex_));
    }
    return currentNamespaceUri_;
}

void XmlReader::MaterializeValue() {
    if (currentValue_.empty()
        && currentValueStart_ != std::string::npos
        && currentValueEnd_ != std::string::npos) {
        currentValue_ = SourceSubstr(currentValueStart_, currentValueEnd_ - currentValueStart_);
    }
}

const std::string& XmlReader::Value() const {
    if (attributeIndex_ >= 0) {
        return CurrentAttributeValue(static_cast<std::size_t>(attributeIndex_));
    }
    if (!started_ || eof_) {
        return EmptyString();
    }
    if (currentNodeType_ == XmlNodeType::XmlDeclaration && currentValue_.empty()) {
        currentValue_ = BuildDeclarationValue(
            currentDeclarationVersion_,
            currentDeclarationEncoding_,
            currentDeclarationStandalone_);
    }
    if (currentValue_.empty()
        && currentValueStart_ != std::string::npos
        && currentValueEnd_ != std::string::npos) {
        currentValue_ = SourceSubstr(currentValueStart_, currentValueEnd_ - currentValueStart_);
    }
    return currentValue_;
}

int XmlReader::Depth() const noexcept {
    if (!started_ || eof_) {
        return 0;
    }
    return currentDepth_ + (attributeIndex_ >= 0 ? 1 : 0);
}

bool XmlReader::IsEmptyElement() const noexcept {
    return NodeType() == XmlNodeType::Element && started_ && !eof_ && currentIsEmptyElement_;
}

bool XmlReader::HasValue() const noexcept {
    switch (NodeType()) {
    case XmlNodeType::Attribute:
    case XmlNodeType::Text:
    case XmlNodeType::CDATA:
    case XmlNodeType::EntityReference:
    case XmlNodeType::ProcessingInstruction:
    case XmlNodeType::Comment:
    case XmlNodeType::Whitespace:
    case XmlNodeType::SignificantWhitespace:
    case XmlNodeType::XmlDeclaration:
        return true;
    default:
        return false;
    }
}

int XmlReader::AttributeCount() const noexcept {
    return static_cast<int>(CurrentAttributes().size());
}

bool XmlReader::HasAttributes() const noexcept {
    return !CurrentAttributes().empty();
}

ReadState XmlReader::GetReadState() const noexcept {
    if (closed_) return ReadState::Closed;
    if (eof_) return ReadState::EndOfFile;
    if (!started_) return ReadState::Initial;
    return ReadState::Interactive;
}

bool XmlReader::HasLineInfo() const noexcept {
    return true;
}

std::size_t XmlReader::LineNumber() const noexcept {
    return lineNumber_;
}

std::size_t XmlReader::LinePosition() const noexcept {
    return linePosition_;
}

std::string XmlReader::GetAttribute(std::string_view name) const {
    const auto& attributes = CurrentAttributes();
    for (std::size_t i = 0; i < attributes.size(); ++i) {
        if (attributes[i].first == name) {
            return CurrentAttributeValue(i);
        }
        if (CurrentAttributeLocalName(i) == name) {
            return CurrentAttributeValue(i);
        }
    }
    return {};
}

std::string XmlReader::GetAttribute(int index) const {
    const auto& attributes = CurrentAttributes();
    if (index < 0 || static_cast<std::size_t>(index) >= attributes.size()) {
        throw std::out_of_range("Attribute index is out of range");
    }
    return CurrentAttributeValue(static_cast<std::size_t>(index));
}

std::string XmlReader::GetAttribute(std::string_view localName, std::string_view namespaceUri) const {
    const auto& attributes = currentAttributes_;
    for (std::size_t i = 0; i < attributes.size(); ++i) {
        if (CurrentAttributeLocalName(i) != localName) {
            continue;
        }
        if (CurrentAttributeNamespaceUri(i) == namespaceUri) {
            return CurrentAttributeValue(i);
        }
    }
    return {};
}

bool XmlReader::MoveToAttribute(std::string_view name) {
    if (name.empty()) {
        throw std::invalid_argument("Attribute name cannot be empty");
    }

    const auto& attributes = CurrentAttributes();
    for (std::size_t i = 0; i < attributes.size(); ++i) {
        if (attributes[i].first == name || CurrentAttributeLocalName(i) == name) {
            attributeIndex_ = static_cast<int>(i);
            return true;
        }
    }
    return false;
}

bool XmlReader::MoveToAttribute(int index) {
    const auto& attributes = CurrentAttributes();
    if (index < 0 || static_cast<std::size_t>(index) >= attributes.size()) {
        throw std::out_of_range("Attribute index is out of range");
    }
    attributeIndex_ = index;
    return true;
}

bool XmlReader::MoveToAttribute(std::string_view localName, std::string_view namespaceUri) {
    for (std::size_t i = 0; i < currentAttributes_.size(); ++i) {
        if (CurrentAttributeLocalName(i) != localName) {
            continue;
        }
        if (CurrentAttributeNamespaceUri(i) == namespaceUri) {
            attributeIndex_ = static_cast<int>(i);
            return true;
        }
    }
    return false;
}

bool XmlReader::MoveToFirstAttribute() {
    if (NodeType() != XmlNodeType::Element || CurrentAttributes().empty()) {
        return false;
    }

    attributeIndex_ = 0;
    return true;
}

bool XmlReader::MoveToNextAttribute() {
    if (attributeIndex_ < 0) {
        return false;
    }
    if (static_cast<std::size_t>(attributeIndex_ + 1) >= CurrentAttributes().size()) {
        return false;
    }

    ++attributeIndex_;
    return true;
}

bool XmlReader::MoveToElement() {
    const bool moved = attributeIndex_ >= 0;
    attributeIndex_ = -1;
    return moved;
}

std::string XmlReader::LookupNamespace(std::string_view prefix) const {
    for (auto it = currentLocalNamespaceDeclarations_.rbegin(); it != currentLocalNamespaceDeclarations_.rend(); ++it) {
        if (it->first == prefix) {
            return it->second;
        }
    }

    return LookupNamespaceUri(prefix);
}

std::string XmlReader::ReadInnerXml() const {
    if (attributeIndex_ >= 0) {
        return Value();
    }
    if (NodeType() != XmlNodeType::Element) {
        return {};
    }
    if (!currentIsEmptyElement_ && currentInnerXml_.empty() && currentElementStart_ != std::string::npos) {
        const auto captured = CaptureElementXml(currentElementStart_, currentContentStart_);
        currentInnerXml_ = captured.first;
        currentOuterXml_ = captured.second;
    }
    return currentInnerXml_;
}

std::string XmlReader::ReadOuterXml() const {
    if (attributeIndex_ >= 0) {
        return Name() + "=\"" + EscapeAttribute(Value(), XmlWriterSettings{}) + "\"";
    }
    if (!started_ || eof_) {
        return {};
    }
    if (currentNodeType_ == XmlNodeType::EndElement || currentNodeType_ == XmlNodeType::EndEntity) {
        return {};
    }
    if (currentOuterXml_.empty()) {
        if (currentNodeType_ == XmlNodeType::Element && currentElementStart_ != std::string::npos) {
            if (currentIsEmptyElement_) {
                currentOuterXml_ = SourceSubstr(currentElementStart_, currentContentStart_ - currentElementStart_);
            } else {
                const auto captured = CaptureElementXml(currentElementStart_, currentContentStart_);
                currentInnerXml_ = captured.first;
                currentOuterXml_ = captured.second;
            }
        } else if (currentNodeStart_ != std::string::npos && currentNodeEnd_ != std::string::npos) {
            currentOuterXml_ = SourceSubstr(currentNodeStart_, currentNodeEnd_ - currentNodeStart_);
        }
    }
    return currentOuterXml_;
}

std::string XmlReader::ReadContentAsString() {
    std::string result;
    while (true) {
        auto nt = NodeType();
        if (nt == XmlNodeType::Text || nt == XmlNodeType::CDATA || nt == XmlNodeType::Whitespace ||
            nt == XmlNodeType::SignificantWhitespace) {
            AppendCurrentValueTo(result);
            Read();
        } else {
            break;
        }
    }
    return result;
}

int XmlReader::ReadContentAsInt() {
    return XmlConvert::ToInt32(ReadContentAsString());
}

long long XmlReader::ReadContentAsLong() {
    return XmlConvert::ToInt64(ReadContentAsString());
}

double XmlReader::ReadContentAsDouble() {
    return XmlConvert::ToDouble(ReadContentAsString());
}

bool XmlReader::ReadContentAsBoolean() {
    return XmlConvert::ToBoolean(ReadContentAsString());
}

std::string XmlReader::ReadString() {
    // Legacy API: accumulate Text/CDATA content until a non-text node
    if (NodeType() == XmlNodeType::Element) {
        if (!Read()) return {};
    }
    std::string result;
    while (true) {
        auto nt = NodeType();
        if (nt == XmlNodeType::Text || nt == XmlNodeType::CDATA ||
            nt == XmlNodeType::Whitespace || nt == XmlNodeType::SignificantWhitespace) {
            AppendCurrentValueTo(result);
            Read();
        } else {
            break;
        }
    }
    return result;
}

int XmlReader::ReadBase64(std::vector<unsigned char>& buffer) {
    const std::size_t originalSize = buffer.size();
    unsigned int accumulator = 0;
    int bits = 0;

    while (true) {
        const auto nodeType = NodeType();
        if (nodeType == XmlNodeType::Text || nodeType == XmlNodeType::CDATA
            || nodeType == XmlNodeType::Whitespace || nodeType == XmlNodeType::SignificantWhitespace) {
            DecodeAndAppendCurrentBase64(buffer, accumulator, bits);
            Read();
        } else {
            break;
        }
    }

    return static_cast<int>(buffer.size() - originalSize);
}

XmlNodeType XmlReader::MoveToContent() {
    MoveToElement();
    while (true) {
        auto nt = NodeType();
        if (nt == XmlNodeType::Element || nt == XmlNodeType::Text || nt == XmlNodeType::CDATA ||
            nt == XmlNodeType::EntityReference || nt == XmlNodeType::EndElement || nt == XmlNodeType::EndEntity) {
            return nt;
        }
        if (!Read()) {
            return NodeType();
        }
    }
}

bool XmlReader::IsStartElement() {
    return MoveToContent() == XmlNodeType::Element;
}

bool XmlReader::IsStartElement(std::string_view name) {
    return MoveToContent() == XmlNodeType::Element && Name() == name;
}

bool XmlReader::IsStartElement(std::string_view localName, std::string_view namespaceUri) {
    return MoveToContent() == XmlNodeType::Element && LocalName() == localName && NamespaceURI() == namespaceUri;
}

void XmlReader::ReadStartElement() {
    if (MoveToContent() != XmlNodeType::Element) {
        throw XmlException("ReadStartElement called when the reader is not positioned on an element");
    }
    Read();
}

void XmlReader::ReadStartElement(std::string_view name) {
    if (MoveToContent() != XmlNodeType::Element) {
        throw XmlException("ReadStartElement called when the reader is not positioned on an element");
    }
    if (Name() != name) {
        throw XmlException("Element '" + std::string(name) + "' was not found. Current element is '" + Name() + "'");
    }
    Read();
}

void XmlReader::ReadStartElement(std::string_view localName, std::string_view namespaceUri) {
    if (MoveToContent() != XmlNodeType::Element) {
        throw XmlException("ReadStartElement called when the reader is not positioned on an element");
    }
    if (LocalName() != localName || NamespaceURI() != namespaceUri) {
        throw XmlException(
            "Element '{" + std::string(namespaceUri) + "}" + std::string(localName) +
            "' was not found. Current element is '{" + NamespaceURI() + "}" + LocalName() + "'");
    }
    Read();
}

void XmlReader::ReadEndElement() {
    if (MoveToContent() != XmlNodeType::EndElement) {
        throw XmlException("ReadEndElement called when the reader is not positioned on an end element");
    }
    Read();
}

std::string XmlReader::ReadElementContentAsString() {
    if (MoveToContent() != XmlNodeType::Element) {
        throw XmlException("ReadElementContentAsString called when the reader is not positioned on an element");
    }
    if (IsEmptyElement()) {
        Read();
        return {};
    }

    std::string result;
    std::size_t closeStart = std::string::npos;
    std::size_t closeEnd = std::string::npos;
    if (TryReadSimpleElementContentAsString(result, closeStart, closeEnd)) {
        const auto [lineNumber, linePosition] = ComputeLineColumn(closeStart);
        position_ = closeStart;
        lineNumber_ = lineNumber;
        linePosition_ = linePosition;
        ParseEndElement();
        Read();
        return result;
    }

    Read();  // move past start element
    while (true) {
        auto nt = NodeType();
        if (nt == XmlNodeType::EndElement) {
            Read();  // consume end element
            return result;
        }
        if (nt == XmlNodeType::Text || nt == XmlNodeType::CDATA || nt == XmlNodeType::Whitespace ||
            nt == XmlNodeType::SignificantWhitespace) {
            AppendCurrentValueTo(result);
            Read();
        } else if (nt == XmlNodeType::Comment || nt == XmlNodeType::ProcessingInstruction) {
            Read();
        } else {
            throw XmlException("ReadElementContentAsString encountered unexpected node type");
        }
    }
}

std::string XmlReader::ReadElementContentAsString(std::string_view localName, std::string_view namespaceUri) {
    if (MoveToContent() != XmlNodeType::Element) {
        throw XmlException("ReadElementContentAsString called when the reader is not positioned on an element");
    }
    if (LocalName() != localName || NamespaceURI() != namespaceUri) {
        throw XmlException(
            "Element '{" + std::string(namespaceUri) + "}" + std::string(localName) +
            "' was not found. Current element is '{" + NamespaceURI() + "}" + LocalName() + "'");
    }
    return ReadElementContentAsString();
}

int XmlReader::ReadElementContentAsInt() {
    return XmlConvert::ToInt32(ReadElementContentAsString());
}

int XmlReader::ReadElementContentAsInt(std::string_view localName, std::string_view namespaceUri) {
    return XmlConvert::ToInt32(ReadElementContentAsString(localName, namespaceUri));
}

long long XmlReader::ReadElementContentAsLong() {
    return XmlConvert::ToInt64(ReadElementContentAsString());
}

long long XmlReader::ReadElementContentAsLong(std::string_view localName, std::string_view namespaceUri) {
    return XmlConvert::ToInt64(ReadElementContentAsString(localName, namespaceUri));
}

double XmlReader::ReadElementContentAsDouble() {
    return XmlConvert::ToDouble(ReadElementContentAsString());
}

double XmlReader::ReadElementContentAsDouble(std::string_view localName, std::string_view namespaceUri) {
    return XmlConvert::ToDouble(ReadElementContentAsString(localName, namespaceUri));
}

bool XmlReader::ReadElementContentAsBoolean() {
    return XmlConvert::ToBoolean(ReadElementContentAsString());
}

bool XmlReader::ReadElementContentAsBoolean(std::string_view localName, std::string_view namespaceUri) {
    return XmlConvert::ToBoolean(ReadElementContentAsString(localName, namespaceUri));
}

std::string XmlReader::ReadElementString() {
    if (MoveToContent() != XmlNodeType::Element) {
        throw XmlException("ReadElementString called when the reader is not positioned on an element");
    }
    if (IsEmptyElement()) {
        Read();
        return {};
    }

    std::string result;
    std::size_t closeStart = std::string::npos;
    std::size_t closeEnd = std::string::npos;
    if (TryReadSimpleElementContentAsString(result, closeStart, closeEnd)) {
        const auto [lineNumber, linePosition] = ComputeLineColumn(closeStart);
        position_ = closeStart;
        lineNumber_ = lineNumber;
        linePosition_ = linePosition;
        ParseEndElement();
        Read();
        return result;
    }

    Read();
    while (true) {
        auto nt = NodeType();
        if (nt == XmlNodeType::EndElement) {
            Read();
            return result;
        }
        if (nt == XmlNodeType::Text || nt == XmlNodeType::CDATA || nt == XmlNodeType::Whitespace ||
            nt == XmlNodeType::SignificantWhitespace) {
            AppendCurrentValueTo(result);
            Read();
        } else if (nt == XmlNodeType::Comment || nt == XmlNodeType::ProcessingInstruction) {
            Read();
        } else {
            throw XmlException("ReadElementString does not support mixed content elements");
        }
    }
}

std::string XmlReader::ReadElementString(std::string_view name) {
    if (MoveToContent() != XmlNodeType::Element) {
        throw XmlException("ReadElementString called when the reader is not positioned on an element");
    }
    if (Name() != name) {
        throw XmlException("Element '" + std::string(name) + "' was not found. Current element is '" + Name() + "'");
    }
    return ReadElementString();
}

std::string XmlReader::ReadElementString(std::string_view localName, std::string_view namespaceUri) {
    if (MoveToContent() != XmlNodeType::Element) {
        throw XmlException("ReadElementString called when the reader is not positioned on an element");
    }
    if (LocalName() != localName || NamespaceURI() != namespaceUri) {
        throw XmlException(
            "Element '{" + std::string(namespaceUri) + "}" + std::string(localName) +
            "' was not found. Current element is '{" + NamespaceURI() + "}" + LocalName() + "'");
    }
    return ReadElementString();
}

void XmlReader::Skip() {
    MoveToElement();
    if (NodeType() == XmlNodeType::Element && !IsEmptyElement()) {
        int depth = Depth();
        while (Read()) {
            if (NodeType() == XmlNodeType::EndElement && Depth() == depth) {
                Read();
                return;
            }
        }
    } else {
        Read();
    }
}

bool XmlReader::ReadToFollowing(std::string_view name) {
    while (Read()) {
        if (NodeType() == XmlNodeType::Element && Name() == name) {
            return true;
        }
    }
    return false;
}

bool XmlReader::ReadToDescendant(std::string_view name) {
    if (NodeType() != XmlNodeType::Element || IsEmptyElement()) {
        return false;
    }
    int startDepth = Depth();
    while (Read()) {
        if (NodeType() == XmlNodeType::Element && Name() == name) {
            return true;
        }
        if (NodeType() == XmlNodeType::EndElement && Depth() == startDepth) {
            return false;
        }
    }
    return false;
}

bool XmlReader::ReadToNextSibling(std::string_view name) {
    int targetDepth = Depth();
    // Skip current node
    if (NodeType() == XmlNodeType::Element && !IsEmptyElement()) {
        Skip();
    } else {
        Read();
    }
    while (!IsEOF()) {
        if (NodeType() == XmlNodeType::Element && Depth() == targetDepth && Name() == name) {
            return true;
        }
        if (NodeType() == XmlNodeType::EndElement && Depth() < targetDepth) {
            return false;
        }
        if (NodeType() == XmlNodeType::Element && !IsEmptyElement()) {
            Skip();
        } else {
            Read();
        }
    }
    return false;
}

XmlReader XmlReader::ReadSubtree() {
    if (!started_ || eof_ || NodeType() != XmlNodeType::Element) {
        return XmlReader::Create(std::string{}, XmlReaderSettings{});
    }

    XmlReaderSettings subtreeSettings = settings_;
    subtreeSettings.Conformance = ConformanceLevel::Fragment;
    if (inputSource_ == nullptr || currentElementStart_ == std::string::npos) {
        return XmlReader::Create(ReadOuterXml(), subtreeSettings);
    }

    std::size_t subtreeLength = 0;
    std::size_t closeStart = std::string::npos;
    if (currentIsEmptyElement_) {
        subtreeLength = currentContentStart_ - currentElementStart_;
    } else {
        const auto bounds = EnsureCurrentElementXmlBounds();
        closeStart = bounds.first;
        const auto closeEnd = bounds.second;
        subtreeLength = closeEnd - currentElementStart_;
    }

    static constexpr std::size_t InMemorySubtreeMaterializeThreshold = 20 * 1024 * 1024;
    const bool streamBackedSource = dynamic_cast<const StreamXmlReaderInputSource*>(inputSource_.get()) != nullptr
        || dynamic_cast<const SubrangeXmlReaderInputSource*>(inputSource_.get()) != nullptr;
    const bool canUseMaterializedFastPath = subtreeSettings.Validation == ValidationType::None
        && subtreeSettings.MaxCharactersInDocument == 0
        && subtreeSettings.MaxCharactersFromEntities == 0;
    if (streamBackedSource
        && canUseMaterializedFastPath
        && subtreeLength <= InMemorySubtreeMaterializeThreshold) {
        return XmlReader::Create(SourceSubstr(currentElementStart_, subtreeLength), subtreeSettings);
    }

    inputSource_->EnableReplay();

    XmlReader reader(subtreeSettings);
    reader.inputSource_ = std::make_shared<SubrangeXmlReaderInputSource>(inputSource_, currentElementStart_, subtreeLength);
    reader.InitializeInputState();
    return reader;
}


}  // namespace System::Xml
