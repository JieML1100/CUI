#define NOMINMAX
#include "NumericUpDown.h"
#include "Form.h"
#include "TextEditCore.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cwctype>
#include <iomanip>
#include <sstream>
#include <utility>

#pragma comment(lib, "Imm32.lib")

namespace
{
	template<typename TValue>
	ControlPropertyOptions<NumericUpDown, TValue> NumericPropertyOptions(
		TValue defaultValue,
		const wchar_t* category,
		int categoryOrder,
		int order,
		ControlPropertyEditorKind editor,
		ControlPropertyFlags flags = ControlPropertyFlags::AffectsRender)
	{
		ControlPropertyOptions<NumericUpDown, TValue> options;
		options.DefaultValue = std::move(defaultValue);
		options.Flags = flags | ControlPropertyFlags::TracksLocalValue;
		options.Design.Category = category;
		options.Design.CategoryOrder = categoryOrder;
		options.Design.Order = order;
		options.Design.Editor = editor;
		options.Design.Persistence = ControlPropertyPersistence::Metadata;
		return options;
	}

	auto NumericPropertySubscriber(const wchar_t* propertyName)
	{
		return [propertyName = std::wstring(propertyName)](
			NumericUpDown& target,
			BindingPropertyMetadata::ChangeHandler handler,
			DataSourceUpdateMode)
		{
			return target.OnPropertyValueChanged.Subscribe(
				[propertyName, handler = std::move(handler)](
					Control*, const ControlPropertyChangedEventArgs& args)
				{
					if (_wcsicmp(args.PropertyName.c_str(), propertyName.c_str()) == 0)
						handler();
				});
		};
	}

	bool NumericColorsEqual(
		const D2D1_COLOR_F& left,
		const D2D1_COLOR_F& right)
	{
		return left.r == right.r && left.g == right.g
			&& left.b == right.b && left.a == right.a;
	}

	ControlPropertyOptions<NumericUpDown, D2D1_COLOR_F> NumericColorOptions(
		D2D1_COLOR_F defaultValue,
		int order)
	{
		auto options = NumericPropertyOptions(
			defaultValue, L"Appearance", 200, order,
			ControlPropertyEditorKind::Color);
		options.Equals = NumericColorsEqual;
		return options;
	}

	ControlPropertyOptions<NumericUpDown, float> NumericMetricOptions(
		float defaultValue,
		int order)
	{
		auto options = NumericPropertyOptions(
			defaultValue, L"Appearance", 200, order,
			ControlPropertyEditorKind::Number);
		options.Coerce = [](
			NumericUpDown&, const float& proposed) -> std::optional<float>
		{
			return std::isfinite(proposed)
				? std::optional<float>{ (std::max)(0.0f, proposed) }
				: std::nullopt;
		};
		options.Design.Minimum = 0.0;
		options.Design.Step = 0.5;
		return options;
	}

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

	CuiTextEdit::EditOptions NumericEditOptions()
	{
		CuiTextEdit::EditOptions options;
		options.allowMultiLine = false;
		return options;
	}

	void CommitTextChange(Control* control, const std::wstring& oldText, const std::wstring& newText)
	{
		if (!control || oldText == newText)
			return;
		control->SetTextInternal(newText);
		control->TextChanged = true;
		control->OnTextChanged(control, oldText, newText);
	}

	bool IsNumericEditCandidate(const std::wstring& text)
	{
		if (text.empty())
			return true;

		size_t i = 0;
		if (text[i] == L'+' || text[i] == L'-')
			i++;

		bool hasDecimalPoint = false;
		for (; i < text.size(); i++)
		{
			const wchar_t ch = text[i];
			if (std::iswdigit(static_cast<wint_t>(ch)))
				continue;
			if (ch == L'.' && !hasDecimalPoint)
			{
				hasDecimalPoint = true;
				continue;
			}
			return false;
		}
		return true;
	}

	bool TryReadClipboardText(HWND owner, std::wstring& text)
	{
		text.clear();
		if (!OpenClipboard(owner))
			return false;

		bool success = false;
		if (IsClipboardFormatAvailable(CF_UNICODETEXT))
		{
			HANDLE hClip = GetClipboardData(CF_UNICODETEXT);
			const wchar_t* clipboardText = hClip ? static_cast<const wchar_t*>(GlobalLock(hClip)) : nullptr;
			if (clipboardText)
			{
				text = clipboardText;
				GlobalUnlock(hClip);
				success = true;
			}
		}
		else if (IsClipboardFormatAvailable(CF_TEXT))
		{
			HANDLE hClip = GetClipboardData(CF_TEXT);
			const char* clipboardText = hClip ? static_cast<const char*>(GlobalLock(hClip)) : nullptr;
			if (clipboardText)
			{
				const int byteLength = lstrlenA(clipboardText);
				const int textLength = MultiByteToWideChar(CP_ACP, 0, clipboardText, byteLength, nullptr, 0);
				if (textLength > 0)
				{
					text.resize(static_cast<size_t>(textLength));
					MultiByteToWideChar(CP_ACP, 0, clipboardText, byteLength, &text[0], textLength);
					success = true;
				}
				GlobalUnlock(hClip);
			}
		}

		CloseClipboard();
		return success;
	}

	bool WriteClipboardText(HWND owner, const std::wstring& text)
	{
		if (text.empty() || !OpenClipboard(owner))
			return false;

		bool success = false;
		if (EmptyClipboard())
		{
			const size_t bytes = (text.size() + 1) * sizeof(wchar_t);
			HGLOBAL hData = GlobalAlloc(GMEM_MOVEABLE, bytes);
			if (hData)
			{
				wchar_t* data = static_cast<wchar_t*>(GlobalLock(hData));
				if (data)
				{
					memcpy(data, text.c_str(), bytes);
					GlobalUnlock(hData);
					if (SetClipboardData(CF_UNICODETEXT, hData))
					{
						success = true;
						hData = nullptr;
					}
				}
				if (hData)
					GlobalFree(hData);
			}
		}

		CloseClipboard();
		return success;
	}

	D2D1_COLOR_F ScaleAlpha(D2D1_COLOR_F color, float scale)
	{
		color.a *= (std::clamp)(scale, 0.0f, 1.0f);
		return color;
	}

	void DrawSpinArrow(D2DGraphics* d2d, const D2D1_RECT_F& rect, bool up, D2D1_COLOR_F color, float stroke = 1.5f)
	{
		if (!d2d) return;
		const float cx = rect.left + RectWidth(rect) * 0.5f;
		const float cy = rect.top + RectHeight(rect) * 0.5f + (up ? -0.4f : 0.4f);
		const float halfW = (std::clamp)((std::min)(RectWidth(rect), RectHeight(rect)) * 0.18f, 2.6f, 4.0f);
		const float halfH = halfW * 0.78f;
		if (up)
		{
			d2d->DrawLine(D2D1::Point2F(cx - halfW, cy + halfH), D2D1::Point2F(cx, cy - halfH), color, stroke);
			d2d->DrawLine(D2D1::Point2F(cx, cy - halfH), D2D1::Point2F(cx + halfW, cy + halfH), color, stroke);
		}
		else
		{
			d2d->DrawLine(D2D1::Point2F(cx - halfW, cy - halfH), D2D1::Point2F(cx, cy + halfH), color, stroke);
			d2d->DrawLine(D2D1::Point2F(cx, cy + halfH), D2D1::Point2F(cx + halfW, cy - halfH), color, stroke);
		}
	}
}

UIClass NumericUpDown::Type()
{
	return UIClass::UI_NumericUpDown;
}

void NumericUpDown::EnsureBindingPropertiesRegistered()
{
	Control::EnsureBindingPropertiesRegistered();
	static const bool registered = []
	{
		auto minimumOptions = NumericPropertyOptions(
			0.0, L"Range", 100, 10, ControlPropertyEditorKind::Number);
		minimumOptions.Coerce = [](
			NumericUpDown&, const double& proposed) -> std::optional<double>
		{
			return std::isfinite(proposed)
				? std::optional<double>{ proposed } : std::nullopt;
		};
		minimumOptions.Design.Step = 0.1;
		BindingPropertyRegistry::Register<NumericUpDown, double>(L"Min",
			[](NumericUpDown& target) { return target.Min; },
			[](NumericUpDown& target, const double& value) { target.Min = value; },
			NumericPropertySubscriber(L"Min"), std::move(minimumOptions));

		auto maximumOptions = NumericPropertyOptions(
			100.0, L"Range", 100, 20, ControlPropertyEditorKind::Number);
		maximumOptions.Coerce = [](
			NumericUpDown& target, const double& proposed) -> std::optional<double>
		{
			if (!std::isfinite(proposed)) return std::nullopt;
			return (std::max)(target.Min, proposed);
		};
		maximumOptions.Design.Step = 0.1;
		BindingPropertyRegistry::Register<NumericUpDown, double>(L"Max",
			[](NumericUpDown& target) { return target.Max; },
			[](NumericUpDown& target, const double& value) { target.Max = value; },
			NumericPropertySubscriber(L"Max"), std::move(maximumOptions));

		auto valueOptions = NumericPropertyOptions(
			0.0, L"Range", 100, 60, ControlPropertyEditorKind::Number);
		valueOptions.Coerce = [](
			NumericUpDown& target, const double& proposed) -> std::optional<double>
		{
			return target.SnapValue(proposed);
		};
		valueOptions.Equals = [](
			const double& left, const double& right)
		{
			return std::fabs(left - right) <= 0.0000001;
		};
		valueOptions.Changed = [](
			NumericUpDown& target, const double& oldValue, const double& newValue)
		{
			if (!target._editing) target.SyncTextFromValue();
			target.OnValueChanged(&target, oldValue, newValue);
		};
		valueOptions.Design.Step = 0.1;
		BindingPropertyRegistry::Register<NumericUpDown, double>(L"Value",
			[](NumericUpDown& target) { return target.Value; },
			[](NumericUpDown& target, const double& value) { target.Value = value; },
			NumericPropertySubscriber(L"Value"), std::move(valueOptions));

		auto stepOptions = NumericPropertyOptions(
			1.0, L"Range", 100, 30, ControlPropertyEditorKind::Number);
		stepOptions.Coerce = [](
			NumericUpDown&, const double& proposed) -> std::optional<double>
		{
			return std::isfinite(proposed)
				? std::optional<double>{ (std::max)(0.0, proposed) }
				: std::nullopt;
		};
		stepOptions.Design.Minimum = 0.0;
		stepOptions.Design.Step = 0.1;
		BindingPropertyRegistry::Register<NumericUpDown, double>(L"Step",
			[](NumericUpDown& target) { return target.Step; },
			[](NumericUpDown& target, const double& value) { target.Step = value; },
			NumericPropertySubscriber(L"Step"), std::move(stepOptions));

		auto decimalOptions = NumericPropertyOptions(
			0, L"Range", 100, 40, ControlPropertyEditorKind::Number);
		decimalOptions.Coerce = [](
			NumericUpDown&, const int& proposed) -> std::optional<int>
		{
			return (std::clamp)(proposed, 0, 15);
		};
		decimalOptions.Design.Minimum = 0.0;
		decimalOptions.Design.Maximum = 15.0;
		decimalOptions.Design.Step = 1.0;
		BindingPropertyRegistry::Register<NumericUpDown, int>(L"DecimalPlaces",
			[](NumericUpDown& target) { return target.DecimalPlaces; },
			[](NumericUpDown& target, const int& value) { target.DecimalPlaces = value; },
			NumericPropertySubscriber(L"DecimalPlaces"), std::move(decimalOptions));

		auto snapOptions = NumericPropertyOptions(
			true, L"Range", 100, 50, ControlPropertyEditorKind::Boolean);
		BindingPropertyRegistry::Register<NumericUpDown, bool>(L"SnapToStep",
			[](NumericUpDown& target) { return target.SnapToStep; },
			[](NumericUpDown& target, const bool& value) { target.SnapToStep = value; },
			NumericPropertySubscriber(L"SnapToStep"), std::move(snapOptions));

		auto selectAllOptions = NumericPropertyOptions(
			true, L"Behavior", 110, 20, ControlPropertyEditorKind::Boolean);
		BindingPropertyRegistry::Register<NumericUpDown, bool>(L"SelectAllOnFocus",
			[](NumericUpDown& target) { return target.SelectAllOnFocus; },
			[](NumericUpDown& target, const bool& value) { target.SelectAllOnFocus = value; },
			NumericPropertySubscriber(L"SelectAllOnFocus"), std::move(selectAllOptions));

		auto wheelOptions = NumericPropertyOptions(
			true, L"Behavior", 110, 30, ControlPropertyEditorKind::Boolean);
		BindingPropertyRegistry::Register<NumericUpDown, bool>(L"UseMouseWheel",
			[](NumericUpDown& target) { return target.UseMouseWheel; },
			[](NumericUpDown& target, const bool& value) { target.UseMouseWheel = value; },
			NumericPropertySubscriber(L"UseMouseWheel"), std::move(wheelOptions));

		BindingPropertyRegistry::Register<NumericUpDown, float>(L"Border",
			[](NumericUpDown& target) { return target.Border; },
			[](NumericUpDown& target, const float& value) { target.Border = value; },
			NumericPropertySubscriber(L"Border"), NumericMetricOptions(1.0f, 10));
		BindingPropertyRegistry::Register<NumericUpDown, float>(L"CornerRadius",
			[](NumericUpDown& target) { return target.CornerRadius; },
			[](NumericUpDown& target, const float& value) { target.CornerRadius = value; },
			NumericPropertySubscriber(L"CornerRadius"), NumericMetricOptions(6.0f, 20));
		BindingPropertyRegistry::Register<NumericUpDown, float>(L"ButtonWidth",
			[](NumericUpDown& target) { return target.ButtonWidth; },
			[](NumericUpDown& target, const float& value) { target.ButtonWidth = value; },
			NumericPropertySubscriber(L"ButtonWidth"), NumericMetricOptions(28.0f, 30));
		BindingPropertyRegistry::Register<NumericUpDown, float>(L"TextPaddingX",
			[](NumericUpDown& target) { return target.TextPaddingX; },
			[](NumericUpDown& target, const float& value) { target.TextPaddingX = value; },
			NumericPropertySubscriber(L"TextPaddingX"), NumericMetricOptions(8.0f, 40));
		BindingPropertyRegistry::Register<NumericUpDown, float>(L"FocusBorder",
			[](NumericUpDown& target) { return target.FocusBorder; },
			[](NumericUpDown& target, const float& value) { target.FocusBorder = value; },
			NumericPropertySubscriber(L"FocusBorder"), NumericMetricOptions(1.6f, 50));

#define CUI_REGISTER_NUMERIC_COLOR(name, propertyName, defaultValue, order) \
		BindingPropertyRegistry::Register<NumericUpDown, D2D1_COLOR_F>(propertyName, \
			[](NumericUpDown& target) { return target.name; }, \
			[](NumericUpDown& target, const D2D1_COLOR_F& value) { target.name = value; }, \
			NumericPropertySubscriber(propertyName), NumericColorOptions(defaultValue, order))

		CUI_REGISTER_NUMERIC_COLOR(PanelBackColor, L"PanelBackColor", cui::theme::palette::Surface, 60);
		CUI_REGISTER_NUMERIC_COLOR(ButtonBackColor, L"ButtonBackColor", cui::theme::palette::SurfaceMuted, 70);
		CUI_REGISTER_NUMERIC_COLOR(ButtonHoverColor, L"ButtonHoverColor", cui::theme::palette::AccentSoft, 80);
		CUI_REGISTER_NUMERIC_COLOR(ButtonPressedColor, L"ButtonPressedColor", cui::theme::palette::AccentSelected, 90);
		CUI_REGISTER_NUMERIC_COLOR(AccentColor, L"AccentColor", cui::theme::palette::Accent, 100);
		CUI_REGISTER_NUMERIC_COLOR(FocusBorderColor, L"FocusBorderColor", cui::theme::palette::Accent, 110);
		CUI_REGISTER_NUMERIC_COLOR(SelectedBackColor, L"SelectedBackColor", cui::theme::palette::SelectionBack, 120);
		CUI_REGISTER_NUMERIC_COLOR(SelectedForeColor, L"SelectedForeColor", cui::theme::palette::TextPrimary, 130);
		CUI_REGISTER_NUMERIC_COLOR(MutedTextColor, L"MutedTextColor", cui::theme::palette::TextMuted, 140);
		CUI_REGISTER_NUMERIC_COLOR(DisabledOverlayColor, L"DisabledOverlayColor", cui::theme::palette::DisabledOverlay, 150);

#undef CUI_REGISTER_NUMERIC_COLOR
		return true;
	}();
	(void)registered;
}

NumericUpDown::NumericUpDown(int x, int y, int width, int height)
{
	this->Location = POINT{ x, y };
	this->Size = SIZE{ width, height };
	this->BackColor = cui::theme::palette::Surface;
	this->BorderColor = cui::theme::palette::BorderStrong;
	this->ForeColor = cui::theme::palette::TextPrimary;
	this->Cursor = CursorKind::IBeam;
	SyncTextFromValue();

	this->OnGotFocus += [this](class Control* sender)
		{
			(void)sender;
			BeginEdit(SelectAllOnFocus);
		};

	this->OnLostFocus += [this](class Control* sender)
		{
			(void)sender;
			if (_editing)
				CommitEdit();
			_dragText = false;
			_dragUp = false;
			_dragDown = false;
			_hoverButton = 0;
			UpdateCaretBlinkState(false, 0, 0, false);
			InvalidateVisual();
		};
}

GET_CPP(NumericUpDown, double, Min)
{
	return _min;
}

SET_CPP(NumericUpDown, double, Min)
{
	if (!SetPropertyField(L"Min", _min, value)) return;
	(void)ReevaluatePropertyValue(L"Max");
	ReevaluateValue();
}

GET_CPP(NumericUpDown, double, Max)
{
	return _max;
}

SET_CPP(NumericUpDown, double, Max)
{
	if (!SetPropertyField(L"Max", _max, value)) return;
	ReevaluateValue();
}

GET_CPP(NumericUpDown, double, Value)
{
	return _value;
}

SET_CPP(NumericUpDown, double, Value)
{
	(void)SetPropertyField(L"Value", _value, value);
}

double NumericUpDown::ClampValue(double value) const
{
	if (!std::isfinite(value)) value = _min;
	return (std::clamp)(value, _min, (std::max)(_min, _max));
}

double NumericUpDown::SnapValue(double value) const
{
	double v = ClampValue(value);
	if (!_snapToStep || _step <= 0.0 || !std::isfinite(_step))
		return v;
	double steps = (v - _min) / _step;
	double snapped = _min + std::round(steps) * _step;
	return ClampValue(snapped);
}

void NumericUpDown::SetCurrentValue(double value)
{
	(void)SetCurrentPropertyField(L"Value", _value, value);
}

void NumericUpDown::ReevaluateValue()
{
	(void)ReevaluatePropertyValue(L"Value");
}

GET_CPP(NumericUpDown, double, Step) { return _step; }
SET_CPP(NumericUpDown, double, Step)
{
	if (!SetPropertyField(L"Step", _step, value)) return;
	ReevaluateValue();
}

GET_CPP(NumericUpDown, int, DecimalPlaces) { return _decimalPlaces; }
SET_CPP(NumericUpDown, int, DecimalPlaces)
{
	if (!SetPropertyField(L"DecimalPlaces", _decimalPlaces, value)) return;
	if (!_editing) SyncTextFromValue();
}

GET_CPP(NumericUpDown, bool, SnapToStep) { return _snapToStep; }
SET_CPP(NumericUpDown, bool, SnapToStep)
{
	if (!SetPropertyField(L"SnapToStep", _snapToStep, value)) return;
	ReevaluateValue();
}

GET_CPP(NumericUpDown, bool, SelectAllOnFocus) { return _selectAllOnFocus; }
SET_CPP(NumericUpDown, bool, SelectAllOnFocus)
{
	(void)SetPropertyField(L"SelectAllOnFocus", _selectAllOnFocus, value);
}

GET_CPP(NumericUpDown, bool, UseMouseWheel) { return _useMouseWheel; }
SET_CPP(NumericUpDown, bool, UseMouseWheel)
{
	(void)SetPropertyField(L"UseMouseWheel", _useMouseWheel, value);
}

#define CUI_NUMERIC_PROPERTY_IMPL(type, name, field, propertyName) \
	GET_CPP(NumericUpDown, type, name) { return field; } \
	SET_CPP(NumericUpDown, type, name) { (void)SetPropertyField(propertyName, field, value); }

CUI_NUMERIC_PROPERTY_IMPL(float, Border, _border, L"Border")
CUI_NUMERIC_PROPERTY_IMPL(float, CornerRadius, _cornerRadius, L"CornerRadius")
CUI_NUMERIC_PROPERTY_IMPL(float, ButtonWidth, _buttonWidth, L"ButtonWidth")
CUI_NUMERIC_PROPERTY_IMPL(float, TextPaddingX, _textPaddingX, L"TextPaddingX")
CUI_NUMERIC_PROPERTY_IMPL(float, FocusBorder, _focusBorder, L"FocusBorder")
CUI_NUMERIC_PROPERTY_IMPL(D2D1_COLOR_F, PanelBackColor, _panelBackColor, L"PanelBackColor")
CUI_NUMERIC_PROPERTY_IMPL(D2D1_COLOR_F, ButtonBackColor, _buttonBackColor, L"ButtonBackColor")
CUI_NUMERIC_PROPERTY_IMPL(D2D1_COLOR_F, ButtonHoverColor, _buttonHoverColor, L"ButtonHoverColor")
CUI_NUMERIC_PROPERTY_IMPL(D2D1_COLOR_F, ButtonPressedColor, _buttonPressedColor, L"ButtonPressedColor")
CUI_NUMERIC_PROPERTY_IMPL(D2D1_COLOR_F, AccentColor, _accentColor, L"AccentColor")
CUI_NUMERIC_PROPERTY_IMPL(D2D1_COLOR_F, FocusBorderColor, _focusBorderColor, L"FocusBorderColor")
CUI_NUMERIC_PROPERTY_IMPL(D2D1_COLOR_F, SelectedBackColor, _selectedBackColor, L"SelectedBackColor")
CUI_NUMERIC_PROPERTY_IMPL(D2D1_COLOR_F, SelectedForeColor, _selectedForeColor, L"SelectedForeColor")
CUI_NUMERIC_PROPERTY_IMPL(D2D1_COLOR_F, MutedTextColor, _mutedTextColor, L"MutedTextColor")
CUI_NUMERIC_PROPERTY_IMPL(D2D1_COLOR_F, DisabledOverlayColor, _disabledOverlayColor, L"DisabledOverlayColor")

#undef CUI_NUMERIC_PROPERTY_IMPL

void NumericUpDown::SyncTextFromValue()
{
	this->Text = FormatValue();
	SelectionStart = SelectionEnd = (std::clamp)(SelectionEnd, 0, static_cast<int>(this->Text.size()));
	HorizontalScrollOffset = 0.0f;
	undoStack.clear();
	redoStack.clear();
}

std::wstring NumericUpDown::FormatValue() const
{
	std::wstringstream stream;
	stream.setf(std::ios::fixed, std::ios::floatfield);
	stream << std::setprecision((std::max)(0, _decimalPlaces)) << _value;
	return stream.str();
}

bool NumericUpDown::TryParseEditText(const std::wstring& text, double& value) const
{
	value = 0.0;
	if (text.empty() || text == L"-" || text == L"+" || text == L"." || text == L"-." || text == L"+.")
		return false;

	wchar_t* end = nullptr;
	const wchar_t* start = text.c_str();
	double parsed = std::wcstod(start, &end);
	while (end && *end && std::iswspace(static_cast<wint_t>(*end)))
		++end;
	if (end == start || (end && *end != L'\0') || !std::isfinite(parsed))
		return false;

	value = parsed;
	return true;
}

bool NumericUpDown::IsEditTextAllowed(const std::wstring& text) const
{
	if (!IsNumericEditCandidate(text))
		return false;
	if (text.empty() || text == L"-" || text == L"+" || text == L"." || text == L"-." || text == L"+.")
		return true;

	double parsed = 0.0;
	if (!TryParseEditText(text, parsed))
		return true;

	const bool hasDecimalPoint = text.find(L'.') != std::wstring::npos;
	const bool isNegative = !text.empty() && text[0] == L'-';
	if (parsed > _max)
	{
		// In an all-negative range, "-1" is still a useful prefix for "-10".
		if (isNegative && _max < 0.0 && !hasDecimalPoint)
			return true;
		return false;
	}
	if (parsed < _min)
	{
		// In a positive range, "1" is still a useful prefix for "10".
		if (!isNegative && _min > 0.0 && !hasDecimalPoint)
			return true;
		return false;
	}

	return true;
}

void NumericUpDown::BeginEdit(bool selectAll)
{
	if (!_editing)
	{
		_editing = true;
		SelectionStart = SelectionEnd = (std::clamp)(SelectionEnd, 0, static_cast<int>(this->Text.size()));
	}

	if (selectAll)
		SelectAllText();
	else
	{
		SelectionStart = (std::clamp)(SelectionStart, 0, static_cast<int>(this->Text.size()));
		SelectionEnd = (std::clamp)(SelectionEnd, 0, static_cast<int>(this->Text.size()));
	}

	UpdateScroll(selectAll);
	UpdateImeCompositionWindow();
	InvalidateVisual();
}

bool NumericUpDown::CommitEdit()
{
	if (!_editing)
		return true;

	double parsed = 0.0;
	if (!TryParseEditText(this->Text, parsed))
	{
		_editing = false;
		SyncTextFromValue();
		SelectionStart = SelectionEnd = static_cast<int>(this->Text.size());
		InvalidateVisual();
		return false;
	}

	_editing = false;
	SetCurrentValue(parsed);
	SyncTextFromValue();
	SelectionStart = SelectionEnd = static_cast<int>(this->Text.size());
	InvalidateVisual();
	return true;
}

void NumericUpDown::CancelEdit()
{
	_editing = false;
	SyncTextFromValue();
	SelectionStart = SelectionEnd = static_cast<int>(this->Text.size());
	InvalidateVisual();
}

void NumericUpDown::SelectAllText()
{
	SelectionStart = 0;
	SelectionEnd = static_cast<int>(this->Text.size());
	HorizontalScrollOffset = 0.0f;
}

void NumericUpDown::InputText(std::wstring input)
{
	std::wstring oldText = this->Text;
	std::wstring newText = this->Text;
	int newSelectionStart = SelectionStart;
	int newSelectionEnd = SelectionEnd;
	const int selStartBefore = SelectionStart;
	const int selEndBefore = SelectionEnd;

	auto result = CuiTextEdit::ReplaceSelection(newText, newSelectionStart, newSelectionEnd, input, NumericEditOptions());
	if (!result.applied || !IsEditTextAllowed(newText))
		return;

	SelectionStart = newSelectionStart;
	SelectionEnd = newSelectionEnd;
	if (result.textChanged && !isApplyingUndoRedo)
	{
		UndoRecord rec;
		rec.pos = result.replaceStart;
		rec.removedText = result.removedText;
		rec.insertedText = result.insertedText;
		rec.selStartBefore = selStartBefore;
		rec.selEndBefore = selEndBefore;
		rec.selStartAfter = SelectionStart;
		rec.selEndAfter = SelectionEnd;
		undoStack.push_back(rec);
		redoStack.clear();
	}
	CommitTextChange(this, oldText, newText);
}

void NumericUpDown::InputBack()
{
	std::wstring oldText = this->Text;
	std::wstring newText = this->Text;
	int newSelectionStart = SelectionStart;
	int newSelectionEnd = SelectionEnd;
	const int selStartBefore = SelectionStart;
	const int selEndBefore = SelectionEnd;

	auto result = CuiTextEdit::Backspace(newText, newSelectionStart, newSelectionEnd, NumericEditOptions());
	if (!result.applied || !IsEditTextAllowed(newText))
		return;

	SelectionStart = newSelectionStart;
	SelectionEnd = newSelectionEnd;
	if (result.textChanged && !isApplyingUndoRedo)
	{
		UndoRecord rec;
		rec.pos = result.replaceStart;
		rec.removedText = result.removedText;
		rec.insertedText = L"";
		rec.selStartBefore = selStartBefore;
		rec.selEndBefore = selEndBefore;
		rec.selStartAfter = SelectionStart;
		rec.selEndAfter = SelectionEnd;
		undoStack.push_back(rec);
		redoStack.clear();
	}
	CommitTextChange(this, oldText, newText);
}

void NumericUpDown::InputDelete()
{
	std::wstring oldText = this->Text;
	std::wstring newText = this->Text;
	int newSelectionStart = SelectionStart;
	int newSelectionEnd = SelectionEnd;
	const int selStartBefore = SelectionStart;
	const int selEndBefore = SelectionEnd;

	auto result = CuiTextEdit::DeleteForward(newText, newSelectionStart, newSelectionEnd, NumericEditOptions());
	if (!result.applied || !IsEditTextAllowed(newText))
		return;

	SelectionStart = newSelectionStart;
	SelectionEnd = newSelectionEnd;
	if (result.textChanged && !isApplyingUndoRedo)
	{
		UndoRecord rec;
		rec.pos = result.replaceStart;
		rec.removedText = result.removedText;
		rec.insertedText = L"";
		rec.selStartBefore = selStartBefore;
		rec.selEndBefore = selEndBefore;
		rec.selStartAfter = SelectionStart;
		rec.selEndAfter = SelectionEnd;
		undoStack.push_back(rec);
		redoStack.clear();
	}
	CommitTextChange(this, oldText, newText);
}

void NumericUpDown::ApplyUndoRecord(const UndoRecord& rec, bool isUndo)
{
	std::wstring oldText = this->Text;
	std::wstring newText = this->Text;
	isApplyingUndoRedo = true;

	int pos = (std::clamp)(rec.pos, 0, static_cast<int>(newText.size()));
	const std::wstring& removeText = isUndo ? rec.insertedText : rec.removedText;
	const std::wstring& insertText = isUndo ? rec.removedText : rec.insertedText;

	if (!removeText.empty() && pos <= static_cast<int>(newText.size()))
	{
		size_t removeLen = (std::min)(removeText.size(), newText.size() - static_cast<size_t>(pos));
		newText.erase(static_cast<size_t>(pos), removeLen);
	}
	if (!insertText.empty())
		newText.insert(static_cast<size_t>(pos), insertText);

	if (!IsEditTextAllowed(newText))
	{
		isApplyingUndoRedo = false;
		return;
	}

	if (isUndo)
	{
		SelectionStart = rec.selStartBefore;
		SelectionEnd = rec.selEndBefore;
	}
	else
	{
		SelectionStart = rec.selStartAfter;
		SelectionEnd = rec.selEndAfter;
	}
	SelectionStart = (std::clamp)(SelectionStart, 0, static_cast<int>(newText.size()));
	SelectionEnd = (std::clamp)(SelectionEnd, 0, static_cast<int>(newText.size()));

	isApplyingUndoRedo = false;
	CommitTextChange(this, oldText, newText);
}

void NumericUpDown::Undo()
{
	if (undoStack.empty()) return;
	UndoRecord rec = undoStack.back();
	undoStack.pop_back();
	ApplyUndoRecord(rec, true);
	redoStack.push_back(rec);
}

void NumericUpDown::Redo()
{
	if (redoStack.empty()) return;
	UndoRecord rec = redoStack.back();
	redoStack.pop_back();
	ApplyUndoRecord(rec, false);
	undoStack.push_back(rec);
}

std::wstring NumericUpDown::GetSelectedString()
{
	auto span = CuiTextEdit::NormalizeSelection(SelectionStart, SelectionEnd, this->Text.size());
	if (!span.HasSelection())
		return L"";
	return this->Text.substr(static_cast<size_t>(span.start), static_cast<size_t>(span.Length()));
}

void NumericUpDown::UpdateScroll(bool arrival)
{
	(void)arrival;
	auto font = this->Font;
	if (!font)
		return;

	auto textRect = TextRect();
	const float renderWidth = (std::max)(1.0f, RectWidth(textRect));
	SelectionStart = (std::clamp)(SelectionStart, 0, static_cast<int>(this->Text.size()));
	SelectionEnd = (std::clamp)(SelectionEnd, 0, static_cast<int>(this->Text.size()));

	float caretLeft = 0.0f;
	float caretRight = 0.0f;
	if (!this->Text.empty())
	{
		auto hit = font->HitTestTextRange(this->Text, static_cast<UINT32>(SelectionEnd), 0);
		if (!hit.empty())
		{
			caretLeft = hit[0].left;
			caretRight = hit[0].left + hit[0].width;
		}
		else
		{
			caretLeft = font->GetTextSize(this->Text).width;
			caretRight = caretLeft;
		}
	}

	if (caretRight - HorizontalScrollOffset > renderWidth - 2.0f)
		HorizontalScrollOffset = caretRight - renderWidth + 2.0f;
	if (caretLeft - HorizontalScrollOffset < 0.0f)
		HorizontalScrollOffset = caretLeft;
	if (HorizontalScrollOffset < 0.0f)
		HorizontalScrollOffset = 0.0f;
}

void NumericUpDown::UpdateImeCompositionWindow()
{
	if (!ParentForm)
		return;

	D2D1_RECT_F imeRect{};
	if (_caretRectCacheValid)
	{
		imeRect = _caretRectCache;
	}
	else
	{
		const auto absoluteLocation = this->GetAbsoluteLocationDip();
		auto textRect = TextRect();
		float caretX = static_cast<float>(absoluteLocation.x) + textRect.left - HorizontalScrollOffset;
		float caretY = static_cast<float>(absoluteLocation.y) + TextTop(this->Font, textRect);
		float caretH = (this->Font && this->Font->FontHeight > 0.0f) ? this->Font->FontHeight : 16.0f;
		imeRect = D2D1_RECT_F{ caretX, caretY, caretX + 1.0f, caretY + caretH };
	}
	ParentForm->SetImeCompositionWindowFromLogicalRect(imeRect);
}

D2D1_RECT_F NumericUpDown::ButtonPanelRect() const
{
	const float w = (float)_size.cx;
	const float h = (float)_size.cy;
	const float bw = (std::clamp)(_buttonWidth, 18.0f, (std::max)(18.0f, w * 0.45f));
	return D2D1::RectF((std::max)(0.0f, w - bw), 0.0f, w, h);
}

D2D1_RECT_F NumericUpDown::UpButtonRect() const
{
	auto panel = ButtonPanelRect();
	const float mid = panel.top + RectHeight(panel) * 0.5f;
	return D2D1::RectF(panel.left, panel.top, panel.right, mid);
}

D2D1_RECT_F NumericUpDown::DownButtonRect() const
{
	auto panel = ButtonPanelRect();
	const float mid = panel.top + RectHeight(panel) * 0.5f;
	return D2D1::RectF(panel.left, mid, panel.right, panel.bottom);
}

D2D1_RECT_F NumericUpDown::TextRect() const
{
	const float h = (float)_size.cy;
	auto buttons = ButtonPanelRect();
	return D2D1::RectF(
		_textPaddingX, 0.0f,
		(std::max)(_textPaddingX, buttons.left - _textPaddingX), h);
}

int NumericUpDown::HitTestButton(int localX, int localY) const
{
	if (PtInRectF(UpButtonRect(), (float)localX, (float)localY))
		return 1;
	if (PtInRectF(DownButtonRect(), (float)localX, (float)localY))
		return -1;
	return 0;
}

int NumericUpDown::HitTestTextPosition(int localX, int localY)
{
	auto font = this->Font;
	if (!font)
		return static_cast<int>(this->Text.size());

	auto textRect = TextRect();
	const float x = ((float)localX - textRect.left) + HorizontalScrollOffset;
	const float y = (float)localY - TextTop(font, textRect);
	return font->HitTestTextPosition(this->Text, FLT_MAX, (std::max)(1.0f, RectHeight(textRect)), x, y);
}

void NumericUpDown::StepBy(int direction)
{
	if (direction == 0)
		return;

	const bool keepEditing = ParentForm && ParentForm->Selected == this;
	if (_editing)
		CommitEdit();

	double delta = Step > 0.0 && std::isfinite(Step) ? Step : 1.0;
	if (GetKeyState(VK_SHIFT) & 0x8000)
		delta *= 10.0;
	SetCurrentValue(_value + delta * (double)direction);

	if (keepEditing)
	{
		_editing = true;
		SelectionStart = SelectionEnd = static_cast<int>(this->Text.size());
		UpdateScroll(true);
		UpdateImeCompositionWindow();
	}
}

void NumericUpDown::StartHoverAnimation(float target)
{
	target = (std::clamp)(target, 0.0f, 1.0f);
	CurrentHoverProgress();
	if (std::fabs(_hoverProgress - target) <= 0.001f)
	{
		_hoverProgress = target;
		_targetHoverProgress = target;
		_animating = false;
		InvalidateVisual();
		return;
	}
	_animStartProgress = _hoverProgress;
	_targetHoverProgress = target;
	_animStartTick = ::GetTickCount64();
	_animating = true;
	InvalidateVisual();
}

float NumericUpDown::CurrentHoverProgress()
{
	if (!_animating)
		return _hoverProgress;
	ULONGLONG now = ::GetTickCount64();
	ULONGLONG elapsed = now >= _animStartTick ? now - _animStartTick : 0;
	const UINT duration = EffectiveAnimationDuration(_animDurationMs);
	float t = duration > 0 ? (float)elapsed / (float)duration : 1.0f;
	if (t >= 1.0f)
	{
		_hoverProgress = _targetHoverProgress;
		_animating = false;
		return _hoverProgress;
	}
	t = 1.0f - std::pow(1.0f - (std::clamp)(t, 0.0f, 1.0f), 3.0f);
	_hoverProgress = _animStartProgress + (_targetHoverProgress - _animStartProgress) * t;
	return _hoverProgress;
}

CursorKind NumericUpDown::QueryCursor(int localX, int localY)
{
	if (!Enable)
		return CursorKind::Arrow;
	return HitTestButton(localX, localY) == 0 ? CursorKind::IBeam : CursorKind::Hand;
}

bool NumericUpDown::CanHandleMouseWheel(int delta, int localX, int localY)
{
	(void)localX;
	(void)localY;
	if (!UseMouseWheel || delta == 0 || !Enable)
		return false;
	return true;
}

bool NumericUpDown::HandlesNavigationKey(WPARAM key) const
{
	return key == VK_UP || key == VK_DOWN || key == VK_HOME || key == VK_END || key == VK_PRIOR || key == VK_NEXT;
}

bool NumericUpDown::IsAnimationRunning()
{
	CurrentHoverProgress();
	return _animating || IsCaretBlinkAnimating();
}

bool NumericUpDown::GetAnimatedInvalidRect(D2D1_RECT_F& outRect)
{
	if (GetCaretBlinkInvalidRect(outRect))
		return true;
	if (!_animating)
		return false;
	outRect = this->AbsRect;
	return true;
}

void NumericUpDown::Update()
{
	if (!this->IsVisual) return;
	auto d2d = this->ParentForm ? this->ParentForm->Render : nullptr;
	if (!d2d) return;

	const float hoverProgress = CurrentHoverProgress();
	const bool isSelected = ParentForm && ParentForm->Selected == this;
	const bool isUnderMouse = ParentForm && ParentForm->UnderMouse == this;
	if (!isUnderMouse && !_dragUp && !_dragDown && _hoverButton != 0)
	{
		_hoverButton = 0;
		StartHoverAnimation(0.0f);
	}

	const auto size = this->GetActualSizeDip();
	const float width = size.width;
	const float height = size.height;
	const float radius = (std::clamp)(CornerRadius, 0.0f, (std::min)(width, height) * 0.5f);
	const float border = (std::max)(0.0f, Border);
	auto panelRect = ButtonPanelRect();
	auto upRect = UpButtonRect();
	auto downRect = DownButtonRect();
	auto textRect = TextRect();
	class Font* fontObj = this->Font;
	std::wstring text = this->Text;
	const float renderHeight = (std::max)(1.0f, RectHeight(textRect));
	textSize = fontObj ? fontObj->GetTextSize(text, FLT_MAX, renderHeight) : D2D1_SIZE_F{ 0,0 };
	const float textY = TextTop(fontObj, textRect);

	this->_caretRectCacheValid = false;
	bool shouldDrawCaret = false;
	D2D1_POINT_2F caretStart{};
	D2D1_POINT_2F caretEnd{};

	this->BeginRender();
	{
		D2D1_COLOR_F base = PanelBackColor.a > 0.0f ? PanelBackColor : this->BackColor;
		d2d->FillRoundRect(0.0f, 0.0f, width, height, base, radius);
		if (isUnderMouse && !_editing)
			d2d->FillRoundRect(1.0f, 1.0f, (std::max)(0.0f, width - 2.0f), (std::max)(0.0f, height - 2.0f),
				ScaleAlpha(ButtonHoverColor, 0.45f), (std::max)(0.0f, radius - 1.0f));

		if (fontObj)
		{
			d2d->PushDrawRect(textRect.left, textRect.top, (std::max)(1.0f, RectWidth(textRect)), RectHeight(textRect));
			const int sels = (std::min)(SelectionStart, SelectionEnd);
			const int sele = (std::max)(SelectionStart, SelectionEnd);
			const int selLen = sele - sels;

			if (isSelected && selLen > 0 && !text.empty())
			{
				auto selRange = fontObj->HitTestTextRange(text, static_cast<UINT32>(sels), static_cast<UINT32>(selLen));
				for (auto sr : selRange)
				{
					d2d->FillRect(
						textRect.left + sr.left - HorizontalScrollOffset,
						textY + sr.top,
						sr.width,
						sr.height,
						this->SelectedBackColor);
				}
			}

			if (!text.empty())
			{
				auto textLayout = Factory::CreateStringLayout(text, FLT_MAX, renderHeight, fontObj->FontObject);
				if (textLayout)
				{
					if (isSelected && selLen > 0)
					{
						d2d->DrawStringLayoutEffect(textLayout,
							textRect.left - HorizontalScrollOffset,
							textY,
							this->ForeColor,
							DWRITE_TEXT_RANGE{ static_cast<UINT32>(sels), static_cast<UINT32>(selLen) },
							this->SelectedForeColor,
							fontObj);
					}
					else
					{
						d2d->DrawStringLayout(textLayout,
							textRect.left - HorizontalScrollOffset,
							textY,
							this->ForeColor);
					}
					textLayout->Release();
				}
			}

			if (isSelected && selLen == 0)
			{
				float caretX = textRect.left - HorizontalScrollOffset;
				float caretTop = textY + 1.0f;
				float caretBottom = textY + (fontObj ? fontObj->FontHeight : 16.0f) - 1.0f;
				if (!text.empty())
				{
					auto caretRange = fontObj->HitTestTextRange(text, static_cast<UINT32>(SelectionEnd), 0);
					if (!caretRange.empty())
					{
						caretX = textRect.left + caretRange[0].left - HorizontalScrollOffset;
						caretTop = textY + caretRange[0].top + 1.0f;
						caretBottom = textY + caretRange[0].top + (caretRange[0].height > 0.0f ? caretRange[0].height : fontObj->FontHeight) - 1.0f;
					}
					else
					{
						caretX = textRect.left + fontObj->GetTextSize(text).width - HorizontalScrollOffset;
					}
				}

				const auto absoluteLocation = this->GetAbsoluteLocationDip();
				this->_caretRectCache = {
					static_cast<float>(absoluteLocation.x) + caretX - 2.0f,
					static_cast<float>(absoluteLocation.y) + caretTop - 2.0f,
					static_cast<float>(absoluteLocation.x) + caretX + 2.0f,
					static_cast<float>(absoluteLocation.y) + caretBottom + 2.0f
				};
				this->_caretRectCacheValid = true;
				shouldDrawCaret = true;
				caretStart = { caretX, caretTop };
				caretEnd = { caretX, caretBottom };
			}

			UpdateCaretBlinkState(isSelected, SelectionStart, SelectionEnd, this->_caretRectCacheValid, this->_caretRectCacheValid ? &this->_caretRectCache : nullptr);
			if (shouldDrawCaret && IsCaretBlinkVisible())
				d2d->DrawLine(caretStart, caretEnd, AccentColor, 1.2f);
			d2d->PopDrawRect();
		}
		else
		{
			UpdateCaretBlinkState(false, 0, 0, false);
		}

		d2d->FillRoundRect(panelRect.left, panelRect.top + 3.0f,
			RectWidth(panelRect) - 3.0f, (std::max)(0.0f, RectHeight(panelRect) - 6.0f), ButtonBackColor, 4.0f);
		if (_hoverButton != 0 && hoverProgress > 0.001f)
		{
			auto rect = _hoverButton > 0 ? upRect : downRect;
			auto color = (_dragUp && _hoverButton > 0) || (_dragDown && _hoverButton < 0)
				? ButtonPressedColor
				: ScaleAlpha(ButtonHoverColor, hoverProgress);
			d2d->FillRoundRect(rect.left + 2.0f, rect.top + 2.0f,
				(std::max)(0.0f, RectWidth(rect) - 4.0f), (std::max)(0.0f, RectHeight(rect) - 4.0f), color, 3.5f);
		}
		d2d->DrawLine(panelRect.left, 5.0f, panelRect.left, (std::max)(5.0f, height - 5.0f), ScaleAlpha(BorderColor, 0.65f), 1.0f);
		d2d->DrawLine(panelRect.left + 3.0f, height * 0.5f, width - 4.0f, height * 0.5f, ScaleAlpha(BorderColor, 0.52f), 1.0f);
		DrawSpinArrow(d2d, upRect, true, _hoverButton > 0 ? ForeColor : MutedTextColor);
		DrawSpinArrow(d2d, downRect, false, _hoverButton < 0 ? ForeColor : MutedTextColor);

		D2D1_COLOR_F borderColor = isSelected ? FocusBorderColor : BorderColor;
		float borderWidth = isSelected ? (std::max)(border, FocusBorder) : border;
		if (borderWidth > 0.0f && borderColor.a > 0.0f)
			d2d->DrawRoundRect(borderWidth * 0.5f, borderWidth * 0.5f,
				(std::max)(0.0f, width - borderWidth), (std::max)(0.0f, height - borderWidth),
				borderColor, borderWidth, radius);

		if (this->Image)
			this->RenderImage(radius);

		if (!Enable)
			d2d->FillRoundRect(0.0f, 0.0f, width, height, DisabledOverlayColor, radius);
	}
	this->EndRender();
}

bool NumericUpDown::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY)
{
	if (!this->Enable || !this->Visible) return true;

	switch (message)
	{
	case WM_MOUSEWHEEL:
		if (UseMouseWheel)
			StepBy(GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? 1 : -1);
		OnMouseWheel(this, MouseEventArgs(MouseButtons::None, 0, localX, localY, GET_WHEEL_DELTA_WPARAM(wParam)));
		return true;
	case WM_MOUSEMOVE:
	{
		if (ParentForm) ParentForm->UnderMouse = this;
		if (_dragText && ParentForm && ParentForm->Selected == this)
		{
			SelectionEnd = HitTestTextPosition(localX, localY);
			UpdateScroll();
			InvalidateVisual();
		}
		else
		{
			int hit = HitTestButton(localX, localY);
			if (hit != _hoverButton)
			{
				_hoverButton = hit;
				StartHoverAnimation(hit == 0 ? 0.0f : 1.0f);
			}
		}
		OnMouseMove(this, MouseEventArgs(MouseButtons::None, 0, localX, localY, HIWORD(wParam)));
		return true;
	}
	case WM_LBUTTONDOWN:
	{
		if (ParentForm) ParentForm->SetSelectedControl(this, false);
		int hit = HitTestButton(localX, localY);
		if (hit != 0)
		{
			_dragText = false;
			_dragUp = hit > 0;
			_dragDown = hit < 0;
			_hoverButton = hit;
			StartHoverAnimation(1.0f);
			StepBy(hit);
		}
		else
		{
			BeginEdit(false);
			_dragText = true;
			SelectionStart = SelectionEnd = HitTestTextPosition(localX, localY);
			UpdateScroll();
			UpdateImeCompositionWindow();
		}
		OnMouseDown(this, MouseEventArgs(MouseButtons::Left, 0, localX, localY, HIWORD(wParam)));
		InvalidateVisual();
		return true;
	}
	case WM_LBUTTONUP:
	{
		if (_dragText && ParentForm && ParentForm->Selected == this)
		{
			SelectionEnd = HitTestTextPosition(localX, localY);
			UpdateScroll();
		}
		_dragText = false;
		_dragUp = false;
		_dragDown = false;
		int hit = HitTestButton(localX, localY);
		if (hit != _hoverButton)
		{
			_hoverButton = hit;
			StartHoverAnimation(hit == 0 ? 0.0f : 1.0f);
		}
		MouseEventArgs e(MouseButtons::Left, 0, localX, localY, HIWORD(wParam));
		OnMouseUp(this, e);
		OnMouseClick(this, e);
		InvalidateVisual();
		return true;
	}
	case WM_LBUTTONDBLCLK:
	{
		if (ParentForm) ParentForm->SetSelectedControl(this, false);
		int hit = HitTestButton(localX, localY);
		if (hit != 0)
		{
			_dragText = false;
			_dragUp = hit > 0;
			_dragDown = hit < 0;
			_hoverButton = hit;
			StartHoverAnimation(1.0f);
			StepBy(hit);
		}
		else
		{
			BeginEdit(false);
			SelectAllText();
		}
		MouseEventArgs e(MouseButtons::Left, 0, localX, localY, HIWORD(wParam));
		OnMouseDoubleClick(this, e);
		InvalidateVisual();
		return true;
	}
	case WM_KEYDOWN:
	{
		if (!_editing)
			BeginEdit(false);

		if (GetAsyncKeyState(VK_CONTROL) & 0x8000)
		{
			if (wParam == 'Z')
			{
				Undo();
				UpdateScroll();
				InvalidateVisual();
				return true;
			}
			if (wParam == 'Y')
			{
				Redo();
				UpdateScroll();
				InvalidateVisual();
				return true;
			}
		}

		UpdateImeCompositionWindow();
		const bool extendSelection = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
		switch (wParam)
		{
		case VK_UP:
			StepBy(1);
			break;
		case VK_DOWN:
			StepBy(-1);
			break;
		case VK_PRIOR:
			StepBy(10);
			break;
		case VK_NEXT:
			StepBy(-10);
			break;
		case VK_RETURN:
			CommitEdit();
			BeginEdit(false);
			SelectionStart = SelectionEnd = static_cast<int>(this->Text.size());
			UpdateScroll(true);
			break;
		case VK_ESCAPE:
			CancelEdit();
			BeginEdit(true);
			break;
		case VK_DELETE:
			InputDelete();
			UpdateScroll();
			break;
		case VK_RIGHT:
		{
			int textLength = static_cast<int>(this->Text.size());
			auto span = CuiTextEdit::NormalizeSelection(SelectionStart, SelectionEnd, this->Text.size());
			if (!extendSelection && span.HasSelection())
			{
				SelectionStart = SelectionEnd = span.end;
				UpdateScroll();
			}
			else if (SelectionEnd < textLength)
			{
				SelectionEnd = CuiTextEdit::GetNextCaretIndex(this->Text, SelectionEnd, false);
				if (!extendSelection)
					SelectionStart = SelectionEnd;
				UpdateScroll();
			}
			break;
		}
		case VK_LEFT:
		{
			auto span = CuiTextEdit::NormalizeSelection(SelectionStart, SelectionEnd, this->Text.size());
			if (!extendSelection && span.HasSelection())
			{
				SelectionStart = SelectionEnd = span.start;
				UpdateScroll();
			}
			else if (SelectionEnd > 0)
			{
				SelectionEnd = CuiTextEdit::GetPreviousCaretIndex(this->Text, SelectionEnd, false);
				if (!extendSelection)
					SelectionStart = SelectionEnd;
				UpdateScroll();
			}
			break;
		}
		case VK_HOME:
			SelectionEnd = 0;
			if (!extendSelection)
				SelectionStart = SelectionEnd;
			UpdateScroll(true);
			break;
		case VK_END:
			SelectionEnd = static_cast<int>(this->Text.size());
			if (!extendSelection)
				SelectionStart = SelectionEnd;
			UpdateScroll(true);
			break;
		default:
			break;
		}
		OnKeyDown(this, KeyEventArgs((Keys)(wParam | 0)));
		InvalidateVisual();
		return true;
	}
	case WM_CHAR:
	{
		if (!_editing)
			BeginEdit(SelectAllOnFocus);

		wchar_t ch = (wchar_t)wParam;
		if (ch == L'\r')
		{
			CommitEdit();
			BeginEdit(false);
			SelectionStart = SelectionEnd = static_cast<int>(this->Text.size());
			UpdateScroll(true);
		}
		else if (ch == 1)
		{
			SelectAllText();
			UpdateScroll(true);
		}
		else if (ch == 8)
		{
			InputBack();
			UpdateScroll();
		}
		else if (ch == 22)
		{
			std::wstring clipboardText;
			if (TryReadClipboardText(this->ParentForm ? this->ParentForm->Handle : nullptr, clipboardText))
			{
				InputText(clipboardText);
				UpdateScroll();
			}
		}
		else if (ch == 3)
		{
			WriteClipboardText(this->ParentForm ? this->ParentForm->Handle : nullptr, GetSelectedString());
		}
		else if (ch == 24)
		{
			WriteClipboardText(this->ParentForm ? this->ParentForm->Handle : nullptr, GetSelectedString());
			InputBack();
			UpdateScroll();
		}
		else if (CuiTextEdit::IsTextInputChar(ch))
		{
			const wchar_t text[] = { ch, L'\0' };
			InputText(text);
			UpdateScroll();
		}
		InvalidateVisual();
		return true;
	}
	case WM_IME_COMPOSITION:
	{
		if (lParam & GCS_RESULTSTR)
		{
			// Unicode windows receive committed IME text through WM_CHAR as well.
			// Keep the edit mutation in one path to avoid duplicate characters.
			InvalidateVisual();
		}
		return true;
	}
	case WM_KEYUP:
		OnKeyUp(this, KeyEventArgs((Keys)(wParam | 0)));
		InvalidateVisual();
		return true;
	default:
		break;
	}

	return Control::ProcessMessage(message, wParam, lParam, localX, localY);
}
