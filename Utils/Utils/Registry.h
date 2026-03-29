#pragma once
#include "defines.h"
#include <string>
#include <vector>
class RegistryKey {
public:
	HKEY hKey;
	RegistryKey(HKEY _hKey);
	RegistryKey(HKEY _hKey, const std::string& subKey);
	RegistryKey(HKEY _hKey, const std::string& subKey, bool writable);
	RegistryKey CreateSubKey(const std::string& subKey);
	RegistryKey CreateSubKey(const std::string& subKey, bool writable);
	RegistryKey OpenSubKey(const std::string& subKey);
	RegistryKey OpenSubKey(const std::string& subKey, bool writable);
	std::string GetValue(const std::string& name);
	void SetValue(const std::string& name, const std::string& value);
	void DeleteValue(const std::string& name);
	void DeleteSubKey(const std::string& subKey);
	void DeleteSubKeyTree(const std::string& subKey);
	std::vector<std::string> GetSubKeyNames();
	std::vector<std::string> GetValueNames();
	void Close();
};
class Registry {
public:
	static RegistryKey OpenBaseKey(HKEY hKey, const std::string& subKey);
	static RegistryKey OpenBaseKey(HKEY hKey, const std::string& subKey, bool writable);
	static RegistryKey OpenRemoteBaseKey(HKEY hKey, const std::string& machineName, const std::string& subKey);
	static RegistryKey OpenRemoteBaseKey(HKEY hKey, const std::string& machineName, const std::string& subKey, bool writable);
};

static RegistryKey ClassesRoot = RegistryKey(HKEY_CLASSES_ROOT);
static RegistryKey CurrentConfig = RegistryKey(HKEY_CURRENT_CONFIG);
static RegistryKey CurrentUser = RegistryKey(HKEY_CURRENT_USER);
static RegistryKey LocalMachine = RegistryKey(HKEY_LOCAL_MACHINE);
static RegistryKey Users = RegistryKey(HKEY_USERS);
