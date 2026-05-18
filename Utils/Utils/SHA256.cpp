#include "SHA256.h"
#include <sstream>
void SHA256::init() {
    count[0] = count[1] = 0;
    state[0] = 0x6a09e667;
    state[1] = 0xbb67ae85;
    state[2] = 0x3c6ef372;
    state[3] = 0xa54ff53a;
    state[4] = 0x510e527f;
    state[5] = 0x9b05688c;
    state[6] = 0x1f83d9ab;
    state[7] = 0x5be0cd19;
}

void SHA256::update(const uint8_t* input, size_t length) {
    size_t index = (count[0] >> 3) & 0x3F;
    count[0] += static_cast<uint32_t>(length << 3);
    if (count[0] < (length << 3)) count[1]++;
    count[1] += static_cast<uint32_t>(length >> 29);
    size_t first_part = 64 - index;

    size_t i = 0;
    if (length >= first_part) {
        std::memcpy(&buffer[index], input, first_part);
        transform(buffer);
        for (i = first_part; i + 63 < length; i += 64) transform(&input[i]);
        index = 0;
    }
    std::memcpy(&buffer[index], &input[i], length - i);
}

void SHA256::finalize() {
    static const uint8_t padding[64] = { 0x80 };
    if (finalized) return;

    uint8_t bits[8];
    uint64_t total_bits = (static_cast<uint64_t>(count[1]) << 32) | count[0];
    bits[0] = (total_bits >> 56) & 0xff;
    bits[1] = (total_bits >> 48) & 0xff;
    bits[2] = (total_bits >> 40) & 0xff;
    bits[3] = (total_bits >> 32) & 0xff;
    bits[4] = (total_bits >> 24) & 0xff;
    bits[5] = (total_bits >> 16) & 0xff;
    bits[6] = (total_bits >> 8) & 0xff;
    bits[7] = total_bits & 0xff;

    size_t index = (count[0] >> 3) & 0x3f;
    size_t pad_len = (index < 56) ? (56 - index) : (120 - index);
    update(padding, pad_len);
    update(bits, 8);

    encode(digest, state, 32);
    finalized = true;
}

std::string SHA256::hexdigest() const {
    if (!finalized) return "";
    std::ostringstream result;
    for (int i = 0; i < 32; i++) result << std::hex << std::setw(2) << std::setfill('0') << (int)digest[i];
    return result.str();
}

void SHA256::transform(const uint8_t block[64]) {
    uint32_t m[64], w[64];
    uint32_t a, b, c, d, e, f, g, h;

    decode(m, block, 64);

    for (int i = 0; i < 16; ++i) w[i] = m[i];
    for (int i = 16; i < 64; ++i) w[i] = delta1(w[i - 2]) + w[i - 7] + delta0(w[i - 15]) + w[i - 16];

    a = state[0];
    b = state[1];
    c = state[2];
    d = state[3];
    e = state[4];
    f = state[5];
    g = state[6];
    h = state[7];

    for (int i = 0; i < 64; ++i) {
        uint32_t t1 = h + sigma1(e) + choice(e, f, g) + K[i] + w[i];
        uint32_t t2 = sigma0(a) + majority(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

void SHA256::encode(uint8_t* output, const uint32_t* input, size_t length) {
    for (size_t i = 0, j = 0; j < length; i++, j += 4) {
        output[j] = (input[i] >> 24) & 0xff;
        output[j + 1] = (input[i] >> 16) & 0xff;
        output[j + 2] = (input[i] >> 8) & 0xff;
        output[j + 3] = input[i] & 0xff;
    }
}

void SHA256::decode(uint32_t* output, const uint8_t* input, size_t length) {
    for (size_t i = 0, j = 0; j < length; i++, j += 4) {
        output[i] = (input[j] << 24) | (input[j + 1] << 16) | (input[j + 2] << 8) | input[j + 3];
    }
}