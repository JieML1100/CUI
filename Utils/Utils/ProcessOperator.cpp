#include "ProcessOperator.h"
#include "MemLoadLibrary2.h"
#include <Psapi.h>
#include "Utils.h"

typedef struct _PARAMX {
    PVOID lpFileData;
    DWORD DataLength;
    PVOID LdrGetProcedureAddress;
    PVOID pLdrLoadDll;
    PVOID RtlInitAnsiString;
    PVOID RtlAnsiStringToUnicodeString;
    PVOID RtlFreeUnicodeString;
    PVOID pMemoryAddress;
}PARAMX, * PPARAMX;
ProcessOperator::ProcessOperator(DWORD pid) :PID(pid) {
}
BOOL ProcessOperator::Open() {
    Handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, this->PID);
    return Handle != NULL;
}
BOOL ProcessOperator::Close() {
	return CloseHandle(this->Handle);
}
BOOL ProcessOperator::Read(ULONG64 addr, const PVOID buffer, SIZE_T size) {
    return ReadProcessMemory(this->Handle, PVOID(addr), buffer, size, NULL);
}
BOOL ProcessOperator::Write(ULONG64 addr, const PVOID buffer, SIZE_T size) {
    return WriteProcessMemory(this->Handle, PVOID(addr), buffer, size, NULL);
}

long long ProcessOperator::VirtualMemorySize() {
	PROCESS_MEMORY_COUNTERS pmc;
    K32GetProcessMemoryInfo(this->Handle, &pmc, sizeof(pmc));
	return pmc.WorkingSetSize;
}
ULONG64 ProcessOperator::AllocateMemory(SIZE_T size, DWORD protect, DWORD type,ULONG64 baseAddr) {
    return (ULONG64)VirtualAllocEx(this->Handle, (PVOID*)baseAddr, size, type, protect);
}

ULONG64 ProcessOperator::AllocateString(std::string s) {
    ULONG64 ptr = AllocateMemory(s.size() + 1, PAGE_READWRITE, MEM_COMMIT, NULL);
    if (ptr) {
        this->Write(ptr, (void*)s.c_str(), s.size() + 1);
        return ptr;
    }
    return NULL;
}
ULONG64 ProcessOperator::AllocateWString(std::wstring s) {
    ULONG64 ptr = AllocateMemory((s.size() + 1) * sizeof(wchar_t), PAGE_READWRITE, MEM_COMMIT, NULL);
    if (ptr) {
        this->Write(ptr, (void*)s.c_str(), (s.size() + 1) * sizeof(wchar_t));
        return ptr;
    }
    return NULL;
}
BOOL ProcessOperator::FreeMemory(ULONG64 addr, SIZE_T size, DWORD freeType) {
    return VirtualFreeEx(this->Handle, (PVOID*)&addr, size, freeType);
}
ULONG64 ProcessOperator::CallRemote(ULONG64 func, std::vector<ULONG64> args) {
    std::vector<uint8_t> code = std::vector<uint8_t>();
    auto pushcode = [](std::vector<uint8_t>&codeList, const void* code, int size){ codeList.insert(codeList.end(),(uint8_t*)code, (uint8_t*)code+size); };
    pushcode(code, "\x55", 1);
    pushcode(code, "\x48\x8B\xEC", 3);
    if (args.size() < 16)
        pushcode(code, "\x48\x83\xEC", 3);
    else
        pushcode(code, "\x48\x81\xEC", 3);
    if (args.size() > 4){
        if (args.size() < 16)
            code.push_back((args.size() + 1) * 8);
        else{
            int val = (args.size() + 1) * 8;
            pushcode(code, &val, 4);
        }
    }
    else{
        code.push_back(0x20);
    }
    for (int i = 0; i < args.size(); i++){
        if (i == 0)
            pushcode(code, "\x48\xB9", 2);
        else if (i == 1)
            pushcode(code, "\x48\xBA", 2);
        else if (i == 2)
            pushcode(code, "\x49\xB8", 2);
        else if (i == 3)
            pushcode(code, "\x49\xB9", 2);
        else
            pushcode(code, "\x48\xB8", 2);
        pushcode(code, &args[i], 8);
        if (i > 3){
            if (i < 16){
                pushcode(code, "\x48\x89\x44\x24", 4);
                code.push_back(i * 8);
            }
            else {
                pushcode(code, "\x48\x89\x84\x24", 4);
                int tmpVal = i * 8;
                pushcode(code, &tmpVal, 4);

            }
        }
    }
    pushcode(code, "\x48\xB8", 2);
    pushcode(code, &func, 8);
    pushcode(code, "\xFF\xD0", 2);
    pushcode(code, "\x48\xA3", 2);
    pushcode(code, "00000000", 8);
    if (args.size() < 16)
        pushcode(code, "\x48\x83\xC4", 3);
    else
        pushcode(code, "\x48\x81\xC4", 3);
    if (args.size() > 4) {
        if (args.size() < 16)
            code.push_back((args.size() + 1) * 8);
        else{
            int val = (args.size() + 1) * 8;
            pushcode(code, &val, 4);
        }
    }
    else {
        code.push_back(0x20);
    }
    pushcode(code, "\x48\x8B\xE5", 3);
    pushcode(code, "\x5D", 1);
    pushcode(code, "\xC3", 1);
    pushcode(code, "\x00\x00\x00\x00\x00\x00\x00\x00", 8);
    ULONG64 membase = this->AllocateMemory(code.size(), PAGE_EXECUTE_READWRITE, MEM_COMMIT, NULL);
    ULONG64* p_rax = (ULONG64*)(code.data() + (code.size() - ((args.size() < 16) ? 25 : 28)));
    *p_rax = (membase + (code.size() - 8));
    if (membase){
        if (this->Write(membase, code.data(), code.size())){
            ULONG64 result = NULL;
            HANDLE tHadnle = CreateRemoteThread(this->Handle, NULL, 0x400, (LPTHREAD_START_ROUTINE)membase, 0, 0, 0);
            if (tHadnle){
                if (WaitForSingleObject(tHadnle, INFINITE) != WAIT_FAILED)
                    result = this->Read<ULONG64>(membase + (code.size() - 8));
            }
            this->FreeMemory(membase, code.size());
            return result;
        }
        this->FreeMemory(membase, code.size());
    }
    return NULL;

    return NULL;
}
ULONG64 ProcessOperator::CallRemote(ULONG64 func,ULONG64 rcx,ULONG64 rdx,ULONG64 r8,ULONG64 r9, ULONG64 rsp20, ULONG64 rsp28, ULONG64 rsp30, ULONG64 rsp38) {
    uint8_t code[] = {
        0x55,                                           
        0x48,0x8B,0xEC,                                 
        0x48,0x83,0xEC,0x50,                            
        0x48,0xB9,0,0,0,0,0,0,0,0,                      
        0x48,0xBA,0,0,0,0,0,0,0,0,                      
        0x49,0xB8,0,0,0,0,0,0,0,0,                      
        0x49,0xB9,0,0,0,0,0,0,0,0,                      
        0x48,0xB8,0,0,0,0,0,0,0,0,                      
        0x48,0x89,0x44,0x24,0x20,                       
        0x48,0xB8,0,0,0,0,0,0,0,0,                      
        0x48,0x89,0x44,0x24,0x28,                       
        0x48,0xB8,0,0,0,0,0,0,0,0,                      
        0x48,0x89,0x44,0x24,0x30,                       
        0x48,0xB8,0,0,0,0,0,0,0,0,                      
        0x48,0x89,0x44,0x24,0x38,                       
        0x48,0xB8,0,0,0,0,0,0,0,0,                      
        0xFF,0xD0,									    
        0x48,0xA3,0,0,0,0,0,0,0,0,                      
        0x48,0x83,0xC4,0x50,                            
        0x48,0x8B,0xE5,                                 
        0x5D,                                           
        0xC3,                                           
        0,0,0,0,0,0,0,0                                 
    };
    ULONG64* p_rcx = (ULONG64*)(code + 10);
    ULONG64* p_rdx = (ULONG64*)(code + 20);
    ULONG64* p_r8 = (ULONG64*)(code + 30);
    ULONG64* p_r9 = (ULONG64*)(code + 40);
    ULONG64* p_rsp20 = (ULONG64*)(code + 50);
    ULONG64* p_rsp28 = (ULONG64*)(code + 65);
    ULONG64* p_rsp30 = (ULONG64*)(code + 80);
    ULONG64* p_rsp38 = (ULONG64*)(code + 95);

    ULONG64* p_call = (ULONG64*)(code + 110);
    ULONG64* p_rax = (ULONG64*)(code + 122);

    *p_rcx = rcx;
    *p_rdx = rdx;
    *p_r8 = r8;
    *p_r9 = r9;
    *p_call = func;
    *p_rsp20 = rsp20;
    *p_rsp28 = rsp28;
    *p_rsp30 = rsp30;
    *p_rsp38 = rsp38;
    ULONG64 membase = this->AllocateMemory(sizeof(code), PAGE_EXECUTE_READWRITE, MEM_COMMIT, NULL);
    *p_rax = ((ULONG64)membase + (sizeof(code) - 8));
    if (membase) {
        if (this->Write(membase, code, sizeof(code))) {
            ULONG64 result = NULL;
            HANDLE tHadnle = CreateRemoteThread(this->Handle, NULL, 0x400, (LPTHREAD_START_ROUTINE)membase, 0, 0, 0);
            if (tHadnle) {
                if(WaitForSingleObject(tHadnle, INFINITE)!= WAIT_FAILED)
                    result = this->Read<ULONG64>(((ULONG64)membase + (sizeof(code) - 8)));
            }
            this->FreeMemory(membase, sizeof(code));
            return result;
        }
        this->FreeMemory(membase, sizeof(code));
    }
    return NULL;
}
ULONG64 ProcessOperator::InjectDll(uint8_t* buffer,SIZE_T size) {
    HMODULE NtBase = GetModuleHandleA("ntdll.dll");
    LPVOID dllbase = VirtualAllocEx(this->Handle, NULL, size, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (dllbase) {
        SIZE_T tmpSize = 0;
        WriteProcessMemory(this->Handle, dllbase, buffer, size, &tmpSize);
        PARAMX MemLoadLibrary2Param = {};
        MemLoadLibrary2Param.pLdrLoadDll = GetProcAddress(NtBase, "LdrLoadDll");
        MemLoadLibrary2Param.RtlInitAnsiString = GetProcAddress(NtBase, "RtlInitAnsiString");
        MemLoadLibrary2Param.RtlAnsiStringToUnicodeString = GetProcAddress(NtBase, "RtlAnsiStringToUnicodeString");
        MemLoadLibrary2Param.RtlFreeUnicodeString = GetProcAddress(NtBase, "RtlFreeUnicodeString");
        MemLoadLibrary2Param.LdrGetProcedureAddress = GetProcAddress(NtBase, "LdrGetProcedureAddress");
        MemLoadLibrary2Param.lpFileData = dllbase;
        MemLoadLibrary2Param.DataLength = size;
        MemLoadLibrary2Param.pMemoryAddress = VirtualAllocEx(this->Handle, NULL, GetSectionSize((PVOID)buffer), MEM_COMMIT, PAGE_EXECUTE_READWRITE);
        auto RemoteMemLoadLibrary2Param = VirtualAllocEx(this->Handle, NULL, sizeof(PARAMX), MEM_COMMIT, PAGE_EXECUTE_READWRITE);
        if (RemoteMemLoadLibrary2Param) {
            WriteProcessMemory(this->Handle, RemoteMemLoadLibrary2Param, &MemLoadLibrary2Param, sizeof(PARAMX), &tmpSize);
            PVOID MemLoadLibrary2Address = VirtualAllocEx(this->Handle, NULL, sizeof(MemLoadLibrary2), MEM_COMMIT, PAGE_EXECUTE_READWRITE);
            if (MemLoadLibrary2Address) {
                WriteProcessMemory(this->Handle, MemLoadLibrary2Address, &MemLoadLibrary2, sizeof(MemLoadLibrary2), &tmpSize);
                uint8_t shellcode[] = {
                    0x90,
                    0xC6,0x05,0xF8,0xFF,0xFF,0xFF,0xC3, 
                    0x48,0xB9,0,0,0,0,0,0,0,0,			
                    0xFF,0x25,0,0,0,0,0,0,0,0,0,0,0,0,	
                    0xC3								
                };
                *(ULONG64*)(shellcode + 10) = (ULONG64)RemoteMemLoadLibrary2Param;
                *(ULONG64*)(shellcode + 24) = (ULONG64)MemLoadLibrary2Address;
                ULONG64 jmpmem = (ULONG64)VirtualAllocEx(this->Handle, NULL, sizeof(shellcode), MEM_COMMIT, PAGE_EXECUTE_READWRITE);
                if (!jmpmem)
                    return NULL;
                WriteProcessMemory(this->Handle, (PVOID)jmpmem, (PVOID)shellcode, sizeof(shellcode), &tmpSize);
                HANDLE tHadnle = CreateRemoteThread(this->Handle, NULL, 0, (LPTHREAD_START_ROUTINE)jmpmem, 0, 0, 0);
                WaitForSingleObject(tHadnle, INFINITE);
                this->FreeMemory(jmpmem);
                this->FreeMemory((ULONG64)dllbase);
                this->FreeMemory((ULONG64)RemoteMemLoadLibrary2Param);
                this->FreeMemory((ULONG64)MemLoadLibrary2Address);

            }
        }
        return (ULONG64)MemLoadLibrary2Param.pMemoryAddress;
    }
    return NULL;
}
ULONG64 ProcessOperator::FindPattern(const char* szModule, const char* sPattern, int offset) {
    ULONG64 offset_ = 0;
    std::vector<PATTERNVALUE> pattern = ParserPattern(sPattern);
    if (pattern.size() == 0) return NULL;
    MODULEINFO mi{ };
    ULONG64 md_str = this->AllocateString(szModule);
    HMODULE remote_hModule = (HMODULE)this->CallRemote((ULONG64)GetModuleHandleA, md_str);
    this->FreeMemory(md_str);
    if (K32GetModuleInformation(this->Handle, remote_hModule, &mi, sizeof(mi))) {
        BYTE* begin = new BYTE[mi.SizeOfImage];
        this->Read((ULONG64)mi.lpBaseOfDll, begin, mi.SizeOfImage);
        DWORD size = mi.SizeOfImage;
        for (unsigned char* curr = begin + offset; curr <= (begin + size) - pattern.size(); curr++) {
            for (int i = 0; i < pattern.size(); i++) {
                if (pattern[i].ignore == 0x11) continue;
                if (!pattern[i].ignore_left && ((curr[i] & 0xF0) >> 4) != pattern[i].left)goto nxt;
                if (!pattern[i].ignore_right && (curr[i] & 0x0F) != pattern[i].right)goto nxt;
            }
            offset_ = (curr - begin);
            delete[] begin;
            return offset_ + (ULONG64)mi.lpBaseOfDll;
        nxt:;
        }
        delete[] begin;
    }
    return NULL;

}
ULONG64 ProcessOperator::FindPattern(ULONG64 _begin, const char* sPattern, int search_size, int offset) {
    ULONG64 offset_ = 0;
    std::vector<PATTERNVALUE> pattern = ParserPattern(sPattern);
    if (pattern.size() == 0) return NULL;
    BYTE* begin = new BYTE[search_size];
    this->Read((ULONG64)_begin, begin, search_size);
    for (unsigned char* curr = begin + offset; curr <= (begin + search_size) - pattern.size(); curr++) {
        for (int i = 0; i < pattern.size(); i++) {
            if (pattern[i].ignore == 0x11) continue;
            if (!pattern[i].ignore_left && ((curr[i] & 0xF0) >> 4) != pattern[i].left)goto nxt;
            if (!pattern[i].ignore_right && (curr[i] & 0x0F) != pattern[i].right)goto nxt;
        }
        offset_ = (curr - begin);
        delete[] begin;
        return offset_ + (ULONG64)_begin;
    nxt:;
    }
    delete[] begin;
    return NULL;
}
ULONG64 ProcessOperator::calcRVA(ULONG64 ptr, int offset) {
    uint8_t* buffer = new uint8_t[offset + 0x10];
    this->Read(ptr, buffer, offset + 0x10);
    int offset_ = *(int*)(buffer + offset);
    delete[] buffer;
    return ptr + offset_ + offset + 4;
}