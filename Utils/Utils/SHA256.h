#pragma once
#include <iostream>
#include <iomanip>
#include <cstring>
#include <cstdint>

class SHA256 {
public:
    SHA256() { init(); }
    void update(const uint8_t* input, size_t length);
    void finalize();
    std::string hexdigest() const;

private:
    void init();
    void transform(const uint8_t block[64]);
    void encode(uint8_t* output, const uint32_t* input, size_t length);
    void decode(uint32_t* output, const uint8_t* input, size_t length);

    static constexpr uint32_t K[64] = {
        0X428A2F98, 0X71374491, 0XB5C0FBCF, 0XE9B5DBA5,
        0X3956C25B, 0X59F111F1, 0X923F82A4, 0XAB1C5ED5,
        0XD807AA98, 0X12835B01, 0X243185BE, 0X550C7DC3,
        0X72BE5D74, 0X80DEB1FE, 0X9BDC06A7, 0XC19BF174,
        0XE49B69C1, 0XEFBE4786, 0X0FC19DC6, 0X240CA1CC,
        0X2DE92C6F, 0X4A7484AA, 0X5CB0A9DC, 0X76F988DA,
        0X983E5152, 0XA831C66D, 0XB00327C8, 0XBF597FC7,
        0XC6E00BF3, 0XD5A79147, 0X06CA6351, 0X14292967,
        0X27B70A85, 0X2E1B2138, 0X4D2C6DFC, 0X53380D13,
        0X650A7354, 0X766A0ABB, 0X81C2C92E, 0X92722C85,
        0XA2BFE8A1, 0XA81A664B, 0XC24B8B70, 0XC76C51A3,
        0XD192E819, 0XD6990624, 0XF40E3585, 0X106AA070,
        0X19A4C116, 0X1E376C08, 0X2748774C, 0X34B0BCB5,
        0X391C0CB3, 0X4ED8AA4A, 0X5B9CCA4F, 0X682E6FF3,
        0X748F82EE, 0X78A5636F, 0X84C87814, 0X8CC70208,
        0X90BEFFFA, 0XA4506CEB, 0XBEF9A3F7, 0XC67178F2
    };

    uint32_t state[8];
    uint32_t count[2];
    uint8_t buffer[64];
    uint8_t digest[32];
    bool finalized = false;

    static inline uint32_t rotate_right(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }
    static inline uint32_t choice(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (~x & z); }
    static inline uint32_t majority(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
    static inline uint32_t sigma0(uint32_t x) { return rotate_right(x, 2) ^ rotate_right(x, 13) ^ rotate_right(x, 22); }
    static inline uint32_t sigma1(uint32_t x) { return rotate_right(x, 6) ^ rotate_right(x, 11) ^ rotate_right(x, 25); }
    static inline uint32_t delta0(uint32_t x) { return rotate_right(x, 7) ^ rotate_right(x, 18) ^ (x >> 3); }
    static inline uint32_t delta1(uint32_t x) { return rotate_right(x, 17) ^ rotate_right(x, 19) ^ (x >> 10); }
};



