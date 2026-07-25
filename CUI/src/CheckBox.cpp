#pragma once
#include "CheckBox.h"
#include "Layout/OverlayLayout.h"
#include "Window.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace
{
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

	D2D1_COLOR_F WithAlpha(D2D1_COLOR_F color, float alpha)
	{
		color.a *= (std::clamp)(alpha, 0.0f, 1.0f);
		return color;
	}
}

UIClass CheckBox::Type() { return UIClass::UI_CheckBox; }

CheckBox::CheckBox()
{
	this->RendererBackgroundColor = D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 0.0f };
}

void CheckBox::ConfigureContentVisual(Control& child)
{
	ContentControl::ConfigureContentVisual(child);
}

void CheckBox::StartCheckAnimation(bool checked)
{
	CurrentCheckProgress();
	this->IsChecked = checked;
	_animStartProgress = _checkProgress;
	_animTargetProgress = checked ? 1.0f : 0.0f;
	if (EffectiveAnimationDuration(_animDurationMs) == 0
		|| std::fabs(_animTargetProgress - _animStartProgress) < 0.001f)
	{
		_checkProgress = _animTargetProgress;
		_animating = false;
		return;
	}
	_animStartTick = ::GetTickCount64();
	_animating = true;
}

float CheckBox::CurrentCheckProgress()
{
	if (!_animating)
	{
		_checkProgress = this->IsChecked ? 1.0f : 0.0f;
		return _checkProgress;
	}

	const ULONGLONG now = ::GetTickCount64();
	const ULONGLONG elapsed = now >= _animStartTick ? (now - _animStartTick) : 0;
	const UINT duration = EffectiveAnimationDuration(_animDurationMs);
	float t = duration > 0 ? (float)elapsed / (float)duration : 1.0f;
	if (t >= 1.0f)
	{
		_checkProgress = _animTargetProgress;
		_animating = false;
		return _checkProgress;
	}
	t = 1.0f - std::pow(1.0f - (std::clamp)(t, 0.0f, 1.0f), 3.0f);
	_checkProgress = _animStartProgress + (_animTargetProgress - _animStartProgress) * t;
	return _checkProgress;
}

bool CheckBox::IsAnimationRunning()
{
	CurrentCheckProgress();
	return _animating;
}

bool CheckBox::GetAnimatedInvalidRect(D2D1_RECT_F& outRect)
{
	if (!IsAnimationRunning()) return false;
	outRect = GetAbsoluteBoundsDip();
	return true;
}

cui::core::Size CheckBox::MeasureCore(
	const cui::core::Constraints& available)
{
	if (GetControlTemplateRoot())
		return ContentControl::MeasureCore(available);
	const auto padding = GetSpecifiedLayout().padding;
	const auto inner = available.Deflate(padding).Normalized();
	const float fontHeight = GetRenderFont() ? GetRenderFont()->FontHeight : 14.0f;
	const float box = (std::max)(14.0f, fontHeight * 0.82f);
	auto* content = GetVisualContent();
	if (!content) content = GetGeneratedPresenter();
	cui::core::Size desired{};
	const float contentOffset = content ? box + TextGap : box;
	if (content && !content->IsCollapsed())
	{
		const auto margin = content->GetSpecifiedLayout().margin;
		desired = content->Measure(cui::core::Constraints{
			cui::core::Size{},
			cui::core::Size{
				(std::max)(0.0f, inner.maximum.width
					- contentOffset - margin.Horizontal()),
				(std::max)(0.0f, inner.maximum.height
					- margin.Vertical()) } });
		desired.width += margin.Horizontal();
		desired.height += margin.Vertical();
	}
	return {
		contentOffset + desired.width + padding.Horizontal(),
		(std::max)(desired.height, box + 2.0f) + padding.Vertical() };
}

void CheckBox::PerformPendingLayout()
{
	if (IsLayoutSuspended() || !_contentLayoutPending) return;
	if (GetControlTemplateRoot())
	{
		ContentControl::PerformPendingLayout();
		return;
	}
	auto* content = GetVisualContent();
	if (!content) content = GetGeneratedPresenter();
	if (content)
	{
		const auto size = GetActualSizeDip();
		const auto padding = GetSpecifiedLayout().padding;
		const float fontHeight = GetRenderFont() ? GetRenderFont()->FontHeight : 14.0f;
		const float box = (std::max)(14.0f, fontHeight * 0.82f);
		const float offset = box + TextGap;
		const std::array<Control*, 1> children{ content };
		cui::layout::ArrangeOverlayChildren(children, cui::core::Rect{
			padding.left + offset,
			padding.top,
			(std::max)(0.0f,
				size.width - padding.Horizontal() - offset),
			(std::max)(0.0f, size.height - padding.Vertical()) });
	}
	_contentLayoutPending = false;
}

void CheckBox::OnRender()
{
	if (this->IsVisible == false)return;
	const bool isUnderMouse = this->IsMouseOver;
	auto d2d = this->GetDrawingContext();
	const auto size = this->GetActualSizeDip();
	float clipW = lastMeasuredWidth > size.width ? lastMeasuredWidth : size.width;
	this->BeginRender(clipW, size.height);
	{
		auto font = this->GetRenderFont();
		const float progress = CurrentCheckProgress();
		const float box = (std::max)(14.0f, font->FontHeight * 0.82f);
		const float x = 0.5f;
		const float y = (size.height - box) * 0.5f;
		const float radius = (std::min)(BoxCornerRadius, box * 0.35f);
		const auto boxColor = LerpColor(BoxBackColor, CheckedBackColor, progress);
		const auto borderColor = LerpColor(BoxBorderColor, CheckedBackColor, progress);

		d2d->FillRoundRect(x, y, box, box, boxColor, radius);
		if (isUnderMouse && UnderMouseColor.a > 0.0f)
			d2d->FillRoundRect(x, y, box, box, UnderMouseColor, radius);
		const float border = BorderThickness.MaxEdge() > 0.0f
			? BorderThickness.MaxEdge() : 1.5f;
		if (borderColor.a > 0.0f)
			d2d->DrawRoundRect(x, y, box, box,
				borderColor, border, radius);

		if (progress > 0.001f)
		{
			const float stroke = (std::max)(1.7f, box * 0.13f);
			const float x1 = x + box * 0.27f;
			const float y1 = y + box * 0.52f;
			const float x2 = x + box * 0.43f;
			const float y2 = y + box * 0.68f;
			const float x3 = x + box * 0.74f;
			const float y3 = y + box * 0.34f;
			const auto markColor = WithAlpha(CheckMarkColor, progress);
			d2d->DrawLine(x1, y1, x2, y2, markColor, stroke);
			d2d->DrawLine(x2, y2, x3, y3, markColor, stroke);
		}

	}
	if (!this->IsEnabled)
	{
		d2d->FillRoundRect(0.0f, 0.0f, clipW, size.height, DisabledOverlayColor, 4.0f);
	}
	this->EndRender();
	lastMeasuredWidth = size.width;
}

void CheckBox::BeforeDefaultMouseUp(MouseButton button, MouseEventArgs& e, bool hasMatchingPress)
{
	(void)e;
	if (button == MouseButton::Left && hasMatchingPress)
		StartCheckAnimation(!this->IsChecked);
}

bool CheckBox::Invoke()
{
	if (!IsEnabled || !IsVisible) return false;
	StartCheckAnimation(!IsChecked);
	RoutedEventArgs eventArgs;
	Click(this, eventArgs);
	InvalidateVisual();
	return true;
}

void CheckBox::SetChecked(bool checked)
{
	if (!IsEnabled || IsChecked == checked) return;
	StartCheckAnimation(checked);
	InvalidateVisual();
}

void CheckBox::Toggle()
{
	SetChecked(!IsChecked);
}
