#include "Clipboard.h"
#include <ShlObj.h>
#include <shellapi.h>
#include <oleidl.h>
#include <comdef.h>
std::string Clipboard::GetText() {
	if (!OpenClipboard(NULL))
		return {};
	HANDLE hData = GetClipboardData(CF_TEXT);
	if (hData == NULL)
		return {};
	char* pszText = static_cast<char*>(GlobalLock(hData));
	if (pszText == NULL)
		return {};
	std::string text(pszText);
	GlobalUnlock(hData);
	CloseClipboard();
	return text;
}
void Clipboard::SetText(std::string str) {
	if (!OpenClipboard(NULL))
		return;

	EmptyClipboard();

	HGLOBAL hClipboardData = GlobalAlloc(GMEM_MOVEABLE, str.size() + 1);
	if (!hClipboardData) {
		CloseClipboard();
		return;
	}

	char* pchData = static_cast<char*>(GlobalLock(hClipboardData));
	if (!pchData) {
		GlobalFree(hClipboardData);
		CloseClipboard();
		return;
	}

	strcpy_s(pchData, str.size() + 1, str.c_str());
	GlobalUnlock(hClipboardData);

	SetClipboardData(CF_TEXT, hClipboardData);

	CloseClipboard();
}
void Clipboard::Clear() {
	if (!OpenClipboard(NULL))
		return;
	EmptyClipboard();
	CloseClipboard();
}
void Clipboard::SetFile(std::string files) {
	UINT uDropEffect = RegisterClipboardFormat(TEXT("Preferred DropEffect"));

	HGLOBAL hGblEffect = GlobalAlloc(GMEM_MOVEABLE, sizeof(DWORD));
	if (!hGblEffect) return;

	LPDWORD lpdDropEffect = (LPDWORD)GlobalLock(hGblEffect);
	if (lpdDropEffect) {
		*lpdDropEffect = DROPEFFECT_COPY;
		GlobalUnlock(hGblEffect);
	}
	else {
		GlobalFree(hGblEffect);
		return;
	}

	DROPFILES stDrop = { 0 };
	stDrop.pFiles = sizeof(DROPFILES);
	stDrop.fWide = FALSE;

	size_t totalSize = sizeof(DROPFILES) + files.size() + 2;
	HGLOBAL hGblFiles = GlobalAlloc(GMEM_MOVEABLE, totalSize);
	if (!hGblFiles) {
		GlobalFree(hGblEffect);
		return;
	}

	LPSTR lpData = (LPSTR)GlobalLock(hGblFiles);
	if (lpData) {
		memcpy(lpData, &stDrop, sizeof(DROPFILES));
		memcpy(lpData + sizeof(DROPFILES), files.c_str(), files.size() + 1);
		*(lpData + sizeof(DROPFILES) + files.size() + 1) = '\0';
		GlobalUnlock(hGblFiles);
	}
	else {
		GlobalFree(hGblFiles);
		GlobalFree(hGblEffect);
		return;
	}

	if (!OpenClipboard(NULL)) {
		GlobalFree(hGblFiles);
		GlobalFree(hGblEffect);
		return;
	}

	EmptyClipboard();
	BOOL success1 = SetClipboardData(CF_HDROP, hGblFiles) != NULL;
	BOOL success2 = SetClipboardData(uDropEffect, hGblEffect) != NULL;

	CloseClipboard();

	if (!success1) GlobalFree(hGblFiles);
	if (!success2) GlobalFree(hGblEffect);
}
void Clipboard::SetFiles(std::vector<std::string> files) {
	if (files.empty()) return;

	std::string fileList;
	for (const auto& file : files) {
		fileList += file + '\0';
	}
	fileList += '\0';

	DROPFILES df = {};
	df.pFiles = sizeof(DROPFILES);
	df.fWide = FALSE;

	HGLOBAL hGbl = GlobalAlloc(GMEM_MOVEABLE, sizeof(DROPFILES) + fileList.size());
	if (!hGbl) return;

	LPSTR data = static_cast<LPSTR>(GlobalLock(hGbl));
	if (!data) {
		GlobalFree(hGbl);
		return;
	}

	memcpy(data, &df, sizeof(df));
	memcpy(data + sizeof(df), fileList.c_str(), fileList.size());
	GlobalUnlock(hGbl);

	UINT cfEffect = RegisterClipboardFormat(TEXT("Preferred DropEffect"));
	HGLOBAL hEffect = GlobalAlloc(GMEM_MOVEABLE, sizeof(DWORD));
	if (hEffect) {
		DWORD* effect = static_cast<DWORD*>(GlobalLock(hEffect));
		if (effect) {
			*effect = DROPEFFECT_COPY;
			GlobalUnlock(hEffect);
		}
		else {
			GlobalFree(hEffect);
			hEffect = nullptr;
		}
	}

	if (!OpenClipboard(nullptr)) {
		GlobalFree(hGbl);
		if (hEffect) GlobalFree(hEffect);
		return;
	}

	EmptyClipboard();
	SetClipboardData(CF_HDROP, hGbl);
	if (hEffect) SetClipboardData(cfEffect, hEffect);
	CloseClipboard();
}
std::string Clipboard::GetFile() {
	std::string path;
	if (::OpenClipboard(NULL)) {
		HDROP hDrop = HDROP(::GetClipboardData(CF_HDROP));
		if (hDrop != NULL) {
			WCHAR szFilePathName[MAX_PATH + 1] = { 0 };
			UINT nNumOfFiles = DragQueryFileW(hDrop, 0xFFFFFFFF, NULL, 0);
			if (nNumOfFiles == 1) {
				memset(szFilePathName, 0, MAX_PATH + 1);
				DragQueryFileW(hDrop, 0, szFilePathName, MAX_PATH);
				_bstr_t path(szFilePathName);
				path = (LPCSTR)path;
			}
		}
		CloseClipboard();
	}

	return path;
}
std::vector<std::string> Clipboard::GetFiles() {
	std::vector<std::string> path_list;
	if (::OpenClipboard(NULL)) {
		HDROP hDrop = HDROP(::GetClipboardData(CF_HDROP));
		if (hDrop != NULL) {
			WCHAR szFilePathName[MAX_PATH + 1] = { 0 };
			UINT nNumOfFiles = DragQueryFileW(hDrop, 0xFFFFFFFF, NULL, 0);
			for (UINT nIndex = 0; nIndex < nNumOfFiles; ++nIndex) {
				memset(szFilePathName, 0, MAX_PATH + 1);
				DragQueryFileW(hDrop, nIndex, szFilePathName, MAX_PATH);
				_bstr_t path(szFilePathName);
				std::string ss = (LPCSTR)path;
				path_list.push_back(ss);
			}
		}
		CloseClipboard();
	}

	return path_list;
}
void Clipboard::SetImage(HBITMAP bmp) {
	if (!OpenClipboard(nullptr))
		return;
	EmptyClipboard();
	SetClipboardData(CF_BITMAP, bmp);
	CloseClipboard();
}
HBITMAP Clipboard::GetImage() {
	if (!OpenClipboard(nullptr))
		return nullptr;
	HANDLE hData = GetClipboardData(CF_BITMAP);
	if (hData == nullptr)
		return nullptr;
	CloseClipboard();
	return (HBITMAP)hData;
}
UINT Clipboard::GetFormat() {
	UINT formatList[] = { 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,0x0080,0x0081,0x0082,0x0083,0x008E,0x0200,0x02FF,0x0300,0x03FF };
	return GetPriorityClipboardFormat(formatList, sizeof(formatList) / sizeof(formatList[0]));
}
