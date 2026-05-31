#pragma once
#include <windows.h>
#include <winhttp.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#pragma comment(lib, "winhttp.lib")

using CONTENT_RECEIVER = std::function<bool(const char* data, size_t data_length)>;
using DOWNLOAD_PROGRESS = std::function<bool(size_t current, size_t total)>;

class HttpHelper {
public:
    struct HttpTimeouts {
        DWORD resolveMs = 10000;
        DWORD connectMs = 10000;
        DWORD sendMs = 30000;
        DWORD receiveMs = 30000;
    };

    struct RequestOptions {
        std::string headers;       // "Key: Value\r\nKey2: Value2"
        std::string cookies;       // "name=value; name2=value2" or "Cookie: name=value"
        std::string userAgent = "HttpHelper/1.0";
        std::string proxyName;     // optional, for WINHTTP_ACCESS_TYPE_NAMED_PROXY
        std::string proxyBypass;   // optional
        HttpTimeouts timeouts;
        bool followRedirects = true;
        bool enableCompression = true;
        bool enableHttp2 = true;
        bool enableHttp3 = true;
        bool ignoreCertificateErrors = false;
        bool collectBody = true;   // false + callback gives real streaming without keeping a full body copy
        size_t maxBodyBytes = 0;   // 0 = unlimited
        CONTENT_RECEIVER callback = nullptr;
        DOWNLOAD_PROGRESS progress = nullptr;
    };

    struct HttpResponse {
        DWORD status = 0;
        std::string body;
        std::string rawHeaders;
        std::vector<std::string> setCookies; // each Set-Cookie value without the "Set-Cookie:" prefix
        std::string location;
        size_t contentLength = 0;            // 0 if unknown
        size_t bytesReceived = 0;
        bool abortedByCallback = false;
    };

    static std::string Get(const std::string& url,
        const std::string& headers = "",
        const std::string& cookies = "") {
        RequestOptions options;
        options.headers = headers;
        options.cookies = cookies;
        return Fetch("GET", url, "", options).body;
    }

    static std::string Get(const std::string& url, RequestOptions options) {
        return Fetch("GET", url, "", std::move(options)).body;
    }

    static std::string Post(const std::string& url,
        const std::string& body,
        const std::string& headers = "",
        const std::string& cookies = "") {
        RequestOptions options;
        options.headers = headers;
        options.cookies = cookies;
        return Fetch("POST", url, body, options).body;
    }

    static std::string Post(const std::string& url,
        const std::string& body,
        RequestOptions options) {
        return Fetch("POST", url, body, std::move(options)).body;
    }

    static std::string GetStream(const std::string& url,
        const std::string& headers = "",
        const std::string& cookies = "",
        CONTENT_RECEIVER callback = nullptr,
        DOWNLOAD_PROGRESS progress = nullptr) {
        RequestOptions options;
        options.headers = headers;
        options.cookies = cookies;
        options.callback = std::move(callback);
        options.progress = std::move(progress);
        return Fetch("GET", url, "", options).body;
    }

    static std::string GetStream(const std::string& url, RequestOptions options) {
        return Fetch("GET", url, "", std::move(options)).body;
    }

    static std::string PostStream(const std::string& url,
        const std::string& body,
        const std::string& headers = "",
        const std::string& cookies = "",
        CONTENT_RECEIVER callback = nullptr,
        DOWNLOAD_PROGRESS progress = nullptr) {
        RequestOptions options;
        options.headers = headers;
        options.cookies = cookies;
        options.callback = std::move(callback);
        options.progress = std::move(progress);
        return Fetch("POST", url, body, options).body;
    }

    static std::string PostStream(const std::string& url,
        const std::string& body,
        RequestOptions options) {
        return Fetch("POST", url, body, std::move(options)).body;
    }

    static HttpResponse Fetch(const std::string& method,
        const std::string& url,
        const std::string& body = "",
        std::string headers = "",
        const std::string& cookies = "",
        bool followRedirects = true) {
        RequestOptions options;
        options.headers = std::move(headers);
        options.cookies = cookies;
        options.followRedirects = followRedirects;
        return Fetch(method, url, body, std::move(options));
    }

    static HttpResponse Fetch(const std::string& method,
        const std::string& url,
        const std::string& body,
        RequestOptions options) {
        return RequestFull(method, url, body, std::move(options));
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
            if (h) WinHttpCloseHandle(h);
        }
    };

    using UniqueHInternet = std::unique_ptr<void, HInternetCloser>;

    static HttpResponse RequestFull(const std::string& method,
        const std::string& url,
        const std::string& body,
        RequestOptions options)
    {
        UrlParts parts = ParseUrl(url);

        if (_stricmp(method.c_str(), "POST") == 0 && !HasHeader(options.headers, "Content-Type")) {
            if (!options.headers.empty() && options.headers.back() != '\n') {
                options.headers += "\r\n";
            }
            options.headers += "Content-Type: application/x-www-form-urlencoded\r\n";
        }

        std::string mergedHeaders = options.headers;
        AppendCookieHeader(mergedHeaders, options.cookies);

        std::wstring userAgent = ToWide(options.userAgent);
        std::wstring proxyName = ToWide(options.proxyName);
        std::wstring proxyBypass = ToWide(options.proxyBypass);
        DWORD accessType = proxyName.empty()
            ? WINHTTP_ACCESS_TYPE_DEFAULT_PROXY
            : WINHTTP_ACCESS_TYPE_NAMED_PROXY;

        UniqueHInternet hSession(
            WinHttpOpen(userAgent.empty() ? L"HttpHelper/1.0" : userAgent.c_str(),
                accessType,
                proxyName.empty() ? WINHTTP_NO_PROXY_NAME : proxyName.c_str(),
                proxyBypass.empty() ? WINHTTP_NO_PROXY_BYPASS : proxyBypass.c_str(),
                0));
        if (!hSession) {
            throw std::runtime_error("WinHttpOpen failed: " + std::to_string(GetLastError()));
        }

        EnableModernProtocols(reinterpret_cast<HINTERNET>(hSession.get()), options);

        if (!WinHttpSetTimeouts(reinterpret_cast<HINTERNET>(hSession.get()),
            options.timeouts.resolveMs,
            options.timeouts.connectMs,
            options.timeouts.sendMs,
            options.timeouts.receiveMs)) {
            throw std::runtime_error("WinHttpSetTimeouts failed: " + std::to_string(GetLastError()));
        }

        UniqueHInternet hConnect(
            WinHttpConnect(reinterpret_cast<HINTERNET>(hSession.get()),
                parts.host.c_str(), parts.port, 0));
        if (!hConnect) {
            throw std::runtime_error("WinHttpConnect failed: " + std::to_string(GetLastError()));
        }

        DWORD flags = parts.https ? WINHTTP_FLAG_SECURE : 0;
        std::wstring wideMethod = ToWide(method);
        UniqueHInternet hRequest(
            WinHttpOpenRequest(reinterpret_cast<HINTERNET>(hConnect.get()),
                wideMethod.c_str(),
                parts.path_and_query.c_str(),
                nullptr,
                WINHTTP_NO_REFERER,
                WINHTTP_DEFAULT_ACCEPT_TYPES,
                flags));
        if (!hRequest) {
            throw std::runtime_error("WinHttpOpenRequest failed: " + std::to_string(GetLastError()));
        }

        DWORD redirectPolicy = options.followRedirects
            ? WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS
            : WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
        WinHttpSetOption(reinterpret_cast<HINTERNET>(hRequest.get()),
            WINHTTP_OPTION_REDIRECT_POLICY,
            &redirectPolicy,
            sizeof(redirectPolicy));

        if (options.ignoreCertificateErrors) {
            DWORD securityFlags =
                SECURITY_FLAG_IGNORE_UNKNOWN_CA |
                SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
                SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
                SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
            WinHttpSetOption(reinterpret_cast<HINTERNET>(hRequest.get()),
                WINHTTP_OPTION_SECURITY_FLAGS,
                &securityFlags,
                sizeof(securityFlags));
        }

        if (options.enableCompression) {
            EnableDecompression(reinterpret_cast<HINTERNET>(hRequest.get()));
        }

        std::wstring wideHeaders = ToWide(mergedHeaders);
        LPCWSTR headerPtr = wideHeaders.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : wideHeaders.c_str();
        DWORD headerLen = wideHeaders.empty() ? 0 : static_cast<DWORD>(wideHeaders.size());

        LPCVOID sendBodyPtr = body.empty() ? WINHTTP_NO_REQUEST_DATA : body.data();
        DWORD sendBodyLen = static_cast<DWORD>(body.size());

        BOOL ok = WinHttpSendRequest(
            reinterpret_cast<HINTERNET>(hRequest.get()),
            headerPtr, headerLen,
            const_cast<LPVOID>(sendBodyPtr),
            sendBodyLen, sendBodyLen, 0);
        if (!ok) {
            throw std::runtime_error("WinHttpSendRequest failed: " + std::to_string(GetLastError()));
        }

        ok = WinHttpReceiveResponse(reinterpret_cast<HINTERNET>(hRequest.get()), nullptr);
        if (!ok) {
            throw std::runtime_error("WinHttpReceiveResponse failed: " + std::to_string(GetLastError()));
        }

        HttpResponse result;
        result.status = QueryStatusCode(reinterpret_cast<HINTERNET>(hRequest.get()));
        result.rawHeaders = QueryRawHeaders(reinterpret_cast<HINTERNET>(hRequest.get()));
        result.contentLength = QueryContentLength(reinterpret_cast<HINTERNET>(hRequest.get()));
        ParseResponseHeaders(result.rawHeaders, result.setCookies, result.location);

        std::vector<char> buffer;
        for (;;) {
            DWORD available = 0;
            if (!WinHttpQueryDataAvailable(reinterpret_cast<HINTERNET>(hRequest.get()), &available)) {
                throw std::runtime_error("WinHttpQueryDataAvailable failed: " + std::to_string(GetLastError()));
            }
            if (available == 0) break;

            buffer.resize(available);
            DWORD downloaded = 0;
            if (!WinHttpReadData(reinterpret_cast<HINTERNET>(hRequest.get()),
                buffer.data(), available, &downloaded)) {
                throw std::runtime_error("WinHttpReadData failed: " + std::to_string(GetLastError()));
            }
            if (downloaded == 0) break;

            if (options.maxBodyBytes != 0 && result.bytesReceived + downloaded > options.maxBodyBytes) {
                throw std::runtime_error("HTTP response exceeds maxBodyBytes");
            }

            result.bytesReceived += downloaded;
            if (options.collectBody) {
                result.body.append(buffer.data(), downloaded);
            }

            if (options.callback && !options.callback(buffer.data(), downloaded)) {
                result.abortedByCallback = true;
                break;
            }

            if (options.progress && !options.progress(result.bytesReceived, result.contentLength)) {
                result.abortedByCallback = true;
                break;
            }
        }

        return result;
    }

    static DWORD QueryStatusCode(HINTERNET hRequest) {
        DWORD code = 0;
        DWORD size = sizeof(code);
        DWORD index = WINHTTP_NO_HEADER_INDEX;
        WinHttpQueryHeaders(hRequest,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &code, &size, &index);
        return code;
    }

    static std::string QueryRawHeaders(HINTERNET hRequest) {
        DWORD size = 0;
        WinHttpQueryHeaders(hRequest,
            WINHTTP_QUERY_RAW_HEADERS_CRLF,
            WINHTTP_HEADER_NAME_BY_INDEX,
            WINHTTP_NO_OUTPUT_BUFFER, &size, WINHTTP_NO_HEADER_INDEX);
        if (size == 0) return std::string();

        std::wstring wbuf(size / sizeof(wchar_t), L'\0');
        if (!WinHttpQueryHeaders(hRequest,
            WINHTTP_QUERY_RAW_HEADERS_CRLF,
            WINHTTP_HEADER_NAME_BY_INDEX,
            wbuf.data(), &size, WINHTTP_NO_HEADER_INDEX)) {
            return std::string();
        }
        while (!wbuf.empty() && wbuf.back() == L'\0') wbuf.pop_back();
        return WideToUtf8(wbuf);
    }

    static void ParseResponseHeaders(const std::string& raw,
        std::vector<std::string>& setCookies,
        std::string& location) {
        size_t pos = 0;
        while (pos < raw.size()) {
            size_t eol = raw.find("\r\n", pos);
            std::string line = (eol == std::string::npos)
                ? raw.substr(pos)
                : raw.substr(pos, eol - pos);
            if (!line.empty()) {
                if (StartsWithInsensitive(line, "Set-Cookie:")) {
                    setCookies.push_back(TrimHeaderValue(line.substr(strlen("Set-Cookie:"))));
                }
                else if (StartsWithInsensitive(line, "Location:")) {
                    location = TrimHeaderValue(line.substr(strlen("Location:")));
                }
            }
            if (eol == std::string::npos) break;
            pos = eol + 2;
        }
    }

    static void EnableModernProtocols(HINTERNET hSession, const RequestOptions& options) {
#ifdef WINHTTP_OPTION_ENABLE_HTTP_PROTOCOL
        DWORD protocols = 0;
#ifdef WINHTTP_PROTOCOL_FLAG_HTTP2
        if (options.enableHttp2) protocols |= WINHTTP_PROTOCOL_FLAG_HTTP2;
#endif
#ifdef WINHTTP_PROTOCOL_FLAG_HTTP3
        if (options.enableHttp3) protocols |= WINHTTP_PROTOCOL_FLAG_HTTP3;
#endif
        if (protocols != 0) {
            WinHttpSetOption(hSession,
                WINHTTP_OPTION_ENABLE_HTTP_PROTOCOL,
                &protocols,
                sizeof(protocols));
        }
#endif
    }

    static void EnableDecompression(HINTERNET hRequest) {
#ifdef WINHTTP_OPTION_DECOMPRESSION
        DWORD decompression = WINHTTP_DECOMPRESSION_FLAG_GZIP | WINHTTP_DECOMPRESSION_FLAG_DEFLATE;
#ifdef WINHTTP_DECOMPRESSION_FLAG_BROTLI
        decompression |= WINHTTP_DECOMPRESSION_FLAG_BROTLI;
#endif
        WinHttpSetOption(hRequest,
            WINHTTP_OPTION_DECOMPRESSION,
            &decompression,
            sizeof(decompression));
#endif
    }

    static size_t QueryContentLength(HINTERNET hRequest) {
        wchar_t buf[64] = {};
        DWORD bufSize = sizeof(buf);
        DWORD index = WINHTTP_NO_HEADER_INDEX;

        BOOL ok = WinHttpQueryHeaders(
            hRequest,
            WINHTTP_QUERY_CONTENT_LENGTH,
            WINHTTP_HEADER_NAME_BY_INDEX,
            buf,
            &bufSize,
            &index);
        if (!ok) return 0;
        return static_cast<size_t>(_wtoi64(buf));
    }

    static void AppendCookieHeader(std::string& headers, const std::string& cookies) {
        if (cookies.empty()) return;

        std::string normalized = cookies;
        if (!StartsWithInsensitive(normalized, "Cookie:")) {
            normalized = "Cookie: " + normalized;
        }

        if (!headers.empty() && headers.back() != '\n') headers += "\r\n";
        headers += normalized;
        if (headers.size() < 2 || headers.substr(headers.size() - 2) != "\r\n") {
            headers += "\r\n";
        }
    }

    static bool HasHeader(const std::string& headers, const std::string& key) {
        std::string wanted = ToLower(key);
        size_t pos = 0;
        while (pos < headers.size()) {
            size_t eol = headers.find('\n', pos);
            std::string line = (eol == std::string::npos)
                ? headers.substr(pos)
                : headers.substr(pos, eol - pos);
            while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) {
                line.pop_back();
            }
            size_t start = line.find_first_not_of(" \t");
            if (start != std::string::npos) line.erase(0, start);
            size_t colon = line.find(':');
            if (colon != std::string::npos && ToLower(line.substr(0, colon)) == wanted) {
                return true;
            }
            if (eol == std::string::npos) break;
            pos = eol + 1;
        }
        return false;
    }

    static bool StartsWithInsensitive(const std::string& s, const std::string& prefix) {
        if (s.size() < prefix.size()) return false;
        for (size_t i = 0; i < prefix.size(); ++i) {
            if (tolower(static_cast<unsigned char>(s[i])) !=
                tolower(static_cast<unsigned char>(prefix[i]))) {
                return false;
            }
        }
        return true;
    }

    static std::string TrimHeaderValue(std::string value) {
        while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.erase(value.begin());
        while (!value.empty() && (value.back() == '\r' || value.back() == '\n' ||
            value.back() == ' ' || value.back() == '\t')) value.pop_back();
        return value;
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
        uc.dwSchemeLength = static_cast<DWORD>(-1);
        uc.dwHostNameLength = static_cast<DWORD>(-1);
        uc.dwUrlPathLength = static_cast<DWORD>(-1);
        uc.dwExtraInfoLength = static_cast<DWORD>(-1);

        if (!WinHttpCrackUrl(wurl.c_str(), static_cast<DWORD>(wurl.size()), 0, &uc)) {
            throw std::runtime_error("WinHttpCrackUrl failed: " + std::to_string(GetLastError()));
        }

        UrlParts parts;
        parts.https = (uc.nScheme == INTERNET_SCHEME_HTTPS);
        parts.port = uc.nPort;
        if (uc.lpszHostName && uc.dwHostNameLength) {
            parts.host.assign(uc.lpszHostName, uc.dwHostNameLength);
        }
        if (uc.lpszUrlPath && uc.dwUrlPathLength) {
            parts.path_and_query.assign(uc.lpszUrlPath, uc.dwUrlPathLength);
        }
        else {
            parts.path_and_query = L"/";
        }
        if (uc.lpszExtraInfo && uc.dwExtraInfoLength) {
            parts.path_and_query.append(uc.lpszExtraInfo, uc.dwExtraInfoLength);
        }
        if (parts.path_and_query.empty()) parts.path_and_query = L"/";
        return parts;
    }

    static std::wstring ToWide(const std::string& s) {
        if (s.empty()) return L"";
        int len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
            s.data(), static_cast<int>(s.size()), nullptr, 0);
        UINT cp = CP_UTF8;
        DWORD flags = MB_ERR_INVALID_CHARS;
        if (len <= 0) {
            cp = CP_ACP;
            flags = 0;
            len = MultiByteToWideChar(cp, flags, s.data(), static_cast<int>(s.size()), nullptr, 0);
        }
        if (len <= 0) {
            throw std::runtime_error("MultiByteToWideChar failed");
        }
        std::wstring ws(static_cast<size_t>(len), L'\0');
        MultiByteToWideChar(cp, flags, s.data(), static_cast<int>(s.size()), ws.data(), len);
        return ws;
    }

    static std::string WideToUtf8(const std::wstring& ws) {
        if (ws.empty()) return std::string();
        int len = WideCharToMultiByte(CP_UTF8, 0, ws.data(), static_cast<int>(ws.size()),
            nullptr, 0, nullptr, nullptr);
        if (len <= 0) return std::string();
        std::string out(static_cast<size_t>(len), '\0');
        WideCharToMultiByte(CP_UTF8, 0, ws.data(), static_cast<int>(ws.size()),
            out.data(), len, nullptr, nullptr);
        return out;
    }
};
