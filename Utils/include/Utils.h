#pragma once
#include "defines.h"
#include <vector>
#include <string>
#include "TimeSpan.h"
#include "StopWatch.h"
#include "StringBuilder.h"
#include "List.h"
#include "File.h"
#include "Guid.h"
#include "Tuple.h"
#include "Dialog.h"
#include "Convert.h"
#include "CRC.h"
#include "CRandom.h"
#include "FileInfo.h"
#include "DateTime.h"
#include "Registry.h"
#include "FileStream.h"
#include "Dictionary.h"
#include "HttpHelper.h"
#include "Environment.h"
#include "StringHelper.h"
#include "Clipboard.h"
#include "Socket.h"


#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#ifndef RELOC
#define RELOC(p,o) (void*)((char*)p ? (((char*)p + o + 4) + (*(int*)((char*)p + o))) : NULL)
#endif
typedef struct _PATTERNVALUE {
	union {
		struct {
			uint8_t right : 4;
			uint8_t left : 4;
		};
		uint8_t value;
	};
	union {
		struct {
			uint8_t ignore_left : 4;
			uint8_t ignore_right : 4;
		};
		uint8_t ignore;
	};
}PATTERNVALUE, * PPATTERNVALUE;
DWORD NtBuildVersion();
std::vector<PATTERNVALUE> ParserPattern(std::string text);
void* FindU64(const char* szModule, ULONG64 value);
void* FindPattern(const char* szModule, std::string sPattern, int offset = 0);
std::vector<void*> FindAllPattern(const char* szModule, std::string sPattern, int offset = 0);
void* FindPattern(void* _begin, std::string sPattern, int search_size, int offset = 0);
std::vector<void*> FindAllPattern(void* _begin, std::string sPattern, int search_size, int offset = 0);
void PrintHex(void* ptr, int count, int splitLine);
void PrintHex(void* ptr, int count);
void MakePermute(std::vector<int> nums, std::vector<std::vector<int>>& result, int start = 0);
std::wstring GetErrorMessage(DWORD err);
PIMAGE_NT_HEADERS RtlImageNtHeader(PVOID Base);
SIZE_T GetSectionSize(_In_ PVOID DllBase);
void EnableDump();
List<void*> CaptureStackTraceEx();
std::string MakeDialogFilterStrring(std::string description, std::vector<std::string> filter);
std::string MakeDialogFilterStrring(std::string description, std::string filter);

