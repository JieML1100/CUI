#pragma once
#include <cstring>
#include <initializer_list>
#include <string>
#include <type_traits>
#include <vector>
class DataPack {
public:
    std::string Id;
    std::vector<uint8_t> Value;
    std::vector<DataPack> Child = std::vector<DataPack>();
    DataPack& operator[](int index);
    DataPack& operator[](const std::string& id);
    __declspec(property (put = resize, get = size)) size_t Count;
    void operator=(const std::initializer_list<uint8_t> data);
    void operator=(const std::initializer_list<uint8_t>* data);
    template<typename T>
    void operator=(T data) {
        static_assert(std::is_trivially_copyable_v<T>, "DataPack only supports trivially copyable types");
        this->Value.resize(sizeof(T));
        std::memcpy(this->Value.data(), &data, sizeof(T));
    }
    template<typename T>
    void operator=(const std::vector<T>& data) {
        static_assert(std::is_trivially_copyable_v<T>, "DataPack only supports trivially copyable types");
        this->Value.resize(data.size() * sizeof(T));
        if (!data.empty())
            std::memcpy(this->Value.data(), data.data(), data.size() * sizeof(T));
    }
    template<typename T>
    void operator=(std::initializer_list<T> data) {
        static_assert(std::is_trivially_copyable_v<T>, "DataPack only supports trivially copyable types");
        this->Value.resize(data.size() * sizeof(T));
        if (data.size() > 0)
            std::memcpy(this->Value.data(), data.begin(), data.size() * sizeof(T));
    }
    template<typename T>
    T operator=(const DataPack& data) {
        static_assert(std::is_trivially_copyable_v<T>, "DataPack only supports trivially copyable types");
        T result = T();
        if (data.Value.size() >= sizeof(T))
            std::memcpy(&result, data.Value.data(), sizeof(T));
        return result;
    }
    void operator=(const char* data);
    void operator=(const wchar_t* data);
    void operator=(char* data);
    void operator=(wchar_t* data);
    void operator=(std::string data);
    void operator=(std::wstring data);
    template<typename T>
    DataPack(T data) {
        static_assert(std::is_trivially_copyable_v<T>, "DataPack only supports trivially copyable types");
        this->Id = "";
        this->Value.resize(sizeof(T));
        std::memcpy(this->Value.data(), &data, sizeof(T));
    }
    template<typename T>
    DataPack(std::string id, T data) {
        static_assert(std::is_trivially_copyable_v<T>, "DataPack only supports trivially copyable types");
        this->Id = id;
        this->Value.resize(sizeof(T));
        std::memcpy(this->Value.data(), &data, sizeof(T));
    }
    DataPack();
    DataPack(const char* key);
    DataPack(const uint8_t* data, int data_len);
    DataPack(std::string id, uint8_t* data, int len);
    DataPack(std::vector<uint8_t> data);
    DataPack(std::initializer_list<uint8_t> data);
    DataPack(std::string id, std::string data);
    DataPack(std::string id, std::wstring data);
    DataPack(std::string id, char* data);
    DataPack(std::string id, const char* data);
    DataPack(std::string id, wchar_t* data);
    DataPack(std::string id, const wchar_t* data);

    void Add(const DataPack& val);
    template<typename T>
    DataPack& Add(std::string key, T val) {
        this->Child.push_back(DataPack(key, val));
        return this->Child[this->Child.size() - 1];
    }
    template<typename T>
    DataPack& Add(T val) {
        this->Child.push_back(DataPack("", val));
        return this->Child[this->Child.size() - 1];
    }
    template<typename T>
    T convert() const {
        static_assert(std::is_trivially_copyable_v<T>, "DataPack only supports trivially copyable types");
        T output{};
        if (this->Value.size() >= sizeof(T))
            std::memcpy(&output, this->Value.data(), sizeof(T));
        return output;
    }
    template<typename T>
    void convert(T& output) const {
        static_assert(std::is_trivially_copyable_v<T>, "DataPack only supports trivially copyable types");
        output = T{};
        if (this->Value.size() >= sizeof(T))
            std::memcpy(&output, this->Value.data(), sizeof(T));
    }
    template<typename T>
    void convert(T* output) const {
        static_assert(std::is_trivially_copyable_v<T>, "DataPack only supports trivially copyable types");
        if (output == nullptr)
            return;
        *output = T{};
        if (this->Value.size() >= sizeof(T))
            std::memcpy(output, this->Value.data(), sizeof(T));
    }
    bool ContainsKsy(const std::string& key) const {
        for (const auto& child : this->Child) {
            if (child.Id == key)
                return true;
        }
        return false;
    }
    void RemoveAt(int index);
    void WriteTo(std::vector<uint8_t>& out) const;
    std::vector<uint8_t> GetBytes() const;
    void clear();
    size_t size() const;
    void resize(size_t value);
};