#include "StringBuilder.h"
#include "Convert.h"
StringBuilder::StringBuilder():buffer() {}
StringBuilder::~StringBuilder() {
}

GET_CPP(StringBuilder, uint32_t, Lenght) {
	return this->buffer.str().size();
}
SET_CPP(StringBuilder, uint32_t, Lenght) {
	if (value < this->buffer.str().size()) {
		this->buffer.str(this->buffer.str().substr(0, value));
	}
	else {
		this->buffer << std::string(value - this->buffer.str().size(), '\0');
	}
}
void StringBuilder::Append(const char* str) {
	this->buffer << str;
}
void StringBuilder::Append(const wchar_t* str) {
	this->buffer << Convert::wstring_to_string(str);
}
void StringBuilder::Append(const char str) {
	this->buffer << str;
}
void StringBuilder::Append(const wchar_t str) {
	wchar_t tmp[2] = { str,0 };
	this->Append(tmp);
}
void StringBuilder::Append(const int str) {
	this->buffer << str;
}
void StringBuilder::Append(const unsigned int str) {
	this->buffer << str;
}
void StringBuilder::Append(const long str) {
	this->buffer << str;
}
void StringBuilder::Append(const unsigned long str) {
	this->buffer << str;
}
void StringBuilder::Append(const long long str) {
	this->buffer << str;
}
void StringBuilder::Append(const unsigned long long str) {
	this->buffer << str;
}
void StringBuilder::Append(const float str) {
	this->buffer << str;
}
void StringBuilder::Append(const double str) {
	this->buffer << str;
}
void StringBuilder::Append(const long double str) {
	this->buffer << str;
}
void StringBuilder::Append(const bool val) {
	this->buffer << (val ? "True" : "False");
}
void StringBuilder::Append(const void* val) {
	this->buffer << val;
}
void StringBuilder::Append(const std::string str) {
	this->buffer << str;
}
void StringBuilder::Append(const std::wstring str) {
	this->buffer << Convert::wstring_to_string(str);
}
void StringBuilder::Append(const StringBuilder str) {
	this->buffer << str.buffer.str();
}
void StringBuilder::AppendLine(const char* str) {
	this->buffer << str << "\n";
}
void StringBuilder::AppendLine(const wchar_t* str) {
	this->buffer << Convert::wstring_to_string(str) << "\n";
}
void StringBuilder::AppendLine(const char str) {
	this->buffer << str << "\n";
}
void StringBuilder::AppendLine(const wchar_t str) {
	wchar_t tmp[2] = { str,0 };
	this->AppendLine(tmp);
}
void StringBuilder::AppendLine(const int str) {
	this->buffer << str << "\n";
}
void StringBuilder::AppendLine(const unsigned int str) {
	this->buffer << str << "\n";
}
void StringBuilder::AppendLine(const long str) {
	this->buffer << str << "\n";
}
void StringBuilder::AppendLine(const unsigned long str) {
	this->buffer << str << "\n";
}
void StringBuilder::AppendLine(const long long str) {
	this->buffer << str << "\n";
}
void StringBuilder::AppendLine(const unsigned long long str) {
	this->buffer << str << "\n";
}
void StringBuilder::AppendLine(const float str) {
	this->buffer << str << "\n";
}
void StringBuilder::AppendLine(const double str) {
	this->buffer << str << "\n";
}
void StringBuilder::AppendLine(const long double str) {
	this->buffer << str << "\n";
}
void StringBuilder::AppendLine(const bool val) {
	this->buffer << (val ? "True" : "False") << "\n";
}
void StringBuilder::AppendLine(const void* val) {
	this->buffer << val << "\n";
}
void StringBuilder::AppendLine(const std::string str) {
	this->buffer << str << "\n";
}
void StringBuilder::AppendLine(const std::wstring str) {
	this->buffer << Convert::wstring_to_string(str) << "\n";
}
void StringBuilder::AppendLine(const StringBuilder str) {
	this->buffer << str.buffer.str() << "\n";
}
std::string StringBuilder::ToString() {
	return this->buffer.str();
}
std::wstring StringBuilder::ToWString() {
	return Convert::string_to_wstring(this->buffer.str());
}
void StringBuilder::Clear() {
	this->buffer.clear();
}