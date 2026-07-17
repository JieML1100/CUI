#pragma once
#include "Control.h"

/**
 * @file CalendarView.h
 * @brief CalendarView/DateRangePicker：日历面板与日期范围选择控件。
 */

enum class CalendarSelectionMode
{
	Single,
	Range
};

struct CalendarDateRange
{
	SYSTEMTIME Start{};
	SYSTEMTIME End{};
	bool HasStart = false;
	bool HasEnd = false;
};

typedef Event<void(class CalendarView*)> CalendarViewEvent;
typedef Event<void(class DateRangePicker*)> DateRangePickerEvent;

class CalendarView : public Control
{
public:
	UIClass Type() override;
	void EnsureBindingPropertiesRegistered() override;
	CalendarView(int x = 0, int y = 0, int width = 280, int height = 300);

	CalendarSelectionMode SelectionMode = CalendarSelectionMode::Single;
	SYSTEMTIME SelectedDate{};
	SYSTEMTIME RangeStart{};
	SYSTEMTIME RangeEnd{};
	bool HasRangeStart = false;
	bool HasRangeEnd = false;

	int DisplayYear = 0;
	int DisplayMonth = 0;
	int HoverDay = -1;
	bool HoverDayInMonth = true;

	bool ShowHeader = true;
	bool ShowWeekNames = true;
	bool ShowTrailingDays = true;
	bool HighlightToday = true;

	float Border = 1.0f;
	float CornerRadius = 8.0f;
	float HeaderHeight = 38.0f;
	float WeekHeaderHeight = 22.0f;
	float CellPadding = 3.0f;
	float NavButtonSize = 28.0f;

	D2D1_COLOR_F SurfaceColor = cui::theme::palette::Surface;
	D2D1_COLOR_F HeaderBackColor = cui::theme::palette::SurfaceMuted;
	D2D1_COLOR_F MutedTextColor = cui::theme::palette::TextMuted;
	D2D1_COLOR_F TrailingTextColor = cui::theme::palette::TextMuted;
	D2D1_COLOR_F HoverColor = cui::theme::palette::AccentSoft;
	D2D1_COLOR_F SelectedBackColor = cui::theme::palette::AccentSelected;
	D2D1_COLOR_F RangeBackColor = cui::theme::palette::AccentSoft;
	D2D1_COLOR_F AccentColor = cui::theme::palette::Accent;
	D2D1_COLOR_F SelectedForeColor = cui::theme::palette::TextPrimary;

	CalendarViewEvent OnSelectionChanged;
	SelectionChangedEvent SelectionChanged;

	void SetSelectedDate(const SYSTEMTIME& date, bool fireEvent = true);
	void SetRange(const SYSTEMTIME& start, const SYSTEMTIME& end, bool fireEvent = true);
	void ClearRange(bool fireEvent = true);
	CalendarDateRange GetRange() const;
	void SetDisplayMonth(int year, int month);
	void AddMonths(int delta);
	int HitTestDate(int localX, int localY, SYSTEMTIME& outDate, bool& inDisplayMonth) const;

	CursorKind QueryCursor(int localX, int localY) override;
	bool HandlesMouseWheel() const override { return true; }
	bool HandlesNavigationKey(WPARAM key) const override;
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY) override;

private:
	struct Layout
	{
		D2D1_RECT_F HeaderRect{ 0,0,0,0 };
		D2D1_RECT_F PrevRect{ 0,0,0,0 };
		D2D1_RECT_F NextRect{ 0,0,0,0 };
		D2D1_RECT_F WeekRect{ 0,0,0,0 };
		D2D1_RECT_F GridRect{ 0,0,0,0 };
		float CellWidth = 0.0f;
		float CellHeight = 0.0f;
	};

	bool _rangeAnchorSet = false;
	SYSTEMTIME _rangeAnchor{};

	Layout CalcLayout() const;
	void DrawHeader(D2DGraphics* d2d, const Layout& layout);
	void DrawCalendarGrid(D2DGraphics* d2d, const Layout& layout);
	void NotifySelectionChanged();
	void SelectDateFromInput(const SYSTEMTIME& date, bool inDisplayMonth);
	void MoveSelectedDate(int days);
	void SyncDisplayFromDate(const SYSTEMTIME& date);
};

class DateRangePicker : public Control
{
public:
	UIClass Type() override;
	DateRangePicker(std::wstring text = L"", int x = 0, int y = 0, int width = 240, int height = 30);

	SYSTEMTIME StartDate{};
	SYSTEMTIME EndDate{};
	bool HasStartDate = false;
	bool HasEndDate = false;
	bool Expand = false;
	bool AutoCloseOnComplete = true;
	bool ShowTrailingDays = true;

	std::wstring Placeholder = L"Select date range";
	std::wstring SeparatorText = L" - ";

	float Border = 1.5f;
	float Round = 6.0f;
	float DropCornerRadius = 8.0f;
	float DropGap = 4.0f;
	float CalendarWidth = 280.0f;
	float CalendarHeight = 290.0f;
	float FooterHeight = 36.0f;
	float ChevronSize = 10.0f;

	D2D1_COLOR_F PanelBackColor = cui::theme::palette::Surface;
	D2D1_COLOR_F DropBackColor = cui::theme::palette::Surface;
	D2D1_COLOR_F DropBorderColor = cui::theme::palette::Border;
	D2D1_COLOR_F HeaderHoverBackColor = cui::theme::palette::AccentSoft;
	D2D1_COLOR_F HoverColor = cui::theme::palette::AccentSoft;
	D2D1_COLOR_F SelectedBackColor = cui::theme::palette::AccentSelected;
	D2D1_COLOR_F RangeBackColor = cui::theme::palette::AccentSoft;
	D2D1_COLOR_F AccentColor = cui::theme::palette::Accent;
	D2D1_COLOR_F SelectedForeColor = cui::theme::palette::TextPrimary;
	D2D1_COLOR_F SecondaryTextColor = cui::theme::palette::TextMuted;
	D2D1_COLOR_F FocusBorderColor = cui::theme::palette::Accent;
	D2D1_COLOR_F ButtonBackColor = cui::theme::palette::Accent;
	D2D1_COLOR_F ButtonTextColor = cui::theme::palette::OnAccent;

	DateRangePickerEvent OnRangeChanged;
	SelectionChangedEvent SelectionChanged;

	void SetExpanded(bool value);
	void SetRange(const SYSTEMTIME& start, const SYSTEMTIME& end, bool fireEvent = true);
	void ClearRange(bool fireEvent = true);
	CalendarDateRange GetRange() const;

	bool AutoCloseOnOutsideClick() const override { return true; }
	bool AutoCloseOnFormFocusLoss() const override { return true; }
	void ClosePopup() override { SetExpanded(false); }
	bool IsAnimationRunning() override;
	UINT GetAnimationIntervalMs() override { return 16; }
	bool GetAnimatedInvalidRect(D2D1_RECT_F& outRect) override;
	bool ContainsForegroundPoint(int localX, int localY) override;
	bool RenderNormalWhenForeground() const override { return true; }
	void InvalidateVisual() override;
	SIZE ActualSize() override;
	CursorKind QueryCursor(int localX, int localY) override;
	bool HandlesMouseWheel() const override { return true; }
	void Update() override;
	void UpdateForeground() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY) override;

private:
	enum class HitPart
	{
		None,
		Header,
		PrevMonth,
		NextMonth,
		DayCell,
		Today,
		Clear
	};

	struct Layout
	{
		D2D1_RECT_F DropRect{ 0,0,0,0 };
		D2D1_RECT_F CalendarRect{ 0,0,0,0 };
		D2D1_RECT_F HeaderRect{ 0,0,0,0 };
		D2D1_RECT_F PrevRect{ 0,0,0,0 };
		D2D1_RECT_F NextRect{ 0,0,0,0 };
		D2D1_RECT_F WeekRect{ 0,0,0,0 };
		D2D1_RECT_F GridRect{ 0,0,0,0 };
		D2D1_RECT_F TodayRect{ 0,0,0,0 };
		D2D1_RECT_F ClearRect{ 0,0,0,0 };
		float CellWidth = 0.0f;
		float CellHeight = 0.0f;
		float DropHeight = 0.0f;
	};

	int _viewYear = 0;
	int _viewMonth = 0;
	HitPart _hoverPart = HitPart::None;
	int _hoverDay = -1;
	bool _hoverInMonth = true;
	bool _rangeAnchorSet = false;
	SYSTEMTIME _rangeAnchor{};
	float _dropProgress = 0.0f;
	float _animStartProgress = 0.0f;
	float _animTargetProgress = 0.0f;
	ULONGLONG _animStartTick = 0;
	UINT _animDurationMs = 180;
	bool _animating = false;
	bool _collapseCleanupPending = false;
	bool _renderingForeground = false;

	float DropdownTop() const;
	float CurrentDropProgress();
	bool IsDropDownVisible();
	Layout CalcLayout() const;
	void SyncViewFromRange();
	void UpdateDisplayText();
	void NotifyRangeChanged();
	void AddMonths(int delta);
	HitPart HitTestPart(const Layout& layout, int localX, int localY, SYSTEMTIME& outDate, bool& inDisplayMonth) const;
	void UpdateHoverState(int localX, int localY);
	void SelectDateFromInput(const SYSTEMTIME& date, bool inDisplayMonth);
	void DrawHeader(D2DGraphics* d2d, const Layout& layout);
	void DrawCalendarGrid(D2DGraphics* d2d, const Layout& layout);
};
