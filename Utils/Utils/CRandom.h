#pragma once
#include "defines.h"
#include <vector>
class Random {
public:
	static int Next();
	static int Next(int min, int max);
	static double NextDouble();
	static std::vector<uint8_t> NextBytes(int count);
	static void NextBytes(void* buffer,int count);
};