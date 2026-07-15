#pragma once
#include "CheckBox.h"
#include "Form.h"
#include <algorithm>
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

CheckBox::CheckBox(std::wstring text, int x, int y)
{
	this->Text = text;
	this->Location = POINT{ x,y };
	this->BackColor = D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 0.0f };
	this->Cursor = CursorKind::Hand;
}

void CheckBox::StartCheckAnimation(bool checked)
{
	CurrentCheckProgress();
	this->Checked = checked;
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
		_checkProgress = this->Checked ? 1.0f : 0.0f;
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
	outRect = this->AbsRect;
	return true;
}

SIZE CheckBox::ActualSize()
{
	auto font = this->Font;
	const auto displayText = GetDisplayText();
	auto textSize = font->GetTextSize(displayText);
	const int box = (std::max)(14, (int)std::ceil(textSize.height * 0.82f));
	const int height = (std::max)((int)std::ceil(textSize.height), box + 2);
	const int width = box + (int)std::ceil(TextGap) + (int)std::ceil(textSize.width);
	return SIZE{ width, height };
}

void CheckBox::Update()
{
	if (this->IsVisual == false)return;
	const bool isUnderMouse = this->ParentForm->UnderMouse == this;
	auto d2d = this->ParentForm->Render;
	const auto size = this->GetActualSizeDip();
	float clipW = lastMeasuredWidth > size.width ? lastMeasuredWidth : size.width;
	this->BeginRender(clipW, size.height);
	{
		auto font = this->Font;
		const auto displayText = GetDisplayText();
		auto textSize = font->GetTextSize(displayText);
		const float progress = CurrentCheckProgress();
		const float box = (std::max)(14.0f, textSize.height * 0.82f);
		const float x = 0.5f;
		const float y = (size.height - box) * 0.5f;
		const float radius = (std::min)(BoxCornerRadius, box * 0.35f);
		const auto boxColor = LerpColor(BoxBackColor, CheckedBackColor, progress);
		const auto borderColor = LerpColor(BoxBorderColor, CheckedBackColor, progress);

		d2d->FillRoundRect(x, y, box, box, boxColor, radius);
		if (isUnderMouse && UnderMouseColor.a > 0.0f)
			d2d->FillRoundRect(x, y, box, box, UnderMouseColor, radius);
		if (Border > 0.0f && borderColor.a > 0.0f)
			d2d->DrawRoundRect(x, y, box, box, borderColor, Border, radius);

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

		const float textY = (size.height - textSize.height) * 0.5f;
		d2d->DrawString(displayText, box + TextGap, (std::max)(0.0f, textY), this->ForeColor, font);
	}
	if (!this->Enable)
	{
		d2d->FillRoundRect(0.0f, 0.0f, clipW, size.height, DisabledOverlayColor, 4.0f);
	}
	this->EndRender();
	lastMeasuredWidth = size.width;
}

bool CheckBox::DefaultRaiseMouseDoubleClick(UINT message, bool wasSelected) const
{
	(void)message;
	return wasSelected;
}

bool CheckBox::DefaultInvalidateVisualOnMouseDoubleClick(UINT message, bool wasSelected) const
{
	(void)message;
	return wasSelected;
}

void CheckBox::BeforeDefaultMouseUp(UINT message, MouseEventArgs& e, bool wasSelected)
{
	(void)e;
	if (message == WM_LBUTTONUP && wasSelected)
	{
		StartCheckAnimation(!this->Checked);
		this->OnChecked(this);
	}
}

void CheckBox::BeforeDefaultMouseDoubleClick(UINT message, MouseEventArgs& e, bool wasSelected)
{
	(void)e;
	if (message == WM_LBUTTONDBLCLK && wasSelected)
	{
		StartCheckAnimation(!this->Checked);
		this->OnChecked(this);
	}
}

bool CheckBox::Invoke()
{
	if (!Enable || !Visible) return false;
	StartCheckAnimation(!Checked);
	OnChecked(this);
	const auto size = GetActualSizeDip();
	OnMouseClick(this, MouseEventArgs(MouseButtons::None, 0,
		static_cast<int>(size.width * 0.5f),
		static_cast<int>(size.height * 0.5f), 0));
	InvalidateVisual();
	return true;
}
