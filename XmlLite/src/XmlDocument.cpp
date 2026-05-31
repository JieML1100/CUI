#include "XmlDocument.h"

#include "XmlConvert.h"
#include "XmlWriter.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace System::Xml {

namespace {

std::string ReadStreamFully(std::istream& stream) {
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

std::string ReadFileFully(const std::string& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw XmlException("Could not open XML file '" + path + "'");
    }
    return ReadStreamFully(stream);
}

std::string Trim(std::string_view value) {
    const auto first = std::find_if_not(value.begin(), value.end(), [](char ch) {
        return XmlConvert::IsWhitespaceChar(ch);
    });
    if (first == value.end()) {
        return {};
    }

    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](char ch) {
        return XmlConvert::IsWhitespaceChar(ch);
    }).base();
    return std::string(first, last);
}

std::string ComposeQualifiedName(std::string_view prefix, std::string_view localName) {
    if (prefix.empty()) {
        return std::string(localName);
    }
    return std::string(prefix) + ":" + std::string(localName);
}

bool IsWhitespaceOnly(std::string_view value) {
    return std::all_of(value.begin(), value.end(), [](char ch) {
        return XmlConvert::IsWhitespaceChar(ch);
    });
}

void AppendUtf8(std::string& output, unsigned int codePoint) {
    if (codePoint <= 0x7F) {
        output.push_back(static_cast<char>(codePoint));
    } else if (codePoint <= 0x7FF) {
        output.push_back(static_cast<char>(0xC0 | ((codePoint >> 6) & 0x1F)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else if (codePoint <= 0xFFFF) {
        output.push_back(static_cast<char>(0xE0 | ((codePoint >> 12) & 0x0F)));
        output.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else if (codePoint <= 0x10FFFF) {
        output.push_back(static_cast<char>(0xF0 | ((codePoint >> 18) & 0x07)));
        output.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else {
        throw XmlException("Invalid XML character reference");
    }
}

class XmlLiteParser final {
public:
    XmlLiteParser(std::string_view xml, XmlDocument& document, XmlReaderSettings settings)
        : xml_(xml), document_(document), settings_(std::move(settings)) {
    }

    void ParseDocument() {
        if (settings_.MaxCharactersInDocument != 0 && xml_.size() > settings_.MaxCharactersInDocument) {
            Throw("The XML document exceeds XmlReaderSettings.MaxCharactersInDocument");
        }

        if (StartsWith("\xEF\xBB\xBF")) {
            Advance(3);
        }

        while (!End()) {
            if (Peek() == '<') {
                if (StartsWith("</")) {
                    Throw("Unexpected end element");
                }
                ParseNode(document_);
                continue;
            }

            const std::string text = ParseTextValue();
            if (!IsWhitespaceOnly(text) && settings_.Conformance != ConformanceLevel::Fragment) {
                Throw("Data at the root level is invalid");
            }
            if (document_.PreserveWhitespace() && !text.empty()) {
                document_.AppendChild(document_.CreateWhitespace(text));
            }
        }

        if (settings_.Conformance == ConformanceLevel::Document && document_.DocumentElement() == nullptr) {
            Throw("Root element is missing");
        }
    }

private:
    bool End() const noexcept {
        return position_ >= xml_.size();
    }

    char Peek() const noexcept {
        return End() ? '\0' : xml_[position_];
    }

    bool StartsWith(std::string_view token) const noexcept {
        return xml_.substr(position_, token.size()) == token;
    }

    void Advance(std::size_t count = 1) {
        for (std::size_t i = 0; i < count && position_ < xml_.size(); ++i) {
            const char ch = xml_[position_++];
            if (ch == '\n') {
                ++line_;
                column_ = 1;
            } else {
                ++column_;
            }
        }
    }

    void Expect(char ch) {
        if (Peek() != ch) {
            Throw(std::string("Expected '") + ch + "'");
        }
        Advance();
    }

    void Throw(const std::string& message) const {
        throw XmlException(message, line_, column_);
    }

    void SkipWhitespace() {
        while (!End() && XmlConvert::IsWhitespaceChar(Peek())) {
            Advance();
        }
    }

    std::string ParseName() {
        if (End() || !XmlConvert::IsStartNameChar(Peek())) {
            Throw("Expected an XML name");
        }

        const std::size_t start = position_;
        Advance();
        while (!End() && XmlConvert::IsNameChar(Peek())) {
            Advance();
        }

        return XmlConvert::VerifyName(xml_.substr(start, position_ - start));
    }

    std::string ParseQuotedValue(bool attribute) {
        const char quote = Peek();
        if (quote != '\'' && quote != '"') {
            Throw("Expected a quoted XML value");
        }
        Advance();

        const std::size_t start = position_;
        while (!End() && Peek() != quote) {
            if (Peek() == '<' && attribute) {
                Throw("The '<' character is not allowed in attribute values");
            }
            Advance();
        }

        if (End()) {
            Throw("Unterminated quoted XML value");
        }

        const std::string value = DecodeEntities(xml_.substr(start, position_ - start));
        Advance();
        return value;
    }

    std::string ParseTextValue() {
        const std::size_t start = position_;
        while (!End() && Peek() != '<') {
            Advance();
        }
        return DecodeEntities(xml_.substr(start, position_ - start));
    }

    std::string DecodeEntities(std::string_view value) const {
        std::string decoded;
        decoded.reserve(value.size());

        for (std::size_t index = 0; index < value.size(); ++index) {
            if (value[index] != '&') {
                decoded.push_back(value[index]);
                continue;
            }

            const auto semicolon = value.find(';', index + 1);
            if (semicolon == std::string_view::npos) {
                throw XmlException("Unterminated XML entity reference", line_, column_);
            }

            const std::string_view entity = value.substr(index + 1, semicolon - index - 1);
            if (entity == "lt") {
                decoded.push_back('<');
            } else if (entity == "gt") {
                decoded.push_back('>');
            } else if (entity == "amp") {
                decoded.push_back('&');
            } else if (entity == "apos") {
                decoded.push_back('\'');
            } else if (entity == "quot") {
                decoded.push_back('"');
            } else if (!entity.empty() && entity.front() == '#') {
                unsigned int codePoint = 0;
                if (entity.size() > 1 && (entity[1] == 'x' || entity[1] == 'X')) {
                    for (char ch : entity.substr(2)) {
                        const int digit = ch >= '0' && ch <= '9' ? ch - '0'
                            : ch >= 'a' && ch <= 'f' ? ch - 'a' + 10
                            : ch >= 'A' && ch <= 'F' ? ch - 'A' + 10
                            : -1;
                        if (digit < 0) {
                            throw XmlException("Invalid XML character reference", line_, column_);
                        }
                        codePoint = (codePoint << 4) | static_cast<unsigned int>(digit);
                    }
                } else {
                    for (char ch : entity.substr(1)) {
                        if (!std::isdigit(static_cast<unsigned char>(ch))) {
                            throw XmlException("Invalid XML character reference", line_, column_);
                        }
                        codePoint = (codePoint * 10) + static_cast<unsigned int>(ch - '0');
                    }
                }
                AppendUtf8(decoded, codePoint);
            } else {
                throw XmlException("Unknown XML entity reference '&" + std::string(entity) + ";'", line_, column_);
            }

            index = semicolon;
        }

        return decoded;
    }

    void ParseNode(XmlNode& parent) {
        if (StartsWith("<!--")) {
            ParseComment(parent);
        } else if (StartsWith("<![CDATA[")) {
            ParseCData(parent);
        } else if (StartsWith("<?")) {
            ParseProcessingInstruction(parent);
        } else if (StartsWith("<!DOCTYPE")) {
            ParseDocumentType(parent);
        } else if (StartsWith("<!")) {
            Throw("Unsupported XML declaration");
        } else {
            ParseElement(parent);
        }
    }

    void ParseComment(XmlNode& parent) {
        Advance(4);
        const std::size_t start = position_;
        const auto end = xml_.find("-->", position_);
        if (end == std::string_view::npos) {
            Throw("Unterminated XML comment");
        }
        Advance(end - position_);
        Advance(3);

        if (!settings_.IgnoreComments) {
            parent.AppendChild(document_.CreateComment(xml_.substr(start, end - start)));
        }
    }

    void ParseCData(XmlNode& parent) {
        Advance(9);
        const std::size_t start = position_;
        const auto end = xml_.find("]]>", position_);
        if (end == std::string_view::npos) {
            Throw("Unterminated CDATA section");
        }
        Advance(end - position_);
        Advance(3);
        parent.AppendChild(document_.CreateCDataSection(xml_.substr(start, end - start)));
    }

    void ParseProcessingInstruction(XmlNode& parent) {
        Advance(2);
        const std::string target = ParseName();
        const bool declaration = target == "xml";

        std::string version = "1.0";
        std::string encoding;
        std::string standalone;
        std::string data;

        if (declaration) {
            if (&parent != &document_ || !document_.ChildNodes().empty()) {
                Throw("XML declaration must be the first node in the document");
            }

            while (true) {
                SkipWhitespace();
                if (StartsWith("?>")) {
                    break;
                }

                const std::string name = ParseName();
                SkipWhitespace();
                Expect('=');
                SkipWhitespace();
                const std::string value = ParseQuotedValue(true);
                if (name == "version") {
                    version = value;
                } else if (name == "encoding") {
                    encoding = value;
                } else if (name == "standalone") {
                    standalone = value;
                }
            }
        } else {
            const std::size_t start = position_;
            const auto end = xml_.find("?>", position_);
            if (end == std::string_view::npos) {
                Throw("Unterminated processing instruction");
            }
            data = Trim(xml_.substr(start, end - start));
            Advance(end - position_);
        }

        if (!StartsWith("?>")) {
            Throw("Unterminated processing instruction");
        }
        Advance(2);

        if (declaration) {
            parent.AppendChild(document_.CreateXmlDeclaration(version, encoding, standalone));
        } else if (!settings_.IgnoreProcessingInstructions) {
            parent.AppendChild(document_.CreateProcessingInstruction(target, data));
        }
    }

    void ParseDocumentType(XmlNode& parent) {
        if (settings_.DtdProcessing == DtdProcessing::Prohibit) {
            Throw("DTD processing is prohibited");
        }

        Advance(9);
        SkipWhitespace();
        const std::string name = ParseName();
        const std::size_t tailStart = position_;

        bool quoted = false;
        char quote = '\0';
        int subsetDepth = 0;
        while (!End()) {
            const char ch = Peek();
            if (quoted) {
                if (ch == quote) {
                    quoted = false;
                }
                Advance();
                continue;
            }

            if (ch == '\'' || ch == '"') {
                quoted = true;
                quote = ch;
                Advance();
            } else if (ch == '[') {
                ++subsetDepth;
                Advance();
            } else if (ch == ']') {
                --subsetDepth;
                Advance();
            } else if (ch == '>' && subsetDepth <= 0) {
                break;
            } else {
                Advance();
            }
        }

        if (End()) {
            Throw("Unterminated document type declaration");
        }

        std::string internalSubset = Trim(xml_.substr(tailStart, position_ - tailStart));
        Advance();

        if (settings_.DtdProcessing != DtdProcessing::Ignore) {
            parent.AppendChild(document_.CreateDocumentType(name, {}, {}, internalSubset));
        }
    }

    void ParseElement(XmlNode& parent) {
        Expect('<');
        const std::string name = ParseName();
        auto element = document_.CreateElement(name);

        while (true) {
            SkipWhitespace();
            if (StartsWith("/>")) {
                Advance(2);
                parent.AppendChild(element);
                return;
            }
            if (StartsWith(">")) {
                Advance();
                parent.AppendChild(element);
                break;
            }

            const std::string attributeName = ParseName();
            if (element->HasAttribute(attributeName)) {
                Throw("Duplicate attribute '" + attributeName + "'");
            }
            SkipWhitespace();
            Expect('=');
            SkipWhitespace();
            element->SetAttribute(attributeName, ParseQuotedValue(true));
        }

        while (!End()) {
            if (StartsWith("</")) {
                Advance(2);
                const std::string endName = ParseName();
                if (endName != name) {
                    Throw("Mismatched end element. Expected '" + name + "' but found '" + endName + "'");
                }
                SkipWhitespace();
                Expect('>');
                element->SetWritesFullEndElement(true);
                return;
            }

            if (Peek() == '<') {
                ParseNode(*element);
                continue;
            }

            const std::string text = ParseTextValue();
            if (text.empty()) {
                continue;
            }

            const bool whitespace = IsWhitespaceOnly(text);
            if (whitespace) {
                if (document_.PreserveWhitespace() && !settings_.IgnoreWhitespace) {
                    element->AppendChild(document_.CreateWhitespace(text));
                }
            } else {
                element->AppendChild(document_.CreateTextNode(text));
            }
        }

        Throw("Unexpected end of XML input");
    }

    std::string_view xml_;
    XmlDocument& document_;
    XmlReaderSettings settings_;
    std::size_t position_ = 0;
    std::size_t line_ = 1;
    std::size_t column_ = 1;
};

}  // namespace

XmlDocument::XmlDocument()
    : XmlNode(XmlNodeType::Document, "#document") {
    SetOwnerDocument(this);
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
    LoadXml(xml, XmlReaderSettings{});
}

void XmlDocument::LoadXml(std::string_view xml, const XmlReaderSettings& settings) {
    RemoveAll();
    XmlLiteParser parser(xml, *this, settings);
    parser.ParseDocument();
}

void XmlDocument::Load(const std::string& path) {
    LoadXml(ReadFileFully(path));
}

void XmlDocument::Load(const std::string& path, const XmlReaderSettings& settings) {
    LoadXml(ReadFileFully(path), settings);
}

void XmlDocument::Load(std::istream& stream) {
    LoadXml(ReadStreamFully(stream));
}

void XmlDocument::Load(std::istream& stream, const XmlReaderSettings& settings) {
    LoadXml(ReadStreamFully(stream), settings);
}

void XmlDocument::Save(const std::string& path, const XmlWriterSettings& settings) const {
    XmlWriter::WriteToFile(*this, path, settings);
}

void XmlDocument::Save(std::ostream& stream, const XmlWriterSettings& settings) const {
    XmlWriter::WriteToStream(*this, stream, settings);
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
    const auto root = DocumentElement();
    if (root == nullptr) {
        return {};
    }

    auto result = root->GetElementsByTagName(name);
    if (name == "*" || root->Name() == name) {
        result.insert(result.begin(), root);
    }
    return result;
}

XmlNodeList XmlDocument::GetElementsByTagNameList(std::string_view name) const {
    std::vector<std::shared_ptr<XmlNode>> nodes;
    for (auto& element : GetElementsByTagName(name)) {
        nodes.push_back(element);
    }
    return XmlNodeList(std::move(nodes));
}

std::vector<std::shared_ptr<XmlElement>> XmlDocument::GetElementsByTagName(std::string_view localName, std::string_view namespaceUri) const {
    const auto root = DocumentElement();
    if (root == nullptr) {
        return {};
    }

    auto result = root->GetElementsByTagName(localName, namespaceUri);
    if ((localName == "*" || root->LocalName() == localName)
        && (namespaceUri == "*" || root->NamespaceURI() == namespaceUri)) {
        result.insert(result.begin(), root);
    }
    return result;
}

XmlNodeList XmlDocument::GetElementsByTagNameList(std::string_view localName, std::string_view namespaceUri) const {
    std::vector<std::shared_ptr<XmlNode>> nodes;
    for (auto& element : GetElementsByTagName(localName, namespaceUri)) {
        nodes.push_back(element);
    }
    return XmlNodeList(std::move(nodes));
}

std::shared_ptr<XmlDocumentFragment> XmlDocument::CreateDocumentFragment() const {
    auto node = std::make_shared<XmlDocumentFragment>();
    node->SetOwnerDocument(const_cast<XmlDocument*>(this));
    return node;
}

std::shared_ptr<XmlElement> XmlDocument::CreateElement(std::string_view name) const {
    auto node = std::make_shared<XmlElement>(std::string(name));
    node->SetOwnerDocument(const_cast<XmlDocument*>(this));
    return node;
}

std::shared_ptr<XmlElement> XmlDocument::CreateElement(std::string_view prefix, std::string_view localName, std::string_view namespaceUri) const {
    auto element = CreateElement(ComposeQualifiedName(prefix, localName));
    if (!namespaceUri.empty()) {
        element->SetAttribute(prefix.empty() ? "xmlns" : "xmlns:" + std::string(prefix), namespaceUri);
    }
    return element;
}

std::shared_ptr<XmlAttribute> XmlDocument::CreateAttribute(std::string_view name, std::string_view value) const {
    auto node = std::make_shared<XmlAttribute>(std::string(name), std::string(value));
    node->SetOwnerDocument(const_cast<XmlDocument*>(this));
    return node;
}

std::shared_ptr<XmlAttribute> XmlDocument::CreateAttribute(
    std::string_view prefix,
    std::string_view localName,
    std::string_view,
    std::string_view value) const {
    return CreateAttribute(ComposeQualifiedName(prefix, localName), value);
}

std::shared_ptr<XmlText> XmlDocument::CreateTextNode(std::string_view value) const {
    auto node = std::make_shared<XmlText>(std::string(value));
    node->SetOwnerDocument(const_cast<XmlDocument*>(this));
    return node;
}

std::shared_ptr<XmlEntityReference> XmlDocument::CreateEntityReference(std::string_view name) const {
    auto node = std::make_shared<XmlEntityReference>(std::string(name));
    node->SetOwnerDocument(const_cast<XmlDocument*>(this));
    return node;
}

std::shared_ptr<XmlWhitespace> XmlDocument::CreateWhitespace(std::string_view value) const {
    auto node = std::make_shared<XmlWhitespace>(std::string(value));
    node->SetOwnerDocument(const_cast<XmlDocument*>(this));
    return node;
}

std::shared_ptr<XmlSignificantWhitespace> XmlDocument::CreateSignificantWhitespace(std::string_view value) const {
    auto node = std::make_shared<XmlSignificantWhitespace>(std::string(value));
    node->SetOwnerDocument(const_cast<XmlDocument*>(this));
    return node;
}

std::shared_ptr<XmlCDataSection> XmlDocument::CreateCDataSection(std::string_view value) const {
    auto node = std::make_shared<XmlCDataSection>(std::string(value));
    node->SetOwnerDocument(const_cast<XmlDocument*>(this));
    return node;
}

std::shared_ptr<XmlComment> XmlDocument::CreateComment(std::string_view value) const {
    auto node = std::make_shared<XmlComment>(std::string(value));
    node->SetOwnerDocument(const_cast<XmlDocument*>(this));
    return node;
}

std::shared_ptr<XmlProcessingInstruction> XmlDocument::CreateProcessingInstruction(std::string_view target, std::string_view data) const {
    auto node = std::make_shared<XmlProcessingInstruction>(std::string(target), std::string(data));
    node->SetOwnerDocument(const_cast<XmlDocument*>(this));
    return node;
}

std::shared_ptr<XmlDeclaration> XmlDocument::CreateXmlDeclaration(
    std::string_view version,
    std::string_view encoding,
    std::string_view standalone) const {
    auto node = std::make_shared<XmlDeclaration>(std::string(version), std::string(encoding), std::string(standalone));
    node->SetOwnerDocument(const_cast<XmlDocument*>(this));
    return node;
}

std::shared_ptr<XmlDocumentType> XmlDocument::CreateDocumentType(
    std::string_view name,
    std::string_view publicId,
    std::string_view systemId,
    std::string_view internalSubset) const {
    auto node = std::make_shared<XmlDocumentType>(
        std::string(name),
        std::string(publicId),
        std::string(systemId),
        std::string(internalSubset));
    node->SetOwnerDocument(const_cast<XmlDocument*>(this));
    return node;
}

std::shared_ptr<XmlNode> XmlDocument::CreateNode(XmlNodeType nodeType, std::string_view name, std::string_view value) const {
    switch (nodeType) {
    case XmlNodeType::Element:
        return CreateElement(name);
    case XmlNodeType::Attribute:
        return CreateAttribute(name, value);
    case XmlNodeType::Text:
        return CreateTextNode(value);
    case XmlNodeType::CDATA:
        return CreateCDataSection(value);
    case XmlNodeType::Comment:
        return CreateComment(value);
    case XmlNodeType::ProcessingInstruction:
        return CreateProcessingInstruction(name, value);
    case XmlNodeType::XmlDeclaration:
        return CreateXmlDeclaration(value.empty() ? "1.0" : value);
    case XmlNodeType::DocumentType:
        return CreateDocumentType(name);
    case XmlNodeType::DocumentFragment:
        return CreateDocumentFragment();
    case XmlNodeType::Whitespace:
        return CreateWhitespace(value);
    case XmlNodeType::SignificantWhitespace:
        return CreateSignificantWhitespace(value);
    case XmlNodeType::EntityReference:
        return CreateEntityReference(name);
    default:
        throw XmlException("Unsupported node type for XmlDocument::CreateNode");
    }
}

std::shared_ptr<XmlNode> XmlDocument::ImportNode(const XmlNode& node, bool deep) const {
    auto imported = node.CloneNode(deep);
    imported->SetOwnerDocumentRecursive(const_cast<XmlDocument*>(this));
    return imported;
}

std::shared_ptr<XmlNode> XmlDocument::AppendChild(const std::shared_ptr<XmlNode>& child) {
    ValidateChildInsertion(child, nullptr);
    return XmlNode::AppendChild(child);
}

void XmlDocument::ValidateChildInsertion(const std::shared_ptr<XmlNode>& child, const XmlNode* replacingChild) const {
    XmlNode::ValidateChildInsertion(child, replacingChild);

    switch (child->NodeType()) {
    case XmlNodeType::XmlDeclaration:
        if (Declaration() != nullptr && Declaration().get() != replacingChild) {
            throw XmlException("The document already has an XML declaration");
        }
        if (!ChildNodes().empty() && ChildNodes().front().get() != replacingChild) {
            throw XmlException("XML declaration must be the first node in the document");
        }
        break;
    case XmlNodeType::DocumentType:
        if (DocumentType() != nullptr && DocumentType().get() != replacingChild) {
            throw XmlException("The document already has a document type declaration");
        }
        if (DocumentElement() != nullptr && DocumentElement().get() != replacingChild) {
            throw XmlException("Document type declaration must appear before the root element");
        }
        break;
    case XmlNodeType::Element:
        if (DocumentElement() != nullptr && DocumentElement().get() != replacingChild) {
            throw XmlException("The document already has a root element");
        }
        break;
    case XmlNodeType::Comment:
    case XmlNodeType::ProcessingInstruction:
    case XmlNodeType::Whitespace:
    case XmlNodeType::SignificantWhitespace:
        break;
    default:
        throw XmlException("This node type cannot be inserted at the document level");
    }
}

}  // namespace System::Xml
