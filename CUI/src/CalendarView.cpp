#define NOMINMAX
#include "CalendarView.h"
#include "Form.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <utility>

namespace
{
	float RectWidth(const D2D1_RECT_F& rect)
	{
		return rect.right - rect.left;
	}

	float RectHeight(const D2D1_RECT_F& rect)
	{
		return rect.bottom - rect.top;
	}

	bool PtInRectF(const D2D1_RECT_F& rect, float x, float y)
	{
		return x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom;
	}

	float TextTop(Font* font, const D2D1_RECT_F& rect)
	{
		const float fontHeight = font ? font->FontHeight : 16.0f;
		return rect.top + (std::max)(0.0f, (RectHeight(rect) - fontHeight) * 0.5f);
	}

	D2D1_COLOR_F FadeColor(D2D1_COLOR_F color, float alphaScale)
	{
		color.a *= alphaScale;
		return color;
	}

	int DaysFromCivil(int y, unsigned m, unsigned d)
	{
		y -= m <= 2;
		const int era = (y >= 0 ? y : y - 399) / 400;
		const unsigned yoe = static_cast<unsigned>(y - era * 400);
		const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
		const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
		return era * 146097 + static_cast<int>(doe) - 719468;
	}

	void CivilFromDays(int z, int& y, int& m, int& d)
	{
		z += 719468;
		const int era = (z >= 0 ? z : z - 146096) / 146097;
		const unsigned doe = static_cast<unsigned>(z - era * 146097);
		const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
		y = static_cast<int>(yoe) + era * 400;
		const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
		const unsigned monthPrime = (5 * doy + 2) / 153;
		d = static_cast<int>(doy - (153 * monthPrime + 2) / 5 + 1);
		m = static_cast<int>(monthPrime + (monthPrime < 10 ? 3 : -9));
		y += (m <= 2);
	}

	SYSTEMTIME MakeDate(int year, int month, int day)
	{
		SYSTEMTIME st{};
		st.wYear = (WORD)year;
		st.wMonth = (WORD)month;
		st.wDay = (WORD)day;
		return st;
	}

	SYSTEMTIME TodayDate()
	{
		SYSTEMTIME st{};
		::GetLocalTime(&st);
		st.wHour = 0;
		st.wMinute = 0;
		st.wSecond = 0;
		st.wMilliseconds = 0;
		return st;
	}

	int DateSerial(const SYSTEMTIME& date)
	{
		return DaysFromCivil((int)date.wYear, (unsigned)date.wMonth, (unsigned)date.wDay);
	}

	SYSTEMTIME DateFromSerial(int serial)
	{
		int year = 0, month = 0, day = 0;
		CivilFromDays(serial, year, month, day);
		return MakeDate(year, month, day);
	}

	int CompareDate(const SYSTEMTIME& lhs, const SYSTEMTIME& rhs)
	{
		int lhsSerial = DateSerial(lhs);
		int rhsSerial = DateSerial(rhs);
		return lhsSerial < rhsSerial ? -1 : (lhsSerial > rhsSerial ? 1 : 0);
	}

	bool IsSameDate(const SYSTEMTIME& lhs, const SYSTEMTIME& rhs)
	{
		return lhs.wYear == rhs.wYear && lhs.wMonth == rhs.wMonth && lhs.wDay == rhs.wDay;
	}

	SYSTEMTIME AddDays(const SYSTEMTIME& date, int days)
	{
		return DateFromSerial(DateSerial(date) + days);
	}

	int DaysInMonth(int year, int month)
	{
		static const int days[] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
		month = (std::clamp)(month, 1, 12);
		int daysInSelectedMonth = days[month - 1];
		bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
		if (month == 2 && leap)
			daysInSelectedMonth = 29;
		return daysInSelectedMonth;
	}

	int FirstWeekday(int year, int month)
	{
		int value = (DaysFromCivil(year, (unsigned)month, 1) + 4) % 7;
		if (value < 0) value += 7;
		return value;
	}

	void NormalizeMonth(int& year, int& month)
	{
		while (month < 1)
		{
			month += 12;
			--year;
		}
		while (month > 12)
		{
			month -= 12;
			++year;
		}
	}

	void AddMonthsTo(int& year, int& month, int delta)
	{
		month += delta;
		NormalizeMonth(year, month);
	}

	SYSTEMTIME AddMonths(const SYSTEMTIME& date, int delta)
	{
		int year = (int)date.wYear;
		int month = (int)date.wMonth + delta;
		NormalizeMonth(year, month);
		int day = (std::min)((int)date.wDay, DaysInMonth(year, month));
		return MakeDate(year, month, day);
	}

	SYSTEMTIME CellDate(int year, int month, int cellIndex, bool& inDisplayMonth)
	{
		const int first = FirstWeekday(year, month);
		const int days = DaysInMonth(year, month);
		int day = cellIndex - first + 1;
		int y = year;
		int m = month;
		inDisplayMonth = true;
		if (day < 1)
		{
			--m;
			NormalizeMonth(y, m);
			day += DaysInMonth(y, m);
			inDisplayMonth = false;
		}
		else if (day > days)
		{
			day -= days;
			++m;
			NormalizeMonth(y, m);
			inDisplayMonth = false;
		}
		return MakeDate(y, m, day);
	}

	std::wstring FormatDate(const SYSTEMTIME& date)
	{
		return std::format(L"{:04d}-{:02d}-{:02d}", date.wYear, date.wMonth, date.wDay);
	}

	bool DateInClosedRange(const SYSTEMTIME& date, const SYSTEMTIME& start, const SYSTEMTIME& end)
	{
		int dateSerial = DateSerial(date);
		int startSerial = DateSerial(start);
		int endSerial = DateSerial(end);
		if (startSerial > endSerial) std::swap(startSerial, endSerial);
		return dateSerial >= startSerial && dateSerial <= endSerial;
	}

	void DrawMonthArrow(D2DGraphics* d2d, const D2D1_RECT_F& rect, bool next, D2D1_COLOR_F color)
	{
		if (!d2d) return;
		const float cx = rect.left + RectWidth(rect) * 0.5f;
		const float cy = rect.top + RectHeight(rect) * 0.5f;
		const float halfW = 3.2f;
		const float halfH = 5.2f;
		if (next)
		{
			d2d->DrawLine(D2D1::Point2F(cx - halfW, cy - halfH), D2D1::Point2F(cx + halfW, cy), color, 1.8f);
			d2d->DrawLine(D2D1::Point2F(cx + halfW, cy), D2D1::Point2F(cx - halfW, cy + halfH), color, 1.8f);
		}
		else
		{
			d2d->DrawLine(D2D1::Point2F(cx + halfW, cy - halfH), D2D1::Point2F(cx - halfW, cy), color, 1.8f);
			d2d->DrawLine(D2D1::Point2F(cx - halfW, cy), D2D1::Point2F(cx + halfW, cy + halfH), color, 1.8f);
		}
	}

	D2D1_POINT_2F RotatePoint(const D2D1_POINT_2F& point, float cx, float cy, float angle)
	{
		const float dx = point.x - cx;
		const float dy = point.y - cy;
		const float s = std::sin(angle);
		const float c = std::cos(angle);
		return D2D1::Point2F(cx + dx * c - dy * s, cy + dx * s + dy * c);
	}

	void DrawDropChevron(D2DGraphics* d2d, float cx, float cy, float size, float progress, D2D1_COLOR_F color)
	{
		if (!d2d) return;
		progress = (std::clamp)(progress, 0.0f, 1.0f);
		const float halfW = size * 0.42f;
		const float halfH = size * 0.26f;
		const float angle = progress * 3.14159265359f;
		D2D1_POINT_2F p1 = D2D1::Point2F(cx - halfW, cy - halfH);
		D2D1_POINT_2F p2 = D2D1::Point2F(cx, cy + halfH);
		D2D1_POINT_2F p3 = D2D1::Point2F(cx + halfW, cy - halfH);
		p1 = RotatePoint(p1, cx, cy, angle);
		p2 = RotatePoint(p2, cx, cy, angle);
		p3 = RotatePoint(p3, cx, cy, angle);
		d2d->DrawLine(p1, p2, color, 1.8f);
		d2d->DrawLine(p2, p3, color, 1.8f);
	}

	void DrawWeekNames(D2DGraphics* d2d, Font* font, const D2D1_RECT_F& weekRect, float cellWidth, D2D1_COLOR_F color)
	{
		static const wchar_t* weekNames[7] = { L"Sun", L"Mon", L"Tue", L"Wed", L"Thu", L"Fri", L"Sat" };
		for (int i = 0; i < 7; ++i)
		{
			std::wstring text = weekNames[i];
			float cx = weekRect.left + cellWidth * (i + 0.5f);
			float cy = weekRect.top + RectHeight(weekRect) * 0.5f;
			d2d->DrawStringCentered(text, cx, cy, color, font);
		}
	}
}

UIClass CalendarView::Type()
{
	return UIClass::UI_CalendarView;
}

void CalendarView::EnsureBindingPropertiesRegistered()
{
	Control::EnsureBindingPropertiesRegistered();
	static const bool registered = []
	{
		using Handler = BindingPropertyMetadata::ChangeHandler;
		BindingPropertyRegistry::Register<CalendarView, SYSTEMTIME>(L"SelectedDate",
			[](CalendarView& target) { return target.SelectedDate; },
			[](CalendarView& target, const SYSTEMTIME& value) { target.SetSelectedDate(value); },
			[](CalendarView& target, Handler handler, DataSourceUpdateMode)
			{
				return target.SelectionChanged.Subscribe(
					[handler = std::move(handler)](Control*) { handler(); });
			});
		return true;
	}();
	(void)registered;
}

CalendarView::CalendarView(int x, int y, int width, int height)
{
	this->Location = POINT{ x, y };
	this->Size = SIZE{ width, height };
	this->BackColor = D2D1_COLOR_F{ 0, 0, 0, 0 };
	this->BorderColor = cui::theme::palette::Border;
	this->ForeColor = cui::theme::palette::TextPrimary;
	this->SelectedDate = TodayDate();
	this->DisplayYear = (int)this->SelectedDate.wYear;
	this->DisplayMonth = (int)this->SelectedDate.wMonth;
}

void CalendarView::SetSelectedDate(const SYSTEMTIME& date, bool fireEvent)
{
	bool changed = !IsSameDate(this->SelectedDate, date);
	this->SelectedDate = MakeDate((int)date.wYear, (int)date.wMonth, (int)date.wDay);
	SyncDisplayFromDate(this->SelectedDate);
	if (changed && fireEvent)
		NotifySelectionChanged();
	this->InvalidateVisual();
}

void CalendarView::SetRange(const SYSTEMTIME& start, const SYSTEMTIME& end, bool fireEvent)
{
	SYSTEMTIME a = MakeDate((int)start.wYear, (int)start.wMonth, (int)start.wDay);
	SYSTEMTIME b = MakeDate((int)end.wYear, (int)end.wMonth, (int)end.wDay);
	if (CompareDate(a, b) > 0)
		std::swap(a, b);
	bool changed = !HasRangeStart || !HasRangeEnd || !IsSameDate(RangeStart, a) || !IsSameDate(RangeEnd, b);
	RangeStart = a;
	RangeEnd = b;
	HasRangeStart = true;
	HasRangeEnd = true;
	_rangeAnchorSet = false;
	SyncDisplayFromDate(RangeStart);
	if (changed && fireEvent)
		NotifySelectionChanged();
	InvalidateVisual();
}

void CalendarView::ClearRange(bool fireEvent)
{
	bool changed = HasRangeStart || HasRangeEnd;
	HasRangeStart = false;
	HasRangeEnd = false;
	_rangeAnchorSet = false;
	if (changed && fireEvent)
		NotifySelectionChanged();
	InvalidateVisual();
}

CalendarDateRange CalendarView::GetRange() const
{
	CalendarDateRange r{};
	r.Start = RangeStart;
	r.End = RangeEnd;
	r.HasStart = HasRangeStart;
	r.HasEnd = HasRangeEnd;
	return r;
}

void CalendarView::SetDisplayMonth(int year, int month)
{
	NormalizeMonth(year, month);
	if (DisplayYear == year && DisplayMonth == month)
		return;
	DisplayYear = year;
	DisplayMonth = month;
	InvalidateVisual();
}

void CalendarView::AddMonths(int delta)
{
	int y = DisplayYear;
	int m = DisplayMonth;
	AddMonthsTo(y, m, delta);
	SetDisplayMonth(y, m);
}

void CalendarView::SyncDisplayFromDate(const SYSTEMTIME& date)
{
	DisplayYear = (int)date.wYear;
	DisplayMonth = (int)date.wMonth;
}

CalendarView::Layout CalendarView::CalcLayout() const
{
	Layout layout{};
	auto size = this->_size;
	const float width = (float)size.cx;
	const float height = (float)size.cy;
	const float border = (std::max)(0.0f, this->Border);
	float top = border;
	if (ShowHeader)
	{
		layout.HeaderRect = D2D1::RectF(border, border, (std::max)(border, width - border), (std::min)(height - border, border + HeaderHeight));
		layout.PrevRect = D2D1::RectF(layout.HeaderRect.left + 7.0f, layout.HeaderRect.top + 5.0f,
			layout.HeaderRect.left + 7.0f + NavButtonSize, layout.HeaderRect.bottom - 5.0f);
		layout.NextRect = D2D1::RectF(layout.HeaderRect.right - 7.0f - NavButtonSize, layout.HeaderRect.top + 5.0f,
			layout.HeaderRect.right - 7.0f, layout.HeaderRect.bottom - 5.0f);
		top = layout.HeaderRect.bottom;
	}
	if (ShowWeekNames)
	{
		layout.WeekRect = D2D1::RectF(border + 8.0f, top, (std::max)(border + 8.0f, width - border - 8.0f),
			(std::min)(height - border, top + WeekHeaderHeight));
		top = layout.WeekRect.bottom;
	}
	layout.GridRect = D2D1::RectF(border + 8.0f, top, (std::max)(border + 8.0f, width - border - 8.0f),
		(std::max)(top, height - border - 8.0f));
	layout.CellWidth = RectWidth(layout.GridRect) / 7.0f;
	layout.CellHeight = RectHeight(layout.GridRect) / 6.0f;
	return layout;
}

int CalendarView::HitTestDate(int localX, int localY, SYSTEMTIME& outDate, bool& inDisplayMonth) const
{
	auto layout = CalcLayout();
	if (!PtInRectF(layout.GridRect, (float)localX, (float)localY) || layout.CellWidth <= 0.0f || layout.CellHeight <= 0.0f)
		return -1;
	int col = (int)(((float)localX - layout.GridRect.left) / layout.CellWidth);
	int row = (int)(((float)localY - layout.GridRect.top) / layout.CellHeight);
	col = (std::clamp)(col, 0, 6);
	row = (std::clamp)(row, 0, 5);
	int cell = row * 7 + col;
	outDate = CellDate(DisplayYear, DisplayMonth, cell, inDisplayMonth);
	if (!ShowTrailingDays && !inDisplayMonth)
		return -1;
	return cell;
}

CursorKind CalendarView::QueryCursor(int localX, int localY)
{
	if (!Enable) return CursorKind::Arrow;
	auto layout = CalcLayout();
	if (ShowHeader && (PtInRectF(layout.PrevRect, (float)localX, (float)localY) || PtInRectF(layout.NextRect, (float)localX, (float)localY)))
		return CursorKind::Hand;
	SYSTEMTIME date{};
	bool inMonth = false;
	return HitTestDate(localX, localY, date, inMonth) >= 0 ? CursorKind::Hand : CursorKind::Arrow;
}

bool CalendarView::HandlesNavigationKey(WPARAM key) const
{
	return key == VK_LEFT || key == VK_RIGHT || key == VK_UP || key == VK_DOWN ||
		key == VK_PRIOR || key == VK_NEXT || key == VK_HOME || key == VK_END;
}

void CalendarView::DrawHeader(D2DGraphics* d2d, const Layout& layout)
{
	if (!ShowHeader || RectHeight(layout.HeaderRect) <= 0.0f)
		return;
	d2d->FillRoundRect(layout.HeaderRect, HeaderBackColor, (std::min)(CornerRadius, 8.0f));
	if (HoverDay == -2)
		d2d->FillRoundRect(layout.PrevRect, HoverColor, 5.0f);
	if (HoverDay == -3)
		d2d->FillRoundRect(layout.NextRect, HoverColor, 5.0f);
	DrawMonthArrow(d2d, layout.PrevRect, false, ForeColor);
	DrawMonthArrow(d2d, layout.NextRect, true, ForeColor);
	std::wstring month = std::format(L"{:04d}-{:02d}", DisplayYear, DisplayMonth);
	d2d->DrawStringCentered(month, layout.HeaderRect.left + RectWidth(layout.HeaderRect) * 0.5f,
		layout.HeaderRect.top + RectHeight(layout.HeaderRect) * 0.5f, ForeColor, Font);
}

void CalendarView::DrawCalendarGrid(D2DGraphics* d2d, const Layout& layout)
{
	if (ShowWeekNames && RectHeight(layout.WeekRect) > 0.0f)
		DrawWeekNames(d2d, Font, layout.WeekRect, layout.CellWidth, MutedTextColor);

	SYSTEMTIME today = TodayDate();
	for (int cell = 0; cell < 42; ++cell)
	{
		bool inMonth = true;
		SYSTEMTIME date = CellDate(DisplayYear, DisplayMonth, cell, inMonth);
		if (!ShowTrailingDays && !inMonth)
			continue;
		int row = cell / 7;
		int col = cell % 7;
		D2D1_RECT_F rect{
			layout.GridRect.left + col * layout.CellWidth,
			layout.GridRect.top + row * layout.CellHeight,
			layout.GridRect.left + (col + 1) * layout.CellWidth,
			layout.GridRect.top + (row + 1) * layout.CellHeight };
		D2D1_RECT_F pill{
			rect.left + CellPadding,
			rect.top + CellPadding,
			rect.right - CellPadding,
			rect.bottom - CellPadding };
		bool selected = false;
		bool rangeMid = false;
		if (SelectionMode == CalendarSelectionMode::Single)
			selected = IsSameDate(date, SelectedDate);
		else
		{
			selected = (HasRangeStart && IsSameDate(date, RangeStart)) || (HasRangeEnd && IsSameDate(date, RangeEnd));
			rangeMid = HasRangeStart && HasRangeEnd && DateInClosedRange(date, RangeStart, RangeEnd) && !selected;
		}
		bool hover = HoverDay > 0 && HoverDay == (int)date.wDay && HoverDayInMonth == inMonth &&
			(inMonth || ShowTrailingDays);
		bool isToday = HighlightToday && IsSameDate(date, today);

		if (rangeMid)
			d2d->FillRoundRect(pill, RangeBackColor, 6.0f);
		if (selected)
		{
			d2d->FillRoundRect(pill, SelectedBackColor, 7.0f);
			d2d->FillRoundRect(pill.left, pill.top + 5.0f, 3.0f, (std::max)(6.0f, RectHeight(pill) - 10.0f), AccentColor, 1.5f);
		}
		else if (hover)
		{
			d2d->FillRoundRect(pill, HoverColor, 7.0f);
		}
		else if (isToday)
		{
			d2d->DrawRoundRect(pill, AccentColor, 1.0f, 7.0f);
		}

		D2D1_COLOR_F textColor = inMonth ? ForeColor : TrailingTextColor;
		if (selected)
			textColor = SelectedForeColor;
		d2d->DrawStringCentered(std::to_wstring(date.wDay), rect.left + RectWidth(rect) * 0.5f,
			rect.top + RectHeight(rect) * 0.5f, textColor, Font);
	}
}

void CalendarView::NotifySelectionChanged()
{
	OnSelectionChanged(this);
	SelectionChanged(this);
}

void CalendarView::SelectDateFromInput(const SYSTEMTIME& date, bool inDisplayMonth)
{
	if (!inDisplayMonth)
		SyncDisplayFromDate(date);
	if (SelectionMode == CalendarSelectionMode::Single)
	{
		SetSelectedDate(date, true);
		return;
	}

	if (!_rangeAnchorSet || (HasRangeStart && HasRangeEnd))
	{
		RangeStart = date;
		RangeEnd = SYSTEMTIME{};
		HasRangeStart = true;
		HasRangeEnd = false;
		_rangeAnchor = date;
		_rangeAnchorSet = true;
		NotifySelectionChanged();
		InvalidateVisual();
		return;
	}

	SYSTEMTIME a = _rangeAnchor;
	SYSTEMTIME b = date;
	if (CompareDate(a, b) > 0)
		std::swap(a, b);
	RangeStart = a;
	RangeEnd = b;
	HasRangeStart = true;
	HasRangeEnd = true;
	_rangeAnchorSet = false;
	NotifySelectionChanged();
	InvalidateVisual();
}

void CalendarView::MoveSelectedDate(int days)
{
	if (SelectionMode == CalendarSelectionMode::Single)
		SetSelectedDate(AddDays(SelectedDate, days), true);
	else if (HasRangeStart)
		SelectDateFromInput(AddDays(HasRangeEnd ? RangeEnd : RangeStart, days), true);
	else
		SelectDateFromInput(TodayDate(), true);
}

void CalendarView::Update()
{
	if (!this->IsVisual) return;
	auto d2d = this->ParentForm ? this->ParentForm->Render : nullptr;
	if (!d2d) return;
	const auto size = this->GetActualSizeDip();
	const float width = size.width;
	const float height = size.height;
	auto layout = CalcLayout();

	this->BeginRender();
	{
		D2D1_COLOR_F surface = this->BackColor.a > 0.0f ? this->BackColor : this->SurfaceColor;
		d2d->FillRoundRect(Border * 0.5f, Border * 0.5f,
			(std::max)(0.0f, width - Border), (std::max)(0.0f, height - Border),
			surface, CornerRadius);
		if (this->Image)
			this->RenderImage(CornerRadius);
		DrawHeader(d2d, layout);
		DrawCalendarGrid(d2d, layout);
		if (Border > 0.0f && BorderColor.a > 0.0f)
			d2d->DrawRoundRect(Border * 0.5f, Border * 0.5f,
				(std::max)(0.0f, width - Border), (std::max)(0.0f, height - Border),
				BorderColor, Border, CornerRadius);
		if (!Enable)
			d2d->FillRoundRect(0.0f, 0.0f, width, height, D2D1_COLOR_F{ 1.0f,1.0f,1.0f,0.48f }, CornerRadius);
	}
	this->EndRender();
}

bool CalendarView::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY)
{
	if (!this->Enable || !this->Visible) return true;
	(void)lParam;
	switch (message)
	{
	case WM_MOUSEWHEEL:
		AddMonths(GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? -1 : 1);
		OnMouseWheel(this, MouseEventArgs(MouseButtons::None, 0, localX, localY, GET_WHEEL_DELTA_WPARAM(wParam)));
		return true;
	case WM_MOUSEMOVE:
	{
		if (ParentForm) ParentForm->UnderMouse = this;
		auto layout = CalcLayout();
		int newHover = -1;
		bool newHoverInMonth = true;
		if (ShowHeader && PtInRectF(layout.PrevRect, (float)localX, (float)localY))
			newHover = -2;
		else if (ShowHeader && PtInRectF(layout.NextRect, (float)localX, (float)localY))
			newHover = -3;
		else
		{
			SYSTEMTIME date{};
			if (HitTestDate(localX, localY, date, newHoverInMonth) >= 0)
				newHover = (int)date.wDay;
		}
		if (newHover != HoverDay || newHoverInMonth != HoverDayInMonth)
		{
			HoverDay = newHover;
			HoverDayInMonth = newHoverInMonth;
			InvalidateVisual();
		}
		OnMouseMove(this, MouseEventArgs(MouseButtons::None, 0, localX, localY, HIWORD(wParam)));
		return true;
	}
	case WM_LBUTTONDOWN:
		if (ParentForm) ParentForm->SetSelectedControl(this, false);
		OnMouseDown(this, MouseEventArgs(MouseButtons::Left, 0, localX, localY, HIWORD(wParam)));
		return true;
	case WM_LBUTTONUP:
	{
		auto layout = CalcLayout();
		if (ShowHeader && PtInRectF(layout.PrevRect, (float)localX, (float)localY))
			AddMonths(-1);
		else if (ShowHeader && PtInRectF(layout.NextRect, (float)localX, (float)localY))
			AddMonths(1);
		else
		{
			SYSTEMTIME date{};
			bool inMonth = true;
			if (HitTestDate(localX, localY, date, inMonth) >= 0)
				SelectDateFromInput(date, inMonth);
		}
		MouseEventArgs e(MouseButtons::Left, 0, localX, localY, HIWORD(wParam));
		OnMouseUp(this, e);
		OnMouseClick(this, e);
		return true;
	}
	case WM_KEYDOWN:
		switch (wParam)
		{
		case VK_LEFT: MoveSelectedDate(-1); break;
		case VK_RIGHT: MoveSelectedDate(1); break;
		case VK_UP: MoveSelectedDate(-7); break;
		case VK_DOWN: MoveSelectedDate(7); break;
		case VK_PRIOR: AddMonths(-1); break;
		case VK_NEXT: AddMonths(1); break;
		case VK_HOME: SetSelectedDate(MakeDate(DisplayYear, DisplayMonth, 1), true); break;
		case VK_END: SetSelectedDate(MakeDate(DisplayYear, DisplayMonth, DaysInMonth(DisplayYear, DisplayMonth)), true); break;
		default: break;
		}
		OnKeyDown(this, KeyEventArgs((Keys)(wParam | 0)));
		return true;
	case WM_KEYUP:
		OnKeyUp(this, KeyEventArgs((Keys)(wParam | 0)));
		return true;
	default:
		break;
	}
	return Control::ProcessMessage(message, wParam, lParam, localX, localY);
}

UIClass DateRangePicker::Type()
{
	return UIClass::UI_DateRangePicker;
}

DateRangePicker::DateRangePicker(std::wstring text, int x, int y, int width, int height)
{
	if (!text.empty())
		this->Placeholder = std::move(text);
	this->Location = POINT{ x, y };
	this->Size = SIZE{ width, height };
	this->BackColor = cui::theme::palette::Surface;
	this->BorderColor = cui::theme::palette::BorderStrong;
	this->ForeColor = cui::theme::palette::TextPrimary;
	this->Cursor = CursorKind::Hand;
	SYSTEMTIME today = TodayDate();
	_viewYear = (int)today.wYear;
	_viewMonth = (int)today.wMonth;
	UpdateDisplayText();
}

float DateRangePicker::DropdownTop() const
{
	return (float)this->_size.cy + (std::max)(0.0f, this->DropGap);
}

float DateRangePicker::CurrentDropProgress()
{
	if (!_animating)
	{
		_dropProgress = this->Expand ? 1.0f : 0.0f;
		return _dropProgress;
	}
	const ULONGLONG now = ::GetTickCount64();
	const ULONGLONG elapsed = now >= _animStartTick ? (now - _animStartTick) : 0;
	const UINT duration = EffectiveAnimationDuration(_animDurationMs);
	float t = duration > 0 ? (float)elapsed / (float)duration : 1.0f;
	if (t >= 1.0f)
	{
		const bool wasCollapsing = (_animTargetProgress <= 0.001f && _dropProgress > 0.001f);
		_dropProgress = _animTargetProgress;
		_animating = false;
		if (wasCollapsing)
			_collapseCleanupPending = true;
		if (_dropProgress <= 0.001f && this->ParentForm && this->ParentForm->ForegroundControl == this)
			this->ParentForm->ForegroundControl = nullptr;
		return _dropProgress;
	}
	t = 1.0f - std::pow(1.0f - (std::clamp)(t, 0.0f, 1.0f), 3.0f);
	_dropProgress = _animStartProgress + (_animTargetProgress - _animStartProgress) * t;
	return _dropProgress;
}

bool DateRangePicker::IsDropDownVisible()
{
	return this->Expand || _animating || _dropProgress > 0.001f;
}

bool DateRangePicker::IsAnimationRunning()
{
	CurrentDropProgress();
	return _animating || _collapseCleanupPending;
}

bool DateRangePicker::GetAnimatedInvalidRect(D2D1_RECT_F& outRect)
{
	if (!IsDropDownVisible() && !_collapseCleanupPending) return false;
	auto abs = this->AbsRect;
	auto layout = CalcLayout();
	abs.right = abs.left + (std::max)((float)this->_size.cx, layout.DropRect.right);
	abs.bottom += (std::max)(0.0f, this->DropGap) + layout.DropHeight;
	outRect = abs;
	return true;
}

SIZE DateRangePicker::ActualSize()
{
	SIZE sz = this->_size;
	if (!_renderingForeground || !IsDropDownVisible())
		return sz;
	auto layout = CalcLayout();
	sz.cx = (LONG)(std::max)((float)sz.cx, RectWidth(layout.DropRect));
	sz.cy += (LONG)std::ceil((std::max)(0.0f, this->DropGap) + layout.DropHeight * CurrentDropProgress());
	return sz;
}

bool DateRangePicker::ContainsForegroundPoint(int localX, int localY)
{
	if (!IsDropDownVisible())
		return false;
	auto layout = CalcLayout();
	D2D1_RECT_F visibleDrop = layout.DropRect;
	visibleDrop.bottom = visibleDrop.top + layout.DropHeight * CurrentDropProgress();
	return PtInRectF(visibleDrop, (float)localX, (float)localY);
}

void DateRangePicker::InvalidateVisual()
{
	if (!this->IsVisual || !this->ParentForm)
	{
		Control::InvalidateVisual();
		return;
	}
	auto currentRect = this->AbsRect;
	if (IsDropDownVisible() || _collapseCleanupPending)
	{
		auto layout = CalcLayout();
		currentRect.right = currentRect.left + (std::max)((float)this->_size.cx, RectWidth(layout.DropRect));
		currentRect.bottom += (std::max)(0.0f, this->DropGap) + layout.DropHeight;
	}
	this->InvalidateVisualRect(currentRect);
}

DateRangePicker::Layout DateRangePicker::CalcLayout() const
{
	Layout layout{};
	float dropW = (std::max)((float)this->_size.cx, CalendarWidth);
	float dropY = DropdownTop();
	float calendarH = (std::max)(210.0f, CalendarHeight);
	layout.DropHeight = calendarH + FooterHeight;
	layout.DropRect = D2D1::RectF(0.0f, dropY, dropW, dropY + layout.DropHeight);
	layout.CalendarRect = D2D1::RectF(8.0f, dropY + 8.0f, dropW - 8.0f, dropY + calendarH - 4.0f);
	layout.HeaderRect = D2D1::RectF(layout.CalendarRect.left, layout.CalendarRect.top,
		layout.CalendarRect.right, layout.CalendarRect.top + 38.0f);
	layout.PrevRect = D2D1::RectF(layout.HeaderRect.left + 7.0f, layout.HeaderRect.top + 5.0f,
		layout.HeaderRect.left + 35.0f, layout.HeaderRect.bottom - 5.0f);
	layout.NextRect = D2D1::RectF(layout.HeaderRect.right - 35.0f, layout.HeaderRect.top + 5.0f,
		layout.HeaderRect.right - 7.0f, layout.HeaderRect.bottom - 5.0f);
	layout.WeekRect = D2D1::RectF(layout.CalendarRect.left, layout.HeaderRect.bottom,
		layout.CalendarRect.right, layout.HeaderRect.bottom + 22.0f);
	layout.GridRect = D2D1::RectF(layout.CalendarRect.left, layout.WeekRect.bottom,
		layout.CalendarRect.right, layout.CalendarRect.bottom);
	layout.CellWidth = RectWidth(layout.GridRect) / 7.0f;
	layout.CellHeight = RectHeight(layout.GridRect) / 6.0f;
	float footerTop = layout.DropRect.bottom - FooterHeight;
	layout.ClearRect = D2D1::RectF(layout.DropRect.left + 10.0f, footerTop + 6.0f, layout.DropRect.left + 84.0f, layout.DropRect.bottom - 8.0f);
	layout.TodayRect = D2D1::RectF(layout.DropRect.right - 86.0f, footerTop + 6.0f, layout.DropRect.right - 10.0f, layout.DropRect.bottom - 8.0f);
	return layout;
}

void DateRangePicker::SetExpanded(bool value)
{
	CurrentDropProgress();
	if (Expand == value && !_animating) return;
	Expand = value;
	_animStartProgress = _dropProgress;
	_animTargetProgress = Expand ? 1.0f : 0.0f;
	_collapseCleanupPending = false;
	if (std::fabs(_animTargetProgress - _animStartProgress) < 0.001f)
	{
		_dropProgress = _animTargetProgress;
		_animating = false;
		if (!Expand && this->ParentForm && this->ParentForm->ForegroundControl == this)
			this->ParentForm->ForegroundControl = nullptr;
	}
	else
	{
		_animStartTick = ::GetTickCount64();
		_animating = true;
	}
	if (this->ParentForm)
	{
		if (Expand)
			this->ParentForm->ForegroundControl = this;
		this->ParentForm->Invalidate(true);
	}
	if (!Expand)
	{
		_hoverPart = HitPart::None;
		_hoverDay = -1;
	}
	InvalidateVisual();
}

void DateRangePicker::SetRange(const SYSTEMTIME& start, const SYSTEMTIME& end, bool fireEvent)
{
	SYSTEMTIME a = MakeDate((int)start.wYear, (int)start.wMonth, (int)start.wDay);
	SYSTEMTIME b = MakeDate((int)end.wYear, (int)end.wMonth, (int)end.wDay);
	if (CompareDate(a, b) > 0)
		std::swap(a, b);
	bool changed = !HasStartDate || !HasEndDate || !IsSameDate(StartDate, a) || !IsSameDate(EndDate, b);
	StartDate = a;
	EndDate = b;
	HasStartDate = true;
	HasEndDate = true;
	_rangeAnchorSet = false;
	SyncViewFromRange();
	UpdateDisplayText();
	if (changed && fireEvent)
		NotifyRangeChanged();
	InvalidateVisual();
}

void DateRangePicker::ClearRange(bool fireEvent)
{
	bool changed = HasStartDate || HasEndDate;
	HasStartDate = false;
	HasEndDate = false;
	_rangeAnchorSet = false;
	UpdateDisplayText();
	if (changed && fireEvent)
		NotifyRangeChanged();
	InvalidateVisual();
}

CalendarDateRange DateRangePicker::GetRange() const
{
	CalendarDateRange r{};
	r.Start = StartDate;
	r.End = EndDate;
	r.HasStart = HasStartDate;
	r.HasEnd = HasEndDate;
	return r;
}

void DateRangePicker::SyncViewFromRange()
{
	if (HasStartDate)
	{
		_viewYear = (int)StartDate.wYear;
		_viewMonth = (int)StartDate.wMonth;
	}
	else
	{
		SYSTEMTIME today = TodayDate();
		_viewYear = (int)today.wYear;
		_viewMonth = (int)today.wMonth;
	}
}

void DateRangePicker::UpdateDisplayText()
{
	if (HasStartDate && HasEndDate)
		this->Text = FormatDate(StartDate) + SeparatorText + FormatDate(EndDate);
	else if (HasStartDate)
		this->Text = FormatDate(StartDate) + SeparatorText;
	else
		this->Text = L"";
}

void DateRangePicker::NotifyRangeChanged()
{
	OnRangeChanged(this);
	SelectionChanged(this);
}

void DateRangePicker::AddMonths(int delta)
{
	AddMonthsTo(_viewYear, _viewMonth, delta);
	InvalidateVisual();
}

DateRangePicker::HitPart DateRangePicker::HitTestPart(const Layout& layout, int localX, int localY, SYSTEMTIME& outDate, bool& inDisplayMonth) const
{
	outDate = SYSTEMTIME{};
	inDisplayMonth = true;
	if (localY >= 0 && localY <= this->_size.cy)
		return HitPart::Header;
	const bool dropVisible = this->Expand || _animating || _dropProgress > 0.001f;
	if (!dropVisible)
		return HitPart::None;
	if (PtInRectF(layout.PrevRect, (float)localX, (float)localY)) return HitPart::PrevMonth;
	if (PtInRectF(layout.NextRect, (float)localX, (float)localY)) return HitPart::NextMonth;
	if (PtInRectF(layout.TodayRect, (float)localX, (float)localY)) return HitPart::Today;
	if (PtInRectF(layout.ClearRect, (float)localX, (float)localY)) return HitPart::Clear;
	if (PtInRectF(layout.GridRect, (float)localX, (float)localY) && layout.CellWidth > 0.0f && layout.CellHeight > 0.0f)
	{
		int col = (int)(((float)localX - layout.GridRect.left) / layout.CellWidth);
		int row = (int)(((float)localY - layout.GridRect.top) / layout.CellHeight);
		col = (std::clamp)(col, 0, 6);
		row = (std::clamp)(row, 0, 5);
		int cell = row * 7 + col;
		outDate = CellDate(_viewYear, _viewMonth, cell, inDisplayMonth);
		if (!ShowTrailingDays && !inDisplayMonth)
			return HitPart::None;
		return HitPart::DayCell;
	}
	return HitPart::None;
}

void DateRangePicker::UpdateHoverState(int localX, int localY)
{
	auto layout = CalcLayout();
	SYSTEMTIME date{};
	bool inMonth = true;
	auto part = HitTestPart(layout, localX, localY, date, inMonth);
	int day = part == HitPart::DayCell ? (int)date.wDay : -1;
	if (part != _hoverPart || day != _hoverDay || inMonth != _hoverInMonth)
	{
		_hoverPart = part;
		_hoverDay = day;
		_hoverInMonth = inMonth;
		InvalidateVisual();
	}
}

void DateRangePicker::SelectDateFromInput(const SYSTEMTIME& date, bool inDisplayMonth)
{
	if (!inDisplayMonth)
	{
		_viewYear = (int)date.wYear;
		_viewMonth = (int)date.wMonth;
	}

	if (!_rangeAnchorSet || (HasStartDate && HasEndDate))
	{
		StartDate = date;
		EndDate = SYSTEMTIME{};
		HasStartDate = true;
		HasEndDate = false;
		_rangeAnchor = date;
		_rangeAnchorSet = true;
		UpdateDisplayText();
		NotifyRangeChanged();
		InvalidateVisual();
		return;
	}

	SYSTEMTIME a = _rangeAnchor;
	SYSTEMTIME b = date;
	if (CompareDate(a, b) > 0)
		std::swap(a, b);
	StartDate = a;
	EndDate = b;
	HasStartDate = true;
	HasEndDate = true;
	_rangeAnchorSet = false;
	UpdateDisplayText();
	NotifyRangeChanged();
	if (AutoCloseOnComplete)
		SetExpanded(false);
	InvalidateVisual();
}

CursorKind DateRangePicker::QueryCursor(int localX, int localY)
{
	if (!Enable) return CursorKind::Arrow;
	auto layout = CalcLayout();
	SYSTEMTIME date{};
	bool inMonth = true;
	auto part = HitTestPart(layout, localX, localY, date, inMonth);
	return part == HitPart::None ? CursorKind::Arrow : CursorKind::Hand;
}

void DateRangePicker::DrawHeader(D2DGraphics* d2d, const Layout& layout)
{
	d2d->FillRoundRect(layout.HeaderRect, DropBorderColor.a > 0.0f ? FadeColor(DropBorderColor, 0.12f) : HeaderHoverBackColor, 7.0f);
	if (_hoverPart == HitPart::PrevMonth)
		d2d->FillRoundRect(layout.PrevRect, HoverColor, 5.0f);
	if (_hoverPart == HitPart::NextMonth)
		d2d->FillRoundRect(layout.NextRect, HoverColor, 5.0f);
	DrawMonthArrow(d2d, layout.PrevRect, false, ForeColor);
	DrawMonthArrow(d2d, layout.NextRect, true, ForeColor);
	std::wstring month = std::format(L"{:04d}-{:02d}", _viewYear, _viewMonth);
	d2d->DrawStringCentered(month, layout.HeaderRect.left + RectWidth(layout.HeaderRect) * 0.5f,
		layout.HeaderRect.top + RectHeight(layout.HeaderRect) * 0.5f, ForeColor, Font);
}

void DateRangePicker::DrawCalendarGrid(D2DGraphics* d2d, const Layout& layout)
{
	DrawWeekNames(d2d, Font, layout.WeekRect, layout.CellWidth, SecondaryTextColor);
	SYSTEMTIME today = TodayDate();
	for (int cell = 0; cell < 42; ++cell)
	{
		bool inMonth = true;
		SYSTEMTIME date = CellDate(_viewYear, _viewMonth, cell, inMonth);
		if (!ShowTrailingDays && !inMonth)
			continue;
		int row = cell / 7;
		int col = cell % 7;
		D2D1_RECT_F rect{
			layout.GridRect.left + col * layout.CellWidth,
			layout.GridRect.top + row * layout.CellHeight,
			layout.GridRect.left + (col + 1) * layout.CellWidth,
			layout.GridRect.top + (row + 1) * layout.CellHeight };
		D2D1_RECT_F pill{
			rect.left + 3.0f,
			rect.top + 3.0f,
			rect.right - 3.0f,
			rect.bottom - 3.0f };
		bool selected = (HasStartDate && IsSameDate(date, StartDate)) || (HasEndDate && IsSameDate(date, EndDate));
		bool rangeMid = HasStartDate && HasEndDate && DateInClosedRange(date, StartDate, EndDate) && !selected;
		bool hover = _hoverPart == HitPart::DayCell && _hoverDay == (int)date.wDay && _hoverInMonth == inMonth;
		bool isToday = IsSameDate(date, today);
		if (rangeMid)
			d2d->FillRoundRect(pill, RangeBackColor, 6.0f);
		if (selected)
		{
			d2d->FillRoundRect(pill, SelectedBackColor, 7.0f);
			d2d->FillRoundRect(pill.left, pill.top + 5.0f, 3.0f, (std::max)(6.0f, RectHeight(pill) - 10.0f), AccentColor, 1.5f);
		}
		else if (hover)
			d2d->FillRoundRect(pill, HoverColor, 7.0f);
		else if (isToday)
			d2d->DrawRoundRect(pill, AccentColor, 1.0f, 7.0f);
		D2D1_COLOR_F textColor = inMonth ? ForeColor : FadeColor(SecondaryTextColor, 0.72f);
		if (selected)
			textColor = SelectedForeColor;
		d2d->DrawStringCentered(std::to_wstring(date.wDay), rect.left + RectWidth(rect) * 0.5f,
			rect.top + RectHeight(rect) * 0.5f, textColor, Font);
	}
}

void DateRangePicker::Update()
{
	if (!this->IsVisual) return;
	auto d2d = this->ParentForm ? this->ParentForm->Render : nullptr;
	if (!d2d) return;
	const auto size = this->GetActualSizeDip();
	const float width = size.width;
	const float dropProgress = CurrentDropProgress();
	const bool selected = ParentForm && ParentForm->Selected == this;
	const bool hoverHeader = ParentForm && ParentForm->UnderMouse == this;

	this->BeginRender(size.width, size.height);
	{
		const float round = (std::min)(Round, (float)this->Height * 0.5f);
		d2d->FillRoundRect(0.0f, 0.0f, (float)this->Width, (float)this->Height, PanelBackColor, round);
		if (hoverHeader || IsDropDownVisible())
			d2d->FillRoundRect(0.0f, 0.0f, (float)this->Width, (float)this->Height, HeaderHoverBackColor, round);
		std::wstring text = this->Text.empty() ? Placeholder : this->Text;
		D2D1_COLOR_F textColor = this->Text.empty() ? SecondaryTextColor : ForeColor;
		D2D1_RECT_F textRect{ 10.0f, 0.0f, (float)this->Width - 34.0f, (float)this->Height };
		d2d->PushDrawRect(textRect.left, textRect.top, (std::max)(1.0f, RectWidth(textRect)), RectHeight(textRect));
		d2d->DrawString(text, textRect.left, TextTop(Font, textRect), textColor, Font);
		d2d->PopDrawRect();
		DrawDropChevron(d2d, (float)this->Width - 16.0f, (float)this->Height * 0.5f, ChevronSize, dropProgress, ForeColor);
		d2d->DrawRoundRect(Border * 0.5f, Border * 0.5f, (float)this->Width - Border, (float)this->Height - Border,
			(selected || IsDropDownVisible()) ? FocusBorderColor : BorderColor, Border, round - Border);

		if (IsDropDownVisible() && dropProgress > 0.001f)
		{
			auto layout = CalcLayout();
			float visibleDropH = layout.DropHeight * dropProgress;
			d2d->PushDrawRect(layout.DropRect.left, layout.DropRect.top, RectWidth(layout.DropRect), visibleDropH);
			d2d->FillRoundRect(layout.DropRect, DropBackColor, DropCornerRadius);
			d2d->DrawRoundRect(layout.DropRect.left + Border * 0.5f, layout.DropRect.top + Border * 0.5f,
				RectWidth(layout.DropRect) - Border, RectHeight(layout.DropRect) - Border,
				DropBorderColor, Border, DropCornerRadius);
			DrawHeader(d2d, layout);
			DrawCalendarGrid(d2d, layout);
			d2d->DrawLine(layout.DropRect.left + 10.0f, layout.TodayRect.top - 7.0f,
				layout.DropRect.right - 10.0f, layout.TodayRect.top - 7.0f, FadeColor(DropBorderColor, 0.72f), 1.0f);

			auto drawButton = [&](const D2D1_RECT_F& rect, const std::wstring& label, bool primary, bool hovered)
				{
					D2D1_COLOR_F back = primary ? ButtonBackColor : FadeColor(DropBorderColor, 0.18f);
					D2D1_COLOR_F fore = primary ? ButtonTextColor : ForeColor;
					d2d->FillRoundRect(rect, back, 6.0f);
					if (hovered)
						d2d->FillRoundRect(rect, HoverColor, 6.0f);
					d2d->DrawRoundRect(rect, primary ? AccentColor : DropBorderColor, 1.0f, 6.0f);
					d2d->DrawStringCentered(label, rect.left + RectWidth(rect) * 0.5f, rect.top + RectHeight(rect) * 0.5f, fore, Font);
				};
			drawButton(layout.ClearRect, L"Clear", false, _hoverPart == HitPart::Clear);
			drawButton(layout.TodayRect, L"Today", true, _hoverPart == HitPart::Today);
			d2d->PopDrawRect();
		}
		if (!Enable)
			d2d->FillRoundRect(0.0f, 0.0f, width, size.height, D2D1_COLOR_F{ 1.0f,1.0f,1.0f,0.48f }, round);
	}
	this->EndRender();
	if (!_animating && _dropProgress <= 0.001f)
		_collapseCleanupPending = false;
}

void DateRangePicker::UpdateForeground()
{
	if (!this->IsVisual) return;
	auto d2d = this->ParentForm ? this->ParentForm->Render : nullptr;
	if (!d2d) return;

	auto layout = CalcLayout();
	const float dropProgress = CurrentDropProgress();
	if (dropProgress <= 0.001f || layout.DropHeight <= 0.0f)
	{
		if (!_animating && _dropProgress <= 0.001f)
			_collapseCleanupPending = false;
		return;
	}

	const auto abs = this->GetAbsoluteLocationDip();
	d2d->PushDrawRect(
		static_cast<float>(abs.x) + layout.DropRect.left,
		static_cast<float>(abs.y) + layout.DropRect.top,
		RectWidth(layout.DropRect),
		layout.DropHeight * dropProgress);
	_renderingForeground = true;
	this->Update();
	_renderingForeground = false;
	d2d->PopDrawRect();
}

bool DateRangePicker::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY)
{
	if (!this->Enable || !this->Visible) return true;
	(void)lParam;
	if (message == WM_LBUTTONDOWN && ParentForm)
		ParentForm->SetSelectedControl(this, false);

	switch (message)
	{
	case WM_MOUSEWHEEL:
		if (Expand)
			AddMonths(GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? -1 : 1);
		OnMouseWheel(this, MouseEventArgs(MouseButtons::None, 0, localX, localY, GET_WHEEL_DELTA_WPARAM(wParam)));
		return true;
	case WM_MOUSEMOVE:
		if (ParentForm) ParentForm->UnderMouse = this;
		if (Expand)
			UpdateHoverState(localX, localY);
		OnMouseMove(this, MouseEventArgs(MouseButtons::None, 0, localX, localY, HIWORD(wParam)));
		return true;
	case WM_LBUTTONDOWN:
		OnMouseDown(this, MouseEventArgs(MouseButtons::Left, 0, localX, localY, HIWORD(wParam)));
		return true;
	case WM_LBUTTONUP:
	{
		auto layout = CalcLayout();
		SYSTEMTIME date{};
		bool inMonth = true;
		auto part = HitTestPart(layout, localX, localY, date, inMonth);
		switch (part)
		{
		case HitPart::Header:
			SetExpanded(!Expand);
			break;
		case HitPart::PrevMonth:
			AddMonths(-1);
			break;
		case HitPart::NextMonth:
			AddMonths(1);
			break;
		case HitPart::DayCell:
			SelectDateFromInput(date, inMonth);
			break;
		case HitPart::Today:
		{
			SYSTEMTIME today = TodayDate();
			SetRange(today, today, true);
			if (AutoCloseOnComplete)
				SetExpanded(false);
			break;
		}
		case HitPart::Clear:
			ClearRange(true);
			break;
		default:
			break;
		}
		MouseEventArgs e(MouseButtons::Left, 0, localX, localY, HIWORD(wParam));
		OnMouseUp(this, e);
		OnMouseClick(this, e);
		return true;
	}
	case WM_KEYDOWN:
		if (wParam == VK_ESCAPE && Expand)
			SetExpanded(false);
		else if (wParam == VK_RETURN || wParam == VK_SPACE)
			SetExpanded(!Expand);
		OnKeyDown(this, KeyEventArgs((Keys)(wParam | 0)));
		return true;
	case WM_KEYUP:
		OnKeyUp(this, KeyEventArgs((Keys)(wParam | 0)));
		return true;
	default:
		break;
	}
	return Control::ProcessMessage(message, wParam, lParam, localX, localY);
}
