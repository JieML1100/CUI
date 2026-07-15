#include "Process.h"
#include "Convert.h"
#include <winternl.h>
#pragma comment(lib, "ntdll.lib")
Process::Process(int _id) {
	this->Id = _id;
}

Process::Process(int _id, std::wstring _processName) {
	this->Id = _id;
	this->ProcessName = _processName;
}

Process Process::CurrentProcess() {
	return Process((int)GetCurrentProcessId());
}
Process Process::FromHandle(HANDLE handle) {
	return Process((int)GetProcessId(handle));
}
Process Process::FromWindow(HWND hwnd) {
	return Process((int)GetWindowThreadProcessId(hwnd, NULL));
}
Process Process::FromThread(HANDLE handle) {
	return Process((int)GetProcessIdOfThread(handle));
}
std::vector<Process> Process::GetProcessesByName(const std::string _name) {
	std::wstring name = Convert::StringToWString(_name);
	std::vector<Process> result = std::vector<Process>();
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnapshot == INVALID_HANDLE_VALUE) return result;
	PROCESSENTRY32W pe = { sizeof(pe) };
	if (Process32FirstW(hSnapshot, &pe)) {
		do {
			if (_wcsicmp(name.c_str(), pe.szExeFile) == 0) {
				result.push_back(Process(pe.th32ProcessID,Convert::StringToWString(_name)));
			}
		} while (Process32NextW(hSnapshot, &pe));
	}
	CloseHandle(hSnapshot);
	return result;
}
std::vector<Process> Process::GetProcesses() {
	std::vector<Process> result = std::vector<Process>();
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnapshot == INVALID_HANDLE_VALUE) return result;
	PROCESSENTRY32W pe = { sizeof(pe) };
	if (Process32FirstW(hSnapshot, &pe)) {
		do {
			result.push_back(Process(pe.th32ProcessID, pe.szExeFile));
		} while (Process32NextW(hSnapshot, &pe));
	}
	CloseHandle(hSnapshot);
	return result;
}
HWND Process::FindMainWindow(int processId) {
	HWND hwnd = NULL;
	auto tmp = std::pair<int, HWND>(processId, hwnd);
	EnumWindows([](HWND wnd, LPARAM param)->BOOL {
			auto p = (std::pair<int, HWND>*)param;
			DWORD processId;
			if (GetWindowThreadProcessId(wnd, &processId) && processId == p->first) {
				p->second = wnd;
				SetLastError(ERROR_SUCCESS);
				return FALSE;
			}
			return TRUE;
		}, (LPARAM)&tmp);
	return hwnd;
}
std::vector<HWND> Process::GetForms(int processId) {
	std::vector<HWND> result = std::vector<HWND>();
	auto tmp = std::pair<int, std::vector<HWND>&>(processId, result);
	EnumWindows([](HWND wnd, LPARAM param)->BOOL {
			auto p = (std::pair<int, std::vector<HWND>&>*)param;
			DWORD processId;
			if (GetWindowThreadProcessId(wnd, &processId) && processId == p->first) {
				p->second.push_back(wnd);
			}
			return TRUE;
		}, (LPARAM)&tmp);
	return result;
}
std::vector<HWND> Process::Forms() {
	return GetForms(this->Id);
}
Process Process::Start(const std::string fileName, const std::string arguments, const std::string workingDirectory) {
	STARTUPINFOA si = { sizeof(si) };
	PROCESS_INFORMATION pi;
	if (!CreateProcessA(fileName.c_str(), (LPSTR)arguments.c_str(), NULL, NULL, FALSE, 0, NULL, workingDirectory.c_str(), &si, &pi)) {
		return Process(0);
	}
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);
	return Process(pi.dwProcessId);
}
HWND Process::MainWindowHandle() {
	return FindMainWindow(this->Id);
}
std::string Process::MainWindowTitle() {
	HWND hwnd = this->MainWindowHandle();
	if (hwnd == NULL) return "";
	int length = GetWindowTextLength(hwnd);
	if (length == 0) return "";
	std::vector<char> buffer(length + 1);
	GetWindowTextA(hwnd, buffer.data(), length + 1);
	return std::string(buffer.data());
}
HANDLE Process::ParentProcessId() {
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, this->Id);
	if (hProcess == NULL) return NULL;
	PROCESS_BASIC_INFORMATION pbi;
	ULONG returnLength;
	NTSTATUS status = NtQueryInformationProcess(hProcess, ProcessBasicInformation, &pbi, sizeof(pbi), &returnLength);
	CloseHandle(hProcess);
	if (status != 0) return NULL;
	return (HANDLE)pbi.UniqueProcessId;
}
int Process::SessionId() {
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, this->Id);
	if (hProcess == NULL) return NULL;
	DWORD sessionId;
	ProcessIdToSessionId(this->Id, &sessionId);
	CloseHandle(hProcess);
	return sessionId;
}
int Process::BasePriority() {
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, this->Id);
	if (hProcess == NULL) return NULL;
	int priority = GetPriorityClass(hProcess);
	CloseHandle(hProcess);
	return priority;
}
std::string Process::MachineName() {
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, this->Id);
	if (hProcess == NULL) return "";
	std::vector<char> buffer(MAX_PATH);
	DWORD size = MAX_PATH;
	if (!QueryFullProcessImageNameA(hProcess, 0, buffer.data(), &size))
	{
		CloseHandle(hProcess);
		return "";
	}
	CloseHandle(hProcess);
	std::string path(buffer.data(), size);
	if (!path.starts_with("\\\\")) return "";
	const size_t separator = path.find('\\', 2);
	if (separator == std::string::npos) return "";
	return path.substr(2, separator - 2);
}
int Process::ExitCode() {
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, this->Id);
	if (hProcess == NULL) return NULL;
	DWORD exitCode;
	GetExitCodeProcess(hProcess, &exitCode);
	CloseHandle(hProcess);
	return exitCode;
}
bool Process::HasExited() {
	return this->ExitCode() != STILL_ACTIVE;
}
MODULEINFO Process::MainModule() {
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, this->Id);
	if (hProcess == NULL) return MODULEINFO();
	HMODULE hMod;
	DWORD cbNeeded;
	if (EnumProcessModules(hProcess, &hMod, sizeof(hMod), &cbNeeded)) {
		MODULEINFO mi;
		GetModuleInformation(hProcess, hMod, &mi, sizeof(mi));
		CloseHandle(hProcess);
		return mi;
	}
	CloseHandle(hProcess);
	return MODULEINFO();
}
void Process::Kill() {
	HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, this->Id);
	if (hProcess == NULL) return;
	TerminateProcess(hProcess, 0);
	CloseHandle(hProcess);
}
std::vector<MODULEENTRY32> Process::Modules() {
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, this->Id);
	if (hProcess == NULL) return std::vector<MODULEENTRY32>();
	std::vector<MODULEENTRY32> result = std::vector<MODULEENTRY32>();
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, this->Id);
	if (hSnapshot == INVALID_HANDLE_VALUE) return result;
	MODULEENTRY32 me = { sizeof(me) };
	if (Module32First(hSnapshot, &me)) {
		do {
			result.push_back(me);
		} while (Module32Next(hSnapshot, &me));
	}
	CloseHandle(hSnapshot);
	CloseHandle(hProcess);
	return result;
}
std::vector<HANDLE> Process::Threads() {
	std::vector<HANDLE> result = std::vector<HANDLE>();
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, this->Id);
	if (hSnapshot == INVALID_HANDLE_VALUE) return result;
	THREADENTRY32 te = { sizeof(te) };
	if (Thread32First(hSnapshot, &te)) {
		do {
			if (te.th32OwnerProcessID == this->Id) {
				HANDLE hThread = OpenThread(THREAD_QUERY_INFORMATION, FALSE, te.th32ThreadID);
				if (hThread != NULL) {
					result.push_back(hThread);
				}
			}
		} while (Thread32Next(hSnapshot, &te));
	}
	CloseHandle(hSnapshot);
	return result;
}
long long Process::NonpagedSystemMemorySize() {
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, this->Id);
	if (hProcess == NULL) return NULL;
	PROCESS_MEMORY_COUNTERS pmc;
	GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc));
	CloseHandle(hProcess);
	return pmc.QuotaNonPagedPoolUsage;
}
long long Process::PagedMemorySize() {
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, this->Id);
	if (hProcess == NULL) return NULL;
	PROCESS_MEMORY_COUNTERS pmc;
	GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc));
	CloseHandle(hProcess);
	return pmc.QuotaPagedPoolUsage;
}
long long Process::PeakPagedMemorySize() {
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, this->Id);
	if (hProcess == NULL) return NULL;
	PROCESS_MEMORY_COUNTERS pmc;
	GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc));
	CloseHandle(hProcess);
	return pmc.PeakPagefileUsage;
}
long long Process::PeakVirtualMemorySize() {
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, this->Id);
	if (hProcess == NULL) return NULL;
	PROCESS_MEMORY_COUNTERS pmc;
	GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc));
	CloseHandle(hProcess);
	return pmc.PeakWorkingSetSize;
}
long long Process::PeakWorkingSet() {
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, this->Id);
	if (hProcess == NULL) return NULL;
	PROCESS_MEMORY_COUNTERS pmc;
	GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc));
	CloseHandle(hProcess);
	return pmc.PeakWorkingSetSize;
}
long long Process::VirtualMemorySize() {
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, this->Id);
	if (hProcess == NULL) return NULL;
	PROCESS_MEMORY_COUNTERS pmc;
	GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc));
	CloseHandle(hProcess);
	return pmc.WorkingSetSize;
}
bool Process::WaitForInputIdle(int milliseconds) {
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | SYNCHRONIZE, FALSE, this->Id);
	if (hProcess == NULL) return false;
	bool result = ::WaitForInputIdle(hProcess, milliseconds) == 0;
	CloseHandle(hProcess);
	return result;
}
bool Process::WaitForExit(int milliseconds) {
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | SYNCHRONIZE, FALSE, this->Id);
	if (hProcess == NULL) return false;
	bool result = ::WaitForSingleObject(hProcess, milliseconds) == WAIT_OBJECT_0;
	CloseHandle(hProcess);
	return result;
}
long long Process::TotalProcessorTime() {
	FILETIME lpCreationTime, lpExitTime, lpKernelTime, lpUserTime;
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, this->Id);
	if (hProcess == NULL) return NULL;
	if (!GetProcessTimes(hProcess, &lpCreationTime, &lpExitTime, &lpKernelTime, &lpUserTime))
	{
		CloseHandle(hProcess);
		return 0;
	}
	CloseHandle(hProcess);
	ULARGE_INTEGER kernelTime{};
	kernelTime.HighPart = lpKernelTime.dwHighDateTime;
	kernelTime.LowPart = lpKernelTime.dwLowDateTime;
	return static_cast<long long>(kernelTime.QuadPart);
}
long long Process::UserProcessorTime() {
	FILETIME lpCreationTime, lpExitTime, lpKernelTime, lpUserTime;
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, this->Id);
	if (hProcess == NULL) return NULL;
	if (!GetProcessTimes(hProcess, &lpCreationTime, &lpExitTime, &lpKernelTime, &lpUserTime))
	{
		CloseHandle(hProcess);
		return 0;
	}
	CloseHandle(hProcess);
	ULARGE_INTEGER userTime{};
	userTime.HighPart = lpUserTime.dwHighDateTime;
	userTime.LowPart = lpUserTime.dwLowDateTime;
	return static_cast<long long>(userTime.QuadPart);
}
