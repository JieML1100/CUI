#include "KpiCard.h"
#include "Form.h"
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

KpiCard::KpiCard(int x, int y, int width, int height)
{
	this->Location = POINT{ x, y };
	this->Size = SIZE{ width, height };
	this->BackColor = D2D1_COLOR_F{ 0, 0, 0, 0 };
	this->BolderColor = D2D1_COLOR_F{ 0.60f, 0.66f, 0.76f, 0.42f };
	this->ForeColor = D2D1_COLOR_F{ 0.90f, 0.92f, 0.96f, 1.0f };
	this->Cursor = CursorKind::Hand;
}

CursorKind KpiCard::QueryCursor(int xof, int yof)
{
	(void)xof;
	(void)yof;
	return (Enable && Clickable) ? CursorKind::Hand : CursorKind::Arrow;
}

void KpiCard::SetSparkline(const std::vector<double>& values)
{
	SparklineValues = values;
	PostRender();
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
	auto size = ActualSize();
	float width = (float)size.cx;
	float height = (float)size.cy;
	bool hot = ParentForm->UnderMouse == this;

	BeginRender();
	D2D1_COLOR_F base = Active ? ActiveBackColor : SurfaceColor;
	d2d->FillRoundRect(Border * 0.5f, Border * 0.5f, width - Border, height - Border, base, CornerRadius);
	if (hot)
		d2d->FillRoundRect(0, 0, width, height, HoverColor, CornerRadius);
	d2d->DrawRoundRect(Border * 0.5f, Border * 0.5f, width - Border, height - Border, Active ? AccentColor : BolderColor, Active ? 1.6f : Border, CornerRadius);

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

bool KpiCard::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof)
{
	if (!Enable || !Visible) return true;
	(void)lParam;
	switch (message)
	{
	case WM_MOUSEMOVE:
	{
		if (ParentForm) ParentForm->UnderMouse = this;
		MouseEventArgs e(MouseButtons::None, 0, xof, yof, HIWORD(wParam));
		OnMouseMove(this, e);
		break;
	}
	case WM_LBUTTONDOWN:
	{
		_pressed = true;
		if (ParentForm) ParentForm->SetSelectedControl(this, false);
		MouseEventArgs e(MouseButtons::Left, 0, xof, yof, HIWORD(wParam));
		OnMouseDown(this, e);
		PostRender();
		break;
	}
	case WM_LBUTTONUP:
	{
		bool clicked = _pressed && xof >= 0 && yof >= 0 && xof <= Width && yof <= Height;
		_pressed = false;
		if (clicked && Clickable)
		{
			if (ToggleActiveOnClick)
				Active = !Active;
			OnCardClick(this);
			MouseEventArgs click(MouseButtons::Left, 1, xof, yof, HIWORD(wParam));
			OnMouseClick(this, click);
		}
		MouseEventArgs e(MouseButtons::Left, 0, xof, yof, HIWORD(wParam));
		OnMouseUp(this, e);
		if (ParentForm && ParentForm->Selected == this)
			ParentForm->SetSelectedControl(NULL, false);
		PostRender();
		break;
	}
	case WM_LBUTTONDBLCLK:
	{
		MouseEventArgs e(MouseButtons::Left, 2, xof, yof, HIWORD(wParam));
		OnMouseDoubleClick(this, e);
		break;
	}
	default:
		return Control::ProcessMessage(message, wParam, lParam, xof, yof);
	}
	return true;
}
