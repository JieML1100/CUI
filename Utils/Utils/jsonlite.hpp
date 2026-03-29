#pragma once

#include <cassert>
#include <cctype>
#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iomanip>
#include <iosfwd>
#include <iostream>
#include <istream>
#include <memory>
#include <new>
#include <optional>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#if defined(__GNUC__) || defined(__clang__)
#define JSONLITE_LIKELY(x) __builtin_expect(!!(x), 1)
#define JSONLITE_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define JSONLITE_LIKELY(x) (x)
#define JSONLITE_UNLIKELY(x) (x)
#endif

namespace jsonlite
{

class JsonException : public std::runtime_error {
public:
    JsonException() noexcept 
        : std::runtime_error("JSON error"), line_(0), column_(0), bytePosition_(0) {}
    explicit JsonException(const char* message) 
        : std::runtime_error(message), line_(0), column_(0), bytePosition_(0) {}
    JsonException(const char* message, size_t line, size_t column, size_t bytePosition) 
        : std::runtime_error(message), line_(line), column_(column), bytePosition_(bytePosition) {}
    JsonException(std::string_view message, size_t line, size_t column, size_t bytePosition)
        : std::runtime_error(std::string(message)), line_(line), column_(column), bytePosition_(bytePosition) {}
    size_t Line() const noexcept { return line_; }
    size_t Column() const noexcept { return column_; }
    size_t BytePosition() const noexcept { return bytePosition_; }
private:
    size_t line_ = 0, column_ = 0, bytePosition_ = 0;
};

class JsonSyntaxException : public JsonException {
public:
    using JsonException::JsonException;
    const char* what() const noexcept override { return std::runtime_error::what(); }
};

class JsonOverflowException : public JsonException {
public:
    using JsonException::JsonException;
};

class JsonInvalidOperationException : public JsonException {
public:
    using JsonException::JsonException;
    explicit JsonInvalidOperationException(const char* message) : JsonException(message) {}
};

class JsonKeyNotFoundException : public JsonException {
public:
    using JsonException::JsonException;
    explicit JsonKeyNotFoundException(std::string_view key)
        : JsonException(("Key not found: " + std::string(key)).c_str()) {}
};

class JsonIndexOutOfRangeException : public JsonException {
public:
    JsonIndexOutOfRangeException(size_t index, size_t size)
        : JsonException(("Index out of range: " + std::to_string(index) + " (size: " + std::to_string(size) + ")").c_str()),
          index_(index), size_(size) {}
    size_t Index() const noexcept { return index_; }
    size_t Size() const noexcept { return size_; }
private:
    size_t index_ = 0, size_ = 0;
};

class JsonInvalidTypeException : public JsonException {
public:
    JsonInvalidTypeException(const char* expected, const char* actual)
        : JsonException(("Expected " + std::string(expected) + " but got " + std::string(actual)).c_str()),
          expected_(expected), actual_(actual) {}
    const char* ExpectedType() const noexcept { return expected_; }
    const char* ActualType() const noexcept { return actual_; }
private:
    const char* expected_ = nullptr;
    const char* actual_ = nullptr;
};

class JsonParsingException : public JsonException {
public:
    JsonParsingException(size_t line, size_t column, size_t bytePosition, std::string_view message)
        : JsonException(message, line, column, bytePosition) {}
};

template<typename T>
using JsonUniquePtr = std::unique_ptr<T>;

template<typename T, typename... Args>
inline JsonUniquePtr<T> MakeJson(Args&&... args) {
    return std::make_unique<T>(std::forward<Args>(args)...);
}

class JsonValue;
class JsonObject;
class JsonArray;
class JsonString;
class JsonNumber;
class JsonBoolean;
class JsonNull;

enum class JsonValueType { Object, Array, String, Number, Boolean, Null };

using string = std::string;
using string_view = std::string_view;

class JsonValue {
public:
    virtual ~JsonValue() = default;
    [[nodiscard]] virtual JsonValueType GetType() const noexcept = 0;
    [[nodiscard]] virtual string_view GetString() const { return {}; }
    [[nodiscard]] virtual int64_t GetInt64() const { return 0; }
    [[nodiscard]] virtual double GetDouble() const { return 0.0; }
    [[nodiscard]] virtual bool GetBoolean() const { return false; }
    [[nodiscard]] virtual bool IsObject() const noexcept { return false; }
    [[nodiscard]] virtual bool IsArray() const noexcept { return false; }
    [[nodiscard]] virtual bool IsString() const noexcept { return false; }
    [[nodiscard]] virtual bool IsNumber() const noexcept { return false; }
    [[nodiscard]] virtual bool IsBoolean() const noexcept { return false; }
    [[nodiscard]] virtual bool IsNull() const noexcept { return false; }
    [[nodiscard]] virtual bool IsIntegral() const noexcept { return false; }
    [[nodiscard]] virtual bool IsFloatingPoint() const noexcept { return false; }
    [[nodiscard]] virtual JsonObject& AsObject() { throw JsonInvalidOperationException("Not an object"); }
    [[nodiscard]] virtual JsonArray& AsArray() { throw JsonInvalidOperationException("Not an array"); }
    [[nodiscard]] virtual JsonString& AsString() { throw JsonInvalidOperationException("Not a string"); }
    [[nodiscard]] virtual JsonNumber& AsNumber() { throw JsonInvalidOperationException("Not a number"); }
    [[nodiscard]] virtual JsonBoolean& AsBoolean() { throw JsonInvalidOperationException("Not a boolean"); }
    [[nodiscard]] virtual JsonNull& AsNull() { throw JsonInvalidOperationException("Not null"); }
    [[nodiscard]] virtual const JsonObject& AsObject() const { throw JsonInvalidOperationException("Not an object"); }
    [[nodiscard]] virtual const JsonArray& AsArray() const { throw JsonInvalidOperationException("Not an array"); }
    [[nodiscard]] virtual const JsonString& AsString() const { throw JsonInvalidOperationException("Not a string"); }
    [[nodiscard]] virtual const JsonNumber& AsNumber() const { throw JsonInvalidOperationException("Not a number"); }
    [[nodiscard]] virtual const JsonBoolean& AsBoolean() const { throw JsonInvalidOperationException("Not a boolean"); }
    [[nodiscard]] virtual const JsonNull& AsNull() const { throw JsonInvalidOperationException("Not null"); }
    [[nodiscard]] virtual bool Equals(const JsonValue* other) const noexcept { return false; }
    [[nodiscard]] virtual size_t GetHashCode() const noexcept { return 0; }
    [[nodiscard]] virtual string ToString() const { return {}; }
    virtual void WriteTo(std::ostream& os) const = 0;
    [[nodiscard]] virtual JsonValue* Clone() const = 0;
protected:
    JsonValue() = default;
    JsonValue(const JsonValue&) = default;
    JsonValue(JsonValue&&) = default;
    JsonValue& operator=(const JsonValue&) = default;
    JsonValue& operator=(JsonValue&&) = default;
};

[[nodiscard]] inline constexpr const char* JsonTypeName(JsonValueType type) noexcept {
    switch (type) {
        case JsonValueType::Object: return "Object";
        case JsonValueType::Array: return "Array";
        case JsonValueType::String: return "String";
        case JsonValueType::Number: return "Number";
        case JsonValueType::Boolean: return "Boolean";
        case JsonValueType::Null: return "Null";
    }
    return "Unknown";
}

class JsonString final : public JsonValue {
public:
    explicit JsonString() : value_() {}
    explicit JsonString(string_view value) : value_(value) {}
    explicit JsonString(const char* value) : value_(value) {}
    JsonString(const JsonString& other) = default;
    JsonString(JsonString&& other) noexcept = default;
    JsonString& operator=(const JsonString& other) = default;
    JsonString& operator=(JsonString&& other) noexcept = default;

    [[nodiscard]] JsonValueType GetType() const noexcept override { return JsonValueType::String; }
    [[nodiscard]] string_view GetString() const noexcept override { return value_; }
    [[nodiscard]] const string& GetValue() const noexcept { return value_; }
    [[nodiscard]] string& GetValue() noexcept { return value_; }
    [[nodiscard]] bool IsString() const noexcept override { return true; }
    [[nodiscard]] JsonString& AsString() override { return *this; }
    [[nodiscard]] const JsonString& AsString() const override { return *this; }
    [[nodiscard]] bool Equals(const JsonValue* other) const noexcept override {
        if (!other || other->GetType() != JsonValueType::String) return false;
        return value_ == other->GetString();
    }
    [[nodiscard]] size_t GetHashCode() const noexcept override { return std::hash<string_view>{}(value_); }
    void WriteTo(std::ostream& os) const override;
    [[nodiscard]] JsonValue* Clone() const override { return new JsonString(value_); }

    static inline string Escape(string_view input) {
        string result;
        result.reserve(input.size() * 2);
        for (char c : input) {
            switch (c) {
                case '"':  result += "\\\""; break;
                case '\\': result += "\\\\"; break;
                case '\b': result += "\\b"; break;
                case '\f': result += "\\f"; break;
                case '\n': result += "\\n"; break;
                case '\r': result += "\\r"; break;
                case '\t': result += "\\t"; break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20) {
                        result += "\\u00";
                        result += "0123456789ABCDEF"[((static_cast<unsigned char>(c) >> 4) & 0xF)];
                        result += "0123456789ABCDEF"[static_cast<unsigned char>(c) & 0xF];
                    } else {
                        result += c;
                    }
                    break;
            }
        }
        return result;
    }

    static inline string Unescape(string_view input) {
        string result;
        result.reserve(input.size());
        size_t i = 0;
        const size_t len = input.size();
        while (i < len) {
            char c = input[i++];
            if (c != '\\' || i >= len) {
                result += c;
                continue;
            }
            char e = input[i++];
            switch (e) {
                case '"':  result += '"'; break;
                case '\\': result += '\\'; break;
                case '/':  result += '/'; break;
                case 'b':  result += '\b'; break;
                case 'f':  result += '\f'; break;
                case 'n':  result += '\n'; break;
                case 'r':  result += '\r'; break;
                case 't':  result += '\t'; break;
                case 'u': {
                    if (i + 4 > len) { result += "\\u"; break; }
                    auto hex4 = [&](size_t idx) -> uint32_t {
                        char h = input[i + idx];
                        if (h >= '0' && h <= '9') return static_cast<uint32_t>(h - '0');
                        if (h >= 'A' && h <= 'F') return static_cast<uint32_t>(h - 'A' + 10);
                        if (h >= 'a' && h <= 'f') return static_cast<uint32_t>(h - 'a' + 10);
                        return 0;
                    };
                    uint32_t cp = (hex4(0) << 12) | (hex4(1) << 8) | (hex4(2) << 4) | hex4(3);
                    i += 4;
                    if (cp >= 0xD800 && cp <= 0xDBFF) {
                        if (i + 6 <= len && input[i] == '\\' && input[i + 1] == 'u') {
                            uint32_t low = (hex4(2) << 12) | (hex4(3) << 8) | (hex4(4) << 4) | hex4(5);
                            i += 6;
                            if (low >= 0xDC00 && low <= 0xDFFF) {
                                cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                            } else {
                                throw JsonInvalidOperationException("Invalid Unicode surrogate pair: missing low surrogate");
                            }
                        } else {
                            throw JsonInvalidOperationException("Invalid Unicode surrogate pair: missing low surrogate");
                        }
                    } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                        throw JsonInvalidOperationException("Invalid Unicode surrogate pair: lone low surrogate");
                    }
                    if (cp <= 0x7F) {
                        result += static_cast<char>(cp);
                    } else if (cp <= 0x7FF) {
                        result += static_cast<char>(0xC0 | (cp >> 6));
                        result += static_cast<char>(0x80 | (cp & 0x3F));
                    } else if (cp <= 0xFFFF) {
                        result += static_cast<char>(0xE0 | (cp >> 12));
                        result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                        result += static_cast<char>(0x80 | (cp & 0x3F));
                    } else {
                        result += static_cast<char>(0xF0 | (cp >> 18));
                        result += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
                        result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                        result += static_cast<char>(0x80 | (cp & 0x3F));
                    }
                    break;
                }
                default: throw JsonInvalidOperationException(("Invalid escape sequence: \\" + std::string(1, e)).c_str());
            }
        }
        return result;
    }

private:
    string value_;
};

inline void JsonString::WriteTo(std::ostream& os) const {
    os << '"' << Escape(value_) << '"';
}

class JsonNumber final : public JsonValue {
public:
    JsonNumber() : value_(int64_t(0)), isInteger_(true) {}
    explicit JsonNumber(int64_t value) : value_(value), isInteger_(true) {}
    explicit JsonNumber(double value) : value_(value), isInteger_(false) {}
    explicit JsonNumber(string_view rawValue) : rawValue_(rawValue) { Parse(rawValue); }
    JsonNumber(const JsonNumber& other) = default;
    JsonNumber(JsonNumber&& other) noexcept = default;
    JsonNumber& operator=(const JsonNumber& other) = default;
    JsonNumber& operator=(JsonNumber&& other) noexcept = default;

    inline bool Parse(string_view input) noexcept {
        rawValue_ = std::string(input);
        if (input.empty()) { value_ = int64_t(0); isInteger_ = true; return false; }
        size_t i = 0;
        if (input[0] == '-') { i = 1; }
        if (i >= input.size()) { value_ = int64_t(0); isInteger_ = true; return false; }
        if (input[i] == '0') {
            ++i;
            if (i < input.size() && input[i] >= '0' && input[i] <= '9') { value_ = int64_t(0); isInteger_ = true; return false; }
            if (i < input.size() && input[i] != '.' && input[i] != 'e' && input[i] != 'E') { value_ = int64_t(0); isInteger_ = true; return false; }
        } else if (input[i] >= '1' && input[i] <= '9') {
            while (i < input.size() && input[i] >= '0' && input[i] <= '9') { ++i; }
        } else { value_ = int64_t(0); isInteger_ = true; return false; }
        bool hasDecimal = false, hasExponent = false;
        if (i < input.size() && input[i] == '.') {
            hasDecimal = true; ++i;
            if (i >= input.size() || input[i] < '0' || input[i] > '9') { value_ = 0.0; isInteger_ = false; return false; }
            while (i < input.size() && input[i] >= '0' && input[i] <= '9') { ++i; }
        }
        if (i < input.size() && (input[i] == 'e' || input[i] == 'E')) {
            hasExponent = true; ++i;
            if (i < input.size() && (input[i] == '+' || input[i] == '-')) { ++i; }
            if (i >= input.size() || input[i] < '0' || input[i] > '9') { value_ = 0.0; isInteger_ = false; return false; }
            while (i < input.size() && input[i] >= '0' && input[i] <= '9') { ++i; }
        }
        isInteger_ = !hasDecimal && !hasExponent;
        if (isInteger_) {
            int64_t result = 0;
            string_view numStr = input.substr(input[0] == '-' ? 1 : 0);
            auto [ptr, ec] = std::from_chars(numStr.data(), numStr.data() + numStr.size(), result);
            if (ec == std::errc{} && ptr == numStr.data() + numStr.size()) { value_ = input[0] == '-' ? -result : result; }
            else { value_ = int64_t(0); return false; }
        } else {
            double result = 0.0;
            char* endptr = nullptr;
            result = std::strtod(input.data(), &endptr);
            if (endptr != input.data() && endptr == input.data() + input.size()) { value_ = result; }
            else { value_ = 0.0; return false; }
            isInteger_ = false;
        }
        return true;
    }

    [[nodiscard]] JsonValueType GetType() const noexcept override { return JsonValueType::Number; }
    [[nodiscard]] int64_t GetInt64() const override {
        if (std::holds_alternative<int64_t>(value_)) return std::get<int64_t>(value_);
        return static_cast<int64_t>(std::get<double>(value_));
    }
    [[nodiscard]] double GetDouble() const override {
        if (std::holds_alternative<int64_t>(value_)) return static_cast<double>(std::get<int64_t>(value_));
        return std::get<double>(value_);
    }
    [[nodiscard]] bool IsNumber() const noexcept override { return true; }
    [[nodiscard]] bool IsIntegral() const noexcept override { return isInteger_; }
    [[nodiscard]] bool IsFloatingPoint() const noexcept override { return !isInteger_; }
    [[nodiscard]] JsonNumber& AsNumber() override { return *this; }
    [[nodiscard]] const JsonNumber& AsNumber() const override { return *this; }
    [[nodiscard]] int64_t GetInt64Value() const { return GetInt64(); }
    [[nodiscard]] double GetDoubleValue() const { return GetDouble(); }
    [[nodiscard]] bool IsInteger() const noexcept { return isInteger_; }
    [[nodiscard]] bool Equals(const JsonValue* other) const noexcept override {
        if (!other || other->GetType() != JsonValueType::Number) return false;
        const auto& otherNum = other->AsNumber();
        if (isInteger_ && otherNum.isInteger_) return GetInt64() == otherNum.GetInt64();
        return GetDouble() == otherNum.GetDouble();
    }
    [[nodiscard]] size_t GetHashCode() const noexcept override {
        return isInteger_ ? std::hash<int64_t>{}(GetInt64()) : std::hash<double>{}(GetDouble());
    }
    void WriteTo(std::ostream& os) const override;
    [[nodiscard]] string ToString() const {
        if (isInteger_) return std::to_string(GetInt64());
        std::ostringstream oss; oss << std::setprecision(17) << GetDouble(); return oss.str();
    }
    void SetInt64(int64_t val) { value_ = val; isInteger_ = true; rawValue_.clear(); }
    void SetDouble(double val) { value_ = val; isInteger_ = false; rawValue_.clear(); }
    [[nodiscard]] JsonValue* Clone() const override {
        return isInteger_ ? new JsonNumber(GetInt64()) : new JsonNumber(GetDouble());
    }

private:
    std::variant<int64_t, double> value_;
    bool isInteger_ = true;
    string rawValue_;
};

inline void JsonNumber::WriteTo(std::ostream& os) const {
    if (isInteger_) os << GetInt64();
    else { std::ostringstream oss; oss << std::setprecision(17) << GetDouble(); os << oss.str(); }
}

class JsonBoolean final : public JsonValue {
public:
    explicit JsonBoolean() : value_(false) {}
    explicit JsonBoolean(bool value) : value_(value) {}
    JsonBoolean(const JsonBoolean& other) = default;
    JsonBoolean(JsonBoolean&& other) noexcept = default;
    JsonBoolean& operator=(const JsonBoolean& other) = default;
    JsonBoolean& operator=(JsonBoolean&& other) noexcept = default;

    [[nodiscard]] JsonValueType GetType() const noexcept override { return JsonValueType::Boolean; }
    [[nodiscard]] bool GetBoolean() const noexcept override { return value_; }
    [[nodiscard]] bool GetValue() const noexcept { return value_; }
    [[nodiscard]] bool IsBoolean() const noexcept override { return true; }
    [[nodiscard]] JsonBoolean& AsBoolean() override { return *this; }
    [[nodiscard]] const JsonBoolean& AsBoolean() const override { return *this; }
    [[nodiscard]] bool Equals(const JsonValue* other) const noexcept override {
        if (!other || other->GetType() != JsonValueType::Boolean) return false;
        return value_ == other->GetBoolean();
    }
    [[nodiscard]] size_t GetHashCode() const noexcept override { return std::hash<bool>{}(value_); }
    void WriteTo(std::ostream& os) const override { os << (value_ ? "true" : "false"); }
    [[nodiscard]] JsonValue* Clone() const override { return new JsonBoolean(value_); }

private:
    bool value_;
};

class JsonNull final : public JsonValue {
public:
    JsonNull() = default;
    JsonNull(const JsonNull& other) = default;
    JsonNull(JsonNull&& other) noexcept = default;
    JsonNull& operator=(const JsonNull& other) = default;
    JsonNull& operator=(JsonNull&& other) noexcept = default;

    [[nodiscard]] JsonValueType GetType() const noexcept override { return JsonValueType::Null; }
    [[nodiscard]] bool IsNull() const noexcept override { return true; }
    [[nodiscard]] JsonNull& AsNull() override { return *this; }
    [[nodiscard]] const JsonNull& AsNull() const override { return *this; }
    [[nodiscard]] bool Equals(const JsonValue* other) const noexcept override {
        return other && other->GetType() == JsonValueType::Null;
    }
    [[nodiscard]] size_t GetHashCode() const noexcept override { return 0; }
    void WriteTo(std::ostream& os) const override { os << "null"; }
    [[nodiscard]] JsonValue* Clone() const override { return new JsonNull(); }
};

class JsonObject final : public JsonValue {
public:
    struct KeyValuePair {
        string key;
        JsonUniquePtr<JsonValue> value;
        KeyValuePair() : key(), value(nullptr) {}
        KeyValuePair(string k, JsonUniquePtr<JsonValue> v) : key(std::move(k)), value(std::move(v)) {}
        KeyValuePair(KeyValuePair&&) = default;
        KeyValuePair& operator=(KeyValuePair&&) = default;
        KeyValuePair(const KeyValuePair&) = delete;
        KeyValuePair& operator=(const KeyValuePair&) = delete;
    };

    JsonObject() : map_(), caseInsensitive_(false), indexDirty_(false) {}
    JsonObject(const JsonObject& other) : map_(), caseInsensitive_(other.caseInsensitive_), indexDirty_(false) {
        for (const auto& [k, v] : other.map_) Add(k, JsonUniquePtr<JsonValue>(v->Clone()));
    }
    JsonObject(JsonObject&& other) noexcept 
        : map_(std::move(other.map_)), caseInsensitive_(other.caseInsensitive_), 
          indexDirty_(other.indexDirty_), index_(std::move(other.index_)) {}
    JsonObject& operator=(const JsonObject& other) {
        if (this != &other) { Clear(); caseInsensitive_ = other.caseInsensitive_;
            for (const auto& [k, v] : other.map_) Add(k, JsonUniquePtr<JsonValue>(v->Clone())); }
        return *this;
    }
    JsonObject& operator=(JsonObject&& other) noexcept {
        if (this != &other) { map_ = std::move(other.map_); caseInsensitive_ = other.caseInsensitive_;
            indexDirty_ = other.indexDirty_; index_ = std::move(other.index_); }
        return *this;
    }
    void SetCaseInsensitive(bool value) { caseInsensitive_ = value; RebuildIndex(); }
    bool GetCaseInsensitive() const { return caseInsensitive_; }

    [[nodiscard]] JsonValueType GetType() const noexcept override { return JsonValueType::Object; }
    [[nodiscard]] bool IsObject() const noexcept override { return true; }
    [[nodiscard]] JsonObject& AsObject() override { return *this; }
    [[nodiscard]] const JsonObject& AsObject() const override { return *this; }

    [[nodiscard]] JsonValue& operator[](string_view key) {
        auto it = Find(key);
        if (it != map_.end()) return *it->value;
        auto newValue = MakeJson<JsonNull>();
        auto& ref = *newValue;
        map_.push_back(KeyValuePair(std::string(key), std::move(newValue)));
        indexDirty_ = true;
        return ref;
    }
    [[nodiscard]] JsonValue* TryGet(string_view key) noexcept {
        RebuildIndex();
        auto it = index_.find(std::string(key));
        return it != index_.end() ? it->second->value.get() : nullptr;
    }
    [[nodiscard]] const JsonValue* TryGet(string_view key) const noexcept {
        return const_cast<JsonObject*>(this)->TryGet(key);
    }
    [[nodiscard]] JsonValue& GetOrAdd(string_view key, JsonUniquePtr<JsonValue> defaultValue) {
        auto it = Find(key);
        if (it != map_.end()) return *it->value;
        auto& ref = *defaultValue;
        map_.push_back(KeyValuePair(std::string(key), std::move(defaultValue)));
        indexDirty_ = true;
        return ref;
    }
    [[nodiscard]] const JsonValue* Get(string_view key) const noexcept { return TryGet(key); }
    [[nodiscard]] JsonValue* Get(string_view key) noexcept { return TryGet(key); }
    [[nodiscard]] bool Contains(string_view key) const noexcept { return TryGet(key) != nullptr; }
    void Add(string key, JsonUniquePtr<JsonValue> value) {
        auto it = Find(key);
        if (it != map_.end()) it->value = std::move(value);
        else { map_.push_back(KeyValuePair(std::move(key), std::move(value))); indexDirty_ = true; }
    }
    void Remove(string_view key) {
        auto it = Find(key);
        if (it != map_.end()) { map_.erase(it); indexDirty_ = true; }
    }
    void Clear() noexcept { map_.clear(); index_.clear(); indexDirty_ = false; }
    [[nodiscard]] size_t Count() const noexcept { return map_.size(); }
    [[nodiscard]] bool Empty() const noexcept { return map_.empty(); }
    [[nodiscard]] size_t GetHashCode() const noexcept override {
        size_t hash = 0;
        for (const auto& [k, v] : map_) { hash ^= std::hash<string>{}(k); hash ^= v->GetHashCode() << 1; }
        return hash;
    }
    [[nodiscard]] bool Equals(const JsonValue* other) const noexcept override {
        if (!other || other->GetType() != JsonValueType::Object) return false;
        const auto& otherObj = other->AsObject();
        if (map_.size() != otherObj.map_.size()) return false;
        for (const auto& [k, v] : map_) {
            auto it = otherObj.Find(k);
            if (it == otherObj.map_.end()) return false;
            if (!v->Equals(it->value.get())) return false;
        }
        return true;
    }
    void WriteTo(std::ostream& os) const override;
    void Reserve(size_t capacity) { map_.reserve(capacity); }

    struct const_iterator {
        using iterator_category = std::random_access_iterator_tag;
        using value_type = KeyValuePair;
        using difference_type = std::ptrdiff_t;
        using pointer = const KeyValuePair*;
        using reference = const KeyValuePair&;
        const_iterator() : obj_(nullptr), index_(0) {}
        const_iterator(const JsonObject* obj, size_t idx) : obj_(obj), index_(idx) {}
        reference operator*() const { return obj_->map_[index_]; }
        pointer operator->() const { return &obj_->map_[index_]; }
        const_iterator& operator++() { ++index_; return *this; }
        const_iterator operator++(int) { const_iterator tmp = *this; ++index_; return tmp; }
        const_iterator& operator--() { --index_; return *this; }
        const_iterator operator--(int) { const_iterator tmp = *this; --index_; return tmp; }
        const_iterator& operator+=(difference_type n) { index_ += n; return *this; }
        const_iterator& operator-=(difference_type n) { index_ -= n; return *this; }
        const_iterator operator+(difference_type n) const { return const_iterator(obj_, index_ + n); }
        const_iterator operator-(difference_type n) const { return const_iterator(obj_, index_ - n); }
        difference_type operator-(const const_iterator& other) const { return index_ - other.index_; }
        reference operator[](difference_type n) const { return obj_->map_[index_ + n]; }
        bool operator==(const const_iterator& other) const { return index_ == other.index_; }
        bool operator!=(const const_iterator& other) const { return index_ != other.index_; }
        bool operator<(const const_iterator& other) const { return index_ < other.index_; }
        bool operator>(const const_iterator& other) const { return index_ > other.index_; }
        bool operator<=(const const_iterator& other) const { return index_ <= other.index_; }
        bool operator>=(const const_iterator& other) const { return index_ >= other.index_; }
    private:
        const JsonObject* obj_;
        size_t index_;
    };
    struct iterator {
        using iterator_category = std::random_access_iterator_tag;
        using value_type = KeyValuePair;
        using difference_type = std::ptrdiff_t;
        using pointer = KeyValuePair*;
        using reference = KeyValuePair&;
        iterator() : obj_(nullptr), index_(0) {}
        iterator(JsonObject* obj, size_t idx) : obj_(obj), index_(idx) {}
        reference operator*() const { return obj_->map_[index_]; }
        pointer operator->() const { return &obj_->map_[index_]; }
        iterator& operator++() { ++index_; return *this; }
        iterator operator++(int) { iterator tmp = *this; ++index_; return tmp; }
        iterator& operator--() { --index_; return *this; }
        iterator operator--(int) { iterator tmp = *this; --index_; return tmp; }
        iterator& operator+=(difference_type n) { index_ += n; return *this; }
        iterator& operator-=(difference_type n) { index_ -= n; return *this; }
        iterator operator+(difference_type n) const { return iterator(obj_, index_ + n); }
        iterator operator-(difference_type n) const { return iterator(obj_, index_ - n); }
        difference_type operator-(const iterator& other) const { return index_ - other.index_; }
        reference operator[](difference_type n) const { return obj_->map_[index_ + n]; }
        bool operator==(const iterator& other) const { return index_ == other.index_; }
        bool operator!=(const iterator& other) const { return index_ != other.index_; }
        bool operator<(const iterator& other) const { return index_ < other.index_; }
        bool operator>(const iterator& other) const { return index_ > other.index_; }
        bool operator<=(const iterator& other) const { return index_ <= other.index_; }
        bool operator>=(const iterator& other) const { return index_ >= other.index_; }
    private:
        JsonObject* obj_;
        size_t index_;
    };

    iterator begin() { return iterator(this, 0); }
    iterator end() { return iterator(this, map_.size()); }
    const_iterator begin() const { return const_iterator(this, 0); }
    const_iterator end() const { return const_iterator(this, map_.size()); }
    const_iterator cbegin() const { return const_iterator(this, 0); }
    const_iterator cend() const { return const_iterator(this, map_.size()); }
    [[nodiscard]] JsonValue* Clone() const override { return new JsonObject(*this); }

private:
    using MapType = std::vector<KeyValuePair>;
    inline typename MapType::iterator Find(string_view key) {
        if (caseInsensitive_) {
            return std::find_if(map_.begin(), map_.end(), [key](const KeyValuePair& kv) {
                if (kv.key.size() != key.size()) return false;
                for (size_t i = 0; i < key.size(); ++i)
                    if (std::tolower(static_cast<unsigned char>(kv.key[i])) != std::tolower(static_cast<unsigned char>(key[i])))
                        return false;
                return true;
            });
        }
        return std::find_if(map_.begin(), map_.end(), [key](const KeyValuePair& kv) { return kv.key == key; });
    }
    inline typename MapType::const_iterator Find(string_view key) const {
        return const_cast<JsonObject*>(this)->Find(key);
    }
    inline void RebuildIndex() const {
        if (!indexDirty_) return;
        index_.clear();
        index_.reserve(map_.size());
        for (const auto& kv : map_) index_.emplace(kv.key, &kv);
        indexDirty_ = false;
    }

    MapType map_;
    bool caseInsensitive_;
    mutable bool indexDirty_;
    mutable std::unordered_map<string, const KeyValuePair*> index_;
};

inline void JsonObject::WriteTo(std::ostream& os) const {
    os << '{';
    bool first = true;
    for (const auto& [k, v] : map_) {
        if (!first) os << ',';
        os << '"' << JsonString::Escape(k) << "\":";
        v->WriteTo(os);
        first = false;
    }
    os << '}';
}

class JsonArray final : public JsonValue {
public:
    JsonArray() : vector_() {}
    JsonArray(std::initializer_list<JsonValue*> init) : vector_() {
        for (auto* v : init) vector_.push_back(JsonUniquePtr<JsonValue>(v->Clone()));
    }
    JsonArray(const JsonArray& other) : vector_() {
        vector_.reserve(other.vector_.size());
        for (const auto& item : other.vector_) vector_.push_back(JsonUniquePtr<JsonValue>(item->Clone()));
    }
    JsonArray(JsonArray&& other) noexcept : vector_(std::move(other.vector_)) {}
    JsonArray& operator=(const JsonArray& other) {
        if (this != &other) { Clear(); vector_.reserve(other.vector_.size());
            for (const auto& item : other.vector_) vector_.push_back(JsonUniquePtr<JsonValue>(item->Clone())); }
        return *this;
    }
    JsonArray& operator=(JsonArray&& other) noexcept {
        if (this != &other) vector_ = std::move(other.vector_);
        return *this;
    }

    [[nodiscard]] JsonValueType GetType() const noexcept override { return JsonValueType::Array; }
    [[nodiscard]] bool IsArray() const noexcept override { return true; }
    [[nodiscard]] JsonArray& AsArray() override { return *this; }
    [[nodiscard]] const JsonArray& AsArray() const override { return *this; }

    [[nodiscard]] JsonValue& operator[](size_t index) {
        if (JSONLITE_UNLIKELY(index >= vector_.size())) throw JsonIndexOutOfRangeException(index, vector_.size());
        return *vector_[index];
    }
    [[nodiscard]] const JsonValue& operator[](size_t index) const {
        if (JSONLITE_UNLIKELY(index >= vector_.size())) throw JsonIndexOutOfRangeException(index, vector_.size());
        return *vector_[index];
    }
    [[nodiscard]] JsonValue* GetAt(size_t index) const noexcept {
        if (index >= vector_.size()) return nullptr;
        return vector_[index].get();
    }
    void Add(JsonUniquePtr<JsonValue> value) { vector_.push_back(std::move(value)); }
    void Add(JsonValue* value) { vector_.push_back(JsonUniquePtr<JsonValue>(value)); }
    void Insert(size_t index, JsonUniquePtr<JsonValue> value) {
        if (JSONLITE_UNLIKELY(index > vector_.size())) throw JsonIndexOutOfRangeException(index, vector_.size());
        vector_.insert(vector_.begin() + index, std::move(value));
    }
    void RemoveAt(size_t index) {
        if (JSONLITE_UNLIKELY(index >= vector_.size())) throw JsonIndexOutOfRangeException(index, vector_.size());
        vector_.erase(vector_.begin() + index);
    }
    void Clear() noexcept { vector_.clear(); }
    void Reserve(size_t capacity) { vector_.reserve(capacity); }
    [[nodiscard]] size_t Count() const noexcept { return vector_.size(); }
    [[nodiscard]] bool Empty() const noexcept { return vector_.empty(); }
    [[nodiscard]] size_t GetHashCode() const noexcept override {
        size_t hash = 0;
        for (const auto& item : vector_) hash ^= item->GetHashCode() << 1;
        return hash;
    }
    [[nodiscard]] bool Equals(const JsonValue* other) const noexcept override {
        if (!other || other->GetType() != JsonValueType::Array) return false;
        const auto& otherArr = other->AsArray();
        if (vector_.size() != otherArr.vector_.size()) return false;
        for (size_t i = 0; i < vector_.size(); ++i)
            if (!vector_[i]->Equals(otherArr.vector_[i].get())) return false;
        return true;
    }
    void WriteTo(std::ostream& os) const override;

    struct const_iterator {
        using iterator_category = std::random_access_iterator_tag;
        using value_type = const JsonUniquePtr<JsonValue>;
        using difference_type = std::ptrdiff_t;
        using pointer = const JsonUniquePtr<JsonValue>*;
        using reference = const JsonUniquePtr<JsonValue>&;
        const_iterator() : arr_(nullptr), index_(0) {}
        const_iterator(const JsonArray* arr, size_t idx) : arr_(arr), index_(idx) {}
        reference operator*() const { return arr_->vector_[index_]; }
        pointer operator->() const { return &arr_->vector_[index_]; }
        const_iterator& operator++() { ++index_; return *this; }
        const_iterator operator++(int) { const_iterator tmp = *this; ++index_; return tmp; }
        const_iterator& operator--() { --index_; return *this; }
        const_iterator operator--(int) { const_iterator tmp = *this; --index_; return tmp; }
        const_iterator& operator+=(difference_type n) { index_ += n; return *this; }
        const_iterator& operator-=(difference_type n) { index_ -= n; return *this; }
        const_iterator operator+(difference_type n) const { return const_iterator(arr_, index_ + n); }
        const_iterator operator-(difference_type n) const { return const_iterator(arr_, index_ - n); }
        difference_type operator-(const const_iterator& other) const { return index_ - other.index_; }
        reference operator[](difference_type n) const { return arr_->vector_[index_ + n]; }
        bool operator==(const const_iterator& other) const { return index_ == other.index_; }
        bool operator!=(const const_iterator& other) const { return index_ != other.index_; }
        bool operator<(const const_iterator& other) const { return index_ < other.index_; }
        bool operator>(const const_iterator& other) const { return index_ > other.index_; }
        bool operator<=(const const_iterator& other) const { return index_ <= other.index_; }
        bool operator>=(const const_iterator& other) const { return index_ >= other.index_; }
    private:
        const JsonArray* arr_;
        size_t index_;
    };
    struct iterator {
        using iterator_category = std::random_access_iterator_tag;
        using value_type = JsonUniquePtr<JsonValue>;
        using difference_type = std::ptrdiff_t;
        using pointer = JsonUniquePtr<JsonValue>*;
        using reference = JsonUniquePtr<JsonValue>&;
        iterator() : arr_(nullptr), index_(0) {}
        iterator(JsonArray* arr, size_t idx) : arr_(arr), index_(idx) {}
        reference operator*() const { return arr_->vector_[index_]; }
        pointer operator->() const { return &arr_->vector_[index_]; }
        iterator& operator++() { ++index_; return *this; }
        iterator operator++(int) { iterator tmp = *this; ++index_; return tmp; }
        iterator& operator--() { --index_; return *this; }
        iterator operator--(int) { iterator tmp = *this; --index_; return tmp; }
        iterator& operator+=(difference_type n) { index_ += n; return *this; }
        iterator& operator-=(difference_type n) { index_ -= n; return *this; }
        iterator operator+(difference_type n) const { return iterator(arr_, index_ + n); }
        iterator operator-(difference_type n) const { return iterator(arr_, index_ - n); }
        difference_type operator-(const iterator& other) const { return index_ - other.index_; }
        reference operator[](difference_type n) const { return arr_->vector_[index_ + n]; }
        bool operator==(const iterator& other) const { return index_ == other.index_; }
        bool operator!=(const iterator& other) const { return index_ != other.index_; }
        bool operator<(const iterator& other) const { return index_ < other.index_; }
        bool operator>(const iterator& other) const { return index_ > other.index_; }
        bool operator<=(const iterator& other) const { return index_ <= other.index_; }
        bool operator>=(const iterator& other) const { return index_ >= other.index_; }
    private:
        JsonArray* arr_;
        size_t index_;
    };

    iterator begin() { return iterator(this, 0); }
    iterator end() { return iterator(this, vector_.size()); }
    const_iterator begin() const { return const_iterator(this, 0); }
    const_iterator end() const { return const_iterator(this, vector_.size()); }
    const_iterator cbegin() const { return const_iterator(this, 0); }
    const_iterator cend() const { return const_iterator(this, vector_.size()); }
    [[nodiscard]] JsonValue* Clone() const override { return new JsonArray(*this); }

private:
    std::vector<JsonUniquePtr<JsonValue>> vector_;
};

inline void JsonArray::WriteTo(std::ostream& os) const {
    os << '[';
    bool first = true;
    for (const auto& item : vector_) {
        if (!first) os << ',';
        item->WriteTo(os);
        first = false;
    }
    os << ']';
}

enum class JsonTokenType { LeftBrace, RightBrace, LeftBracket, RightBracket, Colon, Comma, String, Number, True, False, Null, EndOfFile, Error };

struct JsonToken {
    JsonTokenType type = JsonTokenType::Error;
    string_view text;
    size_t line = 0, column = 0, bytePosition = 0;
    JsonToken() = default;
    JsonToken(JsonTokenType t, string_view txt, size_t ln, size_t col, size_t pos)
        : type(t), text(txt), line(ln), column(col), bytePosition(pos) {}
};

class JsonReader {
public:
    explicit JsonReader(string_view input, bool allowComments = true)
        : input_(input), position_(0), line_(1), column_(1), allowComments_(allowComments) {}
    const JsonToken& CurrentToken() const { return currentToken_; }
    const JsonToken& NextToken() { ReadToken(); return currentToken_; }
    friend class JsonParser;

private:
    inline void SkipWhitespaceAndComments() {
        const size_t len = input_.size();
        while (position_ < len) {
            char c = input_[position_];
            switch (c) {
                case ' ': case '\t': ++column_; ++position_; break;
                case '\r':
                    if (position_ + 1 < len && input_[position_ + 1] == '\n') ++position_;
                    [[fallthrough]];
                case '\n': ++line_; column_ = 1; ++position_; break;
                case '/':
                    if (!allowComments_) throw JsonParsingException(line_, column_, position_, "Comments not allowed");
                    if (position_ + 1 >= len) { ++position_; break; }
                    if (input_[position_ + 1] == '/') {
                        position_ += 2;
                        while (position_ < len && input_[position_] != '\n') ++position_;
                        break;
                    }
                    if (input_[position_ + 1] == '*') {
                        position_ += 2;
                        while (position_ + 1 < len) {
                            if (input_[position_] == '*' && input_[position_ + 1] == '/') { position_ += 2; break; }
                            if (input_[position_] == '\n') { ++line_; column_ = 0; }
                            ++position_;
                        }
                        break;
                    }
                    throw JsonParsingException(line_, column_, position_, "Invalid comment");
                default: return;
            }
        }
    }

    inline void ReadToken() {
        SkipWhitespaceAndComments();
        size_t tokenLine = line_;
        size_t tokenColumn = column_;
        size_t tokenPos = position_;
        if (position_ >= input_.size()) {
            currentToken_ = JsonToken(JsonTokenType::EndOfFile, string_view(), line_, column_, position_);
            return;
        }
        char c = input_[position_];
        switch (c) {
            case '{': currentToken_ = JsonToken(JsonTokenType::LeftBrace, string_view(input_.data() + position_, 1), tokenLine, tokenColumn, tokenPos); ++position_; ++column_; break;
            case '}': currentToken_ = JsonToken(JsonTokenType::RightBrace, string_view(input_.data() + position_, 1), tokenLine, tokenColumn, tokenPos); ++position_; ++column_; break;
            case '[': currentToken_ = JsonToken(JsonTokenType::LeftBracket, string_view(input_.data() + position_, 1), tokenLine, tokenColumn, tokenPos); ++position_; ++column_; break;
            case ']': currentToken_ = JsonToken(JsonTokenType::RightBracket, string_view(input_.data() + position_, 1), tokenLine, tokenColumn, tokenPos); ++position_; ++column_; break;
            case ':': currentToken_ = JsonToken(JsonTokenType::Colon, string_view(input_.data() + position_, 1), tokenLine, tokenColumn, tokenPos); ++position_; ++column_; break;
            case ',': currentToken_ = JsonToken(JsonTokenType::Comma, string_view(input_.data() + position_, 1), tokenLine, tokenColumn, tokenPos); ++position_; ++column_; break;
            case '"': ReadStringToken(tokenLine, tokenColumn, tokenPos); break;
            case 't': ReadKeyword("true", 4, JsonTokenType::True, tokenLine, tokenColumn, tokenPos); break;
            case 'f': ReadKeyword("false", 5, JsonTokenType::False, tokenLine, tokenColumn, tokenPos); break;
            case 'n': ReadKeyword("null", 4, JsonTokenType::Null, tokenLine, tokenColumn, tokenPos); break;
            default:
                if (c == '-' || (c >= '0' && c <= '9')) ReadNumberToken(tokenLine, tokenColumn, tokenPos);
                else throw JsonParsingException(tokenLine, tokenColumn, tokenPos, std::string("Unexpected character: ") + c);
                break;
        }
    }

    inline void ReadStringToken(size_t tokenLine, size_t tokenColumn, size_t tokenPos) {
        ++position_; ++column_;
        size_t start = position_;
        bool escaped = false;
        const size_t len = input_.size();
        while (position_ < len) {
            unsigned char c = static_cast<unsigned char>(input_[position_]);
            if (escaped) { escaped = false; }
            else if (c == '\\') { escaped = true; }
            else if (c == '"') { break; }
            else if (c == '\n' || c == '\r') { throw JsonParsingException(line_, column_, position_, "Unterminated string"); }
            else if (c < 0x20) { throw JsonParsingException(line_, column_, position_, "Control character must be escaped (U+0000 to U+001F)"); }
            if (c == '\n') { ++line_; column_ = 0; }
            ++position_; ++column_;
        }
        if (position_ >= len) throw JsonParsingException(line_, column_, position_, "Unterminated string");
        currentToken_ = JsonToken(JsonTokenType::String, string_view(input_.data() + start, position_ - start), tokenLine, tokenColumn, tokenPos);
        ++position_; ++column_;
    }

    inline void ReadKeyword(const char* keyword, size_t len, JsonTokenType type, size_t tokenLine, size_t tokenColumn, size_t tokenPos) {
        if (position_ + len > input_.size()) throw JsonParsingException(tokenLine, tokenColumn, tokenPos, std::string("Unexpected end of input, expected ") + keyword);
        string_view view(input_.data() + position_, len);
        if (view == keyword) {
            currentToken_ = JsonToken(type, view, tokenLine, tokenColumn, tokenPos);
            position_ += len; column_ += len;
        } else throw JsonParsingException(tokenLine, tokenColumn, tokenPos, std::string("Unexpected token, expected ") + keyword);
    }

    inline void ReadNumberToken(size_t tokenLine, size_t tokenColumn, size_t tokenPos) {
        size_t start = position_;
        if (input_[position_] == '-') { ++position_; ++column_; }
        if (position_ < input_.size() && input_[position_] == '0') {
            ++position_; ++column_;
            if (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9')
                throw JsonParsingException(line_, column_, position_, "Invalid number: leading zero not allowed");
        } else if (position_ < input_.size() && input_[position_] >= '1' && input_[position_] <= '9') {
            while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9') { ++position_; ++column_; }
        } else throw JsonParsingException(line_, column_, position_, "Invalid number");
        if (position_ < input_.size() && input_[position_] == '.') {
            ++position_; ++column_;
            if (position_ >= input_.size() || input_[position_] < '0' || input_[position_] > '9')
                throw JsonParsingException(line_, column_, position_, "Invalid number");
            while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9') { ++position_; ++column_; }
        }
        if (position_ < input_.size() && (input_[position_] == 'e' || input_[position_] == 'E')) {
            ++position_; ++column_;
            if (position_ < input_.size() && (input_[position_] == '+' || input_[position_] == '-')) { ++position_; ++column_; }
            if (position_ >= input_.size() || input_[position_] < '0' || input_[position_] > '9')
                throw JsonParsingException(line_, column_, position_, "Invalid number");
            while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9') { ++position_; ++column_; }
        }
        currentToken_ = JsonToken(JsonTokenType::Number, string_view(input_.data() + start, position_ - start), tokenLine, tokenColumn, tokenPos);
    }

    string_view input_;
    size_t position_ = 0, line_ = 1, column_ = 1;
    JsonToken currentToken_;
    bool allowComments_;
};

enum class PropertyNamingPolicy { Default, CamelCase };

struct JsonParserOptions {
    size_t MaxDepth = 64;
    bool AllowComments = true;
    bool AllowTrailingCommas = true;
    PropertyNamingPolicy NamingPolicy = PropertyNamingPolicy::Default;
};

class JsonParser {
public:
    static JsonUniquePtr<JsonValue> Parse(string_view json, size_t maxDepth = 64) {
        JsonParser parser(json, JsonParserOptions{ maxDepth, true, true });
        return parser.ParseValueWithEOFCheck();
    }
    static JsonUniquePtr<JsonValue> Parse(string_view json, const JsonParserOptions& options) {
        JsonParser parser(json, options);
        return parser.ParseValueWithEOFCheck();
    }
    static JsonUniquePtr<JsonValue> ParseStrict(string_view json, size_t maxDepth = 64) {
        JsonParser parser(json, JsonParserOptions{ maxDepth, false, false });
        return parser.ParseValueWithEOFCheck();
    }

private:
    inline JsonUniquePtr<JsonValue> ParseValueWithEOFCheck() {
        auto value = ParseValue();
        if (JSONLITE_UNLIKELY(reader_.CurrentToken().type != JsonTokenType::EndOfFile))
            throw JsonParsingException(reader_.CurrentToken().line, reader_.CurrentToken().column,
                reader_.CurrentToken().bytePosition, "Unexpected trailing characters");
        return value;
    }
    JsonParser(string_view json, const JsonParserOptions& options)
        : reader_(json, options.AllowComments), options_(options), currentDepth_(0) {
        reader_.NextToken();
    }

    inline JsonUniquePtr<JsonValue> ParseValue() {
        if (JSONLITE_UNLIKELY(currentDepth_ >= options_.MaxDepth))
            throw JsonParsingException(reader_.CurrentToken().line, reader_.CurrentToken().column,
                reader_.CurrentToken().bytePosition, "Maximum nesting depth exceeded");
        ++currentDepth_;
        JsonUniquePtr<JsonValue> result;
        switch (reader_.CurrentToken().type) {
            case JsonTokenType::LeftBrace: result = ParseObject(); break;
            case JsonTokenType::LeftBracket: result = ParseArray(); break;
            case JsonTokenType::String: result = ParseString(); break;
            case JsonTokenType::Number: result = ParseNumber(); break;
            case JsonTokenType::True: result = MakeJson<JsonBoolean>(true); reader_.NextToken(); break;
            case JsonTokenType::False: result = MakeJson<JsonBoolean>(false); reader_.NextToken(); break;
            case JsonTokenType::Null: result = MakeJson<JsonNull>(); reader_.NextToken(); break;
            default: throw JsonParsingException(reader_.CurrentToken().line, reader_.CurrentToken().column,
                reader_.CurrentToken().bytePosition, "Expected a JSON value");
        }
        --currentDepth_;
        return result;
    }

    inline JsonUniquePtr<JsonValue> ParseObject() {
        auto obj = MakeJson<JsonObject>();
        reader_.NextToken();
        if (reader_.CurrentToken().type != JsonTokenType::RightBrace) {
            while (true) {
                if (JSONLITE_UNLIKELY(reader_.CurrentToken().type != JsonTokenType::String))
                    throw JsonParsingException(reader_.CurrentToken().line, reader_.CurrentToken().column,
                        reader_.CurrentToken().bytePosition, "Expected property name");
                string_view key = reader_.CurrentToken().text;
                string unescapedKey = JsonString::Unescape(key);
                if (options_.NamingPolicy == PropertyNamingPolicy::CamelCase) unescapedKey = ToCamelCase(unescapedKey);
                reader_.NextToken();
                if (JSONLITE_UNLIKELY(reader_.CurrentToken().type != JsonTokenType::Colon))
                    throw JsonParsingException(reader_.CurrentToken().line, reader_.CurrentToken().column,
                        reader_.CurrentToken().bytePosition, "Expected ':'");
                reader_.NextToken();
                auto value = ParseValue();
                obj->Add(std::move(unescapedKey), std::move(value));
                if (reader_.CurrentToken().type == JsonTokenType::Comma) {
                    reader_.NextToken();
                    if (reader_.CurrentToken().type == JsonTokenType::RightBrace) {
                        if (JSONLITE_UNLIKELY(!options_.AllowTrailingCommas))
                            throw JsonParsingException(reader_.CurrentToken().line, reader_.CurrentToken().column,
                                reader_.CurrentToken().bytePosition, "Trailing comma not allowed");
                        break;
                    }
                } else if (reader_.CurrentToken().type == JsonTokenType::RightBrace) break;
                else throw JsonParsingException(reader_.CurrentToken().line, reader_.CurrentToken().column,
                    reader_.CurrentToken().bytePosition, "Expected ',' or '}'");
            }
        }
        reader_.NextToken();
        return obj;
    }

    inline JsonUniquePtr<JsonValue> ParseArray() {
        auto arr = MakeJson<JsonArray>();
        reader_.NextToken();
        if (reader_.CurrentToken().type != JsonTokenType::RightBracket) {
            while (true) {
                auto value = ParseValue();
                arr->Add(std::move(value));
                if (reader_.CurrentToken().type == JsonTokenType::Comma) {
                    reader_.NextToken();
                    if (reader_.CurrentToken().type == JsonTokenType::RightBracket) {
                        if (JSONLITE_UNLIKELY(!options_.AllowTrailingCommas))
                            throw JsonParsingException(reader_.CurrentToken().line, reader_.CurrentToken().column,
                                reader_.CurrentToken().bytePosition, "Trailing comma not allowed");
                        break;
                    }
                } else if (reader_.CurrentToken().type == JsonTokenType::RightBracket) break;
                else throw JsonParsingException(reader_.CurrentToken().line, reader_.CurrentToken().column,
                    reader_.CurrentToken().bytePosition, "Expected ',' or ']'");
            }
        }
        reader_.NextToken();
        return arr;
    }

    inline JsonUniquePtr<JsonValue> ParseString() {
        string_view rawText = reader_.CurrentToken().text;
        string unescaped = JsonString::Unescape(rawText);
        reader_.NextToken();
        return MakeJson<JsonString>(unescaped);
    }

    inline JsonUniquePtr<JsonValue> ParseNumber() {
        string_view rawText = reader_.CurrentToken().text;
        auto num = MakeJson<JsonNumber>(rawText);
        reader_.NextToken();
        return num;
    }

    inline std::string ToCamelCase(std::string_view str) const {
        if (str.empty()) return std::string();
        std::string result;
        result.reserve(str.size());
        size_t i = 0;
        while (i < str.size() && str[i] == '_') ++i;
        if (i < str.size()) {
            if (str[i] >= 'A' && str[i] <= 'Z') result += static_cast<char>(str[i] + 32);
            else result += str[i];
            ++i;
        }
        while (i < str.size()) {
            if (str[i] == '_') {
                ++i;
                if (i < str.size()) {
                    if (str[i] >= 'a' && str[i] <= 'z') result += static_cast<char>(str[i] - 32);
                    else result += str[i];
                    ++i;
                }
            } else { result += str[i]; ++i; }
        }
        return result;
    }

    JsonReader reader_;
    JsonParserOptions options_;
    size_t currentDepth_ = 0;
};

enum class EOLStyle { LF, CRLF, CR };

struct JsonWriterOptions {
    bool Indented = true;
    bool WriteBOM = false;
    EOLStyle Eol = EOLStyle::LF;
    size_t IndentSize = 2;
};

class JsonWriter {
public:
    explicit JsonWriter(std::ostream& os, const JsonWriterOptions& options = {})
        : os_(os), options_(options), currentDepth_(0) {}
    void Write(const JsonValue* value) {
        if (options_.WriteBOM) {
            unsigned char bom[] = { 0xEF, 0xBB, 0xBF };
            os_.write(reinterpret_cast<char*>(bom), 3);
        }
        WriteValue(value, 0);
    }
    void Write(const JsonValue& value) { Write(&value); }
    static string ToString(const JsonValue* value, const JsonWriterOptions& options = {}) {
        std::ostringstream oss; JsonWriter writer(oss, options); writer.Write(value); return oss.str();
    }
    static string ToString(const JsonValue& value, const JsonWriterOptions& options = {}) { return ToString(&value, options); }

private:
    inline void WriteIndent(size_t depth) {
        if (options_.Indented)
            for (size_t i = 0; i < depth * options_.IndentSize; ++i) os_ << ' ';
    }
    inline void WriteNewLine() {
        switch (options_.Eol) {
            case EOLStyle::LF: os_ << '\n'; break;
            case EOLStyle::CRLF: os_ << "\r\n"; break;
            case EOLStyle::CR: os_ << '\r'; break;
        }
    }
    inline void WriteValue(const JsonValue* value, size_t depth) {
        if (!value) { os_ << "null"; return; }
        switch (value->GetType()) {
            case JsonValueType::Object: WriteObject(&value->AsObject(), depth); break;
            case JsonValueType::Array: WriteArray(&value->AsArray(), depth); break;
            case JsonValueType::String: WriteString(&value->AsString()); break;
            case JsonValueType::Number: WriteNumber(&value->AsNumber()); break;
            case JsonValueType::Boolean: WriteBoolean(&value->AsBoolean()); break;
            case JsonValueType::Null: os_ << "null"; break;
        }
    }
    inline void WriteObject(const JsonObject* obj, size_t depth) {
        os_ << '{';
        if (obj->Count() == 0) { os_ << '}'; return; }
        if (options_.Indented) WriteNewLine();
        auto it = obj->begin();
        size_t index = 0, count = obj->Count();
        while (true) {
            WriteIndent(depth + 1);
            os_ << '"' << JsonString::Escape(it->key) << "\":";
            if (options_.Indented) os_ << ' ';
            WriteValue(it->value.get(), depth + 1);
            ++it; ++index;
            if (index < count) { os_ << ','; if (options_.Indented) WriteNewLine(); }
            else break;
        }
        if (options_.Indented) { WriteNewLine(); WriteIndent(depth); }
        os_ << '}';
    }
    inline void WriteArray(const JsonArray* arr, size_t depth) {
        os_ << '[';
        if (arr->Count() == 0) { os_ << ']'; return; }
        if (options_.Indented) WriteNewLine();
        for (size_t i = 0; i < arr->Count(); ++i) {
            WriteIndent(depth + 1);
            WriteValue(arr->GetAt(i), depth + 1);
            if (i + 1 < arr->Count()) { os_ << ','; if (options_.Indented) WriteNewLine(); }
        }
        if (options_.Indented) { WriteNewLine(); WriteIndent(depth); }
        os_ << ']';
    }
    inline void WriteString(const JsonString* str) { os_ << '"' << JsonString::Escape(str->GetValue()) << '"'; }
    inline void WriteNumber(const JsonNumber* num) { num->WriteTo(os_); }
    inline void WriteBoolean(const JsonBoolean* val) { os_ << (val->GetValue() ? "true" : "false"); }
    std::ostream& os_;
    const JsonWriterOptions& options_;
    size_t currentDepth_;
};

class JsonElement {
public:
    JsonElement() noexcept : value_(nullptr) {}
    explicit JsonElement(const JsonValue* value) noexcept : value_(value) {}
    JsonElement(std::nullptr_t) noexcept : value_(nullptr) {}
    JsonElement(const JsonElement&) = default;
    JsonElement(JsonElement&&) = default;
    JsonElement& operator=(const JsonElement&) = default;
    JsonElement& operator=(JsonElement&&) = default;

    [[nodiscard]] bool IsObject() const noexcept { return value_ && value_->IsObject(); }
    [[nodiscard]] bool IsArray() const noexcept { return value_ && value_->IsArray(); }
    [[nodiscard]] bool IsString() const noexcept { return value_ && value_->IsString(); }
    [[nodiscard]] bool IsNumber() const noexcept { return value_ && value_->IsNumber(); }
    [[nodiscard]] bool IsBoolean() const noexcept { return value_ && value_->IsBoolean(); }
    [[nodiscard]] bool IsNull() const noexcept { return !value_ || value_->IsNull(); }
    [[nodiscard]] JsonValueType GetType() const noexcept { return value_ ? value_->GetType() : JsonValueType::Null; }
    [[nodiscard]] JsonElement GetProperty(std::string_view name) const {
        if (!IsObject()) return JsonElement();
        return JsonElement(value_->AsObject().Get(name));
    }
    [[nodiscard]] JsonElement GetIndex(size_t index) const {
        if (!IsArray()) return JsonElement();
        return JsonElement(value_->AsArray().GetAt(index));
    }
    [[nodiscard]] bool TryGetProperty(std::string_view name, JsonElement& element) const noexcept {
        if (!IsObject()) return false;
        const JsonValue* prop = value_->AsObject().Get(name);
        if (prop) { element = JsonElement(prop); return true; }
        return false;
    }
    [[nodiscard]] std::optional<JsonElement> GetPropertyOrDefault(std::string_view name) const noexcept {
        JsonElement elem;
        if (TryGetProperty(name, elem)) return elem;
        return std::nullopt;
    }
    [[nodiscard]] string_view GetString() const {
        if (JSONLITE_UNLIKELY(!IsString())) throw JsonInvalidTypeException("String", JsonTypeName(GetType()));
        return value_->GetString();
    }
    [[nodiscard]] int64_t GetInt64() const {
        if (JSONLITE_UNLIKELY(!IsNumber())) throw JsonInvalidTypeException("Number", JsonTypeName(GetType()));
        return value_->GetInt64();
    }
    [[nodiscard]] double GetDouble() const {
        if (JSONLITE_UNLIKELY(!IsNumber())) throw JsonInvalidTypeException("Number", JsonTypeName(GetType()));
        return value_->GetDouble();
    }
    [[nodiscard]] bool GetBoolean() const {
        if (JSONLITE_UNLIKELY(!IsBoolean())) throw JsonInvalidTypeException("Boolean", JsonTypeName(GetType()));
        return value_->GetBoolean();
    }
    [[nodiscard]] const JsonObject& GetObject() const {
        if (JSONLITE_UNLIKELY(!IsObject())) throw JsonInvalidTypeException("Object", JsonTypeName(GetType()));
        return value_->AsObject();
    }
    [[nodiscard]] const JsonArray& GetArray() const {
        if (JSONLITE_UNLIKELY(!IsArray())) throw JsonInvalidTypeException("Array", JsonTypeName(GetType()));
        return value_->AsArray();
    }
    [[nodiscard]] size_t GetArrayLength() const { return IsArray() ? value_->AsArray().Count() : 0; }
    [[nodiscard]] size_t GetObjectPropertyCount() const { return IsObject() ? value_->AsObject().Count() : 0; }
    [[nodiscard]] bool ContainsProperty(std::string_view name) const {
        return IsObject() && value_->AsObject().Contains(name);
    }
    [[nodiscard]] bool operator==(const JsonElement& other) const noexcept {
        if (!value_ && !other.value_) return true;
        if (!value_ || !other.value_) return false;
        return value_->Equals(other.value_);
    }
    [[nodiscard]] bool operator!=(const JsonElement& other) const noexcept { return !(*this == other); }
    [[nodiscard]] explicit operator bool() const noexcept { return value_ != nullptr; }
    [[nodiscard]] const JsonValue* GetValue() const noexcept { return value_; }

    class JsonArrayEnumerable {
    public:
        JsonArrayEnumerable(const JsonArray* arr) : arr_(arr) {}
        class const_iterator {
        public:
            const_iterator(const JsonArray* arr, size_t idx) : arr_(arr), index_(idx) {}
            JsonElement operator*() const { 
                if (!arr_) return JsonElement(nullptr);
                return JsonElement(arr_->GetAt(index_)); 
            }
            const_iterator& operator++() { ++index_; return *this; }
            const_iterator operator++(int) { const_iterator tmp = *this; ++index_; return tmp; }
            bool operator==(const const_iterator& other) const { return index_ == other.index_; }
            bool operator!=(const const_iterator& other) const { return index_ != other.index_; }
        private:
            const JsonArray* arr_;
            size_t index_;
        };
        const_iterator begin() const { return const_iterator(arr_, 0); }
        const_iterator end() const { return const_iterator(arr_, arr_ ? arr_->Count() : 0); }
    private:
        const JsonArray* arr_;
    };

    class JsonObjectEnumerable {
    public:
        JsonObjectEnumerable(const JsonObject* obj) : obj_(obj), beginIter_(), endIter_() {
            if (obj_) { beginIter_ = obj_->begin(); endIter_ = obj_->end(); }
        }
        class const_iterator {
        public:
            using value_type = std::pair<std::string_view, JsonElement>;
            const_iterator(typename JsonObject::const_iterator it, const JsonObject* obj) : iter_(it), obj_(obj) {}
            value_type operator*() const {
                if (!obj_) return { std::string_view(), JsonElement(nullptr) };
                const auto& kv = *iter_;
                return { kv.key, JsonElement(kv.value.get()) };
            }
            const_iterator& operator++() { ++iter_; return *this; }
            const_iterator operator++(int) { const_iterator tmp = *this; ++iter_; return tmp; }
            bool operator==(const const_iterator& other) const { return iter_ == other.iter_; }
            bool operator!=(const const_iterator& other) const { return iter_ != other.iter_; }
        private:
            typename JsonObject::const_iterator iter_;
            const JsonObject* obj_;
        };
        const_iterator begin() const { return const_iterator(beginIter_, obj_); }
        const_iterator end() const { return const_iterator(endIter_, obj_); }
    private:
        const JsonObject* obj_;
        typename JsonObject::const_iterator beginIter_;
        typename JsonObject::const_iterator endIter_;
    };

    [[nodiscard]] JsonArrayEnumerable EnumerateArray() const {
        return JsonArrayEnumerable(IsArray() ? &value_->AsArray() : nullptr);
    }
    [[nodiscard]] JsonObjectEnumerable EnumerateObject() const {
        return JsonObjectEnumerable(IsObject() ? &value_->AsObject() : nullptr);
    }

    template<typename T> [[nodiscard]] T GetValue() const;
    template<typename T> [[nodiscard]] T GetValueOrDefault(const T& defaultValue) const noexcept;

private:
    const JsonValue* value_ = nullptr;
};

template<> inline std::string JsonElement::GetValue<std::string>() const { return std::string(GetString()); }
template<> inline int64_t JsonElement::GetValue<int64_t>() const { return GetInt64(); }
template<> inline double JsonElement::GetValue<double>() const { return GetDouble(); }
template<> inline bool JsonElement::GetValue<bool>() const { return GetBoolean(); }

template<typename T>
inline T JsonElement::GetValueOrDefault(const T& defaultValue) const noexcept {
    try { return GetValue<T>(); } catch (...) { return defaultValue; }
}

inline bool operator==(const JsonElement& lhs, std::nullptr_t) noexcept { return !lhs || lhs.IsNull(); }
inline bool operator==(std::nullptr_t, const JsonElement& rhs) noexcept { return rhs == nullptr; }
inline bool operator!=(const JsonElement& lhs, std::nullptr_t) noexcept { return !(lhs == nullptr); }
inline bool operator!=(std::nullptr_t, const JsonElement& rhs) noexcept { return rhs != nullptr; }

class JsonDocument {
public:
    static JsonDocument Parse(std::string_view json, size_t maxDepth = 64) { return JsonDocument(json, maxDepth); }
    static JsonDocument Parse(std::string_view json, const JsonParserOptions& options) { return JsonDocument(json, options); }
    static JsonDocument Parse(std::istream& stream, size_t maxDepth = 64) {
        std::ostringstream oss; oss << stream.rdbuf(); return Parse(std::string_view(oss.str()), maxDepth);
    }
    JsonDocument() = default;
    JsonDocument(const JsonDocument&) = delete;
    JsonDocument& operator=(const JsonDocument&) = delete;
    JsonDocument(JsonDocument&& other) noexcept : root_(std::move(other.root_)) {}
    JsonDocument& operator=(JsonDocument&& other) noexcept {
        if (this != &other) root_ = std::move(other.root_);
        return *this;
    }
    [[nodiscard]] JsonElement GetRoot() const noexcept { return JsonElement(root_.get()); }
    [[nodiscard]] JsonValue& Root() { return *root_; }
    [[nodiscard]] const JsonValue& Root() const { return *root_; }
    [[nodiscard]] JsonValueType GetRootType() const noexcept { return root_ ? root_->GetType() : JsonValueType::Null; }
    [[nodiscard]] bool IsObject() const noexcept { return root_ && root_->IsObject(); }
    [[nodiscard]] bool IsArray() const noexcept { return root_ && root_->IsArray(); }
    [[nodiscard]] bool IsNull() const noexcept { return !root_ || root_->IsNull(); }
    void WriteTo(std::ostream& stream, const JsonWriterOptions& options = {}) const {
        if (root_) { JsonWriter writer(stream, options); writer.Write(root_.get()); }
    }
    [[nodiscard]] string ToString(const JsonWriterOptions& options = {}) const {
        std::ostringstream oss; WriteTo(oss, options); return oss.str();
    }
    [[nodiscard]] explicit operator bool() const noexcept { return root_ != nullptr; }

private:
    JsonDocument(std::string_view json, size_t maxDepth) { root_ = JsonParser::Parse(json, maxDepth); }
    JsonDocument(std::string_view json, const JsonParserOptions& options) { root_ = JsonParser::Parse(json, options); }
    JsonUniquePtr<JsonValue> root_;
};

namespace detail {
    template<typename T> struct is_optional : std::false_type {};
    template<typename T> struct is_optional<std::optional<T>> : std::true_type {};
    template<typename T> struct is_vector : std::false_type {};
    template<typename T> struct is_vector<std::vector<T>> : std::true_type {};
    template<typename T> struct is_basic_type : std::bool_constant<
        std::is_same_v<T, std::string> || std::is_same_v<T, int64_t> ||
        std::is_same_v<T, double> || std::is_same_v<T, bool>> {};
}

class JsonSerializer {
public:
    template<typename T>
    static T Deserialize(std::string_view json) {
        auto doc = JsonDocument::Parse(json); return DeserializeInternal<T>(doc.GetRoot());
    }
    template<typename T>
    static T Deserialize(std::string_view json, const JsonParserOptions& options) {
        auto doc = JsonDocument::Parse(json, options); return DeserializeInternal<T>(doc.GetRoot());
    }
    template<typename T>
    static T DeserializeFromElement(JsonElement element) { return DeserializeInternal<T>(element); }

private:
    template<typename T>
    static T DeserializeInternal(JsonElement element) {
        if constexpr (detail::is_basic_type<T>::value) {
            return DeserializeBasicType<T>(element);
        } else if constexpr (detail::is_optional<T>::value) {
            using VT = typename T::value_type;
            if (element.IsNull()) return std::nullopt;
            return std::optional<VT>(DeserializeInternal<VT>(element));
        } else if constexpr (detail::is_vector<T>::value) {
            using VT = typename T::value_type;
            return DeserializeVector<VT>(element);
        } else if constexpr (std::is_same_v<T, JsonElement>) {
            return element;
        } else {
            static_assert(sizeof(T) == 0, "Unsupported type for Deserialize");
        }
    }
    template<typename T>
    static T DeserializeBasicType(JsonElement element) {
        if constexpr (std::is_same_v<T, std::string>) return std::string(element.GetString());
        else if constexpr (std::is_same_v<T, int64_t>) return element.GetInt64();
        else if constexpr (std::is_same_v<T, double>) return element.GetDouble();
        else if constexpr (std::is_same_v<T, bool>) return element.GetBoolean();
    }
    template<typename T>
    static std::vector<T> DeserializeVector(JsonElement element) {
        std::vector<T> result;
        if (!element.IsArray()) return result;
        result.reserve(element.GetArrayLength());
        for (const auto& item : element.EnumerateArray()) result.push_back(DeserializeInternal<T>(item));
        return result;
    }
};

template<typename T> struct JsonDeserializer {
    static T FromJson(JsonElement element) { return JsonSerializer::DeserializeFromElement<T>(element); }
};
template<> struct JsonDeserializer<int64_t> { static int64_t FromJson(JsonElement e) { return e.GetInt64(); } };
template<> struct JsonDeserializer<double> { static double FromJson(JsonElement e) { return e.GetDouble(); } };
template<> struct JsonDeserializer<std::string> { static std::string FromJson(JsonElement e) { return std::string(e.GetString()); } };
template<> struct JsonDeserializer<bool> { static bool FromJson(JsonElement e) { return e.GetBoolean(); } };
template<typename T> struct JsonDeserializer<std::optional<T>> {
    static std::optional<T> FromJson(JsonElement e) {
        if (e.IsNull()) return std::nullopt;
        return JsonDeserializer<T>::FromJson(e);
    }
};
template<typename T> struct JsonDeserializer<std::vector<T>> {
    static std::vector<T> FromJson(JsonElement e) {
        std::vector<T> result;
        if (!e.IsArray()) return result;
        for (const auto& item : e.EnumerateArray()) result.push_back(JsonDeserializer<T>::FromJson(item));
        return result;
    }
};

using Json = JsonValue;
using JsonObject = JsonObject;
using JsonArray = JsonArray;
using JsonString = JsonString;
using JsonNumber = JsonNumber;
using JsonBoolean = JsonBoolean;
using JsonNull = JsonNull;
using JsonDocument = JsonDocument;
using JsonElement = JsonElement;
using JsonValue = JsonValue;
using JsonValueType = JsonValueType;
using JsonWriterOptions = JsonWriterOptions;
using JsonTokenType = JsonTokenType;
using JsonToken = JsonToken;
using JsonReader = JsonReader;
using JsonParser = JsonParser;
using JsonWriter = JsonWriter;
using EOLStyle = EOLStyle;
}

namespace json = jsonlite;
