#pragma once

#include "XmlDocument.h"

#include <cstddef>
#include <iosfwd>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace System::Xml {

class XmlReader final {
public:
    static XmlReader Create(const std::string& xml, const XmlReaderSettings& settings = {});
    static XmlReader Create(std::istream& stream, const XmlReaderSettings& settings = {});
    static XmlReader CreateFromFile(const std::string& path, const XmlReaderSettings& settings = {});

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

    std::string ReadInnerXml() const;
    std::string ReadOuterXml() const;
    std::string ReadContentAsString();
    int ReadContentAsInt();
    long long ReadContentAsLong();
    double ReadContentAsDouble();
    bool ReadContentAsBoolean();
    std::string ReadString();

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
    long long ReadElementContentAsLong();
    double ReadElementContentAsDouble();
    bool ReadElementContentAsBoolean();
    std::string ReadElementString();
    std::string ReadElementString(std::string_view name);
    std::string ReadElementString(std::string_view localName, std::string_view namespaceUri);
    void Skip();
    bool ReadToFollowing(std::string_view name);
    bool ReadToDescendant(std::string_view name);
    bool ReadToNextSibling(std::string_view name);
    XmlReader ReadSubtree();
    void Close();

private:
    struct ReaderNode {
        XmlNodeType nodeType = XmlNodeType::None;
        std::string name;
        std::string namespaceUri;
        std::string value;
        int depth = 0;
        bool isEmptyElement = false;
        std::shared_ptr<XmlNode> node;
        std::vector<std::pair<std::string, std::string>> attributes;
    };

    explicit XmlReader(XmlReaderSettings settings = {});

    const ReaderNode* CurrentNode() const noexcept;
    const std::pair<std::string, std::string>* CurrentAttribute() const noexcept;
    std::size_t FindMatchingEndElementIndex(std::size_t elementIndex) const;
    void AppendFlattenedNode(const std::shared_ptr<XmlNode>& node, int depth);
    void LoadDocument(const std::shared_ptr<XmlDocument>& document);

    XmlReaderSettings settings_;
    std::shared_ptr<XmlDocument> document_;
    std::vector<ReaderNode> nodes_;
    std::size_t currentIndex_ = static_cast<std::size_t>(-1);
    int attributeIndex_ = -1;
    ReadState state_ = ReadState::Initial;
    mutable std::string scratch_;
};

}  // namespace System::Xml
