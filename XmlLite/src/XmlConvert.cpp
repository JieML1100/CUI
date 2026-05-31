#include "XmlConvert.h"

#include "XmlTypes.h"

#include <cmath>
#include <cctype>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace System::Xml {

namespace {

bool IsHexDigit(char ch) {
    return std::isxdigit(static_cast<unsigned char>(ch)) != 0;
}

int HexValue(char ch) {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }
    if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    }
    return -1;
}

void AppendEncodedByte(std::string& encoded, unsigned char value) {
    std::ostringstream stream;
    stream << "_x" << std::uppercase << std::setfill('0') << std::setw(4)
           << std::hex << static_cast<unsigned int>(value) << "_";
    encoded += stream.str();
}

}  // namespace

std::string XmlConvert::EncodeName(std::string_view name) {
    std::string encoded;
    encoded.reserve(name.size());

    for (std::size_t index = 0; index < name.size(); ++index) {
        const char ch = name[index];
        const bool valid = index == 0 ? IsStartNameChar(ch) : IsNameChar(ch);
        if (valid) {
            encoded.push_back(ch);
        } else {
            AppendEncodedByte(encoded, static_cast<unsigned char>(ch));
        }
    }

    return encoded;
}

std::string XmlConvert::DecodeName(std::string_view name) {
    std::string decoded;
    decoded.reserve(name.size());

    for (std::size_t index = 0; index < name.size(); ++index) {
        if (index + 6 < name.size()
            && name[index] == '_'
            && name[index + 1] == 'x'
            && name[index + 6] == '_'
            && IsHexDigit(name[index + 2])
            && IsHexDigit(name[index + 3])
            && IsHexDigit(name[index + 4])
            && IsHexDigit(name[index + 5])) {
            int value = 0;
            for (std::size_t hex = index + 2; hex < index + 6; ++hex) {
                value = (value << 4) | HexValue(name[hex]);
            }
            decoded.push_back(static_cast<char>(value));
            index += 6;
            continue;
        }

        decoded.push_back(name[index]);
    }

    return decoded;
}

std::string XmlConvert::EncodeLocalName(std::string_view name) {
    std::string encoded = EncodeName(name);
    std::size_t colon = 0;
    while ((colon = encoded.find(':', colon)) != std::string::npos) {
        encoded.replace(colon, 1, "_x003A_");
        colon += 7;
    }
    return encoded;
}

std::string XmlConvert::EncodeNmToken(std::string_view name) {
    std::string encoded;
    encoded.reserve(name.size());

    for (char ch : name) {
        if (IsNameChar(ch)) {
            encoded.push_back(ch);
        } else {
            AppendEncodedByte(encoded, static_cast<unsigned char>(ch));
        }
    }

    return encoded;
}

bool XmlConvert::IsXmlChar(char ch) {
    const auto value = static_cast<unsigned char>(ch);
    return value == 0x09 || value == 0x0A || value == 0x0D || value >= 0x20;
}

bool XmlConvert::IsWhitespaceChar(char ch) {
    return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
}

bool XmlConvert::IsStartNameChar(char ch) {
    const auto value = static_cast<unsigned char>(ch);
    return value >= 0x80 || std::isalpha(value) != 0 || ch == '_' || ch == ':';
}

bool XmlConvert::IsNameChar(char ch) {
    const auto value = static_cast<unsigned char>(ch);
    return IsStartNameChar(ch) || std::isdigit(value) != 0 || ch == '-' || ch == '.';
}

bool XmlConvert::IsNCNameStartChar(char ch) {
    return IsStartNameChar(ch) && ch != ':';
}

bool XmlConvert::IsNCNameChar(char ch) {
    return IsNameChar(ch) && ch != ':';
}

std::string XmlConvert::VerifyName(std::string_view name) {
    if (name.empty() || !IsStartNameChar(name.front())) {
        throw XmlException("'" + std::string(name) + "' is not a valid XML name");
    }

    for (char ch : name.substr(1)) {
        if (!IsNameChar(ch)) {
            throw XmlException("'" + std::string(name) + "' is not a valid XML name");
        }
    }

    return std::string(name);
}

std::string XmlConvert::VerifyNCName(std::string_view name) {
    if (name.empty() || !IsNCNameStartChar(name.front())) {
        throw XmlException("'" + std::string(name) + "' is not a valid NCName");
    }

    for (char ch : name.substr(1)) {
        if (!IsNCNameChar(ch)) {
            throw XmlException("'" + std::string(name) + "' is not a valid NCName");
        }
    }

    return std::string(name);
}

std::string XmlConvert::VerifyNmToken(std::string_view name) {
    if (name.empty()) {
        throw XmlException("The empty string is not a valid NMTOKEN");
    }

    for (char ch : name) {
        if (!IsNameChar(ch)) {
            throw XmlException("'" + std::string(name) + "' is not a valid NMTOKEN");
        }
    }

    return std::string(name);
}

std::string XmlConvert::VerifyXmlChars(std::string_view content) {
    for (std::size_t index = 0; index < content.size(); ++index) {
        if (!IsXmlChar(content[index])) {
            throw XmlException("Invalid XML character at position " + std::to_string(index));
        }
    }
    return std::string(content);
}

std::string XmlConvert::ToString(bool value) {
    return value ? "true" : "false";
}

std::string XmlConvert::ToString(int value) {
    return std::to_string(value);
}

std::string XmlConvert::ToString(long long value) {
    return std::to_string(value);
}

std::string XmlConvert::ToString(double value) {
    if (std::isnan(value)) {
        return "NaN";
    }
    if (std::isinf(value)) {
        return value > 0 ? "INF" : "-INF";
    }

    std::ostringstream stream;
    stream << std::setprecision(17) << value;
    return stream.str();
}

std::string XmlConvert::ToString(float value) {
    if (std::isnan(value)) {
        return "NaN";
    }
    if (std::isinf(value)) {
        return value > 0 ? "INF" : "-INF";
    }

    std::ostringstream stream;
    stream << std::setprecision(9) << value;
    return stream.str();
}

bool XmlConvert::ToBoolean(std::string_view value) {
    if (value == "true" || value == "1") {
        return true;
    }
    if (value == "false" || value == "0") {
        return false;
    }
    throw XmlException("'" + std::string(value) + "' is not a valid boolean value");
}

int XmlConvert::ToInt32(std::string_view value) {
    const std::string text(value);
    try {
        std::size_t position = 0;
        const int result = std::stoi(text, &position);
        if (position != text.size()) {
            throw XmlException("'" + text + "' is not a valid Int32 value");
        }
        return result;
    } catch (const std::invalid_argument&) {
        throw XmlException("'" + text + "' is not a valid Int32 value");
    } catch (const std::out_of_range&) {
        throw XmlException("'" + text + "' is out of range for Int32");
    }
}

long long XmlConvert::ToInt64(std::string_view value) {
    const std::string text(value);
    try {
        std::size_t position = 0;
        const long long result = std::stoll(text, &position);
        if (position != text.size()) {
            throw XmlException("'" + text + "' is not a valid Int64 value");
        }
        return result;
    } catch (const std::invalid_argument&) {
        throw XmlException("'" + text + "' is not a valid Int64 value");
    } catch (const std::out_of_range&) {
        throw XmlException("'" + text + "' is out of range for Int64");
    }
}

double XmlConvert::ToDouble(std::string_view value) {
    if (value == "NaN") {
        return std::numeric_limits<double>::quiet_NaN();
    }
    if (value == "INF" || value == "Infinity") {
        return std::numeric_limits<double>::infinity();
    }
    if (value == "-INF" || value == "-Infinity") {
        return -std::numeric_limits<double>::infinity();
    }

    const std::string text(value);
    try {
        std::size_t position = 0;
        const double result = std::stod(text, &position);
        if (position != text.size()) {
            throw XmlException("'" + text + "' is not a valid Double value");
        }
        return result;
    } catch (const std::invalid_argument&) {
        throw XmlException("'" + text + "' is not a valid Double value");
    } catch (const std::out_of_range&) {
        throw XmlException("'" + text + "' is out of range for Double");
    }
}

float XmlConvert::ToSingle(std::string_view value) {
    return static_cast<float>(ToDouble(value));
}

}  // namespace System::Xml
