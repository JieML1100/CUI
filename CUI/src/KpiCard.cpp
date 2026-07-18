#include "KpiCard.h"
#include "Form.h"
#include "AdvancedControlPropertyRegistration.h"
#include <algorithm>
#include <cmath>

namespace
{
	float RectWidth(const D2D1_RECT_F& rect)
	{
		return (std::max)(0.0f, rect.right - rect.left);
	}

	float RectHeight(const D2D1_RECT_F& rect)
	{
		return (std::max)(0.0f, rect.bottom - rect.top);
	}
}

UIClass KpiCard::Type()
{
	return UIClass::UI_KpiCard;
}

void KpiCard::EnsureBindingPropertiesRegistered()
{
	Control::EnsureBindingPropertiesRegistered();
	static const bool registered = []
	{
		using namespace cui::advanced_properties;
		RegisterField(L"Title", &KpiCard::Title, std::wstring(L"Metric"),
			L"Data", 600, 10, ControlPropertyEditorKind::Text);
		RegisterField(L"Value", &KpiCard::Value, std::wstring(L"0"),
			L"Data", 600, 20, ControlPropertyEditorKind::Text);
		RegisterField(L"Unit", &KpiCard::Unit, std::wstring{},
			L"Data", 600, 30, ControlPropertyEditorKind::Text);
		RegisterField(L"TrendText", &KpiCard::TrendText, std::wstring{},
			L"Data", 600, 40, ControlPropertyEditorKind::Text);
		RegisterField(L"Caption", &KpiCard::Caption, std::wstring{},
			L"Data", 600, 50, ControlPropertyEditorKind::Text);
		RegisterEnumField(L"TrendDirection", &KpiCard::TrendDirection,
			KpiTrendDirection::Neutral, L"Data", 600, 60,
			{ { L"Neutral", KpiTrendDirection::Neutral },
			  { L"Up", KpiTrendDirection::Up },
			  { L"Down", KpiTrendDirection::Down } });
		RegisterField(L"Clickable", &KpiCard::Clickable, true,
			L"Behavior", 110, 10, ControlPropertyEditorKind::Boolean);
		RegisterField(L"Active", &KpiCard::Active, false,
			L"Behavior", 110, 20, ControlPropertyEditorKind::Boolean);
		RegisterField(L"ShowSparkline", &KpiCard::ShowSparkline, true,
			L"Behavior", 110, 30, ControlPropertyEditorKind::Boolean);
		RegisterField(L"ToggleActiveOnClick", &KpiCard::ToggleActiveOnClick, true,
			L"Behavior", 110, 40, ControlPropertyEditorKind::Boolean);
		RegisterMetric(L"Border", &KpiCard::Border, 1.0f,
			L"Appearance", 200, 10);
		RegisterMetric(L"CornerRadius", &KpiCard::CornerRadius, 8.0f,
			L"Appearance", 200, 20);
		RegisterColor(L"SurfaceColor", &KpiCard::SurfaceColor,
			cui::theme::palette::Surface, 100);
		RegisterColor(L"ActiveBackColor", &KpiCard::ActiveBackColor,
			cui::theme::palette::AccentSelected, 110);
		RegisterColor(L"HoverColor", &KpiCard::HoverColor,
			cui::theme::palette::AccentSoft, 120);
		RegisterColor(L"AccentColor", &KpiCard::AccentColor,
			cui::theme::palette::Accent, 130);
		RegisterColor(L"MutedTextColor", &KpiCard::MutedTextColor,
			cui::theme::palette::TextMuted, 140);
		RegisterColor(L"PositiveColor", &KpiCard::PositiveColor,
			cui::theme::palette::Positive, 150);
		RegisterColor(L"NegativeColor", &KpiCard::NegativeColor,
			cui::theme::palette::Negative, 160);
		RegisterColor(L"NeutralColor", &KpiCard::NeutralColor,
			cui::theme::palette::TextMuted, 170);
		RegisterColor(L"SparklineFillColor", &KpiCard::SparklineFillColor,
			cui::theme::palette::AccentSoft, 180);
		return true;
	}();
	(void)registered;
}

KpiCard::KpiCard(int x, int y, int width, int height)
{
	this->Location = POINT{ x, y };
	this->Size = SIZE{ width, height };
	this->BackColor = D2D1_COLOR_F{ 0, 0, 0, 0 };
	this->BorderColor = cui::theme::palette::Border;
	this->ForeColor = cui::theme::palette::TextPrimary;
	this->Cursor = CursorKind::Hand;
}

CursorKind KpiCard::QueryCursor(int localX, int localY)
{
	(void)localX;
	(void)localY;
	return (Enable && Clickable) ? CursorKind::Hand : CursorKind::Arrow;
}

void KpiCard::SetSparkline(const std::vector<double>& values)
{
	SparklineValues = values;
	InvalidateVisual();
}

D2D1_COLOR_F KpiCard::GetTrendColor() const
{
	switch (TrendDirection)
	{
	case KpiTrendDirection::Up: return PositiveColor;
	case KpiTrendDirection::Down: return NegativeColor;
	case KpiTrendDirection::Neutral:
	default: return NeutralColor;
	}
}

void KpiCard::DrawSparkline(D2DGraphics* d2d, const D2D1_RECT_F& rect)
{
	if (!ShowSparkline || SparklineValues.size() < 2 || RectWidth(rect) <= 4.0f || RectHeight(rect) <= 4.0f)
		return;

	double minValue = SparklineValues[0];
	double maxValue = SparklineValues[0];
	for (double v : SparklineValues)
	{
		if (!std::isfinite(v)) continue;
		minValue = (std::min)(minValue, v);
		maxValue = (std::max)(maxValue, v);
	}
	if (std::fabs(maxValue - minValue) < 0.000001)
	{
		minValue -= 1.0;
		maxValue += 1.0;
	}

	D2D1_COLOR_F lineColor = GetTrendColor();
	D2D1_RECT_F fillRect = rect;
	fillRect.top = rect.top + RectHeight(rect) * 0.34f;
	d2d->FillRoundRect(fillRect, SparklineFillColor, 4.0f);

	const float step = RectWidth(rect) / (float)(SparklineValues.size() - 1);
	D2D1_POINT_2F prev{};
	bool hasPrev = false;
	for (size_t i = 0; i < SparklineValues.size(); ++i)
	{
		double v = SparklineValues[i];
		if (!std::isfinite(v)) continue;
		double t = (v - minValue) / (maxValue - minValue);
		t = std::clamp(t, 0.0, 1.0);
		D2D1_POINT_2F p{
			rect.left + step * (float)i,
			rect.bottom - (float)t * RectHeight(rect)
		};
		if (hasPrev)
			d2d->DrawLine(prev, p, lineColor, 2.2f);
		prev = p;
		hasPrev = true;
	}
	d2d->FillEllipse(prev.x, prev.y, 3.0f, 3.0f, lineColor);
}

void KpiCard::Update()
{
	if (!IsVisual) return;
	auto d2d = ParentForm->Render;
	const auto size = GetActualSizeDip();
	float width = size.width;
	float height = size.height;
	bool hot = ParentForm->UnderMouse == this;

	BeginRender();
	D2D1_COLOR_F base = Active ? ActiveBackColor : SurfaceColor;
	d2d->FillRoundRect(Border * 0.5f, Border * 0.5f, width - Border, height - Border, base, CornerRadius);
	if (hot)
		d2d->FillRoundRect(0, 0, width, height, HoverColor, CornerRadius);
	d2d->DrawRoundRect(Border * 0.5f, Border * 0.5f, width - Border, height - Border, Active ? AccentColor : BorderColor, Active ? 1.6f : Border, CornerRadius);

	const float pad = 14.0f;
	d2d->DrawString(Title, pad, 10.0f, (std::max)(1.0f, width - pad * 2.0f), 18.0f, MutedTextColor, Font);

	auto valueSize = Font->GetTextSize(Value);
	float valueY = 38.0f;
	d2d->DrawString(Value, pad, valueY, (std::max)(1.0f, width - pad * 2.0f - 28.0f), 28.0f, ForeColor, Font);
	if (!Unit.empty())
		d2d->DrawString(Unit, (std::min)(width - 26.0f, pad + valueSize.width + 5.0f), valueY + 5.0f, MutedTextColor, Font);

	if (!TrendText.empty())
	{
		D2D1_COLOR_F trend = GetTrendColor();
		auto trendSize = Font->GetTextSize(TrendText);
		float pillW = (std::min)(width - pad * 2.0f, trendSize.width + 16.0f);
		float pillX = width - pad - pillW;
		float pillY = 13.0f;
		D2D1_COLOR_F pill = trend;
		pill.a = 0.16f;
		d2d->FillRoundRect(pillX, pillY, pillW, 22.0f, pill, 11.0f);
		d2d->DrawString(TrendText, pillX + 8.0f, pillY + 3.0f, trend, Font);
	}

	if (!Caption.empty())
		d2d->DrawString(Caption, pad, height - 23.0f, (std::max)(1.0f, width - pad * 2.0f), 18.0f, MutedTextColor, Font);

	DrawSparkline(d2d, D2D1::RectF(pad, (std::max)(70.0f, height - 46.0f), width - pad, height - 22.0f));

	if (!Enable)
		d2d->FillRoundRect(0, 0, width, height, D2D1_COLOR_F{ 1,1,1,0.45f }, CornerRadius);
	EndRender();
}

bool KpiCard::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY)
{
	if (!Enable || !Visible) return true;
	(void)lParam;
	switch (message)
	{
	case WM_MOUSEMOVE:
	{
		if (ParentForm) ParentForm->UnderMouse = this;
		MouseEventArgs e(MouseButtons::None, 0, localX, localY, HIWORD(wParam));
		OnMouseMove(this, e);
		break;
	}
	case WM_LBUTTONDOWN:
	{
		_pressed = true;
		if (ParentForm) ParentForm->SetSelectedControl(this, false);
		MouseEventArgs e(MouseButtons::Left, 0, localX, localY, HIWORD(wParam));
		OnMouseDown(this, e);
		InvalidateVisual();
		break;
	}
	case WM_LBUTTONUP:
	{
		bool clicked = _pressed && localX >= 0 && localY >= 0 && localX <= Width && localY <= Height;
		_pressed = false;
		if (clicked && Clickable)
		{
			if (ToggleActiveOnClick)
				Active = !Active;
			OnCardClick(this);
			MouseEventArgs click(MouseButtons::Left, 1, localX, localY, HIWORD(wParam));
			OnMouseClick(this, click);
		}
		MouseEventArgs e(MouseButtons::Left, 0, localX, localY, HIWORD(wParam));
		OnMouseUp(this, e);
		if (ParentForm && ParentForm->Selected == this)
			ParentForm->SetSelectedControl(nullptr, false);
		InvalidateVisual();
		break;
	}
	case WM_LBUTTONDBLCLK:
	{
		MouseEventArgs e(MouseButtons::Left, 2, localX, localY, HIWORD(wParam));
		OnMouseDoubleClick(this, e);
		break;
	}
	default:
		return Control::ProcessMessage(message, wParam, lParam, localX, localY);
	}
	return true;
}
