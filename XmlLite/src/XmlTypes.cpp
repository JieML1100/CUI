#include "XmlTypes.h"

namespace System::Xml {

namespace {

std::string BuildExceptionMessage(const std::string& message, std::size_t line, std::size_t column) {
    if (line == 0 && column == 0) {
        return message;
    }

    return message + " Line " + std::to_string(line) + ", position " + std::to_string(column) + ".";
}

}  // namespace

XmlException::XmlException(const std::string& message, std::size_t line, std::size_t column)
    : std::runtime_error(BuildExceptionMessage(message, line, column)), line_(line), column_(column) {
}

std::size_t XmlException::Line() const noexcept {
    return line_;
}

std::size_t XmlException::Column() const noexcept {
    return column_;
}

}  // namespace System::Xml
