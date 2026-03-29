#include "HttpHelper.h"
#include "Utils.h"
#include "httplib.h"
#include <sstream>
#include <iomanip>

// ---------- 内部辅助函数 ----------

// 将 "Key: Value\r\nKey2: Value2" 格式的字符串解析为 httplib::Headers
static httplib::Headers ParseHeaders(const std::string& headers, const std::string& cookies) {
	httplib::Headers result;

	auto addLine = [&](const std::string& line) {
		auto pos = line.find(": ");
		if (pos != std::string::npos) {
			std::string key = line.substr(0, pos);
			std::string val = line.substr(pos + 2);
			while (!val.empty() && (val.back() == '\r' || val.back() == '\n' || val.back() == ' '))
				val.pop_back();
			if (!key.empty() && !val.empty())
				result.emplace(key, val);
		}
	};

	std::istringstream ss(headers);
	std::string line;
	while (std::getline(ss, line)) {
		if (!line.empty() && line.back() == '\r') line.pop_back();
		addLine(line);
	}

	if (!cookies.empty()) {
		// 支持 "Cookie: value" 或纯 "name=value" 两种格式
		if (cookies.substr(0, 8) == "Cookie: ")
			addLine(cookies);
		else
			result.emplace("Cookie", cookies);
	}

	return result;
}

// 将完整 URL 拆分为 baseUrl（scheme+host+port）和 path（含查询串）
static void SplitUrl(const std::string& url, std::string& baseUrl, std::string& path) {
	size_t schemeEnd = url.find("://");
	size_t hostStart = (schemeEnd == std::string::npos) ? 0 : schemeEnd + 3;
	size_t pathStart = url.find('/', hostStart);

	if (pathStart == std::string::npos) {
		// 没有路径部分，检查是否有查询串
		size_t queryStart = url.find('?', hostStart);
		if (queryStart != std::string::npos) {
			baseUrl = url.substr(0, queryStart);
			path = url.substr(queryStart);
		}
		else {
			baseUrl = url;
			path = "/";
		}
	}
	else {
		baseUrl = url.substr(0, pathStart);
		path = url.substr(pathStart);
	}

	if (schemeEnd == std::string::npos)
		baseUrl = "http://" + baseUrl;
}

// 从已解析的 headers 中取 Content-Type，找不到返回默认值
static std::string GetContentType(const httplib::Headers& hdrs,
	const std::string& defaultType = "application/x-www-form-urlencoded") {
	auto it = hdrs.find("Content-Type");
	if (it != hdrs.end()) return it->second;
	return defaultType;
}

// ---------- 工具函数（保持不变） ----------

std::string HttpHelper::UrlEncode(std::string str) {
	auto input = Convert::AnsiToUtf8(str);
	std::ostringstream encoded;
	for (uint8_t c : input) {
		if (c >= ' ' && c <= '~') {
			encoded << static_cast<char>(c);
		}
		else {
			encoded << '%' << std::uppercase << std::setw(2) << std::setfill('0')
				<< std::hex << static_cast<int>(c);
		}
	}
	return encoded.str();
}

std::string HttpHelper::CheckUrl(std::string input) {
	auto replace = [](std::string& str, const char* old_str, const char* new_str) {
		size_t len_old = strlen(old_str);
		size_t len_new = strlen(new_str);
		size_t pos = str.find(old_str);
		while (pos != std::string::npos) {
			str.replace(pos, len_old, new_str);
			pos = str.find(old_str, pos + len_new);
		}
		};
	replace(input, " ",  "%20");
	replace(input, "\"", "%22");
	replace(input, "#",  "%23");
	replace(input, "%",  "%25");
	replace(input, "&",  "%26");
	replace(input, "(",  "%28");
	replace(input, ")",  "%29");
	replace(input, "+",  "%2B");
	replace(input, ",",  "%2C");
	replace(input, "/",  "%2F");
	replace(input, ":",  "%3A");
	replace(input, ";",  "%3B");
	replace(input, "<",  "%3C");
	replace(input, "=",  "%3D");
	replace(input, ">",  "%3E");
	replace(input, "?",  "%3F");
	replace(input, "@",  "%40");
	replace(input, "\\", "%5C");
	replace(input, "|",  "%7C");
	return UrlEncode(input);
}

std::string HttpHelper::GetHostNameFromURL(std::string url) {
	if (!url._Starts_with("https://") && !url._Starts_with("http://"))
		url = "http://" + url;
	std::string hostName;
	size_t pos = url.find("//");
	if (pos != std::string::npos) {
		pos += 2;
		size_t endPos = url.find('/', pos);
		hostName = (endPos != std::string::npos) ? url.substr(pos, endPos - pos) : url.substr(pos);
	}
	return hostName;
}
// ---------- 同步 GET ----------
std::string HttpHelper::Get(const std::string& url,
	const std::string& headers, const std::string& cookies) {
	std::string baseUrl, path;
	SplitUrl(url, baseUrl, path);

	httplib::Client cli(baseUrl);
	cli.set_follow_location(true);

	auto hdrs = ParseHeaders(headers, cookies);
	auto res = cli.Get(path, hdrs);
	if (res && res->status >= 200 && res->status < 300)
		return res->body;
	return "";
}

// ---------- 同步 POST ----------
std::string HttpHelper::Post(const std::string& url, const std::string& body,
	const std::string& headers, const std::string& cookies) {
	std::string baseUrl, path;
	SplitUrl(url, baseUrl, path);

	httplib::Client cli(baseUrl);
	cli.set_follow_location(true);

	auto hdrs = ParseHeaders(headers, cookies);
	std::string contentType = GetContentType(hdrs);
	// Content-Type 已包含在 hdrs 中，Post 时需从 headers 中移除以避免重复
	hdrs.erase("Content-Type");

	auto res = cli.Post(path, hdrs, body, contentType);
	if (res && res->status >= 200 && res->status < 300)
		return res->body;
	return "";
}

// ---------- 流式 GET ----------
std::string HttpHelper::GetStream(const std::string& url,
	const std::string& headers, const std::string& cookies,
	HTTP_STREAM_CALLBACK callback) {
	std::string baseUrl, path;
	SplitUrl(url, baseUrl, path);

	httplib::Client cli(baseUrl);
	cli.set_follow_location(true);

	auto hdrs = ParseHeaders(headers, cookies);
	std::string response;

	cli.Get(path, hdrs,
		[&](const char* data, size_t len) -> bool {
			std::string chunk(data, len);
			response += chunk;
			if (callback) callback(chunk);
			return true;
		});

	return response;
}

// ---------- 流式 POST ----------
std::string HttpHelper::PostStream(const std::string& url, const std::string& body,
	const std::string& headers, const std::string& cookies,
	HTTP_STREAM_CALLBACK callback) {
	std::string baseUrl, path;
	SplitUrl(url, baseUrl, path);

	httplib::Client cli(baseUrl);
	cli.set_follow_location(true);

	auto hdrs = ParseHeaders(headers, cookies);
	std::string contentType = GetContentType(hdrs);
	hdrs.erase("Content-Type");

	std::string response;

	cli.Post(path, hdrs, body, contentType,
		[&](const char* data, size_t len) -> bool {
			std::string chunk(data, len);
			response += chunk;
			if (callback) callback(chunk);
			return true;
		});

	return response;
}
