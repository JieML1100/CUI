#include "StringHelper.h"
#include <stdio.h>
#include <stdarg.h>
#include <format>

#pragma warning(disable: 4267)
#pragma warning(disable: 4244)
#pragma warning(disable: 4018)
std::vector<std::string> StringHelper::Split(std::string str, std::string separator) {
	std::vector<std::string> result = std::vector<std::string>();
	int lastIndex = 0;
	int separatorSize = separator.size();

	for (int i = 0; i <= str.size() - separatorSize; i++) {
		if (str.substr(i, separatorSize) == separator) {
			if (lastIndex != i) {
				result.push_back(std::string(&str[lastIndex], i - lastIndex));
			}
			lastIndex = i + separatorSize;
			i = lastIndex - 1;
		}
	}

	if (lastIndex < str.size()) {
		result.push_back(std::string(&str[lastIndex], str.size() - lastIndex));
	}

	return result;
}
std::vector<std::string> StringHelper::Split(std::string str, std::initializer_list<std::string> separators) {
	std::vector<std::string> result = std::vector<std::string>();
	int lastIndex = 0;

	for (int i = 0; i < str.size(); i++) {
		for (auto separator : separators) {
			int separatorSize = separator.size();
			if (i + separatorSize <= str.size() && str.substr(i, separatorSize) == separator) {
				if (lastIndex != i) {
					result.push_back(std::string(&str[lastIndex], i - lastIndex));
				}
				lastIndex = i + separatorSize;
				i = lastIndex - 1;
				break;
			}
		}
	}

	if (lastIndex < str.size()) {
		result.push_back(std::string(&str[lastIndex], str.size() - lastIndex));
	}

	return result;
}
std::vector<std::string> StringHelper::Split(std::string str, std::initializer_list<char> separators) {
	std::vector<std::string> result = std::vector<std::string>();
	int lastIndex = 0;

	for (int i = 0; i < str.size(); i++) {
		for (auto separator : separators) {
			if (str[i] == separator) {
				if (lastIndex != i) {
					result.push_back(std::string(&str[lastIndex], i - lastIndex));
				}
				lastIndex = i + 1;
				break;
			}
		}
	}

	if (lastIndex < str.size()) {
		result.push_back(std::string(&str[lastIndex], str.size() - lastIndex));
	}

	return result;
}
std::vector<std::wstring> StringHelper::Split(std::wstring str, std::wstring separator) {
	std::vector<std::wstring> result = std::vector<std::wstring>();
	int lastIndex = 0;
	int separatorSize = separator.size();

	for (int i = 0; i <= str.size() - separatorSize; i++) {
		if (str.substr(i, separatorSize) == separator) {
			if (lastIndex != i) {
				result.push_back(std::wstring(&str[lastIndex], i - lastIndex));
			}
			lastIndex = i + separatorSize;
			i = lastIndex - 1;
		}
	}

	if (lastIndex < str.size()) {
		result.push_back(std::wstring(&str[lastIndex], str.size() - lastIndex));
	}

	return result;
}
std::vector<std::wstring> StringHelper::Split(std::wstring str, std::initializer_list<std::wstring> separators) {
	std::vector<std::wstring> result = std::vector<std::wstring>();
	int lastIndex = 0;

	for (int i = 0; i < str.size(); i++) {
		for (auto separator : separators) {
			int separatorSize = separator.size();
			if (i + separatorSize <= str.size() && str.substr(i, separatorSize) == separator) {
				if (lastIndex != i) {
					result.push_back(std::wstring(&str[lastIndex], i - lastIndex));
				}
				lastIndex = i + separatorSize;
				i = lastIndex - 1;
				break;
			}
		}
	}

	if (lastIndex < str.size()) {
		result.push_back(std::wstring(&str[lastIndex], str.size() - lastIndex));
	}

	return result;
}
std::vector<std::wstring> StringHelper::Split(std::wstring str, std::initializer_list<wchar_t> separators) {
	std::vector<std::wstring> result = std::vector<std::wstring>();
	int lastIndex = 0;

	for (int i = 0; i < str.size(); i++) {
		for (auto separator : separators) {
			if (str[i] == separator) {
				if (lastIndex != i) {
					result.push_back(std::wstring(&str[lastIndex], i - lastIndex));
				}
				lastIndex = i + 1;
				break;
			}
		}
	}

	if (lastIndex < str.size()) {
		result.push_back(std::wstring(&str[lastIndex], str.size() - lastIndex));
	}

	return result;
}
std::string StringHelper::Replace(std::string str, std::string oldstr, std::string newstr) {
	std::string result;
	size_t pos = 0;
	size_t prev_pos = 0;
	size_t old_len = oldstr.length();

	while ((pos = str.find(oldstr, pos)) != std::string::npos) {
		result += str.substr(prev_pos, pos - prev_pos);
		result += newstr;
		pos += old_len;
		prev_pos = pos;
	}

	result += str.substr(prev_pos);
	return result;
}
std::wstring StringHelper::Replace(std::wstring str, std::wstring oldstr, std::wstring newstr) {
	std::wstring result;
	size_t pos = 0;
	size_t prev_pos = 0;
	size_t old_len = oldstr.length();

	while ((pos = str.find(oldstr, pos)) != std::string::npos) {
		result += str.substr(prev_pos, pos - prev_pos);
		result += newstr;
		pos += old_len;
		prev_pos = pos;
	}

	result += str.substr(prev_pos);
	return result;
}
std::string StringHelper::ToUpper(std::string str) {
	std::string result(str.size(), '\0');
	for (int i = 0; i < str.size(); i++)
		result[i] = toupper(str[i]);
	return result;
}
std::wstring StringHelper::ToUpper(std::wstring str) {
	std::wstring result(str.size(), L'\0');
	for (int i = 0; i < str.size(); i++)
		result[i] = towupper(str[i]);
	return result;
}
std::string StringHelper::ToLower(std::string str) {
	std::string result(str.size(), '\0');
	for (int i = 0; i < str.size(); i++)
		result[i] = tolower(str[i]);
	return result;
}
std::wstring StringHelper::ToLower(std::wstring str) {
	std::wstring result(str.size(), L'\0');
	for (int i = 0; i < str.size(); i++)
		result[i] = towlower(str[i]);
	return result;
}
std::string StringHelper::Trim(std::string str) {
	std::string result;
	int start = 0;
	int end = str.size() - 1;
	while (start < str.size() && str[start] == ' ') start++;
	while (end >= 0 && str[end] == ' ') end--;
	if (start <= end) {
		result = str.substr(start, end - start + 1);
	}
	return result;
}
std::wstring StringHelper::Trim(std::wstring str) {
	std::wstring result;
	int start = 0;
	int end = str.size() - 1;
	while (start < str.size() && str[start] == ' ') start++;
	while (end >= 0 && str[end] == ' ') end--;
	if (start <= end) {
		result = str.substr(start, end - start + 1);
	}
	return result;
}

std::string StringHelper::TrimLeft(std::string str) {
	std::string result;
	int start = 0;
	int end = str.size() - 1;
	while (start < str.size() && str[start] == ' ') start++;
	if (start <= end) {
		result = str.substr(start, end - start + 1);
	}
	return result;
}
std::wstring StringHelper::TrimLeft(std::wstring str) {
	std::wstring result;
	int start = 0;
	int end = str.size() - 1;
	while (start < str.size() && str[start] == ' ') start++;
	if (start <= end) {
		result = str.substr(start, end - start + 1);
	}
	return result;
}
std::string StringHelper::TrimRight(std::string str) {
	std::string result;
	int start = 0;
	int end = str.size() - 1;
	while (end >= 0 && str[end] == ' ') end--;
	if (start <= end) {
		result = str.substr(start, end - start + 1);
	}
	return result;
}
std::wstring StringHelper::TrimRight(std::wstring str) {
	std::wstring result;
	int start = 0;
	int end = str.size() - 1;
	while (end >= 0 && str[end] == ' ') end--;
	if (start <= end) {
		result = str.substr(start, end - start + 1);
	}
	return result;
}

int StringHelper::IndexOf(std::string str, std::string substr) {
	return str.find(substr);
}
int StringHelper::IndexOf(std::wstring str, std::wstring substr) {
	return str.find(substr);
}
int StringHelper::LastIndexOf(std::string str, std::string substr) {
	return str.rfind(substr);
}
int StringHelper::LastIndexOf(std::wstring str, std::wstring substr) {
	return str.rfind(substr);
}
bool StringHelper::Contains(std::string str, std::string substr) {
	return str.find(substr) != std::string::npos;
}
bool StringHelper::Contains(std::wstring str, std::wstring substr) {
	return str.find(substr) != std::wstring::npos;
}
int StringHelper::GetHashCode(std::string str) {
	int num = 5381;
	int num2 = num;
	char* ptr2 = (char*)str.c_str();
	int num3;
	while ((num3 = *ptr2) != 0) {
		num = ((num << 5) + num) ^ num3;
		num3 = ptr2[1];
		if (num3 == 0) {
			break;
		}
		num2 = ((num2 << 5) + num2) ^ num3;
		ptr2 += 2;
	}
	return num + num2 * 1566083941;
}
int StringHelper::GetHashCode(std::wstring str) {
	int num = 5381;
	int num2 = num;
	wchar_t* ptr2 = (wchar_t*)str.c_str();
	int num3;
	while ((num3 = *ptr2) != 0) {
		num = ((num << 5) + num) ^ num3;
		num3 = ptr2[1];
		if (num3 == 0) {
			break;
		}
		num2 = ((num2 << 5) + num2) ^ num3;
		ptr2 += 2;
	}
	return num + num2 * 1566083941;
}
std::string StringHelper::Insert(std::string str, int index, std::string substr) {
	return str.insert(index, substr);
}
std::wstring StringHelper::Insert(std::wstring str, int index, std::wstring substr) {
	return str.insert(index, substr);
}
std::string StringHelper::Remove(std::string str, int index, int count) {
	return str.erase(index, count);
}
std::wstring StringHelper::Remove(std::wstring str, int index, int count) {
	return str.erase(index, count);
}

std::string StringHelper::Join(std::vector<std::string> strs, std::string separator) {
	std::string result;
	for (int i = 0; i < strs.size(); i++) {
		result += strs[i];
		if (i != strs.size() - 1) {
			result += separator;
		}
	}
	return result;
}
std::wstring StringHelper::Join(std::vector<std::wstring> strs, std::wstring separator) {
	std::wstring result;
	for (int i = 0; i < strs.size(); i++) {
		result += strs[i];
		if (i != strs.size() - 1) {
			result += separator;
		}
	}
	return result;
}

std::wstring StringHelper::Format(const wchar_t* fmt, ...) {
	va_list va;
	va_start(va, fmt);
	int size = __stdio_common_vswprintf(NULL, NULL, 0, fmt, NULL, va);
	std::wstring result(size, L'\0');
	__stdio_common_vswprintf(NULL, &result[0], size + 1, fmt, NULL, va);
	va_end(va);
	return result;
}
std::string StringHelper::Format(const char* fmt, ...) {
	va_list va;
	va_start(va, fmt);
	int size = __stdio_common_vsprintf(NULL, NULL, 0, fmt, NULL, va);
	std::string result(size, '\0');
	__stdio_common_vsprintf(NULL, &result[0], size + 1, fmt, NULL, va);
	va_end(va);
	return result;
}


