#include "WebBrowser.h"

#include <cwchar>

namespace
{
	int HexValue(wchar_t value) noexcept
	{
		if (value >= L'0' && value <= L'9')
			return static_cast<int>(value - L'0');
		if (value >= L'a' && value <= L'f')
			return 10 + static_cast<int>(value - L'a');
		if (value >= L'A' && value <= L'F')
			return 10 + static_cast<int>(value - L'A');
		return -1;
	}

	std::wstring ToWideUtf8(const std::string& value)
	{
		if (value.empty()) return L"";
		const int length = MultiByteToWideChar(
			CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()),
			nullptr, 0);
		std::wstring result(length, L'\0');
		if (length > 0)
			MultiByteToWideChar(
				CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()),
				result.data(), length);
		return result;
	}

	std::string ToUtf8(const std::wstring& value)
	{
		if (value.empty()) return "";
		const int length = WideCharToMultiByte(
			CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()),
			nullptr, 0, nullptr, nullptr);
		std::string result(length, '\0');
		if (length > 0)
			WideCharToMultiByte(
				CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()),
				result.data(), length, nullptr, nullptr);
		return result;
	}
}

// String/URL helpers are kept out of the WebView2 archive member so this
// platform-neutral code never carries WebView2 loader directives with it.
std::wstring WebBrowser::JsStringLiteral(const std::wstring& value)
{
	std::wstring result;
	result.reserve(value.size() + 8);
	result.push_back(L'"');
	for (wchar_t character : value)
	{
		switch (character)
		{
		case L'\\': result += L"\\\\"; break;
		case L'"': result += L"\\\""; break;
		case L'\r': result += L"\\r"; break;
		case L'\n': result += L"\\n"; break;
		case L'\t': result += L"\\t"; break;
		default:
			if (character >= 0 && character < 0x20)
			{
				wchar_t buffer[8];
				swprintf_s(
					buffer, L"\\u%04x", static_cast<unsigned>(character));
				result += buffer;
			}
			else
			{
				result.push_back(character);
			}
			break;
		}
	}
	result.push_back(L'"');
	return result;
}

std::wstring WebBrowser::UrlEncodeUtf8(const std::wstring& value)
{
	const auto isUnreserved = [](unsigned char character) noexcept
	{
		if (character >= 'a' && character <= 'z') return true;
		if (character >= 'A' && character <= 'Z') return true;
		if (character >= '0' && character <= '9') return true;
		switch (character)
		{
		case '-': case '_': case '.': case '~': return true;
		default: return false;
		}
	};

	const std::string utf8 = ToUtf8(value);
	std::wstring result;
	result.reserve(utf8.size() * 3);
	for (unsigned char character : utf8)
	{
		if (isUnreserved(character))
		{
			result.push_back(static_cast<wchar_t>(character));
		}
		else
		{
			wchar_t buffer[4];
			swprintf_s(
				buffer, L"%%%02X", static_cast<unsigned>(character));
			result.append(buffer);
		}
	}
	return result;
}

std::wstring WebBrowser::UrlDecodeUtf8(const std::wstring& value)
{
	std::string bytes;
	bytes.reserve(value.size());
	for (size_t index = 0; index < value.size(); ++index)
	{
		const wchar_t character = value[index];
		if (character == L'%' && index + 2 < value.size())
		{
			const int high = HexValue(value[index + 1]);
			const int low = HexValue(value[index + 2]);
			if (high >= 0 && low >= 0)
			{
				bytes.push_back(static_cast<char>((high << 4) | low));
				index += 2;
				continue;
			}
		}
		// encodeURIComponent does not produce '+', but accept form-style input.
		if (character == L'+')
			bytes.push_back(' ');
		else
			bytes.push_back(static_cast<char>(character & 0xFF));
	}
	return ToWideUtf8(bytes);
}

bool WebBrowser::TryParseCuiUrl(
	const std::wstring& url,
	std::wstring& outAction,
	std::unordered_map<std::wstring, std::wstring>& outQuery)
{
	outAction.clear();
	outQuery.clear();

	constexpr const wchar_t* prefix = L"cui://";
	if (url.rfind(prefix, 0) != 0) return false;

	const std::wstring rest = url.substr(std::wcslen(prefix));
	const size_t queryPosition = rest.find(L'?');
	const std::wstring action = queryPosition == std::wstring::npos
		? rest
		: rest.substr(0, queryPosition);
	if (action.empty()) return false;
	outAction = action;

	if (queryPosition == std::wstring::npos) return true;
	const std::wstring query = rest.substr(queryPosition + 1);
	size_t position = 0;
	while (position < query.size())
	{
		const size_t ampersand = query.find(L'&', position);
		const std::wstring pair = ampersand == std::wstring::npos
			? query.substr(position)
			: query.substr(position, ampersand - position);
		position = ampersand == std::wstring::npos
			? query.size()
			: ampersand + 1;
		if (pair.empty()) continue;

		const size_t equals = pair.find(L'=');
		const std::wstring key = equals == std::wstring::npos
			? pair
			: pair.substr(0, equals);
		const std::wstring queryValue = equals == std::wstring::npos
			? L""
			: pair.substr(equals + 1);
		if (!key.empty()) outQuery[key] = queryValue;
	}
	return true;
}
