#pragma once

#include <string>
#include <string_view>

namespace System::Xml {

class XmlConvert final {
public:
    XmlConvert() = delete;

    static std::string EncodeName(std::string_view name);
    static std::string DecodeName(std::string_view name);
    static std::string EncodeLocalName(std::string_view name);
    static std::string EncodeNmToken(std::string_view name);
    static bool IsXmlChar(char ch);
    static bool IsWhitespaceChar(char ch);
    static bool IsStartNameChar(char ch);
    static bool IsNameChar(char ch);
    static bool IsNCNameStartChar(char ch);
    static bool IsNCNameChar(char ch);
    static std::string VerifyName(std::string_view name);
    static std::string VerifyNCName(std::string_view name);
    static std::string VerifyNmToken(std::string_view name);
    static std::string VerifyXmlChars(std::string_view content);

    static std::string ToString(bool value);
    static std::string ToString(int value);
    static std::string ToString(long long value);
    static std::string ToString(double value);
    static std::string ToString(float value);

    static bool ToBoolean(std::string_view value);
    static int ToInt32(std::string_view value);
    static long long ToInt64(std::string_view value);
    static double ToDouble(std::string_view value);
    static float ToSingle(std::string_view value);
};

}  // namespace System::Xml
