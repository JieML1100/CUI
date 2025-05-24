#include "Factory.h"
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "Dwrite.lib")
#pragma comment(lib, "windowscodecs.lib")

IDXGIFactory1* Factory::dxgiFactory = NULL;
ID2D1Device* Factory::pD2DDevice = NULL;
ID2D1Factory1* Factory::pD2DFactory = NULL;
IDWriteFactory* Factory::pDWriteFactory = NULL;
IWICImagingFactory* Factory::_pImageFactory = NULL;
IDXGIDevice1* Factory::dxgiDevice = NULL;
ID3D11Device* Factory::pd3dDevice = NULL;
ID3D11DeviceContext* Factory::pd3dDeviceContext = NULL;

ID3D11DeviceContext* Factory::D3DDeviceContext()
{
	if (!pd3dDevice)
	{
		UINT createDeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(_DEBUG)
		createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
		D3D_FEATURE_LEVEL featureLevels[] =
		{
			D3D_FEATURE_LEVEL_11_0,
		};
		D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL,
			createDeviceFlags, featureLevels, 1, D3D11_SDK_VERSION,
			&pd3dDevice, NULL, &pd3dDeviceContext);
	}
	return pd3dDeviceContext;
}
ID3D11Device* Factory::D3DDevice()
{
	if (!pd3dDevice)
	{
		UINT createDeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(_DEBUG)
		createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
		D3D_FEATURE_LEVEL featureLevels[] =
		{
			D3D_FEATURE_LEVEL_11_0,
		};
		D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL,
			createDeviceFlags, featureLevels, 1, D3D11_SDK_VERSION,
			&pd3dDevice, NULL, &pd3dDeviceContext);
	}
	return pd3dDevice;
}
IDXGIDevice1* Factory::DxgiDevice()
{
	if (dxgiDevice == NULL)
		D3DDevice()->QueryInterface(&dxgiDevice);
	return dxgiDevice;
}
IDXGIFactory1* Factory::DxgiFactory()
{
	if (dxgiFactory == NULL)
	{
		IDXGIAdapter* adapter = NULL;
		DxgiDevice()->GetAdapter(&adapter);
		adapter->GetParent(__uuidof(IDXGIFactory1), (void**)&dxgiFactory);
	}
	return dxgiFactory;
}
ID2D1Device* Factory::D2DDevice()
{
	if (pD2DDevice == NULL)
	{
		_D2DFactory->CreateDevice(DxgiDevice(), &pD2DDevice);
	}
	return pD2DDevice;
}
ID2D1Factory1* Factory::D2DFactory()
{
	if (!pD2DFactory)
	{
		D2D1_FACTORY_OPTIONS factoryOptions = {};
#if defined(_DEBUG)
		factoryOptions.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif
		D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, __uuidof(ID2D1Factory1), &factoryOptions, reinterpret_cast<void**>(&pD2DFactory));
	}
	return pD2DFactory;
}
IDWriteFactory* Factory::DWriteFactory()
{
	if (!pDWriteFactory)
		DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&pDWriteFactory));
	return pDWriteFactory;
}
IWICImagingFactory* Factory::ImageFactory()
{
	if (!_pImageFactory) {
		auto m = LoadLibraryA("Ole32.dll");
		if (m) {
			HRESULT hr = ((decltype(CoInitialize)*)GetProcAddress(m, "CoInitialize"))(NULL);
			hr = ((decltype(CoCreateInstance)*)GetProcAddress(m, "CoCreateInstance"))(CLSID_WICImagingFactory1, NULL, CLSCTX_INPROC_SERVER, IID_IWICImagingFactory, (LPVOID*)&_pImageFactory);
		}
	}
	return _pImageFactory;
}
IWICBitmap* Factory::CreateWICBitmap(std::wstring path)
{
	IWICBitmap* wb = NULL;
	IWICBitmapDecoder* bitmapdecoder = NULL;
	HRESULT hr = _ImageFactory->CreateDecoderFromFilename(path.c_str(), NULL, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &bitmapdecoder);
	if SUCCEEDED(hr)
	{
		IWICBitmapFrameDecode* pframe = NULL;
		bitmapdecoder->GetFrame(0, &pframe);
		IWICFormatConverter* fmtcovter = NULL;
		_ImageFactory->CreateFormatConverter(&fmtcovter);
		fmtcovter->Initialize(pframe, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, NULL, 0.0f, WICBitmapPaletteTypeCustom);
		pframe->Release();
		bitmapdecoder->Release();
		return (IWICBitmap*)fmtcovter;
	}
	return wb;
}
IWICBitmap* Factory::CreateWICBitmap(unsigned char* data, int size)
{
	ID2D1Bitmap* bmp = NULL;
	IWICBitmapDecoder* bitmapdecoder = NULL;
	IWICStream* pStream = NULL;
	HRESULT hr = _ImageFactory->CreateStream(&pStream);
	if (!SUCCEEDED(hr))
	{
		return NULL;
	}
	hr = pStream->InitializeFromMemory(data, size);
	if (!SUCCEEDED(hr))
	{
		return NULL;
	}
	hr = _ImageFactory->CreateDecoderFromStream(pStream, NULL, WICDecodeMetadataCacheOnDemand, &bitmapdecoder);
	if (!SUCCEEDED(hr))
	{
		return NULL;
	}
	if (bitmapdecoder)
	{
		IWICBitmapFrameDecode* pframe = NULL;
		bitmapdecoder->GetFrame(0, &pframe);
		IWICFormatConverter* fmtcovter = NULL;
		_ImageFactory->CreateFormatConverter(&fmtcovter);
		fmtcovter->Initialize(pframe, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, NULL, 0.0f, WICBitmapPaletteTypeCustom);
		pframe->Release();
		bitmapdecoder->Release();
		return (IWICBitmap*)fmtcovter;
	}
	return NULL;
}
IWICBitmap* Factory::CreateWICBitmap(HBITMAP hb)
{
	IWICBitmap* wb = NULL;
	_ImageFactory->CreateBitmapFromHBITMAP(hb, 0, WICBitmapUsePremultipliedAlpha, &wb);
	return wb;
}
IWICBitmap* Factory::CreateWICBitmap(int width, int height)
{
	IWICBitmap* wb = NULL;
	_ImageFactory->CreateBitmap(width, height, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnDemand, &wb);
	return wb;
}
IWICBitmap* Factory::CreateWICBitmap(HICON hb)
{
	IWICBitmap* wb = NULL;
	_ImageFactory->CreateBitmapFromHICON(hb, &wb);
	return wb;
}
void Factory::SaveBitmap(IWICBitmap* bmp, const wchar_t* path)
{
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
ID2D1PathGeometry* Factory::CreateGeomtry()
{
	ID2D1PathGeometry* geo = NULL;
	HRESULT hr = _D2DFactory->CreatePathGeometry(&geo);
	if (SUCCEEDED(hr))
		return geo;
	return NULL;
}
IDWriteTextLayout* Factory::CreateStringLayout(std::wstring str, float width, float height, IDWriteTextFormat* font)
{
	IDWriteTextLayout* textLayout = NULL;
	_DWriteFactory->CreateTextLayout(str.c_str(), str.size(), font, width, height, &textLayout);
	return textLayout;
}
HRESULT Factory::ExtractID2D1Bitmap1ToIWICBitmap(
	ID2D1DeviceContext* pDeviceContext,
	ID2D1Bitmap1* pSourceBitmap,
	IWICImagingFactory* pWICFactory,
	IWICBitmap** ppWICBitmap)
{
	if (!pDeviceContext || !pSourceBitmap || !pWICFactory || !ppWICBitmap)
		return E_INVALIDARG;

	HRESULT hr = S_OK;

	
	D2D1_SIZE_U size = pSourceBitmap->GetPixelSize();
	D2D1_PIXEL_FORMAT pixelFormat = pSourceBitmap->GetPixelFormat();

	
	D2D1_BITMAP_PROPERTIES1 bitmapProperties = D2D1::BitmapProperties1(
		D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
		pixelFormat);

	ID2D1Bitmap1* pCPUBitmap = NULL;
	hr = pDeviceContext->CreateBitmap(
		size,
		NULL,
		0,
		&bitmapProperties,
		&pCPUBitmap);

	if (SUCCEEDED(hr))
	{
		
		hr = pCPUBitmap->CopyFromBitmap(NULL, pSourceBitmap, NULL);

		if (SUCCEEDED(hr))
		{
			
			D2D1_MAPPED_RECT mappedRect;
			hr = pCPUBitmap->Map(D2D1_MAP_OPTIONS_READ, &mappedRect);
			if (SUCCEEDED(hr))
			{
				
				hr = pWICFactory->CreateBitmap(
					size.width,
					size.height,
					GUID_WICPixelFormat32bppPBGRA, 
					WICBitmapCacheOnLoad,
					ppWICBitmap);

				if (SUCCEEDED(hr))
				{
					
					IWICBitmapLock* pLock = NULL;
					WICRect rect = { 0, 0, static_cast<INT>(size.width), static_cast<INT>(size.height) };
					hr = (*ppWICBitmap)->Lock(&rect, WICBitmapLockWrite, &pLock);
					if (SUCCEEDED(hr))
					{
						
						UINT bufferSize = 0;
						BYTE* pBuffer = NULL;
						UINT stride = 0;
						hr = pLock->GetDataPointer(&bufferSize, &pBuffer);
						if (SUCCEEDED(hr))
						{
							hr = pLock->GetStride(&stride);
							if (SUCCEEDED(hr))
							{
								
								for (UINT y = 0; y < size.height; y++)
								{
									memcpy(
										pBuffer + y * stride,
										mappedRect.bits + y * mappedRect.pitch,
										min(stride, mappedRect.pitch));
								}
							}
						}
						pLock->Release();
					}
				}
				pCPUBitmap->Unmap();
			}
		}
		pCPUBitmap->Release();
	}
	return hr;
}