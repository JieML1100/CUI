#include "ToolBox.h"
#include "../CUI/GUI/Label.h"
#include "../CUI/GUI/Form.h"
#include "../CUI/nanosvg.h"
#include <CppUtils/Graphics/BitmapSource.h>
#include <CppUtils/Graphics/Factory.h>
#include <vector>
#include <cstring>
#include <algorithm>

extern const char* icon0;
extern const char* icon1;
extern const char* icon2;
extern const char* icon3;
extern const char* icon4;
extern const char* _4_ico;
extern const char* _9_ico;

static ID2D1Bitmap* ToBitmapFromSvg(D2DGraphics1* g, const char* data)
{
	if (!g || !data) return NULL;
	int len = (int)strlen(data) + 1;
	char* svg_text = new char[len];
	memcpy(svg_text, data, len);
	NSVGimage* image = nsvgParse(svg_text, "px", 96.0f);
	delete[] svg_text;
	if (!image) return NULL;
	float percen = 1.0f;
	if (image->width > 4096 || image->height > 4096)
	{
		float maxv = image->width > image->height ? image->width : image->height;
		percen = 4096.0f / maxv;
	}
	auto renderSource = BitmapSource::CreateEmpty(image->width * percen, image->height * percen);
	auto subg = new D2DGraphics1(renderSource.get());
	NSVGshape* shape;
	NSVGpath* path;
	subg->BeginRender();
	subg->Clear(D2D1::ColorF(0, 0, 0, 0));
	for (shape = image->shapes; shape != NULL; shape = shape->next)
	{
		auto geo = Factory::CreateGeomtry();
		if (geo)
		{
			ID2D1GeometrySink* skin = NULL;
			geo->Open(&skin);
			if (skin)
			{
				for (path = shape->paths; path != NULL; path = path->next)
				{
					for (int i = 0; i < path->npts - 1; i += 3)
					{
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
		auto _get_svg_brush = [](NSVGpaint paint, float opacity, D2DGraphics1* g) ->ID2D1Brush* {
			const auto ic2fc = [](int colorInt, float opacity)->D2D1_COLOR_F {
				return D2D1_COLOR_F{ (float)GetRValue(colorInt) / 255.0f ,(float)GetGValue(colorInt) / 255.0f ,(float)GetBValue(colorInt) / 255.0f ,opacity };
			};
			switch (paint.type)
			{
			case NSVG_PAINT_NONE:
				return NULL;
			case NSVG_PAINT_COLOR:
				return g->CreateSolidColorBrush(ic2fc(paint.color, opacity));
			case NSVG_PAINT_LINEAR_GRADIENT:
			{
				std::vector<D2D1_GRADIENT_STOP> cols;
				for (int i = 0; i < paint.gradient->nstops; i++)
				{
					auto stop = paint.gradient->stops[i];
					cols.push_back({ stop.offset, ic2fc(stop.color, opacity) });
				}
				return g->CreateLinearGradientBrush(cols.data(), cols.size());
			}
			case NSVG_PAINT_RADIAL_GRADIENT:
			{
				std::vector<D2D1_GRADIENT_STOP> cols;
				for (int i = 0; i < paint.gradient->nstops; i++)
				{
					auto stop = paint.gradient->stops[i];
					cols.push_back({ stop.offset, ic2fc(stop.color, opacity) });
				}
				return g->CreateRadialGradientBrush(cols.data(), cols.size(), { paint.gradient->fx,paint.gradient->fy });
			}
			}
			return NULL;
		};
		ID2D1Brush* brush = _get_svg_brush(shape->fill, shape->opacity, subg);
		if (brush)
		{
			subg->FillGeometry(geo, brush);
			brush->Release();
		}
		brush = _get_svg_brush(shape->stroke, shape->opacity, subg);
		if (brush)
		{
			subg->DrawGeometry(geo, brush, shape->strokeWidth);
			brush->Release();
		}
		geo->Release();
	}
	nsvgDelete(image);
	subg->EndRender();

	auto result = g->CreateBitmap(renderSource);
	renderSource->GetWicBitmap()->Release();
	delete subg;
	return result;
}

static const char* GetToolBoxSvg(UIClass type)
{
	switch (type)
	{
	case UIClass::UI_Button:
		return icon1;
	case UIClass::UI_Label:
		return icon2;
	case UIClass::UI_TextBox:
	case UIClass::UI_PasswordBox:
		return icon0;
	case UIClass::UI_PictureBox:
		return _4_ico;
	case UIClass::UI_GridView:
		return icon4;
	case UIClass::UI_WebBrowser:
		return _9_ico;
	default:
		return icon3;
	}
}

ToolBoxItem::~ToolBoxItem()
{
	if (_iconBitmap)
	{
		_iconBitmap->Release();
		_iconBitmap = nullptr;
	}
}

void ToolBoxItem::EnsureIcon()
{
	if (_iconBitmap || !this->ParentForm || !this->ParentForm->Render) return;
	if (!SvgData) return;
	_iconBitmap = ToBitmapFromSvg(this->ParentForm->Render, SvgData);
}

void ToolBoxItem::Update()
{
	if (!this->IsVisual) return;
	EnsureIcon();
	bool isUnderMouse = this->ParentForm->UnderMouse == this;
	bool isSelected = this->ParentForm->Selected == this;
	auto d2d = this->ParentForm->Render;
	auto abslocation = this->AbsLocation;
	auto size = this->ActualSize();
	auto absRect = this->AbsRect;

	d2d->PushDrawRect(absRect.left, absRect.top, absRect.right - absRect.left, absRect.bottom - absRect.top);
	{
		float roundVal = this->Height * Round;
		d2d->FillRoundRect(abslocation.x + (this->Boder * 0.5f), abslocation.y + (this->Boder * 0.5f), size.cx - this->Boder, size.cy - this->Boder, this->BackColor, roundVal);
		D2D1::ColorF color = isUnderMouse ? (isSelected ? D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.7f) : D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.4f)) : D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f);
		d2d->FillRoundRect(abslocation.x, abslocation.y, size.cx, size.cy, color, roundVal);

		float paddingLeft = 8.0f;
		float gap = 8.0f;
		float iconSize = 20.0f;
		float iconLeft = (float)abslocation.x + paddingLeft;
		float iconTop = (float)abslocation.y + ((float)size.cy - iconSize) / 2.0f;
		if (_iconBitmap)
		{
			d2d->DrawBitmap(_iconBitmap, iconLeft, iconTop, iconSize, iconSize);
		}

		auto font = this->Font;
		auto textSize = font->GetTextSize(this->Text);
		float textLeft = (float)abslocation.x + paddingLeft + iconSize + gap;
		float textTop = (float)abslocation.y + (((float)size.cy - textSize.height) / 2.0f);
		d2d->DrawString(this->Text, textLeft, textTop, this->ForeColor, this->Font);

		d2d->DrawRoundRect(abslocation.x + (this->Boder * 0.5f), abslocation.y + (this->Boder * 0.5f),
			size.cx - this->Boder, size.cy - this->Boder,
			this->BolderColor, this->Boder, roundVal);
	}

	if (!this->Enable)
		d2d->FillRect(abslocation.x, abslocation.y, size.cx, size.cy, { 1.0f ,1.0f ,1.0f ,0.5f });
	d2d->PopDrawRect();
}

ToolBox::ToolBox(int x, int y, int width, int height)
	: Panel(x, y, width, height)
{
	this->BackColor = D2D1::ColorF(0.95f, 0.95f, 0.95f, 1.0f);
	this->Boder = 1.0f;
	
	// 标题
	this->_titleLabel = new Label(L"工具箱", 10, 10);
	this->_titleLabel->Size = {width - 20, 25};
	this->_titleLabel->Font = new ::Font(L"Microsoft YaHei", 16.0f);
	this->AddControl(_titleLabel);
	
	// 获取可用控件
	auto controls = ControlRegistry::GetAvailableControls();
	int yOffset = _contentTop;
	
	for (const auto& ctrl : controls)
	{
		auto item = new ToolBoxItem(ctrl.DisplayName, ctrl.Type, GetToolBoxSvg(ctrl.Type), 10, yOffset, width - 20, 34);
		item->Round = 0.18f;
		
		// 点击事件
		item->OnMouseClick += [this, ctrl](Control* sender, MouseEventArgs e) {
			OnControlSelected(ctrl.Type);
		};
		
		this->AddControl(item);
		_items.push_back(item);
		
		yOffset += 39;
	}

	UpdateScrollLayout();
}

ToolBox::~ToolBox()
{
}

void ToolBox::UpdateScrollLayout()
{
	// contentHeight: 从 contentTop 到最后一个 item 的底部，再加 padding
	int maxBottom = _contentTop;
	for (auto* item : _items)
	{
		if (!item) continue;
		maxBottom = std::max(maxBottom, item->BaseY + item->Height);
	}
	_contentHeight = (maxBottom - _contentTop) + _contentBottomPadding;
	ClampScroll();

	for (auto* item : _items)
	{
		if (!item) continue;
		item->Top = item->BaseY - _scrollOffsetY;
	}
}

void ToolBox::ClampScroll()
{
	int viewport = this->Height - _contentTop;
	if (viewport < 0) viewport = 0;
	int maxScroll = std::max(0, _contentHeight - viewport);
	_scrollOffsetY = std::clamp(_scrollOffsetY, 0, maxScroll);
}

bool ToolBox::TryGetScrollBarLocalRect(D2D1_RECT_F& outTrack, D2D1_RECT_F& outThumb)
{
	const float trackWidth = 10.0f;
	const float trackPad = 2.0f;
	float viewport = (float)std::max(0, this->Height - _contentTop);
	if (_contentHeight <= 0 || viewport <= 0.0f) return false;
	if ((float)_contentHeight <= viewport) return false;

	outTrack = D2D1_RECT_F{
		(float)this->Width - trackWidth - trackPad,
		(float)_contentTop,
		(float)this->Width - trackPad,
		(float)this->Height - trackPad,
	};

	float trackHeight = std::max(0.0f, outTrack.bottom - outTrack.top);
	if (trackHeight <= 0.0f) return false;

	float ratio = viewport / (float)_contentHeight;
	float thumbHeight = std::max(16.0f, trackHeight * ratio);
	float maxScroll = (float)std::max(1, _contentHeight - (int)viewport);
	float scroll01 = (float)_scrollOffsetY / maxScroll;
	float thumbTop = outTrack.top + (trackHeight - thumbHeight) * scroll01;

	outThumb = D2D1_RECT_F{ outTrack.left, thumbTop, outTrack.right, thumbTop + thumbHeight };
	return true;
}

void ToolBox::Update()
{
	UpdateScrollLayout();
	Panel::Update();

	if (!this->ParentForm || !this->ParentForm->Render) return;
	D2D1_RECT_F track{}, thumb{};
	if (!TryGetScrollBarLocalRect(track, thumb)) return;

	auto d2d = this->ParentForm->Render;
	auto abs = this->AbsLocation;
	auto absRect = this->AbsRect;
	d2d->PushDrawRect(absRect.left, absRect.top, absRect.right - absRect.left, absRect.bottom - absRect.top);
	{
		D2D1_RECT_F atrack{ track.left + abs.x, track.top + abs.y, track.right + abs.x, track.bottom + abs.y };
		D2D1_RECT_F athumb{ thumb.left + abs.x, thumb.top + abs.y, thumb.right + abs.x, thumb.bottom + abs.y };
		d2d->FillRect(atrack.left, atrack.top, atrack.right - atrack.left, atrack.bottom - atrack.top, Colors::LightGray);
		d2d->FillRect(athumb.left, athumb.top, athumb.right - athumb.left, athumb.bottom - athumb.top, Colors::DimGrey);
	}
	d2d->PopDrawRect();
}

bool ToolBox::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof)
{
	// 先走 Panel 分发，让子控件（按钮）能正常点击
	Panel::ProcessMessage(message, wParam, lParam, xof, yof);

	switch (message)
	{
	case WM_MOUSEWHEEL:
	{
		int viewport = this->Height - _contentTop;
		if (_contentHeight > viewport)
		{
			// wheel delta: 120 per notch
			int delta = GET_WHEEL_DELTA_WPARAM(wParam);
			int step = 39; // 与 item 间距一致
			_scrollOffsetY -= (delta / 120) * step;
			ClampScroll();
			UpdateScrollLayout();
			this->PostRender();
		}
		return true;
	}
	case WM_LBUTTONDOWN:
	{
		D2D1_RECT_F track{}, thumb{};
		if (TryGetScrollBarLocalRect(track, thumb))
		{
			if (xof >= (int)track.left && xof <= (int)track.right && yof >= (int)track.top && yof <= (int)track.bottom)
			{
				// 点击滚动条区域
				if (yof >= (int)thumb.top && yof <= (int)thumb.bottom)
				{
					_draggingScrollThumb = true;
					_dragStartMouseY = yof;
					_dragStartScrollY = _scrollOffsetY;
					if (this->ParentForm) this->ParentForm->Selected = this;
					return true;
				}
				else
				{
					// 点击轨道：按页滚动
					int viewport = this->Height - _contentTop;
					if (yof < (int)thumb.top) _scrollOffsetY -= viewport;
					else _scrollOffsetY += viewport;
					ClampScroll();
					UpdateScrollLayout();
					this->PostRender();
					return true;
				}
			}
		}
	}
	break;
	case WM_MOUSEMOVE:
	{
		if (_draggingScrollThumb)
		{
			D2D1_RECT_F track{}, thumb{};
			if (TryGetScrollBarLocalRect(track, thumb))
			{
				float viewport = (float)std::max(0, this->Height - _contentTop);
				float trackHeight = std::max(1.0f, track.bottom - track.top);
				float thumbHeight = std::max(1.0f, thumb.bottom - thumb.top);
				float maxThumbMove = std::max(1.0f, trackHeight - thumbHeight);
				float maxScroll = (float)std::max(1, _contentHeight - (int)viewport);
				float dy = (float)(yof - _dragStartMouseY);
				float scrollDy = dy / maxThumbMove * maxScroll;
				_scrollOffsetY = (int)((float)_dragStartScrollY + scrollDy);
				ClampScroll();
				UpdateScrollLayout();
				this->PostRender();
				return true;
			}
		}
	}
	break;
	case WM_LBUTTONUP:
	{
		if (_draggingScrollThumb)
		{
			_draggingScrollThumb = false;
			if (this->ParentForm && this->ParentForm->Selected == this) this->ParentForm->Selected = nullptr;
			return true;
		}
	}
	break;
	}
	return true;
}
