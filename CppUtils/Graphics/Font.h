#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_
#endif

#include "../Utils/defines.h"
#include <D2D1.h>
#include <dwrite.h>
#include <string>
#include <vector>
class Font {
private:
	float _fontSize;
	std::wstring _fontName;
	IDWriteTextFormat* _fontObject;
	Font(IDWriteTextFormat* fontObject, float _fontsize);
public:
	float FontHeight;
	Font(std::wstring fontFamilyName, float _fontsize);
	~Font();
	
	READONLY_PROPERTY(IDWriteTextFormat*, FontObject);
	GET(IDWriteTextFormat*, FontObject);

	PROPERTY(float, FontSize);
	GET(float, FontSize);
	SET(float, FontSize);

	PROPERTY(std::wstring, FontName);
	GET(std::wstring, FontName);
	SET(std::wstring, FontName);

	D2D1_SIZE_F GetTextSize(std::wstring str, float w = FLT_MAX, float h = FLT_MAX);
	D2D1_SIZE_F GetTextSize(IDWriteTextLayout* textLayout);
	D2D1_SIZE_F GetTextSize(wchar_t c);
	int HitTestTextPosition(std::wstring str, float x, float y);
	int HitTestTextPosition(std::wstring str, float width, float height, float x, float y);
	std::vector<DWRITE_HIT_TEST_METRICS> HitTestTextRange(std::wstring str, UINT32 start, UINT32 len);
	std::vector<DWRITE_HIT_TEST_METRICS> HitTestTextRange(IDWriteTextLayout* textLayout, UINT32 start, UINT32 len);
	int HitTestTextPosition(IDWriteTextLayout* textLayout, float x, float y);
	int HitTestTextPosition(IDWriteTextLayout* textLayout, float width, float height, float x, float y);
	static std::vector<std::wstring> GetSystemFonts();
};

