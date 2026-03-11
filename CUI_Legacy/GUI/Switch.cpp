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
	float clipW = last_width > size.cx ? (float)last_width : (float)size.cx;
	this->BeginRender(clipW, (float)size.cy);
	{
		const float progress = CurrentThumbProgress();
		float r = size.cy / 2.0f;
		float x1 = r;
		float x2 = size.cx - r;
		float y = r;
		const float thumbX = x1 + (x2 - x1) * progress;
		auto trackColor = LerpColor(Colors::Red, Colors::Green, progress);
		d2d->FillEllipse({ x1,y }, r, r, trackColor);
		d2d->FillEllipse({ x2,y }, r, r, trackColor);
		d2d->FillRect(x1, 0, x2 - x1, size.cy, trackColor);
		d2d->FillEllipse({ thumbX, y }, r - 2.0f, r - 2.0f, Colors::White);
	}
	if (!this->Enable)
	{
		d2d->FillRect(0, 0, clipW, (float)size.cy, { 1.0f ,1.0f ,1.0f ,0.5f });
	}
	this->EndRender();
	last_width = size.cx;
}

bool Switch::DefaultRaiseMouseDoubleClick(UINT message, bool wasSelected) const
{
	(void)message;
	return wasSelected;
}

bool Switch::DefaultPostRenderOnMouseDoubleClick(UINT message, bool wasSelected) const
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