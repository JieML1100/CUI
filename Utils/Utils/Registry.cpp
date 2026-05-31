#include "Registry.h"
#include <stdexcept>

namespace {
	std::wstring ToWidePath(const std::string& s) {
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

	bool IsPredefinedKey(HKEY hKey) {
		return hKey == HKEY_CLASSES_ROOT ||
			hKey == HKEY_CURRENT_USER ||
			hKey == HKEY_LOCAL_MACHINE ||
			hKey == HKEY_USERS ||
			hKey == HKEY_PERFORMANCE_DATA ||
			hKey == HKEY_CURRENT_CONFIG ||
			hKey == HKEY_DYN_DATA;
	}

	REGSAM DesiredAccess(bool writable) {
		return writable ? (KEY_READ | KEY_WRITE) : KEY_READ;
	}

	void ThrowWin32(const char* message, LSTATUS status) {
		throw std::runtime_error(std::string(message) + ": " + std::to_string(status));
	}
}

RegistryKey::RegistryKey(HKEY _hKey) {
	hKey = _hKey;
}
RegistryKey::RegistryKey(HKEY _hKey, const std::string& subKey) {
	hKey = _hKey;
	if (RegOpenKeyExW(hKey, ToWidePath(subKey).c_str(), 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
		throw std::runtime_error("Cannot open registry key");
	}
}
RegistryKey::RegistryKey(HKEY _hKey, const std::string& subKey, bool writable) {
	hKey = _hKey;
	if (RegOpenKeyExW(hKey, ToWidePath(subKey).c_str(), 0, DesiredAccess(writable), &hKey) != ERROR_SUCCESS) {
		throw std::runtime_error("Cannot open registry key");
	}
}
RegistryKey RegistryKey::CreateSubKey(const std::string& subKey) {
	return CreateSubKey(subKey, false);
}
RegistryKey RegistryKey::CreateSubKey(const std::string& subKey, bool writable) {
	HKEY result = nullptr;
	LSTATUS status = RegCreateKeyExW(hKey, ToWidePath(subKey).c_str(), 0, NULL,
		REG_OPTION_NON_VOLATILE, DesiredAccess(writable), NULL, &result, NULL);
	if (status != ERROR_SUCCESS) {
		ThrowWin32("Cannot create registry key", status);
	}
	return RegistryKey(result);
}
RegistryKey RegistryKey::OpenSubKey(const std::string& subKey) {
	return OpenSubKey(subKey, false);
}
RegistryKey RegistryKey::OpenSubKey(const std::string& subKey, bool writable) {
	HKEY result = nullptr;
	LSTATUS status = RegOpenKeyExW(hKey, ToWidePath(subKey).c_str(), 0, DesiredAccess(writable), &result);
	if (status != ERROR_SUCCESS) {
		ThrowWin32("Cannot open registry key", status);
	}
	return RegistryKey(result);
}
std::string RegistryKey::GetValue(const std::string& name) {
	DWORD type = 0;
	DWORD size = 0;
	std::wstring wname = ToWidePath(name);
	LSTATUS status = RegQueryValueExW(hKey, wname.c_str(), NULL, &type, NULL, &size);
	if (status != ERROR_SUCCESS) {
		ThrowWin32("Cannot query registry value", status);
	}
	if (type != REG_SZ && type != REG_EXPAND_SZ) {
		throw std::runtime_error("Registry value is not a string");
	}

	std::wstring result(size / sizeof(wchar_t), L'\0');
	status = RegQueryValueExW(hKey, wname.c_str(), NULL, NULL,
		reinterpret_cast<LPBYTE>(result.data()), &size);
	if (status != ERROR_SUCCESS) {
		ThrowWin32("Cannot query registry value", status);
	}
	while (!result.empty() && result.back() == L'\0') result.pop_back();
	return WideToUtf8(result);
}
void RegistryKey::SetValue(const std::string& name, const std::string& value) {
	std::wstring wvalue = ToWidePath(value);
	DWORD bytes = static_cast<DWORD>((wvalue.size() + 1) * sizeof(wchar_t));
	LSTATUS status = RegSetValueExW(hKey, ToWidePath(name).c_str(), 0, REG_SZ,
		reinterpret_cast<const BYTE*>(wvalue.c_str()), bytes);
	if (status != ERROR_SUCCESS) {
		ThrowWin32("Cannot set registry value", status);
	}
}
void RegistryKey::DeleteValue(const std::string& name) {
	LSTATUS status = RegDeleteValueW(hKey, ToWidePath(name).c_str());
	if (status != ERROR_SUCCESS) {
		ThrowWin32("Cannot delete registry value", status);
	}
}
void RegistryKey::DeleteSubKey(const std::string& subKey) {
	LSTATUS status = RegDeleteKeyW(hKey, ToWidePath(subKey).c_str());
	if (status != ERROR_SUCCESS) {
		ThrowWin32("Cannot delete registry subkey", status);
	}
}
void RegistryKey::DeleteSubKeyTree(const std::string& subKey) {
	LSTATUS status = RegDeleteTreeW(hKey, ToWidePath(subKey).c_str());
	if (status != ERROR_SUCCESS) {
		ThrowWin32("Cannot delete registry subkey tree", status);
	}
}
std::vector<std::string> RegistryKey::GetSubKeyNames() {
	DWORD subKeyCount = 0;
	DWORD maxSubKeyNameLength = 0;
	LSTATUS status = RegQueryInfoKeyW(hKey, NULL, NULL, NULL, &subKeyCount,
		&maxSubKeyNameLength, NULL, NULL, NULL, NULL, NULL, NULL);
	if (status != ERROR_SUCCESS) {
		ThrowWin32("Cannot query registry key info", status);
	}

	std::vector<std::string> result;
	result.reserve(subKeyCount);
	std::wstring subKeyName(static_cast<size_t>(maxSubKeyNameLength) + 1, L'\0');
	for (DWORD i = 0; i < subKeyCount; i++) {
		DWORD cbName = static_cast<DWORD>(subKeyName.size());
		FILETIME lastWriteTime{};
		status = RegEnumKeyExW(hKey, i, subKeyName.data(), &cbName, NULL, NULL, NULL, &lastWriteTime);
		if (status != ERROR_SUCCESS) {
			ThrowWin32("Cannot enumerate registry key", status);
		}
		result.push_back(WideToUtf8(std::wstring(subKeyName.data(), cbName)));
	}
	return result;
}
std::vector<std::string> RegistryKey::GetValueNames() {
	DWORD valueCount = 0;
	DWORD maxValueNameLength = 0;
	LSTATUS status = RegQueryInfoKeyW(hKey, NULL, NULL, NULL, NULL, NULL, NULL,
		&valueCount, &maxValueNameLength, NULL, NULL, NULL);
	if (status != ERROR_SUCCESS) {
		ThrowWin32("Cannot query registry key info", status);
	}

	std::vector<std::string> result;
	result.reserve(valueCount);
	std::wstring valueName(static_cast<size_t>(maxValueNameLength) + 1, L'\0');
	for (DWORD i = 0; i < valueCount; i++) {
		DWORD cbName = static_cast<DWORD>(valueName.size());
		status = RegEnumValueW(hKey, i, valueName.data(), &cbName, NULL, NULL, NULL, NULL);
		if (status != ERROR_SUCCESS) {
			ThrowWin32("Cannot enumerate registry value", status);
		}
		result.push_back(WideToUtf8(std::wstring(valueName.data(), cbName)));
	}
	return result;
}
void RegistryKey::Close() {
	if (hKey && !IsPredefinedKey(hKey)) {
		RegCloseKey(hKey);
		hKey = nullptr;
	}
}

RegistryKey Registry::OpenBaseKey(HKEY hKey, const std::string& subKey) {
	return OpenBaseKey(hKey, subKey, false);
}
RegistryKey Registry::OpenBaseKey(HKEY hKey, const std::string& subKey, bool writable) {
	return RegistryKey(hKey, subKey, writable);
}
RegistryKey Registry::OpenRemoteBaseKey(HKEY hKey, const std::string& machineName, const std::string& subKey) {
	return OpenRemoteBaseKey(hKey, machineName, subKey, false);
}
RegistryKey Registry::OpenRemoteBaseKey(HKEY hKey, const std::string& machineName, const std::string& subKey, bool writable) {
	HKEY remoteRoot = nullptr;
	LSTATUS status = RegConnectRegistryW(ToWidePath("\\\\" + machineName).c_str(), hKey, &remoteRoot);
	if (status != ERROR_SUCCESS) {
		ThrowWin32("Cannot connect remote registry", status);
	}
	HKEY result = nullptr;
	status = RegOpenKeyExW(remoteRoot, ToWidePath(subKey).c_str(), 0, DesiredAccess(writable), &result);
	RegCloseKey(remoteRoot);
	if (status != ERROR_SUCCESS) {
		ThrowWin32("Cannot open remote registry key", status);
	}
	return RegistryKey(result);
}
