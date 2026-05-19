#define NOMINMAX
#include "ColorPicker.h"
#include "Form.h"

#include <algorithm>
#include <cmath>

namespace
{
	float RectWidth(const D2D1_RECT_F& rect)
	{
		return rect.right - rect.left;
	}

	float RectHeight(const D2D1_RECT_F& rect)
	{
		return rect.bottom - rect.top;
	}

	bool SameColor(D2D1_COLOR_F a, D2D1_COLOR_F b)
	{
		return std::fabs(a.r - b.r) <= 1e-6f &&
			std::fabs(a.g - b.g) <= 1e-6f &&
			std::fabs(a.b - b.b) <= 1e-6f &&
			std::fabs(a.a - b.a) <= 1e-6f;
	}

	D2D1_COLOR_F LerpColor(D2D1_COLOR_F from, D2D1_COLOR_F to, float t)
	{
		t = std::clamp(t, 0.0f, 1.0f);
		return D2D1_COLOR_F{
			from.r + (to.r - from.r) * t,
			from.g + (to.g - from.g) * t,
			from.b + (to.b - from.b) * t,
			from.a + (to.a - from.a) * t
		};
	}

	void DrawPickerChevron(D2DGraphics* d2d, float cx, float cy, float size, float progress, D2D1_COLOR_F color)
	{
		if (!d2d) return;
		progress = std::clamp(progress, 0.0f, 1.0f);
		const float halfW = size * 0.42f;
		const float halfH = size * 0.26f;
		const float angle = progress * 3.14159265359f;
		auto rotate = [&](D2D1_POINT_2F p) {
			const float dx = p.x - cx;
			const float dy = p.y - cy;
			const float s = std::sin(angle);
			const float c = std::cos(angle);
			return D2D1::Point2F(cx + dx * c - dy * s, cy + dx * s + dy * c);
		};
		auto p1 = rotate(D2D1::Point2F(cx - halfW, cy - halfH));
		auto p2 = rotate(D2D1::Point2F(cx, cy + halfH));
		auto p3 = rotate(D2D1::Point2F(cx + halfW, cy - halfH));
		d2d->DrawLine(p1, p2, color, 1.7f);
		d2d->DrawLine(p2, p3, color, 1.7f);
	}

	void DrawCheckerBoard(D2DGraphics* d2d, const D2D1_RECT_F& rect, float cellSize)
	{
		if (!d2d || RectWidth(rect) <= 0.0f || RectHeight(rect) <= 0.0f) return;
		D2D1_COLOR_F a = D2D1_COLOR_F{ 0.72f, 0.74f, 0.78f, 1.0f };
		D2D1_COLOR_F b = D2D1_COLOR_F{ 0.94f, 0.95f, 0.97f, 1.0f };
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
}

UIClass ColorPicker::Type()
{
	return UIClass::UI_ColorPicker;
}

ColorPicker::ColorPicker(int x, int y, int width, int height)
{
	this->Location = POINT{ x, y };
	this->Size = SIZE{ width, height };
	this->BackColor = D2D1_COLOR_F{ 0, 0, 0, 0 };
	this->BorderColor = D2D1_COLOR_F{ 0.55f, 0.60f, 0.68f, 0.75f };
	this->ForeColor = Colors::Black;
	this->Cursor = CursorKind::Hand;
}

ColorPicker::~ColorPicker()
{
	if (_popup)
	{
		if (this->ParentForm && this->ParentForm->ForegroundControl == _popup)
			this->ParentForm->ForegroundControl = nullptr;
		delete _popup;
		_popup = nullptr;
	}
}

GET_CPP(ColorPicker, D2D1_COLOR_F, SelectedColor)
{
	return _selectedColor;
}

SET_CPP(ColorPicker, D2D1_COLOR_F, SelectedColor)
{
	SetSelectedColorInternal(value, true);
}

GET_CPP(ColorPicker, std::wstring, ValueText)
{
	return ColorPickerPopup::ColorToString(_selectedColor);
}

SET_CPP(ColorPicker, std::wstring, ValueText)
{
	D2D1_COLOR_F color{};
	if (ColorPickerPopup::TryParseColor(value, color))
		SetSelectedColorInternal(color, true);
}

bool ColorPicker::TryParseColor(const std::wstring& text, D2D1_COLOR_F& out)
{
	return ColorPickerPopup::TryParseColor(text, out);
}

std::wstring ColorPicker::ColorToString(D2D1_COLOR_F color)
{
	return ColorPickerPopup::ColorToString(color);
}

ColorPickerPopup* ColorPicker::EnsurePopup()
{
	if (_popup)
		return _popup;
	_popup = new ColorPickerPopup(PopupWidth, PopupHeight);
	_popup->OnColorChanged += [this](ColorPickerPopup*, D2D1_COLOR_F color, std::wstring)
		{
			if (UpdateOnPreview)
				SetSelectedColorInternal(color, true);
			else
				InvalidateVisual();
		};
	_popup->OnColorConfirmed += [this](ColorPickerPopup*, D2D1_COLOR_F color, std::wstring)
		{
			SetSelectedColorInternal(color, true);
			_popupOpen = false;
			OnDropDownClosed(this);
			InvalidateVisual();
		};
	_popup->OnCleared += [this](ColorPickerPopup*)
		{
			SetSelectedColorInternal(D2D1_COLOR_F{ 0, 0, 0, 0 }, true);
			_popupOpen = false;
			OnDropDownClosed(this);
			InvalidateVisual();
		};
	_popup->OnCancelled += [this](ColorPickerPopup*)
		{
			if (UpdateOnPreview && RevertPreviewOnCancel)
				SetSelectedColorInternal(_popupStartColor, true);
			_popupOpen = false;
			OnDropDownClosed(this);
			InvalidateVisual();
		};
	return _popup;
}

void ColorPicker::ApplyPopupTheme(ColorPickerPopup* popup)
{
	if (!popup) return;
	popup->AccentColor = AccentColor;
	popup->TextColor = ForeColor;
	popup->MutedTextColor = MutedTextColor;
}

void ColorPicker::OpenPopup()
{
	if (!ParentForm) return;
	auto* popup = EnsurePopup();
	ApplyPopupTheme(popup);
	_popupStartColor = _selectedColor;
	_popupOpen = true;
	D2D1_RECT_F anchor = D2D1::RectF(0.0f, 0.0f, (float)Width, (float)Height);
	popup->ShowAt(this, anchor, _selectedColor);
	OnDropDownOpened(this);
	InvalidateVisual();
}

void ColorPicker::ClosePopup(bool confirm)
{
	if (_popup && _popupOpen)
		_popup->Hide(confirm);
}

void ColorPicker::SetSelectedColorInternal(D2D1_COLOR_F color, bool fireEvent)
{
	color.r = std::clamp(color.r, 0.0f, 1.0f);
	color.g = std::clamp(color.g, 0.0f, 1.0f);
	color.b = std::clamp(color.b, 0.0f, 1.0f);
	color.a = std::clamp(color.a, 0.0f, 1.0f);
	if (SameColor(_selectedColor, color))
		return;
	D2D1_COLOR_F old = _selectedColor;
	_selectedColor = color;
	this->Text = ColorPickerPopup::ColorToString(_selectedColor);
	if (fireEvent)
		OnColorChanged(this, old, _selectedColor, this->Text);
	InvalidateVisual();
}

float ColorPicker::CurrentHoverProgress()
{
	if (!_hoverAnimating)
		return _hoverProgress;
	ULONGLONG now = ::GetTickCount64();
	ULONGLONG elapsed = now >= _hoverAnimStartTick ? now - _hoverAnimStartTick : 0;
	float t = _hoverAnimDurationMs > 0 ? (float)elapsed / (float)_hoverAnimDurationMs : 1.0f;
	if (t >= 1.0f)
	{
		_hoverProgress = _hoverTargetProgress;
		_hoverAnimating = false;
		return _hoverProgress;
	}
	t = 1.0f - std::pow(1.0f - std::clamp(t, 0.0f, 1.0f), 3.0f);
	_hoverProgress = _hoverStartProgress + (_hoverTargetProgress - _hoverStartProgress) * t;
	return _hoverProgress;
}

void ColorPicker::StartHoverAnimation(float target)
{
	target = std::clamp(target, 0.0f, 1.0f);
	CurrentHoverProgress();
	if (std::fabs(_hoverProgress - target) <= 0.001f)
	{
		_hoverProgress = target;
		_hoverAnimating = false;
		return;
	}
	_hoverStartProgress = _hoverProgress;
	_hoverTargetProgress = target;
	_hoverAnimStartTick = ::GetTickCount64();
	_hoverAnimating = true;
	InvalidateVisual();
}

D2D1_RECT_F ColorPicker::SwatchRect() const
{
	float height = (float)_size.cy;
	float sw = std::clamp(SwatchSize, 10.0f, std::max(10.0f, height - 8.0f));
	float left = TextPaddingX;
	float top = (height - sw) * 0.5f;
	return D2D1::RectF(left, top, left + sw, top + sw);
}

D2D1_RECT_F ColorPicker::ArrowRect() const
{
	float width = (float)_size.cx;
	float height = (float)_size.cy;
	float bw = std::clamp(ButtonWidth, 18.0f, std::max(18.0f, width));
	return D2D1::RectF(width - bw, 0.0f, width, height);
}

CursorKind ColorPicker::QueryCursor(int xof, int yof)
{
	(void)xof;
	(void)yof;
	return Enable ? CursorKind::Hand : CursorKind::Arrow;
}

bool ColorPicker::HandlesNavigationKey(WPARAM key) const
{
	return key == VK_RETURN || key == VK_SPACE || key == VK_ESCAPE;
}

bool ColorPicker::IsAnimationRunning()
{
	CurrentHoverProgress();
	return _hoverAnimating;
}

bool ColorPicker::GetAnimatedInvalidRect(D2D1_RECT_F& outRect)
{
	if (!_hoverAnimating)
		return false;
	outRect = this->AbsRect;
	return true;
}

void ColorPicker::Update()
{
	if (!this->IsVisual) return;
	auto d2d = this->ParentForm ? this->ParentForm->Render : nullptr;
	if (!d2d) return;

	const float width = (float)this->Width;
	const float height = (float)this->Height;
	const float border = std::max(0.0f, Border);
	const float radius = std::clamp(CornerRadius, 0.0f, std::min(width, height) * 0.5f);
	const bool focused = this->ParentForm && this->ParentForm->Selected == this;
	const float hover = CurrentHoverProgress();
	const float dropProgress = (_popup && (_popupOpen || _popup->IsAnimationRunning())) ? _popup->CurrentDropProgress() : 0.0f;
	D2D1_COLOR_F panel = LerpColor(PanelBackColor, PanelHoverColor, hover);
	if (hover <= 0.001f) panel = PanelBackColor;

	this->BeginRender(width, height);
	{
		d2d->FillRoundRect(0.0f, 0.0f, width, height, panel, radius);
		auto arrow = ArrowRect();
		d2d->FillRoundRect(arrow.left, arrow.top, RectWidth(arrow), RectHeight(arrow), ButtonBackColor, radius);
		d2d->DrawLine(arrow.left, 6.0f, arrow.left, std::max(6.0f, height - 6.0f), BorderColor, 1.0f);

		auto swatch = SwatchRect();
		DrawCheckerBoard(d2d, swatch, 5.0f);
		d2d->FillRoundRect(swatch, _selectedColor, 4.0f);
		d2d->DrawRoundRect(swatch, D2D1_COLOR_F{ 0, 0, 0, 0.38f }, 1.0f, 4.0f);

		float textLeft = swatch.right + TextPaddingX;
		float textRight = std::max(textLeft + 1.0f, arrow.left - 6.0f);
		std::wstring value = ColorPickerPopup::ColorToString(_selectedColor);
		float fontHeight = Font ? Font->FontHeight : 16.0f;
		float textTop = std::max(0.0f, (height - fontHeight) * 0.5f);
		d2d->PushDrawRect(textLeft, 0.0f, std::max(1.0f, textRight - textLeft), height);
		d2d->DrawString(value, textLeft, textTop, std::max(1.0f, textRight - textLeft), fontHeight + 2.0f, ForeColor, Font);
		d2d->PopDrawRect();

		DrawPickerChevron(d2d, (arrow.left + arrow.right) * 0.5f, height * 0.5f, 10.0f, dropProgress, ForeColor);

		if (border > 0.0f)
		{
			D2D1_COLOR_F borderColor = focused ? FocusBorderColor : BorderColor;
			float borderWidth = focused ? FocusBorder : border;
			d2d->DrawRoundRect(borderWidth * 0.5f, borderWidth * 0.5f,
				std::max(0.0f, width - borderWidth), std::max(0.0f, height - borderWidth),
				borderColor, borderWidth, radius);
		}
		if (!Enable)
			d2d->FillRoundRect(0.0f, 0.0f, width, height, DisabledOverlayColor, radius);
	}
	this->EndRender();

	if (IsAnimationRunning())
		this->InvalidateVisual();
}

bool ColorPicker::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof)
{
	if (!Enable)
		return Control::ProcessMessage(message, wParam, lParam, xof, yof);

	switch (message)
	{
	case WM_MOUSEMOVE:
		if (!_hovered)
		{
			_hovered = true;
			StartHoverAnimation(1.0f);
		}
		break;
	case WM_MOUSELEAVE:
		_hovered = false;
		StartHoverAnimation(0.0f);
		break;
	case WM_LBUTTONDOWN:
	{
		if (ParentForm)
			ParentForm->SetSelectedControl(this);
		if (_popupOpen)
			ClosePopup(false);
		else
			OpenPopup();
		MouseEventArgs e(MouseButtons::Left, 0, xof, yof, HIWORD(wParam));
		this->OnMouseDown(this, e);
		return true;
	}
	case WM_KEYDOWN:
		if (wParam == VK_RETURN || wParam == VK_SPACE)
		{
			if (_popupOpen) ClosePopup(true);
			else OpenPopup();
			KeyEventArgs e((Keys)(wParam | 0));
			this->OnKeyDown(this, e);
			return true;
		}
		if (wParam == VK_ESCAPE && _popupOpen)
		{
			ClosePopup(false);
			KeyEventArgs e((Keys)(wParam | 0));
			this->OnKeyDown(this, e);
			return true;
		}
		break;
	default:
		break;
	}

	return Control::ProcessMessage(message, wParam, lParam, xof, yof);
}
