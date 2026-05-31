#include "XmlNode.h"

#include "XmlConvert.h"
#include "XmlDocument.h"
#include "XmlWriter.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace System::Xml {

namespace {

constexpr std::size_t npos = (std::numeric_limits<std::size_t>::max)();

std::pair<std::string_view, std::string_view> SplitQualifiedName(std::string_view name) {
    const auto separator = name.find(':');
    if (separator == std::string_view::npos) {
        return {{}, name};
    }
    return {name.substr(0, separator), name.substr(separator + 1)};
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

std::string NamespaceDeclarationName(std::string_view prefix) {
    return prefix.empty() ? "xmlns" : "xmlns:" + std::string(prefix);
}

std::string LookupNamespaceUri(const XmlElement* element, std::string_view prefix) {
    if (prefix == "xml") {
        return "http://www.w3.org/XML/1998/namespace";
    }
    if (prefix == "xmlns") {
        return "http://www.w3.org/2000/xmlns/";
    }

    for (const XmlElement* current = element; current != nullptr;) {
        const std::string value = current->FindNamespaceDeclarationValue(prefix);
        if (!value.empty()) {
            return value;
        }

        const XmlNode* parent = current->ParentNode();
        current = parent != nullptr && parent->NodeType() == XmlNodeType::Element
            ? static_cast<const XmlElement*>(parent)
            : nullptr;
    }

    return {};
}

std::string LookupNamespacePrefix(const XmlElement* element, std::string_view namespaceUri) {
    if (namespaceUri == "http://www.w3.org/XML/1998/namespace") {
        return "xml";
    }
    if (namespaceUri == "http://www.w3.org/2000/xmlns/") {
        return "xmlns";
    }

    for (const XmlElement* current = element; current != nullptr;) {
        const std::string prefix = current->FindNamespaceDeclarationPrefix(namespaceUri);
        if (!prefix.empty() || current->FindNamespaceDeclarationValue({}) == namespaceUri) {
            return prefix;
        }

        const XmlNode* parent = current->ParentNode();
        current = parent != nullptr && parent->NodeType() == XmlNodeType::Element
            ? static_cast<const XmlElement*>(parent)
            : nullptr;
    }

    return {};
}

void AppendInnerText(const XmlNode& node, std::string& text) {
    switch (node.NodeType()) {
    case XmlNodeType::Text:
    case XmlNodeType::CDATA:
    case XmlNodeType::Whitespace:
    case XmlNodeType::SignificantWhitespace:
    case XmlNodeType::Attribute:
        text += node.Value();
        return;
    default:
        break;
    }

    for (const auto& child : node.ChildNodes()) {
        AppendInnerText(*child, text);
    }
}

void CollectElementsByName(const XmlNode& node, std::string_view name, std::vector<std::shared_ptr<XmlElement>>& result) {
    for (const auto& child : node.ChildNodes()) {
        if (child->NodeType() == XmlNodeType::Element) {
            auto element = std::static_pointer_cast<XmlElement>(child);
            if (name == "*" || element->Name() == name) {
                result.push_back(element);
            }
            CollectElementsByName(*element, name, result);
        }
    }
}

void CollectElementsByLocalName(
    const XmlNode& node,
    std::string_view localName,
    std::string_view namespaceUri,
    std::vector<std::shared_ptr<XmlElement>>& result) {
    for (const auto& child : node.ChildNodes()) {
        if (child->NodeType() == XmlNodeType::Element) {
            auto element = std::static_pointer_cast<XmlElement>(child);
            if ((localName == "*" || element->LocalName() == localName)
                && (namespaceUri == "*" || element->NamespaceURI() == namespaceUri)) {
                result.push_back(element);
            }
            CollectElementsByLocalName(*element, localName, namespaceUri, result);
        }
    }
}

}  // namespace

XmlNodeList::XmlNodeList(std::vector<std::shared_ptr<XmlNode>> nodes)
    : nodes_(std::move(nodes)) {
}

std::size_t XmlNodeList::Count() const noexcept {
    return nodes_.size();
}

std::shared_ptr<XmlNode> XmlNodeList::Item(std::size_t index) const {
    return index < nodes_.size() ? nodes_[index] : nullptr;
}

bool XmlNodeList::Empty() const noexcept {
    return nodes_.empty();
}

std::vector<std::shared_ptr<XmlNode>>::const_iterator XmlNodeList::begin() const noexcept {
    return nodes_.begin();
}

std::vector<std::shared_ptr<XmlNode>>::const_iterator XmlNodeList::end() const noexcept {
    return nodes_.end();
}

XmlAttributeCollection::XmlAttributeCollection(std::vector<std::shared_ptr<XmlAttribute>> attributes)
    : attributes_(std::move(attributes)) {
}

std::size_t XmlAttributeCollection::Count() const noexcept {
    return attributes_.size();
}

std::shared_ptr<XmlAttribute> XmlAttributeCollection::Item(std::size_t index) const {
    return index < attributes_.size() ? attributes_[index] : nullptr;
}

std::shared_ptr<XmlAttribute> XmlAttributeCollection::Item(std::string_view name) const {
    const auto it = std::find_if(attributes_.begin(), attributes_.end(), [name](const auto& attribute) {
        return attribute->Name() == name;
    });
    return it == attributes_.end() ? nullptr : *it;
}

bool XmlAttributeCollection::Empty() const noexcept {
    return attributes_.empty();
}

std::vector<std::shared_ptr<XmlAttribute>>::const_iterator XmlAttributeCollection::begin() const noexcept {
    return attributes_.begin();
}

std::vector<std::shared_ptr<XmlAttribute>>::const_iterator XmlAttributeCollection::end() const noexcept {
    return attributes_.end();
}

XmlNode::XmlNode(XmlNodeType nodeType, std::string name, std::string value)
    : nodeType_(nodeType), name_(std::move(name)), value_(std::move(value)) {
}

XmlNodeType XmlNode::NodeType() const noexcept {
    return nodeType_;
}

const std::string& XmlNode::Name() const noexcept {
    return name_;
}

std::string XmlNode::LocalName() const {
    return std::string(SplitQualifiedName(name_).second);
}

std::string XmlNode::Prefix() const {
    return std::string(SplitQualifiedName(name_).first);
}

std::string XmlNode::NamespaceURI() const {
    if (nodeType_ == XmlNodeType::Attribute) {
        const auto& attribute = static_cast<const XmlAttribute&>(*this);
        const auto* ownerElement = attribute.OwnerElement();
        if (ownerElement == nullptr) {
            return {};
        }

        const auto [prefix, localName] = SplitQualifiedName(name_);
        if (name_ == "xmlns" || prefix == "xmlns") {
            return "http://www.w3.org/2000/xmlns/";
        }
        if (prefix.empty()) {
            return {};
        }
        return LookupNamespaceUri(ownerElement, prefix);
    }

    const XmlElement* element = nullptr;
    if (nodeType_ == XmlNodeType::Element) {
        element = static_cast<const XmlElement*>(this);
    } else if (parent_ != nullptr && parent_->NodeType() == XmlNodeType::Element) {
        element = static_cast<const XmlElement*>(parent_);
    }

    if (element == nullptr) {
        return {};
    }

    return LookupNamespaceUri(element, Prefix());
}

const std::string& XmlNode::Value() const noexcept {
    return value_;
}

void XmlNode::SetValue(std::string_view value) {
    value_ = std::string(value);
}

XmlNode* XmlNode::ParentNode() noexcept {
    return parent_;
}

const XmlNode* XmlNode::ParentNode() const noexcept {
    return parent_;
}

XmlDocument* XmlNode::OwnerDocument() noexcept {
    return ownerDocument_;
}

const XmlDocument* XmlNode::OwnerDocument() const noexcept {
    return ownerDocument_;
}

const std::vector<std::shared_ptr<XmlNode>>& XmlNode::ChildNodes() const noexcept {
    return childNodes_;
}

XmlNodeList XmlNode::ChildNodeList() const {
    return XmlNodeList(childNodes_);
}

std::shared_ptr<XmlNode> XmlNode::FirstChild() const {
    return childNodes_.empty() ? nullptr : childNodes_.front();
}

std::shared_ptr<XmlNode> XmlNode::LastChild() const {
    return childNodes_.empty() ? nullptr : childNodes_.back();
}

std::shared_ptr<XmlNode> XmlNode::PreviousSibling() const {
    if (parent_ == nullptr) {
        return nullptr;
    }

    const auto& siblings = parent_->ChildNodes();
    for (std::size_t index = 0; index < siblings.size(); ++index) {
        if (siblings[index].get() == this) {
            return index == 0 ? nullptr : siblings[index - 1];
        }
    }

    return nullptr;
}

std::shared_ptr<XmlNode> XmlNode::NextSibling() const {
    if (parent_ == nullptr) {
        return nullptr;
    }

    const auto& siblings = parent_->ChildNodes();
    for (std::size_t index = 0; index < siblings.size(); ++index) {
        if (siblings[index].get() == this) {
            return index + 1 < siblings.size() ? siblings[index + 1] : nullptr;
        }
    }

    return nullptr;
}

std::shared_ptr<XmlNode> XmlNode::SharedFromParent() const {
    if (parent_ == nullptr) {
        return nullptr;
    }

    for (const auto& child : parent_->ChildNodes()) {
        if (child.get() == this) {
            return child;
        }
    }

    return nullptr;
}

bool XmlNode::HasChildNodes() const noexcept {
    return !childNodes_.empty();
}

std::shared_ptr<XmlNode> XmlNode::AppendChild(const std::shared_ptr<XmlNode>& child) {
    ValidateChildInsertion(child, nullptr);
    AttachChildAt(child, childNodes_.size());
    return child;
}

std::shared_ptr<XmlNode> XmlNode::InsertBefore(
    const std::shared_ptr<XmlNode>& newChild,
    const std::shared_ptr<XmlNode>& referenceChild) {
    ValidateChildInsertion(newChild, nullptr);
    const std::size_t index = referenceChild == nullptr ? childNodes_.size() : FindChildIndexOrThrow(referenceChild);
    AttachChildAt(newChild, index);
    return newChild;
}

std::shared_ptr<XmlNode> XmlNode::InsertAfter(
    const std::shared_ptr<XmlNode>& newChild,
    const std::shared_ptr<XmlNode>& referenceChild) {
    ValidateChildInsertion(newChild, nullptr);
    const std::size_t index = referenceChild == nullptr ? childNodes_.size() : FindChildIndexOrThrow(referenceChild) + 1;
    AttachChildAt(newChild, index);
    return newChild;
}

std::shared_ptr<XmlNode> XmlNode::ReplaceChild(
    const std::shared_ptr<XmlNode>& newChild,
    const std::shared_ptr<XmlNode>& oldChild) {
    ValidateChildInsertion(newChild, oldChild.get());
    const std::size_t index = FindChildIndexOrThrow(oldChild);
    oldChild->SetParent(nullptr);
    childNodes_.erase(childNodes_.begin() + static_cast<std::ptrdiff_t>(index));
    AttachChildAt(newChild, index);
    return oldChild;
}

std::shared_ptr<XmlNode> XmlNode::RemoveChild(const std::shared_ptr<XmlNode>& child) {
    const std::size_t index = FindChildIndexOrThrow(child);
    child->SetParent(nullptr);
    childNodes_.erase(childNodes_.begin() + static_cast<std::ptrdiff_t>(index));
    return child;
}

void XmlNode::RemoveAllChildren() {
    for (auto& child : childNodes_) {
        child->SetParent(nullptr);
    }
    childNodes_.clear();
}

void XmlNode::RemoveAll() {
    RemoveAllChildren();
}

std::string XmlNode::InnerText() const {
    std::string text;
    AppendInnerText(*this, text);
    return text;
}

void XmlNode::SetInnerText(std::string_view text) {
    RemoveAllChildren();
    if (!text.empty()) {
        AppendChild(std::make_shared<XmlText>(std::string(text)));
    }
}

std::string XmlNode::InnerXml(const XmlWriterSettings& settings) const {
    std::string xml;
    for (const auto& child : childNodes_) {
        xml += XmlWriter::WriteToString(*child, settings);
    }
    return xml;
}

void XmlNode::SetInnerXml(std::string_view xml) {
    const std::string wrapped = "<__xml_lite_fragment__>" + std::string(xml) + "</__xml_lite_fragment__>";
    const auto document = XmlDocument::Parse(wrapped);
    const auto root = document->DocumentElement();
    RemoveAllChildren();
    if (root == nullptr) {
        return;
    }

    for (const auto& child : root->ChildNodes()) {
        AppendChild(child->CloneNode(true));
    }
}

std::string XmlNode::OuterXml(const XmlWriterSettings& settings) const {
    return XmlWriter::WriteToString(*this, settings);
}

std::shared_ptr<XmlNode> XmlNode::CloneNode(bool deep) const {
    std::shared_ptr<XmlNode> clone;
    switch (nodeType_) {
    case XmlNodeType::Element: {
        const auto& element = static_cast<const XmlElement&>(*this);
        auto elementClone = std::make_shared<XmlElement>(name_);
        elementClone->SetWritesFullEndElement(element.WritesFullEndElement());
        for (const auto& attribute : element.Attributes()) {
            elementClone->SetAttributeNode(std::static_pointer_cast<XmlAttribute>(attribute->CloneNode(true)));
        }
        clone = elementClone;
        break;
    }
    case XmlNodeType::Attribute:
        clone = std::make_shared<XmlAttribute>(name_, value_);
        break;
    case XmlNodeType::Text:
        clone = std::make_shared<XmlText>(value_);
        break;
    case XmlNodeType::Whitespace:
        clone = std::make_shared<XmlWhitespace>(value_);
        break;
    case XmlNodeType::SignificantWhitespace:
        clone = std::make_shared<XmlSignificantWhitespace>(value_);
        break;
    case XmlNodeType::CDATA:
        clone = std::make_shared<XmlCDataSection>(value_);
        break;
    case XmlNodeType::Comment:
        clone = std::make_shared<XmlComment>(value_);
        break;
    case XmlNodeType::ProcessingInstruction: {
        const auto& instruction = static_cast<const XmlProcessingInstruction&>(*this);
        clone = std::make_shared<XmlProcessingInstruction>(instruction.Target(), instruction.Data());
        break;
    }
    case XmlNodeType::XmlDeclaration: {
        const auto& declaration = static_cast<const XmlDeclaration&>(*this);
        clone = std::make_shared<XmlDeclaration>(declaration.Version(), declaration.Encoding(), declaration.Standalone());
        break;
    }
    case XmlNodeType::DocumentType: {
        const auto& documentType = static_cast<const XmlDocumentType&>(*this);
        clone = std::make_shared<XmlDocumentType>(
            documentType.Name(),
            documentType.PublicId(),
            documentType.SystemId(),
            documentType.InternalSubset());
        break;
    }
    case XmlNodeType::DocumentFragment:
        clone = std::make_shared<XmlDocumentFragment>();
        break;
    case XmlNodeType::EntityReference:
        clone = std::make_shared<XmlEntityReference>(name_);
        break;
    default:
        clone = std::make_shared<XmlText>(value_);
        break;
    }

    if (deep) {
        for (const auto& child : childNodes_) {
            clone->AppendChild(child->CloneNode(true));
        }
    }

    return clone;
}

void XmlNode::ValidateChildInsertion(const std::shared_ptr<XmlNode>& child, const XmlNode*) const {
    if (child == nullptr) {
        throw XmlException("Cannot insert a null XML node");
    }

    if (child.get() == this) {
        throw XmlException("Cannot insert a node into itself");
    }

    for (const XmlNode* parent = this; parent != nullptr; parent = parent->ParentNode()) {
        if (parent == child.get()) {
            throw XmlException("Cannot insert an ancestor node as a child");
        }
    }

    if (nodeType_ == XmlNodeType::Text
        || nodeType_ == XmlNodeType::CDATA
        || nodeType_ == XmlNodeType::Comment
        || nodeType_ == XmlNodeType::ProcessingInstruction
        || nodeType_ == XmlNodeType::Attribute) {
        throw XmlException("This node type cannot contain child nodes");
    }

    if (child->NodeType() == XmlNodeType::Attribute) {
        throw XmlException("Attributes must be added through XmlElement::SetAttributeNode");
    }
}

void XmlNode::SetOwnerDocument(XmlDocument* ownerDocument) noexcept {
    ownerDocument_ = ownerDocument;
}

void XmlNode::SetOwnerDocumentRecursive(XmlDocument* ownerDocument) {
    ownerDocument_ = ownerDocument;
    if (nodeType_ == XmlNodeType::Element) {
        auto& element = static_cast<XmlElement&>(*this);
        for (const auto& attribute : element.Attributes()) {
            attribute->SetOwnerDocument(ownerDocument);
        }
    }
    for (auto& child : childNodes_) {
        child->SetOwnerDocumentRecursive(ownerDocument);
    }
}

void XmlNode::SetParent(XmlNode* parent) noexcept {
    parent_ = parent;
}

std::vector<std::shared_ptr<XmlNode>>& XmlNode::MutableChildNodes() noexcept {
    return childNodes_;
}

std::size_t XmlNode::FindChildIndexOrThrow(const std::shared_ptr<XmlNode>& child) const {
    const auto it = std::find_if(childNodes_.begin(), childNodes_.end(), [&child](const auto& existing) {
        return existing == child;
    });
    if (it == childNodes_.end()) {
        throw XmlException("The reference node is not a child of this node");
    }
    return static_cast<std::size_t>(std::distance(childNodes_.begin(), it));
}

void XmlNode::AttachChildAt(const std::shared_ptr<XmlNode>& child, std::size_t index) {
    if (child->ParentNode() != nullptr) {
        child->ParentNode()->RemoveChild(child);
    }

    child->SetParent(this);
    child->SetOwnerDocumentRecursive(ownerDocument_);
    childNodes_.insert(childNodes_.begin() + static_cast<std::ptrdiff_t>(index), child);
}

XmlAttribute::XmlAttribute(std::string name, std::string value)
    : XmlNode(XmlNodeType::Attribute, XmlConvert::VerifyName(name), std::move(value)) {
}

XmlElement* XmlAttribute::OwnerElement() noexcept {
    return ownerElement_;
}

const XmlElement* XmlAttribute::OwnerElement() const noexcept {
    return ownerElement_;
}

void XmlAttribute::SetOwnerElement(XmlElement* ownerElement) noexcept {
    ownerElement_ = ownerElement;
}

XmlCharacterData::XmlCharacterData(XmlNodeType nodeType, std::string name, std::string value)
    : XmlNode(nodeType, std::move(name), std::move(value)) {
}

const std::string& XmlCharacterData::Data() const noexcept {
    return Value();
}

void XmlCharacterData::SetData(std::string_view data) {
    SetValue(data);
}

std::size_t XmlCharacterData::Length() const noexcept {
    return Value().size();
}

void XmlCharacterData::AppendData(std::string_view strData) {
    SetValue(Value() + std::string(strData));
}

void XmlCharacterData::DeleteData(std::size_t offset, std::size_t count) {
    std::string value = Value();
    if (offset > value.size()) {
        throw XmlException("Character data offset is out of range");
    }
    value.erase(offset, count);
    SetValue(value);
}

void XmlCharacterData::InsertData(std::size_t offset, std::string_view strData) {
    std::string value = Value();
    if (offset > value.size()) {
        throw XmlException("Character data offset is out of range");
    }
    value.insert(offset, strData);
    SetValue(value);
}

void XmlCharacterData::ReplaceData(std::size_t offset, std::size_t count, std::string_view strData) {
    std::string value = Value();
    if (offset > value.size()) {
        throw XmlException("Character data offset is out of range");
    }
    value.replace(offset, count, strData);
    SetValue(value);
}

std::string XmlCharacterData::Substring(std::size_t offset, std::size_t count) const {
    if (offset > Value().size()) {
        throw XmlException("Character data offset is out of range");
    }
    return Value().substr(offset, count);
}

XmlText::XmlText(std::string text)
    : XmlCharacterData(XmlNodeType::Text, "#text", std::move(text)) {
}

XmlEntityReference::XmlEntityReference(std::string name)
    : XmlNode(XmlNodeType::EntityReference, XmlConvert::VerifyName(name)) {
}

XmlWhitespace::XmlWhitespace(std::string whitespace)
    : XmlCharacterData(XmlNodeType::Whitespace, "#whitespace", std::move(whitespace)) {
    if (!IsWhitespaceOnly(Value())) {
        throw XmlException("Whitespace nodes can only contain whitespace characters");
    }
}

void XmlWhitespace::SetValue(std::string_view value) {
    if (!IsWhitespaceOnly(value)) {
        throw XmlException("Whitespace nodes can only contain whitespace characters");
    }
    XmlCharacterData::SetValue(value);
}

XmlSignificantWhitespace::XmlSignificantWhitespace(std::string whitespace)
    : XmlCharacterData(XmlNodeType::SignificantWhitespace, "#significant-whitespace", std::move(whitespace)) {
    if (!IsWhitespaceOnly(Value())) {
        throw XmlException("Significant whitespace nodes can only contain whitespace characters");
    }
}

void XmlSignificantWhitespace::SetValue(std::string_view value) {
    if (!IsWhitespaceOnly(value)) {
        throw XmlException("Significant whitespace nodes can only contain whitespace characters");
    }
    XmlCharacterData::SetValue(value);
}

XmlCDataSection::XmlCDataSection(std::string data)
    : XmlCharacterData(XmlNodeType::CDATA, "#cdata-section", std::move(data)) {
}

XmlComment::XmlComment(std::string comment)
    : XmlCharacterData(XmlNodeType::Comment, "#comment", std::move(comment)) {
}

XmlProcessingInstruction::XmlProcessingInstruction(std::string target, std::string data)
    : XmlNode(XmlNodeType::ProcessingInstruction, XmlConvert::VerifyNCName(target), std::move(data)) {
    if (Target() == "xml" || Target() == "XML") {
        throw XmlException("'xml' is reserved for XML declarations");
    }
}

const std::string& XmlProcessingInstruction::Target() const noexcept {
    return Name();
}

const std::string& XmlProcessingInstruction::Data() const noexcept {
    return Value();
}

void XmlProcessingInstruction::SetData(std::string_view data) {
    SetValue(data);
}

XmlDeclaration::XmlDeclaration(std::string version, std::string encoding, std::string standalone)
    : XmlNode(XmlNodeType::XmlDeclaration, "xml"), version_(std::move(version)), encoding_(std::move(encoding)), standalone_(std::move(standalone)) {
    if (version_.empty()) {
        version_ = "1.0";
    }
}

const std::string& XmlDeclaration::Version() const noexcept {
    return version_;
}

const std::string& XmlDeclaration::Encoding() const noexcept {
    return encoding_;
}

const std::string& XmlDeclaration::Standalone() const noexcept {
    return standalone_;
}

XmlDocumentType::XmlDocumentType(
    std::string name,
    std::string publicId,
    std::string systemId,
    std::string internalSubset)
    : XmlNode(XmlNodeType::DocumentType, XmlConvert::VerifyName(name)),
      publicId_(std::move(publicId)),
      systemId_(std::move(systemId)),
      internalSubset_(std::move(internalSubset)) {
}

const std::string& XmlDocumentType::PublicId() const noexcept {
    return publicId_;
}

const std::string& XmlDocumentType::SystemId() const noexcept {
    return systemId_;
}

const std::string& XmlDocumentType::InternalSubset() const noexcept {
    return internalSubset_;
}

XmlDocumentFragment::XmlDocumentFragment()
    : XmlNode(XmlNodeType::DocumentFragment, "#document-fragment") {
}

void XmlDocumentFragment::SetInnerXml(std::string_view xml) {
    XmlNode::SetInnerXml(xml);
}

XmlElement::XmlElement(std::string name)
    : XmlNode(XmlNodeType::Element, XmlConvert::VerifyName(name)) {
}

const std::vector<std::shared_ptr<XmlAttribute>>& XmlElement::Attributes() const noexcept {
    return attributes_;
}

XmlAttributeCollection XmlElement::AttributeNodes() const {
    return XmlAttributeCollection(attributes_);
}

bool XmlElement::HasAttributes() const noexcept {
    return !attributes_.empty();
}

bool XmlElement::HasAttribute(std::string_view name) const {
    return FindAttributeIndex(name) != npos;
}

bool XmlElement::HasAttribute(std::string_view localName, std::string_view namespaceUri) const {
    return FindAttributeIndex(localName, namespaceUri) != npos;
}

std::string XmlElement::GetAttribute(std::string_view name) const {
    const std::size_t index = FindAttributeIndex(name);
    return index == npos ? std::string{} : attributes_[index]->Value();
}

std::string XmlElement::GetAttribute(std::string_view localName, std::string_view namespaceUri) const {
    const std::size_t index = FindAttributeIndex(localName, namespaceUri);
    return index == npos ? std::string{} : attributes_[index]->Value();
}

std::shared_ptr<XmlAttribute> XmlElement::GetAttributeNode(std::string_view name) const {
    const std::size_t index = FindAttributeIndex(name);
    return index == npos ? nullptr : attributes_[index];
}

std::shared_ptr<XmlAttribute> XmlElement::GetAttributeNode(std::string_view localName, std::string_view namespaceUri) const {
    const std::size_t index = FindAttributeIndex(localName, namespaceUri);
    return index == npos ? nullptr : attributes_[index];
}

std::shared_ptr<XmlAttribute> XmlElement::SetAttribute(std::string_view name, std::string_view value) {
    auto attribute = std::make_shared<XmlAttribute>(std::string(name), std::string(value));
    return SetAttributeNode(attribute);
}

void XmlElement::SetAttribute(std::string_view localName, std::string_view namespaceUri, std::string_view value) {
    const std::string prefix = LookupNamespacePrefix(this, namespaceUri);
    SetAttribute(ComposeQualifiedName(prefix, localName), value);
}

std::shared_ptr<XmlAttribute> XmlElement::SetAttributeNode(const std::shared_ptr<XmlAttribute>& attribute) {
    if (attribute == nullptr) {
        throw XmlException("Cannot set a null XML attribute");
    }

    if (attribute->OwnerElement() != nullptr && attribute->OwnerElement() != this) {
        attribute->OwnerElement()->RemoveAttributeNode(attribute);
    }

    const std::size_t index = FindAttributeIndex(attribute->Name());
    if (index != npos) {
        attributes_[index]->SetOwnerElement(nullptr);
        attributes_[index] = attribute;
    } else {
        attributes_.push_back(attribute);
    }

    attribute->SetOwnerElement(this);
    attribute->SetParent(this);
    attribute->SetOwnerDocument(OwnerDocument());
    return attribute;
}

bool XmlElement::RemoveAttribute(std::string_view name) {
    const std::size_t index = FindAttributeIndex(name);
    if (index == npos) {
        return false;
    }

    attributes_[index]->SetOwnerElement(nullptr);
    attributes_[index]->SetParent(nullptr);
    attributes_.erase(attributes_.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

bool XmlElement::RemoveAttribute(std::string_view localName, std::string_view namespaceUri) {
    const std::size_t index = FindAttributeIndex(localName, namespaceUri);
    if (index == npos) {
        return false;
    }

    attributes_[index]->SetOwnerElement(nullptr);
    attributes_[index]->SetParent(nullptr);
    attributes_.erase(attributes_.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

std::shared_ptr<XmlAttribute> XmlElement::RemoveAttributeNode(const std::shared_ptr<XmlAttribute>& attribute) {
    const auto it = std::find(attributes_.begin(), attributes_.end(), attribute);
    if (it == attributes_.end()) {
        throw XmlException("The attribute is not owned by this element");
    }

    auto removed = *it;
    removed->SetOwnerElement(nullptr);
    removed->SetParent(nullptr);
    attributes_.erase(it);
    return removed;
}

void XmlElement::RemoveAllAttributes() {
    for (auto& attribute : attributes_) {
        attribute->SetOwnerElement(nullptr);
        attribute->SetParent(nullptr);
    }
    attributes_.clear();
}

void XmlElement::SetInnerXml(std::string_view xml) {
    XmlNode::SetInnerXml(xml);
}

bool XmlElement::IsEmpty() const noexcept {
    return !HasChildNodes();
}

std::vector<std::shared_ptr<XmlElement>> XmlElement::GetElementsByTagName(std::string_view name) const {
    std::vector<std::shared_ptr<XmlElement>> result;
    CollectElementsByName(*this, name, result);
    return result;
}

XmlNodeList XmlElement::GetElementsByTagNameList(std::string_view name) const {
    std::vector<std::shared_ptr<XmlNode>> nodes;
    for (auto& element : GetElementsByTagName(name)) {
        nodes.push_back(element);
    }
    return XmlNodeList(std::move(nodes));
}

std::vector<std::shared_ptr<XmlElement>> XmlElement::GetElementsByTagName(std::string_view localName, std::string_view namespaceUri) const {
    std::vector<std::shared_ptr<XmlElement>> result;
    CollectElementsByLocalName(*this, localName, namespaceUri, result);
    return result;
}

XmlNodeList XmlElement::GetElementsByTagNameList(std::string_view localName, std::string_view namespaceUri) const {
    std::vector<std::shared_ptr<XmlNode>> nodes;
    for (auto& element : GetElementsByTagName(localName, namespaceUri)) {
        nodes.push_back(element);
    }
    return XmlNodeList(std::move(nodes));
}

bool XmlElement::WritesFullEndElement() const noexcept {
    return writeFullEndElement_;
}

void XmlElement::SetWritesFullEndElement(bool value) noexcept {
    writeFullEndElement_ = value;
}

std::string XmlElement::FindNamespaceDeclarationValue(std::string_view prefix) const {
    const std::string name = NamespaceDeclarationName(prefix);
    return GetAttribute(name);
}

std::string XmlElement::FindNamespaceDeclarationPrefix(std::string_view namespaceUri) const {
    for (const auto& attribute : attributes_) {
        if (attribute->Value() != namespaceUri) {
            continue;
        }

        if (attribute->Name() == "xmlns") {
            return {};
        }

        constexpr std::string_view namespacePrefix = "xmlns:";
        if (attribute->Name().rfind(namespacePrefix, 0) == 0) {
            return attribute->Name().substr(namespacePrefix.size());
        }
    }

    return {};
}

std::size_t XmlElement::FindAttributeIndex(std::string_view name) const {
    for (std::size_t index = 0; index < attributes_.size(); ++index) {
        if (attributes_[index]->Name() == name) {
            return index;
        }
    }
    return npos;
}

std::size_t XmlElement::FindAttributeIndex(std::string_view localName, std::string_view namespaceUri) const {
    for (std::size_t index = 0; index < attributes_.size(); ++index) {
        if (attributes_[index]->LocalName() == localName && attributes_[index]->NamespaceURI() == namespaceUri) {
            return index;
        }
    }
    return npos;
}

}  // namespace System::Xml
