#pragma once
#include "defines.h"
#include <string>
#include <functional>

// 流式回调：每次收到数据块时调用，参数为当前数据块内容
typedef std::function<void(const std::string&)> HTTP_STREAM_CALLBACK;

class HttpHelper {
public:
	// URL 编码
	static std::string UrlEncode(std::string str);
	// 对 URL 中的特殊字符进行转义
	static std::string CheckUrl(std::string input);
	// 从 URL 中提取 Host 部分
	static std::string GetHostNameFromURL(std::string url);

	// 同步 GET：返回完整响应体
	// headers 格式: "Key: Value\r\nKey2: Value2"
	// cookies 格式: "name=value; name2=value2" 或 "Cookie: name=value"
	static std::string Get(const std::string& url,
		const std::string& headers = "",
		const std::string& cookies = "");

	// 同步 POST：返回完整响应体
	// Content-Type 若未在 headers 中指定，默认为 application/x-www-form-urlencoded
	static std::string Post(const std::string& url,
		const std::string& body,
		const std::string& headers = "",
		const std::string& cookies = "");

	// 流式 GET：每次收到数据块时调用 callback，同时返回完整响应体
	static std::string GetStream(const std::string& url,
		const std::string& headers = "",
		const std::string& cookies = "",
		HTTP_STREAM_CALLBACK callback = nullptr);

	// 流式 POST：每次收到数据块时调用 callback，同时返回完整响应体
	static std::string PostStream(const std::string& url,
		const std::string& body,
		const std::string& headers = "",
		const std::string& cookies = "",
		HTTP_STREAM_CALLBACK callback = nullptr);
};

