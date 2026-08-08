#pragma once

#include "Calendar.h"

#include <memory>
#include <string>
#include <utility>

class ButtonBase;
class Popup;
class TextBox;

/** WPF DatePicker text presentation mode. */
enum class DatePickerFormat
{
	Long,
	Short
};

class DatePicker;

class DatePickerDateValidationErrorEventArgs final : public EventArgs
{
public:
	explicit DatePickerDateValidationErrorEventArgs(std::wstring text)
		: Text(std::move(text)) {}

	std::wstring Text;
	bool ThrowException = false;
};

using DatePickerEvent = Event<void(DatePicker*)>;
using DatePickerSelectedDateChangedEvent =
	Event<void(DatePicker*, SelectionChangedEventArgs&)>;
using DatePickerDateValidationErrorEvent =
	Event<void(DatePicker*, DatePickerDateValidationErrorEventArgs&)>;

/**
 * WPF-style date entry composed by its ControlTemplate.
 *
 * Required template parts are PART_TextBox, PART_Button and PART_Popup. The
 * control owns one Calendar instance and transfers it into PART_Popup while a
 * template is active, matching WPF's DatePicker ownership model.
 */
class DatePicker : public Control
{
protected:
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override;
	void OnControlTemplatePresentationChanged() override;
	void OnPresentationWindowChanged(
		Window* previousWindow, Window* currentWindow) override;
	bool ProcessInput(const InputReport& input) override;

public:
	DatePicker();
	~DatePicker() override;
	UIClass Type() override { return UIClass::UI_DatePicker; }

	static const DependencyProperty& SelectedDateProperty();
	static const DependencyProperty& DisplayDateProperty();
	static const DependencyProperty& FirstDayOfWeekProperty();
	static const DependencyProperty& IsTodayHighlightedProperty();
	static const DependencyProperty& IsDropDownOpenProperty();
	static const DependencyProperty& SelectedDateFormatProperty();
	static const DependencyProperty& TextProperty();
	static void RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	void EnsureBindingPropertiesRegistered() override
	{
		RegisterDependencyProperties();
	}
#endif

	SYSTEMTIME GetSelectedDate() const noexcept { return _selectedDate; }
	bool HasSelectedDate() const noexcept { return _selectedDate.wYear != 0; }
	void SetSelectedDate(const SYSTEMTIME& value);
	void ClearSelectedDate();

	SYSTEMTIME GetDisplayDate() const noexcept { return _displayDate; }
	void SetDisplayDate(const SYSTEMTIME& value);
	int GetFirstDayOfWeek() const noexcept { return _firstDayOfWeek; }
	void SetFirstDayOfWeek(int value);
	bool GetIsTodayHighlighted() const noexcept
	{
		return _isTodayHighlighted;
	}
	void SetIsTodayHighlighted(bool value);
	bool GetIsDropDownOpen() const noexcept { return _isDropDownOpen; }
	void SetIsDropDownOpen(bool value);
	DatePickerFormat GetSelectedDateFormat() const noexcept
	{
		return _selectedDateFormat;
	}
	void SetSelectedDateFormat(DatePickerFormat value);
	const std::wstring& GetText() const noexcept { return _text; }
	void SetText(const std::wstring& value);
	std::wstring GetSemanticText() const override { return _text; }

	/** Parses the current editor text and commits it through SelectedDate. */
	bool CommitText();
	Calendar* GetCalendar() const noexcept { return _calendar; }

	DatePickerEvent CalendarOpened;
	DatePickerEvent CalendarClosed;
	DatePickerSelectedDateChangedEvent SelectedDateChanged;
	DatePickerDateValidationErrorEvent DateValidationError;

private:
	SYSTEMTIME _selectedDate{};
	SYSTEMTIME _displayDate{};
	SYSTEMTIME _selectionBeforeOpen{};
	int _firstDayOfWeek = 0;
	bool _isTodayHighlighted = true;
	bool _isDropDownOpen = false;
	DatePickerFormat _selectedDateFormat = DatePickerFormat::Long;
	std::wstring _text;

	std::unique_ptr<Calendar> _calendarStorage;
	Calendar* _calendar = nullptr;
	Popup* _popup = nullptr;
	ButtonBase* _dropDownButton = nullptr;
	TextBox* _textBox = nullptr;

	EventConnection _popupOpened;
	EventConnection _popupClosed;
	EventConnection _buttonClick;
	EventConnection _textChanged;
	EventConnection _textLostFocus;
	EventConnection _textKeyDown;
	EventConnection _calendarSelectedDatesChanged;
	EventConnection _calendarDisplayDateChanged;
	EventConnection _calendarMouseUp;
	EventConnection _calendarKeyDown;

	bool _updatingTextBox = false;
	bool _updatingCalendar = false;
	bool _calendarSelectionCommitted = false;

	void ApplySelectedDateChange(
		const SYSTEMTIME& oldValue, const SYSTEMTIME& newValue);
	void ApplyDisplayDateChange(const SYSTEMTIME& value);
	void ApplyFirstDayOfWeekChange(int value);
	void ApplyIsTodayHighlightedChange(bool value);
	void ApplyIsDropDownOpenChange(bool oldValue, bool newValue);
	void ApplySelectedDateFormatChange();
	void ApplyTextChange();

	void ConfigureTemplateParts();
	void DisconnectTemplateParts();
	void AttachCalendarToPopup();
	void DetachCalendarFromPopup();
	void ConfigurePopup(Popup* popup);
	void SyncCalendar();
	void SyncTextBox();
	void SetCurrentIsDropDownOpen(bool value);
	void SetCurrentSelectedDate(const SYSTEMTIME& value);
	void SetCurrentDisplayDate(const SYSTEMTIME& value);
	void SetCurrentText(const std::wstring& value);
	void ToggleDropDown();
	void RestoreSelectionBeforeOpen();
};
