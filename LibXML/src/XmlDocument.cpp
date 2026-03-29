#include "../include/XmlInternal.h"

namespace System::Xml {

namespace {

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

std::string ReadFileFullyForXmlDocumentLoad(const std::string& path) {
    std::ifstream stream(std::filesystem::path(path), std::ios::binary | std::ios::ate);
    if (!stream) {
        throw XmlException("Failed to open XML file: " + path);
    }

    const std::streamoff fileSize = stream.tellg();
    if (fileSize < 0) {
        throw XmlException("Failed to determine XML file size: " + path);
    }

    std::string xml(static_cast<std::size_t>(fileSize), '\0');
    stream.seekg(0, std::ios::beg);
    if (!xml.empty()) {
        stream.read(xml.data(), static_cast<std::streamsize>(xml.size()));
        if (!stream) {
            throw XmlException("Failed to read XML file: " + path);
        }
    }

    return xml;
}

std::string ReadStreamFullyForXmlDocumentLoad(std::istream& stream) {
    const std::streampos startPosition = stream.tellg();
    if (startPosition != std::streampos(-1)) {
        stream.seekg(0, std::ios::end);
        const std::streampos endPosition = stream.tellg();
        if (endPosition != std::streampos(-1) && endPosition >= startPosition) {
            std::string xml(static_cast<std::size_t>(endPosition - startPosition), '\0');
            stream.seekg(startPosition);
            if (!xml.empty()) {
                stream.read(xml.data(), static_cast<std::streamsize>(xml.size()));
                if (!stream) {
                    throw XmlException("Failed to read XML stream");
                }
            }
            return xml;
        }

        stream.clear();
        stream.seekg(startPosition);
    }

    std::string xml;
    char chunk[64 * 1024];
    while (stream) {
        stream.read(chunk, static_cast<std::streamsize>(sizeof(chunk)));
        const std::streamsize bytesRead = stream.gcount();
        if (bytesRead > 0) {
            xml.append(chunk, static_cast<std::size_t>(bytesRead));
        }
    }

    if (!stream.eof()) {
        throw XmlException("Failed to read XML stream");
    }
    return xml;
}

bool CanUseReaderBackedLoadXmlFastPath(std::string_view xml) noexcept {
    constexpr std::size_t kReaderBackedLoadXmlThreshold = 8u * 1024u * 1024u;
    return xml.size() >= kReaderBackedLoadXmlThreshold
        && xml.find("<!DOCTYPE") == std::string_view::npos;
}

}  // namespace

class XmlParser final {
public:
    enum class MarkupKind {
        None,
        XmlDeclaration,
        Comment,
        CData,
        ProcessingInstruction,
        EndTag,
        DocumentType,
        UnsupportedDeclaration,
        Element,
    };

    explicit XmlParser(std::string_view input, bool sourceWasUtf16 = false)
        : input_(input), position_(0), sourceWasUtf16_(sourceWasUtf16) {
        sourceWasUtf8Bom_ = input_.size() >= 3
            && input_[0] == '\xEF'
            && input_[1] == '\xBB'
            && input_[2] == '\xBF';
        if (input_.size() >= 3 && input_[0] == '\xEF' && input_[1] == '\xBB' && input_[2] == '\xBF') {
            position_ = 3;
        }
    }

    struct PendingDtdIdReference {
        std::string value;
        std::string attributeName;
        std::string elementName;
    };

    void ParseInto(XmlDocument& document) {
        document_ = &document;
        document.RemoveAll();

        auto appendParsedNode = [this, &document](const std::shared_ptr<XmlNode>& node, std::size_t errorPosition) {
            try {
                document.AppendChild(node);
            } catch (const XmlException& exception) {
                if (exception.Line() != 0 || exception.Column() != 0) {
                    throw;
                }

                const auto [line, column] = ComputeLineColumn(errorPosition);
                throw XmlException(exception.what(), line, column);
            }
        };

        SkipWhitespace();
        if (ClassifyMarkup() == XmlMarkupKind::XmlDeclaration) {
            const auto nodeStart = position_;
            appendParsedNode(ParseDeclaration(document), nodeStart);
        }

        SkipWhitespace();
        while (true) {
            bool consumedMarkup = true;
            switch (ClassifyMarkup()) {
            case XmlMarkupKind::Comment: {
                const auto nodeStart = position_;
                appendParsedNode(ParseComment(document), nodeStart);
                break;
            }
            case XmlMarkupKind::ProcessingInstruction: {
                const auto nodeStart = position_;
                appendParsedNode(ParseProcessingInstruction(document), nodeStart);
                break;
            }
            case XmlMarkupKind::DocumentType: {
                const auto nodeStart = position_;
                appendParsedNode(ParseDocumentType(document), nodeStart);
                break;
            }
            default:
                consumedMarkup = false;
                break;
            }

            if (!consumedMarkup) {
                break;
            }

            SkipWhitespace();
        }

        if (Peek() != '<') {
            Throw("Document root element is missing");
        }

        if (ClassifyMarkup() == XmlMarkupKind::EndTag) {
            Consume("</");
            const std::string closeName = ParseName();
            Throw("Unexpected closing tag: </" + closeName + ">");
        }

        if (ClassifyMarkup() == XmlMarkupKind::UnsupportedDeclaration) {
            Throw("Unsupported markup declaration");
        }

        const auto rootStart = position_;
        auto rootElement = ParseElement(document);
        if (!documentTypeName_.empty() && rootElement->Name() != documentTypeName_) {
            Throw("Document type name '" + documentTypeName_ + "' does not match root element name '" + rootElement->Name() + "'");
        }
        appendParsedNode(rootElement, rootStart);
        SkipWhitespace();

        while (!IsEnd()) {
            switch (ClassifyMarkup()) {
            case XmlMarkupKind::Comment: {
                const auto nodeStart = position_;
                appendParsedNode(ParseComment(document), nodeStart);
                break;
            }
            case XmlMarkupKind::ProcessingInstruction: {
                const auto nodeStart = position_;
                appendParsedNode(ParseProcessingInstruction(document), nodeStart);
                break;
            }
            case XmlMarkupKind::DocumentType:
                Throw("DOCTYPE must appear before the root element");
                break;
            default:
                if (IsWhitespace(Peek())) {
                    SkipWhitespace();
                } else {
                    Throw("Unexpected content after the root element");
                }
                break;
            }
        }

        ValidatePendingDtdIdReferences();
    }

private:
    void ValidatePendingDtdIdReferences() const {
        if (pendingDtdIdReferences_.empty()) {
            return;
        }
    }

    XmlMarkupKind ClassifyMarkup() const noexcept {
        return ClassifyXmlMarkupWithCharAt(
            position_,
            [this](std::size_t position) noexcept {
                return position < input_.size() ? input_[position] : '\0';
            });
    }

    bool IsEnd() const noexcept {
        return position_ >= input_.size();
    }

    char Peek() const noexcept {
        return IsEnd() ? '\0' : input_[position_];
    }

    char Read() {
        if (IsEnd()) {
            Throw("Unexpected end of XML document");
        }

        return input_[position_++];
    }

    bool StartsWith(std::string_view token) const noexcept {
        return input_.substr(position_, token.size()) == token;
    }

    void Consume(std::string_view token) {
        if (!StartsWith(token)) {
            Throw("Expected '" + std::string(token) + "'");
        }

        position_ += token.size();
    }

    void SkipWhitespace() {
        SkipXmlWhitespaceAt(input_, position_);
    }

    void Expect(char ch) {
        if (Read() != ch) {
            Throw(std::string("Expected '") + ch + "'");
        }
    }

    [[noreturn]] void Throw(const std::string& message) const {
        auto [line, column] = ComputeLineColumn(position_);
        throw XmlException(message, line, column);
    }

    std::pair<std::size_t, std::size_t> ComputeLineColumn(std::size_t position) const noexcept {
        std::size_t line = 1;
        std::size_t column = 1;

        for (std::size_t index = 0; index < position && index < input_.size(); ++index) {
            if (input_[index] == '\n') {
                ++line;
                column = 1;
            } else {
                ++column;
            }
        }

        return {line, column};
    }

    std::string ParseName() {
        const auto start = position_;
        bool usedAsciiFastPath = false;
        if (start < input_.size()) {
            const unsigned char first = static_cast<unsigned char>(input_[start]);
            if (first < 0x80 && IsNameStartChar(static_cast<char>(first))) {
                const std::size_t available = input_.size() - start;
                const std::size_t nameLength = 1 + ConsumeNameCharsInBuffer(input_.data() + start + 1, available - 1);
                if (nameLength < available && static_cast<unsigned char>(input_[start + nameLength]) < 0x80) {
                    position_ = start + nameLength;
                    usedAsciiFastPath = true;
                }
            }
        }

        if (!usedAsciiFastPath) {
            position_ = ConsumeXmlNameAt(position_, [this](std::size_t index) noexcept {
                return index < input_.size() ? input_[index] : '\0';
            });
            if (position_ == start) {
                Throw("Invalid XML name");
            }
        }

        std::string name(input_.substr(start, position_ - start));
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

    std::string DecodeEntities(std::string_view value) const {
        return DecodeEntityText(
            value,
            [this](const std::string& entity) {
                return LookupDocumentInternalEntityDeclaration(document_, entity);
            },
            [this](const std::string& message) {
                auto [line, column] = ComputeLineColumn(position_);
                throw XmlException(message, line, column);
            });
    }

    std::string ParseQuotedValue(bool decodeEntities = true) {
        const char quote = Read();
        if (quote != '\"' && quote != '\'') {
            Throw("Expected quoted value");
        }

        const auto start = position_;
        const auto quoteEnd = ScanQuotedValueEndAt(start - 1, [this](std::size_t position) noexcept {
            return position < input_.size() ? input_[position] : '\0';
        });
        if (quoteEnd == std::string::npos) {
            Throw("Unterminated quoted value");
        }

        const auto raw = input_.substr(start, quoteEnd - start);
        position_ = quoteEnd + 1;
        return decodeEntities ? DecodeEntities(raw) : std::string(raw);
    }

    void ApplyDtdAttributeDeclarations(XmlElement& element) {
        const auto fixedIt = dtdFixedAttributes_.find(element.Name());
        if (fixedIt != dtdFixedAttributes_.end()) {
            for (const auto& [attributeName, fixedValue] : fixedIt->second) {
                if (element.HasAttribute(attributeName) && element.GetAttribute(attributeName) != fixedValue) {
                    throw XmlException(
                        "DTD validation failed: attribute '" + attributeName
                        + "' on element '" + element.Name() + "' must match the fixed value '" + fixedValue + "'");
                }
            }
        }

        const auto requiredIt = dtdRequiredAttributes_.find(element.Name());
        if (requiredIt != dtdRequiredAttributes_.end()) {
            for (const auto& attributeName : requiredIt->second) {
                if (!element.HasAttribute(attributeName)) {
                    throw XmlException(
                        "DTD validation failed: required attribute '" + attributeName
                        + "' is missing on element '" + element.Name() + "'");
                }
            }
        }

        const auto defaultIt = dtdDefaultAttributes_.find(element.Name());
        if (defaultIt != dtdDefaultAttributes_.end()) {
            for (const auto& [attributeName, defaultValue] : defaultIt->second) {
                if (!element.HasAttribute(attributeName)) {
                    element.SetAttribute(attributeName, defaultValue);
                }
            }
        }

        const auto enumeratedIt = dtdEnumeratedAttributes_.find(element.Name());
        const auto nmTokenIt = dtdNmTokenAttributes_.find(element.Name());
        const auto nameIt = dtdNameAttributes_.find(element.Name());
        const auto idIt = dtdIdAttributes_.find(element.Name());
        if (idIt != dtdIdAttributes_.end() && element.HasAttribute(idIt->second)) {
            const std::string value = element.GetAttribute(idIt->second);
            if (!IsValidDtdName(value)) {
                throw XmlException(
                    "DTD validation failed: attribute '" + idIt->second
                    + "' on element '" + element.Name()
                    + "' must be a valid ID value");
            }
            if (!seenDtdIdAttributeValues_.insert(value).second) {
                throw XmlException(
                    "DTD validation failed: attribute '" + idIt->second
                    + "' on element '" + element.Name()
                    + "' duplicates ID value '" + value + "'");
            }
        }

        if (nmTokenIt != dtdNmTokenAttributes_.end()) {
            for (const auto& [attributeName, allowMultipleTokens] : nmTokenIt->second) {
                if (!element.HasAttribute(attributeName)) {
                    continue;
                }

                if (!IsValidDtdNmTokensValue(element.GetAttribute(attributeName), allowMultipleTokens)) {
                    throw XmlException(
                        "DTD validation failed: attribute '" + attributeName
                        + "' on element '" + element.Name()
                        + "' must be a valid "
                        + std::string(allowMultipleTokens ? "NMTOKENS" : "NMTOKEN")
                        + " value");
                }
            }
        }

        if (nameIt != dtdNameAttributes_.end()) {
            for (const auto& [attributeName, attributeType] : nameIt->second) {
                if (!element.HasAttribute(attributeName)) {
                    continue;
                }

                const std::string value = element.GetAttribute(attributeName);
                const bool allowsMultipleNames = attributeType == "IDREFS" || attributeType == "ENTITIES";
                if (!IsValidDtdNamesValue(value, allowsMultipleNames)) {
                    throw XmlException(
                        "DTD validation failed: attribute '" + attributeName
                        + "' on element '" + element.Name()
                        + "' must be a valid " + attributeType + " value");
                }

                if (attributeType == "ENTITY" || attributeType == "ENTITIES") {
                    ForEachDtdNamesValueToken(value, [&](std::string_view entityName) {
                        if (!HasDocumentUnparsedEntityDeclaration(document_, entityName)) {
                            throw XmlException(
                                "DTD validation failed: attribute '" + attributeName
                                + "' on element '" + element.Name()
                                + "' must reference a declared unparsed " + attributeType + " value");
                        }
                    });
                } else if (attributeType == "IDREF" || attributeType == "IDREFS") {
                    ForEachDtdNamesValueToken(value, [&](std::string_view idValue) {
                        pendingDtdIdReferences_.push_back(PendingDtdIdReference{
                            std::string(idValue),
                            attributeName,
                            element.Name()});
                    });
                }
            }
        }

        if (enumeratedIt == dtdEnumeratedAttributes_.end()) {
            return;
        }

        for (const auto& [attributeName, enumerationValueSet] : enumeratedIt->second) {
            if (!element.HasAttribute(attributeName)) {
                continue;
            }

            if (!AttributeValueMatchesDtdEnumeration(element.GetAttribute(attributeName), enumerationValueSet)) {
                throw XmlException(
                    "DTD validation failed: attribute '" + attributeName
                    + "' on element '" + element.Name()
                    + "' must match one of " + enumerationValueSet);
            }
        }
    }

    bool ResolveElementPreserveSpace(const XmlElement& element, bool inheritedPreserveSpace) const {
        bool preserveSpace = inheritedPreserveSpace;
        for (const auto& attribute : element.Attributes()) {
            if (attribute == nullptr || attribute->Name() != "xml:space") {
                continue;
            }

            if (IsXmlSpacePreserve(attribute->Value())) {
                preserveSpace = true;
            } else if (IsXmlSpaceDefault(attribute->Value())) {
                preserveSpace = false;
            } else {
                throw XmlException("xml:space must be 'default' or 'preserve'");
            }
        }

        return preserveSpace;
    }

    std::shared_ptr<XmlDeclaration> ParseDeclaration(XmlDocument& document) {
        Consume("<?xml");

        std::string version = "1.0";
        std::string encoding;
        std::string standalone;
        bool sawVersion = false;
        bool sawEncoding = false;
        bool sawStandalone = false;

        while (true) {
            SkipWhitespace();
            if (StartsWith("?>")) {
                Consume("?>");
                break;
            }

            const std::string name = ParseName();
            SkipWhitespace();
            if (Read() != '=') {
                Throw("Expected '=' in XML declaration");
            }
            SkipWhitespace();
            const std::string value = ParseQuotedValue();

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

        return document.CreateXmlDeclaration(version, encoding, standalone);
    }

    std::shared_ptr<XmlComment> ParseComment(XmlDocument& document) {
        Consume("<!--");
        const auto end = ScanDelimitedSectionEndAt(position_, "-->", [this](std::string_view terminator, std::size_t position) noexcept {
            return input_.find(terminator, position);
        });
        if (end == std::string_view::npos) {
            Throw("Unterminated comment");
        }

        for (std::size_t probe = position_; probe < end; ++probe) {
            if (!XmlConvert::IsXmlChar(input_[probe])) {
                const auto [line, column] = ComputeLineColumn(probe);
                throw XmlException("Invalid XML character", line, column);
            }
        }

        const auto invalidDoubleHyphen = input_.find("--", position_);
        if (invalidDoubleHyphen != std::string_view::npos && invalidDoubleHyphen < end) {
            const auto [line, column] = ComputeLineColumn(invalidDoubleHyphen);
            throw XmlException("Comment may not contain '--'", line, column);
        }
        if (end > position_ && input_[end - 1] == '-') {
            const auto [line, column] = ComputeLineColumn(end - 1);
            throw XmlException("Comment may not contain '--'", line, column);
        }

        const auto text = std::string(input_.substr(position_, end - position_));
        position_ = end + 3;
        return document.CreateComment(text);
    }

    std::shared_ptr<XmlCDataSection> ParseCData(XmlDocument& document) {
        Consume("<![CDATA[");
        const auto end = ScanDelimitedSectionEndAt(position_, "]]>", [this](std::string_view terminator, std::size_t position) noexcept {
            return input_.find(terminator, position);
        });
        if (end == std::string_view::npos) {
            Throw("Unterminated CDATA section");
        }

        const auto value = std::string(input_.substr(position_, end - position_));
        position_ = end + 3;
        return document.CreateCDataSection(value);
    }

    std::shared_ptr<XmlProcessingInstruction> ParseProcessingInstruction(XmlDocument& document) {
        Consume("<?");
        const std::string target = ParseName();
        if (!IsNamespaceAwareProcessingInstructionTarget(target)) {
            Throw("Invalid XML name");
        }
        if (target.size() == 3
            && (target[0] == 'x' || target[0] == 'X')
            && (target[1] == 'm' || target[1] == 'M')
            && (target[2] == 'l' || target[2] == 'L')) {
            Throw("XML declaration is only allowed at the beginning of the document");
        }
        if (!IsEnd() && !StartsWith("?>") && !IsWhitespace(input_[position_])) {
            Throw("Malformed processing instruction");
        }

        std::string data;
        if (!StartsWith("?>")) {
            const auto end = ScanDelimitedSectionEndAt(position_, "?>", [this](std::string_view terminator, std::size_t position) noexcept {
                return input_.find(terminator, position);
            });
            if (end == std::string_view::npos) {
                Throw("Unterminated processing instruction");
            }

            for (std::size_t probe = position_; probe < end; ++probe) {
                if (!XmlConvert::IsXmlChar(input_[probe])) {
                    const auto [line, column] = ComputeLineColumn(probe);
                    throw XmlException("Invalid XML character", line, column);
                }
            }

            data = Trim(std::string(input_.substr(position_, end - position_)));
            position_ = end;
        }

        Consume("?>");
        return document.CreateProcessingInstruction(target, data);
    }

    std::shared_ptr<XmlDocumentType> ParseDocumentType(XmlDocument& document) {
        Consume("<!DOCTYPE");
        SkipWhitespace();

        const std::string name = ParseName();
        SkipWhitespace();

        std::string publicId;
        std::string systemId;
        std::string internalSubset;

        if (StartsWith("PUBLIC")) {
            Consume("PUBLIC");
            SkipWhitespace();
            publicId = ParseQuotedValue(false);
            if (!IsValidPublicIdentifierLiteral(publicId)) {
                Throw("Malformed DOCTYPE declaration");
            }
            SkipWhitespace();
            systemId = ParseQuotedValue(false);
            SkipWhitespace();
        } else if (StartsWith("SYSTEM")) {
            Consume("SYSTEM");
            SkipWhitespace();
            systemId = ParseQuotedValue(false);
            SkipWhitespace();
        }

        if (Peek() == '[') {
            Read();
            const auto subsetStart = position_;
            int bracketDepth = 1;
            bool inQuote = false;
            char quote = '\0';

            while (!IsEnd() && bracketDepth > 0) {
                const char ch = Read();
                if (inQuote) {
                    if (ch == quote) {
                        inQuote = false;
                    }
                    continue;
                }

                if (ch == '\"' || ch == '\'') {
                    inQuote = true;
                    quote = ch;
                } else if (ch == '[') {
                    ++bracketDepth;
                } else if (ch == ']') {
                    --bracketDepth;
                }
            }

            if (bracketDepth != 0) {
                Throw("Unterminated DOCTYPE internal subset");
            }

            internalSubset = Trim(std::string(input_.substr(subsetStart, position_ - subsetStart - 1)));
            SkipWhitespace();
        }

        if (Read() != '>') {
            Throw("Expected '>' after DOCTYPE");
        }

        dtdRequiredAttributes_.clear();
        dtdDefaultAttributes_.clear();
        dtdFixedAttributes_.clear();
        dtdEnumeratedAttributes_.clear();
        dtdNmTokenAttributes_.clear();
        dtdNameAttributes_.clear();
        dtdIdAttributes_.clear();
        dtdEmptyElementDeclarations_.clear();
        seenDtdIdAttributeValues_.clear();
        pendingDtdIdReferences_.clear();
        if (!internalSubset.empty()) {
            ParseDocumentTypeEmptyElementDeclarations(internalSubset, dtdEmptyElementDeclarations_);
            std::vector<std::shared_ptr<XmlNode>> entities;
            std::vector<std::shared_ptr<XmlNode>> notations;
            ParseDocumentTypeInternalSubset(internalSubset, entities, notations);
            std::unordered_map<std::string, std::string> internalEntityDeclarations;
            PopulateInternalEntityDeclarations(entities, internalEntityDeclarations);
            DtdDeclaredAttributeNames declaredAttributes;
            DtdIdAttributeDeclarations idAttributes;
            DtdNotationAttributeDeclarations notationAttributes;
            DtdNmTokenAttributeDeclarations nmTokenAttributes;
            DtdNameAttributeDeclarations nameAttributes;
            ParseDocumentTypeAttributeDeclarations(
                internalSubset,
                declaredAttributes,
                dtdRequiredAttributes_,
                dtdDefaultAttributes_,
                dtdFixedAttributes_,
                dtdEnumeratedAttributes_,
                idAttributes,
                notationAttributes,
                nmTokenAttributes,
                nameAttributes);
            ValidateDtdDeclaredAttributeValueReferenceOrder(internalSubset);
            ValidateDtdDeclaredAttributeValues(
                dtdDefaultAttributes_,
                dtdFixedAttributes_,
                internalEntityDeclarations);
            dtdIdAttributes_ = std::move(idAttributes);
            dtdNmTokenAttributes_ = std::move(nmTokenAttributes);
            dtdNameAttributes_ = std::move(nameAttributes);
        }
        documentTypeName_ = name;
        return document.CreateDocumentType(name, publicId, systemId, internalSubset);
    }

    void AppendParsedCharacterData(XmlDocument& document, XmlElement& element, bool preserveSpace) {
        if (dtdEmptyElementDeclarations_.find(element.Name()) != dtdEmptyElementDeclarations_.end()) {
            throw XmlException(
                "DTD validation failed: element '" + element.Name() + "' declared EMPTY must not contain character data");
        }

        std::string textBuffer;
        std::size_t rawSegmentStart = std::string::npos;
        bool textBufferMaterialized = false;
        bool sawEntityReference = false;
        auto flushText = [&](std::size_t segmentEnd, bool preserveWhitespaceOnly = false) {
            if (rawSegmentStart == std::string::npos) {
                return;
            }

            if (!textBufferMaterialized) {
                textBuffer.assign(input_.substr(rawSegmentStart, segmentEnd - rawSegmentStart));
                textBufferMaterialized = true;
            }

            if (preserveSpace && IsWhitespaceOnly(textBuffer)) {
                element.AppendChildForOwnedLoad(document.CreateSignificantWhitespace(textBuffer));
            } else if (document.PreserveWhitespace() && IsWhitespaceOnly(textBuffer)) {
                element.AppendChildForOwnedLoad(document.CreateWhitespace(textBuffer));
            } else if (document.PreserveWhitespace() || !IsWhitespaceOnly(textBuffer) || preserveWhitespaceOnly) {
                element.AppendChildForOwnedLoad(document.CreateTextNode(textBuffer));
            }
            textBuffer.clear();
            rawSegmentStart = std::string::npos;
            textBufferMaterialized = false;
        };

        while (!IsEnd() && Peek() != '<') {
            if (StartsWith("]]>") ) {
                Throw("']]>' is not allowed in text content");
            }

            if (Peek() != '&') {
                if (!XmlConvert::IsXmlChar(Peek())) {
                    Throw("Invalid XML character");
                }
                if (rawSegmentStart == std::string::npos) {
                    rawSegmentStart = position_;
                }
                if (textBufferMaterialized) {
                    textBuffer.push_back(Read());
                } else {
                    ++position_;
                }
                continue;
            }

            const auto semicolon = input_.find(';', position_);
            if (semicolon == std::string_view::npos) {
                Throw("Unterminated entity reference");
            }

            const auto entityStart = position_;
            const std::string entity(input_.substr(position_ + 1, semicolon - position_ - 1));
            position_ = semicolon + 1;

            const auto predefined = ResolvePredefinedEntityReferenceValueView(entity);
            if (!predefined.empty()) {
                if (rawSegmentStart == std::string::npos) {
                    rawSegmentStart = entityStart;
                }
                if (!textBufferMaterialized) {
                    textBuffer.assign(input_.substr(rawSegmentStart, entityStart - rawSegmentStart));
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
                    textBuffer.assign(input_.substr(rawSegmentStart, entityStart - rawSegmentStart));
                    textBufferMaterialized = true;
                }
                unsigned int codePoint = 0;
                if (!TryParseNumericEntityReferenceCodePoint(entity, codePoint)) {
                    Throw("Invalid numeric entity reference: &" + entity + ';');
                }
                AppendCodePointUtf8(textBuffer, codePoint);
                continue;
            }

            if (!HasDocumentEntityDeclaration(document_, entity)) {
                Throw("Unknown entity reference: &" + entity + ';');
            }

            flushText(entityStart, true);
            sawEntityReference = true;
            element.AppendChildForOwnedLoad(document.CreateEntityReference(entity));
        }

        flushText(position_, sawEntityReference);
    }

    std::shared_ptr<XmlElement> ParseElement(
        XmlDocument& document,
        bool inheritedPreserveSpace = false,
        const std::vector<std::pair<std::string, std::string>>* inheritedNamespaceBindings = nullptr) {
        Expect('<');
        const std::string name = ParseName();
        const auto [elementPrefix, elementLocalName] = SplitQualifiedNameView(name);
        if (!IsValidXmlQualifiedName(name) || elementPrefix == "xmlns") {
            Throw("Invalid XML name");
        }
        static_cast<void>(elementLocalName);
        auto element = document.CreateElement(name);
        const std::string_view text = input_;
        bool childPreserveSpace = inheritedPreserveSpace;
        std::vector<std::pair<std::string, std::string>> localNamespaceDeclarations;
        bool hasNamespaceSensitiveNames = !elementPrefix.empty();

        auto lookupNamespace = [&localNamespaceDeclarations, inheritedNamespaceBindings](std::string_view prefix) {
            for (auto it = localNamespaceDeclarations.rbegin(); it != localNamespaceDeclarations.rend(); ++it) {
                if (std::string_view(it->first) == prefix) {
                    return it->second;
                }
            }
            if (inheritedNamespaceBindings != nullptr) {
                for (auto it = inheritedNamespaceBindings->rbegin(); it != inheritedNamespaceBindings->rend(); ++it) {
                    if (std::string_view(it->first) == prefix) {
                        return it->second;
                    }
                }
            }
            if (prefix == "xml") {
                return std::string("http://www.w3.org/XML/1998/namespace");
            }
            if (prefix == "xmlns") {
                return std::string("http://www.w3.org/2000/xmlns/");
            }
            return std::string{};
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

        auto namespaceDeclarationUsesTokenizedType = [this, &name](std::string_view attributeName) {
            const auto idIt = dtdIdAttributes_.find(name);
            if (idIt != dtdIdAttributes_.end() && idIt->second == attributeName) {
                return true;
            }

            const auto nmTokenIt = dtdNmTokenAttributes_.find(name);
            if (nmTokenIt != dtdNmTokenAttributes_.end()
                && nmTokenIt->second.find(std::string(attributeName)) != nmTokenIt->second.end()) {
                return true;
            }

            const auto nameIt = dtdNameAttributes_.find(name);
            if (nameIt != dtdNameAttributes_.end()
                && nameIt->second.find(std::string(attributeName)) != nameIt->second.end()) {
                return true;
            }

            const auto enumeratedIt = dtdEnumeratedAttributes_.find(name);
            if (enumeratedIt != dtdEnumeratedAttributes_.end()
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

        auto validateNamespaceAwareNames = [&element, &lookupNamespace](std::string_view currentElementPrefix) {
            if (!currentElementPrefix.empty() && lookupNamespace(currentElementPrefix).empty()) {
                throw XmlException("Undeclared namespace prefix '" + std::string(currentElementPrefix) + "'");
            }

            std::vector<std::pair<std::string, std::string>> expandedAttributeNames;
            const auto& attributes = element->Attributes();
            expandedAttributeNames.reserve(attributes.size());
            for (const auto& attribute : attributes) {
                if (attribute == nullptr) {
                    continue;
                }

                const std::string_view attributeName = attribute->Name();
                const auto [attributePrefix, attributeLocalName] = SplitQualifiedNameView(attributeName);
                if (!IsValidXmlQualifiedName(attributeName)
                    || (attributePrefix == "xmlns" && !IsNamespaceDeclarationName(attributeName))) {
                    throw XmlException("Invalid XML name");
                }

                std::string namespaceUri;
                if (attributeName == "xmlns" || attributePrefix == "xmlns") {
                    namespaceUri = "http://www.w3.org/2000/xmlns/";
                } else if (!attributePrefix.empty()) {
                    namespaceUri = lookupNamespace(attributePrefix);
                    if (namespaceUri.empty()) {
                        throw XmlException("Undeclared namespace prefix '" + std::string(attributePrefix) + "'");
                    }
                }

                for (const auto& expanded : expandedAttributeNames) {
                    if (expanded.first == namespaceUri && expanded.second == attributeLocalName) {
                        throw XmlException("Duplicate attribute '" + std::string(attributeName) + "'");
                    }
                }
                expandedAttributeNames.emplace_back(std::move(namespaceUri), std::string(attributeLocalName));
            }
        };

        while (true) {
            SkipWhitespace();
            bool isEmptyElement = false;
            if (ConsumeXmlStartTagCloseAt(
                    position_,
                    isEmptyElement,
                    [this](std::size_t position) noexcept {
                        return position < input_.size() ? input_[position] : '\0';
                    },
                    [&text](std::size_t position, std::string_view token) noexcept {
                        return SubsetStartsWithAt(text, position, token);
                    })) {
                ApplyDtdAttributeDeclarations(*element);
                const bool inheritedNamespacesPresent = inheritedNamespaceBindings != nullptr && !inheritedNamespaceBindings->empty();
                const bool mayNeedDtdNamespaceScan = dtdDefaultAttributes_.find(name) != dtdDefaultAttributes_.end();
                if (hasNamespaceSensitiveNames || inheritedNamespacesPresent || mayNeedDtdNamespaceScan) {
                    for (const auto& attribute : element->Attributes()) {
                        if (attribute == nullptr) {
                            continue;
                        }

                        const std::string_view attributeName = attribute->Name();
                        const auto [attributePrefix, attributeLocalName] = SplitQualifiedNameView(attributeName);
                        if (!attributePrefix.empty() || attributeName == "xmlns") {
                            hasNamespaceSensitiveNames = true;
                        }
                        if (!IsNamespaceDeclarationName(attributeName)) {
                            continue;
                        }

                        std::string normalizedValue = normalizeNamespaceDeclarationValue(attributeName, attribute->Value());
                        setLocalNamespaceDeclaration(NamespaceDeclarationPrefix(std::string(attributeName)), std::move(normalizedValue));
                    }
                }
                if (hasNamespaceSensitiveNames || inheritedNamespacesPresent || !localNamespaceDeclarations.empty()) {
                    validateNamespaceAwareNames(elementPrefix);
                }
                childPreserveSpace = ResolveElementPreserveSpace(*element, inheritedPreserveSpace);
                if (isEmptyElement) {
                    return element;
                }
                break;
            }

            std::size_t cursor = position_;
            const auto attributeToken = ParseXmlAttributeAssignmentAt(
                cursor,
                [&text](std::size_t& position) {
                    return ParseNameAt(text, position);
                },
                [&text](std::size_t& position) noexcept {
                    SkipXmlWhitespaceAt(text, position);
                },
                [this](std::size_t position) noexcept {
                    return position < input_.size() ? input_[position] : '\0';
                },
                [this](std::size_t position) noexcept {
                    return ScanQuotedValueEndAt(position, [this](std::size_t probe) noexcept {
                        return probe < input_.size() ? input_[probe] : '\0';
                    });
                });

            if (attributeToken.name.empty()) {
                Throw("Invalid XML name");
            }
            const auto [attributePrefix, attributeLocalName] = SplitQualifiedNameView(attributeToken.name);
            if (!IsValidXmlQualifiedName(attributeToken.name)
                || (attributePrefix == "xmlns" && !IsNamespaceDeclarationName(attributeToken.name))) {
                Throw("Invalid XML name");
            }
            if (!attributePrefix.empty() || attributeToken.name == "xmlns") {
                hasNamespaceSensitiveNames = true;
            }
            if (!attributeToken.valid) {
                position_ = cursor;
                if (!attributeToken.sawEquals) {
                    if (!IsEnd()) {
                        ++position_;
                    }
                    Throw("Expected '=' after attribute name");
                }
                if (!IsEnd()) {
                    ++position_;
                }
                Throw("Expected quoted value");
            }

            position_ = cursor;
            const auto invalidLt = input_.find('<', attributeToken.rawValueStart);
            if (invalidLt != std::string_view::npos && invalidLt < attributeToken.rawValueEnd) {
                const auto [line, column] = ComputeLineColumn(invalidLt);
                throw XmlException("'<' is not allowed in attribute values", line, column);
            }
            if (element->HasAttribute(attributeToken.name)) {
                Throw("Duplicate attribute '" + attributeToken.name + "'");
            }
            const auto ampersand = input_.find('&', attributeToken.rawValueStart);
            const bool needsEntityDecoding = ampersand != std::string_view::npos
                && ampersand < attributeToken.rawValueEnd;
            std::string attributeValue = needsEntityDecoding
                ? DecodeEntities(input_.substr(attributeToken.rawValueStart, attributeToken.rawValueEnd - attributeToken.rawValueStart))
                : std::string(input_.substr(attributeToken.rawValueStart, attributeToken.rawValueEnd - attributeToken.rawValueStart));
            if (attributeToken.name == "xmlns") {
                attributeValue = normalizeNamespaceDeclarationValue(attributeToken.name, std::move(attributeValue));
                ValidateNamespaceDeclarationBinding({}, attributeValue);
                setLocalNamespaceDeclaration({}, attributeValue);
            } else {
                if (attributePrefix == "xmlns") {
                    attributeValue = normalizeNamespaceDeclarationValue(attributeToken.name, std::move(attributeValue));
                    ValidateNamespaceDeclarationBinding(attributeLocalName, attributeValue);
                    setLocalNamespaceDeclaration(std::string(attributeLocalName), attributeValue);
                } else if (attributeToken.name == "xml:space"
                    && !IsXmlSpacePreserve(attributeValue)
                    && !IsXmlSpaceDefault(attributeValue)) {
                    Throw("xml:space must be 'default' or 'preserve'");
                }
            }
            element->AppendAttributeForLoad(
                attributeToken.name,
                std::move(attributeValue));
        }

        while (true) {
            switch (ClassifyMarkup()) {
            case XmlMarkupKind::EndTag: {
                Consume("</");
                const std::string closeName = ParseName();
                const auto [closePrefix, closeLocalName] = SplitQualifiedNameView(closeName);
                if (!IsValidXmlQualifiedName(closeName) || closePrefix == "xmlns") {
                    Throw("Invalid XML name");
                }
                static_cast<void>(closeLocalName);
                if (closeName != name) {
                    Throw("Mismatched closing tag. Expected </" + name + "> but found </" + closeName + ">");
                }

                std::size_t cursor = position_;
                if (!ConsumeXmlEndTagCloseAt(
                        cursor,
                        [&text](std::size_t& position) noexcept {
                            SkipXmlWhitespaceAt(text, position);
                        },
                        [this](std::size_t position) noexcept {
                            return position < input_.size() ? input_[position] : '\0';
                        })) {
                    position_ = cursor;
                    if (!IsEnd()) {
                        ++position_;
                    }
                    Throw("Expected '>' after closing tag");
                }
                position_ = cursor;
                return element;
            }
            case XmlMarkupKind::Comment:
                element->AppendChildForOwnedLoad(ParseComment(document));
                break;
            case XmlMarkupKind::CData:
                if (dtdEmptyElementDeclarations_.find(name) != dtdEmptyElementDeclarations_.end()) {
                    throw XmlException(
                        "DTD validation failed: element '" + name + "' declared EMPTY must not contain character data");
                }
                element->AppendChildForOwnedLoad(ParseCData(document));
                break;
            case XmlMarkupKind::ProcessingInstruction:
                element->AppendChildForOwnedLoad(ParseProcessingInstruction(document));
                break;
            case XmlMarkupKind::DocumentType:
                Throw("DOCTYPE is not allowed inside an element");
                break;
            case XmlMarkupKind::UnsupportedDeclaration:
                Throw("Unsupported markup declaration");
                break;
            case XmlMarkupKind::Element:
                if (dtdEmptyElementDeclarations_.find(name) != dtdEmptyElementDeclarations_.end()) {
                    throw XmlException(
                        "DTD validation failed: element '" + name + "' declared EMPTY must not contain child elements");
                }
                {
                    const std::vector<std::pair<std::string, std::string>>* childNamespaceBindings = inheritedNamespaceBindings;
                    std::vector<std::pair<std::string, std::string>> mergedNamespaceBindings;
                    if (!localNamespaceDeclarations.empty()) {
                        if (inheritedNamespaceBindings == nullptr || inheritedNamespaceBindings->empty()) {
                            childNamespaceBindings = &localNamespaceDeclarations;
                        } else {
                            mergedNamespaceBindings = *inheritedNamespaceBindings;
                            mergedNamespaceBindings.insert(
                                mergedNamespaceBindings.end(),
                                localNamespaceDeclarations.begin(),
                                localNamespaceDeclarations.end());
                            childNamespaceBindings = &mergedNamespaceBindings;
                        }
                    }
                    element->AppendChildForOwnedLoad(ParseElement(document, childPreserveSpace, childNamespaceBindings));
                }
                break;
            case XmlMarkupKind::None:
                if (IsEnd()) {
                    Throw("Unexpected end of input inside element <" + name + ">");
                }
                AppendParsedCharacterData(document, *element, childPreserveSpace);
                break;
            case XmlMarkupKind::XmlDeclaration:
                position_ += 5;
                Throw("XML declaration is only allowed at the beginning of the document");
                break;
            }
        }
    }

    std::string_view input_;
    std::size_t position_;
    XmlDocument* document_ = nullptr;
    DtdRequiredAttributeDeclarations dtdRequiredAttributes_;
    DtdDefaultAttributeDeclarations dtdDefaultAttributes_;
    DtdFixedAttributeDeclarations dtdFixedAttributes_;
    DtdEnumeratedAttributeDeclarations dtdEnumeratedAttributes_;
    DtdNmTokenAttributeDeclarations dtdNmTokenAttributes_;
    DtdNameAttributeDeclarations dtdNameAttributes_;
    DtdIdAttributeDeclarations dtdIdAttributes_;
    DtdEmptyElementDeclarations dtdEmptyElementDeclarations_;
    std::unordered_set<std::string> seenDtdIdAttributeValues_;
    std::vector<PendingDtdIdReference> pendingDtdIdReferences_;
    std::string documentTypeName_;
    bool sourceWasUtf16_ = false;
    bool sourceWasUtf8Bom_ = false;
};

std::shared_ptr<XmlDocument> BuildXmlDocumentFromReader(XmlReader& reader) {
    auto document = std::make_shared<XmlDocument>();
    LoadXmlDocumentFromReader(reader, *document);
    return document;
}

XmlDocument::XmlDocument()
    : XmlNode(XmlNodeType::Document, "#document"),
            nodeArenaState_(std::make_shared<NodeArenaState>()) {
    SetOwnerDocumentRecursive(this);
}

XmlDocument::~XmlDocument() {
    RemoveAllChildren();
}

std::shared_ptr<XmlDocument> XmlDocument::Parse(std::string_view xml) {
    auto document = std::make_shared<XmlDocument>();
    document->LoadXml(xml);
    return document;
}

std::shared_ptr<XmlDocument> XmlDocument::Parse(std::string_view xml, const XmlReaderSettings& settings) {
    auto document = std::make_shared<XmlDocument>();
    document->LoadXml(xml, settings);
    return document;
}

void XmlDocument::LoadXml(std::string_view xml) {
    if (CanUseReaderBackedLoadXmlFastPath(xml)) {
        auto reader = XmlReader::Create(std::string(xml));
        LoadXmlDocumentFromReader(reader, *this);
        return;
    }

    XmlParser(xml, false).ParseInto(*this);
}

void XmlDocument::LoadXml(std::string_view xml, const XmlReaderSettings& settings) {
    auto reader = XmlReader::Create(std::string(xml), settings);
    LoadXmlDocumentFromReader(reader, *this);
}

void XmlDocument::Load(const std::string& path) {
    auto reader = XmlReader::CreateFromFile(path);
    LoadXmlDocumentFromReader(reader, *this);
}

void XmlDocument::Load(const std::string& path, const XmlReaderSettings& settings) {
    auto reader = XmlReader::CreateFromFile(path, settings);
    LoadXmlDocumentFromReader(reader, *this);
}

void XmlDocument::Load(std::istream& stream) {
    auto reader = XmlReader::Create(stream);
    LoadXmlDocumentFromReader(reader, *this);
}

void XmlDocument::Load(std::istream& stream, const XmlReaderSettings& settings) {
    auto reader = XmlReader::Create(stream, settings);
    LoadXmlDocumentFromReader(reader, *this);
}

void XmlDocument::Save(const std::string& path, const XmlWriterSettings& settings) const {
    XmlWriter::WriteToFile(*this, path, settings);
}

void XmlDocument::Save(std::ostream& stream, const XmlWriterSettings& settings) const {
    XmlWriter::WriteToStream(*this, stream, settings);
}

std::string NormalizeSchemaSimpleTypeValue(const XmlSchemaSet::SimpleTypeRule& type, std::string_view lexicalValue) {
    if (!type.whiteSpace.has_value() || *type.whiteSpace == "preserve") {
        return std::string(lexicalValue);
    }

    std::string normalized;
    normalized.reserve(lexicalValue.size());
    if (*type.whiteSpace == "replace") {
        for (const char ch : lexicalValue) {
            if (IsWhitespace(ch)) {
                normalized.push_back(' ');
            } else {
                normalized.push_back(ch);
            }
        }
        return normalized;
    }

    if (*type.whiteSpace == "collapse") {
        bool inWhitespace = true;
        for (const char ch : lexicalValue) {
            if (IsWhitespace(ch)) {
                inWhitespace = true;
                continue;
            }
            if (!normalized.empty() && inWhitespace) {
                normalized.push_back(' ');
            }
            normalized.push_back(ch);
            inWhitespace = false;
        }
        return normalized;
    }

    throw XmlException("Schema validation failed: unsupported whiteSpace facet '" + *type.whiteSpace + "'");
}

std::string LookupNamespaceUriInBindings(
    const std::vector<std::pair<std::string, std::string>>& namespaceBindings,
    const std::string& prefix) {
    for (auto it = namespaceBindings.rbegin(); it != namespaceBindings.rend(); ++it) {
        if (it->first == prefix) {
            return it->second;
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

void ValidateSchemaSimpleValueWithQNameResolver(
    const XmlSchemaSet::SimpleTypeRule& type,
    std::string_view value,
    std::string_view label,
    const std::function<std::string(const std::string&)>& resolveNamespaceUri,
    const std::function<bool(const std::string&)>& hasNotationDeclaration,
    const std::function<bool(const std::string&)>& hasUnparsedEntityDeclaration) {
    const auto isPracticalLanguageValue = [](const std::string& lexicalValue) {
        if (lexicalValue.empty()) {
            return false;
        }

        std::size_t segmentLength = 0;
        bool expectLetterOnly = true;
        for (const char ch : lexicalValue) {
            if (ch == '-') {
                if (segmentLength == 0) {
                    return false;
                }
                segmentLength = 0;
                expectLetterOnly = false;
                continue;
            }

            const unsigned char current = static_cast<unsigned char>(ch);
            if (segmentLength >= 8) {
                return false;
            }
            if (expectLetterOnly) {
                if (!std::isalpha(current)) {
                    return false;
                }
            } else if (!std::isalnum(current)) {
                return false;
            }
            ++segmentLength;
        }

        return segmentLength > 0;
    };

    const auto isPracticalAnyUriValue = [&](const std::string& lexicalValue) {
        for (const char ch : lexicalValue) {
            const unsigned char current = static_cast<unsigned char>(ch);
            if (IsWhitespace(ch) || current < 0x20) {
                return false;
            }
        }
        return true;
    };

    const auto decodePracticalBinaryValue = [&](const std::string& baseType, const std::string& lexicalValue) {
        std::vector<unsigned char> decoded;
        bool success = false;
        if (baseType == "xs:hexBinary") {
            success = TryDecodeHexBinary(lexicalValue, decoded);
        } else if (baseType == "xs:base64Binary") {
            success = TryDecodeBase64Binary(lexicalValue, decoded);
        }
        if (!success) {
            throw XmlException("invalid binary lexical form");
        }
        return decoded;
    };

    struct PracticalTemporalValue {
        long long primary = 0;
        int secondary = 0;
        int fractionalNanoseconds = 0;
    };

    const auto isLeapYear = [](const int year) {
        return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    };

    const auto daysInMonth = [&](const int year, const int month) {
        static const int kDaysInMonth[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
        if (month < 1 || month > 12) {
            return 0;
        }
        if (month == 2 && isLeapYear(year)) {
            return 29;
        }
        return kDaysInMonth[month - 1];
    };

    const auto daysFromCivil = [](int year, const unsigned month, const unsigned day) -> long long {
        year -= month <= 2 ? 1 : 0;
        const long long era = static_cast<long long>(year >= 0 ? year : year - 399) / 400LL;
        const unsigned yoe = static_cast<unsigned>(year - static_cast<int>(era * 400LL));
        const unsigned shiftedMonth = month > 2 ? month - 3 : month + 9;
        const unsigned doy = (153 * shiftedMonth + 2) / 5 + day - 1;
        const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
        return era * 146097LL + static_cast<long long>(doe) - 719468LL;
    };

    const auto compareTemporalValues = [](const PracticalTemporalValue& left, const PracticalTemporalValue& right) {
        if (left.primary != right.primary) {
            return left.primary < right.primary ? -1 : 1;
        }
        if (left.secondary != right.secondary) {
            return left.secondary < right.secondary ? -1 : 1;
        }
        if (left.fractionalNanoseconds != right.fractionalNanoseconds) {
            return left.fractionalNanoseconds < right.fractionalNanoseconds ? -1 : 1;
        }
        return 0;
    };

    const auto normalizeTemporalValue = [&](const std::string& baseType, const std::string& lexicalValue) -> PracticalTemporalValue {
        const auto parseFixedWidthNumber = [&](const std::string& text, const std::size_t start, const std::size_t length) {
            if (start + length > text.size()) {
                throw XmlException("invalid temporal lexical form");
            }
            int value = 0;
            for (std::size_t index = 0; index < length; ++index) {
                const char ch = text[start + index];
                if (!std::isdigit(static_cast<unsigned char>(ch))) {
                    throw XmlException("invalid temporal lexical form");
                }
                value = value * 10 + (ch - '0');
            }
            return value;
        };

        const auto parseTimezoneOffsetMinutes = [&](const std::string& text, const std::size_t start) {
            if (start >= text.size()) {
                return 0;
            }
            if (text[start] == 'Z') {
                if (start + 1 != text.size()) {
                    throw XmlException("invalid temporal lexical form");
                }
                return 0;
            }
            if ((text[start] != '+' && text[start] != '-') || start + 6 != text.size() || text[start + 3] != ':') {
                throw XmlException("invalid temporal lexical form");
            }
            const int hours = parseFixedWidthNumber(text, start + 1, 2);
            const int minutes = parseFixedWidthNumber(text, start + 4, 2);
            if (hours > 14 || minutes > 59 || (hours == 14 && minutes != 0)) {
                throw XmlException("invalid temporal lexical form");
            }
            const int totalMinutes = hours * 60 + minutes;
            return text[start] == '-' ? -totalMinutes : totalMinutes;
        };

        const auto parseFractionalNanoseconds = [&](const std::string& digits) {
            if (digits.empty()) {
                throw XmlException("invalid temporal lexical form");
            }
            int value = 0;
            for (std::size_t index = 0; index < 9; ++index) {
                value *= 10;
                if (index < digits.size()) {
                    const char ch = digits[index];
                    if (!std::isdigit(static_cast<unsigned char>(ch))) {
                        throw XmlException("invalid temporal lexical form");
                    }
                    value += ch - '0';
                }
            }
            for (std::size_t index = 9; index < digits.size(); ++index) {
                if (!std::isdigit(static_cast<unsigned char>(digits[index]))) {
                    throw XmlException("invalid temporal lexical form");
                }
            }
            return value;
        };

        auto normalizeDateTimeComponents = [](long long& dayValue, int& secondsOfDay, const int offsetMinutes) {
            secondsOfDay -= offsetMinutes * 60;
            while (secondsOfDay < 0) {
                secondsOfDay += 24 * 60 * 60;
                --dayValue;
            }
            while (secondsOfDay >= 24 * 60 * 60) {
                secondsOfDay -= 24 * 60 * 60;
                ++dayValue;
            }
        };

        if (baseType == "xs:date") {
            if (lexicalValue.size() < 10 || lexicalValue[4] != '-' || lexicalValue[7] != '-') {
                throw XmlException("invalid temporal lexical form");
            }
            const int year = parseFixedWidthNumber(lexicalValue, 0, 4);
            const int month = parseFixedWidthNumber(lexicalValue, 5, 2);
            const int day = parseFixedWidthNumber(lexicalValue, 8, 2);
            if (day < 1 || day > daysInMonth(year, month)) {
                throw XmlException("invalid temporal lexical form");
            }

            const int timezoneOffsetMinutes = parseTimezoneOffsetMinutes(lexicalValue, 10);
            long long absoluteDay = daysFromCivil(year, static_cast<unsigned>(month), static_cast<unsigned>(day));
            int secondsOfDay = 0;
            normalizeDateTimeComponents(absoluteDay, secondsOfDay, timezoneOffsetMinutes);
            return PracticalTemporalValue{ absoluteDay, secondsOfDay, 0 };
        }

        if (baseType == "xs:gYear") {
            if (lexicalValue.size() < 4) {
                throw XmlException("invalid temporal lexical form");
            }
            const int year = parseFixedWidthNumber(lexicalValue, 0, 4);
            const int timezoneOffsetMinutes = parseTimezoneOffsetMinutes(lexicalValue, 4);
            long long absoluteDay = daysFromCivil(year, 1, 1);
            int secondsOfDay = 0;
            normalizeDateTimeComponents(absoluteDay, secondsOfDay, timezoneOffsetMinutes);
            return PracticalTemporalValue{ absoluteDay, secondsOfDay, 0 };
        }

        if (baseType == "xs:gYearMonth") {
            if (lexicalValue.size() < 7 || lexicalValue[4] != '-') {
                throw XmlException("invalid temporal lexical form");
            }
            const int year = parseFixedWidthNumber(lexicalValue, 0, 4);
            const int month = parseFixedWidthNumber(lexicalValue, 5, 2);
            if (month < 1 || month > 12) {
                throw XmlException("invalid temporal lexical form");
            }

            const int timezoneOffsetMinutes = parseTimezoneOffsetMinutes(lexicalValue, 7);
            long long absoluteDay = daysFromCivil(year, static_cast<unsigned>(month), 1);
            int secondsOfDay = 0;
            normalizeDateTimeComponents(absoluteDay, secondsOfDay, timezoneOffsetMinutes);
            return PracticalTemporalValue{ absoluteDay, secondsOfDay, 0 };
        }

        if (baseType == "xs:gMonth") {
            if (lexicalValue.size() < 4 || lexicalValue[0] != '-' || lexicalValue[1] != '-') {
                throw XmlException("invalid temporal lexical form");
            }
            const int month = parseFixedWidthNumber(lexicalValue, 2, 2);
            if (month < 1 || month > 12) {
                throw XmlException("invalid temporal lexical form");
            }
            const int timezoneOffsetMinutes = parseTimezoneOffsetMinutes(lexicalValue, 4);
            long long absoluteDay = daysFromCivil(2000, static_cast<unsigned>(month), 1);
            int secondsOfDay = 0;
            normalizeDateTimeComponents(absoluteDay, secondsOfDay, timezoneOffsetMinutes);
            return PracticalTemporalValue{ absoluteDay, secondsOfDay, 0 };
        }

        if (baseType == "xs:gDay") {
            if (lexicalValue.size() < 5 || lexicalValue[0] != '-' || lexicalValue[1] != '-' || lexicalValue[2] != '-') {
                throw XmlException("invalid temporal lexical form");
            }
            const int day = parseFixedWidthNumber(lexicalValue, 3, 2);
            if (day < 1 || day > 31) {
                throw XmlException("invalid temporal lexical form");
            }
            const int timezoneOffsetMinutes = parseTimezoneOffsetMinutes(lexicalValue, 5);
            long long absoluteDay = daysFromCivil(2000, 1, static_cast<unsigned>(day));
            int secondsOfDay = 0;
            normalizeDateTimeComponents(absoluteDay, secondsOfDay, timezoneOffsetMinutes);
            return PracticalTemporalValue{ absoluteDay, secondsOfDay, 0 };
        }

        if (baseType == "xs:gMonthDay") {
            if (lexicalValue.size() < 7 || lexicalValue[0] != '-' || lexicalValue[1] != '-' || lexicalValue[4] != '-') {
                throw XmlException("invalid temporal lexical form");
            }
            const int month = parseFixedWidthNumber(lexicalValue, 2, 2);
            const int day = parseFixedWidthNumber(lexicalValue, 5, 2);
            if (day < 1 || day > daysInMonth(2000, month)) {
                throw XmlException("invalid temporal lexical form");
            }
            const int timezoneOffsetMinutes = parseTimezoneOffsetMinutes(lexicalValue, 7);
            long long absoluteDay = daysFromCivil(2000, static_cast<unsigned>(month), static_cast<unsigned>(day));
            int secondsOfDay = 0;
            normalizeDateTimeComponents(absoluteDay, secondsOfDay, timezoneOffsetMinutes);
            return PracticalTemporalValue{ absoluteDay, secondsOfDay, 0 };
        }

        if (baseType == "xs:time") {
            if (lexicalValue.size() < 8 || lexicalValue[2] != ':' || lexicalValue[5] != ':') {
                throw XmlException("invalid temporal lexical form");
            }
            const int hour = parseFixedWidthNumber(lexicalValue, 0, 2);
            const int minute = parseFixedWidthNumber(lexicalValue, 3, 2);
            const int second = parseFixedWidthNumber(lexicalValue, 6, 2);
            if (hour > 23 || minute > 59 || second > 59) {
                throw XmlException("invalid temporal lexical form");
            }

            std::size_t timezoneStart = 8;
            int fractionalNanoseconds = 0;
            if (timezoneStart < lexicalValue.size() && lexicalValue[timezoneStart] == '.') {
                const std::size_t fractionStart = timezoneStart + 1;
                std::size_t fractionEnd = fractionStart;
                while (fractionEnd < lexicalValue.size() && std::isdigit(static_cast<unsigned char>(lexicalValue[fractionEnd]))) {
                    ++fractionEnd;
                }
                fractionalNanoseconds = parseFractionalNanoseconds(lexicalValue.substr(fractionStart, fractionEnd - fractionStart));
                timezoneStart = fractionEnd;
            }

            const int timezoneOffsetMinutes = parseTimezoneOffsetMinutes(lexicalValue, timezoneStart);
            int secondsOfDay = hour * 3600 + minute * 60 + second - timezoneOffsetMinutes * 60;
            secondsOfDay %= 24 * 60 * 60;
            if (secondsOfDay < 0) {
                secondsOfDay += 24 * 60 * 60;
            }
            return PracticalTemporalValue{ 0, secondsOfDay, fractionalNanoseconds };
        }

        if (baseType == "xs:dateTime") {
            if (lexicalValue.size() < 19 || lexicalValue[4] != '-' || lexicalValue[7] != '-' || lexicalValue[10] != 'T'
                || lexicalValue[13] != ':' || lexicalValue[16] != ':') {
                throw XmlException("invalid temporal lexical form");
            }
            const int year = parseFixedWidthNumber(lexicalValue, 0, 4);
            const int month = parseFixedWidthNumber(lexicalValue, 5, 2);
            const int day = parseFixedWidthNumber(lexicalValue, 8, 2);
            const int hour = parseFixedWidthNumber(lexicalValue, 11, 2);
            const int minute = parseFixedWidthNumber(lexicalValue, 14, 2);
            const int second = parseFixedWidthNumber(lexicalValue, 17, 2);
            if (day < 1 || day > daysInMonth(year, month) || hour > 23 || minute > 59 || second > 59) {
                throw XmlException("invalid temporal lexical form");
            }

            std::size_t timezoneStart = 19;
            int fractionalNanoseconds = 0;
            if (timezoneStart < lexicalValue.size() && lexicalValue[timezoneStart] == '.') {
                const std::size_t fractionStart = timezoneStart + 1;
                std::size_t fractionEnd = fractionStart;
                while (fractionEnd < lexicalValue.size() && std::isdigit(static_cast<unsigned char>(lexicalValue[fractionEnd]))) {
                    ++fractionEnd;
                }
                fractionalNanoseconds = parseFractionalNanoseconds(lexicalValue.substr(fractionStart, fractionEnd - fractionStart));
                timezoneStart = fractionEnd;
            }

            const int timezoneOffsetMinutes = parseTimezoneOffsetMinutes(lexicalValue, timezoneStart);
            long long absoluteDay = daysFromCivil(year, static_cast<unsigned>(month), static_cast<unsigned>(day));
            int secondsOfDay = hour * 3600 + minute * 60 + second;
            normalizeDateTimeComponents(absoluteDay, secondsOfDay, timezoneOffsetMinutes);
            return PracticalTemporalValue{ absoluteDay, secondsOfDay, fractionalNanoseconds };
        }

        throw XmlException("unsupported temporal base type");
    };

    const auto extractDigitInfo = [&](const std::string& lexicalValue) {
        struct DigitInfo {
            std::size_t totalDigits = 0;
            std::size_t fractionDigits = 0;
        };

        DigitInfo info;
        bool seenDecimalPoint = false;
        bool hasDigit = false;
        std::size_t index = 0;
        if (!lexicalValue.empty() && (lexicalValue[0] == '+' || lexicalValue[0] == '-')) {
            index = 1;
        }
        for (; index < lexicalValue.size(); ++index) {
            const char ch = lexicalValue[index];
            if (ch == '.') {
                if (seenDecimalPoint) {
                    throw XmlException("Schema validation failed: " + std::string(label) + " must use a plain decimal lexical form for digit facets");
                }
                seenDecimalPoint = true;
                continue;
            }
            if (!std::isdigit(static_cast<unsigned char>(ch))) {
                throw XmlException("Schema validation failed: " + std::string(label) + " must use a plain decimal lexical form for digit facets");
            }
            hasDigit = true;
            ++info.totalDigits;
            if (seenDecimalPoint) {
                ++info.fractionDigits;
            }
        }
        if (!hasDigit) {
            throw XmlException("Schema validation failed: " + std::string(label) + " must use a plain decimal lexical form for digit facets");
        }
        return info;
    };

    const std::string normalizedValue = NormalizeSchemaSimpleTypeValue(type, value);
    const bool listVariety = type.variety == XmlSchemaSet::SimpleTypeRule::Variety::List;
    const bool unionVariety = type.variety == XmlSchemaSet::SimpleTypeRule::Variety::Union;

    if (listVariety) {
        if (type.itemType == nullptr) {
            throw XmlException("Schema validation failed: list simpleType is missing an itemType");
        }

        std::vector<std::string> items;
        std::string current;
        std::istringstream stream(normalizedValue);
        while (stream >> current) {
            items.push_back(current);
        }

        if (type.length.has_value() && items.size() != *type.length) {
            throw XmlException("Schema validation failed: " + std::string(label) + " must contain exactly " + std::to_string(*type.length) + " list items");
        }
        if (type.minLength.has_value() && items.size() < *type.minLength) {
            throw XmlException("Schema validation failed: " + std::string(label) + " must contain at least " + std::to_string(*type.minLength) + " list items");
        }
        if (type.maxLength.has_value() && items.size() > *type.maxLength) {
            throw XmlException("Schema validation failed: " + std::string(label) + " must contain at most " + std::to_string(*type.maxLength) + " list items");
        }

        for (std::size_t index = 0; index < items.size(); ++index) {
            ValidateSchemaSimpleValueWithQNameResolver(
                *type.itemType,
                items[index],
                std::string(label) + " list item " + std::to_string(index + 1),
                resolveNamespaceUri,
                hasNotationDeclaration,
                hasUnparsedEntityDeclaration);
        }
    }

    if (unionVariety) {
        if (type.memberTypes.empty()) {
            throw XmlException("Schema validation failed: union simpleType is missing memberTypes");
        }

        bool matchedMemberType = false;
        for (const auto& memberType : type.memberTypes) {
            try {
                ValidateSchemaSimpleValueWithQNameResolver(
                    memberType,
                    normalizedValue,
                    label,
                    resolveNamespaceUri,
                    hasNotationDeclaration,
                    hasUnparsedEntityDeclaration);
                matchedMemberType = true;
                break;
            } catch (const XmlException&) {
            }
        }
        if (!matchedMemberType) {
            throw XmlException("Schema validation failed: " + std::string(label) + " does not match any declared union member type");
        }
    }

    if (!type.enumerationValues.empty()
        && std::find(type.enumerationValues.begin(), type.enumerationValues.end(), normalizedValue) == type.enumerationValues.end()) {
        throw XmlException("Schema validation failed: " + std::string(label) + " must be one of the declared enumeration values");
    }

    const bool binaryLengthFamily = type.baseType == "xs:hexBinary" || type.baseType == "xs:base64Binary";
    const std::size_t valueLength = normalizedValue.size();
    if (!listVariety && !binaryLengthFamily && type.length.has_value() && valueLength != *type.length) {
        throw XmlException("Schema validation failed: " + std::string(label) + " must have length " + std::to_string(*type.length));
    }
    if (!listVariety && !binaryLengthFamily && type.minLength.has_value() && valueLength < *type.minLength) {
        throw XmlException("Schema validation failed: " + std::string(label) + " must have length >= " + std::to_string(*type.minLength));
    }
    if (!listVariety && !binaryLengthFamily && type.maxLength.has_value() && valueLength > *type.maxLength) {
        throw XmlException("Schema validation failed: " + std::string(label) + " must have length <= " + std::to_string(*type.maxLength));
    }

    if (type.pattern.has_value()) {
        try {
            if (!std::regex_match(normalizedValue, std::regex(*type.pattern))) {
                throw XmlException("Schema validation failed: " + std::string(label) + " does not match the declared pattern");
            }
        } catch (const std::regex_error&) {
            throw XmlException("Schema validation failed: unsupported pattern facet '" + *type.pattern + "'");
        }
    }

    if (type.totalDigits.has_value() || type.fractionDigits.has_value()) {
        const auto digitInfo = extractDigitInfo(normalizedValue);
        if (type.totalDigits.has_value() && digitInfo.totalDigits > *type.totalDigits) {
            throw XmlException("Schema validation failed: " + std::string(label) + " must have at most " + std::to_string(*type.totalDigits) + " total digits");
        }
        if (type.fractionDigits.has_value() && digitInfo.fractionDigits > *type.fractionDigits) {
            throw XmlException("Schema validation failed: " + std::string(label) + " must have at most " + std::to_string(*type.fractionDigits) + " fraction digits");
        }
    }

    try {
        if (type.baseType == "xs:anySimpleType" || type.baseType == "xs:string" || type.baseType.empty()) {
            return;
        }
        if (type.baseType == "xs:normalizedString" || type.baseType == "xs:token") {
            return;
        }
        if (IsPracticalIntegerBuiltinType(type.baseType)) {
            const PracticalIntegerValue numericValue = ParsePracticalIntegerOrThrow(normalizedValue);
            if (!PracticalIntegerFitsBuiltinType(type.baseType, numericValue)) {
                throw XmlException("integer value out of range");
            }
            if (type.minInclusive.has_value()
                && ComparePracticalIntegerValues(numericValue, ParsePracticalIntegerOrThrow(*type.minInclusive)) < 0) {
                throw XmlException("Schema validation failed: " + std::string(label) + " must be >= " + *type.minInclusive);
            }
            if (type.maxInclusive.has_value()
                && ComparePracticalIntegerValues(numericValue, ParsePracticalIntegerOrThrow(*type.maxInclusive)) > 0) {
                throw XmlException("Schema validation failed: " + std::string(label) + " must be <= " + *type.maxInclusive);
            }
            if (type.minExclusive.has_value()
                && ComparePracticalIntegerValues(numericValue, ParsePracticalIntegerOrThrow(*type.minExclusive)) <= 0) {
                throw XmlException("Schema validation failed: " + std::string(label) + " must be > " + *type.minExclusive);
            }
            if (type.maxExclusive.has_value()
                && ComparePracticalIntegerValues(numericValue, ParsePracticalIntegerOrThrow(*type.maxExclusive)) >= 0) {
                throw XmlException("Schema validation failed: " + std::string(label) + " must be < " + *type.maxExclusive);
            }
            return;
        }
        if (type.baseType == "xs:boolean") {
            (void)XmlConvert::ToBoolean(normalizedValue);
            return;
        }
        if (type.baseType == "xs:Name") {
            (void)XmlConvert::VerifyName(normalizedValue);
            return;
        }
        if (type.baseType == "xs:NCName") {
            (void)XmlConvert::VerifyNCName(normalizedValue);
            return;
        }
        if (type.baseType == "xs:NMTOKEN") {
            (void)XmlConvert::VerifyNmToken(normalizedValue);
            return;
        }
        if (type.baseType == "xs:ID" || type.baseType == "xs:IDREF") {
            (void)XmlConvert::VerifyNCName(normalizedValue);
            return;
        }
        if (type.baseType == "xs:language") {
            if (!isPracticalLanguageValue(normalizedValue)) {
                throw XmlException("invalid language tag");
            }
            return;
        }
        if (type.baseType == "xs:anyURI") {
            if (!isPracticalAnyUriValue(normalizedValue)) {
                throw XmlException("invalid URI lexical form");
            }
            return;
        }
        if (type.baseType == "xs:duration") {
            const PracticalDurationValue durationValue = ParsePracticalDurationOrThrow(normalizedValue);
            if (type.minInclusive.has_value()
                && ComparePracticalDurationValues(durationValue, ParsePracticalDurationOrThrow(*type.minInclusive)) < 0) {
                throw XmlException("Schema validation failed: " + std::string(label) + " must be >= " + *type.minInclusive);
            }
            if (type.maxInclusive.has_value()
                && ComparePracticalDurationValues(durationValue, ParsePracticalDurationOrThrow(*type.maxInclusive)) > 0) {
                throw XmlException("Schema validation failed: " + std::string(label) + " must be <= " + *type.maxInclusive);
            }
            if (type.minExclusive.has_value()
                && ComparePracticalDurationValues(durationValue, ParsePracticalDurationOrThrow(*type.minExclusive)) <= 0) {
                throw XmlException("Schema validation failed: " + std::string(label) + " must be > " + *type.minExclusive);
            }
            if (type.maxExclusive.has_value()
                && ComparePracticalDurationValues(durationValue, ParsePracticalDurationOrThrow(*type.maxExclusive)) >= 0) {
                throw XmlException("Schema validation failed: " + std::string(label) + " must be < " + *type.maxExclusive);
            }
            return;
        }
        if (type.baseType == "xs:hexBinary" || type.baseType == "xs:base64Binary") {
            const std::size_t binaryValueLength = decodePracticalBinaryValue(type.baseType, normalizedValue).size();
            if (type.length.has_value() && binaryValueLength != *type.length) {
                throw XmlException("Schema validation failed: " + std::string(label) + " must have length " + std::to_string(*type.length));
            }
            if (type.minLength.has_value() && binaryValueLength < *type.minLength) {
                throw XmlException("Schema validation failed: " + std::string(label) + " must have length >= " + std::to_string(*type.minLength));
            }
            if (type.maxLength.has_value() && binaryValueLength > *type.maxLength) {
                throw XmlException("Schema validation failed: " + std::string(label) + " must have length <= " + std::to_string(*type.maxLength));
            }
            return;
        }
        if (type.baseType == "xs:date" || type.baseType == "xs:time" || type.baseType == "xs:dateTime"
            || type.baseType == "xs:gYear" || type.baseType == "xs:gYearMonth"
            || type.baseType == "xs:gMonth" || type.baseType == "xs:gDay" || type.baseType == "xs:gMonthDay") {
            const PracticalTemporalValue normalizedTemporalValue = normalizeTemporalValue(type.baseType, normalizedValue);
            if (type.minInclusive.has_value()) {
                const PracticalTemporalValue minInclusiveValue = normalizeTemporalValue(type.baseType, *type.minInclusive);
                if (compareTemporalValues(normalizedTemporalValue, minInclusiveValue) < 0) {
                    throw XmlException("Schema validation failed: " + std::string(label) + " must be >= " + *type.minInclusive);
                }
            }
            if (type.maxInclusive.has_value()) {
                const PracticalTemporalValue maxInclusiveValue = normalizeTemporalValue(type.baseType, *type.maxInclusive);
                if (compareTemporalValues(normalizedTemporalValue, maxInclusiveValue) > 0) {
                    throw XmlException("Schema validation failed: " + std::string(label) + " must be <= " + *type.maxInclusive);
                }
            }
            if (type.minExclusive.has_value()) {
                const PracticalTemporalValue minExclusiveValue = normalizeTemporalValue(type.baseType, *type.minExclusive);
                if (compareTemporalValues(normalizedTemporalValue, minExclusiveValue) <= 0) {
                    throw XmlException("Schema validation failed: " + std::string(label) + " must be > " + *type.minExclusive);
                }
            }
            if (type.maxExclusive.has_value()) {
                const PracticalTemporalValue maxExclusiveValue = normalizeTemporalValue(type.baseType, *type.maxExclusive);
                if (compareTemporalValues(normalizedTemporalValue, maxExclusiveValue) >= 0) {
                    throw XmlException("Schema validation failed: " + std::string(label) + " must be < " + *type.maxExclusive);
                }
            }
            return;
        }
        if (type.baseType == "xs:QName") {
            const auto [prefix, localName] = SplitQualifiedName(normalizedValue);
            if (prefix.empty()) {
                (void)XmlConvert::VerifyNCName(localName);
                return;
            }

            (void)XmlConvert::VerifyNCName(prefix);
            (void)XmlConvert::VerifyNCName(localName);
            const std::string namespaceUri = resolveNamespaceUri(prefix);
            if (namespaceUri.empty()) {
                throw XmlException("Schema validation failed: " + std::string(label) + " must reference a QName prefix declared in scope");
            }
            return;
        }
        if (type.baseType == "xs:NOTATION") {
            try {
                (void)XmlConvert::VerifyNCName(normalizedValue);
            } catch (const XmlException&) {
                throw XmlException("Schema validation failed: " + std::string(label) + " must be a valid " + type.baseType + " value");
            }

            if (!hasNotationDeclaration(normalizedValue)) {
                throw XmlException("Schema validation failed: " + std::string(label) + " must reference a declared xs:NOTATION value");
            }
            return;
        }
        if (type.baseType == "xs:ENTITY") {
            try {
                (void)XmlConvert::VerifyNCName(normalizedValue);
            } catch (const XmlException&) {
                throw XmlException("Schema validation failed: " + std::string(label) + " must be a valid " + type.baseType + " value");
            }

            if (!hasUnparsedEntityDeclaration(normalizedValue)) {
                throw XmlException("Schema validation failed: " + std::string(label) + " must reference a declared unparsed xs:ENTITY value");
            }
            return;
        }
        if (type.baseType == "xs:float") {
            const float numericValue = XmlConvert::ToSingle(normalizedValue);
            if (type.minInclusive.has_value() && numericValue < XmlConvert::ToSingle(*type.minInclusive)) {
                throw XmlException("Schema validation failed: " + std::string(label) + " must be >= " + *type.minInclusive);
            }
            if (type.maxInclusive.has_value() && numericValue > XmlConvert::ToSingle(*type.maxInclusive)) {
                throw XmlException("Schema validation failed: " + std::string(label) + " must be <= " + *type.maxInclusive);
            }
            if (type.minExclusive.has_value() && numericValue <= XmlConvert::ToSingle(*type.minExclusive)) {
                throw XmlException("Schema validation failed: " + std::string(label) + " must be > " + *type.minExclusive);
            }
            if (type.maxExclusive.has_value() && numericValue >= XmlConvert::ToSingle(*type.maxExclusive)) {
                throw XmlException("Schema validation failed: " + std::string(label) + " must be < " + *type.maxExclusive);
            }
            return;
        }
        if (type.baseType == "xs:decimal") {
            const PracticalDecimalValue numericValue = ParsePracticalDecimalOrThrow(normalizedValue);
            if (type.minInclusive.has_value()
                && ComparePracticalDecimalValues(numericValue, ParsePracticalDecimalOrThrow(*type.minInclusive)) < 0) {
                throw XmlException("Schema validation failed: " + std::string(label) + " must be >= " + *type.minInclusive);
            }
            if (type.maxInclusive.has_value()
                && ComparePracticalDecimalValues(numericValue, ParsePracticalDecimalOrThrow(*type.maxInclusive)) > 0) {
                throw XmlException("Schema validation failed: " + std::string(label) + " must be <= " + *type.maxInclusive);
            }
            if (type.minExclusive.has_value()
                && ComparePracticalDecimalValues(numericValue, ParsePracticalDecimalOrThrow(*type.minExclusive)) <= 0) {
                throw XmlException("Schema validation failed: " + std::string(label) + " must be > " + *type.minExclusive);
            }
            if (type.maxExclusive.has_value()
                && ComparePracticalDecimalValues(numericValue, ParsePracticalDecimalOrThrow(*type.maxExclusive)) >= 0) {
                throw XmlException("Schema validation failed: " + std::string(label) + " must be < " + *type.maxExclusive);
            }
            return;
        }
        if (type.baseType == "xs:double") {
            const double numericValue = XmlConvert::ToDouble(normalizedValue);
            if (type.minInclusive.has_value() && numericValue < XmlConvert::ToDouble(*type.minInclusive)) {
                throw XmlException("Schema validation failed: " + std::string(label) + " must be >= " + *type.minInclusive);
            }
            if (type.maxInclusive.has_value() && numericValue > XmlConvert::ToDouble(*type.maxInclusive)) {
                throw XmlException("Schema validation failed: " + std::string(label) + " must be <= " + *type.maxInclusive);
            }
            if (type.minExclusive.has_value() && numericValue <= XmlConvert::ToDouble(*type.minExclusive)) {
                throw XmlException("Schema validation failed: " + std::string(label) + " must be > " + *type.minExclusive);
            }
            if (type.maxExclusive.has_value() && numericValue >= XmlConvert::ToDouble(*type.maxExclusive)) {
                throw XmlException("Schema validation failed: " + std::string(label) + " must be < " + *type.maxExclusive);
            }
            return;
        }
    } catch (const XmlException&) {
        if (IsPracticalIntegerBuiltinType(type.baseType)) {
            try {
                const PracticalIntegerValue numericValue = ParsePracticalIntegerOrThrow(normalizedValue);
                if (!PracticalIntegerFitsBuiltinType(type.baseType, numericValue)) {
                    throw XmlException("integer value out of range");
                }
            } catch (const XmlException&) {
                throw XmlException("Schema validation failed: " + std::string(label) + " must be a valid " + type.baseType + " value");
            }
            throw;
        }
        if (type.baseType == "xs:float") {
            try {
                (void)XmlConvert::ToSingle(normalizedValue);
            } catch (const XmlException&) {
                throw XmlException("Schema validation failed: " + std::string(label) + " must be a valid " + type.baseType + " value");
            }
            throw;
        }
        if (type.baseType == "xs:decimal") {
            try {
                (void)ParsePracticalDecimalOrThrow(normalizedValue);
            } catch (const XmlException&) {
                throw XmlException("Schema validation failed: " + std::string(label) + " must be a valid " + type.baseType + " value");
            }
            throw;
        }
        if (type.baseType == "xs:double") {
            try {
                (void)XmlConvert::ToDouble(normalizedValue);
            } catch (const XmlException&) {
                throw XmlException("Schema validation failed: " + std::string(label) + " must be a valid " + type.baseType + " value");
            }
            throw;
        }
        if (type.baseType == "xs:boolean") {
            try {
                (void)XmlConvert::ToBoolean(normalizedValue);
            } catch (const XmlException&) {
                throw XmlException("Schema validation failed: " + std::string(label) + " must be a valid " + type.baseType + " value");
            }
            throw;
        }
        if (type.baseType == "xs:Name") {
            try {
                (void)XmlConvert::VerifyName(normalizedValue);
            } catch (const XmlException&) {
                throw XmlException("Schema validation failed: " + std::string(label) + " must be a valid " + type.baseType + " value");
            }
            throw;
        }
        if (type.baseType == "xs:NCName") {
            try {
                (void)XmlConvert::VerifyNCName(normalizedValue);
            } catch (const XmlException&) {
                throw XmlException("Schema validation failed: " + std::string(label) + " must be a valid " + type.baseType + " value");
            }
            throw;
        }
        if (type.baseType == "xs:NOTATION") {
            try {
                (void)XmlConvert::VerifyNCName(normalizedValue);
            } catch (const XmlException&) {
                throw XmlException("Schema validation failed: " + std::string(label) + " must be a valid " + type.baseType + " value");
            }
            throw;
        }
        if (type.baseType == "xs:NMTOKEN") {
            try {
                (void)XmlConvert::VerifyNmToken(normalizedValue);
            } catch (const XmlException&) {
                throw XmlException("Schema validation failed: " + std::string(label) + " must be a valid " + type.baseType + " value");
            }
            throw;
        }
        if (type.baseType == "xs:ID" || type.baseType == "xs:IDREF") {
            try {
                (void)XmlConvert::VerifyNCName(normalizedValue);
            } catch (const XmlException&) {
                throw XmlException("Schema validation failed: " + std::string(label) + " must be a valid " + type.baseType + " value");
            }
            throw;
        }
        if (type.baseType == "xs:language" || type.baseType == "xs:anyURI") {
            throw XmlException("Schema validation failed: " + std::string(label) + " must be a valid " + type.baseType + " value");
        }
        if (type.baseType == "xs:duration") {
            try {
                (void)ParsePracticalDurationOrThrow(normalizedValue);
            } catch (const XmlException&) {
                throw XmlException("Schema validation failed: " + std::string(label) + " must be a valid " + type.baseType + " value");
            }
            throw;
        }
        if (type.baseType == "xs:hexBinary" || type.baseType == "xs:base64Binary") {
            try {
                (void)decodePracticalBinaryValue(type.baseType, normalizedValue);
            } catch (const XmlException&) {
                throw XmlException("Schema validation failed: " + std::string(label) + " must be a valid " + type.baseType + " value");
            }
            throw;
        }
        if (type.baseType == "xs:date" || type.baseType == "xs:time" || type.baseType == "xs:dateTime"
            || type.baseType == "xs:gYear" || type.baseType == "xs:gYearMonth"
            || type.baseType == "xs:gMonth" || type.baseType == "xs:gDay" || type.baseType == "xs:gMonthDay") {
            try {
                (void)normalizeTemporalValue(type.baseType, normalizedValue);
            } catch (const XmlException&) {
                throw XmlException("Schema validation failed: " + std::string(label) + " must be a valid " + type.baseType + " value");
            }
            throw;
        }
        if (type.baseType == "xs:QName") {
            try {
                const auto [prefix, localName] = SplitQualifiedName(normalizedValue);
                if (prefix.empty()) {
                    (void)XmlConvert::VerifyNCName(localName);
                } else {
                    (void)XmlConvert::VerifyNCName(prefix);
                    (void)XmlConvert::VerifyNCName(localName);
                    const std::string namespaceUri = resolveNamespaceUri(prefix);
                    if (namespaceUri.empty()) {
                        throw XmlException("undefined QName prefix");
                    }
                }
            } catch (const XmlException&) {
                throw XmlException("Schema validation failed: " + std::string(label) + " must be a valid " + type.baseType + " value");
            }
            throw;
        }
        throw;
    }
    throw XmlException("Schema validation failed: unsupported simpleType base '" + type.baseType + "'");
}

void ValidateSchemaSimpleValue(
    const XmlSchemaSet::SimpleTypeRule& type,
    std::string_view value,
    std::string_view label,
    const XmlElement* contextElement) {
    ValidateSchemaSimpleValueWithQNameResolver(
        type,
        value,
        label,
        [&](const std::string& prefix) {
            return LookupNamespaceUriOnElement(contextElement, prefix);
        },
        [&](const std::string& name) {
            const XmlDocument* ownerDocument = contextElement == nullptr ? nullptr : contextElement->OwnerDocument();
            return HasDocumentNotationDeclaration(ownerDocument, name);
        },
        [&](const std::string& name) {
            const XmlDocument* ownerDocument = contextElement == nullptr ? nullptr : contextElement->OwnerDocument();
            return HasDocumentUnparsedEntityDeclaration(ownerDocument, name);
        });
}

void ValidateSchemaSimpleValue(
    const XmlSchemaSet::SimpleTypeRule& type,
    std::string_view value,
    std::string_view label,
    const std::vector<std::pair<std::string, std::string>>& namespaceBindings,
    const std::unordered_set<std::string>& notationDeclarationNames,
    const std::unordered_set<std::string>& unparsedEntityDeclarationNames) {
    ValidateSchemaSimpleValueWithQNameResolver(
        type,
        value,
        label,
        [&](const std::string& prefix) {
            return LookupNamespaceUriInBindings(namespaceBindings, prefix);
        },
        [&](const std::string& name) {
            return notationDeclarationNames.find(name) != notationDeclarationNames.end();
        },
        [&](const std::string& name) {
            return unparsedEntityDeclarationNames.find(name) != unparsedEntityDeclarationNames.end();
        });
}

struct PendingSchemaIdReference {
    std::string value;
    std::string label;
};

struct SchemaIdentityValidationState {
    std::unordered_set<std::string> declaredIds;
    std::vector<PendingSchemaIdReference> pendingIdReferences;
};

struct SchemaObservedChildElement {
    std::string name;
    std::string localName;
    std::string namespaceUri;
    const XmlElement* element = nullptr;
};

struct SchemaElementObservedContent {
    std::vector<SchemaObservedChildElement> elementChildren;
    bool hasSignificantText = false;
    std::string textValue;
};

struct SchemaObservedAttribute {
    std::string name;
    std::string localName;
    std::string prefix;
    std::string namespaceUri;
    std::string value;
};

struct SchemaIdentityNodeSnapshot {
    std::string name;
    std::string localName;
    std::string namespaceUri;
    const SchemaIdentityNodeSnapshot* parent = nullptr;
    std::vector<SchemaObservedAttribute> attributes;
    std::string textValue;
    std::vector<std::shared_ptr<SchemaIdentityNodeSnapshot>> children;
};

struct ReaderIdentitySelection {
    const SchemaIdentityNodeSnapshot* node = nullptr;
    const SchemaObservedAttribute* attribute = nullptr;
};

bool MatchesCompiledIdentityPredicate(
    const auto& step,
    const SchemaIdentityNodeSnapshot& snapshot) {
    if (!step.predicateAttributeValue.has_value()) {
        return true;
    }

    const auto attribute = std::find_if(snapshot.attributes.begin(), snapshot.attributes.end(), [&](const auto& candidate) {
        return candidate.localName == step.predicateAttributeLocalName
            && candidate.namespaceUri == step.predicateAttributeNamespaceUri
            && candidate.value == *step.predicateAttributeValue;
    });
    return attribute != snapshot.attributes.end();
}

void AppendDescendantIdentityMatches(
    const SchemaIdentityNodeSnapshot& snapshot,
    const auto& step,
    std::vector<const SchemaIdentityNodeSnapshot*>& matches) {
    for (const auto& child : snapshot.children) {
        if (child == nullptr) {
            continue;
        }
        if (MatchesCompiledIdentityStep(step, child->localName, child->namespaceUri)
            && MatchesCompiledIdentityPredicate(step, *child)) {
            matches.push_back(child.get());
        }
        AppendDescendantIdentityMatches(*child, step, matches);
    }
}

void AppendDescendantIdentityMatches(
    const SchemaIdentityNodeSnapshot& snapshot,
    const auto& step,
    std::vector<ReaderIdentitySelection>& matches) {
    for (const auto& child : snapshot.children) {
        if (child == nullptr) {
            continue;
        }
        if (MatchesCompiledIdentityStep(step, child->localName, child->namespaceUri)
            && MatchesCompiledIdentityPredicate(step, *child)) {
            matches.push_back(ReaderIdentitySelection{child.get(), nullptr});
        }
        AppendDescendantIdentityMatches(*child, step, matches);
    }
}

void AppendAncestorIdentityMatches(
    const SchemaIdentityNodeSnapshot& snapshot,
    const auto& step,
    bool includeSelf,
    std::vector<const SchemaIdentityNodeSnapshot*>& matches) {
    const SchemaIdentityNodeSnapshot* current = includeSelf ? std::addressof(snapshot) : snapshot.parent;
    while (current != nullptr) {
        if (MatchesCompiledIdentityStep(step, current->localName, current->namespaceUri)
            && MatchesCompiledIdentityPredicate(step, *current)) {
            matches.push_back(current);
        }
        current = current->parent;
    }
}

void AppendAncestorIdentityMatches(
    const SchemaIdentityNodeSnapshot& snapshot,
    const auto& step,
    bool includeSelf,
    std::vector<ReaderIdentitySelection>& matches) {
    const SchemaIdentityNodeSnapshot* current = includeSelf ? std::addressof(snapshot) : snapshot.parent;
    while (current != nullptr) {
        if (MatchesCompiledIdentityStep(step, current->localName, current->namespaceUri)
            && MatchesCompiledIdentityPredicate(step, *current)) {
            matches.push_back(ReaderIdentitySelection{current, nullptr});
        }
        current = current->parent;
    }
}

void AppendFollowingSiblingIdentityMatches(
    const SchemaIdentityNodeSnapshot& snapshot,
    const auto& step,
    std::vector<const SchemaIdentityNodeSnapshot*>& matches) {
    const auto* parent = snapshot.parent;
    if (parent == nullptr) {
        return;
    }

    bool foundSelf = false;
    for (const auto& sibling : parent->children) {
        if (!foundSelf) {
            foundSelf = sibling.get() == &snapshot;
            continue;
        }
        if (sibling != nullptr
            && MatchesCompiledIdentityStep(step, sibling->localName, sibling->namespaceUri)
            && MatchesCompiledIdentityPredicate(step, *sibling)) {
            matches.push_back(sibling.get());
        }
    }
}

void AppendFollowingSiblingIdentityMatches(
    const SchemaIdentityNodeSnapshot& snapshot,
    const auto& step,
    std::vector<ReaderIdentitySelection>& matches) {
    const auto* parent = snapshot.parent;
    if (parent == nullptr) {
        return;
    }

    bool foundSelf = false;
    for (const auto& sibling : parent->children) {
        if (!foundSelf) {
            foundSelf = sibling.get() == &snapshot;
            continue;
        }
        if (sibling != nullptr
            && MatchesCompiledIdentityStep(step, sibling->localName, sibling->namespaceUri)
            && MatchesCompiledIdentityPredicate(step, *sibling)) {
            matches.push_back(ReaderIdentitySelection{sibling.get(), nullptr});
        }
    }
}

void AppendPrecedingSiblingIdentityMatches(
    const SchemaIdentityNodeSnapshot& snapshot,
    const auto& step,
    std::vector<const SchemaIdentityNodeSnapshot*>& matches) {
    const auto* parent = snapshot.parent;
    if (parent == nullptr) {
        return;
    }

    std::vector<const SchemaIdentityNodeSnapshot*> preceding;
    for (const auto& sibling : parent->children) {
        if (sibling.get() == &snapshot) {
            break;
        }
        if (sibling != nullptr
            && MatchesCompiledIdentityStep(step, sibling->localName, sibling->namespaceUri)
            && MatchesCompiledIdentityPredicate(step, *sibling)) {
            preceding.push_back(sibling.get());
        }
    }

    for (auto index = preceding.size(); index > 0; --index) {
        matches.push_back(preceding[index - 1]);
    }
}

void AppendPrecedingSiblingIdentityMatches(
    const SchemaIdentityNodeSnapshot& snapshot,
    const auto& step,
    std::vector<ReaderIdentitySelection>& matches) {
    const auto* parent = snapshot.parent;
    if (parent == nullptr) {
        return;
    }

    std::vector<const SchemaIdentityNodeSnapshot*> preceding;
    for (const auto& sibling : parent->children) {
        if (sibling.get() == &snapshot) {
            break;
        }
        if (sibling != nullptr
            && MatchesCompiledIdentityStep(step, sibling->localName, sibling->namespaceUri)
            && MatchesCompiledIdentityPredicate(step, *sibling)) {
            preceding.push_back(sibling.get());
        }
    }

    for (auto index = preceding.size(); index > 0; --index) {
        matches.push_back(ReaderIdentitySelection{preceding[index - 1], nullptr});
    }
}

void CollectSnapshotIdentityNodesInDocumentOrder(
    const SchemaIdentityNodeSnapshot& snapshot,
    std::vector<const SchemaIdentityNodeSnapshot*>& nodes) {
    nodes.push_back(&snapshot);
    for (const auto& child : snapshot.children) {
        if (child == nullptr) {
            continue;
        }
        CollectSnapshotIdentityNodesInDocumentOrder(*child, nodes);
    }
}

void AppendFollowingIdentityMatches(
    const SchemaIdentityNodeSnapshot& snapshot,
    const auto& step,
    std::vector<const SchemaIdentityNodeSnapshot*>& matches) {
    const SchemaIdentityNodeSnapshot* current = &snapshot;
    while (current != nullptr) {
        const auto* parent = current->parent;
        if (parent == nullptr) {
            break;
        }

        bool foundSelf = false;
        for (const auto& sibling : parent->children) {
            if (!foundSelf) {
                foundSelf = sibling.get() == current;
                continue;
            }
            if (sibling != nullptr
                && MatchesCompiledIdentityStep(step, sibling->localName, sibling->namespaceUri)
                && MatchesCompiledIdentityPredicate(step, *sibling)) {
                matches.push_back(sibling.get());
            }
            if (sibling != nullptr) {
                AppendDescendantIdentityMatches(*sibling, step, matches);
            }
        }
        current = parent;
    }
}

void AppendFollowingIdentityMatches(
    const SchemaIdentityNodeSnapshot& snapshot,
    const auto& step,
    std::vector<ReaderIdentitySelection>& matches) {
    const SchemaIdentityNodeSnapshot* current = &snapshot;
    while (current != nullptr) {
        const auto* parent = current->parent;
        if (parent == nullptr) {
            break;
        }

        bool foundSelf = false;
        for (const auto& sibling : parent->children) {
            if (!foundSelf) {
                foundSelf = sibling.get() == current;
                continue;
            }
            if (sibling != nullptr
                && MatchesCompiledIdentityStep(step, sibling->localName, sibling->namespaceUri)
                && MatchesCompiledIdentityPredicate(step, *sibling)) {
                matches.push_back(ReaderIdentitySelection{sibling.get(), nullptr});
            }
            if (sibling != nullptr) {
                AppendDescendantIdentityMatches(*sibling, step, matches);
            }
        }
        current = parent;
    }
}

void AppendPrecedingIdentityMatches(
    const SchemaIdentityNodeSnapshot& snapshot,
    const auto& step,
    std::vector<const SchemaIdentityNodeSnapshot*>& matches) {
    std::unordered_set<const SchemaIdentityNodeSnapshot*> ancestors;
    for (const auto* ancestor = snapshot.parent; ancestor != nullptr; ancestor = ancestor->parent) {
        ancestors.insert(ancestor);
    }

    const SchemaIdentityNodeSnapshot* root = &snapshot;
    while (root->parent != nullptr) {
        root = root->parent;
    }

    std::vector<const SchemaIdentityNodeSnapshot*> allNodes;
    CollectSnapshotIdentityNodesInDocumentOrder(*root, allNodes);

    std::vector<const SchemaIdentityNodeSnapshot*> preceding;
    for (const auto* candidate : allNodes) {
        if (candidate == &snapshot) {
            break;
        }
        if (ancestors.find(candidate) == ancestors.end()
            && MatchesCompiledIdentityStep(step, candidate->localName, candidate->namespaceUri)
            && MatchesCompiledIdentityPredicate(step, *candidate)) {
            preceding.push_back(candidate);
        }
    }

    for (auto index = preceding.size(); index > 0; --index) {
        matches.push_back(preceding[index - 1]);
    }
}

void AppendPrecedingIdentityMatches(
    const SchemaIdentityNodeSnapshot& snapshot,
    const auto& step,
    std::vector<ReaderIdentitySelection>& matches) {
    std::unordered_set<const SchemaIdentityNodeSnapshot*> ancestors;
    for (const auto* ancestor = snapshot.parent; ancestor != nullptr; ancestor = ancestor->parent) {
        ancestors.insert(ancestor);
    }

    const SchemaIdentityNodeSnapshot* root = &snapshot;
    while (root->parent != nullptr) {
        root = root->parent;
    }

    std::vector<const SchemaIdentityNodeSnapshot*> allNodes;
    CollectSnapshotIdentityNodesInDocumentOrder(*root, allNodes);

    std::vector<const SchemaIdentityNodeSnapshot*> preceding;
    for (const auto* candidate : allNodes) {
        if (candidate == &snapshot) {
            break;
        }
        if (ancestors.find(candidate) == ancestors.end()
            && MatchesCompiledIdentityStep(step, candidate->localName, candidate->namespaceUri)
            && MatchesCompiledIdentityPredicate(step, *candidate)) {
            preceding.push_back(candidate);
        }
    }

    for (auto index = preceding.size(); index > 0; --index) {
        matches.push_back(ReaderIdentitySelection{preceding[index - 1], nullptr});
    }
}

struct SchemaValidationFrame {
    std::string name;
    std::string localName;
    std::string namespaceUri;
    int depth = 0;
    bool isEmptyElement = false;
    const XmlElement* element = nullptr;
    const XmlDocument* ownerDocument = nullptr;
    std::vector<std::pair<std::string, std::string>> namespaceBindings;
    std::vector<SchemaObservedAttribute> observedAttributes;
    SchemaElementObservedContent observedContent;
    std::shared_ptr<SchemaIdentityNodeSnapshot> identitySnapshot;
};

std::vector<SchemaObservedAttribute> ObserveSchemaElementAttributes(const XmlElement& element) {
    std::vector<SchemaObservedAttribute> observed;
    observed.reserve(element.Attributes().size());
    for (const auto& attribute : element.Attributes()) {
        if (attribute == nullptr) {
            continue;
        }
        observed.push_back(SchemaObservedAttribute{
            attribute->Name(),
            attribute->LocalName(),
            attribute->Prefix(),
            attribute->NamespaceURI(),
            attribute->Value()});
    }
    return observed;
}

const SchemaObservedAttribute* FindObservedSchemaAttribute(
    const std::vector<SchemaObservedAttribute>& observedAttributes,
    const std::string& localName,
    const std::string& namespaceUri) {
    const auto found = std::find_if(observedAttributes.begin(), observedAttributes.end(), [&](const auto& attribute) {
        return attribute.localName == localName && attribute.namespaceUri == namespaceUri;
    });
    return found == observedAttributes.end() ? nullptr : std::addressof(*found);
}

std::string LookupNamespaceUriOnSchemaFrame(const SchemaValidationFrame& frame, const std::string& prefix) {
    if (frame.namespaceBindings.empty() && frame.element != nullptr) {
        return LookupNamespaceUriOnElement(frame.element, prefix);
    }

    for (auto it = frame.namespaceBindings.rbegin(); it != frame.namespaceBindings.rend(); ++it) {
        if (it->first == prefix) {
            return it->second;
        }
    }
    return {};
}

std::vector<SchemaObservedAttribute> ObserveSchemaReaderAttributes(XmlReader& reader) {
    std::vector<SchemaObservedAttribute> observed;
    if (reader.NodeType() != XmlNodeType::Element || !reader.HasAttributes()) {
        return observed;
    }

    if (reader.MoveToFirstAttribute()) {
        do {
            observed.push_back(SchemaObservedAttribute{
                reader.Name(),
                reader.LocalName(),
                reader.Prefix(),
                reader.NamespaceURI(),
                reader.Value()});
        } while (reader.MoveToNextAttribute());
        reader.MoveToElement();
    }

    return observed;
}

std::vector<std::pair<std::string, std::string>> ExtendSchemaNamespaceBindings(
    const std::vector<std::pair<std::string, std::string>>& parentBindings,
    const std::vector<SchemaObservedAttribute>& observedAttributes) {
    std::vector<std::pair<std::string, std::string>> bindings = parentBindings;
    if (bindings.empty()) {
        bindings.push_back({"xml", "http://www.w3.org/XML/1998/namespace"});
        bindings.push_back({"xmlns", "http://www.w3.org/2000/xmlns/"});
    }

    for (const auto& attribute : observedAttributes) {
        if (attribute.name == "xmlns") {
            bindings.push_back({std::string{}, attribute.value});
            continue;
        }
        if (attribute.prefix == "xmlns") {
            bindings.push_back({attribute.localName, attribute.value});
        }
    }

    return bindings;
}

SchemaValidationFrame BuildSchemaValidationFrame(
    XmlReader& reader,
    const std::vector<std::pair<std::string, std::string>>& parentBindings = {},
    const bool captureIdentitySnapshot = false) {
    SchemaValidationFrame frame;
    frame.name = reader.Name();
    frame.localName = reader.LocalName();
    frame.namespaceUri = reader.NamespaceURI();
    frame.depth = reader.Depth();
    frame.isEmptyElement = reader.IsEmptyElement();
    frame.observedAttributes = ObserveSchemaReaderAttributes(reader);
    frame.namespaceBindings = ExtendSchemaNamespaceBindings(parentBindings, frame.observedAttributes);
    if (captureIdentitySnapshot) {
        frame.identitySnapshot = std::make_shared<SchemaIdentityNodeSnapshot>();
        frame.identitySnapshot->name = frame.name;
        frame.identitySnapshot->localName = frame.localName;
        frame.identitySnapshot->namespaceUri = frame.namespaceUri;
        frame.identitySnapshot->attributes = frame.observedAttributes;
    }
    return frame;
}

void AppendObservedSchemaChildFrame(SchemaValidationFrame& parentFrame, const SchemaValidationFrame& childFrame) {
    parentFrame.observedContent.elementChildren.push_back(SchemaObservedChildElement{
        childFrame.name,
        childFrame.localName,
        childFrame.namespaceUri,
        childFrame.element});
    if (parentFrame.identitySnapshot != nullptr && childFrame.identitySnapshot != nullptr) {
        childFrame.identitySnapshot->parent = parentFrame.identitySnapshot.get();
        parentFrame.identitySnapshot->children.push_back(childFrame.identitySnapshot);
    }
}

void AppendSchemaTextToFrame(SchemaValidationFrame& frame, const std::string& text) {
    if (IsWhitespaceOnly(text)) {
        return;
    }
    frame.observedContent.hasSignificantText = true;
    frame.observedContent.textValue += text;
    if (frame.identitySnapshot != nullptr) {
        frame.identitySnapshot->textValue += text;
    }
}

template <typename AttributeRuleT>
std::optional<std::string> ResolveSchemaAttributeEffectiveValue(
    const AttributeRuleT& attributeRule,
    const XmlElement& element,
    const std::vector<SchemaObservedAttribute>& observedAttributes) {
    const auto* observedAttribute = FindObservedSchemaAttribute(observedAttributes, attributeRule.name, attributeRule.namespaceUri);
    const bool present = observedAttribute != nullptr;
    if (attributeRule.required && !present) {
        throw XmlException(
            "Schema validation failed: required attribute '" + attributeRule.name
            + "' is missing on element '" + element.Name() + "'");
    }

    if (present) {
        if (attributeRule.fixedValue.has_value() && observedAttribute->value != *attributeRule.fixedValue) {
            throw XmlException(
                "Schema validation failed: attribute '" + attributeRule.name
                + "' on element '" + element.Name() + "' must match the fixed value '" + *attributeRule.fixedValue + "'");
        }
        return observedAttribute->value;
    }

    if (!attributeRule.required) {
        if (attributeRule.fixedValue.has_value()) {
            return *attributeRule.fixedValue;
        }
        if (attributeRule.defaultValue.has_value()) {
            return *attributeRule.defaultValue;
        }
    }

    return std::nullopt;
}

template <typename AttributeRuleT>
std::optional<std::string> ResolveSchemaAttributeEffectiveValue(
    const AttributeRuleT& attributeRule,
    const std::string& elementName,
    const std::vector<SchemaObservedAttribute>& observedAttributes) {
    const auto* observedAttribute = FindObservedSchemaAttribute(observedAttributes, attributeRule.name, attributeRule.namespaceUri);
    const bool present = observedAttribute != nullptr;
    if (attributeRule.required && !present) {
        throw XmlException(
            "Schema validation failed: required attribute '" + attributeRule.name
            + "' is missing on element '" + elementName + "'");
    }

    if (present) {
        if (attributeRule.fixedValue.has_value() && observedAttribute->value != *attributeRule.fixedValue) {
            throw XmlException(
                "Schema validation failed: attribute '" + attributeRule.name
                + "' on element '" + elementName + "' must match the fixed value '" + *attributeRule.fixedValue + "'");
        }
        return observedAttribute->value;
    }

    if (!attributeRule.required) {
        if (attributeRule.fixedValue.has_value()) {
            return *attributeRule.fixedValue;
        }
        if (attributeRule.defaultValue.has_value()) {
            return *attributeRule.defaultValue;
        }
    }

    return std::nullopt;
}

SchemaElementObservedContent ObserveSchemaElementContent(const XmlElement& element) {
    SchemaElementObservedContent observed;
    for (const auto& child : element.ChildNodes()) {
        if (child == nullptr) {
            continue;
        }
        if (child->NodeType() == XmlNodeType::Element) {
            const auto* childElement = static_cast<const XmlElement*>(child.get());
            observed.elementChildren.push_back(SchemaObservedChildElement{
                childElement->Name(),
                childElement->LocalName(),
                childElement->NamespaceURI(),
                childElement});
            continue;
        }
        if ((child->NodeType() == XmlNodeType::Text || child->NodeType() == XmlNodeType::CDATA)
            && !IsWhitespaceOnly(child->Value())) {
            observed.hasSignificantText = true;
            observed.textValue += child->Value();
        }
    }
    return observed;
}

SchemaValidationFrame BuildSchemaValidationFrame(const XmlElement& element) {
    SchemaValidationFrame frame;
    frame.name = element.Name();
    frame.localName = element.LocalName();
    frame.namespaceUri = element.NamespaceURI();
    frame.isEmptyElement = !element.HasChildNodes() && !element.WritesFullEndElement();
    frame.element = std::addressof(element);
    frame.ownerDocument = element.OwnerDocument();
    frame.observedAttributes = ObserveSchemaElementAttributes(element);
    frame.observedContent = ObserveSchemaElementContent(element);
    return frame;
}

template <typename ElementRuleT>
std::optional<std::string> ResolveSchemaElementEffectiveValue(
    const ElementRuleT& rule,
    const XmlElement& element,
    const SchemaElementObservedContent& observed,
    const bool isNilled) {
    if (isNilled || !observed.elementChildren.empty()) {
        return std::nullopt;
    }

    if (observed.hasSignificantText) {
        if (rule.fixedValue.has_value() && observed.textValue != *rule.fixedValue) {
            throw XmlException(
                "Schema validation failed: element '" + element.Name() + "' must match the fixed value '" + *rule.fixedValue + "'");
        }
        return observed.textValue;
    }

    if (rule.fixedValue.has_value()) {
        return *rule.fixedValue;
    }
    if (rule.defaultValue.has_value()) {
        return *rule.defaultValue;
    }
    return std::nullopt;
}

template <typename ElementRuleT>
std::optional<std::string> ResolveSchemaElementEffectiveValue(
    const ElementRuleT& rule,
    const std::string& elementName,
    const SchemaElementObservedContent& observed,
    const bool isNilled) {
    if (isNilled || !observed.elementChildren.empty()) {
        return std::nullopt;
    }

    if (observed.hasSignificantText) {
        if (rule.fixedValue.has_value() && observed.textValue != *rule.fixedValue) {
            throw XmlException(
                "Schema validation failed: element '" + elementName + "' must match the fixed value '" + *rule.fixedValue + "'");
        }
        return observed.textValue;
    }

    if (rule.fixedValue.has_value()) {
        return *rule.fixedValue;
    }
    if (rule.defaultValue.has_value()) {
        return *rule.defaultValue;
    }
    return std::nullopt;
}

template <typename ParticleT, typename ChildT, typename FindDeclaredRuleFn, typename ElementMatchesFn,
    typename WildcardMatchesFn, typename WildcardAllowsUndeclaredFn, typename IsElementParticleFn,
    typename IsAnyParticleFn, typename IsSequenceParticleFn, typename IsAllParticleFn,
    typename GetParticleChildrenFn, typename GetParticleNameFn, typename GetParticleNamespaceFn,
    typename GetParticleProcessContentsFn, typename GetParticleMinOccursFn, typename GetParticleMaxOccursFn,
    typename GetChildLocalNameFn, typename GetChildNamespaceFn>
std::vector<std::size_t> CollectObservedParticleMatches(
    const ParticleT& particle,
    const std::vector<ChildT>& elementChildren,
    FindDeclaredRuleFn&& findDeclaredRule,
    ElementMatchesFn&& elementMatchesDeclaration,
    WildcardMatchesFn&& matchesWildcardNamespace,
    WildcardAllowsUndeclaredFn&& wildcardAllowsUndeclared,
    IsElementParticleFn&& isElementParticle,
    IsAnyParticleFn&& isAnyParticle,
    IsSequenceParticleFn&& isSequenceParticle,
    IsAllParticleFn&& isAllParticle,
    GetParticleChildrenFn&& getParticleChildren,
    GetParticleNameFn&& getParticleName,
    GetParticleNamespaceFn&& getParticleNamespace,
    GetParticleProcessContentsFn&& getParticleProcessContents,
    GetParticleMinOccursFn&& getParticleMinOccurs,
    GetParticleMaxOccursFn&& getParticleMaxOccurs,
    GetChildLocalNameFn&& getChildLocalName,
    GetChildNamespaceFn&& getChildNamespace) {
    const auto dedupeIndices = [](std::vector<std::size_t>& indices) {
        std::sort(indices.begin(), indices.end());
        indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
    };

    std::function<std::vector<std::size_t>(const ParticleT&, std::size_t)> collectParticleMatches;
    std::function<std::vector<std::size_t>(const ParticleT&, std::size_t)> collectSingleOccurrenceMatches;

    collectSingleOccurrenceMatches = [&](const ParticleT& currentParticle, std::size_t startIndex) -> std::vector<std::size_t> {
        if (isElementParticle(currentParticle)) {
            const auto* declaredRule = findDeclaredRule(getParticleName(currentParticle), getParticleNamespace(currentParticle));
            if (declaredRule != nullptr
                && startIndex < elementChildren.size()
                && elementMatchesDeclaration(elementChildren[startIndex], *declaredRule)) {
                return {startIndex + 1};
            }
            return {};
        }

        if (isAnyParticle(currentParticle)) {
            if (startIndex < elementChildren.size()
                && matchesWildcardNamespace(getParticleNamespace(currentParticle), getChildNamespace(elementChildren[startIndex]))) {
                const auto* childRule = findDeclaredRule(
                    getChildLocalName(elementChildren[startIndex]), getChildNamespace(elementChildren[startIndex]));
                if (childRule == nullptr && !wildcardAllowsUndeclared(getParticleProcessContents(currentParticle))) {
                    return {};
                }
                return {startIndex + 1};
            }
            return {};
        }

        if (isSequenceParticle(currentParticle)) {
            std::vector<std::size_t> indices{startIndex};
            for (const auto& childParticle : getParticleChildren(currentParticle)) {
                std::vector<std::size_t> nextIndices;
                for (const auto index : indices) {
                    auto childMatches = collectParticleMatches(childParticle, index);
                    nextIndices.insert(nextIndices.end(), childMatches.begin(), childMatches.end());
                }
                dedupeIndices(nextIndices);
                indices = std::move(nextIndices);
                if (indices.empty()) {
                    break;
                }
            }
            return indices;
        }

        if (isAllParticle(currentParticle)) {
            std::vector<std::size_t> indices;
            std::function<void(std::size_t, std::vector<bool>&)> collectAllMatches;

            collectAllMatches = [&](std::size_t currentIndex, std::vector<bool>& usedChildren) {
                bool canStop = true;
                for (std::size_t childIndex = 0; childIndex < getParticleChildren(currentParticle).size(); ++childIndex) {
                    if (!usedChildren[childIndex] && getParticleMinOccurs(getParticleChildren(currentParticle)[childIndex]) > 0) {
                        canStop = false;
                        break;
                    }
                }
                if (canStop) {
                    indices.push_back(currentIndex);
                }

                for (std::size_t childIndex = 0; childIndex < getParticleChildren(currentParticle).size(); ++childIndex) {
                    if (usedChildren[childIndex]) {
                        continue;
                    }

                    auto childMatches = collectSingleOccurrenceMatches(getParticleChildren(currentParticle)[childIndex], currentIndex);
                    for (const auto matchIndex : childMatches) {
                        if (matchIndex == currentIndex) {
                            continue;
                        }

                        std::vector<bool> nextUsed = usedChildren;
                        nextUsed[childIndex] = true;
                        collectAllMatches(matchIndex, nextUsed);
                    }
                }
            };

            std::vector<bool> usedChildren(getParticleChildren(currentParticle).size(), false);
            collectAllMatches(startIndex, usedChildren);
            dedupeIndices(indices);
            return indices;
        }

        std::vector<std::size_t> indices;
        for (const auto& childParticle : getParticleChildren(currentParticle)) {
            auto childMatches = collectParticleMatches(childParticle, startIndex);
            indices.insert(indices.end(), childMatches.begin(), childMatches.end());
        }
        dedupeIndices(indices);
        return indices;
    };

    collectParticleMatches = [&](const ParticleT& currentParticle, std::size_t startIndex) -> std::vector<std::size_t> {
        std::vector<std::size_t> matches;
        if (getParticleMinOccurs(currentParticle) == 0) {
            matches.push_back(startIndex);
        }

        std::vector<std::size_t> currentIndices{startIndex};
        const std::size_t maxRepeats = getParticleMaxOccurs(currentParticle) == std::numeric_limits<std::size_t>::max()
            ? elementChildren.size() - startIndex + 1
            : getParticleMaxOccurs(currentParticle);

        for (std::size_t repeat = 1; repeat <= maxRepeats; ++repeat) {
            std::vector<std::size_t> nextIndices;
            bool progressed = false;
            for (const auto index : currentIndices) {
                auto occurrenceMatches = collectSingleOccurrenceMatches(currentParticle, index);
                for (const auto matchIndex : occurrenceMatches) {
                    nextIndices.push_back(matchIndex);
                    if (matchIndex != index) {
                        progressed = true;
                    }
                }
            }
            dedupeIndices(nextIndices);
            if (nextIndices.empty()) {
                break;
            }
            if (repeat >= getParticleMinOccurs(currentParticle)) {
                matches.insert(matches.end(), nextIndices.begin(), nextIndices.end());
            }
            if (!progressed) {
                break;
            }
            currentIndices = std::move(nextIndices);
        }

        dedupeIndices(matches);
        return matches;
    };

    return collectParticleMatches(particle, 0);
}

void ValidatePendingSchemaIdReferences(const SchemaIdentityValidationState& state) {
    for (const auto& reference : state.pendingIdReferences) {
        if (state.declaredIds.find(reference.value) == state.declaredIds.end()) {
            throw XmlException(
                "Schema validation failed: " + reference.label + " references unknown xs:ID value '" + reference.value + "'");
        }
    }
}

void XmlDocument::Validate(const XmlSchemaSet& schemas) const {
    static const std::string kSchemaInstanceNamespace = "http://www.w3.org/2001/XMLSchema-instance";
    SchemaIdentityValidationState identityState;

    std::function<void(const XmlSchemaSet::SimpleTypeRule&, const std::string&, const std::string&, const XmlElement*)> recordIdentityTypedValue;
    recordIdentityTypedValue = [&](const XmlSchemaSet::SimpleTypeRule& type, const std::string& value, const std::string& label, const XmlElement* contextElement) {
        if (type.variety == XmlSchemaSet::SimpleTypeRule::Variety::List) {
            if (type.itemType == nullptr) {
                return;
            }

            std::istringstream stream(NormalizeSchemaSimpleTypeValue(type, value));
            std::string item;
            std::size_t index = 0;
            while (stream >> item) {
                ++index;
                recordIdentityTypedValue(*type.itemType, item, label + " list item " + std::to_string(index), contextElement);
            }
            return;
        }

        if (type.variety == XmlSchemaSet::SimpleTypeRule::Variety::Union) {
            for (const auto& memberType : type.memberTypes) {
                try {
                    ValidateSchemaSimpleValue(memberType, value, label, contextElement);
                    recordIdentityTypedValue(memberType, value, label, contextElement);
                    return;
                } catch (const XmlException&) {
                }
            }
            return;
        }

        const std::string normalizedValue = NormalizeSchemaSimpleTypeValue(type, value);
        if (type.baseType == "xs:ID") {
            if (!identityState.declaredIds.insert(normalizedValue).second) {
                throw XmlException("Schema validation failed: " + label + " duplicates xs:ID value '" + normalizedValue + "'");
            }
            return;
        }
        if (type.baseType == "xs:IDREF") {
            identityState.pendingIdReferences.push_back(PendingSchemaIdReference{normalizedValue, label});
        }
    };

    const auto root = DocumentElement();
    if (root == nullptr) {
        throw XmlException("Cannot validate an XML document without a root element");
    }

    const auto runtimeSimpleTypeDerivesFrom = [&](const XmlSchemaSet::SimpleTypeRule& derivedType,
        const XmlSchemaSet::SimpleTypeRule& baseType,
        bool& usesRestriction) {
        if (!derivedType.namedTypeName.empty()
            && derivedType.namedTypeName == baseType.namedTypeName
            && derivedType.namedTypeNamespaceUri == baseType.namedTypeNamespaceUri) {
            return true;
        }
        if (derivedType.namedTypeName.empty()
            && baseType.namedTypeName.empty()
            && derivedType.derivationMethod == XmlSchemaSet::SimpleTypeRule::DerivationMethod::None
            && baseType.derivationMethod == XmlSchemaSet::SimpleTypeRule::DerivationMethod::None
            && derivedType.variety == baseType.variety) {
            bool builtinUsesRestriction = false;
            if (BuiltinSimpleTypeDerivesFrom(derivedType.baseType, baseType.baseType, builtinUsesRestriction)) {
                usesRestriction = usesRestriction || builtinUsesRestriction;
                return true;
            }
        }

        const XmlSchemaSet::SimpleTypeRule* current = &derivedType;
        std::unordered_set<std::string> visited;
        while (true) {
            if (current->derivationMethod == XmlSchemaSet::SimpleTypeRule::DerivationMethod::Restriction) {
                usesRestriction = true;
            }
            if (!baseType.namedTypeName.empty()
                && current->derivationBaseName == baseType.namedTypeName
                && current->derivationBaseNamespaceUri == baseType.namedTypeNamespaceUri) {
                return true;
            }
            if (baseType.namedTypeName.empty()
                && current->derivationMethod == XmlSchemaSet::SimpleTypeRule::DerivationMethod::Restriction) {
                bool builtinUsesRestriction = false;
                if (BuiltinSimpleTypeDerivesFrom(current->baseType, baseType.baseType, builtinUsesRestriction)) {
                    usesRestriction = usesRestriction || builtinUsesRestriction;
                    return true;
                }
            }
            if (current->derivationBaseName.empty()) {
                return false;
            }

            const std::string currentKey = current->derivationBaseNamespaceUri + "\n" + current->derivationBaseName;
            if (!visited.insert(currentKey).second) {
                return false;
            }

            current = schemas.FindSimpleTypeRule(current->derivationBaseName, current->derivationBaseNamespaceUri);
            if (current == nullptr) {
                return false;
            }
            if (!baseType.namedTypeName.empty()
                && current->namedTypeName == baseType.namedTypeName
                && current->namedTypeNamespaceUri == baseType.namedTypeNamespaceUri) {
                return true;
            }
        }
    };

    const auto runtimeComplexTypeDerivesFrom = [&](const XmlSchemaSet::ComplexTypeRule& derivedType,
        const XmlSchemaSet::ComplexTypeRule& baseType,
        bool& usesRestriction,
        bool& usesExtension) {
        if (baseType.namedTypeName == "anyType"
            && baseType.namedTypeNamespaceUri == "http://www.w3.org/2001/XMLSchema") {
            return true;
        }
        if (!derivedType.namedTypeName.empty()
            && derivedType.namedTypeName == baseType.namedTypeName
            && derivedType.namedTypeNamespaceUri == baseType.namedTypeNamespaceUri) {
            return true;
        }

        const XmlSchemaSet::ComplexTypeRule* current = &derivedType;
        std::unordered_set<std::string> visited;
        while (true) {
            if (current->derivationMethod == XmlSchemaSet::ComplexTypeRule::DerivationMethod::Restriction) {
                usesRestriction = true;
            } else if (current->derivationMethod == XmlSchemaSet::ComplexTypeRule::DerivationMethod::Extension) {
                usesExtension = true;
            }
            if (current->derivationBaseName == baseType.namedTypeName
                && current->derivationBaseNamespaceUri == baseType.namedTypeNamespaceUri) {
                return true;
            }
            if (current->derivationBaseName.empty()) {
                return false;
            }

            const std::string currentKey = current->derivationBaseNamespaceUri + "\n" + current->derivationBaseName;
            if (!visited.insert(currentKey).second) {
                return false;
            }

            current = schemas.FindComplexTypeRule(current->derivationBaseName, current->derivationBaseNamespaceUri);
            if (current == nullptr) {
                return false;
            }
            if (!baseType.namedTypeName.empty()
                && current->namedTypeName == baseType.namedTypeName
                && current->namedTypeNamespaceUri == baseType.namedTypeNamespaceUri) {
                return true;
            }
        }
    };

    const auto runtimeElementCanSubstituteForHead = [&](const XmlSchemaSet::ElementRule& actualRule,
        const XmlSchemaSet::ElementRule& declaration) {
        bool usesRestriction = false;
        bool usesExtension = false;
        if (declaration.declaredComplexType.has_value()) {
            if (!actualRule.declaredComplexType.has_value()) {
                return false;
            }
            if (!runtimeComplexTypeDerivesFrom(*actualRule.declaredComplexType, *declaration.declaredComplexType, usesRestriction, usesExtension)) {
                return false;
            }
        } else if (declaration.declaredSimpleType.has_value()) {
            if (!actualRule.declaredSimpleType.has_value()) {
                return false;
            }
            if (!runtimeSimpleTypeDerivesFrom(*actualRule.declaredSimpleType, *declaration.declaredSimpleType, usesRestriction)) {
                return false;
            }
        }

        if ((usesRestriction && declaration.blockRestriction)
            || (usesExtension && declaration.blockExtension)) {
            return false;
        }
        return true;
    };

    const auto elementMatchesDeclaration = [&](const SchemaObservedChildElement& element, const XmlSchemaSet::ElementRule& declaration) {
        const auto* actualRule = schemas.FindElementRule(element.localName, element.namespaceUri);
        if (actualRule == nullptr) {
            return false;
        }
        if (actualRule->name == declaration.name && actualRule->namespaceUri == declaration.namespaceUri) {
            return true;
        }
        if (declaration.blockSubstitution) {
            return false;
        }

        std::string currentHeadName = actualRule->substitutionGroupHeadName;
        std::string currentHeadNamespaceUri = actualRule->substitutionGroupHeadNamespaceUri;
        std::unordered_set<std::string> visitedHeads;
        while (!currentHeadName.empty()) {
            const std::string headKey = currentHeadNamespaceUri + "\n" + currentHeadName;
            if (!visitedHeads.insert(headKey).second) {
                break;
            }
            if (currentHeadName == declaration.name && currentHeadNamespaceUri == declaration.namespaceUri) {
                return runtimeElementCanSubstituteForHead(*actualRule, declaration);
            }
            const auto* headRule = schemas.FindElementRule(currentHeadName, currentHeadNamespaceUri);
            if (headRule == nullptr) {
                break;
            }
            currentHeadName = headRule->substitutionGroupHeadName;
            currentHeadNamespaceUri = headRule->substitutionGroupHeadNamespaceUri;
        }
        return false;
    };

    const auto* rootRule = schemas.FindElementRule(root->LocalName(), root->NamespaceURI());
    if (rootRule == nullptr) {
        throw XmlException("Schema validation failed: no schema declaration found for root element '" + root->Name() + "'");
    }
    if (rootRule->isAbstract) {
        throw XmlException("Schema validation failed: element '" + root->Name() + "' is abstract and cannot appear in the document");
    }

    const auto wildcardNamespaceTokenMatches = [&](const std::string& token, const std::string& namespaceUri) {
        if (token.empty() || token == "##any") {
            return true;
        }
        if (token == "##other") {
            return namespaceUri != root->NamespaceURI();
        }
        if (token == "##targetNamespace") {
            return namespaceUri == root->NamespaceURI();
        }
        if (token == "##local") {
            return namespaceUri.empty();
        }
        return namespaceUri == token;
    };

    const auto matchesWildcardNamespace = [&](const std::string& namespaceConstraint, const std::string& namespaceUri) {
        if (namespaceConstraint.empty() || namespaceConstraint == "##any") {
            return true;
        }
        std::istringstream tokens(namespaceConstraint);
        std::string token;
        while (tokens >> token) {
            if (wildcardNamespaceTokenMatches(token, namespaceUri)) {
                return true;
            }
        }
        return false;
    };

    const auto wildcardAllowsValidationSkip = [](const std::string& processContents) {
        return processContents == "skip";
    };

    const auto wildcardAllowsUndeclared = [](const std::string& processContents) {
        return processContents == "lax" || processContents == "skip";
    };

    const auto resolveBuiltinSimpleType = [](const std::string& qualifiedName) -> std::optional<XmlSchemaSet::SimpleTypeRule> {
        const auto descriptor = ResolveBuiltinSimpleTypeDescriptor(qualifiedName);
        if (!descriptor.has_value()) {
            return std::nullopt;
        }

        XmlSchemaSet::SimpleTypeRule rule;
        rule.baseType = descriptor->baseType;
        rule.whiteSpace = descriptor->whiteSpace;
        if (descriptor->variety == BuiltinSimpleTypeDescriptor::Variety::List) {
            rule.variety = XmlSchemaSet::SimpleTypeRule::Variety::List;

            XmlSchemaSet::SimpleTypeRule itemType;
            itemType.baseType = descriptor->itemType;
            itemType.whiteSpace = descriptor->itemWhiteSpace;
            rule.itemType = std::make_shared<XmlSchemaSet::SimpleTypeRule>(itemType);
        }
        return rule;
    };

    const auto resolveBuiltinComplexType = [](const std::string& qualifiedName) -> std::optional<XmlSchemaSet::ComplexTypeRule> {
        if (qualifiedName != "xs:anyType") {
            return std::nullopt;
        }

        XmlSchemaSet::ComplexTypeRule rule;
        rule.namedTypeName = "anyType";
        rule.namedTypeNamespaceUri = "http://www.w3.org/2001/XMLSchema";
        rule.allowsText = true;
        rule.anyAttributeAllowed = true;
        rule.anyAttributeNamespaceConstraint = "##any";
        rule.anyAttributeProcessContents = "lax";

        XmlSchemaSet::Particle particle;
        particle.kind = XmlSchemaSet::Particle::Kind::Any;
        particle.namespaceUri = "##any";
        particle.processContents = "lax";
        particle.minOccurs = 0;
        particle.maxOccurs = std::numeric_limits<std::size_t>::max();
        rule.particle = particle;
        return rule;
    };

    const auto applyComplexTypeToElementRule = [](XmlSchemaSet::ElementRule& elementRule, const XmlSchemaSet::ComplexTypeRule& complexTypeRule) {
        elementRule.allowsText = complexTypeRule.allowsText;
        elementRule.textType = complexTypeRule.textType;
        elementRule.contentModel = complexTypeRule.contentModel;
        elementRule.attributes = complexTypeRule.attributes;
        elementRule.children = complexTypeRule.children;
        elementRule.particle = complexTypeRule.particle;
        elementRule.anyAttributeAllowed = complexTypeRule.anyAttributeAllowed;
        elementRule.anyAttributeNamespaceConstraint = complexTypeRule.anyAttributeNamespaceConstraint;
        elementRule.anyAttributeProcessContents = complexTypeRule.anyAttributeProcessContents;
    };

    const auto applySimpleTypeToElementRule = [](XmlSchemaSet::ElementRule& elementRule, const XmlSchemaSet::SimpleTypeRule& simpleTypeRule) {
        elementRule.declaredSimpleType = simpleTypeRule;
        elementRule.declaredComplexType.reset();
        elementRule.allowsText = true;
        elementRule.textType = simpleTypeRule;
        elementRule.contentModel = XmlSchemaSet::ContentModel::Empty;
        elementRule.attributes.clear();
        elementRule.children.clear();
        elementRule.particle.reset();
        elementRule.anyAttributeAllowed = false;
        elementRule.anyAttributeNamespaceConstraint = "##any";
        elementRule.anyAttributeProcessContents = "strict";
    };

    const auto parseSchemaInstanceTypeName = [&](const SchemaValidationFrame& frame, const std::string& lexicalValue) {
        const auto [prefix, localName] = SplitQualifiedName(lexicalValue);
        try {
            if (prefix.empty()) {
                (void)XmlConvert::VerifyNCName(localName);
                return std::pair<std::string, std::string>{localName, std::string{}};
            }

            (void)XmlConvert::VerifyNCName(prefix);
            (void)XmlConvert::VerifyNCName(localName);
            const std::string namespaceUri = LookupNamespaceUriOnSchemaFrame(frame, prefix);
            if (namespaceUri.empty()) {
                throw XmlException("undefined QName prefix");
            }
            return std::pair<std::string, std::string>{localName, namespaceUri};
        } catch (const XmlException&) {
            throw XmlException(
            "Schema validation failed: xsi:type on element '" + frame.name + "' must be a valid xs:QName value");
        }
    };

    const auto resolveEffectiveElementRule = [&](const SchemaValidationFrame& frame, const XmlSchemaSet::ElementRule& declaration) {
        XmlSchemaSet::ElementRule effectiveRule = declaration;
        const auto* typeAttribute = FindObservedSchemaAttribute(frame.observedAttributes, "type", kSchemaInstanceNamespace);
        if (typeAttribute == nullptr) {
            return effectiveRule;
        }

        const auto [typeLocalName, typeNamespaceUri] = parseSchemaInstanceTypeName(frame, typeAttribute->value);
        const std::string typeDisplayName = typeAttribute->value;

        if (const auto builtinType = resolveBuiltinSimpleType(typeNamespaceUri == "http://www.w3.org/2001/XMLSchema"
                ? "xs:" + typeLocalName
                : std::string{}); builtinType.has_value()) {
            if (declaration.declaredComplexType.has_value()) {
                throw XmlException(
                    "Schema validation failed: xsi:type '" + typeDisplayName + "' on element '" + frame.name
                    + "' must name a type derived from the declared element type");
            }

            bool usesRestriction = false;
            if (declaration.declaredSimpleType.has_value()
                && !runtimeSimpleTypeDerivesFrom(*builtinType, *declaration.declaredSimpleType, usesRestriction)) {
                throw XmlException(
                    "Schema validation failed: xsi:type '" + typeDisplayName + "' on element '" + frame.name
                    + "' must name a type derived from the declared element type");
            }
            if (usesRestriction && declaration.blockRestriction) {
                throw XmlException(
                    "Schema validation failed: xsi:type '" + typeDisplayName + "' on element '" + frame.name
                    + "' uses blocked restriction derivation");
            }

            applySimpleTypeToElementRule(effectiveRule, *builtinType);
            return effectiveRule;
        }

        if (const auto builtinComplexType = resolveBuiltinComplexType(typeNamespaceUri == "http://www.w3.org/2001/XMLSchema"
                ? "xs:" + typeLocalName
                : std::string{}); builtinComplexType.has_value()) {
            if (declaration.declaredSimpleType.has_value()) {
                throw XmlException(
                    "Schema validation failed: xsi:type '" + typeDisplayName + "' on element '" + frame.name
                    + "' must name a type derived from the declared element type");
            }

            bool usesRestriction = false;
            bool usesExtension = false;
            if (declaration.declaredComplexType.has_value()
                && !runtimeComplexTypeDerivesFrom(*builtinComplexType, *declaration.declaredComplexType, usesRestriction, usesExtension)) {
                throw XmlException(
                    "Schema validation failed: xsi:type '" + typeDisplayName + "' on element '" + frame.name
                    + "' must name a type derived from the declared element type");
            }

            effectiveRule.declaredSimpleType.reset();
            effectiveRule.declaredComplexType = *builtinComplexType;
            applyComplexTypeToElementRule(effectiveRule, *builtinComplexType);
            return effectiveRule;
        }

        if (const auto* namedSimpleType = schemas.FindSimpleTypeRule(typeLocalName, typeNamespaceUri); namedSimpleType != nullptr) {
            if (declaration.declaredComplexType.has_value()) {
                throw XmlException(
                    "Schema validation failed: xsi:type '" + typeDisplayName + "' on element '" + frame.name
                    + "' must name a type derived from the declared element type");
            }

            bool usesRestriction = false;
            if (declaration.declaredSimpleType.has_value()
                && !runtimeSimpleTypeDerivesFrom(*namedSimpleType, *declaration.declaredSimpleType, usesRestriction)) {
                throw XmlException(
                    "Schema validation failed: xsi:type '" + typeDisplayName + "' on element '" + frame.name
                    + "' must name a type derived from the declared element type");
            }
            if (usesRestriction && declaration.blockRestriction) {
                throw XmlException(
                    "Schema validation failed: xsi:type '" + typeDisplayName + "' on element '" + frame.name
                    + "' uses blocked restriction derivation");
            }

            applySimpleTypeToElementRule(effectiveRule, *namedSimpleType);
            return effectiveRule;
        }

        if (const auto* namedComplexType = schemas.FindComplexTypeRule(typeLocalName, typeNamespaceUri); namedComplexType != nullptr) {
            if (declaration.declaredSimpleType.has_value()) {
                throw XmlException(
                    "Schema validation failed: xsi:type '" + typeDisplayName + "' on element '" + frame.name
                    + "' must name a type derived from the declared element type");
            }

            bool usesRestriction = false;
            bool usesExtension = false;
            if (declaration.declaredComplexType.has_value()
                && !runtimeComplexTypeDerivesFrom(*namedComplexType, *declaration.declaredComplexType, usesRestriction, usesExtension)) {
                throw XmlException(
                    "Schema validation failed: xsi:type '" + typeDisplayName + "' on element '" + frame.name
                    + "' must name a type derived from the declared element type");
            }
            const bool blockedByBaseType = declaration.declaredComplexType.has_value()
                && ((usesRestriction && declaration.declaredComplexType->blockRestriction)
                    || (usesExtension && declaration.declaredComplexType->blockExtension));
            if ((usesRestriction && declaration.blockRestriction)
                || (usesExtension && declaration.blockExtension)
                || blockedByBaseType) {
                throw XmlException(
                    "Schema validation failed: xsi:type '" + typeDisplayName + "' on element '" + frame.name
                    + "' uses blocked type derivation");
            }

            effectiveRule.declaredSimpleType.reset();
            effectiveRule.declaredComplexType = *namedComplexType;
            applyComplexTypeToElementRule(effectiveRule, *namedComplexType);
            return effectiveRule;
        }

        throw XmlException(
            "Schema validation failed: xsi:type '" + typeDisplayName + "' on element '" + frame.name
            + "' is not a supported schema type");
    };

    const auto validateElement = [&](const auto& self, const XmlElement& element) -> void {
        const SchemaValidationFrame frame = BuildSchemaValidationFrame(element);
        const auto* rule = schemas.FindElementRule(frame.localName, frame.namespaceUri);
        if (rule == nullptr) {
            throw XmlException("Schema validation failed: no schema declaration found for element '" + frame.name + "'");
        }
        if (rule->isAbstract) {
            throw XmlException("Schema validation failed: element '" + frame.name + "' is abstract and cannot appear in the document");
        }

        const XmlSchemaSet::ElementRule effectiveRule = resolveEffectiveElementRule(frame, *rule);
        rule = &effectiveRule;

        if (rule->declaredComplexType.has_value() && rule->declaredComplexType->isAbstract) {
            throw XmlException(
                "Schema validation failed: complexType used by element '" + frame.name
                + "' is abstract and cannot be used directly");
        }

        const auto* nilAttribute = FindObservedSchemaAttribute(frame.observedAttributes, "nil", kSchemaInstanceNamespace);
        bool isNilled = false;
        if (nilAttribute != nullptr) {
            if (!TryParseXmlSchemaBoolean(nilAttribute->value, isNilled)) {
                throw XmlException(
                    "Schema validation failed: xsi:nil on element '" + frame.name + "' must be a valid xs:boolean value");
            }
            if (!rule->isNillable) {
                throw XmlException(
                    "Schema validation failed: element '" + frame.name + "' is not declared nillable and cannot use xsi:nil");
            }
        }

        for (const auto& attributeRule : rule->attributes) {
            const std::optional<std::string> effectiveAttributeValue =
                ResolveSchemaAttributeEffectiveValue(attributeRule, element, frame.observedAttributes);

            if (effectiveAttributeValue.has_value() && attributeRule.type.has_value()) {
                ValidateSchemaSimpleValue(*attributeRule.type, *effectiveAttributeValue,
                    "attribute '" + attributeRule.name + "' on element '" + element.Name() + "'", &element);
                recordIdentityTypedValue(*attributeRule.type, *effectiveAttributeValue,
                    "attribute '" + attributeRule.name + "' on element '" + element.Name() + "'", &element);
            }
        }

        for (const auto& attribute : frame.observedAttributes) {
            if (attribute.name == "xmlns" || attribute.prefix == "xmlns" || attribute.prefix == "xml") {
                continue;
            }
            if (attribute.localName == "nil" && attribute.namespaceUri == kSchemaInstanceNamespace) {
                continue;
            }
            if (attribute.localName == "type" && attribute.namespaceUri == kSchemaInstanceNamespace) {
                continue;
            }
            const bool declared = std::any_of(rule->attributes.begin(), rule->attributes.end(), [&](const auto& attributeRule) {
                return attribute.localName == attributeRule.name && attribute.namespaceUri == attributeRule.namespaceUri;
            });
            const bool wildcardMatches = rule->anyAttributeAllowed
                && matchesWildcardNamespace(rule->anyAttributeNamespaceConstraint, attribute.namespaceUri);
            const bool allowedByWildcard = wildcardMatches
                && wildcardAllowsUndeclared(rule->anyAttributeProcessContents);
            if (!declared && !allowedByWildcard) {
                throw XmlException(
                    "Schema validation failed: attribute '" + attribute.name
                    + "' is not declared for element '" + element.Name() + "'");
            }
        }

        const SchemaElementObservedContent& observedContent = frame.observedContent;
        const auto& elementChildren = observedContent.elementChildren;
        const bool hasSignificantText = observedContent.hasSignificantText;

        if (isNilled && (!elementChildren.empty() || hasSignificantText)) {
            throw XmlException(
                "Schema validation failed: element '" + element.Name() + "' is nilled and must not contain text or child elements");
        }

        const std::optional<std::string> effectiveElementValue =
            ResolveSchemaElementEffectiveValue(*rule, element, observedContent, isNilled);

        if (!isNilled) {
            if (rule->particle.has_value()) {
                const auto matches = CollectObservedParticleMatches(
                    *rule->particle,
                    elementChildren,
                    [&](const std::string& localName, const std::string& namespaceUri) {
                        return schemas.FindElementRule(localName, namespaceUri);
                    },
                    elementMatchesDeclaration,
                    matchesWildcardNamespace,
                    wildcardAllowsUndeclared,
                    [](const auto& particle) { return particle.kind == std::decay_t<decltype(particle)>::Kind::Element; },
                    [](const auto& particle) { return particle.kind == std::decay_t<decltype(particle)>::Kind::Any; },
                    [](const auto& particle) { return particle.kind == std::decay_t<decltype(particle)>::Kind::Sequence; },
                    [](const auto& particle) { return particle.kind == std::decay_t<decltype(particle)>::Kind::All; },
                    [](const auto& particle) -> const auto& { return particle.children; },
                    [](const auto& particle) -> const auto& { return particle.name; },
                    [](const auto& particle) -> const auto& { return particle.namespaceUri; },
                    [](const auto& particle) -> const auto& { return particle.processContents; },
                    [](const auto& particle) { return particle.minOccurs; },
                    [](const auto& particle) { return particle.maxOccurs; },
                    [](const auto& childElement) -> const auto& { return childElement.localName; },
                    [](const auto& childElement) -> const auto& { return childElement.namespaceUri; });
                if (std::find(matches.begin(), matches.end(), elementChildren.size()) == matches.end()) {
                    throw XmlException(
                        "Schema validation failed: child element structure under element '" + element.Name() + "' does not satisfy the declared content model");
                }
            } else if (!rule->children.empty()) {
                if (rule->contentModel == XmlSchemaSet::ContentModel::Choice) {
                    std::optional<std::size_t> selectedChoiceIndex;
                    std::vector<std::size_t> counts(rule->children.size(), 0);
                    for (const auto& childElement : elementChildren) {
                        const auto match = std::find_if(rule->children.begin(), rule->children.end(), [&](const auto& childRule) {
                            const auto* declaredRule = schemas.FindElementRule(childRule.name, childRule.namespaceUri);
                            return declaredRule != nullptr && elementMatchesDeclaration(childElement, *declaredRule);
                        });
                        if (match == rule->children.end()) {
                            throw XmlException(
                                "Schema validation failed: unexpected child element '" + childElement.name
                                + "' under element '" + element.Name() + "'");
                        }

                        const std::size_t choiceIndex = static_cast<std::size_t>(std::distance(rule->children.begin(), match));
                        if (selectedChoiceIndex.has_value() && selectedChoiceIndex.value() != choiceIndex) {
                            throw XmlException(
                                "Schema validation failed: element '" + element.Name() + "' allows only one choice branch at a time");
                        }
                        selectedChoiceIndex = choiceIndex;

                        const auto& choiceRule = rule->children[choiceIndex];
                        ++counts[choiceIndex];
                        if (choiceRule.maxOccurs != std::numeric_limits<std::size_t>::max()
                            && counts[choiceIndex] > choiceRule.maxOccurs) {
                            throw XmlException(
                                "Schema validation failed: child element '" + choiceRule.name
                                + "' exceeded maxOccurs under element '" + element.Name() + "'");
                        }
                    }

                    const auto requiredChoice = std::find_if(rule->children.begin(), rule->children.end(), [](const auto& childRule) {
                        return childRule.minOccurs > 0;
                    });
                    if (!selectedChoiceIndex.has_value()) {
                        if (requiredChoice != rule->children.end()) {
                            throw XmlException(
                                "Schema validation failed: element '" + element.Name() + "' requires one of the declared choice elements");
                        }
                    } else {
                        const auto& selectedRule = rule->children[selectedChoiceIndex.value()];
                        if (counts[selectedChoiceIndex.value()] < selectedRule.minOccurs) {
                            throw XmlException(
                                "Schema validation failed: child element '" + selectedRule.name
                                + "' does not satisfy minOccurs under element '" + element.Name() + "'");
                        }
                    }
                } else {
                    std::size_t childIndex = 0;
                    for (const auto& childRule : rule->children) {
                        std::size_t count = 0;
                        const auto* declaredRule = schemas.FindElementRule(childRule.name, childRule.namespaceUri);
                        if (declaredRule == nullptr) {
                            throw XmlException("Schema validation failed: no schema declaration found for element '" + childRule.name + "'");
                        }
                        while (childIndex < elementChildren.size()) {
                            const auto& childElement = elementChildren[childIndex];
                            if (!elementMatchesDeclaration(childElement, *declaredRule)) {
                                break;
                            }
                            ++count;
                            ++childIndex;
                            if (childRule.maxOccurs != std::numeric_limits<std::size_t>::max() && count == childRule.maxOccurs) {
                                break;
                            }
                        }
                        if (count < childRule.minOccurs) {
                            throw XmlException(
                                "Schema validation failed: expected child element '" + childRule.name
                                + "' under element '" + element.Name() + "'");
                        }
                    }
                    if (childIndex != elementChildren.size()) {
                        throw XmlException(
                            "Schema validation failed: unexpected child element '" + elementChildren[childIndex].name
                            + "' under element '" + element.Name() + "'");
                    }
                }
            } else if (!elementChildren.empty()) {
                throw XmlException(
                    "Schema validation failed: element '" + element.Name() + "' does not allow child elements");
            }

            if (!rule->allowsText && hasSignificantText) {
                throw XmlException(
                    "Schema validation failed: element '" + element.Name() + "' does not allow text content");
            }
            if (rule->textType.has_value() && effectiveElementValue.has_value()) {
                ValidateSchemaSimpleValue(*rule->textType, *effectiveElementValue,
                    "text content of element '" + element.Name() + "'", &element);
                recordIdentityTypedValue(*rule->textType, *effectiveElementValue,
                    "text content of element '" + element.Name() + "'", &element);
            }
        }

        std::function<bool(const XmlSchemaSet::Particle&, const SchemaObservedChildElement&)> explicitParticleMatchesObservedChildElement;
        explicitParticleMatchesObservedChildElement = [&](const XmlSchemaSet::Particle& particle, const SchemaObservedChildElement& childElement) -> bool {
            if (particle.kind == XmlSchemaSet::Particle::Kind::Element) {
                const auto* declaredRule = schemas.FindElementRule(particle.name, particle.namespaceUri);
                return declaredRule != nullptr && elementMatchesDeclaration(childElement, *declaredRule);
            }
            if (particle.kind == XmlSchemaSet::Particle::Kind::Any) {
                return false;
            }
            for (const auto& childParticle : particle.children) {
                if (explicitParticleMatchesObservedChildElement(childParticle, childElement)) {
                    return true;
                }
            }
            return false;
        };

        std::function<const XmlSchemaSet::Particle*(const XmlSchemaSet::Particle&, const SchemaObservedChildElement&)> wildcardParticleForObservedChildElement;
        wildcardParticleForObservedChildElement = [&](const XmlSchemaSet::Particle& particle, const SchemaObservedChildElement& childElement) -> const XmlSchemaSet::Particle* {
            if (particle.kind == XmlSchemaSet::Particle::Kind::Any
                && matchesWildcardNamespace(particle.namespaceUri, childElement.namespaceUri)) {
                return &particle;
            }
            if (particle.kind == XmlSchemaSet::Particle::Kind::Element) {
                return nullptr;
            }
            for (const auto& childParticle : particle.children) {
                if (const auto* found = wildcardParticleForObservedChildElement(childParticle, childElement); found != nullptr) {
                    return found;
                }
            }
            return nullptr;
        };

        const auto shouldValidateObservedChildElement = [&](const SchemaObservedChildElement& childElement) {
            const auto* childRule = schemas.FindElementRule(childElement.localName, childElement.namespaceUri);
            if (childRule == nullptr) {
                return false;
            }

            if (!rule->particle.has_value()) {
                return true;
            }

            if (explicitParticleMatchesObservedChildElement(*rule->particle, childElement)) {
                return true;
            }

            const auto* wildcardParticle = wildcardParticleForObservedChildElement(*rule->particle, childElement);
            if (wildcardParticle == nullptr) {
                return true;
            }

            return !wildcardAllowsValidationSkip(wildcardParticle->processContents);
        };

        for (const auto& childElement : elementChildren) {
            if (shouldValidateObservedChildElement(childElement) && childElement.element != nullptr) {
                self(self, *childElement.element);
            }
        }
    };

    validateElement(validateElement, *root);

    ValidatePendingSchemaIdReferences(identityState);

    if (schemas.HasIdentityConstraints()) {
        ValidateIdentityConstraints(schemas);
    }
}

void XmlDocument::ValidateIdentityConstraints(const XmlSchemaSet& schemas) const {
    const auto root = DocumentElement();
    if (root == nullptr) {
        return;
    }

    const auto evaluateCompiledIdentityConstraintPath = [](const XmlNode& contextNode,
                                                           const auto& path) {
        std::vector<const XmlNode*> currentNodes{std::addressof(contextNode)};
        for (const auto& step : path.steps) {
            std::vector<const XmlNode*> nextNodes;
            for (const XmlNode* node : currentNodes) {
                if (node == nullptr) {
                    continue;
                }

                if (step.kind == std::decay_t<decltype(step)>::Kind::Self) {
                    if (MatchesCompiledIdentityStep(step, node->LocalName(), node->NamespaceURI())) {
                        nextNodes.push_back(node);
                    }
                    continue;
                }

                if (step.kind == std::decay_t<decltype(step)>::Kind::Parent) {
                    const XmlNode* parent = node->ParentNode();
                    if (parent != nullptr
                        && parent->NodeType() == XmlNodeType::Element
                        && MatchesCompiledIdentityStep(step, parent->LocalName(), parent->NamespaceURI())
                        && MatchesCompiledIdentityPredicate(step, *parent)) {
                        nextNodes.push_back(parent);
                    }
                    continue;
                }

                if (step.kind == std::decay_t<decltype(step)>::Kind::Ancestor) {
                    AppendAncestorIdentityMatches(*node, step, false, nextNodes);
                    continue;
                }

                if (step.kind == std::decay_t<decltype(step)>::Kind::AncestorOrSelf) {
                    AppendAncestorIdentityMatches(*node, step, true, nextNodes);
                    continue;
                }

                if (step.kind == std::decay_t<decltype(step)>::Kind::FollowingSibling) {
                    AppendFollowingSiblingIdentityMatches(*node, step, nextNodes);
                    continue;
                }

                if (step.kind == std::decay_t<decltype(step)>::Kind::PrecedingSibling) {
                    AppendPrecedingSiblingIdentityMatches(*node, step, nextNodes);
                    continue;
                }

                if (step.kind == std::decay_t<decltype(step)>::Kind::Following) {
                    AppendFollowingIdentityMatches(*node, step, nextNodes);
                    continue;
                }

                if (step.kind == std::decay_t<decltype(step)>::Kind::Preceding) {
                    AppendPrecedingIdentityMatches(*node, step, nextNodes);
                    continue;
                }

                if (step.kind == std::decay_t<decltype(step)>::Kind::Element) {
                    for (const auto& child : node->ChildNodes()) {
                        if (child == nullptr || child->NodeType() != XmlNodeType::Element) {
                            continue;
                        }
                        if (MatchesCompiledIdentityStep(step, child->LocalName(), child->NamespaceURI())
                            && MatchesCompiledIdentityPredicate(step, *child)) {
                            nextNodes.push_back(child.get());
                        }
                    }
                    continue;
                }

                if (step.kind == std::decay_t<decltype(step)>::Kind::DescendantElement) {
                    AppendDescendantIdentityMatches(*node, step, nextNodes);
                    continue;
                }

                if (step.kind == std::decay_t<decltype(step)>::Kind::DescendantOrSelfElement) {
                    if (node->NodeType() == XmlNodeType::Element
                        && MatchesCompiledIdentityStep(step, node->LocalName(), node->NamespaceURI())
                        && MatchesCompiledIdentityPredicate(step, *node)) {
                        nextNodes.push_back(node);
                    }
                    AppendDescendantIdentityMatches(*node, step, nextNodes);
                    continue;
                }

                if (step.kind == std::decay_t<decltype(step)>::Kind::Attribute && node->NodeType() == XmlNodeType::Element) {
                    const auto* element = static_cast<const XmlElement*>(node);
                    for (const auto& attribute : element->Attributes()) {
                        if (attribute == nullptr) {
                            continue;
                        }
                        if (MatchesCompiledIdentityStep(step, attribute->LocalName(), attribute->NamespaceURI())) {
                            nextNodes.push_back(attribute.get());
                        }
                    }
                }
            }
            currentNodes = std::move(nextNodes);
            if (currentNodes.empty()) {
                break;
            }
        }

        return currentNodes;
    };

    const auto evaluateCompiledIdentityConstraintFieldValue = [](const XmlNode& contextNode,
                                                                 const auto& path) {
        IdentityConstraintFieldEvaluationResult result;

        std::vector<const XmlNode*> currentNodes{std::addressof(contextNode)};
        for (std::size_t stepIndex = 0; stepIndex < path.steps.size(); ++stepIndex) {
            const auto& step = path.steps[stepIndex];
            const bool isLastStep = stepIndex + 1 == path.steps.size();

            if (isLastStep && step.kind == std::decay_t<decltype(step)>::Kind::Attribute) {
                for (const XmlNode* node : currentNodes) {
                    if (node == nullptr || node->NodeType() != XmlNodeType::Element) {
                        continue;
                    }

                    const auto* element = static_cast<const XmlElement*>(node);
                    for (const auto& attribute : element->Attributes()) {
                        if (attribute == nullptr) {
                            continue;
                        }
                        if (!MatchesCompiledIdentityStep(step, attribute->LocalName(), attribute->NamespaceURI())) {
                            continue;
                        }

                        if (result.found) {
                            result.multiple = true;
                            return result;
                        }

                        result.found = true;
                        result.value = attribute->Value();
                    }
                }

                return result;
            }

            std::vector<const XmlNode*> nextNodes;
            for (const XmlNode* node : currentNodes) {
                if (node == nullptr) {
                    continue;
                }

                if (step.kind == std::decay_t<decltype(step)>::Kind::Self) {
                    if (MatchesCompiledIdentityStep(step, node->LocalName(), node->NamespaceURI())) {
                        nextNodes.push_back(node);
                    }
                    continue;
                }

                if (step.kind == std::decay_t<decltype(step)>::Kind::Parent) {
                    const XmlNode* parent = node->ParentNode();
                    if (parent != nullptr
                        && parent->NodeType() == XmlNodeType::Element
                        && MatchesCompiledIdentityStep(step, parent->LocalName(), parent->NamespaceURI())
                        && MatchesCompiledIdentityPredicate(step, *parent)) {
                        nextNodes.push_back(parent);
                    }
                    continue;
                }

                if (step.kind == std::decay_t<decltype(step)>::Kind::Ancestor) {
                    AppendAncestorIdentityMatches(*node, step, false, nextNodes);
                    continue;
                }

                if (step.kind == std::decay_t<decltype(step)>::Kind::AncestorOrSelf) {
                    AppendAncestorIdentityMatches(*node, step, true, nextNodes);
                    continue;
                }

                if (step.kind == std::decay_t<decltype(step)>::Kind::FollowingSibling) {
                    AppendFollowingSiblingIdentityMatches(*node, step, nextNodes);
                    continue;
                }

                if (step.kind == std::decay_t<decltype(step)>::Kind::PrecedingSibling) {
                    AppendPrecedingSiblingIdentityMatches(*node, step, nextNodes);
                    continue;
                }

                if (step.kind == std::decay_t<decltype(step)>::Kind::Following) {
                    AppendFollowingIdentityMatches(*node, step, nextNodes);
                    continue;
                }

                if (step.kind == std::decay_t<decltype(step)>::Kind::Preceding) {
                    AppendPrecedingIdentityMatches(*node, step, nextNodes);
                    continue;
                }

                if (step.kind == std::decay_t<decltype(step)>::Kind::Element) {
                    for (const auto& child : node->ChildNodes()) {
                        if (child == nullptr || child->NodeType() != XmlNodeType::Element) {
                            continue;
                        }
                        if (MatchesCompiledIdentityStep(step, child->LocalName(), child->NamespaceURI())
                            && MatchesCompiledIdentityPredicate(step, *child)) {
                            nextNodes.push_back(child.get());
                        }
                    }
                    continue;
                }

                if (step.kind == std::decay_t<decltype(step)>::Kind::DescendantElement) {
                    AppendDescendantIdentityMatches(*node, step, nextNodes);
                    continue;
                }

                if (step.kind == std::decay_t<decltype(step)>::Kind::DescendantOrSelfElement) {
                    if (node->NodeType() == XmlNodeType::Element
                        && MatchesCompiledIdentityStep(step, node->LocalName(), node->NamespaceURI())
                        && MatchesCompiledIdentityPredicate(step, *node)) {
                        nextNodes.push_back(node);
                    }
                    AppendDescendantIdentityMatches(*node, step, nextNodes);
                }
            }

            currentNodes = std::move(nextNodes);
            if (currentNodes.empty()) {
                return result;
            }
        }

        for (const XmlNode* node : currentNodes) {
            if (node == nullptr) {
                continue;
            }
            if (result.found) {
                result.multiple = true;
                return result;
            }

            result.found = true;
            result.value = IdentityConstraintFieldStringValue(*node);
        }

        return result;
    };

    struct IdentityConstraintTable {
        const XmlSchemaSet::ElementRule::IdentityConstraint* constraint = nullptr;
        std::unordered_set<std::string> tuples;
    };

    struct IdentityConstraintScope {
        const XmlElement* element = nullptr;
        std::vector<IdentityConstraintTable> tables;
        std::unordered_map<std::string, std::size_t> tableIndexByConstraintKey;
    };

    const auto buildConstraintNamespaces = [](const XmlSchemaSet::ElementRule::IdentityConstraint& constraint) {
        XmlNamespaceManager namespaces;
        for (const auto& [prefix, uri] : constraint.namespaceBindings) {
            namespaces.AddNamespace(prefix, uri);
        }
        return namespaces;
    };

    const auto forEachConstraintTuple = [&](const XmlElement& scopeElement,
                                           const XmlSchemaSet::ElementRule::IdentityConstraint& constraint,
                                           const auto& onTuple) {
        const XmlNamespaceManager namespaces = buildConstraintNamespaces(constraint);
        std::vector<const XmlNode*> selectedNodes;
        if (constraint.compiledSelectorPath.has_value()) {
            selectedNodes = evaluateCompiledIdentityConstraintPath(scopeElement, *constraint.compiledSelectorPath);
        } else {
            const XmlNodeList selectedNodeList = scopeElement.SelectNodes(constraint.selectorXPath, namespaces);
            selectedNodes.reserve(selectedNodeList.Count());
            for (std::size_t selectedIndex = 0; selectedIndex < selectedNodeList.Count(); ++selectedIndex) {
                const auto selectedNode = selectedNodeList.Item(selectedIndex);
                if (selectedNode != nullptr) {
                    selectedNodes.push_back(selectedNode.get());
                }
            }
        }

        for (const XmlNode* selectedNode : selectedNodes) {
            if (selectedNode == nullptr) {
                continue;
            }

            bool skipTuple = false;
            if (constraint.fieldXPaths.size() == 1) {
                const std::string& fieldXPath = constraint.fieldXPaths.front();
                const bool hasCompiledFieldPath = !constraint.compiledFieldPaths.empty()
                    && constraint.compiledFieldPaths.front().has_value();
                std::string fieldValue;
                if (hasCompiledFieldPath) {
                    const auto fieldEvaluation = evaluateCompiledIdentityConstraintFieldValue(
                        *selectedNode,
                        *constraint.compiledFieldPaths.front());
                    if (fieldEvaluation.multiple) {
                        throw XmlException(
                            "Schema validation failed: identity constraint '" + constraint.name
                            + "' field XPath '" + fieldXPath + "' must select at most one node per selector match");
                    }
                    if (!fieldEvaluation.found) {
                        if (constraint.kind == XmlSchemaSet::ElementRule::IdentityConstraint::Kind::Key) {
                            throw XmlException(
                                "Schema validation failed: identity constraint '" + constraint.name
                                + "' requires every selector match to provide field '" + fieldXPath + "'");
                        }
                        continue;
                    }
                    fieldValue = fieldEvaluation.value;
                } else {
                    const XmlNodeList fieldNodeList = selectedNode->SelectNodes(fieldXPath, namespaces);
                    if (fieldNodeList.Count() > 1) {
                        throw XmlException(
                            "Schema validation failed: identity constraint '" + constraint.name
                            + "' field XPath '" + fieldXPath + "' must select at most one node per selector match");
                    }
                    if (fieldNodeList.Count() == 0) {
                        if (constraint.kind == XmlSchemaSet::ElementRule::IdentityConstraint::Kind::Key) {
                            throw XmlException(
                                "Schema validation failed: identity constraint '" + constraint.name
                                + "' requires every selector match to provide field '" + fieldXPath + "'");
                        }
                        continue;
                    }

                    const auto fieldNode = fieldNodeList.Item(0);
                    fieldValue = fieldNode == nullptr ? std::string{} : IdentityConstraintFieldStringValue(*fieldNode);
                }

                if (fieldValue.empty()) {
                    if (constraint.kind == XmlSchemaSet::ElementRule::IdentityConstraint::Kind::Key) {
                        throw XmlException(
                            "Schema validation failed: identity constraint '" + constraint.name
                            + "' requires every selector match to provide a non-empty value for field '" + fieldXPath + "'");
                    }
                    continue;
                }

                onTuple(fieldValue);
                continue;
            }

            std::vector<std::string> fieldValues;
            fieldValues.reserve(constraint.fieldXPaths.size());
            for (std::size_t fieldIndex = 0; fieldIndex < constraint.fieldXPaths.size(); ++fieldIndex) {
                const std::string& fieldXPath = constraint.fieldXPaths[fieldIndex];
                const bool hasCompiledFieldPath = fieldIndex < constraint.compiledFieldPaths.size()
                    && constraint.compiledFieldPaths[fieldIndex].has_value();
                if (hasCompiledFieldPath) {
                    const auto fieldEvaluation = evaluateCompiledIdentityConstraintFieldValue(
                        *selectedNode,
                        *constraint.compiledFieldPaths[fieldIndex]);
                    if (fieldEvaluation.multiple) {
                        throw XmlException(
                            "Schema validation failed: identity constraint '" + constraint.name
                            + "' field XPath '" + fieldXPath + "' must select at most one node per selector match");
                    }
                    if (!fieldEvaluation.found) {
                        if (constraint.kind == XmlSchemaSet::ElementRule::IdentityConstraint::Kind::Key) {
                            throw XmlException(
                                "Schema validation failed: identity constraint '" + constraint.name
                                + "' requires every selector match to provide field '" + fieldXPath + "'");
                        }
                        skipTuple = true;
                        break;
                    }

                    if (fieldEvaluation.value.empty()) {
                        if (constraint.kind == XmlSchemaSet::ElementRule::IdentityConstraint::Kind::Key) {
                            throw XmlException(
                                "Schema validation failed: identity constraint '" + constraint.name
                                + "' requires every selector match to provide a non-empty value for field '" + fieldXPath + "'");
                        }
                        skipTuple = true;
                        break;
                    }

                    fieldValues.push_back(fieldEvaluation.value);
                } else {
                    std::vector<const XmlNode*> fieldNodes;
                    const XmlNodeList fieldNodeList = selectedNode->SelectNodes(fieldXPath, namespaces);
                    fieldNodes.reserve(fieldNodeList.Count());
                    for (std::size_t nodeIndex = 0; nodeIndex < fieldNodeList.Count(); ++nodeIndex) {
                        const auto fieldNode = fieldNodeList.Item(nodeIndex);
                        if (fieldNode != nullptr) {
                            fieldNodes.push_back(fieldNode.get());
                        }
                    }

                    if (fieldNodes.size() > 1) {
                        throw XmlException(
                            "Schema validation failed: identity constraint '" + constraint.name
                            + "' field XPath '" + fieldXPath + "' must select at most one node per selector match");
                    }
                    if (fieldNodes.empty()) {
                        if (constraint.kind == XmlSchemaSet::ElementRule::IdentityConstraint::Kind::Key) {
                            throw XmlException(
                                "Schema validation failed: identity constraint '" + constraint.name
                                + "' requires every selector match to provide field '" + fieldXPath + "'");
                        }
                        skipTuple = true;
                        break;
                    }

                    const std::string fieldValue = IdentityConstraintFieldStringValue(*fieldNodes.front());
                    if (fieldValue.empty()) {
                        if (constraint.kind == XmlSchemaSet::ElementRule::IdentityConstraint::Kind::Key) {
                            throw XmlException(
                                "Schema validation failed: identity constraint '" + constraint.name
                                + "' requires every selector match to provide a non-empty value for field '" + fieldXPath + "'");
                        }
                        skipTuple = true;
                        break;
                    }
                    fieldValues.push_back(fieldValue);
                }
            }

            if (!skipTuple) {
                onTuple(SerializeIdentityConstraintTuple(fieldValues));
            }
        }
    };

    const auto validateIdentityConstraints = [&](const auto& self,
                                                 const XmlElement& element,
                                                 std::vector<IdentityConstraintScope>& scopes) -> void {
        const auto* rule = schemas.FindElementRule(element.LocalName(), element.NamespaceURI());
        if (rule == nullptr) {
            return;
        }

        IdentityConstraintScope currentScope;
        currentScope.element = &element;

        for (const auto& constraint : rule->identityConstraints) {
            if (constraint.kind == XmlSchemaSet::ElementRule::IdentityConstraint::Kind::KeyRef) {
                continue;
            }

            IdentityConstraintTable table;
            table.constraint = &constraint;
            forEachConstraintTuple(element, constraint, [&](const std::string& tuple) {
                if (!table.tuples.insert(tuple).second) {
                    const char* kindLabel = constraint.kind == XmlSchemaSet::ElementRule::IdentityConstraint::Kind::Key
                        ? "key"
                        : "unique";
                    throw XmlException(
                        std::string("Schema validation failed: identity constraint '") + constraint.name
                        + "' violates " + kindLabel + " uniqueness under element '" + element.Name() + "'");
                }
            });
            currentScope.tables.push_back(std::move(table));
            currentScope.tableIndexByConstraintKey.emplace(
                BuildIdentityConstraintLookupKey(constraint.name, constraint.namespaceUri),
                currentScope.tables.size() - 1);
        }

        scopes.push_back(std::move(currentScope));

        for (const auto& constraint : rule->identityConstraints) {
            if (constraint.kind != XmlSchemaSet::ElementRule::IdentityConstraint::Kind::KeyRef) {
                continue;
            }

            const IdentityConstraintTable* referencedTable = nullptr;
            const std::string referencedConstraintKey = BuildIdentityConstraintLookupKey(constraint.referName, constraint.referNamespaceUri);
            for (auto scopeIt = scopes.rbegin(); scopeIt != scopes.rend() && referencedTable == nullptr; ++scopeIt) {
                const auto found = scopeIt->tableIndexByConstraintKey.find(referencedConstraintKey);
                if (found != scopeIt->tableIndexByConstraintKey.end()) {
                    referencedTable = std::addressof(scopeIt->tables[found->second]);
                }
            }

            if (referencedTable == nullptr) {
                throw XmlException(
                    "Schema validation failed: keyref '" + constraint.name
                    + "' could not find an in-scope referenced key/unique constraint");
            }

            forEachConstraintTuple(element, constraint, [&](const std::string& tuple) {
                if (referencedTable->tuples.find(tuple) == referencedTable->tuples.end()) {
                    throw XmlException(
                        "Schema validation failed: keyref '" + constraint.name
                        + "' references a value with no matching key/unique tuple");
                }
            });
        }

        for (const auto& child : element.ChildNodes()) {
            if (child == nullptr || child->NodeType() != XmlNodeType::Element) {
                continue;
            }
            self(self, *static_cast<const XmlElement*>(child.get()), scopes);
        }

        scopes.pop_back();
    };

    std::vector<IdentityConstraintScope> identityScopes;
    validateIdentityConstraints(validateIdentityConstraints, *root, identityScopes);
}

bool XmlDocument::PreserveWhitespace() const noexcept {
    return preserveWhitespace_;
}

void XmlDocument::SetPreserveWhitespace(bool value) noexcept {
    preserveWhitespace_ = value;
}

std::string XmlDocument::ToString(const XmlWriterSettings& settings) const {
    return XmlWriter::WriteToString(*this, settings);
}

void XmlDocument::RemoveAll() {
    RemoveAllChildren();
}

std::shared_ptr<XmlDeclaration> XmlDocument::Declaration() const {
    for (const auto& child : ChildNodes()) {
        if (child->NodeType() == XmlNodeType::XmlDeclaration) {
            return std::static_pointer_cast<XmlDeclaration>(child);
        }
    }

    return nullptr;
}

std::shared_ptr<XmlDocumentType> XmlDocument::DocumentType() const {
    for (const auto& child : ChildNodes()) {
        if (child->NodeType() == XmlNodeType::DocumentType) {
            return std::static_pointer_cast<XmlDocumentType>(child);
        }
    }

    return nullptr;
}

std::shared_ptr<XmlElement> XmlDocument::DocumentElement() const {
    for (const auto& child : ChildNodes()) {
        if (child->NodeType() == XmlNodeType::Element) {
            return std::static_pointer_cast<XmlElement>(child);
        }
    }

    return nullptr;
}

std::vector<std::shared_ptr<XmlElement>> XmlDocument::GetElementsByTagName(std::string_view name) const {
    std::vector<std::shared_ptr<XmlElement>> results;
    CollectElementsByName(*this, name, results);
    return results;
}

XmlNodeList XmlDocument::GetElementsByTagNameList(std::string_view name) const {
    std::vector<std::shared_ptr<XmlNode>> nodes;
    for (const auto& element : GetElementsByTagName(name)) {
        nodes.push_back(element);
    }
    return XmlNodeList(std::move(nodes));
}

std::vector<std::shared_ptr<XmlElement>> XmlDocument::GetElementsByTagName(std::string_view localName, std::string_view namespaceUri) const {
    std::vector<std::shared_ptr<XmlElement>> results;
    CollectElementsByNameNS(*this, localName, namespaceUri, results);
    return results;
}

XmlNodeList XmlDocument::GetElementsByTagNameList(std::string_view localName, std::string_view namespaceUri) const {
    std::vector<std::shared_ptr<XmlNode>> nodes;
    for (const auto& element : GetElementsByTagName(localName, namespaceUri)) {
        nodes.push_back(element);
    }
    return XmlNodeList(std::move(nodes));
}

std::shared_ptr<XmlDocumentFragment> XmlDocument::CreateDocumentFragment() const {
    return AllocateOwnedNode<XmlDocumentFragment>();
}

std::shared_ptr<XmlElement> XmlDocument::CreateElement(std::string_view name) const {
    return AllocateOwnedNode<XmlElement>(std::string(name));
}

std::shared_ptr<XmlElement> XmlDocument::CreateElement(std::string_view prefix, std::string_view localName, std::string_view namespaceUri) const {
    std::string qualifiedName = prefix.empty() ? std::string(localName) : (std::string(prefix) + ":" + std::string(localName));
    auto node = AllocateOwnedNode<XmlElement>(std::move(qualifiedName));
    if (!namespaceUri.empty()) {
        std::string nsAttrName = prefix.empty() ? std::string("xmlns") : ("xmlns:" + std::string(prefix));
        node->SetAttribute(nsAttrName, namespaceUri);
    }
    return node;
}

std::shared_ptr<XmlAttribute> XmlDocument::CreateAttribute(std::string_view name, std::string_view value) const {
    return AllocateOwnedNode<XmlAttribute>(std::string(name), std::string(value));
}

std::shared_ptr<XmlAttribute> XmlDocument::CreateAttribute(std::string_view prefix, std::string_view localName, std::string_view namespaceUri, std::string_view value) const {
    (void)namespaceUri;
    std::string qualifiedName = prefix.empty() ? std::string(localName) : (std::string(prefix) + ":" + std::string(localName));
    return AllocateOwnedNode<XmlAttribute>(std::move(qualifiedName), std::string(value));
}

std::shared_ptr<XmlText> XmlDocument::CreateTextNode(std::string_view value) const {
    return AllocateOwnedNode<XmlText>(std::string(value));
}

std::shared_ptr<XmlEntityReference> XmlDocument::CreateEntityReference(std::string_view name) const {
    if (name.empty()) {
        throw XmlException("CreateEntityReference requires an entity name");
    }

    std::string resolvedValue = ResolvePredefinedEntityReferenceValue(name);
    if (resolvedValue.empty()) {
        if (const auto declared = LookupDocumentInternalEntityDeclaration(this, name); declared.has_value()) {
            std::vector<std::string> resolutionStack{std::string(name)};
            resolvedValue = DecodeEntityText(
                *declared,
                [this](const std::string& entity) {
                    return LookupDocumentInternalEntityDeclaration(this, entity);
                },
                [](const std::string& message) {
                    throw XmlException(message);
                },
                0,
                &resolutionStack);
        }
    }

    return AllocateOwnedNode<XmlEntityReference>(std::string(name), std::move(resolvedValue));
}

std::shared_ptr<XmlWhitespace> XmlDocument::CreateWhitespace(std::string_view value) const {
    return AllocateOwnedNode<XmlWhitespace>(std::string(value));
}

std::shared_ptr<XmlSignificantWhitespace> XmlDocument::CreateSignificantWhitespace(std::string_view value) const {
    return AllocateOwnedNode<XmlSignificantWhitespace>(std::string(value));
}

std::shared_ptr<XmlCDataSection> XmlDocument::CreateCDataSection(std::string_view value) const {
    return AllocateOwnedNode<XmlCDataSection>(std::string(value));
}

std::shared_ptr<XmlComment> XmlDocument::CreateComment(std::string_view value) const {
    return AllocateOwnedNode<XmlComment>(std::string(value));
}

std::shared_ptr<XmlProcessingInstruction> XmlDocument::CreateProcessingInstruction(std::string_view target, std::string_view data) const {
    return AllocateOwnedNode<XmlProcessingInstruction>(std::string(target), std::string(data));
}

std::shared_ptr<XmlDeclaration> XmlDocument::CreateXmlDeclaration(
    std::string_view version,
    std::string_view encoding,
    std::string_view standalone) const {
    return AllocateOwnedNode<XmlDeclaration>(std::string(version), std::string(encoding), std::string(standalone));
}

std::shared_ptr<XmlDocumentType> XmlDocument::CreateDocumentType(
    std::string_view name,
    std::string_view publicId,
    std::string_view systemId,
    std::string_view internalSubset) const {
    return AllocateOwnedNode<XmlDocumentType>(std::string(name), std::string(publicId), std::string(systemId), std::string(internalSubset));
}

std::shared_ptr<XmlNode> XmlDocument::CreateNode(
    XmlNodeType nodeType,
    std::string_view name,
    std::string_view value) const {
    switch (nodeType) {
    case XmlNodeType::Element:
        if (name.empty()) {
            throw XmlException("CreateNode requires a qualified name for element nodes");
        }
        return CreateElement(name);
    case XmlNodeType::Attribute:
        if (name.empty()) {
            throw XmlException("CreateNode requires a qualified name for attribute nodes");
        }
        return CreateAttribute(name, value);
    case XmlNodeType::Text:
        return CreateTextNode(value);
    case XmlNodeType::EntityReference:
        return CreateEntityReference(name);
    case XmlNodeType::CDATA:
        return CreateCDataSection(value);
    case XmlNodeType::Comment:
        return CreateComment(value);
    case XmlNodeType::Whitespace:
        return CreateWhitespace(value);
    case XmlNodeType::SignificantWhitespace:
        return CreateSignificantWhitespace(value);
    case XmlNodeType::ProcessingInstruction:
        if (name.empty()) {
            throw XmlException("CreateNode requires a target name for processing instruction nodes");
        }
        return CreateProcessingInstruction(name, value);
    case XmlNodeType::DocumentFragment:
        return CreateDocumentFragment();
    default:
        throw XmlException("CreateNode does not support the specified node type");
    }
}

std::shared_ptr<XmlNode> XmlDocument::ImportNode(const XmlNode& node, bool deep) const {
    if (node.NodeType() == XmlNodeType::Document) {
        throw XmlException("ImportNode does not support document nodes");
    }

    auto imported = node.CloneNode(deep);
    imported->SetOwnerDocumentRecursive(const_cast<XmlDocument*>(this));
    return imported;
}

std::shared_ptr<XmlNode> XmlDocument::AppendChild(const std::shared_ptr<XmlNode>& child) {
    ValidateChildInsertion(child);
    return XmlNode::AppendChild(child);
}

void XmlDocument::SetNodeInserting(XmlNodeChangedEventHandler handler) {
    onNodeInserting_ = std::move(handler);
}

void XmlDocument::SetNodeInserted(XmlNodeChangedEventHandler handler) {
    onNodeInserted_ = std::move(handler);
}

void XmlDocument::SetNodeRemoving(XmlNodeChangedEventHandler handler) {
    onNodeRemoving_ = std::move(handler);
}

void XmlDocument::SetNodeRemoved(XmlNodeChangedEventHandler handler) {
    onNodeRemoved_ = std::move(handler);
}

void XmlDocument::SetNodeChanging(XmlNodeChangedEventHandler handler) {
    onNodeChanging_ = std::move(handler);
}

void XmlDocument::SetNodeChanged(XmlNodeChangedEventHandler handler) {
    onNodeChanged_ = std::move(handler);
}

bool XmlDocument::HasNodeChangeHandlers() const noexcept {
    return static_cast<bool>(onNodeInserting_)
        || static_cast<bool>(onNodeInserted_)
        || static_cast<bool>(onNodeRemoving_)
        || static_cast<bool>(onNodeRemoved_)
        || static_cast<bool>(onNodeChanging_)
        || static_cast<bool>(onNodeChanged_);
}

void XmlDocument::FireNodeInserting(XmlNode* node, XmlNode* newParent) const {
    if (!onNodeInserting_) {
        return;
    }
    onNodeInserting_({XmlNodeChangedAction::Insert, node, node != nullptr ? node->ParentNode() : nullptr, newParent, {}, {}});
}

void XmlDocument::FireNodeInserted(XmlNode* node, XmlNode* newParent) const {
    if (!onNodeInserted_) {
        return;
    }
    onNodeInserted_({XmlNodeChangedAction::Insert, node, nullptr, newParent, {}, {}});
}

void XmlDocument::FireNodeRemoving(XmlNode* node, XmlNode* oldParent) const {
    if (!onNodeRemoving_) {
        return;
    }
    onNodeRemoving_({XmlNodeChangedAction::Remove, node, oldParent, nullptr, {}, {}});
}

void XmlDocument::FireNodeRemoved(XmlNode* node, XmlNode* oldParent) const {
    if (!onNodeRemoved_) {
        return;
    }
    onNodeRemoved_({XmlNodeChangedAction::Remove, node, oldParent, nullptr, {}, {}});
}

void XmlDocument::FireNodeChanging(XmlNode* node, const std::string& oldValue, const std::string& newValue) const {
    if (!onNodeChanging_) {
        return;
    }
    onNodeChanging_({XmlNodeChangedAction::Change, node, node != nullptr ? node->ParentNode() : nullptr, node != nullptr ? node->ParentNode() : nullptr, oldValue, newValue});
}

void XmlDocument::FireNodeChanged(XmlNode* node, const std::string& oldValue, const std::string& newValue) const {
    if (!onNodeChanged_) {
        return;
    }
    onNodeChanged_({XmlNodeChangedAction::Change, node, node != nullptr ? node->ParentNode() : nullptr, node != nullptr ? node->ParentNode() : nullptr, oldValue, newValue});
}

void XmlDocument::ValidateChildInsertion(const std::shared_ptr<XmlNode>& child, const XmlNode* replacingChild) const {
    XmlNode::ValidateChildInsertion(child, replacingChild);

    if (!child) {
        return;
    }

    auto countNodeType = [this, replacingChild](XmlNodeType nodeType) {
        std::size_t count = 0;
        for (const auto& existingChild : ChildNodes()) {
            if (existingChild.get() == replacingChild) {
                continue;
            }
            if (existingChild->NodeType() == nodeType) {
                ++count;
            }
        }
        return count;
    };

    if (child->NodeType() == XmlNodeType::DocumentFragment) {
        std::size_t elementCount = countNodeType(XmlNodeType::Element);
        std::size_t declarationCount = countNodeType(XmlNodeType::XmlDeclaration);
        std::size_t documentTypeCount = countNodeType(XmlNodeType::DocumentType);

        for (const auto& fragmentChild : child->ChildNodes()) {
            if (fragmentChild->NodeType() == XmlNodeType::Element && ++elementCount > 1) {
                throw XmlException("XML document can only contain a single root element");
            }
            if (fragmentChild->NodeType() == XmlNodeType::XmlDeclaration && ++declarationCount > 1) {
                throw XmlException("XML document can only contain a single XML declaration");
            }
            if (fragmentChild->NodeType() == XmlNodeType::DocumentType && ++documentTypeCount > 1) {
                throw XmlException("XML document can only contain a single DOCTYPE declaration");
            }
        }
        return;
    }

    if (child->NodeType() == XmlNodeType::Element && countNodeType(XmlNodeType::Element) > 0) {
        throw XmlException("XML document can only contain a single root element");
    }
    if (child->NodeType() == XmlNodeType::XmlDeclaration && countNodeType(XmlNodeType::XmlDeclaration) > 0) {
        throw XmlException("XML document can only contain a single XML declaration");
    }
    if (child->NodeType() == XmlNodeType::DocumentType && countNodeType(XmlNodeType::DocumentType) > 0) {
        throw XmlException("XML document can only contain a single DOCTYPE declaration");
    }
}

void XmlDocument::ValidateReaderAgainstSchemas(XmlReader& reader, const XmlSchemaSet& schemas) const {
    static const std::string kSchemaInstanceNamespace = "http://www.w3.org/2001/XMLSchema-instance";
    static const std::unordered_set<std::string> emptyStringSet;

    const auto readerCanEvaluateIdentityConstraint = [](const XmlSchemaSet::ElementRule::IdentityConstraint& constraint) {
        if (!constraint.compiledSelectorPath.has_value()) {
            return false;
        }
        if (constraint.compiledFieldPaths.size() != constraint.fieldXPaths.size()) {
            return false;
        }
        return std::all_of(constraint.compiledFieldPaths.begin(), constraint.compiledFieldPaths.end(), [](const auto& fieldPath) {
            return fieldPath.has_value();
        });
    };

    const auto wildcardNamespaceTokenMatchesForDomFallback = [](const std::string& token,
                                                                const std::string& namespaceUri,
                                                                const std::string& rootNamespaceUri) {
        if (token.empty() || token == "##any") {
            return true;
        }
        if (token == "##other") {
            return namespaceUri != rootNamespaceUri;
        }
        if (token == "##targetNamespace") {
            return namespaceUri == rootNamespaceUri;
        }
        if (token == "##local") {
            return namespaceUri.empty();
        }
        return namespaceUri == token;
    };

    const auto matchesWildcardNamespaceForDomFallback = [&](const std::string& namespaceConstraint,
                                                            const std::string& namespaceUri,
                                                            const std::string& rootNamespaceUri) {
        if (namespaceConstraint.empty() || namespaceConstraint == "##any") {
            return true;
        }

        std::istringstream tokens(namespaceConstraint);
        std::string token;
        while (tokens >> token) {
            if (wildcardNamespaceTokenMatchesForDomFallback(token, namespaceUri, rootNamespaceUri)) {
                return true;
            }
        }
        return false;
    };

    std::string rootRuleNamespaceUri;

    struct RuntimeDerivedComplexTypeCacheKey {
        const XmlSchemaSet::ComplexTypeRule* declaredType = nullptr;
        bool declarationBlockRestriction = false;
        bool declarationBlockExtension = false;

        bool operator==(const RuntimeDerivedComplexTypeCacheKey& other) const noexcept {
            return declaredType == other.declaredType
                && declarationBlockRestriction == other.declarationBlockRestriction
                && declarationBlockExtension == other.declarationBlockExtension;
        }
    };

    struct RuntimeDerivedComplexTypeCacheKeyHasher {
        std::size_t operator()(const RuntimeDerivedComplexTypeCacheKey& key) const noexcept {
            std::size_t seed = std::hash<const void*>{}(key.declaredType);
            seed ^= std::hash<bool>{}(key.declarationBlockRestriction) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= std::hash<bool>{}(key.declarationBlockExtension) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            return seed;
        }
    };

    std::unordered_map<RuntimeDerivedComplexTypeCacheKey,
        std::vector<const XmlSchemaSet::ComplexTypeRule*>,
        RuntimeDerivedComplexTypeCacheKeyHasher> runtimeDerivedComplexTypeCandidatesCache;
    std::unordered_map<std::string, std::vector<const XmlSchemaSet::ComplexTypeRule*>> complexTypesByBaseKey;

    for (const auto& namedComplexType : schemas.complexTypes_) {
        if (!namedComplexType.rule.derivationBaseName.empty()) {
            const std::string baseKey = namedComplexType.rule.derivationBaseNamespaceUri + "\n" + namedComplexType.rule.derivationBaseName;
            complexTypesByBaseKey[baseKey].push_back(std::addressof(namedComplexType.rule));
        }
    }

    const auto runtimeComplexTypeCanSubstituteViaXsiType = [&](const XmlSchemaSet::ComplexTypeRule& derivedType,
                                                               const XmlSchemaSet::ComplexTypeRule& baseType,
                                                               bool declarationBlockRestriction,
                                                               bool declarationBlockExtension) {
        if (baseType.namedTypeName.empty() || derivedType.namedTypeName.empty()) {
            return false;
        }
        if (derivedType.namedTypeName == baseType.namedTypeName
            && derivedType.namedTypeNamespaceUri == baseType.namedTypeNamespaceUri) {
            return false;
        }

        bool usesRestriction = false;
        bool usesExtension = false;
        const XmlSchemaSet::ComplexTypeRule* current = &derivedType;
        std::unordered_set<std::string> visited;
        while (true) {
            if (current->derivationMethod == XmlSchemaSet::ComplexTypeRule::DerivationMethod::Restriction) {
                usesRestriction = true;
            } else if (current->derivationMethod == XmlSchemaSet::ComplexTypeRule::DerivationMethod::Extension) {
                usesExtension = true;
            }

            if (current->derivationBaseName == baseType.namedTypeName
                && current->derivationBaseNamespaceUri == baseType.namedTypeNamespaceUri) {
                const bool blockedByBaseType = (usesRestriction && baseType.blockRestriction)
                    || (usesExtension && baseType.blockExtension);
                return !((usesRestriction && declarationBlockRestriction)
                    || (usesExtension && declarationBlockExtension)
                    || blockedByBaseType);
            }
            if (current->derivationBaseName.empty()) {
                return false;
            }

            const std::string currentKey = current->derivationBaseNamespaceUri + "\n" + current->derivationBaseName;
            if (!visited.insert(currentKey).second) {
                return false;
            }

            current = schemas.FindComplexTypeRule(current->derivationBaseName, current->derivationBaseNamespaceUri);
            if (current == nullptr) {
                return false;
            }
        }
    };

    std::function<bool(const XmlSchemaSet::ElementRule&, std::unordered_set<const XmlSchemaSet::ElementRule*>&, std::unordered_set<const XmlSchemaSet::ComplexTypeRule*>&)> elementRuleRequiresDom;
    std::function<bool(const XmlSchemaSet::ComplexTypeRule&, std::unordered_set<const XmlSchemaSet::ElementRule*>&, std::unordered_set<const XmlSchemaSet::ComplexTypeRule*>&)> complexTypeRequiresDom;
    std::function<bool(const XmlSchemaSet::Particle&, std::unordered_set<const XmlSchemaSet::ElementRule*>&, std::unordered_set<const XmlSchemaSet::ComplexTypeRule*>&)> particleRequiresDom;
    std::function<bool(const XmlSchemaSet::ComplexTypeRule&, bool, bool, std::unordered_set<const XmlSchemaSet::ElementRule*>&, std::unordered_set<const XmlSchemaSet::ComplexTypeRule*>&)> runtimeDerivedComplexTypesRequireDom;
    std::function<const std::vector<const XmlSchemaSet::ComplexTypeRule*>&(const XmlSchemaSet::ComplexTypeRule&, bool, bool)> getRuntimeDerivedComplexTypeCandidates;

    getRuntimeDerivedComplexTypeCandidates = [&](const XmlSchemaSet::ComplexTypeRule& declaredType,
                                                 bool declarationBlockRestriction,
                                                 bool declarationBlockExtension) -> const std::vector<const XmlSchemaSet::ComplexTypeRule*>& {
        static const std::vector<const XmlSchemaSet::ComplexTypeRule*> empty;
        if (declaredType.namedTypeName.empty()) {
            return empty;
        }

        const RuntimeDerivedComplexTypeCacheKey key{
            std::addressof(declaredType),
            declarationBlockRestriction,
            declarationBlockExtension};
        const auto cached = runtimeDerivedComplexTypeCandidatesCache.find(key);
        if (cached != runtimeDerivedComplexTypeCandidatesCache.end()) {
            return cached->second;
        }

        std::vector<const XmlSchemaSet::ComplexTypeRule*> candidates;
        std::vector<const XmlSchemaSet::ComplexTypeRule*> pending;
        std::unordered_set<std::string> visitedDerivedKeys;

        const std::string declaredKey = declaredType.namedTypeNamespaceUri + "\n" + declaredType.namedTypeName;
        if (const auto derived = complexTypesByBaseKey.find(declaredKey); derived != complexTypesByBaseKey.end()) {
            pending = derived->second;
        }

        while (!pending.empty()) {
            const XmlSchemaSet::ComplexTypeRule* candidate = pending.back();
            pending.pop_back();
            if (candidate == nullptr || candidate->namedTypeName.empty()) {
                continue;
            }

            const std::string candidateKey = candidate->namedTypeNamespaceUri + "\n" + candidate->namedTypeName;
            if (!visitedDerivedKeys.insert(candidateKey).second) {
                continue;
            }

            if (runtimeComplexTypeCanSubstituteViaXsiType(
                    *candidate,
                    declaredType,
                    declarationBlockRestriction,
                    declarationBlockExtension)) {
                candidates.push_back(candidate);
            }

            if (const auto derived = complexTypesByBaseKey.find(candidateKey); derived != complexTypesByBaseKey.end()) {
                pending.insert(pending.end(), derived->second.begin(), derived->second.end());
            }
        }

        return runtimeDerivedComplexTypeCandidatesCache.emplace(key, std::move(candidates)).first->second;
    };

    runtimeDerivedComplexTypesRequireDom = [&](const XmlSchemaSet::ComplexTypeRule& declaredType,
                                               bool declarationBlockRestriction,
                                               bool declarationBlockExtension,
                                               std::unordered_set<const XmlSchemaSet::ElementRule*>& visitedElementRules,
                                               std::unordered_set<const XmlSchemaSet::ComplexTypeRule*>& visitedComplexTypes) {
        const auto& candidates = getRuntimeDerivedComplexTypeCandidates(
            declaredType,
            declarationBlockRestriction,
            declarationBlockExtension);

        return std::any_of(candidates.begin(), candidates.end(), [&](const auto* candidate) {
            return candidate != nullptr && complexTypeRequiresDom(*candidate, visitedElementRules, visitedComplexTypes);
        });
    };

    particleRequiresDom = [&](const XmlSchemaSet::Particle& particle,
                              std::unordered_set<const XmlSchemaSet::ElementRule*>& visitedElementRules,
                              std::unordered_set<const XmlSchemaSet::ComplexTypeRule*>& visitedComplexTypes) {
        if (particle.kind == XmlSchemaSet::Particle::Kind::Element) {
            if (particle.elementComplexType != nullptr
                && complexTypeRequiresDom(*particle.elementComplexType, visitedElementRules, visitedComplexTypes)) {
                return true;
            }
            if (particle.elementComplexType != nullptr
                && runtimeDerivedComplexTypesRequireDom(
                    *particle.elementComplexType,
                    particle.elementBlockRestriction,
                    particle.elementBlockExtension,
                    visitedElementRules,
                    visitedComplexTypes)) {
                return true;
            }
            if (const auto* declaredRule = schemas.FindElementRule(particle.name, particle.namespaceUri);
                declaredRule != nullptr && elementRuleRequiresDom(*declaredRule, visitedElementRules, visitedComplexTypes)) {
                return true;
            }
        }

        if (particle.kind == XmlSchemaSet::Particle::Kind::Any && particle.processContents != "skip") {
            for (const auto& candidate : schemas.elements_) {
                if (matchesWildcardNamespaceForDomFallback(particle.namespaceUri, candidate.namespaceUri, rootRuleNamespaceUri)
                    && elementRuleRequiresDom(candidate, visitedElementRules, visitedComplexTypes)) {
                    return true;
                }
            }
        }

        return std::any_of(particle.children.begin(), particle.children.end(), [&](const auto& childParticle) {
            return particleRequiresDom(childParticle, visitedElementRules, visitedComplexTypes);
        });
    };

    complexTypeRequiresDom = [&](const XmlSchemaSet::ComplexTypeRule& complexType,
                                 std::unordered_set<const XmlSchemaSet::ElementRule*>& visitedElementRules,
                                 std::unordered_set<const XmlSchemaSet::ComplexTypeRule*>& visitedComplexTypes) {
        if (!visitedComplexTypes.insert(std::addressof(complexType)).second) {
            return false;
        }

        if (complexType.particle.has_value()
            && particleRequiresDom(*complexType.particle, visitedElementRules, visitedComplexTypes)) {
            return true;
        }
        if (runtimeDerivedComplexTypesRequireDom(
                complexType,
                false,
                false,
                visitedElementRules,
                visitedComplexTypes)) {
            return true;
        }

        for (const auto& childRule : complexType.children) {
            if (childRule.declaredComplexType != nullptr
                && complexTypeRequiresDom(*childRule.declaredComplexType, visitedElementRules, visitedComplexTypes)) {
                return true;
            }
            if (childRule.declaredComplexType != nullptr
                && runtimeDerivedComplexTypesRequireDom(
                    *childRule.declaredComplexType,
                    childRule.blockRestriction,
                    childRule.blockExtension,
                    visitedElementRules,
                    visitedComplexTypes)) {
                return true;
            }
            if (const auto* declaredRule = schemas.FindElementRule(childRule.name, childRule.namespaceUri);
                declaredRule != nullptr && elementRuleRequiresDom(*declaredRule, visitedElementRules, visitedComplexTypes)) {
                return true;
            }
        }

        return false;
    };

    elementRuleRequiresDom = [&](const XmlSchemaSet::ElementRule& element,
                                 std::unordered_set<const XmlSchemaSet::ElementRule*>& visitedElementRules,
                                 std::unordered_set<const XmlSchemaSet::ComplexTypeRule*>& visitedComplexTypes) {
        if (!visitedElementRules.insert(std::addressof(element)).second) {
            return false;
        }

        if (std::any_of(element.identityConstraints.begin(), element.identityConstraints.end(), [&](const auto& constraint) {
                return !readerCanEvaluateIdentityConstraint(constraint);
            })) {
            return true;
        }
        if (element.declaredComplexType.has_value()
            && complexTypeRequiresDom(*element.declaredComplexType, visitedElementRules, visitedComplexTypes)) {
            return true;
        }
        if (element.declaredComplexType.has_value()
            && runtimeDerivedComplexTypesRequireDom(
                *element.declaredComplexType,
                element.blockRestriction,
                element.blockExtension,
                visitedElementRules,
                visitedComplexTypes)) {
            return true;
        }
        if (element.particle.has_value()
            && particleRequiresDom(*element.particle, visitedElementRules, visitedComplexTypes)) {
            return true;
        }

        for (const auto& childRule : element.children) {
            if (childRule.declaredComplexType != nullptr
                && complexTypeRequiresDom(*childRule.declaredComplexType, visitedElementRules, visitedComplexTypes)) {
                return true;
            }
            if (const auto* declaredRule = schemas.FindElementRule(childRule.name, childRule.namespaceUri);
                declaredRule != nullptr && elementRuleRequiresDom(*declaredRule, visitedElementRules, visitedComplexTypes)) {
                return true;
            }
        }

        for (const auto& candidate : schemas.elements_) {
            if (candidate.substitutionGroupHeadName == element.name
                && candidate.substitutionGroupHeadNamespaceUri == element.namespaceUri
                && elementRuleRequiresDom(candidate, visitedElementRules, visitedComplexTypes)) {
                return true;
            }
        }

        return false;
    };

    SchemaIdentityValidationState identityState;

    struct ReaderIdentityConstraintTable {
        const XmlSchemaSet::ElementRule::IdentityConstraint* constraint = nullptr;
        std::unordered_set<std::string> tuples;
    };

    struct ReaderPendingKeyRefValidation {
        const XmlSchemaSet::ElementRule::IdentityConstraint* constraint = nullptr;
        std::vector<std::string> tuples;
    };

    struct ReaderIdentityScope {
        int depth = 0;
        std::shared_ptr<XmlSchemaSet::ElementRule> rule;
        std::shared_ptr<SchemaIdentityNodeSnapshot> snapshot;
        std::vector<ReaderIdentityConstraintTable> tables;
        std::vector<ReaderPendingKeyRefValidation> pendingKeyRefs;
        std::unordered_set<std::string> declaredConstraintKeys;
        std::unordered_map<std::string, std::size_t> tableIndexByConstraintKey;
    };

    const auto evaluateIdentitySnapshotStringValue = [&](const auto& self, const SchemaIdentityNodeSnapshot& snapshot) -> std::string {
        std::string value = snapshot.textValue;
        for (const auto& child : snapshot.children) {
            if (child != nullptr) {
                value += self(self, *child);
            }
        }
        return value;
    };

    const auto evaluateCompiledIdentityPathOnSnapshot = [](const SchemaIdentityNodeSnapshot& contextSnapshot, const auto& path) {
        std::vector<ReaderIdentitySelection> currentSelections{{std::addressof(contextSnapshot), nullptr}};
        for (const auto& step : path.steps) {
            std::vector<ReaderIdentitySelection> nextSelections;
            for (const auto& selection : currentSelections) {
                if (selection.node == nullptr) {
                    continue;
                }

                if (step.kind == std::decay_t<decltype(step)>::Kind::Self) {
                    if (MatchesCompiledIdentityStep(step, selection.node->localName, selection.node->namespaceUri)) {
                        nextSelections.push_back(selection);
                    }
                    continue;
                }

                if (step.kind == std::decay_t<decltype(step)>::Kind::Parent) {
                    const auto* parent = selection.node->parent;
                    if (parent != nullptr
                        && MatchesCompiledIdentityStep(step, parent->localName, parent->namespaceUri)
                        && MatchesCompiledIdentityPredicate(step, *parent)) {
                        nextSelections.push_back(ReaderIdentitySelection{parent, nullptr});
                    }
                    continue;
                }

                if (step.kind == std::decay_t<decltype(step)>::Kind::Ancestor) {
                    AppendAncestorIdentityMatches(*selection.node, step, false, nextSelections);
                    continue;
                }

                if (step.kind == std::decay_t<decltype(step)>::Kind::AncestorOrSelf) {
                    AppendAncestorIdentityMatches(*selection.node, step, true, nextSelections);
                    continue;
                }

                if (step.kind == std::decay_t<decltype(step)>::Kind::FollowingSibling) {
                    AppendFollowingSiblingIdentityMatches(*selection.node, step, nextSelections);
                    continue;
                }

                if (step.kind == std::decay_t<decltype(step)>::Kind::PrecedingSibling) {
                    AppendPrecedingSiblingIdentityMatches(*selection.node, step, nextSelections);
                    continue;
                }

                if (step.kind == std::decay_t<decltype(step)>::Kind::Following) {
                    AppendFollowingIdentityMatches(*selection.node, step, nextSelections);
                    continue;
                }

                if (step.kind == std::decay_t<decltype(step)>::Kind::Preceding) {
                    AppendPrecedingIdentityMatches(*selection.node, step, nextSelections);
                    continue;
                }

                if (step.kind == std::decay_t<decltype(step)>::Kind::Element) {
                    for (const auto& child : selection.node->children) {
                        if (child != nullptr
                            && MatchesCompiledIdentityStep(step, child->localName, child->namespaceUri)
                            && MatchesCompiledIdentityPredicate(step, *child)) {
                            nextSelections.push_back(ReaderIdentitySelection{child.get(), nullptr});
                        }
                    }
                    continue;
                }

                if (step.kind == std::decay_t<decltype(step)>::Kind::DescendantElement) {
                    AppendDescendantIdentityMatches(*selection.node, step, nextSelections);
                    continue;
                }

                if (step.kind == std::decay_t<decltype(step)>::Kind::DescendantOrSelfElement) {
                    if (MatchesCompiledIdentityStep(step, selection.node->localName, selection.node->namespaceUri)
                        && MatchesCompiledIdentityPredicate(step, *selection.node)) {
                        nextSelections.push_back(selection);
                    }
                    AppendDescendantIdentityMatches(*selection.node, step, nextSelections);
                    continue;
                }

                if (step.kind == std::decay_t<decltype(step)>::Kind::Attribute) {
                    for (const auto& attribute : selection.node->attributes) {
                        if (MatchesCompiledIdentityStep(step, attribute.localName, attribute.namespaceUri)) {
                            nextSelections.push_back(ReaderIdentitySelection{nullptr, std::addressof(attribute)});
                        }
                    }
                }
            }
            currentSelections = std::move(nextSelections);
            if (currentSelections.empty()) {
                break;
            }
        }

        return currentSelections;
    };

    const auto evaluateCompiledIdentityFieldValueOnSnapshot = [&](const SchemaIdentityNodeSnapshot& contextSnapshot,
                                                                  const auto& path) {
        IdentityConstraintFieldEvaluationResult result;

        std::vector<const SchemaIdentityNodeSnapshot*> currentNodes{std::addressof(contextSnapshot)};
        for (std::size_t stepIndex = 0; stepIndex < path.steps.size(); ++stepIndex) {
            const auto& step = path.steps[stepIndex];
            const bool isLastStep = stepIndex + 1 == path.steps.size();

            if (isLastStep && step.kind == std::decay_t<decltype(step)>::Kind::Attribute) {
                for (const auto* node : currentNodes) {
                    if (node == nullptr) {
                        continue;
                    }

                    for (const auto& attribute : node->attributes) {
                        if (!MatchesCompiledIdentityStep(step, attribute.localName, attribute.namespaceUri)) {
                            continue;
                        }

                        if (result.found) {
                            result.multiple = true;
                            return result;
                        }

                        result.found = true;
                        result.value = attribute.value;
                    }
                }

                return result;
            }

            std::vector<const SchemaIdentityNodeSnapshot*> nextNodes;
            for (const auto* node : currentNodes) {
                if (node == nullptr) {
                    continue;
                }

                if (step.kind == std::decay_t<decltype(step)>::Kind::Self) {
                    if (MatchesCompiledIdentityStep(step, node->localName, node->namespaceUri)) {
                        nextNodes.push_back(node);
                    }
                    continue;
                }

                if (step.kind == std::decay_t<decltype(step)>::Kind::Parent) {
                    const auto* parent = node->parent;
                    if (parent != nullptr
                        && MatchesCompiledIdentityStep(step, parent->localName, parent->namespaceUri)
                        && MatchesCompiledIdentityPredicate(step, *parent)) {
                        nextNodes.push_back(parent);
                    }
                    continue;
                }

                if (step.kind == std::decay_t<decltype(step)>::Kind::Ancestor) {
                    AppendAncestorIdentityMatches(*node, step, false, nextNodes);
                    continue;
                }

                if (step.kind == std::decay_t<decltype(step)>::Kind::AncestorOrSelf) {
                    AppendAncestorIdentityMatches(*node, step, true, nextNodes);
                    continue;
                }

                if (step.kind == std::decay_t<decltype(step)>::Kind::FollowingSibling) {
                    AppendFollowingSiblingIdentityMatches(*node, step, nextNodes);
                    continue;
                }

                if (step.kind == std::decay_t<decltype(step)>::Kind::PrecedingSibling) {
                    AppendPrecedingSiblingIdentityMatches(*node, step, nextNodes);
                    continue;
                }

                if (step.kind == std::decay_t<decltype(step)>::Kind::Following) {
                    AppendFollowingIdentityMatches(*node, step, nextNodes);
                    continue;
                }

                if (step.kind == std::decay_t<decltype(step)>::Kind::Preceding) {
                    AppendPrecedingIdentityMatches(*node, step, nextNodes);
                    continue;
                }

                if (step.kind == std::decay_t<decltype(step)>::Kind::Element) {
                    for (const auto& child : node->children) {
                        if (child != nullptr
                            && MatchesCompiledIdentityStep(step, child->localName, child->namespaceUri)
                            && MatchesCompiledIdentityPredicate(step, *child)) {
                            nextNodes.push_back(child.get());
                        }
                    }
                    continue;
                }

                if (step.kind == std::decay_t<decltype(step)>::Kind::DescendantElement) {
                    AppendDescendantIdentityMatches(*node, step, nextNodes);
                    continue;
                }

                if (step.kind == std::decay_t<decltype(step)>::Kind::DescendantOrSelfElement) {
                    if (MatchesCompiledIdentityStep(step, node->localName, node->namespaceUri)
                        && MatchesCompiledIdentityPredicate(step, *node)) {
                        nextNodes.push_back(node);
                    }
                    AppendDescendantIdentityMatches(*node, step, nextNodes);
                }
            }

            currentNodes = std::move(nextNodes);
            if (currentNodes.empty()) {
                return result;
            }
        }

        for (const auto* node : currentNodes) {
            if (node == nullptr) {
                continue;
            }
            if (result.found) {
                result.multiple = true;
                return result;
            }

            result.found = true;
            result.value = evaluateIdentitySnapshotStringValue(evaluateIdentitySnapshotStringValue, *node);
        }

        return result;
    };

    const auto forEachIdentityConstraintTupleFromSnapshot = [&](const SchemaIdentityNodeSnapshot& scopeSnapshot,
                                                               const XmlSchemaSet::ElementRule::IdentityConstraint& constraint,
                                                               const auto& onTuple) {
        const auto selectedNodes = evaluateCompiledIdentityPathOnSnapshot(scopeSnapshot, *constraint.compiledSelectorPath);

        for (const auto& selectedNode : selectedNodes) {
            if (selectedNode.node == nullptr) {
                continue;
            }

            bool skipTuple = false;
            if (constraint.fieldXPaths.size() == 1) {
                const std::string& fieldXPath = constraint.fieldXPaths.front();
                const auto fieldEvaluation = evaluateCompiledIdentityFieldValueOnSnapshot(
                    *selectedNode.node,
                    *constraint.compiledFieldPaths.front());
                if (fieldEvaluation.multiple) {
                    throw XmlException(
                        "Schema validation failed: identity constraint '" + constraint.name
                        + "' field XPath '" + fieldXPath + "' must select at most one node per selector match");
                }
                if (!fieldEvaluation.found) {
                    if (constraint.kind == XmlSchemaSet::ElementRule::IdentityConstraint::Kind::Key) {
                        throw XmlException(
                            "Schema validation failed: identity constraint '" + constraint.name
                            + "' requires every selector match to provide field '" + fieldXPath + "'");
                    }
                    continue;
                }
                if (fieldEvaluation.value.empty()) {
                    if (constraint.kind == XmlSchemaSet::ElementRule::IdentityConstraint::Kind::Key) {
                        throw XmlException(
                            "Schema validation failed: identity constraint '" + constraint.name
                            + "' requires every selector match to provide a non-empty value for field '" + fieldXPath + "'");
                    }
                    continue;
                }

                onTuple(fieldEvaluation.value);
                continue;
            }

            std::vector<std::string> fieldValues;
            fieldValues.reserve(constraint.fieldXPaths.size());
            for (std::size_t fieldIndex = 0; fieldIndex < constraint.fieldXPaths.size(); ++fieldIndex) {
                const std::string& fieldXPath = constraint.fieldXPaths[fieldIndex];
                const auto fieldEvaluation = evaluateCompiledIdentityFieldValueOnSnapshot(*selectedNode.node, *constraint.compiledFieldPaths[fieldIndex]);
                if (fieldEvaluation.multiple) {
                    throw XmlException(
                        "Schema validation failed: identity constraint '" + constraint.name
                        + "' field XPath '" + fieldXPath + "' must select at most one node per selector match");
                }
                if (!fieldEvaluation.found) {
                    if (constraint.kind == XmlSchemaSet::ElementRule::IdentityConstraint::Kind::Key) {
                        throw XmlException(
                            "Schema validation failed: identity constraint '" + constraint.name
                            + "' requires every selector match to provide field '" + fieldXPath + "'");
                    }
                    skipTuple = true;
                    break;
                }

                if (fieldEvaluation.value.empty()) {
                    if (constraint.kind == XmlSchemaSet::ElementRule::IdentityConstraint::Kind::Key) {
                        throw XmlException(
                            "Schema validation failed: identity constraint '" + constraint.name
                            + "' requires every selector match to provide a non-empty value for field '" + fieldXPath + "'");
                    }
                    skipTuple = true;
                    break;
                }
                fieldValues.push_back(fieldEvaluation.value);
            }

            if (!skipTuple) {
                onTuple(SerializeIdentityConstraintTuple(fieldValues));
            }
        }
    };

    std::vector<ReaderIdentityScope> identityScopeStack;

    const auto findOpenIdentityScopeForConstraint = [&](const std::string& localName, const std::string& namespaceUri) -> ReaderIdentityScope* {
        const std::string constraintKey = BuildIdentityConstraintLookupKey(localName, namespaceUri);
        for (std::size_t index = identityScopeStack.size(); index > 0; --index) {
            ReaderIdentityScope& scope = identityScopeStack[index - 1];
            if (scope.rule == nullptr || scope.declaredConstraintKeys.find(constraintKey) == scope.declaredConstraintKeys.end()) {
                continue;
            }
            return std::addressof(scope);
        }
        return nullptr;
    };

    const auto validatePendingIdentityKeyRefsAgainstScope = [&](const ReaderIdentityScope& scope) {
        for (const auto& pendingKeyRef : scope.pendingKeyRefs) {
            const auto found = scope.tableIndexByConstraintKey.find(
                BuildIdentityConstraintLookupKey(
                    pendingKeyRef.constraint->referName,
                    pendingKeyRef.constraint->referNamespaceUri));
            if (found == scope.tableIndexByConstraintKey.end()) {
                throw XmlException(
                    "Schema validation failed: keyref '" + pendingKeyRef.constraint->name
                    + "' could not find an in-scope referenced key/unique constraint");
            }

            const auto& referencedTable = scope.tables[found->second];

            for (const auto& tuple : pendingKeyRef.tuples) {
                if (referencedTable.tuples.find(tuple) == referencedTable.tuples.end()) {
                    throw XmlException(
                        "Schema validation failed: keyref '" + pendingKeyRef.constraint->name
                        + "' references a value with no matching key/unique tuple");
                }
            }
        }
    };

    const auto finalizeReaderIdentityScope = [&](const SchemaValidationFrame& frame) {
        if (identityScopeStack.empty() || identityScopeStack.back().depth != frame.depth) {
            return;
        }

        ReaderIdentityScope& scope = identityScopeStack.back();
        if (scope.rule == nullptr || scope.snapshot == nullptr) {
            identityScopeStack.pop_back();
            return;
        }

        for (const auto& constraint : scope.rule->identityConstraints) {
            if (constraint.kind == XmlSchemaSet::ElementRule::IdentityConstraint::Kind::KeyRef) {
                continue;
            }

            ReaderIdentityConstraintTable table;
            table.constraint = std::addressof(constraint);
            forEachIdentityConstraintTupleFromSnapshot(*scope.snapshot, constraint, [&](const std::string& tuple) {
                if (!table.tuples.insert(tuple).second) {
                    const char* kindLabel = constraint.kind == XmlSchemaSet::ElementRule::IdentityConstraint::Kind::Key
                        ? "key"
                        : "unique";
                    throw XmlException(
                        std::string("Schema validation failed: identity constraint '") + constraint.name
                        + "' violates " + kindLabel + " uniqueness under element '" + frame.name + "'");
                }
            });
            scope.tables.push_back(std::move(table));
            scope.tableIndexByConstraintKey.emplace(
                BuildIdentityConstraintLookupKey(constraint.name, constraint.namespaceUri),
                scope.tables.size() - 1);
        }

        validatePendingIdentityKeyRefsAgainstScope(scope);

        for (const auto& constraint : scope.rule->identityConstraints) {
            if (constraint.kind != XmlSchemaSet::ElementRule::IdentityConstraint::Kind::KeyRef) {
                continue;
            }

            const auto found = scope.tableIndexByConstraintKey.find(
                BuildIdentityConstraintLookupKey(constraint.referName, constraint.referNamespaceUri));
            if (found != scope.tableIndexByConstraintKey.end()) {
                const auto& referencedTable = scope.tables[found->second];
                forEachIdentityConstraintTupleFromSnapshot(*scope.snapshot, constraint, [&](const std::string& tuple) {
                    if (referencedTable.tuples.find(tuple) == referencedTable.tuples.end()) {
                        throw XmlException(
                            "Schema validation failed: keyref '" + constraint.name
                            + "' references a value with no matching key/unique tuple");
                    }
                });
                continue;
            }

            ReaderIdentityScope* referencedScope = findOpenIdentityScopeForConstraint(constraint.referName, constraint.referNamespaceUri);
            if (referencedScope == nullptr) {
                throw XmlException(
                    "Schema validation failed: keyref '" + constraint.name
                    + "' could not find an in-scope referenced key/unique constraint");
            }

            std::vector<std::string> tuples;
            forEachIdentityConstraintTupleFromSnapshot(*scope.snapshot, constraint, [&](const std::string& tuple) {
                tuples.push_back(tuple);
            });

            referencedScope->pendingKeyRefs.push_back(ReaderPendingKeyRefValidation{
                std::addressof(constraint),
                tuples});
        }

        identityScopeStack.pop_back();
    };

    std::function<void(const XmlSchemaSet::SimpleTypeRule&, const std::string&, const std::string&, const std::vector<std::pair<std::string, std::string>>&)> recordIdentityTypedValue;
    recordIdentityTypedValue = [&](const XmlSchemaSet::SimpleTypeRule& type, const std::string& value, const std::string& label,
                                   const std::vector<std::pair<std::string, std::string>>& namespaceBindings) {
        if (type.variety == XmlSchemaSet::SimpleTypeRule::Variety::List) {
            if (type.itemType == nullptr) {
                return;
            }

            std::istringstream stream(NormalizeSchemaSimpleTypeValue(type, value));
            std::string item;
            std::size_t index = 0;
            while (stream >> item) {
                ++index;
                recordIdentityTypedValue(*type.itemType, item, label + " list item " + std::to_string(index), namespaceBindings);
            }
            return;
        }

        if (type.variety == XmlSchemaSet::SimpleTypeRule::Variety::Union) {
            for (const auto& memberType : type.memberTypes) {
                try {
                    ValidateSchemaSimpleValue(
                        memberType,
                        value,
                        label,
                        namespaceBindings,
                        reader.dtdState_ ? reader.dtdState_->notationDeclarationNames : emptyStringSet,
                        reader.dtdState_ ? reader.dtdState_->unparsedEntityDeclarationNames : emptyStringSet);
                    recordIdentityTypedValue(memberType, value, label, namespaceBindings);
                    return;
                } catch (const XmlException&) {
                }
            }
            return;
        }

        const std::string normalizedValue = NormalizeSchemaSimpleTypeValue(type, value);
        if (type.baseType == "xs:ID") {
            if (!identityState.declaredIds.insert(normalizedValue).second) {
                throw XmlException("Schema validation failed: " + label + " duplicates xs:ID value '" + normalizedValue + "'");
            }
            return;
        }
        if (type.baseType == "xs:IDREF") {
            identityState.pendingIdReferences.push_back(PendingSchemaIdReference{normalizedValue, label});
        }
    };

    const auto runtimeSimpleTypeDerivesFrom = [&](const XmlSchemaSet::SimpleTypeRule& derivedType,
        const XmlSchemaSet::SimpleTypeRule& baseType,
        bool& usesRestriction) {
        if (!derivedType.namedTypeName.empty()
            && derivedType.namedTypeName == baseType.namedTypeName
            && derivedType.namedTypeNamespaceUri == baseType.namedTypeNamespaceUri) {
            return true;
        }
        if (derivedType.namedTypeName.empty()
            && baseType.namedTypeName.empty()
            && derivedType.derivationMethod == XmlSchemaSet::SimpleTypeRule::DerivationMethod::None
            && baseType.derivationMethod == XmlSchemaSet::SimpleTypeRule::DerivationMethod::None
            && derivedType.variety == baseType.variety) {
            bool builtinUsesRestriction = false;
            if (BuiltinSimpleTypeDerivesFrom(derivedType.baseType, baseType.baseType, builtinUsesRestriction)) {
                usesRestriction = usesRestriction || builtinUsesRestriction;
                return true;
            }
        }

        const XmlSchemaSet::SimpleTypeRule* current = &derivedType;
        std::unordered_set<std::string> visited;
        while (true) {
            if (current->derivationMethod == XmlSchemaSet::SimpleTypeRule::DerivationMethod::Restriction) {
                usesRestriction = true;
            }
            if (!baseType.namedTypeName.empty()
                && current->derivationBaseName == baseType.namedTypeName
                && current->derivationBaseNamespaceUri == baseType.namedTypeNamespaceUri) {
                return true;
            }
            if (baseType.namedTypeName.empty()
                && current->derivationMethod == XmlSchemaSet::SimpleTypeRule::DerivationMethod::Restriction) {
                bool builtinUsesRestriction = false;
                if (BuiltinSimpleTypeDerivesFrom(current->baseType, baseType.baseType, builtinUsesRestriction)) {
                    usesRestriction = usesRestriction || builtinUsesRestriction;
                    return true;
                }
            }
            if (current->derivationBaseName.empty()) {
                return false;
            }

            const std::string currentKey = current->derivationBaseNamespaceUri + "\n" + current->derivationBaseName;
            if (!visited.insert(currentKey).second) {
                return false;
            }

            current = schemas.FindSimpleTypeRule(current->derivationBaseName, current->derivationBaseNamespaceUri);
            if (current == nullptr) {
                return false;
            }
            if (!baseType.namedTypeName.empty()
                && current->namedTypeName == baseType.namedTypeName
                && current->namedTypeNamespaceUri == baseType.namedTypeNamespaceUri) {
                return true;
            }
        }
    };

    const auto runtimeComplexTypeDerivesFrom = [&](const XmlSchemaSet::ComplexTypeRule& derivedType,
        const XmlSchemaSet::ComplexTypeRule& baseType,
        bool& usesRestriction,
        bool& usesExtension) {
        if (baseType.namedTypeName == "anyType"
            && baseType.namedTypeNamespaceUri == "http://www.w3.org/2001/XMLSchema") {
            return true;
        }
        if (!derivedType.namedTypeName.empty()
            && derivedType.namedTypeName == baseType.namedTypeName
            && derivedType.namedTypeNamespaceUri == baseType.namedTypeNamespaceUri) {
            return true;
        }

        const XmlSchemaSet::ComplexTypeRule* current = &derivedType;
        std::unordered_set<std::string> visited;
        while (true) {
            if (current->derivationMethod == XmlSchemaSet::ComplexTypeRule::DerivationMethod::Restriction) {
                usesRestriction = true;
            } else if (current->derivationMethod == XmlSchemaSet::ComplexTypeRule::DerivationMethod::Extension) {
                usesExtension = true;
            }
            if (current->derivationBaseName == baseType.namedTypeName
                && current->derivationBaseNamespaceUri == baseType.namedTypeNamespaceUri) {
                return true;
            }
            if (current->derivationBaseName.empty()) {
                return false;
            }

            const std::string currentKey = current->derivationBaseNamespaceUri + "\n" + current->derivationBaseName;
            if (!visited.insert(currentKey).second) {
                return false;
            }

            current = schemas.FindComplexTypeRule(current->derivationBaseName, current->derivationBaseNamespaceUri);
            if (current == nullptr) {
                return false;
            }
            if (!baseType.namedTypeName.empty()
                && current->namedTypeName == baseType.namedTypeName
                && current->namedTypeNamespaceUri == baseType.namedTypeNamespaceUri) {
                return true;
            }
        }
    };

    const auto runtimeElementCanSubstituteForHead = [&](const XmlSchemaSet::ElementRule& actualRule,
        const XmlSchemaSet::ElementRule& declaration) {
        bool usesRestriction = false;
        bool usesExtension = false;
        if (declaration.declaredComplexType.has_value()) {
            if (!actualRule.declaredComplexType.has_value()) {
                return false;
            }
            if (!runtimeComplexTypeDerivesFrom(*actualRule.declaredComplexType, *declaration.declaredComplexType, usesRestriction, usesExtension)) {
                return false;
            }
        } else if (declaration.declaredSimpleType.has_value()) {
            if (!actualRule.declaredSimpleType.has_value()) {
                return false;
            }
            if (!runtimeSimpleTypeDerivesFrom(*actualRule.declaredSimpleType, *declaration.declaredSimpleType, usesRestriction)) {
                return false;
            }
        }

        if ((usesRestriction && declaration.blockRestriction)
            || (usesExtension && declaration.blockExtension)) {
            return false;
        }
        return true;
    };

    const auto elementMatchesDeclaration = [&](const SchemaObservedChildElement& element, const XmlSchemaSet::ElementRule& declaration) {
        const auto* actualRule = schemas.FindElementRule(element.localName, element.namespaceUri);
        if (actualRule == nullptr) {
            return false;
        }
        if (actualRule->name == declaration.name && actualRule->namespaceUri == declaration.namespaceUri) {
            return true;
        }
        if (declaration.blockSubstitution) {
            return false;
        }

        std::string currentHeadName = actualRule->substitutionGroupHeadName;
        std::string currentHeadNamespaceUri = actualRule->substitutionGroupHeadNamespaceUri;
        std::unordered_set<std::string> visitedHeads;
        while (!currentHeadName.empty()) {
            const std::string headKey = currentHeadNamespaceUri + "\n" + currentHeadName;
            if (!visitedHeads.insert(headKey).second) {
                break;
            }
            if (currentHeadName == declaration.name && currentHeadNamespaceUri == declaration.namespaceUri) {
                return runtimeElementCanSubstituteForHead(*actualRule, declaration);
            }
            const auto* headRule = schemas.FindElementRule(currentHeadName, currentHeadNamespaceUri);
            if (headRule == nullptr) {
                break;
            }
            currentHeadName = headRule->substitutionGroupHeadName;
            currentHeadNamespaceUri = headRule->substitutionGroupHeadNamespaceUri;
        }
        return false;
    };

    std::string rootNamespaceUri;
    const auto wildcardNamespaceTokenMatches = [&](const std::string& token, const std::string& namespaceUri) {
        if (token.empty() || token == "##any") {
            return true;
        }
        if (token == "##other") {
            return namespaceUri != rootNamespaceUri;
        }
        if (token == "##targetNamespace") {
            return namespaceUri == rootNamespaceUri;
        }
        if (token == "##local") {
            return namespaceUri.empty();
        }
        return namespaceUri == token;
    };

    const auto matchesWildcardNamespace = [&](const std::string& namespaceConstraint, const std::string& namespaceUri) {
        if (namespaceConstraint.empty() || namespaceConstraint == "##any") {
            return true;
        }
        std::istringstream tokens(namespaceConstraint);
        std::string token;
        while (tokens >> token) {
            if (wildcardNamespaceTokenMatches(token, namespaceUri)) {
                return true;
            }
        }
        return false;
    };

    const auto wildcardAllowsValidationSkip = [](const std::string& processContents) {
        return processContents == "skip";
    };

    const auto wildcardAllowsUndeclared = [](const std::string& processContents) {
        return processContents == "lax" || processContents == "skip";
    };

    const auto resolveBuiltinSimpleType = [](const std::string& qualifiedName) -> std::optional<XmlSchemaSet::SimpleTypeRule> {
        const auto descriptor = ResolveBuiltinSimpleTypeDescriptor(qualifiedName);
        if (!descriptor.has_value()) {
            return std::nullopt;
        }

        XmlSchemaSet::SimpleTypeRule rule;
        rule.baseType = descriptor->baseType;
        rule.whiteSpace = descriptor->whiteSpace;
        if (descriptor->variety == BuiltinSimpleTypeDescriptor::Variety::List) {
            rule.variety = XmlSchemaSet::SimpleTypeRule::Variety::List;

            XmlSchemaSet::SimpleTypeRule itemType;
            itemType.baseType = descriptor->itemType;
            itemType.whiteSpace = descriptor->itemWhiteSpace;
            rule.itemType = std::make_shared<XmlSchemaSet::SimpleTypeRule>(itemType);
        }
        return rule;
    };

    const auto resolveBuiltinComplexType = [](const std::string& qualifiedName) -> std::optional<XmlSchemaSet::ComplexTypeRule> {
        if (qualifiedName != "xs:anyType") {
            return std::nullopt;
        }

        XmlSchemaSet::ComplexTypeRule rule;
        rule.namedTypeName = "anyType";
        rule.namedTypeNamespaceUri = "http://www.w3.org/2001/XMLSchema";
        rule.allowsText = true;
        rule.anyAttributeAllowed = true;
        rule.anyAttributeNamespaceConstraint = "##any";
        rule.anyAttributeProcessContents = "lax";

        XmlSchemaSet::Particle particle;
        particle.kind = XmlSchemaSet::Particle::Kind::Any;
        particle.namespaceUri = "##any";
        particle.processContents = "lax";
        particle.minOccurs = 0;
        particle.maxOccurs = std::numeric_limits<std::size_t>::max();
        rule.particle = particle;
        return rule;
    };

    const auto applyComplexTypeToElementRule = [](XmlSchemaSet::ElementRule& elementRule, const XmlSchemaSet::ComplexTypeRule& complexTypeRule) {
        elementRule.allowsText = complexTypeRule.allowsText;
        elementRule.textType = complexTypeRule.textType;
        elementRule.contentModel = complexTypeRule.contentModel;
        elementRule.attributes = complexTypeRule.attributes;
        elementRule.children = complexTypeRule.children;
        elementRule.particle = complexTypeRule.particle;
        elementRule.anyAttributeAllowed = complexTypeRule.anyAttributeAllowed;
        elementRule.anyAttributeNamespaceConstraint = complexTypeRule.anyAttributeNamespaceConstraint;
        elementRule.anyAttributeProcessContents = complexTypeRule.anyAttributeProcessContents;
    };

    const auto applySimpleTypeToElementRule = [](XmlSchemaSet::ElementRule& elementRule, const XmlSchemaSet::SimpleTypeRule& simpleTypeRule) {
        elementRule.declaredSimpleType = simpleTypeRule;
        elementRule.declaredComplexType.reset();
        elementRule.allowsText = true;
        elementRule.textType = simpleTypeRule;
        elementRule.contentModel = XmlSchemaSet::ContentModel::Empty;
        elementRule.attributes.clear();
        elementRule.children.clear();
        elementRule.particle.reset();
        elementRule.anyAttributeAllowed = false;
        elementRule.anyAttributeNamespaceConstraint = "##any";
        elementRule.anyAttributeProcessContents = "strict";
    };

    const auto parseSchemaInstanceTypeName = [&](const SchemaValidationFrame& frame, const std::string& lexicalValue) {
        const auto [prefix, localName] = SplitQualifiedName(lexicalValue);
        try {
            if (prefix.empty()) {
                (void)XmlConvert::VerifyNCName(localName);
                return std::pair<std::string, std::string>{localName, std::string{}};
            }

            (void)XmlConvert::VerifyNCName(prefix);
            (void)XmlConvert::VerifyNCName(localName);
            const std::string namespaceUri = LookupNamespaceUriOnSchemaFrame(frame, prefix);
            if (namespaceUri.empty()) {
                throw XmlException("undefined QName prefix");
            }
            return std::pair<std::string, std::string>{localName, namespaceUri};
        } catch (const XmlException&) {
            throw XmlException(
                "Schema validation failed: xsi:type on element '" + frame.name + "' must be a valid xs:QName value");
        }
    };

    const auto resolveEffectiveElementRule = [&](const SchemaValidationFrame& frame, const XmlSchemaSet::ElementRule& declaration) {
        XmlSchemaSet::ElementRule effectiveRule = declaration;
        const auto* typeAttribute = FindObservedSchemaAttribute(frame.observedAttributes, "type", kSchemaInstanceNamespace);
        if (typeAttribute == nullptr) {
            return effectiveRule;
        }

        const auto [typeLocalName, typeNamespaceUri] = parseSchemaInstanceTypeName(frame, typeAttribute->value);
        const std::string typeDisplayName = typeAttribute->value;

        if (const auto builtinType = resolveBuiltinSimpleType(typeNamespaceUri == "http://www.w3.org/2001/XMLSchema"
                ? "xs:" + typeLocalName
                : std::string{}); builtinType.has_value()) {
            if (declaration.declaredComplexType.has_value()) {
                throw XmlException(
                    "Schema validation failed: xsi:type '" + typeDisplayName + "' on element '" + frame.name
                    + "' must name a type derived from the declared element type");
            }

            bool usesRestriction = false;
            if (declaration.declaredSimpleType.has_value()
                && !runtimeSimpleTypeDerivesFrom(*builtinType, *declaration.declaredSimpleType, usesRestriction)) {
                throw XmlException(
                    "Schema validation failed: xsi:type '" + typeDisplayName + "' on element '" + frame.name
                    + "' must name a type derived from the declared element type");
            }
            if (usesRestriction && declaration.blockRestriction) {
                throw XmlException(
                    "Schema validation failed: xsi:type '" + typeDisplayName + "' on element '" + frame.name
                    + "' uses blocked restriction derivation");
            }

            applySimpleTypeToElementRule(effectiveRule, *builtinType);
            return effectiveRule;
        }

        if (const auto builtinComplexType = resolveBuiltinComplexType(typeNamespaceUri == "http://www.w3.org/2001/XMLSchema"
                ? "xs:" + typeLocalName
                : std::string{}); builtinComplexType.has_value()) {
            if (declaration.declaredSimpleType.has_value()) {
                throw XmlException(
                    "Schema validation failed: xsi:type '" + typeDisplayName + "' on element '" + frame.name
                    + "' must name a type derived from the declared element type");
            }

            bool usesRestriction = false;
            bool usesExtension = false;
            if (declaration.declaredComplexType.has_value()
                && !runtimeComplexTypeDerivesFrom(*builtinComplexType, *declaration.declaredComplexType, usesRestriction, usesExtension)) {
                throw XmlException(
                    "Schema validation failed: xsi:type '" + typeDisplayName + "' on element '" + frame.name
                    + "' must name a type derived from the declared element type");
            }

            effectiveRule.declaredSimpleType.reset();
            effectiveRule.declaredComplexType = *builtinComplexType;
            applyComplexTypeToElementRule(effectiveRule, *builtinComplexType);
            return effectiveRule;
        }

        if (const auto* namedSimpleType = schemas.FindSimpleTypeRule(typeLocalName, typeNamespaceUri); namedSimpleType != nullptr) {
            if (declaration.declaredComplexType.has_value()) {
                throw XmlException(
                    "Schema validation failed: xsi:type '" + typeDisplayName + "' on element '" + frame.name
                    + "' must name a type derived from the declared element type");
            }

            bool usesRestriction = false;
            if (declaration.declaredSimpleType.has_value()
                && !runtimeSimpleTypeDerivesFrom(*namedSimpleType, *declaration.declaredSimpleType, usesRestriction)) {
                throw XmlException(
                    "Schema validation failed: xsi:type '" + typeDisplayName + "' on element '" + frame.name
                    + "' must name a type derived from the declared element type");
            }
            if (usesRestriction && declaration.blockRestriction) {
                throw XmlException(
                    "Schema validation failed: xsi:type '" + typeDisplayName + "' on element '" + frame.name
                    + "' uses blocked restriction derivation");
            }

            applySimpleTypeToElementRule(effectiveRule, *namedSimpleType);
            return effectiveRule;
        }

        if (const auto* namedComplexType = schemas.FindComplexTypeRule(typeLocalName, typeNamespaceUri); namedComplexType != nullptr) {
            if (declaration.declaredSimpleType.has_value()) {
                throw XmlException(
                    "Schema validation failed: xsi:type '" + typeDisplayName + "' on element '" + frame.name
                    + "' must name a type derived from the declared element type");
            }

            bool usesRestriction = false;
            bool usesExtension = false;
            if (declaration.declaredComplexType.has_value()
                && !runtimeComplexTypeDerivesFrom(*namedComplexType, *declaration.declaredComplexType, usesRestriction, usesExtension)) {
                throw XmlException(
                    "Schema validation failed: xsi:type '" + typeDisplayName + "' on element '" + frame.name
                    + "' must name a type derived from the declared element type");
            }
            const bool blockedByBaseType = declaration.declaredComplexType.has_value()
                && ((usesRestriction && declaration.declaredComplexType->blockRestriction)
                    || (usesExtension && declaration.declaredComplexType->blockExtension));
            if ((usesRestriction && declaration.blockRestriction)
                || (usesExtension && declaration.blockExtension)
                || blockedByBaseType) {
                throw XmlException(
                    "Schema validation failed: xsi:type '" + typeDisplayName + "' on element '" + frame.name
                    + "' uses blocked type derivation");
            }

            effectiveRule.declaredSimpleType.reset();
            effectiveRule.declaredComplexType = *namedComplexType;
            applyComplexTypeToElementRule(effectiveRule, *namedComplexType);
            return effectiveRule;
        }

        throw XmlException(
            "Schema validation failed: xsi:type '" + typeDisplayName + "' on element '" + frame.name
            + "' is not a supported schema type");
    };

    if (reader.NodeType() != XmlNodeType::Element) {
        while (reader.Read()) {
            if (reader.NodeType() == XmlNodeType::Element) {
                break;
            }
        }
    }

    if (reader.NodeType() == XmlNodeType::Element) {
        rootRuleNamespaceUri = reader.NamespaceURI();
        if (const auto* rootRule = schemas.FindElementRule(reader.LocalName(), reader.NamespaceURI()); rootRule != nullptr) {
            const SchemaValidationFrame rootFrame = BuildSchemaValidationFrame(reader);
            const XmlSchemaSet::ElementRule effectiveRootRule = resolveEffectiveElementRule(rootFrame, *rootRule);

            std::unordered_set<const XmlSchemaSet::ElementRule*> visitedElementRules;
            std::unordered_set<const XmlSchemaSet::ComplexTypeRule*> visitedComplexTypes;
            if (elementRuleRequiresDom(effectiveRootRule, visitedElementRules, visitedComplexTypes)) {
                if (const auto* stringSource = dynamic_cast<const StringXmlReaderInputSource*>(reader.inputSource_.get());
                    stringSource != nullptr && stringSource->Text() != nullptr) {
                    const auto document = XmlDocument::Parse(*stringSource->Text());
                    document->Validate(schemas);
                    return;
                }
                const auto document = BuildXmlDocumentFromReader(reader);
                document->Validate(schemas);
                return;
            }
        }
    }

    const auto validateLocalFrame = [&](const SchemaValidationFrame& frame) {
        const auto* rule = schemas.FindElementRule(frame.localName, frame.namespaceUri);
        if (rule == nullptr) {
            throw XmlException("Schema validation failed: no schema declaration found for element '" + frame.name + "'");
        }
        if (rootNamespaceUri.empty()) {
            rootNamespaceUri = frame.namespaceUri;
        }
        if (rule->isAbstract) {
            throw XmlException("Schema validation failed: element '" + frame.name + "' is abstract and cannot appear in the document");
        }

        const XmlSchemaSet::ElementRule effectiveRule = resolveEffectiveElementRule(frame, *rule);
        rule = &effectiveRule;

        if (rule->declaredComplexType.has_value() && rule->declaredComplexType->isAbstract) {
            throw XmlException(
                "Schema validation failed: complexType used by element '" + frame.name
                + "' is abstract and cannot be used directly");
        }

        const auto* nilAttribute = FindObservedSchemaAttribute(frame.observedAttributes, "nil", kSchemaInstanceNamespace);
        bool isNilled = false;
        if (nilAttribute != nullptr) {
            if (!TryParseXmlSchemaBoolean(nilAttribute->value, isNilled)) {
                throw XmlException(
                    "Schema validation failed: xsi:nil on element '" + frame.name + "' must be a valid xs:boolean value");
            }
            if (!rule->isNillable) {
                throw XmlException(
                    "Schema validation failed: element '" + frame.name + "' is not declared nillable and cannot use xsi:nil");
            }
        }

        for (const auto& attributeRule : rule->attributes) {
            const std::optional<std::string> effectiveAttributeValue =
                ResolveSchemaAttributeEffectiveValue(attributeRule, frame.name, frame.observedAttributes);

            if (effectiveAttributeValue.has_value() && attributeRule.type.has_value()) {
                ValidateSchemaSimpleValue(
                    *attributeRule.type,
                    *effectiveAttributeValue,
                    "attribute '" + attributeRule.name + "' on element '" + frame.name + "'",
                    frame.namespaceBindings,
                    reader.dtdState_ ? reader.dtdState_->notationDeclarationNames : emptyStringSet,
                    reader.dtdState_ ? reader.dtdState_->unparsedEntityDeclarationNames : emptyStringSet);
                recordIdentityTypedValue(*attributeRule.type, *effectiveAttributeValue,
                    "attribute '" + attributeRule.name + "' on element '" + frame.name + "'", frame.namespaceBindings);
            }
        }

        for (const auto& attribute : frame.observedAttributes) {
            if (attribute.name == "xmlns" || attribute.prefix == "xmlns" || attribute.prefix == "xml") {
                continue;
            }
            if (attribute.localName == "nil" && attribute.namespaceUri == kSchemaInstanceNamespace) {
                continue;
            }
            if (attribute.localName == "type" && attribute.namespaceUri == kSchemaInstanceNamespace) {
                continue;
            }
            const bool declared = std::any_of(rule->attributes.begin(), rule->attributes.end(), [&](const auto& attributeRule) {
                return attribute.localName == attributeRule.name && attribute.namespaceUri == attributeRule.namespaceUri;
            });
            const bool wildcardMatches = rule->anyAttributeAllowed
                && matchesWildcardNamespace(rule->anyAttributeNamespaceConstraint, attribute.namespaceUri);
            const bool allowedByWildcard = wildcardMatches
                && wildcardAllowsUndeclared(rule->anyAttributeProcessContents);
            if (!declared && !allowedByWildcard) {
                throw XmlException(
                    "Schema validation failed: attribute '" + attribute.name
                    + "' is not declared for element '" + frame.name + "'");
            }
        }

        const SchemaElementObservedContent& observedContent = frame.observedContent;
        const auto& elementChildren = observedContent.elementChildren;
        const bool hasSignificantText = observedContent.hasSignificantText;

        if (isNilled && (!elementChildren.empty() || hasSignificantText)) {
            throw XmlException(
                "Schema validation failed: element '" + frame.name + "' is nilled and must not contain text or child elements");
        }

        const std::optional<std::string> effectiveElementValue =
            ResolveSchemaElementEffectiveValue(*rule, frame.name, observedContent, isNilled);

        if (!isNilled) {
            if (rule->particle.has_value()) {
                const auto matches = CollectObservedParticleMatches(
                    *rule->particle,
                    elementChildren,
                    [&](const std::string& localName, const std::string& namespaceUri) {
                        return schemas.FindElementRule(localName, namespaceUri);
                    },
                    elementMatchesDeclaration,
                    matchesWildcardNamespace,
                    wildcardAllowsUndeclared,
                    [](const auto& particle) { return particle.kind == std::decay_t<decltype(particle)>::Kind::Element; },
                    [](const auto& particle) { return particle.kind == std::decay_t<decltype(particle)>::Kind::Any; },
                    [](const auto& particle) { return particle.kind == std::decay_t<decltype(particle)>::Kind::Sequence; },
                    [](const auto& particle) { return particle.kind == std::decay_t<decltype(particle)>::Kind::All; },
                    [](const auto& particle) -> const auto& { return particle.children; },
                    [](const auto& particle) -> const auto& { return particle.name; },
                    [](const auto& particle) -> const auto& { return particle.namespaceUri; },
                    [](const auto& particle) -> const auto& { return particle.processContents; },
                    [](const auto& particle) { return particle.minOccurs; },
                    [](const auto& particle) { return particle.maxOccurs; },
                    [](const auto& childElement) -> const auto& { return childElement.localName; },
                    [](const auto& childElement) -> const auto& { return childElement.namespaceUri; });
                if (std::find(matches.begin(), matches.end(), elementChildren.size()) == matches.end()) {
                    throw XmlException(
                        "Schema validation failed: child element structure under element '" + frame.name + "' does not satisfy the declared content model");
                }
            } else if (!rule->children.empty()) {
                if (rule->contentModel == XmlSchemaSet::ContentModel::Choice) {
                    std::optional<std::size_t> selectedChoiceIndex;
                    std::vector<std::size_t> counts(rule->children.size(), 0);
                    for (const auto& childElement : elementChildren) {
                        const auto match = std::find_if(rule->children.begin(), rule->children.end(), [&](const auto& childRule) {
                            const auto* declaredRule = schemas.FindElementRule(childRule.name, childRule.namespaceUri);
                            return declaredRule != nullptr && elementMatchesDeclaration(childElement, *declaredRule);
                        });
                        if (match == rule->children.end()) {
                            throw XmlException(
                                "Schema validation failed: unexpected child element '" + childElement.name
                                + "' under element '" + frame.name + "'");
                        }

                        const std::size_t choiceIndex = static_cast<std::size_t>(std::distance(rule->children.begin(), match));
                        if (selectedChoiceIndex.has_value() && selectedChoiceIndex.value() != choiceIndex) {
                            throw XmlException(
                                "Schema validation failed: element '" + frame.name + "' allows only one choice branch at a time");
                        }
                        selectedChoiceIndex = choiceIndex;

                        const auto& choiceRule = rule->children[choiceIndex];
                        ++counts[choiceIndex];
                        if (choiceRule.maxOccurs != std::numeric_limits<std::size_t>::max()
                            && counts[choiceIndex] > choiceRule.maxOccurs) {
                            throw XmlException(
                                "Schema validation failed: child element '" + choiceRule.name
                                + "' exceeded maxOccurs under element '" + frame.name + "'");
                        }
                    }

                    const auto requiredChoice = std::find_if(rule->children.begin(), rule->children.end(), [](const auto& childRule) {
                        return childRule.minOccurs > 0;
                    });
                    if (!selectedChoiceIndex.has_value()) {
                        if (requiredChoice != rule->children.end()) {
                            throw XmlException(
                                "Schema validation failed: element '" + frame.name + "' requires one of the declared choice elements");
                        }
                    } else {
                        const auto& selectedRule = rule->children[selectedChoiceIndex.value()];
                        if (counts[selectedChoiceIndex.value()] < selectedRule.minOccurs) {
                            throw XmlException(
                                "Schema validation failed: child element '" + selectedRule.name
                                + "' does not satisfy minOccurs under element '" + frame.name + "'");
                        }
                    }
                } else {
                    std::size_t childIndex = 0;
                    for (const auto& childRule : rule->children) {
                        std::size_t count = 0;
                        const auto* declaredRule = schemas.FindElementRule(childRule.name, childRule.namespaceUri);
                        if (declaredRule == nullptr) {
                            throw XmlException("Schema validation failed: no schema declaration found for element '" + childRule.name + "'");
                        }
                        while (childIndex < elementChildren.size()) {
                            const auto& childElement = elementChildren[childIndex];
                            if (!elementMatchesDeclaration(childElement, *declaredRule)) {
                                break;
                            }
                            ++count;
                            ++childIndex;
                            if (childRule.maxOccurs != std::numeric_limits<std::size_t>::max() && count == childRule.maxOccurs) {
                                break;
                            }
                        }
                        if (count < childRule.minOccurs) {
                            throw XmlException(
                                "Schema validation failed: expected child element '" + childRule.name
                                + "' under element '" + frame.name + "'");
                        }
                    }
                    if (childIndex != elementChildren.size()) {
                        throw XmlException(
                            "Schema validation failed: unexpected child element '" + elementChildren[childIndex].name
                            + "' under element '" + frame.name + "'");
                    }
                }
            } else if (!elementChildren.empty()) {
                throw XmlException(
                    "Schema validation failed: element '" + frame.name + "' does not allow child elements");
            }

            if (!rule->allowsText && hasSignificantText) {
                throw XmlException(
                    "Schema validation failed: element '" + frame.name + "' does not allow text content");
            }
            if (rule->textType.has_value() && effectiveElementValue.has_value()) {
                ValidateSchemaSimpleValue(
                    *rule->textType,
                    *effectiveElementValue,
                    "text content of element '" + frame.name + "'",
                    frame.namespaceBindings,
                    reader.dtdState_ ? reader.dtdState_->notationDeclarationNames : emptyStringSet,
                    reader.dtdState_ ? reader.dtdState_->unparsedEntityDeclarationNames : emptyStringSet);
                recordIdentityTypedValue(*rule->textType, *effectiveElementValue,
                    "text content of element '" + frame.name + "'", frame.namespaceBindings);
            }
        }
    };

    std::vector<SchemaValidationFrame> frameStack;
    const auto processCurrentReaderNode = [&]() {
        switch (reader.NodeType()) {
        case XmlNodeType::Element: {
            const auto parentBindings = frameStack.empty() ? std::vector<std::pair<std::string, std::string>>{} : frameStack.back().namespaceBindings;
            const auto* declaredRule = schemas.FindElementRule(reader.LocalName(), reader.NamespaceURI());
            SchemaValidationFrame frame = BuildSchemaValidationFrame(reader, parentBindings, false);
            std::shared_ptr<XmlSchemaSet::ElementRule> effectiveRule;
            if (declaredRule != nullptr) {
                effectiveRule = std::make_shared<XmlSchemaSet::ElementRule>(resolveEffectiveElementRule(frame, *declaredRule));
            }
            const bool captureIdentitySnapshot = !identityScopeStack.empty()
                || (effectiveRule != nullptr && !effectiveRule->identityConstraints.empty());
            if (captureIdentitySnapshot) {
                frame.identitySnapshot = std::make_shared<SchemaIdentityNodeSnapshot>();
                frame.identitySnapshot->name = frame.name;
                frame.identitySnapshot->localName = frame.localName;
                frame.identitySnapshot->namespaceUri = frame.namespaceUri;
                frame.identitySnapshot->attributes = frame.observedAttributes;
            }
            frameStack.push_back(std::move(frame));
            if (effectiveRule != nullptr && !effectiveRule->identityConstraints.empty()) {
                std::unordered_set<std::string> declaredConstraintKeys;
                for (const auto& constraint : effectiveRule->identityConstraints) {
                    if (constraint.kind != XmlSchemaSet::ElementRule::IdentityConstraint::Kind::KeyRef) {
                        declaredConstraintKeys.insert(BuildIdentityConstraintLookupKey(constraint.name, constraint.namespaceUri));
                    }
                }
                identityScopeStack.push_back(ReaderIdentityScope{
                    frameStack.back().depth,
                    std::move(effectiveRule),
                    frameStack.back().identitySnapshot,
                    {},
                    {},
                    std::move(declaredConstraintKeys),
                    {}});
            }
            if (frameStack.back().isEmptyElement) {
                validateLocalFrame(frameStack.back());
                finalizeReaderIdentityScope(frameStack.back());
                SchemaValidationFrame completed = std::move(frameStack.back());
                frameStack.pop_back();
                if (!frameStack.empty()) {
                    AppendObservedSchemaChildFrame(frameStack.back(), completed);
                }
            }
            break;
        }
        case XmlNodeType::Text:
        case XmlNodeType::CDATA:
        case XmlNodeType::Whitespace:
        case XmlNodeType::SignificantWhitespace:
            if (!frameStack.empty()) {
                AppendSchemaTextToFrame(frameStack.back(), reader.Value());
            }
            break;
        case XmlNodeType::EndElement:
            if (frameStack.empty()) {
                throw XmlException("Schema validation failed: unexpected end element while streaming validation");
            }
            validateLocalFrame(frameStack.back());
            finalizeReaderIdentityScope(frameStack.back());
            {
                SchemaValidationFrame completed = std::move(frameStack.back());
                frameStack.pop_back();
                if (!frameStack.empty()) {
                    AppendObservedSchemaChildFrame(frameStack.back(), completed);
                }
            }
            break;
        default:
            break;
        }
    };

    if (reader.GetReadState() == ReadState::Interactive && reader.NodeType() != XmlNodeType::None) {
        processCurrentReaderNode();
    }

    while (reader.Read()) {
        processCurrentReaderNode();
    }

    if (!frameStack.empty()) {
        throw XmlException("Schema validation failed: unexpected end of input during streaming validation");
    }
    if (!identityScopeStack.empty()) {
        throw XmlException("Schema validation failed: unexpected end of input during identity constraint validation");
    }

    ValidatePendingSchemaIdReferences(identityState);
}

void ValidateXmlReaderInputAgainstSchemas(const std::shared_ptr<const std::string>& xml, const XmlReaderSettings& settings) {
    if (settings.Validation != ValidationType::Schema) {
        return;
    }
    if (settings.Conformance != ConformanceLevel::Document) {
        throw XmlException("Schema validation is only supported for document conformance mode");
    }
    if (settings.Schemas == nullptr || settings.Schemas->Count() == 0) {
        throw XmlException("Schema validation requires XmlReaderSettings.Schemas to contain at least one schema");
    }

    XmlReaderSettings validationSettings = settings;
    validationSettings.Validation = ValidationType::None;
    auto reader = XmlReader::CreateFromValidatedString(xml, validationSettings);
    XmlDocument validationDocument;
    validationDocument.ValidateReaderAgainstSchemas(reader, *settings.Schemas);
}

void ValidateXmlReaderInputAgainstSchemas(XmlReader& reader, const XmlReaderSettings& settings) {
    if (settings.Validation != ValidationType::Schema) {
        return;
    }
    if (settings.Conformance != ConformanceLevel::Document) {
        throw XmlException("Schema validation is only supported for document conformance mode");
    }
    if (settings.Schemas == nullptr || settings.Schemas->Count() == 0) {
        throw XmlException("Schema validation requires XmlReaderSettings.Schemas to contain at least one schema");
    }

    XmlDocument validationDocument;
    validationDocument.ValidateReaderAgainstSchemas(reader, *settings.Schemas);
}

}  // namespace System::Xml
