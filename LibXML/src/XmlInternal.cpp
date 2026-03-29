#include "../include/XmlReaderInternal.h"

namespace System::Xml {

std::string BuildExceptionMessage(const std::string& message, std::size_t line, std::size_t column) {
    if (line == 0 || column == 0) {
        return message;
    }

    std::ostringstream stream;
    stream << message << " (line " << line << ", column " << column << ')';
    return stream.str();
}

std::string ComposeQualifiedName(std::string_view prefix, std::string_view localName) {
    if (prefix.empty()) {
        return std::string(localName);
    }

    std::string result;
    result.reserve(prefix.size() + 1 + localName.size());
    result.append(prefix);
    result.push_back(':');
    result.append(localName);
    return result;
}

std::string ResolveNodeNamespaceUri(const XmlNode& node) {
    if (node.NodeType() == XmlNodeType::Attribute) {
        const auto prefix = SplitQualifiedNameView(node.Name()).first;
        if (node.Name() == "xmlns" || prefix == "xmlns") {
            return "http://www.w3.org/2000/xmlns/";
        }
        if (prefix.empty()) {
            return {};
        }
        const auto* parent = node.ParentNode();
        if (parent != nullptr && parent->NodeType() == XmlNodeType::Element) {
            return LookupNamespaceUriOnElement(static_cast<const XmlElement*>(parent), prefix);
        }
        return prefix == "xml" ? "http://www.w3.org/XML/1998/namespace" : std::string{};
    }

    if (node.NodeType() == XmlNodeType::Element) {
        const auto* element = static_cast<const XmlElement*>(&node);
        return LookupNamespaceUriOnElement(element, SplitQualifiedNameView(node.Name()).first);
    }

    return {};
}

const std::string& EmptyString() {
    static const std::string empty;
    return empty;
}

void ValidateXmlDeclarationVersion(std::string_view version) {
    if (version.size() < 3 || version[0] != '1' || version[1] != '.') {
        throw XmlException("Malformed XML declaration");
    }

    for (std::size_t index = 2; index < version.size(); ++index) {
        if (version[index] < '0' || version[index] > '9') {
            throw XmlException("Malformed XML declaration");
        }
    }
}

void ValidateXmlDeclarationEncoding(std::string_view encoding) {
    if (encoding.empty()) {
        return;
    }

    const auto isAsciiAlpha = [](char ch) {
        return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
    };
    const auto isAsciiDigit = [](char ch) {
        return ch >= '0' && ch <= '9';
    };

    if (!isAsciiAlpha(encoding.front())) {
        throw XmlException("Malformed XML declaration");
    }

    for (const char ch : encoding) {
        if (isAsciiAlpha(ch) || isAsciiDigit(ch) || ch == '.' || ch == '_' || ch == '-') {
            continue;
        }
        throw XmlException("Malformed XML declaration");
    }
}

void ValidateXmlDeclarationStandalone(std::string_view standalone) {
    if (!standalone.empty() && standalone != "yes" && standalone != "no") {
        throw XmlException("Malformed XML declaration");
    }
}

std::string BuildDeclarationValue(
    const std::string& version,
    const std::string& encoding,
    const std::string& standalone) {
    std::string value = "version=\"" + version + "\"";
    if (!encoding.empty()) {
        value += " encoding=\"" + encoding + "\"";
    }
    if (!standalone.empty()) {
        value += " standalone=\"" + standalone + "\"";
    }
    return value;
}

std::string BuildDeclarationValue(const XmlDeclaration& declaration) {
    return BuildDeclarationValue(declaration.Version(), declaration.Encoding(), declaration.Standalone());
}

StringXmlReaderInputSource::StringXmlReaderInputSource(std::shared_ptr<const std::string> text)
    : text_(std::move(text)) {
}

const std::shared_ptr<const std::string>& StringXmlReaderInputSource::Text() const noexcept {
    return text_;
}

char StringXmlReaderInputSource::CharAt(std::size_t position) const noexcept {
    return position < text_->size() ? (*text_)[position] : '\0';
}

const char* StringXmlReaderInputSource::PtrAt(std::size_t position, std::size_t& available) const noexcept {
    available = 0;
    if (text_ == nullptr || position >= text_->size()) {
        return nullptr;
    }

    available = text_->size() - position;
    return text_->data() + static_cast<std::ptrdiff_t>(position);
}

std::size_t StringXmlReaderInputSource::Find(std::string_view token, std::size_t position) const noexcept {
    if (token.size() == 1) {
        return text_->find(token.front(), position);
    }
    return text_->find(token, position);
}

std::size_t StringXmlReaderInputSource::FindFirstOf(std::string_view tokens, std::size_t position) const noexcept {
    if (tokens.empty()) {
        return position;
    }
    if (text_ == nullptr || position >= text_->size()) {
        return std::string::npos;
    }

    if (tokens == "<&") {
        const std::size_t offset = FindTextSpecialInBuffer(text_->data() + static_cast<std::ptrdiff_t>(position), text_->size() - position);
        return offset == std::string::npos ? std::string::npos : position + offset;
    }

    if (tokens == "\n<&") {
        const std::size_t offset = FindTextLineSpecialInBuffer(text_->data() + static_cast<std::ptrdiff_t>(position), text_->size() - position);
        return offset == std::string::npos ? std::string::npos : position + offset;
    }

    return text_->find_first_of(tokens, position);
}

std::size_t StringXmlReaderInputSource::ScanQuotedValueEnd(std::size_t quoteStart) const noexcept {
    if (text_ == nullptr || quoteStart >= text_->size()) {
        return std::string::npos;
    }

    const char quote = (*text_)[quoteStart];
    if (quote != '"' && quote != '\'') {
        return std::string::npos;
    }

    const std::size_t scanStart = quoteStart + 1;
    const std::size_t remaining = scanStart < text_->size() ? text_->size() - scanStart : 0;
    const std::size_t offset = FindQuoteOrNulInBuffer(text_->data() + scanStart, remaining, quote);
    if (offset == std::string::npos) {
        return std::string::npos;
    }

    const std::size_t match = scanStart + offset;
    return (*text_)[match] == quote ? match : std::string::npos;
}

std::string StringXmlReaderInputSource::Slice(std::size_t start, std::size_t count) const {
    return text_->substr(start, count);
}

void StringXmlReaderInputSource::AppendSliceTo(std::string& target, std::size_t start, std::size_t count) const {
    if (text_ == nullptr || start >= text_->size()) {
        return;
    }

    const std::size_t available = text_->size() - start;
    const std::size_t actualCount = count == std::string::npos ? available : (std::min)(count, available);
    target.append(*text_, start, actualCount);
}

}  // namespace System::Xml
