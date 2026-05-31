#pragma once

#include "XmlTypes.h"

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace System::Xml {

class XmlDocument;
class XmlAttribute;
class XmlElement;
class XmlReader;

class XmlNode;

class XmlNodeList final {
public:
    XmlNodeList() = default;
    explicit XmlNodeList(std::vector<std::shared_ptr<XmlNode>> nodes);

    std::size_t Count() const noexcept;
    std::shared_ptr<XmlNode> Item(std::size_t index) const;
    bool Empty() const noexcept;

    std::vector<std::shared_ptr<XmlNode>>::const_iterator begin() const noexcept;
    std::vector<std::shared_ptr<XmlNode>>::const_iterator end() const noexcept;

private:
    std::vector<std::shared_ptr<XmlNode>> nodes_;
};

class XmlAttributeCollection final {
public:
    XmlAttributeCollection() = default;
    explicit XmlAttributeCollection(std::vector<std::shared_ptr<XmlAttribute>> attributes);

    std::size_t Count() const noexcept;
    std::shared_ptr<XmlAttribute> Item(std::size_t index) const;
    std::shared_ptr<XmlAttribute> Item(std::string_view name) const;
    bool Empty() const noexcept;

    std::vector<std::shared_ptr<XmlAttribute>>::const_iterator begin() const noexcept;
    std::vector<std::shared_ptr<XmlAttribute>>::const_iterator end() const noexcept;

private:
    std::vector<std::shared_ptr<XmlAttribute>> attributes_;
};

class XmlNode {
public:
    virtual ~XmlNode() = default;

    XmlNodeType NodeType() const noexcept;
    const std::string& Name() const noexcept;
    std::string LocalName() const;
    std::string Prefix() const;
    std::string NamespaceURI() const;
    virtual const std::string& Value() const noexcept;
    virtual void SetValue(std::string_view value);

    XmlNode* ParentNode() noexcept;
    const XmlNode* ParentNode() const noexcept;
    XmlDocument* OwnerDocument() noexcept;
    const XmlDocument* OwnerDocument() const noexcept;

    const std::vector<std::shared_ptr<XmlNode>>& ChildNodes() const noexcept;
    XmlNodeList ChildNodeList() const;
    std::shared_ptr<XmlNode> FirstChild() const;
    std::shared_ptr<XmlNode> LastChild() const;
    std::shared_ptr<XmlNode> PreviousSibling() const;
    std::shared_ptr<XmlNode> NextSibling() const;
    std::shared_ptr<XmlNode> SharedFromParent() const;
    bool HasChildNodes() const noexcept;

    virtual std::shared_ptr<XmlNode> AppendChild(const std::shared_ptr<XmlNode>& child);
    virtual std::shared_ptr<XmlNode> InsertBefore(
        const std::shared_ptr<XmlNode>& newChild,
        const std::shared_ptr<XmlNode>& referenceChild);
    virtual std::shared_ptr<XmlNode> InsertAfter(
        const std::shared_ptr<XmlNode>& newChild,
        const std::shared_ptr<XmlNode>& referenceChild);
    virtual std::shared_ptr<XmlNode> ReplaceChild(
        const std::shared_ptr<XmlNode>& newChild,
        const std::shared_ptr<XmlNode>& oldChild);
    virtual std::shared_ptr<XmlNode> RemoveChild(const std::shared_ptr<XmlNode>& child);
    void RemoveAllChildren();
    virtual void RemoveAll();

    std::string InnerText() const;
    virtual void SetInnerText(std::string_view text);
    std::string InnerXml(const XmlWriterSettings& settings = {}) const;
    virtual void SetInnerXml(std::string_view xml);
    std::string OuterXml(const XmlWriterSettings& settings = {}) const;
    std::shared_ptr<XmlNode> CloneNode(bool deep) const;

protected:
    XmlNode(XmlNodeType nodeType, std::string name = {}, std::string value = {});

    virtual void ValidateChildInsertion(const std::shared_ptr<XmlNode>& child, const XmlNode* replacingChild = nullptr) const;
    void SetOwnerDocument(XmlDocument* ownerDocument) noexcept;
    void SetOwnerDocumentRecursive(XmlDocument* ownerDocument);
    void SetParent(XmlNode* parent) noexcept;
    std::vector<std::shared_ptr<XmlNode>>& MutableChildNodes() noexcept;

private:
    std::size_t FindChildIndexOrThrow(const std::shared_ptr<XmlNode>& child) const;
    void AttachChildAt(const std::shared_ptr<XmlNode>& child, std::size_t index);

    XmlNodeType nodeType_;
    std::string name_;
    std::string value_;
    XmlNode* parent_ = nullptr;
    XmlDocument* ownerDocument_ = nullptr;
    std::vector<std::shared_ptr<XmlNode>> childNodes_;

    friend class XmlDocument;
    friend class XmlElement;
    friend class XmlAttribute;
};

class XmlAttribute final : public XmlNode {
public:
    XmlAttribute(std::string name, std::string value = {});

    XmlElement* OwnerElement() noexcept;
    const XmlElement* OwnerElement() const noexcept;

private:
    void SetOwnerElement(XmlElement* ownerElement) noexcept;

    XmlElement* ownerElement_ = nullptr;

    friend class XmlElement;
};

class XmlCharacterData : public XmlNode {
public:
    const std::string& Data() const noexcept;
    void SetData(std::string_view data);
    std::size_t Length() const noexcept;
    void AppendData(std::string_view strData);
    void DeleteData(std::size_t offset, std::size_t count);
    void InsertData(std::size_t offset, std::string_view strData);
    void ReplaceData(std::size_t offset, std::size_t count, std::string_view strData);
    std::string Substring(std::size_t offset, std::size_t count) const;

protected:
    XmlCharacterData(XmlNodeType nodeType, std::string name, std::string value = {});
};

class XmlText final : public XmlCharacterData {
public:
    explicit XmlText(std::string text);
};

class XmlEntityReference final : public XmlNode {
public:
    explicit XmlEntityReference(std::string name);
};

class XmlWhitespace final : public XmlCharacterData {
public:
    explicit XmlWhitespace(std::string whitespace);
    void SetValue(std::string_view value) override;
};

class XmlSignificantWhitespace final : public XmlCharacterData {
public:
    explicit XmlSignificantWhitespace(std::string whitespace);
    void SetValue(std::string_view value) override;
};

class XmlCDataSection final : public XmlCharacterData {
public:
    explicit XmlCDataSection(std::string data);
};

class XmlComment final : public XmlCharacterData {
public:
    explicit XmlComment(std::string comment);
};

class XmlProcessingInstruction final : public XmlNode {
public:
    XmlProcessingInstruction(std::string target, std::string data = {});

    const std::string& Target() const noexcept;
    const std::string& Data() const noexcept;
    void SetData(std::string_view data);
};

class XmlDeclaration final : public XmlNode {
public:
    XmlDeclaration(std::string version = "1.0", std::string encoding = {}, std::string standalone = {});

    const std::string& Version() const noexcept;
    const std::string& Encoding() const noexcept;
    const std::string& Standalone() const noexcept;

private:
    std::string version_;
    std::string encoding_;
    std::string standalone_;
};

class XmlDocumentType final : public XmlNode {
public:
    XmlDocumentType(
        std::string name,
        std::string publicId = {},
        std::string systemId = {},
        std::string internalSubset = {});

    const std::string& PublicId() const noexcept;
    const std::string& SystemId() const noexcept;
    const std::string& InternalSubset() const noexcept;

private:
    std::string publicId_;
    std::string systemId_;
    std::string internalSubset_;
};

class XmlDocumentFragment final : public XmlNode {
public:
    XmlDocumentFragment();

    void SetInnerXml(std::string_view xml) override;
};

class XmlElement final : public XmlNode {
public:
    explicit XmlElement(std::string name);

    const std::vector<std::shared_ptr<XmlAttribute>>& Attributes() const noexcept;
    XmlAttributeCollection AttributeNodes() const;
    bool HasAttributes() const noexcept;
    bool HasAttribute(std::string_view name) const;
    bool HasAttribute(std::string_view localName, std::string_view namespaceUri) const;
    std::string GetAttribute(std::string_view name) const;
    std::string GetAttribute(std::string_view localName, std::string_view namespaceUri) const;
    std::shared_ptr<XmlAttribute> GetAttributeNode(std::string_view name) const;
    std::shared_ptr<XmlAttribute> GetAttributeNode(std::string_view localName, std::string_view namespaceUri) const;
    std::shared_ptr<XmlAttribute> SetAttribute(std::string_view name, std::string_view value);
    void SetAttribute(std::string_view localName, std::string_view namespaceUri, std::string_view value);
    std::shared_ptr<XmlAttribute> SetAttributeNode(const std::shared_ptr<XmlAttribute>& attribute);
    bool RemoveAttribute(std::string_view name);
    bool RemoveAttribute(std::string_view localName, std::string_view namespaceUri);
    std::shared_ptr<XmlAttribute> RemoveAttributeNode(const std::shared_ptr<XmlAttribute>& attribute);
    void RemoveAllAttributes();
    void SetInnerXml(std::string_view xml) override;
    bool IsEmpty() const noexcept;
    std::vector<std::shared_ptr<XmlElement>> GetElementsByTagName(std::string_view name) const;
    XmlNodeList GetElementsByTagNameList(std::string_view name) const;
    std::vector<std::shared_ptr<XmlElement>> GetElementsByTagName(std::string_view localName, std::string_view namespaceUri) const;
    XmlNodeList GetElementsByTagNameList(std::string_view localName, std::string_view namespaceUri) const;
    bool WritesFullEndElement() const noexcept;
    void SetWritesFullEndElement(bool value) noexcept;
    std::string FindNamespaceDeclarationValue(std::string_view prefix) const;
    std::string FindNamespaceDeclarationPrefix(std::string_view namespaceUri) const;

private:
    std::size_t FindAttributeIndex(std::string_view name) const;
    std::size_t FindAttributeIndex(std::string_view localName, std::string_view namespaceUri) const;

    std::vector<std::shared_ptr<XmlAttribute>> attributes_;
    bool writeFullEndElement_ = false;

    friend class XmlDocument;
    friend class XmlReader;
};

}  // namespace System::Xml
