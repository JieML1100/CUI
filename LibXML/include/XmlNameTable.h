#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_set>

namespace System::Xml {

class XmlNameTable final {
public:
    XmlNameTable();
    ~XmlNameTable();

    XmlNameTable(const XmlNameTable&) = delete;
    XmlNameTable& operator=(const XmlNameTable&) = delete;

    XmlNameTable(XmlNameTable&& other) noexcept;
    XmlNameTable& operator=(XmlNameTable&& other) noexcept;

    const std::string& Add(std::string_view value);
    const std::string* Get(std::string_view value) const;
    std::size_t Count() const noexcept;

private:
    struct StringPtrHash {
        using is_transparent = void;
        std::size_t operator()(const std::string* s) const noexcept {
            return std::hash<std::string_view>{}(*s);
        }
        std::size_t operator()(std::string_view sv) const noexcept {
            return std::hash<std::string_view>{}(sv);
        }
    };

    struct StringPtrEqual {
        using is_transparent = void;
        bool operator()(const std::string* lhs, const std::string* rhs) const noexcept {
            return *lhs == *rhs;
        }
        bool operator()(const std::string* lhs, std::string_view rhs) const noexcept {
            return *lhs == rhs;
        }
        bool operator()(std::string_view lhs, const std::string* rhs) const noexcept {
            return lhs == *rhs;
        }
    };

    std::unordered_set<const std::string*, StringPtrHash, StringPtrEqual> values_;

    struct ArenaBlock {
        static constexpr std::size_t Capacity = 1024;
        alignas(std::string) unsigned char storage[Capacity * sizeof(std::string)];
        std::size_t used = 0;
        ArenaBlock* next = nullptr;
    };
    ArenaBlock* firstBlock_ = nullptr;
    ArenaBlock* currentBlock_ = nullptr;

    const std::string& AllocateString(std::string_view value);
};

}  // namespace System::Xml
