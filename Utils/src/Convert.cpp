#include "Convert.h"
#include <Windows.h>
#include <iomanip>
#include <iostream>
#include <sstream>
#include "MD5.h"
#include "SHA256.h"

#include <cstdint>
#include <vector>
#include <string>
#include <cstring>
#include <cassert>
#include <stdexcept>

#pragma warning(disable: 4267)
#pragma warning(disable: 4244)
#pragma warning(disable: 4018)

namespace {
static constexpr const char hexChars[] = "0123456789ABCDEF";
static constexpr const wchar_t hexCharsW[] = L"0123456789ABCDEF";

std::string ToHexUInt(uint64_t value, size_t width) {
	std::string output(width, '\0');
	for (size_t i = 0; i < width; ++i) {
		size_t shift = (width - i - 1) * 4;
		output[i] = hexChars[(value >> shift) & 0x0F];
	}
	return output;
}

std::wstring ToHexUIntW(uint64_t value, size_t width) {
	std::wstring output(width, L'\0');
	for (size_t i = 0; i < width; ++i) {
		size_t shift = (width - i - 1) * 4;
		output[i] = hexCharsW[(value >> shift) & 0x0F];
	}
	return output;
}

void WriteHexByte(uint8_t value, char* output) {
	output[0] = hexChars[(value >> 4) & 0x0F];
	output[1] = hexChars[value & 0x0F];
}

void WriteHexByte(uint8_t value, wchar_t* output) {
	output[0] = hexCharsW[(value >> 4) & 0x0F];
	output[1] = hexCharsW[value & 0x0F];
}
}
static constexpr const uint8_t base64_table[] = {
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x3E,0x00,0x00,0x00,0x3F,
	0x34,0x35,0x36,0x37,0x38,0x39,0x3A,0x3B,0x3C,0x3D,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,
	0x0F,0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x00,0x00,0x00,0x00,0x00,
	0x00,0x1A,0x1B,0x1C,0x1D,0x1E,0x1F,0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,
	0x29,0x2A,0x2B,0x2C,0x2D,0x2E,0x2F,0x30,0x31,0x32,0x33,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};
static constexpr const uint8_t base85_table[] = {
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,
	0x0F,0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1A,0x1B,0x1C,0x1D,0x1E,
	0x1F,0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2A,0x2B,0x2C,0x2D,0x2E,
	0x2F,0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x3A,0x3B,0x3C,0x3D,0x3E,
	0x3F,0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4A,0x4B,0x4C,0x4D,0x4E,
	0x4F,0x50,0x51,0x52,0x53,0x54,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};
static constexpr const uint8_t hex_table_str[] = {
	0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,
	0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,
	0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,
	0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x10,0x10,0x10,0x10,0x10,0x10,
	0x10,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,
	0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,
	0x10,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,
	0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,
	0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,
	0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,
	0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,
	0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,
	0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,
	0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,
	0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,
	0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,
};
std::string Convert::ToHex(const uint8_t input) {
	return ToHexUInt(input, 2);
}
std::string Convert::ToHex(const int8_t input) {
	return ToHexUInt(static_cast<uint8_t>(input), 2);
}
std::wstring Convert::ToHexW(const uint8_t input) {
	return ToHexUIntW(input, 2);
}
std::wstring Convert::ToHexW(const int8_t input) {
	return ToHexUIntW(static_cast<uint8_t>(input), 2);
}
std::string Convert::ToHex(const uint16_t input) {
	return ToHexUInt(input, 4);
}
std::string Convert::ToHex(const int16_t input) {
	return ToHexUInt(static_cast<uint16_t>(input), 4);
}
std::wstring Convert::ToHexW(const uint16_t input) {
	return ToHexUIntW(input, 4);
}
std::wstring Convert::ToHexW(const int16_t input) {
	return ToHexUIntW(static_cast<uint16_t>(input), 4);
}
std::string Convert::ToHex(const uint32_t input) {
	return ToHexUInt(input, 8);
}
std::string Convert::ToHex(const int32_t input) {
	return ToHexUInt(static_cast<uint32_t>(input), 8);
}
std::wstring Convert::ToHexW(const uint32_t input) {
	return ToHexUIntW(input, 8);
}
std::wstring Convert::ToHexW(const int32_t input) {
	return ToHexUIntW(static_cast<uint32_t>(input), 8);
}
std::string Convert::ToHex(const uint64_t input) {
	return ToHexUInt(input, 16);
}
std::string Convert::ToHex(const int64_t input) {
	return ToHexUInt(static_cast<uint64_t>(input), 16);
}
std::wstring Convert::ToHexW(const uint64_t input) {
	return ToHexUIntW(input, 16);
}
std::wstring Convert::ToHexW(const int64_t input) {
	return ToHexUIntW(static_cast<uint64_t>(input), 16);
}
std::string Convert::ToHex(const void* input, size_t size) {
	if (input == nullptr && size > 0) return "";
	std::string result(size * 2, '\0');
	const uint8_t* bytes = static_cast<const uint8_t*>(input);
	for (size_t i = 0; i < size; ++i) {
		WriteHexByte(bytes[i], &result[i * 2]);
	}
	return result;
}
std::wstring Convert::ToHexW(const void* input, size_t size) {
	if (input == nullptr && size > 0) return L"";
	std::wstring result(size * 2, '\0');
	const uint8_t* bytes = static_cast<const uint8_t*>(input);
	for (size_t i = 0; i < size; ++i) {
		WriteHexByte(bytes[i], &result[i * 2]);
	}
	return result;
}
std::vector<uint8_t> Convert::FromHex(const std::string hex) {
	std::vector<uint8_t> result = std::vector<uint8_t>();
	uint8_t _highBits = 0;
	bool ish = true;
	for (const char& c : hex) {
		uint8_t v = hex_table_str[static_cast<uint8_t>(c)];
		if (v == 0x10) {
			if (!ish) {
				result.push_back(_highBits);
				ish = true;
			}
			continue;
		}
		if (ish)
			_highBits = v;
		else
			result.push_back((_highBits << 4) | v);
		ish = !ish;
	}
	if (!ish)
		result.push_back(_highBits);
	return result;

}
std::vector<uint8_t> Convert::FromHex(const std::wstring hex) {
	std::vector<uint8_t> result = std::vector<uint8_t>();
	uint8_t _highBits = 0;
	bool ish = true;
	for (const wchar_t& c : hex) {
		if (c > 0xFF)
			continue;
		uint8_t v = hex_table_str[c];
		if (v == 0x10) {
			if (!ish) {
				result.push_back(_highBits);
				ish = true;
			}
			continue;
		}
		if (ish)
			_highBits = v;
		else
			result.push_back((_highBits << 4) | v);
		ish = !ish;
	}
	if (!ish)
		result.push_back(_highBits);
	return result;

}
std::wstring Convert::MultiByteToWide(const std::string& str, uint32_t codePage) {
	if (str.empty()) return L"";
	int len = MultiByteToWideChar(codePage, 0, str.c_str(), static_cast<int>(str.length()), NULL, 0);
	if (len <= 0) return L"";
	std::wstring wstr(len, L'\0');
	MultiByteToWideChar(codePage, 0, str.c_str(), static_cast<int>(str.length()), &wstr[0], len);
	return wstr;
}
std::string Convert::WideToMultiByte(const std::wstring& wstr, uint32_t codePage) {
	if (wstr.empty()) return "";
	int len = ::WideCharToMultiByte(codePage, 0, wstr.c_str(), static_cast<int>(wstr.length()), NULL, 0, NULL, NULL);
	if (len <= 0) return "";
	std::string str(len, '\0');
	WideCharToMultiByte(codePage, 0, wstr.c_str(), static_cast<int>(wstr.length()), &str[0], len, NULL, NULL);
	return str;
}
std::string Convert::AnsiToUtf8(const std::string str) {
	std::wstring wstr = MultiByteToWide(str, CP_ACP);
	return WideToMultiByte(wstr, CP_UTF8);
}
std::string Convert::Utf8ToAnsi(const std::string str) {
	std::wstring wstr = MultiByteToWide(str, CP_UTF8);
	return WideToMultiByte(wstr, CP_ACP);
}
std::u16string Convert::Utf8ToUtf16(const std::string utf8Str) {
	std::wstring wideStr = MultiByteToWide(utf8Str, CP_UTF8);
	return std::u16string(wideStr.begin(), wideStr.end());
}
std::string Convert::Utf16ToUtf8(const std::u16string utf16Str) {
	std::wstring wideStr(utf16Str.begin(), utf16Str.end());
	return WideToMultiByte(wideStr, CP_UTF8);
}
std::u32string Convert::Utf8ToUtf32(const std::string utf8Str) {
	std::wstring wideStr = MultiByteToWide(utf8Str, CP_UTF8);
	return std::u32string(wideStr.begin(), wideStr.end());
}
std::string Convert::Utf32ToUtf8(const std::u32string utf32Str) {
	std::wstring wideStr(utf32Str.begin(), utf32Str.end());
	return WideToMultiByte(wideStr, CP_UTF8);
}
std::wstring Convert::AnsiToUnicode(const std::string ansiStr) {
	return MultiByteToWide(ansiStr, CP_ACP);
}
std::string Convert::UnicodeToAnsi(const std::wstring unicodeStr) {
	return WideToMultiByte(unicodeStr, CP_ACP);
}
std::wstring Convert::Utf8ToUnicode(const std::string utf8Str) {
	return MultiByteToWide(utf8Str, CP_UTF8);
}
std::string Convert::UnicodeToUtf8(const std::wstring unicodeStr) {
	return WideToMultiByte(unicodeStr, CP_UTF8);
}
std::string Convert::WStringToString(const std::wstring wstr) {
	return WideToMultiByte(wstr, CP_ACP);
}
std::wstring Convert::StringToWString(const std::string str) {
	return MultiByteToWide(str, CP_ACP);
}
std::string Convert::ToBase64(const void* data, size_t size) {
	static const char base64_chars[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz"
		"0123456789+/";
	uint8_t* input = (uint8_t*)data;
	std::string output(((size + 2) / 3) * 4, '\0');

	int char_count = 0;
	uint8_t char_array_3[4] = { 0 };
	char char_array_4[4] = { 0 };
	size_t index = 0;
	while (size-- > 0) {
		char_array_3[char_count++] = *input++;
		if (char_count == 3) {
			char_array_4[0] = base64_chars[(char_array_3[0] & 0xFC) >> 2];
			char_array_4[1] = base64_chars[((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xF0) >> 4)];
			char_array_4[2] = base64_chars[((char_array_3[1] & 0x0F) << 2) + ((char_array_3[2] & 0xC0) >> 6)];
			char_array_4[3] = base64_chars[char_array_3[2] & 0x3F];
			for (int i = 0; i < 4; ++i)
				output[index++] = char_array_4[i];
			char_count = 0;
		}
	}

	if (char_count > 0) {
		for (int i = char_count; i < 3; ++i)
			char_array_3[i] = 0;
		char_array_4[0] = base64_chars[(char_array_3[0] & 0xFC) >> 2];
		char_array_4[1] = base64_chars[((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xF0) >> 4)];
		char_array_4[2] = base64_chars[((char_array_3[1] & 0x0F) << 2) + ((char_array_3[2] & 0xC0) >> 6)];
		char_array_4[3] = base64_chars[char_array_3[2] & 0x3F];

		for (int i = 0; i < char_count + 1; i++)
			output[index++] = char_array_4[i];

		while (char_count++ < 3)
			output[index++] = '=';
	}

	return output;
}
std::string Convert::ToBase64(const std::vector<uint8_t>& input) {
	return Convert::ToBase64(input.data(), input.size());
}
std::string Convert::ToBase64(const std::string input) {
	return Convert::ToBase64(input.data(), input.size());
}
std::string Convert::FromBase64(const std::string input) {
	std::string normalized;
	normalized.reserve(input.size());
	for (unsigned char c : input) {
		if (c == ' ' || c == '\t' || c == '\r' || c == '\n') continue;
		normalized.push_back(static_cast<char>(c));
	}
	if (normalized.empty()) return "";

	size_t input_length = normalized.size();
	if (input_length % 4 != 0) {
		throw std::invalid_argument("Invalid base64 length");
	}
	size_t output_length = (input_length / 4) * 3;

	if (normalized[input_length - 1] == '=') output_length--;
	if (normalized[input_length - 2] == '=') output_length--;

	std::string output(output_length, '\0');

	int char_count = 0;
	uint8_t char_array_4[4] = { 0 };
	uint8_t char_array_3[4] = { 0 };
	size_t index = 0;
	for (size_t pos = 0; pos < normalized.size(); ++pos) {
		unsigned char c = static_cast<unsigned char>(normalized[pos]);
		if (c == '=') {
			char_array_4[char_count++] = 0;
		}
		else {
			uint8_t v = base64_table[c];
			if (v == 0 && c != 'A') {
				throw std::invalid_argument("Invalid base64 character");
			}
			char_array_4[char_count++] = v;
		}
		if (char_count == 4) {
			char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
			char_array_3[1] = ((char_array_4[1] & 0xF) << 4) + ((char_array_4[2] & 0x3C) >> 2);
			char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

			for (int i = 0; i < 3 && index < output.size(); ++i) {
				output[index++] = static_cast<char>(char_array_3[i]);
			}
			char_count = 0;
		}
	}

	return output;
}
std::vector<uint8_t> Convert::FromBase64ToBytes(const std::string input) {
	auto tmp = FromBase64(input);
	return std::vector<uint8_t>(tmp.begin(), tmp.end());
}
std::string Convert::ToBase85(const std::string input) {
	static const char base85_chars[] =
		"!\"#$%&'()*+,-./0123456789:;<=>?@"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"[\\]^_`abcdefghijklmnopqrstu";

	size_t input_length = input.size();
	size_t output_length = ((input_length + 3) / 4) * 5;
	std::string output(output_length, '\0');

	uint8_t char_array_4[4] = { 0 };
	char char_array_5[5] = { 0 };
	size_t index = 0;
	int char_count = 0;

	for (const uint8_t& c : input) {
		char_array_4[char_count++] = c;
		if (char_count == 4) {
			uint32_t value =
				(uint32_t(char_array_4[0]) << 24) |
				(uint32_t(char_array_4[1]) << 16) |
				(uint32_t(char_array_4[2]) << 8) |
				uint32_t(char_array_4[3]);

			if (value == 0) {
				output[index++] = 'z';
			}
			else {
				for (int i = 4; i >= 0; --i) {
					char_array_5[i] = base85_chars[value % 85];
					value /= 85;
				}
				for (int i = 0; i < 5; ++i) {
					output[index++] = char_array_5[i];
				}
			}
			char_count = 0;
			for (int i = 0; i < 4; ++i) {
				char_array_4[i] = 0;
			}
		}
	}

	if (char_count > 0) {
		for (int i = char_count; i < 4; ++i)
			char_array_4[i] = 0;
		uint32_t value =
			(uint32_t(char_array_4[0]) << 24) |
			(uint32_t(char_array_4[1]) << 16) |
			(uint32_t(char_array_4[2]) << 8) |
			uint32_t(char_array_4[3]);

		for (int i = 4; i >= 0; --i) {
			char_array_5[i] = base85_chars[value % 85];
			value /= 85;
		}
		for (int i = 0; i < char_count + 1; ++i) {
			output[index++] = char_array_5[i];
		}
	}

	output.resize(index);
	return output;
}
std::string Convert::FromBase85(const std::string input) {
	size_t input_length = input.size();
	size_t output_length = ((input_length + 4) / 5) * 4;
	std::string output(output_length, '\0');

	uint8_t char_array_5[5] = { 0 };
	uint8_t char_array_4[4] = { 0 };
	size_t index = 0;
	int char_count = 0;

	for (size_t i = 0; i < input_length; ++i) {
		if (input[i] == 'z') {
			for (int j = 0; j < 4; ++j) {
				output[index++] = '\0';
			}
			continue;
		}

		char_array_5[char_count++] = base85_table[(uint8_t)input[i]];
		if (char_count == 5) {
			uint32_t value = 0;
			for (int j = 0; j < 5; ++j) {
				value = value * 85 + char_array_5[j];
			}
			char_array_4[0] = (value >> 24) & 0xFF;
			char_array_4[1] = (value >> 16) & 0xFF;
			char_array_4[2] = (value >> 8) & 0xFF;
			char_array_4[3] = value & 0xFF;
			for (int j = 0; j < 4; ++j) {
				output[index++] = static_cast<char>(char_array_4[j]);
			}
			char_count = 0;
		}
	}

	if (char_count > 0) {
		for (int i = char_count; i < 5; ++i) {
			char_array_5[i] = 0x54;
		}
		uint32_t value = 0;
		for (int j = 0; j < 5; ++j) {
			value = value * 85 + char_array_5[j];
		}
		char_array_4[0] = (value >> 24) & 0xFF;
		char_array_4[1] = (value >> 16) & 0xFF;
		char_array_4[2] = (value >> 8) & 0xFF;
		char_array_4[3] = value & 0xFF;
		for (int i = 0; i < char_count - 1; ++i) {
			output[index++] = char_array_4[i];
		}
	}

	output.resize(index);
	return output;
}
std::string Convert::ToBase85(const std::vector<uint8_t>& input) {
	return ToBase85(std::string((char*)input.data(), input.size()));
}
std::vector<uint8_t> Convert::FromBase85ToBytes(const std::string input) {
	auto tmp = FromBase85(input);
	return std::vector<uint8_t>(tmp.begin(), tmp.end());
}
std::string Convert::CalcMD5(const void* data, size_t size) {
	MD5 md5;
	md5.update(reinterpret_cast<const uint8_t*>(data), size);
	md5.finalize();
	return md5.hexdigest();
}
std::string Convert::CalcSHA256(const void* data, size_t size) {
	SHA256 sha256;
	sha256.update(reinterpret_cast<const uint8_t*>(data), size);
	sha256.finalize();
	return sha256.hexdigest();
}
std::string Convert::CalcMD5(const std::vector<uint8_t>& data) {
	MD5 md5;
	md5.update(reinterpret_cast<const uint8_t*>(data.data()), data.size());
	md5.finalize();
	return md5.hexdigest();
}
std::string Convert::CalcSHA256(const std::vector<uint8_t>& data) {
	SHA256 sha256;
	sha256.update(reinterpret_cast<const uint8_t*>(data.data()), data.size());
	sha256.finalize();
	return sha256.hexdigest();
}
std::string Convert::CalcMD5(const std::string& data) {
	MD5 md5;
	md5.update(reinterpret_cast<const uint8_t*>(data.data()), data.size());
	md5.finalize();
	return md5.hexdigest();
}
std::string Convert::CalcSHA256(const std::string& data) {
	SHA256 sha256;
	sha256.update(reinterpret_cast<const uint8_t*>(data.data()), data.size());
	sha256.finalize();
	return sha256.hexdigest();
}
int Convert::ToInt32(const std::string input) {
	return atoi(input.c_str());
}
long long Convert::ToInt64(const std::string input) {
	return _atoi64(input.c_str());
}
double Convert::ToFloat(const std::string input) {
	return atof(input.c_str());
}
