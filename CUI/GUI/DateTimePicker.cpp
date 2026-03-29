#include "DateTimePicker.h"
#include "Form.h"
#include <format>

UIClass DateTimePicker::Type() { return UIClass::UI_DateTimePicker; }

DateTimePicker::DateTimePicker(std::wstring text, int x, int y, int width, int height)
{
	this->Text = text;
	this->Location = POINT{ x, y };
	this->Size = SIZE{ width, height };
	this->BackColor = Colors::LightGray;
	this->BolderColor = Colors::DimGrey;
	this->Cursor = CursorKind::Hand;

	SYSTEMTIME st{};
	::GetLocalTime(&st);
	_value = st;
	_viewYear = (int)st.wYear;
	_viewMonth = (int)st.wMonth;
	_showDate = true;
	_showTime = true;
	UpdateDisplayText();
}

GET_CPP(DateTimePicker, SYSTEMTIME, Value)
{
	return _value;
}

SET_CPP(DateTimePicker, SYSTEMTIME, Value)
{
	SetValueInternal(value, true);
}

GET_CPP(DateTimePicker, DateTimePickerMode, Mode)
{
	if (_showDate && _showTime) return DateTimePickerMode::DateTime;
	if (_showDate) return DateTimePickerMode::DateOnly;
	return DateTimePickerMode::TimeOnly;
}

SET_CPP(DateTimePicker, DateTimePickerMode, Mode)
{
	switch (value)
	{
	case DateTimePickerMode::DateOnly:
		_showDate = true;
		_showTime = false;
		break;
	case DateTimePickerMode::TimeOnly:
		_showDate = false;
		_showTime = true;
		break;
	case DateTimePickerMode::DateTime:
	default:
		_showDate = true;
		_showTime = true;
		break;
	}
	EnsureShowFlags();
	UpdateDisplayText();
	PostRender();
}

GET_CPP(DateTimePicker, bool, AllowDateSelection)
{
	return _allowDate;
}

SET_CPP(DateTimePicker, bool, AllowDateSelection)
{
	_allowDate = value;
	EnsureShowFlags();
	UpdateDisplayText();
	PostRender();
}

GET_CPP(DateTimePicker, bool, AllowTimeSelection)
{
	return _allowTime;
}

SET_CPP(DateTimePicker, bool, AllowTimeSelection)
{
	_allowTime = value;
	EnsureShowFlags();
	UpdateDisplayText();
	PostRender();
}

SIZE DateTimePicker::ActualSize()
{
	if (!Expand || IsInlineTimeMode())
		return this->Size;

	LayoutMetrics layout{};
	if (!GetLayoutMetrics(layout))
		return this->Size;

	SIZE sz = this->Size;
	sz.cy += (LONG)layout.dropHeight;
	return sz;
}

CursorKind DateTimePicker::QueryCursor(int xof, int yof)
{
	if (!this->Enable) return CursorKind::Arrow;
	if (yof >= 0 && yof <= this->Height)
	{
		if (IsInlineTimeMode())
		{
			LayoutMetrics layout{};
			int day = -1;
			if (GetInlineTimeLayout(layout))
			{
				auto part = HitTestPart(layout, xof, yof, day);
				if (part == HitPart::HourField || part == HitPart::MinuteField)
					return CursorKind::IBeam;
				if (part != HitPart::None)
					return CursorKind::Hand;
			}
		}
		return CursorKind::Hand;
	}
	if (!Expand) return CursorKind::Arrow;

	LayoutMetrics layout{};
	if (!GetLayoutMetrics(layout)) return CursorKind::Arrow;
	int day = -1;
	auto part = HitTestPart(layout, xof, yof, day);
	if (part == HitPart::HourField || part == HitPart::MinuteField)
		return CursorKind::IBeam;
	if (part != HitPart::None)
		return CursorKind::Hand;
	return CursorKind::Arrow;
}

void DateTimePicker::Update()
{
	if (!this->IsVisual) return;
	auto d2d = this->ParentForm->Render;
	auto size = this->ActualSize();

	bool isUnderMouse = this->ParentForm->UnderMouse == this;
	bool isSelected = this->ParentForm->Selected == this;
	bool inlineTime = IsInlineTimeMode();
	auto blendColor = [](const D2D1_COLOR_F& a, const D2D1_COLOR_F& b, float t) -> D2D1_COLOR_F
		{
			float clamped = std::clamp(t, 0.0f, 1.0f);
			return D2D1_COLOR_F{
				a.r + (b.r - a.r) * clamped,
				a.g + (b.g - a.g) * clamped,
				a.b + (b.b - a.b) * clamped,
				a.a + (b.a - a.a) * clamped };
		};

	this->BeginRender((float)size.cx, (float)size.cy);
	{
		const float round = (std::min)(this->Round, (float)this->Height * 0.5f);
		D2D1_COLOR_F baseColor = this->BackColor;
		if (isUnderMouse && !this->Expand)
			baseColor = blendColor(this->BackColor, this->HoverColor, 0.22f);

		d2d->FillRoundRect(0, 0, (float)this->Width, (float)this->Height, baseColor, round);

		auto font = this->Font;
		auto drawTimeField = [&](const D2D1_RECT_F& fieldRect, const D2D1_RECT_F& upRect, const D2D1_RECT_F& downRect,
			EditField field, const std::wstring& text, bool hoverUp, bool hoverDown)
			{
				bool showEditHighlight = isSelected && _editField == field;
				D2D1_COLOR_F borderColor = showEditHighlight ? this->FocusBorderColor : this->DropBorderColor;
				float borderWidth = showEditHighlight ? 1.5f : 1.0f;
				d2d->FillRoundRect(fieldRect.left, fieldRect.top, fieldRect.right - fieldRect.left, fieldRect.bottom - fieldRect.top, this->PanelBackColor, 5.0f);

				D2D1_COLOR_F spinBackColor = blendColor(this->PanelBackColor, this->DropBorderColor, 0.18f);
				float spinLeft = (std::min)(upRect.left, downRect.left);
				d2d->FillRect(spinLeft, fieldRect.top + 1.0f, fieldRect.right - spinLeft - 1.0f, fieldRect.bottom - fieldRect.top - 2.0f, spinBackColor);
				if (hoverUp)
					d2d->FillRect(upRect.left, upRect.top, upRect.right - upRect.left, upRect.bottom - upRect.top, this->HoverColor);
				if (hoverDown)
					d2d->FillRect(downRect.left, downRect.top, downRect.right - downRect.left, downRect.bottom - downRect.top, this->HoverColor);

				float dividerY = upRect.bottom;
				d2d->DrawLine(D2D1::Point2F(spinLeft + 1.0f, dividerY), D2D1::Point2F(fieldRect.right - 1.5f, dividerY), this->DropBorderColor, 1.0f);
				d2d->DrawRoundRect(fieldRect.left, fieldRect.top, fieldRect.right - fieldRect.left, fieldRect.bottom - fieldRect.top, borderColor, borderWidth, 5.0f);

				float textRight = spinLeft - 3.0f;
				float textAreaWidth = (std::max)(0.0f, textRight - fieldRect.left);
				auto textSize = font->GetTextSize(text);
				float textX = fieldRect.left + (std::max)(0.0f, (textAreaWidth - textSize.width) * 0.5f);
				float textY = fieldRect.top + (std::max)(0.0f, ((fieldRect.bottom - fieldRect.top) - textSize.height) * 0.5f);

				if (showEditHighlight && !text.empty())
				{
					int activeIndex = _editBuffer.empty() ? 0 : (int)std::min<size_t>(_editBuffer.size(), text.size() - 1);
					float cursorX = textX;
					for (size_t i = 0; i < text.size(); ++i)
					{
						std::wstring ch(1, text[i]);
						auto charSize = font->GetTextSize(ch);
						if ((int)i == activeIndex)
						{
							float pillPadX = 3.0f;
							float pillPadY = 2.0f;
							d2d->FillRoundRect(cursorX - pillPadX, textY - pillPadY, charSize.width + pillPadX * 2.0f,
								charSize.height + pillPadY * 2.0f, this->AccentColor, 4.0f);
							d2d->DrawString(ch, cursorX, textY, Colors::White, font);
						}
						else
						{
							d2d->DrawString(ch, cursorX, textY, this->ForeColor, font);
						}
						cursorX += charSize.width;
					}
				}
				else
				{
					d2d->DrawString(text, textX, textY, this->ForeColor, font);
				}

				auto drawArrowButton = [&](const D2D1_RECT_F& buttonRect, bool up, bool hovered)
					{
						float buttonWidth = buttonRect.right - buttonRect.left;
						float buttonHeight = buttonRect.bottom - buttonRect.top;
						float cx = buttonRect.left + buttonWidth * 0.5f;
						float cy = buttonRect.top + buttonHeight * 0.5f + (up ? -0.5f : 0.5f);
						float half = std::clamp((std::min)(buttonWidth, buttonHeight) * 0.18f, 2.5f, 4.0f);
						D2D1_TRIANGLE tri{};
						if (up)
						{
							tri.point1 = D2D1::Point2F(cx - half, cy + half * 0.75f);
							tri.point2 = D2D1::Point2F(cx + half, cy + half * 0.75f);
							tri.point3 = D2D1::Point2F(cx, cy - half);
						}
						else
						{
							tri.point1 = D2D1::Point2F(cx - half, cy - half * 0.75f);
							tri.point2 = D2D1::Point2F(cx + half, cy - half * 0.75f);
							tri.point3 = D2D1::Point2F(cx, cy + half);
						}
						d2d->FillTriangle(tri, hovered ? this->ForeColor : this->SecondaryTextColor);
					};

				drawArrowButton(upRect, true, hoverUp);
				drawArrowButton(downRect, false, hoverDown);
			};
		if (inlineTime)
		{
			LayoutMetrics layout{};
			if (GetInlineTimeLayout(layout))
			{
				std::wstring hourText = GetTimeEditText(EditField::Hour, _value.wHour);
				std::wstring minuteText = GetTimeEditText(EditField::Minute, _value.wMinute);

				drawTimeField(layout.hourRect, layout.hourUpRect, layout.hourDownRect,
					EditField::Hour, hourText, _hoverPart == HitPart::HourUp, _hoverPart == HitPart::HourDown);
				drawTimeField(layout.minuteRect, layout.minuteUpRect, layout.minuteDownRect,
					EditField::Minute, minuteText, _hoverPart == HitPart::MinuteUp, _hoverPart == HitPart::MinuteDown);

				std::wstring colon = L":";
				auto colonSize = font->GetTextSize(colon);
				float colonX = layout.contentLeft + layout.contentWidth * 0.5f - colonSize.width * 0.5f;
				float colonY = layout.timeTop + (layout.timeHeight - colonSize.height) * 0.5f;
				d2d->DrawString(colon, colonX, colonY, this->SecondaryTextColor, font);
			}
			else
			{
				auto textSize = font->GetTextSize(this->Text);
				float textX = 10.0f;
				float textY = (std::max)(0.0f, ((float)this->Height - textSize.height) * 0.5f);
				d2d->DrawString(this->Text, textX, textY, this->ForeColor, font);
			}
		}
		else
		{
			auto textSize = font->GetTextSize(this->Text);
			float textX = 10.0f;
			float textY = (std::max)(0.0f, ((float)this->Height - textSize.height) * 0.5f);
			d2d->DrawString(this->Text, textX, textY, this->ForeColor, font);

			// 下拉箭头
			{
				const float h = (float)this->Height;
				float iconSize = h * 0.35f;
				if (iconSize < 8.0f) iconSize = 8.0f;
				if (iconSize > 12.0f) iconSize = 12.0f;
				const float padRight = 10.0f;
				const float cx = (float)this->Width - padRight - iconSize * 0.5f;
				const float cy = h * 0.5f;
				const float half = iconSize * 0.5f;
				const float triH = iconSize * 0.6f;
				D2D1_TRIANGLE tri{};
				if (this->Expand)
				{
					tri.point1 = D2D1::Point2F(cx - half, cy + triH * 0.5f);
					tri.point2 = D2D1::Point2F(cx + half, cy + triH * 0.5f);
					tri.point3 = D2D1::Point2F(cx, cy - triH * 0.5f);
				}
				else
				{
					tri.point1 = D2D1::Point2F(cx - half, cy - triH * 0.5f);
					tri.point2 = D2D1::Point2F(cx + half, cy - triH * 0.5f);
					tri.point3 = D2D1::Point2F(cx, cy + triH * 0.5f);
				}
				d2d->FillTriangle(tri, this->ForeColor);
			}
		}

		D2D1_COLOR_F borderColor = isSelected ? FocusBorderColor : this->BolderColor;
		d2d->DrawRoundRect(this->Boder * 0.5f, this->Boder * 0.5f,
			(float)this->Width - this->Boder, (float)this->Height - this->Boder,
			borderColor, this->Boder, round - this->Boder);

		if (this->Expand && !inlineTime)
		{
			LayoutMetrics layout{};
			if (GetLayoutMetrics(layout))
			{
				float dropX = 0.0f;
				float dropY = (float)this->Height;
				float dropW = (float)this->Width;
				float dropH = layout.dropHeight;
				d2d->FillRoundRect(dropX, dropY, dropW, dropH, this->DropBackColor, round);
				d2d->DrawRoundRect(dropX + (this->Boder * 0.5f), dropY + (this->Boder * 0.5f),
					dropW - this->Boder, dropH - this->Boder,
					this->DropBorderColor, this->Boder, round);

				auto toAbs = [&](D2D1_RECT_F r) -> D2D1_RECT_F {
					return r;
					};

				auto drawToggle = [&](D2D1_RECT_F rect, bool enabled, const wchar_t* label, bool hovered)
					{
						D2D1_COLOR_F fill = enabled ? this->AccentColor : blendColor(this->DropBackColor, this->DropBorderColor, 0.42f);
						if (hovered)
							fill = enabled ? D2D1_COLOR_F{ this->AccentColor.r, this->AccentColor.g, this->AccentColor.b, 0.85f }
							: blendColor(this->HoverColor, this->DropBorderColor, 0.30f);
						auto absRect = toAbs(rect);
						d2d->FillRoundRect(absRect.left, absRect.top, absRect.right - absRect.left, absRect.bottom - absRect.top, fill, 6.0f);
						D2D1_COLOR_F textColor = enabled ? Colors::White : this->ForeColor;
						auto ts = font->GetTextSize(label);
						float tx = absRect.left + ((absRect.right - absRect.left) - ts.width) * 0.5f;
						float ty = absRect.top + ((absRect.bottom - absRect.top) - ts.height) * 0.5f;
						d2d->DrawString(label, tx, ty, textColor, font);
					};

				if (layout.showToggleRow)
				{
					drawToggle(layout.toggleDateRect, _showDate, L"日期", _hoverPart == HitPart::ToggleDate);
					drawToggle(layout.toggleTimeRect, _showTime, L"时间", _hoverPart == HitPart::ToggleTime);
				}

				if (layout.showDate)
				{
					// 月份标题与切换
					auto prevAbs = toAbs(layout.prevRect);
					auto nextAbs = toAbs(layout.nextRect);
					if (_hoverPart == HitPart::PrevMonth)
						d2d->FillRoundRect(prevAbs.left, prevAbs.top, prevAbs.right - prevAbs.left, prevAbs.bottom - prevAbs.top, this->HoverColor, 4.0f);
					if (_hoverPart == HitPart::NextMonth)
						d2d->FillRoundRect(nextAbs.left, nextAbs.top, nextAbs.right - nextAbs.left, nextAbs.bottom - nextAbs.top, this->HoverColor, 4.0f);

					const float cy = prevAbs.top + (prevAbs.bottom - prevAbs.top) * 0.5f;
					const float triHalf = 5.5f;
					D2D1_TRIANGLE triL{
						D2D1::Point2F(prevAbs.left + 8.0f, cy),
						D2D1::Point2F(prevAbs.left + 8.0f + triHalf, cy - triHalf),
						D2D1::Point2F(prevAbs.left + 8.0f + triHalf, cy + triHalf) };
					D2D1_TRIANGLE triR{
						D2D1::Point2F(nextAbs.right - 8.0f, cy),
						D2D1::Point2F(nextAbs.right - 8.0f - triHalf, cy - triHalf),
						D2D1::Point2F(nextAbs.right - 8.0f - triHalf, cy + triHalf) };
					d2d->FillTriangle(triL, this->ForeColor);
					d2d->FillTriangle(triR, this->ForeColor);

					std::wstring monthText = std::format(L"{:04d}年{:02d}月", _viewYear, _viewMonth);
					auto monthSize = font->GetTextSize(monthText);
					float monthCenterX = layout.contentLeft + layout.contentWidth * 0.5f;
					float monthX = monthCenterX - monthSize.width * 0.5f;
					float monthY = layout.monthHeaderTop + (layout.monthHeaderHeight - monthSize.height) * 0.5f;
					d2d->DrawString(monthText, monthX, monthY, this->ForeColor, font);

					// 星期标题
					static const wchar_t* weekNames[7] = { L"日", L"一", L"二", L"三", L"四", L"五", L"六" };
					for (int i = 0; i < 7; i++)
					{
						std::wstring w = weekNames[i];
						auto ts = font->GetTextSize(w);
						float cx = layout.contentLeft + layout.cellWidth * (i + 0.5f);
						float ty = layout.weekTop + (layout.weekHeight - ts.height) * 0.5f;
						float tx = cx - ts.width * 0.5f;
						d2d->DrawString(w, tx, ty, this->SecondaryTextColor, font);
					}

					int days = DaysInMonth(_viewYear, _viewMonth);
					int first = FirstWeekday(_viewYear, _viewMonth);
					SYSTEMTIME today{};
					::GetLocalTime(&today);
				for (int i = 0; i < layout.gridRows * 7; i++)
					{
						int day = i - first + 1;
						if (day < 1 || day > days) continue;
						int row = i / 7;
						int col = i % 7;
						float left = layout.contentLeft + layout.cellWidth * col;
						float top = layout.gridTop + layout.cellHeight * row;
						D2D1_RECT_F rect{ left, top, left + layout.cellWidth, top + layout.cellHeight };

						bool isSelectedDate = (_value.wYear == _viewYear && _value.wMonth == _viewMonth && _value.wDay == day);
						bool isHover = (_hoverPart == HitPart::DayCell && _hoverDay == day);
						bool isToday = (today.wYear == _viewYear && today.wMonth == _viewMonth && today.wDay == day);

						if (isSelectedDate)
							d2d->FillRoundRect(rect.left + 2.0f, rect.top + 2.0f, (rect.right - rect.left) - 4.0f, (rect.bottom - rect.top) - 4.0f, this->AccentColor, 6.0f);
						else if (isHover)
							d2d->FillRoundRect(rect.left + 2.0f, rect.top + 2.0f, (rect.right - rect.left) - 4.0f, (rect.bottom - rect.top) - 4.0f, this->HoverColor, 6.0f);
						else if (isToday)
							d2d->DrawRoundRect(rect.left + 2.0f, rect.top + 2.0f, (rect.right - rect.left) - 4.0f, (rect.bottom - rect.top) - 4.0f, this->AccentColor, 1.0f, 6.0f);

						auto ds = font->GetTextSize(std::to_wstring(day));
						float tx = rect.left + (rect.right - rect.left - ds.width) * 0.5f;
						float ty = rect.top + (rect.bottom - rect.top - ds.height) * 0.5f;
						D2D1_COLOR_F dayColor = isSelectedDate ? Colors::White : this->ForeColor;
						d2d->DrawString(std::to_wstring(day), tx, ty, dayColor, font);
					}
				}

				if (layout.showTime)
				{
					std::wstring hourText = GetTimeEditText(EditField::Hour, _value.wHour);
					std::wstring minuteText = GetTimeEditText(EditField::Minute, _value.wMinute);

					drawTimeField(layout.hourRect, layout.hourUpRect, layout.hourDownRect,
						EditField::Hour, hourText, _hoverPart == HitPart::HourUp, _hoverPart == HitPart::HourDown);
					drawTimeField(layout.minuteRect, layout.minuteUpRect, layout.minuteDownRect,
						EditField::Minute, minuteText, _hoverPart == HitPart::MinuteUp, _hoverPart == HitPart::MinuteDown);

					// 冒号
					std::wstring colon = L":";
					auto colonSize = font->GetTextSize(colon);
					float colonX = layout.contentLeft + layout.contentWidth * 0.5f - colonSize.width * 0.5f;
					float colonY = layout.timeTop + (layout.timeHeight - colonSize.height) * 0.5f;
					d2d->DrawString(colon, colonX, colonY, this->SecondaryTextColor, font);
				}
			}
		}

		if (!this->Enable)
		{
			d2d->FillRect(0.0f, 0.0f, (float)size.cx, (float)size.cy, blendColor(this->DropBackColor, Colors::White, 0.35f));
		}
	}
	this->EndRender();
}

bool DateTimePicker::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof)
{
	if (!this->Enable || !this->Visible) return true;

	if (WM_LBUTTONDOWN == message)
	{
		if (this->ParentForm && this->ParentForm->Selected && this->ParentForm->Selected != this)
		{
			auto se = this->ParentForm->Selected;
			this->ParentForm->Selected = this;
			se->PostRender();
		}
		else if (this->ParentForm)
		{
			this->ParentForm->Selected = this;
		}
	}

	switch (message)
	{
	case WM_MOUSEWHEEL:
	{
		if (this->Expand || IsInlineTimeMode())
		{
			LayoutMetrics layout{};
			bool hasLayout = this->Expand ? GetLayoutMetrics(layout) : GetInlineTimeLayout(layout);
			if (hasLayout)
			{
				int delta = GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? -1 : 1;
				if (layout.showTime && yof >= (int)layout.timeTop)
				{
					AdjustMinute(-delta * 5);
				}
				else if (layout.showDate)
				{
					AdjustMonth(delta);
				}
			}
		}
		MouseEventArgs event_obj = MouseEventArgs(MouseButtons::None, 0, xof, yof, GET_WHEEL_DELTA_WPARAM(wParam));
		this->OnMouseWheel(this, event_obj);
	}
	break;
	case WM_MOUSEMOVE:
	{
		this->ParentForm->UnderMouse = this;
		if (this->Expand || IsInlineTimeMode())
			UpdateHoverState(xof, yof);
		MouseEventArgs event_obj = MouseEventArgs(MouseButtons::None, 0, xof, yof, HIWORD(wParam));
		this->OnMouseMove(this, event_obj);
	}
	break;
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_MBUTTONDOWN:
	{
		MouseEventArgs event_obj = MouseEventArgs(FromParamToMouseButtons(message), 0, xof, yof, HIWORD(wParam));
		this->OnMouseDown(this, event_obj);
	}
	break;
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
	case WM_MBUTTONUP:
	{
		if (WM_LBUTTONUP == message && this->ParentForm && this->ParentForm->Selected == this)
		{
			if (IsInlineTimeMode())
			{
				if (yof >= 0 && yof <= this->Height)
				{
					LayoutMetrics layout{};
					int day = -1;
					if (GetInlineTimeLayout(layout))
					{
						auto part = HitTestPart(layout, xof, yof, day);
						if (_editField != EditField::None)
						{
							bool sameField = (part == HitPart::HourField && _editField == EditField::Hour) ||
								(part == HitPart::MinuteField && _editField == EditField::Minute);
							if (!sameField)
								CommitTimeEdit(false);
						}
						switch (part)
						{
						case HitPart::HourField:
							BeginTimeEdit(EditField::Hour);
							break;
						case HitPart::MinuteField:
							BeginTimeEdit(EditField::Minute);
							break;
						case HitPart::HourUp:
							AdjustHour(1);
							break;
						case HitPart::HourDown:
							AdjustHour(-1);
							break;
						case HitPart::MinuteUp:
							AdjustMinute(1);
							break;
						case HitPart::MinuteDown:
							AdjustMinute(-1);
							break;
						default:
							break;
						}
					}
				}
				else
				{
					if (_editField != EditField::None)
						CommitTimeEdit(false);
				}
			}
			else if (!this->Expand)
			{
				if (yof >= 0 && yof <= this->Height)
					SetExpanded(true);
			}
			else
			{
				if (yof >= 0 && yof <= this->Height)
				{
					SetExpanded(false);
				}
				else
				{
					LayoutMetrics layout{};
					int day = -1;
					if (GetLayoutMetrics(layout))
					{
						auto part = HitTestPart(layout, xof, yof, day);
						if (_editField != EditField::None)
						{
							bool sameField = (part == HitPart::HourField && _editField == EditField::Hour) ||
								(part == HitPart::MinuteField && _editField == EditField::Minute);
							if (!sameField)
								CommitTimeEdit(false);
						}
						switch (part)
						{
						case HitPart::ToggleDate:
							if (AllowModeSwitch) ToggleDateSection();
							break;
						case HitPart::ToggleTime:
							if (AllowModeSwitch) ToggleTimeSection();
							break;
						case HitPart::PrevMonth:
							AdjustMonth(-1);
							break;
						case HitPart::NextMonth:
							AdjustMonth(1);
							break;
						case HitPart::DayCell:
							if (day > 0)
							{
								SYSTEMTIME nv = _value;
								nv.wYear = (WORD)_viewYear;
								nv.wMonth = (WORD)_viewMonth;
								nv.wDay = (WORD)day;
								SetValueInternal(nv, true);
								if (!_showTime)
									SetExpanded(false);
							}
							break;
						case HitPart::HourField:
							BeginTimeEdit(EditField::Hour);
							break;
						case HitPart::MinuteField:
							BeginTimeEdit(EditField::Minute);
							break;
						case HitPart::HourUp:
							AdjustHour(1);
							break;
						case HitPart::HourDown:
							AdjustHour(-1);
							break;
						case HitPart::MinuteUp:
							AdjustMinute(1);
							break;
						case HitPart::MinuteDown:
							AdjustMinute(-1);
							break;
						default:
							break;
						}
					}
				}
			}
		}
		if (this->ParentForm)
		{
			if (_editField == EditField::None)
				this->ParentForm->Selected = NULL;
			else
				this->ParentForm->Selected = this;
		}
		MouseEventArgs event_obj = MouseEventArgs(FromParamToMouseButtons(message), 0, xof, yof, HIWORD(wParam));
		this->OnMouseUp(this, event_obj);
		this->PostRender();
	}
	break;
	case WM_LBUTTONDBLCLK:
	{
		MouseEventArgs event_obj = MouseEventArgs(FromParamToMouseButtons(message), 0, xof, yof, HIWORD(wParam));
		this->OnMouseDoubleClick(this, event_obj);
	}
	break;
	case WM_KEYDOWN:
	{
		if (_editField != EditField::None && wParam == VK_ESCAPE)
		{
			CancelTimeEdit();
		}
		KeyEventArgs event_obj = KeyEventArgs((Keys)(wParam | 0));
		this->OnKeyDown(this, event_obj);
	}
	break;
	case WM_CHAR:
	{
		if ((this->Expand || IsInlineTimeMode()) && _showTime && _allowTime)
		{
			if (HandleTimeEditChar((wchar_t)wParam))
				return true;
		}
	}
	break;
	case WM_KEYUP:
	{
		KeyEventArgs event_obj = KeyEventArgs((Keys)(wParam | 0));
		this->OnKeyUp(this, event_obj);
	}
	break;
	}
	return true;
}

void DateTimePicker::EnsureShowFlags()
{
	if (!_allowDate) _showDate = false;
	if (!_allowTime) _showTime = false;
	if (!_showDate && !_showTime)
	{
		if (_allowDate)
			_showDate = true;
		else if (_allowTime)
			_showTime = true;
		else
		{
			_allowDate = true;
			_showDate = true;
		}
	}
	if (!_showTime || !_allowTime)
		CommitTimeEdit(false);
	if (IsInlineTimeMode() && Expand)
		SetExpanded(false);
}

void DateTimePicker::SyncViewFromValue()
{
	_viewYear = (int)_value.wYear;
	_viewMonth = (int)_value.wMonth;
}

void DateTimePicker::UpdateDisplayText()
{
	if (_showDate && _showTime)
	{
		this->Text = std::format(L"{:04d}-{:02d}-{:02d} {:02d}:{:02d}",
			_value.wYear, _value.wMonth, _value.wDay, _value.wHour, _value.wMinute);
	}
	else if (_showDate)
	{
		this->Text = std::format(L"{:04d}-{:02d}-{:02d}",
			_value.wYear, _value.wMonth, _value.wDay);
	}
	else
	{
		this->Text = std::format(L"{:02d}:{:02d}",
			_value.wHour, _value.wMinute);
	}
}

int DateTimePicker::DaysInMonth(int year, int month)
{
	static const int days[] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
	int m = std::clamp(month, 1, 12);
	int d = days[m - 1];
	const bool isLeapYear = ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
	if (m == 2 && isLeapYear)
		d = 29;
	return d;
}

int DateTimePicker::FirstWeekday(int year, int month)
{
	static const int offsets[] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
	int y = year;
	int m = std::clamp(month, 1, 12);
	if (m < 3) y -= 1;
	return (y + y / 4 - y / 100 + y / 400 + offsets[m - 1] + 1) % 7;
}

void DateTimePicker::AdjustMonth(int delta)
{
	int m = _viewMonth + delta;
	int y = _viewYear;
	if (m < 1) { m = 12; y -= 1; }
	if (m > 12) { m = 1; y += 1; }
	_viewMonth = m;
	_viewYear = y;
	PostRender();
}

void DateTimePicker::AdjustHour(int delta)
{
	int h = (int)_value.wHour + delta;
	if (h < 0) h += 24;
	if (h >= 24) h -= 24;
	SYSTEMTIME nv = _value;
	nv.wHour = (WORD)h;
	SetValueInternal(nv, true);
}

void DateTimePicker::AdjustMinute(int delta)
{
	int total = (int)_value.wHour * 60 + (int)_value.wMinute + delta;
	int day = 24 * 60;
	total = ((total % day) + day) % day;
	SYSTEMTIME nv = _value;
	nv.wHour = (WORD)(total / 60);
	nv.wMinute = (WORD)(total % 60);
	SetValueInternal(nv, true);
}

void DateTimePicker::BeginTimeEdit(EditField field)
{
	if (!_allowTime || !_showTime) return;
	if (_editField != field)
		_editBuffer.clear();
	_editField = field;
	if (this->ParentForm)
		this->ParentForm->Selected = this;
	PostRender();
}

void DateTimePicker::CommitTimeEdit(bool keepEditing)
{
	if (_editField == EditField::None) return;
	if (!_editBuffer.empty())
	{
		int value = 0;
		for (wchar_t ch : _editBuffer)
			value = value * 10 + (int)(ch - L'0');
		if (_editField == EditField::Hour)
			value = std::clamp(value, 0, 23);
		else
			value = std::clamp(value, 0, 59);
		SYSTEMTIME nv = _value;
		if (_editField == EditField::Hour)
			nv.wHour = (WORD)value;
		else
			nv.wMinute = (WORD)value;
		SetValueInternal(nv, true);
	}
	_editBuffer.clear();
	if (!keepEditing)
		_editField = EditField::None;
	PostRender();
}

void DateTimePicker::CancelTimeEdit()
{
	if (_editField == EditField::None) return;
	_editBuffer.clear();
	_editField = EditField::None;
	PostRender();
}

bool DateTimePicker::HandleTimeEditChar(wchar_t ch)
{
	if (_editField == EditField::None) return false;
	if (ch >= L'0' && ch <= L'9')
	{
		if (_editBuffer.size() >= 2)
			_editBuffer.clear();
		_editBuffer.push_back(ch);
		if (_editBuffer.size() >= 2)
		{
			EditField completedField = _editField;
			if (completedField == EditField::Hour)
			{
				CommitTimeEdit(false);
				BeginTimeEdit(EditField::Minute);
			}
			else
			{
				CommitTimeEdit(true);
			}
		}
		else
			PostRender();
		return true;
	}
	if (ch == 8)
	{
		if (!_editBuffer.empty())
		{
			_editBuffer.pop_back();
			PostRender();
		}
		return true;
	}
	if (ch == L'\r')
	{
		CommitTimeEdit(false);
		return true;
	}
	return false;
}

std::wstring DateTimePicker::GetTimeEditText(EditField field, int value) const
{
	std::wstring text = std::format(L"{:02d}", value);
	if (_editField != field || _editBuffer.empty())
		return text;
	if (_editBuffer.size() == 1)
		return std::wstring(L"0") + _editBuffer;
	return _editBuffer.substr(0, 2);
}

bool DateTimePicker::IsInlineTimeMode() const
{
	return _showTime && !_showDate;
}

bool DateTimePicker::GetInlineTimeLayout(LayoutMetrics& out)
{
	if (this->Width <= 0 || this->Height <= 0) return false;

	const float pad = 8.0f;
	out = LayoutMetrics{};
	out.contentLeft = pad;
	out.contentWidth = (std::max)(0.0f, (float)this->Width - pad * 2.0f);
	out.showDate = false;
	out.showTime = true;
	out.showToggleRow = false;

	float timeHeight = (std::min)(32.0f, (float)this->Height - 6.0f);
	if (timeHeight < 18.0f)
		timeHeight = (std::max)(0.0f, (float)this->Height - 4.0f);
	float y = (std::max)(0.0f, ((float)this->Height - timeHeight) * 0.5f);

	out.timeTop = y;
	out.timeHeight = timeHeight;
	float fieldW = (std::min)(64.0f, (std::max)(44.0f, out.contentWidth * 0.28f));
	float gap = 10.0f;
	float centerX = out.contentLeft + out.contentWidth * 0.5f;
	out.hourRect = { centerX - fieldW - gap, y, centerX - gap, y + out.timeHeight };
	out.minuteRect = { centerX + gap, y, centerX + gap + fieldW, y + out.timeHeight };
	float arrowW = std::clamp(out.timeHeight * 0.56f, 18.0f, 22.0f);
	float splitGap = out.timeHeight >= 24.0f ? 2.0f : 1.0f;
	float halfHeight = out.timeHeight * 0.5f;
	out.hourUpRect = { out.hourRect.right - arrowW, y, out.hourRect.right, y + halfHeight - splitGap * 0.5f };
	out.hourDownRect = { out.hourRect.right - arrowW, y + halfHeight + splitGap * 0.5f, out.hourRect.right, y + out.timeHeight };
	out.minuteUpRect = { out.minuteRect.right - arrowW, y, out.minuteRect.right, y + halfHeight - splitGap * 0.5f };
	out.minuteDownRect = { out.minuteRect.right - arrowW, y + halfHeight + splitGap * 0.5f, out.minuteRect.right, y + out.timeHeight };
	out.dropHeight = 0.0f;
	return true;
}

void DateTimePicker::SetValueInternal(const SYSTEMTIME& value, bool fireEvent)
{
	bool changed = _value.wYear != value.wYear || _value.wMonth != value.wMonth ||
		_value.wDay != value.wDay || _value.wHour != value.wHour || _value.wMinute != value.wMinute ||
		_value.wSecond != value.wSecond || _value.wMilliseconds != value.wMilliseconds;
	_value = value;
	SyncViewFromValue();
	UpdateDisplayText();
	PostRender();
	if (changed && fireEvent)
		this->OnSelectionChanged(this);
}

bool DateTimePicker::GetLayoutMetrics(LayoutMetrics& out)
{
	if (this->Width <= 0 || this->Height <= 0) return false;

	const float pad = 8.0f;
	const float baseTop = (float)this->Height;
	out.contentLeft = pad;
	out.contentWidth = (std::max)(0.0f, (float)this->Width - pad * 2.0f);
	out.contentTop = baseTop + pad;
	float y = out.contentTop;

	out.showToggleRow = AllowModeSwitch && _allowDate && _allowTime;
	if (out.showToggleRow)
	{
		const float toggleHeight = 24.0f;
		float chipW = (std::min)(72.0f, (std::max)(48.0f, (out.contentWidth - 8.0f) * 0.5f));
		float totalW = chipW * 2.0f + 8.0f;
		float startX = out.contentLeft + (std::max)(0.0f, (out.contentWidth - totalW) * 0.5f);
		out.toggleDateRect = { startX, y, startX + chipW, y + toggleHeight };
		out.toggleTimeRect = { startX + chipW + 8.0f, y, startX + chipW * 2.0f + 8.0f, y + toggleHeight };
		y += toggleHeight + 6.0f;
	}

	out.showDate = _showDate;
	if (out.showDate)
	{
		out.monthHeaderTop = y;
		out.monthHeaderHeight = 26.0f;
		out.prevRect = { out.contentLeft, y, out.contentLeft + 24.0f, y + out.monthHeaderHeight };
		out.nextRect = { out.contentLeft + out.contentWidth - 24.0f, y, out.contentLeft + out.contentWidth, y + out.monthHeaderHeight };
		y += out.monthHeaderHeight + 6.0f;

		out.weekTop = y;
		out.weekHeight = 20.0f;
		y += out.weekHeight;

		out.gridTop = y;
		out.cellWidth = out.contentWidth / 7.0f;
		out.cellHeight = std::clamp(out.cellWidth, 22.0f, 32.0f);
		{
			int first = FirstWeekday(_viewYear, _viewMonth);
			int days = DaysInMonth(_viewYear, _viewMonth);
			int cells = first + days;
			int rows = (cells + 6) / 7;
			out.gridRows = std::clamp(rows, 4, 6);
		}
		y += out.cellHeight * out.gridRows;
		if (_showTime)
			y += 6.0f;
	}

	out.showTime = _showTime;
	if (out.showTime)
	{
		out.timeTop = y;
		out.timeHeight = 32.0f;
		float fieldW = (std::min)(64.0f, (std::max)(44.0f, out.contentWidth * 0.28f));
		float gap = 10.0f;
		float centerX = out.contentLeft + out.contentWidth * 0.5f;
		out.hourRect = { centerX - fieldW - gap, y, centerX - gap, y + out.timeHeight };
		out.minuteRect = { centerX + gap, y, centerX + gap + fieldW, y + out.timeHeight };
		float arrowW = std::clamp(out.timeHeight * 0.56f, 18.0f, 22.0f);
		float splitGap = out.timeHeight >= 24.0f ? 2.0f : 1.0f;
		float halfHeight = out.timeHeight * 0.5f;
		out.hourUpRect = { out.hourRect.right - arrowW, y, out.hourRect.right, y + halfHeight - splitGap * 0.5f };
		out.hourDownRect = { out.hourRect.right - arrowW, y + halfHeight + splitGap * 0.5f, out.hourRect.right, y + out.timeHeight };
		out.minuteUpRect = { out.minuteRect.right - arrowW, y, out.minuteRect.right, y + halfHeight - splitGap * 0.5f };
		out.minuteDownRect = { out.minuteRect.right - arrowW, y + halfHeight + splitGap * 0.5f, out.minuteRect.right, y + out.timeHeight };
		y += out.timeHeight;
	}

	out.dropHeight = (y + pad) - baseTop;
	if (out.dropHeight < 0.0f) out.dropHeight = 0.0f;
	return true;
}

bool DateTimePicker::HitTestDayCell(const LayoutMetrics& layout, int xof, int yof, int& outDay) const
{
	if (!layout.showDate) return false;
	float gridLeft = layout.contentLeft;
	float gridTop = layout.gridTop;
	float gridRight = gridLeft + layout.cellWidth * 7.0f;
	float gridBottom = gridTop + layout.cellHeight * layout.gridRows;
	if (xof < gridLeft || xof > gridRight || yof < gridTop || yof > gridBottom)
		return false;

	int col = (int)((xof - gridLeft) / layout.cellWidth);
	int row = (int)((yof - gridTop) / layout.cellHeight);
	col = std::clamp(col, 0, 6);
	row = std::clamp(row, 0, 5);
	int idx = row * 7 + col;
	int first = FirstWeekday(_viewYear, _viewMonth);
	int day = idx - first + 1;
	int days = DaysInMonth(_viewYear, _viewMonth);
	if (day < 1 || day > days) return false;
	outDay = day;
	return true;
}

void DateTimePicker::UpdateHoverState(int xof, int yof)
{
	LayoutMetrics layout{};
	bool hasLayout = IsInlineTimeMode() ? GetInlineTimeLayout(layout) : GetLayoutMetrics(layout);
	if (!hasLayout) return;
	int day = -1;
	auto part = HitTestPart(layout, xof, yof, day);
	if (part != _hoverPart || day != _hoverDay)
	{
		_hoverPart = part;
		_hoverDay = day;
		PostRender();
	}
}

DateTimePicker::HitPart DateTimePicker::HitTestPart(const LayoutMetrics& layout, int xof, int yof, int& outDay) const
{
	outDay = -1;
	auto inRect = [&](const D2D1_RECT_F& r) -> bool
		{
			return xof >= r.left && xof <= r.right && yof >= r.top && yof <= r.bottom;
		};

	if (layout.showToggleRow)
	{
		if (inRect(layout.toggleDateRect)) return HitPart::ToggleDate;
		if (inRect(layout.toggleTimeRect)) return HitPart::ToggleTime;
	}
	if (layout.showDate)
	{
		if (inRect(layout.prevRect)) return HitPart::PrevMonth;
		if (inRect(layout.nextRect)) return HitPart::NextMonth;
		if (HitTestDayCell(layout, xof, yof, outDay)) return HitPart::DayCell;
	}
	if (layout.showTime)
	{
		if (inRect(layout.hourUpRect)) return HitPart::HourUp;
		if (inRect(layout.hourDownRect)) return HitPart::HourDown;
		if (inRect(layout.minuteUpRect)) return HitPart::MinuteUp;
		if (inRect(layout.minuteDownRect)) return HitPart::MinuteDown;
		if (inRect(layout.hourRect)) return HitPart::HourField;
		if (inRect(layout.minuteRect)) return HitPart::MinuteField;
	}
	return HitPart::None;
}

void DateTimePicker::ToggleDateSection()
{
	if (!_allowDate) return;
	bool next = !_showDate;
	if (!next && !_showTime) return;
	_showDate = next;
	EnsureShowFlags();
	UpdateDisplayText();
	if (this->Expand && this->ParentForm) this->ParentForm->Invalidate(true);
	PostRender();
}

void DateTimePicker::ToggleTimeSection()
{
	if (!_allowTime) return;
	bool next = !_showTime;
	if (!next && !_showDate) return;
	if (!next)
		CommitTimeEdit(false);
	_showTime = next;
	EnsureShowFlags();
	UpdateDisplayText();
	if (this->Expand && this->ParentForm) this->ParentForm->Invalidate(true);
	PostRender();
}

void DateTimePicker::SetExpanded(bool value)
{
	if (value && IsInlineTimeMode())
		value = false;
	if (Expand == value) return;
	Expand = value;
	if (this->ParentForm)
	{
		if (Expand)
			this->ParentForm->ForegroundControl = this;
		else if (this->ParentForm->ForegroundControl == this)
			this->ParentForm->ForegroundControl = NULL;
		this->ParentForm->Invalidate(true);
	}
	if (!Expand)
	{
		CommitTimeEdit(false);
		_hoverPart = HitPart::None;
		_hoverDay = -1;
	}
	PostRender();
}

