#pragma once
// DOM traversal and serialization internal helpers.

#include "XmlUtilityInternal.h"

namespace System::Xml {

XmlNodeList EvaluateXPathFromDocument(const XmlDocument& document, std::string_view xpath, const XmlNamespaceManager* namespaces = nullptr);
XmlNodeList EvaluateXPathFromElement(const XmlElement& element, std::string_view xpath, const XmlNamespaceManager* namespaces = nullptr);
XmlNodeList EvaluateXPathFromNode(const XmlNode& node, std::string_view xpath, const XmlNamespaceManager* namespaces);

namespace {

void CollectElementsByName(
    const XmlNode& node,
    std::string_view name,
    std::vector<std::shared_ptr<XmlElement>>& results) {
    for (const auto& child : node.ChildNodes()) {
        if (child->NodeType() == XmlNodeType::Element) {
            auto element = std::static_pointer_cast<XmlElement>(child);
            if (name == "*" || element->Name() == name || element->LocalName() == name) {
                results.push_back(element);
            }

            CollectElementsByName(*element, name, results);
        }
    }
}

void CollectElementsByNameNS(
    const XmlNode& node,
    std::string_view localName,
    std::string_view namespaceUri,
    std::vector<std::shared_ptr<XmlElement>>& results) {
    for (const auto& child : node.ChildNodes()) {
        if (child->NodeType() == XmlNodeType::Element) {
            auto element = std::static_pointer_cast<XmlElement>(child);
            const bool nameMatch = (localName == "*" || element->LocalName() == localName);
            const bool nsMatch = (namespaceUri == "*" || element->NamespaceURI() == namespaceUri);
            if (nameMatch && nsMatch) {
                results.push_back(element);
            }
            CollectElementsByNameNS(*element, localName, namespaceUri, results);
        }
    }
}
bool SubsetStartsWithAt(std::string_view text, std::size_t position, std::string_view token) {
    return text.substr(position, token.size()) == token;
}

std::string_view ParseNameAtView(std::string_view text, std::size_t& position) {
    const auto start = position;
    position = ConsumeXmlNameAt(position, [text](std::size_t index) noexcept {
        return index < text.size() ? text[index] : '\0';
    });
    if (position == start) {
        return {};
    }

    return std::string_view(text.data() + start, position - start);
}

std::string ParseNameAt(std::string_view text, std::size_t& position) {
    const std::string_view name = ParseNameAtView(text, position);
    return std::string(name);
}

void SkipXmlWhitespaceAt(std::string_view text, std::size_t& position) noexcept {
    while (position < text.size() && IsWhitespace(text[position])) {
        ++position;
    }
}

bool SkipTagAt(std::string_view text, std::size_t& position, bool& isEmptyElement) {
    if (position >= text.size() || text[position] != '<') {
        return false;
    }

    ++position;
    if (ParseNameAtView(text, position).empty()) {
        return false;
    }

    while (position < text.size()) {
        const char ch = text[position];
        if (ch == '\"' || ch == '\'') {
            const char quote = ch;
            ++position;
            while (position < text.size() && text[position] != quote) {
                ++position;
            }
            if (position == text.size()) {
                return false;
            }
            ++position;
            continue;
        }

        if (SubsetStartsWithAt(text, position, "/>")) {
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

    return false;
}

std::pair<std::size_t, std::size_t> ComputeLineColumnAt(std::string_view text, std::size_t position) noexcept {
    std::size_t line = 1;
    std::size_t column = 1;

    for (std::size_t index = 0; index < position && index < text.size(); ++index) {
        if (text[index] == '\n') {
            ++line;
            column = 1;
        } else {
            ++column;
        }
    }

    return {line, column};
}

std::pair<std::size_t, std::size_t> FindElementClose(std::string_view text, std::size_t position) {
    int depth = 1;

    while (position < text.size()) {
        if (text[position] != '<') {
            ++position;
            continue;
        }

        if (SubsetStartsWithAt(text, position, "<!--")) {
            const auto end = text.find("-->", position + 4);
            if (end == std::string_view::npos) {
                return {std::string_view::npos, std::string_view::npos};
            }
            position = end + 3;
            continue;
        }

        if (SubsetStartsWithAt(text, position, "<![CDATA[")) {
            const auto end = text.find("]]>", position + 9);
            if (end == std::string_view::npos) {
                return {std::string_view::npos, std::string_view::npos};
            }
            position = end + 3;
            continue;
        }

        if (SubsetStartsWithAt(text, position, "<?")) {
            const auto end = text.find("?>", position + 2);
            if (end == std::string_view::npos) {
                return {std::string_view::npos, std::string_view::npos};
            }
            position = end + 2;
            continue;
        }

        if (SubsetStartsWithAt(text, position, "</")) {
            const auto closeStart = position;
            position += 2;
            if (ParseNameAtView(text, position).empty()) {
                return {std::string_view::npos, std::string_view::npos};
            }
            while (position < text.size() && text[position] != '>') {
                ++position;
            }
            if (position == text.size()) {
                return {std::string_view::npos, std::string_view::npos};
            }
            ++position;
            --depth;
            if (depth == 0) {
                return {closeStart, position};
            }
            continue;
        }

        if (SubsetStartsWithAt(text, position, "<!")) {
            const auto end = text.find('>', position + 2);
            if (end == std::string_view::npos) {
                return {std::string_view::npos, std::string_view::npos};
            }
            position = end + 1;
            continue;
        }

        bool isEmptyElement = false;
        if (!SkipTagAt(text, position, isEmptyElement)) {
            return {std::string_view::npos, std::string_view::npos};
        }
        if (!isEmptyElement) {
            ++depth;
        }
    }

    return {std::string_view::npos, std::string_view::npos};
}

std::string EscapeText(std::string_view value, const XmlWriterSettings& settings) {
    std::string normalized;
    std::string_view source = value;
    if (settings.NewLineHandling == XmlNewLineHandling::Replace) {
        normalized = NormalizeNewLines(value, settings.NewLineChars);
        source = normalized;
    }
    std::string escaped;
    escaped.reserve(source.size());

    for (std::size_t i = 0; i < source.size(); ++i) {
        const char ch = source[i];
        switch (ch) {
        case '&':
            escaped += "&amp;";
            break;
        case '<':
            escaped += "&lt;";
            break;
        case '>':
            escaped += "&gt;";
            break;
        case '\r':
            if (settings.NewLineHandling == XmlNewLineHandling::Entitize) {
                escaped += "&#xD;";
            } else {
                escaped.push_back(ch);
            }
            break;
        case '\n':
            if (settings.NewLineHandling == XmlNewLineHandling::Entitize) {
                escaped += "&#xA;";
            } else {
                escaped.push_back(ch);
            }
            break;
        default:
            escaped.push_back(ch);
            break;
        }
    }

    return escaped;
}

std::string EscapeAttribute(std::string_view value, const XmlWriterSettings& settings) {
    std::string normalized;
    std::string_view source = value;
    if (settings.NewLineHandling == XmlNewLineHandling::Replace) {
        normalized = NormalizeNewLines(value, settings.NewLineChars);
        source = normalized;
    }
    std::string escaped;
    escaped.reserve(source.size());

    for (const char ch : source) {
        switch (ch) {
        case '&':
            escaped += "&amp;";
            break;
        case '<':
            escaped += "&lt;";
            break;
        case '>':
            escaped += "&gt;";
            break;
        case '\"':
            escaped += "&quot;";
            break;
        case '\'':
            escaped += "&apos;";
            break;
        case '\r':
            if (settings.NewLineHandling == XmlNewLineHandling::Entitize) {
                escaped += "&#xD;";
            } else {
                escaped.push_back(ch);
            }
            break;
        case '\n':
            if (settings.NewLineHandling == XmlNewLineHandling::Entitize) {
                escaped += "&#xA;";
            } else {
                escaped.push_back(ch);
            }
            break;
        default:
            escaped.push_back(ch);
            break;
        }
    }

    return escaped;
}

}  // namespace

}  // namespace System::Xml
