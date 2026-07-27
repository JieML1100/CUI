#define NOMINMAX
#include "NumericUpDown.h"
#include "Window.h"
#include "TextEditCore.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cwctype>
#include <iomanip>
#include <sstream>
#include <utility>


namespace
{
	template<typename TValue>
	DependencyPropertyOptions<NumericUpDown, TValue> NumericPropertyOptions(
		TValue defaultValue,
		const wchar_t* category,
		int categoryOrder,
		int order,
		DependencyPropertyEditorKind editor,
		DependencyPropertyFlags flags = DependencyPropertyFlags::AffectsRender)
	{
		DependencyPropertyOptions<NumericUpDown, TValue> options;
		options.DefaultValue = std::move(defaultValue);
		options.Flags = flags;
		options.Design.Category = category;
		options.Design.CategoryOrder = categoryOrder;
		options.Design.Order = order;
		options.Design.Editor = editor;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		return options;
	}

	auto NumericPropertySubscriber(const wchar_t* propertyName)
	{
		return [propertyName = std::wstring(propertyName)](
			NumericUpDown& target,
			DependencyPropertyMetadata::ChangeHandler handler,
			DataSourceUpdateMode)
		{
			return target.OnPropertyValueChanged.Subscribe(
				[propertyName, handler = std::move(handler)](
					DependencyObject*, const DependencyPropertyChangedEventArgs& args)
				{
					if (args.PropertyName == propertyName)
						handler();
				});
		};
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

void NumericUpDown::RegisterDependencyProperties()
{
	RangeBase::RegisterDependencyProperties();
	static const bool registered = []
	{
		auto stepOptions = NumericPropertyOptions(
			1.0, L"Range", 100, 30, DependencyPropertyEditorKind::Number);
		stepOptions.Validate = [](const double& proposed)
		{
			return std::isfinite(proposed);
		};
		stepOptions.Coerce = [](
			NumericUpDown&, const double& proposed) -> std::optional<double>
		{
			return (std::max)(0.0, proposed);
		};
		stepOptions.Design.Minimum = 0.0;
		stepOptions.Design.Step = 0.1;
		DependencyPropertyRegistry::Register<NumericUpDown, double>(L"Increment",
			[](NumericUpDown& target) { return target.Increment; },
			[](NumericUpDown& target, const double& value)
			{ target.Increment = value; },
			NumericPropertySubscriber(L"Increment"), std::move(stepOptions));

		auto decimalOptions = NumericPropertyOptions(
			0, L"Range", 100, 40, DependencyPropertyEditorKind::Number);
		decimalOptions.Coerce = [](
			NumericUpDown&, const int& proposed) -> std::optional<int>
		{
			return (std::clamp)(proposed, 0, 15);
		};
		decimalOptions.Design.Minimum = 0.0;
		decimalOptions.Design.Maximum = 15.0;
		decimalOptions.Design.Step = 1.0;
		DependencyPropertyRegistry::Register<NumericUpDown, int>(L"DecimalPlaces",
			[](NumericUpDown& target) { return target.DecimalPlaces; },
			[](NumericUpDown& target, const int& value) { target.DecimalPlaces = value; },
			NumericPropertySubscriber(L"DecimalPlaces"), std::move(decimalOptions));

		auto snapOptions = NumericPropertyOptions(
			true, L"Range", 100, 50, DependencyPropertyEditorKind::Boolean);
		DependencyPropertyRegistry::Register<NumericUpDown, bool>(
			L"IsSnapToIncrementEnabled",
			[](NumericUpDown& target)
			{ return target.IsSnapToIncrementEnabled; },
			[](NumericUpDown& target, const bool& value)
			{ target.IsSnapToIncrementEnabled = value; },
			NumericPropertySubscriber(L"IsSnapToIncrementEnabled"),
			std::move(snapOptions));

		auto selectAllOptions = NumericPropertyOptions(
			true, L"Behavior", 110, 20, DependencyPropertyEditorKind::Boolean);
		DependencyPropertyRegistry::Register<NumericUpDown, bool>(L"SelectAllOnFocus",
			[](NumericUpDown& target) { return target.SelectAllOnFocus; },
			[](NumericUpDown& target, const bool& value) { target.SelectAllOnFocus = value; },
			NumericPropertySubscriber(L"SelectAllOnFocus"), std::move(selectAllOptions));

		auto wheelOptions = NumericPropertyOptions(
			true, L"Behavior", 110, 30, DependencyPropertyEditorKind::Boolean);
		DependencyPropertyRegistry::Register<NumericUpDown, bool>(L"UseMouseWheel",
			[](NumericUpDown& target) { return target.UseMouseWheel; },
			[](NumericUpDown& target, const bool& value) { target.UseMouseWheel = value; },
			NumericPropertySubscriber(L"UseMouseWheel"), std::move(wheelOptions));
		RegisterControlBorderThicknessMetadata<NumericUpDown>(1.0f, 60);
		return true;
	}();
	(void)registered;
}

NumericUpDown::NumericUpDown()
{
	RegisterDependencyProperties();
	this->RendererBackgroundColor = cui::theme::palette::Surface;
	this->RendererBorderColor = cui::theme::palette::BorderStrong;
	this->RendererForegroundColor = cui::theme::palette::TextPrimary;
	(void)TrySetPropertyValue(
		L"Padding", BindingValue(Thickness{ 8.0f, 0.0f, 8.0f, 0.0f }),
		DependencyPropertyValueSource::Theme);
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

double NumericUpDown::CoerceRangeValue(double value) const
{
	double v = RangeBase::CoerceRangeValue(value);
	if (!_isSnapToIncrementEnabled || _increment <= 0.0
		|| !std::isfinite(_increment))
		return v;
	double steps = (v - MinimumCore()) / _increment;
	double snapped = MinimumCore() + std::round(steps) * _increment;
	return RangeBase::CoerceRangeValue(snapped);
}

void NumericUpDown::OnRangeValueChanged(double, double)
{
	if (!_editing) SyncTextFromValue();
}

GET_CPP(NumericUpDown, double, Increment) { return _increment; }
SET_CPP(NumericUpDown, double, Increment)
{
	if (!SetPropertyField(L"Increment", _increment, value)) return;
	ReevaluateRangeValue();
}

GET_CPP(NumericUpDown, int, DecimalPlaces) { return _decimalPlaces; }
SET_CPP(NumericUpDown, int, DecimalPlaces)
{
	if (!SetPropertyField(L"DecimalPlaces", _decimalPlaces, value)) return;
	if (!_editing) SyncTextFromValue();
}

GET_CPP(NumericUpDown, bool, IsSnapToIncrementEnabled)
{
	return _isSnapToIncrementEnabled;
}
SET_CPP(NumericUpDown, bool, IsSnapToIncrementEnabled)
{
	if (!SetPropertyField(L"IsSnapToIncrementEnabled",
		_isSnapToIncrementEnabled, value)) return;
	ReevaluateRangeValue();
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

void NumericUpDown::SyncTextFromValue()
{
	_editText = FormatValue();
	_selectionStart = _selectionEnd = (std::clamp)(_selectionEnd, 0, static_cast<int>(_editText.size()));
	_horizontalScrollOffset = 0.0f;
	undoStack.clear();
	redoStack.clear();
}

std::wstring NumericUpDown::FormatValue() const
{
	std::wstringstream stream;
	stream.setf(std::ios::fixed, std::ios::floatfield);
	stream << std::setprecision((std::max)(0, _decimalPlaces)) << ValueCore();
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
	if (parsed > MaximumCore())
	{
		// In an all-negative range, "-1" is still a useful prefix for "-10".
		if (isNegative && MaximumCore() < 0.0 && !hasDecimalPoint)
			return true;
		return false;
	}
	if (parsed < MinimumCore())
	{
		// In a positive range, "1" is still a useful prefix for "10".
		if (!isNegative && MinimumCore() > 0.0 && !hasDecimalPoint)
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
		_selectionStart = _selectionEnd = (std::clamp)(_selectionEnd, 0, static_cast<int>(_editText.size()));
	}

	if (selectAll)
		SelectAllText();
	else
	{
		_selectionStart = (std::clamp)(_selectionStart, 0, static_cast<int>(_editText.size()));
		_selectionEnd = (std::clamp)(_selectionEnd, 0, static_cast<int>(_editText.size()));
	}

	UpdateScroll(selectAll);
	InvalidateVisual();
}

bool NumericUpDown::CommitEdit()
{
	if (!_editing)
		return true;

	double parsed = 0.0;
	if (!TryParseEditText(_editText, parsed))
	{
		_editing = false;
		SyncTextFromValue();
		_selectionStart = _selectionEnd = static_cast<int>(_editText.size());
		InvalidateVisual();
		return false;
	}

	_editing = false;
	SetCurrentRangeValue(parsed);
	SyncTextFromValue();
	_selectionStart = _selectionEnd = static_cast<int>(_editText.size());
	InvalidateVisual();
	return true;
}

void NumericUpDown::CancelEdit()
{
	_editing = false;
	SyncTextFromValue();
	_selectionStart = _selectionEnd = static_cast<int>(_editText.size());
	InvalidateVisual();
}

void NumericUpDown::SelectAllText()
{
	_selectionStart = 0;
	_selectionEnd = static_cast<int>(_editText.size());
	_horizontalScrollOffset = 0.0f;
}

void NumericUpDown::InputText(std::wstring input)
{
	std::wstring oldText = _editText;
	std::wstring newText = _editText;
	int newSelectionStart = _selectionStart;
	int newSelectionEnd = _selectionEnd;
	const int selStartBefore = _selectionStart;
	const int selEndBefore = _selectionEnd;

	auto result = CuiTextEdit::ReplaceSelection(newText, newSelectionStart, newSelectionEnd, input, NumericEditOptions());
	if (!result.applied || !IsEditTextAllowed(newText))
		return;

	_selectionStart = newSelectionStart;
	_selectionEnd = newSelectionEnd;
	if (result.textChanged && !isApplyingUndoRedo)
	{
		UndoRecord rec;
		rec.pos = result.replaceStart;
		rec.removedText = result.removedText;
		rec.insertedText = result.insertedText;
		rec.selStartBefore = selStartBefore;
		rec.selEndBefore = selEndBefore;
		rec.selStartAfter = _selectionStart;
		rec.selEndAfter = _selectionEnd;
		undoStack.push_back(rec);
		redoStack.clear();
	}
	if (oldText != newText) _editText = std::move(newText);
}

void NumericUpDown::InputBack()
{
	std::wstring oldText = _editText;
	std::wstring newText = _editText;
	int newSelectionStart = _selectionStart;
	int newSelectionEnd = _selectionEnd;
	const int selStartBefore = _selectionStart;
	const int selEndBefore = _selectionEnd;

	auto result = CuiTextEdit::Backspace(newText, newSelectionStart, newSelectionEnd, NumericEditOptions());
	if (!result.applied || !IsEditTextAllowed(newText))
		return;

	_selectionStart = newSelectionStart;
	_selectionEnd = newSelectionEnd;
	if (result.textChanged && !isApplyingUndoRedo)
	{
		UndoRecord rec;
		rec.pos = result.replaceStart;
		rec.removedText = result.removedText;
		rec.insertedText = L"";
		rec.selStartBefore = selStartBefore;
		rec.selEndBefore = selEndBefore;
		rec.selStartAfter = _selectionStart;
		rec.selEndAfter = _selectionEnd;
		undoStack.push_back(rec);
		redoStack.clear();
	}
	if (oldText != newText) _editText = std::move(newText);
}

void NumericUpDown::InputDelete()
{
	std::wstring oldText = _editText;
	std::wstring newText = _editText;
	int newSelectionStart = _selectionStart;
	int newSelectionEnd = _selectionEnd;
	const int selStartBefore = _selectionStart;
	const int selEndBefore = _selectionEnd;

	auto result = CuiTextEdit::DeleteForward(newText, newSelectionStart, newSelectionEnd, NumericEditOptions());
	if (!result.applied || !IsEditTextAllowed(newText))
		return;

	_selectionStart = newSelectionStart;
	_selectionEnd = newSelectionEnd;
	if (result.textChanged && !isApplyingUndoRedo)
	{
		UndoRecord rec;
		rec.pos = result.replaceStart;
		rec.removedText = result.removedText;
		rec.insertedText = L"";
		rec.selStartBefore = selStartBefore;
		rec.selEndBefore = selEndBefore;
		rec.selStartAfter = _selectionStart;
		rec.selEndAfter = _selectionEnd;
		undoStack.push_back(rec);
		redoStack.clear();
	}
	if (oldText != newText) _editText = std::move(newText);
}

void NumericUpDown::ApplyUndoRecord(const UndoRecord& rec, bool isUndo)
{
	std::wstring oldText = _editText;
	std::wstring newText = _editText;
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
		_selectionStart = rec.selStartBefore;
		_selectionEnd = rec.selEndBefore;
	}
	else
	{
		_selectionStart = rec.selStartAfter;
		_selectionEnd = rec.selEndAfter;
	}
	_selectionStart = (std::clamp)(_selectionStart, 0, static_cast<int>(newText.size()));
	_selectionEnd = (std::clamp)(_selectionEnd, 0, static_cast<int>(newText.size()));

	isApplyingUndoRedo = false;
	if (oldText != newText) _editText = std::move(newText);
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
	auto span = CuiTextEdit::NormalizeSelection(_selectionStart, _selectionEnd, _editText.size());
	if (!span.HasSelection())
		return L"";
	return _editText.substr(static_cast<size_t>(span.start), static_cast<size_t>(span.Length()));
}

void NumericUpDown::UpdateScroll(bool arrival)
{
	(void)arrival;
	auto font = this->GetRenderFont();
	if (!font)
		return;

	auto textRect = TextRect();
	const float renderWidth = (std::max)(1.0f, RectWidth(textRect));
	_selectionStart = (std::clamp)(_selectionStart, 0, static_cast<int>(_editText.size()));
	_selectionEnd = (std::clamp)(_selectionEnd, 0, static_cast<int>(_editText.size()));

	float caretLeft = 0.0f;
	float caretRight = 0.0f;
	if (!_editText.empty())
	{
		auto hit = font->HitTestTextRange(_editText, static_cast<UINT32>(_selectionEnd), 0);
		if (!hit.empty())
		{
			caretLeft = hit[0].left;
			caretRight = hit[0].left + hit[0].width;
		}
		else
		{
			caretLeft = font->GetTextSize(_editText).width;
			caretRight = caretLeft;
		}
	}

	if (caretRight - _horizontalScrollOffset > renderWidth - 2.0f)
		_horizontalScrollOffset = caretRight - renderWidth + 2.0f;
	if (caretLeft - _horizontalScrollOffset < 0.0f)
		_horizontalScrollOffset = caretLeft;
	if (_horizontalScrollOffset < 0.0f)
		_horizontalScrollOffset = 0.0f;
}

bool NumericUpDown::TryGetTextInputCaretRect(D2D1_RECT_F& outRect)
{
	if (!GetPresentationWindow())
		return false;

	if (_caretRectCacheValid)
	{
		outRect = _caretRectCache;
	}
	else
	{
		const auto absoluteLocation = this->GetAbsoluteLocationDip();
		auto textRect = TextRect();
		float caretX = static_cast<float>(absoluteLocation.x) + textRect.left - _horizontalScrollOffset;
		float caretY = static_cast<float>(absoluteLocation.y) + TextTop(this->GetRenderFont(), textRect);
		float caretH = (this->GetRenderFont() && this->GetRenderFont()->FontHeight > 0.0f) ? this->GetRenderFont()->FontHeight : 16.0f;
		outRect = D2D1_RECT_F{ caretX, caretY, caretX + 1.0f, caretY + caretH };
	}
	return true;
}

bool NumericUpDown::ApplyTextInput(const TextCompositionEventArgs& input)
{
	if (input.Text.empty()) return false;
	if (!_editing) BeginEdit(SelectAllOnFocus);
	const std::wstring previous = _editText;
	InputText(input.Text);
	UpdateScroll();
	InvalidateVisual();
	return _editText != previous;
}

D2D1_RECT_F NumericUpDown::ButtonPanelRect() const
{
	const auto actualSize = GetActualSizeDip();
	const float w = actualSize.width;
	const float h = actualSize.height;
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

D2D1_RECT_F NumericUpDown::TextRect()
{
	const float h = GetActualSizeDip().height;
	auto buttons = ButtonPanelRect();
	return D2D1::RectF(
		Padding.Left, Padding.Top,
		(std::max)(Padding.Left, buttons.left - Padding.Right),
		(std::max)(Padding.Top, h - Padding.Bottom));
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
	auto font = this->GetRenderFont();
	if (!font)
		return static_cast<int>(_editText.size());

	auto textRect = TextRect();
	const float x = ((float)localX - textRect.left) + _horizontalScrollOffset;
	const float y = (float)localY - TextTop(font, textRect);
	return font->HitTestTextPosition(_editText, FLT_MAX, (std::max)(1.0f, RectHeight(textRect)), x, y);
}

void NumericUpDown::StepBy(int direction, bool accelerated)
{
	if (direction == 0)
		return;

	const bool keepEditing = GetPresentationWindow() && GetPresentationWindow()->GetKeyboardFocusedElement() == this;
	if (_editing)
		CommitEdit();

	double delta = Increment > 0.0 && std::isfinite(Increment)
		? Increment : 1.0;
	if (accelerated)
		delta *= 10.0;
	SetCurrentRangeValue(Value + delta * (double)direction);

	if (keepEditing)
	{
		_editing = true;
		_selectionStart = _selectionEnd = static_cast<int>(_editText.size());
		UpdateScroll(true);
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
	if (!IsEnabled)
		return CursorKind::Arrow;
	if (_dragUp || _dragDown)
		return CursorKind::Hand;
	return HitTestButton(localX, localY) == 0 ? CursorKind::IBeam : CursorKind::Hand;
}

bool NumericUpDown::CanHandleMouseWheel(int delta, int localX, int localY)
{
	(void)localX;
	(void)localY;
	if (!UseMouseWheel || delta == 0 || !IsEnabled)
		return false;
	return true;
}

bool NumericUpDown::HandlesNavigationKey(Key key) const
{
	return key == Key::Left || key == Key::Right
		|| key == Key::Up || key == Key::Down
		|| key == Key::Home || key == Key::End
		|| key == Key::PageUp || key == Key::PageDown;
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
	outRect = GetAbsoluteBoundsDip();
	return true;
}

void NumericUpDown::OnRender()
{
	if (!this->IsVisible) return;
	auto d2d = this->GetDrawingContext();
	if (!d2d) return;

	const float hoverProgress = CurrentHoverProgress();
	const bool isSelected = GetPresentationWindow() && GetPresentationWindow()->GetKeyboardFocusedElement() == this;
	const bool isUnderMouse = IsMouseOver;
	if (!isUnderMouse && !_dragUp && !_dragDown && _hoverButton != 0)
	{
		_hoverButton = 0;
		StartHoverAnimation(0.0f);
	}

	const auto size = this->GetActualSizeDip();
	const float width = size.width;
	const float height = size.height;
	const float radius = (std::clamp)(_cornerRadius, 0.0f,
		(std::min)(width, height) * 0.5f);
	const float border = BorderThickness.MaxEdge();
	auto panelRect = ButtonPanelRect();
	auto upRect = UpButtonRect();
	auto downRect = DownButtonRect();
	auto textRect = TextRect();
	class Font* fontObj = this->GetRenderFont();
	std::wstring text = _editText;
	const float renderHeight = (std::max)(1.0f, RectHeight(textRect));
	_textSize = fontObj ? fontObj->GetTextSize(text, FLT_MAX, renderHeight) : D2D1_SIZE_F{ 0,0 };
	const float textY = TextTop(fontObj, textRect);

	this->_caretRectCacheValid = false;
	bool shouldDrawCaret = false;
	D2D1_POINT_2F caretStart{};
	D2D1_POINT_2F caretEnd{};

	this->BeginRender();
	if (GetControlTemplateRoot())
	{
		this->EndRender();
		return;
	}
	{
		d2d->FillRoundRect(0.0f, 0.0f, width, height, RendererBackgroundColor, radius);
		if (isUnderMouse && !_editing)
			d2d->FillRoundRect(1.0f, 1.0f, (std::max)(0.0f, width - 2.0f), (std::max)(0.0f, height - 2.0f),
				ScaleAlpha(_buttonHoverColor, 0.45f), (std::max)(0.0f, radius - 1.0f));

		if (fontObj)
		{
			d2d->PushDrawRect(textRect.left, textRect.top, (std::max)(1.0f, RectWidth(textRect)), RectHeight(textRect));
			const int sels = (std::min)(_selectionStart, _selectionEnd);
			const int sele = (std::max)(_selectionStart, _selectionEnd);
			const int selLen = sele - sels;

			if (isSelected && selLen > 0 && !text.empty())
			{
				auto selRange = fontObj->HitTestTextRange(text, static_cast<UINT32>(sels), static_cast<UINT32>(selLen));
				for (auto sr : selRange)
				{
					d2d->FillRect(
						textRect.left + sr.left - _horizontalScrollOffset,
						textY + sr.top,
						sr.width,
						sr.height,
						_selectedBackColor);
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
							textRect.left - _horizontalScrollOffset,
							textY,
							this->RendererForegroundColor,
							DWRITE_TEXT_RANGE{ static_cast<UINT32>(sels), static_cast<UINT32>(selLen) },
							_selectedForeColor,
							fontObj);
					}
					else
					{
						d2d->DrawStringLayout(textLayout,
							textRect.left - _horizontalScrollOffset,
							textY,
							this->RendererForegroundColor);
					}
					textLayout->Release();
				}
			}

			if (isSelected && selLen == 0)
			{
				float caretX = textRect.left - _horizontalScrollOffset;
				float caretTop = textY + 1.0f;
				float caretBottom = textY + (fontObj ? fontObj->FontHeight : 16.0f) - 1.0f;
				if (!text.empty())
				{
					auto caretRange = fontObj->HitTestTextRange(text, static_cast<UINT32>(_selectionEnd), 0);
					if (!caretRange.empty())
					{
						caretX = textRect.left + caretRange[0].left - _horizontalScrollOffset;
						caretTop = textY + caretRange[0].top + 1.0f;
						caretBottom = textY + caretRange[0].top + (caretRange[0].height > 0.0f ? caretRange[0].height : fontObj->FontHeight) - 1.0f;
					}
					else
					{
						caretX = textRect.left + fontObj->GetTextSize(text).width - _horizontalScrollOffset;
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

			UpdateCaretBlinkState(isSelected, _selectionStart, _selectionEnd, this->_caretRectCacheValid, this->_caretRectCacheValid ? &this->_caretRectCache : nullptr);
			if (shouldDrawCaret && IsCaretBlinkVisible())
				d2d->DrawLine(caretStart, caretEnd,
					cui::theme::palette::Accent, 1.2f);
			d2d->PopDrawRect();
		}
		else
		{
			UpdateCaretBlinkState(false, 0, 0, false);
		}

		d2d->FillRoundRect(panelRect.left, panelRect.top + 3.0f,
			RectWidth(panelRect) - 3.0f,
			(std::max)(0.0f, RectHeight(panelRect) - 6.0f),
			_buttonBackColor, 4.0f);
		if (_hoverButton != 0 && hoverProgress > 0.001f)
		{
			auto rect = _hoverButton > 0 ? upRect : downRect;
			auto color = (_dragUp && _hoverButton > 0) || (_dragDown && _hoverButton < 0)
				? _buttonPressedColor
				: ScaleAlpha(_buttonHoverColor, hoverProgress);
			d2d->FillRoundRect(rect.left + 2.0f, rect.top + 2.0f,
				(std::max)(0.0f, RectWidth(rect) - 4.0f), (std::max)(0.0f, RectHeight(rect) - 4.0f), color, 3.5f);
		}
		d2d->DrawLine(panelRect.left, 5.0f, panelRect.left, (std::max)(5.0f, height - 5.0f), ScaleAlpha(RendererBorderColor, 0.65f), 1.0f);
		d2d->DrawLine(panelRect.left + 3.0f, height * 0.5f, width - 4.0f, height * 0.5f, ScaleAlpha(RendererBorderColor, 0.52f), 1.0f);
		DrawSpinArrow(d2d, upRect, true,
			_hoverButton > 0 ? RendererForegroundColor : _mutedTextColor);
		DrawSpinArrow(d2d, downRect, false,
			_hoverButton < 0 ? RendererForegroundColor : _mutedTextColor);

		D2D1_COLOR_F borderColor = isSelected
			? cui::theme::palette::Accent : RendererBorderColor;
		float borderWidth = isSelected
			? (std::max)(border, _focusBorder) : border;
		if (borderWidth > 0.0f && borderColor.a > 0.0f)
			d2d->DrawRoundRect(borderWidth * 0.5f, borderWidth * 0.5f,
				(std::max)(0.0f, width - borderWidth), (std::max)(0.0f, height - borderWidth),
				borderColor, borderWidth, radius);



		if (!IsEnabled)
			d2d->FillRoundRect(0.0f, 0.0f, width, height,
				_disabledOverlayColor, radius);
	}
	this->EndRender();
}

bool NumericUpDown::ProcessInput(const InputReport& input)
{
	if (!this->IsEnabled || !this->IsVisible) return true;

	switch (input.Kind)
	{
	case InputReportKind::MouseWheel:
		if (UseMouseWheel)
			StepBy(input.WheelDelta > 0 ? 1 : -1, input.HasModifier(ModifierKeys::Shift));
		{
			auto args = input.CreateMouseEventArgs();
			OnMouseWheel(this, args);
		}
		return true;
	case InputReportKind::PointerMove:
	{
		if (_dragText && GetPresentationWindow() && GetPresentationWindow()->GetKeyboardFocusedElement() == this)
		{
			_selectionEnd = HitTestTextPosition(input.X, input.Y);
			UpdateScroll();
			InvalidateVisual();
		}
		else
		{
			int hit = HitTestButton(input.X, input.Y);
			if (hit != _hoverButton)
			{
				_hoverButton = hit;
				StartHoverAnimation(hit == 0 ? 0.0f : 1.0f);
			}
		}
		{
			auto args = input.CreateMouseEventArgs();
			OnMouseMove(this, args);
		}
		return true;
	}
	case InputReportKind::PointerDown:
	{
		if (input.ChangedButton != MouseButton::Left)
			return Control::ProcessInput(input);
		(void)CaptureMouse();
		if (GetPresentationWindow()) GetPresentationWindow()->SetKeyboardFocus(this, false);
		int hit = HitTestButton(input.X, input.Y);
		if (hit != 0)
		{
			_dragText = false;
			_dragUp = hit > 0;
			_dragDown = hit < 0;
			_hoverButton = hit;
			StartHoverAnimation(1.0f);
			StepBy(hit, input.HasModifier(ModifierKeys::Shift));
		}
		else
		{
			BeginEdit(false);
			_dragText = true;
			_selectionStart = _selectionEnd = HitTestTextPosition(input.X, input.Y);
			UpdateScroll();
		}
		{
			auto args = input.CreateMouseEventArgs();
			OnMouseDown(this, args);
		}
		InvalidateVisual();
		return true;
	}
	case InputReportKind::PointerUp:
	{
		if (input.ChangedButton != MouseButton::Left)
			return Control::ProcessInput(input);
		if (_dragText && GetPresentationWindow() && GetPresentationWindow()->GetKeyboardFocusedElement() == this)
		{
			_selectionEnd = HitTestTextPosition(input.X, input.Y);
			UpdateScroll();
		}
		_dragText = false;
		_dragUp = false;
		_dragDown = false;
		int hit = HitTestButton(input.X, input.Y);
		if (hit != _hoverButton)
		{
			_hoverButton = hit;
			StartHoverAnimation(hit == 0 ? 0.0f : 1.0f);
		}
		auto e = input.CreateMouseEventArgs();
		OnMouseUp(this, e);
		if (IsMouseCaptured()) (void)ReleaseMouseCapture();
		InvalidateVisual();
		return true;
	}
	case InputReportKind::Cancel:
	case InputReportKind::CaptureLost:
		_dragText = false;
		_dragUp = false;
		_dragDown = false;
		if (input.Kind == InputReportKind::Cancel && IsMouseCaptured())
			(void)ReleaseMouseCapture();
		InvalidateVisual();
		return Control::ProcessInput(input);
	case InputReportKind::PointerDoubleClick:
	{
		if (input.ChangedButton != MouseButton::Left)
			return Control::ProcessInput(input);
		(void)CaptureMouse();
		if (GetPresentationWindow()) GetPresentationWindow()->SetKeyboardFocus(this, false);
		int hit = HitTestButton(input.X, input.Y);
		if (hit != 0)
		{
			_dragText = false;
			_dragUp = hit > 0;
			_dragDown = hit < 0;
			_hoverButton = hit;
			StartHoverAnimation(1.0f);
			StepBy(hit, input.HasModifier(ModifierKeys::Shift));
		}
		else
		{
			BeginEdit(false);
			SelectAllText();
		}
		auto e = input.CreateMouseEventArgs();
		OnMouseDoubleClick(this, e);
		InvalidateVisual();
		return true;
	}
	case InputReportKind::KeyDown:
	{
		if (!_editing)
			BeginEdit(false);
		bool handled = false;

		if (input.HasModifier(ModifierKeys::Control))
		{
			if (input.Key == Key::A)
			{
				SelectAllText();
				UpdateScroll(true);
				InvalidateVisual();
				return true;
			}
			if (input.Key == Key::C)
			{
				WriteClipboardText(GetPresentationWindow() ? GetPresentationWindow()->Handle : nullptr,
					GetSelectedString());
				return true;
			}
			if (input.Key == Key::V)
			{
				std::wstring clipboardText;
				if (TryReadClipboardText(GetPresentationWindow() ? GetPresentationWindow()->Handle : nullptr,
					clipboardText)) InputText(clipboardText);
				UpdateScroll();
				InvalidateVisual();
				return true;
			}
			if (input.Key == Key::X)
			{
				WriteClipboardText(GetPresentationWindow() ? GetPresentationWindow()->Handle : nullptr,
					GetSelectedString());
				InputBack();
				UpdateScroll();
				InvalidateVisual();
				return true;
			}
			if (input.Key == Key::Z)
			{
				Undo();
				UpdateScroll();
				InvalidateVisual();
				return true;
			}
			if (input.Key == Key::Y)
			{
				Redo();
				UpdateScroll();
				InvalidateVisual();
				return true;
			}
		}

		const bool extendSelection = input.HasModifier(ModifierKeys::Shift);
		switch (input.Key)
		{
		case Key::Up:
			StepBy(1, input.HasModifier(ModifierKeys::Shift));
			handled = true;
			break;
		case Key::Down:
			StepBy(-1, input.HasModifier(ModifierKeys::Shift));
			handled = true;
			break;
		case Key::PageUp:
			StepBy(10, input.HasModifier(ModifierKeys::Shift));
			handled = true;
			break;
		case Key::PageDown:
			StepBy(-10, input.HasModifier(ModifierKeys::Shift));
			handled = true;
			break;
		case Key::Return:
			CommitEdit();
			BeginEdit(false);
			_selectionStart = _selectionEnd = static_cast<int>(_editText.size());
			UpdateScroll(true);
			handled = true;
			break;
		case Key::Escape:
			CancelEdit();
			BeginEdit(true);
			handled = true;
			break;
		case Key::Delete:
			InputDelete();
			UpdateScroll();
			handled = true;
			break;
		case Key::Back:
			InputBack();
			UpdateScroll();
			handled = true;
			break;
		case Key::Right:
		{
			handled = true;
			int textLength = static_cast<int>(_editText.size());
			auto span = CuiTextEdit::NormalizeSelection(_selectionStart, _selectionEnd, _editText.size());
			if (!extendSelection && span.HasSelection())
			{
				_selectionStart = _selectionEnd = span.end;
				UpdateScroll();
			}
			else if (_selectionEnd < textLength)
			{
				_selectionEnd = CuiTextEdit::GetNextCaretIndex(_editText, _selectionEnd, false);
				if (!extendSelection)
					_selectionStart = _selectionEnd;
				UpdateScroll();
			}
			break;
		}
		case Key::Left:
		{
			handled = true;
			auto span = CuiTextEdit::NormalizeSelection(_selectionStart, _selectionEnd, _editText.size());
			if (!extendSelection && span.HasSelection())
			{
				_selectionStart = _selectionEnd = span.start;
				UpdateScroll();
			}
			else if (_selectionEnd > 0)
			{
				_selectionEnd = CuiTextEdit::GetPreviousCaretIndex(_editText, _selectionEnd, false);
				if (!extendSelection)
					_selectionStart = _selectionEnd;
				UpdateScroll();
			}
			break;
		}
		case Key::Home:
			handled = true;
			_selectionEnd = 0;
			if (!extendSelection)
				_selectionStart = _selectionEnd;
			UpdateScroll(true);
			break;
		case Key::End:
			handled = true;
			_selectionEnd = static_cast<int>(_editText.size());
			if (!extendSelection)
				_selectionStart = _selectionEnd;
			UpdateScroll(true);
			break;
		default:
			break;
		}
		{
			auto args = input.CreateKeyEventArgs();
			OnKeyDown(this, args);
		}
		InvalidateVisual();
		return handled;
	}
	case InputReportKind::KeyUp:
		{
			auto args = input.CreateKeyEventArgs();
			OnKeyUp(this, args);
		}
		InvalidateVisual();
		return true;
	default:
		break;
	}

	return Control::ProcessInput(input);
}
