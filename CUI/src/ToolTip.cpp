#include "ToolTip.h"
#include "Form.h"
#include <algorithm>

namespace
{
	float EaseOutCubic(float t)
	{
		t = (std::clamp)(t, 0.0f, 1.0f);
		const float inv = 1.0f - t;
		return 1.0f - inv * inv * inv;
	}

	D2D1_COLOR_F FadeColor(D2D1_COLOR_F color, float alpha)
	{
		color.a *= (std::clamp)(alpha, 0.0f, 1.0f);
		return color;
	}
}

UIClass ToolTip::Type() { return UIClass::UI_ToolTip; }

ToolTip::ToolTip(std::wstring text)
{
	this->Text = text;
	this->BackColor = D2D1_COLOR_F{ 0, 0, 0, 0 };
	this->BorderColor = D2D1_COLOR_F{ 0, 0, 0, 0 };
	this->ForeColor = PopupTextColor;
	this->Enable = false;
	this->Visible = true;
	this->Location = POINT{ 0, 0 };
	this->Size = SIZE{ 0, 0 };
}

SIZE ToolTip::ActualSize()
{
	if (!this->ParentForm)
		return SIZE{ 0, 0 };
	return this->ParentForm->ClientSize;
}

POINT ToolTip::CalcPopupOrigin()
{
	POINT origin{ 0, 0 };
	if (!_target || !this->ParentForm)
		return origin;

	auto targetRect = _target->AbsRect;
	auto font = this->Font;
	auto textSize = font->GetTextSize(this->Text);
	float popupW = textSize.width + PaddingX * 2.0f;
	float popupH = textSize.height + PaddingY * 2.0f;
	float x = targetRect.left + (float)PopupOffsetX;
	float y = targetRect.bottom + (float)PopupOffsetY;
	float maxW = (float)this->ParentForm->ClientSize.cx;
	float maxH = (float)this->ParentForm->ClientSize.cy;
	if (x + popupW > maxW)
		x = (std::max)(0.0f, maxW - popupW - 8.0f);
	if (y + popupH > maxH)
		y = (std::max)(0.0f, targetRect.top - popupH - 6.0f);
	if (x < 0.0f) x = 0.0f;
	if (y < 0.0f) y = 0.0f;
	origin.x = (LONG)x;
	origin.y = (LONG)y;
	return origin;
}

float ToolTip::CurrentPopupProgress()
{
	if (!_popupVisible)
	{
		_popupAnimating = false;
		_popupProgress = 0.0f;
		return _popupProgress;
	}

	if (!_popupAnimating)
	{
		_popupProgress = 1.0f;
		return _popupProgress;
	}

	const ULONGLONG now = ::GetTickCount64();
	const ULONGLONG elapsed = now >= _popupAnimStartTick ? (now - _popupAnimStartTick) : 0;
	const UINT duration = EffectiveAnimationDuration(PopupAnimationDurationMs);
	float t = duration > 0 ? (float)elapsed / (float)duration : 1.0f;
	if (t >= 1.0f)
	{
		_popupProgress = _popupTargetProgress;
		_popupAnimating = false;
		return _popupProgress;
	}

	t = EaseOutCubic(t);
	_popupProgress = _popupStartProgress + (_popupTargetProgress - _popupStartProgress) * t;
	return _popupProgress;
}

void ToolTip::BeginPopupReveal(float startProgress)
{
	_popupStartProgress = (std::clamp)(startProgress, 0.0f, 1.0f);
	_popupTargetProgress = 1.0f;
	_popupProgress = _popupStartProgress;
	_popupAnimStartTick = ::GetTickCount64();
	_popupAnimating = EffectiveAnimationDuration(PopupAnimationDurationMs) > 0
		&& _popupStartProgress < _popupTargetProgress;
	if (!_popupAnimating)
		_popupProgress = _popupTargetProgress;
}

bool ToolTip::IsAnimationRunning()
{
	CurrentPopupProgress();
	return _popupAnimating;
}

bool ToolTip::GetAnimatedInvalidRect(D2D1_RECT_F& outRect)
{
	if (!_popupAnimating)
		return false;
	outRect = this->AbsRect;
	return true;
}

void ToolTip::Update()
{
	if (!this->IsVisual || !_popupVisible || !this->ParentForm || !_target || !_target->IsVisual)
		return;

	auto d2d = this->ParentForm->Render;
	const auto size = this->GetActualSizeDip();
	auto font = this->Font;
	auto textSize = font->GetTextSize(this->Text);
	float popupW = textSize.width + PaddingX * 2.0f;
	float popupH = textSize.height + PaddingY * 2.0f;
	POINT origin = CalcPopupOrigin();
	const float progress = CurrentPopupProgress();
	const float alpha = 0.25f + 0.75f * progress;
	const float lift = (1.0f - progress) * 4.0f;
	const float maxX = (float)this->ParentForm->ClientSize.cx - popupW;
	const float maxY = (float)this->ParentForm->ClientSize.cy - popupH - (std::max)(0.0f, ShadowOffsetY);
	const float drawX = (std::clamp)((float)origin.x, 0.0f, (std::max)(0.0f, maxX));
	const float drawY = (std::clamp)((float)origin.y + lift, 0.0f, (std::max)(0.0f, maxY));

	this->BeginRender(size.width, size.height);
	{
		if (PopupShadowColor.a > 0.0f && ShadowOffsetY > 0.0f)
		{
			d2d->FillRoundRect(drawX + 1.0f, drawY + ShadowOffsetY, popupW, popupH, FadeColor(PopupShadowColor, alpha), CornerRadius);
		}
		d2d->FillRoundRect(drawX, drawY, popupW, popupH, FadeColor(PopupBackColor, alpha), CornerRadius);
		if (Border > 0.0f && PopupBorderColor.a > 0.0f)
			d2d->DrawRoundRect(drawX, drawY, popupW, popupH, FadeColor(PopupBorderColor, alpha), Border, CornerRadius);
		d2d->DrawString(this->Text, drawX + PaddingX, drawY + PaddingY, FadeColor(PopupTextColor, alpha), font);
	}
	this->EndRender();
}

void ToolTip::Bind(class Control* target)
{
	_target = target;
	if (!_target)
		return;

	_target->OnMouseEnter += [this](class Control* sender, MouseEventArgs e)
		{
			(void)sender;
			(void)e;
			this->Show();
		};
	_target->OnMouseLeave += [this](class Control* sender, MouseEventArgs e)
		{
			(void)sender;
			(void)e;
			this->Hide();
		};
	_target->OnMouseDown += [this](class Control* sender, MouseEventArgs e)
		{
			(void)sender;
			(void)e;
			this->Hide();
		};
	_target->OnMouseWheel += [this](class Control* sender, MouseEventArgs e)
		{
			(void)sender;
			(void)e;
			this->Hide();
		};
}

void ToolTip::Bind(class Control* target, const std::wstring& text)
{
	Bind(target);
	this->Text = text;
}

void ToolTip::Show()
{
	if (!_target || !_target->ParentForm || !_target->IsVisual)
		return;
	_popupVisible = true;
	BeginPopupReveal(0.12f);
	if (this->ParentForm)
		this->ParentForm->Invalidate(false);
}

void ToolTip::Hide()
{
	if (!_popupVisible)
		return;
	_popupVisible = false;
	_popupAnimating = false;
	_popupProgress = 0.0f;
	if (this->ParentForm)
		this->ParentForm->Invalidate(false);
}
