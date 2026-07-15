#pragma once
#include "defines.h"
#include <cstdint>
#include <string>
#include <vector>

enum class ClipboardFormat : int
{
	Unknown = -1,
	None = 0,
	Text = CF_TEXT,
	Bitmap = CF_BITMAP,
	MetafilePict = CF_METAFILEPICT,
	Sylk = CF_SYLK,
	Dif = CF_DIF,
	Tiff = CF_TIFF,
	OemText = CF_OEMTEXT,
	Dib = CF_DIB,
	Palette = CF_PALETTE,
	PenData = CF_PENDATA,
	Riff = CF_RIFF,
	Wave = CF_WAVE,
	UnicodeText = CF_UNICODETEXT,
	EnhMetafile = CF_ENHMETAFILE,
	HDrop = CF_HDROP,
	Locale = CF_LOCALE,
	DibV5 = CF_DIBV5,
	OwnerDisplay = CF_OWNERDISPLAY,
	DspText = CF_DSPTEXT,
	DspBitmap = CF_DSPBITMAP,
	DspMetafilePict = CF_DSPMETAFILEPICT,
	DspEnhMetafile = CF_DSPENHMETAFILE,
	PrivateFirst = CF_PRIVATEFIRST,
	PrivateLast = CF_PRIVATELAST,
	GdiObjectFirst = CF_GDIOBJFIRST,
	GdiObjectLast = CF_GDIOBJLAST,

	Image = MetafilePict,
	FileDrop = HDrop
};

class Clipboard {
public:
	static std::string GetText();
	static std::wstring GetTextW();
	static bool SetText(std::string str);
	static bool SetText(std::wstring str);
	static bool Clear();
	static bool SetFile(std::string file);
	static bool SetFile(std::wstring file);
	static bool SetFiles(std::vector<std::string> files);
	static bool SetFiles(std::vector<std::wstring> files);
	static std::string GetFile();
	static std::wstring GetFileW();
	static std::vector<std::string> GetFiles();
	static std::vector<std::wstring> GetFilesW();
	static bool SetImage(HBITMAP bmp);
	static HBITMAP GetImage();
	static ClipboardFormat GetFormat();
	static UINT GetFormatId();
	static std::vector<UINT> GetFormats();
	static std::vector<uint8_t> GetData(UINT format);
	static bool SetData(UINT format, const void* data, size_t size);
	static bool SetData(UINT format, std::vector<uint8_t> data);
	static bool IsFormatAvailable(ClipboardFormat format);
	static bool IsFormatAvailable(UINT format);
	static std::string GetFormatName(UINT format);
	static std::wstring GetFormatNameW(UINT format);
	static UINT RegisterFormat(std::string name);
	static UINT RegisterFormat(std::wstring name);
	static ClipboardFormat FromWin32Format(UINT format);
	static UINT ToWin32Format(ClipboardFormat format);
};

