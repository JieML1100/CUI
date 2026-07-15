#pragma once
#include "defines.h"
#include <vector>
#include <string>
#include <TlHelp32.h>
#include <Psapi.h>
class Process {
public:
	int Id;
	std::wstring ProcessName;
	Process(int _id);
	Process(int _id, std::wstring _processName);
	static Process CurrentProcess();
	static Process FromHandle(HANDLE handle);
	static Process FromWindow(HWND hwnd);
	static Process FromThread(HANDLE handle);
	static std::vector<Process> GetProcessesByName(const std::string _name);
	static std::vector<Process> GetProcesses();
	static HWND FindMainWindow(int processId);
	static std::vector<HWND> GetForms(int processId);
	static Process Start(const std::string fileName, const std::string arguments = "", const std::string workingDirectory = "");
	HWND MainWindowHandle();
	std::vector<HWND> Forms();
	std::string MainWindowTitle();
	HANDLE ParentProcessId();
	int SessionId();
	int BasePriority();
	std::string MachineName();
	int ExitCode();
	bool HasExited();
	MODULEINFO MainModule();
	std::vector<MODULEENTRY32> Modules();
	std::vector<HANDLE> Threads();
	long long NonpagedSystemMemorySize();
	long long PagedMemorySize();
	long long PeakPagedMemorySize();
	long long PeakVirtualMemorySize();
	long long PeakWorkingSet();
	long long VirtualMemorySize();
	bool WaitForInputIdle(int milliseconds = INFINITE);
	bool WaitForExit(int milliseconds = INFINITE);
	long long TotalProcessorTime();
	long long UserProcessorTime();
	void Kill();
};

