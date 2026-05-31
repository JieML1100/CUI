#include "XmlReader.h"

#include "XmlConvert.h"
#include "XmlWriter.h"

#include <algorithm>
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

std::pair<std::string_view, std::string_view> SplitQualifiedName(std::string_view name) {
    const auto separator = name.find(':');
    if (separator == std::string_view::npos) {
        return {{}, name};
    }
    return {name.substr(0, separator), name.substr(separator + 1)};
}

bool IsContentNode(XmlNodeType nodeType) {
    return nodeType != XmlNodeType::Whitespace
        && nodeType != XmlNodeType::SignificantWhitespace
        && nodeType != XmlNodeType::Comment
        && nodeType != XmlNodeType::ProcessingInstruction;
}

}  // namespace

XmlReader XmlReader::Create(const std::string& xml, const XmlReaderSettings& settings) {
    XmlReader reader(settings);
    reader.LoadDocument(XmlDocument::Parse(xml, settings));
    return reader;
}

XmlReader XmlReader::Create(std::istream& stream, const XmlReaderSettings& settings) {
    return Create(ReadStreamFully(stream), settings);
}

XmlReader XmlReader::CreateFromFile(const std::string& path, const XmlReaderSettings& settings) {
    return Create(ReadFileFully(path), settings);
}

XmlReader::XmlReader(XmlReaderSettings settings)
    : settings_(std::move(settings)) {
}

bool XmlReader::Read() {
    if (state_ == ReadState::Closed) {
        return false;
    }

    attributeIndex_ = -1;
    const std::size_t nextIndex = currentIndex_ + 1;
    if (nextIndex >= nodes_.size()) {
        currentIndex_ = nodes_.size();
        state_ = ReadState::EndOfFile;
        return false;
    }

    currentIndex_ = nextIndex;
    state_ = ReadState::Interactive;
    return true;
}

bool XmlReader::IsEOF() const noexcept {
    return state_ == ReadState::EndOfFile;
}

ReadState XmlReader::GetReadState() const noexcept {
    return state_;
}

XmlNodeType XmlReader::NodeType() const {
    if (CurrentAttribute() != nullptr) {
        return XmlNodeType::Attribute;
    }
    const auto* current = CurrentNode();
    return current == nullptr ? XmlNodeType::None : current->nodeType;
}

const std::string& XmlReader::Name() const {
    if (const auto* attribute = CurrentAttribute()) {
        return attribute->first;
    }
    const auto* current = CurrentNode();
    return current == nullptr ? scratch_ : current->name;
}

std::string XmlReader::LocalName() const {
    return std::string(SplitQualifiedName(Name()).second);
}

std::string XmlReader::Prefix() const {
    return std::string(SplitQualifiedName(Name()).first);
}

const std::string& XmlReader::NamespaceURI() const {
    if (CurrentAttribute() != nullptr) {
        scratch_.clear();
        return scratch_;
    }
    const auto* current = CurrentNode();
    return current == nullptr ? scratch_ : current->namespaceUri;
}

const std::string& XmlReader::Value() const {
    if (const auto* attribute = CurrentAttribute()) {
        return attribute->second;
    }
    const auto* current = CurrentNode();
    return current == nullptr ? scratch_ : current->value;
}

int XmlReader::Depth() const noexcept {
    const auto* current = CurrentNode();
    return current == nullptr ? 0 : current->depth;
}

bool XmlReader::IsEmptyElement() const noexcept {
    const auto* current = CurrentNode();
    return current != nullptr && current->isEmptyElement;
}

bool XmlReader::HasValue() const noexcept {
    const auto* current = CurrentNode();
    return CurrentAttribute() != nullptr || (current != nullptr && !current->value.empty());
}

int XmlReader::AttributeCount() const noexcept {
    const auto* current = CurrentNode();
    return current == nullptr ? 0 : static_cast<int>(current->attributes.size());
}

bool XmlReader::HasAttributes() const noexcept {
    return AttributeCount() > 0;
}

std::string XmlReader::GetAttribute(std::string_view name) const {
    const auto* current = CurrentNode();
    if (current == nullptr) {
        return {};
    }

    const auto it = std::find_if(current->attributes.begin(), current->attributes.end(), [name](const auto& attribute) {
        return attribute.first == name;
    });
    return it == current->attributes.end() ? std::string{} : it->second;
}

std::string XmlReader::GetAttribute(int index) const {
    const auto* current = CurrentNode();
    if (current == nullptr || index < 0 || static_cast<std::size_t>(index) >= current->attributes.size()) {
        return {};
    }
    return current->attributes[static_cast<std::size_t>(index)].second;
}

std::string XmlReader::GetAttribute(std::string_view localName, std::string_view namespaceUri) const {
    if (!namespaceUri.empty()) {
        return {};
    }

    const auto* current = CurrentNode();
    if (current == nullptr) {
        return {};
    }

    const auto it = std::find_if(current->attributes.begin(), current->attributes.end(), [localName](const auto& attribute) {
        return SplitQualifiedName(attribute.first).second == localName;
    });
    return it == current->attributes.end() ? std::string{} : it->second;
}

bool XmlReader::MoveToAttribute(std::string_view name) {
    const auto* current = CurrentNode();
    if (current == nullptr) {
        return false;
    }

    for (std::size_t index = 0; index < current->attributes.size(); ++index) {
        if (current->attributes[index].first == name) {
            attributeIndex_ = static_cast<int>(index);
            return true;
        }
    }
    return false;
}

bool XmlReader::MoveToAttribute(int index) {
    const auto* current = CurrentNode();
    if (current == nullptr || index < 0 || static_cast<std::size_t>(index) >= current->attributes.size()) {
        return false;
    }
    attributeIndex_ = index;
    return true;
}

bool XmlReader::MoveToAttribute(std::string_view localName, std::string_view namespaceUri) {
    if (!namespaceUri.empty()) {
        return false;
    }

    const auto* current = CurrentNode();
    if (current == nullptr) {
        return false;
    }

    for (std::size_t index = 0; index < current->attributes.size(); ++index) {
        if (SplitQualifiedName(current->attributes[index].first).second == localName) {
            attributeIndex_ = static_cast<int>(index);
            return true;
        }
    }
    return false;
}

bool XmlReader::MoveToFirstAttribute() {
    return MoveToAttribute(0);
}

bool XmlReader::MoveToNextAttribute() {
    return MoveToAttribute(attributeIndex_ + 1);
}

bool XmlReader::MoveToElement() {
    if (attributeIndex_ < 0) {
        return false;
    }
    attributeIndex_ = -1;
    return true;
}

std::string XmlReader::ReadInnerXml() const {
    const auto* current = CurrentNode();
    if (current == nullptr || current->node == nullptr || current->nodeType != XmlNodeType::Element) {
        return {};
    }
    return current->node->InnerXml();
}

std::string XmlReader::ReadOuterXml() const {
    if (const auto* attribute = CurrentAttribute()) {
        return attribute->first + "=\"" + attribute->second + "\"";
    }

    const auto* current = CurrentNode();
    if (current == nullptr) {
        return {};
    }
    if (current->nodeType == XmlNodeType::EndElement) {
        return "</" + current->name + ">";
    }
    return current->node == nullptr ? std::string{} : current->node->OuterXml();
}

std::string XmlReader::ReadContentAsString() {
    const auto* current = CurrentNode();
    if (current == nullptr) {
        return {};
    }
    if (current->nodeType == XmlNodeType::Element && current->node != nullptr) {
        return current->node->InnerText();
    }
    return Value();
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
    return ReadContentAsString();
}

XmlNodeType XmlReader::MoveToContent() {
    while (CurrentNode() != nullptr && !IsContentNode(NodeType())) {
        if (!Read()) {
            return XmlNodeType::None;
        }
    }
    return NodeType();
}

bool XmlReader::IsStartElement() {
    return MoveToContent() == XmlNodeType::Element;
}

bool XmlReader::IsStartElement(std::string_view name) {
    return IsStartElement() && Name() == name;
}

bool XmlReader::IsStartElement(std::string_view localName, std::string_view namespaceUri) {
    return IsStartElement() && LocalName() == localName && NamespaceURI() == namespaceUri;
}

void XmlReader::ReadStartElement() {
    if (!IsStartElement()) {
        throw XmlException("The current node is not a start element");
    }
    Read();
}

void XmlReader::ReadStartElement(std::string_view name) {
    if (!IsStartElement(name)) {
        throw XmlException("The current node is not the expected start element");
    }
    Read();
}

void XmlReader::ReadStartElement(std::string_view localName, std::string_view namespaceUri) {
    if (!IsStartElement(localName, namespaceUri)) {
        throw XmlException("The current node is not the expected start element");
    }
    Read();
}

void XmlReader::ReadEndElement() {
    if (NodeType() != XmlNodeType::EndElement) {
        throw XmlException("The current node is not an end element");
    }
    Read();
}

std::string XmlReader::ReadElementContentAsString() {
    if (!IsStartElement()) {
        throw XmlException("The current node is not a start element");
    }
    const std::string value = CurrentNode()->node->InnerText();
    Skip();
    return value;
}

std::string XmlReader::ReadElementContentAsString(std::string_view localName, std::string_view namespaceUri) {
    if (!IsStartElement(localName, namespaceUri)) {
        throw XmlException("The current node is not the expected start element");
    }
    return ReadElementContentAsString();
}

int XmlReader::ReadElementContentAsInt() {
    return XmlConvert::ToInt32(ReadElementContentAsString());
}

long long XmlReader::ReadElementContentAsLong() {
    return XmlConvert::ToInt64(ReadElementContentAsString());
}

double XmlReader::ReadElementContentAsDouble() {
    return XmlConvert::ToDouble(ReadElementContentAsString());
}

bool XmlReader::ReadElementContentAsBoolean() {
    return XmlConvert::ToBoolean(ReadElementContentAsString());
}

std::string XmlReader::ReadElementString() {
    return ReadElementContentAsString();
}

std::string XmlReader::ReadElementString(std::string_view name) {
    if (!IsStartElement(name)) {
        throw XmlException("The current node is not the expected start element");
    }
    return ReadElementContentAsString();
}

std::string XmlReader::ReadElementString(std::string_view localName, std::string_view namespaceUri) {
    return ReadElementContentAsString(localName, namespaceUri);
}

void XmlReader::Skip() {
    attributeIndex_ = -1;
    const auto* current = CurrentNode();
    if (current == nullptr) {
        return;
    }

    if (current->nodeType == XmlNodeType::Element && !current->isEmptyElement) {
        currentIndex_ = FindMatchingEndElementIndex(currentIndex_);
    }
    Read();
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
    if (NodeType() != XmlNodeType::Element) {
        return false;
    }

    const int startDepth = Depth();
    while (Read()) {
        if (Depth() <= startDepth) {
            return false;
        }
        if (NodeType() == XmlNodeType::Element && Name() == name) {
            return true;
        }
    }
    return false;
}

bool XmlReader::ReadToNextSibling(std::string_view name) {
    const int startDepth = Depth();
    Skip();
    while (CurrentNode() != nullptr && Depth() == startDepth) {
        if (NodeType() == XmlNodeType::Element && Name() == name) {
            return true;
        }
        Skip();
    }
    return false;
}

XmlReader XmlReader::ReadSubtree() {
    if (NodeType() != XmlNodeType::Element) {
        return XmlReader(settings_);
    }
    return Create(ReadOuterXml(), settings_);
}

void XmlReader::Close() {
    state_ = ReadState::Closed;
}

const XmlReader::ReaderNode* XmlReader::CurrentNode() const noexcept {
    return currentIndex_ < nodes_.size() ? &nodes_[currentIndex_] : nullptr;
}

const std::pair<std::string, std::string>* XmlReader::CurrentAttribute() const noexcept {
    const auto* current = CurrentNode();
    if (current == nullptr || attributeIndex_ < 0 || static_cast<std::size_t>(attributeIndex_) >= current->attributes.size()) {
        return nullptr;
    }
    return &current->attributes[static_cast<std::size_t>(attributeIndex_)];
}

std::size_t XmlReader::FindMatchingEndElementIndex(std::size_t elementIndex) const {
    const int depth = nodes_[elementIndex].depth;
    for (std::size_t index = elementIndex + 1; index < nodes_.size(); ++index) {
        if (nodes_[index].depth == depth && nodes_[index].nodeType == XmlNodeType::EndElement) {
            return index;
        }
    }
    return elementIndex;
}

void XmlReader::LoadDocument(const std::shared_ptr<XmlDocument>& document) {
    document_ = document;
    nodes_.clear();
    currentIndex_ = static_cast<std::size_t>(-1);
    attributeIndex_ = -1;
    state_ = ReadState::Initial;

    for (const auto& child : document_->ChildNodes()) {
        AppendFlattenedNode(child, 0);
    }
}

void XmlReader::AppendFlattenedNode(const std::shared_ptr<XmlNode>& node, int depth) {
    if (node->NodeType() == XmlNodeType::Element) {
        const auto element = std::static_pointer_cast<XmlElement>(node);
        ReaderNode readerNode;
        readerNode.nodeType = XmlNodeType::Element;
        readerNode.name = element->Name();
        readerNode.namespaceUri = element->NamespaceURI();
        readerNode.depth = depth;
        readerNode.isEmptyElement = !element->HasChildNodes();
        readerNode.node = node;
        for (const auto& attribute : element->Attributes()) {
            readerNode.attributes.emplace_back(attribute->Name(), attribute->Value());
        }
        nodes_.push_back(std::move(readerNode));

        for (const auto& child : element->ChildNodes()) {
            AppendFlattenedNode(child, depth + 1);
        }

        if (element->HasChildNodes()) {
            ReaderNode endNode;
            endNode.nodeType = XmlNodeType::EndElement;
            endNode.name = element->Name();
            endNode.namespaceUri = element->NamespaceURI();
            endNode.depth = depth;
            endNode.node = node;
            nodes_.push_back(std::move(endNode));
        }
        return;
    }

    ReaderNode readerNode;
    readerNode.nodeType = node->NodeType();
    readerNode.name = node->Name();
    readerNode.namespaceUri = node->NamespaceURI();
    readerNode.value = node->Value();
    readerNode.depth = depth;
    readerNode.node = node;
    nodes_.push_back(std::move(readerNode));
}

}  // namespace System::Xml
