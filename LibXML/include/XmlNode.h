#pragma once

#include "XmlTypes.h"

#include <cstddef>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace System::Xml {

class XmlDocument;
class XmlAttribute;
class XmlDeclaration;
class XmlDocumentType;
class XmlDocumentFragment;
class XmlEntity;
class XmlElement;
class XmlNamespaceManager;
class XmlNameTable;
class XmlNodeReader;
class XmlNotation;
class XmlParser;
class XmlReader;
class XmlWriter;
class XPathNavigator;
class XPathDocument;
class XPathValue;
class XPathExpression;

void LoadXmlDocumentFromReader(XmlReader& reader, XmlDocument& document);
std::string LookupNamespaceUriOnElement(const XmlElement* element, std::string_view prefix);
bool LookupNamespaceUriOnElementView(const XmlElement* element, std::string_view prefix, std::string_view& namespaceUri) noexcept;

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

class XmlNamedNodeMap final {
public:
    XmlNamedNodeMap() = default;
    explicit XmlNamedNodeMap(std::vector<std::shared_ptr<XmlNode>> nodes);

    std::size_t Count() const noexcept;
    std::shared_ptr<XmlNode> Item(std::size_t index) const;
    std::shared_ptr<XmlNode> GetNamedItem(std::string_view name) const;
    bool Empty() const noexcept;

    std::vector<std::shared_ptr<XmlNode>>::const_iterator begin() const noexcept;
    std::vector<std::shared_ptr<XmlNode>>::const_iterator end() const noexcept;

private:
    std::vector<std::shared_ptr<XmlNode>> nodes_;
};

class XmlNode {
public:
    virtual ~XmlNode() = default;

    XmlNodeType NodeType() const noexcept;
    const std::string& Name() const noexcept;
    std::string LocalName() const;
    std::string Prefix() const;
    std::string NamespaceURI() const;
    std::string GetNamespaceOfPrefix(std::string_view prefix) const;
    std::string GetPrefixOfNamespace(std::string_view namespaceUri) const;
    virtual const std::string& Value() const noexcept;
    virtual void SetValue(std::string_view value);

    virtual void Normalize();

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
    XPathNavigator CreateNavigator() const;

    std::shared_ptr<XmlNode> SelectSingleNode(std::string_view xpath) const;
    std::shared_ptr<XmlNode> SelectSingleNode(std::string_view xpath, const XmlNamespaceManager& namespaces) const;
    XmlNodeList SelectNodes(std::string_view xpath) const;
    XmlNodeList SelectNodes(std::string_view xpath, const XmlNamespaceManager& namespaces) const;
    XPathValue Evaluate(std::string_view xpath) const;
    XPathValue Evaluate(std::string_view xpath, const XmlNamespaceManager& namespaces) const;
    XPathValue Evaluate(const XPathExpression& expression) const;
    XPathValue Evaluate(const XPathExpression& expression, const XmlNamespaceManager& namespaces) const;

protected:
    XmlNode(XmlNodeType nodeType, std::string name = {}, std::string value = {});

    virtual void ValidateChildInsertion(const std::shared_ptr<XmlNode>& child, const XmlNode* replacingChild = nullptr) const;
    void SetOwnerDocument(XmlDocument* ownerDocument) noexcept;
    void SetOwnerDocumentRecursive(XmlDocument* ownerDocument);
    void SetParent(XmlNode* parent) noexcept;
    std::vector<std::shared_ptr<XmlNode>>& MutableChildNodes() noexcept;
    std::shared_ptr<XmlNode> AppendChildForLoad(const std::shared_ptr<XmlNode>& child);
    std::shared_ptr<XmlNode> AppendChildForOwnedLoad(const std::shared_ptr<XmlNode>& child);

private:
    static constexpr std::size_t DetachedSiblingIndex = (std::numeric_limits<std::size_t>::max)();

    std::size_t FindChildIndexOrThrow(const std::shared_ptr<XmlNode>& child) const;
    void SetSiblingIndex(std::size_t siblingIndex) noexcept;
    std::size_t ResolveSiblingIndex() const noexcept;
    void ReindexChildNodesFrom(std::size_t startIndex) noexcept;

    XmlNodeType nodeType_;
    std::string name_;
    std::string value_;
    XmlNode* parent_;
    XmlDocument* ownerDocument_;
    std::size_t siblingIndex_;
    std::unique_ptr<std::vector<std::shared_ptr<XmlNode>>> childNodes_;

    friend class XmlDocument;
    friend class XmlElement;
    friend class XmlParser;
    friend class XmlWriter;
    friend void LoadXmlDocumentFromReader(XmlReader& reader, XmlDocument& document);
};

class XmlAttribute final : public XmlNode {
public:
    XmlAttribute(std::string name, std::string value = {});

    XmlElement* OwnerElement() noexcept;
    const XmlElement* OwnerElement() const noexcept;
    std::shared_ptr<XmlAttribute> SharedFromOwnerElement() const;

private:
    static constexpr std::size_t DetachedAttributeIndex = (std::numeric_limits<std::size_t>::max)();

    void SetAttributeIndex(std::size_t attributeIndex) noexcept;
    void ResetAttributeIndex() noexcept;
    std::size_t ResolveAttributeIndex() const noexcept;

    std::size_t attributeIndex_ = DetachedAttributeIndex;

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

    std::shared_ptr<XmlText> SplitText(std::size_t offset);
};

class XmlEntityReference final : public XmlNode {
public:
    explicit XmlEntityReference(std::string name, std::string resolvedValue = {});
};

class XmlEntity final : public XmlNode {
public:
    XmlEntity(
        std::string name,
        std::string replacementText = {},
        std::string publicId = {},
        std::string systemId = {},
        std::string notationName = {});

    const std::string& PublicId() const noexcept;
    const std::string& SystemId() const noexcept;
    const std::string& NotationName() const noexcept;

private:
    std::string publicId_;
    std::string systemId_;
    std::string notationName_;
};

class XmlNotation final : public XmlNode {
public:
    XmlNotation(std::string name, std::string publicId = {}, std::string systemId = {});

    const std::string& PublicId() const noexcept;
    const std::string& SystemId() const noexcept;

private:
    std::string publicId_;
    std::string systemId_;
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
    XmlDocumentType(
        std::string name,
        std::string publicId,
        std::string systemId,
        std::string internalSubset,
        std::vector<std::shared_ptr<XmlNode>> entities,
        std::vector<std::shared_ptr<XmlNode>> notations);

    const std::string& PublicId() const noexcept;
    const std::string& SystemId() const noexcept;
    const std::string& InternalSubset() const noexcept;
    XmlNamedNodeMap Entities() const;
    XmlNamedNodeMap Notations() const;

private:
    std::string publicId_;
    std::string systemId_;
    std::string internalSubset_;
    std::vector<std::shared_ptr<XmlNode>> entities_;
    std::vector<std::shared_ptr<XmlNode>> notations_;

    friend class XmlNode;
};

class XmlDocumentFragment final : public XmlNode {
public:
    XmlDocumentFragment();

    void SetInnerXml(std::string_view xml);
};

class XmlElement final : public XmlNode {
public:
    explicit XmlElement(std::string name);

    const std::vector<std::shared_ptr<XmlAttribute>>& Attributes() const;
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
    void SetInnerXml(std::string_view xml);
    bool IsEmpty() const noexcept;
    std::vector<std::shared_ptr<XmlElement>> GetElementsByTagName(std::string_view name) const;
    XmlNodeList GetElementsByTagNameList(std::string_view name) const;
    std::vector<std::shared_ptr<XmlElement>> GetElementsByTagName(std::string_view localName, std::string_view namespaceUri) const;
    XmlNodeList GetElementsByTagNameList(std::string_view localName, std::string_view namespaceUri) const;
    bool WritesFullEndElement() const noexcept;
    XmlNodeReader CreateReader() const;
    std::string FindNamespaceDeclarationValue(std::string_view prefix) const;
    bool HasNamespaceDeclaration(std::string_view prefix) const;
    std::string FindNamespaceDeclarationPrefix(std::string_view namespaceUri) const;

private:
    struct TransparentStringHash {
        using is_transparent = void;

        std::size_t operator()(std::string_view value) const noexcept {
            return std::hash<std::string_view>{}(value);
        }

        std::size_t operator()(const std::string& value) const noexcept {
            return std::hash<std::string_view>{}(value);
        }
    };

    struct TransparentStringEqual {
        using is_transparent = void;

        bool operator()(std::string_view left, std::string_view right) const noexcept {
            return left == right;
        }

        bool operator()(const std::string& left, std::string_view right) const noexcept {
            return std::string_view(left) == right;
        }

        bool operator()(std::string_view left, const std::string& right) const noexcept {
            return left == std::string_view(right);
        }

        bool operator()(const std::string& left, const std::string& right) const noexcept {
            return left == right;
        }
    };

    using AttributeNameIndex = std::unordered_map<std::string_view, std::size_t, TransparentStringHash, TransparentStringEqual>;
    using AttributeLocalNameIndex = std::unordered_map<std::string_view, std::vector<std::size_t>, TransparentStringHash, TransparentStringEqual>;
    using PendingAttributeNameIndex = std::unordered_map<std::string_view, std::size_t, TransparentStringHash, TransparentStringEqual>;
    using PendingAttributeLocalNameIndex = std::unordered_map<std::string_view, std::vector<std::size_t>, TransparentStringHash, TransparentStringEqual>;

    struct PendingLoadAttribute {
        std::size_t nameOffset = 0;
        std::size_t nameLength = 0;
        std::size_t valueOffset = 0;
        std::size_t valueLength = 0;
    };

    bool TryFindNamespaceDeclarationValueView(std::string_view prefix, std::string_view& value) const noexcept;
    bool TryFindNamespaceDeclarationPrefixView(std::string_view namespaceUri, std::string_view& prefix) const noexcept;
    std::string_view PendingLoadAttributeNameView(const PendingLoadAttribute& attribute) const noexcept;
    std::string_view PendingLoadAttributeValueView(const PendingLoadAttribute& attribute) const noexcept;
    void MarkAttributeIndexesDirty() const noexcept;
    void EnsureAttributeIndexes() const;
    std::size_t FindIndexedAttributeIndex(std::string_view name) const noexcept;
    std::size_t FindIndexedPendingLoadAttributeIndex(std::string_view name) const noexcept;
    const std::vector<std::size_t>* FindIndexedAttributeLocalNameMatches(std::string_view localName) const noexcept;
    const std::vector<std::size_t>* FindIndexedPendingLoadAttributeLocalNameMatches(std::string_view localName) const noexcept;
    std::size_t FindAttributeIndex(std::string_view localName, std::string_view namespaceUri) const;
    std::shared_ptr<XmlAttribute> MaterializePendingLoadAttribute(std::size_t index) const;
    std::size_t FindPendingLoadAttributeIndex(std::string_view name) const noexcept;
    std::size_t FindPendingLoadAttributeIndex(std::string_view localName, std::string_view namespaceUri) const;
    void EnsureAttributesMaterialized() const;
    void ReserveAttributesForLoad(std::size_t count, std::size_t totalStorageBytes = 0);
    void AppendAttributeForLoad(std::string name, std::string value);
    void ReindexAttributesFrom(std::size_t startIndex) noexcept;
    mutable std::vector<std::shared_ptr<XmlAttribute>> attributes_;
    mutable std::vector<PendingLoadAttribute> pendingLoadAttributes_;
    mutable std::unique_ptr<AttributeNameIndex> attributeNameIndex_;
    mutable std::unique_ptr<AttributeLocalNameIndex> attributeLocalNameIndex_;
    mutable std::unique_ptr<PendingAttributeNameIndex> pendingLoadAttributeNameIndex_;
    mutable std::unique_ptr<PendingAttributeLocalNameIndex> pendingLoadAttributeLocalNameIndex_;
    mutable bool attributeNameIndexesDirty_ = true;
    mutable std::string pendingLoadAttributeStorage_;
    bool writeFullEndElement_ = false;

    friend void LoadXmlDocumentFromReader(XmlReader& reader, XmlDocument& document);
    friend std::string LookupNamespaceUriOnElement(const XmlElement* element, std::string_view prefix);
    friend bool LookupNamespaceUriOnElementView(const XmlElement* element, std::string_view prefix, std::string_view& namespaceUri) noexcept;
    friend bool TryGetAttributeValueViewInternal(const XmlElement& element, std::string_view name, std::string_view& value) noexcept;
    friend bool AttributeValueEqualsInternal(const XmlElement& element, std::string_view name, std::string_view expectedValue) noexcept;
    friend class XmlParser;
    friend class XmlReader;
    friend class XmlWriter;
    friend class XmlNode;
};

bool TryGetAttributeValueViewInternal(const XmlElement& element, std::string_view name, std::string_view& value) noexcept;
bool AttributeValueEqualsInternal(const XmlElement& element, std::string_view name, std::string_view expectedValue) noexcept;

}  // namespace System::Xml
