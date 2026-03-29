#include "Registry.h"
RegistryKey::RegistryKey(HKEY _hKey) {
	hKey = _hKey;
}
RegistryKey::RegistryKey(HKEY _hKey, const std::string& subKey) {
	hKey = _hKey;
	if (RegOpenKeyExA(hKey, subKey.c_str(), 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
		throw std::exception("Cannot open registry key");
	}
}
RegistryKey::RegistryKey(HKEY _hKey, const std::string& subKey, bool writable) {
	hKey = _hKey;
	if (RegOpenKeyExA(hKey, subKey.c_str(), 0, writable ? KEY_WRITE : KEY_READ, &hKey) != ERROR_SUCCESS) {
		throw std::exception("Cannot open registry key");
	}
}
RegistryKey RegistryKey::CreateSubKey(const std::string& subKey) {
	return CreateSubKey(subKey, false);
}
RegistryKey RegistryKey::CreateSubKey(const std::string& subKey, bool writable) {
	HKEY result;
	if (RegCreateKeyExA(hKey, subKey.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, writable ? KEY_WRITE : KEY_READ, NULL, &result, NULL) != ERROR_SUCCESS) {
		throw std::exception("Cannot create registry key");
	}
	return RegistryKey(result);
}
RegistryKey RegistryKey::OpenSubKey(const std::string& subKey) {
	return OpenSubKey(subKey, false);
}
RegistryKey RegistryKey::OpenSubKey(const std::string& subKey, bool writable) {
	HKEY result;
	if (RegOpenKeyExA(hKey, subKey.c_str(), 0, writable ? KEY_WRITE : KEY_READ, &result) != ERROR_SUCCESS) {
		throw std::exception("Cannot open registry key");
	}
	return RegistryKey(result);
}
std::string RegistryKey::GetValue(const std::string& name) {
	DWORD type;
	DWORD size = 0;
	if (RegQueryValueExA(hKey, name.c_str(), NULL, &type, NULL, &size) != ERROR_SUCCESS) {
		throw std::exception("Cannot query registry value");
	}
	if (type != REG_SZ) {
		throw std::exception("Registry value is not REG_SZ");
	}
	std::string result(size / sizeof(char), '\0');
	if (RegQueryValueExA(hKey, name.c_str(), NULL, NULL, (LPBYTE)result.data(), &size) != ERROR_SUCCESS) {
		throw std::exception("Cannot query registry value");
	}
	return result;
}
void RegistryKey::SetValue(const std::string& name, const std::string& value) {
	if (RegSetValueExA(hKey, name.c_str(), 0, REG_SZ, (LPBYTE)value.c_str(), value.size() * sizeof(char)) != ERROR_SUCCESS) {
		throw std::exception("Cannot set registry value");
	}
}
void RegistryKey::DeleteValue(const std::string& name) {
	if (RegDeleteValueA(hKey, name.c_str()) != ERROR_SUCCESS) {
		throw std::exception("Cannot delete registry value");
	}
}
void RegistryKey::DeleteSubKey(const std::string& subKey) {
	if (RegDeleteKeyA(hKey, subKey.c_str()) != ERROR_SUCCESS) {
		throw std::exception("Cannot delete registry subkey");
	}
}
void RegistryKey::DeleteSubKeyTree(const std::string& subKey) {
	if (RegDeleteTreeA(hKey, subKey.c_str()) != ERROR_SUCCESS) {
		throw std::exception("Cannot delete registry subkey tree");
	}
}
std::vector<std::string> RegistryKey::GetSubKeyNames() {
	DWORD subKeyCount;
	DWORD maxSubKeyNameLength;
	if (RegQueryInfoKeyA(hKey, NULL, NULL, NULL, &subKeyCount, &maxSubKeyNameLength, NULL, NULL, NULL, NULL, NULL, NULL) != ERROR_SUCCESS) {
		throw std::exception("Cannot query registry key info");
	}
	std::vector<std::string> result;
	for (DWORD i = 0; i < subKeyCount; i++) {
		DWORD valueNameSize, valueDataSize, valueType;
		std::string subKeyName(256, '\0');
		FILETIME lastWriteTime{};
		DWORD cbName = 256;
		LSTATUS state = RegEnumKeyExA(hKey, i, (LPSTR)subKeyName.data(), &cbName, NULL, NULL, NULL, &lastWriteTime);
		if (state != ERROR_SUCCESS) {
			throw std::exception("Cannot enumerate registry key");
		}
		subKeyName.resize(cbName);
		result.push_back(subKeyName);
	}
	return result;
}
std::vector<std::string> RegistryKey::GetValueNames() {
	DWORD valueCount;
	DWORD maxValueNameLength;
	if (RegQueryInfoKeyA(hKey, NULL, NULL, NULL, NULL, NULL, NULL, &valueCount, &maxValueNameLength, NULL, NULL, NULL) != ERROR_SUCCESS) {
		throw std::exception("Cannot query registry key info");
	}
	std::vector<std::string> result;
	for (DWORD i = 0; i < valueCount; i++) {
		std::string valueName(256, '\0');
		DWORD cbName = 256;
		if (RegEnumValueA(hKey, i, (LPSTR)valueName.data(), &cbName, NULL, NULL, NULL, NULL) != ERROR_SUCCESS) {
			throw std::exception("Cannot enumerate registry value");
		}
		valueName.resize(cbName);
		result.push_back(valueName);
	}
	return result;
}
void RegistryKey::Close() {
	RegCloseKey(hKey);
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
	return RegistryKey(hKey, "\\\\" + machineName + "\\" + subKey, writable);
}