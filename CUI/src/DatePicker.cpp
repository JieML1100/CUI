#define NOMINMAX
#include "DatePicker.h"

#include "ButtonBase.h"
#include "DependencyPropertyInfrastructure.h"
#include "EventInfrastructure.h"
#include "Popup.h"
#include "TemplateInfrastructure.h"
#include "TextBox.h"
#include "TreeInfrastructure.h"
#include "Window.h"

#include <OleAuto.h>

#include <algorithm>
#include <cwctype>
#include <optional>
#include <stdexcept>
#include <utility>

#pragma comment(lib, "OleAut32.lib")

namespace
{
	class BooleanFlagScope final
	{
	public:
		explicit BooleanFlagScope(bool& flag) noexcept
			: _flag(flag), _previous(flag)
		{
			_flag = true;
		}

		~BooleanFlagScope() { _flag = _previous; }

		BooleanFlagScope(const BooleanFlagScope&) = delete;
		BooleanFlagScope& operator=(const BooleanFlagScope&) = delete;

	private:
		bool& _flag;
		bool _previous;
	};

	bool IsEmptyDate(const SYSTEMTIME& value) noexcept
	{
		return value.wYear == 0;
	}

	int DaysInMonth(int year, int month) noexcept
	{
		static constexpr int days[] =
			{ 31,28,31,30,31,30,31,31,30,31,30,31 };
		if (month < 1 || month > 12) return 0;
		int result = days[month - 1];
		const bool leap = (year % 4 == 0 && year % 100 != 0)
			|| year % 400 == 0;
		if (month == 2 && leap) result = 29;
		return result;
	}

	bool IsValidDate(const SYSTEMTIME& value) noexcept
	{
		if (IsEmptyDate(value)) return true;
		return value.wYear >= 1 && value.wYear <= 9999
			&& value.wMonth >= 1 && value.wMonth <= 12
			&& value.wDay >= 1
			&& value.wDay <= DaysInMonth(value.wYear, value.wMonth);
	}

	SYSTEMTIME NormalizeDate(const SYSTEMTIME& value) noexcept
	{
		if (IsEmptyDate(value) || !IsValidDate(value)) return {};
		SYSTEMTIME result{};
		result.wYear = value.wYear;
		result.wMonth = value.wMonth;
		result.wDay = value.wDay;
		return result;
	}

	SYSTEMTIME TodayDate() noexcept
	{
		SYSTEMTIME result{};
		::GetLocalTime(&result);
		result.wHour = 0;
		result.wMinute = 0;
		result.wSecond = 0;
		result.wMilliseconds = 0;
		return result;
	}

	bool EqualDate(const SYSTEMTIME& left, const SYSTEMTIME& right) noexcept
	{
		return left.wYear == right.wYear
			&& left.wMonth == right.wMonth
			&& left.wDay == right.wDay;
	}

	int CurrentFirstDayOfWeek() noexcept
	{
		wchar_t value[8]{};
		if (::GetLocaleInfoEx(
			LOCALE_NAME_USER_DEFAULT, LOCALE_IFIRSTDAYOFWEEK,
			value, static_cast<int>(std::size(value))) <= 0)
			return 0;
		// Windows uses Monday=0..Sunday=6; Calendar uses Sunday=0.
		const int windowsValue = value[0] >= L'0' && value[0] <= L'6'
			? value[0] - L'0' : 6;
		return (windowsValue + 1) % 7;
	}

	std::wstring Trim(std::wstring value)
	{
		const auto first = std::find_if_not(
			value.begin(), value.end(), [](wchar_t ch)
			{ return std::iswspace(static_cast<wint_t>(ch)) != 0; });
		const auto last = std::find_if_not(
			value.rbegin(), value.rend(), [](wchar_t ch)
			{ return std::iswspace(static_cast<wint_t>(ch)) != 0; }).base();
		if (first >= last) return {};
		return std::wstring(first, last);
	}

	std::wstring FormatDate(
		const SYSTEMTIME& value, DatePickerFormat format)
	{
		if (IsEmptyDate(value) || !IsValidDate(value)) return {};
		const DWORD flags = format == DatePickerFormat::Short
			? DATE_SHORTDATE : DATE_LONGDATE;
		const int length = ::GetDateFormatEx(
			LOCALE_NAME_USER_DEFAULT, flags, &value,
			nullptr, nullptr, 0, nullptr);
		if (length <= 1) return {};
		std::wstring result(static_cast<size_t>(length), L'\0');
		if (::GetDateFormatEx(
			LOCALE_NAME_USER_DEFAULT, flags, &value,
			nullptr, result.data(), length, nullptr) <= 0)
			return {};
		result.resize(static_cast<size_t>(length - 1));
		return result;
	}

	bool TryParseDate(const std::wstring& source, SYSTEMTIME& result)
	{
		const auto text = Trim(source);
		if (text.empty())
		{
			result = {};
			return true;
		}

		DATE value = 0.0;
		if (SUCCEEDED(::VarDateFromStr(
			const_cast<wchar_t*>(text.c_str()), LOCALE_USER_DEFAULT,
			VAR_DATEVALUEONLY, &value)))
		{
			SYSTEMTIME parsed{};
			if (::VariantTimeToSystemTime(value, &parsed)
				&& IsValidDate(parsed))
			{
				result = NormalizeDate(parsed);
				return true;
			}
		}

		// Keep an invariant ISO path even when the user locale parser rejects it.
		int year = 0;
		int month = 0;
		int day = 0;
		wchar_t tail = 0;
		const int parsed = swscanf_s(
			text.c_str(), L"%d-%d-%d%c",
			&year, &month, &day, &tail, 1u);
		SYSTEMTIME iso{};
		iso.wYear = static_cast<WORD>(year);
		iso.wMonth = static_cast<WORD>(month);
		iso.wDay = static_cast<WORD>(day);
		if (parsed == 3 && IsValidDate(iso) && !IsEmptyDate(iso))
		{
			result = iso;
			return true;
		}
		return false;
	}

	std::optional<SYSTEMTIME> ConvertDatePickerDateValue(
		const BindingValue& value)
	{
		SYSTEMTIME typed{};
		if (value.TryGet(typed))
			return IsValidDate(typed)
				? std::optional<SYSTEMTIME>(NormalizeDate(typed))
				: std::nullopt;
		std::wstring text;
		if (!value.TryGet(text)) return std::nullopt;
		SYSTEMTIME parsed{};
		return TryParseDate(text, parsed)
			? std::optional<SYSTEMTIME>(parsed) : std::nullopt;
	}

	template<typename TValue>
	DependencyPropertyOptions<DatePicker, TValue> DatePickerOptions(
		TValue defaultValue
		CUI_DESIGN_METADATA_ARGUMENTS(
			const wchar_t* category,
			int categoryOrder,
			int order,
			DependencyPropertyEditorKind editor),
		DependencyPropertyFlags flags = DependencyPropertyFlags::None)
	{
		DependencyPropertyOptions<DatePicker, TValue> options;
		options.DefaultValue = std::move(defaultValue);
		options.Flags = flags;
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = category;
		options.Design.CategoryOrder = categoryOrder;
		options.Design.Order = order;
		options.Design.Editor = editor;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		)
		return options;
	}

	using DependencyPropertyAccessor = const DependencyProperty& (*)();

	auto DatePickerSubscriber(DependencyPropertyAccessor propertyAccessor)
	{
		return [propertyAccessor](
			DatePicker& target,
			DependencyPropertyMetadata::ChangeHandler handler,
			DataSourceUpdateMode)
		{
			return target.OnPropertyValueChanged.Subscribe(
				[propertyAccessor, handler = std::move(handler)](
					DependencyObject*,
					const DependencyPropertyChangedEventArgs& args)
				{
					if (args.Property == &propertyAccessor()) handler();
				});
		};
	}

	class DatePickerAutomationPeer final : public AutomationPeer
	{
	public:
		explicit DatePickerAutomationPeer(Control& owner)
			: AutomationPeer(
				owner, AutomationControlType::Custom, L"DatePicker") {}

		AutomationPattern GetPatternSet() const noexcept override
		{
			return AutomationPattern::Value
				| AutomationPattern::ExpandCollapse;
		}

		std::wstring GetValue() const override
		{
			const auto* picker = dynamic_cast<const DatePicker*>(&Owner());
			return picker ? picker->GetText() : std::wstring{};
		}

		bool IsReadOnly() const override { return false; }

		AutomationOperationResult SetValue(
			const std::wstring& value) override
		{
			auto* picker = dynamic_cast<DatePicker*>(&Owner());
			if (!picker) return AutomationOperationResult::NotSupported;
			if (!picker->IsEffectivelyEnabled())
				return AutomationOperationResult::ElementNotEnabled;
			picker->SetText(value);
			return picker->CommitText()
				? AutomationOperationResult::Succeeded
				: AutomationOperationResult::InvalidArgument;
		}

		bool TryGetExpanded(bool& value) const override
		{
			const auto* picker = dynamic_cast<const DatePicker*>(&Owner());
			if (!picker) return false;
			value = picker->GetIsDropDownOpen();
			return true;
		}

		AutomationOperationResult SetExpanded(bool value) override
		{
			auto* picker = dynamic_cast<DatePicker*>(&Owner());
			if (!picker) return AutomationOperationResult::NotSupported;
			if (!picker->IsEffectivelyEnabled())
				return AutomationOperationResult::ElementNotEnabled;
			picker->SetIsDropDownOpen(value);
			return picker->GetIsDropDownOpen() == value
				? AutomationOperationResult::Succeeded
				: AutomationOperationResult::InvalidOperation;
		}
	};
}

const DependencyProperty& DatePicker::SelectedDateProperty()
{
	static const auto registration = []
	{
		auto options = DatePickerOptions(
			SYSTEMTIME{} CUI_DESIGN_METADATA_ARGUMENTS(
				L"Common", 0, 10, DependencyPropertyEditorKind::Auto),
			DependencyPropertyFlags::AffectsRender
				| DependencyPropertyFlags::BindsTwoWayByDefault);
		options.Equals = [](const SYSTEMTIME& left, const SYSTEMTIME& right)
		{ return EqualDate(left, right); };
		options.Convert = ConvertDatePickerDateValue;
		options.Validate = [](const SYSTEMTIME& value)
		{ return IsValidDate(value); };
		options.Changed = [](DatePicker& target,
			const SYSTEMTIME& oldValue, const SYSTEMTIME& newValue)
		{
			target.ApplySelectedDateChange(oldValue, newValue);
		};
		return DependencyPropertyRegistry::RegisterStatic<
			DatePicker, SYSTEMTIME>(
				DependencyPropertyRegistrationLiteral(L"SelectedDate"),
				[](DatePicker& target) { return target._selectedDate; },
				[](DatePicker& target, const SYSTEMTIME& value)
				{ target._selectedDate = NormalizeDate(value); },
				DatePickerSubscriber(&DatePicker::SelectedDateProperty),
				std::move(options));
	}();
	return *registration;
}

const DependencyProperty& DatePicker::DisplayDateProperty()
{
	static const auto registration = []
	{
		auto options = DatePickerOptions(
			TodayDate() CUI_DESIGN_METADATA_ARGUMENTS(
				L"Common", 0, 20, DependencyPropertyEditorKind::Auto),
			DependencyPropertyFlags::AffectsRender
				| DependencyPropertyFlags::BindsTwoWayByDefault);
		options.Equals = [](const SYSTEMTIME& left, const SYSTEMTIME& right)
		{ return EqualDate(left, right); };
		options.Convert = ConvertDatePickerDateValue;
		options.Validate = [](const SYSTEMTIME& value)
		{ return !IsEmptyDate(value) && IsValidDate(value); };
		options.Changed = [](DatePicker& target,
			const SYSTEMTIME&, const SYSTEMTIME& value)
		{ target.ApplyDisplayDateChange(value); };
		return DependencyPropertyRegistry::RegisterStatic<
			DatePicker, SYSTEMTIME>(
				DependencyPropertyRegistrationLiteral(L"DisplayDate"),
				[](DatePicker& target) { return target._displayDate; },
				[](DatePicker& target, const SYSTEMTIME& value)
				{ target._displayDate = NormalizeDate(value); },
				DatePickerSubscriber(&DatePicker::DisplayDateProperty),
				std::move(options));
	}();
	return *registration;
}

const DependencyProperty& DatePicker::FirstDayOfWeekProperty()
{
	static const auto registration = []
	{
		auto options = DatePickerOptions(
			CurrentFirstDayOfWeek() CUI_DESIGN_METADATA_ARGUMENTS(
				L"Behavior", 110, 10, DependencyPropertyEditorKind::Choice),
			DependencyPropertyFlags::AffectsRender);
		options.Validate = [](const int& value)
		{ return value >= 0 && value <= 6; };
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Choices = {
			{ L"Sunday", BindingValue(0) },
			{ L"Monday", BindingValue(1) },
			{ L"Tuesday", BindingValue(2) },
			{ L"Wednesday", BindingValue(3) },
			{ L"Thursday", BindingValue(4) },
			{ L"Friday", BindingValue(5) },
			{ L"Saturday", BindingValue(6) }
		};
		)
		options.Changed = [](DatePicker& target, const int&, const int& value)
		{ target.ApplyFirstDayOfWeekChange(value); };
		return DependencyPropertyRegistry::RegisterStatic<DatePicker, int>(
			DependencyPropertyRegistrationLiteral(L"FirstDayOfWeek"),
			[](DatePicker& target) { return target._firstDayOfWeek; },
			[](DatePicker& target, const int& value)
			{ target._firstDayOfWeek = value; }, {}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& DatePicker::IsTodayHighlightedProperty()
{
	static const auto registration = []
	{
		auto options = DatePickerOptions(
			true CUI_DESIGN_METADATA_ARGUMENTS(
				L"Appearance", 20, 10,
				DependencyPropertyEditorKind::Boolean),
			DependencyPropertyFlags::AffectsRender);
		options.Changed = [](DatePicker& target,
			const bool&, const bool& value)
		{ target.ApplyIsTodayHighlightedChange(value); };
		return DependencyPropertyRegistry::RegisterStatic<DatePicker, bool>(
			DependencyPropertyRegistrationLiteral(L"IsTodayHighlighted"),
			[](DatePicker& target) { return target._isTodayHighlighted; },
			[](DatePicker& target, const bool& value)
			{ target._isTodayHighlighted = value; }, {}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& DatePicker::IsDropDownOpenProperty()
{
	static const auto registration = []
	{
		auto options = DatePickerOptions(
			false CUI_DESIGN_METADATA_ARGUMENTS(
				L"Behavior", 110, 20,
				DependencyPropertyEditorKind::Boolean),
			DependencyPropertyFlags::AffectsRender
				| DependencyPropertyFlags::BindsTwoWayByDefault);
		options.Changed = [](DatePicker& target,
			const bool& oldValue, const bool& newValue)
		{ target.ApplyIsDropDownOpenChange(oldValue, newValue); };
		return DependencyPropertyRegistry::RegisterStatic<DatePicker, bool>(
			DependencyPropertyRegistrationLiteral(L"IsDropDownOpen"),
			[](DatePicker& target) { return target._isDropDownOpen; },
			[](DatePicker& target, const bool& value)
			{ target._isDropDownOpen = value; },
			DatePickerSubscriber(&DatePicker::IsDropDownOpenProperty),
			std::move(options));
	}();
	return *registration;
}

const DependencyProperty& DatePicker::SelectedDateFormatProperty()
{
	static const auto registration = []
	{
		auto options = DatePickerOptions(
			static_cast<int>(DatePickerFormat::Long)
			CUI_DESIGN_METADATA_ARGUMENTS(
				L"Appearance", 20, 20,
				DependencyPropertyEditorKind::Choice),
			DependencyPropertyFlags::AffectsRender);
		options.Validate = [](const int& value)
		{
			return value >= static_cast<int>(DatePickerFormat::Long)
				&& value <= static_cast<int>(DatePickerFormat::Short);
		};
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Choices = {
			{ L"Long", BindingValue(static_cast<int>(DatePickerFormat::Long)) },
			{ L"Short", BindingValue(static_cast<int>(DatePickerFormat::Short)) }
		};
		)
		options.Changed = [](DatePicker& target, const int&, const int&)
		{ target.ApplySelectedDateFormatChange(); };
		return DependencyPropertyRegistry::RegisterStatic<DatePicker, int>(
			DependencyPropertyRegistrationLiteral(L"SelectedDateFormat"),
			[](DatePicker& target)
			{ return static_cast<int>(target._selectedDateFormat); },
			[](DatePicker& target, const int& value)
			{
				target._selectedDateFormat =
					static_cast<DatePickerFormat>(value);
			}, {}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& DatePicker::TextProperty()
{
	static const auto registration = []
	{
		auto options = DatePickerOptions(
			std::wstring{} CUI_DESIGN_METADATA_ARGUMENTS(
				L"Common", 0, 30, DependencyPropertyEditorKind::Text),
			DependencyPropertyFlags::AffectsRender
				| DependencyPropertyFlags::BindsTwoWayByDefault);
		options.Changed = [](DatePicker& target,
			const std::wstring&, const std::wstring&)
		{ target.ApplyTextChange(); };
		return DependencyPropertyRegistry::RegisterStatic<
			DatePicker, std::wstring>(
				DependencyPropertyRegistrationLiteral(L"Text"),
				[](DatePicker& target) { return target._text; },
				[](DatePicker& target, const std::wstring& value)
				{ target._text = value; },
				DatePickerSubscriber(&DatePicker::TextProperty),
				std::move(options));
	}();
	return *registration;
}

void DatePicker::RegisterDependencyProperties()
{
	Control::RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	(void)SelectedDateProperty();
	(void)DisplayDateProperty();
	(void)FirstDayOfWeekProperty();
	(void)IsTodayHighlightedProperty();
	(void)IsDropDownOpenProperty();
	(void)SelectedDateFormatProperty();
	(void)TextProperty();
#endif
}

DatePicker::DatePicker()
	: _displayDate(TodayDate()),
	_firstDayOfWeek(CurrentFirstDayOfWeek()),
	_calendarStorage(std::make_unique<Calendar>()),
	_calendar(_calendarStorage.get())
{
	RegisterDependencyProperties();
	_calendar->SetSelectionMode(CalendarSelectionMode::SingleDate);
	_calendar->ClearSelectedDate();
	_calendarSelectedDatesChanged =
		_calendar->SelectedDatesChanged.Subscribe(
			[this](Control*, SelectionChangedEventArgs&)
			{
				if (_updatingCalendar || !_calendar) return;
				const auto selected = _calendar->GetSelectedDate();
				if (IsEmptyDate(selected)) return;
				_calendarSelectionCommitted = true;
				BooleanFlagScope guard(_updatingCalendar);
				SetCurrentSelectedDate(selected);
			});
	_calendarDisplayDateChanged = _calendar->DisplayDateChanged.Subscribe(
		[this](CalendarView*)
		{
			if (_updatingCalendar || !_calendar) return;
			BooleanFlagScope guard(_updatingCalendar);
			SetCurrentDisplayDate(_calendar->GetDisplayDate());
		});
	_calendarMouseUp = _calendar->OnMouseUp.Subscribe(
		[this](Control*, MouseEventArgs& args)
		{
			if (args.ChangedButton == MouseButton::Left
				&& _calendarSelectionCommitted && _isDropDownOpen)
				SetCurrentIsDropDownOpen(false);
			_calendarSelectionCommitted = false;
		});
	_calendarKeyDown = _calendar->OnKeyDown.Subscribe(
		[this](Control*, KeyEventArgs& args)
		{
			if (args.Key == Key::Escape && _isDropDownOpen)
			{
				RestoreSelectionBeforeOpen();
				SetCurrentIsDropDownOpen(false);
				args.Handled = true;
			}
			else if ((args.Key == Key::Return || args.Key == Key::Space)
				&& _isDropDownOpen)
			{
				SetCurrentIsDropDownOpen(false);
				args.Handled = true;
			}
			else
			{
				_calendarSelectionCommitted = false;
			}
		});
	SyncCalendar();
}

DatePicker::~DatePicker()
{
	DisconnectTemplateParts();
}

std::unique_ptr<AutomationPeer> DatePicker::OnCreateAutomationPeer()
{
	return std::make_unique<DatePickerAutomationPeer>(*this);
}

void DatePicker::SetSelectedDate(const SYSTEMTIME& value)
{
	if (!IsValidDate(value)) return;
	(void)TrySetPropertyValue(
		SelectedDateProperty(), BindingValue(NormalizeDate(value)));
}

void DatePicker::ClearSelectedDate()
{
	(void)TrySetPropertyValue(
		SelectedDateProperty(), BindingValue(SYSTEMTIME{}));
}

void DatePicker::SetDisplayDate(const SYSTEMTIME& value)
{
	if (IsEmptyDate(value) || !IsValidDate(value)) return;
	(void)TrySetPropertyValue(
		DisplayDateProperty(), BindingValue(NormalizeDate(value)));
}

void DatePicker::SetFirstDayOfWeek(int value)
{
	(void)TrySetPropertyValue(FirstDayOfWeekProperty(), BindingValue(value));
}

void DatePicker::SetIsTodayHighlighted(bool value)
{
	(void)TrySetPropertyValue(
		IsTodayHighlightedProperty(), BindingValue(value));
}

void DatePicker::SetIsDropDownOpen(bool value)
{
	(void)TrySetPropertyValue(IsDropDownOpenProperty(), BindingValue(value));
}

void DatePicker::SetSelectedDateFormat(DatePickerFormat value)
{
	(void)TrySetPropertyValue(
		SelectedDateFormatProperty(),
		BindingValue(static_cast<int>(value)));
}

void DatePicker::SetText(const std::wstring& value)
{
	(void)TrySetPropertyValue(TextProperty(), BindingValue(value));
}

void DatePicker::SetCurrentIsDropDownOpen(bool value)
{
	(void)TrySetCurrentPropertyValue(
		IsDropDownOpenProperty(), BindingValue(value));
}

void DatePicker::SetCurrentSelectedDate(const SYSTEMTIME& value)
{
	(void)TrySetCurrentPropertyValue(
		SelectedDateProperty(), BindingValue(NormalizeDate(value)));
}

void DatePicker::SetCurrentDisplayDate(const SYSTEMTIME& value)
{
	if (IsEmptyDate(value) || !IsValidDate(value)) return;
	(void)TrySetCurrentPropertyValue(
		DisplayDateProperty(), BindingValue(NormalizeDate(value)));
}

void DatePicker::SetCurrentText(const std::wstring& value)
{
	(void)TrySetCurrentPropertyValue(TextProperty(), BindingValue(value));
}

void DatePicker::ApplySelectedDateChange(
	const SYSTEMTIME&, const SYSTEMTIME& newValue)
{
	if (!IsEmptyDate(newValue)) SetCurrentDisplayDate(newValue);
	SyncCalendar();

	SetCurrentText(IsEmptyDate(newValue)
		? std::wstring{}
		: FormatDate(newValue, _selectedDateFormat));

	SelectionChangedEventArgs args;
	cui::framework::EventAccess::Raise(SelectedDateChanged, this, args);
	NotifyAccessibilityValueChanged();
}

void DatePicker::ApplyDisplayDateChange(const SYSTEMTIME&)
{
	SyncCalendar();
}

void DatePicker::ApplyFirstDayOfWeekChange(int)
{
	SyncCalendar();
}

void DatePicker::ApplyIsTodayHighlightedChange(bool)
{
	SyncCalendar();
}

void DatePicker::ApplyIsDropDownOpenChange(bool oldValue, bool newValue)
{
	if (oldValue == newValue) return;
	if (newValue && !CommitText())
	{
		SetCurrentIsDropDownOpen(false);
		return;
	}
	if (!_popup)
	{
		(void)ApplyTemplate();
		if (!_popup && newValue)
		{
			_lastTemplateError =
				L"DatePicker ControlTemplate 必须包含 PART_Popup。";
			SetCurrentIsDropDownOpen(false);
			return;
		}
	}
	if (_popup)
	{
		(void)_popup->TrySetCurrentPropertyValue(
			Popup::IsOpenProperty(), BindingValue(newValue));
		if (newValue) _popup->UpdatePlacement();
	}
	InvalidateVisual();
}

void DatePicker::ApplySelectedDateFormatChange()
{
	if (!HasSelectedDate()) return;
	SetCurrentText(FormatDate(_selectedDate, _selectedDateFormat));
}

void DatePicker::ApplyTextChange()
{
	SyncTextBox();
	NotifyAccessibilityValueChanged();
}

bool DatePicker::CommitText()
{
	const auto candidate = _textBox ? _textBox->GetText() : _text;
	SYSTEMTIME parsed{};
	if (!TryParseDate(candidate, parsed))
	{
		DatePickerDateValidationErrorEventArgs args(candidate);
		cui::framework::EventAccess::Raise(DateValidationError, this, args);
		SetCurrentText(HasSelectedDate()
			? FormatDate(_selectedDate, _selectedDateFormat)
			: std::wstring{});
		if (args.ThrowException)
			throw std::invalid_argument("DatePicker text is not a valid date");
		return false;
	}

	if (IsEmptyDate(parsed))
	{
		SetCurrentSelectedDate({});
		SetCurrentText({});
		return true;
	}

	SetCurrentSelectedDate(parsed);
	SetCurrentText(FormatDate(parsed, _selectedDateFormat));
	return true;
}

void DatePicker::SyncCalendar()
{
	if (!_calendar || _updatingCalendar) return;
	BooleanFlagScope guard(_updatingCalendar);
	_calendar->SetSelectionMode(CalendarSelectionMode::SingleDate);
	_calendar->SetFirstDayOfWeek(_firstDayOfWeek);
	_calendar->SetIsTodayHighlighted(_isTodayHighlighted);
	_calendar->SetDisplayDate(_displayDate);
	if (HasSelectedDate()) _calendar->SetSelectedDate(_selectedDate);
	else _calendar->ClearSelectedDate();
}

void DatePicker::SyncTextBox()
{
	if (!_textBox || _updatingTextBox) return;
	if (_textBox->GetText() == _text) return;
	BooleanFlagScope guard(_updatingTextBox);
	(void)_textBox->TrySetCurrentPropertyValue(
		TextBox::TextProperty(), BindingValue(_text));
}

void DatePicker::AttachCalendarToPopup()
{
	if (!_popup || !_calendar) return;
	if (_popup->GetChild() == _calendar) return;
	if (_popup->GetChild())
	{
		_lastTemplateError =
			L"DatePicker 的 PART_Popup.Child 由控件内部 Calendar 独占。";
		return;
	}
	if (!_calendarStorage) return;
	cui::framework::TreeAccess::SetTemplatedParent(*_calendar, this);
	(void)_popup->SetChild(std::move(_calendarStorage));
}

void DatePicker::DetachCalendarFromPopup()
{
	if (!_popup || !_calendar || _popup->GetChild() != _calendar) return;
	auto detached = _popup->DetachChild();
	_calendarStorage.reset(static_cast<Calendar*>(detached.release()));
	if (_calendarStorage)
		cui::framework::TreeAccess::SetTemplatedParent(
			*_calendarStorage, nullptr);
}

void DatePicker::ConfigurePopup(Popup* popup)
{
	_popup = popup;
	if (!_popup) return;
	(void)cui::framework::DependencyPropertyAccess::SetValue(
		*_popup, Popup::PlacementTargetProperty(),
		BindingValue(ControlWeakReference(this)),
		DependencyPropertyValueSource::Template);
	(void)cui::framework::DependencyPropertyAccess::SetValue(
		*_popup, Popup::PlacementProperty(),
		BindingValue(PlacementMode::Bottom),
		DependencyPropertyValueSource::Template);
	(void)cui::framework::DependencyPropertyAccess::SetValue(
		*_popup, Popup::StaysOpenProperty(), BindingValue(false),
		DependencyPropertyValueSource::Template);
	AttachCalendarToPopup();
	_popupOpened = _popup->Opened.Subscribe([this](Popup*)
	{
		_selectionBeforeOpen = _selectedDate;
		_calendarSelectionCommitted = false;
		if (!_isDropDownOpen) SetCurrentIsDropDownOpen(true);
		if (_calendar) (void)_calendar->Focus();
		cui::framework::EventAccess::Raise(CalendarOpened, this);
	});
	_popupClosed = _popup->Closed.Subscribe([this](Popup*)
	{
		_calendarSelectionCommitted = false;
		if (_isDropDownOpen) SetCurrentIsDropDownOpen(false);
		cui::framework::EventAccess::Raise(CalendarClosed, this);
	});
}

void DatePicker::DisconnectTemplateParts()
{
	_popupOpened.Disconnect();
	_popupClosed.Disconnect();
	_buttonClick.Disconnect();
	_textChanged.Disconnect();
	_textLostFocus.Disconnect();
	_textKeyDown.Disconnect();
	if (_popup)
	{
		(void)_popup->TrySetCurrentPropertyValue(
			Popup::IsOpenProperty(), BindingValue(false));
		DetachCalendarFromPopup();
	}
	_popup = nullptr;
	_dropDownButton = nullptr;
	_textBox = nullptr;
}

void DatePicker::ConfigureTemplateParts()
{
	_popup = dynamic_cast<Popup*>(FindDeclarativeTemplatePart(
		MakeTemplatePartToken(L"PART_Popup")));
	_dropDownButton = dynamic_cast<ButtonBase*>(FindDeclarativeTemplatePart(
		MakeTemplatePartToken(L"PART_Button")));
	_textBox = dynamic_cast<TextBox*>(FindDeclarativeTemplatePart(
		MakeTemplatePartToken(L"PART_TextBox")));

	if (!_popup || !_dropDownButton || !_textBox)
	{
		_lastTemplateError =
			L"DatePicker ControlTemplate 必须包含 PART_TextBox、PART_Button 和 PART_Popup。";
	}
	else
	{
		_lastTemplateError.clear();
	}
	ConfigurePopup(_popup);
	if (_dropDownButton)
		_buttonClick = _dropDownButton->Click.Subscribe(
			[this](Control*, RoutedEventArgs& args)
			{
				ToggleDropDown();
				args.Handled = true;
			});
	if (_textBox)
	{
		_textChanged = _textBox->OnTextChanged.Subscribe(
			[this](Control*, TextChangedEventArgs&)
			{
				if (_updatingTextBox || !_textBox) return;
				(void)TrySetCurrentPropertyValue(
					TextProperty(), BindingValue(_textBox->GetText()));
			});
		_textLostFocus = _textBox->OnLostFocus.Subscribe(
			[this](Control*) { (void)CommitText(); });
		_textKeyDown = _textBox->OnKeyDown.Subscribe(
			[this](Control*, KeyEventArgs& args)
			{
				const Key effectiveKey = args.Key == Key::System
					? args.SystemKey : args.Key;
				if (effectiveKey == Key::Down
					&& args.HasModifier(ModifierKeys::Alt))
				{
					ToggleDropDown();
					args.Handled = true;
				}
				else if (effectiveKey == Key::F4)
				{
					ToggleDropDown();
					args.Handled = true;
				}
				else if (effectiveKey == Key::Return)
				{
					(void)CommitText();
					args.Handled = true;
				}
				else if (effectiveKey == Key::Escape && _isDropDownOpen)
				{
					RestoreSelectionBeforeOpen();
					SetCurrentIsDropDownOpen(false);
					args.Handled = true;
				}
			});
	}
	SyncTextBox();
	SyncCalendar();
	if (!_text.empty() && !HasSelectedDate()) (void)CommitText();
	if (_popup)
		(void)_popup->TrySetCurrentPropertyValue(
			Popup::IsOpenProperty(), BindingValue(_isDropDownOpen));
}

void DatePicker::OnControlTemplatePresentationChanged()
{
	DisconnectTemplateParts();
	if (GetControlTemplateRoot()) ConfigureTemplateParts();
}

void DatePicker::OnPresentationWindowChanged(
	Window* previousWindow, Window* currentWindow)
{
	Control::OnPresentationWindowChanged(previousWindow, currentWindow);
	if (currentWindow && _isDropDownOpen)
	{
		(void)ApplyTemplate();
		if (_popup)
			(void)_popup->TrySetCurrentPropertyValue(
				Popup::IsOpenProperty(), BindingValue(true));
	}
}

void DatePicker::ToggleDropDown()
{
	if (_isDropDownOpen) SetCurrentIsDropDownOpen(false);
	else if (CommitText()) SetCurrentIsDropDownOpen(true);
}

void DatePicker::RestoreSelectionBeforeOpen()
{
	SetCurrentSelectedDate(_selectionBeforeOpen);
}

bool DatePicker::ProcessInput(const InputReport& input)
{
	if (input.Kind == InputReportKind::KeyDown)
	{
		const Key effectiveKey = input.Key == Key::System
			? input.SystemKey : input.Key;
		if ((effectiveKey == Key::Down
			&& input.HasModifier(ModifierKeys::Alt))
			|| effectiveKey == Key::F4)
		{
			ToggleDropDown();
			return true;
		}
		if (effectiveKey == Key::Escape && _isDropDownOpen)
		{
			RestoreSelectionBeforeOpen();
			SetCurrentIsDropDownOpen(false);
			return true;
		}
		if (effectiveKey == Key::Return)
		{
			(void)CommitText();
			return true;
		}
	}
	return Control::ProcessInput(input);
}
