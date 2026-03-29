#pragma once
#include "defines.h"
#include <string>
#include <sstream>
class StringBuilder {
private:
	std::stringstream buffer;
public:
	StringBuilder();
	~StringBuilder();
	PROPERTY(uint32_t, Lenght);
	GET(uint32_t, Lenght);
	SET(uint32_t, Lenght);

	void Append(const char* str);
	void Append(const wchar_t* str);
	void Append(const char str);
	void Append(const wchar_t str);
	void Append(const int str);
	void Append(const unsigned int str);
	void Append(const long str);
	void Append(const unsigned long str);
	void Append(const long long str);
	void Append(const unsigned long long str);
	void Append(const float str);
	void Append(const double str);
	void Append(const long double str);
	void Append(const bool val);
	void Append(const void* val);
	void Append(const std::string str);
	void Append(const std::wstring str);
	void Append(const StringBuilder str);
	void AppendLine(const char* str);
	void AppendLine(const wchar_t* str);
	void AppendLine(const char str);
	void AppendLine(const wchar_t str);
	void AppendLine(const int str);
	void AppendLine(const unsigned int str);
	void AppendLine(const long str);
	void AppendLine(const unsigned long str);
	void AppendLine(const long long str);
	void AppendLine(const unsigned long long str);
	void AppendLine(const float str);
	void AppendLine(const double str);
	void AppendLine(const long double str);
	void AppendLine(const bool val);
	void AppendLine(const void* val);
	void AppendLine(const std::string str);
	void AppendLine(const std::wstring str);
	void AppendLine(const StringBuilder str);
	std::string ToString();
	std::wstring ToWString();
	void Clear();
	template<typename T>
	StringBuilder& operator << (T str) {
		this->Append(str);
		return *this;
	}
};

