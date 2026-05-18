#pragma once
#include <iostream>
#include <iomanip>
#include <cstring>
#include <cstdint>
#include <sstream>
class MD5 {
public:
    MD5() { init(); }
    void update(const uint8_t* input, size_t length);
    void finalize();
    std::string hexdigest() const;

private:
    void init();
    void transform(const uint8_t block[64]);
    void encode(uint8_t* output, const uint32_t* input, size_t length);
    void decode(uint32_t* output, const uint8_t* input, size_t length);

    static constexpr uint32_t S[64] = {
        7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,
        5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,
        4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,
        6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21
    };
    static constexpr uint32_t K[64] = {
        0XD76AA478, 0XE8C7B756, 0X242070DB, 0XC1BDCEEE,
        0XF57C0FAF, 0X4787C62A, 0XA8304613, 0XFD469501,
        0X698098D8, 0X8B44F7AF, 0XFFFF5BB1, 0X895CD7BE,
        0X6B901122, 0XFD987193, 0XA679438E, 0X49B40821,
        0XF61E2562, 0XC040B340, 0X265E5A51, 0XE9B6C7AA,
        0XD62F105D, 0X02441453, 0XD8A1E681, 0XE7D3FBC8,
        0X21E1CDE6, 0XC33707D6, 0XF4D50D87, 0X455A14ED,
        0XA9E3E905, 0XFCEFA3F8, 0X676F02D9, 0X8D2A4C8A,
        0XFFFA3942, 0X8771F681, 0X6D9D6122, 0XFDE5380C,
        0XA4BEEA44, 0X4BDECFA9, 0XF6BB4B60, 0XBEBFBC70,
        0X289B7EC6, 0XEAA127FA, 0XD4EF3085, 0X04881D05,
        0XD9D4D039, 0XE6DB99E5, 0X1FA27CF8, 0XC4AC5665,
        0XF4292244, 0X432AFF97, 0XAB9423A7, 0XFC93A039,
        0X655B59C3, 0X8F0CCC92, 0XFFEFF47D, 0X85845DD1,
        0X6FA87E4F, 0XFE2CE6E0, 0XA3014314, 0X4E0811A1,
        0XF7537E82, 0XBD3AF235, 0X2AD7D2BB, 0XEB86D391
    };

    uint32_t state[4];
    uint32_t count[2];
    uint8_t buffer[64];
    uint8_t digest[16];
    bool finalized = false;

    static inline uint32_t F(uint32_t x, uint32_t y, uint32_t z) { return (x & y) | (~x & z); }
    static inline uint32_t G(uint32_t x, uint32_t y, uint32_t z) { return (x & z) | (y & ~z); }
    static inline uint32_t H(uint32_t x, uint32_t y, uint32_t z) { return x ^ y ^ z; }
    static inline uint32_t I(uint32_t x, uint32_t y, uint32_t z) { return y ^ (x | ~z); }
    static inline uint32_t rotate_left(uint32_t x, uint32_t n) { return (x << n) | (x >> (32 - n)); }

    static inline void FF(uint32_t& a, uint32_t b, uint32_t c, uint32_t d, uint32_t x, uint32_t s, uint32_t ac) {
        a = rotate_left(a + F(b, c, d) + x + ac, s) + b;
    }
    static inline void GG(uint32_t& a, uint32_t b, uint32_t c, uint32_t d, uint32_t x, uint32_t s, uint32_t ac) {
        a = rotate_left(a + G(b, c, d) + x + ac, s) + b;
    }
    static inline void HH(uint32_t& a, uint32_t b, uint32_t c, uint32_t d, uint32_t x, uint32_t s, uint32_t ac) {
        a = rotate_left(a + H(b, c, d) + x + ac, s) + b;
    }
    static inline void II(uint32_t& a, uint32_t b, uint32_t c, uint32_t d, uint32_t x, uint32_t s, uint32_t ac) {
        a = rotate_left(a + I(b, c, d) + x + ac, s) + b;
    }
};
