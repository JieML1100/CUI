#include "Environment.h"
#include <Psapi.h>
#include <ShlObj.h>
std::string Environment::CommandLine() {
	return GetCommandLineA();
}
std::string Environment::CurrentDirectory() {
	char buffer[MAX_PATH];
	GetCurrentDirectoryA(MAX_PATH, buffer);
	return std::string(buffer);
}
void Environment::CurrentDirectory(std::string path) {
	SetCurrentDirectoryA(path.c_str());
}
std::string Environment::SystemDirectory() {
	char buffer[MAX_PATH];
	GetSystemDirectoryA(buffer, MAX_PATH);
	return std::string(buffer);
}
std::string Environment::WindowsDirectory() {
	char buffer[MAX_PATH];
	GetWindowsDirectoryA(buffer, MAX_PATH);
	return std::string(buffer);
}
std::string Environment::MachineName() {
	char buffer[MAX_COMPUTERNAME_LENGTH + 1];
	DWORD size = MAX_COMPUTERNAME_LENGTH + 1;
	GetComputerNameA(buffer, &size);
	return std::string(buffer);
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
	char buffer[MAX_PATH] = { 0 };
	DWORD size = MAX_PATH;
	GetUserNameA(buffer, &size);
	return std::string(buffer);
}
std::vector<std::string> Environment::LogicalDrives() {
	std::vector<std::string> result;
	char buffer[MAX_PATH];
	GetLogicalDriveStringsA(MAX_PATH, buffer);
	std::string str = std::string(buffer);
	int index = 0;
	while (index < str.size()) {
		std::string drive = str.substr(index, 3);
		result.push_back(drive);
		index += 4;
	}
	return result;
}
std::string Environment::GetFolderPath(SpecialFolder folder, SpecialFolderOption option) {
	char buffer[MAX_PATH] = {};
	SHGetFolderPathA(NULL, (int)folder | (int)option, NULL, 0, buffer);
	return std::string(buffer);
}