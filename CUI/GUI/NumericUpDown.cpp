#define NOMINMAX
#include "NumericUpDown.h"
#include "Form.h"

#include <algorithm>
#include <cmath>
#include <cwctype>
#include <iomanip>
#include <sstream>

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

	bool PtInRectF(const D2D1_RECT_F& rect, float x, float y)
	{
		return x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom;
	}

	float TextTop(Font* font, const D2D1_RECT_F& rect)
	{
		const float fontHeight = font ? font->FontHeight : 16.0f;
		return rect.top + (std::max)(0.0f, (RectHeight(rect) - fontHeight) * 0.5f);
	}

	D2D1_COLOR_F LerpColor(const D2D1_COLOR_F& from, const D2D1_COLOR_F& to, float t)
	{
		t = (std::clamp)(t, 0.0f, 1.0f);
		return D2D1_COLOR_F{
			from.r + (to.r - from.r) * t,
			from.g + (to.g - from.g) * t,
			from.b + (to.b - from.b) * t,
			from.a + (to.a - from.a) * t
		};
	}

	D2D1_COLOR_F ScaleAlpha(D2D1_COLOR_F color, float scale)
	{
		color.a *= (std::clamp)(scale, 0.0f, 1.0f);
		return color;
	}

	void DrawSpinArrow(D2DGraphics* d2d, const D2D1_RECT_F& rect, bool up, D2D1_COLOR_F color, float stroke = 1.5f)
	{
		if (!d2d) return;
		const float cx = rect.left + RectWidth(rect) * 0.5f;
		const float cy = rect.top + RectHeight(rect) * 0.5f + (up ? -0.4f : 0.4f);
		const float halfW = (std::clamp)((std::min)(RectWidth(rect), RectHeight(rect)) * 0.18f, 2.6f, 4.0f);
		const float halfH = halfW * 0.78f;
		if (up)
		{
			d2d->DrawLine(D2D1::Point2F(cx - halfW, cy + halfH), D2D1::Point2F(cx, cy - halfH), color, stroke);
			d2d->DrawLine(D2D1::Point2F(cx, cy - halfH), D2D1::Point2F(cx + halfW, cy + halfH), color, stroke);
		}
		else
		{
			d2d->DrawLine(D2D1::Point2F(cx - halfW, cy - halfH), D2D1::Point2F(cx, cy + halfH), color, stroke);
			d2d->DrawLine(D2D1::Point2F(cx, cy + halfH), D2D1::Point2F(cx + halfW, cy - halfH), color, stroke);
		}
	}
}

UIClass NumericUpDown::Type()
{
	return UIClass::UI_NumericUpDown;
}

NumericUpDown::NumericUpDown(int x, int y, int width, int height)
{
	this->Location = POINT{ x, y };
	this->Size = SIZE{ width, height };
	this->BackColor = D2D1_COLOR_F{ 0, 0, 0, 0 };
	this->BolderColor = D2D1_COLOR_F{ 0.55f, 0.60f, 0.68f, 0.70f };
	this->ForeColor = Colors::Black;
	this->Cursor = CursorKind::IBeam;
	SyncTextFromValue();

	this->OnLostFocus += [this](class Control* sender)
		{
			(void)sender;
			if (_editing)
				CommitEdit();
			_dragUp = false;
			_dragDown = false;
			_selectAllPending = false;
			PostRender();
		};
}

GET_CPP(NumericUpDown, double, Min)
{
	return _min;
}

SET_CPP(NumericUpDown, double, Min)
{
	_min = value;
	if (_max < _min)
		_max = _min;
	SetValueInternal(_value, false);
	PostRender();
}

GET_CPP(NumericUpDown, double, Max)
{
	return _max;
}

SET_CPP(NumericUpDown, double, Max)
{
	_max = value;
	if (_min > _max)
		_min = _max;
	SetValueInternal(_value, false);
	PostRender();
}

GET_CPP(NumericUpDown, double, Value)
{
	return _value;
}

SET_CPP(NumericUpDown, double, Value)
{
	SetValueInternal(value, true);
}

double NumericUpDown::ClampValue(double value) const
{
	if (_max < _min)
		return _min;
	return (std::clamp)(value, _min, _max);
}

double NumericUpDown::SnapValue(double value) const
{
	double v = ClampValue(value);
	if (!SnapToStep || Step <= 0.0 || !std::isfinite(Step))
		return v;
	double steps = (v - _min) / Step;
	double snapped = _min + std::round(steps) * Step;
	return ClampValue(snapped);
}

void NumericUpDown::SetValueInternal(double value, bool fireEvent)
{
	double old = _value;
	double next = SnapValue(value);
	if (!std::isfinite(next))
		next = _min;
	if (std::fabs(next - _value) <= 0.0000001)
	{
		if (!_editing)
			SyncTextFromValue();
		return;
	}

	_value = next;
	if (!_editing)
		SyncTextFromValue();
	else
		this->Text = _editText;
	PostRender();
	if (fireEvent)
		OnValueChanged(this, old, _value);
}

void NumericUpDown::SyncTextFromValue()
{
	_editText = FormatValue();
	this->Text = _editText;
}

std::wstring NumericUpDown::FormatValue() const
{
	std::wstringstream stream;
	stream.setf(std::ios::fixed, std::ios::floatfield);
	stream << std::setprecision((std::max)(0, DecimalPlaces)) << _value;
	return stream.str();
}

void NumericUpDown::BeginEdit()
{
	_editing = true;
	_selectAllPending = SelectAllOnFocus;
	_editText = this->Text.empty() ? FormatValue() : this->Text;
	this->Text = _editText;
	if (ParentForm)
		ParentForm->SetSelectedControl(this, false);
	PostRender();
}

bool NumericUpDown::CommitEdit()
{
	if (!_editing)
		return true;
	std::wstring text = _editText;
	if (text.empty() || text == L"-" || text == L"+" || text == L"." || text == L"-." || text == L"+.")
	{
		_editing = false;
		_selectAllPending = false;
		SyncTextFromValue();
		PostRender();
		return false;
	}

	wchar_t* end = nullptr;
	const wchar_t* start = text.c_str();
	double parsed = std::wcstod(start, &end);
	while (end && *end && std::iswspace(*end))
		++end;
	if (end == start || (end && *end != L'\0') || !std::isfinite(parsed))
	{
		_editing = false;
		_selectAllPending = false;
		SyncTextFromValue();
		PostRender();
		return false;
	}

	_editing = false;
	_selectAllPending = false;
	SetValueInternal(parsed, true);
	SyncTextFromValue();
	PostRender();
	return true;
}

void NumericUpDown::CancelEdit()
{
	_editing = false;
	_selectAllPending = false;
	SyncTextFromValue();
	PostRender();
}

D2D1_RECT_F NumericUpDown::ButtonPanelRect() const
{
	const float w = (float)_size.cx;
	const float h = (float)_size.cy;
	const float bw = (std::clamp)(ButtonWidth, 18.0f, (std::max)(18.0f, w * 0.45f));
	return D2D1::RectF((std::max)(0.0f, w - bw), 0.0f, w, h);
}

D2D1_RECT_F NumericUpDown::UpButtonRect() const
{
	auto panel = ButtonPanelRect();
	const float mid = panel.top + RectHeight(panel) * 0.5f;
	return D2D1::RectF(panel.left, panel.top, panel.right, mid);
}

D2D1_RECT_F NumericUpDown::DownButtonRect() const
{
	auto panel = ButtonPanelRect();
	const float mid = panel.top + RectHeight(panel) * 0.5f;
	return D2D1::RectF(panel.left, mid, panel.right, panel.bottom);
}

D2D1_RECT_F NumericUpDown::TextRect() const
{
	const float h = (float)_size.cy;
	auto buttons = ButtonPanelRect();
	return D2D1::RectF(TextPaddingX, 0.0f, (std::max)(TextPaddingX, buttons.left - TextPaddingX), h);
}

int NumericUpDown::HitTestButton(int xof, int yof) const
{
	if (PtInRectF(UpButtonRect(), (float)xof, (float)yof))
		return 1;
	if (PtInRectF(DownButtonRect(), (float)xof, (float)yof))
		return -1;
	return 0;
}

void NumericUpDown::StepBy(int direction)
{
	if (direction == 0)
		return;
	if (_editing)
		CommitEdit();
	double delta = Step > 0.0 && std::isfinite(Step) ? Step : 1.0;
	if (GetKeyState(VK_SHIFT) & 0x8000)
		delta *= 10.0;
	SetValueInternal(_value + delta * (double)direction, true);
}

void NumericUpDown::StartHoverAnimation(float target)
{
	target = (std::clamp)(target, 0.0f, 1.0f);
	CurrentHoverProgress();
	if (std::fabs(_hoverProgress - target) <= 0.001f)
	{
		_hoverProgress = target;
		_targetHoverProgress = target;
		_animating = false;
		PostRender();
		return;
	}
	_animStartProgress = _hoverProgress;
	_targetHoverProgress = target;
	_animStartTick = ::GetTickCount64();
	_animating = true;
	PostRender();
}

float NumericUpDown::CurrentHoverProgress()
{
	if (!_animating)
		return _hoverProgress;
	ULONGLONG now = ::GetTickCount64();
	ULONGLONG elapsed = now >= _animStartTick ? now - _animStartTick : 0;
	float t = _animDurationMs > 0 ? (float)elapsed / (float)_animDurationMs : 1.0f;
	if (t >= 1.0f)
	{
		_hoverProgress = _targetHoverProgress;
		_animating = false;
		return _hoverProgress;
	}
	t = 1.0f - std::pow(1.0f - (std::clamp)(t, 0.0f, 1.0f), 3.0f);
	_hoverProgress = _animStartProgress + (_targetHoverProgress - _animStartProgress) * t;
	return _hoverProgress;
}

CursorKind NumericUpDown::QueryCursor(int xof, int yof)
{
	if (!Enable)
		return CursorKind::Arrow;
	return HitTestButton(xof, yof) == 0 ? CursorKind::IBeam : CursorKind::Hand;
}

bool NumericUpDown::CanHandleMouseWheel(int delta, int xof, int yof)
{
	(void)xof;
	(void)yof;
	if (!UseMouseWheel || delta == 0 || !Enable)
		return false;
	return true;
}

bool NumericUpDown::HandlesNavigationKey(WPARAM key) const
{
	return key == VK_UP || key == VK_DOWN || key == VK_HOME || key == VK_END;
}

bool NumericUpDown::IsAnimationRunning()
{
	CurrentHoverProgress();
	return _animating || IsCaretBlinkAnimating();
}

bool NumericUpDown::GetAnimatedInvalidRect(D2D1_RECT_F& outRect)
{
	if (GetCaretBlinkInvalidRect(outRect))
		return true;
	if (!_animating)
		return false;
	outRect = this->AbsRect;
	return true;
}

void NumericUpDown::Update()
{
	if (!this->IsVisual) return;
	auto d2d = this->ParentForm ? this->ParentForm->Render : nullptr;
	if (!d2d) return;

	const float hoverProgress = CurrentHoverProgress();
	const bool isSelected = ParentForm && ParentForm->Selected == this;
	const bool isUnderMouse = ParentForm && ParentForm->UnderMouse == this;
	if (!isUnderMouse && !_dragUp && !_dragDown && _hoverButton != 0)
	{
		_hoverButton = 0;
		StartHoverAnimation(0.0f);
	}

	auto size = this->ActualSize();
	const float width = (float)size.cx;
	const float height = (float)size.cy;
	const float radius = (std::clamp)(CornerRadius, 0.0f, (std::min)(width, height) * 0.5f);
	const float border = (std::max)(0.0f, Border);
	auto panelRect = ButtonPanelRect();
	auto upRect = UpButtonRect();
	auto downRect = DownButtonRect();
	auto textRect = TextRect();
	class Font* fontObj = this->Font;

	this->BeginRender();
	{
		D2D1_COLOR_F base = PanelBackColor.a > 0.0f ? PanelBackColor : this->BackColor;
		d2d->FillRoundRect(0.0f, 0.0f, width, height, base, radius);
		if (isUnderMouse && !_editing)
			d2d->FillRoundRect(1.0f, 1.0f, (std::max)(0.0f, width - 2.0f), (std::max)(0.0f, height - 2.0f),
				ScaleAlpha(ButtonHoverColor, 0.45f), (std::max)(0.0f, radius - 1.0f));

		d2d->FillRoundRect(panelRect.left, panelRect.top + 3.0f,
			RectWidth(panelRect) - 3.0f, (std::max)(0.0f, RectHeight(panelRect) - 6.0f), ButtonBackColor, 4.0f);
		if (_hoverButton != 0 && hoverProgress > 0.001f)
		{
			auto rect = _hoverButton > 0 ? upRect : downRect;
			auto color = (_dragUp && _hoverButton > 0) || (_dragDown && _hoverButton < 0)
				? ButtonPressedColor
				: ScaleAlpha(ButtonHoverColor, hoverProgress);
			d2d->FillRoundRect(rect.left + 2.0f, rect.top + 2.0f,
				(std::max)(0.0f, RectWidth(rect) - 4.0f), (std::max)(0.0f, RectHeight(rect) - 4.0f), color, 3.5f);
		}
		d2d->DrawLine(panelRect.left, 5.0f, panelRect.left, (std::max)(5.0f, height - 5.0f), ScaleAlpha(BolderColor, 0.65f), 1.0f);
		d2d->DrawLine(panelRect.left + 3.0f, height * 0.5f, width - 4.0f, height * 0.5f, ScaleAlpha(BolderColor, 0.52f), 1.0f);
		DrawSpinArrow(d2d, upRect, true, _hoverButton > 0 ? ForeColor : MutedTextColor);
		DrawSpinArrow(d2d, downRect, false, _hoverButton < 0 ? ForeColor : MutedTextColor);

		std::wstring text = _editing ? _editText : FormatValue();
		D2D1_COLOR_F textColor = text.empty() ? MutedTextColor : ForeColor;
		float textY = TextTop(fontObj, textRect);
		d2d->PushDrawRect(textRect.left, textRect.top, (std::max)(1.0f, RectWidth(textRect)), RectHeight(textRect));
		d2d->DrawString(text, textRect.left, textY, (std::max)(1.0f, RectWidth(textRect)), RectHeight(textRect), textColor, fontObj);

		bool caretVisible = isSelected && _editing;
		D2D1_RECT_F caretAbs{};
		bool caretRectValid = false;
		if (caretVisible)
		{
			float textWidth = text.empty() || !fontObj ? 0.0f : fontObj->GetTextSize(text).width;
			float caretX = textRect.left + (std::min)(textWidth + 1.0f, (std::max)(1.0f, RectWidth(textRect) - 2.0f));
			float caretTop = textY + 1.0f;
			float caretBottom = textY + (fontObj ? fontObj->FontHeight : 16.0f) - 1.0f;
			auto abs = this->AbsLocation;
			caretAbs = D2D1::RectF((float)abs.x + caretX - 2.0f, (float)abs.y + caretTop - 2.0f,
				(float)abs.x + caretX + 2.0f, (float)abs.y + caretBottom + 2.0f);
			caretRectValid = true;
			UpdateCaretBlinkState(true, (int)text.size(), (int)text.size(), true, &caretAbs);
			if (IsCaretBlinkVisible())
				d2d->DrawLine(caretX, caretTop, caretX, caretBottom, AccentColor, 1.2f);
		}
		if (!caretVisible)
			UpdateCaretBlinkState(false, 0, 0, false);
		d2d->PopDrawRect();

		D2D1_COLOR_F borderColor = isSelected ? FocusBorderColor : BolderColor;
		float borderWidth = isSelected ? (std::max)(border, FocusBorder) : border;
		if (borderWidth > 0.0f && borderColor.a > 0.0f)
			d2d->DrawRoundRect(borderWidth * 0.5f, borderWidth * 0.5f,
				(std::max)(0.0f, width - borderWidth), (std::max)(0.0f, height - borderWidth),
				borderColor, borderWidth, radius);

		if (this->Image)
			this->RenderImage(radius);

		if (!Enable)
			d2d->FillRoundRect(0.0f, 0.0f, width, height, DisabledOverlayColor, radius);
	}
	this->EndRender();
}

bool NumericUpDown::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof)
{
	if (!this->Enable || !this->Visible) return true;
	(void)lParam;

	switch (message)
	{
	case WM_MOUSEWHEEL:
		if (UseMouseWheel)
			StepBy(GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? 1 : -1);
		OnMouseWheel(this, MouseEventArgs(MouseButtons::None, 0, xof, yof, GET_WHEEL_DELTA_WPARAM(wParam)));
		return true;
	case WM_MOUSEMOVE:
	{
		if (ParentForm) ParentForm->UnderMouse = this;
		int hit = HitTestButton(xof, yof);
		if (hit != _hoverButton)
		{
			_hoverButton = hit;
			StartHoverAnimation(hit == 0 ? 0.0f : 1.0f);
		}
		OnMouseMove(this, MouseEventArgs(MouseButtons::None, 0, xof, yof, HIWORD(wParam)));
		return true;
	}
	case WM_LBUTTONDOWN:
	{
		if (ParentForm) ParentForm->SetSelectedControl(this, false);
		int hit = HitTestButton(xof, yof);
		if (hit != 0)
		{
			_dragUp = hit > 0;
			_dragDown = hit < 0;
			_hoverButton = hit;
			StartHoverAnimation(1.0f);
			StepBy(hit);
		}
		else
		{
			BeginEdit();
		}
		OnMouseDown(this, MouseEventArgs(MouseButtons::Left, 0, xof, yof, HIWORD(wParam)));
		PostRender();
		return true;
	}
	case WM_LBUTTONUP:
	{
		_dragUp = false;
		_dragDown = false;
		int hit = HitTestButton(xof, yof);
		if (hit != _hoverButton)
		{
			_hoverButton = hit;
			StartHoverAnimation(hit == 0 ? 0.0f : 1.0f);
		}
		MouseEventArgs e(MouseButtons::Left, 0, xof, yof, HIWORD(wParam));
		OnMouseUp(this, e);
		OnMouseClick(this, e);
		PostRender();
		return true;
	}
	case WM_KEYDOWN:
	{
		switch (wParam)
		{
		case VK_UP:
			StepBy(1);
			break;
		case VK_DOWN:
			StepBy(-1);
			break;
		case VK_HOME:
			if (_editing) CommitEdit();
			SetValueInternal(_min, true);
			break;
		case VK_END:
			if (_editing) CommitEdit();
			SetValueInternal(_max, true);
			break;
		case VK_RETURN:
			CommitEdit();
			break;
		case VK_ESCAPE:
			CancelEdit();
			break;
		default:
			break;
		}
		OnKeyDown(this, KeyEventArgs((Keys)(wParam | 0)));
		return true;
	}
	case WM_CHAR:
	{
		if (!_editing)
			BeginEdit();
		wchar_t ch = (wchar_t)wParam;
		if (ch == L'\r')
		{
			CommitEdit();
		}
		else if (ch == 8)
		{
			if (_selectAllPending)
			{
				_editText.clear();
				_selectAllPending = false;
			}
			else if (!_editText.empty())
			{
				_editText.pop_back();
			}
			this->Text = _editText;
		}
		else if (ch == 1)
		{
			_selectAllPending = true;
		}
		else if ((ch >= L'0' && ch <= L'9') || ch == L'.' || ch == L'-' || ch == L'+')
		{
			if (_selectAllPending)
			{
				_editText.clear();
				_selectAllPending = false;
			}
			if ((ch == L'.' && _editText.find(L'.') != std::wstring::npos) ||
				((ch == L'-' || ch == L'+') && !_editText.empty()))
			{
				PostRender();
				return true;
			}
			_editText.push_back(ch);
			this->Text = _editText;
		}
		PostRender();
		return true;
	}
	case WM_KEYUP:
		OnKeyUp(this, KeyEventArgs((Keys)(wParam | 0)));
		return true;
	default:
		break;
	}

	return Control::ProcessMessage(message, wParam, lParam, xof, yof);
}
