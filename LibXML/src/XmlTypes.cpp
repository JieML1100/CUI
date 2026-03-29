#include "../include/XmlUtilityInternal.h"

namespace System::Xml {

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
