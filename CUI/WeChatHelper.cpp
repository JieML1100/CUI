#include "WeChatHelper.h"

WeChatHelper::WeChatHelper()
{
	op = NULL;
	auto pros = Process::GetProcessesByName("WeChat.exe");
	if (pros.size())
	{
		op = new ProcessOperator(pros[0].Id);
		op->Open();
		ULONG64 ptr = op->FindPattern("WeChatWin.dll", "E8 ? ? ? ? ? 89 5C 24 38 89 5C 24 30");
		if (ptr)
		{
			ptr = op->FindPattern(ptr, "E8 ? ? ? ? 48 8D", 64, 7);
			if (ptr)
			{
				SendTextMsg = (decltype(SendTextMsg))op->calcRVA(ptr, 1);
				if (op->Read<BYTE>(ptr + 7) == 0x8D) {
					FreeChatMsg = (decltype(FreeChatMsg))op->calcRVA(ptr, 13);
				}
				else {
					FreeChatMsg = (decltype(FreeChatMsg))op->calcRVA(ptr, 10);
				}
			}
			ptr = op->FindPattern("WeChatWin.dll", "E8 ? ? ? ? 0F B6 88 F8 07 00 00");
			if (ptr) {
				GetAccountService = (decltype(GetAccountService))op->calcRVA(ptr, 1);
				std::printf("GetAccountService = %p\n", GetAccountService);
			}
			else {
				std::printf("GetAccountService Init Error\n");
			}
			ptr = op->FindPattern("WeChatWin.dll", "48 89 01 48 83 C1 08 E8 ? ? ? ? 90 48 8D 8B ? ? 00 00");
			if (ptr) {
				ChatMsgInstanceCounter = (decltype(ChatMsgInstanceCounter))op->calcRVA(ptr, 8);
				std::printf("ChatMsgInstanceCounter = %p\n", ChatMsgInstanceCounter);
			}
			else {
				std::printf("ChatMsgInstanceCounter Init Error\n");
			}
			ptr = op->FindPattern("WeChatWin.dll", "48 8B DA E8 ? ? ? ? 45 33 C0 48 8B D3 E8");
			if (ptr) {
				GetContactMgr = (decltype(GetContactMgr))op->calcRVA(ptr, 4);
				std::printf("GetContactMgr = %p\n", GetContactMgr);
			}
			else {
				std::printf("GetContactMgr Init Error\n");
			}
			ptr = op->FindPattern("WeChatWin.dll", "FF 10 48 81 C3 ? 06 00 00 48 3B DF 75");
			if (ptr) {
				ContactSize = op->Read<UINT32>(ptr + 5);
				std::printf("ContactSize = %d\n", ContactSize);
			}
			else {
				std::printf("ContactSize Init Error\n");
			}
			ptr = op->FindPattern("WeChatWin.dll", "4C 8D 45 A0 48 8D 97 60 01 00 00 E8 ? ? ? ?");
			if (ptr) {
				GetMemberFromChatRoom = (decltype(GetMemberFromChatRoom))op->calcRVA(ptr, 12);
				std::printf("GetMemberFromChatRoom = %p\n", GetMemberFromChatRoom);
			}
			else {
				std::printf("GetMemberFromChatRoom Init Error\n");
			}
			ptr = op->FindPattern("WeChatWin.dll", "E8 ? ? ? ? 90 E8 ? ? ? ? 48 8B D8 48 8D 95 ? ? ? ? 48 8D 4C 24");
			if (ptr) {
				NewChatRoom = (decltype(NewChatRoom))op->calcRVA(ptr, 1);
				std::printf("NewChatRoom = %p\n", NewChatRoom);
			}
			else {
				std::printf("NewChatRoom Init Error\n");
			}
			ptr = op->FindPattern("WeChatWin.dll", "0F B6 9C 24  ? ? 00 00 48 8D 8C 24 ? ? 00 00 E8 ? ? ? ? 0F B6 C3");
			if (ptr) {
				FreeChatRoom = (decltype(FreeChatRoom))op->calcRVA(ptr, 17);
				std::printf("FreeChatRoom = %p\n", FreeChatRoom);
			}
			else {
				std::printf("FreeChatRoom Init Error\n");
			}
		}
	}
}
void WeChatHelper::SendTextMessage(std::wstring wxIdstr, std::wstring msgstr)
{
	ULONG64 ProcessHeap = op->CallRemote((ULONG64)GetProcessHeap, 0);
	
	ULONG64 wxid = op->CallRemote((ULONG64)HeapAlloc, ProcessHeap, 0, wxIdstr.size() * 2);
	op->Write(wxid, (void*)wxIdstr.data(), wxIdstr.size() * 2);
	ULONG64 msg = op->CallRemote((ULONG64)HeapAlloc, ProcessHeap, 0, msgstr.size() * 2);
	op->Write(msg, (void*)msgstr.data(), msgstr.size() * 2);

	ULONG64 wxid_struct = op->CallRemote((ULONG64)HeapAlloc, ProcessHeap, 0, 0x20);
	ULONG64 msg_struct = op->CallRemote((ULONG64)HeapAlloc, ProcessHeap, 0, 0x20);

	op->Write(wxid_struct, wxid);
	op->Write(wxid_struct + 8 + 0, wxIdstr.size());
	op->Write(wxid_struct + 8 + 4, wxIdstr.size());

	op->Write(msg_struct, msg);
	op->Write(msg_struct + 8 + 0, msgstr.size());
	op->Write(msg_struct + 8 + 4, msgstr.size());
	
	ULONG64 chat_msg = op->AllocateMemory(0x1000);
	
	op->CallRemote(SendTextMsg, chat_msg, wxid_struct, msg_struct, 0, 1, 1, 0, 0);
	op->CallRemote(FreeChatMsg, chat_msg);
	
	op->FreeMemory(chat_msg);
	op->CallRemote((ULONG64)HeapFree, ProcessHeap, 0, wxid_struct);
	op->CallRemote((ULONG64)HeapFree, ProcessHeap, 0, msg_struct);
	op->CallRemote((ULONG64)HeapFree, ProcessHeap, 0, wxid);
	op->CallRemote((ULONG64)HeapFree, ProcessHeap, 0, msg);
}