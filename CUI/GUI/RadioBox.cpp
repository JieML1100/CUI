#pragma once
#include "RadioBox.h"
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

UIClass RadioBox::Type() { return UIClass::UI_RadioBox; }

RadioBox::RadioBox(std::wstring text, int x, int y)
{
	this->Text = text;
	this->Location = POINT{ x,y };
	this->BackColor = D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 0.0f };
	this->Cursor = CursorKind::Hand;
}

void RadioBox::StartSelectionAnimation(bool checked)
{
	CurrentSelectionProgress();
	this->Checked = checked;
	_animStartProgress = _selectProgress;
	_animTargetProgress = checked ? 1.0f : 0.0f;
	if (std::fabs(_animTargetProgress - _animStartProgress) < 0.001f)
	{
		_selectProgress = _animTargetProgress;
		_animating = false;
		return;
	}
	_animStartTick = ::GetTickCount64();
	_animating = true;
}

float RadioBox::CurrentSelectionProgress()
{
	if (!_animating)
	{
		_selectProgress = this->Checked ? 1.0f : 0.0f;
		return _selectProgress;
	}

	const ULONGLONG now = ::GetTickCount64();
	const ULONGLONG elapsed = now >= _animStartTick ? (now - _animStartTick) : 0;
	float t = _animDurationMs > 0 ? (float)elapsed / (float)_animDurationMs : 1.0f;
	if (t >= 1.0f)
	{
		_selectProgress = _animTargetProgress;
		_animating = false;
		return _selectProgress;
	}
	t = 1.0f - std::pow(1.0f - (std::clamp)(t, 0.0f, 1.0f), 3.0f);
	_selectProgress = _animStartProgress + (_animTargetProgress - _animStartProgress) * t;
	return _selectProgress;
}

bool RadioBox::IsAnimationRunning()
{
	CurrentSelectionProgress();
	return _animating;
}

bool RadioBox::GetAnimatedInvalidRect(D2D1_RECT_F& outRect)
{
	if (!IsAnimationRunning()) return false;
	outRect = this->AbsRect;
	return true;
}

SIZE RadioBox::ActualSize()
{
	auto font = this->Font;
	auto textSize = font->GetTextSize(this->Text);
	const int circle = (std::max)(14, (int)std::ceil(textSize.height * 0.82f));
	const int height = (std::max)((int)std::ceil(textSize.height), circle + 2);
	const int width = circle + (int)std::ceil(TextGap) + (int)std::ceil(textSize.width);
	return SIZE{ width, height };
}

void RadioBox::Update()
{
	if (this->IsVisual == false)return;
	const bool isUnderMouse = this->ParentForm->UnderMouse == this;
	auto d2d = this->ParentForm->Render;
	auto size = this->ActualSize();
	float clipW = last_width > size.cx ? last_width : (float)size.cx;
	this->BeginRender(clipW, (float)size.cy);
	{
		auto font = this->Font;
		auto textSize = font->GetTextSize(this->Text);
		const float progress = CurrentSelectionProgress();
		const float circle = (std::max)(14.0f, textSize.height * 0.82f);
		const float radius = circle * 0.5f;
		const float cx = radius + 0.5f;
		const float cy = (float)size.cy * 0.5f;
		const auto backColor = LerpColor(CircleBackColor, WithAlpha(SelectedColor, 0.18f), progress);
		const auto borderColor = LerpColor(CircleBorderColor, SelectedColor, progress);

		d2d->FillEllipse(cx, cy, radius, radius, backColor);
		if (isUnderMouse && UnderMouseColor.a > 0.0f)
			d2d->FillEllipse(cx, cy, radius, radius, UnderMouseColor);
		if (BorderThickness > 0.0f && borderColor.a > 0.0f)
			d2d->DrawEllipse(cx, cy, radius, radius, borderColor, BorderThickness);

		if (progress > 0.001f)
		{
			const float dotRadius = radius * (0.24f + 0.22f * progress);
			d2d->FillEllipse(cx, cy, dotRadius, dotRadius, WithAlpha(SelectedColor, progress));
			const float innerDot = dotRadius * 0.52f;
			d2d->FillEllipse(cx, cy, innerDot, innerDot, WithAlpha(DotColor, progress));
		}

		const float textY = ((float)size.cy - textSize.height) * 0.5f;
		d2d->DrawString(this->Text, circle + TextGap, (std::max)(0.0f, textY), this->ForeColor, font);
	}
	if (!this->Enable)
	{
		d2d->FillRoundRect(0.0f, 0.0f, clipW, (float)size.cy, DisabledOverlayColor, 4.0f);
	}
	this->EndRender();
	last_width = static_cast<float>(size.cx);
}

bool RadioBox::DefaultRaiseMouseDoubleClick(UINT message, bool wasSelected) const
{
	(void)message;
	return wasSelected;
}

bool RadioBox::DefaultInvalidateVisualOnMouseDoubleClick(UINT message, bool wasSelected) const
{
	(void)message;
	return wasSelected;
}

void RadioBox::BeforeDefaultMouseUp(UINT message, MouseEventArgs& e, bool wasSelected)
{
	(void)e;
	if (message == WM_LBUTTONUP && wasSelected && this->Checked == false)
	{
		StartSelectionAnimation(true);
		this->OnChecked(this);
	}
}

void RadioBox::BeforeDefaultMouseDoubleClick(UINT message, MouseEventArgs& e, bool wasSelected)
{
	(void)e;
	if (message == WM_LBUTTONDBLCLK && wasSelected && this->Checked == false)
	{
		StartSelectionAnimation(true);
		this->OnChecked(this);
	}
}
