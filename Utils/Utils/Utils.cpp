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
#ifdef ZLIB_VERSION
std::vector<uint8_t> Compress(uint8_t* buffer, int len) {
	std::vector<uint8_t> result = std::vector<uint8_t>();
	uLong destLen = compressBound(len);
	unsigned char* ostream = (unsigned char*)malloc(destLen);
	int res = compress(ostream, &destLen, (const unsigned char*)buffer, len);
	if (res == Z_BUF_ERROR) {
		return result;
	}
	if (res == Z_MEM_ERROR) {
		return result;
	}
	int ldx = result.size();
	result.resize(destLen);
	memcpy(&result.operator[](ldx), ostream, destLen);

	return result;
}
std::vector<uint8_t> Decompress(uint8_t* buffer, int len, int maxlen) {
	uLong destLen = maxlen;
	std::vector<uint8_t> result = std::vector<uint8_t>();
	uint8_t* o2stream = new uint8_t[maxlen];
	int des = uncompress(o2stream, &destLen, buffer, len);
	result.resize(destLen);
	memcpy(result.data(), o2stream, destLen);

	return result;
}
std::vector<uint8_t> GDecompress(std::vector<uint8_t> compressedBytes) {
	std::vector<uint8_t> uncompressedBytes = std::vector<uint8_t>();
	if (compressedBytes.size() == 0)return uncompressedBytes;
	unsigned full_length = compressedBytes.size();
	unsigned half_length = compressedBytes.size() / 2;

	unsigned uncompLength = full_length;
	char* uncomp = (char*)calloc(sizeof(char), uncompLength);

	z_stream strm;
	strm.next_in = (Bytef*)compressedBytes.data();
	strm.avail_in = compressedBytes.size();
	strm.total_out = 0;
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;

	bool done = false;

	if (inflateInit2(&strm, (16 + MAX_WBITS)) != Z_OK) {
		free(uncomp);
		return uncompressedBytes;
	}

	while (!done) {
		if (strm.total_out >= uncompLength) {
			char* uncomp2 = (char*)calloc(sizeof(char), uncompLength + half_length);
			memcpy(uncomp2, uncomp, uncompLength);
			uncompLength += half_length;
			free(uncomp);
			uncomp = uncomp2;
		}

		strm.next_out = (Bytef*)(uncomp + strm.total_out);
		strm.avail_out = uncompLength - strm.total_out;
		int err = inflate(&strm, Z_SYNC_FLUSH);
		if (err == Z_STREAM_END) done = true;
		else if (err != Z_OK) {
			break;
		}
	}

	if (inflateEnd(&strm) != Z_OK) {
		free(uncomp);
		return uncompressedBytes;
	}

	for (size_t i = 0; i < strm.total_out; ++i) {
		uncompressedBytes.push_back(uncomp[i]);
	}
	free(uncomp);
	return uncompressedBytes;
}
std::vector<uint8_t> GCompress(std::vector<uint8_t> input) {
	std::vector<uint8_t> res = std::vector<uint8_t>();
	res.resize(input.size());
	z_stream zs;
	zs.zalloc = Z_NULL;
	zs.zfree = Z_NULL;
	zs.opaque = Z_NULL;
	zs.avail_in = (uInt)input.size();
	zs.next_in = (Bytef*)input.data();
	zs.avail_out = (uInt)input.size();
	zs.next_out = (Bytef*)res.data();
	deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 | 16, 8, Z_DEFAULT_STRATEGY);
	deflate(&zs, Z_FINISH);
	deflateEnd(&zs);
	res.resize(zs.total_out);
	return res;
}
std::vector<uint8_t> GDecompress(uint8_t* compressedBytes, ULONG len) {
	std::vector<uint8_t> uncompressedBytes = std::vector<uint8_t>();
	if (len == 0)return uncompressedBytes;
	unsigned full_length = len;
	unsigned half_length = len / 2;

	unsigned uncompLength = full_length;
	char* uncomp = (char*)calloc(sizeof(char), uncompLength);

	z_stream strm;
	strm.next_in = compressedBytes;
	strm.avail_in = len;
	strm.total_out = 0;
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;

	bool done = false;

	if (inflateInit2(&strm, (16 + MAX_WBITS)) != Z_OK) {
		free(uncomp);
		return uncompressedBytes;
	}

	while (!done) {
		if (strm.total_out >= uncompLength) {
			char* uncomp2 = (char*)calloc(sizeof(char), uncompLength + half_length);
			memcpy(uncomp2, uncomp, uncompLength);
			uncompLength += half_length;
			free(uncomp);
			uncomp = uncomp2;
		}

		strm.next_out = (Bytef*)(uncomp + strm.total_out);
		strm.avail_out = uncompLength - strm.total_out;
		int err = inflate(&strm, Z_SYNC_FLUSH);
		if (err == Z_STREAM_END) done = true;
		else if (err != Z_OK) {
			break;
		}
	}

	if (inflateEnd(&strm) != Z_OK) {
		free(uncomp);
		return uncompressedBytes;
	}

	for (size_t i = 0; i < strm.total_out; ++i) {
		uncompressedBytes.push_back(uncomp[i]);
	}
	free(uncomp);
	return uncompressedBytes;
}
std::vector<uint8_t> GCompress(uint8_t* input, ULONG len) {
	std::vector<uint8_t> res = std::vector<uint8_t>();
	res.resize(len);
	z_stream zs;
	zs.zalloc = Z_NULL;
	zs.zfree = Z_NULL;
	zs.opaque = Z_NULL;
	zs.avail_in = (uInt)len;
	zs.next_in = (Bytef*)input;
	zs.avail_out = (uInt)len;
	zs.next_out = (Bytef*)res.data();
	deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 | 16, 8, Z_DEFAULT_STRATEGY);
	deflate(&zs, Z_FINISH);
	deflateEnd(&zs);
	res.resize(zs.total_out);
	return res;
}
std::vector<uint8_t> GCompress(std::initializer_list<uint8_t> input) {
	return GCompress((uint8_t*)input.begin(), input.size());
}
std::vector<uint8_t> GCompress(std::initializer_list<uint8_t>* input) {
	return GCompress((uint8_t*)input->begin(), input->size());
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
#endif