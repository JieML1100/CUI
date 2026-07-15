#include "Utils.h"
#include <locale>
#include <Psapi.h>
#include <sstream>
#include <fstream>
#include <codecvt>
#include <iostream>
#include <stdexcept>
#include <TlHelp32.h>

#include <dbghelp.h>
#include <Wtsapi32.h>
#pragma comment(lib, "Dbghelp.lib")
#pragma comment(lib,"Wtsapi32.lib")

#pragma warning(disable: 4267)
#pragma warning(disable: 4244)
#pragma warning(disable: 4018)
DWORD NtBuildVersion() {
	static DWORD res = 0;
	if (!res) {
		typedef LONG(NTAPI* fnRtlGetVersion)(PRTL_OSVERSIONINFOW lpVersionInformation);
		fnRtlGetVersion pRtlGetVersion = NULL;
		while (pRtlGetVersion == NULL) {
			HMODULE ntdll = GetModuleHandle(TEXT("ntdll.dll"));
			if (ntdll) {
				pRtlGetVersion = (fnRtlGetVersion)GetProcAddress(ntdll, "RtlGetVersion");
			}
		}
		RTL_OSVERSIONINFOW osversion{};
		pRtlGetVersion(&osversion);
		res = osversion.dwBuildNumber;
	}
	return res;
}
std::vector<PATTERNVALUE> ParserPattern(std::string text) {
#define IS_HEX(c) (((c) >= '0' && (c) <= '9') || ((c) >= 'a' && (c) <= 'f') || ((c) >= 'A' && (c) <= 'F'))
#define HEX_TO_VALUE(c) (((c) >= '0' && (c) <= '9') ? (c) - '0' : ((c) >= 'a' && (c) <= 'f') ? (c) - 'a' + 10 : ((c) >= 'A' && (c) <= 'F') ? (c) - 'A' + 10 : 0)

	std::vector<PATTERNVALUE> result;
	bool firstChar = TRUE;
	PATTERNVALUE val = { 0 };

	for (char c : text) {
		
		if (IS_HEX(c)) {
			if (firstChar) {
				val.left = HEX_TO_VALUE(c);
				firstChar = FALSE;
			}
			else {
				val.right = HEX_TO_VALUE(c);
				result.push_back(val);
				val = { 0 };
				firstChar = TRUE;
			}
		}
		
		else if (c == '?' || c == '*') {
			if (firstChar) {
				val.left = 0;
				val.ignore_left = 1;
				firstChar = FALSE;
			}
			else {
				val.right = 0;
				val.ignore_right = 1;
				result.push_back(val);
				val = { 0 };
				firstChar = TRUE;
			}
		}
		
		else if (!firstChar && val.ignore_left) {
			val.right = 0;
			val.ignore_right = 1;
			result.push_back(val);
			val = { 0 };
			firstChar = TRUE;
		}
		
		else {
			firstChar = TRUE;
		}
	}
	
	if (!firstChar && (val.left != 0 || val.ignore_left != 0)) {
		val.right = 0;
		val.ignore_right = 1;
		result.push_back(val);
	}
#undef IS_HEX
#undef HEX_TO_VALUE
	return result;
}
void* FindU64(const char* szModule, ULONG64 value) {
	MODULEINFO mi{ };
	if (GetModuleInformation(GetCurrentProcess(), GetModuleHandleA(szModule), &mi, sizeof(mi))) {
		unsigned char* begin = (unsigned char*)mi.lpBaseOfDll;
		DWORD size = mi.SizeOfImage;
		for (unsigned char* p = begin; p <= (begin + size) - 8; p++) {
			if (*(ULONG64*)p == value) {
				return p;
			}
		}
	}
	return NULL;
}
void* FindPattern(const char* szModule, std::string sPattern, int offset) {
	std::vector<PATTERNVALUE> pattern = ParserPattern(sPattern);
	if (pattern.size() == 0) return NULL;
	MODULEINFO mi{ };
	if (GetModuleInformation(GetCurrentProcess(), GetModuleHandleA(szModule), &mi, sizeof(mi))) {
		unsigned char* begin = (unsigned char*)mi.lpBaseOfDll;
		DWORD size = mi.SizeOfImage;
		for (unsigned char* curr = begin + offset; curr <= (begin + size) - pattern.size(); curr++) {
			for (int i = 0; i < pattern.size(); i++) {
				if (pattern[i].ignore == 0x11) continue;
				if (!pattern[i].ignore_left && ((curr[i] & 0xF0) >> 4) != pattern[i].left)goto nxt;
				if (!pattern[i].ignore_right && (curr[i] & 0x0F) != pattern[i].right)goto nxt;
			}
			return curr;
		nxt:;
		}
	}
	return NULL;
}
std::vector<void*> FindAllPattern(const char* szModule, std::string sPattern, int offset) {
	std::vector<void*> result = std::vector<void*>();
	std::vector<PATTERNVALUE> pattern = ParserPattern(sPattern);
	if (pattern.size() == 0) return result;
	MODULEINFO mi{ };
	HMODULE m = NULL;
	if (szModule)
		m = LoadLibraryA(szModule);
	else
		m = GetModuleHandle(NULL);
	if (!m) {
		printf("GetModule Infomation Failed!\n");
		return result;
	}
	if (GetModuleInformation(GetCurrentProcess(), m, &mi, sizeof(mi))) {
		unsigned char* begin = (unsigned char*)mi.lpBaseOfDll;
		DWORD size = mi.SizeOfImage;
		for (unsigned char* curr = begin + offset; curr <= (begin + size) - pattern.size(); curr++) {
			for (int i = 0; i < pattern.size(); i++) {
				if (pattern[i].ignore == 0x11) continue;
				if (!pattern[i].ignore_left && ((curr[i] & 0xF0) >> 4) != pattern[i].left)goto nxt;
				if (!pattern[i].ignore_right && (curr[i] & 0x0F) != pattern[i].right)goto nxt;
			}
			result.push_back(curr);
		nxt:;
		}
	}
	else {
		printf("GetModule Infomation Failed!\n");
	}
	return result;
}
void* FindPattern(void* _begin, std::string sPattern, int search_size, int offset) {
	std::vector<PATTERNVALUE> pattern = ParserPattern(sPattern);
	if (pattern.size() == 0) return NULL;
	unsigned char* begin = (unsigned char*)_begin;
	for (unsigned char* curr = begin + offset; curr <= (begin + search_size) - pattern.size(); curr++) {
		for (int i = 0; i < pattern.size(); i++) {
			if (pattern[i].ignore == 0x11) continue;
			if (!pattern[i].ignore_left && ((curr[i] & 0xF0) >> 4) != pattern[i].left)goto nxt;
			if (!pattern[i].ignore_right && (curr[i] & 0x0F) != pattern[i].right)goto nxt;
		}
		return curr;
	nxt:;
	}
	return NULL;
}
std::vector<void*> FindAllPattern(void* _begin, std::string sPattern, int search_size, int offset) {
	std::vector<void*> result = std::vector<void*>();
	std::vector<PATTERNVALUE> pattern = ParserPattern(sPattern);
	if (pattern.size() == 0) return result;
	unsigned char* begin = (unsigned char*)_begin;
	for (unsigned char* curr = begin + offset; curr <= (begin + search_size) - pattern.size(); curr++) {
		for (int i = 0; i < pattern.size(); i++) {
			if (pattern[i].ignore == 0x11) continue;
			if (!pattern[i].ignore_left && ((curr[i] & 0xF0) >> 4) != pattern[i].left)goto nxt;
			if (!pattern[i].ignore_right && (curr[i] & 0x0F) != pattern[i].right)goto nxt;
		}
		result.push_back(curr);
	nxt:;
	}
	return result;
}
void PrintHex(void* ptr, int count, int splitLine) {
	const char keys[] = "0123456789ABCDEF";
	uint8_t* tmp = (uint8_t*)ptr;
	for (uint8_t* b = tmp; b < tmp + count; b += splitLine) {
		for (int j = 0; j < splitLine && b + j < tmp + count; j++) {
			printf("%c%c ", keys[b[j] / 0x10], keys[b[j] % 0x10]);
		}
		printf("\n");
	}
}
void PrintHex(void* ptr, int count) {
	const char keys[] = "0123456789ABCDEF";
	uint8_t* tmp = (uint8_t*)ptr;
	for (uint8_t* b = tmp; b < tmp + count; b++) {
		printf("%c%c ", keys[*b / 0x10], keys[*b % 0x10]);
	}
}
void MakePermute(std::vector<int> nums, std::vector<std::vector<int>>& result, int start) {
	if (start == nums.size() - 1) {
		result.push_back(nums);
		return;
	}

	for (int i = start; i < nums.size(); i++) {
		std::swap(nums[start], nums[i]);
		MakePermute(nums, result, start + 1);
		std::swap(nums[start], nums[i]);
	}
}
std::wstring GetErrorMessage(DWORD err) {
	LPWSTR errorMsgBuffer = NULL;
	DWORD size = FormatMessageW(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		nullptr,
		err,
		0,
		(LPWSTR)&errorMsgBuffer,
		0,
		nullptr
	);
	if (size > 0) {
		std::wstring emsg = std::wstring(errorMsgBuffer);
		LocalFree(errorMsgBuffer);
		return emsg;
	}
	return L"Unknown error";
}
PIMAGE_NT_HEADERS RtlImageNtHeader(PVOID Base) {
	static PIMAGE_NT_HEADERS(*_RtlImageNtHeader)(PVOID Base) = NULL;
	if (_RtlImageNtHeader == NULL) {
		HMODULE NtBase = GetModuleHandle(TEXT("ntdll.dll"));
		_RtlImageNtHeader = (decltype(_RtlImageNtHeader))GetProcAddress(NtBase, "RtlImageNtHeader");
	}
	return _RtlImageNtHeader(Base);
}
SIZE_T GetSectionSize(_In_ PVOID DllBase) {
	PIMAGE_NT_HEADERS pNTHeader = RtlImageNtHeader(DllBase);
	PIMAGE_SECTION_HEADER pSectionHeader = (PIMAGE_SECTION_HEADER)((DWORD64)pNTHeader + sizeof(IMAGE_NT_HEADERS64));
	ULONG nAlign = pNTHeader->OptionalHeader.SectionAlignment;
	SIZE_T ImageSize = (pNTHeader->OptionalHeader.SizeOfHeaders + nAlign - 1) / nAlign * nAlign;
	for (int i = 0; i < pNTHeader->FileHeader.NumberOfSections; ++i) {
		int CodeSize = pSectionHeader[i].Misc.VirtualSize;
		int LoadSize = pSectionHeader[i].SizeOfRawData;
		int MaxSize = (LoadSize > CodeSize) ? (LoadSize) : (CodeSize);
		int SectionSize = (pSectionHeader[i].VirtualAddress + MaxSize + nAlign - 1) / nAlign * nAlign;
		if (ImageSize < SectionSize)
			ImageSize = SectionSize;
	}
	return ImageSize;
}
void EnableDump() {
	SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
	SetUnhandledExceptionFilter([](EXCEPTION_POINTERS* exceptionInfo) -> LONG {
		char processPath[MAX_PATH];
		GetModuleFileNameA(NULL, processPath, MAX_PATH);
		HANDLE hDumpFile = CreateFileA("crash_dump.dmp", GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);

		if (hDumpFile != INVALID_HANDLE_VALUE) {
			MINIDUMP_EXCEPTION_INFORMATION dumpInfo;
			dumpInfo.ThreadId = GetCurrentThreadId();
			dumpInfo.ExceptionPointers = exceptionInfo;
			dumpInfo.ClientPointers = TRUE;
			MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hDumpFile, MiniDumpNormal, &dumpInfo, NULL, NULL);
			CloseHandle(hDumpFile);
		}
		return EXCEPTION_CONTINUE_SEARCH;
		});
}
List<void*> CaptureStackTraceEx() {
	List<void*> result = List<void*>();
	const int MAX_STACKTRACE = 64;
	void* stack[MAX_STACKTRACE] = {};
	WORD numFrames = RtlCaptureStackBackTrace(0, MAX_STACKTRACE, stack, NULL);
	for (int i = 0; i < MAX_STACKTRACE; ++i) {
		DWORD64 address = reinterpret_cast<DWORD64>(stack[i]);
		result.Add((void*)address);
	}
	return result;
}

std::string MakeDialogFilterStrring(std::string description, std::vector<std::string> filter) {
	std::string result = description;
	result.append("(");
	for (auto& str : filter) {
		result.append(str);
		result.append(";");
	}
	result.append(")");
	result.append(1, '\0');
	for (auto& str : filter) {
		result.append(str);
		result.append(";");
	}
	result.append(2, '\0');
	return result;
}
std::string MakeDialogFilterStrring(std::string description, std::string filter) {
	std::string result = description;
	result.append("(");
	result.append(filter);
	result.append(")");
	result.append(1, '\0');
	result.append(filter);
	result.append(2, '\0');
	return result;
}
