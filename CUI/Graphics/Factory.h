#pragma once
#include <D2D1_1.h>
#include <d3d11.h>
#include <DWrite.h>
#include <wincodec.h>
#include <string>

class Factory
{
private:
	static IDXGIFactory1* dxgiFactory;
	static ID2D1Device* pD2DDevice;
	static ID2D1Factory1* pD2DFactory;
	static IDWriteFactory* pDWriteFactory;
	static IWICImagingFactory* _pImageFactory;
	static IDXGIDevice1* dxgiDevice;
	static ID3D11Device* pd3dDevice;
	static ID3D11DeviceContext* pd3dDeviceContext;
public:
	static ID3D11DeviceContext* D3DDeviceContext();
	static ID3D11Device* D3DDevice();
	static IDXGIDevice1* DxgiDevice();
	static IDXGIFactory1* DxgiFactory();
	static ID2D1Device* D2DDevice();
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
	static HRESULT ExtractID2D1Bitmap1ToIWICBitmap(
		ID2D1DeviceContext* pDeviceContext,
		ID2D1Bitmap1* pSourceBitmap,
		IWICImagingFactory* pWICFactory,
		IWICBitmap** ppWICBitmap);
};
#define _D3DDeviceContext  Factory::D3DDeviceContext()
#define _D3DDevice   Factory::D3DDevice()
#define _D2DFactory     Factory::D2DFactory()
#define _DWriteFactory  Factory::DWriteFactory()
#define _ImageFactory   Factory::ImageFactory()
#define _IDXGIDevice1   Factory::DxgiDevice()
#define _DxgiFactory   Factory::DxgiFactory()
#define _D2DDevice   Factory::D2DDevice()