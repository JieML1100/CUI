#include "CRandom.h"
#include "Utils.h"
#include <random>
std::random_device rd;
std::mt19937 gen(rd());

int Random::Next() {
    std::uniform_int_distribution<> dist(0, INT_MAX);
    return dist(gen);
}

int Random::Next(int min, int max) {
    std::uniform_int_distribution<> dist(min, max);
    return dist(gen);
}

double Random::NextDouble() {
    std::uniform_real_distribution<> dist(0.0, 1.0);
    return dist(gen);
}

std::vector<uint8_t> Random::NextBytes(int count) {
    std::vector<uint8_t> bytes(count);
    std::uniform_int_distribution<> dist(0, 0xFF);
    for (int i = 0; i < count; ++i) {
        bytes[i] = static_cast<uint8_t>(dist(gen));
    }
    return bytes;
}

void Random::NextBytes(void* buffer, int count) {
    uint8_t* byteBuffer = static_cast<uint8_t*>(buffer);
    std::uniform_int_distribution<> dist(0, 0xFF);
    for (int i = 0; i < count; ++i) {
        byteBuffer[i] = static_cast<uint8_t>(dist(gen));
	}
}