#include "Environment.h"
#include <Psapi.h>
#include <ShlObj.h>
#include <vector>

namespace {
	std::string WideToUtf8(const std::wstring& s) {
		if (s.empty()) return "";
		int len = WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()),
			nullptr, 0, nullptr, nullptr);
		if (len <= 0) return "";
		std::string out(static_cast<size_t>(len), '\0');
		WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()),
			out.data(), len, nullptr, nullptr);
		return out;
	}

	std::wstring Utf8ToWide(const std::string& s) {
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
		if (len <= 0) return L"";
		std::wstring out(static_cast<size_t>(len), L'\0');
		MultiByteToWideChar(cp, flags, s.data(), static_cast<int>(s.size()), out.data(), len);
		return out;
	}
}

std::string Environment::CommandLine() {
	return WideToUtf8(GetCommandLineW());
}
std::string Environment::CurrentDirectory() {
	DWORD len = GetCurrentDirectoryW(0, nullptr);
	if (len == 0) return "";
	std::wstring buffer(len, L'\0');
	DWORD written = GetCurrentDirectoryW(len, buffer.data());
	buffer.resize(written);
	return WideToUtf8(buffer);
}
void Environment::CurrentDirectory(std::string path) {
	SetCurrentDirectoryW(Utf8ToWide(path).c_str());
}
std::string Environment::SystemDirectory() {
	UINT len = GetSystemDirectoryW(nullptr, 0);
	if (len == 0) return "";
	std::wstring buffer(len, L'\0');
	UINT written = GetSystemDirectoryW(buffer.data(), len);
	buffer.resize(written);
	return WideToUtf8(buffer);
}
std::string Environment::WindowsDirectory() {
	UINT len = GetWindowsDirectoryW(nullptr, 0);
	if (len == 0) return "";
	std::wstring buffer(len, L'\0');
	UINT written = GetWindowsDirectoryW(buffer.data(), len);
	buffer.resize(written);
	return WideToUtf8(buffer);
}
std::string Environment::MachineName() {
	DWORD size = 0;
	GetComputerNameW(nullptr, &size);
	std::wstring buffer(size, L'\0');
	if (!GetComputerNameW(buffer.data(), &size)) return "";
	buffer.resize(size);
	return WideToUtf8(buffer);
}
int Environment::ProcessorCount() {
	SYSTEM_INFO info;
	GetSystemInfo(&info);
	return info.dwNumberOfProcessors;
}
int Environment::SystemPageSize() {
	SYSTEM_INFO info;
	GetSystemInfo(&info);
	return info.dwPageSize;
}
long long Environment::WorkingSet() {
	PROCESS_MEMORY_COUNTERS pmc;
	GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));
	return pmc.WorkingSetSize;
}
std::string Environment::UserName() {
	DWORD size = 0;
	GetUserNameW(nullptr, &size);
	std::wstring buffer(size, L'\0');
	if (!GetUserNameW(buffer.data(), &size)) return "";
	while (!buffer.empty() && buffer.back() == L'\0') buffer.pop_back();
	return WideToUtf8(buffer);
}
std::vector<std::string> Environment::LogicalDrives() {
	DWORD len = GetLogicalDriveStringsW(0, nullptr);
	std::vector<std::string> result;
	if (len == 0) return result;

	std::wstring buffer(len, L'\0');
	DWORD written = GetLogicalDriveStringsW(len, buffer.data());
	if (written == 0) return result;

	const wchar_t* p = buffer.c_str();
	while (*p) {
		std::wstring drive = p;
		result.push_back(WideToUtf8(drive));
		p += drive.size() + 1;
	}
	return result;
}
std::string Environment::GetFolderPath(SpecialFolder folder, SpecialFolderOption option) {
	wchar_t buffer[MAX_PATH] = {};
	if (SHGetFolderPathW(NULL, (int)folder | (int)option, NULL, 0, buffer) != S_OK) {
		return "";
	}
	return WideToUtf8(buffer);
}
