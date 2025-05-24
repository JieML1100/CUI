#include "Graphics.h"

D2DGraphics::D2DGraphics()
{
	pD2DDeviceContext = NULL;
	Default_Brush = NULL;
	Default_Brush_Back = NULL;
	pD2DTargetBitmap = NULL;
	DefaultFontObject = NULL;
}
D2DGraphics::D2DGraphics(UINT width, UINT height) {
	HRESULT hr = S_OK;
	hr = _D2DDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_ENABLE_MULTITHREADED_OPTIMIZATIONS, &pD2DDeviceContext);
	D2D1_BITMAP_PROPERTIES1 bitmapProperties = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_GDI_COMPATIBLE, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
	hr = pD2DDeviceContext->CreateBitmap(D2D1::SizeU(width, height), NULL, 0, &bitmapProperties, &pD2DTargetBitmap);
	if FAILED(hr)
		return;
	pD2DDeviceContext->SetTarget(pD2DTargetBitmap);
	ConfigDefaultObjects();
}
D2DGraphics::D2DGraphics(ID2D1Bitmap1* bmp) {

	HRESULT hr = S_OK;
	pD2DTargetBitmap = bmp;
	hr = _D2DDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_ENABLE_MULTITHREADED_OPTIMIZATIONS, &pD2DDeviceContext);
	if FAILED(hr)
		return;
	pD2DDeviceContext->SetTarget(pD2DTargetBitmap);
	ConfigDefaultObjects();
}
D2DGraphics::~D2DGraphics() {
	delete DefaultFontObject;
	pD2DDeviceContext->Release();
	Default_Brush->Release();
	Default_Brush_Back->Release();
	if (pD2DTargetBitmap)
		pD2DTargetBitmap->Release();
}
void D2DGraphics::ConfigDefaultObjects() {
	this->DefaultFontObject = new Font(L"Arial", 18);
	D2D1_COLOR_F color = D2D1_COLOR_F{ 0.5f,0.6f,0.7f,1.0f };
	pD2DDeviceContext->CreateSolidColorBrush(color, &this->Default_Brush);
	this->Default_Brush->SetColor(color);
	pD2DDeviceContext->CreateSolidColorBrush(color, &this->Default_Brush_Back);
	this->Default_Brush_Back->SetColor(color);
}
ID2D1SolidColorBrush* D2DGraphics::GetColorBrush(D2D1_COLOR_F newcolor) {
	this->Default_Brush->SetColor(newcolor);
	return this->Default_Brush;
}
ID2D1SolidColorBrush* D2DGraphics::GetColorBrush(COLORREF newcolor) {
	this->Default_Brush->SetColor(D2D1_COLOR_F{ GetRValue(newcolor) / 255.0f,GetGValue(newcolor) / 255.0f,GetBValue(newcolor) / 255.0f,1.0f });
	return this->Default_Brush;
}
ID2D1SolidColorBrush* D2DGraphics::GetColorBrush(int r, int g, int b) {
	this->Default_Brush->SetColor(D2D1_COLOR_F{ r / 255.0f,g / 255.0f,b / 255.0f,1.0f });
	return this->Default_Brush;
}
ID2D1SolidColorBrush* D2DGraphics::GetColorBrush(float r, float g, float b, float a) {
	this->Default_Brush->SetColor(D2D1_COLOR_F{ r,g,b,a });
	return this->Default_Brush;
}
ID2D1SolidColorBrush* D2DGraphics::GetBackColorBrush(D2D1_COLOR_F newcolor) {
	this->Default_Brush_Back->SetColor(newcolor);
	return this->Default_Brush_Back;
}
ID2D1SolidColorBrush* D2DGraphics::GetBackColorBrush(COLORREF newcolor) {
	D2D1_COLOR_F _newcolor = { GetRValue(newcolor) / 255.0f,GetGValue(newcolor) / 255.0f,GetBValue(newcolor) / 255.0f,1.0f };
	this->Default_Brush_Back->SetColor(_newcolor);
	return this->Default_Brush_Back;
}
ID2D1SolidColorBrush* D2DGraphics::GetBackColorBrush(int r, int g, int b) {
	D2D1_COLOR_F _newcolor = { r / 255.0f,g / 255.0f,b / 255.0f,1.0f };
	this->Default_Brush_Back->SetColor(_newcolor);
	return this->Default_Brush_Back;
}
ID2D1SolidColorBrush* D2DGraphics::GetBackColorBrush(float r, float g, float b, float a) {
	D2D1_COLOR_F _newcolor = { r,g,b,a };
	this->Default_Brush_Back->SetColor(_newcolor);
	return this->Default_Brush_Back;
}
void D2DGraphics::BeginRender() {
	pD2DDeviceContext->BeginDraw();
}
void D2DGraphics::EndRender() {
	pD2DDeviceContext->EndDraw();
}
void D2DGraphics::ReSize(UINT width, UINT height) {}
void D2DGraphics::Clear(D2D1_COLOR_F color) {
	pD2DDeviceContext->Clear(color);
}
void D2DGraphics::DrawLine(D2D1_POINT_2F p1, D2D1_POINT_2F p2, D2D1_COLOR_F color, float linewidth) {
	pD2DDeviceContext->DrawLine(p1, p2, this->GetColorBrush(color), linewidth);
}
void D2DGraphics::DrawLine(float p1_x, float p1_y, float p2_x, float p2_y, D2D1_COLOR_F color, float linewidth) {
	pD2DDeviceContext->DrawLine({ p1_x,p1_y }, { p2_x,p2_y }, this->GetColorBrush(color), linewidth);
}
void D2DGraphics::DrawLine(D2D1_POINT_2F p1, D2D1_POINT_2F p2, ID2D1Brush* brush, float linewidth) {
	pD2DDeviceContext->DrawLine(p1, p2, brush, linewidth);
}
void D2DGraphics::DrawLine(float p1_x, float p1_y, float p2_x, float p2_y, ID2D1Brush* brush, float linewidth) {
	pD2DDeviceContext->DrawLine({ p1_x,p1_y }, { p2_x,p2_y }, brush, linewidth);
}
void D2DGraphics::DrawRect(D2D1_RECT_F rect, D2D1_COLOR_F color, float linewidth) {
	pD2DDeviceContext->DrawRectangle(rect, this->GetColorBrush(color), linewidth);
}
void D2DGraphics::DrawRect(float left, float top, float width, float height, D2D1_COLOR_F color, float linewidth) {
	pD2DDeviceContext->DrawRectangle({ left,top,left + width,top + height }, this->GetColorBrush(color), linewidth);
}
void D2DGraphics::DrawRoundRect(D2D1_RECT_F rect, D2D1_COLOR_F color, float linewidth, float r) {
	pD2DDeviceContext->DrawRoundedRectangle(D2D1::RoundedRect(rect, r, r), this->GetColorBrush(color), linewidth);
}
void D2DGraphics::DrawRoundRect(float left, float top, float width, float height, D2D1_COLOR_F color, float linewidth, float r) {
	pD2DDeviceContext->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(left, top, left + width, top + height), r, r), this->GetColorBrush(color), linewidth);
}
void D2DGraphics::FillRect(D2D1_RECT_F rect, D2D1_COLOR_F color) {
	pD2DDeviceContext->FillRectangle(rect, this->GetColorBrush(color));
}
void D2DGraphics::FillRect(D2D1_RECT_F rect, ID2D1Brush* brush) {
	pD2DDeviceContext->FillRectangle(rect, brush);
}
void D2DGraphics::FillRect(float left, float top, float width, float height, D2D1_COLOR_F color) {
	pD2DDeviceContext->FillRectangle({ left,top,left + width,top + height }, this->GetColorBrush(color));
}
void D2DGraphics::FillRect(float left, float top, float width, float height, ID2D1Brush* brush) {
	pD2DDeviceContext->FillRectangle({ left,top,left + width,top + height }, brush);
}
void D2DGraphics::FillRoundRect(D2D1_RECT_F rect, D2D1_COLOR_F color, float r) {
	pD2DDeviceContext->FillRoundedRectangle(D2D1::RoundedRect(rect, r, r), this->GetColorBrush(color));
}
void D2DGraphics::FillRoundRect(float left, float top, float width, float height, D2D1_COLOR_F color, float r) {
	pD2DDeviceContext->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(left, top, left + width, top + height), r, r), this->GetColorBrush(color));
}
void D2DGraphics::DrawEllipse(D2D1_POINT_2F cent, float xr, float yr, D2D1_COLOR_F color, float linewidth) {
	pD2DDeviceContext->DrawEllipse({ cent ,xr,yr }, this->GetColorBrush(color), linewidth = 1.0f);
}
void D2DGraphics::DrawEllipse(float x, float y, float xr, float yr, D2D1_COLOR_F color, float linewidth) {
	pD2DDeviceContext->DrawEllipse({ {x,y} ,xr,yr }, this->GetColorBrush(color), linewidth = 1.0f);
}
void D2DGraphics::FillEllipse(D2D1_POINT_2F cent, float xr, float yr, D2D1_COLOR_F color) {
	pD2DDeviceContext->FillEllipse({ cent ,xr,yr }, this->GetColorBrush(color));
}
void D2DGraphics::FillEllipse(float cx, float cy, float xr, float yr, D2D1_COLOR_F color) {
	pD2DDeviceContext->FillEllipse({ {cx,cy} ,xr,yr }, this->GetColorBrush(color));
}
void D2DGraphics::DrawGeometry(ID2D1Geometry* geo, D2D1_COLOR_F color, float linewidth) {
	pD2DDeviceContext->DrawGeometry(geo, this->GetColorBrush(color), linewidth);
}
void D2DGraphics::FillGeometry(ID2D1Geometry* geo, D2D1_COLOR_F color) {
	pD2DDeviceContext->FillGeometry(geo, this->GetColorBrush(color));
}
void D2DGraphics::DrawGeometry(ID2D1Geometry* geo, ID2D1Brush* brush, float linewidth) {
	pD2DDeviceContext->DrawGeometry(geo, brush, linewidth);
}
void D2DGraphics::FillGeometry(ID2D1Geometry* geo, ID2D1Brush* brush) {
	pD2DDeviceContext->FillGeometry(geo, brush);
}
void D2DGraphics::FillPie(D2D1_POINT_2F center, float width, float height, float startAngle, float sweepAngle, D2D1_COLOR_F color)
{
	ID2D1PathGeometry* geo = Factory::CreateGeomtry();
	if (!geo) return;
	ID2D1GeometrySink* tmp = NULL;
	if (SUCCEEDED(geo->Open(&tmp))) {
		tmp->BeginFigure(center, D2D1_FIGURE_BEGIN_FILLED);
		float startRad = startAngle * (3.14159265359f / 180.0f);
		float sweepRad = sweepAngle * (3.14159265359f / 180.0f);
		D2D1_POINT_2F startPoint = { center.x + (width / 2) * cosf(startRad),center.y - (height / 2) * sinf(startRad) };
		tmp->AddLine(startPoint);
		D2D1_SIZE_F arcSize = { width / 2, height / 2 };
		D2D1_ARC_SIZE arcSizeFlag = (sweepAngle <= 180.0f) ? D2D1_ARC_SIZE_SMALL : D2D1_ARC_SIZE_LARGE;
		tmp->AddArc(D2D1::ArcSegment(center,arcSize,0.0f,D2D1_SWEEP_DIRECTION_CLOCKWISE,arcSizeFlag));
		tmp->EndFigure(D2D1_FIGURE_END_CLOSED);
		tmp->Close();
		pD2DDeviceContext->FillGeometry(geo, this->GetColorBrush(color));
		tmp->Release();
	}
	geo->Release();
}
void D2DGraphics::FillPie(D2D1_POINT_2F center, float width, float height, float startAngle, float sweepAngle, ID2D1Brush* brush)
{
	ID2D1PathGeometry* geo = Factory::CreateGeomtry();
	if (!geo) return;
	ID2D1GeometrySink* tmp = NULL;
	if (SUCCEEDED(geo->Open(&tmp))) {
		tmp->BeginFigure(center, D2D1_FIGURE_BEGIN_FILLED);
		float startRad = startAngle * (3.14159265359f / 180.0f);
		float sweepRad = sweepAngle * (3.14159265359f / 180.0f);
		D2D1_POINT_2F startPoint = { center.x + (width / 2) * cosf(startRad),center.y - (height / 2) * sinf(startRad) };
		tmp->AddLine(startPoint);
		D2D1_SIZE_F arcSize = { width / 2, height / 2 };
		D2D1_ARC_SIZE arcSizeFlag = (sweepAngle <= 180.0f) ? D2D1_ARC_SIZE_SMALL : D2D1_ARC_SIZE_LARGE;
		tmp->AddArc(D2D1::ArcSegment(center, arcSize, 0.0f, D2D1_SWEEP_DIRECTION_CLOCKWISE, arcSizeFlag));
		tmp->EndFigure(D2D1_FIGURE_END_CLOSED);
		tmp->Close();
		pD2DDeviceContext->FillGeometry(geo, brush);
		tmp->Release();
	}
	geo->Release();
}
void D2DGraphics::DrawBitmap(ID2D1Bitmap* bmp, float x, float y, float opacity) {
	D2D1_SIZE_F siz = bmp->GetSize();
	pD2DDeviceContext->DrawBitmap(bmp, D2D1::RectF(x, y, siz.width + x, siz.height + y), opacity);
}
void D2DGraphics::DrawBitmap(ID2D1Bitmap* bmp, D2D1_RECT_F rect, float opacity) {
	pD2DDeviceContext->DrawBitmap(bmp, rect, opacity);
}
void D2DGraphics::DrawBitmap(ID2D1Bitmap* bmp, D2D1_RECT_F destRect, D2D1_RECT_F srcRect, float opacity) {
	pD2DDeviceContext->DrawBitmap(bmp, destRect, opacity, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, srcRect);
}
void D2DGraphics::DrawBitmap(ID2D1Bitmap* bmp, float x, float y, float w, float h, float opacity) {
	D2D1_RECT_F rect = D2D1::RectF(x, y, w + x, h + y);
	pD2DDeviceContext->DrawBitmap(bmp, rect, opacity);
}
void D2DGraphics::DrawBitmap(ID2D1Bitmap* bmp, float dest_x, float dest_y, float dest_w, float dest_h, float src_x, float src_y, float src_w, float src_h, float opacity) {
	pD2DDeviceContext->DrawBitmap(bmp, D2D1::RectF(dest_x, dest_y, dest_w + dest_x, dest_h + dest_y), opacity, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, D2D1::RectF(src_x, src_y, src_w + src_x, src_h + src_y));
}
IDWriteTextLayout* D2DGraphics::CreateStringLayout(std::wstring str, float width, float height, Font* font) {
	IDWriteTextLayout* textLayout = NULL;
	IDWriteTextFormat* fnt = font ? font->FontObject : this->DefaultFontObject->FontObject;
	_DWriteFactory->CreateTextLayout(str.c_str(), str.size(), fnt, width, height, &textLayout);
	return textLayout;
}
void D2DGraphics::DrawStringLayout(IDWriteTextLayout* layout, float x, float y, D2D1_COLOR_F color) {
	pD2DDeviceContext->DrawTextLayout(D2D1::Point2F(x, y), layout, this->GetColorBrush(color));
}
void D2DGraphics::DrawStringLayout(IDWriteTextLayout* layout, float x, float y, ID2D1Brush* brush) {
	pD2DDeviceContext->DrawTextLayout(D2D1::Point2F(x, y), layout, brush);
}
void D2DGraphics::DrawStringLayoutEffect(IDWriteTextLayout* layout, float x, float y, D2D1_COLOR_F color, DWRITE_TEXT_RANGE subRange, D2D1_COLOR_F fontBack, Font* font) {
	layout->SetDrawingEffect(NULL, DWRITE_TEXT_RANGE{ 0, UINT_MAX });
	layout->SetDrawingEffect(this->GetBackColorBrush(fontBack), subRange);
	pD2DDeviceContext->DrawTextLayout(D2D1::Point2F(x, y), layout, this->GetColorBrush(color));
}
void D2DGraphics::DrawStringLayoutEffect(IDWriteTextLayout* layout, float x, float y, ID2D1Brush* brush, DWRITE_TEXT_RANGE subRange, D2D1_COLOR_F fontBack, Font* font) {
	layout->SetDrawingEffect(NULL, DWRITE_TEXT_RANGE{ 0, UINT_MAX });
	layout->SetDrawingEffect(this->GetBackColorBrush(fontBack), subRange);
	pD2DDeviceContext->DrawTextLayout(D2D1::Point2F(x, y), layout, brush);
}
void D2DGraphics::DrawString(std::wstring str, float x, float y, D2D1_COLOR_F color, Font* font) {
	D2D1_RECT_F rect = D2D1::RectF(x, y, FLT_MAX, FLT_MAX);
	pD2DDeviceContext->DrawText(str.c_str(), str.size(), (font ? font : this->DefaultFontObject)->FontObject, &rect, this->GetColorBrush(color));
}
void D2DGraphics::DrawString(std::wstring str, float x, float y, ID2D1Brush* brush, Font* font) {
	D2D1_RECT_F rect = D2D1::RectF(x, y, FLT_MAX, FLT_MAX);
	auto fnt = font ? font->FontObject : this->DefaultFontObject->FontObject;
	pD2DDeviceContext->DrawText(str.c_str(), str.size(), fnt, &rect, brush);
}
void D2DGraphics::DrawString(std::wstring str, float x, float y, float w, float h, D2D1_COLOR_F color, Font* font) {
	IDWriteTextLayout* textLayout = CreateStringLayout(str, w, h, font ? font : this->DefaultFontObject);
	if (!textLayout) return;
	pD2DDeviceContext->DrawTextLayout({ x,y }, textLayout, this->GetColorBrush(color));
	textLayout->Release();
}
void D2DGraphics::DrawString(std::wstring str, float x, float y, float w, float h, ID2D1Brush* brush, Font* font) {
	IDWriteTextLayout* textLayout = CreateStringLayout(str, w, h, font ? font : this->DefaultFontObject);
	if (!textLayout) return;
	pD2DDeviceContext->DrawTextLayout({ x,y }, textLayout, brush);
	textLayout->Release();
}
void D2DGraphics::FillTriangle(D2D1_TRIANGLE triangle, D2D1_COLOR_F color) {
	ID2D1PathGeometry* geo = Factory::CreateGeomtry();
	if (!geo) return;
	ID2D1GeometrySink* tmp = NULL;
	if SUCCEEDED(geo->Open(&tmp)) {
		tmp->BeginFigure(triangle.point1, D2D1_FIGURE_BEGIN_FILLED);
		tmp->AddLine(triangle.point2);
		tmp->AddLine(triangle.point3);
		tmp->AddLine(triangle.point1);
		tmp->EndFigure(D2D1_FIGURE_END::D2D1_FIGURE_END_CLOSED);
		tmp->Close();
		pD2DDeviceContext->FillGeometry(geo, this->GetColorBrush(color));
		tmp->Release();
	}
	geo->Release();
}
void D2DGraphics::DrawTriangle(D2D1_TRIANGLE triangle, D2D1_COLOR_F color, float width) {
	this->DrawLine(triangle.point1, triangle.point2, color, width);
	this->DrawLine(triangle.point2, triangle.point3, color, width);
	this->DrawLine(triangle.point3, triangle.point1, color, width);
}
void D2DGraphics::FillPolygon(std::vector<D2D1_POINT_2F> points, D2D1_COLOR_F color) {
	if (points.size() > 2) {
		ID2D1PathGeometry* geo = Factory::CreateGeomtry();
		if (!geo) return;
		ID2D1GeometrySink* tmp = NULL;
		if SUCCEEDED(geo->Open(&tmp)) {
			tmp->BeginFigure(points.begin()[0], D2D1_FIGURE_BEGIN_FILLED);
			tmp->AddLines(points.data() + 1, points.size() - 1);
			tmp->EndFigure(D2D1_FIGURE_END::D2D1_FIGURE_END_CLOSED);
			tmp->Close();
			pD2DDeviceContext->FillGeometry(geo, this->GetColorBrush(color));
			tmp->Release();
		}
		geo->Release();
	}
}
void D2DGraphics::FillPolygon(std::initializer_list<D2D1_POINT_2F> points, D2D1_COLOR_F color) {
	if (points.size() > 2) {
		ID2D1PathGeometry* geo = Factory::CreateGeomtry();
		if (!geo) return;
		ID2D1GeometrySink* tmp = NULL;
		if SUCCEEDED(geo->Open(&tmp)) {
			tmp->BeginFigure(points.begin()[0], D2D1_FIGURE_BEGIN_FILLED);
			tmp->AddLines(points.begin() + 1, points.size() - 1);
			tmp->EndFigure(D2D1_FIGURE_END::D2D1_FIGURE_END_CLOSED);
			tmp->Close();
			pD2DDeviceContext->FillGeometry(geo, this->GetColorBrush(color));
			tmp->Release();
		}
		geo->Release();
	}
}
void D2DGraphics::DrawPolygon(std::initializer_list<D2D1_POINT_2F> points, D2D1_COLOR_F color, float width) {
	if (points.size() > 1) {
		ID2D1PathGeometry* geo = Factory::CreateGeomtry();
		if (!geo) return;
		ID2D1GeometrySink* tmp = NULL;
		if SUCCEEDED(geo->Open(&tmp)) {
			tmp->BeginFigure(points.begin()[0], D2D1_FIGURE_BEGIN_HOLLOW);
			tmp->AddLines(points.begin() + 1, points.size() - 1);
			tmp->EndFigure(D2D1_FIGURE_END::D2D1_FIGURE_END_OPEN);
			tmp->Close();
			pD2DDeviceContext->DrawGeometry(geo, this->GetColorBrush(color), width);
			tmp->Release();
		}
		this->DrawGeometry(geo, color, width);
		geo->Release();
	}
}
void D2DGraphics::DrawPolygon(std::vector<D2D1_POINT_2F> points, D2D1_COLOR_F color, float width) {
	if (points.size() > 1) {
		ID2D1PathGeometry* geo = Factory::CreateGeomtry();
		if (!geo) return;
		ID2D1GeometrySink* tmp = NULL;
		if SUCCEEDED(geo->Open(&tmp)) {
			tmp->BeginFigure(points[0], D2D1_FIGURE_BEGIN_HOLLOW);
			tmp->AddLines(points.data() + 1, points.size() - 1);
			tmp->EndFigure(D2D1_FIGURE_END::D2D1_FIGURE_END_OPEN);
			tmp->Close();
			pD2DDeviceContext->DrawGeometry(geo, this->GetColorBrush(color), width);
			tmp->Release();
		}
		this->DrawGeometry(geo, color, width);
		geo->Release();
	}
}
void D2DGraphics::DrawArc(D2D1_POINT_2F center, float size, float sa, float ea, D2D1_COLOR_F color, float width) {
	const auto __AngleToPoint = [](D2D1_POINT_2F Cent, float Angle, float Len) {
		return Len > 0 ? D2D1::Point2F((Cent.x + (sin(Angle * (float)M_PI / 180.0f) * Len)), (Cent.y - (cos(Angle * (float)M_PI / 180.0f) * Len))) : Cent;
		};
	ID2D1PathGeometry* geo = Factory::CreateGeomtry();
	if (!geo) return;
	float ts = sa, te = ea;
	if (te < ts) te += 360.0f;
	D2D1_ARC_SIZE sweep = (te - ts < 180.0f) ? D2D1_ARC_SIZE_SMALL : D2D1_ARC_SIZE_LARGE;
	ID2D1GeometrySink* pSink = NULL;
	if SUCCEEDED(geo->Open(&pSink)) {
		auto start = __AngleToPoint(center, sa, size),end = __AngleToPoint(center, ea, size);
		pSink->BeginFigure(start, D2D1_FIGURE_BEGIN_FILLED);
		pSink->AddArc(D2D1::ArcSegment(end, D2D1::SizeF(size, size), 0.0f, D2D1_SWEEP_DIRECTION_CLOCKWISE, sweep));
		pSink->EndFigure(D2D1_FIGURE_END::D2D1_FIGURE_END_OPEN);
		pSink->Close();
		pD2DDeviceContext->DrawGeometry(geo, this->GetColorBrush(color), width);
		pSink->Release();
	}
	geo->Release();
}
void D2DGraphics::DrawArcCounter(D2D1_POINT_2F center, float size, float sa, float ea, D2D1_COLOR_F color, float width) {
	const auto __AngleToPoint = [](D2D1_POINT_2F Cent, float Angle, float Len) {
		return Len > 0 ? D2D1::Point2F((Cent.x + (sin(Angle * (float)M_PI / 180.0f) * Len)), (Cent.y - (cos(Angle * (float)M_PI / 180.0f) * Len))) : Cent;
		};
	ID2D1PathGeometry* geo = Factory::CreateGeomtry();
	if (!geo) return;
	float ts = sa, te = ea;
	if (te < ts) te += 360.0f;
	D2D1_ARC_SIZE sweep = (te - ts < 180.0f) ? D2D1_ARC_SIZE_SMALL : D2D1_ARC_SIZE_LARGE;
	ID2D1GeometrySink* pSink = NULL;
	if SUCCEEDED(geo->Open(&pSink)) {
		auto start = __AngleToPoint(center, sa, size),end = __AngleToPoint(center, ea, size);
		pSink->BeginFigure(start, D2D1_FIGURE_BEGIN_FILLED);
		pSink->AddArc(D2D1::ArcSegment(end, D2D1::SizeF(size, size), 0.0f, D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE, sweep));
		pSink->EndFigure(D2D1_FIGURE_END::D2D1_FIGURE_END_OPEN);
		pSink->Close();
		pD2DDeviceContext->DrawGeometry(geo, this->GetColorBrush(color), width);
		pSink->Release();
	}
	geo->Release();
}
void D2DGraphics::PushDrawRect(float left, float top, float width, float height) {
	this->pD2DDeviceContext->PushAxisAlignedClip(D2D1::RectF(left, top, left + width, top + height), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
	
	
}
void D2DGraphics::PopDrawRect() {
	this->pD2DDeviceContext->PopAxisAlignedClip();
	
}
void D2DGraphics::SetAntialiasMode(D2D1_ANTIALIAS_MODE antialiasMode) {
	pD2DDeviceContext->SetAntialiasMode(antialiasMode);
}
void D2DGraphics::SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE antialiasMode) {
	pD2DDeviceContext->SetTextAntialiasMode(antialiasMode);
}
ID2D1Bitmap* D2DGraphics::CreateBitmap(IWICBitmap* wb) {
	ID2D1Bitmap* bmp = NULL;
	HRESULT hr = pD2DDeviceContext->CreateBitmapFromWicBitmap(wb, &bmp);
	return bmp;
}
ID2D1Bitmap* D2DGraphics::CreateBitmap(std::wstring path) {
	ID2D1Bitmap* bmp = NULL;
	IWICBitmapDecoder* bitmapdecoder = NULL;
	_ImageFactory->CreateDecoderFromFilename(path.c_str(), NULL, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &bitmapdecoder);
	if (bitmapdecoder) {
		IWICBitmapFrameDecode* pframe = NULL;
		bitmapdecoder->GetFrame(0, &pframe);
		IWICFormatConverter* fmtcovter = NULL;
		_ImageFactory->CreateFormatConverter(&fmtcovter);
		fmtcovter->Initialize(pframe, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, NULL, 0.0f, WICBitmapPaletteTypeCustom);
		pD2DDeviceContext->CreateBitmapFromWicBitmap(fmtcovter, NULL, &bmp);
		pframe->Release();
		fmtcovter->Release();
		bitmapdecoder->Release();
	}
	return bmp;
}
ID2D1Bitmap* D2DGraphics::CreateBitmap(void* data, int size) {
	ID2D1Bitmap* bmp = NULL;
	IWICBitmapDecoder* bitmapdecoder = NULL;
	IWICStream* pStream = NULL;
	HRESULT hr = _ImageFactory->CreateStream(&pStream);
	if FAILED(hr)
		return NULL;
	hr = pStream->InitializeFromMemory((WICInProcPointer)data, size);
	if FAILED(hr)
		return NULL;
	hr = _ImageFactory->CreateDecoderFromStream(pStream, NULL, WICDecodeMetadataCacheOnDemand, &bitmapdecoder);
	if FAILED(hr)
		return NULL;
	if (bitmapdecoder) {
		IWICBitmapFrameDecode* pframe = NULL;
		bitmapdecoder->GetFrame(0, &pframe);
		IWICFormatConverter* fmtcovter = NULL;
		_ImageFactory->CreateFormatConverter(&fmtcovter);
		fmtcovter->Initialize(pframe, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, NULL, 0.0f, WICBitmapPaletteTypeCustom);
		pD2DDeviceContext->CreateBitmapFromWicBitmap(fmtcovter, NULL, &bmp);
		pframe->Release();
		fmtcovter->Release();
		bitmapdecoder->Release();
	}
	return bmp;
}
ID2D1Bitmap* D2DGraphics::CreateBitmap(HBITMAP hb) {
	IWICBitmap* wb = NULL;
	ID2D1Bitmap* bmp = NULL;
	_ImageFactory->CreateBitmapFromHBITMAP(hb, 0, WICBitmapUsePremultipliedAlpha, &wb);
	HRESULT hr = pD2DDeviceContext->CreateBitmapFromWicBitmap(wb, &bmp);
	wb->Release();
	return bmp;
}
ID2D1Bitmap* D2DGraphics::CreateBitmap(HICON hb) {
	IWICBitmap* wb = NULL;
	ID2D1Bitmap* bmp = NULL;
	_ImageFactory->CreateBitmapFromHICON(hb, &wb);
	pD2DDeviceContext->CreateBitmapFromWicBitmap(wb, &bmp);
	wb->Release();
	return bmp;
}
ID2D1Bitmap* D2DGraphics::CreateBitmap(int width, int height) {
	IWICBitmap* wb = NULL;
	ID2D1Bitmap* bmp = NULL;
	_ImageFactory->CreateBitmap(width, height, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnDemand, &wb);
	auto hr = pD2DDeviceContext->CreateBitmapFromWicBitmap(wb, &bmp);
	wb->Release();
	return bmp;
}
ID2D1Bitmap1* D2DGraphics::GetBitmap() {
	return pD2DTargetBitmap;
}
ID2D1Bitmap* D2DGraphics::GetSharedBitmap() {
	if (!pD2DTargetBitmap)
		return NULL;
	ID2D1Bitmap* sharedBitmap = NULL;
	HRESULT hr = pD2DDeviceContext->CreateSharedBitmap(__uuidof(ID2D1Bitmap1), static_cast<void*>(pD2DTargetBitmap), NULL, &sharedBitmap);
	return sharedBitmap;
}
IWICBitmap* D2DGraphics::GetWicBitmap() {
	IWICBitmap* wic = NULL;
	HRESULT hr = Factory::ExtractID2D1Bitmap1ToIWICBitmap(pD2DDeviceContext, pD2DTargetBitmap, _ImageFactory, &wic);
	return wic;
}
ID2D1LinearGradientBrush* D2DGraphics::CreateLinearGradientBrush(D2D1_GRADIENT_STOP* stops, unsigned int stopcount) {
	ID2D1GradientStopCollection* collection = NULL;
	ID2D1LinearGradientBrush* m_LinearBrush = NULL;
	if SUCCEEDED(pD2DDeviceContext->CreateGradientStopCollection(stops, stopcount, &collection)) {
		pD2DDeviceContext->CreateLinearGradientBrush({}, collection, &m_LinearBrush);
		collection->Release();
	}
	return m_LinearBrush;
}
ID2D1RadialGradientBrush* D2DGraphics::CreateRadialGradientBrush(D2D1_GRADIENT_STOP* stops, unsigned int stopcount, D2D1_POINT_2F center) {
	ID2D1GradientStopCollection* collection = NULL;
	ID2D1RadialGradientBrush* brush = NULL;
	if SUCCEEDED(pD2DDeviceContext->CreateGradientStopCollection(stops, stopcount, &collection)) {
		D2D1_RADIAL_GRADIENT_BRUSH_PROPERTIES props = {};
		props.center = center;
		pD2DDeviceContext->CreateRadialGradientBrush(props, collection, &brush);
		collection->Release();
	}
	return brush;
}	
ID2D1BitmapBrush* D2DGraphics::CreateitmapBrush(ID2D1Bitmap* bmp)
{
	ID2D1BitmapBrush* brush = NULL;
	if SUCCEEDED(pD2DDeviceContext->CreateBitmapBrush(bmp, NULL, NULL, &brush))
		return brush;
	return NULL;
}
ID2D1SolidColorBrush* D2DGraphics::CreateSolidColorBrush(D2D1_COLOR_F color) {
	ID2D1SolidColorBrush* result = NULL;
	pD2DDeviceContext->CreateSolidColorBrush(color, &result);
	return result;
}
D2D1_SIZE_F D2DGraphics::Size() {
	return this->pD2DDeviceContext->GetSize();
}
ID2D1Bitmap* D2DGraphics::ToBitmapFromSvg(const char* data) {
	int len = strlen(data) + 1;
	char* svg_text = new char[len];
	memcpy(svg_text, data, len);
	NSVGimage* image = nsvgParse(svg_text, "px", 96.0f);
	float percen = 1.0f;
	if (image->width > 4096 || image->height > 4096) {
		float maxv = image->width > image->height ? image->width : image->height;
		percen = 4096.0f / maxv;
	}
	auto subg = new D2DGraphics(image->width * percen, image->height * percen);
	NSVGshape* shape;
	NSVGpath* path;
	subg->BeginRender();
	subg->Clear(D2D1::ColorF(0, 0, 0, 0));
	for (shape = image->shapes; shape != NULL; shape = shape->next) {
		auto geo = Factory::CreateGeomtry();
		if (geo) {
			ID2D1GeometrySink* skin = NULL;
			geo->Open(&skin);
			if (skin) {
				for (path = shape->paths; path != NULL; path = path->next) {
					for (int i = 0; i < path->npts - 1; i += 3) {
						float* p = &path->pts[i * 2];
						if (i == 0)
							skin->BeginFigure({ p[0] * percen, p[1] * percen }, D2D1_FIGURE_BEGIN_FILLED);
						skin->AddBezier({ {p[2] * percen, p[3] * percen},{p[4] * percen, p[5] * percen},{p[6] * percen, p[7] * percen} });
					}
					skin->EndFigure(path->closed ? D2D1_FIGURE_END_CLOSED : D2D1_FIGURE_END_OPEN);
				}
			}
			skin->Close();
		}
		auto _get_svg_brush = [](NSVGpaint paint, float opacity, D2DGraphics* g) ->ID2D1Brush* {
			const auto ic2fc = [](int colorInt, float opacity)->D2D1_COLOR_F {
				return D2D1_COLOR_F{ (float)GetRValue(colorInt) / 255.0f ,(float)GetGValue(colorInt) / 255.0f ,(float)GetBValue(colorInt) / 255.0f ,opacity };
				};
			switch (paint.type) {
			case NSVG_PAINT_NONE: {
				return NULL;
			}
			break;
			case NSVG_PAINT_COLOR: {
				return g->CreateSolidColorBrush(ic2fc(paint.color, opacity));
			}
			break;
			case NSVG_PAINT_LINEAR_GRADIENT: {
				std::vector<D2D1_GRADIENT_STOP> cols;
				for (int i = 0; i < paint.gradient->nstops; i++) {
					auto stop = paint.gradient->stops[i];
					cols.push_back({ stop.offset, ic2fc(stop.color, opacity) });
				}
				return g->CreateLinearGradientBrush(cols.data(), cols.size());
			}
			break;
			case NSVG_PAINT_RADIAL_GRADIENT: {
				std::vector<D2D1_GRADIENT_STOP> cols;
				for (int i = 0; i < paint.gradient->nstops; i++) {
					auto stop = paint.gradient->stops[i];
					cols.push_back({ stop.offset, ic2fc(stop.color, opacity) });
				}
				return g->CreateRadialGradientBrush(cols.data(), cols.size(), { paint.gradient->fx,paint.gradient->fy });
			}
			break;
			}
			return NULL;
			};
		ID2D1Brush* brush = _get_svg_brush(shape->fill, shape->opacity, subg);
		if (brush) {
			subg->FillGeometry(geo, brush);
			brush->Release();
		}
		brush = _get_svg_brush(shape->stroke, shape->opacity, subg);
		if (brush) {
			subg->DrawGeometry(geo, brush, shape->strokeWidth);
			brush->Release();
		}
		geo->Release();
	}
	nsvgDelete(image);
	subg->EndRender();
	auto result = (ID2D1Bitmap*)subg->GetSharedBitmap();
	delete subg;
	return result;
}

void D2DGraphics::SetTransform(D2D1_MATRIX_3X2_F matrix)
{
	pD2DDeviceContext->SetTransform(matrix);
}
void D2DGraphics::ClearTransform()
{
	pD2DDeviceContext->SetTransform(D2D1::Matrix3x2F::Identity());
}
std::vector<ID2D1Bitmap*> D2DGraphics::CreateBitmapFromGif(const char* path) {
	std::vector<ID2D1Bitmap*> result = std::vector<ID2D1Bitmap*>();
	ID2D1Bitmap* bmp = NULL;
	IWICBitmapDecoder* bitmapdecoder = NULL;
	HRESULT hr = _ImageFactory->CreateDecoderFromFilename(Convert::string_to_wstring(path).c_str(), NULL, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &bitmapdecoder);
	if FAILED(hr)
		return result;
	UINT count = 0;
	bitmapdecoder->GetFrameCount(&count);
	for (int i = 0; i < count; i++) {
		IWICBitmapFrameDecode* pframe = NULL;
		bitmapdecoder->GetFrame(i, &pframe);
		IWICFormatConverter* fmtcovter = NULL;
		_ImageFactory->CreateFormatConverter(&fmtcovter);
		fmtcovter->Initialize(pframe, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, NULL, 0.0f, WICBitmapPaletteTypeCustom);
		pD2DDeviceContext->CreateBitmapFromWicBitmap(fmtcovter, NULL, &bmp);
		pframe->Release();
		fmtcovter->Release();
		result.push_back(bmp);
	}
	bitmapdecoder->Release();
	return result;
}
std::vector<ID2D1Bitmap*> D2DGraphics::CreateBitmapFromGifBuffer(void* data, int size) {
	std::vector<ID2D1Bitmap*> result = std::vector<ID2D1Bitmap*>();
	ID2D1Bitmap* bmp = NULL;
	IWICStream* pStream = NULL;
	IWICBitmapDecoder* bitmapdecoder = NULL;
	HRESULT hr = _ImageFactory->CreateStream(&pStream);
	if FAILED(hr)
		return result;
	hr = pStream->InitializeFromMemory((WICInProcPointer)data, size);
	if FAILED(hr)
		return result;
	hr = _ImageFactory->CreateDecoderFromStream(pStream, NULL, WICDecodeMetadataCacheOnDemand, &bitmapdecoder);
	if FAILED(hr)
		return result;
	if (bitmapdecoder) {
		UINT count = 0;
		bitmapdecoder->GetFrameCount(&count);
		for (int i = 0; i < count; i++) {
			IWICBitmapFrameDecode* pframe = NULL;
			if SUCCEEDED(bitmapdecoder->GetFrame(i, &pframe)) {
				IWICFormatConverter* fmtcovter = NULL;
				if SUCCEEDED(_ImageFactory->CreateFormatConverter(&fmtcovter)) {
					if SUCCEEDED(fmtcovter->Initialize(pframe, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, NULL, 0.0f, WICBitmapPaletteTypeCustom)) {
						if SUCCEEDED(pD2DDeviceContext->CreateBitmapFromWicBitmap(fmtcovter, NULL, &bmp))
							result.push_back(bmp);
					}
					fmtcovter->Release();
				}
				pframe->Release();
			}
		}
		bitmapdecoder->Release();
	}
	return result;
}
HBITMAP D2DGraphics::CopyFromScreen(int x, int y, int width, int height) {
	HDC sourceDC = GetDC(NULL);
	HDC momDC = CreateCompatibleDC(sourceDC);
	HBITMAP memBitmap = CreateCompatibleBitmap(sourceDC, width, height);
	SelectObject(momDC, memBitmap);
	BitBlt(momDC, 0, 0, width, height, sourceDC, x, y, SRCCOPY);
	DeleteDC(momDC);
	ReleaseDC(NULL, sourceDC);
	return memBitmap;
}
HBITMAP D2DGraphics::CopyFromWidnow(HWND hWnd, int x, int y, int width, int height) {
	HDC sourceDC = GetDC(hWnd);
	HDC momDC;
	momDC = ::CreateCompatibleDC(sourceDC);
	HBITMAP memBitmap;
	memBitmap = ::CreateCompatibleBitmap(sourceDC, width, height);
	SelectObject(momDC, memBitmap);
	BitBlt(momDC, 0, 0, width, height, sourceDC, x, y, SRCCOPY);
	DeleteDC(momDC);
	ReleaseDC(hWnd, sourceDC);
	return memBitmap;
}
SIZE D2DGraphics::GetScreenSize(int index) {
	std::vector<SIZE> sizes = std::vector<SIZE>();
	auto callback = [](HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
		std::vector<SIZE>* tmp = (std::vector<SIZE>*)dwData;
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
	EnumDisplayMonitors(NULL, NULL, (MONITORENUMPROC)callback, (LPARAM)&sizes);
	if (sizes.size() > index) {
		return sizes[index];
	}
	return{ GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN) };
}

D2D1_SIZE_F D2DGraphics::GetTextLayoutSize(IDWriteTextLayout* textLayout)
{
	D2D1_SIZE_F minSize = { 0,0 };
	DWRITE_TEXT_METRICS metrics;
	HRESULT hr = textLayout->GetMetrics(&metrics);
	if SUCCEEDED(hr)
	{
		minSize = D2D1::Size((float)ceil(metrics.widthIncludingTrailingWhitespace), (float)ceil(metrics.height));
		return minSize;
	}
	return D2D1_SIZE_F{ 0,0 };
}
HRESULT HwndGraphics::InitDevice() {
	HRESULT hr = S_OK;
	RECT rc{};
	GetClientRect(hwnd, &rc);
	UINT width = rc.right - rc.left;
	UINT height = rc.bottom - rc.top;
	DXGI_SWAP_CHAIN_DESC swapChainDesc = { 0 };
	swapChainDesc.BufferDesc.Width = width;
	swapChainDesc.BufferDesc.Height = height;
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
	swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
	swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = 1;
	swapChainDesc.OutputWindow = hwnd;
	swapChainDesc.Windowed = TRUE;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	swapChainDesc.Flags = 0;
	hr = _DxgiFactory->CreateSwapChain(_D3DDevice, &swapChainDesc, &pSwapChain);
	if FAILED(hr)
		return hr;
	hr = _D2DDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &pD2DDeviceContext);
	if FAILED(hr)
		return hr;
	ReSize(width, height);
	return S_OK;
}
HwndGraphics::HwndGraphics(HWND hWnd) {
	hwnd = hWnd;
	InitDevice();
	ConfigDefaultObjects();
}
void HwndGraphics::ReSize(UINT width, UINT height) {
	HRESULT hr = S_OK;
	if (pD2DDeviceContext)
		pD2DDeviceContext->SetTarget(NULL);
	if (pD2DTargetBitmap) {
		pD2DTargetBitmap->Release();
		pD2DTargetBitmap = NULL;
	}
	RECT rec{};
	GetClientRect(hwnd, &rec);
	hr = pSwapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
	if FAILED(hr)
		return;
	IDXGISurface* dxgiBackBuffer = NULL;
	hr = pSwapChain->GetBuffer(0, IID_PPV_ARGS(&dxgiBackBuffer));
	if FAILED(hr)
		return;
	D2D1_BITMAP_PROPERTIES1 bitmapProperties = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
	hr = pD2DDeviceContext->CreateBitmapFromDxgiSurface(dxgiBackBuffer, &bitmapProperties, &pD2DTargetBitmap);
	dxgiBackBuffer->Release();
	if FAILED(hr)
		return;
	pD2DDeviceContext->SetTarget(pD2DTargetBitmap);
}
void HwndGraphics::BeginRender() {
	D2DGraphics::BeginRender();
}
void HwndGraphics::EndRender() {
	D2DGraphics::EndRender();
	pSwapChain->Present(1, 0);
}
void IDXGISwapChainGraphics::ReSize(UINT width, UINT height) {
	HRESULT hr = S_OK;
	if (pD2DDeviceContext)
		pD2DDeviceContext->SetTarget(NULL);
	if (pD2DTargetBitmap) {
		pD2DTargetBitmap->Release();
		pD2DTargetBitmap = NULL;
	}
	hr = pSwapChain->ResizeBuffers(0,width,height,DXGI_FORMAT_UNKNOWN,0);
	if FAILED(hr)
		return;
	IDXGISurface* dxgiBackBuffer = NULL;
	hr = pSwapChain->GetBuffer(0, IID_PPV_ARGS(&dxgiBackBuffer));
	if FAILED(hr)
		return;
	D2D1_BITMAP_PROPERTIES1 bitmapProperties = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
	hr = pD2DDeviceContext->CreateBitmapFromDxgiSurface(dxgiBackBuffer, &bitmapProperties, &pD2DTargetBitmap);
	dxgiBackBuffer->Release();
	if FAILED(hr)
		return;
	pD2DDeviceContext->SetTarget(pD2DTargetBitmap);
}
IDXGISwapChainGraphics::IDXGISwapChainGraphics(IDXGISwapChain* swapchain) {
	HRESULT hr = S_OK;
	pSwapChain = swapchain;
	hr = _D2DDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &pD2DDeviceContext);
	if FAILED(hr)
		return;
	DXGI_SWAP_CHAIN_DESC swapChainDesc;
	hr = pSwapChain->GetDesc(&swapChainDesc);
	if FAILED(hr)
		return;
	ReSize(swapChainDesc.BufferDesc.Width, swapChainDesc.BufferDesc.Height);
	ConfigDefaultObjects();
}
void IDXGISwapChainGraphics::BeginRender() {
	D2DGraphics::BeginRender();
}
void IDXGISwapChainGraphics::EndRender() {
	D2DGraphics::EndRender();
	pSwapChain->Present(1, 0);
}
void DxgiSurfaceGraphics::ReSize(UINT width, UINT height) {
	if (pD2DDeviceContext)
		pD2DDeviceContext->SetTarget(NULL);
	if (pD2DTargetBitmap) {
		pD2DTargetBitmap->Release();
		pD2DTargetBitmap = NULL;
	}
	D2D1_BITMAP_PROPERTIES1 bitmapProperties = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
	HRESULT hr = pD2DDeviceContext->CreateBitmapFromDxgiSurface(pDxgiSurface, &bitmapProperties, &pD2DTargetBitmap);
	if FAILED(hr)
		return;
	pD2DDeviceContext->SetTarget(pD2DTargetBitmap);
}
DxgiSurfaceGraphics::DxgiSurfaceGraphics(IDXGISurface* dxgiSurface) {
	HRESULT hr = S_OK;
	pDxgiSurface = dxgiSurface;
	pDxgiSurface->AddRef();
	hr = _D2DDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &pD2DDeviceContext);
	if FAILED(hr)
		return;
	ReSize(0, 0);
	ConfigDefaultObjects();
}
void DxgiSurfaceGraphics::BeginRender() {
	D2DGraphics::BeginRender();
}
void DxgiSurfaceGraphics::EndRender() {
	D2DGraphics::EndRender();
}