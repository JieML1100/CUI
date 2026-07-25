#pragma once
#include "Control.h"

/**
 * @file CalendarView.h
 * @brief CalendarView：可模板化日期面板的原生行为宿主。
 */

enum class CalendarSelectionMode
{
	None,
	SingleDate,
	SingleRange
};

struct CalendarDateRange
{
	SYSTEMTIME Start{};
	SYSTEMTIME End{};
	bool HasStart = false;
	bool HasEnd = false;
};

typedef Event<void(class CalendarView*)> CalendarViewEvent;

class CalendarView : public Control
{
protected:
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override
	{
		return std::make_unique<AutomationPeer>(
			*this, AutomationControlType::Calendar, L"Calendar");
	}

private:
	CalendarSelectionMode _selectionMode = CalendarSelectionMode::SingleDate;
	SYSTEMTIME _selectedDate{};
	SYSTEMTIME RangeStart{};
	SYSTEMTIME RangeEnd{};
	bool HasRangeStart = false;
	bool HasRangeEnd = false;

	int DisplayYear = 0;
	int DisplayMonth = 0;
	int HoverDay = -1;
	bool HoverDayInMonth = true;
	bool _pointerPressActive = false;

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

public:
	using UIElement::SelectedDatesChanged;
	UIClass Type() override;
	static void RegisterDependencyProperties();
	void EnsureBindingPropertiesRegistered() override { RegisterDependencyProperties(); }
	CalendarView();

	CalendarSelectionMode GetSelectionMode() const noexcept
	{
		return _selectionMode;
	}
	void SetSelectionMode(CalendarSelectionMode value);
	SYSTEMTIME GetSelectedDate() const noexcept { return _selectedDate; }

	void SetSelectedDate(const SYSTEMTIME& date);
	void SetRange(const SYSTEMTIME& start, const SYSTEMTIME& end, bool fireEvent = true);
	void ClearRange(bool fireEvent = true);
	CalendarDateRange GetRange() const;
	void SetDisplayMonth(int year, int month);
	void AddMonths(int delta);
	int HitTestDate(int localX, int localY, SYSTEMTIME& outDate, bool& inDisplayMonth) const;

	CursorKind QueryCursor(int localX, int localY) override;
	bool HandlesMouseWheel() const override { return true; }
	bool HandlesNavigationKey(Key key) const override;
protected:
	void OnRender() override;
	bool ProcessInput(const InputReport& input) override;
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
	void NotifySelectedDatesChanged();
	void SelectDateFromInput(const SYSTEMTIME& date, bool inDisplayMonth);
	void MoveSelectedDate(int days);
	void SyncDisplayFromDate(const SYSTEMTIME& date);
};
