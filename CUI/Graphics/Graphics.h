#pragma once
#include <d3d11.h>
#include <d2d1_1.h>
#include <dxgi1_2.h>
#include <wrl.h>

#include "Font.h"
#include "Colors.h"
#include "Factory.h"
#include "Utils/Convert.h"
#include "nanosvg.h"

#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"d3d11.lib")

class D2DGraphics
{
public:
	ID2D1DeviceContext* pD2DDeviceContext = NULL;
	ID2D1SolidColorBrush* Default_Brush = NULL;
	ID2D1SolidColorBrush* Default_Brush_Back = NULL;
	ID2D1Bitmap1* pD2DTargetBitmap = NULL;
	Font* DefaultFontObject = NULL;
	void ConfigDefaultObjects();
	ID2D1SolidColorBrush* GetColorBrush(D2D1_COLOR_F newcolor);
	ID2D1SolidColorBrush* GetColorBrush(COLORREF newcolor);
	ID2D1SolidColorBrush* GetColorBrush(int r, int g, int b);
	ID2D1SolidColorBrush* GetColorBrush(float r, float g, float b, float a = 1.0f);
	ID2D1SolidColorBrush* GetBackColorBrush(D2D1_COLOR_F newcolor);
	ID2D1SolidColorBrush* GetBackColorBrush(COLORREF newcolor);
	ID2D1SolidColorBrush* GetBackColorBrush(int r, int g, int b);
	ID2D1SolidColorBrush* GetBackColorBrush(float r, float g, float b, float a = 1.0f);
public:
	D2DGraphics();
	D2DGraphics(UINT width, UINT height); 
	D2DGraphics(ID2D1Bitmap1* bmp);
	~D2DGraphics();
	virtual void BeginRender();
	virtual void EndRender();
	virtual void ReSize(UINT width, UINT height);
	void Clear(D2D1_COLOR_F color);

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

	IDWriteTextLayout* CreateStringLayout(std::wstring str, float width, float height, Font* font = nullptr);

	void DrawStringLayout(IDWriteTextLayout* layout, float x, float y, D2D1_COLOR_F color);
	void DrawStringLayout(IDWriteTextLayout* layout, float x, float y, ID2D1Brush* brush);

	void DrawStringLayoutEffect(IDWriteTextLayout* layout, float x, float y, D2D1_COLOR_F color, DWRITE_TEXT_RANGE subRange, D2D1_COLOR_F fontBack, Font* font = NULL);
	void DrawStringLayoutEffect(IDWriteTextLayout* layout, float x, float y, ID2D1Brush* brush, DWRITE_TEXT_RANGE subRange, D2D1_COLOR_F fontBack, Font* font = NULL);

	void DrawString(std::wstring str, float x, float y, D2D1_COLOR_F color, Font* font = nullptr);
	void DrawString(std::wstring str, float x, float y, ID2D1Brush* brush, Font* font = nullptr);
	void DrawString(std::wstring str, float x, float y, float w, float h, D2D1_COLOR_F color, Font* font = nullptr);
	void DrawString(std::wstring str, float x, float y, float w, float h, ID2D1Brush* brush, Font* font = nullptr);

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

	ID2D1Bitmap* CreateBitmap(IWICBitmap* wb);
	ID2D1Bitmap* CreateBitmap(std::wstring path);
	ID2D1Bitmap* CreateBitmap(void* data, int size);
	ID2D1Bitmap* CreateBitmap(HBITMAP hb);
	ID2D1Bitmap* CreateBitmap(HICON hb);
	ID2D1Bitmap* CreateBitmap(int width, int height);

	ID2D1Bitmap1* GetBitmap();
	ID2D1Bitmap* GetSharedBitmap();
	IWICBitmap* GetWicBitmap();

	ID2D1LinearGradientBrush* CreateLinearGradientBrush(D2D1_GRADIENT_STOP* stops, unsigned int stopcount);
	ID2D1RadialGradientBrush* CreateRadialGradientBrush(D2D1_GRADIENT_STOP* stops, unsigned int stopcount, D2D1_POINT_2F center);
	ID2D1BitmapBrush* CreateitmapBrush(ID2D1Bitmap* bmp);
	ID2D1SolidColorBrush* CreateSolidColorBrush(D2D1_COLOR_F color);
	D2D1_SIZE_F Size();
	ID2D1Bitmap* ToBitmapFromSvg(const char* data);
	void SetTransform(D2D1_MATRIX_3X2_F matrix);
	void ClearTransform();

	std::vector<ID2D1Bitmap*> CreateBitmapFromGif(const char* path);
	std::vector<ID2D1Bitmap*> CreateBitmapFromGifBuffer(void* data, int size);
	static HBITMAP CopyFromScreen(int x, int y, int width, int height);
	static HBITMAP CopyFromWidnow(HWND handle, int x, int y, int width, int height);
	static SIZE GetScreenSize(int index = 0);
	static D2D1_SIZE_F GetTextLayoutSize(IDWriteTextLayout* textLayout);
};
class HwndGraphics : public D2DGraphics
{
private:
	HWND hwnd;
	IDXGISwapChain* pSwapChain;
	HRESULT InitDevice();
public:
	HwndGraphics(HWND hWnd);
	void ReSize(UINT width, UINT height) override;
	void BeginRender() override;
	void EndRender() override;
};
class IDXGISwapChainGraphics : public D2DGraphics
{
private:
	IDXGISwapChain* pSwapChain = nullptr;
public:
	IDXGISwapChainGraphics(IDXGISwapChain* swapchain);
	void ReSize(UINT width, UINT height) override;
	void BeginRender() override;
	void EndRender() override;
};
class DxgiSurfaceGraphics : public D2DGraphics
{
private:
	IDXGISurface* pDxgiSurface = NULL;
public:
	DxgiSurfaceGraphics(IDXGISurface* dxgiSurface);
	void ReSize(UINT width, UINT height) override;
	void BeginRender() override;
	void EndRender() override;
};
