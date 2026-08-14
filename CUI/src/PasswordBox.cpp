#pragma once
#define NOMINMAX
#include "PasswordBox.h"
#include "TextBox.h"
#include "TextBoxBase.h"
#include "Window.h"
#include "TextEditCore.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <float.h>
#include <stdexcept>
#include <typeindex>

namespace
{
	CuiTextEdit::EditOptions PasswordEditOptions()
	{
		CuiTextEdit::EditOptions options;
		options.allowMultiLine = false;
		return options;
	}

	template<typename TValue>
	DependencyPropertyOptions<PasswordBox, TValue> PasswordOptions(
		TValue defaultValue
		CUI_DESIGN_METADATA_ARGUMENTS(
			int order,
			DependencyPropertyEditorKind editor),
		DependencyPropertyFlags flags =
			DependencyPropertyFlags::AffectsRender)
	{
		DependencyPropertyOptions<PasswordBox, TValue> options;
		options.DefaultValue = std::move(defaultValue);
		options.Flags = flags;
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Behavior";
		options.Design.CategoryOrder = 300;
		options.Design.Order = order;
		options.Design.Editor = editor;
		options.Design.Persistence =
			DependencyPropertyPersistence::Metadata;
		)
		return options;
	}

	using DependencyPropertyAccessor = const DependencyProperty& (*)();

	auto PasswordSubscriber(DependencyPropertyAccessor propertyAccessor)
	{
		return [propertyAccessor](
			PasswordBox& target,
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

	DependencyPropertyOptions<PasswordBox, cui::drawing::Brush>
		PasswordBrushOptions(
			cui::drawing::Brush value
			CUI_DESIGN_METADATA_ARGUMENTS(int order))
	{
		auto options = PasswordOptions(
			std::move(value) CUI_DESIGN_METADATA_ARGUMENTS(
				order, DependencyPropertyEditorKind::Text));
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Appearance";
		options.Design.CategoryOrder = 200;
		)
		options.Equals = [](const cui::drawing::Brush& left,
			const cui::drawing::Brush& right) { return left == right; };
		options.Convert = [](const BindingValue& value)
			-> std::optional<cui::drawing::Brush>
		{
			cui::drawing::Brush brush;
			if (value.TryGet(brush)) return brush;
			D2D1_COLOR_F color{};
			if (value.TryGet(color))
				return cui::drawing::MakeSolidColorBrush(color);
			return std::nullopt;
		};
		return options;
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
		CloseClipboard();
		return success;
	}
}

UIClass PasswordBox::Type() { return UIClass::UI_PasswordBox; }

namespace
{
	const DependencyPropertyMetadataRegistration&
		PasswordBoxMaxLengthMetadataRelation()
	{
		static const DependencyPropertyMetadataRegistration relation = []
		{
			auto options = PasswordOptions(
				0 CUI_DESIGN_METADATA_ARGUMENTS(
					30, DependencyPropertyEditorKind::Number),
				DependencyPropertyFlags::None);
			CUI_DESIGN_METADATA_ONLY(
			options.Design.Minimum = 0.0;
			options.Design.Step = 1.0;
			)
			return DependencyPropertyRegistry::AddOwnerStatic<PasswordBox, int>(
				TextBox::MaxLengthProperty(),
				[](PasswordBox& target) { return target.MaxLength; },
				[](PasswordBox& target, const int& value)
				{ target.MaxLength = value; },
				PasswordSubscriber(&TextBox::MaxLengthProperty),
				std::move(options));
		}();
		return relation;
	}

	const DependencyPropertyMetadataRegistration&
		PasswordBoxSelectionBrushMetadataRelation()
	{
		static const DependencyPropertyMetadataRegistration relation = []
		{
			return DependencyPropertyRegistry::AddOwnerStatic<
				PasswordBox, cui::drawing::Brush>(
					TextBoxBase::SelectionBrushProperty(),
					[](PasswordBox& target) { return target.SelectionBrush; },
					[](PasswordBox& target,
						const cui::drawing::Brush& value)
					{ target.SelectionBrush = value; },
					PasswordSubscriber(&TextBoxBase::SelectionBrushProperty),
					PasswordBrushOptions(
						cui::drawing::MakeSolidColorBrush(
							cui::theme::palette::Accent)
						CUI_DESIGN_METADATA_ARGUMENTS(40)));
		}();
		return relation;
	}

	const DependencyPropertyMetadataRegistration&
		PasswordBoxSelectionOpacityMetadataRelation()
	{
		static const DependencyPropertyMetadataRegistration relation = []
		{
			auto options = PasswordOptions(
				0.4 CUI_DESIGN_METADATA_ARGUMENTS(
					50, DependencyPropertyEditorKind::Number));
			options.Coerce = [](PasswordBox&,
				const double& value) -> std::optional<double>
			{
				return (std::clamp)(value, 0.0, 1.0);
			};
			CUI_DESIGN_METADATA_ONLY(
			options.Design.Minimum = 0.0;
			options.Design.Maximum = 1.0;
			options.Design.Step = 0.05;
			)
			return DependencyPropertyRegistry::AddOwnerStatic<
				PasswordBox, double>(TextBoxBase::SelectionOpacityProperty(),
				[](PasswordBox& target) { return target.SelectionOpacity; },
				[](PasswordBox& target, const double& value)
				{ target.SelectionOpacity = value; },
				PasswordSubscriber(&TextBoxBase::SelectionOpacityProperty),
				std::move(options));
		}();
		return relation;
	}

	const DependencyPropertyMetadataRegistration&
		PasswordBoxSelectionTextBrushMetadataRelation()
	{
		static const DependencyPropertyMetadataRegistration relation = []
		{
			return DependencyPropertyRegistry::AddOwnerStatic<
				PasswordBox, cui::drawing::Brush>(
					TextBoxBase::SelectionTextBrushProperty(),
					[](PasswordBox& target)
					{ return target.SelectionTextBrush; },
					[](PasswordBox& target,
						const cui::drawing::Brush& value)
					{ target.SelectionTextBrush = value; },
					PasswordSubscriber(
						&TextBoxBase::SelectionTextBrushProperty),
					PasswordBrushOptions(
						cui::drawing::MakeSolidColorBrush(
							cui::theme::palette::OnAccent)
						CUI_DESIGN_METADATA_ARGUMENTS(60)));
		}();
		return relation;
	}

	const DependencyPropertyMetadataRegistration&
		PasswordBoxCaretBrushMetadataRelation()
	{
		static const DependencyPropertyMetadataRegistration relation = []
		{
			return DependencyPropertyRegistry::AddOwnerStatic<
				PasswordBox, cui::drawing::Brush>(
					TextBoxBase::CaretBrushProperty(),
					[](PasswordBox& target) { return target.CaretBrush; },
					[](PasswordBox& target,
						const cui::drawing::Brush& value)
					{ target.CaretBrush = value; },
					PasswordSubscriber(&TextBoxBase::CaretBrushProperty),
					PasswordBrushOptions(
						cui::drawing::Brush{}
						CUI_DESIGN_METADATA_ARGUMENTS(70)));
		}();
		return relation;
	}

	const DependencyPropertyMetadataRegistration&
		PasswordBoxInactiveSelectionMetadataRelation()
	{
		static const DependencyPropertyMetadataRegistration relation = []
		{
			return DependencyPropertyRegistry::AddOwnerStatic<PasswordBox, bool>(
				TextBoxBase::IsInactiveSelectionHighlightEnabledProperty(),
				[](PasswordBox& target)
				{ return target.IsInactiveSelectionHighlightEnabled; },
				[](PasswordBox& target, const bool& value)
				{ target.IsInactiveSelectionHighlightEnabled = value; },
				PasswordSubscriber(
					&TextBoxBase::IsInactiveSelectionHighlightEnabledProperty),
				PasswordOptions(
					false CUI_DESIGN_METADATA_ARGUMENTS(
						80, DependencyPropertyEditorKind::Boolean)));
		}();
		return relation;
	}
}

const DependencyProperty& PasswordBox::PasswordProperty()
{
	static const auto registration = []
	{
		using Handler = DependencyPropertyMetadata::ChangeHandler;
		DependencyPropertyOptions<PasswordBox, std::wstring> options;
		options.DefaultValue = std::wstring{};
		options.Flags = DependencyPropertyFlags::AffectsMeasure
			| DependencyPropertyFlags::AffectsRender;
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Common";
		options.Design.CategoryOrder = 0;
		options.Design.Order = 10;
		options.Design.Editor = DependencyPropertyEditorKind::Text;
		options.Design.Persistence = DependencyPropertyPersistence::Native;
		)
		options.Changed = [](
			PasswordBox& target, const std::wstring&, const std::wstring&)
		{
			target.PublishPasswordChanged();
		};
		return DependencyPropertyRegistry::RegisterStatic<
			PasswordBox, std::wstring>(
				DependencyPropertyRegistrationLiteral(L"Password"),
				[](PasswordBox& target) { return target.Password; },
				[](PasswordBox& target, const std::wstring& value)
				{ target.Password = value; },
				[](PasswordBox& target, Handler handler,
					DataSourceUpdateMode mode)
				{
					if (mode == DataSourceUpdateMode::OnValidation)
					{
						return target.OnLostFocus.Subscribe(
							[handler = std::move(handler)](Control*)
							{ handler(); });
					}
					return target.PasswordChanged.Subscribe(
						[handler = std::move(handler)](
							Control*, RoutedEventArgs&) { handler(); });
				}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& PasswordBox::PasswordCharProperty()
{
	static const auto registration = []
	{
		auto options = PasswordOptions(
			std::wstring(L"*") CUI_DESIGN_METADATA_ARGUMENTS(
				20, DependencyPropertyEditorKind::Text));
		options.Validate = [](const std::wstring& value)
		{
			if (value.size() == 1) return value[0] != L'\0';
			return value.size() == 2
				&& CuiTextEdit::IsHighSurrogate(value[0])
				&& CuiTextEdit::IsLowSurrogate(value[1]);
		};
		return DependencyPropertyRegistry::RegisterStatic<
			PasswordBox, std::wstring>(
				DependencyPropertyRegistrationLiteral(L"PasswordChar"),
				[](PasswordBox& target) { return target.PasswordChar; },
				[](PasswordBox& target, const std::wstring& value)
				{ target.PasswordChar = value; },
				PasswordSubscriber(&PasswordBox::PasswordCharProperty),
				std::move(options));
	}();
	return *registration;
}

void PasswordBox::RegisterDependencyProperties()
{
	Control::RegisterDependencyProperties();
	CUI_DESIGN_METADATA_ONLY(
	TextBox::RegisterDependencyProperties();
	(void)PasswordProperty();
	(void)PasswordCharProperty();
	(void)PasswordBoxMaxLengthMetadataRelation();
	(void)PasswordBoxSelectionBrushMetadataRelation();
	(void)PasswordBoxSelectionOpacityMetadataRelation();
	(void)PasswordBoxSelectionTextBrushMetadataRelation();
	(void)PasswordBoxCaretBrushMetadataRelation();
	(void)PasswordBoxInactiveSelectionMetadataRelation();
	(void)RegisterControlBorderThicknessMetadata<
			PasswordBox, Control>(
				1.0f CUI_DESIGN_METADATA_ARGUMENTS(60));
	)
}

const DependencyPropertyMetadata*
PasswordBox::ResolveExactDependencyPropertyMetadata(
	const DependencyProperty& property) const
{
	if (&property == &TextBox::MaxLengthProperty())
		return &PasswordBoxMaxLengthMetadataRelation().Metadata();
	if (&property == &TextBoxBase::SelectionBrushProperty())
		return &PasswordBoxSelectionBrushMetadataRelation().Metadata();
	if (&property == &TextBoxBase::SelectionOpacityProperty())
		return &PasswordBoxSelectionOpacityMetadataRelation().Metadata();
	if (&property == &TextBoxBase::SelectionTextBrushProperty())
		return &PasswordBoxSelectionTextBrushMetadataRelation().Metadata();
	if (&property == &TextBoxBase::CaretBrushProperty())
		return &PasswordBoxCaretBrushMetadataRelation().Metadata();
	if (&property ==
		&TextBoxBase::IsInactiveSelectionHighlightEnabledProperty())
	{
		return &PasswordBoxInactiveSelectionMetadataRelation().Metadata();
	}
	if (&property == &Control::BorderThicknessProperty())
	{
		return &RegisterControlBorderThicknessMetadata<
			PasswordBox, Control>(
				1.0f CUI_DESIGN_METADATA_ARGUMENTS(60)).Metadata();
	}
	return Control::ResolveExactDependencyPropertyMetadata(property);
}

GET_CPP(PasswordBox, std::wstring, Password)
{
	return _password;
}

SET_CPP(PasswordBox, std::wstring, Password)
{
	if (!SetPropertyField(
		PasswordProperty(), _password, std::move(value))) return;
	_selectionStart = (std::clamp)(
		_selectionStart, 0, static_cast<int>(_password.size()));
	_selectionEnd = (std::clamp)(
		_selectionEnd, 0, static_cast<int>(_password.size()));
	InvalidateTextLayout();
}

GET_CPP(PasswordBox, std::wstring, PasswordChar)
{
	return _passwordChar;
}
SET_CPP(PasswordBox, std::wstring, PasswordChar)
{
	if (!SetPropertyField(
		PasswordCharProperty(), _passwordChar, std::move(value))) return;
	InvalidateTextLayout();
}
GET_CPP(PasswordBox, int, MaxLength) { return _maxLength; }
SET_CPP(PasswordBox, int, MaxLength)
{
	(void)SetPropertyField(TextBox::MaxLengthProperty(), _maxLength, value);
}
GET_CPP(PasswordBox, cui::drawing::Brush, SelectionBrush)
{
	return _selectionBrush;
}
SET_CPP(PasswordBox, cui::drawing::Brush, SelectionBrush)
{
	(void)SetPropertyField(
		TextBoxBase::SelectionBrushProperty(),
		_selectionBrush, std::move(value));
}
GET_CPP(PasswordBox, double, SelectionOpacity)
{
	return _selectionOpacity;
}
SET_CPP(PasswordBox, double, SelectionOpacity)
{
	(void)SetPropertyField(
		TextBoxBase::SelectionOpacityProperty(), _selectionOpacity, value);
}
GET_CPP(PasswordBox, cui::drawing::Brush, SelectionTextBrush)
{
	return _selectionTextBrush;
}
SET_CPP(PasswordBox, cui::drawing::Brush, SelectionTextBrush)
{
	(void)SetPropertyField(
		TextBoxBase::SelectionTextBrushProperty(),
		_selectionTextBrush, std::move(value));
}
GET_CPP(PasswordBox, cui::drawing::Brush, CaretBrush)
{
	return _caretBrush;
}
SET_CPP(PasswordBox, cui::drawing::Brush, CaretBrush)
{
	(void)SetPropertyField(
		TextBoxBase::CaretBrushProperty(), _caretBrush, std::move(value));
}
GET_CPP(PasswordBox, bool, IsInactiveSelectionHighlightEnabled)
{
	return _isInactiveSelectionHighlightEnabled;
}
SET_CPP(PasswordBox, bool, IsInactiveSelectionHighlightEnabled)
{
	(void)SetPropertyField(
		TextBoxBase::IsInactiveSelectionHighlightEnabledProperty(),
		_isInactiveSelectionHighlightEnabled, value);
}

void PasswordBox::CommitPasswordEdit(std::wstring value)
{
	if (_password == value) return;
	_password = std::move(value);
	PublishPasswordChanged();
}

void PasswordBox::PublishPasswordChanged()
{
	InvalidateTextLayout();
	RoutedEventArgs args;
	PasswordChanged(this, args);
	NotifyAccessibilityValueChanged();
	RequestLayout();
	InvalidateVisual();
}

bool PasswordBox::HandlesNavigationKey(Key key) const
{
	switch (key)
	{
	case Key::Left:
	case Key::Right:
	case Key::Up:
	case Key::Down:
	case Key::Home:
	case Key::End:
	case Key::PageUp:
	case Key::PageDown:
		return true;
	default:
		return false;
	}
}
PasswordBox::PasswordBox()
{
	RegisterDependencyProperties();
}
void PasswordBox::InputText(std::wstring input)
{
	std::wstring newText = this->Password;
	auto options = PasswordEditOptions();
	options.maxTextLength = static_cast<size_t>(_maxLength);
	CuiTextEdit::ReplaceSelection(
		newText, this->_selectionStart,
		this->_selectionEnd, input, options);
	CommitPasswordEdit(std::move(newText));
}
void PasswordBox::InputBack()
{
	std::wstring newText = this->Password;
	CuiTextEdit::Backspace(newText, this->_selectionStart, this->_selectionEnd, PasswordEditOptions());
	CommitPasswordEdit(std::move(newText));
}
void PasswordBox::InputDelete()
{
	std::wstring newText = this->Password;
	CuiTextEdit::DeleteForward(newText, this->_selectionStart, this->_selectionEnd, PasswordEditOptions());
	CommitPasswordEdit(std::move(newText));
}
void PasswordBox::UpdateScroll(bool arrival)
{
	(void)arrival;
	const float renderWidth = (std::max)(
		0.0f, this->ActualWidth - Padding.Left - Padding.Right);
	DWRITE_HIT_TEST_METRICS caret{};
	if (!GetCaretLayoutMetrics(_selectionEnd, caret))
	{
		_horizontalScrollOffset = 0.0f;
		return;
	}
	if ((caret.left + caret.width) - _horizontalScrollOffset
		> renderWidth)
	{
		_horizontalScrollOffset =
			(caret.left + caret.width) - renderWidth;
	}
	if (caret.left - _horizontalScrollOffset < 0.0f)
	{
		_horizontalScrollOffset = caret.left;
	}
	_horizontalScrollOffset = (std::max)(
		0.0f, _horizontalScrollOffset);
}
std::wstring PasswordBox::GetSelectedString()
{
	auto span = CuiTextEdit::NormalizeSelection(this->_selectionStart, this->_selectionEnd, this->Password.size());
	if (!span.HasSelection())
		return L"";
	return this->Password.substr(static_cast<size_t>(span.start), static_cast<size_t>(span.Length()));
}

std::wstring PasswordBox::GetDisplayText()
{
	if (_password.empty()) return {};
	const std::wstring mask = _passwordChar.empty()
		? std::wstring(L"*") : _passwordChar;
	std::wstring result;
	result.reserve(_password.size() * mask.size());
	for (size_t index = 0; index < _password.size();)
	{
		result.append(mask);
		index += CuiTextEdit::HasSurrogatePairAt(
			_password, static_cast<int>(index)) ? 2u : 1u;
	}
	return result;
}

void PasswordBox::InvalidateTextLayout() noexcept
{
	_textLayout.Reset();
	_layoutText.clear();
	_layoutFont = nullptr;
	_caretRectCacheValid = false;
}

IDWriteTextLayout* PasswordBox::EnsureTextLayout()
{
	auto* font = GetRenderFont();
	if (!font || !font->FontObject) return nullptr;
	const auto display = GetDisplayText();
	if (_textLayout
		&& _layoutText == display
		&& _layoutFont == font->FontObject)
	{
		return _textLayout.Get();
	}
	_textLayout.Reset();
	_layoutText = display;
	_layoutFont = font->FontObject;
	_textLayout.Attach(Factory::CreateStringLayout(
		display, FLT_MAX,
		(std::max)(1.0f,
			ActualHeight - Padding.Top - Padding.Bottom),
		font->FontObject));
	if (_textLayout)
		_textSize = font->GetTextSize(_textLayout.Get());
	return _textLayout.Get();
}

float PasswordBox::GetTextOriginX()
{
	return Padding.Left - _horizontalScrollOffset;
}

float PasswordBox::GetTextOriginY()
{
	const float renderHeight = (std::max)(
		0.0f, ActualHeight - Padding.Top - Padding.Bottom);
	const float remaining = (std::max)(
		0.0f, renderHeight - _textSize.height);
	switch (VerticalContentAlignment)
	{
	case VerticalAlignment::Bottom:
		return Padding.Top + remaining;
	case VerticalAlignment::Center:
		return Padding.Top + remaining * 0.5f;
	case VerticalAlignment::Top:
	case VerticalAlignment::Stretch:
	default:
		return Padding.Top;
	}
}

bool PasswordBox::GetCaretLayoutMetrics(
	int caretIndex, DWRITE_HIT_TEST_METRICS& metrics)
{
	auto* layout = EnsureTextLayout();
	if (!layout) return false;
	// Password selection indices are password UTF-16 offsets. The display
	// string may use a surrogate-pair mask, so map through text elements.
	int displayIndex = 0;
	for (int index = 0;
		index < (std::clamp)(
			caretIndex, 0, static_cast<int>(_password.size()));)
	{
		displayIndex += static_cast<int>(_passwordChar.empty()
			? 1u : _passwordChar.size());
		index += CuiTextEdit::HasSurrogatePairAt(_password, index)
			? 2 : 1;
	}
	FLOAT x = 0.0f;
	FLOAT y = 0.0f;
	return SUCCEEDED(layout->HitTestTextPosition(
		static_cast<UINT32>(displayIndex),
		FALSE, &x, &y, &metrics));
}

int PasswordBox::HitTestTextPosition(
	float localX, float localY)
{
	auto* layout = EnsureTextLayout();
	if (!layout) return 0;
	BOOL trailing = FALSE;
	BOOL inside = FALSE;
	DWRITE_HIT_TEST_METRICS metrics{};
	if (FAILED(layout->HitTestPoint(
		localX - GetTextOriginX(),
		localY - GetTextOriginY(),
		&trailing, &inside, &metrics)))
	{
		return 0;
	}
	const int displayIndex = static_cast<int>(
		metrics.textPosition
		+ (trailing ? metrics.length : 0));
	const size_t maskLength = _passwordChar.empty()
		? 1u : _passwordChar.size();
	const int elementIndex = maskLength == 0
		? displayIndex
		: displayIndex / static_cast<int>(maskLength);
	int passwordIndex = 0;
	for (int element = 0;
		element < elementIndex
			&& passwordIndex < static_cast<int>(_password.size());
		element++)
	{
		passwordIndex += CuiTextEdit::HasSurrogatePairAt(
			_password, passwordIndex) ? 2 : 1;
	}
	return (std::clamp)(
		passwordIndex, 0, static_cast<int>(_password.size()));
}

// ---- 公共选择/编辑 API ----
int PasswordBox::GetSelectionStart()
{
	auto span = CuiTextEdit::NormalizeSelection(
		_selectionStart, _selectionEnd, Password.size());
	return span.start;
}

int PasswordBox::GetSelectionLength()
{
	auto span = CuiTextEdit::NormalizeSelection(this->_selectionStart, this->_selectionEnd, this->Password.size());
	return span.HasSelection() ? static_cast<int>(span.Length()) : 0;
}

int PasswordBox::GetCaretIndex()
{
	return (std::clamp)(
		_selectionEnd, 0, static_cast<int>(Password.size()));
}

void PasswordBox::SetCaretIndex(int value)
{
	Select(value, 0);
}

bool PasswordBox::HasSelection()
{
	return GetSelectionLength() > 0;
}

void PasswordBox::Select(int start, int length)
{
	const int textLen = static_cast<int>(this->Password.size());
	start = (std::clamp)(start, 0, textLen);
	length = (std::clamp)(length, 0, textLen - start);
	this->_selectionStart = start;
	this->_selectionEnd = start + length;
	this->InvalidateVisual();
}

void PasswordBox::SelectAll()
{
	this->_selectionStart = 0;
	this->_selectionEnd = static_cast<int>(this->Password.size());
	this->InvalidateVisual();
}

void PasswordBox::ClearSelection()
{
	this->_selectionEnd = this->_selectionStart;
	this->InvalidateVisual();
}

void PasswordBox::Clear()
{
	this->SelectAll();
	this->InputBack();
}

void PasswordBox::InsertText(const std::wstring& text)
{
	if (text.empty() && !HasSelection()) return;
	this->InputText(text);
}

bool PasswordBox::Paste()
{
	std::wstring clipboardText;
	if (!TryReadClipboardText(this->GetPresentationWindow() ? this->GetPresentationWindow()->Handle : nullptr, clipboardText))
		return false;
	if (clipboardText.empty()) return false;
	this->InputText(clipboardText);
	return true;
}

bool PasswordBox::ApplyTextInput(const TextCompositionEventArgs& input)
{
	if (input.Text.empty()) return false;
	InputText(input.Text);
	UpdateScroll();
	InvalidateVisual();
	return true;
}

bool PasswordBox::TryGetTextInputCaretRect(D2D1_RECT_F& outRect)
{
	if (_caretRectCacheValid)
	{
		outRect = TransformAbsoluteRectToRenderSpace(_caretRectCache);
		return true;
	}
	const auto absolute = GetAbsoluteLocationDip();
	const float x = static_cast<float>(absolute.x)
		+ Padding.Left - _horizontalScrollOffset;
	const float y = static_cast<float>(absolute.y) + Padding.Top;
	const float height = GetRenderFont() && GetRenderFont()->FontHeight > 0.0f
		? GetRenderFont()->FontHeight : 16.0f;
	outRect = TransformAbsoluteRectToRenderSpace(
		D2D1::RectF(x, y, x + 1.0f, y + height));
	return true;
}

void PasswordBox::OnRender()
{
	if (!IsVisible) return;
	auto* d2d = GetDrawingContext();
	auto* font = GetRenderFont();
	auto* layout = EnsureTextLayout();
	if (!d2d || !font || !layout) return;
	const auto size = GetActualSizeDip();
	const auto brushSize = D2D1::SizeF(size.width, size.height);
	Microsoft::WRL::ComPtr<ID2D1Brush> foreground;
	foreground.Attach(CreateForegroundBrush(*d2d, brushSize));
	auto selectionDefinition = SelectionBrush;
	selectionDefinition.Opacity *= static_cast<float>(
		(std::clamp)(SelectionOpacity, 0.0, 1.0));
	Microsoft::WRL::ComPtr<ID2D1Brush> selectionBrush;
	selectionBrush.Attach(
		selectionDefinition.CreateBrush(*d2d, brushSize));
	Microsoft::WRL::ComPtr<ID2D1Brush> selectionTextBrush;
	selectionTextBrush.Attach(
		SelectionTextBrush.CreateBrush(*d2d, brushSize));
	Microsoft::WRL::ComPtr<ID2D1Brush> caretBrush;
	caretBrush.Attach(CaretBrush.CreateBrush(*d2d, brushSize));

	const bool focused = GetPresentationWindow()
		&& GetPresentationWindow()->GetKeyboardFocusedElement() == this;
	const bool showSelection =
		focused || IsInactiveSelectionHighlightEnabled;
	const auto span = CuiTextEdit::NormalizeSelection(
		_selectionStart, _selectionEnd, _password.size());
	const size_t maskLength =
		_passwordChar.empty() ? 1u : _passwordChar.size();
	auto toDisplayIndex = [&](int passwordIndex)
	{
		int elements = 0;
		for (int index = 0;
			index < (std::clamp)(
				passwordIndex, 0, static_cast<int>(_password.size()));)
		{
			elements++;
			index += CuiTextEdit::HasSurrogatePairAt(
				_password, index) ? 2 : 1;
		}
		return static_cast<UINT32>(
			static_cast<size_t>(elements) * maskLength);
	};
	const UINT32 selectionStart = toDisplayIndex(span.start);
	const UINT32 selectionEnd = toDisplayIndex(span.end);
	const UINT32 selectionLength =
		selectionEnd - selectionStart;
	const float originX = GetTextOriginX();
	const float originY = GetTextOriginY();
	_caretRectCacheValid = false;
	bool shouldDrawCaret = false;
	D2D1_POINT_2F caretStart{};
	D2D1_POINT_2F caretEnd{};

	BeginRender();
	if (showSelection && selectionLength > 0 && selectionBrush)
	{
		auto ranges = font->HitTestTextRange(
			layout, selectionStart, selectionLength);
		for (const auto& range : ranges)
		{
			d2d->FillRect(
				range.left + originX,
				range.top + originY,
				range.width, range.height,
				selectionBrush.Get());
		}
	}
	if (showSelection && selectionLength > 0
		&& selectionTextBrush)
	{
		if (foreground)
			d2d->DrawStringLayoutEffect(
				layout, originX, originY, foreground.Get(),
				DWRITE_TEXT_RANGE{
					selectionStart, selectionLength },
				selectionTextBrush.Get(), font);
		else
			d2d->DrawStringLayoutEffect(
				layout, originX, originY,
				RendererForegroundColor,
				DWRITE_TEXT_RANGE{
					selectionStart, selectionLength },
				selectionTextBrush.Get(), font);
	}
	else if (foreground)
	{
		d2d->DrawStringLayout(
			layout, originX, originY, foreground.Get());
	}
	else
	{
		d2d->DrawStringLayout(
			layout, originX, originY,
			RendererForegroundColor);
	}

	if (focused && !span.HasSelection())
	{
		DWRITE_HIT_TEST_METRICS caret{};
		if (GetCaretLayoutMetrics(_selectionEnd, caret))
		{
			const float x = caret.left + originX;
			const float y = caret.top + originY;
			const float height = caret.height > 0.0f
				? caret.height : font->FontHeight;
			const auto absolute = GetAbsoluteLocationDip();
			_caretRectCache = D2D1::RectF(
				static_cast<float>(absolute.x) + x - 2.0f,
				static_cast<float>(absolute.y) + y - 2.0f,
				static_cast<float>(absolute.x) + x + 2.0f,
				static_cast<float>(absolute.y) + y + height + 2.0f);
			_caretRectCacheValid = true;
			shouldDrawCaret = true;
			caretStart = D2D1::Point2F(x, y);
			caretEnd = D2D1::Point2F(x, y + height);
		}
	}
	UpdateCaretBlinkState(
		focused, _selectionStart, _selectionEnd,
		_caretRectCacheValid,
		_caretRectCacheValid ? &_caretRectCache : nullptr);
	if (shouldDrawCaret && IsCaretBlinkVisible())
	{
		if (caretBrush)
			d2d->DrawLine(caretStart, caretEnd, caretBrush.Get());
		else if (foreground)
			d2d->DrawLine(caretStart, caretEnd, foreground.Get());
		else
			d2d->DrawLine(
				caretStart, caretEnd, RendererForegroundColor);
	}
	EndRender();
}

bool PasswordBox::GetAnimatedInvalidRect(D2D1_RECT_F& outRect)
{
	return GetCaretBlinkInvalidRect(outRect);
}
bool PasswordBox::ProcessInput(const InputReport& input)
{
	if (!this->IsEnabled || !this->IsVisible) return true;
	switch (input.Kind)
	{
	case InputReportKind::MouseWheel:
	{
		auto eventArgs = input.CreateMouseEventArgs();
		this->OnMouseWheel(this, eventArgs);
	}
	break;
	case InputReportKind::PointerMove:
	{
		if (input.IsButtonPressed(MouseButton::Left)
			&& this->GetPresentationWindow()->GetKeyboardFocusedElement() == this)
		{
			auto font = this->GetRenderFont();
			float renderHeight = this->ActualHeight - Padding.Top - Padding.Bottom;
			std::wstring maskedText = this->GetDisplayText();
			_selectionEnd = font->HitTestTextPosition(maskedText, FLT_MAX,
				renderHeight, (input.X - Padding.Left)
					+ this->_horizontalScrollOffset, input.Y - Padding.Top);
			UpdateScroll();
			this->InvalidateVisual();
		}
		auto eventArgs = input.CreateMouseEventArgs();
		this->OnMouseMove(this, eventArgs);
	}
	break;
	case InputReportKind::PointerDown:
	{
		if (input.ChangedButton == MouseButton::Left)
		{
			(void)CaptureMouse();
			if (this->GetPresentationWindow()->GetKeyboardFocusedElement() != this)
			{
				auto previousSelection = this->GetPresentationWindow()->GetKeyboardFocusedElement();
				this->GetPresentationWindow()->SetKeyboardFocus(this, false);
				if (previousSelection) previousSelection->InvalidateVisual();
			}
			auto font = this->GetRenderFont();
			float renderHeight = this->ActualHeight - Padding.Top - Padding.Bottom;
			std::wstring maskedText = this->GetDisplayText();
			this->_selectionStart = this->_selectionEnd =
				font->HitTestTextPosition(maskedText, FLT_MAX, renderHeight,
					(input.X - Padding.Left) + this->_horizontalScrollOffset,
					input.Y - Padding.Top);
		}
		auto eventArgs = input.CreateMouseEventArgs();
		this->OnMouseDown(this, eventArgs);
		this->InvalidateVisual();
	}
	break;
	case InputReportKind::PointerUp:
	{
		if (this->GetPresentationWindow()->GetKeyboardFocusedElement() == this)
		{
			float renderHeight = this->ActualHeight - Padding.Top - Padding.Bottom;
			auto font = this->GetRenderFont();
			std::wstring maskedText = this->GetDisplayText();
			_selectionEnd = font->HitTestTextPosition(maskedText, FLT_MAX,
				renderHeight, (input.X - Padding.Left)
					+ this->_horizontalScrollOffset, input.Y - Padding.Top);
		}
		auto eventArgs = input.CreateMouseEventArgs();
		this->OnMouseUp(this, eventArgs);
		this->InvalidateVisual();
		if (input.ChangedButton == MouseButton::Left && IsMouseCaptured())
			(void)ReleaseMouseCapture();
	}
	break;
	case InputReportKind::Cancel:
	case InputReportKind::CaptureLost:
		if (input.Kind == InputReportKind::Cancel && IsMouseCaptured())
			(void)ReleaseMouseCapture();
		return Control::ProcessInput(input);
	case InputReportKind::PointerDoubleClick:
	{
		if (input.ChangedButton != MouseButton::Left)
			return Control::ProcessInput(input);
		this->GetPresentationWindow()->SetKeyboardFocus(this, false);
		this->_selectionStart = 0;
		this->_selectionEnd = static_cast<int>(this->Password.size());
		this->_horizontalScrollOffset = 0.0f;
		auto eventArgs = input.CreateMouseEventArgs();
		this->OnMouseDoubleClick(this, eventArgs);
		this->InvalidateVisual();
	}
	break;
	case InputReportKind::KeyDown:
	{
		bool handled = false;
		if (input.HasModifier(ModifierKeys::Control))
		{
			if (input.Key == Key::A)
			{
				SelectAll();
				UpdateScroll();
				InvalidateVisual();
				return true;
			}
			if (input.Key == Key::V)
			{
				(void)Paste();
				UpdateScroll();
				InvalidateVisual();
				return true;
			}
		}
		if (input.Key == Key::Back)
		{
			handled = true;
			InputBack();
			UpdateScroll();
		}
		else if (input.Key == Key::Delete)
		{
			handled = true;
			this->InputDelete();
			UpdateScroll();
		}
		else if (input.Key == Key::Right)
		{
			handled = true;
			int textLength = static_cast<int>(this->Password.size());
			const bool extendSelection = input.HasModifier(ModifierKeys::Shift);
			auto span = CuiTextEdit::NormalizeSelection(this->_selectionStart, this->_selectionEnd, this->Password.size());
			if (!extendSelection && span.HasSelection())
			{
				this->_selectionStart = this->_selectionEnd = span.end;
				UpdateScroll();
			}
			else if (this->_selectionEnd < textLength)
			{
				this->_selectionEnd = CuiTextEdit::GetNextCaretIndex(this->Password, this->_selectionEnd, false);
				if (!extendSelection)
					this->_selectionStart = this->_selectionEnd;
				UpdateScroll();
			}
		}
		else if (input.Key == Key::Left)
		{
			handled = true;
			const bool extendSelection = input.HasModifier(ModifierKeys::Shift);
			auto span = CuiTextEdit::NormalizeSelection(this->_selectionStart, this->_selectionEnd, this->Password.size());
			if (!extendSelection && span.HasSelection())
			{
				this->_selectionStart = this->_selectionEnd = span.start;
				UpdateScroll();
			}
			else if (this->_selectionEnd > 0)
			{
				this->_selectionEnd = CuiTextEdit::GetPreviousCaretIndex(this->Password, this->_selectionEnd, false);
				if (!extendSelection)
					this->_selectionStart = this->_selectionEnd;
				UpdateScroll();
			}
		}
		else if (input.Key == Key::Home)
		{
			handled = true;
			this->_selectionEnd = 0;
			if (!input.HasModifier(ModifierKeys::Shift))
				this->_selectionStart = this->_selectionEnd;
			UpdateScroll();
		}
		else if (input.Key == Key::End)
		{
			handled = true;
			this->_selectionEnd = static_cast<int>(this->Password.size());
			if (!input.HasModifier(ModifierKeys::Shift))
				this->_selectionStart = this->_selectionEnd;
			UpdateScroll();
		}
		else if (input.Key == Key::PageUp)
		{
			handled = true;
			this->_selectionEnd = 0;
			if (!input.HasModifier(ModifierKeys::Shift))
				this->_selectionStart = this->_selectionEnd;
			UpdateScroll(true);
		}
		else if (input.Key == Key::PageDown)
		{
			handled = true;
			this->_selectionEnd = static_cast<int>(this->Password.size());
			if (!input.HasModifier(ModifierKeys::Shift))
				this->_selectionStart = this->_selectionEnd;
			UpdateScroll(true);
		}
		else if (input.Key == Key::Up || input.Key == Key::Down
			|| input.Key == Key::Return || input.Key == Key::Escape)
		{
			handled = true;
		}
		auto eventArgs = input.CreateKeyEventArgs();
		this->OnKeyDown(this, eventArgs);
		this->InvalidateVisual();
		return handled;
	}
	case InputReportKind::KeyUp:
	{
		auto eventArgs = input.CreateKeyEventArgs();
		this->OnKeyUp(this, eventArgs);
		this->InvalidateVisual();
	}
	break;
	}
	return true;
}
