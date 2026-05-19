#pragma once
#include "Switch.h"
#include "Form.h"
#include <algorithm>
#include <cmath>
#pragma comment(lib, "Imm32.lib")
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

Switch::Switch(int x, int y, int width, int height)
{
	this->Location = POINT{ x,y };
	this->Size = SIZE{ width,height };
	auto bc = this->BackColor;
	bc.a = 0.0f;
	this->BackColor = bc;
	this->Cursor = CursorKind::Hand;
	SyncAnimationState();
}

void Switch::SyncAnimationState()
{
	_thumbProgress = this->Checked ? 1.0f : 0.0f;
	_animStartProgress = _thumbProgress;
	_animTargetProgress = _thumbProgress;
	_animating = false;
	_animStartTick = 0;
}

void Switch::StartToggleAnimation(bool checked)
{
	CurrentThumbProgress();
	this->Checked = checked;
	_animStartProgress = _thumbProgress;
	_animTargetProgress = checked ? 1.0f : 0.0f;
	if (std::fabs(_animTargetProgress - _animStartProgress) < 0.001f)
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
		_thumbProgress = this->Checked ? 1.0f : 0.0f;
		return _thumbProgress;
	}

	const ULONGLONG now = ::GetTickCount64();
	const ULONGLONG elapsed = now >= _animStartTick ? (now - _animStartTick) : 0;
	float t = _animDurationMs > 0 ? (float)elapsed / (float)_animDurationMs : 1.0f;
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
	outRect = this->AbsRect;
	return true;
}
void Switch::Update()
{
	if (this->IsVisual == false)return;
	auto d2d = this->ParentForm->Render;
	auto size = this->ActualSize();
	const float actualWidth = static_cast<float>(size.cx);
	const float actualHeight = static_cast<float>(size.cy);
	float clipW = last_width > actualWidth ? last_width : actualWidth;
	this->BeginRender(clipW, actualHeight);
	{
		const float progress = CurrentThumbProgress();
		const bool hover = this->ParentForm && this->ParentForm->UnderMouse == this;
		const bool pressed = this->ParentForm && this->ParentForm->Selected == this;
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
		if (BorderThickness > 0.0f && TrackBorderColor.a > 0.0f)
			d2d->DrawRoundRect(0.5f, 0.5f, actualWidth - 1.0f, actualHeight - 1.0f, TrackBorderColor, BorderThickness, trackRadius);

		if (ThumbShadowColor.a > 0.0f)
			d2d->FillRoundRect(thumbL, thumbT + 1.0f, thumbW, thumbDiameter, WithAlpha(ThumbShadowColor, pressed ? 0.34f : 0.22f), thumbRadius);
		d2d->FillRoundRect(thumbL, thumbT, thumbW, thumbDiameter, ThumbColor, thumbRadius);
	}
	if (!this->Enable)
	{
		d2d->FillRoundRect(0.0f, 0.0f, clipW, actualHeight, DisabledOverlayColor, actualHeight * 0.5f);
	}
	this->EndRender();
	last_width = actualWidth;
}

bool Switch::DefaultRaiseMouseDoubleClick(UINT message, bool wasSelected) const
{
	(void)message;
	return wasSelected;
}

bool Switch::DefaultInvalidateVisualOnMouseDoubleClick(UINT message, bool wasSelected) const
{
	(void)message;
	return wasSelected;
}

void Switch::BeforeDefaultMouseUp(UINT message, MouseEventArgs& e, bool wasSelected)
{
	(void)e;
	if (message == WM_LBUTTONUP && wasSelected)
	{
		StartToggleAnimation(!this->Checked);
		this->OnChecked(this);
	}
}

void Switch::BeforeDefaultMouseDoubleClick(UINT message, MouseEventArgs& e, bool wasSelected)
{
	(void)e;
	if (message == WM_LBUTTONDBLCLK && wasSelected)
	{
		StartToggleAnimation(!this->Checked);
		this->OnChecked(this);
	}
}
