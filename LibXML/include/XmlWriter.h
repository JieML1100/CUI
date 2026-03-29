#pragma once

#include "XmlDocument.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace System::Xml {

class XmlReader;

class XmlWriter final {
public:
    explicit XmlWriter(XmlWriterSettings settings = {});
    XmlWriter(std::ostream& stream, XmlWriterSettings settings = {});

    WriteState GetWriteState() const noexcept;
    std::string LookupPrefix(std::string_view namespaceUri) const;

    void WriteStartDocument(
        std::string_view version = "1.0",
        std::string_view encoding = {},
        std::string_view standalone = {});
    void WriteDocType(
        std::string_view name,
        std::string_view publicId = {},
        std::string_view systemId = {},
        std::string_view internalSubset = {});
    void WriteStartElement(std::string_view name);
    void WriteStartElement(std::string_view prefix, std::string_view localName, std::string_view namespaceUri = {});
    void WriteStartAttribute(std::string_view name);
    void WriteStartAttribute(std::string_view prefix, std::string_view localName, std::string_view namespaceUri = {});
    void WriteEndAttribute();
    void WriteAttributeString(std::string_view name, std::string_view value);
    void WriteAttributeString(
        std::string_view prefix,
        std::string_view localName,
        std::string_view namespaceUri,
        std::string_view value);
    // Legacy aliases (identical to WriteAttributeString with string_view)
    void WriteAttributeStringView(std::string_view name, std::string_view value) { WriteAttributeString(name, value); }
    void WriteAttributeStringView(
        std::string_view prefix,
        std::string_view localName,
        std::string_view namespaceUri,
        std::string_view value) { WriteAttributeString(prefix, localName, namespaceUri, value); }
    void WriteAttributes(const XmlAttributeCollection& attributes);
    void WriteAttributes(const XmlElement& element);
    void WriteCharEntity(unsigned int codePoint);
    void WriteEntityRef(std::string_view name);
    void WriteString(std::string_view text);
    void WriteValue(std::string_view value);
    void WriteValue(bool value);
    void WriteValue(int value);
    void WriteValue(double value);
    void WriteName(std::string_view name);
    void WriteQualifiedName(std::string_view localName, std::string_view namespaceUri = {});
    void WriteWhitespace(std::string_view whitespace);
    void WriteSignificantWhitespace(std::string_view whitespace);
    void WriteCData(std::string_view text);
    void WriteComment(std::string_view text);
    void WriteProcessingInstruction(std::string_view name, std::string_view text);
    void WriteRaw(std::string_view xml);
    void WriteBase64(const unsigned char* data, std::size_t length);
    void WriteElementString(std::string_view name, std::string_view value);
    void WriteElementString(std::string_view prefix, std::string_view localName, std::string_view namespaceUri, std::string_view value);
    void WriteNode(const XmlNode& node);
    void WriteNode(XmlReader& reader, bool defattr = true);
    void WriteEndElement();
    void WriteFullEndElement();
    void WriteEndDocument();
    void Flush() const;
    void Close();
    std::string GetString() const;
    void Save(std::ostream& stream) const;
    void Save(const std::string& path) const;

    static std::string WriteToString(const XmlNode& node, const XmlWriterSettings& settings = {});
    static void WriteToStream(const XmlNode& node, std::ostream& stream, const XmlWriterSettings& settings = {});
    static void WriteToFile(const XmlNode& node, const std::string& path, const XmlWriterSettings& settings = {});

private:
    struct DirectElementState {
        std::string name;
        bool startTagOpen = true;
        bool hasAnyChild = false;
        bool hasTextLikeChild = false;
        bool hasNonTextChild = false;
        bool forceFullEnd = false;
        std::unique_ptr<std::unordered_map<std::string, std::string>> namespaceDeclarations;
    };

    struct InMemoryMutationCheckpoint {
        std::vector<std::shared_ptr<XmlElement>> elementStack;
        std::vector<bool> startTagOpenStack;
        std::shared_ptr<XmlElement> targetParent;
        std::size_t parentChildCount = 0;
        std::size_t documentChildCount = 0;
        std::size_t fragmentChildCount = 0;
        bool startDocumentWritten = false;
        bool hadFragmentRoot = false;
    };

    void EnsureDocumentOpen(std::string_view operation) const;
    void EnsureOpenElement(std::string_view operation) const;
    void EnsureOpenStartElement(std::string_view operation) const;
    void EnsureNoOpenAttribute(std::string_view operation) const;
    void EnsureOutputReady() const;
    void MarkCurrentElementContentStarted();
    std::string LookupNamespacePrefix(std::string_view namespaceUri) const;
    std::shared_ptr<XmlDocumentFragment> EnsureFragmentRoot() const;
    void AppendDocumentLevelNode(const std::shared_ptr<XmlNode>& node);
    bool UsesDirectOutput() const noexcept;
    void WriteDirectTopLevelSeparatorIfNeeded();
    void EnsureDirectCurrentStartTagClosed();
    void PrepareDirectParentForChild(bool textLikeChild);
    void WriteDirectAttribute(std::string_view name, std::string_view value);
    bool HasDirectNamespaceBinding(std::string_view prefix, std::string_view namespaceUri) const;
    std::string LookupDirectNamespacePrefix(std::string_view namespaceUri) const;
    void DeclareDirectNamespaceIfNeeded(std::string_view prefix, std::string_view namespaceUri);
    void WriteDirectTopLevelNode(const XmlNode& node);
    void WriteNodeInMemory(const XmlNode& node);
    InMemoryMutationCheckpoint CaptureInMemoryMutationCheckpoint() const;
    void RollbackInMemoryMutation(const InMemoryMutationCheckpoint& checkpoint);

    XmlDocument document_;
    mutable std::shared_ptr<XmlDocumentFragment> fragmentRoot_;
    XmlWriterSettings settings_;
    std::vector<std::shared_ptr<XmlElement>> elementStack_;
    std::vector<bool> startTagOpenStack_;
    std::ostream* directOutputStream_ = nullptr;
    std::vector<DirectElementState> directElementStack_;
    std::optional<std::string> currentAttributeName_;
    std::string currentAttributeValue_;
    bool startDocumentWritten_ = false;
    bool documentClosed_ = false;
    bool directHasRootElement_ = false;
    bool directHasDocumentType_ = false;
    bool directHasTopLevelContent_ = false;
};

}  // namespace System::Xml
