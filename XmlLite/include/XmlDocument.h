#pragma once

#include "XmlNode.h"

#include <iosfwd>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace System::Xml {

class XmlDocument final : public XmlNode {
public:
    XmlDocument();
    ~XmlDocument() override = default;

    static std::shared_ptr<XmlDocument> Parse(std::string_view xml);
    static std::shared_ptr<XmlDocument> Parse(std::string_view xml, const XmlReaderSettings& settings);

    void LoadXml(std::string_view xml);
    void LoadXml(std::string_view xml, const XmlReaderSettings& settings);
    void Load(const std::string& path);
    void Load(const std::string& path, const XmlReaderSettings& settings);
    void Load(std::istream& stream);
    void Load(std::istream& stream, const XmlReaderSettings& settings);
    void Save(const std::string& path, const XmlWriterSettings& settings = {}) const;
    void Save(std::ostream& stream, const XmlWriterSettings& settings = {}) const;

    bool PreserveWhitespace() const noexcept;
    void SetPreserveWhitespace(bool value) noexcept;

    std::string ToString(const XmlWriterSettings& settings = {}) const;
    void RemoveAll() override;

    std::shared_ptr<XmlDeclaration> Declaration() const;
    std::shared_ptr<XmlDocumentType> DocumentType() const;
    std::shared_ptr<XmlElement> DocumentElement() const;
    std::vector<std::shared_ptr<XmlElement>> GetElementsByTagName(std::string_view name) const;
    XmlNodeList GetElementsByTagNameList(std::string_view name) const;
    std::vector<std::shared_ptr<XmlElement>> GetElementsByTagName(std::string_view localName, std::string_view namespaceUri) const;
    XmlNodeList GetElementsByTagNameList(std::string_view localName, std::string_view namespaceUri) const;

    std::shared_ptr<XmlDocumentFragment> CreateDocumentFragment() const;
    std::shared_ptr<XmlElement> CreateElement(std::string_view name) const;
    std::shared_ptr<XmlElement> CreateElement(std::string_view prefix, std::string_view localName, std::string_view namespaceUri = {}) const;
    std::shared_ptr<XmlAttribute> CreateAttribute(std::string_view name, std::string_view value = {}) const;
    std::shared_ptr<XmlAttribute> CreateAttribute(std::string_view prefix, std::string_view localName, std::string_view namespaceUri, std::string_view value = {}) const;
    std::shared_ptr<XmlText> CreateTextNode(std::string_view value) const;
    std::shared_ptr<XmlEntityReference> CreateEntityReference(std::string_view name) const;
    std::shared_ptr<XmlWhitespace> CreateWhitespace(std::string_view value) const;
    std::shared_ptr<XmlSignificantWhitespace> CreateSignificantWhitespace(std::string_view value) const;
    std::shared_ptr<XmlCDataSection> CreateCDataSection(std::string_view value) const;
    std::shared_ptr<XmlComment> CreateComment(std::string_view value) const;
    std::shared_ptr<XmlProcessingInstruction> CreateProcessingInstruction(std::string_view target, std::string_view data = {}) const;
    std::shared_ptr<XmlDeclaration> CreateXmlDeclaration(
        std::string_view version = "1.0",
        std::string_view encoding = {},
        std::string_view standalone = {}) const;
    std::shared_ptr<XmlDocumentType> CreateDocumentType(
        std::string_view name,
        std::string_view publicId = {},
        std::string_view systemId = {},
        std::string_view internalSubset = {}) const;
    std::shared_ptr<XmlNode> CreateNode(
        XmlNodeType nodeType,
        std::string_view name = {},
        std::string_view value = {}) const;
    std::shared_ptr<XmlNode> ImportNode(const XmlNode& node, bool deep) const;

    std::shared_ptr<XmlNode> AppendChild(const std::shared_ptr<XmlNode>& child) override;

protected:
    void ValidateChildInsertion(const std::shared_ptr<XmlNode>& child, const XmlNode* replacingChild = nullptr) const override;

private:
    bool preserveWhitespace_ = false;
};

}  // namespace System::Xml
