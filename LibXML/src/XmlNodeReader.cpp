#include "../include/XmlInternal.h"

namespace System::Xml {

XmlNodeReader::XmlNodeReader(const XmlNode& node, const XmlReaderSettings& settings)
    : settings_(settings) {
    BuildEvents(node, 0, false);
}

bool XmlNodeReader::Read() {
    attributeIndex_ = -1;
    started_ = true;

    while (currentIndex_ < events_.size()) {
        const auto& event = events_[currentIndex_++];
        if (settings_.IgnoreComments && event.nodeType == XmlNodeType::Comment) {
            continue;
        }
        if (settings_.IgnoreProcessingInstructions && event.nodeType == XmlNodeType::ProcessingInstruction) {
            continue;
        }
        if (settings_.IgnoreWhitespace
            && (event.nodeType == XmlNodeType::Whitespace || event.nodeType == XmlNodeType::SignificantWhitespace)) {
            continue;
        }

        eof_ = false;
        return true;
    }

    eof_ = true;
    return false;
}

bool XmlNodeReader::IsEOF() const noexcept {
    return eof_;
}

ReadState XmlNodeReader::GetReadState() const noexcept {
    if (eof_) return ReadState::EndOfFile;
    if (!started_) return ReadState::Initial;
    return ReadState::Interactive;
}

const XmlNameTable& XmlNodeReader::NameTable() const noexcept {
    return nameTable_;
}

XmlNodeType XmlNodeReader::NodeType() const {
    if (attributeIndex_ >= 0) {
        return XmlNodeType::Attribute;
    }
    const auto* event = CurrentEvent();
    return event == nullptr ? XmlNodeType::None : event->nodeType;
}

const std::string& XmlNodeReader::Name() const {
    if (attributeIndex_ >= 0) {
        return CurrentAttributes()[static_cast<std::size_t>(attributeIndex_)].first;
    }
    const auto* event = CurrentEvent();
    return event == nullptr ? EmptyString() : event->name;
}

std::string XmlNodeReader::LocalName() const {
    const auto* event = CurrentEvent();
    if (attributeIndex_ >= 0) {
        return event == nullptr ? std::string{} : event->attributeLocalNames[static_cast<std::size_t>(attributeIndex_)];
    }
    if (event == nullptr) {
        return {};
    }
    return event->localName;
}

std::string XmlNodeReader::Prefix() const {
    const auto* event = CurrentEvent();
    if (attributeIndex_ >= 0) {
        return event == nullptr ? std::string{} : event->attributePrefixes[static_cast<std::size_t>(attributeIndex_)];
    }
    if (event == nullptr) {
        return {};
    }
    return event->prefix;
}

const std::string& XmlNodeReader::NamespaceURI() const {
    const auto* event = CurrentEvent();
    if (attributeIndex_ >= 0) {
        return event == nullptr ? EmptyString() : event->attributeNamespaceUris[static_cast<std::size_t>(attributeIndex_)];
    }
    return event == nullptr ? EmptyString() : event->namespaceUri;
}

const std::string& XmlNodeReader::Value() const {
    if (attributeIndex_ >= 0) {
        return CurrentAttributes()[static_cast<std::size_t>(attributeIndex_)].second;
    }
    const auto* event = CurrentEvent();
    return event == nullptr ? EmptyString() : event->value;
}

int XmlNodeReader::Depth() const noexcept {
    const auto* event = CurrentEvent();
    if (event == nullptr) {
        return 0;
    }
    return event->depth + (attributeIndex_ >= 0 ? 1 : 0);
}

bool XmlNodeReader::IsEmptyElement() const noexcept {
    const auto* event = CurrentEvent();
    return attributeIndex_ < 0 && event != nullptr && event->nodeType == XmlNodeType::Element && event->isEmptyElement;
}

bool XmlNodeReader::HasValue() const noexcept {
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

int XmlNodeReader::AttributeCount() const noexcept {
    return static_cast<int>(CurrentAttributes().size());
}

bool XmlNodeReader::HasAttributes() const noexcept {
    return !CurrentAttributes().empty();
}

std::string XmlNodeReader::GetAttribute(std::string_view name) const {
    const auto* event = CurrentEvent();
    if (event == nullptr) {
        return {};
    }

    const auto& attributes = CurrentAttributes();
    const auto found = std::find_if(attributes.begin(), attributes.end(), [&name, event, &attributes](const auto& attribute) {
        const std::size_t index = static_cast<std::size_t>(&attribute - attributes.data());
        return attribute.first == name || event->attributeLocalNames[index] == name;
    });
    return found == attributes.end() ? std::string{} : found->second;
}

std::string XmlNodeReader::GetAttribute(int index) const {
    const auto& attributes = CurrentAttributes();
    if (index < 0 || static_cast<std::size_t>(index) >= attributes.size()) {
        throw std::out_of_range("Attribute index is out of range");
    }
    return attributes[static_cast<std::size_t>(index)].second;
}

std::string XmlNodeReader::GetAttribute(std::string_view localName, std::string_view namespaceUri) const {
    const auto* event = CurrentEvent();
    if (event == nullptr) return {};
    for (std::size_t i = 0; i < event->attributes.size(); ++i) {
        const auto ns = i < event->attributeNamespaceUris.size() ? event->attributeNamespaceUris[i] : std::string{};
        if (event->attributeLocalNames[i] == localName && ns == namespaceUri) {
            return event->attributes[i].second;
        }
    }
    return {};
}

bool XmlNodeReader::MoveToAttribute(std::string_view name) {
    if (name.empty()) {
        throw std::invalid_argument("Attribute name cannot be empty");
    }

    const auto* event = CurrentEvent();
    if (event == nullptr) {
        return false;
    }

    const auto& attributes = CurrentAttributes();
    const auto found = std::find_if(attributes.begin(), attributes.end(), [&name, event, &attributes](const auto& attribute) {
        const std::size_t index = static_cast<std::size_t>(&attribute - attributes.data());
        return attribute.first == name || event->attributeLocalNames[index] == name;
    });
    if (found == attributes.end()) {
        return false;
    }

    attributeIndex_ = static_cast<int>(std::distance(attributes.begin(), found));
    return true;
}

bool XmlNodeReader::MoveToAttribute(int index) {
    const auto& attributes = CurrentAttributes();
    if (index < 0 || static_cast<std::size_t>(index) >= attributes.size()) {
        throw std::out_of_range("Attribute index is out of range");
    }
    attributeIndex_ = index;
    return true;
}

bool XmlNodeReader::MoveToAttribute(std::string_view localName, std::string_view namespaceUri) {
    const auto* event = CurrentEvent();
    if (event == nullptr) return false;
    for (std::size_t i = 0; i < event->attributes.size(); ++i) {
        const auto ns = i < event->attributeNamespaceUris.size() ? event->attributeNamespaceUris[i] : std::string{};
        if (event->attributeLocalNames[i] == localName && ns == namespaceUri) {
            attributeIndex_ = static_cast<int>(i);
            return true;
        }
    }
    return false;
}

bool XmlNodeReader::MoveToFirstAttribute() {
    if (NodeType() != XmlNodeType::Element || CurrentAttributes().empty()) {
        return false;
    }

    attributeIndex_ = 0;
    return true;
}

bool XmlNodeReader::MoveToNextAttribute() {
    if (attributeIndex_ < 0) {
        return false;
    }
    if (static_cast<std::size_t>(attributeIndex_ + 1) >= CurrentAttributes().size()) {
        return false;
    }

    ++attributeIndex_;
    return true;
}

bool XmlNodeReader::MoveToElement() {
    const bool moved = attributeIndex_ >= 0;
    attributeIndex_ = -1;
    return moved;
}

std::string XmlNodeReader::LookupNamespace(std::string_view prefix) const {
    const auto* event = CurrentEvent();
    if (event == nullptr) return {};

    if (event->sourceNode != nullptr) {
        return event->sourceNode->GetNamespaceOfPrefix(prefix);
    }

    for (std::size_t i = 0; i < event->attributes.size(); ++i) {
        const auto& [name, value] = event->attributes[i];
        if (name == "xmlns" && prefix.empty()) return value;
        if (name.rfind("xmlns:", 0) == 0 && name.substr(6) == prefix) return value;
    }
    if (prefix == "xml") return "http://www.w3.org/XML/1998/namespace";
    if (prefix == "xmlns") return "http://www.w3.org/2000/xmlns/";
    return {};
}

std::string XmlNodeReader::ReadInnerXml() const {
    if (attributeIndex_ >= 0) {
        return Value();
    }
    const auto* event = CurrentEvent();
    if (event != nullptr && event->nodeType == XmlNodeType::Attribute) {
        return event->value;
    }
    if (event == nullptr || event->nodeType != XmlNodeType::Element) {
        return {};
    }
    return event->innerXml;
}

std::string XmlNodeReader::ReadOuterXml() const {
    if (attributeIndex_ >= 0) {
        return Name() + "=\"" + EscapeAttribute(Value(), XmlWriterSettings{}) + "\"";
    }
    const auto* event = CurrentEvent();
    if (event == nullptr || event->nodeType == XmlNodeType::EndElement || event->nodeType == XmlNodeType::EndEntity) {
        return {};
    }
    return event->outerXml;
}

std::string XmlNodeReader::ReadContentAsString() {
    std::string result;
    while (true) {
        auto nt = NodeType();
        if (nt == XmlNodeType::Text || nt == XmlNodeType::CDATA || nt == XmlNodeType::Whitespace ||
            nt == XmlNodeType::SignificantWhitespace) {
            result += Value();
            Read();
        } else {
            break;
        }
    }
    return result;
}

int XmlNodeReader::ReadContentAsInt() {
    return XmlConvert::ToInt32(ReadContentAsString());
}

long long XmlNodeReader::ReadContentAsLong() {
    return XmlConvert::ToInt64(ReadContentAsString());
}

double XmlNodeReader::ReadContentAsDouble() {
    return XmlConvert::ToDouble(ReadContentAsString());
}

bool XmlNodeReader::ReadContentAsBoolean() {
    return XmlConvert::ToBoolean(ReadContentAsString());
}

std::string XmlNodeReader::ReadString() {
    if (NodeType() == XmlNodeType::Element) {
        if (!Read()) return {};
    }
    std::string result;
    while (true) {
        auto nt = NodeType();
        if (nt == XmlNodeType::Text || nt == XmlNodeType::CDATA ||
            nt == XmlNodeType::Whitespace || nt == XmlNodeType::SignificantWhitespace) {
            result += Value();
            Read();
        } else {
            break;
        }
    }
    return result;
}

int XmlNodeReader::ReadBase64(std::vector<unsigned char>& buffer) {
    const std::size_t originalSize = buffer.size();
    unsigned int accumulator = 0;
    int bits = 0;

    while (true) {
        const auto nodeType = NodeType();
        if (nodeType == XmlNodeType::Text || nodeType == XmlNodeType::CDATA
            || nodeType == XmlNodeType::Whitespace || nodeType == XmlNodeType::SignificantWhitespace) {
            AppendDecodedBase64Chunk(Value(), buffer, accumulator, bits);
            Read();
        } else {
            break;
        }
    }

    return static_cast<int>(buffer.size() - originalSize);
}

XmlNodeType XmlNodeReader::MoveToContent() {
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

bool XmlNodeReader::IsStartElement() {
    return MoveToContent() == XmlNodeType::Element;
}

bool XmlNodeReader::IsStartElement(std::string_view name) {
    return MoveToContent() == XmlNodeType::Element && Name() == name;
}

bool XmlNodeReader::IsStartElement(std::string_view localName, std::string_view namespaceUri) {
    return MoveToContent() == XmlNodeType::Element && LocalName() == localName && NamespaceURI() == namespaceUri;
}

void XmlNodeReader::ReadStartElement() {
    if (MoveToContent() != XmlNodeType::Element) {
        throw XmlException("ReadStartElement called when the reader is not positioned on an element");
    }
    Read();
}

void XmlNodeReader::ReadStartElement(std::string_view name) {
    if (MoveToContent() != XmlNodeType::Element) {
        throw XmlException("ReadStartElement called when the reader is not positioned on an element");
    }
    if (Name() != name) {
        throw XmlException("Element '" + std::string(name) + "' was not found. Current element is '" + Name() + "'");
    }
    Read();
}

void XmlNodeReader::ReadStartElement(std::string_view localName, std::string_view namespaceUri) {
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

void XmlNodeReader::ReadEndElement() {
    if (MoveToContent() != XmlNodeType::EndElement) {
        throw XmlException("ReadEndElement called when the reader is not positioned on an end element");
    }
    Read();
}

std::string XmlNodeReader::ReadElementContentAsString() {
    if (MoveToContent() != XmlNodeType::Element) {
        throw XmlException("ReadElementContentAsString called when the reader is not positioned on an element");
    }
    if (IsEmptyElement()) {
        Read();
        return {};
    }
    Read();
    std::string result;
    while (true) {
        auto nt = NodeType();
        if (nt == XmlNodeType::EndElement) {
            Read();
            return result;
        }
        if (nt == XmlNodeType::Text || nt == XmlNodeType::CDATA || nt == XmlNodeType::Whitespace ||
            nt == XmlNodeType::SignificantWhitespace) {
            result += Value();
            Read();
        } else if (nt == XmlNodeType::Comment || nt == XmlNodeType::ProcessingInstruction) {
            Read();
        } else {
            throw XmlException("ReadElementContentAsString encountered unexpected node type");
        }
    }
}

std::string XmlNodeReader::ReadElementContentAsString(std::string_view localName, std::string_view namespaceUri) {
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

int XmlNodeReader::ReadElementContentAsInt() {
    return XmlConvert::ToInt32(ReadElementContentAsString());
}

int XmlNodeReader::ReadElementContentAsInt(std::string_view localName, std::string_view namespaceUri) {
    return XmlConvert::ToInt32(ReadElementContentAsString(localName, namespaceUri));
}

long long XmlNodeReader::ReadElementContentAsLong() {
    return XmlConvert::ToInt64(ReadElementContentAsString());
}

long long XmlNodeReader::ReadElementContentAsLong(std::string_view localName, std::string_view namespaceUri) {
    return XmlConvert::ToInt64(ReadElementContentAsString(localName, namespaceUri));
}

double XmlNodeReader::ReadElementContentAsDouble() {
    return XmlConvert::ToDouble(ReadElementContentAsString());
}

double XmlNodeReader::ReadElementContentAsDouble(std::string_view localName, std::string_view namespaceUri) {
    return XmlConvert::ToDouble(ReadElementContentAsString(localName, namespaceUri));
}

bool XmlNodeReader::ReadElementContentAsBoolean() {
    return XmlConvert::ToBoolean(ReadElementContentAsString());
}

bool XmlNodeReader::ReadElementContentAsBoolean(std::string_view localName, std::string_view namespaceUri) {
    return XmlConvert::ToBoolean(ReadElementContentAsString(localName, namespaceUri));
}

std::string XmlNodeReader::ReadElementString() {
    if (MoveToContent() != XmlNodeType::Element) {
        throw XmlException("ReadElementString called when the reader is not positioned on an element");
    }
    if (IsEmptyElement()) {
        Read();
        return {};
    }
    Read();
    std::string result;
    while (true) {
        auto nt = NodeType();
        if (nt == XmlNodeType::EndElement) {
            Read();
            return result;
        }
        if (nt == XmlNodeType::Text || nt == XmlNodeType::CDATA || nt == XmlNodeType::Whitespace ||
            nt == XmlNodeType::SignificantWhitespace) {
            result += Value();
            Read();
        } else if (nt == XmlNodeType::Comment || nt == XmlNodeType::ProcessingInstruction) {
            Read();
        } else {
            throw XmlException("ReadElementString does not support mixed content elements");
        }
    }
}

std::string XmlNodeReader::ReadElementString(std::string_view name) {
    if (MoveToContent() != XmlNodeType::Element) {
        throw XmlException("ReadElementString called when the reader is not positioned on an element");
    }
    if (Name() != name) {
        throw XmlException("Element '" + std::string(name) + "' was not found. Current element is '" + Name() + "'");
    }
    return ReadElementString();
}

std::string XmlNodeReader::ReadElementString(std::string_view localName, std::string_view namespaceUri) {
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

void XmlNodeReader::Skip() {
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

bool XmlNodeReader::ReadToFollowing(std::string_view name) {
    while (Read()) {
        if (NodeType() == XmlNodeType::Element && Name() == name) {
            return true;
        }
    }
    return false;
}

bool XmlNodeReader::ReadToDescendant(std::string_view name) {
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

bool XmlNodeReader::ReadToNextSibling(std::string_view name) {
    int targetDepth = Depth();
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

void XmlNodeReader::BuildEvents(const XmlNode& node, int depth, bool preserveSpace) {
    switch (node.NodeType()) {
    case XmlNodeType::Document:
    case XmlNodeType::DocumentFragment:
        for (const auto& child : node.ChildNodes()) {
            if (child != nullptr) {
                BuildEvents(*child, depth, preserveSpace);
            }
        }
        return;
    case XmlNodeType::Element: {
        std::vector<std::pair<std::string, std::string>> attributes;
        std::vector<std::string> attributeNamespaceUris;
        const auto& element = static_cast<const XmlElement&>(node);
        bool childPreserveSpace = preserveSpace;
        attributes.reserve(element.Attributes().size());
        attributeNamespaceUris.reserve(element.Attributes().size());
        for (const auto& attribute : element.Attributes()) {
            attributes.emplace_back(attribute->Name(), attribute->Value());
            attributeNamespaceUris.push_back(attribute->NamespaceURI());
            if (attribute->Name() == "xml:space") {
                if (IsXmlSpacePreserve(attribute->Value())) {
                    childPreserveSpace = true;
                } else if (IsXmlSpaceDefault(attribute->Value())) {
                    childPreserveSpace = false;
                }
            }
        }

        const bool isEmptyElement = !element.HasChildNodes() && !element.WritesFullEndElement();
        AppendEvent(
            XmlNodeType::Element,
            &node,
            element.Name(),
            element.NamespaceURI(),
            {},
            depth,
            isEmptyElement,
            element.InnerXml(),
            element.OuterXml(),
            std::move(attributes),
            std::move(attributeNamespaceUris));

        for (const auto& child : element.ChildNodes()) {
            if (child != nullptr) {
                BuildEvents(*child, depth + 1, childPreserveSpace);
            }
        }

        if (!isEmptyElement) {
            AppendEvent(XmlNodeType::EndElement, &node, element.Name(), element.NamespaceURI(), {}, depth, false, {}, "</" + element.Name() + ">" );
        }
        return;
    }
    case XmlNodeType::EntityReference:
        AppendEvent(
            XmlNodeType::EntityReference,
            &node,
            node.Name(),
            node.NamespaceURI(),
            node.Value(),
            depth,
            false,
            {},
            node.OuterXml());
        if (!node.Value().empty()) {
            const XmlNodeType textNodeType = IsWhitespaceOnly(node.Value())
                ? (preserveSpace ? XmlNodeType::SignificantWhitespace : XmlNodeType::Whitespace)
                : XmlNodeType::Text;
            AppendEvent(textNodeType, &node, {}, {}, node.Value(), depth + 1, false, {}, node.Value());
        }
        AppendEvent(XmlNodeType::EndEntity, &node, node.Name(), node.NamespaceURI(), {}, depth, false, {}, node.OuterXml());
        return;
    default:
        const bool anonymousReaderNode = node.NodeType() == XmlNodeType::Text
            || node.NodeType() == XmlNodeType::CDATA
            || node.NodeType() == XmlNodeType::Comment
            || node.NodeType() == XmlNodeType::Whitespace
            || node.NodeType() == XmlNodeType::SignificantWhitespace;
        AppendEvent(
            node.NodeType(),
            &node,
            anonymousReaderNode ? std::string{} : node.Name(),
            node.NamespaceURI(),
            node.Value(),
            depth,
            false,
            node.NodeType() == XmlNodeType::Element ? node.InnerXml() : std::string{},
            node.OuterXml());
        return;
    }
}

void XmlNodeReader::AppendEvent(
    XmlNodeType nodeType,
    const XmlNode* sourceNode,
    std::string name,
    std::string namespaceUri,
    std::string value,
    int depth,
    bool isEmptyElement,
    std::string innerXml,
    std::string outerXml,
    std::vector<std::pair<std::string, std::string>> attributes,
    std::vector<std::string> attributeNamespaceUris) {
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

    const auto [prefixView, localNameView] = SplitQualifiedNameView(name);
    std::string localName(localNameView);
    std::string prefix(prefixView);
    std::vector<std::string> attributeLocalNames;
    std::vector<std::string> attributePrefixes;
    attributeLocalNames.reserve(attributes.size());
    attributePrefixes.reserve(attributes.size());
    for (const auto& attribute : attributes) {
        const auto [attributePrefixView, attributeLocalNameView] = SplitQualifiedNameView(attribute.first);
        attributeLocalNames.emplace_back(attributeLocalNameView);
        attributePrefixes.emplace_back(attributePrefixView);
    }

    events_.push_back(NodeEvent{
        nodeType,
        sourceNode,
        std::move(name),
        std::move(localName),
        std::move(prefix),
        std::move(namespaceUri),
        std::move(value),
        depth,
        isEmptyElement,
        std::move(innerXml),
        std::move(outerXml),
        std::move(attributes),
        std::move(attributeLocalNames),
        std::move(attributePrefixes),
        std::move(attributeNamespaceUris)});
}

const XmlNodeReader::NodeEvent* XmlNodeReader::CurrentEvent() const noexcept {
    if (!started_ || currentIndex_ == 0 || currentIndex_ > events_.size()) {
        return nullptr;
    }
    return &events_[currentIndex_ - 1];
}

const std::vector<std::pair<std::string, std::string>>& XmlNodeReader::CurrentAttributes() const noexcept {
    const auto* event = CurrentEvent();
    static const std::vector<std::pair<std::string, std::string>> empty;
    return event == nullptr ? empty : event->attributes;
}

}  // namespace System::Xml
