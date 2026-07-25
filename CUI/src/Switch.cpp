#pragma once
#include "Switch.h"
#include "Window.h"
#include <algorithm>
#include <cmath>
UIClass Switch::Type() { return UIClass::UI_Switch; }

static D2D1_COLOR_F LerpColor(const D2D1_COLOR_F& from, const D2D1_COLOR_F& to, float t)
{
	t = (std::clamp)(t, 0.0f, 1.0f);
	return D2D1_COLOR_F{
		from.r + (to.r - from.r) * t,
		from.g + (to.g - from.g) * t,
		from.b + (to.b - from.b) * t,
		from.a + (to.a - from.a) * t
	};
}

static D2D1_COLOR_F WithAlpha(D2D1_COLOR_F color, float alpha)
{
	color.a *= (std::clamp)(alpha, 0.0f, 1.0f);
	return color;
}

Switch::Switch()
{
	auto bc = this->RendererBackgroundColor;
	bc.a = 0.0f;
	this->RendererBackgroundColor = bc;
	SyncAnimationState();
}

void Switch::SyncAnimationState()
{
	_thumbProgress = this->IsChecked ? 1.0f : 0.0f;
	_animStartProgress = _thumbProgress;
	_animTargetProgress = _thumbProgress;
	_animating = false;
	_animStartTick = 0;
}

void Switch::StartToggleAnimation(bool checked)
{
	CurrentThumbProgress();
	this->IsChecked = checked;
	_animStartProgress = _thumbProgress;
	_animTargetProgress = checked ? 1.0f : 0.0f;
	if (EffectiveAnimationDuration(_animDurationMs) == 0
		|| std::fabs(_animTargetProgress - _animStartProgress) < 0.001f)
	{
		_thumbProgress = _animTargetProgress;
		_animating = false;
		return;
	}
	_animStartTick = ::GetTickCount64();
	_animating = true;
}

float Switch::CurrentThumbProgress()
{
	if (!_animating)
	{
		_thumbProgress = this->IsChecked ? 1.0f : 0.0f;
		return _thumbProgress;
	}

	const ULONGLONG now = ::GetTickCount64();
	const ULONGLONG elapsed = now >= _animStartTick ? (now - _animStartTick) : 0;
	const UINT duration = EffectiveAnimationDuration(_animDurationMs);
	float t = duration > 0 ? (float)elapsed / (float)duration : 1.0f;
	if (t >= 1.0f)
	{
		_thumbProgress = _animTargetProgress;
		_animating = false;
		return _thumbProgress;
	}
	t = 1.0f - std::pow(1.0f - (std::clamp)(t, 0.0f, 1.0f), 3.0f);
	_thumbProgress = _animStartProgress + (_animTargetProgress - _animStartProgress) * t;
	return _thumbProgress;
}

bool Switch::IsAnimationRunning()
{
	CurrentThumbProgress();
	return _animating;
}

bool Switch::GetAnimatedInvalidRect(D2D1_RECT_F& outRect)
{
	if (!IsAnimationRunning()) return false;
	outRect = GetAbsoluteBoundsDip();
	return true;
}
void Switch::OnRender()
{
	if (this->IsVisible == false)return;
	auto d2d = this->GetDrawingContext();
	const auto size = this->GetActualSizeDip();
	const float actualWidth = size.width;
	const float actualHeight = size.height;
	float clipW = lastMeasuredWidth > actualWidth ? lastMeasuredWidth : actualWidth;
	this->BeginRender(clipW, actualHeight);
	{
		const float progress = CurrentThumbProgress();
		const bool hover = this->IsMouseOver;
		const bool pressed = HasControlStyleState(
			this->GetStyleState(), ControlStyleState::Pressed);
		const float trackRadius = actualHeight * 0.5f;
		const float pad = (std::clamp)(TrackPadding, 1.5f, actualHeight * 0.35f);
		const float thumbDiameter = (std::max)(4.0f, actualHeight - pad * 2.0f);
		const float thumbRadius = thumbDiameter * 0.5f;
		const float thumbTravel = (std::max)(0.0f, actualWidth - pad * 2.0f - thumbDiameter);
		const float thumbCenterX = pad + thumbRadius + thumbTravel * progress;
		const float thumbCenterY = actualHeight * 0.5f;
		const float thumbStretch = (pressed || _animating) ? 2.0f : 0.0f;
		const float thumbW = thumbDiameter + thumbStretch;
		const float thumbL = (std::clamp)(thumbCenterX - thumbW * 0.5f, pad, actualWidth - pad - thumbW);
		const float thumbT = pad;
		const auto trackColor = LerpColor(TrackOffColor, TrackOnColor, progress);

		d2d->FillRoundRect(0.0f, 0.0f, actualWidth, actualHeight, trackColor, trackRadius);
		if (hover && UnderMouseColor.a > 0.0f)
			d2d->FillRoundRect(1.0f, 1.0f, actualWidth - 2.0f, actualHeight - 2.0f, UnderMouseColor, (std::max)(0.0f, trackRadius - 1.0f));
		const float border = BorderThickness.MaxEdge() > 0.0f
			? BorderThickness.MaxEdge() : 1.0f;
		if (TrackBorderColor.a > 0.0f)
			d2d->DrawRoundRect(0.5f, 0.5f,
				actualWidth - 1.0f, actualHeight - 1.0f,
				TrackBorderColor, border, trackRadius);

		if (ThumbShadowColor.a > 0.0f)
			d2d->FillRoundRect(thumbL, thumbT + 1.0f, thumbW, thumbDiameter, WithAlpha(ThumbShadowColor, pressed ? 0.34f : 0.22f), thumbRadius);
		d2d->FillRoundRect(thumbL, thumbT, thumbW, thumbDiameter, ThumbColor, thumbRadius);
	}
	if (!this->IsEnabled)
	{
		d2d->FillRoundRect(0.0f, 0.0f, clipW, actualHeight, DisabledOverlayColor, actualHeight * 0.5f);
	}
	this->EndRender();
	lastMeasuredWidth = actualWidth;
}

void Switch::BeforeDefaultMouseUp(MouseButton button, MouseEventArgs& e, bool hasMatchingPress)
{
	(void)e;
	if (button == MouseButton::Left && hasMatchingPress)
		StartToggleAnimation(!this->IsChecked);
}

void Switch::SetChecked(bool checked)
{
	if (!IsEnabled || IsChecked == checked) return;
	StartToggleAnimation(checked);
	InvalidateVisual();
}

void Switch::Toggle()
{
	SetChecked(!IsChecked);
}

bool Switch::Invoke()
{
	if (!IsEnabled || !IsVisible) return false;
	StartToggleAnimation(!IsChecked);
	RoutedEventArgs eventArgs;
	Click(this, eventArgs);
	InvalidateVisual();
	return true;
}
