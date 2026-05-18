#pragma once
#include "defines.h"
#include <unordered_map>
#include <vector>
#include <optional>
template <class _Kty, class _Vty>
class Dictionary : public std::unordered_map<_Kty, _Vty> {
public:
    Dictionary() = default;

    Dictionary(std::initializer_list<std::pair<_Kty, _Vty>> list) {
        for (const auto& pair : list) {
            Add(pair.first, pair.second);
        }
    }

    Dictionary(const Dictionary& other)
        : std::unordered_map<_Kty, _Vty>(other) {
    }

    Dictionary(Dictionary&& other) noexcept
        : std::unordered_map<_Kty, _Vty>(std::move(other)) {
    }

    ~Dictionary() = default;

    bool ContainsKey(_Kty key) const {
        return this->find(key) != this->end();
    }

    void Add(_Kty key, _Vty value) {
        (*this)[key] = value;
    }

    bool Remove(_Kty key) {
        return this->erase(key) > 0;
    }

    void Clear() {
        this->clear();
    }

    int Count() const {
        return this->size();
    }

    std::vector<_Kty> Keys() const {
        std::vector<_Kty> keys;
        for (const auto& pair : *this) {
            keys.push_back(pair.first);
        }
        return keys;
    }

    std::vector<_Vty> Values() const {
        std::vector<_Vty> values;
        for (const auto& pair : *this) {
            values.push_back(pair.second);
        }
        return values;
    }
    std::optional<_Vty> GetValue(_Kty key) const {
        if (ContainsKey(key)) {
            return (*this).at(key);
        }
        else {
            return std::nullopt;
        }
    }
};