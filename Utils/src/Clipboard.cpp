#include "Clipboard.h"
#include <ShlObj.h>
#include <shellapi.h>
#include <cstring>

namespace {
const wchar_t* ClipboardOwnerClassName() {
	return L"CppUtilsClipboardOwnerWindow";
}

LRESULT CALLBACK ClipboardOwnerWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
	return DefWindowProcW(hwnd, message, wParam, lParam);
}

bool EnsureClipboardOwnerClass() {
	static bool registered = []() {
		WNDCLASSW wc = {};
		wc.lpfnWndProc = ClipboardOwnerWndProc;
		wc.hInstance = GetModuleHandleW(nullptr);
		wc.lpszClassName = ClipboardOwnerClassName();
		return RegisterClassW(&wc) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
	}();
	return registered;
}

HWND CreateClipboardOwnerWindow() {
	if (!EnsureClipboardOwnerClass()) return nullptr;
	return CreateWindowExW(0, ClipboardOwnerClassName(), L"", 0, 0, 0, 0, 0,
		HWND_MESSAGE, nullptr, GetModuleHandleW(nullptr), nullptr);
}

class ClipboardOwnerWindow {
public:
	ClipboardOwnerWindow() : _hwnd(CreateClipboardOwnerWindow()) {}
	~ClipboardOwnerWindow() {
		if (_hwnd) DestroyWindow(_hwnd);
	}

	ClipboardOwnerWindow(const ClipboardOwnerWindow&) = delete;
	ClipboardOwnerWindow& operator=(const ClipboardOwnerWindow&) = delete;

	HWND Handle() const { return _hwnd; }
	bool Valid() const { return _hwnd != nullptr; }

private:
	HWND _hwnd;
};

class ClipboardGuard {
public:
	explicit ClipboardGuard(HWND owner = nullptr) : _opened(OpenClipboard(owner) != FALSE) {}
	~ClipboardGuard() {
		if (_opened) CloseClipboard();
	}

	ClipboardGuard(const ClipboardGuard&) = delete;
	ClipboardGuard& operator=(const ClipboardGuard&) = delete;

	bool Opened() const { return _opened; }

private:
	bool _opened;
};

std::wstring MultiByteToWide(const std::string& str, UINT codePage, DWORD flags = 0) {
	if (str.empty()) return L"";
	int len = MultiByteToWideChar(codePage, flags, str.data(), static_cast<int>(str.size()), nullptr, 0);
	if (len <= 0) return L"";
	std::wstring result(static_cast<size_t>(len), L'\0');
	MultiByteToWideChar(codePage, flags, str.data(), static_cast<int>(str.size()), result.data(), len);
	return result;
}

std::wstring Utf8ToWide(const std::string& str) {
	std::wstring result = MultiByteToWide(str, CP_UTF8, MB_ERR_INVALID_CHARS);
	if (!result.empty() || str.empty()) return result;
	return MultiByteToWide(str, CP_ACP);
}

std::string WideToMultiByte(const std::wstring& str, UINT codePage) {
	if (str.empty()) return "";
	int len = WideCharToMultiByte(codePage, 0, str.data(), static_cast<int>(str.size()),
		nullptr, 0, nullptr, nullptr);
	if (len <= 0) return "";
	std::string result(static_cast<size_t>(len), '\0');
	WideCharToMultiByte(codePage, 0, str.data(), static_cast<int>(str.size()),
		result.data(), len, nullptr, nullptr);
	return result;
}

std::string WideToUtf8(const std::wstring& str) {
	return WideToMultiByte(str, CP_UTF8);
}

std::vector<std::wstring> Utf8ToWide(const std::vector<std::string>& values) {
	std::vector<std::wstring> result;
	result.reserve(values.size());
	for (const auto& value : values) {
		result.push_back(Utf8ToWide(value));
	}
	return result;
}

UINT ClipboardTextCodePage() {
	HGLOBAL hLocale = static_cast<HGLOBAL>(GetClipboardData(CF_LOCALE));
	if (!hLocale) return CP_ACP;

	LCID* locale = static_cast<LCID*>(GlobalLock(hLocale));
	if (!locale) return CP_ACP;
	LCID lcid = *locale;
	GlobalUnlock(hLocale);

	DWORD codePage = CP_ACP;
	if (GetLocaleInfoW(lcid, LOCALE_IDEFAULTANSICODEPAGE | LOCALE_RETURN_NUMBER,
		reinterpret_cast<LPWSTR>(&codePage), sizeof(codePage) / sizeof(wchar_t)) <= 0) {
		return CP_ACP;
	}
	return codePage == 0 ? CP_ACP : static_cast<UINT>(codePage);
}

std::wstring ReadWideText(HGLOBAL hData) {
	SIZE_T byteSize = GlobalSize(hData);
	const wchar_t* data = static_cast<const wchar_t*>(GlobalLock(hData));
	if (!data) return L"";

	size_t maxLength = byteSize / sizeof(wchar_t);
	size_t length = 0;
	while ((maxLength == 0 || length < maxLength) && data[length] != L'\0') {
		++length;
	}

	std::wstring result(data, length);
	GlobalUnlock(hData);
	return result;
}

std::string ReadNarrowText(HGLOBAL hData) {
	SIZE_T byteSize = GlobalSize(hData);
	const char* data = static_cast<const char*>(GlobalLock(hData));
	if (!data) return "";

	size_t maxLength = byteSize;
	size_t length = 0;
	while ((maxLength == 0 || length < maxLength) && data[length] != '\0') {
		++length;
	}

	std::string result(data, length);
	GlobalUnlock(hData);
	return result;
}

HGLOBAL CreateGlobalMemory(const void* data, size_t size) {
	HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, size == 0 ? 1 : size);
	if (!hMem) return nullptr;

	void* target = GlobalLock(hMem);
	if (!target) {
		GlobalFree(hMem);
		return nullptr;
	}

	if (data && size > 0) {
		std::memcpy(target, data, size);
	}
	GlobalUnlock(hMem);
	return hMem;
}

HGLOBAL CreateWideTextMemory(const std::wstring& text) {
	return CreateGlobalMemory(text.c_str(), (text.size() + 1) * sizeof(wchar_t));
}

HGLOBAL CreateDropEffectMemory() {
	DWORD effect = DROPEFFECT_COPY;
	return CreateGlobalMemory(&effect, sizeof(effect));
}

HGLOBAL CreateDropFilesMemory(const std::vector<std::wstring>& files) {
	std::wstring fileList;
	for (const auto& file : files) {
		if (file.empty()) continue;
		fileList.append(file);
		fileList.push_back(L'\0');
	}
	if (fileList.empty()) return nullptr;
	fileList.push_back(L'\0');

	const size_t fileBytes = fileList.size() * sizeof(wchar_t);
	HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, sizeof(DROPFILES) + fileBytes);
	if (!hMem) return nullptr;

	void* data = GlobalLock(hMem);
	if (!data) {
		GlobalFree(hMem);
		return nullptr;
	}

	DROPFILES drop = {};
	drop.pFiles = sizeof(DROPFILES);
	drop.fWide = TRUE;
	std::memcpy(data, &drop, sizeof(drop));
	std::memcpy(static_cast<BYTE*>(data) + sizeof(drop), fileList.data(), fileBytes);
	GlobalUnlock(hMem);
	return hMem;
}

std::wstring StandardFormatName(UINT format) {
	switch (format) {
	case CF_TEXT:
		return L"CF_TEXT";
	case CF_BITMAP:
		return L"CF_BITMAP";
	case CF_METAFILEPICT:
		return L"CF_METAFILEPICT";
	case CF_SYLK:
		return L"CF_SYLK";
	case CF_DIF:
		return L"CF_DIF";
	case CF_TIFF:
		return L"CF_TIFF";
	case CF_OEMTEXT:
		return L"CF_OEMTEXT";
	case CF_DIB:
		return L"CF_DIB";
	case CF_PALETTE:
		return L"CF_PALETTE";
	case CF_PENDATA:
		return L"CF_PENDATA";
	case CF_RIFF:
		return L"CF_RIFF";
	case CF_WAVE:
		return L"CF_WAVE";
	case CF_UNICODETEXT:
		return L"CF_UNICODETEXT";
	case CF_ENHMETAFILE:
		return L"CF_ENHMETAFILE";
	case CF_HDROP:
		return L"CF_HDROP";
	case CF_LOCALE:
		return L"CF_LOCALE";
	case CF_DIBV5:
		return L"CF_DIBV5";
	case CF_OWNERDISPLAY:
		return L"CF_OWNERDISPLAY";
	case CF_DSPTEXT:
		return L"CF_DSPTEXT";
	case CF_DSPBITMAP:
		return L"CF_DSPBITMAP";
	case CF_DSPMETAFILEPICT:
		return L"CF_DSPMETAFILEPICT";
	case CF_DSPENHMETAFILE:
		return L"CF_DSPENHMETAFILE";
	case CF_PRIVATEFIRST:
		return L"CF_PRIVATEFIRST";
	case CF_PRIVATELAST:
		return L"CF_PRIVATELAST";
	case CF_GDIOBJFIRST:
		return L"CF_GDIOBJFIRST";
	case CF_GDIOBJLAST:
		return L"CF_GDIOBJLAST";
	default:
		return L"";
	}
}
}

std::string Clipboard::GetText() {
	return WideToUtf8(GetTextW());
}

std::wstring Clipboard::GetTextW() {
	ClipboardGuard clipboard;
	if (!clipboard.Opened()) return L"";

	HGLOBAL hData = static_cast<HGLOBAL>(GetClipboardData(CF_UNICODETEXT));
	if (hData) return ReadWideText(hData);

	hData = static_cast<HGLOBAL>(GetClipboardData(CF_TEXT));
	if (hData) return MultiByteToWide(ReadNarrowText(hData), ClipboardTextCodePage());

	hData = static_cast<HGLOBAL>(GetClipboardData(CF_OEMTEXT));
	if (hData) return MultiByteToWide(ReadNarrowText(hData), CP_OEMCP);

	return L"";
}

bool Clipboard::SetText(std::string str) {
	return SetText(Utf8ToWide(str));
}

bool Clipboard::SetText(std::wstring str) {
	HGLOBAL hText = CreateWideTextMemory(str);
	if (!hText) return false;

	ClipboardOwnerWindow owner;
	if (!owner.Valid()) {
		GlobalFree(hText);
		return false;
	}

	ClipboardGuard clipboard(owner.Handle());
	if (!clipboard.Opened() || !EmptyClipboard()) {
		GlobalFree(hText);
		return false;
	}

	if (!SetClipboardData(CF_UNICODETEXT, hText)) {
		GlobalFree(hText);
		return false;
	}

	return true;
}

bool Clipboard::Clear() {
	ClipboardOwnerWindow owner;
	if (!owner.Valid()) return false;

	ClipboardGuard clipboard(owner.Handle());
	return clipboard.Opened() && EmptyClipboard() != FALSE;
}

bool Clipboard::SetFile(std::string file) {
	return SetFiles(std::vector<std::string>{ file });
}

bool Clipboard::SetFile(std::wstring file) {
	return SetFiles(std::vector<std::wstring>{ file });
}

bool Clipboard::SetFiles(std::vector<std::string> files) {
	return SetFiles(Utf8ToWide(files));
}

bool Clipboard::SetFiles(std::vector<std::wstring> files) {
	HGLOBAL hFiles = CreateDropFilesMemory(files);
	if (!hFiles) return false;

	HGLOBAL hEffect = CreateDropEffectMemory();
	ClipboardOwnerWindow owner;
	if (!owner.Valid()) {
		GlobalFree(hFiles);
		if (hEffect) GlobalFree(hEffect);
		return false;
	}

	ClipboardGuard clipboard(owner.Handle());
	if (!clipboard.Opened() || !EmptyClipboard()) {
		GlobalFree(hFiles);
		if (hEffect) GlobalFree(hEffect);
		return false;
	}

	if (!SetClipboardData(CF_HDROP, hFiles)) {
		GlobalFree(hFiles);
		if (hEffect) GlobalFree(hEffect);
		return false;
	}

	UINT cfEffect = RegisterClipboardFormatW(L"Preferred DropEffect");
	if (hEffect) {
		if (cfEffect == 0 || !SetClipboardData(cfEffect, hEffect)) {
			GlobalFree(hEffect);
		}
	}

	return true;
}

std::string Clipboard::GetFile() {
	return WideToUtf8(GetFileW());
}

std::wstring Clipboard::GetFileW() {
	std::vector<std::wstring> files = GetFilesW();
	return files.size() == 1 ? files[0] : L"";
}

std::vector<std::string> Clipboard::GetFiles() {
	std::vector<std::wstring> wideFiles = GetFilesW();
	std::vector<std::string> result;
	result.reserve(wideFiles.size());
	for (const auto& file : wideFiles) {
		result.push_back(WideToUtf8(file));
	}
	return result;
}

std::vector<std::wstring> Clipboard::GetFilesW() {
	std::vector<std::wstring> result;
	ClipboardGuard clipboard;
	if (!clipboard.Opened()) return result;

	HDROP hDrop = static_cast<HDROP>(GetClipboardData(CF_HDROP));
	if (!hDrop) return result;

	UINT count = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
	result.reserve(count);
	for (UINT i = 0; i < count; ++i) {
		UINT length = DragQueryFileW(hDrop, i, nullptr, 0);
		if (length == 0) continue;

		std::wstring path(static_cast<size_t>(length) + 1, L'\0');
		UINT copied = DragQueryFileW(hDrop, i, path.data(), length + 1);
		path.resize(copied);
		result.push_back(path);
	}

	return result;
}

bool Clipboard::SetImage(HBITMAP bmp) {
	if (!bmp) return false;

	HBITMAP copy = static_cast<HBITMAP>(CopyImage(bmp, IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION));
	if (!copy) {
		copy = static_cast<HBITMAP>(CopyImage(bmp, IMAGE_BITMAP, 0, 0, 0));
	}
	if (!copy) return false;

	ClipboardOwnerWindow owner;
	if (!owner.Valid()) {
		DeleteObject(copy);
		return false;
	}

	ClipboardGuard clipboard(owner.Handle());
	if (!clipboard.Opened() || !EmptyClipboard()) {
		DeleteObject(copy);
		return false;
	}

	if (!SetClipboardData(CF_BITMAP, copy)) {
		DeleteObject(copy);
		return false;
	}

	return true;
}

HBITMAP Clipboard::GetImage() {
	ClipboardGuard clipboard;
	if (!clipboard.Opened()) return nullptr;

	HBITMAP bmp = static_cast<HBITMAP>(GetClipboardData(CF_BITMAP));
	if (!bmp) return nullptr;

	HBITMAP copy = static_cast<HBITMAP>(CopyImage(bmp, IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION));
	if (!copy) {
		copy = static_cast<HBITMAP>(CopyImage(bmp, IMAGE_BITMAP, 0, 0, 0));
	}
	return copy;
}

ClipboardFormat Clipboard::GetFormat() {
	return FromWin32Format(GetFormatId());
}

UINT Clipboard::GetFormatId() {
	UINT formatList[] = {
		CF_HDROP,
		CF_UNICODETEXT,
		CF_TEXT,
		CF_OEMTEXT,
		CF_DIBV5,
		CF_DIB,
		CF_BITMAP,
		CF_ENHMETAFILE,
		CF_METAFILEPICT,
		CF_TIFF,
		CF_PALETTE,
		CF_SYLK,
		CF_DIF,
		CF_PENDATA,
		CF_RIFF,
		CF_WAVE,
		CF_LOCALE,
		CF_OWNERDISPLAY,
		CF_DSPTEXT,
		CF_DSPBITMAP,
		CF_DSPMETAFILEPICT,
		CF_DSPENHMETAFILE,
		CF_PRIVATEFIRST,
		CF_PRIVATELAST,
		CF_GDIOBJFIRST,
		CF_GDIOBJLAST
	};

	int format = GetPriorityClipboardFormat(formatList, static_cast<int>(sizeof(formatList) / sizeof(formatList[0])));
	if (format > 0) return static_cast<UINT>(format);

	if (format == -1) {
		std::vector<UINT> formats = GetFormats();
		return formats.empty() ? 0 : formats[0];
	}

	return 0;
}

std::vector<UINT> Clipboard::GetFormats() {
	std::vector<UINT> result;
	ClipboardGuard clipboard;
	if (!clipboard.Opened()) return result;

	UINT format = 0;
	while ((format = EnumClipboardFormats(format)) != 0) {
		result.push_back(format);
	}

	return result;
}

std::vector<uint8_t> Clipboard::GetData(UINT format) {
	std::vector<uint8_t> result;
	if (format == 0) return result;

	ClipboardGuard clipboard;
	if (!clipboard.Opened()) return result;

	HGLOBAL hData = static_cast<HGLOBAL>(GetClipboardData(format));
	if (!hData) return result;

	SIZE_T size = GlobalSize(hData);
	if (size == 0) return result;

	const void* data = GlobalLock(hData);
	if (!data) return result;

	const uint8_t* bytes = static_cast<const uint8_t*>(data);
	result.assign(bytes, bytes + size);
	GlobalUnlock(hData);
	return result;
}

bool Clipboard::SetData(UINT format, const void* data, size_t size) {
	if (format == 0) return false;

	HGLOBAL hData = CreateGlobalMemory(data, size);
	if (!hData) return false;

	ClipboardOwnerWindow owner;
	if (!owner.Valid()) {
		GlobalFree(hData);
		return false;
	}

	ClipboardGuard clipboard(owner.Handle());
	if (!clipboard.Opened() || !EmptyClipboard()) {
		GlobalFree(hData);
		return false;
	}

	if (!SetClipboardData(format, hData)) {
		GlobalFree(hData);
		return false;
	}

	return true;
}

bool Clipboard::SetData(UINT format, std::vector<uint8_t> data) {
	return SetData(format, data.data(), data.size());
}

bool Clipboard::IsFormatAvailable(ClipboardFormat format) {
	UINT win32Format = ToWin32Format(format);
	return win32Format != 0 && IsFormatAvailable(win32Format);
}

bool Clipboard::IsFormatAvailable(UINT format) {
	return format != 0 && IsClipboardFormatAvailable(format) != FALSE;
}

std::string Clipboard::GetFormatName(UINT format) {
	return WideToUtf8(GetFormatNameW(format));
}

std::wstring Clipboard::GetFormatNameW(UINT format) {
	std::wstring standardName = StandardFormatName(format);
	if (!standardName.empty()) return standardName;

	wchar_t name[256] = {};
	int length = GetClipboardFormatNameW(format, name, static_cast<int>(sizeof(name) / sizeof(name[0])));
	if (length > 0) return std::wstring(name, static_cast<size_t>(length));

	if (format >= CF_PRIVATEFIRST && format <= CF_PRIVATELAST) return L"CF_PRIVATE";
	if (format >= CF_GDIOBJFIRST && format <= CF_GDIOBJLAST) return L"CF_GDIOBJ";
	if (format >= 0xC000 && format <= 0xFFFF) return L"Registered Clipboard Format";
	return L"";
}

UINT Clipboard::RegisterFormat(std::string name) {
	return RegisterFormat(Utf8ToWide(name));
}

UINT Clipboard::RegisterFormat(std::wstring name) {
	if (name.empty()) return 0;
	return RegisterClipboardFormatW(name.c_str());
}

ClipboardFormat Clipboard::FromWin32Format(UINT format) {
	switch (format) {
	case 0:
		return ClipboardFormat::None;
	case CF_TEXT:
		return ClipboardFormat::Text;
	case CF_BITMAP:
		return ClipboardFormat::Bitmap;
	case CF_METAFILEPICT:
		return ClipboardFormat::MetafilePict;
	case CF_SYLK:
		return ClipboardFormat::Sylk;
	case CF_DIF:
		return ClipboardFormat::Dif;
	case CF_TIFF:
		return ClipboardFormat::Tiff;
	case CF_OEMTEXT:
		return ClipboardFormat::OemText;
	case CF_DIB:
		return ClipboardFormat::Dib;
	case CF_PALETTE:
		return ClipboardFormat::Palette;
	case CF_PENDATA:
		return ClipboardFormat::PenData;
	case CF_RIFF:
		return ClipboardFormat::Riff;
	case CF_WAVE:
		return ClipboardFormat::Wave;
	case CF_UNICODETEXT:
		return ClipboardFormat::UnicodeText;
	case CF_ENHMETAFILE:
		return ClipboardFormat::EnhMetafile;
	case CF_HDROP:
		return ClipboardFormat::HDrop;
	case CF_LOCALE:
		return ClipboardFormat::Locale;
	case CF_DIBV5:
		return ClipboardFormat::DibV5;
	case CF_OWNERDISPLAY:
		return ClipboardFormat::OwnerDisplay;
	case CF_DSPTEXT:
		return ClipboardFormat::DspText;
	case CF_DSPBITMAP:
		return ClipboardFormat::DspBitmap;
	case CF_DSPMETAFILEPICT:
		return ClipboardFormat::DspMetafilePict;
	case CF_DSPENHMETAFILE:
		return ClipboardFormat::DspEnhMetafile;
	case CF_PRIVATEFIRST:
		return ClipboardFormat::PrivateFirst;
	case CF_PRIVATELAST:
		return ClipboardFormat::PrivateLast;
	case CF_GDIOBJFIRST:
		return ClipboardFormat::GdiObjectFirst;
	case CF_GDIOBJLAST:
		return ClipboardFormat::GdiObjectLast;
	default:
		return ClipboardFormat::Unknown;
	}
}

UINT Clipboard::ToWin32Format(ClipboardFormat format) {
	if (format == ClipboardFormat::Unknown || format == ClipboardFormat::None) return 0;
	return static_cast<UINT>(format);
}
