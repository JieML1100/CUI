#include "MD5.h"

void MD5::init() {
    count[0] = count[1] = 0;
    state[0] = 0x67452301;
    state[1] = 0xefcdab89;
    state[2] = 0x98badcfe;
    state[3] = 0x10325476;
}

void MD5::update(const uint8_t* input, size_t length) {
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

void MD5::finalize() {
    static const uint8_t padding[64] = { 0x80 };
    if (finalized) return;

    uint8_t bits[8];
    encode(bits, count, 8);
    size_t index = (count[0] >> 3) & 0x3f;
    size_t pad_len = (index < 56) ? (56 - index) : (120 - index);
    update(padding, pad_len);
    update(bits, 8);

    encode(digest, state, 16);
    finalized = true;
}

std::string MD5::hexdigest() const {
    if (!finalized) return "";
    std::ostringstream result;
    for (int i = 0; i < 16; i++) result << std::hex << std::setw(2) << std::setfill('0') << (int)digest[i];
    return result.str();
}

void MD5::transform(const uint8_t block[64]) {
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3], x[16];
    decode(x, block, 64);

    for (int i = 0; i < 64; i++) {
        if (i < 16) FF(a, b, c, d, x[i], S[i], K[i]);
        else if (i < 32) GG(a, b, c, d, x[(5 * i + 1) % 16], S[i], K[i]);
        else if (i < 48) HH(a, b, c, d, x[(3 * i + 5) % 16], S[i], K[i]);
        else II(a, b, c, d, x[(7 * i) % 16], S[i], K[i]);

        uint32_t temp = d; d = c; c = b; b = a; a = temp;
    }

    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
}

void MD5::encode(uint8_t* output, const uint32_t* input, size_t length) {
    for (size_t i = 0, j = 0; j < length; i++, j += 4) {
        output[j] = input[i] & 0xff;
        output[j + 1] = (input[i] >> 8) & 0xff;
        output[j + 2] = (input[i] >> 16) & 0xff;
        output[j + 3] = (input[i] >> 24) & 0xff;
    }
}

void MD5::decode(uint32_t* output, const uint8_t* input, size_t length) {
    for (size_t i = 0, j = 0; j < length; i++, j += 4) {
        output[i] = input[j] | (input[j + 1] << 8) | (input[j + 2] << 16) | (input[j + 3] << 24);
    }
}