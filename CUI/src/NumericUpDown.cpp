#include "NumericUpDown.h"

#include "ButtonBase.h"
#include "TextBox.h"

#include <algorithm>
#include <cmath>
#include <cwctype>
#include <iomanip>
#include <stdexcept>
#include <sstream>
#include <typeindex>
#include <utility>

namespace
{
	template<typename TValue>
	DependencyPropertyOptions<NumericUpDown, TValue>
		NumericPropertyOptions(
			TValue defaultValue
			CUI_DESIGN_METADATA_ARGUMENTS(
				const wchar_t* category,
				int categoryOrder,
				int order,
				DependencyPropertyEditorKind editor),
			DependencyPropertyFlags flags =
				DependencyPropertyFlags::AffectsRender)
	{
		DependencyPropertyOptions<NumericUpDown, TValue> options;
		options.DefaultValue = std::move(defaultValue);
		options.Flags = flags;
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = category;
		options.Design.CategoryOrder = categoryOrder;
		options.Design.Order = order;
		options.Design.Editor = editor;
		options.Design.Persistence =
			DependencyPropertyPersistence::Metadata;
		)
		return options;
	}

	bool IsNumericEditCandidate(
		const std::wstring& text)
	{
		if (text.empty()) return true;

		std::size_t index = 0;
		if (text[index] == L'+' || text[index] == L'-')
			++index;

		bool hasDecimalPoint = false;
		for (; index < text.size(); ++index)
		{
			const wchar_t value = text[index];
			if (std::iswdigit(static_cast<wint_t>(value)))
				continue;
			if (value == L'.' && !hasDecimalPoint)
			{
				hasDecimalPoint = true;
				continue;
			}
			return false;
		}
		return true;
	}

	const DependencyPropertyMetadataRegistration&
		NumericMaximumMetadataRelation()
	{
		static const DependencyPropertyMetadataRegistration relation = []
		{
			const auto& property = RangeBase::MaximumProperty();
			DependencyPropertyOptions<NumericUpDown, double> options;
			options.DefaultValue = 100.0;
			options.Flags = DependencyPropertyFlags::AffectsRender;
			CUI_DESIGN_METADATA_ONLY(
			const std::type_index rangeOwner[] = {
				std::type_index(typeid(RangeBase))
			};
			const auto* base =
				DependencyPropertyRegistry::FindRegistered(
					rangeOwner, L"Maximum");
			if (!base)
				throw std::logic_error(
					"RangeBase.Maximum must be registered before NumericUpDown");
			options.Design = base->Design();
			)
			return DependencyPropertyRegistry::OverrideMetadataStatic<
				NumericUpDown, RangeBase, double>(
					property, std::move(options));
		}();
		return relation;
	}
}

UIClass NumericUpDown::Type()
{
	return UIClass::UI_NumericUpDown;
}

const DependencyProperty& NumericUpDown::IncrementProperty()
{
	static const auto registration = []
	{
		auto options = NumericPropertyOptions(
			1.0 CUI_DESIGN_METADATA_ARGUMENTS(
				L"Range", 100, 30, DependencyPropertyEditorKind::Number));
		options.Validate = [](const double& proposed)
		{
			return std::isfinite(proposed) && proposed >= 0.0;
		};
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Minimum = 0.0;
		options.Design.Step = 0.1;
		)
		options.Changed = [](
			NumericUpDown& target, const double&, const double&)
		{
			target.ReevaluateRangeValue();
		};
		return DependencyPropertyRegistry::RegisterStatic<
			NumericUpDown, double>(
				DependencyPropertyRegistrationLiteral(L"Increment"),
				[](NumericUpDown& target) { return target.Increment; },
				[](NumericUpDown& target, const double& value)
				{ target.Increment = value; }, {}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& NumericUpDown::DecimalPlacesProperty()
{
	static const auto registration = []
	{
		auto options = NumericPropertyOptions(
			0 CUI_DESIGN_METADATA_ARGUMENTS(
				L"Range", 100, 40, DependencyPropertyEditorKind::Number));
		options.Coerce = [](
			NumericUpDown&, const int& proposed) -> std::optional<int>
		{
			return (std::clamp)(proposed, 0, 15);
		};
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Minimum = 0.0;
		options.Design.Maximum = 15.0;
		options.Design.Step = 1.0;
		)
		options.Changed = [](
			NumericUpDown& target, const int&, const int&)
		{
			if (!target._editing) target.SyncTextFromValue();
		};
		return DependencyPropertyRegistry::RegisterStatic<
			NumericUpDown, int>(
				DependencyPropertyRegistrationLiteral(L"DecimalPlaces"),
				[](NumericUpDown& target) { return target.DecimalPlaces; },
				[](NumericUpDown& target, const int& value)
				{ target.DecimalPlaces = value; }, {}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& NumericUpDown::IsSnapToIncrementEnabledProperty()
{
	static const auto registration = []
	{
		auto options = NumericPropertyOptions(
			true CUI_DESIGN_METADATA_ARGUMENTS(
				L"Range", 100, 50, DependencyPropertyEditorKind::Boolean));
		options.Changed = [](
			NumericUpDown& target, const bool&, const bool&)
		{
			target.ReevaluateRangeValue();
		};
		return DependencyPropertyRegistry::RegisterStatic<
			NumericUpDown, bool>(
				DependencyPropertyRegistrationLiteral(
					L"IsSnapToIncrementEnabled"),
				[](NumericUpDown& target)
				{ return target.IsSnapToIncrementEnabled; },
				[](NumericUpDown& target, const bool& value)
				{ target.IsSnapToIncrementEnabled = value; }, {},
				std::move(options));
	}();
	return *registration;
}

const DependencyProperty& NumericUpDown::SelectAllOnFocusProperty()
{
	static const auto registration = []
	{
		auto options = NumericPropertyOptions(
			true CUI_DESIGN_METADATA_ARGUMENTS(
				L"Behavior", 110, 20,
				DependencyPropertyEditorKind::Boolean));
		return DependencyPropertyRegistry::RegisterStatic<
			NumericUpDown, bool>(
				DependencyPropertyRegistrationLiteral(L"SelectAllOnFocus"),
				[](NumericUpDown& target) { return target.SelectAllOnFocus; },
				[](NumericUpDown& target, const bool& value)
				{ target.SelectAllOnFocus = value; }, {}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& NumericUpDown::UseMouseWheelProperty()
{
	static const auto registration = []
	{
		auto options = NumericPropertyOptions(
			true CUI_DESIGN_METADATA_ARGUMENTS(
				L"Behavior", 110, 30,
				DependencyPropertyEditorKind::Boolean));
		return DependencyPropertyRegistry::RegisterStatic<
			NumericUpDown, bool>(
				DependencyPropertyRegistrationLiteral(L"UseMouseWheel"),
				[](NumericUpDown& target) { return target.UseMouseWheel; },
				[](NumericUpDown& target, const bool& value)
				{ target.UseMouseWheel = value; }, {}, std::move(options));
	}();
	return *registration;
}

void NumericUpDown::RegisterDependencyProperties()
{
	RangeBase::RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	(void)IncrementProperty();
	(void)DecimalPlacesProperty();
	(void)IsSnapToIncrementEnabledProperty();
	(void)SelectAllOnFocusProperty();
	(void)UseMouseWheelProperty();
#endif
	CUI_DESIGN_METADATA_ONLY(
	(void)NumericMaximumMetadataRelation();
	(void)RegisterControlBorderThicknessMetadata<
			NumericUpDown, RangeBase>(
				1.0f CUI_DESIGN_METADATA_ARGUMENTS(60));
	)
}

const DependencyPropertyMetadata*
NumericUpDown::ResolveExactDependencyPropertyMetadata(
	const DependencyProperty& property) const
{
	if (&property == &RangeBase::MaximumProperty())
		return &NumericMaximumMetadataRelation().Metadata();
	if (&property == &Control::BorderThicknessProperty())
	{
		return &RegisterControlBorderThicknessMetadata<
			NumericUpDown, RangeBase>(
				1.0f CUI_DESIGN_METADATA_ARGUMENTS(60)).Metadata();
	}
	return RangeBase::ResolveExactDependencyPropertyMetadata(property);
}

NumericUpDown::NumericUpDown()
	: RangeBase(100.0)
{
	RegisterDependencyProperties();
	SyncTextFromValue();
}

double NumericUpDown::CoerceRangeValue(
	double value) const
{
	double next = RangeBase::CoerceRangeValue(value);
	if (!_isSnapToIncrementEnabled
		|| _increment <= 0.0
		|| !std::isfinite(_increment))
		return next;

	const double steps =
		(next - MinimumCore()) / _increment;
	const double snapped = MinimumCore()
		+ std::round(steps) * _increment;
	return RangeBase::CoerceRangeValue(snapped);
}

void NumericUpDown::OnRangeValueChanged(
	double oldValue, double newValue)
{
	(void)oldValue;
	(void)newValue;
	if (!_editing)
		SyncTextFromValue();
}

GET_CPP(NumericUpDown, double, Increment)
{
	return _increment;
}

SET_CPP(NumericUpDown, double, Increment)
{
	(void)SetPropertyField(
		IncrementProperty(), _increment, value);
}

GET_CPP(NumericUpDown, int, DecimalPlaces)
{
	return _decimalPlaces;
}

SET_CPP(NumericUpDown, int, DecimalPlaces)
{
	(void)SetPropertyField(
		DecimalPlacesProperty(), _decimalPlaces, value);
}

GET_CPP(NumericUpDown, bool, IsSnapToIncrementEnabled)
{
	return _isSnapToIncrementEnabled;
}

SET_CPP(NumericUpDown, bool, IsSnapToIncrementEnabled)
{
	(void)SetPropertyField(
		IsSnapToIncrementEnabledProperty(),
		_isSnapToIncrementEnabled, value);
}

GET_CPP(NumericUpDown, bool, SelectAllOnFocus)
{
	return _selectAllOnFocus;
}

SET_CPP(NumericUpDown, bool, SelectAllOnFocus)
{
	(void)SetPropertyField(
		SelectAllOnFocusProperty(), _selectAllOnFocus, value);
}

GET_CPP(NumericUpDown, bool, UseMouseWheel)
{
	return _useMouseWheel;
}

SET_CPP(NumericUpDown, bool, UseMouseWheel)
{
	(void)SetPropertyField(
		UseMouseWheelProperty(), _useMouseWheel, value);
}

std::wstring NumericUpDown::FormatValue() const
{
	std::wstringstream stream;
	stream.setf(std::ios::fixed, std::ios::floatfield);
	stream << std::setprecision(
		(std::max)(0, _decimalPlaces))
		<< ValueCore();
	return stream.str();
}

void NumericUpDown::SyncTextFromValue()
{
	const ControlWeakReference lifetime(this);
	const std::wstring formatted = FormatValue();
	_editText = formatted;
	auto* editor = _textBoxPart;
	if (!editor || editor->Text == formatted)
		return;

	_synchronizingText = true;
	try
	{
		editor->Text = formatted;
	}
	catch (...)
	{
		if (auto* source = dynamic_cast<NumericUpDown*>(lifetime.Get()))
			source->_synchronizingText = false;
		throw;
	}
	if (auto* source = dynamic_cast<NumericUpDown*>(lifetime.Get()))
		source->_synchronizingText = false;
}

bool NumericUpDown::TryParseEditText(
	const std::wstring& text, double& value) const
{
	value = 0.0;
	if (text.empty() || text == L"-"
		|| text == L"+" || text == L"."
		|| text == L"-." || text == L"+.")
		return false;

	wchar_t* end = nullptr;
	const wchar_t* start = text.c_str();
	const double parsed = std::wcstod(start, &end);
	while (end && *end
		&& std::iswspace(static_cast<wint_t>(*end)))
		++end;
	if (end == start || (end && *end != L'\0')
		|| !std::isfinite(parsed))
		return false;

	value = parsed;
	return true;
}

bool NumericUpDown::IsEditTextAllowed(
	const std::wstring& text) const
{
	if (!IsNumericEditCandidate(text))
		return false;
	if (text.empty() || text == L"-"
		|| text == L"+" || text == L"."
		|| text == L"-." || text == L"+.")
		return true;

	double parsed = 0.0;
	if (!TryParseEditText(text, parsed))
		return true;

	const bool hasDecimalPoint =
		text.find(L'.') != std::wstring::npos;
	const bool isNegative =
		!text.empty() && text.front() == L'-';
	if (parsed > MaximumCore())
	{
		return isNegative
			&& MaximumCore() < 0.0
			&& !hasDecimalPoint;
	}
	if (parsed < MinimumCore())
	{
		return !isNegative
			&& MinimumCore() > 0.0
			&& !hasDecimalPoint;
	}
	return true;
}

void NumericUpDown::BeginEdit(bool selectAll)
{
	_editing = true;
	if (_textBoxPart && selectAll)
		_textBoxPart->SelectAll();
}

bool NumericUpDown::CommitEdit()
{
	const ControlWeakReference lifetime(this);
	const std::wstring candidate = _textBoxPart
		? _textBoxPart->Text : _editText;
	double parsed = 0.0;
	_editing = false;
	if (!TryParseEditText(candidate, parsed))
	{
		SyncTextFromValue();
		return false;
	}

	SetCurrentRangeValue(parsed);
	auto* source = dynamic_cast<NumericUpDown*>(lifetime.Get());
	if (!source) return true;
	source->SyncTextFromValue();
	source = dynamic_cast<NumericUpDown*>(lifetime.Get());
	if (!source) return true;
	if (auto* editor = source->_textBoxPart)
		editor->CaretIndex =
			static_cast<int>(editor->Text.size());
	return true;
}

void NumericUpDown::CancelEdit()
{
	const ControlWeakReference lifetime(this);
	_editing = false;
	SyncTextFromValue();
	auto* source = dynamic_cast<NumericUpDown*>(lifetime.Get());
	if (!source) return;
	if (auto* editor = source->_textBoxPart)
		editor->CaretIndex =
			static_cast<int>(editor->Text.size());
}

void NumericUpDown::StepBy(int direction)
{
	if (direction == 0) return;
	const ControlWeakReference lifetime(this);

	const bool keepEditing = _textBoxPart
		&& _textBoxPart->IsKeyboardFocused;
	if (_editing)
	{
		(void)CommitEdit();
		if (!lifetime.Get()) return;
	}

	auto* source = dynamic_cast<NumericUpDown*>(lifetime.Get());
	if (!source) return;
	const double step =
		source->_increment > 0.0 && std::isfinite(source->_increment)
		? source->_increment : 1.0;
	const double next = source->ValueCore()
		+ step * static_cast<double>(direction);
	source->SetCurrentRangeValue(next);
	source = dynamic_cast<NumericUpDown*>(lifetime.Get());
	if (!source) return;
	source->SyncTextFromValue();
	source = dynamic_cast<NumericUpDown*>(lifetime.Get());
	if (!source) return;

	if (keepEditing && source->_textBoxPart)
	{
		source->_editing = true;
		auto* editor = source->_textBoxPart;
		editor->CaretIndex =
			static_cast<int>(editor->Text.size());
	}
}

void NumericUpDown::OnEditorTextChanged(
	TextChangedEventArgs& args)
{
	if (_synchronizingText || !_textBoxPart)
		return;
	const ControlWeakReference lifetime(this);
	const ControlWeakReference editorLifetime(_textBoxPart);

	const std::wstring candidate = _textBoxPart->Text;
	if (IsEditTextAllowed(candidate))
	{
		_editText = candidate;
		_editing = true;
		return;
	}

	const int caret = _textBoxPart->CaretIndex;
	const std::wstring previous = _editText;
	_synchronizingText = true;
	try
	{
		_textBoxPart->Text = previous;
	}
	catch (...)
	{
		if (auto* source = dynamic_cast<NumericUpDown*>(lifetime.Get()))
			source->_synchronizingText = false;
		throw;
	}
	auto* source = dynamic_cast<NumericUpDown*>(lifetime.Get());
	auto* editor = dynamic_cast<TextBox*>(editorLifetime.Get());
	if (source)
	{
		source->_synchronizingText = false;
		if (editor && source->_textBoxPart == editor)
			editor->CaretIndex = (std::clamp)(
				caret - 1, 0,
				static_cast<int>(previous.size()));
	}
	args.Handled = true;
}

void NumericUpDown::OnEditorKeyDown(
	KeyEventArgs& args)
{
	if (args.HasModifier(ModifierKeys::Control)
		|| args.HasModifier(ModifierKeys::Alt))
		return;

	switch (args.Key)
	{
	case Key::Up:
		StepBy(1);
		break;
	case Key::Down:
		StepBy(-1);
		break;
	case Key::PageUp:
		StepBy(10);
		break;
	case Key::PageDown:
		StepBy(-10);
		break;
	case Key::Return:
	{
		const ControlWeakReference lifetime(this);
		(void)CommitEdit();
		if (auto* source = dynamic_cast<NumericUpDown*>(lifetime.Get()))
			source->BeginEdit(false);
		break;
	}
	case Key::Escape:
	{
		const ControlWeakReference lifetime(this);
		CancelEdit();
		if (auto* source = dynamic_cast<NumericUpDown*>(lifetime.Get()))
			source->BeginEdit(true);
		break;
	}
	default:
		return;
	}
	args.Handled = true;
}

void NumericUpDown::OnEditorMouseWheel(
	MouseEventArgs& args)
{
	if (!_useMouseWheel || args.WheelDelta == 0)
		return;
	StepBy(args.WheelDelta > 0 ? 1 : -1);
	args.Handled = true;
}

bool NumericUpDown::ApplyTextInput(
	const TextCompositionEventArgs& input)
{
	if (!_textBoxPart || input.Text.empty())
		return false;
	BeginEdit(false);
	_textBoxPart->InsertText(input.Text);
	return true;
}

bool NumericUpDown::TryGetTextInputCaretRect(
	D2D1_RECT_F& outRect)
{
	(void)outRect;
	// The focused TextBox template part owns IME/caret geometry.
	return false;
}

int NumericUpDown::HitTestSpinButton(
	int localX, int localY) const noexcept
{
	const auto size = GetActualSizeDip();
	if (localX < 0 || localY < 0
		|| static_cast<float>(localX) >= size.width
		|| static_cast<float>(localY) >= size.height)
	{
		return 0;
	}

	// This is the same compact hit geometry as the framework template's
	// 28-DIP spin column. It also keeps an untemplated control predictable
	// during construction and directed input dispatch.
	constexpr float spinButtonWidth = 28.0f;
	const float spinLeft = (std::max)(
		0.0f, size.width - spinButtonWidth);
	if (static_cast<float>(localX) < spinLeft)
		return 0;
	return static_cast<float>(localY) < size.height * 0.5f
		? 1 : -1;
}

CursorKind NumericUpDown::QueryCursor(
	int localX, int localY)
{
	if (!IsEffectivelyEnabled()) return CursorKind::Arrow;
	if (_capturedSpinDirection != 0 && IsMouseCaptured())
		return CursorKind::Hand;
	return HitTestSpinButton(localX, localY) == 0
		? CursorKind::IBeam : CursorKind::Hand;
}

bool NumericUpDown::CanHandleMouseWheel(
	int delta, int localX, int localY)
{
	(void)localX;
	(void)localY;
	return _useMouseWheel && delta != 0
		&& IsEffectivelyEnabled();
}

bool NumericUpDown::HandlesNavigationKey(
	Key key) const
{
	return key == Key::Left || key == Key::Right
		|| key == Key::Up || key == Key::Down
		|| key == Key::Home || key == Key::End
		|| key == Key::PageUp
		|| key == Key::PageDown;
}

bool NumericUpDown::ProcessInput(
	const InputReport& input)
{
	if (!IsEffectivelyEnabled() || !IsVisible)
		return true;
	const ControlWeakReference lifetime(this);

	bool handled = false;
	if (input.Kind == InputReportKind::MouseWheel
		&& _useMouseWheel && input.WheelDelta != 0)
	{
		handled = true;
		StepBy(input.WheelDelta > 0 ? 1 : -1);
		if (!lifetime.Get()) return true;
	}
	else if (input.Kind == InputReportKind::KeyDown)
	{
		KeyEventArgs args(input.Key, input.Modifiers);
		OnEditorKeyDown(args);
		handled = args.Handled;
		if (!lifetime.Get()) return true;
	}
	else if (input.Kind == InputReportKind::PointerDown
		&& input.ChangedButton == MouseButton::Left)
	{
		const int direction = HitTestSpinButton(input.X, input.Y);
		if (direction != 0)
		{
			handled = true;
			_capturedSpinDirection = direction;
			const bool captured = CaptureMouse();
			auto* source = dynamic_cast<NumericUpDown*>(lifetime.Get());
			if (!source) return true;
			source->StepBy(direction);
			source = dynamic_cast<NumericUpDown*>(lifetime.Get());
			if (!source) return true;
			if (!captured) source->_capturedSpinDirection = 0;
		}
	}
	else if (input.Kind == InputReportKind::PointerUp
		&& input.ChangedButton == MouseButton::Left
		&& _capturedSpinDirection != 0)
	{
		handled = true;
		_capturedSpinDirection = 0;
		if (IsMouseCaptured())
			(void)ReleaseMouseCapture();
		if (!lifetime.Get()) return true;
	}
	else if (input.Kind == InputReportKind::Cancel
		|| input.Kind == InputReportKind::CaptureLost)
	{
		_capturedSpinDirection = 0;
	}

	if (input.Kind == InputReportKind::KeyDown)
	{
		auto args = input.CreateKeyEventArgs();
		auto* source = dynamic_cast<NumericUpDown*>(lifetime.Get());
		if (!source) return true;
		source->OnKeyDown(source, args);
		return handled || args.Handled;
	}

	auto* source = dynamic_cast<NumericUpDown*>(lifetime.Get());
	if (!source) return handled;
	const bool routed = source->Control::ProcessInput(input);
	return handled || routed;
}

void NumericUpDown::OnControlTemplatePresentationChanged()
{
	const ControlWeakReference lifetime(this);
	ClearTemplatePartEventConnections();
	_textBoxPart = nullptr;
	_increaseButtonPart = nullptr;
	_decreaseButtonPart = nullptr;
	RangeBase::OnControlTemplatePresentationChanged();

	auto* source =
		dynamic_cast<NumericUpDown*>(lifetime.Get());
	if (!source) return;

	source->_textBoxPart = dynamic_cast<TextBox*>(
		source->FindDeclarativeTemplatePart(
			MakeTemplatePartToken(L"PART_TextBox")));
	source->_increaseButtonPart =
		dynamic_cast<ButtonBase*>(
			source->FindDeclarativeTemplatePart(
				MakeTemplatePartToken(L"PART_IncreaseButton")));
	source->_decreaseButtonPart =
		dynamic_cast<ButtonBase*>(
			source->FindDeclarativeTemplatePart(
				MakeTemplatePartToken(L"PART_DecreaseButton")));

	if (source->_increaseButtonPart)
	{
		source->RetainTemplatePartEventConnection(
			source->_increaseButtonPart->Click.Subscribe(
				[lifetime](
					Control* sender, RoutedEventArgs& args)
				{
					auto* owner =
						dynamic_cast<NumericUpDown*>(
							lifetime.Get());
					if (!owner
						|| sender
							!= owner->_increaseButtonPart)
						return;
					owner->StepBy(1);
					args.Handled = true;
				}));
	}
	if (source->_decreaseButtonPart)
	{
		source->RetainTemplatePartEventConnection(
			source->_decreaseButtonPart->Click.Subscribe(
				[lifetime](
					Control* sender, RoutedEventArgs& args)
				{
					auto* owner =
						dynamic_cast<NumericUpDown*>(
							lifetime.Get());
					if (!owner
						|| sender
							!= owner->_decreaseButtonPart)
						return;
					owner->StepBy(-1);
					args.Handled = true;
				}));
	}
	if (source->_textBoxPart)
	{
		auto* editor = source->_textBoxPart;
		source->RetainTemplatePartEventConnection(
			editor->OnTextChanged.Subscribe(
				[lifetime](
					Control*, TextChangedEventArgs& args)
				{
					auto* owner =
						dynamic_cast<NumericUpDown*>(
							lifetime.Get());
					if (owner)
						owner->OnEditorTextChanged(args);
				}));
		source->RetainTemplatePartEventConnection(
			editor->OnGotKeyboardFocus.Subscribe(
				[lifetime](
					Control*,
					KeyboardFocusChangedEventArgs&)
				{
					auto* owner =
						dynamic_cast<NumericUpDown*>(
							lifetime.Get());
					if (owner)
						owner->BeginEdit(
							owner->_selectAllOnFocus);
				}));
		source->RetainTemplatePartEventConnection(
			editor->OnLostKeyboardFocus.Subscribe(
				[lifetime](
					Control*,
					KeyboardFocusChangedEventArgs&)
				{
					auto* owner =
						dynamic_cast<NumericUpDown*>(
							lifetime.Get());
					if (owner)
						(void)owner->CommitEdit();
				}));
		source->RetainTemplatePartEventConnection(
			editor->OnPreviewKeyDown.Subscribe(
				[lifetime](
					Control*, KeyEventArgs& args)
				{
					auto* owner =
						dynamic_cast<NumericUpDown*>(
							lifetime.Get());
					if (owner)
						owner->OnEditorKeyDown(args);
				}));
		source->RetainTemplatePartEventConnection(
			editor->OnPreviewMouseWheel.Subscribe(
				[lifetime](
					Control*, MouseEventArgs& args)
				{
					auto* owner =
						dynamic_cast<NumericUpDown*>(
							lifetime.Get());
					if (owner)
						owner->OnEditorMouseWheel(args);
				}));
	}
	source->SyncTextFromValue();
}
