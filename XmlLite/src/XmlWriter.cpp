#include "XmlWriter.h"

#include "XmlConvert.h"
#include "XmlReader.h"

#include <fstream>
#include <ostream>
#include <sstream>

namespace System::Xml {

namespace {

bool IsTextLike(XmlNodeType nodeType) {
    return nodeType == XmlNodeType::Text
        || nodeType == XmlNodeType::CDATA
        || nodeType == XmlNodeType::Whitespace
        || nodeType == XmlNodeType::SignificantWhitespace
        || nodeType == XmlNodeType::EntityReference;
}

std::string EscapeText(std::string_view value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (char ch : value) {
        switch (ch) {
        case '&': escaped += "&amp;"; break;
        case '<': escaped += "&lt;"; break;
        case '>': escaped += "&gt;"; break;
        default: escaped.push_back(ch); break;
        }
    }
    return escaped;
}

std::string EscapeAttribute(std::string_view value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (char ch : value) {
        switch (ch) {
        case '&': escaped += "&amp;"; break;
        case '<': escaped += "&lt;"; break;
        case '"': escaped += "&quot;"; break;
        case '\'': escaped += "&apos;"; break;
        case '\r': escaped += "&#xD;"; break;
        case '\n': escaped += "&#xA;"; break;
        case '\t': escaped += "&#x9;"; break;
        default: escaped.push_back(ch); break;
        }
    }
    return escaped;
}

void AppendIndent(std::string& output, int depth, const XmlWriterSettings& settings) {
    for (int i = 0; i < depth; ++i) {
        output += settings.IndentChars;
    }
}

void SerializeNode(const XmlNode& node, const XmlWriterSettings& settings, std::string& output, int depth);

void SerializeChildren(const XmlNode& node, const XmlWriterSettings& settings, std::string& output, int depth) {
    bool multiline = false;
    if (settings.Indent) {
        for (const auto& child : node.ChildNodes()) {
            if (!IsTextLike(child->NodeType())) {
                multiline = true;
                break;
            }
        }
    }

    for (const auto& child : node.ChildNodes()) {
        if (multiline) {
            output += settings.NewLineChars;
            AppendIndent(output, depth + 1, settings);
        }
        SerializeNode(*child, settings, output, depth + 1);
    }

    if (multiline && node.HasChildNodes()) {
        output += settings.NewLineChars;
        AppendIndent(output, depth, settings);
    }
}

void SerializeDocumentType(const XmlDocumentType& documentType, std::string& output) {
    output += "<!DOCTYPE ";
    output += documentType.Name();

    const std::string& internalSubset = documentType.InternalSubset();
    if (!documentType.PublicId().empty()) {
        output += " PUBLIC \"";
        output += EscapeAttribute(documentType.PublicId());
        output += "\" \"";
        output += EscapeAttribute(documentType.SystemId());
        output += "\"";
    } else if (!documentType.SystemId().empty()) {
        output += " SYSTEM \"";
        output += EscapeAttribute(documentType.SystemId());
        output += "\"";
    }

    if (!internalSubset.empty()) {
        if (documentType.PublicId().empty()
            && documentType.SystemId().empty()
            && (internalSubset.rfind("PUBLIC", 0) == 0
                || internalSubset.rfind("SYSTEM", 0) == 0
                || internalSubset.front() == '[')) {
            output.push_back(' ');
            output += internalSubset;
        } else {
            output += " [";
            output += internalSubset;
            output += "]";
        }
    }

    output.push_back('>');
}

void SerializeNode(const XmlNode& node, const XmlWriterSettings& settings, std::string& output, int depth) {
    switch (node.NodeType()) {
    case XmlNodeType::Document:
    case XmlNodeType::DocumentFragment: {
        bool first = true;
        for (const auto& child : node.ChildNodes()) {
            if (!first && settings.Indent) {
                output += settings.NewLineChars;
            }
            SerializeNode(*child, settings, output, depth);
            first = false;
        }
        break;
    }
    case XmlNodeType::Element: {
        const auto& element = static_cast<const XmlElement&>(node);
        output.push_back('<');
        output += element.Name();
        for (const auto& attribute : element.Attributes()) {
            output.push_back(' ');
            output += attribute->Name();
            output += "=\"";
            output += EscapeAttribute(attribute->Value());
            output.push_back('"');
        }

        if (!element.HasChildNodes() && !element.WritesFullEndElement()) {
            output += "/>";
            break;
        }

        output.push_back('>');
        SerializeChildren(element, settings, output, depth);
        output += "</";
        output += element.Name();
        output.push_back('>');
        break;
    }
    case XmlNodeType::Text:
    case XmlNodeType::Whitespace:
    case XmlNodeType::SignificantWhitespace:
        output += EscapeText(node.Value());
        break;
    case XmlNodeType::CDATA:
        output += "<![CDATA[";
        output += node.Value();
        output += "]]>";
        break;
    case XmlNodeType::Comment:
        output += "<!--";
        output += node.Value();
        output += "-->";
        break;
    case XmlNodeType::ProcessingInstruction:
        output += "<?";
        output += node.Name();
        if (!node.Value().empty()) {
            output.push_back(' ');
            output += node.Value();
        }
        output += "?>";
        break;
    case XmlNodeType::XmlDeclaration: {
        if (settings.OmitXmlDeclaration) {
            break;
        }
        const auto& declaration = static_cast<const XmlDeclaration&>(node);
        output += "<?xml version=\"";
        output += declaration.Version();
        output.push_back('"');
        if (!declaration.Encoding().empty()) {
            output += " encoding=\"";
            output += declaration.Encoding();
            output.push_back('"');
        }
        if (!declaration.Standalone().empty()) {
            output += " standalone=\"";
            output += declaration.Standalone();
            output.push_back('"');
        }
        output += "?>";
        break;
    }
    case XmlNodeType::DocumentType:
        SerializeDocumentType(static_cast<const XmlDocumentType&>(node), output);
        break;
    case XmlNodeType::EntityReference:
        output.push_back('&');
        output += node.Name();
        output.push_back(';');
        break;
    default:
        break;
    }
}

std::string ComposeQualifiedName(std::string_view prefix, std::string_view localName) {
    if (prefix.empty()) {
        return std::string(localName);
    }
    return std::string(prefix) + ":" + std::string(localName);
}

}  // namespace

XmlWriter::XmlWriter(XmlWriterSettings settings)
    : settings_(std::move(settings)) {
}

XmlWriter::XmlWriter(std::ostream& stream, XmlWriterSettings settings)
    : settings_(std::move(settings)), stream_(&stream) {
}

WriteState XmlWriter::GetWriteState() const noexcept {
    return state_;
}

void XmlWriter::WriteStartDocument(std::string_view version, std::string_view encoding, std::string_view standalone) {
    EnsureOpen();
    EnsureNoAttribute();
    if (!output_.empty()) {
        throw XmlException("XML declaration must be the first writer output");
    }

    if (!settings_.OmitXmlDeclaration) {
        output_ += "<?xml version=\"";
        output_ += version.empty() ? "1.0" : std::string(version);
        output_.push_back('"');
        if (!encoding.empty()) {
            output_ += " encoding=\"";
            output_ += encoding;
            output_.push_back('"');
        }
        if (!standalone.empty()) {
            output_ += " standalone=\"";
            output_ += standalone;
            output_.push_back('"');
        }
        output_ += "?>";
    }
    state_ = WriteState::Prolog;
}

void XmlWriter::WriteDocType(std::string_view name, std::string_view publicId, std::string_view systemId, std::string_view internalSubset) {
    EnsureOpen();
    EnsureNoAttribute();
    CloseStartTagIfNeeded(false);
    if (!output_.empty() && settings_.Indent) {
        output_ += settings_.NewLineChars;
    }
    XmlDocumentType documentType{std::string(name), std::string(publicId), std::string(systemId), std::string(internalSubset)};
    SerializeDocumentType(documentType, output_);
    state_ = WriteState::Prolog;
}

void XmlWriter::WriteStartElement(std::string_view name) {
    EnsureOpen();
    EnsureNoAttribute();
    CloseStartTagIfNeeded(false);
    MarkParentHasChild(false);
    output_.push_back('<');
    output_ += XmlConvert::VerifyName(name);
    elementStack_.push_back({std::string(name), true, false, true});
    state_ = WriteState::Element;
}

void XmlWriter::WriteStartElement(std::string_view prefix, std::string_view localName, std::string_view namespaceUri) {
    WriteStartElement(ComposeQualifiedName(prefix, localName));
    if (!namespaceUri.empty()) {
        WriteAttributeString(prefix.empty() ? "xmlns" : "xmlns:" + std::string(prefix), namespaceUri);
    }
}

void XmlWriter::WriteStartAttribute(std::string_view name) {
    EnsureOpen();
    if (elementStack_.empty() || !elementStack_.back().startTagOpen) {
        throw XmlException("Attributes can only be written immediately after WriteStartElement");
    }
    if (currentAttributeName_.has_value()) {
        throw XmlException("An attribute is already open");
    }

    currentAttributeName_ = XmlConvert::VerifyName(name);
    currentAttributeValue_.clear();
    state_ = WriteState::Attribute;
}

void XmlWriter::WriteStartAttribute(std::string_view prefix, std::string_view localName, std::string_view) {
    WriteStartAttribute(ComposeQualifiedName(prefix, localName));
}

void XmlWriter::WriteEndAttribute() {
    if (!currentAttributeName_.has_value()) {
        throw XmlException("No attribute is open");
    }
    WriteAttributeString(*currentAttributeName_, currentAttributeValue_);
    currentAttributeName_.reset();
    currentAttributeValue_.clear();
    state_ = WriteState::Element;
}

void XmlWriter::WriteAttributeString(std::string_view name, std::string_view value) {
    EnsureOpen();
    if (elementStack_.empty() || !elementStack_.back().startTagOpen) {
        throw XmlException("Attributes can only be written immediately after WriteStartElement");
    }

    output_.push_back(' ');
    output_ += XmlConvert::VerifyName(name);
    output_ += "=\"";
    output_ += EscapeAttribute(value);
    output_.push_back('"');
}

void XmlWriter::WriteAttributeString(
    std::string_view prefix,
    std::string_view localName,
    std::string_view namespaceUri,
    std::string_view value) {
    WriteAttributeString(ComposeQualifiedName(prefix, localName), value);
    if (!namespaceUri.empty() && prefix != "xmlns") {
        WriteAttributeString(prefix.empty() ? "xmlns" : "xmlns:" + std::string(prefix), namespaceUri);
    }
}

void XmlWriter::WriteString(std::string_view text) {
    EnsureOpen();
    if (currentAttributeName_.has_value()) {
        currentAttributeValue_ += text;
        return;
    }

    CloseStartTagIfNeeded(true);
    MarkParentHasChild(true);
    output_ += EscapeText(text);
    state_ = WriteState::Content;
}

void XmlWriter::WriteValue(std::string_view value) {
    WriteString(value);
}

void XmlWriter::WriteValue(bool value) {
    WriteString(XmlConvert::ToString(value));
}

void XmlWriter::WriteValue(int value) {
    WriteString(XmlConvert::ToString(value));
}

void XmlWriter::WriteValue(double value) {
    WriteString(XmlConvert::ToString(value));
}

void XmlWriter::WriteWhitespace(std::string_view whitespace) {
    for (char ch : whitespace) {
        if (!XmlConvert::IsWhitespaceChar(ch)) {
            throw XmlException("WriteWhitespace can only write whitespace characters");
        }
    }
    WriteString(whitespace);
}

void XmlWriter::WriteCData(std::string_view text) {
    EnsureOpen();
    EnsureNoAttribute();
    CloseStartTagIfNeeded(true);
    MarkParentHasChild(true);
    output_ += "<![CDATA[";
    output_ += text;
    output_ += "]]>";
    state_ = WriteState::Content;
}

void XmlWriter::WriteComment(std::string_view text) {
    EnsureOpen();
    EnsureNoAttribute();
    CloseStartTagIfNeeded(false);
    MarkParentHasChild(false);
    output_ += "<!--";
    output_ += text;
    output_ += "-->";
    state_ = WriteState::Content;
}

void XmlWriter::WriteProcessingInstruction(std::string_view name, std::string_view text) {
    EnsureOpen();
    EnsureNoAttribute();
    CloseStartTagIfNeeded(false);
    MarkParentHasChild(false);
    output_ += "<?";
    output_ += XmlConvert::VerifyNCName(name);
    if (!text.empty()) {
        output_.push_back(' ');
        output_ += text;
    }
    output_ += "?>";
    state_ = WriteState::Content;
}

void XmlWriter::WriteRaw(std::string_view xml) {
    EnsureOpen();
    EnsureNoAttribute();
    CloseStartTagIfNeeded(false);
    MarkParentHasChild(false);
    output_ += xml;
    state_ = WriteState::Content;
}

void XmlWriter::WriteElementString(std::string_view name, std::string_view value) {
    WriteStartElement(name);
    WriteString(value);
    WriteEndElement();
}

void XmlWriter::WriteElementString(std::string_view prefix, std::string_view localName, std::string_view namespaceUri, std::string_view value) {
    WriteStartElement(prefix, localName, namespaceUri);
    WriteString(value);
    WriteEndElement();
}

void XmlWriter::WriteNode(const XmlNode& node) {
    EnsureOpen();
    EnsureNoAttribute();
    const bool textLike = IsTextLike(node.NodeType());
    CloseStartTagIfNeeded(textLike);
    MarkParentHasChild(textLike);
    output_ += WriteToString(node, settings_);
    state_ = WriteState::Content;
}

void XmlWriter::WriteNode(XmlReader& reader, bool) {
    if (reader.NodeType() == XmlNodeType::None && !reader.Read()) {
        return;
    }
    WriteRaw(reader.ReadOuterXml());
}

void XmlWriter::WriteEndElement() {
    EnsureOpen();
    EnsureNoAttribute();
    if (elementStack_.empty()) {
        throw XmlException("No element is open");
    }

    auto state = elementStack_.back();
    elementStack_.pop_back();
    if (state.startTagOpen) {
        output_ += "/>";
    } else {
        if (settings_.Indent && !state.textOnly && state.hasChild) {
            output_ += settings_.NewLineChars;
            WriteIndent(static_cast<int>(elementStack_.size()));
        }
        output_ += "</";
        output_ += state.name;
        output_.push_back('>');
    }

    state_ = elementStack_.empty() ? WriteState::Content : WriteState::Element;
}

void XmlWriter::WriteFullEndElement() {
    EnsureOpen();
    EnsureNoAttribute();
    if (elementStack_.empty()) {
        throw XmlException("No element is open");
    }
    CloseStartTagIfNeeded(false);
    elementStack_.back().hasChild = true;
    WriteEndElement();
}

void XmlWriter::WriteEndDocument() {
    while (!elementStack_.empty()) {
        WriteEndElement();
    }
}

void XmlWriter::Flush() const {
    if (stream_ != nullptr) {
        *stream_ << output_;
        stream_->flush();
    }
}

void XmlWriter::Close() {
    WriteEndDocument();
    Flush();
    state_ = WriteState::Closed;
}

std::string XmlWriter::GetString() const {
    return output_;
}

void XmlWriter::Save(std::ostream& stream) const {
    stream << output_;
}

void XmlWriter::Save(const std::string& path) const {
    std::ofstream stream(path, std::ios::binary);
    if (!stream) {
        throw XmlException("Could not open XML file '" + path + "' for writing");
    }
    Save(stream);
}

std::string XmlWriter::WriteToString(const XmlNode& node, const XmlWriterSettings& settings) {
    std::string output;
    SerializeNode(node, settings, output, 0);
    return output;
}

void XmlWriter::WriteToStream(const XmlNode& node, std::ostream& stream, const XmlWriterSettings& settings) {
    stream << WriteToString(node, settings);
}

void XmlWriter::WriteToFile(const XmlNode& node, const std::string& path, const XmlWriterSettings& settings) {
    std::ofstream stream(path, std::ios::binary);
    if (!stream) {
        throw XmlException("Could not open XML file '" + path + "' for writing");
    }
    WriteToStream(node, stream, settings);
}

void XmlWriter::EnsureOpen() const {
    if (state_ == WriteState::Closed) {
        throw XmlException("The XmlWriter is closed");
    }
}

void XmlWriter::EnsureNoAttribute() const {
    if (currentAttributeName_.has_value()) {
        throw XmlException("Close the current attribute before writing another node");
    }
}

void XmlWriter::CloseStartTagIfNeeded(bool childIsText) {
    if (elementStack_.empty() || !elementStack_.back().startTagOpen) {
        return;
    }

    output_.push_back('>');
    elementStack_.back().startTagOpen = false;
    if (!childIsText) {
        elementStack_.back().textOnly = false;
    }
}

void XmlWriter::WriteIndent(int depth) {
    AppendIndent(output_, depth, settings_);
}

void XmlWriter::MarkParentHasChild(bool childIsText) {
    if (elementStack_.empty()) {
        return;
    }

    auto& parent = elementStack_.back();
    if (settings_.Indent && !childIsText && !parent.startTagOpen) {
        output_ += settings_.NewLineChars;
        WriteIndent(static_cast<int>(elementStack_.size()));
    }
    parent.hasChild = true;
    if (!childIsText) {
        parent.textOnly = false;
    }
}

}  // namespace System::Xml
