#pragma once
#include "Control.h"

#include <array>

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
		return std::make_unique<CalendarViewAutomationPeer>(*this);
	}

private:
	CalendarSelectionMode _selectionMode = CalendarSelectionMode::SingleDate;
	SYSTEMTIME _selectedDate{};
	SYSTEMTIME _currentDate{};
	SYSTEMTIME _displayDate{};
	SYSTEMTIME RangeStart{};
	SYSTEMTIME RangeEnd{};
	bool HasRangeStart = false;
	bool HasRangeEnd = false;

	int DisplayYear = 0;
	int DisplayMonth = 0;
	int HoverDay = -1;
	bool HoverDayInMonth = true;
	bool _pointerPressActive = false;
	std::array<uint32_t, 52> _accessibilityIds{};

	bool ShowHeader = true;
	bool ShowWeekNames = true;
	bool ShowTrailingDays = true;
	int _firstDayOfWeek = 0;
	bool _isTodayHighlighted = true;

	float CornerRadius = 4.0f;
	float HeaderHeight = 38.0f;
	float WeekHeaderHeight = 22.0f;
	float CellPadding = 3.0f;
	float NavButtonSize = 28.0f;

	D2D1_COLOR_F SurfaceColor = cui::theme::palette::Surface;
	D2D1_COLOR_F MutedTextColor = cui::theme::palette::TextMuted;
	D2D1_COLOR_F TrailingTextColor = cui::theme::palette::TextMuted;
	D2D1_COLOR_F HoverColor = cui::theme::palette::AccentSoft;
	D2D1_COLOR_F RangeBackColor = cui::theme::palette::AccentSoft;
	D2D1_COLOR_F AccentColor = cui::theme::palette::Accent;
	D2D1_COLOR_F SelectedForeColor = cui::theme::palette::OnAccent;

public:
	using UIElement::SelectedDatesChanged;
	UIClass Type() override;
	/** WPF dependency-property identities used by generated/native code. */
	static const DependencyProperty& SelectionModeProperty();
	static const DependencyProperty& SelectedDateProperty();
	static const DependencyProperty& DisplayDateProperty();
	static const DependencyProperty& FirstDayOfWeekProperty();
	static const DependencyProperty& IsTodayHighlightedProperty();
	static void RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	void EnsureBindingPropertiesRegistered() override { RegisterDependencyProperties(); }
#endif
	CalendarView();

	CalendarSelectionMode GetSelectionMode() const noexcept
	{
		return _selectionMode;
	}
	void SetSelectionMode(CalendarSelectionMode value);
	SYSTEMTIME GetSelectedDate() const noexcept { return _selectedDate; }
	bool HasSelectedDate() const noexcept { return _selectedDate.wYear != 0; }

	void SetSelectedDate(const SYSTEMTIME& date);
	void ClearSelectedDate();
	SYSTEMTIME GetDisplayDate() const noexcept { return _displayDate; }
	void SetDisplayDate(const SYSTEMTIME& date);
	int GetFirstDayOfWeek() const noexcept { return _firstDayOfWeek; }
	void SetFirstDayOfWeek(int value);
	bool GetIsTodayHighlighted() const noexcept
	{
		return _isTodayHighlighted;
	}
	void SetIsTodayHighlighted(bool value);
	void SetRange(const SYSTEMTIME& start, const SYSTEMTIME& end, bool fireEvent = true);
	void ClearRange(bool fireEvent = true);
	CalendarDateRange GetRange() const;
	void SetDisplayMonth(int year, int month);
	void AddMonths(int delta);
	int HitTestDate(int localX, int localY, SYSTEMTIME& outDate, bool& inDisplayMonth) const;

	CursorKind QueryCursor(int localX, int localY) override;
	bool HandlesMouseWheel() const override { return true; }
	// CalendarView paints and handles its cells as one input leaf. Theme
	// decoration (for example PART_OuterChrome) must not become the deepest
	// pointer target and bypass CalendarView::ProcessInput.
	bool HitTestChildren() const override { return false; }
	bool HandlesNavigationKey(Key key) const override;
	bool TryGetAccessibilityVirtualNode(
		uint32_t id, AccessibilityVirtualNode& result);
	size_t GetAccessibilityVirtualChildCount(uint32_t parentId) const noexcept;
	bool TryGetAccessibilityVirtualChildAt(
		uint32_t parentId, size_t index, uint32_t& result) const noexcept;
	bool TryGetAccessibilityVirtualSibling(
		uint32_t parentId, uint32_t id, bool next,
		uint32_t& result) const noexcept;
	bool TryHitTestAccessibilityVirtualNode(
		float localX, float localY, uint32_t& result) const;
	AccessibilityVirtualContainerInfo
		GetAccessibilityVirtualContainerInfo() const noexcept;
	void GetAccessibilityVirtualSelection(
		std::vector<uint32_t>& result) const;
	bool GetAccessibilityVirtualItemAt(
		int row, int column, uint32_t& result) const noexcept;
	void GetAccessibilityVirtualColumnHeaders(
		std::vector<uint32_t>& result) const;
	bool InvokeAccessibilityVirtualNode(uint32_t id);
	bool SelectAccessibilityVirtualNode(
		uint32_t id, AccessibilitySelectionAction action);
	CalendarViewEvent DisplayDateChanged;
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
	void SelectDateFromInput(
		const SYSTEMTIME& date, bool inDisplayMonth, bool extendRange = false);
	void MoveSelectedDate(int days, bool extendRange = false);
	void MoveSelectedDateByMonths(int months, bool extendRange = false);
	void SyncDisplayFromDate(const SYSTEMTIME& date);
};
