#pragma once
#include <vector>
#include <string>
#include <codecvt>
class Convert {
public:
	static std::string ToHex(const uint8_t input);
	static std::wstring ToHexW(const uint8_t input);
	static std::string ToHex(const int8_t input);
	static std::wstring ToHexW(const int8_t input);
	static std::string ToHex(const uint16_t input);
	static std::wstring ToHexW(const uint16_t input);
	static std::string ToHex(const int16_t input);
	static std::wstring ToHexW(const int16_t input);
	static std::string ToHex(const uint32_t input);
	static std::wstring ToHexW(const uint32_t input);
	static std::string ToHex(const int32_t input);
	static std::wstring ToHexW(const int32_t input);
	static std::string ToHex(const uint64_t input);
	static std::wstring ToHexW(const uint64_t input);
	static std::string ToHex(const int64_t input);
	static std::wstring ToHexW(const int64_t input);

	static std::string ToHex(const void* input, size_t size);
	static std::wstring ToHexW(const void* input, size_t size);

	static std::vector<uint8_t> FromHex(const std::string hex);
	static std::vector<uint8_t> FromHex(const std::wstring hex);

	static std::string AnsiToUtf8(const std::string str);
	static std::string Utf8ToAnsi(const std::string str);
	static std::wstring AnsiToUnicode(const std::string ansiStr);
	static std::string UnicodeToAnsi(const std::wstring unicodeStr);
	static std::u16string Utf8ToUtf16(const std::string utf8Str);
	static std::string Utf16ToUtf8(const std::u16string utf16Str);
	static std::u32string Utf8ToUtf32(const std::string utf8Str);
	static std::string Utf32ToUtf8(const std::u32string utf32Str);
	static std::wstring Utf8ToUnicode(const std::string utf8Str);
	static std::string UnicodeToUtf8(const std::wstring unicodeStr);
	static std::string wstring_to_string(const std::wstring wstr);
	static std::wstring string_to_wstring(const std::string str);
	static std::string ToBase64(const void* data,size_t size);
	static std::string ToBase64(const std::string input);
	static std::string FromBase64(const std::string input);
	static std::string ToBase64(const std::vector<uint8_t>& input);
	static std::vector<uint8_t> FromBase64ToBytes(const std::string input);
	static std::string ToBase85(const std::string input);
	static std::string FromBase85(const std::string input);
	static std::string ToBase85(const std::vector<uint8_t>& input);
	static std::vector<uint8_t> FromBase85ToBytes(const std::string input);

	static std::string CalcMD5(const void* data,size_t size);
	static std::string CalcSHA256(const void* data, size_t size);
	static std::string CalcMD5(const std::vector<uint8_t>& data);
	static std::string CalcSHA256(const std::vector<uint8_t>& data);
	static std::string CalcMD5(const std::string& data);
	static std::string CalcSHA256(const std::string& data);
	static int ToInt32(const std::string input);
	static long long ToInt64(const std::string input);
	static double ToFloat(const std::string input);
#define ToDouble ToFloat

private:
	static std::wstring MultiByteToWide(const std::string& str, uint32_t codePage);
	static std::string WideToMultiByte(const std::wstring& wstr, uint32_t codePage);
};
