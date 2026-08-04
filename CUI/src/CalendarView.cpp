#define NOMINMAX
#include "CalendarView.h"
#include "Window.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <utility>

namespace
{
	constexpr size_t CalendarPreviousIndex = 0;
	constexpr size_t CalendarHeaderIndex = 1;
	constexpr size_t CalendarNextIndex = 2;
	constexpr size_t CalendarColumnHeaderIndex = 3;
	constexpr size_t CalendarDayIndex = 10;
	constexpr size_t CalendarDayCount = 42;

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

	SYSTEMTIME AddMonthsToDate(const SYSTEMTIME& date, int delta)
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

const DependencyProperty& CalendarView::SelectionModeProperty()
{
	static const auto registration = []
	{
		DependencyPropertyOptions<CalendarView, int> options;
		options.DefaultValue =
			static_cast<int>(CalendarSelectionMode::SingleDate);
		options.Flags = DependencyPropertyFlags::AffectsRender;
		options.Validate = [](const int& value)
		{
			return value >= static_cast<int>(CalendarSelectionMode::None)
				&& value <= static_cast<int>(
					CalendarSelectionMode::SingleRange);
		};
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Behavior";
		options.Design.CategoryOrder = 110;
		options.Design.Order = 10;
		options.Design.Editor = DependencyPropertyEditorKind::Choice;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		options.Design.Choices = {
			{ L"None", BindingValue(static_cast<int>(
				CalendarSelectionMode::None)) },
			{ L"SingleDate", BindingValue(static_cast<int>(
				CalendarSelectionMode::SingleDate)) },
			{ L"SingleRange", BindingValue(static_cast<int>(
				CalendarSelectionMode::SingleRange)) }
		};
		)
		return DependencyPropertyRegistry::RegisterStatic<CalendarView, int>(
			DependencyPropertyRegistrationLiteral(L"SelectionMode"),
			[](CalendarView& target)
			{ return static_cast<int>(target._selectionMode); },
			[](CalendarView& target, const int& value)
			{
				target._selectionMode =
					static_cast<CalendarSelectionMode>(value);
			}, {}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& CalendarView::SelectedDateProperty()
{
	static const auto registration = []
	{
		using Handler = DependencyPropertyMetadata::ChangeHandler;
		DependencyPropertyOptions<CalendarView, SYSTEMTIME> options;
		options.DefaultValue = TodayDate();
		options.Flags = DependencyPropertyFlags::AffectsRender
			| DependencyPropertyFlags::BindsTwoWayByDefault;
		options.Equals = [](const SYSTEMTIME& left,
			const SYSTEMTIME& right) { return IsSameDate(left, right); };
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Common";
		options.Design.CategoryOrder = 0;
		options.Design.Order = 10;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		)
		options.Changed = [](
			CalendarView& target, const SYSTEMTIME&, const SYSTEMTIME& value)
		{
			target.SyncDisplayFromDate(value);
			target.NotifySelectedDatesChanged();
		};
		return DependencyPropertyRegistry::RegisterStatic<
			CalendarView, SYSTEMTIME>(
				DependencyPropertyRegistrationLiteral(L"SelectedDate"),
				[](CalendarView& target) { return target._selectedDate; },
				[](CalendarView& target, const SYSTEMTIME& value)
				{
					target._selectedDate = MakeDate(
						static_cast<int>(value.wYear),
						static_cast<int>(value.wMonth),
						static_cast<int>(value.wDay));
					target._currentDate = target._selectedDate;
				},
				[](CalendarView& target, Handler handler,
					DataSourceUpdateMode)
				{
					return target.SelectedDatesChanged.Subscribe(
						[handler = std::move(handler)](
							Control*, SelectionChangedEventArgs&) { handler(); });
				}, std::move(options));
	}();
	return *registration;
}

void CalendarView::RegisterDependencyProperties()
{
	Control::RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	(void)SelectionModeProperty();
	(void)SelectedDateProperty();
#endif
}

CalendarView::CalendarView()
{
	this->RendererBackgroundColor = D2D1_COLOR_F{ 0, 0, 0, 0 };
	this->RendererBorderColor = cui::theme::palette::Border;
	this->RendererForegroundColor = cui::theme::palette::TextPrimary;
	this->_selectedDate = TodayDate();
	this->_currentDate = this->_selectedDate;
	this->DisplayYear = (int)this->_selectedDate.wYear;
	this->DisplayMonth = (int)this->_selectedDate.wMonth;
	for (auto& id : _accessibilityIds)
		id = AllocateAccessibilityVirtualId();
}

void CalendarView::SetSelectionMode(CalendarSelectionMode value)
{
	(void)TrySetPropertyValue(SelectionModeProperty(),
		BindingValue(static_cast<int>(value)));
}

void CalendarView::SetSelectedDate(const SYSTEMTIME& date)
{
	const auto normalized = MakeDate(
		static_cast<int>(date.wYear),
		static_cast<int>(date.wMonth),
		static_cast<int>(date.wDay));
	_currentDate = normalized;
	(void)TrySetCurrentPropertyValue(
		SelectedDateProperty(), BindingValue(normalized));
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
	_currentDate = RangeEnd;
	SyncDisplayFromDate(RangeStart);
	if (changed && fireEvent)
		NotifySelectedDatesChanged();
	InvalidateVisual();
}

void CalendarView::ClearRange(bool fireEvent)
{
	bool changed = HasRangeStart || HasRangeEnd;
	HasRangeStart = false;
	HasRangeEnd = false;
	_rangeAnchorSet = false;
	if (changed && fireEvent)
		NotifySelectedDatesChanged();
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
	NotifyAccessibilityVirtualChanged(
		_accessibilityIds[CalendarHeaderIndex], AccessibilityChange::Name);
	for (size_t cell = 0; cell < CalendarDayCount; ++cell)
		NotifyAccessibilityVirtualChanged(
			_accessibilityIds[CalendarDayIndex + cell],
			AccessibilityChange::Name);
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
	SetDisplayMonth((int)date.wYear, (int)date.wMonth);
}

CalendarView::Layout CalendarView::CalcLayout() const
{
	Layout layout{};
	const auto size = GetActualSizeDip();
	const float width = size.width;
	const float height = size.height;
	const float inset = 0.0f;
	float top = inset;
	if (ShowHeader)
	{
		layout.HeaderRect = D2D1::RectF(inset, inset,
			(std::max)(inset, width - inset),
			(std::min)(height - inset, inset + HeaderHeight));
		layout.PrevRect = D2D1::RectF(layout.HeaderRect.left + 7.0f, layout.HeaderRect.top + 5.0f,
			layout.HeaderRect.left + 7.0f + NavButtonSize, layout.HeaderRect.bottom - 5.0f);
		layout.NextRect = D2D1::RectF(layout.HeaderRect.right - 7.0f - NavButtonSize, layout.HeaderRect.top + 5.0f,
			layout.HeaderRect.right - 7.0f, layout.HeaderRect.bottom - 5.0f);
		top = layout.HeaderRect.bottom;
	}
	if (ShowWeekNames)
	{
		layout.WeekRect = D2D1::RectF(inset + 8.0f, top,
			(std::max)(inset + 8.0f, width - inset - 8.0f),
			(std::min)(height - inset, top + WeekHeaderHeight));
		top = layout.WeekRect.bottom;
	}
	layout.GridRect = D2D1::RectF(inset + 8.0f, top,
		(std::max)(inset + 8.0f, width - inset - 8.0f),
		(std::max)(top, height - inset - 8.0f));
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
	if (!IsEnabled) return CursorKind::Arrow;
	auto layout = CalcLayout();
	if (ShowHeader && (PtInRectF(layout.PrevRect, (float)localX, (float)localY) || PtInRectF(layout.NextRect, (float)localX, (float)localY)))
		return CursorKind::Hand;
	SYSTEMTIME date{};
	bool inMonth = false;
	return HitTestDate(localX, localY, date, inMonth) >= 0 ? CursorKind::Hand : CursorKind::Arrow;
}

bool CalendarView::HandlesNavigationKey(Key key) const
{
	return key == Key::Left || key == Key::Right
		|| key == Key::Up || key == Key::Down
		|| key == Key::PageUp || key == Key::PageDown
		|| key == Key::Home || key == Key::End
		|| key == Key::Return || key == Key::Space;
}

size_t CalendarView::GetAccessibilityVirtualChildCount(
	uint32_t parentId) const noexcept
{
	return parentId == 0 ? _accessibilityIds.size() : 0;
}

bool CalendarView::TryGetAccessibilityVirtualChildAt(
	uint32_t parentId, size_t index, uint32_t& result) const noexcept
{
	result = 0;
	if (parentId != 0 || index >= _accessibilityIds.size()) return false;
	result = _accessibilityIds[index];
	return result != 0;
}

bool CalendarView::TryGetAccessibilityVirtualSibling(
	uint32_t parentId, uint32_t id, bool next,
	uint32_t& result) const noexcept
{
	result = 0;
	if (parentId != 0 || id == 0) return false;
	const auto found = std::find(
		_accessibilityIds.begin(), _accessibilityIds.end(), id);
	if (found == _accessibilityIds.end()) return false;
	const auto index = static_cast<size_t>(
		std::distance(_accessibilityIds.begin(), found));
	if ((!next && index == 0)
		|| (next && index + 1 >= _accessibilityIds.size()))
		return false;
	result = _accessibilityIds[next ? index + 1 : index - 1];
	return result != 0;
}

bool CalendarView::TryGetAccessibilityVirtualNode(
	uint32_t id, AccessibilityVirtualNode& result)
{
	const auto found = std::find(
		_accessibilityIds.begin(), _accessibilityIds.end(), id);
	if (found == _accessibilityIds.end()) return false;
	const size_t index = static_cast<size_t>(
		std::distance(_accessibilityIds.begin(), found));
	const auto layout = CalcLayout();
	const auto snapshot = GetAccessibilitySnapshot();
	const std::wstring prefix = snapshot.AutomationId.empty()
		? L"calendar" : snapshot.AutomationId;

	result = {};
	result.Id = id;
	result.Enabled = IsEffectivelyEnabled();
	result.ReadOnly = false;

	if (index == CalendarPreviousIndex || index == CalendarNextIndex)
	{
		const bool next = index == CalendarNextIndex;
		result.ControlType = AutomationControlType::Button;
		result.Patterns = AutomationPattern::Invoke;
		result.Name = next ? L"Next month" : L"Previous month";
		result.AutomationId = prefix + (next ? L".next" : L".previous");
		result.BoundsDip = next ? layout.NextRect : layout.PrevRect;
		result.Visible = IsVisible && ShowHeader;
		return true;
	}

	if (index == CalendarHeaderIndex)
	{
		result.ControlType = AutomationControlType::HeaderItem;
		result.Name = std::format(
			L"{:04d}-{:02d}", DisplayYear, DisplayMonth);
		result.Value = result.Name;
		result.AutomationId = prefix + L".header";
		result.BoundsDip = layout.HeaderRect;
		result.Visible = IsVisible && ShowHeader;
		result.ReadOnly = true;
		return true;
	}

	if (index >= CalendarColumnHeaderIndex
		&& index < CalendarDayIndex)
	{
		static const wchar_t* names[] = {
			L"Sunday", L"Monday", L"Tuesday", L"Wednesday",
			L"Thursday", L"Friday", L"Saturday"
		};
		const int column = static_cast<int>(
			index - CalendarColumnHeaderIndex);
		result.ControlType = AutomationControlType::HeaderItem;
		result.Name = names[column];
		result.AutomationId =
			prefix + L".column-" + std::to_wstring(column);
		result.BoundsDip = D2D1::RectF(
			layout.WeekRect.left + layout.CellWidth * column,
			layout.WeekRect.top,
			layout.WeekRect.left + layout.CellWidth * (column + 1),
			layout.WeekRect.bottom);
		result.Visible = IsVisible && ShowWeekNames;
		result.Column = column;
		result.ReadOnly = true;
		return true;
	}

	const int cell = static_cast<int>(index - CalendarDayIndex);
	if (cell < 0 || cell >= static_cast<int>(CalendarDayCount))
		return false;
	bool inDisplayMonth = true;
	const auto date = CellDate(
		DisplayYear, DisplayMonth, cell, inDisplayMonth);
	const int row = cell / 7;
	const int column = cell % 7;
	const bool selected = _selectionMode == CalendarSelectionMode::SingleDate
		? IsSameDate(date, _selectedDate)
		: HasRangeStart && HasRangeEnd
			&& DateInClosedRange(date, RangeStart, RangeEnd);

	result.ControlType = AutomationControlType::Button;
	result.Patterns = AutomationPattern::Invoke
		| AutomationPattern::SelectionItem
		| AutomationPattern::GridItem
		| AutomationPattern::TableItem;
	result.Name = FormatDate(date);
	result.Value = result.Name;
	result.AutomationId =
		prefix + L".cell-" + std::to_wstring(cell);
	result.BoundsDip = D2D1::RectF(
		layout.GridRect.left + layout.CellWidth * column,
		layout.GridRect.top + layout.CellHeight * row,
		layout.GridRect.left + layout.CellWidth * (column + 1),
		layout.GridRect.top + layout.CellHeight * (row + 1));
	result.Visible = IsVisible && (inDisplayMonth || ShowTrailingDays);
	result.Selected = selected;
	result.Row = row;
	result.Column = column;
	return true;
}

bool CalendarView::TryHitTestAccessibilityVirtualNode(
	float localX, float localY, uint32_t& result) const
{
	result = 0;
	const auto layout = CalcLayout();
	if (ShowHeader)
	{
		if (PtInRectF(layout.PrevRect, localX, localY))
		{
			result = _accessibilityIds[CalendarPreviousIndex];
			return true;
		}
		if (PtInRectF(layout.NextRect, localX, localY))
		{
			result = _accessibilityIds[CalendarNextIndex];
			return true;
		}
		if (PtInRectF(layout.HeaderRect, localX, localY))
		{
			result = _accessibilityIds[CalendarHeaderIndex];
			return true;
		}
	}
	if (ShowWeekNames && PtInRectF(layout.WeekRect, localX, localY)
		&& layout.CellWidth > 0.0f)
	{
		const int column = std::clamp(
			static_cast<int>((localX - layout.WeekRect.left)
				/ layout.CellWidth), 0, 6);
		result = _accessibilityIds[
			CalendarColumnHeaderIndex + static_cast<size_t>(column)];
		return true;
	}
	SYSTEMTIME date{};
	bool inDisplayMonth = true;
	const int cell = HitTestDate(
		static_cast<int>(localX), static_cast<int>(localY),
		date, inDisplayMonth);
	if (cell < 0) return false;
	result = _accessibilityIds[
		CalendarDayIndex + static_cast<size_t>(cell)];
	return true;
}

AccessibilityVirtualContainerInfo
CalendarView::GetAccessibilityVirtualContainerInfo() const noexcept
{
	AccessibilityVirtualContainerInfo result;
	result.Patterns = AutomationPattern::Selection
		| AutomationPattern::Grid
		| AutomationPattern::Table;
	result.CanSelectMultiple =
		_selectionMode == CalendarSelectionMode::SingleRange;
	result.IsSelectionRequired =
		_selectionMode != CalendarSelectionMode::None;
	result.RowCount = 6;
	result.ColumnCount = 7;
	return result;
}

void CalendarView::GetAccessibilityVirtualSelection(
	std::vector<uint32_t>& result) const
{
	result.clear();
	if (_selectionMode == CalendarSelectionMode::None) return;
	for (size_t cell = 0; cell < CalendarDayCount; ++cell)
	{
		bool inDisplayMonth = true;
		const auto date = CellDate(
			DisplayYear, DisplayMonth, static_cast<int>(cell),
			inDisplayMonth);
		if (!inDisplayMonth && !ShowTrailingDays) continue;
		const bool selected =
			_selectionMode == CalendarSelectionMode::SingleDate
				? IsSameDate(date, _selectedDate)
				: HasRangeStart && HasRangeEnd
					&& DateInClosedRange(date, RangeStart, RangeEnd);
		if (selected)
			result.push_back(
				_accessibilityIds[CalendarDayIndex + cell]);
	}
}

bool CalendarView::GetAccessibilityVirtualItemAt(
	int row, int column, uint32_t& result) const noexcept
{
	result = 0;
	if (row < 0 || row >= 6 || column < 0 || column >= 7)
		return false;
	const size_t cell = static_cast<size_t>(row * 7 + column);
	bool inDisplayMonth = true;
	(void)CellDate(
		DisplayYear, DisplayMonth, static_cast<int>(cell),
		inDisplayMonth);
	if (!inDisplayMonth && !ShowTrailingDays) return false;
	result = _accessibilityIds[CalendarDayIndex + cell];
	return result != 0;
}

void CalendarView::GetAccessibilityVirtualColumnHeaders(
	std::vector<uint32_t>& result) const
{
	result.assign(
		_accessibilityIds.begin() + CalendarColumnHeaderIndex,
		_accessibilityIds.begin() + CalendarDayIndex);
}

bool CalendarView::InvokeAccessibilityVirtualNode(uint32_t id)
{
	if (!IsEffectivelyEnabled()) return false;
	if (id == _accessibilityIds[CalendarPreviousIndex])
	{
		AddMonths(-1);
		return true;
	}
	if (id == _accessibilityIds[CalendarNextIndex])
	{
		AddMonths(1);
		return true;
	}
	const auto found = std::find(
		_accessibilityIds.begin() + CalendarDayIndex,
		_accessibilityIds.end(), id);
	if (found == _accessibilityIds.end()) return false;
	const int cell = static_cast<int>(
		std::distance(_accessibilityIds.begin() + CalendarDayIndex, found));
	bool inDisplayMonth = true;
	const auto date = CellDate(
		DisplayYear, DisplayMonth, cell, inDisplayMonth);
	if (!inDisplayMonth && !ShowTrailingDays) return false;
	SelectDateFromInput(date, inDisplayMonth);
	NotifyAccessibilityVirtualChanged(id, AccessibilityChange::Invoke);
	return true;
}

bool CalendarView::SelectAccessibilityVirtualNode(
	uint32_t id, AccessibilitySelectionAction action)
{
	if (action == AccessibilitySelectionAction::Remove
		|| !IsEffectivelyEnabled()
		|| _selectionMode == CalendarSelectionMode::None)
		return false;
	const auto found = std::find(
		_accessibilityIds.begin() + CalendarDayIndex,
		_accessibilityIds.end(), id);
	if (found == _accessibilityIds.end()) return false;
	const int cell = static_cast<int>(
		std::distance(_accessibilityIds.begin() + CalendarDayIndex, found));
	bool inDisplayMonth = true;
	const auto date = CellDate(
		DisplayYear, DisplayMonth, cell, inDisplayMonth);
	if (!inDisplayMonth && !ShowTrailingDays) return false;
	SelectDateFromInput(
		date, inDisplayMonth,
		action == AccessibilitySelectionAction::Add);
	NotifyAccessibilityVirtualChanged(id, AccessibilityChange::Selection);
	return true;
}

void CalendarView::DrawHeader(D2DGraphics* d2d, const Layout& layout)
{
	if (!ShowHeader || RectHeight(layout.HeaderRect) <= 0.0f)
		return;
	if (HoverDay == -2)
		d2d->FillRoundRect(layout.PrevRect, HoverColor, 5.0f);
	if (HoverDay == -3)
		d2d->FillRoundRect(layout.NextRect, HoverColor, 5.0f);
	DrawMonthArrow(d2d, layout.PrevRect, false, RendererForegroundColor);
	DrawMonthArrow(d2d, layout.NextRect, true, RendererForegroundColor);
	std::wstring month = std::format(L"{:04d}-{:02d}", DisplayYear, DisplayMonth);
	d2d->DrawStringCentered(month, layout.HeaderRect.left + RectWidth(layout.HeaderRect) * 0.5f,
		layout.HeaderRect.top + RectHeight(layout.HeaderRect) * 0.5f, RendererForegroundColor, GetRenderFont());
}

void CalendarView::DrawCalendarGrid(D2DGraphics* d2d, const Layout& layout)
{
	if (ShowWeekNames && RectHeight(layout.WeekRect) > 0.0f)
		DrawWeekNames(d2d, GetRenderFont(), layout.WeekRect, layout.CellWidth, MutedTextColor);

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
		if (_selectionMode == CalendarSelectionMode::SingleDate)
			selected = IsSameDate(date, _selectedDate);
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
			d2d->FillRoundRect(pill, AccentColor, 7.0f);
		}
		else if (hover)
		{
			d2d->FillRoundRect(pill, HoverColor, 7.0f);
		}
		else if (isToday)
		{
			d2d->DrawRoundRect(pill, AccentColor, 1.0f, 7.0f);
		}

		D2D1_COLOR_F textColor = inMonth ? RendererForegroundColor : TrailingTextColor;
		if (selected)
			textColor = SelectedForeColor;
		d2d->DrawStringCentered(std::to_wstring(date.wDay), rect.left + RectWidth(rect) * 0.5f,
			rect.top + RectHeight(rect) * 0.5f, textColor, GetRenderFont());
		if (IsKeyboardFocused && IsSameDate(date, _currentDate))
			d2d->DrawRoundRect(
				pill.left + 1.0f, pill.top + 1.0f,
				(std::max)(0.0f, RectWidth(pill) - 2.0f),
				(std::max)(0.0f, RectHeight(pill) - 2.0f),
				selected ? SelectedForeColor : AccentColor, 1.2f, 6.0f);
	}
}

void CalendarView::NotifySelectedDatesChanged()
{
	SelectionChangedEventArgs args;
	SelectedDatesChanged(this, args);
	for (size_t cell = 0; cell < CalendarDayCount; ++cell)
		NotifyAccessibilityVirtualChanged(
			_accessibilityIds[CalendarDayIndex + cell],
			AccessibilityChange::Selection);
}

void CalendarView::SelectDateFromInput(
	const SYSTEMTIME& date, bool inDisplayMonth, bool extendRange)
{
	const auto previousCurrent = _currentDate;
	_currentDate = date;
	if (!inDisplayMonth)
		SyncDisplayFromDate(date);
	if (_selectionMode == CalendarSelectionMode::None)
	{
		InvalidateVisual();
		return;
	}
	if (_selectionMode == CalendarSelectionMode::SingleDate)
	{
		SetSelectedDate(date);
		return;
	}

	if (!extendRange)
	{
		_rangeAnchor = date;
		_rangeAnchorSet = true;
		SetRange(date, date);
		_rangeAnchor = date;
		_rangeAnchorSet = true;
		return;
	}

	if (!_rangeAnchorSet)
	{
		_rangeAnchor = previousCurrent.wYear != 0
			? previousCurrent
			: HasRangeStart ? RangeStart : date;
		_rangeAnchorSet = true;
	}
	SYSTEMTIME a = _rangeAnchor;
	SYSTEMTIME b = date;
	if (CompareDate(a, b) > 0)
		std::swap(a, b);
	RangeStart = a;
	RangeEnd = b;
	HasRangeStart = true;
	HasRangeEnd = true;
	NotifySelectedDatesChanged();
	InvalidateVisual();
}

void CalendarView::MoveSelectedDate(int days, bool extendRange)
{
	auto current = _currentDate;
	if (current.wYear == 0)
		current = _selectionMode == CalendarSelectionMode::SingleDate
			? _selectedDate
			: HasRangeEnd ? RangeEnd
			: HasRangeStart ? RangeStart : TodayDate();
	const auto next = AddDays(current, days);
	SelectDateFromInput(
		next, next.wYear == DisplayYear && next.wMonth == DisplayMonth,
		extendRange);
	SyncDisplayFromDate(next);
}

void CalendarView::MoveSelectedDateByMonths(
	int months, bool extendRange)
{
	auto current = _currentDate.wYear != 0
		? _currentDate : TodayDate();
	const auto next = AddMonthsToDate(current, months);
	SelectDateFromInput(
		next, next.wYear == DisplayYear && next.wMonth == DisplayMonth,
		extendRange);
	SyncDisplayFromDate(next);
}

void CalendarView::OnRender()
{
	if (!this->IsVisible) return;
	auto d2d = this->GetDrawingContext();
	if (!d2d) return;
	const auto size = this->GetActualSizeDip();
	const float width = size.width;
	const float height = size.height;
	auto layout = CalcLayout();

	this->BeginRender();
	{
		D2D1_COLOR_F surface = this->RendererBackgroundColor.a > 0.0f ? this->RendererBackgroundColor : this->SurfaceColor;
		d2d->FillRoundRect(
			0.0f, 0.0f, width, height, surface, CornerRadius);

		DrawHeader(d2d, layout);
		DrawCalendarGrid(d2d, layout);
	}
	this->EndRender();
}

bool CalendarView::ProcessInput(const InputReport& input)
{
	if (!this->IsEnabled || !this->IsVisible) return true;
	switch (input.Kind)
	{
	case InputReportKind::MouseWheel:
		AddMonths(input.WheelDelta > 0 ? -1 : 1);
		{
			auto args = input.CreateMouseEventArgs();
			OnMouseWheel(this, args);
		}
		return true;
	case InputReportKind::PointerMove:
	{
		auto layout = CalcLayout();
		int newHover = -1;
		bool newHoverInMonth = true;
		if (ShowHeader && PtInRectF(layout.PrevRect, (float)input.X, (float)input.Y))
			newHover = -2;
		else if (ShowHeader && PtInRectF(layout.NextRect, (float)input.X, (float)input.Y))
			newHover = -3;
		else
		{
			SYSTEMTIME date{};
			if (HitTestDate(input.X, input.Y, date, newHoverInMonth) >= 0)
				newHover = (int)date.wDay;
		}
		if (newHover != HoverDay || newHoverInMonth != HoverDayInMonth)
		{
			HoverDay = newHover;
			HoverDayInMonth = newHoverInMonth;
			InvalidateVisual();
		}
		auto args = input.CreateMouseEventArgs();
		OnMouseMove(this, args);
		return true;
	}
	case InputReportKind::PointerDown:
		if (input.ChangedButton != MouseButton::Left)
			return Control::ProcessInput(input);
		_pointerPressActive = true;
		(void)CaptureMouse();
		if (GetPresentationWindow()) GetPresentationWindow()->SetKeyboardFocus(this, false);
		{
			auto args = input.CreateMouseEventArgs();
			OnMouseDown(this, args);
		}
		return true;
	case InputReportKind::PointerUp:
	{
		if (input.ChangedButton != MouseButton::Left)
			return Control::ProcessInput(input);
		const bool activate = _pointerPressActive;
		_pointerPressActive = false;
		if (IsMouseCaptured()) (void)ReleaseMouseCapture();
		if (activate)
		{
			auto layout = CalcLayout();
			if (ShowHeader && PtInRectF(layout.PrevRect, (float)input.X, (float)input.Y))
				AddMonths(-1);
			else if (ShowHeader && PtInRectF(layout.NextRect, (float)input.X, (float)input.Y))
				AddMonths(1);
			else
			{
				SYSTEMTIME date{};
				bool inMonth = true;
				if (HitTestDate(input.X, input.Y, date, inMonth) >= 0)
					SelectDateFromInput(
						date, inMonth,
						input.HasModifier(ModifierKeys::Shift));
			}
		}
		auto e = input.CreateMouseEventArgs();
		OnMouseUp(this, e);
		return true;
	}
	case InputReportKind::Cancel:
	case InputReportKind::CaptureLost:
		_pointerPressActive = false;
		if (input.Kind == InputReportKind::Cancel && IsMouseCaptured())
			(void)ReleaseMouseCapture();
		return Control::ProcessInput(input);
	case InputReportKind::KeyDown:
	{
		const bool extendRange =
			input.HasModifier(ModifierKeys::Shift);
		switch (input.Key)
		{
		case Key::Left: MoveSelectedDate(-1, extendRange); break;
		case Key::Right: MoveSelectedDate(1, extendRange); break;
		case Key::Up: MoveSelectedDate(-7, extendRange); break;
		case Key::Down: MoveSelectedDate(7, extendRange); break;
		case Key::PageUp:
			MoveSelectedDateByMonths(
				input.HasModifier(ModifierKeys::Control) ? -12 : -1,
				extendRange);
			break;
		case Key::PageDown:
			MoveSelectedDateByMonths(
				input.HasModifier(ModifierKeys::Control) ? 12 : 1,
				extendRange);
			break;
		case Key::Home:
			SelectDateFromInput(
				MakeDate(DisplayYear, DisplayMonth, 1), true,
				extendRange);
			break;
		case Key::End:
			SelectDateFromInput(
				MakeDate(DisplayYear, DisplayMonth,
					DaysInMonth(DisplayYear, DisplayMonth)),
				true, extendRange);
			break;
		case Key::Return:
		case Key::Space:
			SelectDateFromInput(
				_currentDate.wYear != 0 ? _currentDate : TodayDate(),
				true, extendRange);
			break;
		default:
			return Control::ProcessInput(input);
		}
		{
			auto args = input.CreateKeyEventArgs();
			OnKeyDown(this, args);
		}
		return true;
	}
	case InputReportKind::KeyUp:
		{
			auto args = input.CreateKeyEventArgs();
			OnKeyUp(this, args);
		}
		return true;
	default:
		break;
	}
	return Control::ProcessInput(input);
}
