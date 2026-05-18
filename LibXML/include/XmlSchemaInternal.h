#pragma once
// Schema validation and builtin type internal helpers.

#include "XmlUtilityInternal.h"

namespace System::Xml {

void ValidateXmlReaderInputAgainstSchemas(const std::shared_ptr<const std::string>& xml, const XmlReaderSettings& settings);
void ValidateXmlReaderInputAgainstSchemas(XmlReader& reader, const XmlReaderSettings& settings);

namespace {

struct PracticalIntegerValue {
    bool negative = false;
    unsigned long long magnitude = 0;
};

bool TryParsePracticalInteger(const std::string& input, PracticalIntegerValue& result) {
    if (input.empty()) {
        return false;
    }

    std::size_t index = 0;
    bool negative = false;
    if (input[index] == '+' || input[index] == '-') {
        negative = input[index] == '-';
        ++index;
    }
    if (index == input.size()) {
        return false;
    }

    unsigned long long magnitude = 0;
    for (; index < input.size(); ++index) {
        const unsigned char digitChar = static_cast<unsigned char>(input[index]);
        if (std::isdigit(digitChar) == 0) {
            return false;
        }

        const unsigned int digit = static_cast<unsigned int>(digitChar - '0');
        if (magnitude > (std::numeric_limits<unsigned long long>::max() - digit) / 10ULL) {
            return false;
        }
        magnitude = magnitude * 10ULL + digit;
    }

    result.negative = negative && magnitude != 0;
    result.magnitude = magnitude;
    return true;
}

PracticalIntegerValue ParsePracticalIntegerOrThrow(const std::string& input) {
    PracticalIntegerValue result;
    if (!TryParsePracticalInteger(input, result)) {
        throw XmlException("invalid integer lexical form");
    }
    return result;
}

int ComparePracticalIntegerValues(const PracticalIntegerValue& left, const PracticalIntegerValue& right) {
    if (left.negative != right.negative) {
        return left.negative ? -1 : 1;
    }
    if (left.magnitude == right.magnitude) {
        return 0;
    }
    if (left.negative) {
        return left.magnitude > right.magnitude ? -1 : 1;
    }
    return left.magnitude < right.magnitude ? -1 : 1;
}

bool IsPracticalIntegerBuiltinType(const std::string& qualifiedName) {
    return qualifiedName == "xs:integer"
        || qualifiedName == "xs:long"
        || qualifiedName == "xs:int"
        || qualifiedName == "xs:short"
        || qualifiedName == "xs:byte"
        || qualifiedName == "xs:nonNegativeInteger"
        || qualifiedName == "xs:positiveInteger"
        || qualifiedName == "xs:unsignedLong"
        || qualifiedName == "xs:unsignedInt"
        || qualifiedName == "xs:unsignedShort"
        || qualifiedName == "xs:unsignedByte"
        || qualifiedName == "xs:nonPositiveInteger"
        || qualifiedName == "xs:negativeInteger";
}

std::pair<std::optional<PracticalIntegerValue>, std::optional<PracticalIntegerValue>> GetPracticalIntegerBounds(const std::string& qualifiedName) {
    const auto positive = [](unsigned long long magnitude) {
        return PracticalIntegerValue{false, magnitude};
    };
    const auto negative = [](unsigned long long magnitude) {
        return PracticalIntegerValue{true, magnitude};
    };

    if (qualifiedName == "xs:byte") {
        return {negative(128ULL), positive(127ULL)};
    }
    if (qualifiedName == "xs:short") {
        return {negative(32768ULL), positive(32767ULL)};
    }
    if (qualifiedName == "xs:int") {
        return {negative(2147483648ULL), positive(2147483647ULL)};
    }
    if (qualifiedName == "xs:long" || qualifiedName == "xs:integer") {
        return {negative(9223372036854775808ULL), positive(9223372036854775807ULL)};
    }
    if (qualifiedName == "xs:unsignedByte") {
        return {positive(0ULL), positive(255ULL)};
    }
    if (qualifiedName == "xs:unsignedShort") {
        return {positive(0ULL), positive(65535ULL)};
    }
    if (qualifiedName == "xs:unsignedInt") {
        return {positive(0ULL), positive(4294967295ULL)};
    }
    if (qualifiedName == "xs:unsignedLong" || qualifiedName == "xs:nonNegativeInteger") {
        return {positive(0ULL), positive(std::numeric_limits<unsigned long long>::max())};
    }
    if (qualifiedName == "xs:positiveInteger") {
        return {positive(1ULL), positive(std::numeric_limits<unsigned long long>::max())};
    }
    if (qualifiedName == "xs:nonPositiveInteger") {
        return {negative(9223372036854775808ULL), positive(0ULL)};
    }
    if (qualifiedName == "xs:negativeInteger") {
        return {negative(9223372036854775808ULL), negative(1ULL)};
    }
    return {std::nullopt, std::nullopt};
}

bool PracticalIntegerFitsBuiltinType(const std::string& qualifiedName, const PracticalIntegerValue& value) {
    const auto [minValue, maxValue] = GetPracticalIntegerBounds(qualifiedName);
    if (minValue.has_value() && ComparePracticalIntegerValues(value, *minValue) < 0) {
        return false;
    }
    if (maxValue.has_value() && ComparePracticalIntegerValues(value, *maxValue) > 0) {
        return false;
    }
    return true;
}

struct PracticalDecimalValue {
    bool negative = false;
    std::string digits = "0";
    std::size_t scale = 0;
};

PracticalDecimalValue ParsePracticalDecimalOrThrow(const std::string& lexicalValue) {
    if (lexicalValue.empty()) {
        throw XmlException("invalid decimal lexical form");
    }

    std::size_t index = 0;
    bool negative = false;
    if (lexicalValue[index] == '+' || lexicalValue[index] == '-') {
        negative = lexicalValue[index] == '-';
        ++index;
    }

    bool seenDecimalPoint = false;
    bool sawDigit = false;
    std::string integerDigits;
    std::string fractionalDigits;
    for (; index < lexicalValue.size(); ++index) {
        const char ch = lexicalValue[index];
        if (std::isdigit(static_cast<unsigned char>(ch))) {
            sawDigit = true;
            if (seenDecimalPoint) {
                fractionalDigits.push_back(ch);
            } else {
                integerDigits.push_back(ch);
            }
            continue;
        }
        if (ch == '.' && !seenDecimalPoint) {
            seenDecimalPoint = true;
            continue;
        }
        throw XmlException("invalid decimal lexical form");
    }

    if (!sawDigit) {
        throw XmlException("invalid decimal lexical form");
    }

    while (!fractionalDigits.empty() && fractionalDigits.back() == '0') {
        fractionalDigits.pop_back();
    }

    std::string digits = integerDigits + fractionalDigits;
    const std::size_t firstNonZero = digits.find_first_not_of('0');
    if (firstNonZero == std::string::npos) {
        return PracticalDecimalValue{};
    }

    digits.erase(0, firstNonZero);
    PracticalDecimalValue value;
    value.negative = negative;
    value.digits = std::move(digits);
    value.scale = fractionalDigits.size();
    return value;
}

int ComparePracticalDecimalValues(const PracticalDecimalValue& left, const PracticalDecimalValue& right) {
    if (left.negative != right.negative) {
        return left.negative ? -1 : 1;
    }

    const std::size_t commonScale = std::max(left.scale, right.scale);
    const std::string leftScaled = left.digits + std::string(commonScale - left.scale, '0');
    const std::string rightScaled = right.digits + std::string(commonScale - right.scale, '0');

    int comparison = 0;
    if (leftScaled.size() < rightScaled.size()) {
        comparison = -1;
    } else if (leftScaled.size() > rightScaled.size()) {
        comparison = 1;
    } else if (leftScaled < rightScaled) {
        comparison = -1;
    } else if (leftScaled > rightScaled) {
        comparison = 1;
    }

    if (comparison == 0) {
        return 0;
    }
    return left.negative ? -comparison : comparison;
}

std::optional<std::string> GetBuiltinSimpleTypeBase(const std::string& qualifiedName) {
    if (qualifiedName == "xs:string" || qualifiedName == "xs:boolean" || qualifiedName == "xs:float"
        || qualifiedName == "xs:double" || qualifiedName == "xs:decimal" || qualifiedName == "xs:duration"
        || qualifiedName == "xs:date" || qualifiedName == "xs:time" || qualifiedName == "xs:dateTime"
        || qualifiedName == "xs:gYear" || qualifiedName == "xs:gYearMonth"
        || qualifiedName == "xs:gMonth" || qualifiedName == "xs:gDay" || qualifiedName == "xs:gMonthDay"
        || qualifiedName == "xs:hexBinary" || qualifiedName == "xs:base64Binary"
        || qualifiedName == "xs:QName" || qualifiedName == "xs:NOTATION"
        || qualifiedName == "xs:NMTOKENS" || qualifiedName == "xs:IDREFS" || qualifiedName == "xs:ENTITIES") {
        return "xs:anySimpleType";
    }
    if (qualifiedName == "xs:normalizedString") {
        return "xs:string";
    }
    if (qualifiedName == "xs:token") {
        return "xs:normalizedString";
    }
    if (qualifiedName == "xs:language" || qualifiedName == "xs:Name" || qualifiedName == "xs:NMTOKEN" || qualifiedName == "xs:anyURI") {
        return "xs:token";
    }
    if (qualifiedName == "xs:NCName") {
        return "xs:Name";
    }
    if (qualifiedName == "xs:ID" || qualifiedName == "xs:IDREF" || qualifiedName == "xs:ENTITY") {
        return "xs:NCName";
    }
    if (qualifiedName == "xs:integer") {
        return "xs:decimal";
    }
    if (qualifiedName == "xs:long" || qualifiedName == "xs:nonPositiveInteger" || qualifiedName == "xs:nonNegativeInteger") {
        return "xs:integer";
    }
    if (qualifiedName == "xs:int") {
        return "xs:long";
    }
    if (qualifiedName == "xs:short") {
        return "xs:int";
    }
    if (qualifiedName == "xs:byte") {
        return "xs:short";
    }
    if (qualifiedName == "xs:negativeInteger") {
        return "xs:nonPositiveInteger";
    }
    if (qualifiedName == "xs:positiveInteger" || qualifiedName == "xs:unsignedLong") {
        return "xs:nonNegativeInteger";
    }
    if (qualifiedName == "xs:unsignedInt") {
        return "xs:unsignedLong";
    }
    if (qualifiedName == "xs:unsignedShort") {
        return "xs:unsignedInt";
    }
    if (qualifiedName == "xs:unsignedByte") {
        return "xs:unsignedShort";
    }
    return std::nullopt;
}

bool BuiltinSimpleTypeDerivesFrom(const std::string& derivedType, const std::string& baseType, bool& usesRestriction) {
    if (derivedType.empty() || baseType.empty()) {
        return false;
    }
    if (derivedType == baseType) {
        return true;
    }

    std::string current = derivedType;
    std::unordered_set<std::string> visited;
    while (visited.insert(current).second) {
        const std::optional<std::string> parent = GetBuiltinSimpleTypeBase(current);
        if (!parent.has_value()) {
            return false;
        }
        usesRestriction = true;
        if (*parent == baseType) {
            return true;
        }
        current = *parent;
    }
    return false;
}

struct BuiltinSimpleTypeDescriptor {
    enum class Variety {
        Atomic,
        List,
    };

    Variety variety = Variety::Atomic;
    std::string baseType;
    std::optional<std::string> whiteSpace;
    std::string itemType;
    std::optional<std::string> itemWhiteSpace;
};

std::optional<BuiltinSimpleTypeDescriptor> ResolveBuiltinSimpleTypeDescriptor(const std::string& qualifiedName) {
    if (qualifiedName == "xs:anySimpleType"
        || qualifiedName == "xs:int" || qualifiedName == "xs:integer"
        || qualifiedName == "xs:long" || qualifiedName == "xs:short" || qualifiedName == "xs:byte"
        || qualifiedName == "xs:nonNegativeInteger" || qualifiedName == "xs:positiveInteger"
        || qualifiedName == "xs:unsignedLong" || qualifiedName == "xs:unsignedInt"
        || qualifiedName == "xs:unsignedShort" || qualifiedName == "xs:unsignedByte"
        || qualifiedName == "xs:nonPositiveInteger" || qualifiedName == "xs:negativeInteger"
        || qualifiedName == "xs:boolean" || qualifiedName == "xs:float" || qualifiedName == "xs:double"
        || qualifiedName == "xs:decimal" || qualifiedName == "xs:string" || qualifiedName == "xs:duration"
        || qualifiedName == "xs:token" || qualifiedName == "xs:normalizedString"
        || qualifiedName == "xs:language" || qualifiedName == "xs:anyURI"
        || qualifiedName == "xs:date" || qualifiedName == "xs:time" || qualifiedName == "xs:dateTime"
        || qualifiedName == "xs:gYear" || qualifiedName == "xs:gYearMonth"
        || qualifiedName == "xs:gMonth" || qualifiedName == "xs:gDay" || qualifiedName == "xs:gMonthDay"
        || qualifiedName == "xs:hexBinary" || qualifiedName == "xs:base64Binary"
        || qualifiedName == "xs:Name" || qualifiedName == "xs:NCName"
        || qualifiedName == "xs:NMTOKEN" || qualifiedName == "xs:QName" || qualifiedName == "xs:NOTATION"
        || qualifiedName == "xs:ID" || qualifiedName == "xs:IDREF" || qualifiedName == "xs:ENTITY") {
        BuiltinSimpleTypeDescriptor rule;
        rule.baseType = qualifiedName;
        if (qualifiedName == "xs:normalizedString") {
            rule.whiteSpace = "replace";
        }
        if (qualifiedName == "xs:boolean" || qualifiedName == "xs:float" || qualifiedName == "xs:double"
            || qualifiedName == "xs:decimal"
            || qualifiedName == "xs:token" || qualifiedName == "xs:language" || qualifiedName == "xs:anyURI"
            || qualifiedName == "xs:duration"
            || IsPracticalIntegerBuiltinType(qualifiedName)
            || qualifiedName == "xs:date" || qualifiedName == "xs:time" || qualifiedName == "xs:dateTime"
            || qualifiedName == "xs:gYear" || qualifiedName == "xs:gYearMonth"
            || qualifiedName == "xs:gMonth" || qualifiedName == "xs:gDay" || qualifiedName == "xs:gMonthDay"
            || qualifiedName == "xs:hexBinary" || qualifiedName == "xs:base64Binary"
            || qualifiedName == "xs:Name" || qualifiedName == "xs:NCName"
            || qualifiedName == "xs:NMTOKEN" || qualifiedName == "xs:QName" || qualifiedName == "xs:NOTATION"
            || qualifiedName == "xs:ID" || qualifiedName == "xs:IDREF" || qualifiedName == "xs:ENTITY") {
            rule.whiteSpace = "collapse";
        }
        return rule;
    }
    if (qualifiedName == "xs:NMTOKENS") {
        BuiltinSimpleTypeDescriptor rule;
        rule.variety = BuiltinSimpleTypeDescriptor::Variety::List;
        rule.baseType = "xs:string";
        rule.whiteSpace = "collapse";
        rule.itemType = "xs:NMTOKEN";
        rule.itemWhiteSpace = "collapse";
        return rule;
    }
    if (qualifiedName == "xs:IDREFS") {
        BuiltinSimpleTypeDescriptor rule;
        rule.variety = BuiltinSimpleTypeDescriptor::Variety::List;
        rule.baseType = "xs:string";
        rule.whiteSpace = "collapse";
        rule.itemType = "xs:IDREF";
        rule.itemWhiteSpace = "collapse";
        return rule;
    }
    if (qualifiedName == "xs:ENTITIES") {
        BuiltinSimpleTypeDescriptor rule;
        rule.variety = BuiltinSimpleTypeDescriptor::Variety::List;
        rule.baseType = "xs:string";
        rule.whiteSpace = "collapse";
        rule.itemType = "xs:ENTITY";
        rule.itemWhiteSpace = "collapse";
        return rule;
    }
    return std::nullopt;
}

struct PracticalDurationValue {
    bool negative = false;
    unsigned long long wholeSeconds = 0;
    int fractionalNanoseconds = 0;
};

int ComparePracticalDurationValues(const PracticalDurationValue& left, const PracticalDurationValue& right) {
    if (left.negative != right.negative) {
        return left.negative ? -1 : 1;
    }

    if (left.wholeSeconds != right.wholeSeconds) {
        if (left.negative) {
            return left.wholeSeconds > right.wholeSeconds ? -1 : 1;
        }
        return left.wholeSeconds < right.wholeSeconds ? -1 : 1;
    }

    if (left.fractionalNanoseconds != right.fractionalNanoseconds) {
        if (left.negative) {
            return left.fractionalNanoseconds > right.fractionalNanoseconds ? -1 : 1;
        }
        return left.fractionalNanoseconds < right.fractionalNanoseconds ? -1 : 1;
    }

    return 0;
}

PracticalDurationValue ParsePracticalDurationOrThrow(const std::string& lexicalValue) {
    std::size_t index = 0;
    bool negative = false;
    if (!lexicalValue.empty() && lexicalValue[0] == '-') {
        negative = true;
        index = 1;
    }

    if (index >= lexicalValue.size() || lexicalValue[index] != 'P') {
        throw XmlException("invalid duration lexical form");
    }
    ++index;

    bool inTimeSection = false;
    bool sawComponent = false;
    bool sawTimeComponent = false;
    unsigned long long wholeSeconds = 0;
    int fractionalNanoseconds = 0;

    const auto parseUnsignedComponent = [&](const std::string& text, const std::size_t start, const std::size_t end) {
        if (start >= end) {
            throw XmlException("invalid duration lexical form");
        }
        unsigned long long value = 0;
        for (std::size_t pos = start; pos < end; ++pos) {
            const char ch = text[pos];
            if (!std::isdigit(static_cast<unsigned char>(ch))) {
                throw XmlException("invalid duration lexical form");
            }
            if (value > (std::numeric_limits<unsigned long long>::max() - static_cast<unsigned long long>(ch - '0')) / 10ULL) {
                throw XmlException("invalid duration lexical form");
            }
            value = value * 10ULL + static_cast<unsigned long long>(ch - '0');
        }
        return value;
    };

    const auto addSecondsChecked = [&](const unsigned long long value) {
        if (wholeSeconds > std::numeric_limits<unsigned long long>::max() - value) {
            throw XmlException("invalid duration lexical form");
        }
        wholeSeconds += value;
    };

    while (index < lexicalValue.size()) {
        if (lexicalValue[index] == 'T') {
            if (inTimeSection) {
                throw XmlException("invalid duration lexical form");
            }
            inTimeSection = true;
            ++index;
            if (index >= lexicalValue.size()) {
                throw XmlException("invalid duration lexical form");
            }
            continue;
        }

        const std::size_t numberStart = index;
        while (index < lexicalValue.size() && std::isdigit(static_cast<unsigned char>(lexicalValue[index]))) {
            ++index;
        }
        if (numberStart == index) {
            throw XmlException("invalid duration lexical form");
        }

        unsigned long long wholePart = parseUnsignedComponent(lexicalValue, numberStart, index);
        int fractionPart = 0;
        if (index < lexicalValue.size() && lexicalValue[index] == '.') {
            if (!inTimeSection) {
                throw XmlException("invalid duration lexical form");
            }
            ++index;
            const std::size_t fractionStart = index;
            while (index < lexicalValue.size() && std::isdigit(static_cast<unsigned char>(lexicalValue[index]))) {
                ++index;
            }
            if (fractionStart == index) {
                throw XmlException("invalid duration lexical form");
            }
            for (std::size_t pos = 0; pos < 9; ++pos) {
                fractionPart *= 10;
                const std::size_t digitIndex = fractionStart + pos;
                if (digitIndex < index) {
                    fractionPart += lexicalValue[digitIndex] - '0';
                }
            }
        }

        if (index >= lexicalValue.size()) {
            throw XmlException("invalid duration lexical form");
        }

        const char designator = lexicalValue[index++];
        sawComponent = true;
        switch (designator) {
        case 'D':
            if (inTimeSection) {
                throw XmlException("invalid duration lexical form");
            }
            if (wholePart > std::numeric_limits<unsigned long long>::max() / 86400ULL) {
                throw XmlException("invalid duration lexical form");
            }
            addSecondsChecked(wholePart * 86400ULL);
            break;
        case 'H':
            if (!inTimeSection || fractionPart != 0) {
                throw XmlException("invalid duration lexical form");
            }
            sawTimeComponent = true;
            if (wholePart > std::numeric_limits<unsigned long long>::max() / 3600ULL) {
                throw XmlException("invalid duration lexical form");
            }
            addSecondsChecked(wholePart * 3600ULL);
            break;
        case 'M':
            if (!inTimeSection || fractionPart != 0) {
                throw XmlException("invalid duration lexical form");
            }
            sawTimeComponent = true;
            if (wholePart > std::numeric_limits<unsigned long long>::max() / 60ULL) {
                throw XmlException("invalid duration lexical form");
            }
            addSecondsChecked(wholePart * 60ULL);
            break;
        case 'S':
            if (!inTimeSection) {
                throw XmlException("invalid duration lexical form");
            }
            sawTimeComponent = true;
            addSecondsChecked(wholePart);
            fractionalNanoseconds = fractionPart;
            break;
        case 'Y':
        case 'W':
            throw XmlException("unsupported duration lexical form");
        default:
            throw XmlException("invalid duration lexical form");
        }
    }

    if (!sawComponent || (inTimeSection && !sawTimeComponent)) {
        throw XmlException("invalid duration lexical form");
    }

    return PracticalDurationValue{ negative, wholeSeconds, fractionalNanoseconds };
}

bool TryDecodeHexBinary(const std::string& input, std::vector<unsigned char>& result) {
    if (input.size() % 2 != 0) {
        return false;
    }

    auto decodeHexNibble = [](const char ch) -> int {
        if (ch >= '0' && ch <= '9') return ch - '0';
        if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
        if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
        return -1;
    };

    result.clear();
    result.reserve(input.size() / 2);
    for (std::size_t index = 0; index < input.size(); index += 2) {
        const int high = decodeHexNibble(input[index]);
        const int low = decodeHexNibble(input[index + 1]);
        if (high < 0 || low < 0) {
            result.clear();
            return false;
        }
        result.push_back(static_cast<unsigned char>((high << 4) | low));
    }
    return true;
}

bool TryDecodeBase64Binary(const std::string& input, std::vector<unsigned char>& result) {
    if (input.empty()) {
        result.clear();
        return true;
    }
    if (std::any_of(input.begin(), input.end(), IsWhitespace) || input.size() % 4 != 0) {
        return false;
    }

    std::size_t padding = 0;
    if (!input.empty() && input.back() == '=') {
        padding = 1;
        if (input.size() >= 2 && input[input.size() - 2] == '=') {
            padding = 2;
        }
    }
    for (std::size_t index = 0; index + padding < input.size(); ++index) {
        if (input[index] == '=') {
            return false;
        }
    }
    if (padding > 2) {
        return false;
    }

    result.clear();
    result.reserve((input.size() / 4) * 3);
    for (std::size_t index = 0; index < input.size(); index += 4) {
        int values[4] = { 0, 0, 0, 0 };
        int localPadding = 0;
        for (int offset = 0; offset < 4; ++offset) {
            const char ch = input[index + offset];
            if (ch == '=') {
                values[offset] = 0;
                ++localPadding;
                continue;
            }
            const int value = DecodeBase64Char(ch);
            if (value < 0) {
                result.clear();
                return false;
            }
            values[offset] = value;
        }
        if (localPadding > 0 && index + 4 != input.size()) {
            result.clear();
            return false;
        }
        if (localPadding == 1 && input[index + 3] != '=') {
            result.clear();
            return false;
        }
        if (localPadding == 2 && !(input[index + 2] == '=' && input[index + 3] == '=')) {
            result.clear();
            return false;
        }
        if (localPadding > 2) {
            result.clear();
            return false;
        }

        const unsigned int chunk = (static_cast<unsigned int>(values[0]) << 18)
            | (static_cast<unsigned int>(values[1]) << 12)
            | (static_cast<unsigned int>(values[2]) << 6)
            | static_cast<unsigned int>(values[3]);
        result.push_back(static_cast<unsigned char>((chunk >> 16) & 0xFF));
        if (localPadding < 2) {
            result.push_back(static_cast<unsigned char>((chunk >> 8) & 0xFF));
        }
        if (localPadding < 1) {
            result.push_back(static_cast<unsigned char>(chunk & 0xFF));
        }
    }
    return true;
}

bool IsWhitespaceOnly(std::string_view value) {
    return std::all_of(value.begin(), value.end(), IsWhitespace);
}

bool IsXmlSpacePreserve(const std::string& value) {
    return value == "preserve";
}

bool IsXmlSpaceDefault(const std::string& value) {
    return value == "default";
}

std::string_view ResolvePredefinedEntityReferenceValueView(std::string_view name) noexcept {
    if (name == "lt") {
        return "<";
    }
    if (name == "gt") {
        return ">";
    }
    if (name == "amp") {
        return "&";
    }
    if (name == "apos") {
        return "'";
    }
    if (name == "quot") {
        return "\"";
    }
    return {};
}

std::string ResolvePredefinedEntityReferenceValue(std::string_view name) {
    return std::string(ResolvePredefinedEntityReferenceValueView(name));
}

bool TryParseUnsignedInteger(std::string_view digits, const unsigned int base, unsigned int& value) noexcept {
    if (digits.empty() || (base != 10 && base != 16)) {
        return false;
    }

    unsigned int parsed = 0;
    for (const char ch : digits) {
        unsigned int digit = 0;
        if (ch >= '0' && ch <= '9') {
            digit = static_cast<unsigned int>(ch - '0');
        } else if (base == 16 && ch >= 'a' && ch <= 'f') {
            digit = static_cast<unsigned int>(10 + (ch - 'a'));
        } else if (base == 16 && ch >= 'A' && ch <= 'F') {
            digit = static_cast<unsigned int>(10 + (ch - 'A'));
        } else {
            return false;
        }

        if (digit >= base) {
            return false;
        }
        if (parsed > (std::numeric_limits<unsigned int>::max() - digit) / base) {
            return false;
        }

        parsed = parsed * base + digit;
    }

    value = parsed;
    return true;
}

bool TryParseNumericEntityReferenceCodePoint(std::string_view entity, unsigned int& codePoint) noexcept {
    if (entity.size() < 2 || entity.front() != '#') {
        return false;
    }

    if (entity.size() > 2 && entity[1] == 'x') {
        if (!TryParseUnsignedInteger(entity.substr(2), 16, codePoint)) {
            return false;
        }

        return IsValidXmlCharacterCodePoint(codePoint);
    }

    if (entity.size() > 2 && entity[1] == 'X') {
        return false;
    }

    if (!TryParseUnsignedInteger(entity.substr(1), 10, codePoint)) {
        return false;
    }

    return IsValidXmlCharacterCodePoint(codePoint);
}

void AppendCodePointUtf8(std::string& output, unsigned int codePoint) {
    if (codePoint <= 0x7F) {
        output.push_back(static_cast<char>(codePoint));
    } else if (codePoint <= 0x7FF) {
        output.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else if (codePoint <= 0xFFFF) {
        output.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
        output.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else {
        output.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
        output.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    }
}

std::optional<std::string> LookupInternalEntityDeclaration(const XmlDocumentType* documentType, std::string_view name) {
    if (documentType == nullptr) {
        return std::nullopt;
    }

    const auto entityNode = documentType->Entities().GetNamedItem(name);
    if (entityNode == nullptr || entityNode->NodeType() != XmlNodeType::Entity) {
        return std::nullopt;
    }

    const auto* entity = static_cast<const XmlEntity*>(entityNode.get());
    if (!entity->PublicId().empty() || !entity->SystemId().empty() || !entity->NotationName().empty()) {
        return std::nullopt;
    }

    return entity->Value();
}

std::optional<std::string> LookupDocumentInternalEntityDeclaration(const XmlDocument* document, std::string_view name) {
    return document == nullptr ? std::nullopt : LookupInternalEntityDeclaration(document->DocumentType().get(), name);
}

bool HasDocumentEntityDeclaration(const XmlDocument* document, std::string_view name) {
    return document != nullptr
        && document->DocumentType() != nullptr
        && document->DocumentType()->Entities().GetNamedItem(name) != nullptr;
}

bool HasDocumentUnparsedEntityDeclaration(const XmlDocument* document, std::string_view name) {
    if (document == nullptr || document->DocumentType() == nullptr) {
        return false;
    }

    const auto entityNode = document->DocumentType()->Entities().GetNamedItem(name);
    if (entityNode == nullptr || entityNode->NodeType() != XmlNodeType::Entity) {
        return false;
    }

    const auto* entity = static_cast<const XmlEntity*>(entityNode.get());
    return !entity->NotationName().empty();
}

bool HasDocumentNotationDeclaration(const XmlDocument* document, const std::string& name) {
    return document != nullptr
        && document->DocumentType() != nullptr
        && document->DocumentType()->Notations().GetNamedItem(name) != nullptr;
}

void PopulateInternalEntityDeclarations(
    const std::vector<std::shared_ptr<XmlNode>>& entities,
    std::unordered_map<std::string, std::string>& declarations) {
    for (const auto& node : entities) {
        if (!node || node->NodeType() != XmlNodeType::Entity) {
            continue;
        }

        const auto* entity = static_cast<const XmlEntity*>(node.get());
        if (!entity->PublicId().empty() || !entity->SystemId().empty() || !entity->NotationName().empty()) {
            continue;
        }

        declarations.try_emplace(entity->Name(), entity->Value());
    }
}

template <typename Resolver, typename ErrorHandler>
void DecodeEntityTextTo(
    std::string& decoded,
    std::string_view value,
    const Resolver& resolver,
    const ErrorHandler& onError,
    int depth = 0,
    const std::vector<std::string>* resolutionStack = nullptr) {
    if (depth > 16) {
        onError("Entity reference recursion limit exceeded");
        return;
    }

    decoded.reserve(decoded.size() + value.size());

    std::size_t cursor = 0;
    while (cursor < value.size()) {
        const auto ampersand = value.find('&', cursor);
        if (ampersand == std::string_view::npos) {
            decoded.append(value.data() + cursor, value.size() - cursor);
            break;
        }

        if (ampersand > cursor) {
            decoded.append(value.data() + cursor, ampersand - cursor);
        }

        const auto semicolon = value.find(';', ampersand + 1);
        if (semicolon == std::string_view::npos) {
            onError("Unterminated entity reference");
            return;
        }

        const std::string_view entity = value.substr(ampersand + 1, semicolon - ampersand - 1);
        const auto predefined = ResolvePredefinedEntityReferenceValueView(entity);
        if (!predefined.empty()) {
            decoded.append(predefined.data(), predefined.size());
        } else if (!entity.empty() && entity.front() == '#') {
            unsigned int codePoint = 0;
            if (!TryParseNumericEntityReferenceCodePoint(entity, codePoint)) {
                onError("Invalid numeric entity reference: &" + std::string(entity) + ';');
                return;
            }
            AppendCodePointUtf8(decoded, codePoint);
        } else {
            const std::string entityName(entity);
            if (const auto resolved = resolver(entityName); resolved.has_value()) {
                if (resolutionStack != nullptr
                    && std::find(resolutionStack->begin(), resolutionStack->end(), entityName) != resolutionStack->end()) {
                    onError("Entity reference cycle detected: &" + entityName + ';');
                    return;
                }

                auto nestedResolutionStack = resolutionStack == nullptr ? std::vector<std::string>{} : *resolutionStack;
                nestedResolutionStack.push_back(entityName);
                DecodeEntityTextTo(decoded, *resolved, resolver, onError, depth + 1, &nestedResolutionStack);
            } else {
                onError("Unknown entity reference: &" + entityName + ';');
                return;
            }
        }

        cursor = semicolon + 1;
    }
}

template <typename Resolver, typename ErrorHandler>
std::string DecodeEntityText(
    std::string_view value,
    const Resolver& resolver,
    const ErrorHandler& onError,
    int depth = 0,
    const std::vector<std::string>* resolutionStack = nullptr) {
    std::string decoded;
    DecodeEntityTextTo(decoded, value, resolver, onError, depth, resolutionStack);
    return decoded;
}

void ValidateWhitespaceValue(std::string_view value, const char* nodeTypeName) {
    if (!IsWhitespaceOnly(value)) {
        throw XmlException(std::string(nodeTypeName) + " nodes can only contain whitespace characters");
    }
}

bool TryParseXmlSchemaBoolean(const std::string& value, bool& result) {
    if (value == "true" || value == "1") {
        result = true;
        return true;
    }
    if (value == "false" || value == "0") {
        result = false;
        return true;
    }
    return false;
}

std::filesystem::path NormalizeSchemaPath(const std::filesystem::path& path) {
    std::error_code error;
    const auto canonical = std::filesystem::weakly_canonical(path, error);
    if (!error) {
        return canonical;
    }

    error.clear();
    const auto absolute = std::filesystem::absolute(path, error);
    if (!error) {
        return absolute.lexically_normal();
    }

    return path.lexically_normal();
}

std::vector<std::pair<std::string, std::string>> CollectInScopeNamespaceBindings(const XmlElement& element) {
    std::vector<const XmlElement*> ancestry;
    for (auto current = &element; current != nullptr; ) {
        ancestry.push_back(current);
        const XmlNode* parent = current->ParentNode();
        current = parent != nullptr && parent->NodeType() == XmlNodeType::Element
            ? static_cast<const XmlElement*>(parent)
            : nullptr;
    }
    std::reverse(ancestry.begin(), ancestry.end());

    std::unordered_map<std::string, std::string> bindings;
    for (const auto* current : ancestry) {
        for (const auto& attribute : current->Attributes()) {
            if (!IsNamespaceDeclarationName(attribute->Name())) {
                continue;
            }
            bindings[NamespaceDeclarationPrefix(attribute->Name())] = attribute->Value();
        }
    }

    bindings["xml"] = "http://www.w3.org/XML/1998/namespace";
    bindings["xmlns"] = "http://www.w3.org/2000/xmlns/";

    std::vector<std::pair<std::string, std::string>> collected;
    collected.reserve(bindings.size());
    for (const auto& [prefix, uri] : bindings) {
        collected.emplace_back(prefix, uri);
    }
    return collected;
}

std::string SerializeIdentityConstraintTuple(const std::vector<std::string>& values) {
    if (values.size() == 1) {
        return values.front();
    }

    std::string serialized;
    for (const auto& value : values) {
        serialized += std::to_string(value.size());
        serialized.push_back(':');
        serialized += value;
        serialized.push_back('|');
    }
    return serialized;
}

std::string BuildIdentityConstraintLookupKey(const std::string& localName, const std::string& namespaceUri) {
    std::string key;
    key.reserve(namespaceUri.size() + localName.size() + 1);
    key += namespaceUri;
    key.push_back('\x1f');
    key += localName;
    return key;
}

std::string IdentityConstraintFieldStringValue(const XmlNode& node) {
    switch (node.NodeType()) {
    case XmlNodeType::Attribute:
    case XmlNodeType::Text:
    case XmlNodeType::CDATA:
    case XmlNodeType::Whitespace:
    case XmlNodeType::SignificantWhitespace:
        return node.Value();
    default:
        return node.InnerText();
    }
}

struct IdentityConstraintFieldEvaluationResult {
    bool found = false;
    bool multiple = false;
    std::string value;
};

bool MatchesCompiledIdentityStep(
    const auto& step,
    std::string_view localName,
    std::string_view namespaceUri) {
    if (step.localName.empty()) {
        return true;
    }
    const bool localNameMatches = step.localName == "*" || localName == step.localName;
    const bool namespaceMatches = step.localName == "*" && step.namespaceUri.empty()
        ? true
        : namespaceUri == step.namespaceUri;
    return localNameMatches && namespaceMatches;
}

bool MatchesCompiledIdentityPredicate(const auto& step, const XmlNode& node) {
    if (!step.predicateAttributeValue.has_value()) {
        return true;
    }
    if (node.NodeType() != XmlNodeType::Element) {
        return false;
    }

    const auto* element = static_cast<const XmlElement*>(&node);
    const auto attribute = element->GetAttributeNode(step.predicateAttributeLocalName, step.predicateAttributeNamespaceUri);
    return attribute != nullptr && attribute->Value() == *step.predicateAttributeValue;
}

void AppendDescendantIdentityMatches(
    const XmlNode& node,
    const auto& step,
    std::vector<const XmlNode*>& matches) {
    for (const auto& child : node.ChildNodes()) {
        if (child == nullptr || child->NodeType() != XmlNodeType::Element) {
            continue;
        }
        if (MatchesCompiledIdentityStep(step, child->LocalName(), child->NamespaceURI())
            && MatchesCompiledIdentityPredicate(step, *child)) {
            matches.push_back(child.get());
        }
        AppendDescendantIdentityMatches(*child, step, matches);
    }
}

void AppendAncestorIdentityMatches(
    const XmlNode& node,
    const auto& step,
    bool includeSelf,
    std::vector<const XmlNode*>& matches) {
    const XmlNode* current = includeSelf ? std::addressof(node) : node.ParentNode();
    while (current != nullptr) {
        if (current->NodeType() == XmlNodeType::Element
            && MatchesCompiledIdentityStep(step, current->LocalName(), current->NamespaceURI())
            && MatchesCompiledIdentityPredicate(step, *current)) {
            matches.push_back(current);
        }
        current = current->ParentNode();
    }
}

void AppendFollowingSiblingIdentityMatches(
    const XmlNode& node,
    const auto& step,
    std::vector<const XmlNode*>& matches) {
    const XmlNode* parent = node.ParentNode();
    if (parent == nullptr) {
        return;
    }

    bool foundSelf = false;
    for (const auto& sibling : parent->ChildNodes()) {
        if (!foundSelf) {
            foundSelf = sibling.get() == &node;
            continue;
        }
        if (sibling != nullptr
            && sibling->NodeType() == XmlNodeType::Element
            && MatchesCompiledIdentityStep(step, sibling->LocalName(), sibling->NamespaceURI())
            && MatchesCompiledIdentityPredicate(step, *sibling)) {
            matches.push_back(sibling.get());
        }
    }
}

void AppendPrecedingSiblingIdentityMatches(
    const XmlNode& node,
    const auto& step,
    std::vector<const XmlNode*>& matches) {
    const XmlNode* parent = node.ParentNode();
    if (parent == nullptr) {
        return;
    }

    std::vector<const XmlNode*> preceding;
    for (const auto& sibling : parent->ChildNodes()) {
        if (sibling.get() == &node) {
            break;
        }
        if (sibling != nullptr
            && sibling->NodeType() == XmlNodeType::Element
            && MatchesCompiledIdentityStep(step, sibling->LocalName(), sibling->NamespaceURI())
            && MatchesCompiledIdentityPredicate(step, *sibling)) {
            preceding.push_back(sibling.get());
        }
    }

    for (auto index = preceding.size(); index > 0; --index) {
        matches.push_back(preceding[index - 1]);
    }
}

void CollectElementIdentityNodesInDocumentOrder(const XmlNode& node, std::vector<const XmlNode*>& nodes) {
    if (node.NodeType() == XmlNodeType::Element) {
        nodes.push_back(&node);
    }
    for (const auto& child : node.ChildNodes()) {
        if (child == nullptr || child->NodeType() != XmlNodeType::Element) {
            continue;
        }
        CollectElementIdentityNodesInDocumentOrder(*child, nodes);
    }
}

void AppendFollowingIdentityMatches(
    const XmlNode& node,
    const auto& step,
    std::vector<const XmlNode*>& matches) {
    const XmlNode* current = &node;
    while (current != nullptr) {
        const XmlNode* parent = current->ParentNode();
        if (parent == nullptr) {
            break;
        }

        bool foundSelf = false;
        for (const auto& sibling : parent->ChildNodes()) {
            if (!foundSelf) {
                foundSelf = sibling.get() == current;
                continue;
            }
            if (sibling != nullptr
                && sibling->NodeType() == XmlNodeType::Element
                && MatchesCompiledIdentityStep(step, sibling->LocalName(), sibling->NamespaceURI())
                && MatchesCompiledIdentityPredicate(step, *sibling)) {
                matches.push_back(sibling.get());
            }
            if (sibling != nullptr) {
                AppendDescendantIdentityMatches(*sibling, step, matches);
            }
        }
        current = parent;
    }
}

void AppendPrecedingIdentityMatches(
    const XmlNode& node,
    const auto& step,
    std::vector<const XmlNode*>& matches) {
    std::unordered_set<const XmlNode*> ancestors;
    for (const XmlNode* ancestor = node.ParentNode(); ancestor != nullptr; ancestor = ancestor->ParentNode()) {
        ancestors.insert(ancestor);
    }

    const XmlNode* root = &node;
    while (root->ParentNode() != nullptr) {
        root = root->ParentNode();
    }

    std::vector<const XmlNode*> allNodes;
    if (root->NodeType() == XmlNodeType::Element) {
        CollectElementIdentityNodesInDocumentOrder(*root, allNodes);
    } else {
        for (const auto& child : root->ChildNodes()) {
            if (child == nullptr || child->NodeType() != XmlNodeType::Element) {
                continue;
            }
            CollectElementIdentityNodesInDocumentOrder(*child, allNodes);
        }
    }

    std::vector<const XmlNode*> preceding;
    for (const XmlNode* candidate : allNodes) {
        if (candidate == &node) {
            break;
        }
        if (ancestors.find(candidate) == ancestors.end()
            && MatchesCompiledIdentityStep(step, candidate->LocalName(), candidate->NamespaceURI())
            && MatchesCompiledIdentityPredicate(step, *candidate)) {
            preceding.push_back(candidate);
        }
    }

    for (auto index = preceding.size(); index > 0; --index) {
        matches.push_back(preceding[index - 1]);
    }
}

const XmlElement* FindTopLevelSchemaDeclaration(
    const XmlElement& schemaRoot,
    const std::string& schemaNamespace,
    const std::string& declarationLocalName,
    const std::string& declarationName) {
    for (const auto& child : schemaRoot.ChildNodes()) {
        if (child == nullptr || child->NodeType() != XmlNodeType::Element) {
            continue;
        }

        const auto* childElement = static_cast<const XmlElement*>(child.get());
        if (childElement->NamespaceURI() != schemaNamespace || childElement->LocalName() != declarationLocalName) {
            continue;
        }
        if (AttributeValueEqualsInternal(*childElement, "name", declarationName)) {
            return childElement;
        }
    }

    return nullptr;
}

std::string BuildRedefineAliasName(const std::string& declarationName, std::size_t index) {
    return declarationName + "__libxml_redefine_base_" + std::to_string(index);
}

bool ShouldRewriteRedefineReference(
    const XmlElement& context,
    const std::string& qualifiedName,
    const std::string& expectedLocalName,
    const std::string& targetNamespace) {
    if (qualifiedName.empty()) {
        return false;
    }

    const auto separator = qualifiedName.find(':');
    if (separator == std::string::npos) {
        return qualifiedName == expectedLocalName;
    }

    const std::string prefix = qualifiedName.substr(0, separator);
    const std::string localName = qualifiedName.substr(separator + 1);
    return localName == expectedLocalName && context.GetNamespaceOfPrefix(prefix) == targetNamespace;
}

std::string RewriteRedefineQualifiedName(
    const XmlElement& context,
    const std::string& qualifiedName,
    const std::string& expectedLocalName,
    const std::string& aliasName,
    const std::string& targetNamespace) {
    if (!ShouldRewriteRedefineReference(context, qualifiedName, expectedLocalName, targetNamespace)) {
        return qualifiedName;
    }

    const auto separator = qualifiedName.find(':');
    if (separator == std::string::npos) {
        return aliasName;
    }

    return qualifiedName.substr(0, separator + 1) + aliasName;
}

void RewriteRedefineReferences(
    const std::shared_ptr<XmlNode>& node,
    const std::string& schemaNamespace,
    const std::string& targetNamespace,
    const std::string& originalName,
    const std::string& aliasName,
    const std::string& redefineLocalName) {
    if (node == nullptr || node->NodeType() != XmlNodeType::Element) {
        return;
    }

    auto element = std::static_pointer_cast<XmlElement>(node);
    if (element->NamespaceURI() == schemaNamespace) {
        const std::string& localName = element->LocalName();
        std::string_view attributeValue;
        if ((localName == "restriction" || localName == "extension")
            && TryGetAttributeValueViewInternal(*element, "base", attributeValue)
            && !attributeValue.empty()) {
            element->SetAttribute(
                "base",
                RewriteRedefineQualifiedName(*element, std::string(attributeValue), originalName, aliasName, targetNamespace));
        }
        if (localName == redefineLocalName
            && TryGetAttributeValueViewInternal(*element, "ref", attributeValue)
            && !attributeValue.empty()) {
            element->SetAttribute(
                "ref",
                RewriteRedefineQualifiedName(*element, std::string(attributeValue), originalName, aliasName, targetNamespace));
        }
        if (localName == "list"
            && TryGetAttributeValueViewInternal(*element, "itemType", attributeValue)
            && !attributeValue.empty()) {
            element->SetAttribute(
                "itemType",
                RewriteRedefineQualifiedName(*element, std::string(attributeValue), originalName, aliasName, targetNamespace));
        }
        if (localName == "union"
            && TryGetAttributeValueViewInternal(*element, "memberTypes", attributeValue)
            && !attributeValue.empty()) {
            std::istringstream members{std::string(attributeValue)};
            std::string rewrittenMemberTypes;
            std::string memberType;
            bool first = true;
            while (members >> memberType) {
                if (!first) {
                    rewrittenMemberTypes.push_back(' ');
                }
                rewrittenMemberTypes += RewriteRedefineQualifiedName(*element, memberType, originalName, aliasName, targetNamespace);
                first = false;
            }
            element->SetAttribute("memberTypes", rewrittenMemberTypes);
        }
    }

    for (const auto& child : element->ChildNodes()) {
        RewriteRedefineReferences(child, schemaNamespace, targetNamespace, originalName, aliasName, redefineLocalName);
    }
}

void CopySchemaNamespaceBindings(const XmlElement& schemaRoot, const std::shared_ptr<XmlElement>& declarationElement) {
    if (declarationElement == nullptr) {
        return;
    }

    for (const auto& attribute : schemaRoot.Attributes()) {
        if (attribute == nullptr) {
            continue;
        }

        const std::string& attributeName = attribute->Name();
        if (attributeName == "xmlns" || attributeName.rfind("xmlns:", 0) == 0) {
            declarationElement->SetAttribute(attributeName, attribute->Value());
        }
    }
}

std::string BuildSchemaAddXmlPayload(
    const XmlElement& schemaRoot,
    const std::filesystem::path& schemaPath,
    const std::string& schemaNamespace) {
    const std::string targetNamespace = schemaRoot.GetAttribute("targetNamespace");
    std::string payload = "<" + schemaRoot.Name();
    for (const auto& attribute : schemaRoot.Attributes()) {
        if (attribute == nullptr) {
            continue;
        }
        payload.push_back(' ');
        payload += attribute->OuterXml();
    }
    payload.push_back('>');

    for (const auto& child : schemaRoot.ChildNodes()) {
        if (child == nullptr || child->NodeType() != XmlNodeType::Element) {
            continue;
        }

        const auto* childElement = static_cast<const XmlElement*>(child.get());
        if (childElement->NamespaceURI() != schemaNamespace) {
            continue;
        }

        if (childElement->LocalName() == "override") {
            for (const auto& overrideChild : childElement->ChildNodes()) {
                if (overrideChild == nullptr || overrideChild->NodeType() != XmlNodeType::Element) {
                    continue;
                }
                const auto* overrideChildElement = static_cast<const XmlElement*>(overrideChild.get());
                if (overrideChildElement->NamespaceURI() != schemaNamespace) {
                    continue;
                }
                const std::string& overrideLocalName = overrideChildElement->LocalName();
                if (overrideLocalName != "annotation"
                    && overrideLocalName != "element"
                    && overrideLocalName != "attribute"
                    && overrideLocalName != "simpleType"
                    && overrideLocalName != "complexType"
                    && overrideLocalName != "group"
                    && overrideLocalName != "attributeGroup") {
                    throw XmlException("XML Schema xs:override can only contain annotation, element, attribute, simpleType, complexType, group, and attributeGroup declarations");
                }
                payload += overrideChildElement->OuterXml();
            }
            continue;
        }

        if (childElement->LocalName() == "include" || childElement->LocalName() == "import") {
            for (const auto& referenceChild : childElement->ChildNodes()) {
                if (referenceChild == nullptr || referenceChild->NodeType() != XmlNodeType::Element) {
                    continue;
                }
                const auto* referenceChildElement = static_cast<const XmlElement*>(referenceChild.get());
                if (referenceChildElement->NamespaceURI() == schemaNamespace
                    && referenceChildElement->LocalName() != "annotation") {
                    throw XmlException("XML Schema xs:" + childElement->LocalName() + " can only declare annotation children");
                }
            }
        }

        if (childElement->LocalName() == "redefine") {
            const std::string schemaLocation = childElement->GetAttribute("schemaLocation");
            if (schemaLocation.empty()) {
                throw XmlException("XML Schema redefine requires a schemaLocation when loaded via AddFile");
            }

            const std::filesystem::path referencedPath = NormalizeSchemaPath(schemaPath.parent_path() / std::filesystem::path(schemaLocation));
            std::ifstream referencedStream(referencedPath, std::ios::binary);
            if (!referencedStream) {
                throw XmlException("Failed to open referenced XML schema file: " + referencedPath.generic_string());
            }

            std::ostringstream referencedBuffer;
            referencedBuffer << referencedStream.rdbuf();
            const auto referencedDocument = XmlDocument::Parse(referencedBuffer.str());
            const auto referencedRoot = referencedDocument->DocumentElement();
            if (referencedRoot == nullptr || referencedRoot->LocalName() != "schema" || referencedRoot->NamespaceURI() != schemaNamespace) {
                throw XmlException("Referenced XML schema file is not a valid XML Schema document: " + referencedPath.generic_string());
            }

            std::size_t redefineIndex = 0;
            for (const auto& redefineChild : childElement->ChildNodes()) {
                if (redefineChild == nullptr || redefineChild->NodeType() != XmlNodeType::Element) {
                    continue;
                }

                const auto* redefineChildElement = static_cast<const XmlElement*>(redefineChild.get());
                if (redefineChildElement->NamespaceURI() != schemaNamespace) {
                    continue;
                }

                const std::string& redefineLocalName = redefineChildElement->LocalName();
                if (redefineLocalName != "simpleType"
                    && redefineLocalName != "complexType"
                    && redefineLocalName != "group"
                    && redefineLocalName != "attributeGroup") {
                    throw XmlException("XML Schema xs:redefine currently supports only top-level simpleType, complexType, group, and attributeGroup declarations");
                }

                const std::string declarationName = redefineChildElement->GetAttribute("name");
                if (declarationName.empty()) {
                    throw XmlException("XML Schema xs:redefine declarations require a name");
                }

                const auto* originalDeclaration = FindTopLevelSchemaDeclaration(
                    *referencedRoot,
                    schemaNamespace,
                    redefineLocalName,
                    declarationName);
                if (originalDeclaration == nullptr) {
                    throw XmlException(
                        "XML Schema xs:redefine could not find the referenced " + redefineLocalName + " '" + declarationName + "'");
                }

                const std::string aliasName = BuildRedefineAliasName(declarationName, redefineIndex++);
                auto originalClone = std::static_pointer_cast<XmlElement>(originalDeclaration->CloneNode(true));
                CopySchemaNamespaceBindings(schemaRoot, originalClone);
                originalClone->SetAttribute("name", aliasName);
                payload += originalClone->OuterXml();

                auto redefineClone = std::static_pointer_cast<XmlElement>(redefineChildElement->CloneNode(true));
                CopySchemaNamespaceBindings(schemaRoot, redefineClone);
                RewriteRedefineReferences(redefineClone, schemaNamespace, targetNamespace, declarationName, aliasName, redefineLocalName);
                payload += redefineClone->OuterXml();
            }
            continue;
        }

        payload += childElement->OuterXml();
    }

    payload += "</" + schemaRoot.Name() + ">";
    return payload;
}

}  // namespace

}  // namespace System::Xml
