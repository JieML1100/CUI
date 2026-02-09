#include "DateTimePicker.h"
#include "Form.h"

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
	if (!Expand)
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
	if (yof >= 0 && yof <= this->Height) return CursorKind::Hand;
	if (!Expand) return CursorKind::Arrow;

	LayoutMetrics layout{};
	if (!GetLayoutMetrics(layout)) return CursorKind::Arrow;
	int day = -1;
	auto part = HitTestPart(layout, xof, yof, day);
	if (part != HitPart::None)
		return CursorKind::Hand;
	return CursorKind::Arrow;
}

void DateTimePicker::Update()
{
	if (!this->IsVisual) return;
	auto d2d = this->ParentForm->Render;
	auto abs = this->AbsLocation;
	auto size = this->ActualSize();
	auto absRect = this->AbsRect;
	if (this->Expand)
		absRect.bottom = absRect.top + size.cy;

	bool isUnderMouse = this->ParentForm->UnderMouse == this;
	bool isSelected = this->ParentForm->Selected == this;

	d2d->PushDrawRect(absRect.left, absRect.top, absRect.right - absRect.left, absRect.bottom - absRect.top);
	{
		const float round = std::min(this->Round, (float)this->Height * 0.5f);
		D2D1_COLOR_F baseColor = this->BackColor;
		if (isUnderMouse && !this->Expand)
			baseColor = D2D1_COLOR_F{ 0.96f, 0.96f, 0.96f, 1.0f };

		d2d->FillRoundRect((float)abs.x, (float)abs.y, (float)this->Width, (float)this->Height, baseColor, round);

		auto font = this->Font;
		auto textSize = font->GetTextSize(this->Text);
		float textX = (float)abs.x + 10.0f;
		float textY = (float)abs.y + std::max(0.0f, ((float)this->Height - textSize.height) * 0.5f);
		d2d->DrawString(this->Text, textX, textY, this->ForeColor, font);

		// 下拉箭头
		{
			const float h = (float)this->Height;
			float iconSize = h * 0.35f;
			if (iconSize < 8.0f) iconSize = 8.0f;
			if (iconSize > 12.0f) iconSize = 12.0f;
			const float padRight = 10.0f;
			const float cx = (float)abs.x + (float)this->Width - padRight - iconSize * 0.5f;
			const float cy = (float)abs.y + h * 0.5f;
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

		D2D1_COLOR_F borderColor = isSelected ? FocusBorderColor : this->BolderColor;
		d2d->DrawRoundRect((float)abs.x + (this->Boder * 0.5f), (float)abs.y + (this->Boder * 0.5f),
			(float)this->Width - this->Boder, (float)this->Height - this->Boder,
			borderColor, this->Boder, round);

		if (this->Expand)
		{
			LayoutMetrics layout{};
			if (GetLayoutMetrics(layout))
			{
				float dropX = (float)abs.x;
				float dropY = (float)abs.y + (float)this->Height;
				float dropW = (float)this->Width;
				float dropH = layout.dropHeight;
				d2d->FillRoundRect(dropX, dropY, dropW, dropH, this->DropBackColor, round);
				d2d->DrawRoundRect(dropX + (this->Boder * 0.5f), dropY + (this->Boder * 0.5f),
					dropW - this->Boder, dropH - this->Boder,
					this->DropBorderColor, this->Boder, round);

				auto toAbs = [&](D2D1_RECT_F r) -> D2D1_RECT_F {
					r.left += abs.x; r.right += abs.x;
					r.top += abs.y; r.bottom += abs.y;
					return r;
					};

				auto drawToggle = [&](D2D1_RECT_F rect, bool enabled, const wchar_t* label, bool hovered)
					{
						D2D1_COLOR_F fill = enabled ? this->AccentColor : D2D1_COLOR_F{ 0.92f,0.92f,0.92f,1.0f };
						if (hovered)
							fill = enabled ? D2D1_COLOR_F{ this->AccentColor.r, this->AccentColor.g, this->AccentColor.b, 0.85f }
							: D2D1_COLOR_F{ 0.86f,0.86f,0.86f,1.0f };
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

					std::wstring monthText = StringHelper::Format(L"%04d年%02d月", _viewYear, _viewMonth);
					auto monthSize = font->GetTextSize(monthText);
					float monthCenterX = abs.x + layout.contentLeft + layout.contentWidth * 0.5f;
					float monthX = monthCenterX - monthSize.width * 0.5f;
					float monthY = abs.y + layout.monthHeaderTop + (layout.monthHeaderHeight - monthSize.height) * 0.5f;
					d2d->DrawString(monthText, monthX, monthY, this->ForeColor, font);

					// 星期标题
					static const wchar_t* weekNames[7] = { L"日", L"一", L"二", L"三", L"四", L"五", L"六" };
					for (int i = 0; i < 7; i++)
					{
						std::wstring w = weekNames[i];
						auto ts = font->GetTextSize(w);
						float cx = abs.x + layout.contentLeft + layout.cellWidth * (i + 0.5f);
						float ty = abs.y + layout.weekTop + (layout.weekHeight - ts.height) * 0.5f;
						float tx = cx - ts.width * 0.5f;
						d2d->DrawString(w, tx, ty, this->SecondaryTextColor, font);
					}

					int days = DaysInMonth(_viewYear, _viewMonth);
					int first = FirstWeekday(_viewYear, _viewMonth);
					SYSTEMTIME today{};
					::GetLocalTime(&today);
					for (int i = 0; i < 42; i++)
					{
						int day = i - first + 1;
						if (day < 1 || day > days) continue;
						int row = i / 7;
						int col = i % 7;
						float left = abs.x + layout.contentLeft + layout.cellWidth * col;
						float top = abs.y + layout.gridTop + layout.cellHeight * row;
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
					auto drawTimeBox = [&](D2D1_RECT_F rect, bool hoverUp, bool hoverDown)
						{
							auto absRect = toAbs(rect);
							d2d->FillRoundRect(absRect.left, absRect.top, absRect.right - absRect.left, absRect.bottom - absRect.top, this->PanelBackColor, 5.0f);
							d2d->DrawRoundRect(absRect.left, absRect.top, absRect.right - absRect.left, absRect.bottom - absRect.top, this->DropBorderColor, 1.0f, 5.0f);
							if (hoverUp)
								d2d->FillRect(absRect.right - 16.0f, absRect.top, 16.0f, (absRect.bottom - absRect.top) * 0.5f, this->HoverColor);
							if (hoverDown)
								d2d->FillRect(absRect.right - 16.0f, absRect.top + (absRect.bottom - absRect.top) * 0.5f, 16.0f, (absRect.bottom - absRect.top) * 0.5f, this->HoverColor);
						};

					drawTimeBox(layout.hourRect, _hoverPart == HitPart::HourUp, _hoverPart == HitPart::HourDown);
					drawTimeBox(layout.minuteRect, _hoverPart == HitPart::MinuteUp, _hoverPart == HitPart::MinuteDown);

					std::wstring hourText = StringHelper::Format(L"%02d", _value.wHour);
					std::wstring minuteText = StringHelper::Format(L"%02d", _value.wMinute);
					auto hSize = font->GetTextSize(hourText);
					auto mSize = font->GetTextSize(minuteText);

					auto hourAbs = toAbs(layout.hourRect);
					auto minuteAbs = toAbs(layout.minuteRect);
					float hx = hourAbs.left + (hourAbs.right - hourAbs.left - 16.0f - hSize.width) * 0.5f;
					float hy = hourAbs.top + (hourAbs.bottom - hourAbs.top - hSize.height) * 0.5f;
					float mx = minuteAbs.left + (minuteAbs.right - minuteAbs.left - 16.0f - mSize.width) * 0.5f;
					float my = minuteAbs.top + (minuteAbs.bottom - minuteAbs.top - mSize.height) * 0.5f;
					d2d->DrawString(hourText, hx, hy, this->ForeColor, font);
					d2d->DrawString(minuteText, mx, my, this->ForeColor, font);

					// 上下三角
					auto drawArrow = [&](float cx, float cy, bool up)
						{
							const float half = 4.5f;
							D2D1_TRIANGLE tri{};
							if (up)
							{
								tri.point1 = D2D1::Point2F(cx - half, cy + half);
								tri.point2 = D2D1::Point2F(cx + half, cy + half);
								tri.point3 = D2D1::Point2F(cx, cy - half);
							}
							else
							{
								tri.point1 = D2D1::Point2F(cx - half, cy - half);
								tri.point2 = D2D1::Point2F(cx + half, cy - half);
								tri.point3 = D2D1::Point2F(cx, cy + half);
							}
							d2d->FillTriangle(tri, this->SecondaryTextColor);
						};

					float hourRight = hourAbs.right - 8.0f;
					float minuteRight = minuteAbs.right - 8.0f;
					drawArrow(hourRight, hourAbs.top + (hourAbs.bottom - hourAbs.top) * 0.32f, true);
					drawArrow(hourRight, hourAbs.top + (hourAbs.bottom - hourAbs.top) * 0.68f, false);
					drawArrow(minuteRight, minuteAbs.top + (minuteAbs.bottom - minuteAbs.top) * 0.32f, true);
					drawArrow(minuteRight, minuteAbs.top + (minuteAbs.bottom - minuteAbs.top) * 0.68f, false);

					// 冒号
					std::wstring colon = L":";
					auto colonSize = font->GetTextSize(colon);
					float colonX = abs.x + layout.contentLeft + layout.contentWidth * 0.5f - colonSize.width * 0.5f;
					float colonY = abs.y + layout.timeTop + (layout.timeHeight - colonSize.height) * 0.5f;
					d2d->DrawString(colon, colonX, colonY, this->SecondaryTextColor, font);
				}
			}
		}

		if (!this->Enable)
		{
			d2d->FillRect((float)abs.x, (float)abs.y, (float)size.cx, (float)size.cy, { 1.0f ,1.0f ,1.0f ,0.5f });
		}
	}
	d2d->PopDrawRect();
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
		if (this->Expand)
		{
			LayoutMetrics layout{};
			if (GetLayoutMetrics(layout))
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
		if (this->Expand)
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
			if (!this->Expand)
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
			this->ParentForm->Selected = NULL;
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
		KeyEventArgs event_obj = KeyEventArgs((Keys)(wParam | 0));
		this->OnKeyDown(this, event_obj);
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
		this->Text = StringHelper::Format(L"%04d-%02d-%02d %02d:%02d",
			_value.wYear, _value.wMonth, _value.wDay, _value.wHour, _value.wMinute);
	}
	else if (_showDate)
	{
		this->Text = StringHelper::Format(L"%04d-%02d-%02d",
			_value.wYear, _value.wMonth, _value.wDay);
	}
	else
	{
		this->Text = StringHelper::Format(L"%02d:%02d",
			_value.wHour, _value.wMinute);
	}
}

int DateTimePicker::DaysInMonth(int year, int month)
{
	static const int days[] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
	int m = std::clamp(month, 1, 12);
	int d = days[m - 1];
	if (m == 2 && DateTime::IsLeapYear(year))
		d = 29;
	return d;
}

int DateTimePicker::FirstWeekday(int year, int month)
{
	DateTime dt(year, month, 1, 0, 0, 0, 0);
	return (int)dt.DayOfWeek;
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
	out.contentWidth = std::max(0.0f, (float)this->Width - pad * 2.0f);
	out.contentTop = baseTop + pad;
	float y = out.contentTop;

	out.showToggleRow = AllowModeSwitch && _allowDate && _allowTime;
	if (out.showToggleRow)
	{
		const float toggleHeight = 24.0f;
		float chipW = std::min(72.0f, std::max(48.0f, (out.contentWidth - 8.0f) * 0.5f));
		float totalW = chipW * 2.0f + 8.0f;
		float startX = out.contentLeft + std::max(0.0f, (out.contentWidth - totalW) * 0.5f);
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
		y += out.cellHeight * 6.0f + 6.0f;
	}

	out.showTime = _showTime;
	if (out.showTime)
	{
		out.timeTop = y;
		out.timeHeight = 32.0f;
		float fieldW = std::min(64.0f, std::max(44.0f, out.contentWidth * 0.28f));
		float gap = 10.0f;
		float centerX = out.contentLeft + out.contentWidth * 0.5f;
		out.hourRect = { centerX - fieldW - gap, y, centerX - gap, y + out.timeHeight };
		out.minuteRect = { centerX + gap, y, centerX + gap + fieldW, y + out.timeHeight };
		const float arrowW = 16.0f;
		out.hourUpRect = { out.hourRect.right - arrowW, y, out.hourRect.right, y + out.timeHeight * 0.5f };
		out.hourDownRect = { out.hourRect.right - arrowW, y + out.timeHeight * 0.5f, out.hourRect.right, y + out.timeHeight };
		out.minuteUpRect = { out.minuteRect.right - arrowW, y, out.minuteRect.right, y + out.timeHeight * 0.5f };
		out.minuteDownRect = { out.minuteRect.right - arrowW, y + out.timeHeight * 0.5f, out.minuteRect.right, y + out.timeHeight };
		y += out.timeHeight + 6.0f;
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
	float gridBottom = gridTop + layout.cellHeight * 6.0f;
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
	if (!GetLayoutMetrics(layout)) return;
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
	}
	return HitPart::None;
}

void DateTimePicker::ToggleDateSection()
{
	if (!_allowDate) return;
	bool next = !_showDate;
	if (!next && !_showTime) return;
	_showDate = next;
	UpdateDisplayText();
	if (this->Expand && this->ParentForm) this->ParentForm->Invalidate(true);
	PostRender();
}

void DateTimePicker::ToggleTimeSection()
{
	if (!_allowTime) return;
	bool next = !_showTime;
	if (!next && !_showDate) return;
	_showTime = next;
	UpdateDisplayText();
	if (this->Expand && this->ParentForm) this->ParentForm->Invalidate(true);
	PostRender();
}

void DateTimePicker::SetExpanded(bool value)
{
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
		_hoverPart = HitPart::None;
		_hoverDay = -1;
	}
	PostRender();
}

