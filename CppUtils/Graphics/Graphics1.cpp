#include "Graphics1.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <numbers>

#include <d2derr.h>

#include <d3d11.h>
#include <d3d10_1.h>
#include <dxgi1_2.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

using Microsoft::WRL::ComPtr;
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
// 常量定义
constexpr float INV_255_1 = 1.0f / 255.0f;
constexpr float DEG_TO_RAD = M_PI / 180.0f;
constexpr float OUTLINE_OFFSET = 1.0f;

namespace {
	struct SharedD2D11Resources {
		ComPtr<ID3D11Device> d3dDevice;
		ComPtr<ID3D11DeviceContext> d3dContext;
		ComPtr<IDXGIDevice> dxgiDevice;
		ComPtr<ID2D1Device> d2dDevice;
	};

	SharedD2D11Resources& Shared() {
		static SharedD2D11Resources s;
		return s;
	}

	bool IsDeviceRemovedHr(HRESULT hr) {
		return hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET || hr == DXGI_ERROR_DEVICE_HUNG ||
			hr == DXGI_ERROR_DRIVER_INTERNAL_ERROR;
	}

	void ResetSharedDevice() {
		auto& s = Shared();
		s.d2dDevice.Reset();
		s.dxgiDevice.Reset();
		s.d3dContext.Reset();
		s.d3dDevice.Reset();
	}

	HRESULT CreateSharedDeviceIfNeeded() {
		auto& s = Shared();
		// 远程桌面断开/重连、显示设备切换等可能导致 D3D 设备被移除。
		// 如果继续复用旧设备，会导致后续 CreateDeviceContext/Present/EndDraw 反复失败。
		if (s.d3dDevice) {
			HRESULT reason = s.d3dDevice->GetDeviceRemovedReason();
			if (reason != S_OK && IsDeviceRemovedHr(reason)) {
				ResetSharedDevice();
			}
		}
		if (s.d2dDevice && s.dxgiDevice && s.d3dDevice) {
			return S_OK;
		}

		UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_VIDEO_SUPPORT;
#if defined(_DEBUG)
		flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

		D3D_FEATURE_LEVEL featureLevels[] = {
			D3D_FEATURE_LEVEL_11_1,
			D3D_FEATURE_LEVEL_11_0,
			D3D_FEATURE_LEVEL_10_1,
			D3D_FEATURE_LEVEL_10_0,
		};
		D3D_FEATURE_LEVEL obtained = D3D_FEATURE_LEVEL_10_0;

		ComPtr<ID3D11Device> dev;
		ComPtr<ID3D11DeviceContext> ctx;
		HRESULT hr = D3D11CreateDevice(
			nullptr,
			D3D_DRIVER_TYPE_HARDWARE,
			nullptr,
			flags,
			featureLevels,
			_countof(featureLevels),
			D3D11_SDK_VERSION,
			&dev,
			&obtained,
			&ctx);

		if (FAILED(hr)) {
			// fallback to WARP
			hr = D3D11CreateDevice(
				nullptr,
				D3D_DRIVER_TYPE_WARP,
				nullptr,
				flags,
				featureLevels,
				_countof(featureLevels),
				D3D11_SDK_VERSION,
				&dev,
				&obtained,
				&ctx);
		}
		if (FAILED(hr)) {
			return hr;
		}

		// 共享设备会被 UI 线程（D2D）与后台线程（例如视频解码/处理）并发使用。
		// 启用 multithread protection，避免 D3D11 immediate context 的跨线程调用引发未定义行为。
		ComPtr<ID3D10Multithread> multithread;
		if (SUCCEEDED(dev.As(&multithread)) && multithread) {
			multithread->SetMultithreadProtected(TRUE);
		}

		ComPtr<IDXGIDevice> dxgiDevice;
		hr = dev.As(&dxgiDevice);
		if (FAILED(hr)) {
			return hr;
		}

		ComPtr<ID2D1Device> d2dDevice;
		hr = _D2DFactory->CreateDevice(dxgiDevice.Get(), &d2dDevice);
		if (FAILED(hr)) {
			return hr;
		}

		s.d3dDevice = dev;
		s.d3dContext = ctx;
		s.dxgiDevice = dxgiDevice;
		s.d2dDevice = d2dDevice;
		return S_OK;
	}

	bool WritePixelsToWicBitmap(IWICBitmap* dst, const void* src, UINT stride, UINT width, UINT height) {
		if (!dst || !src || width == 0 || height == 0 || stride == 0) {
			return false;
		}

		WICRect rect{ 0,0, static_cast<INT>(width), static_cast<INT>(height) };
		ComPtr<IWICBitmapLock> lock;
		if (FAILED(dst->Lock(&rect, WICBitmapLockWrite, &lock))) {
			return false;
		}
		UINT dstStride = 0;
		UINT dstSize = 0;
		BYTE* dstPtr = nullptr;
		if (FAILED(lock->GetStride(&dstStride)) || FAILED(lock->GetDataPointer(&dstSize, &dstPtr)) || !dstPtr) {
			return false;
		}

		const BYTE* srcBytes = static_cast<const BYTE*>(src);
		UINT copyStride = std::min<UINT>(dstStride, stride);
		for (UINT y = 0; y < height; ++y) {
			memcpy(dstPtr + y * dstStride, srcBytes + y * stride, copyStride);
		}
		return true;
	}
}

HRESULT Graphics1_EnsureSharedD3DDevice() {
	return CreateSharedDeviceIfNeeded();
}

ID3D11Device* Graphics1_GetSharedD3DDevice() {
	CreateSharedDeviceIfNeeded();
	return Shared().d3dDevice.Get();
}

IDXGIDevice* Graphics1_GetSharedDXGIDevice() {
	CreateSharedDeviceIfNeeded();
	return Shared().dxgiDevice.Get();
}

namespace {
	Font* DefaultFontObject1() {
		static Font* defaultFont = new Font(L"Arial", 18.0f);
		return defaultFont;
	}
}

D2DGraphics1::D2DGraphics1() = default;

D2DGraphics1::D2DGraphics1(IWICBitmap* bitmap, bool takeOwnership) {
	InitializeWithWicBitmap(
		bitmap,
		takeOwnership,
		96.0f,
		96.0f,
		DXGI_FORMAT_B8G8R8A8_UNORM,
		D2D1_ALPHA_MODE_PREMULTIPLIED);
}

D2DGraphics1::D2DGraphics1(const BitmapSource* bitmap) {
	InitializeWithWicBitmap(
		bitmap ? bitmap->GetWicBitmap() : nullptr,
		false,
		96.0f,
		96.0f,
		DXGI_FORMAT_B8G8R8A8_UNORM,
		D2D1_ALPHA_MODE_PREMULTIPLIED);
}

D2DGraphics1::D2DGraphics1(IDXGISwapChain* swapChain) {
	InitializeWithSwapChain(swapChain);
}

D2DGraphics1::D2DGraphics1(const InitOptions& options) {
	Initialize(options);
}

D2DGraphics1::~D2DGraphics1() = default;

void D2DGraphics1::SetDpi(FLOAT dpiX, FLOAT dpiY) {
	if (pDeviceContext) {
		pDeviceContext->SetDpi(dpiX, dpiY);
	}
}

HRESULT D2DGraphics1::EnsureDeviceContext() {
	return EnsureDeviceResources();
}

HRESULT D2DGraphics1::EnsureDeviceResources() {
	if (pD2DDevice && pDeviceContext) {
		return S_OK;
	}

	HRESULT hr = CreateSharedDeviceIfNeeded();
	if (FAILED(hr)) {
		return hr;
	}

	pD2DDevice = Shared().d2dDevice;
	if (!pD2DDevice) {
		return E_FAIL;
	}

	ComPtr<ID2D1DeviceContext> dc;
	hr = pD2DDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &dc);
	if (FAILED(hr)) {
		return hr;
	}
	pDeviceContext = dc;
	return S_OK;
}

void D2DGraphics1::ResetTarget() {
	pSwapChain.Reset();
	pTargetBitmap.Reset();
	pDeviceContext.Reset();
	pD2DDevice.Reset();

	pWicTargetBitmap.Reset();
	Default_Brush.Reset();
	Default_Brush_Back.Reset();

	surfaceKind = SurfaceKind::None;
	wicDirty = false;
}

HRESULT D2DGraphics1::ConfigDefaultObjects() {
	if (!pDeviceContext) {
		return E_FAIL;
	}
	D2D1_COLOR_F defaultColor{ 1.0f, 1.0f, 1.0f, 1.0f };
	if (!Default_Brush) {
		HRESULT hr = pDeviceContext->CreateSolidColorBrush(defaultColor, Default_Brush.ReleaseAndGetAddressOf());
		if (FAILED(hr)) {
			return hr;
		}
	}
	else {
		Default_Brush->SetColor(defaultColor);
	}

	if (!Default_Brush_Back) {
		HRESULT hr = pDeviceContext->CreateSolidColorBrush(defaultColor, Default_Brush_Back.ReleaseAndGetAddressOf());
		if (FAILED(hr)) {
			return hr;
		}
	}
	else {
		Default_Brush_Back->SetColor(defaultColor);
	}

	return S_OK;
}

HRESULT D2DGraphics1::Initialize(const InitOptions& options) {
	HRESULT hr = S_OK;
	switch (options.kind) {
	case SurfaceKind::Offscreen:
		hr = InitializeWithSize(options.width, options.height, options.dpiX, options.dpiY, options.format, options.alphaMode);
		break;
	case SurfaceKind::ExternalBitmap:
		hr = InitializeWithWicBitmap(options.wicBitmap, options.takeOwnership, options.dpiX, options.dpiY, options.format, options.alphaMode);
		break;
	case SurfaceKind::None:
	case SurfaceKind::Compatible:
	case SurfaceKind::Hwnd:
	case SurfaceKind::DxgiSwapChain:
	default:
		hr = E_INVALIDARG;
		break;
	}
	return hr;
}

HRESULT D2DGraphics1::CreateTargetBitmapForSize(UINT width, UINT height, FLOAT dpiX, FLOAT dpiY, DXGI_FORMAT format, D2D1_ALPHA_MODE alphaMode) {
	if (!pDeviceContext) {
		return E_FAIL;
	}
	if (width == 0 || height == 0) {
		return E_INVALIDARG;
	}

	D2D1_BITMAP_PROPERTIES1 props = D2D1::BitmapProperties1(
		D2D1_BITMAP_OPTIONS_TARGET,
		D2D1::PixelFormat(format, alphaMode),
		dpiX,
		dpiY);

	ComPtr<ID2D1Bitmap1> bmp;
	HRESULT hr = pDeviceContext->CreateBitmap(D2D1::SizeU(width, height), nullptr, 0, &props, &bmp);
	if (FAILED(hr)) {
		return hr;
	}

	pTargetBitmap = bmp;
	pDeviceContext->SetTarget(pTargetBitmap.Get());
	return S_OK;
}

HRESULT D2DGraphics1::InitializeWithSize(UINT width, UINT height, FLOAT dpiX, FLOAT dpiY, DXGI_FORMAT format, D2D1_ALPHA_MODE alphaMode) {
	if (width == 0 || height == 0) {
		return E_INVALIDARG;
	}
	ResetTarget();

	HRESULT hr = EnsureDeviceResources();
	if (FAILED(hr)) {
		return hr;
	}

	hr = CreateTargetBitmapForSize(width, height, dpiX, dpiY, format, alphaMode);
	if (FAILED(hr)) {
		return hr;
	}

	ComPtr<IWICBitmap> wic;
	hr = _ImageFactory->CreateBitmap(width, height, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, &wic);
	if (FAILED(hr)) {
		return hr;
	}
	pWicTargetBitmap = wic;

	surfaceKind = SurfaceKind::Offscreen;
	wicDirty = true;
	return ConfigDefaultObjects();
}

HRESULT D2DGraphics1::InitializeWithWicBitmap(IWICBitmap* bitmap, bool takeOwnership, FLOAT dpiX, FLOAT dpiY, DXGI_FORMAT format, D2D1_ALPHA_MODE alphaMode) {
	if (!bitmap) {
		return E_INVALIDARG;
	}
	ResetTarget();

	if (takeOwnership) {
		pWicTargetBitmap.Attach(bitmap);
	}
	else {
		pWicTargetBitmap = bitmap;
	}

	UINT width = 0, height = 0;
	if (FAILED(pWicTargetBitmap->GetSize(&width, &height)) || width == 0 || height == 0) {
		ResetTarget();
		return E_INVALIDARG;
	}

	HRESULT hr = EnsureDeviceResources();
	if (FAILED(hr)) {
		ResetTarget();
		return hr;
	}

	hr = CreateTargetBitmapForSize(width, height, dpiX, dpiY, format, alphaMode);
	if (FAILED(hr)) {
		ResetTarget();
		return hr;
	}

	WICRect rect{ 0,0, static_cast<INT>(width), static_cast<INT>(height) };
	ComPtr<IWICBitmapLock> lock;
	if (SUCCEEDED(pWicTargetBitmap->Lock(&rect, WICBitmapLockRead, &lock))) {
		UINT stride = 0;
		UINT bufSize = 0;
		BYTE* buf = nullptr;
		if (SUCCEEDED(lock->GetStride(&stride)) && SUCCEEDED(lock->GetDataPointer(&bufSize, &buf)) && buf && stride) {
			pTargetBitmap->CopyFromMemory(nullptr, buf, stride);
		}
	}

	surfaceKind = SurfaceKind::ExternalBitmap;
	wicDirty = false;
	return ConfigDefaultObjects();
}

HRESULT D2DGraphics1::CreateTargetBitmapForSwapChain(IDXGISwapChain* swapChain) {
	if (!swapChain || !pDeviceContext) {
		return E_INVALIDARG;
	}
	ComPtr<IDXGISurface> surface;
	HRESULT hr = swapChain->GetBuffer(0, IID_PPV_ARGS(&surface));
	if (FAILED(hr)) {
		return hr;
	}
	if (!surface) {
		return E_FAIL;
	}

	D2D1_BITMAP_PROPERTIES1 props = D2D1::BitmapProperties1(
		D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
		D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED),
		0.0f,
		0.0f);

	ComPtr<ID2D1Bitmap1> bmp;
	hr = pDeviceContext->CreateBitmapFromDxgiSurface(surface.Get(), &props, &bmp);
	if (FAILED(hr)) {
		return hr;
	}
	pTargetBitmap = bmp;
	pDeviceContext->SetTarget(pTargetBitmap.Get());
	return S_OK;
}

HRESULT D2DGraphics1::InitializeWithSwapChain(IDXGISwapChain* swapChain) {
	if (!swapChain) {
		return E_INVALIDARG;
	}
	ResetTarget();

	HRESULT hr = EnsureDeviceResources();
	if (FAILED(hr)) {
		return hr;
	}

	pSwapChain = swapChain;
	hr = CreateTargetBitmapForSwapChain(pSwapChain.Get());
	if (FAILED(hr)) {
		ResetTarget();
		return hr;
	}

	surfaceKind = SurfaceKind::DxgiSwapChain;
	wicDirty = false;
	return ConfigDefaultObjects();
}

void D2DGraphics1::BeginRender() {
	if (!pDeviceContext) {
		return;
	}
	pDeviceContext->BeginDraw();
}

HRESULT D2DGraphics1::SyncTargetToWicIfNeeded() {
	if (!pDeviceContext || !pTargetBitmap || !pWicTargetBitmap) {
		return S_OK;
	}
	if (!wicDirty) {
		return S_OK;
	}

	// 使用 CPU_READ bitmap 做 readback
	D2D1_SIZE_U size = pTargetBitmap->GetPixelSize();
	if (size.width == 0 || size.height == 0) {
		wicDirty = false;
		return S_OK;
	}

	D2D1_BITMAP_PROPERTIES1 cpuProps = D2D1::BitmapProperties1(
		// CPU_READ 位图不能作为渲染目标，也不能被绘制；显式加 CANNOT_DRAW 更符合 D2D1.1 约束，兼容性更好
		D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
		pTargetBitmap->GetPixelFormat(),
		96.0f,
		96.0f);

	ComPtr<ID2D1Bitmap1> cpuBitmap;
	HRESULT hr = pDeviceContext->CreateBitmap(size, nullptr, 0, &cpuProps, &cpuBitmap);
	if (FAILED(hr)) {
		return hr;
	}

	// 从我们自己的 target bitmap 拷贝到 cpuBitmap（比 CopyFromRenderTarget 更确定/更稳定）
	hr = cpuBitmap->CopyFromBitmap(nullptr, pTargetBitmap.Get(), nullptr);
	if (FAILED(hr)) {
		return hr;
	}

	D2D1_MAPPED_RECT mapped{};
	hr = cpuBitmap->Map(D2D1_MAP_OPTIONS_READ, &mapped);
	if (FAILED(hr)) {
		return hr;
	}

	bool ok = WritePixelsToWicBitmap(pWicTargetBitmap.Get(), mapped.bits, mapped.pitch, size.width, size.height);
	cpuBitmap->Unmap();

	if (!ok) {
		return E_FAIL;
	}

	wicDirty = false;
	return S_OK;
}

void D2DGraphics1::EndRender() {
	if (!pDeviceContext) {
		return;
	}
	HRESULT hr = pDeviceContext->EndDraw();

	// 标记回写：offscreen/externalbitmap 需要保持 WIC 同步
	if (surfaceKind == SurfaceKind::Offscreen || surfaceKind == SurfaceKind::ExternalBitmap) {
		wicDirty = true;
		SyncTargetToWicIfNeeded();
	}

	if (surfaceKind == SurfaceKind::DxgiSwapChain && pSwapChain) {
		// 忽略 present 的返回值（调用方可自行处理）
		pSwapChain->Present(1, 0);
	}

	// 如果目标丢失，尝试重建（对 swapchain 特别常见）
	if (hr == D2DERR_RECREATE_TARGET) {
		if (surfaceKind == SurfaceKind::DxgiSwapChain && pSwapChain) {
			CreateTargetBitmapForSwapChain(pSwapChain.Get());
			ConfigDefaultObjects();
		}
	}
}

void D2DGraphics1::ReSize(UINT width, UINT height) {
	if (!pDeviceContext) {
		return;
	}

	width = std::max<UINT>(1, width);
	height = std::max<UINT>(1, height);

	if (surfaceKind == SurfaceKind::Offscreen) {
		InitializeWithSize(width, height, 96.0f, 96.0f, DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED);
		return;
	}

	if (surfaceKind == SurfaceKind::DxgiSwapChain && pSwapChain) {
		// 释放 target bitmap，然后 resize swapchain buffers，再重新绑定 target
		pTargetBitmap.Reset();
		pDeviceContext->SetTarget(nullptr);
		pDeviceContext->Flush();

		pSwapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
		CreateTargetBitmapForSwapChain(pSwapChain.Get());
		ConfigDefaultObjects();
		return;
	}
}

void D2DGraphics1::Clear(D2D1_COLOR_F color) {
	auto* ctx = pDeviceContext.Get();
	if (!ctx) return;
	ctx->Clear(color);
	wicDirty = true;
}

ID2D1SolidColorBrush* D2DGraphics1::GetColorBrush(D2D1_COLOR_F newcolor) {
	if (!Default_Brush && pDeviceContext) {
		if (FAILED(pDeviceContext->CreateSolidColorBrush(newcolor, Default_Brush.ReleaseAndGetAddressOf()))) {
			return nullptr;
		}
	}
	if (!Default_Brush) return nullptr;
	Default_Brush->SetColor(newcolor);
	return Default_Brush.Get();
}
ID2D1SolidColorBrush* D2DGraphics1::GetColorBrush(COLORREF newcolor) {
	return GetColorBrush(D2D1_COLOR_F{ GetRValue(newcolor) * INV_255_1,GetGValue(newcolor) * INV_255_1,GetBValue(newcolor) * INV_255_1,1.0f });
}
ID2D1SolidColorBrush* D2DGraphics1::GetColorBrush(int r, int g, int b) {
	return GetColorBrush(D2D1_COLOR_F{ r * INV_255_1,g * INV_255_1,b * INV_255_1,1.0f });
}
ID2D1SolidColorBrush* D2DGraphics1::GetColorBrush(float r, float g, float b, float a) {
	return GetColorBrush(D2D1_COLOR_F{ r,g,b,a });
}

ID2D1SolidColorBrush* D2DGraphics1::GetBackColorBrush(D2D1_COLOR_F newcolor) {
	if (!Default_Brush_Back && pDeviceContext) {
		if (FAILED(pDeviceContext->CreateSolidColorBrush(newcolor, Default_Brush_Back.ReleaseAndGetAddressOf()))) {
			return nullptr;
		}
	}
	if (!Default_Brush_Back) return nullptr;
	Default_Brush_Back->SetColor(newcolor);
	return Default_Brush_Back.Get();
}
ID2D1SolidColorBrush* D2DGraphics1::GetBackColorBrush(COLORREF newcolor) {
	return GetBackColorBrush(D2D1_COLOR_F{ GetRValue(newcolor) * INV_255_1,GetGValue(newcolor) * INV_255_1,GetBValue(newcolor) * INV_255_1,1.0f });
}
ID2D1SolidColorBrush* D2DGraphics1::GetBackColorBrush(int r, int g, int b) {
	return GetBackColorBrush(D2D1_COLOR_F{ r * INV_255_1,g * INV_255_1,b * INV_255_1,1.0f });
}
ID2D1SolidColorBrush* D2DGraphics1::GetBackColorBrush(float r, float g, float b, float a) {
	return GetBackColorBrush(D2D1_COLOR_F{ r,g,b,a });
}

// ---- 绘制/文本 API：基本沿用 Graphics.cpp（DeviceContext 继承 RenderTarget）----

void D2DGraphics1::DrawLine(D2D1_POINT_2F p1, D2D1_POINT_2F p2, D2D1_COLOR_F color, float linewidth) {
	auto* ctx = pDeviceContext.Get();
	if (!ctx) return;
	auto brush = GetColorBrush(color);
	if (!brush) return;
	ctx->DrawLine(p1, p2, brush, linewidth);
}
void D2DGraphics1::DrawLine(D2D1_POINT_2F p1, D2D1_POINT_2F p2, ID2D1Brush* brush, float linewidth) {
	auto* ctx = pDeviceContext.Get();
	if (!ctx || !brush) return;
	ctx->DrawLine(p1, p2, brush, linewidth);
}
void D2DGraphics1::DrawLine(float p1_x, float p1_y, float p2_x, float p2_y, D2D1_COLOR_F color, float linewidth) {
	DrawLine(D2D1::Point2F(p1_x, p1_y), D2D1::Point2F(p2_x, p2_y), color, linewidth);
}
void D2DGraphics1::DrawLine(float p1_x, float p1_y, float p2_x, float p2_y, ID2D1Brush* brush, float linewidth) {
	DrawLine(D2D1::Point2F(p1_x, p1_y), D2D1::Point2F(p2_x, p2_y), brush, linewidth);
}
void D2DGraphics1::DrawRect(D2D1_RECT_F rect, D2D1_COLOR_F color, float linewidth) {
	auto* ctx = pDeviceContext.Get();
	if (!ctx) return;
	auto brush = GetColorBrush(color);
	if (!brush) return;
	ctx->DrawRectangle(rect, brush, linewidth);
}
void D2DGraphics1::DrawRect(float left, float top, float width, float height, D2D1_COLOR_F color, float linewidth) {
	DrawRect(D2D1::RectF(left, top, left + width, top + height), color, linewidth);
}
void D2DGraphics1::DrawRoundRect(D2D1_RECT_F rect, D2D1_COLOR_F color, float linewidth, float r) {
	auto* ctx = pDeviceContext.Get();
	if (!ctx) return;
	auto brush = GetColorBrush(color);
	if (!brush) return;
	ctx->DrawRoundedRectangle(D2D1::RoundedRect(rect, r, r), brush, linewidth);
}
void D2DGraphics1::DrawRoundRect(float left, float top, float width, float height, D2D1_COLOR_F color, float linewidth, float r) {
	DrawRoundRect(D2D1::RectF(left, top, left + width, top + height), color, linewidth, r);
}
void D2DGraphics1::FillRect(D2D1_RECT_F rect, D2D1_COLOR_F color) {
	auto* ctx = pDeviceContext.Get();
	if (!ctx) return;
	auto brush = GetColorBrush(color);
	if (!brush) return;
	ctx->FillRectangle(rect, brush);
	wicDirty = true;
}
void D2DGraphics1::FillRect(D2D1_RECT_F rect, ID2D1Brush* brush) {
	auto* ctx = pDeviceContext.Get();
	if (!ctx || !brush) return;
	ctx->FillRectangle(rect, brush);
	wicDirty = true;
}
void D2DGraphics1::FillRect(float left, float top, float width, float height, D2D1_COLOR_F color) {
	FillRect(D2D1::RectF(left, top, left + width, top + height), color);
}
void D2DGraphics1::FillRect(float left, float top, float width, float height, ID2D1Brush* brush) {
	FillRect(D2D1::RectF(left, top, left + width, top + height), brush);
}
void D2DGraphics1::FillRoundRect(D2D1_RECT_F rect, D2D1_COLOR_F color, float r) {
	auto* ctx = pDeviceContext.Get();
	if (!ctx) return;
	auto brush = GetColorBrush(color);
	if (!brush) return;
	ctx->FillRoundedRectangle(D2D1::RoundedRect(rect, r, r), brush);
	wicDirty = true;
}
void D2DGraphics1::FillRoundRect(float left, float top, float width, float height, D2D1_COLOR_F color, float r) {
	FillRoundRect(D2D1::RectF(left, top, left + width, top + height), color, r);
}
void D2DGraphics1::DrawEllipse(D2D1_POINT_2F cent, float xr, float yr, D2D1_COLOR_F color, float linewidth) {
	auto* ctx = pDeviceContext.Get();
	if (!ctx) return;
	auto brush = GetColorBrush(color);
	if (!brush) return;
	ctx->DrawEllipse(D2D1::Ellipse(cent, xr, yr), brush, linewidth);
}
void D2DGraphics1::DrawEllipse(float x, float y, float xr, float yr, D2D1_COLOR_F color, float linewidth) {
	DrawEllipse(D2D1::Point2F(x, y), xr, yr, color, linewidth);
}
void D2DGraphics1::FillEllipse(D2D1_POINT_2F cent, float xr, float yr, D2D1_COLOR_F color) {
	auto* ctx = pDeviceContext.Get();
	if (!ctx) return;
	auto brush = GetColorBrush(color);
	if (!brush) return;
	ctx->FillEllipse(D2D1::Ellipse(cent, xr, yr), brush);
	wicDirty = true;
}
void D2DGraphics1::FillEllipse(float cx, float cy, float xr, float yr, D2D1_COLOR_F color) {
	FillEllipse(D2D1::Point2F(cx, cy), xr, yr, color);
}
void D2DGraphics1::DrawGeometry(ID2D1Geometry* geo, D2D1_COLOR_F color, float linewidth) {
	auto* ctx = pDeviceContext.Get();
	if (!ctx || !geo) return;
	auto brush = GetColorBrush(color);
	if (!brush) return;
	ctx->DrawGeometry(geo, brush, linewidth);
}
void D2DGraphics1::DrawGeometry(ID2D1Geometry* geo, ID2D1Brush* brush, float linewidth) {
	auto* ctx = pDeviceContext.Get();
	if (!ctx || !geo || !brush) return;
	ctx->DrawGeometry(geo, brush, linewidth);
}
void D2DGraphics1::FillGeometry(ID2D1Geometry* geo, D2D1_COLOR_F color) {
	auto* ctx = pDeviceContext.Get();
	if (!ctx || !geo) return;
	auto brush = GetColorBrush(color);
	if (!brush) return;
	ctx->FillGeometry(geo, brush);
	wicDirty = true;
}
void D2DGraphics1::FillGeometry(ID2D1Geometry* geo, ID2D1Brush* brush) {
	auto* ctx = pDeviceContext.Get();
	if (!ctx || !geo || !brush) return;
	ctx->FillGeometry(geo, brush);
	wicDirty = true;
}

namespace {
	// 辅助函数：创建饼图几何形状
	ComPtr<ID2D1PathGeometry> CreatePieGeometry(D2D1_POINT_2F center, float width, float height,
		float startAngle, float sweepAngle) {
		ComPtr<ID2D1PathGeometry> geo;
		geo.Attach(Factory::CreateGeomtry());
		if (!geo) return nullptr;

		ComPtr<ID2D1GeometrySink> sink;
		if (FAILED(geo->Open(&sink))) return nullptr;

		sink->BeginFigure(center, D2D1_FIGURE_BEGIN_FILLED);
		float startRad = startAngle * DEG_TO_RAD;
		float endRad = (startAngle + sweepAngle) * DEG_TO_RAD;
		D2D1_POINT_2F startPoint{ center.x + (width * 0.5f) * cosf(startRad), center.y - (height * 0.5f) * sinf(startRad) };
		D2D1_POINT_2F endPoint{ center.x + (width * 0.5f) * cosf(endRad), center.y - (height * 0.5f) * sinf(endRad) };
		sink->AddLine(startPoint);
		D2D1_SIZE_F arcSize{ width * 0.5f, height * 0.5f };
		D2D1_ARC_SIZE arcSizeFlag = (std::fabs(sweepAngle) <= 180.0f) ? D2D1_ARC_SIZE_SMALL : D2D1_ARC_SIZE_LARGE;
		D2D1_SWEEP_DIRECTION sweepDir = sweepAngle >= 0.0f ? D2D1_SWEEP_DIRECTION_CLOCKWISE : D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE;
		sink->AddArc(D2D1::ArcSegment(endPoint, arcSize, 0.0f, sweepDir, arcSizeFlag));
		sink->EndFigure(D2D1_FIGURE_END_CLOSED);
		sink->Close();
		return geo;
	}
}

void D2DGraphics1::FillPie(D2D1_POINT_2F center, float width, float height, float startAngle, float sweepAngle, D2D1_COLOR_F color) {
	auto* ctx = pDeviceContext.Get();
	if (!ctx) return;
	auto brush = GetColorBrush(color);
	if (!brush) return;
	auto geo = CreatePieGeometry(center, width, height, startAngle, sweepAngle);
	if (!geo) return;
	ctx->FillGeometry(geo.Get(), brush);
	wicDirty = true;
}

void D2DGraphics1::FillPie(D2D1_POINT_2F center, float width, float height, float startAngle, float sweepAngle, ID2D1Brush* brush) {
	auto* ctx = pDeviceContext.Get();
	if (!ctx || !brush) return;
	auto geo = CreatePieGeometry(center, width, height, startAngle, sweepAngle);
	if (!geo) return;
	ctx->FillGeometry(geo.Get(), brush);
	wicDirty = true;
}

void D2DGraphics1::DrawBitmap(ID2D1Bitmap* bmp, float x, float y, float opacity) {
	auto* ctx = pDeviceContext.Get();
	if (!ctx || !bmp) return;
	D2D1_SIZE_F siz = bmp->GetSize();
	ctx->DrawBitmap(bmp, D2D1::RectF(x, y, siz.width + x, siz.height + y), opacity);
}
void D2DGraphics1::DrawBitmap(ID2D1Bitmap* bmp, D2D1_RECT_F rect, float opacity) {
	auto* ctx = pDeviceContext.Get();
	if (!ctx || !bmp) return;
	ctx->DrawBitmap(bmp, rect, opacity);
}
void D2DGraphics1::DrawBitmap(ID2D1Bitmap* bmp, D2D1_RECT_F destRect, D2D1_RECT_F srcRect, float opacity) {
	auto* ctx = pDeviceContext.Get();
	if (!ctx || !bmp) return;
	ctx->DrawBitmap(bmp, destRect, opacity, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, srcRect);
}
void D2DGraphics1::DrawBitmap(ID2D1Bitmap* bmp, float x, float y, float w, float h, float opacity) {
	auto* ctx = pDeviceContext.Get();
	if (!ctx || !bmp) return;
	ctx->DrawBitmap(bmp, D2D1::RectF(x, y, w + x, h + y), opacity);
}
void D2DGraphics1::DrawBitmap(ID2D1Bitmap* bmp, float dest_x, float dest_y, float dest_w, float dest_h, float src_x, float src_y, float src_w, float src_h, float opacity) {
	auto* ctx = pDeviceContext.Get();
	if (!ctx || !bmp) return;
	ctx->DrawBitmap(bmp, D2D1::RectF(dest_x, dest_y, dest_w + dest_x, dest_h + dest_y), opacity, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, D2D1::RectF(src_x, src_y, src_w + src_x, src_h + src_y));
}

void D2DGraphics1::FillOpacityMask(ID2D1Bitmap* mask, D2D1_POINT_2F destPoint, D2D1_COLOR_F color, D2D1_OPACITY_MASK_CONTENT content) {
	auto* ctx = pDeviceContext.Get();
	if (!ctx || !mask) return;
	auto brush = GetColorBrush(color);
	if (!brush) return;
	auto bitmapSize = mask->GetSize();
	D2D1_RECT_F maskRect = D2D1::RectF(0, 0, bitmapSize.width, bitmapSize.height);
	D2D1_RECT_F destRect = D2D1::RectF(destPoint.x, destPoint.y, destPoint.x + bitmapSize.width, destPoint.y + bitmapSize.height);
	ctx->FillOpacityMask(mask, brush, content, &destRect, &maskRect);
	wicDirty = true;
}
void D2DGraphics1::FillOpacityMask(ID2D1Bitmap* mask, D2D1_RECT_F destRect, D2D1_COLOR_F color, D2D1_OPACITY_MASK_CONTENT content) {
	auto* ctx = pDeviceContext.Get();
	if (!ctx || !mask) return;
	auto brush = GetColorBrush(color);
	if (!brush) return;
	auto bitmapSize = mask->GetSize();
	D2D1_RECT_F maskRect = D2D1::RectF(0, 0, bitmapSize.width, bitmapSize.height);
	ctx->FillOpacityMask(mask, brush, content, &destRect, &maskRect);
	wicDirty = true;
}
void D2DGraphics1::FillOpacityMask(ID2D1Bitmap* mask, D2D1_RECT_F destRect, D2D1_RECT_F srcRect, D2D1_COLOR_F color, D2D1_OPACITY_MASK_CONTENT content) {
	auto* ctx = pDeviceContext.Get();
	if (!ctx || !mask) return;
	auto brush = GetColorBrush(color);
	if (!brush) return;
	ctx->FillOpacityMask(mask, brush, content, &destRect, &srcRect);
	wicDirty = true;
}

void D2DGraphics1::FillMesh(ID2D1Mesh* mesh, D2D1_COLOR_F color) {
	auto* ctx = pDeviceContext.Get();
	if (!ctx || !mesh) return;
	auto brush = GetColorBrush(color);
	if (!brush) return;
	ctx->FillMesh(mesh, brush);
	wicDirty = true;
}

IDWriteTextLayout* D2DGraphics1::CreateStringLayout(const std::wstring& str, float width, float height, Font* font) {
	IDWriteTextLayout* textLayout = nullptr;
	Font* resolvedFont = font ? font : DefaultFontObject1();
	if (!resolvedFont) return nullptr;
	IDWriteTextFormat* fnt = resolvedFont->FontObject;
	_DWriteFactory->CreateTextLayout(str.c_str(), static_cast<UINT32>(str.size()), fnt, width, height, &textLayout);
	return textLayout;
}

void D2DGraphics1::DrawStringLayout(IDWriteTextLayout* layout, float x, float y, D2D1_COLOR_F color) {
	auto* ctx = pDeviceContext.Get();
	if (!ctx || !layout) return;
	auto brush = GetColorBrush(color);
	if (!brush) return;
	ctx->DrawTextLayout(D2D1::Point2F(x, y), layout, brush);
	wicDirty = true;
}
void D2DGraphics1::DrawStringLayout(IDWriteTextLayout* layout, float x, float y, ID2D1Brush* brush) {
	auto* ctx = pDeviceContext.Get();
	if (!ctx || !layout || !brush) return;
	ctx->DrawTextLayout(D2D1::Point2F(x, y), layout, brush);
	wicDirty = true;
}

void D2DGraphics1::DrawStringLayoutCentered(IDWriteTextLayout* layout, float centerX, float centerY, D2D1_COLOR_F color) {
	auto* ctx = pDeviceContext.Get();
	if (!ctx || !layout) return;
	auto brush = GetColorBrush(color);
	if (!brush) return;
	D2D1_SIZE_F textSize = GetTextLayoutSize(layout);
	float x = centerX - textSize.width * 0.5f;
	float y = centerY - textSize.height * 0.5f;
	ctx->DrawTextLayout(D2D1::Point2F(x, y), layout, brush);
	wicDirty = true;
}
void D2DGraphics1::DrawStringLayoutCentered(IDWriteTextLayout* layout, float centerX, float centerY, ID2D1Brush* brush) {
	auto* ctx = pDeviceContext.Get();
	if (!ctx || !layout || !brush) return;
	D2D1_SIZE_F textSize = GetTextLayoutSize(layout);
	float x = centerX - textSize.width * 0.5f;
	float y = centerY - textSize.height * 0.5f;
	ctx->DrawTextLayout(D2D1::Point2F(x, y), layout, brush);
	wicDirty = true;
}

namespace {
	void DrawTextOutline(ID2D1DeviceContext* ctx, IDWriteTextLayout* layout, float x, float y,
		ID2D1SolidColorBrush* outlineBrush) {
		ctx->DrawTextLayout(D2D1::Point2F(x - OUTLINE_OFFSET, y - OUTLINE_OFFSET), layout, outlineBrush);
		ctx->DrawTextLayout(D2D1::Point2F(x + OUTLINE_OFFSET, y - OUTLINE_OFFSET), layout, outlineBrush);
		ctx->DrawTextLayout(D2D1::Point2F(x - OUTLINE_OFFSET, y + OUTLINE_OFFSET), layout, outlineBrush);
		ctx->DrawTextLayout(D2D1::Point2F(x + OUTLINE_OFFSET, y + OUTLINE_OFFSET), layout, outlineBrush);
	}
}

void D2DGraphics1::DrawStringLayoutOutlined(IDWriteTextLayout* layout, float x, float y, D2D1_COLOR_F textColor, D2D1_COLOR_F outlineColor) {
	auto* ctx = pDeviceContext.Get();
	if (!ctx || !layout) return;
	auto textBrush = GetColorBrush(textColor);
	auto outlineBrush = GetBackColorBrush(outlineColor);
	if (!textBrush || !outlineBrush) return;
	DrawTextOutline(ctx, layout, x, y, outlineBrush);
	ctx->DrawTextLayout(D2D1::Point2F(x, y), layout, textBrush);
	wicDirty = true;
}

void D2DGraphics1::DrawStringLayoutOutlined(IDWriteTextLayout* layout, float x, float y, ID2D1Brush* textBrush, D2D1_COLOR_F outlineColor) {
	auto* ctx = pDeviceContext.Get();
	if (!ctx || !layout || !textBrush) return;
	auto outlineBrush = GetBackColorBrush(outlineColor);
	if (!outlineBrush) return;
	DrawTextOutline(ctx, layout, x, y, outlineBrush);
	ctx->DrawTextLayout(D2D1::Point2F(x, y), layout, textBrush);
	wicDirty = true;
}

void D2DGraphics1::DrawStringLayoutCenteredOutlined(IDWriteTextLayout* layout, float centerX, float centerY, D2D1_COLOR_F textColor, D2D1_COLOR_F outlineColor) {
	auto* ctx = pDeviceContext.Get();
	if (!ctx || !layout) return;
	auto textBrush = GetColorBrush(textColor);
	auto outlineBrush = GetBackColorBrush(outlineColor);
	if (!textBrush || !outlineBrush) return;
	D2D1_SIZE_F textSize = GetTextLayoutSize(layout);
	float x = centerX - textSize.width * 0.5f;
	float y = centerY - textSize.height * 0.5f;
	DrawTextOutline(ctx, layout, x, y, outlineBrush);
	ctx->DrawTextLayout(D2D1::Point2F(x, y), layout, textBrush);
	wicDirty = true;
}

void D2DGraphics1::DrawStringLayoutCenteredOutlined(IDWriteTextLayout* layout, float centerX, float centerY, ID2D1Brush* textBrush, D2D1_COLOR_F outlineColor) {
	auto* ctx = pDeviceContext.Get();
	if (!ctx || !layout || !textBrush) return;
	auto outlineBrush = GetBackColorBrush(outlineColor);
	if (!outlineBrush) return;
	D2D1_SIZE_F textSize = GetTextLayoutSize(layout);
	float x = centerX - textSize.width * 0.5f;
	float y = centerY - textSize.height * 0.5f;
	DrawTextOutline(ctx, layout, x, y, outlineBrush);
	ctx->DrawTextLayout(D2D1::Point2F(x, y), layout, textBrush);
	wicDirty = true;
}

void D2DGraphics1::DrawStringLayoutEffect(IDWriteTextLayout* layout, float x, float y, D2D1_COLOR_F color, DWRITE_TEXT_RANGE subRange, D2D1_COLOR_F fontBack, Font* font) {
	if (!layout) return;
	auto* ctx = pDeviceContext.Get();
	if (!ctx) return;
	auto backBrush = GetBackColorBrush(fontBack);
	auto frontBrush = GetColorBrush(color);
	if (!backBrush || !frontBrush) return;
	layout->SetDrawingEffect(NULL, DWRITE_TEXT_RANGE{ 0, UINT_MAX });
	layout->SetDrawingEffect(backBrush, subRange);
	ctx->DrawTextLayout(D2D1::Point2F(x, y), layout, frontBrush);
	wicDirty = true;
}
void D2DGraphics1::DrawStringLayoutEffect(IDWriteTextLayout* layout, float x, float y, ID2D1Brush* brush, DWRITE_TEXT_RANGE subRange, D2D1_COLOR_F fontBack, Font* font) {
	if (!layout || !brush) return;
	auto* ctx = pDeviceContext.Get();
	if (!ctx) return;
	auto backBrush = GetBackColorBrush(fontBack);
	if (!backBrush) return;
	layout->SetDrawingEffect(NULL, DWRITE_TEXT_RANGE{ 0, UINT_MAX });
	layout->SetDrawingEffect(backBrush, subRange);
	ctx->DrawTextLayout(D2D1::Point2F(x, y), layout, brush);
	wicDirty = true;
}

void D2DGraphics1::DrawString(const std::wstring& str, float x, float y, D2D1_COLOR_F color, Font* font) {
	D2D1_RECT_F rect = D2D1::RectF(x, y, FLT_MAX, FLT_MAX);
	Font* resolvedFont = font ? font : DefaultFontObject1();
	if (!resolvedFont) return;
	auto* ctx = pDeviceContext.Get();
	if (!ctx) return;
	auto brush = GetColorBrush(color);
	if (!brush) return;
	ctx->DrawText(str.c_str(), static_cast<UINT32>(str.size()), resolvedFont->FontObject, &rect, brush);
	wicDirty = true;
}
void D2DGraphics1::DrawString(const std::wstring& str, float x, float y, ID2D1Brush* brush, Font* font) {
	D2D1_RECT_F rect = D2D1::RectF(x, y, FLT_MAX, FLT_MAX);
	Font* resolvedFont = font ? font : DefaultFontObject1();
	if (!resolvedFont) return;
	auto* ctx = pDeviceContext.Get();
	if (!ctx || !brush) return;
	ctx->DrawText(str.c_str(), static_cast<UINT32>(str.size()), resolvedFont->FontObject, &rect, brush);
	wicDirty = true;
}
void D2DGraphics1::DrawString(const std::wstring& str, float x, float y, float w, float h, D2D1_COLOR_F color, Font* font) {
	IDWriteTextLayout* textLayout = CreateStringLayout(str, w, h, font ? font : DefaultFontObject1());
	if (!textLayout) return;
	auto* ctx = pDeviceContext.Get();
	if (!ctx) { textLayout->Release(); return; }
	auto brush = GetColorBrush(color);
	if (!brush) { textLayout->Release(); return; }
	ctx->DrawTextLayout({ x,y }, textLayout, brush);
	textLayout->Release();
	wicDirty = true;
}
void D2DGraphics1::DrawString(const std::wstring& str, float x, float y, float w, float h, ID2D1Brush* brush, Font* font) {
	IDWriteTextLayout* textLayout = CreateStringLayout(str, w, h, font ? font : DefaultFontObject1());
	if (!textLayout) return;
	auto* ctx = pDeviceContext.Get();
	if (!ctx || !brush) { textLayout->Release(); return; }
	ctx->DrawTextLayout({ x,y }, textLayout, brush);
	textLayout->Release();
	wicDirty = true;
}
void D2DGraphics1::DrawStringCentered(const std::wstring& str, float centerX, float centerY, D2D1_COLOR_F color, Font* font) {
	Font* resolvedFont = font ? font : DefaultFontObject1();
	if (!resolvedFont) return;
	IDWriteTextLayout* textLayout = CreateStringLayout(str, FLT_MAX, FLT_MAX, resolvedFont);
	if (!textLayout) return;
	auto* ctx = pDeviceContext.Get();
	if (!ctx) { textLayout->Release(); return; }
	auto brush = GetColorBrush(color);
	if (!brush) { textLayout->Release(); return; }
	D2D1_SIZE_F textSize = GetTextLayoutSize(textLayout);
	float x = centerX - textSize.width * 0.5f;
	float y = centerY - textSize.height * 0.5f;
	ctx->DrawTextLayout({ x, y }, textLayout, brush);
	textLayout->Release();
	wicDirty = true;
}
void D2DGraphics1::DrawStringCentered(const std::wstring& str, float centerX, float centerY, ID2D1Brush* brush, Font* font) {
	Font* resolvedFont = font ? font : DefaultFontObject1();
	if (!resolvedFont) return;
	IDWriteTextLayout* textLayout = CreateStringLayout(str, FLT_MAX, FLT_MAX, resolvedFont);
	if (!textLayout) return;
	auto* ctx = pDeviceContext.Get();
	if (!ctx || !brush) { textLayout->Release(); return; }
	D2D1_SIZE_F textSize = GetTextLayoutSize(textLayout);
	float x = centerX - textSize.width * 0.5f;
	float y = centerY - textSize.height * 0.5f;
	ctx->DrawTextLayout({ x, y }, textLayout, brush);
	textLayout->Release();
	wicDirty = true;
}
void D2DGraphics1::DrawStringOutlined(const std::wstring& str, float x, float y, D2D1_COLOR_F textColor, D2D1_COLOR_F outlineColor, Font* font) {
	Font* resolvedFont = font ? font : DefaultFontObject1();
	if (!resolvedFont) return;
	IDWriteTextLayout* textLayout = CreateStringLayout(str, FLT_MAX, FLT_MAX, resolvedFont);
	if (!textLayout) return;
	auto* ctx = pDeviceContext.Get();
	if (!ctx) { textLayout->Release(); return; }
	auto textBrush = GetColorBrush(textColor);
	auto outlineBrush = GetBackColorBrush(outlineColor);
	if (!textBrush || !outlineBrush) { textLayout->Release(); return; }
	DrawTextOutline(ctx, textLayout, x, y, outlineBrush);
	ctx->DrawTextLayout({ x, y }, textLayout, textBrush);
	textLayout->Release();
	wicDirty = true;
}
void D2DGraphics1::DrawStringOutlined(const std::wstring& str, float x, float y, ID2D1Brush* textBrush, D2D1_COLOR_F outlineColor, Font* font) {
	Font* resolvedFont = font ? font : DefaultFontObject1();
	if (!resolvedFont) return;
	IDWriteTextLayout* textLayout = CreateStringLayout(str, FLT_MAX, FLT_MAX, resolvedFont);
	if (!textLayout) return;
	auto* ctx = pDeviceContext.Get();
	if (!ctx || !textBrush) { textLayout->Release(); return; }
	auto outlineBrush = GetBackColorBrush(outlineColor);
	if (!outlineBrush) { textLayout->Release(); return; }
	DrawTextOutline(ctx, textLayout, x, y, outlineBrush);
	ctx->DrawTextLayout({ x, y }, textLayout, textBrush);
	textLayout->Release();
	wicDirty = true;
}
void D2DGraphics1::DrawStringCenteredOutlined(const std::wstring& str, float centerX, float centerY, D2D1_COLOR_F textColor, D2D1_COLOR_F outlineColor, Font* font) {
	Font* resolvedFont = font ? font : DefaultFontObject1();
	if (!resolvedFont) return;
	IDWriteTextLayout* textLayout = CreateStringLayout(str, FLT_MAX, FLT_MAX, resolvedFont);
	if (!textLayout) return;
	auto* ctx = pDeviceContext.Get();
	if (!ctx) { textLayout->Release(); return; }
	auto textBrush = GetColorBrush(textColor);
	auto outlineBrush = GetBackColorBrush(outlineColor);
	if (!textBrush || !outlineBrush) { textLayout->Release(); return; }
	D2D1_SIZE_F textSize = GetTextLayoutSize(textLayout);
	float x = centerX - textSize.width * 0.5f;
	float y = centerY - textSize.height * 0.5f;
	DrawTextOutline(ctx, textLayout, x, y, outlineBrush);
	ctx->DrawTextLayout({ x, y }, textLayout, textBrush);
	textLayout->Release();
	wicDirty = true;
}
void D2DGraphics1::DrawStringCenteredOutlined(const std::wstring& str, float centerX, float centerY, ID2D1Brush* textBrush, D2D1_COLOR_F outlineColor, Font* font) {
	Font* resolvedFont = font ? font : DefaultFontObject1();
	if (!resolvedFont) return;
	IDWriteTextLayout* textLayout = CreateStringLayout(str, FLT_MAX, FLT_MAX, resolvedFont);
	if (!textLayout) return;
	auto* ctx = pDeviceContext.Get();
	if (!ctx || !textBrush) { textLayout->Release(); return; }
	auto outlineBrush = GetBackColorBrush(outlineColor);
	if (!outlineBrush) { textLayout->Release(); return; }
	D2D1_SIZE_F textSize = GetTextLayoutSize(textLayout);
	float x = centerX - textSize.width * 0.5f;
	float y = centerY - textSize.height * 0.5f;
	DrawTextOutline(ctx, textLayout, x, y, outlineBrush);
	ctx->DrawTextLayout({ x, y }, textLayout, textBrush);
	textLayout->Release();
	wicDirty = true;
}

void D2DGraphics1::FillTriangle(D2D1_TRIANGLE triangle, D2D1_COLOR_F color) {
	auto* ctx = pDeviceContext.Get();
	if (!ctx) {
		return;
	}
	auto brush = GetColorBrush(color);
	if (!brush) {
		return;
	}
	ComPtr<ID2D1PathGeometry> geo;
	geo.Attach(Factory::CreateGeomtry());
	if (!geo) {
		return;
	}
	ComPtr<ID2D1GeometrySink> sink;
	if (FAILED(geo->Open(&sink))) {
		return;
	}

	sink->BeginFigure(triangle.point1, D2D1_FIGURE_BEGIN_FILLED);
	sink->AddLine(triangle.point2);
	sink->AddLine(triangle.point3);
	sink->EndFigure(D2D1_FIGURE_END_CLOSED);
	sink->Close();
	ctx->FillGeometry(geo.Get(), brush);
	wicDirty = true;
}
void D2DGraphics1::DrawTriangle(D2D1_TRIANGLE triangle, D2D1_COLOR_F color, float width) {
	auto* ctx = pDeviceContext.Get();
	if (!ctx) return;
	auto brush = GetColorBrush(color);
	if (!brush) return;
	ctx->DrawLine(triangle.point1, triangle.point2, brush, width);
	ctx->DrawLine(triangle.point2, triangle.point3, brush, width);
	ctx->DrawLine(triangle.point3, triangle.point1, brush, width);
	wicDirty = true;
}

void D2DGraphics1::FillPolygon(std::vector<D2D1_POINT_2F> points, D2D1_COLOR_F color) {
	if (points.size() <= 2) return;
	auto* ctx = pDeviceContext.Get();
	if (!ctx) return;
	auto brush = GetColorBrush(color);
	if (!brush) return;
	ComPtr<ID2D1PathGeometry> geo;
	geo.Attach(Factory::CreateGeomtry());
	if (!geo) return;
	ComPtr<ID2D1GeometrySink> sink;
	if (FAILED(geo->Open(&sink))) return;
	sink->BeginFigure(points.front(), D2D1_FIGURE_BEGIN_FILLED);
	sink->AddLines(points.data() + 1, static_cast<UINT32>(points.size() - 1));
	sink->EndFigure(D2D1_FIGURE_END_CLOSED);
	sink->Close();
	ctx->FillGeometry(geo.Get(), brush);
	wicDirty = true;
}
void D2DGraphics1::FillPolygon(std::initializer_list<D2D1_POINT_2F> points, D2D1_COLOR_F color) {
	if (points.size() <= 2) return;
	auto* ctx = pDeviceContext.Get();
	if (!ctx) return;
	auto brush = GetColorBrush(color);
	if (!brush) return;
	ComPtr<ID2D1PathGeometry> geo;
	geo.Attach(Factory::CreateGeomtry());
	if (!geo) return;
	ComPtr<ID2D1GeometrySink> sink;
	if (FAILED(geo->Open(&sink))) return;
	sink->BeginFigure(*points.begin(), D2D1_FIGURE_BEGIN_FILLED);
	sink->AddLines(points.begin() + 1, static_cast<UINT32>(points.size() - 1));
	sink->EndFigure(D2D1_FIGURE_END_CLOSED);
	sink->Close();
	ctx->FillGeometry(geo.Get(), brush);
	wicDirty = true;
}
void D2DGraphics1::DrawPolygon(std::initializer_list<D2D1_POINT_2F> points, D2D1_COLOR_F color, float width) {
	if (points.size() <= 1) return;
	auto* ctx = pDeviceContext.Get();
	if (!ctx) return;
	auto brush = GetColorBrush(color);
	if (!brush) return;
	ComPtr<ID2D1PathGeometry> geo;
	geo.Attach(Factory::CreateGeomtry());
	if (!geo) return;
	ComPtr<ID2D1GeometrySink> sink;
	if (FAILED(geo->Open(&sink))) return;
	sink->BeginFigure(*points.begin(), D2D1_FIGURE_BEGIN_HOLLOW);
	sink->AddLines(points.begin() + 1, static_cast<UINT32>(points.size() - 1));
	sink->EndFigure(D2D1_FIGURE_END_OPEN);
	sink->Close();
	ctx->DrawGeometry(geo.Get(), brush, width);
	wicDirty = true;
}
void D2DGraphics1::DrawPolygon(std::vector<D2D1_POINT_2F> points, D2D1_COLOR_F color, float width) {
	if (points.size() <= 1) return;
	auto* ctx = pDeviceContext.Get();
	if (!ctx) return;
	auto brush = GetColorBrush(color);
	if (!brush) return;
	ComPtr<ID2D1PathGeometry> geo;
	geo.Attach(Factory::CreateGeomtry());
	if (!geo) return;
	ComPtr<ID2D1GeometrySink> sink;
	if (FAILED(geo->Open(&sink))) return;
	sink->BeginFigure(points.front(), D2D1_FIGURE_BEGIN_HOLLOW);
	sink->AddLines(points.data() + 1, static_cast<UINT32>(points.size() - 1));
	sink->EndFigure(D2D1_FIGURE_END_OPEN);
	sink->Close();
	ctx->DrawGeometry(geo.Get(), brush, width);
	wicDirty = true;
}

void D2DGraphics1::DrawArc(D2D1_POINT_2F center, float size, float sa, float ea, D2D1_COLOR_F color, float width) {
	const auto angleToPoint = [](D2D1_POINT_2F cent, float angle, float len) {
		return len > 0 ? D2D1::Point2F(
			cent.x + sinf(angle * DEG_TO_RAD) * len,
			cent.y - cosf(angle * DEG_TO_RAD) * len) : cent;
		};
	auto* ctx = pDeviceContext.Get();
	if (!ctx) return;
	auto brush = GetColorBrush(color);
	if (!brush) return;
	ComPtr<ID2D1PathGeometry> geo;
	geo.Attach(Factory::CreateGeomtry());
	if (!geo) return;
	float ts = sa, te = ea;
	if (te < ts) te += 360.0f;
	D2D1_ARC_SIZE sweep = (te - ts < 180.0f) ? D2D1_ARC_SIZE_SMALL : D2D1_ARC_SIZE_LARGE;
	ComPtr<ID2D1GeometrySink> sink;
	if (FAILED(geo->Open(&sink))) return;
	auto start = angleToPoint(center, sa, size);
	auto end = angleToPoint(center, ea, size);
	sink->BeginFigure(start, D2D1_FIGURE_BEGIN_HOLLOW);
	sink->AddArc(D2D1::ArcSegment(end, D2D1::SizeF(size, size), 0.0f, D2D1_SWEEP_DIRECTION_CLOCKWISE, sweep));
	sink->EndFigure(D2D1_FIGURE_END_OPEN);
	sink->Close();
	ctx->DrawGeometry(geo.Get(), brush, width);
	wicDirty = true;
}
void D2DGraphics1::DrawArcCounter(D2D1_POINT_2F center, float size, float sa, float ea, D2D1_COLOR_F color, float width) {
	const auto angleToPoint = [](D2D1_POINT_2F cent, float angle, float len) {
		return len > 0 ? D2D1::Point2F(
			cent.x + sinf(angle * DEG_TO_RAD) * len,
			cent.y - cosf(angle * DEG_TO_RAD) * len) : cent;
		};
	auto* ctx = pDeviceContext.Get();
	if (!ctx) return;
	auto brush = GetColorBrush(color);
	if (!brush) return;
	ComPtr<ID2D1PathGeometry> geo;
	geo.Attach(Factory::CreateGeomtry());
	if (!geo) return;
	float ts = sa, te = ea;
	if (te < ts) te += 360.0f;
	D2D1_ARC_SIZE sweep = (te - ts < 180.0f) ? D2D1_ARC_SIZE_SMALL : D2D1_ARC_SIZE_LARGE;
	ComPtr<ID2D1GeometrySink> sink;
	if (FAILED(geo->Open(&sink))) return;
	auto start = angleToPoint(center, sa, size);
	auto end = angleToPoint(center, ea, size);
	sink->BeginFigure(start, D2D1_FIGURE_BEGIN_HOLLOW);
	sink->AddArc(D2D1::ArcSegment(end, D2D1::SizeF(size, size), 0.0f, D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE, sweep));
	sink->EndFigure(D2D1_FIGURE_END_OPEN);
	sink->Close();
	ctx->DrawGeometry(geo.Get(), brush, width);
	wicDirty = true;
}

void D2DGraphics1::PushDrawRect(float left, float top, float width, float height) {
	auto* ctx = pDeviceContext.Get();
	if (!ctx) return;
	ctx->PushAxisAlignedClip(D2D1::RectF(left, top, left + width, top + height), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
}
void D2DGraphics1::PopDrawRect() {
	auto* ctx = pDeviceContext.Get();
	if (!ctx) return;
	ctx->PopAxisAlignedClip();
}
void D2DGraphics1::SetAntialiasMode(D2D1_ANTIALIAS_MODE antialiasMode) {
	auto* ctx = pDeviceContext.Get();
	if (!ctx) return;
	ctx->SetAntialiasMode(antialiasMode);
}
void D2DGraphics1::SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE antialiasMode) {
	auto* ctx = pDeviceContext.Get();
	if (!ctx) return;
	ctx->SetTextAntialiasMode(antialiasMode);
}

ID2D1RenderTarget* D2DGraphics1::GetRenderTargetRaw() const {
	return pDeviceContext.Get();
}
ComPtr<ID2D1RenderTarget> D2DGraphics1::GetRenderTarget() const {
	ComPtr<ID2D1RenderTarget> t;
	if (pDeviceContext) {
		pDeviceContext.As(&t);
	}
	return t;
}
ID2D1DeviceContext* D2DGraphics1::GetDeviceContextRaw() const {
	return pDeviceContext.Get();
}
ComPtr<ID2D1DeviceContext> D2DGraphics1::GetDeviceContext() const {
	return pDeviceContext;
}
IWICBitmap* D2DGraphics1::GetTargetWicBitmap() const {
	return pWicTargetBitmap.Get();
}

ID2D1Bitmap* D2DGraphics1::CreateBitmap(IWICBitmap* wb) {
	auto* ctx = pDeviceContext.Get();
	if (!ctx || !wb) return nullptr;
	ComPtr<ID2D1Bitmap> bitmap;
	if (FAILED(ctx->CreateBitmapFromWicBitmap(wb, &bitmap))) {
		return nullptr;
	}
	return bitmap.Detach();
}
ID2D1Bitmap* D2DGraphics1::CreateBitmap(IWICFormatConverter* conv, D2D1_BITMAP_PROPERTIES* props) {
	auto* ctx = pDeviceContext.Get();
	if (!ctx || !conv) return nullptr;
	ComPtr<ID2D1Bitmap> bitmap;
	if (FAILED(ctx->CreateBitmapFromWicBitmap(conv, props, &bitmap))) {
		return nullptr;
	}
	return bitmap.Detach();
}
ID2D1Bitmap* D2DGraphics1::CreateBitmap(const std::shared_ptr<BitmapSource>& bitmapSource) {
	auto* ctx = pDeviceContext.Get();
	if (!ctx || !bitmapSource) return nullptr;
	ComPtr<ID2D1Bitmap> bitmap;
	if (FAILED(ctx->CreateBitmapFromWicBitmap(bitmapSource->GetWicBitmap(), nullptr, &bitmap))) {
		return nullptr;
	}
	return bitmap.Detach();
}

ID2D1Bitmap1* D2DGraphics1::CreateBitmapFromDxgiSurface(IDXGISurface* surface) {
	auto* ctx = pDeviceContext.Get();
	if (!ctx || !surface) return nullptr;

	D2D1_BITMAP_PROPERTIES1 props = D2D1::BitmapProperties1(
		D2D1_BITMAP_OPTIONS_NONE,
		D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
		96.0f,
		96.0f);

	ComPtr<ID2D1Bitmap1> bitmap;
	HRESULT hr = ctx->CreateBitmapFromDxgiSurface(surface, &props, &bitmap);
	if (FAILED(hr)) {
		return nullptr;
	}
	return bitmap.Detach();
}

void D2DGraphics1::DrawDxgiSurface(IDXGISurface* surface, float x, float y, float width, float height, float opacity) {
	auto* ctx = pDeviceContext.Get();
	if (!ctx || !surface) return;

	ComPtr<ID2D1Bitmap1> bitmap;
	bitmap.Attach(CreateBitmapFromDxgiSurface(surface));
	if (!bitmap) return;

	D2D1_RECT_F destRect = D2D1::RectF(x, y, x + width, y + height);
	ctx->DrawBitmap(bitmap.Get(), destRect, opacity, D2D1_INTERPOLATION_MODE_LINEAR);
	wicDirty = true;
}

ID2D1LinearGradientBrush* D2DGraphics1::CreateLinearGradientBrush(D2D1_GRADIENT_STOP* stops, unsigned int stopcount) {
	auto* ctx = pDeviceContext.Get();
	if (!ctx || !stops || stopcount == 0) return nullptr;
	ComPtr<ID2D1GradientStopCollection> collection;
	if (FAILED(ctx->CreateGradientStopCollection(stops, stopcount, &collection))) return nullptr;
	ComPtr<ID2D1LinearGradientBrush> brush;
	if (FAILED(ctx->CreateLinearGradientBrush({}, collection.Get(), &brush))) return nullptr;
	return brush.Detach();
}
ID2D1RadialGradientBrush* D2DGraphics1::CreateRadialGradientBrush(D2D1_GRADIENT_STOP* stops, unsigned int stopcount, D2D1_POINT_2F center) {
	auto* ctx = pDeviceContext.Get();
	if (!ctx || !stops || stopcount == 0) return nullptr;
	ComPtr<ID2D1GradientStopCollection> collection;
	if (FAILED(ctx->CreateGradientStopCollection(stops, stopcount, &collection))) return nullptr;
	D2D1_RADIAL_GRADIENT_BRUSH_PROPERTIES props{};
	props.center = center;
	ComPtr<ID2D1RadialGradientBrush> brush;
	if (FAILED(ctx->CreateRadialGradientBrush(props, collection.Get(), &brush))) return nullptr;
	return brush.Detach();
}
ID2D1BitmapBrush* D2DGraphics1::CreateBitmapBrush(ID2D1Bitmap* bmp) {
	auto* ctx = pDeviceContext.Get();
	if (!ctx || !bmp) return nullptr;
	ComPtr<ID2D1BitmapBrush> brush;
	if (FAILED(ctx->CreateBitmapBrush(bmp, nullptr, nullptr, &brush))) return nullptr;
	return brush.Detach();
}
ID2D1SolidColorBrush* D2DGraphics1::CreateSolidColorBrush(D2D1_COLOR_F color) {
	auto* ctx = pDeviceContext.Get();
	if (!ctx) return nullptr;
	ComPtr<ID2D1SolidColorBrush> result;
	if (FAILED(ctx->CreateSolidColorBrush(color, &result))) return nullptr;
	return result.Detach();
}

D2D1_SIZE_F D2DGraphics1::Size() {
	if (!pDeviceContext) return D2D1::SizeF(0.0f, 0.0f);
	if (pTargetBitmap) {
		auto sz = pTargetBitmap->GetSize();
		return sz;
	}
	return pDeviceContext->GetSize();
}

void D2DGraphics1::SetTransform(D2D1_MATRIX_3X2_F matrix) {
	if (pDeviceContext) pDeviceContext->SetTransform(matrix);
}
void D2DGraphics1::ClearTransform() {
	if (pDeviceContext) pDeviceContext->SetTransform(D2D1::Matrix3x2F::Identity());
}

D2D1_SIZE_F D2DGraphics1::GetTextLayoutSize(IDWriteTextLayout* textLayout) {
	D2D1_SIZE_F minSize = { 0,0 };
	if (!textLayout) return minSize;
	DWRITE_TEXT_METRICS metrics;
	HRESULT hr = textLayout->GetMetrics(&metrics);
	if (SUCCEEDED(hr)) {
		minSize = D2D1::Size((float)ceil(metrics.widthIncludingTrailingWhitespace), (float)ceil(metrics.height));
		return minSize;
	}
	return minSize;
}

CompatibleGraphics1::CompatibleGraphics1(D2DGraphics1* parent, D2D1_SIZE_F desiredSize) {
	Initialize(parent, desiredSize);
}

HRESULT CompatibleGraphics1::Initialize(D2DGraphics1* parent, D2D1_SIZE_F desiredSize) {
	if (!parent) return E_INVALIDARG;
	parent->EnsureDeviceContext();
	auto* pdc = parent->GetDeviceContextRaw();
	if (pdc) {
		pdc->GetDevice(&parentDevice);
	}
	if (!parentDevice) return E_FAIL;
	this->desiredSize = desiredSize;
	return RecreateTarget();
}

HRESULT CompatibleGraphics1::RecreateTarget() {
	ResetTarget();
	pD2DDevice = parentDevice;
	if (!pD2DDevice) return E_FAIL;
	HRESULT hr = pD2DDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &pDeviceContext);
	if (FAILED(hr)) return hr;

	UINT w = std::max<UINT>(1, static_cast<UINT>(desiredSize.width));
	UINT h = std::max<UINT>(1, static_cast<UINT>(desiredSize.height));

	hr = CreateTargetBitmapForSize(w, h, 96.0f, 96.0f, DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED);
	if (FAILED(hr)) return hr;

	surfaceKind = SurfaceKind::Compatible;
	wicDirty = false;
	return ConfigDefaultObjects();
}

void CompatibleGraphics1::ReSize(UINT width, UINT height) {
	width = std::max<UINT>(1, width);
	height = std::max<UINT>(1, height);
	desiredSize = D2D1::SizeF((FLOAT)width, (FLOAT)height);
	RecreateTarget();
}

ID2D1Bitmap* CompatibleGraphics1::GetBitmap() const {
	return pTargetBitmap.Get();
}

HRESULT HwndGraphics1::InitDevice() {
	if (!hwnd) return E_INVALIDARG;

	HRESULT hr = EnsureDeviceResources();
	if (FAILED(hr)) return hr;

	RECT rc{};
	GetClientRect(hwnd, &rc);
	UINT width = std::max<UINT>(1, static_cast<UINT>(rc.right - rc.left));
	UINT height = std::max<UINT>(1, static_cast<UINT>(rc.bottom - rc.top));

	// CreateSwapChainForHwnd 需要 IDXGIFactory2
	ComPtr<IDXGIDevice> dxgiDevice = Shared().dxgiDevice;
	if (!dxgiDevice) return E_FAIL;
	ComPtr<IDXGIAdapter> adapter;
	hr = dxgiDevice->GetAdapter(&adapter);
	if (FAILED(hr)) return hr;
	ComPtr<IDXGIFactory2> factory;
	hr = adapter->GetParent(IID_PPV_ARGS(&factory));
	if (FAILED(hr)) return hr;

	DXGI_SWAP_CHAIN_DESC1 desc{};
	desc.Width = width;
	desc.Height = height;
	desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	desc.BufferCount = 2;
	desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
	desc.Scaling = DXGI_SCALING_STRETCH;
	desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

	ComPtr<IDXGISwapChain1> swapChain1;
	hr = factory->CreateSwapChainForHwnd(Shared().d3dDevice.Get(), hwnd, &desc, nullptr, nullptr, &swapChain1);
	if (FAILED(hr)) return hr;

	factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

	pSwapChain = swapChain1;
	hr = CreateTargetBitmapForSwapChain(pSwapChain.Get());
	if (FAILED(hr)) return hr;

	surfaceKind = SurfaceKind::Hwnd;
	wicDirty = false;
	return ConfigDefaultObjects();
}

HwndGraphics1::HwndGraphics1(HWND hWnd) {
	hwnd = hWnd;
	InitDevice();
}

void HwndGraphics1::ReSize(UINT width, UINT height) {
	if (!pSwapChain || !pDeviceContext) return;
	width = std::max<UINT>(1, width);
	height = std::max<UINT>(1, height);

	pTargetBitmap.Reset();
	pDeviceContext->SetTarget(nullptr);
	pDeviceContext->Flush();

	HRESULT hr = pSwapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
	if (FAILED(hr) && IsDeviceRemovedHr(hr)) {
		_deviceLost = true;
		ResetTarget();
		if (SUCCEEDED(InitDevice())) {
			_deviceLost = false;
			::InvalidateRect(hwnd, nullptr, FALSE);
		}
		return;
	}
	CreateTargetBitmapForSwapChain(pSwapChain.Get());
	ConfigDefaultObjects();
}

void HwndGraphics1::BeginRender() {
	D2DGraphics1::BeginRender();
}

void HwndGraphics1::EndRender() {
	// Hwnd：既需要 EndDraw，也需要 Present
	if (!pDeviceContext) return;
	_lastEndDrawHr = pDeviceContext->EndDraw();
	_lastPresentHr = S_OK;
	if (pSwapChain) {
		_lastPresentHr = pSwapChain->Present(1, 0);
	}
	if (_lastEndDrawHr == D2DERR_RECREATE_TARGET && pSwapChain) {
		CreateTargetBitmapForSwapChain(pSwapChain.Get());
		ConfigDefaultObjects();
		_deviceLost = false;
		return;
	}
	if (IsDeviceRemovedHr(_lastEndDrawHr) || IsDeviceRemovedHr(_lastPresentHr)) {
		_deviceLost = true;
		ResetTarget();
		if (SUCCEEDED(InitDevice())) {
			_deviceLost = false;
			::InvalidateRect(hwnd, nullptr, FALSE);
		}
	}
}

// ---------------- CompositionSwapChainGraphics1 ----------------

CompositionSwapChainGraphics1::CompositionSwapChainGraphics1(IDXGISwapChain1* swapChain) {
	swapChain1 = swapChain;
	InitializeWithSwapChain(swapChain);
}

void CompositionSwapChainGraphics1::ReSize(UINT width, UINT height) {
	if (!pSwapChain || !pDeviceContext) return;
	width = std::max<UINT>(1, width);
	height = std::max<UINT>(1, height);

	pTargetBitmap.Reset();
	pDeviceContext->SetTarget(nullptr);
	pDeviceContext->Flush();

	HRESULT hr = pSwapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
	if (FAILED(hr) && IsDeviceRemovedHr(hr)) {
		// Composition swapchain 的生命周期由 DCompLayeredHost 管理：这里只标记，交由上层重建。
		_deviceLost = true;
		return;
	}
	CreateTargetBitmapForSwapChain(pSwapChain.Get());
	ConfigDefaultObjects();
}

void CompositionSwapChainGraphics1::BeginRender() {
	D2DGraphics1::BeginRender();
}

void CompositionSwapChainGraphics1::EndRender() {
	if (!pDeviceContext) return;
	_lastEndDrawHr = pDeviceContext->EndDraw();
	_lastPresentHr = S_OK;
	if (pSwapChain) {
		_lastPresentHr = pSwapChain->Present(1, 0);
	}
	if (_lastEndDrawHr == D2DERR_RECREATE_TARGET && pSwapChain) {
		CreateTargetBitmapForSwapChain(pSwapChain.Get());
		ConfigDefaultObjects();
		_deviceLost = false;
		return;
	}
	if (IsDeviceRemovedHr(_lastEndDrawHr) || IsDeviceRemovedHr(_lastPresentHr)) {
		_deviceLost = true;
	}
}


