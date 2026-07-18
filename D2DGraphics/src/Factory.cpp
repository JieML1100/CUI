#include "Factory.h"
#include <wrl/client.h>
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "Dwrite.lib")
#pragma comment(lib, "windowscodecs.lib")

ID2D1Factory1* Factory::pD2DFactory = NULL;
IDWriteFactory* Factory::pDWriteFactory = NULL;
IWICImagingFactory* Factory::_pImageFactory = NULL;

ID2D1Factory1* Factory::D2DFactory() {
	if (!pD2DFactory) {
		D2D1_FACTORY_OPTIONS factoryOptions = {};
#if defined(_DEBUG)
		factoryOptions.debugLevel = D2D1_DEBUG_LEVEL_ERROR;
#endif
		D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, __uuidof(ID2D1Factory1), &factoryOptions, reinterpret_cast<void**>(&pD2DFactory));
	}
	return pD2DFactory;
}
IDWriteFactory* Factory::DWriteFactory() {
	if (!pDWriteFactory)
		DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&pDWriteFactory));
	return pDWriteFactory;
}
IWICImagingFactory* Factory::ImageFactory() {
	if (!_pImageFactory) {
		auto m = LoadLibraryA("Ole32.dll");
		if (m) {
			HRESULT hr = ((decltype(CoInitialize)*)GetProcAddress(m, "CoInitialize"))(NULL);
			hr = ((decltype(CoCreateInstance)*)GetProcAddress(m, "CoCreateInstance"))(CLSID_WICImagingFactory1, NULL, CLSCTX_INPROC_SERVER, IID_IWICImagingFactory, (LPVOID*)&_pImageFactory);
		}
	}
	return _pImageFactory;
}
IWICBitmap* Factory::CreateWICBitmap(const std::wstring& path) {
	using Microsoft::WRL::ComPtr;

	if (_ImageFactory == nullptr || path.empty())
		return nullptr;

	ComPtr<IWICBitmapDecoder> decoder;
	ComPtr<IWICBitmapFrameDecode> frame;
	ComPtr<IWICFormatConverter> converter;
	ComPtr<IWICBitmap> bitmap;

	HRESULT hr = _ImageFactory->CreateDecoderFromFilename(
		path.c_str(),
		nullptr,
		GENERIC_READ,
		WICDecodeMetadataCacheOnLoad,
		decoder.GetAddressOf());

	if (FAILED(hr))
		return nullptr;

	hr = decoder->GetFrame(
		0,
		frame.GetAddressOf());

	if (FAILED(hr))
		return nullptr;

	hr = _ImageFactory->CreateFormatConverter(
		converter.GetAddressOf());

	if (FAILED(hr))
		return nullptr;

	hr = converter->Initialize(
		frame.Get(),
		GUID_WICPixelFormat32bppPBGRA,
		WICBitmapDitherTypeNone,
		nullptr,
		0.0,
		WICBitmapPaletteTypeCustom);

	if (FAILED(hr))
		return nullptr;

	hr = _ImageFactory->CreateBitmapFromSource(
		converter.Get(),
		WICBitmapCacheOnLoad,
		bitmap.GetAddressOf());

	if (FAILED(hr))
		return nullptr;

	return bitmap.Detach();
}
IWICBitmap* Factory::CreateWICBitmap(const uint8_t* data, size_t size)
{
	using Microsoft::WRL::ComPtr;

	if (_ImageFactory == nullptr ||
		data == nullptr ||
		size == 0 ||
		size > static_cast<size_t>(MAXDWORD))
	{
		return nullptr;
	}

	ComPtr<IWICStream> stream;
	ComPtr<IWICBitmapDecoder> decoder;
	ComPtr<IWICBitmapFrameDecode> frame;
	ComPtr<IWICFormatConverter> converter;
	ComPtr<IWICBitmap> bitmap;

	HRESULT hr = _ImageFactory->CreateStream(
		stream.GetAddressOf());

	if (FAILED(hr))
		return nullptr;

	hr = stream->InitializeFromMemory(reinterpret_cast<BYTE*>(const_cast<uint8_t*>(data)), static_cast<DWORD>(size));

	if (FAILED(hr))
		return nullptr;

	hr = _ImageFactory->CreateDecoderFromStream(
		stream.Get(),
		nullptr,
		WICDecodeMetadataCacheOnDemand,
		decoder.GetAddressOf());

	if (FAILED(hr))
		return nullptr;

	hr = decoder->GetFrame(
		0,
		frame.GetAddressOf());

	if (FAILED(hr))
		return nullptr;

	hr = _ImageFactory->CreateFormatConverter(
		converter.GetAddressOf());

	if (FAILED(hr))
		return nullptr;

	hr = converter->Initialize(
		frame.Get(),
		GUID_WICPixelFormat32bppPBGRA,
		WICBitmapDitherTypeNone,
		nullptr,
		0.0,
		WICBitmapPaletteTypeCustom);

	if (FAILED(hr))
		return nullptr;

	hr = _ImageFactory->CreateBitmapFromSource(
		converter.Get(),
		WICBitmapCacheOnLoad,
		bitmap.GetAddressOf());

	if (FAILED(hr))
		return nullptr;

	return bitmap.Detach();
}
IWICBitmap* Factory::CreateWICBitmap(HBITMAP hb) {
	IWICBitmap* wb = NULL;
	_ImageFactory->CreateBitmapFromHBITMAP(hb, 0, WICBitmapUsePremultipliedAlpha, &wb);
	return wb;
}
IWICBitmap* Factory::CreateWICBitmap(int width, int height) {
	IWICBitmap* wb = NULL;
	_ImageFactory->CreateBitmap(width, height, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnDemand, &wb);
	return wb;
}
IWICBitmap* Factory::CreateWICBitmap(HICON hb) {
	IWICBitmap* wb = NULL;
	_ImageFactory->CreateBitmapFromHICON(hb, &wb);
	return wb;
}
void Factory::SaveBitmap(IWICBitmap* bmp, const wchar_t* path) {
	UINT w, h;
	bmp->GetSize(&w, &h);
	IWICBitmapEncoder* pEncoder = NULL;
	IWICStream* pStream = NULL;
	IWICBitmapFrameEncode* pFrameEncode = NULL;
	WICPixelFormatGUID format = GUID_WICPixelFormat32bppPBGRA;
	_ImageFactory->CreateStream(&pStream);
	_ImageFactory->CreateEncoder(GUID_ContainerFormatPng, NULL, &pEncoder);
	pStream->InitializeFromFilename(path, GENERIC_WRITE);
	pEncoder->Initialize(pStream, WICBitmapEncoderNoCache);
	pEncoder->CreateNewFrame(&pFrameEncode, NULL);
	pFrameEncode->Initialize(NULL);
	pFrameEncode->SetSize(w, h);
	pFrameEncode->SetPixelFormat(&format);
	pFrameEncode->WriteSource(bmp, NULL);
	pFrameEncode->Commit();
	pEncoder->Commit();
	pFrameEncode->Release();
	pStream->Release();
	pEncoder->Release();
}
ID2D1PathGeometry* Factory::CreateGeomtry() {
	ID2D1PathGeometry* geo = NULL;
	HRESULT hr = _D2DFactory->CreatePathGeometry(&geo);
	if (SUCCEEDED(hr))
		return geo;
	return NULL;
}
IDWriteTextLayout* Factory::CreateStringLayout(std::wstring str, float width, float height, IDWriteTextFormat* font) {
	IDWriteTextLayout* textLayout = NULL;
	_DWriteFactory->CreateTextLayout(str.c_str(), static_cast<UINT32>(str.size()), font, width, height, &textLayout);
	return textLayout;
}