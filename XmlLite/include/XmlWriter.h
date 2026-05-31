#pragma once

#include "XmlDocument.h"

#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace System::Xml {

class XmlReader;

class XmlWriter final {
public:
    explicit XmlWriter(XmlWriterSettings settings = {});
    XmlWriter(std::ostream& stream, XmlWriterSettings settings = {});

    WriteState GetWriteState() const noexcept;

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
    void WriteString(std::string_view text);
    void WriteValue(std::string_view value);
    void WriteValue(bool value);
    void WriteValue(int value);
    void WriteValue(double value);
    void WriteWhitespace(std::string_view whitespace);
    void WriteCData(std::string_view text);
    void WriteComment(std::string_view text);
    void WriteProcessingInstruction(std::string_view name, std::string_view text);
    void WriteRaw(std::string_view xml);
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
    struct ElementState {
        std::string name;
        bool startTagOpen = true;
        bool hasChild = false;
        bool textOnly = true;
    };

    void EnsureOpen() const;
    void EnsureNoAttribute() const;
    void CloseStartTagIfNeeded(bool childIsText);
    void WriteIndent(int depth);
    void MarkParentHasChild(bool childIsText);

    XmlWriterSettings settings_;
    std::ostream* stream_ = nullptr;
    std::string output_;
    std::vector<ElementState> elementStack_;
    std::optional<std::string> currentAttributeName_;
    std::string currentAttributeValue_;
    WriteState state_ = WriteState::Start;
};

}  // namespace System::Xml
