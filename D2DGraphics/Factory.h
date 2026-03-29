#pragma once
#include <D2D1_1.h>
#include <DWrite.h>
#include <wincodec.h>
#include <string>
#pragma comment(lib, "d2d1.lib")

class Factory {
private:
	static ID2D1Factory1* pD2DFactory;
	static IDWriteFactory* pDWriteFactory;
	static IWICImagingFactory* _pImageFactory;
public:
	static ID2D1Factory1* D2DFactory();
	static IDWriteFactory* DWriteFactory();
	static IWICImagingFactory* ImageFactory();
	static IWICBitmap* CreateWICBitmap(std::wstring path);
	static IWICBitmap* CreateWICBitmap(unsigned char* data, int size);
	static IWICBitmap* CreateWICBitmap(HBITMAP hb);
	static IWICBitmap* CreateWICBitmap(int width, int height);
	static IWICBitmap* CreateWICBitmap(HICON hb);
	static void SaveBitmap(IWICBitmap* bmp, const wchar_t* path);
	static ID2D1PathGeometry* CreateGeomtry();
	static IDWriteTextLayout* CreateStringLayout(std::wstring str, float width, float height, IDWriteTextFormat* font);
};
#define _D2DFactory     Factory::D2DFactory()
#define _DWriteFactory  Factory::DWriteFactory()
#define _ImageFactory   Factory::ImageFactory()