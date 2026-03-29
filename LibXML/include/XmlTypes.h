#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

namespace System::Xml {

class XmlSchemaSet;
class XmlResolver;
class XmlNode;

enum class XmlNodeType {
    None,
    Element,
    Attribute,
    Text,
    CDATA,
    EntityReference,
    Entity,
    ProcessingInstruction,
    Comment,
    Document,
    DocumentType,
    DocumentFragment,
    Notation,
    Whitespace,
    SignificantWhitespace,
    EndElement,
    EndEntity,
    XmlDeclaration,
};

enum class XmlNewLineHandling {
    None,
    Replace,
    Entitize,
};

enum class DtdProcessing {
    Prohibit,
    Ignore,
    Parse,
};

enum class ConformanceLevel {
    Auto,
    Fragment,
    Document,
};

enum class ValidationType {
    None,
    Dtd,
    Schema,
};

enum class ReadState {
    Initial,
    Interactive,
    EndOfFile,
    Closed,
    Error,
};

enum class WriteState {
    Start,
    Prolog,
    Element,
    Attribute,
    Content,
    Closed,
    Error,
};

struct XmlWriterSettings {
    bool Indent = false;
    bool OmitXmlDeclaration = false;
    std::string IndentChars = "  ";
    std::string NewLineChars = "\r\n";
    XmlNewLineHandling NewLineHandling = XmlNewLineHandling::None;
    std::string Encoding = "utf-8";
    ConformanceLevel Conformance = ConformanceLevel::Document;
};

struct XmlReaderSettings {
    bool IgnoreComments = false;
    bool IgnoreWhitespace = false;
    bool IgnoreProcessingInstructions = false;
    DtdProcessing DtdProcessing = DtdProcessing::Parse;
    ConformanceLevel Conformance = ConformanceLevel::Document;
    ValidationType Validation = ValidationType::None;
    std::size_t MaxCharactersInDocument = 0;
    std::size_t MaxCharactersFromEntities = 0;
    std::shared_ptr<XmlSchemaSet> Schemas;
    std::shared_ptr<XmlResolver> Resolver;
};

class XmlException : public std::runtime_error {
public:
    XmlException(const std::string& message, std::size_t line = 0, std::size_t column = 0);

    std::size_t Line() const noexcept;
    std::size_t Column() const noexcept;

private:
    std::size_t line_;
    std::size_t column_;
};

enum class XmlNodeChangedAction {
    Insert,
    Remove,
    Change,
};

struct XmlNodeChangedEventArgs {
    XmlNodeChangedAction Action;
    XmlNode* Node;
    XmlNode* OldParent;
    XmlNode* NewParent;
    std::string OldValue;
    std::string NewValue;
};

using XmlNodeChangedEventHandler = std::function<void(const XmlNodeChangedEventArgs&)>;

}  // namespace System::Xml
