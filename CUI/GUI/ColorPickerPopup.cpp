#define NOMINMAX
#include "ColorPickerPopup.h"
#include "Form.h"

#include <algorithm>
#include <cmath>
#include <cwctype>
#include <iomanip>
#include <sstream>
#include <wrl/client.h>

namespace
{
	static std::vector<std::wstring> g_colorHistory;

	static float RectWidth(const D2D1_RECT_F& rect)
	{
		return rect.right - rect.left;
	}

	static float RectHeight(const D2D1_RECT_F& rect)
	{
		return rect.bottom - rect.top;
	}

	static bool PtInRectF(const D2D1_RECT_F& rect, float x, float y)
	{
		return x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom;
	}

	static std::wstring Trim(std::wstring s)
	{
		while (!s.empty() && iswspace(s.front())) s.erase(s.begin());
		while (!s.empty() && iswspace(s.back())) s.pop_back();
		return s;
	}

	static std::wstring Lower(std::wstring s)
	{
		for (auto& ch : s) ch = (wchar_t)towlower(ch);
		return s;
	}

	static int HexValue(wchar_t ch)
	{
		if (ch >= L'0' && ch <= L'9') return ch - L'0';
		if (ch >= L'a' && ch <= L'f') return 10 + ch - L'a';
		if (ch >= L'A' && ch <= L'F') return 10 + ch - L'A';
		return -1;
	}

	static int ToByte(float v)
	{
		return (int)std::round(std::clamp(v, 0.0f, 1.0f) * 255.0f);
	}

	static D2D1_COLOR_F HsvToRgb(float h, float s, float v, float a)
	{
		h = std::fmod(h, 360.0f);
		if (h < 0.0f) h += 360.0f;
		s = std::clamp(s, 0.0f, 1.0f);
		v = std::clamp(v, 0.0f, 1.0f);
		float c = v * s;
		float x = c * (1.0f - std::fabs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
		float m = v - c;
		float r = 0.0f, g = 0.0f, b = 0.0f;
		if (h < 60.0f) { r = c; g = x; }
		else if (h < 120.0f) { r = x; g = c; }
		else if (h < 180.0f) { g = c; b = x; }
		else if (h < 240.0f) { g = x; b = c; }
		else if (h < 300.0f) { r = x; b = c; }
		else { r = c; b = x; }
		return D2D1_COLOR_F{ r + m, g + m, b + m, std::clamp(a, 0.0f, 1.0f) };
	}

	static void RgbToHsv(D2D1_COLOR_F color, float& h, float& s, float& v)
	{
		float r = std::clamp(color.r, 0.0f, 1.0f);
		float g = std::clamp(color.g, 0.0f, 1.0f);
		float b = std::clamp(color.b, 0.0f, 1.0f);
		float maxv = std::max(r, std::max(g, b));
		float minv = std::min(r, std::min(g, b));
		float d = maxv - minv;
		v = maxv;
		s = maxv <= 0.0f ? 0.0f : d / maxv;
		if (d <= 0.0001f) h = 0.0f;
		else if (maxv == r) h = 60.0f * std::fmod(((g - b) / d), 6.0f);
		else if (maxv == g) h = 60.0f * (((b - r) / d) + 2.0f);
		else h = 60.0f * (((r - g) / d) + 4.0f);
		if (h < 0.0f) h += 360.0f;
	}

	static D2D1_COLOR_F WithAlpha(D2D1_COLOR_F color, float alpha)
	{
		color.a = alpha;
		return color;
	}

	static float EaseOutCubic(float t)
	{
		t = std::clamp(t, 0.0f, 1.0f);
		return 1.0f - std::pow(1.0f - t, 3.0f);
	}

	static SIZE GetTopLevelContentSize(Form* form)
	{
		if (!form) return SIZE{ 1, 1 };
		if (form->Handle)
		{
			RECT rc{};
			if (::GetClientRect(form->Handle, &rc))
			{
				float scale = form->GetDpiScale();
				if (scale <= 0.0f) scale = 1.0f;
				const int contentW = (int)std::floor((float)(rc.right - rc.left) / scale);
				const int contentH = (int)std::floor((float)std::max<LONG>(0, rc.bottom - rc.top - form->ClientTop()) / scale);
				return SIZE{ std::max(1, contentW), std::max(1, contentH) };
			}
		}
		auto fallback = form->ClientSize;
		return SIZE{ std::max<LONG>(1, fallback.cx), std::max<LONG>(1, fallback.cy) };
	}

	static SIZE FitPopupSizeToWindow(Form* form, SIZE preferred)
	{
		auto content = GetTopLevelContentSize(form);
		return SIZE{
			std::min((int)preferred.cx, std::max(1, (int)content.cx - 4)),
			std::min((int)preferred.cy, std::max(1, (int)content.cy - 4))
		};
	}
}

ColorPickerPopup::ColorPickerPopup(int width, int height)
{
	_preferredSize = SIZE{ width, height };
	this->Location = POINT{ 0,0 };
	this->Size = SIZE{ width, height };
	this->BackColor = D2D1_COLOR_F{ 0,0,0,0 };
	this->BorderColor = D2D1_COLOR_F{ 0,0,0,0 };
	this->Visible = false;
	this->Cursor = CursorKind::Arrow;
}

std::vector<std::wstring> ColorPickerPopup::CommonColorValues()
{
	return {
		L"#EF4444", L"#F59E0B", L"#EAB308", L"#22C55E", L"#14B8A6",
		L"#2563EB", L"#C026D3", L"#C46B3A", L"#F97316", L"#FACC15"
	};
}

std::vector<std::wstring> ColorPickerPopup::HistoryColorValues()
{
	return g_colorHistory;
}

bool ColorPickerPopup::TryParseColor(const std::wstring& text, D2D1_COLOR_F& out)
{
	std::wstring s = Lower(Trim(text));
	if (s.empty()) return false;
	if (s[0] == L'#')
	{
		s.erase(s.begin());
		if (s.size() != 6 && s.size() != 8) return false;
		int values[8]{};
		for (size_t i = 0; i < s.size(); i++)
		{
			values[i] = HexValue(s[i]);
			if (values[i] < 0) return false;
		}
		auto byteAt = [&](int i) { return values[i] * 16 + values[i + 1]; };
		int offset = s.size() == 8 ? 2 : 0;
		float a = s.size() == 8 ? byteAt(0) / 255.0f : 1.0f;
		out = D2D1_COLOR_F{ byteAt(offset) / 255.0f, byteAt(offset + 2) / 255.0f, byteAt(offset + 4) / 255.0f, a };
		return true;
	}
	auto parseArgs = [&](const std::wstring& prefix, std::vector<float>& values) {
		if (s.rfind(prefix, 0) != 0 || s.back() != L')') return false;
		std::wstring inner = s.substr(prefix.size(), s.size() - prefix.size() - 1);
		std::wstringstream ss(inner);
		std::wstring part;
		while (std::getline(ss, part, L','))
		{
			try { values.push_back(std::stof(Trim(part))); }
			catch (...) { return false; }
		}
		return true;
		};
	std::vector<float> values;
	if (parseArgs(L"rgba(", values) && values.size() == 4)
	{
		out = D2D1_COLOR_F{
			std::clamp(values[0] / 255.0f, 0.0f, 1.0f),
			std::clamp(values[1] / 255.0f, 0.0f, 1.0f),
			std::clamp(values[2] / 255.0f, 0.0f, 1.0f),
			std::clamp(values[3], 0.0f, 1.0f)
		};
		return true;
	}
	values.clear();
	if (parseArgs(L"rgb(", values) && values.size() == 3)
	{
		out = D2D1_COLOR_F{
			std::clamp(values[0] / 255.0f, 0.0f, 1.0f),
			std::clamp(values[1] / 255.0f, 0.0f, 1.0f),
			std::clamp(values[2] / 255.0f, 0.0f, 1.0f),
			1.0f
		};
		return true;
	}
	return false;
}

std::wstring ColorPickerPopup::ColorToString(D2D1_COLOR_F color)
{
	int r = ToByte(color.r);
	int g = ToByte(color.g);
	int b = ToByte(color.b);
	int alpha = ToByte(color.a);
	std::wostringstream ss;
	ss << L"#" << std::uppercase << std::hex << std::setfill(L'0');
	if (color.a < 0.995f)
	{
		ss << std::setw(2) << alpha;
	}
	ss << std::setw(2) << r << std::setw(2) << g << std::setw(2) << b;
	return ss.str();
}

std::wstring ColorPickerPopup::CurrentValueText() const
{
	return ColorToString(this->SelectedColor);
}

ColorPickerPopup::Layout ColorPickerPopup::CalcLayout() const
{
	Layout layout{};
	const float w = (float)this->_size.cx;
	const float h = (float)this->_size.cy;
	const float pad = 16.0f;
	const float hueW = 18.0f;
	layout.SvRect = D2D1::RectF(pad, pad, std::max(pad + 40.0f, w - pad - hueW - 14.0f), std::max(pad + 80.0f, h - 140.0f));
	layout.HueRect = D2D1::RectF(layout.SvRect.right + 12.0f, layout.SvRect.top, layout.SvRect.right + 12.0f + hueW, layout.SvRect.bottom);
	layout.AlphaRect = D2D1::RectF(pad, layout.SvRect.bottom + 12.0f, layout.HueRect.right, layout.SvRect.bottom + 30.0f);
	float swatchY = layout.AlphaRect.bottom + 12.0f;
	auto common = CommonColorValues();
	int swatchCols = std::max(1, (int)std::floor((w - pad * 2.0f + 12.0f) / 42.0f));
	for (int i = 0; i < (int)common.size(); i++)
	{
		int row = i / swatchCols;
		int col = i % swatchCols;
		float x = pad + col * 42.0f;
		float y = swatchY + row * 40.0f;
		layout.CommonRects.push_back(D2D1::RectF(x, y, x + 30.0f, y + 30.0f));
	}
	float historyY = swatchY + ((int)common.size() + swatchCols - 1) / swatchCols * 40.0f;
	int historyCount = std::min(10, (int)g_colorHistory.size());
	for (int i = 0; i < historyCount; i++)
	{
		int row = i / swatchCols;
		int col = i % swatchCols;
		float x = pad + col * 42.0f;
		float y = historyY + row * 40.0f;
		layout.HistoryRects.push_back(D2D1::RectF(x, y, x + 30.0f, y + 30.0f));
	}
	float bottomY = h - 48.0f;
	layout.OkRect = D2D1::RectF(std::max(pad, w - 70.0f), bottomY, std::max(pad + 54.0f, w - 16.0f), bottomY + 34.0f);
	layout.ClearRect = D2D1::RectF(std::max(pad, layout.OkRect.left - 72.0f), bottomY, std::max(pad + 60.0f, layout.OkRect.left - 12.0f), bottomY + 34.0f);
	float inputRight = std::min(pad + 240.0f, layout.ClearRect.left - 16.0f);
	layout.InputRect = D2D1::RectF(pad, bottomY, std::max(pad + 80.0f, inputRight), bottomY + 34.0f);
	return layout;
}

void ColorPickerPopup::SetFromColor(D2D1_COLOR_F color)
{
	color.r = std::clamp(color.r, 0.0f, 1.0f);
	color.g = std::clamp(color.g, 0.0f, 1.0f);
	color.b = std::clamp(color.b, 0.0f, 1.0f);
	color.a = std::clamp(color.a, 0.0f, 1.0f);
	this->SelectedColor = color;
	RgbToHsv(color, _hue, _saturation, _value);
	_alpha = color.a;
}

void ColorPickerPopup::UpdateColorFromHsv()
{
	this->SelectedColor = HsvToRgb(_hue, _saturation, _value, _alpha);
	this->OnColorChanged(this, this->SelectedColor, CurrentValueText());
	this->InvalidateVisual();
}

float ColorPickerPopup::CurrentDropProgress()
{
	if (!_animating)
	{
		_dropProgress = _expanded ? 1.0f : 0.0f;
		return _dropProgress;
	}

	const UINT64 now = GetTickCount64();
	const UINT64 elapsed = now >= _animStartTick ? (now - _animStartTick) : 0;
	float t = _animDurationMs > 0 ? (float)elapsed / (float)_animDurationMs : 1.0f;
	if (t >= 1.0f)
	{
		const bool wasCollapsing = _animTargetProgress <= 0.001f;
		_dropProgress = _animTargetProgress;
		_animating = false;
		if (wasCollapsing)
			FinishCollapsed();
		return _dropProgress;
	}

	t = EaseOutCubic(t);
	_dropProgress = _animStartProgress + (_animTargetProgress - _animStartProgress) * t;
	return _dropProgress;
}

void ColorPickerPopup::SetExpanded(bool expanded)
{
	const bool wantExpand = expanded;
	CurrentDropProgress();
	_expanded = wantExpand;
	_animStartProgress = _dropProgress;
	_animTargetProgress = wantExpand ? 1.0f : 0.0f;
	_collapseCleanupPending = !wantExpand;
	if (std::fabs(_animTargetProgress - _animStartProgress) < 0.001f)
	{
		_dropProgress = _animTargetProgress;
		_animating = false;
		if (!wantExpand)
			FinishCollapsed();
	}
	else
	{
		_animStartTick = GetTickCount64();
		_animating = true;
	}
	if (this->ParentForm)
		this->ParentForm->Invalidate(true);
	this->InvalidateVisual();
}

void ColorPickerPopup::FinishCollapsed()
{
	Form* form = this->ParentForm;
	Control* owner = this->_owner;
	_dropProgress = 0.0f;
	_animating = false;
	_collapseCleanupPending = false;
	_expanded = false;
	_visiblePopup = false;
	this->Visible = false;
	_dragSV = _dragHue = _dragAlpha = false;
	_hasAnchorRect = false;
	if (form && form->ForegroundControl == this)
		form->ForegroundControl = nullptr;
	if (form && form->Selected == this)
		form->SetSelectedControl(owner && owner->IsVisual ? owner : nullptr, false);
	this->_owner = nullptr;
	if (form)
		form->Invalidate(true);
}

bool ColorPickerPopup::IsAnimationRunning()
{
	CurrentDropProgress();
	return _animating || _collapseCleanupPending;
}

bool ColorPickerPopup::GetAnimatedInvalidRect(D2D1_RECT_F& outRect)
{
	if (!this->Visible && !_collapseCleanupPending) return false;
	outRect = this->AbsRect;
	if (_hasAnchorRect)
	{
		auto loc = this->AbsLocation;
		D2D1_RECT_F anchorAbs = D2D1::RectF(
			(float)loc.x + _anchorRect.left,
			(float)loc.y + _anchorRect.top,
			(float)loc.x + _anchorRect.right,
			(float)loc.y + _anchorRect.bottom);
		outRect.left = std::min(outRect.left, anchorAbs.left);
		outRect.top = std::min(outRect.top, anchorAbs.top);
		outRect.right = std::max(outRect.right, anchorAbs.right);
		outRect.bottom = std::max(outRect.bottom, anchorAbs.bottom);
	}
	return true;
}

bool ColorPickerPopup::ContainsPoint(int localX, int localY)
{
	if (Control::ContainsPoint(localX, localY))
		return true;
	return _hasAnchorRect && PtInRectF(_anchorRect, (float)localX, (float)localY);
}

void ColorPickerPopup::ShowAt(Form* form, int x, int y, D2D1_COLOR_F initialColor)
{
	if (!form) return;
	this->_owner = nullptr;
	this->ParentForm = form;
	this->Visible = true;
	_visiblePopup = true;
	_hasAnchorRect = false;
	SetFromColor(initialColor);
	SIZE client = GetTopLevelContentSize(form);
	int clientW = (int)client.cx;
	int clientH = (int)client.cy;
	this->Size = FitPopupSizeToWindow(form, _preferredSize);
	int minX = clientW > this->Width + 4 ? 2 : 0;
	int minY = clientH > this->Height + 4 ? 2 : 0;
	int maxX = std::max(minX, clientW - this->Width - minX);
	int maxY = std::max(minY, clientH - this->Height - minY);
	int px = std::clamp(x, minX, maxX);
	int py = std::clamp(y, minY, maxY);
	this->SetRuntimeLocation(POINT{ px, py });
	if (form->ForegroundControl && form->ForegroundControl != this && form->ForegroundControl->AutoCloseOnOutsideClick())
		form->ForegroundControl->ClosePopup();
	form->ForegroundControl = this;
	form->SetSelectedControl(this, false);
	SetExpanded(true);
}

void ColorPickerPopup::ShowAt(Control* relativeTo, const D2D1_RECT_F& anchorRect, D2D1_COLOR_F initialColor)
{
	if (!relativeTo || !relativeTo->ParentForm) return;
	auto popupSize = FitPopupSizeToWindow(relativeTo->ParentForm, _preferredSize);
	auto abs = relativeTo->AbsLocation;
	int x = (int)std::round((float)abs.x + anchorRect.left);
	int yBelow = (int)std::round((float)abs.y + anchorRect.bottom + 4.0f);
	int yAbove = (int)std::round((float)abs.y + anchorRect.top - popupSize.cy - 4.0f);
	SIZE client = GetTopLevelContentSize(relativeTo->ParentForm);
	int y = (yBelow + popupSize.cy <= client.cy - 2 || yAbove < 2) ? yBelow : yAbove;
	ShowAt(relativeTo->ParentForm, x, y, initialColor);
	this->_owner = relativeTo;
	auto loc = this->ActualLocation;
	_anchorRect = D2D1::RectF(
		(float)abs.x + anchorRect.left - (float)loc.x,
		(float)abs.y + anchorRect.top - (float)loc.y,
		(float)abs.x + anchorRect.right - (float)loc.x,
		(float)abs.y + anchorRect.bottom - (float)loc.y);
	_hasAnchorRect = true;
}

void ColorPickerPopup::Hide(bool confirm)
{
	if (!_visiblePopup && !this->Visible && !_animating) return;
	if (!_expanded && _animating && _animTargetProgress <= 0.001f) return;
	if (confirm)
		Confirm();
	else
		this->OnCancelled(this);
	SetExpanded(false);
}

void ColorPickerPopup::Confirm()
{
	std::wstring value = CurrentValueText();
	AddHistory(value);
	this->OnColorConfirmed(this, this->SelectedColor, value);
}

void ColorPickerPopup::ClearValue()
{
	this->OnCleared(this);
	SetExpanded(false);
}

void ColorPickerPopup::AddHistory(const std::wstring& value)
{
	if (value.empty()) return;
	auto lower = Lower(value);
	g_colorHistory.erase(
		std::remove_if(g_colorHistory.begin(), g_colorHistory.end(),
			[&](const std::wstring& item) { return Lower(item) == lower; }),
		g_colorHistory.end());
	g_colorHistory.insert(g_colorHistory.begin(), value);
	if (g_colorHistory.size() > 10)
		g_colorHistory.resize(10);
}

void ColorPickerPopup::SetSVFromPoint(int localX, int localY)
{
	auto layout = CalcLayout();
	_saturation = std::clamp(((float)localX - layout.SvRect.left) / std::max(1.0f, RectWidth(layout.SvRect)), 0.0f, 1.0f);
	_value = 1.0f - std::clamp(((float)localY - layout.SvRect.top) / std::max(1.0f, RectHeight(layout.SvRect)), 0.0f, 1.0f);
	UpdateColorFromHsv();
}

void ColorPickerPopup::SetHueFromPoint(int localX, int localY)
{
	(void)localX;
	auto layout = CalcLayout();
	float t = std::clamp(((float)localY - layout.HueRect.top) / std::max(1.0f, RectHeight(layout.HueRect)), 0.0f, 1.0f);
	_hue = t * 360.0f;
	UpdateColorFromHsv();
}

void ColorPickerPopup::SetAlphaFromPoint(int localX, int localY)
{
	(void)localY;
	auto layout = CalcLayout();
	_alpha = std::clamp(((float)localX - layout.AlphaRect.left) / std::max(1.0f, RectWidth(layout.AlphaRect)), 0.0f, 1.0f);
	UpdateColorFromHsv();
}

void ColorPickerPopup::UpdateHover(int localX, int localY)
{
	auto layout = CalcLayout();
	_hoverCommon = -1;
	_hoverHistory = -1;
	_hoverClear = PtInRectF(layout.ClearRect, (float)localX, (float)localY);
	_hoverOk = PtInRectF(layout.OkRect, (float)localX, (float)localY);
	for (int i = 0; i < (int)layout.CommonRects.size(); i++)
		if (PtInRectF(layout.CommonRects[i], (float)localX, (float)localY)) _hoverCommon = i;
	for (int i = 0; i < (int)layout.HistoryRects.size(); i++)
		if (PtInRectF(layout.HistoryRects[i], (float)localX, (float)localY)) _hoverHistory = i;
}

CursorKind ColorPickerPopup::QueryCursor(int localX, int localY)
{
	const bool inPanel = Control::ContainsPoint(localX, localY);
	if (!inPanel && _hasAnchorRect && PtInRectF(_anchorRect, (float)localX, (float)localY))
		return CursorKind::Hand;
	auto layout = CalcLayout();
	if (PtInRectF(layout.SvRect, (float)localX, (float)localY) ||
		PtInRectF(layout.HueRect, (float)localX, (float)localY) ||
		PtInRectF(layout.AlphaRect, (float)localX, (float)localY) ||
		PtInRectF(layout.ClearRect, (float)localX, (float)localY) ||
		PtInRectF(layout.OkRect, (float)localX, (float)localY))
		return CursorKind::Hand;
	for (auto& r : layout.CommonRects)
		if (PtInRectF(r, (float)localX, (float)localY)) return CursorKind::Hand;
	for (auto& r : layout.HistoryRects)
		if (PtInRectF(r, (float)localX, (float)localY)) return CursorKind::Hand;
	return CursorKind::Arrow;
}

void ColorPickerPopup::DrawCheckerBoard(D2DGraphics* d2d, const D2D1_RECT_F& rect, float cellSize) const
{
	D2D1_COLOR_F a = D2D1_COLOR_F{ 0.22f, 0.22f, 0.23f, 1.0f };
	D2D1_COLOR_F b = D2D1_COLOR_F{ 0.34f, 0.34f, 0.35f, 1.0f };
	for (float y = rect.top; y < rect.bottom; y += cellSize)
	{
		for (float x = rect.left; x < rect.right; x += cellSize)
		{
			int ix = (int)((x - rect.left) / cellSize);
			int iy = (int)((y - rect.top) / cellSize);
			d2d->FillRect(D2D1::RectF(x, y, std::min(x + cellSize, rect.right), std::min(y + cellSize, rect.bottom)), ((ix + iy) % 2) ? a : b);
		}
	}
}

void ColorPickerPopup::DrawSwatch(D2DGraphics* d2d, const D2D1_RECT_F& rect, D2D1_COLOR_F color, bool selected, bool hover) const
{
	if (hover)
		d2d->FillRoundRect(D2D1::RectF(rect.left - 3.0f, rect.top - 3.0f, rect.right + 3.0f, rect.bottom + 3.0f),
			D2D1_COLOR_F{ 1,1,1,0.10f }, 5.0f);
	DrawCheckerBoard(d2d, rect, 7.0f);
	d2d->FillRoundRect(rect, color, 5.0f);
	D2D1_COLOR_F border = selected ? Colors::White : D2D1_COLOR_F{ 0,0,0,0.32f };
	d2d->DrawRoundRect(rect, border, selected ? 2.0f : 1.0f, 5.0f);
}

void ColorPickerPopup::DrawSV(D2DGraphics* d2d, const D2D1_RECT_F& rect) const
{
	const float step = 4.0f;
	for (float y = rect.top; y < rect.bottom; y += step)
	{
		float v = 1.0f - std::clamp((y - rect.top) / std::max(1.0f, RectHeight(rect)), 0.0f, 1.0f);
		for (float x = rect.left; x < rect.right; x += step)
		{
			float s = std::clamp((x - rect.left) / std::max(1.0f, RectWidth(rect)), 0.0f, 1.0f);
			d2d->FillRect(D2D1::RectF(x, y, std::min(x + step, rect.right), std::min(y + step, rect.bottom)),
				HsvToRgb(_hue, s, v, 1.0f));
		}
	}
	d2d->DrawRoundRect(rect, D2D1_COLOR_F{ 0,0,0,0.35f }, 1.0f, 2.0f);
	float cx = rect.left + _saturation * RectWidth(rect);
	float cy = rect.top + (1.0f - _value) * RectHeight(rect);
	d2d->FillEllipse(cx, cy, 4.4f, 4.4f, D2D1_COLOR_F{ 0,0,0,0.70f });
	d2d->DrawEllipse(cx, cy, 5.3f, 5.3f, Colors::White, 1.2f);
}

void ColorPickerPopup::DrawHue(D2DGraphics* d2d, const D2D1_RECT_F& rect) const
{
	for (float y = rect.top; y < rect.bottom; y += 2.0f)
	{
		float h = std::clamp((y - rect.top) / std::max(1.0f, RectHeight(rect)), 0.0f, 1.0f) * 360.0f;
		d2d->FillRect(D2D1::RectF(rect.left, y, rect.right, std::min(y + 2.0f, rect.bottom)), HsvToRgb(h, 1.0f, 1.0f, 1.0f));
	}
	float y = rect.top + (_hue / 360.0f) * RectHeight(rect);
	d2d->DrawLine(rect.left - 1.0f, y, rect.right + 1.0f, y, Colors::Black, 2.0f);
	d2d->DrawLine(rect.left - 1.0f, y + 2.0f, rect.right + 1.0f, y + 2.0f, Colors::White, 1.2f);
}

void ColorPickerPopup::DrawAlpha(D2DGraphics* d2d, const D2D1_RECT_F& rect) const
{
	DrawCheckerBoard(d2d, rect, 8.0f);
	for (float x = rect.left; x < rect.right; x += 3.0f)
	{
		float a = std::clamp((x - rect.left) / std::max(1.0f, RectWidth(rect)), 0.0f, 1.0f);
		d2d->FillRect(D2D1::RectF(x, rect.top, std::min(x + 3.0f, rect.right), rect.bottom), WithAlpha(this->SelectedColor, a));
	}
	float x = rect.left + _alpha * RectWidth(rect);
	d2d->DrawLine(x, rect.top - 2.0f, x, rect.bottom + 2.0f, Colors::Black, 2.0f);
	d2d->DrawLine(x + 2.0f, rect.top - 2.0f, x + 2.0f, rect.bottom + 2.0f, Colors::White, 1.2f);
	d2d->DrawRoundRect(rect, D2D1_COLOR_F{ 0,0,0,0.35f }, 1.0f, 2.0f);
}

void ColorPickerPopup::Update()
{
	if (!this->IsVisual || !this->Visible) return;
	auto d2d = this->ParentForm ? this->ParentForm->Render : nullptr;
	if (!d2d) return;
	auto layout = CalcLayout();
	auto size = ActualSize();
	float w = (float)size.cx;
	float h = (float)size.cy;
	class Font* fontObj = this->Font;
	const float dropProgress = CurrentDropProgress();
	const float renderHeight = h * std::clamp(dropProgress, 0.0f, 1.0f);
	const float showOpacity = std::clamp(dropProgress, 0.0f, 1.0f);
	Microsoft::WRL::ComPtr<ID2D1Layer> opacityLayer;
	bool pushedOpacityLayer = false;

	this->BeginRender();
	{
		if (renderHeight <= 0.001f)
		{
			this->EndRender();
			return;
		}
		d2d->PushDrawRect(0.0f, 0.0f, w, renderHeight);
		auto* renderTarget = d2d->GetRenderTargetRaw();
		if (showOpacity < 0.999f && renderTarget &&
			SUCCEEDED(renderTarget->CreateLayer(D2D1::SizeF(w, renderHeight), opacityLayer.GetAddressOf())))
		{
			auto params = D2D1::LayerParameters(
				D2D1::RectF(0.0f, 0.0f, w, renderHeight),
				nullptr,
				D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,
				D2D1::Matrix3x2F::Identity(),
				showOpacity);
			renderTarget->PushLayer(&params, opacityLayer.Get());
			pushedOpacityLayer = true;
		}
		d2d->FillRoundRect(0.0f, 0.0f, w, h, this->PanelBackColor, 8.0f);
		DrawSV(d2d, layout.SvRect);
		DrawHue(d2d, layout.HueRect);
		DrawAlpha(d2d, layout.AlphaRect);

		auto common = CommonColorValues();
		for (int i = 0; i < (int)layout.CommonRects.size() && i < (int)common.size(); i++)
		{
			D2D1_COLOR_F color{};
			TryParseColor(common[i], color);
			DrawSwatch(d2d, layout.CommonRects[i], color, Lower(common[i]) == Lower(CurrentValueText()), i == _hoverCommon);
		}

		auto history = HistoryColorValues();
		for (int i = 0; i < (int)layout.HistoryRects.size() && i < (int)history.size(); i++)
		{
			D2D1_COLOR_F color{};
			TryParseColor(history[i], color);
			DrawSwatch(d2d, layout.HistoryRects[i], color, Lower(history[i]) == Lower(CurrentValueText()), i == _hoverHistory);
		}

		d2d->FillRoundRect(layout.InputRect, D2D1_COLOR_F{ 0.10f,0.10f,0.11f,1.0f }, 4.0f);
		d2d->DrawRoundRect(layout.InputRect, D2D1_COLOR_F{ 0.36f,0.37f,0.39f,1.0f }, 1.0f, 4.0f);
		const float previewSize = std::min(22.0f, std::max(10.0f, RectHeight(layout.InputRect) - 12.0f));
		const auto previewRect = D2D1::RectF(
			layout.InputRect.left + 8.0f,
			layout.InputRect.top + (RectHeight(layout.InputRect) - previewSize) * 0.5f,
			layout.InputRect.left + 8.0f + previewSize,
			layout.InputRect.top + (RectHeight(layout.InputRect) + previewSize) * 0.5f);
		DrawCheckerBoard(d2d, previewRect, 5.5f);
		d2d->FillRoundRect(previewRect, this->SelectedColor, 4.0f);
		d2d->DrawRoundRect(previewRect, D2D1_COLOR_F{ 0.0f,0.0f,0.0f,0.45f }, 1.0f, 4.0f);
		const float textX = previewRect.right + 10.0f;
		d2d->DrawString(CurrentValueText(), textX, layout.InputRect.top + 7.0f,
			std::max(1.0f, layout.InputRect.right - textX - 8.0f), 20.0f, this->TextColor, fontObj);

		auto drawButton = [&](const D2D1_RECT_F& rect, const std::wstring& text, bool hover) {
			D2D1_COLOR_F back = this->ButtonBackColor;
			if (hover) back = D2D1_COLOR_F{ 0.22f,0.22f,0.24f,1.0f };
			d2d->FillRoundRect(rect, back, 4.0f);
			d2d->DrawRoundRect(rect, this->ButtonBorderColor, 1.0f, 4.0f);
			auto textSize = fontObj ? fontObj->GetTextSize(text) : D2D1_SIZE_F{ 36.0f, 16.0f };
			d2d->DrawString(text, rect.left + std::max(0.0f, (RectWidth(rect) - textSize.width) * 0.5f),
				rect.top + std::max(0.0f, (RectHeight(rect) - textSize.height) * 0.5f),
				this->TextColor, fontObj);
			};
		drawButton(layout.ClearRect, L"Clear", _hoverClear);
		drawButton(layout.OkRect, L"OK", _hoverOk);
		d2d->DrawRoundRect(0.5f, 0.5f, w - 1.0f, h - 1.0f, this->PanelBorderColor, 1.0f, 8.0f);
		if (pushedOpacityLayer)
			renderTarget->PopLayer();
		d2d->PopDrawRect();
	}
	this->EndRender();

	if (IsAnimationRunning())
		this->InvalidateVisual();
}

bool ColorPickerPopup::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY)
{
	if (!this->Enable || !this->Visible) return true;
	const bool inPanel = Control::ContainsPoint(localX, localY);
	const bool draggingPicker = _dragSV || _dragHue || _dragAlpha;
	if (!draggingPicker && !inPanel && _hasAnchorRect && PtInRectF(_anchorRect, (float)localX, (float)localY))
	{
		if (_expanded && message == WM_LBUTTONDOWN)
		{
			Hide(false);
			MouseEventArgs e(MouseButtons::Left, 0, localX, localY, HIWORD(wParam));
			this->OnMouseDown(this, e);
			return true;
		}
		if (message == WM_LBUTTONUP)
		{
			MouseEventArgs e(MouseButtons::Left, 0, localX, localY, HIWORD(wParam));
			this->OnMouseUp(this, e);
			return true;
		}
		return true;
	}
	switch (message)
	{
	case WM_MOUSEMOVE:
	{
		if (this->ParentForm) this->ParentForm->UnderMouse = this;
		if (_dragSV) SetSVFromPoint(localX, localY);
		else if (_dragHue) SetHueFromPoint(localX, localY);
		else if (_dragAlpha) SetAlphaFromPoint(localX, localY);
		UpdateHover(localX, localY);
		MouseEventArgs e(MouseButtons::None, 0, localX, localY, HIWORD(wParam));
		this->OnMouseMove(this, e);
		return true;
	}
	case WM_LBUTTONDOWN:
	{
		auto layout = CalcLayout();
		UpdateHover(localX, localY);
		if (PtInRectF(layout.SvRect, (float)localX, (float)localY))
		{
			_dragSV = true;
			SetSVFromPoint(localX, localY);
			if (this->ParentForm && this->ParentForm->Handle) SetCapture(this->ParentForm->Handle);
		}
		else if (PtInRectF(layout.HueRect, (float)localX, (float)localY))
		{
			_dragHue = true;
			SetHueFromPoint(localX, localY);
			if (this->ParentForm && this->ParentForm->Handle) SetCapture(this->ParentForm->Handle);
		}
		else if (PtInRectF(layout.AlphaRect, (float)localX, (float)localY))
		{
			_dragAlpha = true;
			SetAlphaFromPoint(localX, localY);
			if (this->ParentForm && this->ParentForm->Handle) SetCapture(this->ParentForm->Handle);
		}
		else if (_hoverCommon >= 0)
		{
			auto values = CommonColorValues();
			if (_hoverCommon < (int)values.size())
			{
				D2D1_COLOR_F color{};
				if (TryParseColor(values[_hoverCommon], color))
				{
					SetFromColor(color);
					OnColorChanged(this, this->SelectedColor, CurrentValueText());
					InvalidateVisual();
				}
			}
		}
		else if (_hoverHistory >= 0)
		{
			auto values = HistoryColorValues();
			if (_hoverHistory < (int)values.size())
			{
				D2D1_COLOR_F color{};
				if (TryParseColor(values[_hoverHistory], color))
				{
					SetFromColor(color);
					OnColorChanged(this, this->SelectedColor, CurrentValueText());
					InvalidateVisual();
				}
			}
		}
		else if (_hoverClear)
			ClearValue();
		else if (_hoverOk)
			Hide(true);
		MouseEventArgs e(MouseButtons::Left, 0, localX, localY, HIWORD(wParam));
		this->OnMouseDown(this, e);
		return true;
	}
	case WM_LBUTTONUP:
	{
		_dragSV = _dragHue = _dragAlpha = false;
		ReleaseCapture();
		MouseEventArgs e(MouseButtons::Left, 0, localX, localY, HIWORD(wParam));
		this->OnMouseUp(this, e);
		return true;
	}
	case WM_KEYDOWN:
	{
		if (wParam == VK_ESCAPE) Hide(false);
		else if (wParam == VK_RETURN) Hide(true);
		KeyEventArgs e((Keys)(wParam | 0));
		this->OnKeyDown(this, e);
		return true;
	}
	default:
		break;
	}
	return Control::ProcessMessage(message, wParam, lParam, localX, localY);
}
