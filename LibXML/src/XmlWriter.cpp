#include "../include/XmlInternal.h"

namespace System::Xml {

namespace {

void WriteNodesFromReader(XmlWriter& writer, XmlReader& reader) {
    while (reader.GetReadState() != ReadState::EndOfFile) {
        writer.WriteNode(reader);
    }
}

void WriteAttributeNode(XmlWriter& writer, const XmlNode& attribute) {
    if (IsNamespaceDeclarationName(attribute.Name())) {
        writer.WriteAttributeString(std::string_view(attribute.Name()), std::string_view(attribute.Value()));
        return;
    }

    writer.WriteAttributeString(
        std::string_view(attribute.Prefix()),
        std::string_view(attribute.LocalName()),
        std::string_view(attribute.NamespaceURI()),
        std::string_view(attribute.Value()));
}

void WritePendingAttribute(XmlWriter& writer, const XmlElement& element, std::string_view name, std::string_view value) {
    if (IsNamespaceDeclarationName(name)) {
        writer.WriteAttributeString(name, value);
        return;
    }

    const auto [prefix, localName] = SplitQualifiedNameView(name);
    std::string_view namespaceUri;
    static_cast<void>(LookupNamespaceUriOnElementView(&element, prefix, namespaceUri));
    writer.WriteAttributeString(
        prefix,
        localName,
        namespaceUri,
        value);
}

}  // namespace

namespace {

void AppendIndent(std::string& output, int depth, const XmlWriterSettings& settings) {
    for (int index = 0; index < depth; ++index) {
        output += settings.IndentChars;
    }
}

bool ChildrenCanBeIndented(const XmlNode& node) {
    if (!node.HasChildNodes()) {
        return false;
    }

    for (const auto& child : node.ChildNodes()) {
        if (IsTextLike(child->NodeType())) {
            return false;
        }
    }

    return true;
}

void SerializeNode(const XmlNode& node, const XmlWriterSettings& settings, std::string& output, int depth);

void SerializeNodeToStream(const XmlNode& node, const XmlWriterSettings& settings, std::ostream& stream, int depth);

void WriteChildren(const XmlNode& node, const XmlWriterSettings& settings, std::string& output, int depth, bool indented) {
    for (std::size_t index = 0; index < node.ChildNodes().size(); ++index) {
        const auto& child = node.ChildNodes()[index];
        if (indented) {
            output += settings.NewLineChars;
            AppendIndent(output, depth, settings);
        }

        SerializeNode(*child, settings, output, depth);
    }
}

void WriteChildrenToStream(const XmlNode& node, const XmlWriterSettings& settings, std::ostream& stream, int depth, bool indented) {
    for (std::size_t index = 0; index < node.ChildNodes().size(); ++index) {
        const auto& child = node.ChildNodes()[index];
        if (indented) {
            stream << settings.NewLineChars;
            std::string indentation;
            AppendIndent(indentation, depth, settings);
            stream << indentation;
        }

        SerializeNodeToStream(*child, settings, stream, depth);
    }
}

void SerializeNode(const XmlNode& node, const XmlWriterSettings& settings, std::string& output, int depth) {
    switch (node.NodeType()) {
    case XmlNodeType::Document: {
        bool firstWritten = false;
        for (const auto& child : node.ChildNodes()) {
            if (settings.OmitXmlDeclaration && child->NodeType() == XmlNodeType::XmlDeclaration) {
                continue;
            }

            if (firstWritten && settings.Indent) {
                output += settings.NewLineChars;
            }

            SerializeNode(*child, settings, output, depth);
            firstWritten = true;
        }
        break;
    }
    case XmlNodeType::DocumentFragment: {
        bool firstWritten = false;
        for (const auto& child : node.ChildNodes()) {
            if (firstWritten && settings.Indent) {
                output += settings.NewLineChars;
            }

            SerializeNode(*child, settings, output, depth);
            firstWritten = true;
        }
        break;
    }
    case XmlNodeType::XmlDeclaration: {
        const auto& declaration = static_cast<const XmlDeclaration&>(node);
        output += "<?xml version=\"" + EscapeAttribute(declaration.Version(), settings) + "\"";
        if (!declaration.Encoding().empty()) {
            output += " encoding=\"" + EscapeAttribute(declaration.Encoding(), settings) + "\"";
        }
        if (!declaration.Standalone().empty()) {
            output += " standalone=\"" + EscapeAttribute(declaration.Standalone(), settings) + "\"";
        }
        output += "?>";
        break;
    }
    case XmlNodeType::DocumentType: {
        const auto& documentType = static_cast<const XmlDocumentType&>(node);
        output += "<!DOCTYPE " + documentType.Name();
        if (!documentType.PublicId().empty()) {
            output += " PUBLIC \"" + documentType.PublicId() + "\" \"" + documentType.SystemId() + "\"";
        } else if (!documentType.SystemId().empty()) {
            output += " SYSTEM \"" + documentType.SystemId() + "\"";
        }
        if (!documentType.InternalSubset().empty()) {
            output += " [" + documentType.InternalSubset() + "]";
        }
        output += '>';
        break;
    }
    case XmlNodeType::Element: {
        const auto& element = static_cast<const XmlElement&>(node);
        output += '<';
        output += element.Name();
        for (const auto& attribute : element.Attributes()) {
            output += ' ';
            output += attribute->Name();
            output += "=\"";
            output += EscapeAttribute(attribute->Value(), settings);
            output += '\"';
        }

        if (!element.HasChildNodes() && !element.WritesFullEndElement()) {
            output += "/>";
            break;
        }

        const bool indentedChildren = settings.Indent && ChildrenCanBeIndented(element);
        output += '>';
        WriteChildren(element, settings, output, indentedChildren ? depth + 1 : depth, indentedChildren);
        if (indentedChildren) {
            output += settings.NewLineChars;
            AppendIndent(output, depth, settings);
        }
        output += "</" + element.Name() + ">";
        break;
    }
    case XmlNodeType::Attribute:
        output += node.Name();
        output += "=\"";
        output += EscapeAttribute(node.Value(), settings);
        output += '"';
        break;
    case XmlNodeType::Text:
    case XmlNodeType::Whitespace:
    case XmlNodeType::SignificantWhitespace:
        output += EscapeText(node.Value(), settings);
        break;
    case XmlNodeType::EntityReference:
        output += '&';
        output += node.Name();
        output += ';';
        break;
    case XmlNodeType::Entity: {
        const auto& entity = static_cast<const XmlEntity&>(node);
        output += "<!ENTITY " + entity.Name();
        if (!entity.PublicId().empty()) {
            output += " PUBLIC \"" + entity.PublicId() + "\" \"" + entity.SystemId() + "\"";
        } else if (!entity.SystemId().empty()) {
            output += " SYSTEM \"" + entity.SystemId() + "\"";
        } else {
            output += " \"" + entity.Value() + "\"";
        }
        if (!entity.NotationName().empty()) {
            output += " NDATA " + entity.NotationName();
        }
        output += '>';
        break;
    }
    case XmlNodeType::Notation: {
        const auto& notation = static_cast<const XmlNotation&>(node);
        output += "<!NOTATION " + notation.Name();
        if (!notation.PublicId().empty()) {
            output += " PUBLIC \"" + notation.PublicId() + "\"";
            if (!notation.SystemId().empty()) {
                output += " \"" + notation.SystemId() + "\"";
            }
        } else if (!notation.SystemId().empty()) {
            output += " SYSTEM \"" + notation.SystemId() + "\"";
        }
        output += '>';
        break;
    }
    case XmlNodeType::CDATA:
        output += "<![CDATA[" + node.Value() + "]]>";
        break;
    case XmlNodeType::Comment:
        output += "<!--" + node.Value() + "-->";
        break;
    case XmlNodeType::ProcessingInstruction: {
        const auto& instruction = static_cast<const XmlProcessingInstruction&>(node);
        output += "<?" + instruction.Target();
        if (!instruction.Data().empty()) {
            output += ' ';
            output += instruction.Data();
        }
        output += "?>";
        break;
    }
    default:
        throw XmlException("Unsupported node type for serialization");
    }
}

void SerializeNodeToStream(const XmlNode& node, const XmlWriterSettings& settings, std::ostream& stream, int depth) {
    switch (node.NodeType()) {
    case XmlNodeType::Document: {
        bool firstWritten = false;
        for (const auto& child : node.ChildNodes()) {
            if (settings.OmitXmlDeclaration && child->NodeType() == XmlNodeType::XmlDeclaration) {
                continue;
            }

            if (firstWritten && settings.Indent) {
                stream << settings.NewLineChars;
            }

            SerializeNodeToStream(*child, settings, stream, depth);
            firstWritten = true;
        }
        break;
    }
    case XmlNodeType::DocumentFragment: {
        bool firstWritten = false;
        for (const auto& child : node.ChildNodes()) {
            if (firstWritten && settings.Indent) {
                stream << settings.NewLineChars;
            }

            SerializeNodeToStream(*child, settings, stream, depth);
            firstWritten = true;
        }
        break;
    }
    case XmlNodeType::XmlDeclaration: {
        const auto& declaration = static_cast<const XmlDeclaration&>(node);
        stream << "<?xml version=\"" << EscapeAttribute(declaration.Version(), settings) << '\"';
        if (!declaration.Encoding().empty()) {
            stream << " encoding=\"" << EscapeAttribute(declaration.Encoding(), settings) << '\"';
        }
        if (!declaration.Standalone().empty()) {
            stream << " standalone=\"" << EscapeAttribute(declaration.Standalone(), settings) << '\"';
        }
        stream << "?>";
        break;
    }
    case XmlNodeType::DocumentType: {
        const auto& documentType = static_cast<const XmlDocumentType&>(node);
        stream << "<!DOCTYPE " << documentType.Name();
        if (!documentType.PublicId().empty()) {
            stream << " PUBLIC \"" << documentType.PublicId() << "\" \"" << documentType.SystemId() << "\"";
        } else if (!documentType.SystemId().empty()) {
            stream << " SYSTEM \"" << documentType.SystemId() << "\"";
        }
        if (!documentType.InternalSubset().empty()) {
            stream << " [" << documentType.InternalSubset() << ']';
        }
        stream << '>';
        break;
    }
    case XmlNodeType::Element: {
        const auto& element = static_cast<const XmlElement&>(node);
        stream << '<' << element.Name();
        for (const auto& attribute : element.Attributes()) {
            stream << ' ' << attribute->Name() << "=\"" << EscapeAttribute(attribute->Value(), settings) << '\"';
        }

        if (!element.HasChildNodes() && !element.WritesFullEndElement()) {
            stream << "/>";
            break;
        }

        const bool indentedChildren = settings.Indent && ChildrenCanBeIndented(element);
        stream << '>';
        WriteChildrenToStream(element, settings, stream, indentedChildren ? depth + 1 : depth, indentedChildren);
        if (indentedChildren) {
            stream << settings.NewLineChars;
            std::string indentation;
            AppendIndent(indentation, depth, settings);
            stream << indentation;
        }
        stream << "</" << element.Name() << ">";
        break;
    }
    case XmlNodeType::Attribute:
        stream << node.Name() << "=\"" << EscapeAttribute(node.Value(), settings) << '\"';
        break;
    case XmlNodeType::Text:
    case XmlNodeType::Whitespace:
    case XmlNodeType::SignificantWhitespace:
        stream << EscapeText(node.Value(), settings);
        break;
    case XmlNodeType::EntityReference:
        stream << '&' << node.Name() << ';';
        break;
    case XmlNodeType::Entity: {
        const auto& entity = static_cast<const XmlEntity&>(node);
        stream << "<!ENTITY " << entity.Name();
        if (!entity.PublicId().empty()) {
            stream << " PUBLIC \"" << entity.PublicId() << "\" \"" << entity.SystemId() << "\"";
        } else if (!entity.SystemId().empty()) {
            stream << " SYSTEM \"" << entity.SystemId() << "\"";
        } else {
            stream << " \"" << entity.Value() << "\"";
        }
        if (!entity.NotationName().empty()) {
            stream << " NDATA " << entity.NotationName();
        }
        stream << '>';
        break;
    }
    case XmlNodeType::Notation: {
        const auto& notation = static_cast<const XmlNotation&>(node);
        stream << "<!NOTATION " << notation.Name();
        if (!notation.PublicId().empty()) {
            stream << " PUBLIC \"" << notation.PublicId() << "\"";
            if (!notation.SystemId().empty()) {
                stream << " \"" << notation.SystemId() << "\"";
            }
        } else if (!notation.SystemId().empty()) {
            stream << " SYSTEM \"" << notation.SystemId() << "\"";
        }
        stream << '>';
        break;
    }
    case XmlNodeType::CDATA:
        stream << "<![CDATA[" << node.Value() << "]]>";
        break;
    case XmlNodeType::Comment:
        stream << "<!--" << node.Value() << "-->";
        break;
    case XmlNodeType::ProcessingInstruction: {
        const auto& instruction = static_cast<const XmlProcessingInstruction&>(node);
        stream << "<?" << instruction.Target();
        if (!instruction.Data().empty()) {
            stream << ' ' << instruction.Data();
        }
        stream << "?>";
        break;
    }
    default:
        throw XmlException("Unsupported node type for serialization");
    }
}


}  // namespace

XmlWriter::XmlWriter(XmlWriterSettings settings) : settings_(std::move(settings)) {}

XmlWriter::XmlWriter(std::ostream& stream, XmlWriterSettings settings)
    : settings_(std::move(settings)), directOutputStream_(&stream) {}

bool XmlWriter::UsesDirectOutput() const noexcept {
    return directOutputStream_ != nullptr;
}

XmlWriter::InMemoryMutationCheckpoint XmlWriter::CaptureInMemoryMutationCheckpoint() const {
    InMemoryMutationCheckpoint checkpoint;
    checkpoint.elementStack = elementStack_;
    checkpoint.startTagOpenStack = startTagOpenStack_;
    checkpoint.startDocumentWritten = startDocumentWritten_;
    checkpoint.targetParent = checkpoint.elementStack.empty() ? std::shared_ptr<XmlElement>{} : checkpoint.elementStack.back();
    checkpoint.parentChildCount = checkpoint.targetParent == nullptr ? 0 : checkpoint.targetParent->ChildNodes().size();
    checkpoint.documentChildCount = document_.ChildNodes().size();
    checkpoint.hadFragmentRoot = fragmentRoot_ != nullptr;
    checkpoint.fragmentChildCount = fragmentRoot_ == nullptr ? 0 : fragmentRoot_->ChildNodes().size();
    return checkpoint;
}

void XmlWriter::RollbackInMemoryMutation(const InMemoryMutationCheckpoint& checkpoint) {
    if (checkpoint.targetParent != nullptr) {
        while (checkpoint.targetParent->ChildNodes().size() > checkpoint.parentChildCount) {
            checkpoint.targetParent->RemoveChild(checkpoint.targetParent->LastChild());
        }
    }

    while (document_.ChildNodes().size() > checkpoint.documentChildCount) {
        document_.RemoveChild(document_.LastChild());
    }

    if (fragmentRoot_ != nullptr) {
        while (fragmentRoot_->ChildNodes().size() > checkpoint.fragmentChildCount) {
            fragmentRoot_->RemoveChild(fragmentRoot_->LastChild());
        }
        if (!checkpoint.hadFragmentRoot && fragmentRoot_->ChildNodes().empty()) {
            fragmentRoot_.reset();
        }
    }

    elementStack_ = checkpoint.elementStack;
    startTagOpenStack_ = checkpoint.startTagOpenStack;
    startDocumentWritten_ = checkpoint.startDocumentWritten;
}

std::shared_ptr<XmlDocumentFragment> XmlWriter::EnsureFragmentRoot() const {
    if (fragmentRoot_ == nullptr) {
        fragmentRoot_ = document_.CreateDocumentFragment();
    }
    return fragmentRoot_;
}

WriteState XmlWriter::GetWriteState() const noexcept {
    if (documentClosed_) return WriteState::Closed;
    if (currentAttributeName_.has_value()) return WriteState::Attribute;
    if (UsesDirectOutput()) {
        if (!directElementStack_.empty()) {
            if (directElementStack_.back().startTagOpen) return WriteState::Element;
            return WriteState::Content;
        }
        if (startDocumentWritten_ || directHasTopLevelContent_) return WriteState::Prolog;
        return WriteState::Start;
    }
    if (!elementStack_.empty()) {
        if (!startTagOpenStack_.empty() && startTagOpenStack_.back()) return WriteState::Element;
        return WriteState::Content;
    }
    if (startDocumentWritten_ || !document_.ChildNodes().empty()) return WriteState::Prolog;
    return WriteState::Start;
}

std::string XmlWriter::LookupPrefix(std::string_view namespaceUri) const {
    if (UsesDirectOutput()) {
        return LookupDirectNamespacePrefix(namespaceUri);
    }
    return LookupNamespacePrefix(namespaceUri);
}

void XmlWriter::EnsureDocumentOpen(std::string_view operation) const {
    if (documentClosed_) {
        throw XmlException("Cannot " + std::string(operation) + " after the XML document has been closed");
    }
}

void XmlWriter::EnsureOpenElement(std::string_view operation) const {
    EnsureDocumentOpen(operation);
    const bool hasOpenElement = UsesDirectOutput() ? !directElementStack_.empty() : !elementStack_.empty();
    if (!hasOpenElement) {
        throw XmlException("Cannot " + std::string(operation) + " outside of an element");
    }
}

void XmlWriter::EnsureOpenStartElement(std::string_view operation) const {
    EnsureOpenElement(operation);
    const bool startTagOpen = UsesDirectOutput()
        ? directElementStack_.back().startTagOpen
        : startTagOpenStack_.back();
    if (!startTagOpen) {
        throw XmlException("Cannot " + std::string(operation) + " after the current element start tag has been closed");
    }
}

void XmlWriter::EnsureNoOpenAttribute(std::string_view operation) const {
    if (currentAttributeName_.has_value()) {
        throw XmlException("Cannot " + std::string(operation) + " while an attribute is still open");
    }
}

void XmlWriter::EnsureOutputReady() const {
    EnsureNoOpenAttribute("produce XML output");
    const bool hasOpenElements = UsesDirectOutput() ? !directElementStack_.empty() : !elementStack_.empty();
    if (hasOpenElements) {
        throw XmlException("Cannot produce XML output while elements are still open");
    }
    if (settings_.Conformance == ConformanceLevel::Fragment) {
        return;
    }
    const bool hasRootElement = UsesDirectOutput()
        ? directHasRootElement_
        : document_.DocumentElement() != nullptr;
    if (!hasRootElement) {
        throw XmlException("Cannot produce XML output without a root element");
    }
}

void XmlWriter::MarkCurrentElementContentStarted() {
    if (UsesDirectOutput()) {
        if (!directElementStack_.empty()) {
            directElementStack_.back().startTagOpen = false;
        }
        return;
    }
    if (!startTagOpenStack_.empty()) {
        startTagOpenStack_.back() = false;
    }
}

std::string XmlWriter::LookupNamespacePrefix(std::string_view namespaceUri) const {
    if (namespaceUri.empty()) {
        return {};
    }

    if (namespaceUri == "http://www.w3.org/XML/1998/namespace") {
        return "xml";
    }
    if (namespaceUri == "http://www.w3.org/2000/xmlns/") {
        return "xmlns";
    }
    if (elementStack_.empty()) {
        return {};
    }

    for (const XmlElement* current = elementStack_.back().get(); current != nullptr; ) {
        for (const auto& attribute : current->Attributes()) {
            if (!IsNamespaceDeclarationName(attribute->Name())) {
                continue;
            }
            if (attribute->Value() == namespaceUri) {
                return NamespaceDeclarationPrefix(attribute->Name());
            }
        }

        const XmlNode* parent = current->ParentNode();
        current = parent != nullptr && parent->NodeType() == XmlNodeType::Element
            ? static_cast<const XmlElement*>(parent)
            : nullptr;
    }

    return {};
}

void XmlWriter::WriteDirectTopLevelSeparatorIfNeeded() {
    if (settings_.Indent && directHasTopLevelContent_) {
        *directOutputStream_ << settings_.NewLineChars;
    }
}

void XmlWriter::EnsureDirectCurrentStartTagClosed() {
    if (directElementStack_.empty() || !directElementStack_.back().startTagOpen) {
        return;
    }
    *directOutputStream_ << '>';
    directElementStack_.back().startTagOpen = false;
}

void XmlWriter::PrepareDirectParentForChild(bool textLikeChild) {
    if (directElementStack_.empty()) {
        return;
    }

    auto& parent = directElementStack_.back();
    EnsureDirectCurrentStartTagClosed();
    if (!textLikeChild && settings_.Indent && !parent.hasTextLikeChild) {
        *directOutputStream_ << settings_.NewLineChars;
        std::string indentation;
        AppendIndent(indentation, static_cast<int>(directElementStack_.size()), settings_);
        *directOutputStream_ << indentation;
    }
    parent.hasAnyChild = true;
    if (textLikeChild) {
        parent.hasTextLikeChild = true;
    } else {
        parent.hasNonTextChild = true;
    }
}

void XmlWriter::WriteDirectAttribute(std::string_view name, std::string_view value) {
    *directOutputStream_ << ' ' << name << "=\"" << EscapeAttribute(value, settings_) << '"';
}

bool XmlWriter::HasDirectNamespaceBinding(std::string_view prefix, std::string_view namespaceUri) const {
    if (namespaceUri.empty()) {
        return prefix.empty();
    }

    if (prefix == "xml") {
        return namespaceUri == "http://www.w3.org/XML/1998/namespace";
    }

    for (auto it = directElementStack_.rbegin(); it != directElementStack_.rend(); ++it) {
        if (!it->namespaceDeclarations) {
            continue;
        }
        const auto found = it->namespaceDeclarations->find(std::string(prefix));
        if (found != it->namespaceDeclarations->end()) {
            return found->second == namespaceUri;
        }
    }

    return false;
}

std::string XmlWriter::LookupDirectNamespacePrefix(std::string_view namespaceUri) const {
    if (namespaceUri.empty()) {
        return {};
    }
    if (namespaceUri == "http://www.w3.org/XML/1998/namespace") {
        return "xml";
    }
    if (namespaceUri == "http://www.w3.org/2000/xmlns/") {
        return "xmlns";
    }

    for (auto it = directElementStack_.rbegin(); it != directElementStack_.rend(); ++it) {
        if (!it->namespaceDeclarations) {
            continue;
        }
        for (const auto& [prefix, uri] : *it->namespaceDeclarations) {
            if (uri == namespaceUri) {
                return prefix;
            }
        }
    }

    return {};
}

void XmlWriter::DeclareDirectNamespaceIfNeeded(std::string_view prefix, std::string_view namespaceUri) {
    if (namespaceUri.empty() || directElementStack_.empty() || HasDirectNamespaceBinding(prefix, namespaceUri)) {
        return;
    }

    auto& current = directElementStack_.back();
    const std::string attributeName = prefix.empty() ? "xmlns" : "xmlns:" + std::string(prefix);
    WriteDirectAttribute(attributeName, namespaceUri);
    if (!current.namespaceDeclarations) {
        current.namespaceDeclarations = std::make_unique<std::unordered_map<std::string, std::string>>();
    }
    (*current.namespaceDeclarations)[std::string(prefix)] = std::string(namespaceUri);
}

void XmlWriter::WriteDirectTopLevelNode(const XmlNode& node) {
    if (settings_.Conformance == ConformanceLevel::Fragment) {
        WriteDirectTopLevelSeparatorIfNeeded();
        SerializeNodeToStream(node, settings_, *directOutputStream_, 0);
        directHasTopLevelContent_ = true;
        return;
    }

    switch (node.NodeType()) {
    case XmlNodeType::XmlDeclaration:
        if (directHasTopLevelContent_) {
            throw XmlException("XML declaration must be the first node in the document");
        }
        break;
    case XmlNodeType::DocumentType:
        if (directHasDocumentType_) {
            throw XmlException("XML document can only contain a single DOCTYPE declaration");
        }
        if (directHasRootElement_) {
            throw XmlException("DOCTYPE must appear before the root element");
        }
        break;
    case XmlNodeType::Element:
        if (directHasRootElement_) {
            throw XmlException("The XML document already has a root element");
        }
        break;
    default:
        break;
    }

    WriteDirectTopLevelSeparatorIfNeeded();
    SerializeNodeToStream(node, settings_, *directOutputStream_, 0);
    directHasTopLevelContent_ = true;
    if (node.NodeType() == XmlNodeType::DocumentType) {
        directHasDocumentType_ = true;
    } else if (node.NodeType() == XmlNodeType::Element) {
        directHasRootElement_ = true;
    }
}

void XmlWriter::WriteStartDocument(std::string_view version, std::string_view encoding, std::string_view standalone) {
    EnsureDocumentOpen("write the XML declaration");
    EnsureNoOpenAttribute("write the XML declaration");
    ValidateXmlDeclarationVersion(version);
    ValidateXmlDeclarationEncoding(encoding);
    ValidateXmlDeclarationStandalone(standalone);
    if (settings_.Conformance == ConformanceLevel::Fragment) {
        throw XmlException("XML declaration is only allowed in document conformance mode");
    }
    if (UsesDirectOutput()) {
        if (startDocumentWritten_) {
            throw XmlException("XML declaration has already been written");
        }
        if (!directElementStack_.empty()) {
            throw XmlException("Cannot write XML declaration inside an element");
        }
        if (directHasTopLevelContent_) {
            throw XmlException("XML declaration must be the first node in the document");
        }

        const std::string effectiveEncoding = encoding.empty() ? settings_.Encoding : std::string(encoding);
        *directOutputStream_ << "<?xml version=\"" << EscapeAttribute(version, settings_) << '\"';
        if (!effectiveEncoding.empty()) {
            *directOutputStream_ << " encoding=\"" << EscapeAttribute(effectiveEncoding, settings_) << '\"';
        }
        if (!standalone.empty()) {
            *directOutputStream_ << " standalone=\"" << EscapeAttribute(standalone, settings_) << '\"';
        }
        *directOutputStream_ << "?>";
        startDocumentWritten_ = true;
        directHasTopLevelContent_ = true;
        return;
    }
    if (document_.Declaration() != nullptr) {
        throw XmlException("XML declaration has already been written");
    }
    if (!elementStack_.empty()) {
        throw XmlException("Cannot write XML declaration inside an element");
    }
    if (!document_.ChildNodes().empty()) {
        throw XmlException("XML declaration must be the first node in the document");
    }

    const std::string effectiveEncoding = encoding.empty() ? settings_.Encoding : std::string(encoding);
    document_.AppendChild(document_.CreateXmlDeclaration(version, effectiveEncoding, standalone));
    startDocumentWritten_ = true;
}

void XmlWriter::WriteDocType(
    std::string_view name,
    std::string_view publicId,
    std::string_view systemId,
    std::string_view internalSubset) {
    EnsureDocumentOpen("write a document type declaration");
    EnsureNoOpenAttribute("write a document type declaration");
    if (settings_.Conformance == ConformanceLevel::Fragment) {
        throw XmlException("DOCTYPE is only allowed in document conformance mode");
    }
    if (UsesDirectOutput()) {
        if (!directElementStack_.empty()) {
            throw XmlException("Cannot write DOCTYPE inside an element");
        }
        if (directHasRootElement_) {
            throw XmlException("DOCTYPE must appear before the root element");
        }
        if (directHasDocumentType_) {
            throw XmlException("XML document can only contain a single DOCTYPE declaration");
        }

        WriteDirectTopLevelSeparatorIfNeeded();
        *directOutputStream_ << "<!DOCTYPE " << name;
        if (!publicId.empty()) {
            *directOutputStream_ << " PUBLIC \"" << publicId << "\" \"" << systemId << "\"";
        } else if (!systemId.empty()) {
            *directOutputStream_ << " SYSTEM \"" << systemId << "\"";
        }
        if (!internalSubset.empty()) {
            *directOutputStream_ << " [" << internalSubset << ']';
        }
        *directOutputStream_ << '>';
        directHasDocumentType_ = true;
        directHasTopLevelContent_ = true;
        return;
    }
    if (!elementStack_.empty()) {
        throw XmlException("Cannot write DOCTYPE inside an element");
    }
    if (document_.DocumentElement() != nullptr) {
        throw XmlException("DOCTYPE must appear before the root element");
    }

    document_.AppendChild(document_.CreateDocumentType(name, publicId, systemId, internalSubset));
}

void XmlWriter::WriteStartElement(std::string_view name) {
    EnsureDocumentOpen("write a start element");
    EnsureNoOpenAttribute("write a start element");
    if (UsesDirectOutput()) {
        if (directElementStack_.empty()) {
            if (settings_.Conformance != ConformanceLevel::Fragment && directHasRootElement_) {
                throw XmlException("The XML document already has a root element");
            }
            WriteDirectTopLevelSeparatorIfNeeded();
            if (settings_.Conformance != ConformanceLevel::Fragment) {
                directHasRootElement_ = true;
            }
            directHasTopLevelContent_ = true;
        } else {
            PrepareDirectParentForChild(false);
        }

        directElementStack_.push_back(DirectElementState{});
        directElementStack_.back().name = std::string(name);
        *directOutputStream_ << '<' << name;
        return;
    }
    if (settings_.Conformance != ConformanceLevel::Fragment
        && elementStack_.empty() && document_.DocumentElement() != nullptr) {
        throw XmlException("The XML document already has a root element");
    }

    auto element = document_.CreateElement(name);
    if (!elementStack_.empty()) {
        MarkCurrentElementContentStarted();
    }
    if (elementStack_.empty()) {
        AppendDocumentLevelNode(element);
    } else {
        elementStack_.back()->AppendChild(element);
    }
    elementStack_.push_back(element);
    startTagOpenStack_.push_back(true);
}

void XmlWriter::WriteStartElement(std::string_view prefix, std::string_view localName, std::string_view namespaceUri) {
    WriteStartElement(ComposeQualifiedName(prefix, localName));
    if (UsesDirectOutput()) {
        if (!namespaceUri.empty()) {
            DeclareDirectNamespaceIfNeeded(prefix, namespaceUri);
        }
        return;
    }
    if (!namespaceUri.empty()) {
        const std::string xmlnsName = prefix.empty() ? "xmlns" : "xmlns:" + std::string(prefix);
        if (!elementStack_.back()->HasAttribute(xmlnsName)) {
            elementStack_.back()->SetAttribute(xmlnsName, std::string(namespaceUri));
        }
    }
}

void XmlWriter::WriteStartAttribute(std::string_view name) {
    EnsureOpenStartElement("write an attribute");
    EnsureNoOpenAttribute("write an attribute");
    currentAttributeName_ = std::string(name);
    currentAttributeValue_.clear();
}

void XmlWriter::WriteStartAttribute(
    std::string_view prefix,
    std::string_view localName,
    std::string_view namespaceUri) {
    WriteStartAttribute(ComposeQualifiedName(prefix, localName));

    if (UsesDirectOutput()) {
        if (!prefix.empty() && !namespaceUri.empty()) {
            DeclareDirectNamespaceIfNeeded(prefix, namespaceUri);
        }
        return;
    }

    if (!prefix.empty() && !namespaceUri.empty()) {
        const std::string xmlnsName = "xmlns:" + std::string(prefix);
        if (!elementStack_.back()->HasAttribute(xmlnsName)) {
            elementStack_.back()->SetAttribute(xmlnsName, std::string(namespaceUri));
        }
    }
}

void XmlWriter::WriteEndAttribute() {
    EnsureDocumentOpen("end an attribute");
    if (!currentAttributeName_.has_value()) {
        throw XmlException("Cannot end an attribute when no attribute is open");
    }

    if (UsesDirectOutput()) {
        WriteDirectAttribute(*currentAttributeName_, currentAttributeValue_);
        currentAttributeName_.reset();
        currentAttributeValue_.clear();
        return;
    }

    elementStack_.back()->SetAttribute(*currentAttributeName_, currentAttributeValue_);
    currentAttributeName_.reset();
    currentAttributeValue_.clear();
}

void XmlWriter::WriteAttributeString(std::string_view name, std::string_view value) {
    EnsureOpenStartElement("write an attribute");
    EnsureNoOpenAttribute("write an attribute");
    if (UsesDirectOutput()) {
        WriteDirectAttribute(name, value);
        return;
    }

    elementStack_.back()->SetAttribute(std::string(name), std::string(value));
}

void XmlWriter::WriteAttributeString(
    std::string_view prefix,
    std::string_view localName,
    std::string_view namespaceUri,
    std::string_view value) {
    EnsureOpenStartElement("write an attribute");
    EnsureNoOpenAttribute("write an attribute");

    const std::string qualifiedName = ComposeQualifiedName(prefix, localName);
    if (UsesDirectOutput()) {
        if (!prefix.empty() && !namespaceUri.empty()) {
            DeclareDirectNamespaceIfNeeded(prefix, namespaceUri);
        }
        WriteDirectAttribute(qualifiedName, value);
        return;
    }

    if (!prefix.empty() && !namespaceUri.empty()) {
        const std::string xmlnsName = "xmlns:" + std::string(prefix);
        if (!elementStack_.back()->HasAttribute(xmlnsName)) {
            elementStack_.back()->SetAttribute(xmlnsName, std::string(namespaceUri));
        }
    }

    elementStack_.back()->SetAttribute(qualifiedName, std::string(value));
}

void XmlWriter::WriteAttributes(const XmlAttributeCollection& attributes) {
    EnsureOpenStartElement("write attributes");
    EnsureNoOpenAttribute("write attributes");
    for (const auto& attribute : attributes) {
        if (attribute != nullptr) {
            WriteAttributeString(attribute->Name(), attribute->Value());
        }
    }
}

void XmlWriter::WriteAttributes(const XmlElement& element) {
    EnsureOpenStartElement("write attributes");
    EnsureNoOpenAttribute("write attributes");
    for (const auto& attribute : element.attributes_) {
        if (attribute != nullptr) {
            WriteAttributeNode(*this, *attribute);
        }
    }
    for (const auto& pendingAttribute : element.pendingLoadAttributes_) {
        WritePendingAttribute(
            *this,
            element,
            element.PendingLoadAttributeNameView(pendingAttribute),
            element.PendingLoadAttributeValueView(pendingAttribute));
    }
}

void XmlWriter::WriteCharEntity(unsigned int codePoint) {
    EnsureNoOpenAttribute("write a character entity");
    EnsureOpenElement("write a character entity");
    if (codePoint == 0 || codePoint > 0x10FFFF || (codePoint >= 0xD800 && codePoint <= 0xDFFF)) {
        throw XmlException("WriteCharEntity requires a valid Unicode scalar value");
    }

    std::ostringstream name;
    name << "#x" << std::uppercase << std::hex << codePoint;
    if (UsesDirectOutput()) {
        PrepareDirectParentForChild(true);
        *directOutputStream_ << '&' << name.str() << ';';
        return;
    }
    MarkCurrentElementContentStarted();
    elementStack_.back()->AppendChild(document_.CreateEntityReference(name.str()));
}

void XmlWriter::WriteEntityRef(std::string_view name) {
    EnsureNoOpenAttribute("write an entity reference");
    EnsureOpenElement("write an entity reference");
    if (name.empty()) {
        throw XmlException("WriteEntityRef requires an entity name");
    }

    if (UsesDirectOutput()) {
        PrepareDirectParentForChild(true);
        *directOutputStream_ << '&' << name << ';';
        return;
    }

    MarkCurrentElementContentStarted();
    elementStack_.back()->AppendChild(document_.CreateEntityReference(name));
}

void XmlWriter::WriteString(std::string_view text) {
    if (currentAttributeName_.has_value()) {
        currentAttributeValue_ += text;
        return;
    }

    EnsureOpenElement("write text");

    if (UsesDirectOutput()) {
        PrepareDirectParentForChild(true);
        *directOutputStream_ << EscapeText(text, settings_);
        return;
    }

    MarkCurrentElementContentStarted();
    elementStack_.back()->AppendChild(document_.CreateTextNode(text));
}

void XmlWriter::WriteValue(std::string_view value) {
    WriteString(value);
}

void XmlWriter::WriteValue(bool value) {
    WriteString(value ? "true" : "false");
}

void XmlWriter::WriteValue(int value) {
    WriteString(std::to_string(value));
}

void XmlWriter::WriteValue(double value) {
    std::ostringstream stream;
    stream << value;
    WriteString(stream.str());
}

void XmlWriter::WriteName(std::string_view name) {
    if (!IsValidXmlQualifiedName(name)) {
        throw XmlException("WriteName requires a valid XML name");
    }
    WriteString(name);
}

void XmlWriter::WriteQualifiedName(std::string_view localName, std::string_view namespaceUri) {
    if (!IsValidXmlNameToken(localName)) {
        throw XmlException("WriteQualifiedName requires a valid XML local name");
    }

    if (namespaceUri.empty()) {
        WriteName(localName);
        return;
    }

    const auto prefix = UsesDirectOutput()
        ? LookupDirectNamespacePrefix(namespaceUri)
        : LookupNamespacePrefix(namespaceUri);
    if (!prefix.empty()) {
        WriteName(ComposeQualifiedName(prefix, localName));
        return;
    }

    if (!UsesDirectOutput() && !elementStack_.empty()) {
        std::string_view defaultNamespaceUri;
        if (LookupNamespaceUriOnElementView(elementStack_.back().get(), {}, defaultNamespaceUri)
            && defaultNamespaceUri == namespaceUri) {
            WriteName(localName);
            return;
        }
    }

    if (UsesDirectOutput() && HasDirectNamespaceBinding({}, namespaceUri)) {
        WriteName(localName);
        return;
    }

    throw XmlException("WriteQualifiedName requires a namespace prefix bound to URI: " + std::string(namespaceUri));
}

void XmlWriter::WriteWhitespace(std::string_view whitespace) {
    EnsureDocumentOpen("write whitespace");
    EnsureNoOpenAttribute("write whitespace");
    if (UsesDirectOutput()) {
        if (directElementStack_.empty()) {
            WriteDirectTopLevelSeparatorIfNeeded();
            *directOutputStream_ << EscapeText(whitespace, settings_);
            directHasTopLevelContent_ = true;
        } else {
            PrepareDirectParentForChild(true);
            *directOutputStream_ << EscapeText(whitespace, settings_);
        }
        return;
    }
    auto node = document_.CreateWhitespace(whitespace);
    if (elementStack_.empty()) {
        AppendDocumentLevelNode(node);
    } else {
        MarkCurrentElementContentStarted();
        elementStack_.back()->AppendChild(node);
    }
}

void XmlWriter::WriteSignificantWhitespace(std::string_view whitespace) {
    EnsureDocumentOpen("write significant whitespace");
    EnsureNoOpenAttribute("write significant whitespace");
    if (UsesDirectOutput()) {
        if (directElementStack_.empty()) {
            WriteDirectTopLevelSeparatorIfNeeded();
            *directOutputStream_ << EscapeText(whitespace, settings_);
            directHasTopLevelContent_ = true;
        } else {
            PrepareDirectParentForChild(true);
            *directOutputStream_ << EscapeText(whitespace, settings_);
        }
        return;
    }
    auto node = document_.CreateSignificantWhitespace(whitespace);
    if (elementStack_.empty()) {
        AppendDocumentLevelNode(node);
    } else {
        MarkCurrentElementContentStarted();
        elementStack_.back()->AppendChild(node);
    }
}

void XmlWriter::WriteCData(std::string_view text) {
    EnsureNoOpenAttribute("write CDATA");
    EnsureOpenElement("write CDATA");

    if (UsesDirectOutput()) {
        PrepareDirectParentForChild(true);
        *directOutputStream_ << "<![CDATA[" << text << "]]>";
        return;
    }

    MarkCurrentElementContentStarted();
    elementStack_.back()->AppendChild(document_.CreateCDataSection(text));
}

void XmlWriter::WriteComment(std::string_view text) {
    EnsureDocumentOpen("write a comment");
    EnsureNoOpenAttribute("write a comment");
    if (UsesDirectOutput()) {
        if (directElementStack_.empty()) {
            WriteDirectTopLevelSeparatorIfNeeded();
            *directOutputStream_ << "<!--" << text << "-->";
            directHasTopLevelContent_ = true;
        } else {
            PrepareDirectParentForChild(false);
            *directOutputStream_ << "<!--" << text << "-->";
        }
        return;
    }
    auto comment = document_.CreateComment(text);
    if (elementStack_.empty()) {
        AppendDocumentLevelNode(comment);
    } else {
        MarkCurrentElementContentStarted();
        elementStack_.back()->AppendChild(comment);
    }
}

void XmlWriter::WriteProcessingInstruction(std::string_view name, std::string_view text) {
    EnsureDocumentOpen("write a processing instruction");
    EnsureNoOpenAttribute("write a processing instruction");
    if (UsesDirectOutput()) {
        if (directElementStack_.empty()) {
            WriteDirectTopLevelSeparatorIfNeeded();
            *directOutputStream_ << "<?" << name;
            if (!text.empty()) {
                *directOutputStream_ << ' ' << text;
            }
            *directOutputStream_ << "?>";
            directHasTopLevelContent_ = true;
        } else {
            PrepareDirectParentForChild(false);
            *directOutputStream_ << "<?" << name;
            if (!text.empty()) {
                *directOutputStream_ << ' ' << text;
            }
            *directOutputStream_ << "?>";
        }
        return;
    }
    auto instruction = document_.CreateProcessingInstruction(name, text);
    if (elementStack_.empty()) {
        AppendDocumentLevelNode(instruction);
    } else {
        MarkCurrentElementContentStarted();
        elementStack_.back()->AppendChild(instruction);
    }
}

void XmlWriter::WriteRaw(std::string_view xml) {
    EnsureDocumentOpen("write raw XML");
    EnsureNoOpenAttribute("write raw XML");
    if (xml.empty()) {
        return;
    }

    const auto selectTopLevelRawConformance = [this](std::string_view rawXml, bool hasOpenElement) {
        if (hasOpenElement) {
            return ConformanceLevel::Fragment;
        }
        if (settings_.Conformance == ConformanceLevel::Fragment) {
            return ConformanceLevel::Fragment;
        }

        const auto firstNonWhitespace = rawXml.find_first_not_of(" \t\r\n");
        if (firstNonWhitespace == std::string_view::npos) {
            return ConformanceLevel::Fragment;
        }

        const auto prolog = rawXml.substr(firstNonWhitespace);
        if (prolog.starts_with("<?xml") || prolog.starts_with("<!DOCTYPE")) {
            return ConformanceLevel::Document;
        }

        return ConformanceLevel::Fragment;
    };

    if (UsesDirectOutput()) {
        XmlReaderSettings readerSettings;
        readerSettings.Conformance = selectTopLevelRawConformance(xml, !directElementStack_.empty());
        auto reader = XmlReader::Create(std::string(xml), readerSettings);
        WriteNodesFromReader(*this, reader);
        return;
    }

    const auto checkpoint = CaptureInMemoryMutationCheckpoint();

    try {
        XmlReaderSettings readerSettings;
        readerSettings.Conformance = selectTopLevelRawConformance(xml, !checkpoint.elementStack.empty());
        auto reader = XmlReader::Create(std::string(xml), readerSettings);
        WriteNodesFromReader(*this, reader);
        return;
    } catch (...) {
        RollbackInMemoryMutation(checkpoint);
        throw;
    }
}

void XmlWriter::WriteBase64(const unsigned char* data, std::size_t length) {
    EnsureDocumentOpen("write base64 content");
    EnsureNoOpenAttribute("write base64 content");
    if (data == nullptr && length != 0) {
        throw XmlException("WriteBase64 requires a non-null buffer when length is non-zero");
    }
    WriteString(EncodeBase64(data, length));
}

void XmlWriter::WriteElementString(std::string_view name, std::string_view value) {
    EnsureDocumentOpen("write an element string");
    EnsureNoOpenAttribute("write an element string");
    WriteStartElement(name);
    WriteString(value);
    WriteEndElement();
}

void XmlWriter::WriteElementString(std::string_view prefix, std::string_view localName, std::string_view namespaceUri, std::string_view value) {
    EnsureDocumentOpen("write an element string");
    EnsureNoOpenAttribute("write an element string");
    WriteStartElement(prefix, localName, namespaceUri);
    WriteString(value);
    WriteEndElement();
}

void XmlWriter::WriteNode(const XmlNode& node) {
    EnsureDocumentOpen("write a node");
    EnsureNoOpenAttribute("write a node");

    if (node.NodeType() == XmlNodeType::Document) {
        if (UsesDirectOutput()) {
            if (!directElementStack_.empty()) {
                throw XmlException("Cannot write document-level nodes inside an element");
            }
            for (const auto& child : node.ChildNodes()) {
                if (child != nullptr) {
                    WriteNode(*child);
                }
            }
            return;
        }

        if (!elementStack_.empty()) {
            throw XmlException("Cannot write document-level nodes inside an element");
        }

        const auto checkpoint = CaptureInMemoryMutationCheckpoint();
        try {
            for (const auto& child : node.ChildNodes()) {
                if (child != nullptr) {
                    WriteNodeInMemory(*child);
                }
            }
        } catch (...) {
            RollbackInMemoryMutation(checkpoint);
            throw;
        }

        return;
    }

    if (UsesDirectOutput() && node.NodeType() == XmlNodeType::DocumentFragment) {
        for (const auto& child : node.ChildNodes()) {
            if (child != nullptr) {
                WriteNode(*child);
            }
        }
        return;
    }

    if (node.NodeType() == XmlNodeType::Attribute) {
        EnsureOpenStartElement("write an attribute node");
        WriteAttributeNode(*this, node);
        return;
    }

    if (UsesDirectOutput()) {
        if (directElementStack_.empty()) {
            if (node.NodeType() == XmlNodeType::XmlDeclaration || node.NodeType() == XmlNodeType::DocumentType || node.NodeType() == XmlNodeType::Element
                || node.NodeType() == XmlNodeType::Comment || node.NodeType() == XmlNodeType::ProcessingInstruction
                || node.NodeType() == XmlNodeType::Whitespace || node.NodeType() == XmlNodeType::SignificantWhitespace
                || node.NodeType() == XmlNodeType::Entity || node.NodeType() == XmlNodeType::Notation
                || node.NodeType() == XmlNodeType::Text || node.NodeType() == XmlNodeType::CDATA || node.NodeType() == XmlNodeType::EntityReference) {
                WriteDirectTopLevelNode(node);
                return;
            }
            throw XmlException("Unsupported node type for serialization");
        }

        if (node.NodeType() == XmlNodeType::XmlDeclaration || node.NodeType() == XmlNodeType::DocumentType) {
            throw XmlException("Cannot write document-level nodes inside an element");
        }

        const bool textLike = node.NodeType() == XmlNodeType::Text
            || node.NodeType() == XmlNodeType::Whitespace
            || node.NodeType() == XmlNodeType::SignificantWhitespace
            || node.NodeType() == XmlNodeType::CDATA
            || node.NodeType() == XmlNodeType::EntityReference;
        PrepareDirectParentForChild(textLike);
        SerializeNodeToStream(node, settings_, *directOutputStream_, static_cast<int>(directElementStack_.size()));
        return;
    }

    if (!elementStack_.empty()
        && (node.NodeType() == XmlNodeType::XmlDeclaration || node.NodeType() == XmlNodeType::DocumentType)) {
        throw XmlException("Cannot write document-level nodes inside an element");
    }

    const auto checkpoint = CaptureInMemoryMutationCheckpoint();
    try {
        WriteNodeInMemory(node);
    } catch (...) {
        RollbackInMemoryMutation(checkpoint);
        throw;
    }
}

void XmlWriter::WriteNodeInMemory(const XmlNode& node) {
    if (!elementStack_.empty()
        && (node.NodeType() == XmlNodeType::XmlDeclaration || node.NodeType() == XmlNodeType::DocumentType)) {
        throw XmlException("Cannot write document-level nodes inside an element");
    }

    switch (node.NodeType()) {
    case XmlNodeType::XmlDeclaration: {
        const auto& declaration = static_cast<const XmlDeclaration&>(node);
        WriteStartDocument(
            declaration.Version(),
            declaration.Encoding(),
            declaration.Standalone());
        return;
    }
    case XmlNodeType::DocumentType: {
        const auto& documentType = static_cast<const XmlDocumentType&>(node);
        WriteDocType(
            documentType.Name(),
            documentType.PublicId(),
            documentType.SystemId(),
            documentType.InternalSubset());
        return;
    }
    case XmlNodeType::Element: {
        const auto& element = static_cast<const XmlElement&>(node);
        WriteStartElement(element.Prefix(), element.LocalName(), element.NamespaceURI());
        WriteAttributes(element);
        for (const auto& child : element.ChildNodes()) {
            if (child != nullptr) {
                WriteNodeInMemory(*child);
            }
        }
        if (element.WritesFullEndElement()) {
            WriteFullEndElement();
        } else {
            WriteEndElement();
        }
        return;
    }
    case XmlNodeType::DocumentFragment:
        for (const auto& child : node.ChildNodes()) {
            if (child != nullptr) {
                WriteNodeInMemory(*child);
            }
        }
        return;
    case XmlNodeType::Text:
        WriteString(node.Value());
        return;
    case XmlNodeType::CDATA:
        WriteCData(node.Value());
        return;
    case XmlNodeType::Comment:
        WriteComment(node.Value());
        return;
    case XmlNodeType::ProcessingInstruction: {
        const auto& instruction = static_cast<const XmlProcessingInstruction&>(node);
        WriteProcessingInstruction(instruction.Target(), instruction.Data());
        return;
    }
    case XmlNodeType::Whitespace:
        WriteWhitespace(node.Value());
        return;
    case XmlNodeType::SignificantWhitespace:
        WriteSignificantWhitespace(node.Value());
        return;
    case XmlNodeType::EntityReference:
        WriteEntityRef(node.Name());
        return;
    default:
        break;
    }

    const auto sameDocumentNode = node.OwnerDocument() == &document_;
    auto clonedOrImported = sameDocumentNode
        ? node.CloneNode(true)
        : document_.ImportNode(node, true);
    if (elementStack_.empty()) {
        AppendDocumentLevelNode(clonedOrImported);
        return;
    }

    MarkCurrentElementContentStarted();
    elementStack_.back()->AppendChild(clonedOrImported);
}

void XmlWriter::WriteNode(XmlReader& reader, bool defattr) {
    EnsureDocumentOpen("write a node");
    EnsureNoOpenAttribute("write a node");

    if (reader.GetReadState() == ReadState::Initial) {
        reader.Read();
    }

    const bool canRollback = !UsesDirectOutput();
    const auto checkpoint = canRollback ? CaptureInMemoryMutationCheckpoint() : InMemoryMutationCheckpoint{};

    try {
        int startDepth = reader.Depth();
        do {
            switch (reader.NodeType()) {
            case XmlNodeType::Element: {
                WriteStartElement(reader.Prefix(), reader.LocalName(), reader.NamespaceURI());
                if (defattr && reader.MoveToFirstAttribute()) {
                    do {
                        if (IsNamespaceDeclarationName(reader.Name())) {
                            WriteAttributeString(std::string_view(reader.Name()), std::string_view(reader.Value()));
                        } else {
                            WriteAttributeString(
                                std::string_view(reader.Prefix()),
                                std::string_view(reader.LocalName()),
                                std::string_view(reader.NamespaceURI()),
                                std::string_view(reader.Value()));
                        }
                    } while (reader.MoveToNextAttribute());
                    reader.MoveToElement();
                }
                if (reader.IsEmptyElement()) {
                    WriteEndElement();
                }
                break;
            }
            case XmlNodeType::Text:
                WriteString(reader.Value());
                break;
            case XmlNodeType::CDATA:
                WriteCData(reader.Value());
                break;
            case XmlNodeType::Comment:
                WriteComment(reader.Value());
                break;
            case XmlNodeType::ProcessingInstruction:
                WriteProcessingInstruction(reader.Name(), reader.Value());
                break;
            case XmlNodeType::Whitespace:
                WriteWhitespace(reader.Value());
                break;
            case XmlNodeType::SignificantWhitespace:
                WriteSignificantWhitespace(reader.Value());
                break;
            case XmlNodeType::EntityReference:
                WriteEntityRef(reader.Name());
                {
                    int openEntityCount = 1;
                    while (openEntityCount > 0 && reader.Read()) {
                        if (reader.NodeType() == XmlNodeType::EntityReference) {
                            ++openEntityCount;
                        } else if (reader.NodeType() == XmlNodeType::EndEntity) {
                            --openEntityCount;
                        }
                    }
                }
                break;
            case XmlNodeType::EndElement:
                WriteFullEndElement();
                break;
            case XmlNodeType::XmlDeclaration: {
                std::string version = reader.GetAttribute("version");
                std::string encoding = reader.GetAttribute("encoding");
                std::string standalone = reader.GetAttribute("standalone");
                WriteStartDocument(
                    version.empty() ? "1.0" : version,
                    encoding,
                    standalone);
                break;
            }
            case XmlNodeType::DocumentType:
                if (UsesDirectOutput() ? !directElementStack_.empty() : !elementStack_.empty()) {
                    throw XmlException("Cannot write document-level nodes inside an element");
                }

                {
                    const auto declaration = ParseDocumentTypeDeclaration(reader.ReadOuterXml());
                    WriteDocType(
                        declaration.name,
                        declaration.publicId,
                        declaration.systemId,
                        declaration.internalSubset);
                }
                break;
            default:
                break;
            }
        } while (reader.Read() && reader.Depth() > startDepth);

        if (reader.NodeType() == XmlNodeType::EndElement && reader.Depth() == startDepth) {
            WriteFullEndElement();
            reader.Read();
        }
    } catch (...) {
        if (!canRollback) {
            throw;
        }

        RollbackInMemoryMutation(checkpoint);
        throw;
    }
}

void XmlWriter::AppendDocumentLevelNode(const std::shared_ptr<XmlNode>& node) {
    if (node == nullptr) {
        return;
    }

    if (node->NodeType() == XmlNodeType::DocumentFragment) {
        if (node->OwnerDocument() == &document_) {
            while (node->FirstChild() != nullptr) {
                AppendDocumentLevelNode(node->RemoveChild(node->FirstChild()));
            }
            return;
        }

        for (const auto& child : node->ChildNodes()) {
            if (child != nullptr) {
                AppendDocumentLevelNode(document_.ImportNode(*child, true));
            }
        }
        return;
    }

    if (settings_.Conformance == ConformanceLevel::Fragment) {
        if (node->NodeType() == XmlNodeType::XmlDeclaration) {
            throw XmlException("XML declaration is only allowed in document conformance mode");
        }
        if (node->NodeType() == XmlNodeType::DocumentType) {
            throw XmlException("DOCTYPE is only allowed in document conformance mode");
        }
        EnsureFragmentRoot()->AppendChild(node);
        return;
    }

    if (node->NodeType() == XmlNodeType::XmlDeclaration && !document_.ChildNodes().empty()) {
        throw XmlException("XML declaration must be the first node in the document");
    }

    if (node->NodeType() == XmlNodeType::DocumentType && document_.DocumentElement() != nullptr) {
        throw XmlException("DOCTYPE must appear before the root element");
    }

    document_.AppendChild(node);
}

void XmlWriter::WriteEndElement() {
    EnsureNoOpenAttribute("close an element");
    EnsureOpenElement("close an element");

    if (UsesDirectOutput()) {
        auto& current = directElementStack_.back();
        if (current.startTagOpen && !current.forceFullEnd) {
            *directOutputStream_ << "/>";
            directElementStack_.pop_back();
            return;
        }

        if (current.startTagOpen) {
            *directOutputStream_ << '>';
            current.startTagOpen = false;
        }
        if (current.hasNonTextChild && !current.hasTextLikeChild && settings_.Indent) {
            *directOutputStream_ << settings_.NewLineChars;
            std::string indentation;
            AppendIndent(indentation, static_cast<int>(directElementStack_.size()) - 1, settings_);
            *directOutputStream_ << indentation;
        }
        *directOutputStream_ << "</" << current.name << ">";
        directElementStack_.pop_back();
        return;
    }

    elementStack_.pop_back();
    startTagOpenStack_.pop_back();
}

void XmlWriter::WriteFullEndElement() {
    EnsureNoOpenAttribute("close an element");
    EnsureOpenElement("close an element");

    if (UsesDirectOutput()) {
        directElementStack_.back().forceFullEnd = true;
        WriteEndElement();
        return;
    }

    elementStack_.back()->writeFullEndElement_ = true;
    elementStack_.pop_back();
    startTagOpenStack_.pop_back();
}

void XmlWriter::WriteEndDocument() {
    EnsureDocumentOpen("end the document");
    EnsureNoOpenAttribute("end the document");
    while (UsesDirectOutput() ? !directElementStack_.empty() : !elementStack_.empty()) {
        WriteEndElement();
    }
    documentClosed_ = true;
    if (UsesDirectOutput()) {
        directOutputStream_->flush();
    }
}

void XmlWriter::Flush() const {
    EnsureDocumentOpen("flush the writer");
    EnsureNoOpenAttribute("flush the writer");
    if (UsesDirectOutput()) {
        directOutputStream_->flush();
    }
}

void XmlWriter::Close() {
    EnsureDocumentOpen("close the writer");
    EnsureNoOpenAttribute("close the writer");
    WriteEndDocument();
}

std::string XmlWriter::GetString() const {
    EnsureOutputReady();
    if (UsesDirectOutput()) {
        const auto* stringStream = dynamic_cast<const std::ostringstream*>(directOutputStream_);
        if (stringStream == nullptr) {
            throw XmlException("GetString is only available for in-memory writers");
        }
        return stringStream->str();
    }
    return settings_.Conformance == ConformanceLevel::Fragment
        ? EnsureFragmentRoot()->InnerXml(settings_)
        : document_.ToString(settings_);
}

void XmlWriter::Save(std::ostream& stream) const {
    EnsureOutputReady();
    if (UsesDirectOutput()) {
        if (directOutputStream_ != &stream) {
            throw XmlException("Save is not available for stream-backed writers bound to a different output stream");
        }
        directOutputStream_->flush();
        return;
    }
    if (settings_.Conformance == ConformanceLevel::Fragment) {
        WriteToStream(*EnsureFragmentRoot(), stream, settings_);
        return;
    }
    WriteToStream(document_, stream, settings_);
}

void XmlWriter::Save(const std::string& path) const {
    EnsureOutputReady();
    if (UsesDirectOutput()) {
        throw XmlException("Save(path) is not available for stream-backed writers");
    }
    std::ofstream stream(std::filesystem::path(path), std::ios::binary | std::ios::trunc);
    if (!stream) {
        throw XmlException("Failed to write XML file: " + path);
    }
    Save(stream);
}

std::string XmlWriter::WriteToString(const XmlNode& node, const XmlWriterSettings& settings) {
    std::string output;
    SerializeNode(node, settings, output, 0);
    return output;
}

void XmlWriter::WriteToStream(const XmlNode& node, std::ostream& stream, const XmlWriterSettings& settings) {
    SerializeNodeToStream(node, settings, stream, 0);
}

void XmlWriter::WriteToFile(const XmlNode& node, const std::string& path, const XmlWriterSettings& settings) {
    std::ofstream stream(std::filesystem::path(path), std::ios::binary | std::ios::trunc);
    if (!stream) {
        throw XmlException("Failed to write XML file: " + path);
    }

    WriteToStream(node, stream, settings);
}

// ============================================================================
// XmlConvert
// ============================================================================


}  // namespace System::Xml
