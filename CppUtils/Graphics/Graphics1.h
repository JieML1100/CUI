#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_
#endif

#include <d2d1.h>
#include <d2d1_1.h>
#include <dxgi.h>
#include <dxgi1_2.h>
#include <dxgiformat.h>
#include <memory>
#include <optional>
#include <vector>
#include <wrl/client.h>

#include "Font.h"
#include "Colors.h"
#include "Factory.h"
#include "BitmapSource.h"

#ifndef _LIB
#if defined(_MT)
// MT (静态运行时库)
#if !defined(_DLL)
#if defined(_M_X64) && defined(_DEBUG)
#pragma comment(lib, "Graphics_x64_MTd.lib")
#elif defined(_M_X64) && !defined(_DEBUG)
#pragma comment(lib, "Graphics_x64_MT.lib")
#elif defined(_M_IX86) && defined(_DEBUG)
#pragma comment(lib, "Graphics_x86_MTd.lib")
#elif defined(_M_IX86) && !defined(_DEBUG)
#pragma comment(lib, "Graphics_x86_MT.lib")
#else
#error "Unsupported architecture or configuration"
#endif
// MD (动态运行时库)
#else
#if defined(_M_X64) && defined(_DEBUG)
#pragma comment(lib, "Graphics_x64_MDd.lib")
#elif defined(_M_X64) && !defined(_DEBUG)
#pragma comment(lib, "Graphics_x64_MD.lib")
#elif defined(_M_IX86) && defined(_DEBUG)
#pragma comment(lib, "Graphics_x86_MDd.lib")
#elif defined(_M_IX86) && !defined(_DEBUG)
#pragma comment(lib, "Graphics_x86_MD.lib")
#else
#error "Unsupported architecture or configuration"
#endif
#endif
#else
#pragma message("Graphics: automatic runtime library linking skipped because _MT is not defined.")
#endif
#endif

EXTERN_C Font* DefaultFontObject;

class D2DGraphics1 {
public:
	enum class SurfaceKind {
		None,
		Offscreen,
		Compatible,
		Hwnd,
		ExternalBitmap,
		DxgiSwapChain
	};

	struct InitOptions {
		SurfaceKind kind = SurfaceKind::None;
		UINT width = 0;
		UINT height = 0;
		IWICBitmap* wicBitmap = nullptr;
		bool takeOwnership = false;
		FLOAT dpiX = 96.0f;
		FLOAT dpiY = 96.0f;
		DXGI_FORMAT format = DXGI_FORMAT_B8G8R8A8_UNORM;
		D2D1_ALPHA_MODE alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
	};

	D2DGraphics1();
	explicit D2DGraphics1(IWICBitmap* bitmap, bool takeOwnership = false);
	explicit D2DGraphics1(const BitmapSource* bitmap);
	explicit D2DGraphics1(IDXGISwapChain* swapChain);
	explicit D2DGraphics1(const InitOptions& options);
	virtual ~D2DGraphics1();

	virtual void BeginRender();
	virtual void EndRender();
	virtual void ReSize(UINT width, UINT height);

	HRESULT EnsureDeviceContext();

	void Clear(D2D1_COLOR_F color);

	ID2D1SolidColorBrush* GetColorBrush(D2D1_COLOR_F newcolor);
	ID2D1SolidColorBrush* GetColorBrush(COLORREF newcolor);
	ID2D1SolidColorBrush* GetColorBrush(int r, int g, int b);
	ID2D1SolidColorBrush* GetColorBrush(float r, float g, float b, float a = 1.0f);

	ID2D1SolidColorBrush* GetBackColorBrush(D2D1_COLOR_F newcolor);
	ID2D1SolidColorBrush* GetBackColorBrush(COLORREF newcolor);
	ID2D1SolidColorBrush* GetBackColorBrush(int r, int g, int b);
	ID2D1SolidColorBrush* GetBackColorBrush(float r, float g, float b, float a = 1.0f);

	void DrawLine(D2D1_POINT_2F p1, D2D1_POINT_2F p2, D2D1_COLOR_F color, float linewidth = 1.0f);
	void DrawLine(D2D1_POINT_2F p1, D2D1_POINT_2F p2, ID2D1Brush* brush, float linewidth = 1.0f);

	void DrawLine(float p1_x, float p1_y, float p2_x, float p2_y, D2D1_COLOR_F color, float linewidth = 1.0f);
	void DrawLine(float p1_x, float p1_y, float p2_x, float p2_y, ID2D1Brush* brush, float linewidth = 1.0f);

	void DrawRect(D2D1_RECT_F rect, D2D1_COLOR_F color, float linewidth = 1.0f);
	void DrawRect(float left, float top, float width, float height, D2D1_COLOR_F color, float linewidth = 1.0f);

	void DrawRoundRect(D2D1_RECT_F rect, D2D1_COLOR_F color, float linewidth = 1.0f, float r = 0.5f);
	void DrawRoundRect(float left, float top, float width, float height, D2D1_COLOR_F color, float linewidth = 1.0f, float r = 0.5f);

	void FillRect(D2D1_RECT_F rect, D2D1_COLOR_F color);
	void FillRect(D2D1_RECT_F rect, ID2D1Brush* brush);

	void FillRect(float left, float top, float width, float height, D2D1_COLOR_F color);
	void FillRect(float left, float top, float width, float height, ID2D1Brush* brush);

	void FillRoundRect(D2D1_RECT_F rect, D2D1_COLOR_F color, float r = 0.5f);
	void FillRoundRect(float left, float top, float width, float height, D2D1_COLOR_F color, float r = 0.5f);

	void DrawEllipse(D2D1_POINT_2F cent, float xr, float yr, D2D1_COLOR_F color, float linewidth = 1.0f);
	void DrawEllipse(float x, float y, float xr, float yr, D2D1_COLOR_F color, float linewidth = 1.0f);

	void FillEllipse(D2D1_POINT_2F cent, float xr, float yr, D2D1_COLOR_F color);
	void FillEllipse(float cx, float cy, float xr, float yr, D2D1_COLOR_F color);

	void DrawGeometry(ID2D1Geometry* geo, D2D1_COLOR_F color, float linewidth = 1.0f);
	void DrawGeometry(ID2D1Geometry* geo, ID2D1Brush* brush, float linewidth = 1.0f);

	void FillGeometry(ID2D1Geometry* geo, D2D1_COLOR_F color);
	void FillGeometry(ID2D1Geometry* geo, ID2D1Brush* brush);

	void FillPie(D2D1_POINT_2F center, float width, float height, float startAngle, float sweepAngle, D2D1_COLOR_F color);
	void FillPie(D2D1_POINT_2F center, float width, float height, float startAngle, float sweepAngle, ID2D1Brush* brush);

	void DrawBitmap(ID2D1Bitmap* bmp, float x = 0.f, float y = 0.f, float opacity = 1.0f);
	void DrawBitmap(ID2D1Bitmap* bmp, D2D1_RECT_F rect, float opacity = 1.0f);
	void DrawBitmap(ID2D1Bitmap* bmp, D2D1_RECT_F destRect, D2D1_RECT_F srcRect, float opacity = 1.0f);

	void DrawBitmap(ID2D1Bitmap* bmp, float x, float y, float w, float h, float opacity = 1.0f);
	void DrawBitmap(ID2D1Bitmap* bmp, float dest_x, float dest_y, float dest_w, float dest_h, float src_x, float src_y, float src_w, float src_h, float opacity = 1.0f);

	void FillOpacityMask(ID2D1Bitmap* mask, D2D1_POINT_2F destPoint, D2D1_COLOR_F color, D2D1_OPACITY_MASK_CONTENT content = D2D1_OPACITY_MASK_CONTENT_GRAPHICS);
	void FillOpacityMask(ID2D1Bitmap* mask, D2D1_RECT_F destRect, D2D1_COLOR_F color, D2D1_OPACITY_MASK_CONTENT content = D2D1_OPACITY_MASK_CONTENT_GRAPHICS);
	void FillOpacityMask(ID2D1Bitmap* mask, D2D1_RECT_F destRect, D2D1_RECT_F srcRect, D2D1_COLOR_F color, D2D1_OPACITY_MASK_CONTENT content = D2D1_OPACITY_MASK_CONTENT_GRAPHICS);

	void FillMesh(ID2D1Mesh* mesh, D2D1_COLOR_F color);

	IDWriteTextLayout* CreateStringLayout(const std::wstring& str, float width, float height, Font* font = nullptr);

	void DrawStringLayout(IDWriteTextLayout* layout, float x, float y, D2D1_COLOR_F color);
	void DrawStringLayout(IDWriteTextLayout* layout, float x, float y, ID2D1Brush* brush);

	void DrawStringLayoutCentered(IDWriteTextLayout* layout, float centerX, float centerY, D2D1_COLOR_F color);
	void DrawStringLayoutCentered(IDWriteTextLayout* layout, float centerX, float centerY, ID2D1Brush* brush);

	void DrawStringLayoutOutlined(IDWriteTextLayout* layout, float x, float y, D2D1_COLOR_F textColor, D2D1_COLOR_F outlineColor);
	void DrawStringLayoutOutlined(IDWriteTextLayout* layout, float x, float y, ID2D1Brush* textBrush, D2D1_COLOR_F outlineColor);

	void DrawStringLayoutCenteredOutlined(IDWriteTextLayout* layout, float centerX, float centerY, D2D1_COLOR_F textColor, D2D1_COLOR_F outlineColor);
	void DrawStringLayoutCenteredOutlined(IDWriteTextLayout* layout, float centerX, float centerY, ID2D1Brush* textBrush, D2D1_COLOR_F outlineColor);

	void DrawStringLayoutEffect(IDWriteTextLayout* layout, float x, float y, D2D1_COLOR_F color, DWRITE_TEXT_RANGE subRange, D2D1_COLOR_F fontBack, Font* font = nullptr);
	void DrawStringLayoutEffect(IDWriteTextLayout* layout, float x, float y, ID2D1Brush* brush, DWRITE_TEXT_RANGE subRange, D2D1_COLOR_F fontBack, Font* font = nullptr);

	void DrawString(const std::wstring& str, float x, float y, D2D1_COLOR_F color, Font* font = nullptr);
	void DrawString(const std::wstring& str, float x, float y, ID2D1Brush* brush, Font* font = nullptr);
	void DrawString(const std::wstring& str, float x, float y, float w, float h, D2D1_COLOR_F color, Font* font = nullptr);
	void DrawString(const std::wstring& str, float x, float y, float w, float h, ID2D1Brush* brush, Font* font = nullptr);

	void DrawStringCentered(const std::wstring& str, float centerX, float centerY, D2D1_COLOR_F color, Font* font = nullptr);
	void DrawStringCentered(const std::wstring& str, float centerX, float centerY, ID2D1Brush* brush, Font* font = nullptr);

	void DrawStringOutlined(const std::wstring& str, float x, float y, D2D1_COLOR_F textColor, D2D1_COLOR_F outlineColor, Font* font = nullptr);
	void DrawStringOutlined(const std::wstring& str, float x, float y, ID2D1Brush* textBrush, D2D1_COLOR_F outlineColor, Font* font = nullptr);

	void DrawStringCenteredOutlined(const std::wstring& str, float centerX, float centerY, D2D1_COLOR_F textColor, D2D1_COLOR_F outlineColor, Font* font = nullptr);
	void DrawStringCenteredOutlined(const std::wstring& str, float centerX, float centerY, ID2D1Brush* textBrush, D2D1_COLOR_F outlineColor, Font* font = nullptr);

	void DrawTriangle(D2D1_TRIANGLE triangle, D2D1_COLOR_F color, float width = 1.0f);
	void FillTriangle(D2D1_TRIANGLE triangle, D2D1_COLOR_F color);

	void FillPolygon(std::initializer_list<D2D1_POINT_2F> points, D2D1_COLOR_F color);
	void FillPolygon(std::vector<D2D1_POINT_2F> points, D2D1_COLOR_F color);

	void DrawPolygon(std::initializer_list<D2D1_POINT_2F> points, D2D1_COLOR_F color, float width = 1.0f);
	void DrawPolygon(std::vector<D2D1_POINT_2F> points, D2D1_COLOR_F color, float width = 1.0f);

	void DrawArc(D2D1_POINT_2F center, float size, float sa, float ea, D2D1_COLOR_F color, float width = 1.0f);
	void DrawArcCounter(D2D1_POINT_2F center, float size, float sa, float ea, D2D1_COLOR_F color, float width = 1.0f);

	void PushDrawRect(float left, float top, float width, float height);
	void PopDrawRect();
	void SetAntialiasMode(D2D1_ANTIALIAS_MODE antialiasMode);
	void SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE antialiasMode);

	ID2D1RenderTarget* GetRenderTargetRaw() const;
	Microsoft::WRL::ComPtr<ID2D1RenderTarget> GetRenderTarget() const;

	ID2D1DeviceContext* GetDeviceContextRaw() const;
	Microsoft::WRL::ComPtr<ID2D1DeviceContext> GetDeviceContext() const;

	ID2D1Bitmap1* CreateBitmapFromDxgiSurface(IDXGISurface* surface);
	void DrawDxgiSurface(IDXGISurface* surface, float x, float y, float width, float height, float opacity = 1.0f);

	IWICBitmap* GetTargetWicBitmap() const;

	ID2D1Bitmap* CreateBitmap(IWICBitmap* wb);
	ID2D1Bitmap* CreateBitmap(IWICFormatConverter* conv, D2D1_BITMAP_PROPERTIES* props = nullptr);
	ID2D1Bitmap* CreateBitmap(const std::shared_ptr<BitmapSource>& bitmapSource);

	ID2D1LinearGradientBrush* CreateLinearGradientBrush(D2D1_GRADIENT_STOP* stops, unsigned int stopcount);
	ID2D1RadialGradientBrush* CreateRadialGradientBrush(D2D1_GRADIENT_STOP* stops, unsigned int stopcount, D2D1_POINT_2F center);
	ID2D1BitmapBrush* CreateBitmapBrush(ID2D1Bitmap* bmp);
	ID2D1SolidColorBrush* CreateSolidColorBrush(D2D1_COLOR_F color);
	D2D1_SIZE_F Size();
	void SetTransform(D2D1_MATRIX_3X2_F matrix);
	void ClearTransform();

	static D2D1_SIZE_F GetTextLayoutSize(IDWriteTextLayout* textLayout);

protected:
	HRESULT Initialize(const InitOptions& options);
	HRESULT InitializeWithSize(UINT width, UINT height, FLOAT dpiX, FLOAT dpiY, DXGI_FORMAT format, D2D1_ALPHA_MODE alphaMode);
	HRESULT InitializeWithWicBitmap(IWICBitmap* bitmap, bool takeOwnership, FLOAT dpiX, FLOAT dpiY, DXGI_FORMAT format, D2D1_ALPHA_MODE alphaMode);
	HRESULT InitializeWithSwapChain(IDXGISwapChain* swapChain);

	void ResetTarget();
	HRESULT ConfigDefaultObjects();
	HRESULT EnsureDeviceResources();
	HRESULT CreateTargetBitmapForSize(UINT width, UINT height, FLOAT dpiX, FLOAT dpiY, DXGI_FORMAT format, D2D1_ALPHA_MODE alphaMode);
	HRESULT CreateTargetBitmapForSwapChain(IDXGISwapChain* swapChain);
	HRESULT SyncTargetToWicIfNeeded();

protected:
	Microsoft::WRL::ComPtr<ID2D1Device> pD2DDevice;
	Microsoft::WRL::ComPtr<ID2D1DeviceContext> pDeviceContext;
	Microsoft::WRL::ComPtr<ID2D1Bitmap1> pTargetBitmap;
	Microsoft::WRL::ComPtr<IDXGISwapChain> pSwapChain;

	Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> Default_Brush;
	Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> Default_Brush_Back;
	Microsoft::WRL::ComPtr<IWICBitmap> pWicTargetBitmap;

	SurfaceKind surfaceKind = SurfaceKind::None;
	bool wicDirty = false;
};

class CompatibleGraphics1 : public D2DGraphics1 {
public:
	CompatibleGraphics1(D2DGraphics1* parent, D2D1_SIZE_F desiredSize);

	void ReSize(UINT width, UINT height) override;
	ID2D1Bitmap* GetBitmap() const;

private:
	HRESULT Initialize(D2DGraphics1* parent, D2D1_SIZE_F desiredSize);
	HRESULT RecreateTarget();

private:
	Microsoft::WRL::ComPtr<ID2D1Device> parentDevice;
	D2D1_SIZE_F desiredSize;
};

class HwndGraphics1 : public D2DGraphics1 {
private:
	HWND hwnd = nullptr;
	HRESULT InitDevice();
public:
	HwndGraphics1(HWND hWnd);
	void ReSize(UINT width, UINT height) override;
	void BeginRender() override;
	void EndRender() override;
};

class CompositionSwapChainGraphics1 : public D2DGraphics1 {
public:
	explicit CompositionSwapChainGraphics1(IDXGISwapChain1* swapChain);
	void ReSize(UINT width, UINT height) override;
	void BeginRender() override;
	void EndRender() override;

private:
	Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain1;
};

struct ID3D11Device;
struct IDXGIDevice;
HRESULT Graphics1_EnsureSharedD3DDevice();
ID3D11Device* Graphics1_GetSharedD3DDevice();
IDXGIDevice* Graphics1_GetSharedDXGIDevice();


