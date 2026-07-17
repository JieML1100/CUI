#define NOMINMAX
#include "AnchorPickerPopup.h"
#include "Form.h"

#include <algorithm>
#include <cmath>
#include <cwctype>

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
		return x >= rect.left && x <= rect.right
			&& y >= rect.top && y <= rect.bottom;
	}

	static float EaseOutCubic(float t)
	{
		t = std::clamp(t, 0.0f, 1.0f);
		return 1.0f - std::pow(1.0f - t, 3.0f);
	}

	static std::wstring Trim(std::wstring text)
	{
		while (!text.empty() && iswspace(text.front())) text.erase(text.begin());
		while (!text.empty() && iswspace(text.back())) text.pop_back();
		return text;
	}

	static std::wstring Lower(std::wstring text)
	{
		for (auto& ch : text) ch = static_cast<wchar_t>(towlower(ch));
		return text;
	}

	static SIZE GetTopLevelContentSize(Form* form)
	{
		if (!form) return SIZE{ 1, 1 };
		if (form->Handle)
		{
			RECT rc{};
			if (::GetClientRect(form->Handle, &rc))
			{
				float scale = form->GetDpiScale();
				if (scale <= 0.0f) scale = 1.0f;
				const int width = static_cast<int>(std::floor(
					static_cast<float>(rc.right - rc.left) / scale));
				const int height = static_cast<int>(std::floor(
					static_cast<float>(std::max<LONG>(0,
						rc.bottom - rc.top - form->ClientTop())) / scale));
				return SIZE{ std::max(1, width), std::max(1, height) };
			}
		}
		const auto fallback = form->ClientSize;
		return SIZE{ std::max<LONG>(1, fallback.cx),
			std::max<LONG>(1, fallback.cy) };
	}

	static SIZE FitPopupSizeToWindow(Form* form, SIZE preferred)
	{
		const auto content = GetTopLevelContentSize(form);
		return SIZE{
			std::min(static_cast<int>(preferred.cx),
				std::max(1, static_cast<int>(content.cx) - 4)),
			std::min(static_cast<int>(preferred.cy),
				std::max(1, static_cast<int>(content.cy) - 4))
		};
	}

	static uint8_t EdgeAt(int index)
	{
		static constexpr uint8_t edges[] = {
			AnchorStyles::Top,
			AnchorStyles::Left,
			AnchorStyles::Right,
			AnchorStyles::Bottom
		};
		return index >= 0 && index < 4 ? edges[index] : AnchorStyles::None;
	}
}

AnchorPickerPopup::AnchorPickerPopup(int width, int height)
{
	_preferredSize = SIZE{ width, height };
	this->Location = POINT{ 0,0 };
	this->Size = SIZE{ width,height };
	this->BackColor = D2D1_COLOR_F{ 0,0,0,0 };
	this->BorderColor = D2D1_COLOR_F{ 0,0,0,0 };
	this->Visible = false;
	this->Cursor = CursorKind::Arrow;
}

bool AnchorPickerPopup::TryParseAnchors(
	const std::wstring& text, uint8_t& out)
{
	auto value = Trim(text);
	if (value.empty()) return false;

	auto tryNumber = [&out](const std::wstring& number) -> bool
	{
		wchar_t* end = nullptr;
		const long parsed = wcstol(number.c_str(), &end, 10);
		if (end == number.c_str()) return false;
		while (end && *end && iswspace(*end)) ++end;
		if (!end || *end != L'\0' || parsed < 0 || parsed > 15) return false;
		out = static_cast<uint8_t>(parsed);
		return true;
	};
	if (tryNumber(value)) return true;

	const auto open = value.find_last_of(L'(');
	const auto close = value.find_last_of(L')');
	if (open != std::wstring::npos && close == value.size() - 1
		&& open + 1 < close && tryNumber(value.substr(open + 1, close - open - 1)))
		return true;

	value = Lower(value);
	if (value == L"none" || value == L"\u65e0")
	{
		out = AnchorStyles::None;
		return true;
	}
	uint8_t parsed = AnchorStyles::None;
	if (value.find(L"left") != std::wstring::npos
		|| value.find(L'\u5de6') != std::wstring::npos) parsed |= AnchorStyles::Left;
	if (value.find(L"top") != std::wstring::npos
		|| value.find(L'\u4e0a') != std::wstring::npos) parsed |= AnchorStyles::Top;
	if (value.find(L"right") != std::wstring::npos
		|| value.find(L'\u53f3') != std::wstring::npos) parsed |= AnchorStyles::Right;
	if (value.find(L"bottom") != std::wstring::npos
		|| value.find(L'\u4e0b') != std::wstring::npos) parsed |= AnchorStyles::Bottom;
	if (parsed == AnchorStyles::None) return false;
	out = parsed;
	return true;
}

std::wstring AnchorPickerPopup::AnchorToString(uint8_t anchors)
{
	anchors &= 0x0f;
	if (anchors == AnchorStyles::None) return L"None";
	std::wstring result;
	auto append = [&result](const wchar_t* text)
	{
		if (!result.empty()) result += L", ";
		result += text;
	};
	if ((anchors & AnchorStyles::Top) != 0) append(L"Top");
	if ((anchors & AnchorStyles::Left) != 0) append(L"Left");
	if ((anchors & AnchorStyles::Right) != 0) append(L"Right");
	if ((anchors & AnchorStyles::Bottom) != 0) append(L"Bottom");
	return result;
}

std::wstring AnchorPickerPopup::CurrentValueText() const
{
	return std::to_wstring(this->SelectedAnchors & 0x0f);
}

void AnchorPickerPopup::SetSelectedAnchors(uint8_t anchors)
{
	this->SelectedAnchors = anchors & 0x0f;
	this->InvalidateVisual();
}

bool AnchorPickerPopup::ToggleAnchor(uint8_t edge)
{
	edge &= 0x0f;
	if (edge != AnchorStyles::Left && edge != AnchorStyles::Top
		&& edge != AnchorStyles::Right && edge != AnchorStyles::Bottom)
		return false;
	this->SelectedAnchors = (this->SelectedAnchors ^ edge) & 0x0f;
	NotifyChanged();
	return true;
}

void AnchorPickerPopup::NotifyChanged()
{
	this->OnAnchorChanged(
		this, this->SelectedAnchors, CurrentValueText());
	this->InvalidateVisual();
}

AnchorPickerPopup::Layout AnchorPickerPopup::CalcLayout() const
{
	Layout layout{};
	const float width = static_cast<float>(this->_size.cx);
	const float height = static_cast<float>(this->_size.cy);
	const float pad = 14.0f;
	const float centerX = width * 0.5f;

	layout.DiagramPanelRect = D2D1::RectF(
		pad, 58.0f, width - pad, std::min(188.0f, height - 70.0f));
	layout.ParentRect = D2D1::RectF(
		layout.DiagramPanelRect.left + 24.0f,
		layout.DiagramPanelRect.top + 18.0f,
		layout.DiagramPanelRect.right - 24.0f,
		layout.DiagramPanelRect.bottom - 18.0f);
	const float childWidth = std::min(70.0f,
		std::max(42.0f, RectWidth(layout.ParentRect) * 0.42f));
	const float childHeight = std::min(46.0f,
		std::max(30.0f, RectHeight(layout.ParentRect) * 0.46f));
	const float centerY = (layout.ParentRect.top + layout.ParentRect.bottom) * 0.5f;
	layout.ChildRect = D2D1::RectF(
		centerX - childWidth * 0.5f, centerY - childHeight * 0.5f,
		centerX + childWidth * 0.5f, centerY + childHeight * 0.5f);
	layout.EdgeRects[0] = D2D1::RectF(
		centerX - 18.0f, layout.ParentRect.top - 8.0f,
		centerX + 18.0f, layout.ChildRect.top + 7.0f);
	layout.EdgeRects[1] = D2D1::RectF(
		layout.ParentRect.left - 8.0f, centerY - 18.0f,
		layout.ChildRect.left + 7.0f, centerY + 18.0f);
	layout.EdgeRects[2] = D2D1::RectF(
		layout.ChildRect.right - 7.0f, centerY - 18.0f,
		layout.ParentRect.right + 8.0f, centerY + 18.0f);
	layout.EdgeRects[3] = D2D1::RectF(
		centerX - 18.0f, layout.ChildRect.bottom - 7.0f,
		centerX + 18.0f, layout.ParentRect.bottom + 8.0f);

	const float buttonHeight = 30.0f;
	const float buttonTop = std::max(220.0f, height - pad - buttonHeight);
	layout.SummaryRect = D2D1::RectF(
		pad, layout.DiagramPanelRect.bottom + 8.0f,
		width - pad, buttonTop - 8.0f);
	layout.NoneRect = D2D1::RectF(
		pad, buttonTop, pad + 64.0f, buttonTop + buttonHeight);
	layout.OkRect = D2D1::RectF(
		width - pad - 56.0f, buttonTop,
		width - pad, buttonTop + buttonHeight);
	return layout;
}

float AnchorPickerPopup::CurrentDropProgress()
{
	if (!_animating)
	{
		_dropProgress = _expanded ? 1.0f : 0.0f;
		return _dropProgress;
	}
	const UINT64 now = GetTickCount64();
	const UINT64 elapsed = now >= _animStartTick ? now - _animStartTick : 0;
	const UINT duration = EffectiveAnimationDuration(_animDurationMs);
	float t = duration > 0 ? static_cast<float>(elapsed) / duration : 1.0f;
	if (t >= 1.0f)
	{
		const bool wasCollapsing = _animTargetProgress <= 0.001f;
		_dropProgress = _animTargetProgress;
		_animating = false;
		if (wasCollapsing) FinishCollapsed();
		return _dropProgress;
	}
	t = EaseOutCubic(t);
	_dropProgress = _animStartProgress
		+ (_animTargetProgress - _animStartProgress) * t;
	return _dropProgress;
}

void AnchorPickerPopup::SetExpanded(bool expanded)
{
	CurrentDropProgress();
	_expanded = expanded;
	_animStartProgress = _dropProgress;
	_animTargetProgress = expanded ? 1.0f : 0.0f;
	_collapseCleanupPending = !expanded;
	if (std::fabs(_animTargetProgress - _animStartProgress) < 0.001f)
	{
		_dropProgress = _animTargetProgress;
		_animating = false;
		if (!expanded) FinishCollapsed();
	}
	else
	{
		_animStartTick = GetTickCount64();
		_animating = true;
	}
	if (this->ParentForm) this->ParentForm->Invalidate(true);
	this->InvalidateVisual();
}

void AnchorPickerPopup::FinishCollapsed()
{
	Form* form = this->ParentForm;
	Control* owner = _owner;
	_dropProgress = 0.0f;
	_animating = false;
	_collapseCleanupPending = false;
	_expanded = false;
	_visiblePopup = false;
	this->Visible = false;
	_hasAnchorRect = false;
	_hoverEdge = -1;
	_hoverNone = _hoverOk = false;
	if (form && form->ForegroundControl == this)
		form->ForegroundControl = nullptr;
	if (form && form->Selected == this)
		form->SetSelectedControl(owner && owner->IsVisual ? owner : nullptr, false);
	_owner = nullptr;
	if (form) form->Invalidate(true);
}

bool AnchorPickerPopup::IsAnimationRunning()
{
	CurrentDropProgress();
	return _animating || _collapseCleanupPending;
}

bool AnchorPickerPopup::GetAnimatedInvalidRect(D2D1_RECT_F& outRect)
{
	if (!this->Visible && !_collapseCleanupPending) return false;
	outRect = this->AbsRect;
	if (_hasAnchorRect)
	{
		const auto loc = this->GetAbsoluteLocationDip();
		const auto anchorAbs = D2D1::RectF(
			loc.x + _anchorRect.left, loc.y + _anchorRect.top,
			loc.x + _anchorRect.right, loc.y + _anchorRect.bottom);
		outRect.left = std::min(outRect.left, anchorAbs.left);
		outRect.top = std::min(outRect.top, anchorAbs.top);
		outRect.right = std::max(outRect.right, anchorAbs.right);
		outRect.bottom = std::max(outRect.bottom, anchorAbs.bottom);
	}
	return true;
}

bool AnchorPickerPopup::ContainsPoint(int localX, int localY)
{
	if (Control::ContainsPoint(localX, localY)) return true;
	return _hasAnchorRect
		&& PtInRectF(_anchorRect, static_cast<float>(localX), static_cast<float>(localY));
}

void AnchorPickerPopup::ShowAt(
	Form* form, int x, int y, uint8_t initialAnchors)
{
	if (!form) return;
	_owner = nullptr;
	this->ParentForm = form;
	this->Visible = true;
	_visiblePopup = true;
	_hasAnchorRect = false;
	_focusEdge = 0;
	SetSelectedAnchors(initialAnchors);
	const auto client = GetTopLevelContentSize(form);
	this->Size = FitPopupSizeToWindow(form, _preferredSize);
	const int minX = client.cx > this->Width + 4 ? 2 : 0;
	const int minY = client.cy > this->Height + 4 ? 2 : 0;
	const int maxX = std::max(minX, static_cast<int>(client.cx) - this->Width - minX);
	const int maxY = std::max(minY, static_cast<int>(client.cy) - this->Height - minY);
	this->SetRuntimeLocation(POINT{
		std::clamp(x, minX, maxX), std::clamp(y, minY, maxY) });
	if (form->ForegroundControl && form->ForegroundControl != this
		&& form->ForegroundControl->AutoCloseOnOutsideClick())
		form->ForegroundControl->ClosePopup();
	form->ForegroundControl = this;
	form->SetSelectedControl(this, false);
	SetExpanded(true);
}

void AnchorPickerPopup::ShowAt(
	Control* relativeTo, const D2D1_RECT_F& anchorRect, uint8_t initialAnchors)
{
	if (!relativeTo || !relativeTo->ParentForm) return;
	const auto popupSize = FitPopupSizeToWindow(
		relativeTo->ParentForm, _preferredSize);
	const auto abs = relativeTo->GetAbsoluteLocationDip();
	const int x = static_cast<int>(std::round(abs.x + anchorRect.left));
	const int below = static_cast<int>(std::round(abs.y + anchorRect.bottom + 4.0f));
	const int above = static_cast<int>(std::round(
		abs.y + anchorRect.top - popupSize.cy - 4.0f));
	const auto client = GetTopLevelContentSize(relativeTo->ParentForm);
	const int y = below + popupSize.cy <= client.cy - 2 || above < 2
		? below : above;
	ShowAt(relativeTo->ParentForm, x, y, initialAnchors);
	_owner = relativeTo;
	const auto location = this->GetActualLocationDip();
	_anchorRect = D2D1::RectF(
		abs.x + anchorRect.left - location.x,
		abs.y + anchorRect.top - location.y,
		abs.x + anchorRect.right - location.x,
		abs.y + anchorRect.bottom - location.y);
	_hasAnchorRect = true;
}

void AnchorPickerPopup::Hide(bool confirm)
{
	if (!_visiblePopup && !this->Visible && !_animating) return;
	if (!_expanded && _animating && _animTargetProgress <= 0.001f) return;
	if (confirm) Confirm();
	else this->OnCancelled(this);
	SetExpanded(false);
}

void AnchorPickerPopup::Confirm()
{
	this->OnAnchorConfirmed(
		this, this->SelectedAnchors, CurrentValueText());
}

void AnchorPickerPopup::UpdateHover(int localX, int localY)
{
	const auto layout = CalcLayout();
	_hoverEdge = -1;
	for (int index = 0; index < 4; ++index)
	{
		if (PtInRectF(layout.EdgeRects[index],
			static_cast<float>(localX), static_cast<float>(localY)))
		{
			_hoverEdge = index;
			break;
		}
	}
	_hoverNone = PtInRectF(layout.NoneRect,
		static_cast<float>(localX), static_cast<float>(localY));
	_hoverOk = PtInRectF(layout.OkRect,
		static_cast<float>(localX), static_cast<float>(localY));
	this->InvalidateVisual();
}

CursorKind AnchorPickerPopup::QueryCursor(int localX, int localY)
{
	if (!Control::ContainsPoint(localX, localY) && _hasAnchorRect
		&& PtInRectF(_anchorRect, static_cast<float>(localX), static_cast<float>(localY)))
		return CursorKind::Hand;
	const auto layout = CalcLayout();
	for (const auto& rect : layout.EdgeRects)
		if (PtInRectF(rect, static_cast<float>(localX), static_cast<float>(localY)))
			return CursorKind::Hand;
	if (PtInRectF(layout.NoneRect, static_cast<float>(localX), static_cast<float>(localY))
		|| PtInRectF(layout.OkRect, static_cast<float>(localX), static_cast<float>(localY)))
		return CursorKind::Hand;
	return CursorKind::Arrow;
}

void AnchorPickerPopup::Update()
{
	if (!this->IsVisual || !this->Visible) return;
	auto d2d = this->ParentForm ? this->ParentForm->Render : nullptr;
	if (!d2d) return;
	const auto layout = CalcLayout();
	const auto size = this->GetActualSizeDip();
	const float width = size.width;
	const float height = size.height;
	const float progress = std::clamp(CurrentDropProgress(), 0.0f, 1.0f);
	const float renderHeight = height * progress;
	class Font* fontObj = this->Font;

	this->BeginRender();
	{
		if (renderHeight <= 0.001f)
		{
			this->EndRender();
			return;
		}
		d2d->PushDrawRect(0.0f, 0.0f, width, renderHeight);
		d2d->FillRoundRect(0.0f, 0.0f, width, height,
			this->PanelBackColor, 8.0f);
		d2d->DrawString(L"Anchor", 14.0f, 12.0f,
			width - 28.0f, 22.0f, this->TextColor, fontObj);
		d2d->DrawString(L"Click an edge to toggle it", 14.0f, 34.0f,
			width - 28.0f, 18.0f, this->MutedTextColor, fontObj);

		d2d->FillRoundRect(layout.DiagramPanelRect,
			D2D1_COLOR_F{ 0.085f,0.085f,0.095f,1.0f }, 6.0f);
		d2d->DrawRoundRect(layout.DiagramPanelRect,
			D2D1_COLOR_F{ 0.30f,0.31f,0.34f,1.0f }, 1.0f, 6.0f);
		d2d->DrawRoundRect(layout.ParentRect,
			D2D1_COLOR_F{ 0.49f,0.51f,0.56f,1.0f }, 1.2f, 3.0f);
		d2d->FillRoundRect(layout.ChildRect,
			D2D1_COLOR_F{ 0.20f,0.21f,0.24f,1.0f }, 4.0f);
		d2d->DrawRoundRect(layout.ChildRect,
			D2D1_COLOR_F{ 0.70f,0.72f,0.77f,1.0f }, 1.2f, 4.0f);

		const float centerX = (layout.ParentRect.left + layout.ParentRect.right) * 0.5f;
		const float centerY = (layout.ParentRect.top + layout.ParentRect.bottom) * 0.5f;
		for (int index = 0; index < 4; ++index)
		{
			const uint8_t edge = EdgeAt(index);
			const bool active = (this->SelectedAnchors & edge) != 0;
			const bool hover = _hoverEdge == index || _focusEdge == index;
			if (hover)
				d2d->FillRoundRect(layout.EdgeRects[index],
					D2D1_COLOR_F{ AccentColor.r, AccentColor.g, AccentColor.b, 0.10f }, 5.0f);
			const auto color = active ? this->AccentColor : this->MutedTextColor;
			const float thickness = active ? 3.0f : 1.4f;
			D2D1_POINT_2F start{};
			D2D1_POINT_2F end{};
			if (index == 0)
			{
				start = D2D1::Point2F(centerX, layout.ParentRect.top + 1.0f);
				end = D2D1::Point2F(centerX, layout.ChildRect.top);
			}
			else if (index == 1)
			{
				start = D2D1::Point2F(layout.ParentRect.left + 1.0f, centerY);
				end = D2D1::Point2F(layout.ChildRect.left, centerY);
			}
			else if (index == 2)
			{
				start = D2D1::Point2F(layout.ChildRect.right, centerY);
				end = D2D1::Point2F(layout.ParentRect.right - 1.0f, centerY);
			}
			else
			{
				start = D2D1::Point2F(centerX, layout.ChildRect.bottom);
				end = D2D1::Point2F(centerX, layout.ParentRect.bottom - 1.0f);
			}
			d2d->DrawLine(start, end, color, thickness);
			const float cap = active ? 4.0f : 3.0f;
			d2d->FillRect(D2D1::RectF(
				start.x - cap, start.y - cap, start.x + cap, start.y + cap), color);
			d2d->FillRect(D2D1::RectF(
				end.x - cap, end.y - cap, end.x + cap, end.y + cap), color);
		}

		d2d->FillRoundRect(layout.SummaryRect,
			D2D1_COLOR_F{ 0.10f,0.10f,0.11f,1.0f }, 4.0f);
		d2d->DrawRoundRect(layout.SummaryRect,
			D2D1_COLOR_F{ 0.33f,0.34f,0.37f,1.0f }, 1.0f, 4.0f);
		d2d->DrawString(AnchorToString(this->SelectedAnchors),
			layout.SummaryRect.left + 8.0f, layout.SummaryRect.top + 4.0f,
			std::max(1.0f, RectWidth(layout.SummaryRect) - 16.0f),
			std::max(1.0f, RectHeight(layout.SummaryRect) - 8.0f),
			this->TextColor, fontObj);

		auto drawButton = [&](const D2D1_RECT_F& rect,
			const std::wstring& text, bool hover, bool accent)
		{
			auto back = accent
				? D2D1_COLOR_F{ AccentColor.r, AccentColor.g, AccentColor.b, 0.82f }
				: this->ButtonBackColor;
			if (hover)
			{
				back.r = std::min(1.0f, back.r + 0.07f);
				back.g = std::min(1.0f, back.g + 0.07f);
				back.b = std::min(1.0f, back.b + 0.07f);
			}
			d2d->FillRoundRect(rect, back, 4.0f);
			d2d->DrawRoundRect(rect,
				accent ? this->AccentColor : this->ButtonBorderColor, 1.0f, 4.0f);
			const auto textSize = fontObj
				? fontObj->GetTextSize(text) : D2D1_SIZE_F{ 36.0f,16.0f };
			d2d->DrawString(text,
				rect.left + std::max(0.0f, (RectWidth(rect) - textSize.width) * 0.5f),
				rect.top + std::max(0.0f, (RectHeight(rect) - textSize.height) * 0.5f),
				this->TextColor, fontObj);
		};
		drawButton(layout.NoneRect, L"None", _hoverNone, false);
		drawButton(layout.OkRect, L"OK", _hoverOk, true);
		d2d->DrawRoundRect(0.5f, 0.5f, width - 1.0f, height - 1.0f,
			this->PanelBorderColor, 1.0f, 8.0f);
		d2d->PopDrawRect();
	}
	this->EndRender();
	if (IsAnimationRunning()) this->InvalidateVisual();
}

bool AnchorPickerPopup::ProcessMessage(
	UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY)
{
	(void)lParam;
	if (!this->Enable || !this->Visible) return true;
	const bool inPanel = Control::ContainsPoint(localX, localY);
	if (!inPanel && _hasAnchorRect
		&& PtInRectF(_anchorRect, static_cast<float>(localX), static_cast<float>(localY)))
	{
		if (_expanded && message == WM_LBUTTONDOWN)
		{
			Hide(false);
			MouseEventArgs e(MouseButtons::Left, 0, localX, localY, HIWORD(wParam));
			this->OnMouseDown(this, e);
		}
		return true;
	}

	switch (message)
	{
	case WM_MOUSEMOVE:
	{
		if (this->ParentForm) this->ParentForm->UnderMouse = this;
		UpdateHover(localX, localY);
		MouseEventArgs e(MouseButtons::None, 0, localX, localY, HIWORD(wParam));
		this->OnMouseMove(this, e);
		return true;
	}
	case WM_LBUTTONDOWN:
	{
		UpdateHover(localX, localY);
		if (_hoverEdge >= 0)
		{
			_focusEdge = _hoverEdge;
			ToggleAnchor(EdgeAt(_hoverEdge));
		}
		else if (_hoverNone)
		{
			if (this->SelectedAnchors != AnchorStyles::None)
			{
				this->SelectedAnchors = AnchorStyles::None;
				NotifyChanged();
			}
		}
		else if (_hoverOk)
			Hide(true);
		MouseEventArgs e(MouseButtons::Left, 0, localX, localY, HIWORD(wParam));
		this->OnMouseDown(this, e);
		return true;
	}
	case WM_LBUTTONUP:
	{
		MouseEventArgs e(MouseButtons::Left, 0, localX, localY, HIWORD(wParam));
		this->OnMouseUp(this, e);
		return true;
	}
	case WM_KEYDOWN:
	{
		if (wParam == VK_ESCAPE) Hide(false);
		else if (wParam == VK_RETURN) Hide(true);
		else if (wParam == VK_SPACE) ToggleAnchor(EdgeAt(_focusEdge));
		else if (wParam == VK_UP) _focusEdge = 0;
		else if (wParam == VK_LEFT) _focusEdge = 1;
		else if (wParam == VK_RIGHT) _focusEdge = 2;
		else if (wParam == VK_DOWN) _focusEdge = 3;
		this->InvalidateVisual();
		KeyEventArgs e(static_cast<Keys>(wParam | 0));
		this->OnKeyDown(this, e);
		return true;
	}
	default:
		break;
	}
	return Control::ProcessMessage(message, wParam, lParam, localX, localY);
}
