#pragma once
#include <windows.h>
#include <winhttp.h>

#include <string>
#include <vector>
#include <functional>
#include <stdexcept>
#include <memory>
#include <algorithm>

#pragma comment(lib, "winhttp.lib")

using CONTENT_RECEIVER = std::function<bool(const char* data, size_t data_length)>;
using DOWNLOAD_PROGRESS = std::function<bool(size_t current, size_t total)>;

class HttpHelper {
public:
    // 同步 GET：返回完整响应体
    // headers 格式: "Key: Value\r\nKey2: Value2"
    // cookies 格式: "name=value; name2=value2" 或 "Cookie: name=value"
    static std::string Get(const std::string& url,
        const std::string& headers = "",
        const std::string& cookies = "") {
        return Request("GET", url, "", headers, cookies, nullptr, nullptr);
    }

    // 同步 POST：返回完整响应体
    // Content-Type 若未在 headers 中指定，默认为 application/x-www-form-urlencoded
    static std::string Post(const std::string& url,
        const std::string& body,
        const std::string& headers = "",
        const std::string& cookies = "") {
        return Request("POST", url, body, headers, cookies, nullptr, nullptr);
    }

    // 流式 GET：每次收到数据块时调用 callback，同时返回完整响应体
    static std::string GetStream(const std::string& url,
        const std::string& headers = "",
        const std::string& cookies = "",
        CONTENT_RECEIVER callback = nullptr,
        DOWNLOAD_PROGRESS progress = nullptr) {
        return Request("GET", url, "", headers, cookies, std::move(callback), std::move(progress));
    }

    // 流式 POST：每次收到数据块时调用 callback，同时返回完整响应体
    static std::string PostStream(const std::string& url,
        const std::string& body,
        const std::string& headers = "",
        const std::string& cookies = "",
        CONTENT_RECEIVER callback = nullptr,
        DOWNLOAD_PROGRESS progress = nullptr) {
        return Request("POST", url, body, headers, cookies, std::move(callback), std::move(progress));
    }

private:
    struct UrlParts {
        bool https = false;
        INTERNET_PORT port = 0;
        std::wstring host;
        std::wstring path_and_query;
    };

    struct HInternetCloser {
        void operator()(HINTERNET h) const {
            if (h) {
                WinHttpCloseHandle(h);
            }
        }
    };

    using UniqueHInternet = std::unique_ptr<void, HInternetCloser>;

    static std::string Request(const std::string& method,
        const std::string& url,
        const std::string& body,
        std::string headers,
        const std::string& cookies,
        CONTENT_RECEIVER callback,
        DOWNLOAD_PROGRESS progress)
    {
        UrlParts parts = ParseUrl(url);

        // POST 默认补 Content-Type
        if (_stricmp(method.c_str(), "POST") == 0 && !HasHeader(headers, "Content-Type")) {
            if (!headers.empty() && headers.back() != '\n') {
                headers += "\r\n";
            }
            headers += "Content-Type: application/x-www-form-urlencoded\r\n";
        }

        // 处理 cookies
        std::string merged_headers = headers;
        AppendCookieHeader(merged_headers, cookies);

        UniqueHInternet hSession(
            WinHttpOpen(L"HttpHelper/1.0",
                WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                WINHTTP_NO_PROXY_NAME,
                WINHTTP_NO_PROXY_BYPASS,
                0));

        if (!hSession) {
            throw std::runtime_error("WinHttpOpen failed: " + std::to_string(GetLastError()));
        }

        // 尝试启用 HTTP/2（支持时）
        EnableModernProtocols(reinterpret_cast<HINTERNET>(hSession.get()));

        // 常用超时，可按需改
        WinHttpSetTimeouts(reinterpret_cast<HINTERNET>(hSession.get()),
            10000,  // resolve
            10000,  // connect
            30000,  // send
            30000); // receive

        UniqueHInternet hConnect(
            WinHttpConnect(reinterpret_cast<HINTERNET>(hSession.get()),
                parts.host.c_str(),
                parts.port,
                0));

        if (!hConnect) {
            throw std::runtime_error("WinHttpConnect failed: " + std::to_string(GetLastError()));
        }

        DWORD flags = parts.https ? WINHTTP_FLAG_SECURE : 0;

        UniqueHInternet hRequest(
            WinHttpOpenRequest(reinterpret_cast<HINTERNET>(hConnect.get()),
                ToWide(method).c_str(),
                parts.path_and_query.c_str(),
                nullptr,
                WINHTTP_NO_REFERER,
                WINHTTP_DEFAULT_ACCEPT_TYPES,
                flags));

        if (!hRequest) {
            throw std::runtime_error("WinHttpOpenRequest failed: " + std::to_string(GetLastError()));
        }

        // 自动跟随重定向
        DWORD redirect_policy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
        WinHttpSetOption(reinterpret_cast<HINTERNET>(hRequest.get()),
            WINHTTP_OPTION_REDIRECT_POLICY,
            &redirect_policy,
            sizeof(redirect_policy));

        // 自动解压（若 SDK/系统支持）
#ifdef WINHTTP_OPTION_DECOMPRESSION
        DWORD decompression = WINHTTP_DECOMPRESSION_FLAG_GZIP | WINHTTP_DECOMPRESSION_FLAG_DEFLATE;
#ifdef WINHTTP_DECOMPRESSION_FLAG_BROTLI
        decompression |= WINHTTP_DECOMPRESSION_FLAG_BROTLI;
#endif
        WinHttpSetOption(reinterpret_cast<HINTERNET>(hRequest.get()),
            WINHTTP_OPTION_DECOMPRESSION,
            &decompression,
            sizeof(decompression));
#endif

        std::wstring wide_headers = ToWide(merged_headers);
        LPCWSTR header_ptr = wide_headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : wide_headers.c_str();
        DWORD header_len = wide_headers.empty() ? 0 : static_cast<DWORD>(wide_headers.size());

        LPCVOID send_body_ptr = body.empty() ? WINHTTP_NO_REQUEST_DATA : body.data();
        DWORD send_body_len = static_cast<DWORD>(body.size());

        BOOL ok = WinHttpSendRequest(
            reinterpret_cast<HINTERNET>(hRequest.get()),
            header_ptr,
            header_len,
            const_cast<LPVOID>(send_body_ptr),
            send_body_len,
            send_body_len,
            0);

        if (!ok) {
            throw std::runtime_error("WinHttpSendRequest failed: " + std::to_string(GetLastError()));
        }

        ok = WinHttpReceiveResponse(reinterpret_cast<HINTERNET>(hRequest.get()), nullptr);
        if (!ok) {
            throw std::runtime_error("WinHttpReceiveResponse failed: " + std::to_string(GetLastError()));
        }

        // 也可以在这里读取状态码并自行判断是否抛错
        // 这里只返回响应体，不强制按状态码失败
        size_t total = QueryContentLength(reinterpret_cast<HINTERNET>(hRequest.get()));
        size_t current = 0;

        std::string response;
        std::vector<char> buffer;

        for (;;) {
            DWORD available = 0;
            if (!WinHttpQueryDataAvailable(reinterpret_cast<HINTERNET>(hRequest.get()), &available)) {
                throw std::runtime_error("WinHttpQueryDataAvailable failed: " + std::to_string(GetLastError()));
            }

            if (available == 0) {
                break;
            }

            buffer.resize(available);

            DWORD downloaded = 0;
            if (!WinHttpReadData(reinterpret_cast<HINTERNET>(hRequest.get()),
                buffer.data(),
                available,
                &downloaded)) {
                throw std::runtime_error("WinHttpReadData failed: " + std::to_string(GetLastError()));
            }

            if (downloaded == 0) {
                break;
            }

            response.append(buffer.data(), downloaded);
            current += downloaded;

            if (callback) {
                if (!callback(buffer.data(), downloaded)) {
                    break;
                }
            }

            if (progress) {
                if (!progress(current, total)) {
                    break;
                }
            }
        }

        return response;
    }

    static void EnableModernProtocols(HINTERNET hSession) {
#ifdef WINHTTP_OPTION_ENABLE_HTTP_PROTOCOL
        DWORD protocols = 0;
#ifdef WINHTTP_PROTOCOL_FLAG_HTTP2
        protocols |= WINHTTP_PROTOCOL_FLAG_HTTP2;
#endif
#ifdef WINHTTP_PROTOCOL_FLAG_HTTP3
        protocols |= WINHTTP_PROTOCOL_FLAG_HTTP3;
#endif
        if (protocols != 0) {
            WinHttpSetOption(hSession,
                WINHTTP_OPTION_ENABLE_HTTP_PROTOCOL,
                &protocols,
                sizeof(protocols));
        }
#endif
    }

    static size_t QueryContentLength(HINTERNET hRequest) {
        wchar_t buf[64] = {};
        DWORD buf_size = sizeof(buf);
        DWORD index = WINHTTP_NO_HEADER_INDEX;

        BOOL ok = WinHttpQueryHeaders(
            hRequest,
            WINHTTP_QUERY_CONTENT_LENGTH,
            WINHTTP_HEADER_NAME_BY_INDEX,
            buf,
            &buf_size,
            &index);

        if (!ok) {
            return 0;
        }

        try {
            return static_cast<size_t>(_wtoi64(buf));
        }
        catch (...) {
            return 0;
        }
    }

    static void AppendCookieHeader(std::string& headers, const std::string& cookies) {
        if (cookies.empty()) {
            return;
        }

        std::string normalized = cookies;
        if (!StartsWithInsensitive(normalized, "Cookie:")) {
            normalized = "Cookie: " + normalized;
        }

        if (!headers.empty() && headers.back() != '\n') {
            headers += "\r\n";
        }

        headers += normalized;
        if (headers.size() < 2 || headers.substr(headers.size() - 2) != "\r\n") {
            headers += "\r\n";
        }
    }

    static bool HasHeader(const std::string& headers, const std::string& key) {
        std::string h = ToLower(headers);
        std::string k = ToLower(key) + ":";
        return h.find(k) != std::string::npos;
    }

    static bool StartsWithInsensitive(const std::string& s, const std::string& prefix) {
        if (s.size() < prefix.size()) {
            return false;
        }
        for (size_t i = 0; i < prefix.size(); ++i) {
            if (tolower(static_cast<unsigned char>(s[i])) != tolower(static_cast<unsigned char>(prefix[i]))) {
                return false;
            }
        }
        return true;
    }

    static std::string ToLower(const std::string& s) {
        std::string out = s;
        std::transform(out.begin(), out.end(), out.begin(),
            [](unsigned char c) { return static_cast<char>(tolower(c)); });
        return out;
    }

    static UrlParts ParseUrl(const std::string& url) {
        std::wstring wurl = ToWide(url);

        URL_COMPONENTS uc{};
        uc.dwStructSize = sizeof(uc);

        wchar_t host_name[256] = {};
        wchar_t url_path[2048] = {};
        wchar_t extra_info[2048] = {};

        uc.lpszHostName = host_name;
        uc.dwHostNameLength = static_cast<DWORD>(std::size(host_name));

        uc.lpszUrlPath = url_path;
        uc.dwUrlPathLength = static_cast<DWORD>(std::size(url_path));

        uc.lpszExtraInfo = extra_info;
        uc.dwExtraInfoLength = static_cast<DWORD>(std::size(extra_info));

        if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &uc)) {
            throw std::runtime_error("WinHttpCrackUrl failed: " + std::to_string(GetLastError()));
        }

        UrlParts parts;
        parts.https = (uc.nScheme == INTERNET_SCHEME_HTTPS);
        parts.port = uc.nPort;
        parts.host.assign(uc.lpszHostName, uc.dwHostNameLength);

        std::wstring path;
        if (uc.dwUrlPathLength > 0) {
            path.assign(uc.lpszUrlPath, uc.dwUrlPathLength);
        }
        else {
            path = L"/";
        }

        if (uc.dwExtraInfoLength > 0) {
            path.append(uc.lpszExtraInfo, uc.dwExtraInfoLength);
        }

        if (path.empty()) {
            path = L"/";
        }

        parts.path_and_query = path;
        return parts;
    }

    static std::wstring ToWide(const std::string& s) {
        if (s.empty()) {
            return L"";
        }

        int len = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
        if (len <= 0) {
            throw std::runtime_error("MultiByteToWideChar failed");
        }

        std::wstring ws(len, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), ws.data(), len);
        return ws;
    }
};