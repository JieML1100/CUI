#define NOMINMAX
#include "Toast.h"
#include "Form.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace
{
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

	static D2D1_COLOR_F ScaleAlpha(D2D1_COLOR_F color, float alpha)
	{
		color.a *= alpha;
		return color;
	}

	static float TextTop(Font* font, const D2D1_RECT_F& rect)
	{
		const float fontHeight = font ? font->FontHeight : 16.0f;
		return rect.top + std::max(0.0f, (RectHeight(rect) - fontHeight) * 0.5f);
	}
}

ToastItem::ToastItem(std::wstring title, std::wstring message, ToastKind kind, UINT durationMs)
	: Title(std::move(title)), Message(std::move(message)), Kind(kind), DurationMs(durationMs)
{
}

UIClass ToastHost::Type()
{
	return UIClass::UI_ToastHost;
}

ToastHost::ToastHost(int x, int y, int width, int height)
{
	this->Location = { x, y };
	this->Size = { width, height };
	this->BackColor = D2D1_COLOR_F{ 1, 1, 1, 0 };
	this->BorderColor = D2D1_COLOR_F{ 1, 1, 1, 0 };
}

D2D1_COLOR_F ToastHost::KindColor(ToastKind kind) const
{
	switch (kind)
	{
	case ToastKind::Success: return this->SuccessColor;
	case ToastKind::Warning: return this->WarningColor;
	case ToastKind::Error: return this->ErrorColor;
	case ToastKind::Info:
	default: return this->InfoColor;
	}
}

int ToastHost::ShowToast(const std::wstring& title, const std::wstring& message, ToastKind kind, UINT durationMs)
{
	ToastItem toast(title, message, kind, durationMs == 0 ? this->DefaultDurationMs : durationMs);
	toast.CreatedTick = GetTickCount64();
	this->Toasts.push_back(std::move(toast));
	if (this->MaxVisible > 0)
	{
		while ((int)this->Toasts.size() > std::max(this->MaxVisible, 1) * 3)
			this->Toasts.erase(this->Toasts.begin());
	}
	this->InvalidateVisual();
	return (int)this->Toasts.size() - 1;
}

bool ToastHost::DismissToast(int index)
{
	if (index < 0 || index >= (int)this->Toasts.size()) return false;
	if (this->Toasts[index].Dismissing) return true;
	this->Toasts[index].Dismissing = true;
	this->Toasts[index].DismissStartTick = GetTickCount64();
	this->HoveredIndex = -1;
	this->PressedCloseIndex = -1;
	this->InvalidateVisual();
	return true;
}

void ToastHost::ClearToasts()
{
	this->Toasts.clear();
	this->HoveredIndex = -1;
	this->PressedCloseIndex = -1;
	this->InvalidateVisual();
}

size_t ToastHost::ToastCount() const
{
	return this->Toasts.size();
}

bool ToastHost::RemoveExpired()
{
	UINT64 now = GetTickCount64();
	bool changed = false;
	for (int i = (int)this->Toasts.size() - 1; i >= 0; i--)
	{
		auto& toast = this->Toasts[i];
		if (toast.Dismissing)
		{
			if (now - toast.DismissStartTick >= toast.DismissDurationMs)
			{
				this->Toasts.erase(this->Toasts.begin() + i);
				this->OnToastDismissed(this, i);
				changed = true;
			}
			continue;
		}
		if (!toast.Persistent && toast.DurationMs > 0 && now - toast.CreatedTick >= toast.DurationMs)
		{
			toast.Dismissing = true;
			toast.DismissStartTick = now;
			changed = true;
		}
	}
	if (changed)
	{
		this->HoveredIndex = -1;
		this->PressedCloseIndex = -1;
	}
	return changed;
}

std::vector<int> ToastHost::VisibleIndices() const
{
	std::vector<int> indices;
	int count = (int)this->Toasts.size();
	if (count <= 0) return indices;
	int maxVisible = this->MaxVisible <= 0 ? count : std::min(this->MaxVisible, count);
	indices.reserve(maxVisible);
	if (this->NewestOnTop)
	{
		for (int i = count - 1; i >= 0 && (int)indices.size() < maxVisible; i--)
			indices.push_back(i);
	}
	else
	{
		int start = std::max(0, count - maxVisible);
		for (int i = start; i < count; i++)
			indices.push_back(i);
	}
	return indices;
}

D2D1_RECT_F ToastHost::GetToastRect(int visibleOrdinal) const
{
	float width = std::min(this->ToastWidth, std::max(1.0f, (float)this->_size.cx - this->Padding * 2.0f));
	float height = std::min(this->ToastHeight, std::max(32.0f, (float)this->_size.cy - this->Padding * 2.0f));
	float x = std::max(this->Padding, (float)this->_size.cx - this->Padding - width);
	float y = this->Padding + visibleOrdinal * (height + this->Gap);
	return D2D1::RectF(x, y, x + width, y + height);
}

D2D1_RECT_F ToastHost::GetCloseRect(const D2D1_RECT_F& toastRect) const
{
	float size = 22.0f;
	return D2D1::RectF(toastRect.right - size - 8.0f, toastRect.top + 8.0f, toastRect.right - 8.0f, toastRect.top + 8.0f + size);
}

int ToastHost::HitTestToast(int xof, int yof) const
{
	auto indices = VisibleIndices();
	for (int i = 0; i < (int)indices.size(); i++)
	{
		if (indices[i] < 0 || indices[i] >= (int)this->Toasts.size() || this->Toasts[indices[i]].Dismissing)
			continue;
		if (PtInRectF(GetToastRect(i), (float)xof, (float)yof))
			return indices[i];
	}
	return -1;
}

int ToastHost::HitTestClose(int xof, int yof) const
{
	if (!this->ShowCloseButton) return -1;
	auto indices = VisibleIndices();
	for (int i = 0; i < (int)indices.size(); i++)
	{
		if (indices[i] < 0 || indices[i] >= (int)this->Toasts.size() || this->Toasts[indices[i]].Dismissing)
			continue;
		auto rect = GetToastRect(i);
		if (PtInRectF(GetCloseRect(rect), (float)xof, (float)yof))
			return indices[i];
	}
	return -1;
}

CursorKind ToastHost::QueryCursor(int xof, int yof)
{
	return HitTestToast(xof, yof) >= 0 ? CursorKind::Hand : CursorKind::Arrow;
}

bool ToastHost::IsAnimationRunning()
{
	return !this->Toasts.empty();
}

bool ToastHost::GetAnimatedInvalidRect(D2D1_RECT_F& outRect)
{
	outRect = this->AbsRect;
	return true;
}

void ToastHost::Update()
{
	if (!this->IsVisual) return;
	auto d2d = this->ParentForm ? this->ParentForm->Render : nullptr;
	if (!d2d) return;
	bool changed = RemoveExpired();
	UINT64 now = GetTickCount64();
	auto indices = VisibleIndices();
	class Font* fontObj = this->_font;

	this->BeginRender();
	{
		for (int i = 0; i < (int)indices.size(); i++)
		{
			int index = indices[i];
			if (index < 0 || index >= (int)this->Toasts.size()) continue;
			const auto& toast = this->Toasts[index];
			auto rect = GetToastRect(i);
			float dismissProgress = 0.0f;
			if (toast.Dismissing)
			{
				dismissProgress = toast.DismissDurationMs > 0
					? std::clamp((float)(now - toast.DismissStartTick) / (float)toast.DismissDurationMs, 0.0f, 1.0f)
					: 1.0f;
				rect.top -= 18.0f * dismissProgress;
				rect.bottom -= 18.0f * dismissProgress;
			}
			float alpha = 1.0f - dismissProgress;
			if (alpha <= 0.01f) continue;
			if (rect.top >= (float)this->_size.cy || rect.bottom <= 0.0f) continue;

			auto accent = ScaleAlpha(KindColor(toast.Kind), alpha);
			D2D1_COLOR_F back = ScaleAlpha(this->ToastBackColor, alpha);
			D2D1_COLOR_F border = ScaleAlpha(this->ToastBorderColor, alpha);
			D2D1_COLOR_F titleColor = ScaleAlpha(this->TitleColor, alpha);
			D2D1_COLOR_F messageColor = ScaleAlpha(this->MessageColor, alpha);
			if (!toast.Dismissing && index == this->HoveredIndex)
				back.a = std::min(1.0f, back.a + 0.04f);
			d2d->FillRoundRect(rect, back, this->CornerRadius);
			d2d->DrawRoundRect(rect, border, 1.0f, this->CornerRadius);
			float accentInset = std::clamp(this->AccentInsetY, 0.0f, RectHeight(rect) * 0.35f);
			float accentHeight = std::max(1.0f, RectHeight(rect) - accentInset * 2.0f);
			d2d->FillRoundRect(rect.left, rect.top + accentInset, this->AccentWidth, accentHeight, accent, this->AccentWidth * 0.5f);

			float textLeft = rect.left + this->AccentWidth + 12.0f;
			float textRight = rect.right - (this->ShowCloseButton ? 42.0f : 12.0f);
			auto titleRect = D2D1::RectF(textLeft, rect.top + 9.0f, textRight, rect.top + 32.0f);
			auto msgRect = D2D1::RectF(textLeft, rect.top + 36.0f, textRight, rect.bottom - 10.0f);
			if (toast.Title.empty())
				msgRect.top = rect.top + 12.0f;

			if (!toast.Title.empty())
			{
				d2d->PushDrawRect(titleRect.left, titleRect.top, std::max(1.0f, RectWidth(titleRect)), RectHeight(titleRect));
				d2d->DrawString(toast.Title, titleRect.left, TextTop(fontObj, titleRect),
					std::max(1.0f, RectWidth(titleRect)), RectHeight(titleRect), titleColor, fontObj);
				d2d->PopDrawRect();
			}

			if (!toast.Message.empty() && RectHeight(msgRect) > 1.0f)
			{
				d2d->PushDrawRect(msgRect.left, msgRect.top, std::max(1.0f, RectWidth(msgRect)), RectHeight(msgRect));
				d2d->DrawString(toast.Message, msgRect.left, msgRect.top,
					std::max(1.0f, RectWidth(msgRect)), RectHeight(msgRect), messageColor, fontObj);
				d2d->PopDrawRect();
			}

			if (this->ShowCloseButton)
			{
				auto closeRect = GetCloseRect(rect);
				if (!toast.Dismissing && index == this->HoveredIndex)
					d2d->FillRoundRect(closeRect, ScaleAlpha(this->CloseHoverColor, alpha), 4.0f);
				auto p1 = D2D1::Point2F(closeRect.left + 7.0f, closeRect.top + 7.0f);
				auto p2 = D2D1::Point2F(closeRect.right - 7.0f, closeRect.bottom - 7.0f);
				auto p3 = D2D1::Point2F(closeRect.right - 7.0f, closeRect.top + 7.0f);
				auto p4 = D2D1::Point2F(closeRect.left + 7.0f, closeRect.bottom - 7.0f);
				d2d->DrawLine(p1, p2, messageColor, 1.5f);
				d2d->DrawLine(p3, p4, messageColor, 1.5f);
			}
		}
	}
	this->EndRender();

	if (changed)
		this->InvalidateVisual();
}

bool ToastHost::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof)
{
	if (!this->Enable || !this->Visible) return true;

	switch (message)
	{
	case WM_MOUSEMOVE:
	{
		if (this->ParentForm)
			this->ParentForm->UnderMouse = this;
		int hit = HitTestToast(xof, yof);
		if (hit != this->HoveredIndex)
		{
			this->HoveredIndex = hit;
			this->InvalidateVisual();
		}
		MouseEventArgs e(MouseButtons::None, 0, xof, yof, HIWORD(wParam));
		this->OnMouseMove(this, e);
		return true;
	}
	case WM_LBUTTONDOWN:
	{
		if (this->ParentForm)
			this->ParentForm->SetSelectedControl(this, false);
		this->PressedCloseIndex = HitTestClose(xof, yof);
		MouseEventArgs e(MouseButtons::Left, 0, xof, yof, HIWORD(wParam));
		this->OnMouseDown(this, e);
		return true;
	}
	case WM_LBUTTONUP:
	{
		int closeHit = HitTestClose(xof, yof);
		if (closeHit >= 0 && closeHit == this->PressedCloseIndex)
			DismissToast(closeHit);
		else
		{
			int hit = HitTestToast(xof, yof);
			if (hit >= 0)
			{
				this->OnToastClick(this, hit);
				MouseEventArgs click(MouseButtons::Left, 0, xof, yof, HIWORD(wParam));
				this->OnMouseClick(this, click);
			}
		}
		this->PressedCloseIndex = -1;
		MouseEventArgs e(MouseButtons::Left, 0, xof, yof, HIWORD(wParam));
		this->OnMouseUp(this, e);
		return true;
	}
	default:
		break;
	}

	return Control::ProcessMessage(message, wParam, lParam, xof, yof);
}
