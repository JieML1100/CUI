#include "Graphics.h"

#include <algorithm>
#include <cmath>

#include <d2derr.h>

using Microsoft::WRL::ComPtr;

constexpr float INV_255 = 1.0f / 255.0f;
constexpr float PI_F = 3.14159265358979323846f;
constexpr float DEG_TO_RAD = PI_F / 180.0f;
constexpr float OUTLINE_OFFSET = 1.0f;

namespace {
	Font* GetDefaultFont() {
		static Font* defaultFont = new Font(L"Arial", 18.0f);
		return defaultFont;
	}
}

Font* DefaultFontObject = GetDefaultFont();
D2DGraphics::D2DGraphics() = default;

D2DGraphics::D2DGraphics(IWICBitmap* bitmap, bool takeOwnership) {
	InitializeWithWicBitmap(
		bitmap,
		takeOwnership,
		96.0f,
		96.0f,
		DXGI_FORMAT_B8G8R8A8_UNORM,
		D2D1_ALPHA_MODE_PREMULTIPLIED);
}

D2DGraphics::D2DGraphics(const BitmapSource* bitmap) {
	InitializeWithWicBitmap(
		bitmap->GetWicBitmap(),
		false,
		96.0f,
		96.0f,
		DXGI_FORMAT_B8G8R8A8_UNORM,
		D2D1_ALPHA_MODE_PREMULTIPLIED);
}

D2DGraphics::D2DGraphics(IDXGISwapChain* swapChain) {
	InitializeWithSwapChain(swapChain);
}

D2DGraphics::D2DGraphics(const InitOptions& options) {
	Initialize(options);
}

D2DGraphics::~D2DGraphics() = default;

void D2DGraphics::SetDpi(FLOAT dpiX, FLOAT dpiY) {
	_dpiX = dpiX;
	_dpiY = dpiY;
	if (pRenderTarget) {
		pRenderTarget->SetDpi(dpiX, dpiY);
	}
}

HRESULT D2DGraphics::ConfigDefaultObjects() {
	if (!pRenderTarget) {
		return E_FAIL;
	}

	D2D1_COLOR_F defaultColor{ 1.0f, 1.0f, 1.0f, 1.0f };
	if (!Default_Brush) {
		HRESULT hr = pRenderTarget->CreateSolidColorBrush(defaultColor, Default_Brush.ReleaseAndGetAddressOf());
		if (FAILED(hr)) {
			return hr;
		}
	}
	else {
		Default_Brush->SetColor(defaultColor);
	}

	if (!Default_Brush_Back) {
		HRESULT hr = pRenderTarget->CreateSolidColorBrush(defaultColor, Default_Brush_Back.ReleaseAndGetAddressOf());
		if (FAILED(hr)) {
			return hr;
		}
	}
	else {
		Default_Brush_Back->SetColor(defaultColor);
	}

	return S_OK;
}


HRESULT D2DGraphics::Initialize(const InitOptions& options) {
	HRESULT hr = S_OK;
	switch (options.kind) {
	case SurfaceKind::Offscreen:
		hr = InitializeWithSize(
			options.width,
			options.height,
			options.dpiX,
			options.dpiY,
			options.format,
			options.alphaMode);
		break;
	case SurfaceKind::ExternalBitmap:
		hr = InitializeWithWicBitmap(
			options.wicBitmap,
			options.takeOwnership,
			options.dpiX,
			options.dpiY,
			options.format,
			options.alphaMode);
		break;
	case SurfaceKind::None:
	case SurfaceKind::Hwnd:
	default:
		hr = E_INVALIDARG;
		break;
	}
	return hr;
}

HRESULT D2DGraphics::InitializeWithSize(UINT width, UINT height, FLOAT dpiX, FLOAT dpiY, DXGI_FORMAT format, D2D1_ALPHA_MODE alphaMode) {
	if (width == 0 || height == 0) {
		return E_INVALIDARG;
	}

	ResetTarget();

	ComPtr<IWICBitmap> wicBitmap;
	HRESULT hr = _ImageFactory->CreateBitmap(
		width,
		height,
		GUID_WICPixelFormat32bppPBGRA,
		WICBitmapCacheOnLoad,
		&wicBitmap);
	if (FAILED(hr)) {
		return hr;
	}

	hr = CreateRenderTargetFromBitmap(wicBitmap.Get(), dpiX, dpiY, format, alphaMode);
	if (FAILED(hr)) {
		return hr;
	}

	pWicTargetBitmap = wicBitmap;
	surfaceKind = SurfaceKind::Offscreen;
	return ConfigDefaultObjects();
}

HRESULT D2DGraphics::InitializeWithWicBitmap(IWICBitmap* bitmap, bool takeOwnership, FLOAT dpiX, FLOAT dpiY, DXGI_FORMAT format, D2D1_ALPHA_MODE alphaMode) {
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

	HRESULT hr = CreateRenderTargetFromBitmap(pWicTargetBitmap.Get(), dpiX, dpiY, format, alphaMode);
	if (FAILED(hr)) {
		pWicTargetBitmap.Reset();
		return hr;
	}

	surfaceKind = SurfaceKind::ExternalBitmap;
	return ConfigDefaultObjects();
}

HRESULT D2DGraphics::CreateRenderTargetFromBitmap(IWICBitmap* bitmap, FLOAT dpiX, FLOAT dpiY, DXGI_FORMAT format, D2D1_ALPHA_MODE alphaMode) {
	if (!bitmap) {
		return E_INVALIDARG;
	}

	D2D1_RENDER_TARGET_PROPERTIES properties = D2D1::RenderTargetProperties(
		D2D1_RENDER_TARGET_TYPE_DEFAULT,
		D2D1::PixelFormat(format, alphaMode),
		dpiX,
		dpiY);

	ComPtr<ID2D1RenderTarget> target;
	HRESULT hr = _D2DFactory->CreateWicBitmapRenderTarget(bitmap, &properties, &target);
	if (FAILED(hr)) {
		return hr;
	}

	pRenderTarget = target;
	return S_OK;
}

HRESULT D2DGraphics::InitializeWithSwapChain(IDXGISwapChain* swapChain) {
	if (!swapChain) {
		return E_INVALIDARG;
	}

	ResetTarget();

	ComPtr<IDXGISurface> dxgiSurface;
	HRESULT hr = swapChain->GetBuffer(0, IID_PPV_ARGS(&dxgiSurface));
	if (FAILED(hr)) {
		return hr;
	}

	D2D1_RENDER_TARGET_PROPERTIES properties = D2D1::RenderTargetProperties(
		D2D1_RENDER_TARGET_TYPE_DEFAULT,
		D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED));

	ComPtr<ID2D1RenderTarget> target;
	hr = _D2DFactory->CreateDxgiSurfaceRenderTarget(dxgiSurface.Get(), &properties, &target);
	if (FAILED(hr)) {
		return hr;
	}

	pRenderTarget = target;
	surfaceKind = SurfaceKind::DxgiSwapChain;
	return ConfigDefaultObjects();
}


void D2DGraphics::ResetTarget() {
	pRenderTarget.Reset();
	pWicTargetBitmap.Reset();
	Default_Brush.Reset();
	Default_Brush_Back.Reset();
	surfaceKind = SurfaceKind::None;
}

ID2D1RenderTarget* D2DGraphics::GetRenderTargetRaw() const {
	return pRenderTarget.Get();
}

ComPtr<ID2D1RenderTarget> D2DGraphics::GetRenderTarget() const {
	ComPtr<ID2D1RenderTarget> target;
	if (pRenderTarget) {
		target = pRenderTarget;
	}
	return target;
}

IWICBitmap* D2DGraphics::GetTargetWicBitmap() const {
	return pWicTargetBitmap.Get();
}
ID2D1SolidColorBrush* D2DGraphics::GetColorBrush(D2D1_COLOR_F newcolor) {
	if (!Default_Brush && pRenderTarget) {
		if (FAILED(pRenderTarget->CreateSolidColorBrush(newcolor, Default_Brush.ReleaseAndGetAddressOf()))) {
			return nullptr;
		}
	}
	if (!Default_Brush) {
		return nullptr;
	}
	//*(D2D1_COLOR_F*)((char*)Default_Brush.Get() + 0x38) = *(D2D1_COLOR_F*)&newcolor;
	Default_Brush->SetColor(newcolor);
	return Default_Brush.Get();
}
ID2D1SolidColorBrush* D2DGraphics::GetColorBrush(COLORREF newcolor) {
	return GetColorBrush(D2D1_COLOR_F{ GetRValue(newcolor) * INV_255,GetGValue(newcolor) * INV_255,GetBValue(newcolor) * INV_255,1.0f });
}
ID2D1SolidColorBrush* D2DGraphics::GetColorBrush(int r, int g, int b) {
	return GetColorBrush(D2D1_COLOR_F{ r * INV_255,g * INV_255,b * INV_255,1.0f });
}
ID2D1SolidColorBrush* D2DGraphics::GetColorBrush(float r, float g, float b, float a) {
	return GetColorBrush(D2D1_COLOR_F{ r,g,b,a });
}
ID2D1SolidColorBrush* D2DGraphics::GetBackColorBrush(D2D1_COLOR_F newcolor) {
	if (!Default_Brush_Back && pRenderTarget) {
		if (FAILED(pRenderTarget->CreateSolidColorBrush(newcolor, Default_Brush_Back.ReleaseAndGetAddressOf()))) {
			return nullptr;
		}
	}
	if (!Default_Brush_Back) {
		return nullptr;
	}
	//*(D2D1_COLOR_F*)((char*)Default_Brush_Back.Get() + 0x38) = *(D2D1_COLOR_F*)&newcolor;
	Default_Brush_Back->SetColor(newcolor);
	return Default_Brush_Back.Get();
}
ID2D1SolidColorBrush* D2DGraphics::GetBackColorBrush(COLORREF newcolor) {
	D2D1_COLOR_F _newcolor = { GetRValue(newcolor) * INV_255,GetGValue(newcolor) * INV_255,GetBValue(newcolor) * INV_255,1.0f };
	return GetBackColorBrush(_newcolor);
}
ID2D1SolidColorBrush* D2DGraphics::GetBackColorBrush(int r, int g, int b) {
	D2D1_COLOR_F _newcolor = { r * INV_255,g * INV_255,b * INV_255, 1.0f };
	return GetBackColorBrush(_newcolor);
}
ID2D1SolidColorBrush* D2DGraphics::GetBackColorBrush(float r, float g, float b, float a) {
	D2D1_COLOR_F _newcolor = { r,g,b,a };
	return GetBackColorBrush(_newcolor);
}
void D2DGraphics::BeginRender() {
	if (!pRenderTarget) {
		return;
	}
	pRenderTarget->BeginDraw();
}
void D2DGraphics::EndRender() {
	if (!pRenderTarget) {
		return;
	}
	pRenderTarget->EndDraw();
}
void D2DGraphics::ReSize(UINT width, UINT height) {
	if (surfaceKind != SurfaceKind::Offscreen) {
		return;
	}
	width = std::max<UINT>(1, width);
	height = std::max<UINT>(1, height);

	FLOAT dpiX = 96.0f;
	FLOAT dpiY = 96.0f;
	DXGI_FORMAT format = DXGI_FORMAT_B8G8R8A8_UNORM;
	D2D1_ALPHA_MODE alpha = D2D1_ALPHA_MODE_PREMULTIPLIED;

	if (pRenderTarget) {
		pRenderTarget->GetDpi(&dpiX, &dpiY);
		D2D1_PIXEL_FORMAT pixelFormat = pRenderTarget->GetPixelFormat();
		format = pixelFormat.format;
		alpha = pixelFormat.alphaMode;
	}

	InitializeWithSize(width, height, dpiX, dpiY, format, alpha);
}
void D2DGraphics::Clear(D2D1_COLOR_F color) {
	if (!pRenderTarget) {
		return;
	}
	pRenderTarget->Clear(color);
}
void D2DGraphics::DrawLine(D2D1_POINT_2F p1, D2D1_POINT_2F p2, D2D1_COLOR_F color, float linewidth) {
	auto* ctx = pRenderTarget.Get();
	if (!ctx) {
		return;
	}
	auto brush = this->GetColorBrush(color);
	if (!brush) {
		return;
	}
	ctx->DrawLine(p1, p2, brush, linewidth);
}
void D2DGraphics::DrawLine(float p1_x, float p1_y, float p2_x, float p2_y, D2D1_COLOR_F color, float linewidth) {
	DrawLine(D2D1::Point2F(p1_x, p1_y), D2D1::Point2F(p2_x, p2_y), color, linewidth);
}
void D2DGraphics::DrawLine(D2D1_POINT_2F p1, D2D1_POINT_2F p2, ID2D1Brush* brush, float linewidth) {
	auto* ctx = pRenderTarget.Get();
	if (!ctx || !brush) {
		return;
	}
	ctx->DrawLine(p1, p2, brush, linewidth);
}
void D2DGraphics::DrawLine(float p1_x, float p1_y, float p2_x, float p2_y, ID2D1Brush* brush, float linewidth) {
	DrawLine(D2D1::Point2F(p1_x, p1_y), D2D1::Point2F(p2_x, p2_y), brush, linewidth);
}
void D2DGraphics::DrawRect(D2D1_RECT_F rect, D2D1_COLOR_F color, float linewidth) {
	auto* ctx = pRenderTarget.Get();
	if (!ctx) {
		return;
	}
	auto brush = this->GetColorBrush(color);
	if (!brush) {
		return;
	}
	ctx->DrawRectangle(rect, brush, linewidth);
}
void D2DGraphics::DrawRect(float left, float top, float width, float height, D2D1_COLOR_F color, float linewidth) {
	DrawRect(D2D1::RectF(left, top, left + width, top + height), color, linewidth);
}
void D2DGraphics::DrawRoundRect(D2D1_RECT_F rect, D2D1_COLOR_F color, float linewidth, float r) {
	auto* ctx = pRenderTarget.Get();
	if (!ctx) {
		return;
	}
	auto brush = this->GetColorBrush(color);
	if (!brush) {
		return;
	}
	ctx->DrawRoundedRectangle(D2D1::RoundedRect(rect, r, r), brush, linewidth);
}
void D2DGraphics::DrawRoundRect(float left, float top, float width, float height, D2D1_COLOR_F color, float linewidth, float r) {
	DrawRoundRect(D2D1::RectF(left, top, left + width, top + height), color, linewidth, r);
}
void D2DGraphics::FillRect(D2D1_RECT_F rect, D2D1_COLOR_F color) {
	auto* ctx = pRenderTarget.Get();
	if (!ctx) {
		return;
	}
	auto brush = this->GetColorBrush(color);
	if (!brush) {
		return;
	}
	ctx->FillRectangle(rect, brush);
}
void D2DGraphics::FillRect(D2D1_RECT_F rect, ID2D1Brush* brush) {
	auto* ctx = pRenderTarget.Get();
	if (!ctx || !brush) {
		return;
	}
	ctx->FillRectangle(rect, brush);
}
void D2DGraphics::FillRect(float left, float top, float width, float height, D2D1_COLOR_F color) {
	FillRect(D2D1::RectF(left, top, left + width, top + height), color);
}
void D2DGraphics::FillRect(float left, float top, float width, float height, ID2D1Brush* brush) {
	FillRect(D2D1::RectF(left, top, left + width, top + height), brush);
}
void D2DGraphics::FillRoundRect(D2D1_RECT_F rect, D2D1_COLOR_F color, float r) {
	auto* ctx = pRenderTarget.Get();
	if (!ctx) {
		return;
	}
	auto brush = this->GetColorBrush(color);
	if (!brush) {
		return;
	}
	ctx->FillRoundedRectangle(D2D1::RoundedRect(rect, r, r), brush);
}
void D2DGraphics::FillRoundRect(float left, float top, float width, float height, D2D1_COLOR_F color, float r) {
	FillRoundRect(D2D1::RectF(left, top, left + width, top + height), color, r);
}
void D2DGraphics::DrawEllipse(D2D1_POINT_2F cent, float xr, float yr, D2D1_COLOR_F color, float linewidth) {
	auto* ctx = pRenderTarget.Get();
	if (!ctx) {
		return;
	}
	auto brush = this->GetColorBrush(color);
	if (!brush) {
		return;
	}
	ctx->DrawEllipse(D2D1::Ellipse(cent, xr, yr), brush, linewidth);
}
void D2DGraphics::DrawEllipse(float x, float y, float xr, float yr, D2D1_COLOR_F color, float linewidth) {
	DrawEllipse(D2D1::Point2F(x, y), xr, yr, color, linewidth);
}
void D2DGraphics::FillEllipse(D2D1_POINT_2F cent, float xr, float yr, D2D1_COLOR_F color) {
	auto* ctx = pRenderTarget.Get();
	if (!ctx) {
		return;
	}
	auto brush = this->GetColorBrush(color);
	if (!brush) {
		return;
	}
	ctx->FillEllipse(D2D1::Ellipse(cent, xr, yr), brush);
}
void D2DGraphics::FillEllipse(float cx, float cy, float xr, float yr, D2D1_COLOR_F color) {
	FillEllipse(D2D1::Point2F(cx, cy), xr, yr, color);
}
void D2DGraphics::DrawGeometry(ID2D1Geometry* geo, D2D1_COLOR_F color, float linewidth) {
	auto* ctx = pRenderTarget.Get();
	if (!ctx || !geo) {
		return;
	}
	auto brush = this->GetColorBrush(color);
	if (!brush) {
		return;
	}
	ctx->DrawGeometry(geo, brush, linewidth);
}
void D2DGraphics::FillGeometry(ID2D1Geometry* geo, D2D1_COLOR_F color) {
	auto* ctx = pRenderTarget.Get();
	if (!ctx || !geo) {
		return;
	}
	auto brush = this->GetColorBrush(color);
	if (!brush) {
		return;
	}
	ctx->FillGeometry(geo, brush);
}
void D2DGraphics::DrawGeometry(ID2D1Geometry* geo, ID2D1Brush* brush, float linewidth) {
	auto* ctx = pRenderTarget.Get();
	if (!ctx || !geo || !brush) {
		return;
	}
	ctx->DrawGeometry(geo, brush, linewidth);
}
void D2DGraphics::FillGeometry(ID2D1Geometry* geo, ID2D1Brush* brush) {
	auto* ctx = pRenderTarget.Get();
	if (!ctx || !geo || !brush) {
		return;
	}
	ctx->FillGeometry(geo, brush);
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

	// 辅助函数：绘制轮廓文本
	void DrawTextOutline(ID2D1RenderTarget* ctx, IDWriteTextLayout* layout, float x, float y,
		ID2D1SolidColorBrush* outlineBrush) {
		ctx->DrawTextLayout(D2D1::Point2F(x - OUTLINE_OFFSET, y - OUTLINE_OFFSET), layout, outlineBrush);
		ctx->DrawTextLayout(D2D1::Point2F(x + OUTLINE_OFFSET, y - OUTLINE_OFFSET), layout, outlineBrush);
		ctx->DrawTextLayout(D2D1::Point2F(x - OUTLINE_OFFSET, y + OUTLINE_OFFSET), layout, outlineBrush);
		ctx->DrawTextLayout(D2D1::Point2F(x + OUTLINE_OFFSET, y + OUTLINE_OFFSET), layout, outlineBrush);
	}
}

void D2DGraphics::FillPie(D2D1_POINT_2F center, float width, float height, float startAngle, float sweepAngle, D2D1_COLOR_F color) {
	auto* ctx = pRenderTarget.Get();
	if (!ctx) return;
	auto brush = GetColorBrush(color);
	if (!brush) return;
	auto geo = CreatePieGeometry(center, width, height, startAngle, sweepAngle);
	if (!geo) return;
	ctx->FillGeometry(geo.Get(), brush);
}

void D2DGraphics::FillPie(D2D1_POINT_2F center, float width, float height, float startAngle, float sweepAngle, ID2D1Brush* brush) {
	auto* ctx = pRenderTarget.Get();
	if (!ctx || !brush) return;
	auto geo = CreatePieGeometry(center, width, height, startAngle, sweepAngle);
	if (!geo) return;
	ctx->FillGeometry(geo.Get(), brush);
}
void D2DGraphics::DrawBitmap(ID2D1Bitmap* bmp, float x, float y, float opacity) {
	auto* ctx = pRenderTarget.Get();
	if (!ctx || !bmp) {
		return;
	}
	D2D1_SIZE_F siz = bmp->GetSize();
	ctx->DrawBitmap(bmp, D2D1::RectF(x, y, siz.width + x, siz.height + y), opacity);
}
void D2DGraphics::DrawBitmap(ID2D1Bitmap* bmp, D2D1_RECT_F rect, float opacity) {
	auto* ctx = pRenderTarget.Get();
	if (!ctx || !bmp) {
		return;
	}
	ctx->DrawBitmap(bmp, rect, opacity);
}
void D2DGraphics::DrawBitmap(ID2D1Bitmap* bmp, D2D1_RECT_F destRect, D2D1_RECT_F srcRect, float opacity) {
	auto* ctx = pRenderTarget.Get();
	if (!ctx || !bmp) {
		return;
	}
	ctx->DrawBitmap(bmp, destRect, opacity, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, srcRect);
}
void D2DGraphics::DrawBitmap(ID2D1Bitmap* bmp, float x, float y, float w, float h, float opacity) {
	auto* ctx = pRenderTarget.Get();
	if (!ctx || !bmp) {
		return;
	}
	D2D1_RECT_F rect = D2D1::RectF(x, y, w + x, h + y);
	ctx->DrawBitmap(bmp, rect, opacity);
}
void D2DGraphics::DrawBitmap(ID2D1Bitmap* bmp, float dest_x, float dest_y, float dest_w, float dest_h, float src_x, float src_y, float src_w, float src_h, float opacity) {
	auto* ctx = pRenderTarget.Get();
	if (!ctx || !bmp) {
		return;
	}
	ctx->DrawBitmap(bmp, D2D1::RectF(dest_x, dest_y, dest_w + dest_x, dest_h + dest_y), opacity, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, D2D1::RectF(src_x, src_y, src_w + src_x, src_h + src_y));
}

void D2DGraphics::FillOpacityMask(ID2D1Bitmap* mask, D2D1_POINT_2F destPoint, D2D1_COLOR_F color, D2D1_OPACITY_MASK_CONTENT content) {
	auto* ctx = pRenderTarget.Get();
	if (!ctx || !mask) {
		return;
	}
	auto brush = this->GetColorBrush(color);
	if (!brush) {
		return;
	}
	auto bitmapSize = mask->GetSize();
	D2D1_RECT_F maskRect = D2D1::RectF(0, 0, bitmapSize.width, bitmapSize.height);
	D2D1_RECT_F destRect = D2D1::RectF(destPoint.x, destPoint.y, destPoint.x + bitmapSize.width, destPoint.y + bitmapSize.height);
	ctx->FillOpacityMask(mask, brush, content, &destRect, &maskRect);
}

void D2DGraphics::FillOpacityMask(ID2D1Bitmap* mask, D2D1_RECT_F destRect, D2D1_COLOR_F color, D2D1_OPACITY_MASK_CONTENT content) {
	auto* ctx = pRenderTarget.Get();
	if (!ctx || !mask) {
		return;
	}
	auto brush = this->GetColorBrush(color);
	if (!brush) {
		return;
	}
	auto bitmapSize = mask->GetSize();
	D2D1_RECT_F maskRect = D2D1::RectF(0, 0, bitmapSize.width, bitmapSize.height);
	ctx->FillOpacityMask(mask, brush, content, &destRect, &maskRect);
}

void D2DGraphics::FillOpacityMask(ID2D1Bitmap* mask, D2D1_RECT_F destRect, D2D1_RECT_F srcRect, D2D1_COLOR_F color, D2D1_OPACITY_MASK_CONTENT content) {
	auto* ctx = pRenderTarget.Get();
	if (!ctx || !mask) {
		return;
	}
	auto brush = this->GetColorBrush(color);
	if (!brush) {
		return;
	}
	auto bitmapSize = mask->GetSize();
	ctx->FillOpacityMask(mask, brush, content, &destRect, &srcRect);
}

void D2DGraphics::FillMesh(ID2D1Mesh* mesh, D2D1_COLOR_F color) {
	auto* ctx = pRenderTarget.Get();
	if (!ctx || !mesh) {
		return;
	}
	auto brush = this->GetColorBrush(color);
	if (!brush) {
		return;
	}
	ctx->FillMesh(mesh, brush);
}

IDWriteTextLayout* D2DGraphics::CreateStringLayout(const std::wstring& str, float width, float height, Font* font) {
	IDWriteTextLayout* textLayout = nullptr;
	Font* resolvedFont = font ? font : DefaultFontObject;
	if (!resolvedFont) {
		return nullptr;
	}
	IDWriteTextFormat* fnt = resolvedFont->FontObject;
	_DWriteFactory->CreateTextLayout(str.c_str(), static_cast<UINT32>(str.size()), fnt, width, height, &textLayout);
	return textLayout;
}
void D2DGraphics::DrawStringLayout(IDWriteTextLayout* layout, float x, float y, D2D1_COLOR_F color) {
	auto* ctx = pRenderTarget.Get();
	if (!ctx || !layout) {
		return;
	}
	auto brush = GetColorBrush(color);
	if (!brush) {
		return;
	}
	ctx->DrawTextLayout(D2D1::Point2F(x, y), layout, brush);
}
void D2DGraphics::DrawStringLayout(IDWriteTextLayout* layout, float x, float y, ID2D1Brush* brush) {
	auto* ctx = pRenderTarget.Get();
	if (!ctx || !layout || !brush) {
		return;
	}
	ctx->DrawTextLayout(D2D1::Point2F(x, y), layout, brush);
}

void D2DGraphics::DrawStringLayoutCentered(IDWriteTextLayout* layout, float centerX, float centerY, D2D1_COLOR_F color) {
	auto* ctx = pRenderTarget.Get();
	if (!ctx || !layout) {
		return;
	}
	auto brush = GetColorBrush(color);
	if (!brush) {
		return;
	}

	D2D1_SIZE_F textSize = GetTextLayoutSize(layout);
	float x = centerX - textSize.width * 0.5f;
	float y = centerY - textSize.height * 0.5f;
	ctx->DrawTextLayout(D2D1::Point2F(x, y), layout, brush);
}

void D2DGraphics::DrawStringLayoutCentered(IDWriteTextLayout* layout, float centerX, float centerY, ID2D1Brush* brush) {
	auto* ctx = pRenderTarget.Get();
	if (!ctx || !layout || !brush) {
		return;
	}

	D2D1_SIZE_F textSize = GetTextLayoutSize(layout);
	float x = centerX - textSize.width * 0.5f;
	float y = centerY - textSize.height * 0.5f;

	ctx->DrawTextLayout(D2D1::Point2F(x, y), layout, brush);
}

void D2DGraphics::DrawStringLayoutOutlined(IDWriteTextLayout* layout, float x, float y, D2D1_COLOR_F textColor, D2D1_COLOR_F outlineColor) {
	auto* ctx = pRenderTarget.Get();
	if (!ctx || !layout) return;
	auto textBrush = GetColorBrush(textColor);
	auto outlineBrush = GetBackColorBrush(outlineColor);
	if (!textBrush || !outlineBrush) return;
	DrawTextOutline(ctx, layout, x, y, outlineBrush);
	ctx->DrawTextLayout(D2D1::Point2F(x, y), layout, textBrush);
}

void D2DGraphics::DrawStringLayoutOutlined(IDWriteTextLayout* layout, float x, float y, ID2D1Brush* textBrush, D2D1_COLOR_F outlineColor) {
	auto* ctx = pRenderTarget.Get();
	if (!ctx || !layout || !textBrush) return;
	auto outlineBrush = GetBackColorBrush(outlineColor);
	if (!outlineBrush) return;
	DrawTextOutline(ctx, layout, x, y, outlineBrush);
	ctx->DrawTextLayout(D2D1::Point2F(x, y), layout, textBrush);
}

void D2DGraphics::DrawStringLayoutCenteredOutlined(IDWriteTextLayout* layout, float centerX, float centerY, D2D1_COLOR_F textColor, D2D1_COLOR_F outlineColor) {
	auto* ctx = pRenderTarget.Get();
	if (!ctx || !layout) return;
	auto textBrush = GetColorBrush(textColor);
	auto outlineBrush = GetBackColorBrush(outlineColor);
	if (!textBrush || !outlineBrush) return;
	D2D1_SIZE_F textSize = GetTextLayoutSize(layout);
	float x = centerX - textSize.width * 0.5f;
	float y = centerY - textSize.height * 0.5f;
	DrawTextOutline(ctx, layout, x, y, outlineBrush);
	ctx->DrawTextLayout(D2D1::Point2F(x, y), layout, textBrush);
}

void D2DGraphics::DrawStringLayoutCenteredOutlined(IDWriteTextLayout* layout, float centerX, float centerY, ID2D1Brush* textBrush, D2D1_COLOR_F outlineColor) {
	auto* ctx = pRenderTarget.Get();
	if (!ctx || !layout || !textBrush) return;
	auto outlineBrush = GetBackColorBrush(outlineColor);
	if (!outlineBrush) return;
	D2D1_SIZE_F textSize = GetTextLayoutSize(layout);
	float x = centerX - textSize.width * 0.5f;
	float y = centerY - textSize.height * 0.5f;
	DrawTextOutline(ctx, layout, x, y, outlineBrush);
	ctx->DrawTextLayout(D2D1::Point2F(x, y), layout, textBrush);
}

void D2DGraphics::DrawStringLayoutEffect(IDWriteTextLayout* layout, float x, float y, D2D1_COLOR_F color, DWRITE_TEXT_RANGE subRange, D2D1_COLOR_F fontBack, Font* font) {
	if (!layout) {
		return;
	}
	auto* ctx = pRenderTarget.Get();
	if (!ctx) {
		return;
	}
	auto backBrush = GetBackColorBrush(fontBack);
	auto frontBrush = GetColorBrush(color);
	if (!backBrush || !frontBrush) {
		return;
	}
	layout->SetDrawingEffect(NULL, DWRITE_TEXT_RANGE{ 0, UINT_MAX });
	layout->SetDrawingEffect(backBrush, subRange);
	ctx->DrawTextLayout(D2D1::Point2F(x, y), layout, frontBrush);
}
void D2DGraphics::DrawStringLayoutEffect(IDWriteTextLayout* layout, float x, float y, ID2D1Brush* brush, DWRITE_TEXT_RANGE subRange, D2D1_COLOR_F fontBack, Font* font) {
	if (!layout || !brush) {
		return;
	}
	auto* ctx = pRenderTarget.Get();
	if (!ctx) {
		return;
	}
	auto backBrush = GetBackColorBrush(fontBack);
	if (!backBrush) {
		return;
	}
	layout->SetDrawingEffect(NULL, DWRITE_TEXT_RANGE{ 0, UINT_MAX });
	layout->SetDrawingEffect(backBrush, subRange);
	ctx->DrawTextLayout(D2D1::Point2F(x, y), layout, brush);
}
void D2DGraphics::DrawString(const std::wstring& str, float x, float y, D2D1_COLOR_F color, Font* font) {
	D2D1_RECT_F rect = D2D1::RectF(x, y, FLT_MAX, FLT_MAX);
	Font* resolvedFont = font ? font : DefaultFontObject;
	if (!resolvedFont) return;
	auto* ctx = pRenderTarget.Get();
	if (!ctx) return;
	auto brush = GetColorBrush(color);
	if (!brush) return;
	ctx->DrawText(str.c_str(), static_cast<UINT32>(str.size()), resolvedFont->FontObject, &rect, brush);
}
void D2DGraphics::DrawString(const std::wstring& str, float x, float y, ID2D1Brush* brush, Font* font) {
	D2D1_RECT_F rect = D2D1::RectF(x, y, FLT_MAX, FLT_MAX);
	Font* resolvedFont = font ? font : DefaultFontObject;
	if (!resolvedFont) return;
	auto* ctx = pRenderTarget.Get();
	if (!ctx || !brush) return;
	auto fnt = resolvedFont->FontObject;
	ctx->DrawText(str.c_str(), static_cast<UINT32>(str.size()), fnt, &rect, brush);
}
void D2DGraphics::DrawString(const std::wstring& str, float x, float y, float w, float h, D2D1_COLOR_F color, Font* font) {
	IDWriteTextLayout* textLayout = CreateStringLayout(str, w, h, font ? font : DefaultFontObject);
	if (!textLayout) return;
	auto* ctx = pRenderTarget.Get();
	if (!ctx) {
		textLayout->Release();
		return;
	}
	auto brush = GetColorBrush(color);
	if (!brush) {
		textLayout->Release();
		return;
	}
	ctx->DrawTextLayout({ x,y }, textLayout, brush);
	textLayout->Release();
}
void D2DGraphics::DrawString(const std::wstring& str, float x, float y, float w, float h, ID2D1Brush* brush, Font* font) {
	IDWriteTextLayout* textLayout = CreateStringLayout(str, w, h, font ? font : DefaultFontObject);
	if (!textLayout) return;
	auto* ctx = pRenderTarget.Get();
	if (!ctx || !brush) {
		textLayout->Release();
		return;
	}
	ctx->DrawTextLayout({ x,y }, textLayout, brush);
	textLayout->Release();
}

void D2DGraphics::DrawStringCentered(const std::wstring& str, float centerX, float centerY, D2D1_COLOR_F color, Font* font) {
	Font* resolvedFont = font ? font : DefaultFontObject;
	if (!resolvedFont) return;
	IDWriteTextLayout* textLayout = CreateStringLayout(str, FLT_MAX, FLT_MAX, resolvedFont);
	if (!textLayout) return;
	auto* ctx = pRenderTarget.Get();
	if (!ctx) {
		textLayout->Release();
		return;
	}
	auto brush = GetColorBrush(color);
	if (!brush) {
		textLayout->Release();
		return;
	}

	D2D1_SIZE_F textSize = GetTextLayoutSize(textLayout);
	float x = centerX - textSize.width * 0.5f;
	float y = centerY - textSize.height * 0.5f;

	ctx->DrawTextLayout({ x, y }, textLayout, brush);
	textLayout->Release();
}

void D2DGraphics::DrawStringCentered(const std::wstring& str, float centerX, float centerY, ID2D1Brush* brush, Font* font) {
	Font* resolvedFont = font ? font : DefaultFontObject;
	if (!resolvedFont) return;
	IDWriteTextLayout* textLayout = CreateStringLayout(str, FLT_MAX, FLT_MAX, resolvedFont);
	if (!textLayout) return;
	auto* ctx = pRenderTarget.Get();
	if (!ctx || !brush) {
		textLayout->Release();
		return;
	}

	D2D1_SIZE_F textSize = GetTextLayoutSize(textLayout);
	float x = centerX - textSize.width * 0.5f;
	float y = centerY - textSize.height * 0.5f;

	ctx->DrawTextLayout({ x, y }, textLayout, brush);
	textLayout->Release();
}

void D2DGraphics::DrawStringOutlined(const std::wstring& str, float x, float y, D2D1_COLOR_F textColor, D2D1_COLOR_F outlineColor, Font* font) {
	Font* resolvedFont = font ? font : DefaultFontObject;
	if (!resolvedFont) return;
	IDWriteTextLayout* textLayout = CreateStringLayout(str, FLT_MAX, FLT_MAX, resolvedFont);
	if (!textLayout) return;
	auto* ctx = pRenderTarget.Get();
	if (!ctx) {
		textLayout->Release();
		return;
	}
	auto textBrush = GetColorBrush(textColor);
	auto outlineBrush = GetBackColorBrush(outlineColor);
	if (!textBrush || !outlineBrush) {
		textLayout->Release();
		return;
	}

	DrawTextOutline(ctx, textLayout, x, y, outlineBrush);
	ctx->DrawTextLayout({ x, y }, textLayout, textBrush);
	textLayout->Release();
}

void D2DGraphics::DrawStringOutlined(const std::wstring& str, float x, float y, ID2D1Brush* textBrush, D2D1_COLOR_F outlineColor, Font* font) {
	Font* resolvedFont = font ? font : DefaultFontObject;
	if (!resolvedFont) return;
	IDWriteTextLayout* textLayout = CreateStringLayout(str, FLT_MAX, FLT_MAX, resolvedFont);
	if (!textLayout) return;
	auto* ctx = pRenderTarget.Get();
	if (!ctx || !textBrush) {
		textLayout->Release();
		return;
	}
	auto outlineBrush = GetBackColorBrush(outlineColor);
	if (!outlineBrush) {
		textLayout->Release();
		return;
	}

	DrawTextOutline(ctx, textLayout, x, y, outlineBrush);
	ctx->DrawTextLayout({ x, y }, textLayout, textBrush);
	textLayout->Release();
}

void D2DGraphics::DrawStringCenteredOutlined(const std::wstring& str, float centerX, float centerY, D2D1_COLOR_F textColor, D2D1_COLOR_F outlineColor, Font* font) {
	Font* resolvedFont = font ? font : DefaultFontObject;
	if (!resolvedFont) return;
	IDWriteTextLayout* textLayout = CreateStringLayout(str, FLT_MAX, FLT_MAX, resolvedFont);
	if (!textLayout) return;
	auto* ctx = pRenderTarget.Get();
	if (!ctx) {
		textLayout->Release();
		return;
	}
	auto textBrush = GetColorBrush(textColor);
	auto outlineBrush = GetBackColorBrush(outlineColor);
	if (!textBrush || !outlineBrush) {
		textLayout->Release();
		return;
	}

	D2D1_SIZE_F textSize = GetTextLayoutSize(textLayout);
	float x = centerX - textSize.width * 0.5f;
	float y = centerY - textSize.height * 0.5f;

	DrawTextOutline(ctx, textLayout, x, y, outlineBrush);
	ctx->DrawTextLayout({ x, y }, textLayout, textBrush);
	textLayout->Release();
}

void D2DGraphics::DrawStringCenteredOutlined(const std::wstring& str, float centerX, float centerY, ID2D1Brush* textBrush, D2D1_COLOR_F outlineColor, Font* font) {
	Font* resolvedFont = font ? font : DefaultFontObject;
	if (!resolvedFont) return;
	IDWriteTextLayout* textLayout = CreateStringLayout(str, FLT_MAX, FLT_MAX, resolvedFont);
	if (!textLayout) return;
	auto* ctx = pRenderTarget.Get();
	if (!ctx || !textBrush) {
		textLayout->Release();
		return;
	}
	auto outlineBrush = GetBackColorBrush(outlineColor);
	if (!outlineBrush) {
		textLayout->Release();
		return;
	}

	D2D1_SIZE_F textSize = GetTextLayoutSize(textLayout);
	float x = centerX - textSize.width * 0.5f;
	float y = centerY - textSize.height * 0.5f;

	DrawTextOutline(ctx, textLayout, x, y, outlineBrush);
	ctx->DrawTextLayout({ x, y }, textLayout, textBrush);
	textLayout->Release();
}

void D2DGraphics::FillTriangle(D2D1_TRIANGLE triangle, D2D1_COLOR_F color) {
	auto* ctx = pRenderTarget.Get();
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
}
void D2DGraphics::DrawTriangle(D2D1_TRIANGLE triangle, D2D1_COLOR_F color, float width) {
	auto* ctx = pRenderTarget.Get();
	if (!ctx) {
		return;
	}
	auto brush = GetColorBrush(color);
	if (!brush) {
		return;
	}
	ctx->DrawLine(triangle.point1, triangle.point2, brush, width);
	ctx->DrawLine(triangle.point2, triangle.point3, brush, width);
	ctx->DrawLine(triangle.point3, triangle.point1, brush, width);
}
void D2DGraphics::FillPolygon(std::vector<D2D1_POINT_2F> points, D2D1_COLOR_F color) {
	if (points.size() <= 2) {
		return;
	}
	ID2D1RenderTarget* ctx = pRenderTarget.Get();
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
	sink->BeginFigure(points.front(), D2D1_FIGURE_BEGIN_FILLED);
	sink->AddLines(points.data() + 1, static_cast<UINT32>(points.size() - 1));
	sink->EndFigure(D2D1_FIGURE_END_CLOSED);
	sink->Close();
	ctx->FillGeometry(geo.Get(), brush);
}
void D2DGraphics::FillPolygon(std::initializer_list<D2D1_POINT_2F> points, D2D1_COLOR_F color) {
	if (points.size() <= 2) {
		return;
	}
	auto* ctx = pRenderTarget.Get();
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
	sink->BeginFigure(*points.begin(), D2D1_FIGURE_BEGIN_FILLED);
	sink->AddLines(points.begin() + 1, static_cast<UINT32>(points.size() - 1));
	sink->EndFigure(D2D1_FIGURE_END_CLOSED);
	sink->Close();
	ctx->FillGeometry(geo.Get(), brush);
}
void D2DGraphics::DrawPolygon(std::initializer_list<D2D1_POINT_2F> points, D2D1_COLOR_F color, float width) {
	if (points.size() <= 1) {
		return;
	}
	auto* ctx = pRenderTarget.Get();
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
	sink->BeginFigure(*points.begin(), D2D1_FIGURE_BEGIN_HOLLOW);
	sink->AddLines(points.begin() + 1, static_cast<UINT32>(points.size() - 1));
	sink->EndFigure(D2D1_FIGURE_END_OPEN);
	sink->Close();
	ctx->DrawGeometry(geo.Get(), brush, width);
}
void D2DGraphics::DrawPolygon(std::vector<D2D1_POINT_2F> points, D2D1_COLOR_F color, float width) {
	if (points.size() <= 1) {
		return;
	}
	auto* ctx = pRenderTarget.Get();
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
	sink->BeginFigure(points.front(), D2D1_FIGURE_BEGIN_HOLLOW);
	sink->AddLines(points.data() + 1, static_cast<UINT32>(points.size() - 1));
	sink->EndFigure(D2D1_FIGURE_END_OPEN);
	sink->Close();
	ctx->DrawGeometry(geo.Get(), brush, width);
}
void D2DGraphics::DrawArc(D2D1_POINT_2F center, float size, float sa, float ea, D2D1_COLOR_F color, float width) {
	const auto angleToPoint = [](D2D1_POINT_2F cent, float angle, float len) {
		return len > 0 ? D2D1::Point2F(
			cent.x + sinf(angle * DEG_TO_RAD) * len,
			cent.y - cosf(angle * DEG_TO_RAD) * len) : cent;
		};
	auto* ctx = pRenderTarget.Get();
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
}
void D2DGraphics::DrawArcCounter(D2D1_POINT_2F center, float size, float sa, float ea, D2D1_COLOR_F color, float width) {
	const auto angleToPoint = [](D2D1_POINT_2F cent, float angle, float len) {
		return len > 0 ? D2D1::Point2F(
			cent.x + sinf(angle * DEG_TO_RAD) * len,
			cent.y - cosf(angle * DEG_TO_RAD) * len) : cent;
		};
	auto* ctx = pRenderTarget.Get();
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
}
void D2DGraphics::PushDrawRect(float left, float top, float width, float height) {
	auto* ctx = pRenderTarget.Get();
	if (!ctx) {
		return;
	}
	ctx->PushAxisAlignedClip(D2D1::RectF(left, top, left + width, top + height), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
}
void D2DGraphics::PopDrawRect() {
	auto* ctx = pRenderTarget.Get();
	if (!ctx) {
		return;
	}
	ctx->PopAxisAlignedClip();

}
void D2DGraphics::SetAntialiasMode(D2D1_ANTIALIAS_MODE antialiasMode) {
	auto* ctx = pRenderTarget.Get();
	if (!ctx) {
		return;
	}
	ctx->SetAntialiasMode(antialiasMode);
}
void D2DGraphics::SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE antialiasMode) {
	auto* ctx = pRenderTarget.Get();
	if (!ctx) {
		return;
	}
	ctx->SetTextAntialiasMode(antialiasMode);
}
ID2D1Bitmap* D2DGraphics::CreateBitmap(IWICBitmap* wb) {
	auto* ctx = pRenderTarget.Get();
	if (!ctx || !wb) {
		return nullptr;
	}
	ComPtr<ID2D1Bitmap> bitmap;
	if (FAILED(ctx->CreateBitmapFromWicBitmap(wb, &bitmap))) {
		return nullptr;
	}
	return bitmap.Detach();
}
ID2D1Bitmap* D2DGraphics::CreateBitmap(IWICFormatConverter* conv, D2D1_BITMAP_PROPERTIES* props) {

	auto* ctx = pRenderTarget.Get();
	if (!ctx || !conv) {
		return nullptr;
	}
	ComPtr<ID2D1Bitmap> bitmap;
	if (FAILED(pRenderTarget->CreateBitmapFromWicBitmap(conv, props, &bitmap))) {
		return nullptr;
	}
	return bitmap.Detach();
}

ID2D1Bitmap* D2DGraphics::CreateBitmap(const std::shared_ptr<BitmapSource>& bitmapSource) {
	auto* ctx = pRenderTarget.Get();
	if (!ctx || !bitmapSource) {
		return nullptr;
	}
	ComPtr<ID2D1Bitmap> bitmap;
	if (FAILED(pRenderTarget->CreateBitmapFromWicBitmap(bitmapSource->GetWicBitmap(), nullptr, &bitmap))) {
		return nullptr;
	}
	return bitmap.Detach();
}

ID2D1LinearGradientBrush* D2DGraphics::CreateLinearGradientBrush(D2D1_GRADIENT_STOP* stops, unsigned int stopcount) {
	auto* ctx = pRenderTarget.Get();
	if (!ctx || !stops || stopcount == 0) {
		return nullptr;
	}
	ComPtr<ID2D1GradientStopCollection> collection;
	if (FAILED(ctx->CreateGradientStopCollection(stops, stopcount, &collection))) {
		return nullptr;
	}
	ComPtr<ID2D1LinearGradientBrush> brush;
	if (FAILED(ctx->CreateLinearGradientBrush({}, collection.Get(), &brush))) {
		return nullptr;
	}
	return brush.Detach();
}
ID2D1RadialGradientBrush* D2DGraphics::CreateRadialGradientBrush(D2D1_GRADIENT_STOP* stops, unsigned int stopcount, D2D1_POINT_2F center) {
	auto* ctx = pRenderTarget.Get();
	if (!ctx || !stops || stopcount == 0) {
		return nullptr;
	}
	ComPtr<ID2D1GradientStopCollection> collection;
	if (FAILED(ctx->CreateGradientStopCollection(stops, stopcount, &collection))) {
		return nullptr;
	}
	D2D1_RADIAL_GRADIENT_BRUSH_PROPERTIES props = {};
	props.center = center;
	ComPtr<ID2D1RadialGradientBrush> brush;
	if (FAILED(ctx->CreateRadialGradientBrush(props, collection.Get(), &brush))) {
		return nullptr;
	}
	return brush.Detach();
}
ID2D1BitmapBrush* D2DGraphics::CreateBitmapBrush(ID2D1Bitmap* bmp) {
	auto* ctx = pRenderTarget.Get();
	if (!ctx || !bmp) {
		return nullptr;
	}
	ComPtr<ID2D1BitmapBrush> brush;
	if (FAILED(ctx->CreateBitmapBrush(bmp, nullptr, nullptr, &brush))) {
		return nullptr;
	}
	return brush.Detach();
}
ID2D1SolidColorBrush* D2DGraphics::CreateSolidColorBrush(D2D1_COLOR_F color) {
	auto* ctx = pRenderTarget.Get();
	if (!ctx) {
		return nullptr;
	}
	ComPtr<ID2D1SolidColorBrush> result;
	if (FAILED(ctx->CreateSolidColorBrush(color, &result))) {
		return nullptr;
	}
	return result.Detach();
}
D2D1_SIZE_F D2DGraphics::Size() {
	return pRenderTarget ? pRenderTarget->GetSize() : D2D1::SizeF(0.0f, 0.0f);
}

void D2DGraphics::SetTransform(D2D1_MATRIX_3X2_F matrix) {
	if (pRenderTarget) {
		pRenderTarget->SetTransform(matrix);
	}
}
void D2DGraphics::ClearTransform() {
	if (pRenderTarget) {
		pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());
	}
}
HBITMAP D2DGraphics::CopyFromScreen(int x, int y, int width, int height) {
	HDC sourceDC = GetDC(NULL);
	HDC memDC = CreateCompatibleDC(sourceDC);
	HBITMAP memBitmap = CreateCompatibleBitmap(sourceDC, width, height);
	SelectObject(memDC, memBitmap);
	BitBlt(memDC, 0, 0, width, height, sourceDC, x, y, SRCCOPY);
	DeleteDC(memDC);
	ReleaseDC(NULL, sourceDC);
	return memBitmap;
}
HBITMAP D2DGraphics::CopyFromWidnow(HWND hWnd, int x, int y, int width, int height) {
	HDC sourceDC = GetDC(hWnd);
	HDC memDC = ::CreateCompatibleDC(sourceDC);
	HBITMAP memBitmap = ::CreateCompatibleBitmap(sourceDC, width, height);
	SelectObject(memDC, memBitmap);
	BitBlt(memDC, 0, 0, width, height, sourceDC, x, y, SRCCOPY);
	DeleteDC(memDC);
	ReleaseDC(hWnd, sourceDC);
	return memBitmap;
}
SIZE D2DGraphics::GetScreenSize(int index) {
	std::vector<SIZE> sizes = std::vector<SIZE>();
	auto callback = [](HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) -> BOOL {
		std::vector<SIZE>* tmp = reinterpret_cast<std::vector<SIZE>*>(dwData);
		MONITORINFOEX miex{};
		miex.cbSize = sizeof(miex);
		GetMonitorInfo(hMonitor, &miex);
		DEVMODE dm{};
		dm.dmSize = sizeof(dm);
		dm.dmDriverExtra = 0;
		EnumDisplaySettings(miex.szDevice, ENUM_CURRENT_SETTINGS, &dm);
		tmp->push_back(SIZE{ (int)dm.dmPelsWidth,(int)dm.dmPelsHeight });
		return TRUE;
		};
	EnumDisplayMonitors(NULL, NULL, callback, (LPARAM)&sizes);
	if (sizes.size() > index) {
		return sizes[index];
	}
	return{ GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN) };
}

D2D1_SIZE_F D2DGraphics::GetTextLayoutSize(IDWriteTextLayout* textLayout) {
	D2D1_SIZE_F minSize = { 0,0 };
	DWRITE_TEXT_METRICS metrics;
	HRESULT hr = textLayout->GetMetrics(&metrics);
	if (SUCCEEDED(hr)) {
		minSize = D2D1::Size((float)ceil(metrics.widthIncludingTrailingWhitespace), (float)ceil(metrics.height));
		return minSize;
	}
	return D2D1_SIZE_F{ 0,0 };
}

CompatibleGraphics::CompatibleGraphics(D2DGraphics* parent, D2D1_SIZE_F desiredSize) {
	Initialize(parent, desiredSize);
}

HRESULT CompatibleGraphics::Initialize(D2DGraphics* parent, D2D1_SIZE_F desiredSize) {
	parentRenderTarget = parent->GetRenderTarget();
	if (!parentRenderTarget) {
		return E_INVALIDARG;
	}

	this->desiredSize = desiredSize;

	return RecreateTarget();
}

void CompatibleGraphics::ResetCompatibleTarget() {
	compatibleTarget.Reset();
	D2DGraphics::ResetTarget();
}

HRESULT CompatibleGraphics::RecreateTarget() {
	if (!parentRenderTarget) {
		return E_INVALIDARG;
	}

	ResetCompatibleTarget();

	ComPtr<ID2D1BitmapRenderTarget> target;
	HRESULT hr = parentRenderTarget->CreateCompatibleRenderTarget(desiredSize, &target);
	if (FAILED(hr)) {
		return hr;
	}

	compatibleTarget = target;
	pRenderTarget = compatibleTarget;
	surfaceKind = SurfaceKind::Compatible;
	return ConfigDefaultObjects();
}

void CompatibleGraphics::ReSize(UINT width, UINT height) {
	width = std::max<UINT>(1, width);
	height = std::max<UINT>(1, height);
	desiredSize = D2D1::SizeF((FLOAT)width, (FLOAT)height);
	RecreateTarget();
}

ID2D1Bitmap* CompatibleGraphics::GetBitmap() const {
	if (!compatibleTarget) {
		return nullptr;
	}
	ComPtr<ID2D1Bitmap> bitmap;
	HRESULT hr = compatibleTarget->GetBitmap(&bitmap);
	if (FAILED(hr)) {
		return nullptr;
	}
	return bitmap.Detach();
}

ID2D1BitmapRenderTarget* CompatibleGraphics::GetBitmapRenderTarget() const {
	return compatibleTarget.Get();
}

HRESULT HwndGraphics::InitDevice() {
	if (!hwnd) {
		return E_INVALIDARG;
	}

	RECT rc{};
	GetClientRect(hwnd, &rc);
	UINT width = std::max<UINT>(1, static_cast<UINT>(rc.right - rc.left));
	UINT height = std::max<UINT>(1, static_cast<UINT>(rc.bottom - rc.top));

	auto makeProps = [&](D2D1_RENDER_TARGET_TYPE type) {
		return D2D1::RenderTargetProperties(
			type,
			D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
	};

	D2D1_HWND_RENDER_TARGET_PROPERTIES hwndProps = D2D1::HwndRenderTargetProperties(
		hwnd,
		D2D1::SizeU(width, height),
		D2D1_PRESENT_OPTIONS_IMMEDIATELY);

	ComPtr<ID2D1HwndRenderTarget> target;
	HRESULT hr = _D2DFactory->CreateHwndRenderTarget(makeProps(D2D1_RENDER_TARGET_TYPE_DEFAULT), hwndProps, &target);
	if (FAILED(hr)) {
		// RDP 断开/重连、驱动重启等场景下硬件 RT 有时会创建失败；回退到软件 RT 以保证可用性。
		hr = _D2DFactory->CreateHwndRenderTarget(makeProps(D2D1_RENDER_TARGET_TYPE_SOFTWARE), hwndProps, &target);
		if (FAILED(hr)) {
			return hr;
		}
	}

	pHwndRenderTarget = target;
	pRenderTarget = pHwndRenderTarget;
	// render target 重建后 DPI 会回到默认/系统 DPI；这里强制恢复到逻辑 DPI
	pRenderTarget->SetDpi(_dpiX, _dpiY);
	surfaceKind = SurfaceKind::Hwnd;
	return ConfigDefaultObjects();
}

HwndGraphics::HwndGraphics(HWND hWnd) {
	hwnd = hWnd;
	InitDevice();
}

void HwndGraphics::ReSize(UINT width, UINT height) {
	bool recreatedFromNull = false;
	if (!pHwndRenderTarget) {
		// 典型场景：之前因 D2DERR_RECREATE_TARGET ResetTarget() 后 InitDevice 失败，
		// 这时 pHwndRenderTarget 为空；需要在后续 Resize/Render 时继续尝试恢复。
		if (FAILED(InitDevice())) {
			return;
		}
		recreatedFromNull = true;
	}
	width = std::max<UINT>(1, width);
	height = std::max<UINT>(1, height);
	HRESULT hr = pHwndRenderTarget->Resize(D2D1::SizeU(width, height));
	if (recreatedFromNull) {
		::PostMessageW(hwnd, WM_CUI_RENDER_TARGET_RECREATED, 0, 0);
	}
	if (hr == D2DERR_RECREATE_TARGET) {
		// 典型触发场景：远程桌面断开/重连、显示设备切换等导致 render target 失效。
		ResetTarget();
		if (SUCCEEDED(InitDevice())) {
			::PostMessageW(hwnd, WM_CUI_RENDER_TARGET_RECREATED, 0, 0);
			::InvalidateRect(hwnd, nullptr, FALSE);
			::UpdateWindow(hwnd);
		}
	}
}

void HwndGraphics::BeginRender() {
	if (!pRenderTarget) {
		// 从“无 target”状态尝试恢复（例如 InitDevice 曾临时失败）。
		if (SUCCEEDED(InitDevice())) {
			::PostMessageW(hwnd, WM_CUI_RENDER_TARGET_RECREATED, 0, 0);
		}
	}
	D2DGraphics::BeginRender();
}

void HwndGraphics::EndRender() {
	if (!pRenderTarget) {
		return;
	}
	HRESULT hr = pRenderTarget->EndDraw();
	if (hr == D2DERR_RECREATE_TARGET) {
		// 目标丢失：重建 HwndRenderTarget（下一次渲染即可恢复）。
		ResetTarget();
		if (SUCCEEDED(InitDevice())) {
			::PostMessageW(hwnd, WM_CUI_RENDER_TARGET_RECREATED, 0, 0);
			::InvalidateRect(hwnd, nullptr, FALSE);
			::UpdateWindow(hwnd);
		}
	}
}