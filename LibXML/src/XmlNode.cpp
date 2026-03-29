#include "../include/XmlInternal.h"

namespace System::Xml {

namespace {

constexpr std::string_view kXmlNamespaceUri = "http://www.w3.org/XML/1998/namespace";
constexpr std::string_view kXmlnsNamespaceUri = "http://www.w3.org/2000/xmlns/";
constexpr std::size_t kAttributeIndexThreshold = 8;

const std::vector<std::shared_ptr<XmlNode>>& EmptyXmlNodeChildren() noexcept {
    static const std::vector<std::shared_ptr<XmlNode>> empty;
    return empty;
}

std::string BuildValidatedXmlDeclarationValue(
    const std::string& version,
    const std::string& encoding,
    const std::string& standalone) {
    ValidateXmlDeclarationVersion(version);
    ValidateXmlDeclarationEncoding(encoding);
    ValidateXmlDeclarationStandalone(standalone);
    return BuildDeclarationValue(version, encoding, standalone);
}

std::vector<std::shared_ptr<XmlNode>> CloneDocumentTypeDeclarations(const std::vector<std::shared_ptr<XmlNode>>& declarations) {
    std::vector<std::shared_ptr<XmlNode>> clones;
    clones.reserve(declarations.size());
    for (const auto& declaration : declarations) {
        if (declaration == nullptr) {
            continue;
        }

        switch (declaration->NodeType()) {
        case XmlNodeType::Entity: {
            const auto* entity = static_cast<const XmlEntity*>(declaration.get());
            clones.push_back(std::make_shared<XmlEntity>(
                entity->Name(),
                entity->Value(),
                entity->PublicId(),
                entity->SystemId(),
                entity->NotationName()));
            break;
        }
        case XmlNodeType::Notation: {
            const auto* notation = static_cast<const XmlNotation*>(declaration.get());
            clones.push_back(std::make_shared<XmlNotation>(notation->Name(), notation->PublicId(), notation->SystemId()));
            break;
        }
        default:
            throw XmlException("Unsupported DOCTYPE declaration node type");
        }
    }
    return clones;
}

bool PrefixResolvesToNamespaceUri(const XmlElement* element, std::string_view prefix, std::string_view namespaceUri) noexcept {
    if (prefix.empty()) {
        return namespaceUri.empty();
    }

    std::string_view resolvedNamespaceUri;
    return LookupNamespaceUriOnElementView(element, prefix, resolvedNamespaceUri) && resolvedNamespaceUri == namespaceUri;
}

}  // namespace

bool LookupNamespaceUriOnElementView(const XmlElement* element, std::string_view prefix, std::string_view& namespaceUri) noexcept {
    for (auto current = element; current != nullptr; ) {
        if (current->TryFindNamespaceDeclarationValueView(prefix, namespaceUri)) {
            return true;
        }

        const XmlNode* parent = current->ParentNode();
        current = parent != nullptr && parent->NodeType() == XmlNodeType::Element
            ? static_cast<const XmlElement*>(parent)
            : nullptr;
    }

    if (prefix == "xml") {
        namespaceUri = kXmlNamespaceUri;
        return true;
    }
    if (prefix == "xmlns") {
        namespaceUri = kXmlnsNamespaceUri;
        return true;
    }

    namespaceUri = {};
    return false;
}

std::string LookupNamespaceUriOnElement(const XmlElement* element, std::string_view prefix) {
    std::string_view namespaceUri;
    return LookupNamespaceUriOnElementView(element, prefix, namespaceUri) ? std::string(namespaceUri) : std::string{};
}

void LoadXmlDocumentFromReader(XmlReader& reader, XmlDocument& document) {
    document.RemoveAll();
    std::vector<std::shared_ptr<XmlElement>> elementStack;
    int entityExpansionDepth = 0;
    const bool useFastAppend = !document.HasNodeChangeHandlers();

    const auto createOwnedTextNode = [&document, &reader]() {
        reader.MaterializeValue();
        return document.AllocateOwnedNode<XmlText>(std::move(reader.currentValue_));
    };

    const auto createOwnedWhitespaceNode = [&document, &reader]() {
        reader.MaterializeValue();
        return document.AllocateOwnedNode<XmlWhitespace>(std::move(reader.currentValue_));
    };

    const auto createOwnedSignificantWhitespaceNode = [&document, &reader]() {
        reader.MaterializeValue();
        return document.AllocateOwnedNode<XmlSignificantWhitespace>(std::move(reader.currentValue_));
    };

    const auto createOwnedCDataNode = [&document, &reader]() {
        reader.MaterializeValue();
        return document.AllocateOwnedNode<XmlCDataSection>(std::move(reader.currentValue_));
    };

    const auto createOwnedCommentNode = [&document, &reader]() {
        reader.MaterializeValue();
        return document.AllocateOwnedNode<XmlComment>(std::move(reader.currentValue_));
    };

    const auto createOwnedElementNode = [&document, &reader]() {
        auto node = document.AllocateOwnedNode<XmlElement>(std::move(reader.currentName_));
        reader.AppendCurrentAttributesForLoad(*node);
        return node;
    };

    const auto collectDocumentTypeDeclarations = [&reader](const auto& declaration) {
        std::vector<std::shared_ptr<XmlNode>> entities;
        std::vector<std::shared_ptr<XmlNode>> notations;

        if (reader.dtdState_ != nullptr) {
            entities = reader.dtdState_->parsedEntities;
            notations = reader.dtdState_->parsedNotations;
        }

        if ((!entities.empty() || !notations.empty()) || reader.settings_.DtdProcessing != DtdProcessing::Parse) {
            return std::pair{std::move(entities), std::move(notations)};
        }

        if (!declaration.internalSubset.empty()) {
            ParseDocumentTypeInternalSubset(declaration.internalSubset, entities, notations);
        }

        if (reader.settings_.Resolver != nullptr && !declaration.systemId.empty()) {
            const std::string absoluteUri = reader.settings_.Resolver->ResolveUri(
                reader.baseUri_,
                declaration.systemId);
            const std::string externalSubset = Trim(reader.settings_.Resolver->GetEntity(absoluteUri));
            if (!externalSubset.empty()) {
                ParseDocumentTypeInternalSubset(externalSubset, entities, notations, true);
            }
        }

        return std::pair{std::move(entities), std::move(notations)};
    };

    const auto appendNode = [&](std::shared_ptr<XmlNode> node, bool ownerAlreadyAssigned) {
        if (node == nullptr) {
            return;
        }
        if (elementStack.empty()) {
            if (useFastAppend) {
                if (ownerAlreadyAssigned) {
                    document.AppendChildForOwnedLoad(node);
                } else {
                    document.AppendChildForLoad(node);
                }
            } else {
                document.AppendChild(node);
            }
            return;
        }

        if (useFastAppend) {
            if (ownerAlreadyAssigned) {
                elementStack.back()->AppendChildForOwnedLoad(node);
            } else {
                elementStack.back()->AppendChildForLoad(node);
            }
        } else {
            elementStack.back()->AppendChild(node);
        }
    };

    const auto appendCurrentReaderNode = [&]() {
        const auto nodeType = reader.NodeType();
        if (entityExpansionDepth > 0) {
            if (nodeType == XmlNodeType::EntityReference) {
                ++entityExpansionDepth;
            } else if (nodeType == XmlNodeType::EndEntity) {
                --entityExpansionDepth;
            }
            return;
        }

        switch (nodeType) {
        case XmlNodeType::Element: {
            auto element = createOwnedElementNode();
            appendNode(element, true);
            if (!reader.IsEmptyElement()) {
                elementStack.push_back(std::move(element));
            }
            break;
        }
        case XmlNodeType::EndElement:
            if (!elementStack.empty()) {
                elementStack.pop_back();
            }
            break;
        case XmlNodeType::Text:
            appendNode(createOwnedTextNode(), true);
            break;
        case XmlNodeType::CDATA:
            appendNode(createOwnedCDataNode(), true);
            break;
        case XmlNodeType::Whitespace:
            if (document.PreserveWhitespace()) {
                appendNode(createOwnedWhitespaceNode(), true);
            }
            break;
        case XmlNodeType::SignificantWhitespace:
            appendNode(createOwnedSignificantWhitespaceNode(), true);
            break;
        case XmlNodeType::Comment:
            appendNode(createOwnedCommentNode(), true);
            break;
        case XmlNodeType::ProcessingInstruction:
            appendNode(document.CreateProcessingInstruction(reader.Name(), reader.Value()), true);
            break;
        case XmlNodeType::XmlDeclaration: {
            const auto declaration = ParseXmlDeclarationValue(reader.Value());
            appendNode(document.CreateXmlDeclaration(declaration.version, declaration.encoding, declaration.standalone), true);
            break;
        }
        case XmlNodeType::DocumentType: {
            const auto declaration = ParseDocumentTypeDeclaration(reader.ReadOuterXml());
            auto [entities, notations] = collectDocumentTypeDeclarations(declaration);
            appendNode(document.AllocateOwnedNode<XmlDocumentType>(
                declaration.name,
                declaration.publicId,
                declaration.systemId,
                declaration.internalSubset,
                std::move(entities),
                std::move(notations)),
                false);
            break;
        }
        case XmlNodeType::EntityReference:
            if (const std::string& resolvedValue = reader.Value(); !resolvedValue.empty()) {
                appendNode(document.AllocateOwnedNode<XmlEntityReference>(reader.Name(), resolvedValue), true);
            } else {
                appendNode(document.CreateEntityReference(reader.Name()), true);
            }
            entityExpansionDepth = 1;
            break;
        case XmlNodeType::EndEntity:
        case XmlNodeType::None:
        case XmlNodeType::Attribute:
        case XmlNodeType::Entity:
        case XmlNodeType::DocumentFragment:
        case XmlNodeType::Notation:
            break;
        }
    };

    if (reader.GetReadState() == ReadState::Interactive && reader.NodeType() != XmlNodeType::None) {
        appendCurrentReaderNode();
    }

    while (reader.Read()) {
        appendCurrentReaderNode();
    }
}

XmlNodeList::XmlNodeList(std::vector<std::shared_ptr<XmlNode>> nodes) : nodes_(std::move(nodes)) {
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
    const auto found = std::find_if(attributes_.begin(), attributes_.end(), [&name](const auto& attribute) {
        return attribute->Name() == name || attribute->LocalName() == name;
    });
    return found == attributes_.end() ? nullptr : *found;
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

XmlNamedNodeMap::XmlNamedNodeMap(std::vector<std::shared_ptr<XmlNode>> nodes) : nodes_(std::move(nodes)) {
}

std::size_t XmlNamedNodeMap::Count() const noexcept {
    return nodes_.size();
}

std::shared_ptr<XmlNode> XmlNamedNodeMap::Item(std::size_t index) const {
    return index < nodes_.size() ? nodes_[index] : nullptr;
}

std::shared_ptr<XmlNode> XmlNamedNodeMap::GetNamedItem(std::string_view name) const {
    const auto found = std::find_if(nodes_.begin(), nodes_.end(), [&name](const auto& node) {
        return node != nullptr && node->Name() == name;
    });
    return found == nodes_.end() ? nullptr : *found;
}

bool XmlNamedNodeMap::Empty() const noexcept {
    return nodes_.empty();
}

std::vector<std::shared_ptr<XmlNode>>::const_iterator XmlNamedNodeMap::begin() const noexcept {
    return nodes_.begin();
}

std::vector<std::shared_ptr<XmlNode>>::const_iterator XmlNamedNodeMap::end() const noexcept {
    return nodes_.end();
}

XmlNode::XmlNode(XmlNodeType nodeType, std::string name, std::string value)
    : nodeType_(nodeType),
      name_(std::move(name)),
      value_(std::move(value)),
      parent_(nullptr),
      ownerDocument_(nullptr),
      siblingIndex_(DetachedSiblingIndex) {
}

XmlNodeType XmlNode::NodeType() const noexcept {
    return nodeType_;
}

const std::string& XmlNode::Name() const noexcept {
    return name_;
}

std::string XmlNode::LocalName() const {
    return std::string{SplitQualifiedNameView(Name()).second};
}

std::string XmlNode::Prefix() const {
    return std::string{SplitQualifiedNameView(Name()).first};
}

std::string XmlNode::NamespaceURI() const {
    return ResolveNodeNamespaceUri(*this);
}

std::string XmlNode::GetNamespaceOfPrefix(std::string_view prefix) const {
    if (NodeType() == XmlNodeType::Element) {
        return LookupNamespaceUriOnElement(static_cast<const XmlElement*>(this), prefix);
    }
    if (NodeType() == XmlNodeType::Attribute) {
        const auto* parent = ParentNode();
        if (parent != nullptr && parent->NodeType() == XmlNodeType::Element) {
            return LookupNamespaceUriOnElement(static_cast<const XmlElement*>(parent), prefix);
        }
    }
    // Walk up to nearest element ancestor
    for (const auto* current = ParentNode(); current != nullptr; current = current->ParentNode()) {
        if (current->NodeType() == XmlNodeType::Element) {
            return LookupNamespaceUriOnElement(static_cast<const XmlElement*>(current), prefix);
        }
    }
    if (prefix == "xml") return "http://www.w3.org/XML/1998/namespace";
    if (prefix == "xmlns") return "http://www.w3.org/2000/xmlns/";
    return {};
}

std::string XmlNode::GetPrefixOfNamespace(std::string_view namespaceUri) const {
    if (namespaceUri.empty()) return {};
    if (NodeType() == XmlNodeType::Element) {
        return LookupPrefixOnElement(static_cast<const XmlElement*>(this), namespaceUri);
    }
    if (NodeType() == XmlNodeType::Attribute) {
        const auto* parent = ParentNode();
        if (parent != nullptr && parent->NodeType() == XmlNodeType::Element) {
            return LookupPrefixOnElement(static_cast<const XmlElement*>(parent), namespaceUri);
        }
    }
    for (const auto* current = ParentNode(); current != nullptr; current = current->ParentNode()) {
        if (current->NodeType() == XmlNodeType::Element) {
            return LookupPrefixOnElement(static_cast<const XmlElement*>(current), namespaceUri);
        }
    }
    if (namespaceUri == "http://www.w3.org/XML/1998/namespace") return "xml";
    if (namespaceUri == "http://www.w3.org/2000/xmlns/") return "xmlns";
    return {};
}

const std::string& XmlNode::Value() const noexcept {
    return value_;
}

void XmlNode::SetValue(std::string_view value) {
    switch (NodeType()) {
    case XmlNodeType::Attribute:
    case XmlNodeType::Text:
    case XmlNodeType::CDATA:
    case XmlNodeType::Comment:
    case XmlNodeType::ProcessingInstruction:
    case XmlNodeType::Whitespace:
    case XmlNodeType::SignificantWhitespace: {
        const std::string oldValue = value_;
        value_ = value;
        if (ownerDocument_ != nullptr) {
            ownerDocument_->FireNodeChanging(this, oldValue, value_);
        }
        if (ownerDocument_ != nullptr) {
            ownerDocument_->FireNodeChanged(this, oldValue, value_);
        }
        return;
    }
    default:
        throw XmlException(std::string("Cannot set a value on node type '") + NodeTypeDisplayName(NodeType()) + "'.");
    }
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
    return childNodes_ == nullptr ? EmptyXmlNodeChildren() : *childNodes_;
}

XmlNodeList XmlNode::ChildNodeList() const {
    return XmlNodeList(childNodes_ == nullptr ? std::vector<std::shared_ptr<XmlNode>>{} : *childNodes_);
}

std::vector<std::shared_ptr<XmlNode>>& XmlNode::MutableChildNodes() noexcept {
    if (childNodes_ == nullptr) {
        childNodes_ = std::make_unique<std::vector<std::shared_ptr<XmlNode>>>();
    }
    return *childNodes_;
}

std::shared_ptr<XmlNode> XmlNode::FirstChild() const {
    const auto& children = ChildNodes();
    return children.empty() ? nullptr : children.front();
}

std::shared_ptr<XmlNode> XmlNode::LastChild() const {
    const auto& children = ChildNodes();
    return children.empty() ? nullptr : children.back();
}

std::shared_ptr<XmlNode> XmlNode::PreviousSibling() const {
    if (parent_ == nullptr) {
        return nullptr;
    }

    const auto& siblings = parent_->ChildNodes();
    const auto siblingIndex = ResolveSiblingIndex();
    if (siblingIndex == DetachedSiblingIndex || siblingIndex == 0 || siblingIndex > siblings.size()) {
        return nullptr;
    }

    return siblings[siblingIndex - 1];
}

std::shared_ptr<XmlNode> XmlNode::NextSibling() const {
    if (parent_ == nullptr) {
        return nullptr;
    }

    const auto& siblings = parent_->ChildNodes();
    const auto siblingIndex = ResolveSiblingIndex();
    if (siblingIndex == DetachedSiblingIndex || siblingIndex + 1 >= siblings.size()) {
        return nullptr;
    }

    return siblings[siblingIndex + 1];
}

std::shared_ptr<XmlNode> XmlNode::SharedFromParent() const {
    if (parent_ == nullptr || NodeType() == XmlNodeType::Attribute) {
        return nullptr;
    }

    const auto& siblings = parent_->ChildNodes();
    const auto siblingIndex = ResolveSiblingIndex();
    if (siblingIndex == DetachedSiblingIndex || siblingIndex >= siblings.size()) {
        return nullptr;
    }

    const auto& sibling = siblings[siblingIndex];
    return sibling != nullptr && sibling.get() == this ? sibling : nullptr;
}

bool XmlNode::HasChildNodes() const noexcept {
    return childNodes_ != nullptr && !childNodes_->empty();
}

void XmlNode::ValidateChildInsertion(const std::shared_ptr<XmlNode>& child, const XmlNode* replacingChild) const {
    if (!child) {
        throw XmlException("Cannot append a null child node");
    }
    if (child.get() == this) {
        throw XmlException("A node cannot be appended to itself");
    }
    if (child->OwnerDocument() != nullptr && OwnerDocument() != nullptr && child->OwnerDocument() != OwnerDocument()) {
        throw XmlException("The node belongs to a different document");
    }
    if (child->NodeType() == XmlNodeType::DocumentFragment) {
        for (const auto& fragmentChild : child->ChildNodes()) {
            if (!fragmentChild) {
                throw XmlException("Document fragments cannot contain null child nodes");
            }
            if (fragmentChild.get() == this) {
                throw XmlException("A node cannot be appended to itself");
            }
            if (fragmentChild->NodeType() == XmlNodeType::Attribute) {
                throw XmlException("Attributes cannot be appended as child nodes");
            }
            if (fragmentChild->OwnerDocument() != nullptr
                && OwnerDocument() != nullptr
                && fragmentChild->OwnerDocument() != OwnerDocument()) {
                throw XmlException("The node belongs to a different document");
            }
            if (fragmentChild->ParentNode() != nullptr
                && fragmentChild->ParentNode() != child.get()
                && fragmentChild->ParentNode() != this) {
                throw XmlException("The node already has a parent");
            }
            if (fragmentChild->ParentNode() == this && fragmentChild.get() != replacingChild) {
                throw XmlException("The node already belongs to the current parent");
            }
        }
        return;
    }
    if (child->NodeType() == XmlNodeType::Attribute) {
        throw XmlException("Attributes cannot be appended as child nodes");
    }
    if (child->ParentNode() != nullptr && child->ParentNode() != this) {
        throw XmlException("The node already has a parent");
    }
    if (child->ParentNode() == this && child.get() != replacingChild) {
        throw XmlException("The node already belongs to the current parent");
    }
}

std::shared_ptr<XmlNode> XmlNode::AppendChild(const std::shared_ptr<XmlNode>& child) {
    ValidateChildInsertion(child);

    if (child->NodeType() == XmlNodeType::DocumentFragment) {
        while (child->FirstChild() != nullptr) {
            auto fragmentChild = child->RemoveChild(child->FirstChild());
            AppendChild(fragmentChild);
        }
        return child;
    }

    if (ownerDocument_ != nullptr) {
        ownerDocument_->FireNodeInserting(child.get(), this);
    }
    auto& children = MutableChildNodes();
    child->SetParent(this);
    if (child->OwnerDocument() != ownerDocument_) {
        child->SetOwnerDocumentRecursive(ownerDocument_);
    }
    child->SetSiblingIndex(children.size());
    children.push_back(child);
    if (ownerDocument_ != nullptr) {
        ownerDocument_->FireNodeInserted(child.get(), this);
    }
    return child;
}

std::shared_ptr<XmlNode> XmlNode::AppendChildForLoad(const std::shared_ptr<XmlNode>& child) {
    if (child == nullptr) {
        return child;
    }

    auto& children = MutableChildNodes();
    child->SetParent(this);
    if (child->OwnerDocument() != ownerDocument_) {
        child->SetOwnerDocumentRecursive(ownerDocument_);
    }
    child->SetSiblingIndex(children.size());
    children.push_back(child);
    return child;
}

std::shared_ptr<XmlNode> XmlNode::AppendChildForOwnedLoad(const std::shared_ptr<XmlNode>& child) {
    if (child == nullptr) {
        return child;
    }

    auto& children = MutableChildNodes();
    child->SetParent(this);
    child->SetSiblingIndex(children.size());
    children.push_back(child);
    return child;
}

std::shared_ptr<XmlNode> XmlNode::InsertBefore(
    const std::shared_ptr<XmlNode>& newChild,
    const std::shared_ptr<XmlNode>& referenceChild) {
    if (!referenceChild) {
        return AppendChild(newChild);
    }

    const auto index = FindChildIndexOrThrow(referenceChild);
    ValidateChildInsertion(newChild);

    if (newChild->NodeType() == XmlNodeType::DocumentFragment) {
        auto& children = MutableChildNodes();
        auto insertionIndex = index;
        while (newChild->FirstChild() != nullptr) {
            auto fragmentChild = newChild->RemoveChild(newChild->FirstChild());
            ValidateChildInsertion(fragmentChild);
            if (ownerDocument_ != nullptr) {
                ownerDocument_->FireNodeInserting(fragmentChild.get(), this);
            }
            fragmentChild->SetParent(this);
            fragmentChild->SetOwnerDocumentRecursive(ownerDocument_);
            children.insert(children.begin() + static_cast<std::ptrdiff_t>(insertionIndex), fragmentChild);
            if (ownerDocument_ != nullptr) {
                ownerDocument_->FireNodeInserted(fragmentChild.get(), this);
            }
            ++insertionIndex;
        }
        ReindexChildNodesFrom(index);
        return newChild;
    }

    if (ownerDocument_ != nullptr) {
        ownerDocument_->FireNodeInserting(newChild.get(), this);
    }
    auto& children = MutableChildNodes();
    newChild->SetParent(this);
    newChild->SetOwnerDocumentRecursive(ownerDocument_);
    children.insert(children.begin() + static_cast<std::ptrdiff_t>(index), newChild);
    ReindexChildNodesFrom(index);
    if (ownerDocument_ != nullptr) {
        ownerDocument_->FireNodeInserted(newChild.get(), this);
    }
    return newChild;
}

std::shared_ptr<XmlNode> XmlNode::InsertAfter(
    const std::shared_ptr<XmlNode>& newChild,
    const std::shared_ptr<XmlNode>& referenceChild) {
    if (!referenceChild) {
        ValidateChildInsertion(newChild);
        if (ownerDocument_ != nullptr) {
            ownerDocument_->FireNodeInserting(newChild.get(), this);
        }
        auto& children = MutableChildNodes();
        newChild->SetParent(this);
        newChild->SetOwnerDocumentRecursive(ownerDocument_);
        children.insert(children.begin(), newChild);
        ReindexChildNodesFrom(0);
        if (ownerDocument_ != nullptr) {
            ownerDocument_->FireNodeInserted(newChild.get(), this);
        }
        return newChild;
    }

    const auto index = FindChildIndexOrThrow(referenceChild);
    ValidateChildInsertion(newChild);

    if (newChild->NodeType() == XmlNodeType::DocumentFragment) {
        auto& children = MutableChildNodes();
        auto insertionIndex = index + 1;
        while (newChild->FirstChild() != nullptr) {
            auto fragmentChild = newChild->RemoveChild(newChild->FirstChild());
            ValidateChildInsertion(fragmentChild);
            if (ownerDocument_ != nullptr) {
                ownerDocument_->FireNodeInserting(fragmentChild.get(), this);
            }
            fragmentChild->SetParent(this);
            fragmentChild->SetOwnerDocumentRecursive(ownerDocument_);
            children.insert(children.begin() + static_cast<std::ptrdiff_t>(insertionIndex), fragmentChild);
            if (ownerDocument_ != nullptr) {
                ownerDocument_->FireNodeInserted(fragmentChild.get(), this);
            }
            ++insertionIndex;
        }
        ReindexChildNodesFrom(index + 1);
        return newChild;
    }

    if (ownerDocument_ != nullptr) {
        ownerDocument_->FireNodeInserting(newChild.get(), this);
    }
    auto& children = MutableChildNodes();
    newChild->SetParent(this);
    newChild->SetOwnerDocumentRecursive(ownerDocument_);
    children.insert(children.begin() + static_cast<std::ptrdiff_t>(index + 1), newChild);
    ReindexChildNodesFrom(index + 1);
    if (ownerDocument_ != nullptr) {
        ownerDocument_->FireNodeInserted(newChild.get(), this);
    }
    return newChild;
}

std::shared_ptr<XmlNode> XmlNode::ReplaceChild(
    const std::shared_ptr<XmlNode>& newChild,
    const std::shared_ptr<XmlNode>& oldChild) {
    if (newChild == oldChild) {
        return oldChild;
    }

    const auto index = FindChildIndexOrThrow(oldChild);
    ValidateChildInsertion(newChild, oldChild.get());

    if (newChild->NodeType() == XmlNodeType::DocumentFragment) {
        auto& children = MutableChildNodes();
        if (ownerDocument_ != nullptr) {
            ownerDocument_->FireNodeRemoving(oldChild.get(), this);
        }
        oldChild->SetParent(nullptr);
        children.erase(children.begin() + static_cast<std::ptrdiff_t>(index));
        if (ownerDocument_ != nullptr) {
            ownerDocument_->FireNodeRemoved(oldChild.get(), this);
        }

        auto insertionIndex = index;
        while (newChild->FirstChild() != nullptr) {
            auto fragmentChild = newChild->RemoveChild(newChild->FirstChild());
            ValidateChildInsertion(fragmentChild);
            if (ownerDocument_ != nullptr) {
                ownerDocument_->FireNodeInserting(fragmentChild.get(), this);
            }
            fragmentChild->SetParent(this);
            fragmentChild->SetOwnerDocumentRecursive(ownerDocument_);
            children.insert(children.begin() + static_cast<std::ptrdiff_t>(insertionIndex), fragmentChild);
            if (ownerDocument_ != nullptr) {
                ownerDocument_->FireNodeInserted(fragmentChild.get(), this);
            }
            ++insertionIndex;
        }
        ReindexChildNodesFrom(index);
        return oldChild;
    }

    if (ownerDocument_ != nullptr) {
        ownerDocument_->FireNodeRemoving(oldChild.get(), this);
        ownerDocument_->FireNodeInserting(newChild.get(), this);
    }
    oldChild->SetParent(nullptr);
    newChild->SetParent(this);
    newChild->SetOwnerDocumentRecursive(ownerDocument_);
    MutableChildNodes()[index] = newChild;
    newChild->SetSiblingIndex(index);
    if (ownerDocument_ != nullptr) {
        ownerDocument_->FireNodeRemoved(oldChild.get(), this);
        ownerDocument_->FireNodeInserted(newChild.get(), this);
    }
    return oldChild;
}

std::shared_ptr<XmlNode> XmlNode::RemoveChild(const std::shared_ptr<XmlNode>& child) {
    const auto index = FindChildIndexOrThrow(child);
    auto& children = MutableChildNodes();
    auto removed = children[index];
    if (ownerDocument_ != nullptr) {
        ownerDocument_->FireNodeRemoving(removed.get(), this);
    }
    removed->SetParent(nullptr);
    children.erase(children.begin() + static_cast<std::ptrdiff_t>(index));
    ReindexChildNodesFrom(index);
    if (ownerDocument_ != nullptr) {
        ownerDocument_->FireNodeRemoved(removed.get(), this);
    }
    return removed;
}

void XmlNode::RemoveAllChildren() {
    if (childNodes_ == nullptr) {
        return;
    }

    for (const auto& child : *childNodes_) {
        child->SetParent(nullptr);
    }
    childNodes_->clear();
    childNodes_.reset();
}

void XmlNode::RemoveAll() {
    if (auto* element = dynamic_cast<XmlElement*>(this)) {
        element->RemoveAllAttributes();
    }
    RemoveAllChildren();
}

std::shared_ptr<XmlNode> XmlNode::SelectSingleNode(std::string_view xpath) const {
    const auto nodes = SelectNodes(xpath);
    return nodes.Item(0);
}

std::shared_ptr<XmlNode> XmlNode::SelectSingleNode(std::string_view xpath, const XmlNamespaceManager& namespaces) const {
    const auto nodes = SelectNodes(xpath, namespaces);
    return nodes.Item(0);
}

XmlNodeList XmlNode::SelectNodes(std::string_view xpath) const {
    return EvaluateXPathFromNode(*this, xpath, nullptr);
}

XmlNodeList XmlNode::SelectNodes(std::string_view xpath, const XmlNamespaceManager& namespaces) const {
    return EvaluateXPathFromNode(*this, xpath, &namespaces);
}

XPathValue XmlNode::Evaluate(std::string_view xpath) const {
    return CreateNavigator().Evaluate(xpath);
}

XPathValue XmlNode::Evaluate(std::string_view xpath, const XmlNamespaceManager& namespaces) const {
    return CreateNavigator().Evaluate(xpath, namespaces);
}

XPathValue XmlNode::Evaluate(const XPathExpression& expression) const {
    return CreateNavigator().Evaluate(expression);
}

XPathValue XmlNode::Evaluate(const XPathExpression& expression, const XmlNamespaceManager& namespaces) const {
    return CreateNavigator().Evaluate(expression, namespaces);
}

std::string XmlNode::InnerText() const {
    if (NodeType() == XmlNodeType::Text
        || NodeType() == XmlNodeType::EntityReference
        || NodeType() == XmlNodeType::CDATA
        || NodeType() == XmlNodeType::Whitespace
        || NodeType() == XmlNodeType::SignificantWhitespace
        || NodeType() == XmlNodeType::Comment
        || NodeType() == XmlNodeType::ProcessingInstruction
        || NodeType() == XmlNodeType::XmlDeclaration) {
        return Value();
    }

    std::string text;
    for (const auto& child : ChildNodes()) {
        if (child->NodeType() == XmlNodeType::Comment
            || child->NodeType() == XmlNodeType::ProcessingInstruction
            || child->NodeType() == XmlNodeType::XmlDeclaration
            || child->NodeType() == XmlNodeType::DocumentType) {
            continue;
        }
        text += child->InnerText();
    }
    return text;
}

void XmlNode::SetInnerText(std::string_view text) {
    switch (NodeType()) {
    case XmlNodeType::Document:
    case XmlNodeType::DocumentType:
    case XmlNodeType::DocumentFragment:
        throw XmlException("Cannot set InnerText on " + Name());
    case XmlNodeType::Text:
    case XmlNodeType::CDATA:
    case XmlNodeType::Comment:
    case XmlNodeType::Whitespace:
    case XmlNodeType::SignificantWhitespace:
    case XmlNodeType::ProcessingInstruction:
        SetValue(text);
        return;
    default:
        break;
    }
    RemoveAllChildren();
    if (!text.empty()) {
        auto ownerDoc = OwnerDocument();
        auto textNode = ownerDoc ? ownerDoc->CreateTextNode(std::string(text)) : std::make_shared<XmlText>(std::string(text));
        AppendChild(textNode);
    }
}

std::string XmlNode::InnerXml(const XmlWriterSettings& settings) const {
    std::string xml;
    for (const auto& child : ChildNodes()) {
        xml += XmlWriter::WriteToString(*child, settings);
    }
    return xml;
}

void XmlNode::SetInnerXml(std::string_view xml) {
    (void)xml;
    throw XmlException("Setting InnerXml is not supported for " + Name());
}

std::string XmlNode::OuterXml(const XmlWriterSettings& settings) const {
    return XmlWriter::WriteToString(*this, settings);
}

std::shared_ptr<XmlNode> XmlNode::CloneNode(bool deep) const {
    std::shared_ptr<XmlNode> clone;
    XmlDocument* ownerDocument = ownerDocument_;

    switch (NodeType()) {
    case XmlNodeType::Document: {
        auto documentClone = std::make_shared<XmlDocument>();
        documentClone->SetPreserveWhitespace(static_cast<const XmlDocument*>(this)->PreserveWhitespace());
        if (deep) {
            for (const auto& child : ChildNodes()) {
                auto childClone = child->CloneNode(true);
                childClone->SetOwnerDocumentRecursive(documentClone.get());
                documentClone->AppendChild(childClone);
            }
        }
        return documentClone;
    }
    case XmlNodeType::Element: {
        auto elementClone = ownerDocument != nullptr
            ? ownerDocument->AllocateNode<XmlElement>(Name())
            : std::make_shared<XmlElement>(Name());
        const auto* sourceElement = static_cast<const XmlElement*>(this);
        elementClone->writeFullEndElement_ = sourceElement->writeFullEndElement_;
        for (const auto& attribute : sourceElement->Attributes()) {
            elementClone->SetAttribute(attribute->Name(), attribute->Value());
        }
        clone = elementClone;
        break;
    }
    case XmlNodeType::Attribute:
        clone = ownerDocument != nullptr
            ? ownerDocument->AllocateNode<XmlAttribute>(Name(), Value())
            : std::make_shared<XmlAttribute>(Name(), Value());
        break;
    case XmlNodeType::Text:
        clone = ownerDocument != nullptr
            ? ownerDocument->AllocateNode<XmlText>(Value())
            : std::make_shared<XmlText>(Value());
        break;
    case XmlNodeType::EntityReference:
        clone = ownerDocument != nullptr
            ? ownerDocument->AllocateNode<XmlEntityReference>(Name(), Value())
            : std::make_shared<XmlEntityReference>(Name(), Value());
        break;
    case XmlNodeType::Entity: {
        const auto* entity = static_cast<const XmlEntity*>(this);
        clone = std::make_shared<XmlEntity>(
            entity->Name(),
            entity->Value(),
            entity->PublicId(),
            entity->SystemId(),
            entity->NotationName());
        break;
    }
    case XmlNodeType::Notation: {
        const auto* notation = static_cast<const XmlNotation*>(this);
        clone = std::make_shared<XmlNotation>(notation->Name(), notation->PublicId(), notation->SystemId());
        break;
    }
    case XmlNodeType::Whitespace:
        clone = ownerDocument != nullptr
            ? ownerDocument->AllocateNode<XmlWhitespace>(Value())
            : std::make_shared<XmlWhitespace>(Value());
        break;
    case XmlNodeType::SignificantWhitespace:
        clone = ownerDocument != nullptr
            ? ownerDocument->AllocateNode<XmlSignificantWhitespace>(Value())
            : std::make_shared<XmlSignificantWhitespace>(Value());
        break;
    case XmlNodeType::CDATA:
        clone = ownerDocument != nullptr
            ? ownerDocument->AllocateNode<XmlCDataSection>(Value())
            : std::make_shared<XmlCDataSection>(Value());
        break;
    case XmlNodeType::Comment:
        clone = ownerDocument != nullptr
            ? ownerDocument->AllocateNode<XmlComment>(Value())
            : std::make_shared<XmlComment>(Value());
        break;
    case XmlNodeType::ProcessingInstruction: {
        const auto* instruction = static_cast<const XmlProcessingInstruction*>(this);
        clone = ownerDocument != nullptr
            ? ownerDocument->AllocateNode<XmlProcessingInstruction>(instruction->Target(), instruction->Data())
            : std::make_shared<XmlProcessingInstruction>(instruction->Target(), instruction->Data());
        break;
    }
    case XmlNodeType::XmlDeclaration: {
        const auto* declaration = static_cast<const XmlDeclaration*>(this);
        clone = ownerDocument != nullptr
            ? ownerDocument->AllocateNode<XmlDeclaration>(declaration->Version(), declaration->Encoding(), declaration->Standalone())
            : std::make_shared<XmlDeclaration>(declaration->Version(), declaration->Encoding(), declaration->Standalone());
        break;
    }
    case XmlNodeType::DocumentType: {
        const auto* documentType = static_cast<const XmlDocumentType*>(this);
        clone = ownerDocument != nullptr
            ? ownerDocument->AllocateNode<XmlDocumentType>(
                documentType->Name(),
                documentType->PublicId(),
                documentType->SystemId(),
                documentType->InternalSubset(),
                documentType->entities_,
                documentType->notations_)
            : std::make_shared<XmlDocumentType>(
                documentType->Name(),
                documentType->PublicId(),
                documentType->SystemId(),
                documentType->InternalSubset(),
                documentType->entities_,
                documentType->notations_);
        break;
    }
    case XmlNodeType::DocumentFragment:
        clone = ownerDocument != nullptr
            ? ownerDocument->AllocateNode<XmlDocumentFragment>()
            : std::make_shared<XmlDocumentFragment>();
        break;
    default:
        throw XmlException("CloneNode does not support the specified node type");
    }

    if (ownerDocument_ != nullptr) {
        clone->SetOwnerDocumentRecursive(ownerDocument_);
    }

    if (deep && NodeType() != XmlNodeType::Attribute) {
        for (const auto& child : ChildNodes()) {
            clone->AppendChild(child->CloneNode(true));
        }
    }

    return clone;
}

XPathNavigator XmlNode::CreateNavigator() const {
    return XPathNavigator(this);
}

void XmlNode::SetOwnerDocument(XmlDocument* ownerDocument) noexcept {
    ownerDocument_ = ownerDocument;
}

void XmlNode::SetOwnerDocumentRecursive(XmlDocument* ownerDocument) {
    ownerDocument_ = ownerDocument;

    if (auto* element = dynamic_cast<XmlElement*>(this)) {
        for (const auto& attribute : element->attributes_) {
            attribute->SetOwnerDocumentRecursive(ownerDocument);
        }
    }

    if (auto* documentType = dynamic_cast<XmlDocumentType*>(this)) {
        for (const auto& entity : documentType->entities_) {
            entity->SetOwnerDocumentRecursive(ownerDocument);
        }
        for (const auto& notation : documentType->notations_) {
            notation->SetOwnerDocumentRecursive(ownerDocument);
        }
    }

    for (const auto& child : ChildNodes()) {
        child->SetOwnerDocumentRecursive(ownerDocument);
    }
}

void XmlNode::SetParent(XmlNode* parent) noexcept {
    parent_ = parent;
    if (parent == nullptr) {
        siblingIndex_ = DetachedSiblingIndex;
    }
}

std::size_t XmlNode::FindChildIndexOrThrow(const std::shared_ptr<XmlNode>& child) const {
    if (child == nullptr || child->ParentNode() != this) {
        throw XmlException("Reference child is not a child of the current node");
    }

    const auto& children = ChildNodes();
    const auto siblingIndex = child->ResolveSiblingIndex();
    if (siblingIndex != DetachedSiblingIndex && siblingIndex < children.size() && children[siblingIndex].get() == child.get()) {
        return siblingIndex;
    }

    const auto found = std::find_if(children.begin(), children.end(), [&child](const auto& current) {
        return current.get() == child.get();
    });
    if (found == children.end()) {
        throw XmlException("Reference child is not a child of the current node");
    }

    const auto resolvedIndex = static_cast<std::size_t>(std::distance(children.begin(), found));
    child->SetSiblingIndex(resolvedIndex);
    return resolvedIndex;
}

void XmlNode::SetSiblingIndex(std::size_t siblingIndex) noexcept {
    siblingIndex_ = siblingIndex;
}

std::size_t XmlNode::ResolveSiblingIndex() const noexcept {
    if (parent_ == nullptr || siblingIndex_ == DetachedSiblingIndex) {
        return DetachedSiblingIndex;
    }

    const auto& siblings = parent_->ChildNodes();
    if (siblingIndex_ < siblings.size() && siblings[siblingIndex_].get() == this) {
        return siblingIndex_;
    }

    const auto found = std::find_if(siblings.begin(), siblings.end(), [this](const auto& sibling) {
        return sibling.get() == this;
    });
    if (found == siblings.end()) {
        return DetachedSiblingIndex;
    }

    const auto resolvedIndex = static_cast<std::size_t>(std::distance(siblings.begin(), found));
    const_cast<XmlNode*>(this)->siblingIndex_ = resolvedIndex;
    return resolvedIndex;
}

void XmlNode::ReindexChildNodesFrom(std::size_t startIndex) noexcept {
    if (childNodes_ == nullptr || startIndex >= childNodes_->size()) {
        return;
    }

    for (std::size_t index = startIndex; index < childNodes_->size(); ++index) {
        auto& child = (*childNodes_)[index];
        if (child != nullptr) {
            child->parent_ = this;
            child->siblingIndex_ = index;
        }
    }
}

XmlAttribute::XmlAttribute(std::string name, std::string value)
    : XmlNode(XmlNodeType::Attribute, std::move(name), std::move(value)) {
}

XmlElement* XmlAttribute::OwnerElement() noexcept {
    return ParentNode() != nullptr && ParentNode()->NodeType() == XmlNodeType::Element
        ? static_cast<XmlElement*>(ParentNode())
        : nullptr;
}

const XmlElement* XmlAttribute::OwnerElement() const noexcept {
    return ParentNode() != nullptr && ParentNode()->NodeType() == XmlNodeType::Element
        ? static_cast<const XmlElement*>(ParentNode())
        : nullptr;
}

std::shared_ptr<XmlAttribute> XmlAttribute::SharedFromOwnerElement() const {
    const auto* owner = OwnerElement();
    if (owner == nullptr) {
        return nullptr;
    }

    const auto& attributes = owner->Attributes();
    const auto attributeIndex = ResolveAttributeIndex();
    if (attributeIndex == DetachedAttributeIndex || attributeIndex >= attributes.size()) {
        return nullptr;
    }

    const auto& attribute = attributes[attributeIndex];
    return attribute != nullptr && attribute.get() == this ? attribute : nullptr;
}

void XmlAttribute::SetAttributeIndex(std::size_t attributeIndex) noexcept {
    attributeIndex_ = attributeIndex;
}

void XmlAttribute::ResetAttributeIndex() noexcept {
    attributeIndex_ = DetachedAttributeIndex;
}

std::size_t XmlAttribute::ResolveAttributeIndex() const noexcept {
    return attributeIndex_;
}

// XmlNode::Normalize — merge adjacent Text nodes recursively
void XmlNode::Normalize() {
    if (childNodes_ == nullptr) {
        return;
    }

    auto& children = *childNodes_;
    std::size_t i = 0;
    while (i < children.size()) {
        if (children[i]->NodeType() == XmlNodeType::Text) {
            // Accumulate consecutive text nodes
            std::string combined = children[i]->Value();
            std::size_t j = i + 1;
            while (j < children.size() && children[j]->NodeType() == XmlNodeType::Text) {
                combined += children[j]->Value();
                children[j]->SetParent(nullptr);
                ++j;
            }
            if (j > i + 1) {
                children[i]->SetValue(combined);
                children.erase(children.begin() + static_cast<std::ptrdiff_t>(i + 1),
                               children.begin() + static_cast<std::ptrdiff_t>(j));
            } else if (combined.empty()) {
                // Remove empty text node
                children[i]->SetParent(nullptr);
                children.erase(children.begin() + static_cast<std::ptrdiff_t>(i));
                continue;
            }
        } else {
            children[i]->Normalize();
        }
        ++i;
    }
}

// XmlCharacterData
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
    SetValue(std::string(Value()) + std::string(strData));
}

void XmlCharacterData::DeleteData(std::size_t offset, std::size_t count) {
    const std::string& v = Value();
    if (offset > v.size()) {
        throw XmlException("DeleteData offset out of range");
    }
    std::string result = v.substr(0, offset);
    if (offset + count < v.size()) {
        result += v.substr(offset + count);
    }
    SetValue(result);
}

void XmlCharacterData::InsertData(std::size_t offset, std::string_view strData) {
    const std::string& v = Value();
    if (offset > v.size()) {
        throw XmlException("InsertData offset out of range");
    }
    SetValue(v.substr(0, offset) + std::string(strData) + v.substr(offset));
}

void XmlCharacterData::ReplaceData(std::size_t offset, std::size_t count, std::string_view strData) {
    const std::string& v = Value();
    if (offset > v.size()) {
        throw XmlException("ReplaceData offset out of range");
    }
    std::string result = v.substr(0, offset) + std::string(strData);
    if (offset + count < v.size()) {
        result += v.substr(offset + count);
    }
    SetValue(result);
}

std::string XmlCharacterData::Substring(std::size_t offset, std::size_t count) const {
    const std::string& v = Value();
    if (offset > v.size()) {
        throw XmlException("Substring offset out of range");
    }
    return v.substr(offset, count);
}

XmlText::XmlText(std::string text) : XmlCharacterData(XmlNodeType::Text, "#text", std::move(text)) {
}

std::shared_ptr<XmlText> XmlText::SplitText(std::size_t offset) {
    const std::string& current = Value();
    if (offset > current.size()) {
        throw XmlException("SplitText offset exceeds text length");
    }
    std::string tail = current.substr(offset);
    SetValue(current.substr(0, offset));

    auto* ownerDocument = OwnerDocument();
    auto newNode = ownerDocument != nullptr
        ? ownerDocument->CreateTextNode(tail)
        : std::make_shared<XmlText>(std::move(tail));
    if (ParentNode() != nullptr) {
        auto next = NextSibling();
        if (next != nullptr) {
            ParentNode()->InsertBefore(newNode, next);
        } else {
            ParentNode()->AppendChild(newNode);
        }
    }
    return newNode;
}

XmlEntityReference::XmlEntityReference(std::string name, std::string resolvedValue)
    : XmlNode(XmlNodeType::EntityReference, std::move(name), std::move(resolvedValue)) {
}

XmlEntity::XmlEntity(
    std::string name,
    std::string replacementText,
    std::string publicId,
    std::string systemId,
    std::string notationName)
    : XmlNode(XmlNodeType::Entity, std::move(name), std::move(replacementText)),
      publicId_(std::move(publicId)),
      systemId_(std::move(systemId)),
      notationName_(std::move(notationName)) {
}

const std::string& XmlEntity::PublicId() const noexcept {
    return publicId_;
}

const std::string& XmlEntity::SystemId() const noexcept {
    return systemId_;
}

const std::string& XmlEntity::NotationName() const noexcept {
    return notationName_;
}

XmlNotation::XmlNotation(std::string name, std::string publicId, std::string systemId)
    : XmlNode(XmlNodeType::Notation, std::move(name)),
      publicId_(std::move(publicId)),
      systemId_(std::move(systemId)) {
}

const std::string& XmlNotation::PublicId() const noexcept {
    return publicId_;
}

const std::string& XmlNotation::SystemId() const noexcept {
    return systemId_;
}

XmlWhitespace::XmlWhitespace(std::string whitespace)
    : XmlCharacterData(XmlNodeType::Whitespace, "#whitespace", std::move(whitespace)) {
    ValidateWhitespaceValue(Value(), "Whitespace");
}

void XmlWhitespace::SetValue(std::string_view value) {
    ValidateWhitespaceValue(value, "Whitespace");
    XmlNode::SetValue(value);
}

XmlSignificantWhitespace::XmlSignificantWhitespace(std::string whitespace)
    : XmlCharacterData(XmlNodeType::SignificantWhitespace, "#significant-whitespace", std::move(whitespace)) {
    ValidateWhitespaceValue(Value(), "SignificantWhitespace");
}

void XmlSignificantWhitespace::SetValue(std::string_view value) {
    ValidateWhitespaceValue(value, "SignificantWhitespace");
    XmlNode::SetValue(value);
}

XmlCDataSection::XmlCDataSection(std::string data) : XmlCharacterData(XmlNodeType::CDATA, "#cdata-section", std::move(data)) {
}

XmlComment::XmlComment(std::string comment) : XmlCharacterData(XmlNodeType::Comment, "#comment", std::move(comment)) {
}

XmlProcessingInstruction::XmlProcessingInstruction(std::string target, std::string data)
    : XmlNode(XmlNodeType::ProcessingInstruction, std::move(target), std::move(data)) {
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
    : XmlNode(XmlNodeType::XmlDeclaration, "xml", BuildValidatedXmlDeclarationValue(version, encoding, standalone)),
      version_(std::move(version)),
      encoding_(std::move(encoding)),
            standalone_(std::move(standalone)) {
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
    : XmlDocumentType(
        std::move(name),
        std::move(publicId),
        std::move(systemId),
        std::move(internalSubset),
        {},
        {}) {
}

XmlDocumentType::XmlDocumentType(
    std::string name,
    std::string publicId,
    std::string systemId,
    std::string internalSubset,
    std::vector<std::shared_ptr<XmlNode>> entities,
    std::vector<std::shared_ptr<XmlNode>> notations)
    : XmlNode(XmlNodeType::DocumentType, std::move(name)),
      publicId_(std::move(publicId)),
      systemId_(std::move(systemId)),
      internalSubset_(std::move(internalSubset)) {
    if (entities.empty() && notations.empty()) {
        ParseDocumentTypeInternalSubset(internalSubset_, entities_, notations_);
        std::unordered_map<std::string, std::string> internalEntityDeclarations;
        PopulateInternalEntityDeclarations(entities_, internalEntityDeclarations);
        DtdRequiredAttributeDeclarations requiredAttributes;
        DtdDefaultAttributeDeclarations defaultAttributes;
        DtdFixedAttributeDeclarations fixedAttributes;
        DtdEnumeratedAttributeDeclarations enumeratedAttributes;
        DtdDeclaredAttributeNames declaredAttributes;
        DtdIdAttributeDeclarations idAttributes;
        DtdNotationAttributeDeclarations notationAttributes;
        DtdNmTokenAttributeDeclarations nmTokenAttributes;
        DtdNameAttributeDeclarations nameAttributes;
        ParseDocumentTypeAttributeDeclarations(
            internalSubset_,
            declaredAttributes,
            requiredAttributes,
            defaultAttributes,
            fixedAttributes,
            enumeratedAttributes,
            idAttributes,
            notationAttributes,
            nmTokenAttributes,
            nameAttributes);
        ValidateDtdDeclaredAttributeValueReferenceOrder(internalSubset_);
        ValidateDtdDeclaredAttributeValues(
            defaultAttributes,
            fixedAttributes,
            internalEntityDeclarations);
    } else {
        entities_ = CloneDocumentTypeDeclarations(entities);
        notations_ = CloneDocumentTypeDeclarations(notations);
    }
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

XmlNamedNodeMap XmlDocumentType::Entities() const {
    return XmlNamedNodeMap(entities_);
}

XmlNamedNodeMap XmlDocumentType::Notations() const {
    return XmlNamedNodeMap(notations_);
}

XmlDocumentFragment::XmlDocumentFragment() : XmlNode(XmlNodeType::DocumentFragment, "#document-fragment") {
}

void XmlDocumentFragment::SetInnerXml(std::string_view xml) {
    auto* ownerDocument = OwnerDocument();
    if (ownerDocument == nullptr) {
        throw XmlException("Document fragment must belong to an owner document before parsing XML");
    }

    XmlDocument scratch;
    scratch.SetPreserveWhitespace(ownerDocument->PreserveWhitespace());
    scratch.LoadXml("<__fragment_root__>" + std::string(xml) + "</__fragment_root__>");

    RemoveAllChildren();

    const auto wrapper = scratch.DocumentElement();
    while (wrapper != nullptr && wrapper->FirstChild() != nullptr) {
        auto child = wrapper->RemoveChild(wrapper->FirstChild());
        AppendChild(ownerDocument->ImportNode(*child, true));
    }
}

XmlElement::XmlElement(std::string name) : XmlNode(XmlNodeType::Element, std::move(name)) {
}

std::string_view XmlElement::PendingLoadAttributeNameView(const PendingLoadAttribute& attribute) const noexcept {
    return std::string_view(pendingLoadAttributeStorage_).substr(attribute.nameOffset, attribute.nameLength);
}

std::string_view XmlElement::PendingLoadAttributeValueView(const PendingLoadAttribute& attribute) const noexcept {
    return std::string_view(pendingLoadAttributeStorage_).substr(attribute.valueOffset, attribute.valueLength);
}

void XmlElement::MarkAttributeIndexesDirty() const noexcept {
    attributeNameIndexesDirty_ = true;
}

void XmlElement::EnsureAttributeIndexes() const {
    if (!attributeNameIndexesDirty_) {
        return;
    }

    if (attributes_.size() >= kAttributeIndexThreshold) {
        if (!attributeNameIndex_) {
            attributeNameIndex_ = std::make_unique<AttributeNameIndex>();
        } else {
            attributeNameIndex_->clear();
        }
        attributeNameIndex_->reserve(attributes_.size());

        if (!attributeLocalNameIndex_) {
            attributeLocalNameIndex_ = std::make_unique<AttributeLocalNameIndex>();
        } else {
            attributeLocalNameIndex_->clear();
        }
        attributeLocalNameIndex_->reserve(attributes_.size());

        for (std::size_t index = 0; index < attributes_.size(); ++index) {
            const auto& attribute = attributes_[index];
            if (attribute == nullptr) {
                continue;
            }

            attributeNameIndex_->insert_or_assign(attribute->Name(), index);
            (*attributeLocalNameIndex_)[SplitQualifiedNameView(attribute->Name()).second].push_back(index);
        }
    } else {
        attributeNameIndex_.reset();
        attributeLocalNameIndex_.reset();
    }

    if (pendingLoadAttributes_.size() >= kAttributeIndexThreshold) {
        if (!pendingLoadAttributeNameIndex_) {
            pendingLoadAttributeNameIndex_ = std::make_unique<PendingAttributeNameIndex>();
        } else {
            pendingLoadAttributeNameIndex_->clear();
        }
        pendingLoadAttributeNameIndex_->reserve(pendingLoadAttributes_.size());

        if (!pendingLoadAttributeLocalNameIndex_) {
            pendingLoadAttributeLocalNameIndex_ = std::make_unique<PendingAttributeLocalNameIndex>();
        } else {
            pendingLoadAttributeLocalNameIndex_->clear();
        }
        pendingLoadAttributeLocalNameIndex_->reserve(pendingLoadAttributes_.size());

        for (std::size_t index = 0; index < pendingLoadAttributes_.size(); ++index) {
            const auto name = PendingLoadAttributeNameView(pendingLoadAttributes_[index]);
            pendingLoadAttributeNameIndex_->insert_or_assign(name, index);
            (*pendingLoadAttributeLocalNameIndex_)[SplitQualifiedNameView(name).second].push_back(index);
        }
    } else {
        pendingLoadAttributeNameIndex_.reset();
        pendingLoadAttributeLocalNameIndex_.reset();
    }

    attributeNameIndexesDirty_ = false;
}

std::size_t XmlElement::FindIndexedAttributeIndex(std::string_view name) const noexcept {
    EnsureAttributeIndexes();
    if (attributeNameIndex_ != nullptr) {
        const auto found = attributeNameIndex_->find(name);
        return found == attributeNameIndex_->end() ? std::string::npos : found->second;
    }

    const auto found = std::find_if(attributes_.begin(), attributes_.end(), [&](const auto& attribute) {
        return attribute != nullptr && attribute->Name() == name;
    });
    return found == attributes_.end() ? std::string::npos : static_cast<std::size_t>(std::distance(attributes_.begin(), found));
}

std::size_t XmlElement::FindIndexedPendingLoadAttributeIndex(std::string_view name) const noexcept {
    EnsureAttributeIndexes();
    if (pendingLoadAttributeNameIndex_ != nullptr) {
        const auto found = pendingLoadAttributeNameIndex_->find(name);
        return found == pendingLoadAttributeNameIndex_->end() ? std::string::npos : found->second;
    }

    for (std::size_t index = 0; index < pendingLoadAttributes_.size(); ++index) {
        if (PendingLoadAttributeNameView(pendingLoadAttributes_[index]) == name) {
            return index;
        }
    }
    return std::string::npos;
}

const std::vector<std::size_t>* XmlElement::FindIndexedAttributeLocalNameMatches(std::string_view localName) const noexcept {
    EnsureAttributeIndexes();
    if (attributeLocalNameIndex_ == nullptr) {
        return nullptr;
    }

    const auto found = attributeLocalNameIndex_->find(localName);
    return found == attributeLocalNameIndex_->end() ? nullptr : &found->second;
}

const std::vector<std::size_t>* XmlElement::FindIndexedPendingLoadAttributeLocalNameMatches(std::string_view localName) const noexcept {
    EnsureAttributeIndexes();
    if (pendingLoadAttributeLocalNameIndex_ == nullptr) {
        return nullptr;
    }

    const auto found = pendingLoadAttributeLocalNameIndex_->find(localName);
    return found == pendingLoadAttributeLocalNameIndex_->end() ? nullptr : &found->second;
}

std::size_t XmlElement::FindAttributeIndex(std::string_view localName, std::string_view namespaceUri) const {
    if (const auto* matches = FindIndexedAttributeLocalNameMatches(localName); matches != nullptr) {
        for (const auto index : *matches) {
            const auto& attribute = attributes_[index];
            const auto [prefix, attributeLocalName] = SplitQualifiedNameView(attribute->Name());
            if (attributeLocalName != localName) {
                continue;
            }
            if (prefix.empty()) {
                if (namespaceUri.empty()) {
                    return index;
                }
                continue;
            }
            if (PrefixResolvesToNamespaceUri(this, prefix, namespaceUri)) {
                return index;
            }
        }
        return std::string::npos;
    }

    const auto found = std::find_if(attributes_.begin(), attributes_.end(), [&](const auto& attribute) {
        const auto [prefix, attributeLocalName] = SplitQualifiedNameView(attribute->Name());
        if (attributeLocalName != localName) {
            return false;
        }
        if (prefix.empty()) {
            return namespaceUri.empty();
        }
        return PrefixResolvesToNamespaceUri(this, prefix, namespaceUri);
    });
    return found == attributes_.end() ? std::string::npos : static_cast<std::size_t>(std::distance(attributes_.begin(), found));
}

bool XmlElement::TryFindNamespaceDeclarationValueView(std::string_view prefix, std::string_view& value) const noexcept {
    for (const auto& attribute : attributes_) {
        const std::string_view attributeName(attribute->Name());
        if (!IsNamespaceDeclarationName(attributeName)) {
            continue;
        }
        if (NamespaceDeclarationPrefixView(attributeName) == prefix) {
            value = attribute->Value();
            return true;
        }
    }

    for (const auto& pendingAttribute : pendingLoadAttributes_) {
        const std::string_view pendingName = PendingLoadAttributeNameView(pendingAttribute);
        if (!IsNamespaceDeclarationName(pendingName)) {
            continue;
        }
        if (NamespaceDeclarationPrefixView(pendingName) == prefix) {
            value = PendingLoadAttributeValueView(pendingAttribute);
            return true;
        }
    }

    return false;
}

bool XmlElement::TryFindNamespaceDeclarationPrefixView(std::string_view namespaceUri, std::string_view& prefix) const noexcept {
    for (const auto& attribute : attributes_) {
        const std::string_view attributeName(attribute->Name());
        if (!IsNamespaceDeclarationName(attributeName)) {
            continue;
        }
        if (attribute->Value() == namespaceUri) {
            prefix = NamespaceDeclarationPrefixView(attributeName);
            return true;
        }
    }

    for (const auto& pendingAttribute : pendingLoadAttributes_) {
        const std::string_view pendingName = PendingLoadAttributeNameView(pendingAttribute);
        if (!IsNamespaceDeclarationName(pendingName)) {
            continue;
        }
        if (PendingLoadAttributeValueView(pendingAttribute) == namespaceUri) {
            prefix = NamespaceDeclarationPrefixView(pendingName);
            return true;
        }
    }

    prefix = {};
    return false;
}

bool TryGetAttributeValueViewInternal(const XmlElement& element, std::string_view name, std::string_view& value) noexcept {
    if (const auto attributeIndex = element.FindIndexedAttributeIndex(name); attributeIndex != std::string::npos) {
        value = element.attributes_[attributeIndex]->Value();
        return true;
    }

    if (const auto pendingIndex = element.FindIndexedPendingLoadAttributeIndex(name); pendingIndex != std::string::npos) {
        value = element.PendingLoadAttributeValueView(element.pendingLoadAttributes_[pendingIndex]);
        return true;
    }

    value = {};
    return false;
}

bool AttributeValueEqualsInternal(const XmlElement& element, std::string_view name, std::string_view expectedValue) noexcept {
    std::string_view value;
    return TryGetAttributeValueViewInternal(element, name, value) && value == expectedValue;
}

std::shared_ptr<XmlAttribute> XmlElement::MaterializePendingLoadAttribute(std::size_t index) const {
    if (index >= pendingLoadAttributes_.size()) {
        return nullptr;
    }

    const std::size_t materializedIndex = attributes_.size() + index;
    EnsureAttributesMaterialized();
    return materializedIndex < attributes_.size() ? attributes_[materializedIndex] : nullptr;
}

std::size_t XmlElement::FindPendingLoadAttributeIndex(std::string_view name) const noexcept {
    return FindIndexedPendingLoadAttributeIndex(name);
}

std::size_t XmlElement::FindPendingLoadAttributeIndex(std::string_view localName, std::string_view namespaceUri) const {
    if (const auto* matches = FindIndexedPendingLoadAttributeLocalNameMatches(localName); matches != nullptr) {
        for (const auto index : *matches) {
            const auto& pendingAttribute = pendingLoadAttributes_[index];
            const auto [prefix, pendingLocalName] = SplitQualifiedNameView(PendingLoadAttributeNameView(pendingAttribute));
            if (pendingLocalName != localName) {
                continue;
            }
            if (prefix.empty()) {
                if (namespaceUri.empty()) {
                    return index;
                }
                continue;
            }
            if (PrefixResolvesToNamespaceUri(this, prefix, namespaceUri)) {
                return index;
            }
        }
        return std::string::npos;
    }

    for (std::size_t index = 0; index < pendingLoadAttributes_.size(); ++index) {
        const auto& pendingAttribute = pendingLoadAttributes_[index];
        const auto [prefix, pendingLocalName] = SplitQualifiedNameView(PendingLoadAttributeNameView(pendingAttribute));
        if (pendingLocalName != localName) {
            continue;
        }
        if (prefix.empty()) {
            if (namespaceUri.empty()) {
                return index;
            }
            continue;
        }
        if (PrefixResolvesToNamespaceUri(this, prefix, namespaceUri)) {
            return index;
        }
    }
    return std::string::npos;
}

std::string XmlElement::FindNamespaceDeclarationValue(std::string_view prefix) const {
    std::string_view value;
    return TryFindNamespaceDeclarationValueView(prefix, value) ? std::string(value) : std::string{};
}

bool XmlElement::HasNamespaceDeclaration(std::string_view prefix) const {
    std::string_view value;
    return TryFindNamespaceDeclarationValueView(prefix, value);
}

std::string XmlElement::FindNamespaceDeclarationPrefix(std::string_view namespaceUri) const {
    std::string_view prefix;
    return TryFindNamespaceDeclarationPrefixView(namespaceUri, prefix) ? std::string(prefix) : std::string{};
}

void XmlElement::EnsureAttributesMaterialized() const {
    if (pendingLoadAttributes_.empty()) {
        return;
    }

    attributes_.reserve(attributes_.size() + pendingLoadAttributes_.size());
    auto* ownerDocument = OwnerDocument();
    for (const auto& pendingAttribute : pendingLoadAttributes_) {
        auto attribute = ownerDocument != nullptr
            ? ownerDocument->CreateAttribute(
                std::string(PendingLoadAttributeNameView(pendingAttribute)),
                std::string(PendingLoadAttributeValueView(pendingAttribute)))
            : std::make_shared<XmlAttribute>(
                std::string(PendingLoadAttributeNameView(pendingAttribute)),
                std::string(PendingLoadAttributeValueView(pendingAttribute)));
        attribute->SetParent(const_cast<XmlElement*>(this));
        attribute->SetOwnerDocument(const_cast<XmlDocument*>(ownerDocument));
            attribute->SetAttributeIndex(attributes_.size());
        attributes_.push_back(std::move(attribute));
    }

    pendingLoadAttributes_.clear();
    pendingLoadAttributeStorage_.clear();
    pendingLoadAttributeNameIndex_.reset();
    pendingLoadAttributeLocalNameIndex_.reset();
    MarkAttributeIndexesDirty();
}

const std::vector<std::shared_ptr<XmlAttribute>>& XmlElement::Attributes() const {
    EnsureAttributesMaterialized();
    return attributes_;
}

XmlAttributeCollection XmlElement::AttributeNodes() const {
    EnsureAttributesMaterialized();
    return XmlAttributeCollection(attributes_);
}

bool XmlElement::HasAttributes() const noexcept {
    return !attributes_.empty() || !pendingLoadAttributes_.empty();
}

bool XmlElement::HasAttribute(std::string_view name) const {
    std::string_view value;
    return TryGetAttributeValueViewInternal(*this, name, value);
}

bool XmlElement::HasAttribute(std::string_view localName, std::string_view namespaceUri) const {
    return FindAttributeIndex(localName, namespaceUri) != std::string::npos
        || FindPendingLoadAttributeIndex(localName, namespaceUri) != std::string::npos;
}

std::string XmlElement::GetAttribute(std::string_view name) const {
    std::string_view value;
    return TryGetAttributeValueViewInternal(*this, name, value) ? std::string(value) : std::string{};
}

std::string XmlElement::GetAttribute(std::string_view localName, std::string_view namespaceUri) const {
    if (const auto index = FindAttributeIndex(localName, namespaceUri); index != std::string::npos) {
        return attributes_[index]->Value();
    }

    const auto pendingIndex = FindPendingLoadAttributeIndex(localName, namespaceUri);
    return pendingIndex == std::string::npos
        ? std::string{}
        : std::string(PendingLoadAttributeValueView(pendingLoadAttributes_[pendingIndex]));
}

std::shared_ptr<XmlAttribute> XmlElement::GetAttributeNode(std::string_view name) const {
    if (const auto index = FindIndexedAttributeIndex(name); index != std::string::npos) {
        return attributes_[index];
    }

    const auto pendingIndex = FindIndexedPendingLoadAttributeIndex(name);
    if (pendingIndex != std::string::npos) {
        return MaterializePendingLoadAttribute(pendingIndex);
    }

    return nullptr;
}

std::shared_ptr<XmlAttribute> XmlElement::GetAttributeNode(std::string_view localName, std::string_view namespaceUri) const {
    if (const auto index = FindAttributeIndex(localName, namespaceUri); index != std::string::npos) {
        return attributes_[index];
    }

    const auto pendingIndex = FindPendingLoadAttributeIndex(localName, namespaceUri);
    if (pendingIndex != std::string::npos) {
        return MaterializePendingLoadAttribute(pendingIndex);
    }

    return nullptr;
}

std::shared_ptr<XmlAttribute> XmlElement::SetAttribute(std::string_view name, std::string_view value) {
    if (auto attribute = GetAttributeNode(name)) {
        attribute->SetValue(value);
        return attribute;
    }

    auto* ownerDocument = OwnerDocument();
    auto attribute = ownerDocument != nullptr
        ? ownerDocument->CreateAttribute(std::string(name), std::string(value))
        : std::make_shared<XmlAttribute>(std::string(name), std::string(value));
    attribute->SetParent(this);
    attribute->SetOwnerDocumentRecursive(OwnerDocument());
    attribute->SetAttributeIndex(attributes_.size());
    attributes_.push_back(attribute);
    MarkAttributeIndexesDirty();
    return attribute;
}

void XmlElement::ReindexAttributesFrom(std::size_t startIndex) noexcept {
    for (std::size_t index = startIndex; index < attributes_.size(); ++index) {
        if (attributes_[index] != nullptr) {
            attributes_[index]->SetAttributeIndex(index);
        }
    }
}

void XmlElement::ReserveAttributesForLoad(std::size_t count, std::size_t totalStorageBytes) {
    pendingLoadAttributes_.reserve(count);
    if (totalStorageBytes != 0) {
        pendingLoadAttributeStorage_.reserve(pendingLoadAttributeStorage_.size() + totalStorageBytes);
    }
}

void XmlElement::AppendAttributeForLoad(std::string name, std::string value) {
    const std::size_t nameOffset = pendingLoadAttributeStorage_.size();
    pendingLoadAttributeStorage_ += name;
    const std::size_t valueOffset = pendingLoadAttributeStorage_.size();
    pendingLoadAttributeStorage_ += value;
    pendingLoadAttributes_.push_back(PendingLoadAttribute{
        nameOffset,
        name.size(),
        valueOffset,
        value.size()});
    MarkAttributeIndexesDirty();
}

void XmlElement::SetAttribute(std::string_view localName, std::string_view namespaceUri, std::string_view value) {
    // Find existing attribute with matching localName+ns, or create prefix:localName
    if (auto existing = GetAttributeNode(localName, namespaceUri)) {
        existing->SetValue(value);
        return;
    }
    if (namespaceUri.empty()) {
        SetAttribute(localName, value);
        return;
    }
    // Search for existing prefix bound to namespaceUri on this element/ancestors.
    std::string existingPrefix = FindNamespaceDeclarationPrefix(namespaceUri);
    if (existingPrefix.empty()) {
        const XmlNode* p = ParentNode();
        while (p != nullptr && p->NodeType() == XmlNodeType::Element && existingPrefix.empty()) {
            existingPrefix = static_cast<const XmlElement*>(p)->FindNamespaceDeclarationPrefix(namespaceUri);
            p = p->ParentNode();
        }
    }
    if (existingPrefix.empty()) {
        // Generate a prefix
        std::size_t prefixIndex = attributes_.size() + pendingLoadAttributes_.size();
        do {
            existingPrefix = "ns" + std::to_string(prefixIndex++);
        } while (HasNamespaceDeclaration(existingPrefix));
        SetAttribute("xmlns:" + existingPrefix, namespaceUri);
    }
    SetAttribute(existingPrefix + ":" + std::string(localName), value);
}

std::shared_ptr<XmlAttribute> XmlElement::SetAttributeNode(const std::shared_ptr<XmlAttribute>& attribute) {
    EnsureAttributesMaterialized();
    if (!attribute) {
        throw XmlException("Cannot attach a null attribute node");
    }

    if (attribute->OwnerDocument() != nullptr && OwnerDocument() != nullptr && attribute->OwnerDocument() != OwnerDocument()) {
        throw XmlException("The attribute belongs to a different document");
    }

    if (attribute->OwnerElement() != nullptr && attribute->OwnerElement() != this) {
        throw XmlException("The attribute already belongs to another element");
    }

    const auto found = std::find_if(attributes_.begin(), attributes_.end(), [&attribute](const auto& current) {
        return current->Name() == attribute->Name();
    });

    if (found != attributes_.end() && *found == attribute) {
        return attribute;
    }

    std::shared_ptr<XmlAttribute> replaced;
    if (found != attributes_.end()) {
        replaced = *found;
        replaced->SetParent(nullptr);
        replaced->ResetAttributeIndex();
        attribute->SetAttributeIndex(static_cast<std::size_t>(found - attributes_.begin()));
        *found = attribute;
    } else {
        attribute->SetAttributeIndex(attributes_.size());
        attributes_.push_back(attribute);
    }

    attribute->SetParent(this);
    attribute->SetOwnerDocumentRecursive(OwnerDocument());
    MarkAttributeIndexesDirty();
    return replaced;
}

void XmlElement::SetInnerXml(std::string_view xml) {
    XmlDocument scratch;
    if (OwnerDocument() != nullptr) {
        scratch.SetPreserveWhitespace(OwnerDocument()->PreserveWhitespace());
    }
    scratch.LoadXml("<__element_inner_xml__>" + std::string(xml) + "</__element_inner_xml__>");

    RemoveAllChildren();

    const auto wrapper = scratch.DocumentElement();
    while (wrapper != nullptr && wrapper->FirstChild() != nullptr) {
        auto child = wrapper->RemoveChild(wrapper->FirstChild());
        if (OwnerDocument() != nullptr) {
            AppendChild(OwnerDocument()->ImportNode(*child, true));
        } else {
            AppendChild(child);
        }
    }
}

bool XmlElement::RemoveAttribute(std::string_view name) {
    if (const auto pendingIndex = FindPendingLoadAttributeIndex(name); pendingIndex != std::string::npos) {
        pendingLoadAttributes_.erase(pendingLoadAttributes_.begin() + static_cast<std::ptrdiff_t>(pendingIndex));
        MarkAttributeIndexesDirty();
        return true;
    }

    const auto index = FindIndexedAttributeIndex(name);
    if (index == std::string::npos) {
        return false;
    }

    attributes_[index]->SetParent(nullptr);
    attributes_[index]->ResetAttributeIndex();
    attributes_.erase(attributes_.begin() + static_cast<std::ptrdiff_t>(index));
    ReindexAttributesFrom(index);
    MarkAttributeIndexesDirty();
    return true;
}

bool XmlElement::RemoveAttribute(std::string_view localName, std::string_view namespaceUri) {
    if (const auto pendingIndex = FindPendingLoadAttributeIndex(localName, namespaceUri); pendingIndex != std::string::npos) {
        pendingLoadAttributes_.erase(pendingLoadAttributes_.begin() + static_cast<std::ptrdiff_t>(pendingIndex));
        MarkAttributeIndexesDirty();
        return true;
    }

    const auto index = FindAttributeIndex(localName, namespaceUri);
    if (index == std::string::npos) {
        return false;
    }

    attributes_[index]->SetParent(nullptr);
    attributes_[index]->ResetAttributeIndex();
    attributes_.erase(attributes_.begin() + static_cast<std::ptrdiff_t>(index));
    ReindexAttributesFrom(index);
    MarkAttributeIndexesDirty();
    return true;
}

std::shared_ptr<XmlAttribute> XmlElement::RemoveAttributeNode(const std::shared_ptr<XmlAttribute>& attribute) {
    EnsureAttributesMaterialized();
    if (!attribute) {
        return nullptr;
    }

    const auto found = std::find(attributes_.begin(), attributes_.end(), attribute);
    if (found == attributes_.end()) {
        return nullptr;
    }

    auto removed = *found;
    removed->SetParent(nullptr);
    removed->ResetAttributeIndex();
    const auto removedIndex = static_cast<std::size_t>(found - attributes_.begin());
    attributes_.erase(found);
    ReindexAttributesFrom(removedIndex);
    MarkAttributeIndexesDirty();
    return removed;
}

void XmlElement::RemoveAllAttributes() {
    pendingLoadAttributes_.clear();
    for (const auto& attribute : attributes_) {
        attribute->SetParent(nullptr);
        attribute->ResetAttributeIndex();
    }
    attributes_.clear();
    attributeNameIndex_.reset();
    attributeLocalNameIndex_.reset();
    pendingLoadAttributeNameIndex_.reset();
    pendingLoadAttributeLocalNameIndex_.reset();
    attributeNameIndexesDirty_ = true;
}

bool XmlElement::IsEmpty() const noexcept {
    return !HasChildNodes() && !writeFullEndElement_;
}

std::vector<std::shared_ptr<XmlElement>> XmlElement::GetElementsByTagName(std::string_view name) const {
    std::vector<std::shared_ptr<XmlElement>> results;
    CollectElementsByName(*this, name, results);
    return results;
}

XmlNodeList XmlElement::GetElementsByTagNameList(std::string_view name) const {
    std::vector<std::shared_ptr<XmlNode>> nodes;
    for (const auto& element : GetElementsByTagName(name)) {
        nodes.push_back(element);
    }
    return XmlNodeList(std::move(nodes));
}

std::vector<std::shared_ptr<XmlElement>> XmlElement::GetElementsByTagName(std::string_view localName, std::string_view namespaceUri) const {
    std::vector<std::shared_ptr<XmlElement>> results;
    CollectElementsByNameNS(*this, localName, namespaceUri, results);
    return results;
}

XmlNodeList XmlElement::GetElementsByTagNameList(std::string_view localName, std::string_view namespaceUri) const {
    std::vector<std::shared_ptr<XmlNode>> nodes;
    for (const auto& element : GetElementsByTagName(localName, namespaceUri)) {
        nodes.push_back(element);
    }
    return XmlNodeList(std::move(nodes));
}

bool XmlElement::WritesFullEndElement() const noexcept {
    return writeFullEndElement_;
}

XmlNodeReader XmlElement::CreateReader() const {
    return XmlNodeReader(*this);
}

}  // namespace System::Xml
