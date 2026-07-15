#pragma once
#include "defines.h"
#include <string>
#pragma comment(lib, "Rpcrt4.lib")
class Guid {
public:
	GUID Data;
	Guid();
	Guid(GUID guid);
	Guid(std::string str);
	static Guid NewGuid();
	static Guid Parse(const std::string& input);
	std::string ToString();
	bool operator==(const Guid& other);
	bool operator!=(const Guid& other);
};

