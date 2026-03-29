#pragma once

#include "XmlTypes.h"
#include "XmlNameTable.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace System::Xml {

class XmlNode;

class XmlNodeReader final {
public:
    explicit XmlNodeReader(const XmlNode& node, const XmlReaderSettings& settings = {});

    bool Read();
    bool IsEOF() const noexcept;
    ReadState GetReadState() const noexcept;

    XmlNodeType NodeType() const;
    const std::string& Name() const;
    std::string LocalName() const;
    std::string Prefix() const;
    const std::string& NamespaceURI() const;
    const std::string& Value() const;
    int Depth() const noexcept;
    bool IsEmptyElement() const noexcept;
    bool HasValue() const noexcept;
    int AttributeCount() const noexcept;
    bool HasAttributes() const noexcept;

    std::string GetAttribute(std::string_view name) const;
    std::string GetAttribute(int index) const;
    std::string GetAttribute(std::string_view localName, std::string_view namespaceUri) const;
    bool MoveToAttribute(std::string_view name);
    bool MoveToAttribute(int index);
    bool MoveToAttribute(std::string_view localName, std::string_view namespaceUri);
    bool MoveToFirstAttribute();
    bool MoveToNextAttribute();
    bool MoveToElement();
    std::string LookupNamespace(std::string_view prefix) const;

    std::string ReadInnerXml() const;
    std::string ReadOuterXml() const;
    std::string ReadContentAsString();
    int ReadContentAsInt();
    long long ReadContentAsLong();
    double ReadContentAsDouble();
    bool ReadContentAsBoolean();
    std::string ReadString();
    int ReadBase64(std::vector<unsigned char>& buffer);

    XmlNodeType MoveToContent();
    bool IsStartElement();
    bool IsStartElement(std::string_view name);
    bool IsStartElement(std::string_view localName, std::string_view namespaceUri);
    void ReadStartElement();
    void ReadStartElement(std::string_view name);
    void ReadStartElement(std::string_view localName, std::string_view namespaceUri);
    void ReadEndElement();
    std::string ReadElementContentAsString();
    std::string ReadElementContentAsString(std::string_view localName, std::string_view namespaceUri);
    int ReadElementContentAsInt();
    int ReadElementContentAsInt(std::string_view localName, std::string_view namespaceUri);
    long long ReadElementContentAsLong();
    long long ReadElementContentAsLong(std::string_view localName, std::string_view namespaceUri);
    double ReadElementContentAsDouble();
    double ReadElementContentAsDouble(std::string_view localName, std::string_view namespaceUri);
    bool ReadElementContentAsBoolean();
    bool ReadElementContentAsBoolean(std::string_view localName, std::string_view namespaceUri);
    std::string ReadElementString();
    std::string ReadElementString(std::string_view name);
    std::string ReadElementString(std::string_view localName, std::string_view namespaceUri);
    void Skip();
    bool ReadToFollowing(std::string_view name);
    bool ReadToDescendant(std::string_view name);
    bool ReadToNextSibling(std::string_view name);
    const XmlNameTable& NameTable() const noexcept;

private:
    struct NodeEvent {
        XmlNodeType nodeType = XmlNodeType::None;
        const XmlNode* sourceNode = nullptr;
        std::string name;
        std::string localName;
        std::string prefix;
        std::string namespaceUri;
        std::string value;
        int depth = 0;
        bool isEmptyElement = false;
        std::string innerXml;
        std::string outerXml;
        std::vector<std::pair<std::string, std::string>> attributes;
        std::vector<std::string> attributeLocalNames;
        std::vector<std::string> attributePrefixes;
        std::vector<std::string> attributeNamespaceUris;
    };

    void BuildEvents(const XmlNode& node, int depth, bool preserveSpace);
    void AppendEvent(
        XmlNodeType nodeType,
        const XmlNode* sourceNode,
        std::string name,
        std::string namespaceUri,
        std::string value,
        int depth,
        bool isEmptyElement,
        std::string innerXml,
        std::string outerXml,
        std::vector<std::pair<std::string, std::string>> attributes = {},
        std::vector<std::string> attributeNamespaceUris = {});
    const NodeEvent* CurrentEvent() const noexcept;
    const std::vector<std::pair<std::string, std::string>>& CurrentAttributes() const noexcept;

    XmlReaderSettings settings_;
    std::vector<NodeEvent> events_;
    std::size_t currentIndex_ = 0;
    bool started_ = false;
    bool eof_ = false;
    int attributeIndex_ = -1;
    XmlNameTable nameTable_;
};

}  // namespace System::Xml
