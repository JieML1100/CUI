#pragma once
#include "Control.h"
#include <algorithm>

/**
 * @file DateTimePicker.h
 * @brief DateTimePicker：日期/时间选择控件（支持日期、时间或日期+时间）。
 */
enum class DateTimePickerMode : uint8_t
{
	DateOnly,
	TimeOnly,
	DateTime
};

class DateTimePicker : public Control
{
private:
	enum class HitPart : uint8_t
	{
		None,
		ToggleDate,
		ToggleTime,
		PrevMonth,
		NextMonth,
		DayCell,
		HourUp,
		HourDown,
		MinuteUp,
		MinuteDown
	};

	struct LayoutMetrics
	{
		float dropHeight = 0.0f;
		float contentLeft = 0.0f;
		float contentTop = 0.0f;
		float contentWidth = 0.0f;

		bool showToggleRow = false;
		D2D1_RECT_F toggleDateRect{};
		D2D1_RECT_F toggleTimeRect{};

		bool showDate = false;
		float monthHeaderTop = 0.0f;
		float monthHeaderHeight = 0.0f;
		D2D1_RECT_F prevRect{};
		D2D1_RECT_F nextRect{};
		float weekTop = 0.0f;
		float weekHeight = 0.0f;
		float gridTop = 0.0f;
		float cellWidth = 0.0f;
		float cellHeight = 0.0f;

		bool showTime = false;
		float timeTop = 0.0f;
		float timeHeight = 0.0f;
		D2D1_RECT_F hourRect{};
		D2D1_RECT_F minuteRect{};
		D2D1_RECT_F hourUpRect{};
		D2D1_RECT_F hourDownRect{};
		D2D1_RECT_F minuteUpRect{};
		D2D1_RECT_F minuteDownRect{};
	};

	SYSTEMTIME _value{};
	int _viewYear = 0;
	int _viewMonth = 0;
	bool _showDate = true;
	bool _showTime = true;
	bool _allowDate = true;
	bool _allowTime = true;
	HitPart _hoverPart = HitPart::None;
	int _hoverDay = -1;

	void EnsureShowFlags();
	void SyncViewFromValue();
	void UpdateDisplayText();
	static int DaysInMonth(int year, int month);
	static int FirstWeekday(int year, int month);
	void AdjustMonth(int delta);
	void AdjustHour(int delta);
	void AdjustMinute(int delta);
	void SetValueInternal(const SYSTEMTIME& value, bool fireEvent);
	bool GetLayoutMetrics(LayoutMetrics& out);
	bool HitTestDayCell(const LayoutMetrics& layout, int xof, int yof, int& outDay) const;
	void UpdateHoverState(int xof, int yof);
	HitPart HitTestPart(const LayoutMetrics& layout, int xof, int yof, int& outDay) const;
	void ToggleDateSection();
	void ToggleTimeSection();

public:
	/** @brief 控件边框宽度。 */
	float Boder = 1.5f;
	/** @brief 圆角半径。 */
	float Round = 6.0f;

	/** @brief 选择变化事件。 */
	SelectionChangedEvent OnSelectionChanged;
	/** @brief 背景色。 */
	D2D1_COLOR_F PanelBackColor = Colors::WhiteSmoke;
	/** @brief 下拉面板背景色。 */
	D2D1_COLOR_F DropBackColor = D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.98f };
	/** @brief 下拉面板边框色。 */
	D2D1_COLOR_F DropBorderColor = Colors::LightGray;
	/** @brief 悬停高亮色。 */
	D2D1_COLOR_F HoverColor = D2D1_COLOR_F{ 0.85f, 0.90f, 0.98f, 0.9f };
	/** @brief 选中高亮色。 */
	D2D1_COLOR_F AccentColor = D2D1_COLOR_F{ 0.20f, 0.55f, 0.95f, 0.95f };
	/** @brief 次要文本颜色。 */
	D2D1_COLOR_F SecondaryTextColor = Colors::DimGrey;
	/** @brief 焦点边框色。 */
	D2D1_COLOR_F FocusBorderColor = D2D1_COLOR_F{ 0.20f, 0.55f, 0.95f, 0.8f };

	/** @brief 是否允许在下拉中切换日期/时间。 */
	bool AllowModeSwitch = true;
	/** @brief 是否展开下拉面板。 */
	bool Expand = false;
	/** @brief 展开/收起下拉面板。 */
	void SetExpanded(bool value);

	DateTimePicker(std::wstring text = L"", int x = 0, int y = 0, int width = 200, int height = 28);
	virtual UIClass Type() override;

	PROPERTY(SYSTEMTIME, Value);
	GET(SYSTEMTIME, Value);
	SET(SYSTEMTIME, Value);

	PROPERTY(DateTimePickerMode, Mode);
	GET(DateTimePickerMode, Mode);
	SET(DateTimePickerMode, Mode);

	PROPERTY(bool, AllowDateSelection);
	GET(bool, AllowDateSelection);
	SET(bool, AllowDateSelection);

	PROPERTY(bool, AllowTimeSelection);
	GET(bool, AllowTimeSelection);
	SET(bool, AllowTimeSelection);

	SIZE ActualSize() override;
	CursorKind QueryCursor(int xof, int yof) override;
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof) override;
};

