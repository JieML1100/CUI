#pragma once

#include "XmlNode.h"

#include <array>
#include <iostream>
#include <memory>
#include <memory_resource>
#include <string>
#include <utility>
#include <vector>

namespace System::Xml {

class XmlSchemaSet;
class XmlReader;
class XmlWriter;

class XmlDocument final : public XmlNode {
public:
    XmlDocument();
    ~XmlDocument() override;

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
    void Validate(const XmlSchemaSet& schemas) const;

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

    void SetNodeInserting(XmlNodeChangedEventHandler handler);
    void SetNodeInserted(XmlNodeChangedEventHandler handler);
    void SetNodeRemoving(XmlNodeChangedEventHandler handler);
    void SetNodeRemoved(XmlNodeChangedEventHandler handler);
    void SetNodeChanging(XmlNodeChangedEventHandler handler);
    void SetNodeChanged(XmlNodeChangedEventHandler handler);

protected:
    void ValidateChildInsertion(const std::shared_ptr<XmlNode>& child, const XmlNode* replacingChild = nullptr) const override;

private:
    struct NodeArenaState {
        std::array<std::byte, 4096> initialBuffer{};
        std::pmr::monotonic_buffer_resource resource;

        NodeArenaState()
            : resource(initialBuffer.data(), initialBuffer.size()) {
        }
    };

    template <typename TValue>
    class NodeArenaAllocator {
    public:
        using value_type = TValue;

        NodeArenaAllocator() = default;

        explicit NodeArenaAllocator(std::shared_ptr<NodeArenaState> state) noexcept
            : state_(std::move(state)) {
        }

        template <typename TOther>
        NodeArenaAllocator(const NodeArenaAllocator<TOther>& other) noexcept
            : state_(other.state_) {
        }

        [[nodiscard]] TValue* allocate(std::size_t count) {
            std::pmr::polymorphic_allocator<TValue> allocator(&state_->resource);
            return allocator.allocate(count);
        }

        void deallocate(TValue* pointer, std::size_t count) noexcept {
            std::pmr::polymorphic_allocator<TValue> allocator(&state_->resource);
            allocator.deallocate(pointer, count);
        }

        template <typename TOther>
        bool operator==(const NodeArenaAllocator<TOther>& other) const noexcept {
            return state_.get() == other.state_.get();
        }

        template <typename TOther>
        bool operator!=(const NodeArenaAllocator<TOther>& other) const noexcept {
            return !(*this == other);
        }

    private:
        std::shared_ptr<NodeArenaState> state_;

        template <typename TOther>
        friend class NodeArenaAllocator;
    };

    template <typename TNode, typename... TArgs>
    std::shared_ptr<TNode> AllocateNode(TArgs&&... args) const {
        NodeArenaAllocator<TNode> allocator(nodeArenaState_);
        return std::allocate_shared<TNode>(allocator, std::forward<TArgs>(args)...);
    }

    template <typename TNode, typename... TArgs>
    std::shared_ptr<TNode> AllocateOwnedNode(TArgs&&... args) const {
        auto node = AllocateNode<TNode>(std::forward<TArgs>(args)...);
        node->SetOwnerDocument(const_cast<XmlDocument*>(this));
        return node;
    }

    mutable std::shared_ptr<NodeArenaState> nodeArenaState_;
    bool preserveWhitespace_ = false;
    XmlNodeChangedEventHandler onNodeInserting_;
    XmlNodeChangedEventHandler onNodeInserted_;
    XmlNodeChangedEventHandler onNodeRemoving_;
    XmlNodeChangedEventHandler onNodeRemoved_;
    XmlNodeChangedEventHandler onNodeChanging_;
    XmlNodeChangedEventHandler onNodeChanged_;

    void FireNodeInserting(XmlNode* node, XmlNode* newParent) const;
    void FireNodeInserted(XmlNode* node, XmlNode* newParent) const;
    void FireNodeRemoving(XmlNode* node, XmlNode* oldParent) const;
    void FireNodeRemoved(XmlNode* node, XmlNode* oldParent) const;
    void FireNodeChanging(XmlNode* node, const std::string& oldValue, const std::string& newValue) const;
    void FireNodeChanged(XmlNode* node, const std::string& oldValue, const std::string& newValue) const;
    bool HasNodeChangeHandlers() const noexcept;
    void ValidateReaderAgainstSchemas(XmlReader& reader, const XmlSchemaSet& schemas) const;
    void ValidateIdentityConstraints(const XmlSchemaSet& schemas) const;

    friend void ValidateXmlReaderInputAgainstSchemas(const std::shared_ptr<const std::string>& xml, const XmlReaderSettings& settings);
    friend void ValidateXmlReaderInputAgainstSchemas(XmlReader& reader, const XmlReaderSettings& settings);
    friend void LoadXmlDocumentFromReader(XmlReader& reader, XmlDocument& document);

    friend class XmlNode;
};

}  // namespace System::Xml
