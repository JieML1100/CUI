#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>

namespace System::Xml {

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
    std::size_t MaxCharactersInDocument = 0;
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

}  // namespace System::Xml
