#include "CRC.h"

#include <array>

namespace {
constexpr uint32_t CRC24QMask = 0x00FFFFFF;

constexpr std::array<uint8_t, 256> make_crc8_table() {
    std::array<uint8_t, 256> table{};
    for (size_t i = 0; i < table.size(); ++i) {
        uint8_t crc = (uint8_t)i;
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x80) != 0 ? (uint8_t)((crc << 1) ^ CRC::CRC8Polynomial) : (uint8_t)(crc << 1);
        }
        table[i] = crc;
    }
    return table;
}

constexpr std::array<uint16_t, 256> make_crc16_table() {
    std::array<uint16_t, 256> table{};
    for (size_t i = 0; i < table.size(); ++i) {
        uint16_t crc = (uint16_t)i;
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 1) != 0 ? (uint16_t)((crc >> 1) ^ 0xA001u) : (uint16_t)(crc >> 1);
        }
        table[i] = crc;
    }
    return table;
}

constexpr std::array<uint16_t, 256> make_crc16_ccitt_false_table() {
    std::array<uint16_t, 256> table{};
    for (size_t i = 0; i < table.size(); ++i) {
        uint16_t crc = (uint16_t)(i << 8);
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x8000) != 0 ? (uint16_t)((crc << 1) ^ CRC::CRC16CCITTFalsePolynomial) : (uint16_t)(crc << 1);
        }
        table[i] = crc;
    }
    return table;
}

constexpr std::array<uint32_t, 256> make_crc24q_table() {
    std::array<uint32_t, 256> table{};
    for (size_t i = 0; i < table.size(); ++i) {
        uint32_t crc = (uint32_t)i << 16;
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x800000u) != 0 ? ((crc << 1) ^ CRC::CRC24QPolynomial) : (crc << 1);
            crc &= CRC24QMask;
        }
        table[i] = crc;
    }
    return table;
}

constexpr std::array<uint32_t, 256> make_crc32_table() {
    std::array<uint32_t, 256> table{};
    for (size_t i = 0; i < table.size(); ++i) {
        uint32_t crc = (uint32_t)i;
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 1) != 0 ? ((crc >> 1) ^ 0xEDB88320u) : (crc >> 1);
        }
        table[i] = crc;
    }
    return table;
}

constexpr std::array<uint64_t, 256> make_crc64_table() {
    std::array<uint64_t, 256> table{};
    for (size_t i = 0; i < table.size(); ++i) {
        uint64_t crc = (uint64_t)i << 56;
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x8000000000000000ull) != 0 ? ((crc << 1) ^ CRC::CRC64Polynomial) : (crc << 1);
        }
        table[i] = crc;
    }
    return table;
}

const std::array<uint8_t, 256>& crc8_table() {
    static constexpr auto table = make_crc8_table();
    return table;
}

const std::array<uint16_t, 256>& crc16_table() {
    static constexpr auto table = make_crc16_table();
    return table;
}

const std::array<uint16_t, 256>& crc16_ccitt_false_table() {
    static constexpr auto table = make_crc16_ccitt_false_table();
    return table;
}

const std::array<uint32_t, 256>& crc24q_table() {
    static constexpr auto table = make_crc24q_table();
    return table;
}

const std::array<uint32_t, 256>& crc32_table() {
    static constexpr auto table = make_crc32_table();
    return table;
}

const std::array<uint64_t, 256>& crc64_table() {
    static constexpr auto table = make_crc64_table();
    return table;
}

const uint8_t* bytes_from(const void* data, size_t length) {
    if (data == nullptr || length == 0)
        return nullptr;
    return (const uint8_t*)data;
}
}

uint8_t CRC::CRC8(const void* data, size_t length) {
    return CRC8Update(CRC8Initial, data, length);
}

uint8_t CRC::CRC8(const std::vector<uint8_t>& data) {
    return CRC8(data.data(), data.size());
}

uint8_t CRC::CRC8(const std::string& data) {
    return CRC8(data.data(), data.size());
}

uint8_t CRC::CRC8Update(uint8_t crc, const void* data, size_t length) {
    const uint8_t* bytes = bytes_from(data, length);
    if (bytes == nullptr)
        return crc;

    const auto& table = crc8_table();
    for (size_t i = 0; i < length; ++i)
        crc = table[(uint8_t)(crc ^ bytes[i])];
    return crc;
}

uint16_t CRC::CRC16(const void* data, size_t length) {
    return CRC16Update(CRC16Initial, data, length);
}

uint16_t CRC::CRC16(const std::vector<uint8_t>& data) {
    return CRC16(data.data(), data.size());
}

uint16_t CRC::CRC16(const std::string& data) {
    return CRC16(data.data(), data.size());
}

uint16_t CRC::CRC16Update(uint16_t crc, const void* data, size_t length) {
    const uint8_t* bytes = bytes_from(data, length);
    if (bytes == nullptr)
        return crc;

    const auto& table = crc16_table();
    for (size_t i = 0; i < length; ++i)
        crc = (uint16_t)((crc >> 8) ^ table[(crc ^ bytes[i]) & 0xFFu]);
    return crc;
}

uint16_t CRC::CRC16CCITTFalse(const void* data, size_t length) {
    return CRC16CCITTFalseUpdate(CRC16CCITTFalseInitial, data, length);
}

uint16_t CRC::CRC16CCITTFalse(const std::vector<uint8_t>& data) {
    return CRC16CCITTFalse(data.data(), data.size());
}

uint16_t CRC::CRC16CCITTFalse(const std::string& data) {
    return CRC16CCITTFalse(data.data(), data.size());
}

uint16_t CRC::CRC16CCITTFalseUpdate(uint16_t crc, const void* data, size_t length) {
    const uint8_t* bytes = bytes_from(data, length);
    if (bytes == nullptr)
        return crc;

    const auto& table = crc16_ccitt_false_table();
    for (size_t i = 0; i < length; ++i)
        crc = (uint16_t)((crc << 8) ^ table[((crc >> 8) ^ bytes[i]) & 0xFFu]);
    return crc;
}

uint32_t CRC::CRC24Q(const void* data, size_t length) {
    return CRC24QUpdate(CRC24QInitial, data, length);
}

uint32_t CRC::CRC24Q(const std::vector<uint8_t>& data) {
    return CRC24Q(data.data(), data.size());
}

uint32_t CRC::CRC24Q(const std::string& data) {
    return CRC24Q(data.data(), data.size());
}

uint32_t CRC::CRC24QUpdate(uint32_t crc, const void* data, size_t length) {
    const uint8_t* bytes = bytes_from(data, length);
    if (bytes == nullptr)
        return crc & CRC24QMask;

    const auto& table = crc24q_table();
    crc &= CRC24QMask;
    for (size_t i = 0; i < length; ++i)
        crc = (((crc << 8) & CRC24QMask) ^ table[((crc >> 16) ^ bytes[i]) & 0xFFu]) & CRC24QMask;
    return crc;
}

uint32_t CRC::CRC32(const void* data, size_t length) {
    return CRC32Finish(CRC32Update(CRC32Initial, data, length));
}

uint32_t CRC::CRC32(const std::vector<uint8_t>& data) {
    return CRC32(data.data(), data.size());
}

uint32_t CRC::CRC32(const std::string& data) {
    return CRC32(data.data(), data.size());
}

uint32_t CRC::CRC32Update(uint32_t crc, const void* data, size_t length) {
    const uint8_t* bytes = bytes_from(data, length);
    if (bytes == nullptr)
        return crc;

    const auto& table = crc32_table();
    for (size_t i = 0; i < length; ++i)
        crc = (crc >> 8) ^ table[(crc ^ bytes[i]) & 0xFFu];
    return crc;
}

uint32_t CRC::CRC32Finish(uint32_t crc) {
    return crc ^ CRC32XorOut;
}

uint64_t CRC::CRC64(const void* data, size_t length) {
    return CRC64Update(CRC64Initial, data, length);
}

uint64_t CRC::CRC64(const std::vector<uint8_t>& data) {
    return CRC64(data.data(), data.size());
}

uint64_t CRC::CRC64(const std::string& data) {
    return CRC64(data.data(), data.size());
}

uint64_t CRC::CRC64Update(uint64_t crc, const void* data, size_t length) {
    const uint8_t* bytes = bytes_from(data, length);
    if (bytes == nullptr)
        return crc;

    const auto& table = crc64_table();
    for (size_t i = 0; i < length; ++i)
        crc = (crc << 8) ^ table[((crc >> 56) ^ bytes[i]) & 0xFFu];
    return crc;
}
