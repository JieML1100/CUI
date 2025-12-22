#pragma once
#include "defines.h"
#include <string>
#include <vector>
#include <functional>
typedef std::function<void(std::string)> HTTP_STREAM_CALLBACK;
class HttpHelper {
public:
	static std::string UrlEncode(std::string str);
	static std::string CheckUrl(std::string input);
	static std::string GetHostNameFromURL(std::string url);
	static std::string Get(std::string url, std::string headers = "", std::string cookies = "");
	static std::string GetStream(std::string url, std::string headers = "", std::string cookies = "", HTTP_STREAM_CALLBACK callback = NULL);
	static std::vector<uint8_t> HttpGetBytes(std::string url);
	static std::string Post(std::string url, std::string body, std::string headers, std::string cookies = "");
	
	static std::string PostStream(std::string url, std::string body, std::string headers, std::string cookies = "", HTTP_STREAM_CALLBACK callback = NULL);
	static std::string HttpRequest(const std::string& host, const std::string& port,
		const std::string& path, const std::string& method,
		const std::string& data);
	static std::string HttpRequestGet(const std::string& url);
	static std::string HttpRequestPost(const std::string& url, const std::string& data);
};

