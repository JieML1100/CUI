#pragma once

#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace System::Xml {

class XmlNode;
class XmlDocument;
class XmlNamespaceManager;
enum class XmlNodeType;
enum class XPathValueType {
    None,
    NodeSet,
    String,
    Number,
    Boolean,
};

class XPathValue;
class XPathExpression;

class XPathNavigator final {
public:
    XPathNavigator() = default;
    explicit XPathNavigator(const XmlNode* node, const XmlNode* namespaceParent = nullptr);

    bool IsEmpty() const noexcept;
    XmlNodeType NodeType() const;
    std::string Name() const;
    std::string LocalName() const;
    std::string Prefix() const;
    std::string NamespaceURI() const;
    std::string Value() const;
    std::string InnerXml() const;
    std::string OuterXml() const;

    XPathNavigator Clone() const;

    bool MoveToFirstChild();
    bool MoveToNext();
    bool MoveToPrevious();
    bool MoveToParent();
    bool MoveToFirstAttribute();
    bool MoveToNextAttribute();
    void MoveToRoot();

    XPathNavigator SelectSingleNode(std::string_view xpath) const;
    XPathNavigator SelectSingleNode(std::string_view xpath, const XmlNamespaceManager& namespaces) const;
    std::vector<XPathNavigator> Select(std::string_view xpath) const;
    std::vector<XPathNavigator> Select(std::string_view xpath, const XmlNamespaceManager& namespaces) const;

    XPathValue Evaluate(std::string_view xpath) const;
    XPathValue Evaluate(std::string_view xpath, const XmlNamespaceManager& namespaces) const;
    XPathValue Evaluate(const XPathExpression& expression) const;
    XPathValue Evaluate(const XPathExpression& expression, const XmlNamespaceManager& namespaces) const;

    const XmlNode* UnderlyingNode() const noexcept;

private:
    const XmlNode* node_ = nullptr;
    const XmlNode* namespaceParent_ = nullptr;
};

class XPathValue final {
public:
    XPathValue() = default;
    explicit XPathValue(std::vector<XPathNavigator> nodeSet);
    explicit XPathValue(std::string stringValue);
    explicit XPathValue(double numberValue);
    explicit XPathValue(bool booleanValue);

    XPathValueType Type() const noexcept;
    bool IsNodeSet() const noexcept;
    bool IsString() const noexcept;
    bool IsNumber() const noexcept;
    bool IsBoolean() const noexcept;

    const std::vector<XPathNavigator>& AsNodeSet() const;
    const std::string& AsString() const;
    double AsNumber() const;
    bool AsBoolean() const;

private:
    XPathValueType type_ = XPathValueType::None;
    std::vector<XPathNavigator> nodeSet_;
    std::string stringValue_;
    double numberValue_ = 0.0;
    bool booleanValue_ = false;
};

class XPathExpression final {
public:
    XPathExpression() = default;

    static XPathExpression Compile(std::string_view xpath);

    const std::string& Expression() const noexcept;
    bool Empty() const noexcept;

private:
    explicit XPathExpression(std::string expression);

    std::string expression_;
};

class XPathDocument final {
public:
    XPathDocument();
    explicit XPathDocument(std::string_view xml);

    static std::shared_ptr<XPathDocument> Parse(std::string_view xml);

    void LoadXml(std::string_view xml);
    void Load(const std::string& path);
    void Load(std::istream& stream);

    XPathNavigator CreateNavigator() const;
    const XmlDocument& Document() const noexcept;

private:
    std::shared_ptr<XmlDocument> document_;
};

}  // namespace System::Xml
