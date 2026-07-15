#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>

enum class DataPackParseStatus {
    Ok,
    NullInput,
    TooSmall,
    InvalidHeader,
    UnsupportedVersion,
    UnsupportedFlags,
    SizeMismatch,
    DepthLimitExceeded,
    SizeLimitExceeded,
    ValueLimitExceeded,
    IdLimitExceeded,
    ChildLimitExceeded,
    InvalidMarker,
    InvalidLength,
    InvalidTerminator,
    AllocationFailed,
    Exception,
};

struct DataPackParseResult {
    DataPackParseStatus Status = DataPackParseStatus::Ok;
    size_t Offset = 0;
    std::string Message;

    bool Success() const { return Status == DataPackParseStatus::Ok; }
    operator bool() const { return Success(); }
};

struct DataPackParseOptions {
    size_t MaxDepth = 64;
    size_t MaxBytes = 0;
    size_t MaxValueBytes = 0;
    size_t MaxIdBytes = 65535;
    size_t MaxChildren = 0;
    bool RequireFullBuffer = true;
};

enum class DataPackWireVersion : uint8_t {
    Legacy = 1,
    Version2 = 2,
};

struct DataPackWriteOptions {
    DataPackWireVersion Version = DataPackWireVersion::Legacy;
};

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
        if (data.size() > std::numeric_limits<size_t>::max() / sizeof(T)) {
            this->Value.clear();
            return;
        }
        const size_t byteCount = data.size() * sizeof(T);
        this->Value.resize(byteCount);
        if (byteCount > 0)
            std::memcpy(this->Value.data(), data.data(), byteCount);
    }

    template<typename T>
    void operator=(std::initializer_list<T> data) {
        static_assert(std::is_trivially_copyable_v<T>, "DataPack only supports trivially copyable types");
        if (data.size() > std::numeric_limits<size_t>::max() / sizeof(T)) {
            this->Value.clear();
            return;
        }
        const size_t byteCount = data.size() * sizeof(T);
        this->Value.resize(byteCount);
        if (byteCount > 0)
            std::memcpy(this->Value.data(), data.begin(), byteCount);
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
    DataPack(const uint8_t* data, size_t data_len);
    DataPack(std::string id, uint8_t* data, int len);
    DataPack(std::string id, const uint8_t* data, size_t len);
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

    bool ContainsKey(const std::string& key) const;
    bool ContainsKsy(const std::string& key) const { return ContainsKey(key); }

    void RemoveAt(int index);
    void WriteTo(std::vector<uint8_t>& out) const;
    std::vector<uint8_t> GetBytes() const;
    std::vector<uint8_t> GetBytes(DataPackWriteOptions options) const;

    static DataPackParseResult TryParse(const uint8_t* data, size_t data_len, DataPack& out, DataPackParseOptions options = {});
    static bool Validate(const uint8_t* data, size_t data_len, DataPackParseOptions options = {});

    void clear();
    size_t size() const;
    void resize(size_t value);
};
