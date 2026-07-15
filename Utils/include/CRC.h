#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class CRC {
public:
    static constexpr uint8_t CRC8Initial = 0x00;
    static constexpr uint8_t CRC8Polynomial = 0x07;

    static constexpr uint16_t CRC16Initial = 0x0000;
    static constexpr uint16_t CRC16Polynomial = 0x8005;

    static constexpr uint16_t CRC16CCITTFalseInitial = 0xFFFF;
    static constexpr uint16_t CRC16CCITTFalsePolynomial = 0x1021;

    static constexpr uint32_t CRC24QInitial = 0x000000;
    static constexpr uint32_t CRC24QPolynomial = 0x864CFB;

    static constexpr uint32_t CRC32Initial = 0xFFFFFFFF;
    static constexpr uint32_t CRC32Polynomial = 0x04C11DB7;
    static constexpr uint32_t CRC32XorOut = 0xFFFFFFFF;

    static constexpr uint64_t CRC64Initial = 0x0000000000000000ull;
    static constexpr uint64_t CRC64Polynomial = 0x42F0E1EBA9EA3693ull;

    static uint8_t CRC8(const void* data, size_t length);
    static uint8_t CRC8(const std::vector<uint8_t>& data);
    static uint8_t CRC8(const std::string& data);
    static uint8_t CRC8Update(uint8_t crc, const void* data, size_t length);

    static uint16_t CRC16(const void* data, size_t length);
    static uint16_t CRC16(const std::vector<uint8_t>& data);
    static uint16_t CRC16(const std::string& data);
    static uint16_t CRC16Update(uint16_t crc, const void* data, size_t length);

    static uint16_t CRC16CCITTFalse(const void* data, size_t length);
    static uint16_t CRC16CCITTFalse(const std::vector<uint8_t>& data);
    static uint16_t CRC16CCITTFalse(const std::string& data);
    static uint16_t CRC16CCITTFalseUpdate(uint16_t crc, const void* data, size_t length);

    static uint32_t CRC24Q(const void* data, size_t length);
    static uint32_t CRC24Q(const std::vector<uint8_t>& data);
    static uint32_t CRC24Q(const std::string& data);
    static uint32_t CRC24QUpdate(uint32_t crc, const void* data, size_t length);

    static uint32_t CRC32(const void* data, size_t length);
    static uint32_t CRC32(const std::vector<uint8_t>& data);
    static uint32_t CRC32(const std::string& data);
    static uint32_t CRC32Update(uint32_t crc, const void* data, size_t length);
    static uint32_t CRC32Finish(uint32_t crc);

    static uint64_t CRC64(const void* data, size_t length);
    static uint64_t CRC64(const std::vector<uint8_t>& data);
    static uint64_t CRC64(const std::string& data);
    static uint64_t CRC64Update(uint64_t crc, const void* data, size_t length);
};
