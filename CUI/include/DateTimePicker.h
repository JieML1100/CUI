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
		HourField,
		MinuteField,
		HourUp,
		HourDown,
		MinuteUp,
		MinuteDown
	};

	enum class EditField : uint8_t
	{
		None,
		Hour,
		Minute
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
		int gridRows = 6;
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
	EditField _editField = EditField::None;
	std::wstring _editBuffer;
	float _dropProgress = 0.0f;
	float _animStartProgress = 0.0f;
	float _animTargetProgress = 0.0f;
	ULONGLONG _animStartTick = 0;
	UINT _animDurationMs = 180;
	bool _animating = false;
	bool _collapseCleanupPending = false;
	bool _renderingForeground = false;

	void EnsureShowFlags();
	void SyncViewFromValue();
	void UpdateDisplayText();
	static int DaysInMonth(int year, int month);
	static int FirstWeekday(int year, int month);
	void AdjustMonth(int delta);
	void AdjustHour(int delta);
	void AdjustMinute(int delta);
	void SetValueInternal(const SYSTEMTIME& value, bool fireEvent);
	void BeginTimeEdit(EditField field);
	void CommitTimeEdit(bool keepEditing);
	void CancelTimeEdit();
	bool HandleTimeEditChar(wchar_t ch);
	std::wstring GetTimeEditText(EditField field, int value) const;
	bool IsInlineTimeMode() const;
	bool GetLayoutMetrics(LayoutMetrics& out);
	bool GetInlineTimeLayout(LayoutMetrics& out);
	float DropdownTop();
	float CurrentDropProgress();
	bool IsDropDownVisible();
	bool HitTestDayCell(const LayoutMetrics& layout, int localX, int localY, int& outDay) const;
	void UpdateHoverState(int localX, int localY);
	HitPart HitTestPart(const LayoutMetrics& layout, int localX, int localY, int& outDay) const;
	void ToggleDateSection();
	void ToggleTimeSection();

public:
	/** @brief 控件边框宽度。 */
	float BorderThickness = 1.5f;
	/** @brief 圆角半径。 */
	float Round = 6.0f;
	/** @brief 下拉面板圆角半径。 */
	float DropCornerRadius = 7.0f;
	/** @brief 输入框与下拉面板之间的间距。 */
	float DropGap = 4.0f;
	/** @brief 日期单元/切换标签内边距。 */
	float ItemPadding = 3.0f;
	/** @brief 右侧展开图标尺寸。 */
	float ChevronSize = 10.0f;

	/** @brief 选择变化事件。 */
	SelectionChangedEvent OnSelectionChanged;
	/** @brief 背景色。 */
	D2D1_COLOR_F PanelBackColor = cui::theme::palette::Surface;
	/** @brief 下拉面板背景色。 */
	D2D1_COLOR_F DropBackColor = cui::theme::palette::Surface;
	/** @brief 下拉面板边框色。 */
	D2D1_COLOR_F DropBorderColor = cui::theme::palette::Border;
	/** @brief 输入框悬停背景色。 */
	D2D1_COLOR_F HeaderHoverBackColor = cui::theme::palette::AccentSoft;
	/** @brief 悬停高亮色。 */
	D2D1_COLOR_F HoverColor = cui::theme::palette::AccentSoft;
	/** @brief 选中高亮色。 */
	D2D1_COLOR_F AccentColor = cui::theme::palette::Accent;
	/** @brief 柔和选中背景色。 */
	D2D1_COLOR_F SelectedBackColor = cui::theme::palette::AccentSelected;
	/** @brief 选中项文本颜色。 */
	D2D1_COLOR_F SelectedForeColor = cui::theme::palette::TextPrimary;
	/** @brief 次要文本颜色。 */
	D2D1_COLOR_F SecondaryTextColor = cui::theme::palette::TextMuted;
	/** @brief 焦点边框色。 */
	D2D1_COLOR_F FocusBorderColor = cui::theme::palette::Accent;

	/** @brief 是否允许在下拉中切换日期/时间。 */
	bool AllowModeSwitch = true;
	/** @brief 是否展开下拉面板。 */
	bool Expand = false;
	/** @brief 展开/收起下拉面板。 */
	void SetExpanded(bool value);

	DateTimePicker(std::wstring text = L"", int x = 0, int y = 0, int width = 200, int height = 28);
	virtual UIClass Type() override;
	void EnsureBindingPropertiesRegistered() override;
	bool AutoCloseOnOutsideClick() const override { return true; }
	bool AutoCloseOnFormFocusLoss() const override { return true; }
	void ClosePopup() override { SetExpanded(false); }
	bool IsAnimationRunning() override;
	UINT GetAnimationIntervalMs() override { return 16; }
	bool GetAnimatedInvalidRect(D2D1_RECT_F& outRect) override;
	bool ContainsForegroundPoint(int localX, int localY) override;
	bool RenderNormalWhenForeground() const override { return true; }
	void InvalidateVisual() override;

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
	CursorKind QueryCursor(int localX, int localY) override;
	bool HandlesMouseWheel() const override { return true; }
	void Update() override;
	void UpdateForeground() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY) override;
};
